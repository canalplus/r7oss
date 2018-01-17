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

#include <sys/types.h>
#include <sys/stat.h>

#include <initng_toolbox.h>
#include <initng_handler.h>
#include <initng_global.h>
#include <initng_env_variable.h>
#include <initng_event_hook.h>
#include <initng_static_event_types.h>


INITNG_PLUGIN_MACRO;

s_entry UMASK = { "umask", INT, NULL, "Set this umask on launch." };

static int do_umask(s_event * event)
{
    s_event_after_fork_data * data = NULL;

    assert(event->event_type = &EVENT_AFTER_FORK);
    assert(event->data);

    data = event->data;

    assert(data->service);
    assert(data->service->name);

    {
       int value8 = 0;
       int value10 = get_int(&UMASK, data->service);
       int r = 0;
       int c = 0;

       if (!is(&UMASK, data->service)) {
           D_("umask not set");
           return (TRUE);
       }

       value10 = get_int(&UMASK, data->service);

       D_("Service %s umask(decimal) %d", value10);
       
       if (value10<0 || 7777<value10) {
           F_("Invalid umask value %d for service %s", value10, data->service->name);
           return (FALSE);
       }

       // the umask value in .i file is interpreted as decimal int, while it is meant to be octal 
       // so adjust the read decimal value to the correct octal interpretation
       while(0<value10) {
           r = value10 % 10;
           if (7<r) {
               F_("Invalid umask value %d for service %s", value10, data->service->name);
               return (FALSE);
           }
           value10 /= 10;

           value8 += r<<(3*c);
           c++;
       }

       D_("Service %s umask(octal) %o", value8);
       umask(value8);
    }

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

	initng_service_data_type_register(&UMASK);
	return (initng_event_hook_register(&EVENT_AFTER_FORK, &do_umask));
}

void module_unload(void)
{
	D_("module_unload();\n");
	initng_service_data_type_unregister(&UMASK);
	initng_event_hook_unregister(&EVENT_AFTER_FORK, &do_umask);
}
