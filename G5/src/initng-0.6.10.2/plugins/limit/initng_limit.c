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
#include <errno.h>
#include <initng_handler.h>
#include <initng_global.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

#include <assert.h>

#include <sys/time.h>
#include <sys/resource.h>

INITNG_PLUGIN_MACRO;

/* stolen from man setrlimit */
s_entry RLIMIT_AS_SOFT = { "rlimit_as_soft", INT, NULL,
	"The maximum size of process virtual memory, soft limit."
};
s_entry RLIMIT_AS_HARD = { "rlimit_as_hard", INT, NULL,
	"The maximum size of process virtual memory, hard limit."
};
s_entry RLIMIT_CORE_SOFT = { "rlimit_core_soft", INT, NULL,
	"The maximum size of the core file, soft limit."
};
s_entry RLIMIT_CORE_HARD = { "rlimit_core_hard", INT, NULL,
	"The maximum size of the core file, hard limit."
};
s_entry RLIMIT_CPU_SOFT = { "rlimit_cpu_soft", INT, NULL,
	"The maximum of cputime, in seconds processes can use, soft limit."
};
s_entry RLIMIT_CPU_HARD = { "rlimit_cpu_hard", INT, NULL,
	"The maximum of cputime, in seconds processes can use, hard limit."
};
s_entry RLIMIT_DATA_SOFT = { "rlimit_data_soft", INT, NULL,
	"The maximum size of the process uninitalized data, soft limit."
};
s_entry RLIMIT_DATA_HARD = { "rlimit_data_hard", INT, NULL,
	"The maximum size of the process uninitalized data, hard limit."
};
s_entry RLIMIT_FSIZE_SOFT = { "rlimit_fsize_soft", INT, NULL,
	"The maximum filesize of a file the process can create, soft limit."
};
s_entry RLIMIT_FSIZE_HARD = { "rlimit_fsize_hard", INT, NULL,
	"The maximum filesize of a file the process can create, hard limit."
};
s_entry RLIMIT_MEMLOCK_SOFT = { "rlimit_memlock_soft", INT, NULL,
	"The maximum amount of memory, the process can lock, soft limit."
};
s_entry RLIMIT_MEMLOCK_HARD = { "rlimit_memlock_hard", INT, NULL,
	"The maximum amount of memory, the process can lock, hard limit."
};
s_entry RLIMIT_NOFILE_SOFT = { "rlimit_nofile_soft", INT, NULL,
	"The maximum number of files the process can open, at the same time, soft limit."
};
s_entry RLIMIT_NOFILE_HARD = { "rlimit_nofile_hard", INT, NULL,
	"The maximum number of files the process can open, at the same time, hard limit."
};
s_entry RLIMIT_NPROC_SOFT = { "rlimit_nproc_soft", INT, NULL,
	"The maximum number of processes that can be created, soft limit."
};
s_entry RLIMIT_NPROC_HARD = { "rlimit_nproc_hard", INT, NULL,
	"The maximum number of processes that can be created, hard limit."
};
s_entry RLIMIT_RSS_SOFT = { "rlimit_rss_soft", INT, NULL,
	"The maximum no of virtual pages in ram, soft limit."
};
s_entry RLIMIT_RSS_HARD = { "rlimit_rss_hard", INT, NULL,
	"The maximum no of virtual pages in ram, hard limit."
};
s_entry RLIMIT_SIGPENDING_SOFT = { "rlimit_sigpending_soft", INT, NULL,
	"The maximum amount of signals that can be queued, soft limit."
};
s_entry RLIMIT_SIGPENDING_HARD = { "rlimit_sigpending_hard", INT, NULL,
	"The maximum amount of signals that can be queued, soft limit."
};
s_entry RLIMIT_STACK_SOFT = { "rlimit_stack_soft", INT, NULL,
	"The maximum size of the process stack, hard limit."
};
s_entry RLIMIT_STACK_HARD = { "rlimit_stack_hard", INT, NULL,
	"The maximum size of the process stack, soft limit."
};

/* fetch the correct error string */
static const char *err_desc(void)
{
	switch (errno)
	{
		case EFAULT:
			return ("Rlim prints outside the accessible address space.\n");
		case EINVAL:
			return ("Resource is not valid.\n");
		case EPERM:
			return ("Unprivileged process tried to set rlimit.\n");
		default:
			break;
	}
	return (NULL);
}

/* this function set rlimit if it should, w-o overwriting old values. */
static int set_limit(s_entry * soft, s_entry * hard, active_db_h * service,
					 int ltype, int times)
{
	int si = FALSE;
	int sh = FALSE;
	struct rlimit l;

	/* check if the limits are set */
	si = is(soft, service);
	sh = is(hard, service);

	/* make sure any is set */
	if (si == FALSE && sh == FALSE)
		return (0);

	/* get the current limit data */
	if (getrlimit(ltype, &l) != 0)
	{
		F_("getrlimit failed!, service %s, limit type %i: %s\n",
		   service->name, ltype, err_desc());
		return (-1);
	}

	D_("current: soft: %i, hard: %i\n", l.rlim_cur, l.rlim_max);
	/* if soft limit is set, get it */
	if (si)
	{
		l.rlim_cur = (get_int(soft, service) * times);
	}

	/* if hard limit is set, get it */
	if (sh)
	{
		l.rlim_max = (get_int(hard, service) * times);
	}

	/* make sure hard is at least same big, but hopefully bigger. */
	if (l.rlim_cur > l.rlim_max)
		l.rlim_max = l.rlim_cur;

	D_("now: soft: %i, hard: %i\n", l.rlim_cur, l.rlim_max);

	/* set the limit and return status */
	if (setrlimit(ltype, &l) != 0)
	{
		F_("setrlimit failed, service: %s, limit type %i: %s\n",
		   service->name, ltype, err_desc());
		return (-1);
	}

	return (0);
}

static int do_limit(s_event * event)
{
	s_event_after_fork_data * data;

	int ret = 0;

	assert(event->event_type == &EVENT_AFTER_FORK);
	assert(event->data);

	data = event->data;

	assert(data->service);
	assert(data->service->name);
	assert(data->process);

	D_("do_limit!\n");

	/* Handle RLIMIT_AS */
	ret += set_limit(&RLIMIT_AS_SOFT, &RLIMIT_AS_HARD, data->service, RLIMIT_AS, 1024);

	/* Handle RLIMIT_CORE */
	ret += set_limit(&RLIMIT_CORE_SOFT, &RLIMIT_CORE_HARD, data->service, RLIMIT_CORE,
					 1024);

	/* Handle RLIMIT_CPU */
	ret += set_limit(&RLIMIT_CPU_SOFT, &RLIMIT_CPU_HARD, data->service, RLIMIT_CPU, 1);

	/* Handle RLIMIT_DATA */
	ret += set_limit(&RLIMIT_DATA_SOFT, &RLIMIT_DATA_HARD, data->service, RLIMIT_DATA,
					 1024);

	/* Handle RLIMIT_FSIZE */
	ret += set_limit(&RLIMIT_FSIZE_SOFT, &RLIMIT_FSIZE_HARD, data->service, RLIMIT_FSIZE,
					 1024);

	/* Handle RLIMIT_MEMLOCK */
	ret += set_limit(&RLIMIT_MEMLOCK_SOFT, &RLIMIT_MEMLOCK_HARD, data->service,
					 RLIMIT_MEMLOCK, 1024);

	/* Handle RLIMIT_NOFILE */
	ret += set_limit(&RLIMIT_NOFILE_SOFT, &RLIMIT_NOFILE_HARD, data->service,
					 RLIMIT_NOFILE, 1);

	/* Handle RLIMIT_NPROC */
	ret += set_limit(&RLIMIT_NPROC_SOFT, &RLIMIT_NPROC_HARD, data->service, RLIMIT_NPROC,
					 1);

	/* Handle RLIMIT_RSS */
	ret += set_limit(&RLIMIT_RSS_SOFT, &RLIMIT_RSS_HARD, data->service, RLIMIT_RSS, 1024);

#ifdef RLIMIT_SIGPENDING
	/* for some reason, this seems missing on some systems */
	/* Handle RLIMIT_SIGPENDING */
	ret += set_limit(&RLIMIT_SIGPENDING_SOFT, &RLIMIT_SIGPENDING_HARD, data->service,
					 RLIMIT_SIGPENDING, 1);
#endif

	/* Handle RLIMIT_STACK */
	ret += set_limit(&RLIMIT_STACK_SOFT, &RLIMIT_STACK_HARD, data->service, RLIMIT_STACK,
					 1024);

	/* make sure every rlimit suceeded */
	if (ret != 0)
		return (FAIL);

	/* return happily */
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

	/* Add all options to initng */
	initng_service_data_type_register(&RLIMIT_AS_SOFT);
	initng_service_data_type_register(&RLIMIT_AS_HARD);
	initng_service_data_type_register(&RLIMIT_CORE_SOFT);
	initng_service_data_type_register(&RLIMIT_CORE_HARD);
	initng_service_data_type_register(&RLIMIT_CPU_SOFT);
	initng_service_data_type_register(&RLIMIT_CPU_HARD);
	initng_service_data_type_register(&RLIMIT_DATA_SOFT);
	initng_service_data_type_register(&RLIMIT_DATA_HARD);
	initng_service_data_type_register(&RLIMIT_FSIZE_SOFT);
	initng_service_data_type_register(&RLIMIT_FSIZE_HARD);
	initng_service_data_type_register(&RLIMIT_MEMLOCK_SOFT);
	initng_service_data_type_register(&RLIMIT_MEMLOCK_HARD);
	initng_service_data_type_register(&RLIMIT_NOFILE_SOFT);
	initng_service_data_type_register(&RLIMIT_NOFILE_HARD);
	initng_service_data_type_register(&RLIMIT_NPROC_SOFT);
	initng_service_data_type_register(&RLIMIT_NPROC_HARD);
	initng_service_data_type_register(&RLIMIT_RSS_SOFT);
	initng_service_data_type_register(&RLIMIT_RSS_HARD);
	initng_service_data_type_register(&RLIMIT_SIGPENDING_SOFT);
	initng_service_data_type_register(&RLIMIT_SIGPENDING_HARD);
	initng_service_data_type_register(&RLIMIT_STACK_SOFT);
	initng_service_data_type_register(&RLIMIT_STACK_HARD);

	/* add the after fork function hook */
	initng_event_hook_register(&EVENT_AFTER_FORK, &do_limit);

	/* always return happily */
	return (TRUE);
}

void module_unload(void)
{
	D_("module_unload();\n");

	/* remove the hook */
	initng_event_hook_unregister(&EVENT_AFTER_FORK, &do_limit);

	/* Del all options to initng */
	initng_service_data_type_unregister(&RLIMIT_AS_SOFT);
	initng_service_data_type_unregister(&RLIMIT_AS_HARD);
	initng_service_data_type_unregister(&RLIMIT_CORE_SOFT);
	initng_service_data_type_unregister(&RLIMIT_CORE_HARD);
	initng_service_data_type_unregister(&RLIMIT_CPU_SOFT);
	initng_service_data_type_unregister(&RLIMIT_CPU_HARD);
	initng_service_data_type_unregister(&RLIMIT_DATA_SOFT);
	initng_service_data_type_unregister(&RLIMIT_DATA_HARD);
	initng_service_data_type_unregister(&RLIMIT_FSIZE_SOFT);
	initng_service_data_type_unregister(&RLIMIT_FSIZE_HARD);
	initng_service_data_type_unregister(&RLIMIT_MEMLOCK_SOFT);
	initng_service_data_type_unregister(&RLIMIT_MEMLOCK_HARD);
	initng_service_data_type_unregister(&RLIMIT_NOFILE_SOFT);
	initng_service_data_type_unregister(&RLIMIT_NOFILE_HARD);
	initng_service_data_type_unregister(&RLIMIT_NPROC_SOFT);
	initng_service_data_type_unregister(&RLIMIT_NPROC_HARD);
	initng_service_data_type_unregister(&RLIMIT_RSS_SOFT);
	initng_service_data_type_unregister(&RLIMIT_RSS_HARD);
	initng_service_data_type_unregister(&RLIMIT_SIGPENDING_SOFT);
	initng_service_data_type_unregister(&RLIMIT_SIGPENDING_HARD);
	initng_service_data_type_unregister(&RLIMIT_STACK_SOFT);
	initng_service_data_type_unregister(&RLIMIT_STACK_HARD);

}
