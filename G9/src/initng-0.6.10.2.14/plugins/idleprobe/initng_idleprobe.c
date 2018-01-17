/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 * Copyright (C) 2005 neuron <neuron@hollowtube.mine.nu>
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
#include <stdlib.h>							/* free() exit() */
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_toolbox.h>
#include <initng_static_data_id.h>
#include <initng_static_states.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>
#include <initng_fd.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <time.h>

INITNG_PLUGIN_MACRO;

typedef unsigned long long ull_t;
typedef unsigned long long ll_t;


static FILE *fp_proc = NULL;


static int is_cpu_idle(int wait);

#define DEFAULT_IDLE 10

/* system have to be idle for 10 seconds */
s_entry WAIT_FOR_CPU_IDLE = { "wait_for_cpu_idle", SET, NULL,
	"If this is set, initng will wait until CPU has been idling for 10 seconds before starting."
};

/* set the seconds system have to be idle */
s_entry WAIT_FOR_CPU_COUNT = { "wait_for_cpu_below", INT, NULL,
	"Sets the seconds system have to idle, before this service is started."
};

ull_t old_cpu_user = 0;
ull_t old_cpu_nice = 0;
ull_t old_cpu_system = 0;
ull_t old_cpu_idle = 0;
ull_t old_cpu_dummy1 = 0;
ull_t old_cpu_dummy2 = 0;
ull_t old_cpu_dummy3 = 0;
time_t last = 0;

static int is_cpu_idle(int wait)
{
	static ull_t cpu_user, cpu_nice, cpu_system, cpu_idle;
	static ull_t cpu_dummy1, cpu_dummy2, cpu_dummy3;
	static ll_t cpu_user_frm, cpu_nice_frm, cpu_system_frm, cpu_idle_frm;
	static ll_t cpu_dummy1_frm, cpu_dummy2_frm, cpu_dummy3_frm, cpu_tot_frm;

	static char buf[256 + 64];
	static int idle_cnt = 0;

	float scale;

	D_("is_cpu_idle(%i);\n", wait);
	/* check so that wait value is correct */
	if (wait < 0)
	{
		F_("Wrong value for parameter wait_for_cpu_idle (%i)!\n", wait);
		return (FALSE);
	}

	/* Make sure this is maximum run once every second, and mainloop */
	if (g.now.tv_sec == last)
	{
		/* make sure rerun interrupt again in 1 second */
		initng_global_set_sleep(1);
		return (FALSE);
	}

	/* update last run counter */
	last = g.now.tv_sec;

	/* if fp_proc not open, try to open it */
	if (!fp_proc)
	{
		fp_proc = fopen("/proc/stat", "r");
		initng_fd_set_cloexec(fileno(fp_proc));
	}

	/* if still not open, return false */
	if (!fp_proc)
		return (FALSE);


	rewind(fp_proc);
	fflush(fp_proc);

	// first value the CPU summary line
	if (!fgets(buf, sizeof(buf), fp_proc))
	{
		F_("Failed to read from /proc/stat!\n");
		return (FALSE);
	}

	/* okay, lets start parse /proc/stat */
	if (sscanf(buf, "cpu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
			   &cpu_user,
			   &cpu_nice,
			   &cpu_system,
			   &cpu_idle, &cpu_dummy1, &cpu_dummy2, &cpu_dummy3) < 4)
	{
		F_("Failed to read from /proc/stat!\n");
		return (FALSE);
	}

	D_("cpu_user: %i\n", (int) cpu_user);
	D_("cpu_nice: %i\n", (int) cpu_nice);
	D_("cpu_system: %i\n", (int) cpu_system);
	D_("cpu_idle: %i\n\n", (int) cpu_idle);

	cpu_user_frm = cpu_user - old_cpu_user;
	cpu_nice_frm = cpu_nice - old_cpu_nice;
	cpu_system_frm = cpu_system - old_cpu_system;
	cpu_idle_frm = cpu_idle > old_cpu_idle ? cpu_idle - old_cpu_idle : 0;
	cpu_dummy1_frm = cpu_dummy1 - old_cpu_dummy1;
	cpu_dummy2_frm = cpu_dummy2 - old_cpu_dummy2;
	cpu_dummy3_frm = cpu_dummy3 - old_cpu_dummy3;
	cpu_tot_frm = cpu_user_frm + cpu_nice_frm + cpu_system_frm + cpu_idle_frm + cpu_dummy1_frm + cpu_dummy2_frm + cpu_dummy3_frm;
	if (cpu_tot_frm < 1)
		cpu_tot_frm = 1;
	scale = 100.0 / (float) cpu_tot_frm;

	if (old_cpu_user)
	{
		if ((float) cpu_idle_frm * scale > 90.0)
		{
			D_("idle_cnt: %i\n", idle_cnt);
			if (++idle_cnt > wait)
				return (TRUE);
		}
		else
		{
			idle_cnt = 0;
		}
	}

	old_cpu_user = cpu_user;
	old_cpu_nice = cpu_nice;
	old_cpu_system = cpu_system;
	old_cpu_idle = cpu_idle;
	old_cpu_dummy1 = cpu_dummy1;
	old_cpu_dummy2 = cpu_dummy2;
	old_cpu_dummy3 = cpu_dummy3;

	/* make sure this will at least be run in one second */
	initng_global_set_sleep(1);

	return (FALSE);
}

static int check_cpu_idle(s_event * event)
{
	S_;
	active_db_h * service;
	int value = 0;

	assert(event->event_type == &EVENT_START_DEP_MET);
	assert(event->data);

	service = event->data;

	if ((value = get_int(&WAIT_FOR_CPU_COUNT, service)) > 0)
	{
		if (is_cpu_idle(value) < TRUE)
			return FAIL;
	}

	if (is(&WAIT_FOR_CPU_IDLE, service))
		if (is_cpu_idle(DEFAULT_IDLE) < TRUE)
			return FAIL;

	return (TRUE);
}

int module_init(int api_version)
{
	S_;
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_service_data_type_register(&WAIT_FOR_CPU_IDLE);
	initng_service_data_type_register(&WAIT_FOR_CPU_COUNT);
	initng_event_hook_register(&EVENT_START_DEP_MET, &check_cpu_idle);
	return (TRUE);
}

void module_unload(void)
{
	S_;
	initng_service_data_type_unregister(&WAIT_FOR_CPU_IDLE);
	initng_service_data_type_unregister(&WAIT_FOR_CPU_COUNT);
	initng_event_hook_unregister(&EVENT_START_DEP_MET, &check_cpu_idle);

	if (fp_proc)
		fclose(fp_proc);
}
