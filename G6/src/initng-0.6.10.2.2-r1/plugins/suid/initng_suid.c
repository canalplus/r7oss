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

#include <initng.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pwd.h>
#include <grp.h>

#include <initng_toolbox.h>
#include <initng_handler.h>
#include <initng_global.h>
#include <initng_env_variable.h>
#include <initng_event_hook.h>
#include <initng_static_event_types.h>


INITNG_PLUGIN_MACRO;

s_entry SUID = { "suid", STRING, NULL, "Set this user id on launch." };
s_entry SGID = { "sgid", STRING, NULL, "Set this group id on launch." };

void adjust_env(active_db_h * service, const char *vn, const char *vv);
void adjust_env(active_db_h * service, const char *vn, const char *vv)
{
	/* add to service cache */
	if (is_var(&ENV, vn, service))
	{
		return;								/* Assume they were set by .i file, don't override */
	}
	else
	{
		set_string_var(&ENV, i_strdup(vn), service, i_strdup(vv));
	}
}

static int do_suid(s_event * event)
{
	s_event_after_fork_data * data;

	struct passwd *passwd = NULL;
	struct group *group = NULL;

	/* NOTE: why we have ret here? */
	int ret = TRUE;

	int i = 0;
	int gid = 0;
	int uid = 0;
	const char *tmp = NULL;
	const char *tmp2 = NULL;
	char *groupname = NULL;
	char *username = NULL;

	assert(event->event_type == &EVENT_AFTER_FORK);
	assert(event->data);

	data = event->data;

	assert(data->service);
	assert(data->service->name);

	if ((tmp = get_string(&SGID, data->service)))
		groupname = fix_variables(tmp, data->service);

	if ((tmp2 = get_string(&SUID, data->service)))
		username = fix_variables(tmp2, data->service);

	if (username && !groupname)
	{
		i = strcspn(username, ":");
		if (username[i] == ':')
		{
			groupname = strdup(username + i + 1);
			username[i] = 0;
		}
	}


	if (groupname)
		group = getgrnam(groupname);
	if (username)
		passwd = getpwnam(username);

	if (passwd)
	{
		uid = passwd->pw_uid;
		gid = passwd->pw_gid;
	}
	else if (username)
	{
		F_("USER \"%s\" not found!\n", username);
		ret += 2;
	}

	if (group)
		gid = group->gr_gid;
	else if (groupname)
	{
		F_("GROUP \"%s\" not found!\n", groupname);
		ret += 1;
	}

	if (gid)
	{
		D_("Change to gid %i", gid);
		setgid(gid);
	}

	if (passwd)
		initgroups(passwd->pw_name, passwd->pw_gid);

	if (uid)
	{
		D_("Change to uid %i", uid);
		setuid(uid);

		/* Set UID-related env variables */
		adjust_env(data->service, "USER", passwd->pw_name);
		adjust_env(data->service, "HOME", passwd->pw_dir);
		adjust_env(data->service, "PATH", "/bin:/usr/bin");
	}

	fix_free(groupname, tmp);
	fix_free(username, tmp2);
	/* group and passwd are static data structures - don't free */

	return (TRUE);
}

int module_init(int api_version)
{
	D_("module_init();\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_service_data_type_register(&SUID);
	initng_service_data_type_register(&SGID);
	return (initng_event_hook_register(&EVENT_AFTER_FORK, &do_suid));
}

void module_unload(void)
{
	D_("module_unload();\n");
	initng_service_data_type_unregister(&SUID);
	initng_service_data_type_unregister(&SGID);
	initng_event_hook_unregister(&EVENT_AFTER_FORK, &do_suid);
}
