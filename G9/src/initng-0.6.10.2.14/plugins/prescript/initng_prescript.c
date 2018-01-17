#include <initng.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_toolbox.h>
#include <initng_static_data_id.h>
#include <initng_static_states.h>
#include <initng_env_variable.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

#if 0
#undef D_
static int _initng_error_print_debug(const char *file, const char *func, int line,
							 const char *format, ...)
{
	fprintf(stderr, "@@prescript %s: ", func);

	va_list arg;
	va_start(arg, format);
	int done = vfprintf(stderr, format, arg);
	va_end(arg);

	return done;
}

#define D_(fmt, ...) _initng_error_print_debug(__FILE__, (const char*)__PRETTY_FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#endif

INITNG_PLUGIN_MACRO;

s_entry NOPRESCRIPT = { "noprescript", SET, NULL,
	"If this option is set, daemon prescript script will not be generated automatically"
};

/* based on *BUGGY* src/initng_struct_data.c d_copy_all */
static void d_copy_entry(s_entry *entry, data_head * from, data_head * to)
{
	s_data *tmp = NULL;
	s_data *current = NULL;

	D_("copy %s\n", entry->opt_name);
	list_for_each_entry(current, &from->head.list, list)
	{
		if (!current->type)
			continue;

		if(current->type != entry) {
			continue;
		}

		/* allocate the new one */
		tmp = (s_data *) i_calloc(1, sizeof(s_data));

		/* copy */
		memcpy(tmp, current, sizeof(s_data));
		memset(&tmp->list, 0, sizeof(tmp->list));

		/* copy the data */
		switch (current->type->opt_type)
		{
			case STRING:
			case STRINGS:
			case VARIABLE_STRING:
			case VARIABLE_STRINGS:
				if (current->t.s) {
					tmp->t.s = i_strdup(current->t.s);
				}
				break;
			default:
				break;
		}

		/* copy variable name */
		tmp->vn = current->vn ? i_strdup(current->vn) : NULL;

		/* add to list */
		list_add(&tmp->list, &to->head.list);
	}
}

static int can_read(const char *path)
{
	return (access(path, F_OK|R_OK) != -1);
}

static int is_file(const char *path)
{
	struct stat sb;
	if(stat(path, &sb) == 0) {
		mode_t mode = sb.st_mode & S_IFMT;
		return S_ISREG(mode) && can_read(path);
	}
	return 0;
}

#ifdef STARTERS
static char * generate_script_from_dir(const char *path)
{
	char *str_buffer = NULL;
	char *starter_file_name = NULL;
	asprintf(&starter_file_name, "%s/starter", path);
	if(is_file(starter_file_name)) {
		asprintf(&str_buffer, "/bin/sh %s/starter", path);
	}
	return str_buffer;
}
#else
static char * generate_script_from_dir(const char *path)
{
	struct dirent **namelist;
	int n = scandir(path, &namelist, 0, alphasort);
	if (n < 0) {
		perror("scandir");
		return NULL;
	}

	int len = 0;
	int len_path = strlen(path) + 1 /* for '/' */;
	/*char str_prefix[] = "/bin/sh ";*/
	char str_prefix[] = "/bin/sh -e ";
	char str_suffix[] = "\n";

	int n_valid = 0;
	int i;
	/* calculate len */
	for(i = 0; i < n; i++) {
		if(namelist[i]->d_name[0] != '.' &&
				strcmp("starter", namelist[i]->d_name) != 0) {
			char *dir_file_path = NULL;
			asprintf(&dir_file_path, "%s/%s", path, namelist[i]->d_name);
			if(is_file(dir_file_path)) {
				D_("in dir: %s|%s\n", path, namelist[i]->d_name);
				len += (sizeof(str_prefix) - 1) + len_path + strlen(namelist[i]->d_name) + (sizeof(str_suffix) - 1);
				n_valid++;
			}
			if(dir_file_path) {
				free(dir_file_path);
			}
		}
	}
	len ++; /* terminal 0 */

	/* build script */
	char *str_buffer = NULL;
	if(n_valid) str_buffer = i_calloc(1, len);
	for(i = 0; i < n; i++) {
		if(n_valid && namelist[i]->d_name[0] != '.' &&
				strcmp("starter", namelist[i]->d_name) != 0) {
			strcat(str_buffer, str_prefix);
			strcat(str_buffer, path);
			strcat(str_buffer, "/");
			strcat(str_buffer, namelist[i]->d_name);
			strcat(str_buffer, str_suffix);
		}
		free(namelist[i]);
	}
	free(namelist);

	if(n_valid) {
		D_("%d, %d, script=<%s>\n", strlen(str_buffer), len, str_buffer);
	}

	return str_buffer;
}
#endif

static char * generate_script_by_service(service_cache_h *service)
{
	if(service == NULL || service->name == NULL) {
		return NULL;
	}

	const char prefix[] = "/etc/initng/prescript";

	char *path = NULL;
	asprintf(&path, "%s/%s", prefix, service->name);

	char *script = NULL;

	struct stat path_stat;
	int ret = stat(path, &path_stat);
	if(ret == 0) {
		if(S_ISDIR(path_stat.st_mode) == 1) { /* directory */
			script = generate_script_from_dir(path);
		} else { /* file */
			if(is_file(path)) {
				D_("'%s' ok\n", path);
				/*asprintf(&script, "/bin/sh %s\n", path);*/
				asprintf(&script, "%s\n", path);
			}
		}
	} else {
		/*perror("stat");*/
	}
	free(path);

	return script;
#if 0
#endif
}

#define N_ARRAY(x) (sizeof(x)/sizeof((x)[0]))

static int service_add_parse(s_event *event)
{
	if(event == NULL || event->data == NULL) {
		return FAIL;
	}

	service_cache_h *service = event->data;
	if(service->name == NULL) {
		return FAIL;
	}

	if(is(&NOPRESCRIPT, service)) {
		return TRUE;
	}

	stype_h *type_service = initng_service_type_get_by_name("service");
#ifdef STARTERS
	s_entry *script = initng_service_data_type_find("exec");
	s_entry *script_opt = initng_service_data_type_find("exec_opt");
#else
	s_entry *script = initng_service_data_type_find("script");
	s_entry *script_opt = initng_service_data_type_find("script_opt");
#endif

	/* attributes to copy to child service */
	const char *to_copy[] = {"need", "use", "require", "wait_for_dbus", "env", "stdall"};
	static s_entry* entry[N_ARRAY(to_copy)] = {0};

	{
		unsigned int i;
		for(i = 0; i < N_ARRAY(to_copy); i++) {
			if(entry[i] == NULL) {
				entry[i] = initng_service_data_type_find(to_copy[i]);
			}
		}
	}

	int ret = TRUE;
	D_("%s prescript extra-parse\n", service->name);
	char *script_str = generate_script_by_service(service);
	if(script_str != NULL) {
		char *new_service_name = NULL;
		asprintf(&new_service_name, "%s/prescript", service->name);

		if(initng_active_db_find_by_name(new_service_name)) {
			/* already exists */
		} else {
			/* new prescript service cache entry */
			service_cache_h *service_new = initng_service_cache_new(new_service_name, type_service);
			D_("service_new = %p\n", service_new);

			if(service_new) {
				/* set prescript service attributes */
				set_string_var(script, i_strdup("start"), service_new, script_str);
				set_string_var(script_opt, i_strdup("start"), service_new, i_strdup("-e"));
				script_str = NULL; /* don't free script_str */

				unsigned int i;
				for(i = 0; i < N_ARRAY(to_copy); i++) {
					if(entry[i] != NULL) {
						d_copy_entry(entry[i], &service->data, &service_new->data);
					}
				}

				/* add new prescript service cache entry to cache */
				initng_service_cache_register(service_new);

				/* load prescript service to active db */
				active_db_h *active = initng_common_load_to_active(new_service_name);

				/* add prescript service to original service dependencies */
				D_("active = %p\n", active);
				set_another_string(&NEED, service, i_strdup(new_service_name));
			} else {
				ret = FAIL;
			}
		}
		if(script_str) {
			free(script_str);
			script_str = NULL;
		}
		free(new_service_name);
		new_service_name = NULL;
	}

	return ret;
}


int module_init(int api_version)
{
	D_("module_init();\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return FALSE;
	}

	initng_service_data_type_register(&NOPRESCRIPT);
	initng_event_hook_register(&EVENT_ADDITIONAL_PARSE, &service_add_parse);
	return TRUE;
}

void module_unload(void)
{
	D_("module_unload();\n");
	initng_service_data_type_unregister(&NOPRESCRIPT);
	initng_event_hook_unregister(&EVENT_ADDITIONAL_PARSE, &service_add_parse);
}
