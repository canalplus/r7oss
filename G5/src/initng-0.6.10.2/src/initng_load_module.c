/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <errno.h>
#include "initng.h"
#include "initng_global.h"
//#include "initng_service_cache.h"
#include "initng_load_module.h"
#include "initng_module.h"
#include "initng_toolbox.h"
#include "initng_main.h"
#include "initng_common.h"
#include <initng-paths.h>

/* 
 * Some modules depend on other modules.
 *
 * A module declares its dependencies by adding an array called
 * module_needs, with the names of the modules that it needs, like
 * this:
 *
 * char *module_needs[] = {
 *     "foo",
 *     "bar",
 *     NULL
 * };
 *
 * When a module is loaded, it checks to see if all of the modules
 * that it depends on are already loaded. If they are not, it refuses
 * to load.
 *
 * The initng_load_all_modules() routine will insure that modules are
 * loaded in the correct order. It does this by loading all modules
 * that have their dependencies met, and iterating until either all
 * modules are loaded or there are no modules left to load. (This is
 * effectively a breadth-first search.)
 *
 * Unloading follows the same logic, except in reverse. A module may
 * not be unloaded if a module that depends on it is loaded. Likewise,
 * the initng_unload_all_modules() routine will unload modules in the
 * proper order. (Again, through a breadth-first search.)
 *
 * Modules that result in circular dependencies can never be loaded.
 *
 * Several things are missing from the dependency setup:
 *
 * - Some notion of "services". A module can depend on only specific
 *   module, say "cpout", not "any module that provides output".
 *
 * - Priority mechanism. The callback functions from modules are
 *   invoked in the order that they are loaded.
 *
 * - Ability for modules to refuse to unload.
 * 
 * Hopefully these will not matter!
 */

/* Note: modules are added to the *start* of the linked list when they
 * are loaded. This makes unloading all modules easy, because a module
 * always sits before the modules that it depends on in the list. */

/*
 * See if we have already loaded a module with the given name.
 *
 * Returns TRUE if already loaded, else FALSE.
 */
static int initng_load_module_is_loaded(const char *module_name)
{
	m_h *m = NULL;

	assert(module_name != NULL);

	while_module_db(m)
	{
		if (strcmp(m->module_name, module_name) == 0 ||
			strcmp(m->module_filename, module_name) == 0)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/* 
 * Returns TRUE if module has its dependencies met, else FALSE.
 */
static int initng_load_module_needs_are_loaded(const m_h * m)
{
	char **needs;
	int retval;

	assert(m != NULL);

	/* if there are no needs, then we have met them */
	if (m->module_needs == NULL)
	{
		return TRUE;
	}

	/* otherwise check each one */
	needs = m->module_needs;
	retval = TRUE;
	if (needs != NULL)
	{
		while (*needs != NULL)
		{
			if (!initng_load_module_is_loaded(*needs))
			{
				F_("Plugin \"%s\" (%s) requires plugin \"%s\" to work, unlodading %s.\n", m->module_name, m->module_filename, *needs, m->module_name);
				retval = FALSE;
			}
			needs++;
		}
	}
	return retval;
}

/* 
 * Read the module information from the file. Does not actually call
 * module_init() for the module, so it is not "loaded" at this point.
 */
m_h *initng_load_module_open(const char *module_path, const char *module_name)
{
	struct stat st;
	char *errmsg;
	m_h *m = NULL;

	assert(module_path != NULL);
	assert(module_name != NULL);

	/* allocate, the new module info struct */
	if (!(m = (m_h *) i_calloc(1, sizeof(m_h))))
	{
		F_("Unable to allocate memory, for new module description.\n");
		return (NULL);
	}

	/* make sure this flag is not set */
	m->initziated = FALSE;

	/* check that file exists */
	if (stat(module_path, &st) != 0)
	{
		F_("Module \"%s\" not found\n", module_path);
		free(m);
		return (NULL);
	}

	/* open module */
	dlerror();								/* clear any existing error */
	m->module_dlhandle = dlopen(module_path, RTLD_LAZY);
	/* 
	 * this breaks ngc2 on my testbox - neuron : 
	 * g.modules[i].module = dlopen(module_name, RTLD_NOW | RTLD_GLOBAL); 
	 * */
	if (m->module_dlhandle == NULL)
	{
		F_("Error opening module %s; %s\n", module_name, dlerror());
		free(m);
		return (NULL);
	}

	D_("Success opening module \"%s\"\n", module_name);

	/* match api_version */
	{
		int *plugin_api;

		plugin_api = dlsym(m->module_dlhandle, "plugin_api_version");
		if (!plugin_api)
		{
			F_("Symbol \"plugin_api_version\" not found, the macro INITNG_PLUGIN_MACRO is not added to plugin %s :: %s\n", module_path, dlerror());
			initng_load_module_close_and_free(m);
			return (NULL);
		}

		/* match the API version */
		if (*plugin_api != API_VERSION)
		{
			F_("Plugin %s has wrong api version, this meens that its compiled with another version of initng core.\n", module_path);
			initng_load_module_close_and_free(m);
			return (NULL);
		}
		D_("Plugin %s ver match: %i\n", module_path, *plugin_api);
	}

	/* get initialization function */
	dlerror();								/* clear any existing error */
	m->module_init = dlsym(m->module_dlhandle, "module_init");
	if (m->module_init == NULL)
	{
		errmsg = dlerror();
		F_("Error reading module_init(); %s\n", errmsg);
		initng_load_module_close_and_free(m);
		return (NULL);
	}

	/* get unload function */
	dlerror();								/* clear any existing error */
	m->module_unload = dlsym(m->module_dlhandle, "module_unload");
	if (m->module_unload == NULL)
	{
		errmsg = dlerror();
		F_("Error reading module_unload(); %s\n", errmsg);
		initng_load_module_close_and_free(m);
		return (NULL);
	}

	/* get dependency list (may be NULL - this is not an error) */
	dlerror();								/* clear any existing error */
	m->module_needs = (char **) dlsym(m->module_dlhandle, "module_needs");
	/* XXX: is there any way to check for "not found", since we don't 
	 * consider that an error? */

	/* set module name in database */
	m->module_name = i_strdup(module_name);
	m->module_filename = i_strdup(module_path);

	return (m);
}

/* 
 * Close the module.
 */
void initng_load_module_close_and_free(m_h * m)
{
	assert(m != NULL);

	/* free module name */
	if (m->module_name)
	{
		/*printf("Free: %s\n", m->module_name); */
		free(m->module_name);
		m->module_name = NULL;
	}

	/* free module_filename */
	if (m->module_filename)
	{
		free(m->module_filename);
		m->module_filename = NULL;
	}

	/* close the lib */
	if (m->module_dlhandle)
		dlclose(m->module_dlhandle);

	/* remove from list if added */
	list_del(&m->list);

	/* free struct */
	free(m);
}

/* load a dynamic module */
/* XXX: more information! */
m_h *initng_load_module(const char *module)
{
	char *module_path = NULL;
	char *module_name = NULL;
	m_h *new_m;

	assert(module != NULL);

	/* if its a full path starting with / and contains .so */
	if (module[0] == '/' && strstr(module, ".so"))
	{
		/* then copy module name, to module_path */
		module_path = i_strdup(module);

		int len = strlen(module);
		int i = len;

		/*
		 * backwards step until we stand on
		 * first char after the last '/' char
		 */
		while (i > 1 && module[i - 1] != '/')
			i--;

		/* libsyslog.so -->> syslog.so */
		if (strncmp("lib", &module[i], 3) == 0)
			i += 3;

		/* now set module_name */
		module_name = i_strndup(&module[i], len - i - 3);
	}
	else
	{
		/* set modulename from module */
		module_name = (char *) module;
	}

	/* look for duplicates */
	if (initng_load_module_is_loaded(module_name))
	{
		F_("Module \"%s\" already loaded, won't load it twice\n",
		   module_name);

		/* free module_name if duped */
		if (module_name != module)
			free(module_name);
		if (module_path)
			free(module_path);
		return (NULL);
	}

	if (!module_path)
	{
		/* build a path */
		module_path = (char *) i_calloc(1,
										strlen(INITNG_PLUGIN_DIR) +
										strlen(module_name) + 30);
		strcpy(module_path, INITNG_PLUGIN_DIR "/lib");
		strcat(module_path, module_name);
		strcat(module_path, ".so");
	}

	/* load information from library */
	new_m = initng_load_module_open(module_path, module_name);

	/* try again with a static path */
	if (!new_m)
	{
		strcpy(module_path, "/lib/initng/");
		strcat(module_path, module_name);
		strcat(module_path, ".so");
		new_m = initng_load_module_open(module_path, module_name);

		/* try 3d time with a new static path */
		if (!new_m)
		{

			strcpy(module_path, "/lib32/initng/");
			strcat(module_path, module_name);
			strcat(module_path, ".so");
			new_m = initng_load_module_open(module_path, module_name);

			/* try 4d time with a new static path */
			if (!new_m)
			{

				strcpy(module_path, "/lib64/initng/");
				strcat(module_path, module_name);
				strcat(module_path, ".so");
				new_m = initng_load_module_open(module_path, module_name);

				/* make sure this succeded */
				if (!new_m)
				{
					F_("Unable to load module \"%s\"\n", module);
					/* free, these are duped in initng_load_module_open(a,b) */
					if (module_name != module)
						free(module_name);
					free(module_path);
					return (NULL);
				}
			}
		}
	}
	/* free, these are duped in initng_load_module_open(a,b) */
	if (module_name != module)
		free(module_name);
	free(module_path);

	/* see if we have our dependencies met */
	if (!initng_load_module_needs_are_loaded(new_m))
	{
		F_("Not loading module \"%s\", missing needed module(s)\n",
		   module_path);
		initng_load_module_close_and_free(new_m);
		return (NULL);
	}

	/* run module_init */
	new_m->initziated = (*new_m->module_init) (API_VERSION);

	D_("for module \"%s\" return: %i\n", module_path, new_m->initziated);

	if (new_m->initziated < 1)
	{
		F_("Module %s did not load correctly (module_init() returned %i)\n",
		   module_path, new_m->initziated);
		/* XXX: used to be here, but why? */
		/* sleep(1); */
		initng_load_module_close_and_free(new_m);
		return (NULL);
	}

	assert(new_m->module_name);
	list_add(&new_m->list, &g.module_db.list);
	/* and we're done */
	return (new_m);
}



/*
 * See the named module is needed elsewhere.
 *
 * Returns TRUE if the module is needed, else FALSE.
 */
static int initng_load_module_is_needed(const char *module_name)
{
	char **needs;
	m_h *m = NULL;
	int retval = FALSE;

	assert(module_name != NULL);

	while_module_db(m)
	{
		/* if not this module, have needs set, continue.. */
		if (!(m->module_needs))
			continue;
		needs = m->module_needs;

		while (*needs != NULL)
		{
			if (strcmp(module_name, *needs) == 0)
			{
				F_("Module \"%s\" needed by \"%s\"\n", module_name,
				   m->module_name);
				retval = TRUE;
			}
			needs++;
		}
	}
	return retval;
}

/*
 * Invoke callback, clean up module.
 */
static void initng_unload_module(m_h * module)
{
	assert(module != NULL);

	/* run the unload hook */
	(*module->module_unload) ();

	/* close and free the module entry in db */
	initng_load_module_close_and_free(module);
}

/* XXX: more information! */
int initng_unload_module_named(const char *name)
{
	m_h *m = NULL;

	assert(name != NULL);

	D_("initng_load_module_named(%s);\n", name);




	if (!initng_load_module_is_loaded(name))
	{
		F_("Not unloading module \"%s\", it is not loaded\n", name);
		return FALSE;
	}


	/* find the named module in our linked list */

	while_module_db(m)
	{
		if (strcmp(m->module_name, name) == 0)
		{
			m->marked_for_removal = TRUE;
			g.modules_to_unload = TRUE;
			return (TRUE);
		}
	}

	return FALSE;
}

int initng_load_module_load_all(const char *plugin_path)
{
	struct dirent **filelist;
	int files;
	int i;
	char *module_path = NULL;
	char *module_name = NULL;
	int success = FALSE;

	m_h *current, *safe = NULL;

	/* open plugin dir */
	if ((files = scandir(plugin_path, &filelist, 0, alphasort)) < 1)
	{
		F_("Unable to open plugin directory %s.\n", plugin_path);
		return (FALSE);
	}

	/* memory for full path */
	module_path = i_calloc(strlen(plugin_path) + NAME_MAX + 2, 1);

	/* get every entry */
	for (i = 0; i < files; i++)
	{
		/* check for files, ending with .so */
		if (fnmatch("lib*.so", filelist[i]->d_name, 0) == 0)
		{
			module_name = i_strndup(filelist[i]->d_name + 3,
									strlen(filelist[i]->d_name + 3) - 3);

			/* search the plugin name, for blacklisted */
			if (initng_common_service_blacklisted(module_name))
			{
				F_("Plugin %s blacklisted.\n", module_name);
				free(module_name);
				module_name = NULL;
				free(filelist[i]);
				continue;
			}

			/* set full path */
			strcpy(module_path, plugin_path);
			strcat(module_path, "/");
			strcat(module_path, filelist[i]->d_name);

			/* open the module */
			current = initng_load_module_open(module_path, module_name);
			free(module_name);				/* initng_load_module_open strdups this itself */
			module_name = NULL;

			/* add this to the list of loaded modules */
			if (!current)
			{
				free(filelist[i]);
				continue;
			}

			/* add to list and continue */
			assert(current->module_name);
			list_add(&current->list, &g.module_db.list);

			/* This is true until any plugin loads sucessfully */
			success = TRUE;
		}
		else
		{
#ifdef DEBUG
			if (filelist[i]->d_name[0] != '.')
			{
				D_("Won't load module \"%s\", doesn't match \"*.so\" pattern.\n", filelist[i]->d_name);
			}
#endif
		}
		free(filelist[i]);
	}

	free(filelist);
	free(module_path);

	/* load the entries on our TODO list */
	while_module_db_safe(current, safe)
	{
		/* already initialized */
		if (current->initziated == TRUE)
			continue;

		if (!initng_load_module_needs_are_loaded(current))
		{
			initng_load_module_close_and_free(current);
			continue;
		}

		/* if we did find find a module with needs loaded, try to load it */
		current->initziated = (*current->module_init) (API_VERSION);
		D_("for module \"%s\" return: %i\n", current->module_name,
		   current->initziated);

		/* check if it was initialized correctly */
		if (current->initziated != TRUE)
		{
			if (g.i_am == I_AM_INIT || g.i_am == I_AM_FAKE_INIT)
				F_("Module %s did not load correctly (module_init() returned %i)\n", current->module_name, current->initziated);
			initng_load_module_close_and_free(current);
		}

	}

	/*    initng_load_static_modules(); */
	return (success);
}


/* this will dlclose all arrays */
/* XXX: more information needed */

/* 
 * We can simply unload modules in order, because 
 */
void initng_unload_module_unload_all(void)
{
	m_h *m, *safe = NULL;

	while_module_db_safe(m, safe)
	{
		initng_unload_module(m);
	}

	/* reset the list, to make sure its empty */
	INIT_LIST_HEAD(&g.module_db.list);

	D_("initng_load_module_close_all()\n");
}

/* 
 * We can simply unload marked modules in order.
 */
void initng_unload_module_unload_marked(void)
{
	m_h *m, *safe = NULL;

	S_;



	while_module_db_safe(m, safe)
	{
		if (m->marked_for_removal == TRUE)
		{
			if (initng_load_module_is_needed(m->module_name))
			{
				F_("Not unloading module \"%s\", it is needed\n",
				   m->module_name);
				m->marked_for_removal = FALSE;
				continue;
			}
			D_("now unloading marked module %s.\n", m->module_name);

			initng_unload_module(m);
		}
	}
}
