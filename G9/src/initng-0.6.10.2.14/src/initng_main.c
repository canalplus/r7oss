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

#include "initng.h"

#include <time.h>							/* time() */
#include <fcntl.h>							/* fcntl() */
#include <sys/un.h>							/* memmove() strcmp() */
#include <sys/wait.h>						/* waitpid() sa */
#include <linux/kd.h>						/* KDSIGACCEPT */
#include <sys/ioctl.h>						/* ioctl() */
#include <stdlib.h>							/* free() exit() */
#include <sys/reboot.h>						/* reboot() RB_DISABLE_CAD */
#include <sys/mount.h>
#include <termios.h>
#include <stdio.h>
#include <sys/klog.h>
#include <errno.h>
#ifdef SELINUX
#include <selinux/selinux.h>
#include <selinux/get_context_list.h>
#endif
#include "initng_global.h"
#include "initng_signal.h"
#include "initng_handler.h"
#include "initng_execute.h"
#include "initng_active_db.h"
#include "initng_load_module.h"
#include "initng_plugin_callers.h"
#include "initng_toolbox.h"
#ifdef HAVE_COREDUMPER
#include <google/coredumper.h>
#endif


#include "initng_main.h"
/*extern char **environ; */

static int local_sulogin_count = 0;
static void initng_hard(h_then t);

/*
 * Poll if there are any processes left, and if not quit initng
 */
int initng_main_ready_to_quit(void)
{
	active_db_h *current = NULL;

	/* If the last process has died, quit initng */
	while_active_db(current)
	{
		/* Don't check with failed services */
		if (IS_FAILED(current))
			continue;

		/* if its down, its nothing to count on */
		if (IS_DOWN(current))
			continue;

		/*
		 * if we got here,
		 * its must be an non failing service,
		 * left.
		 */
		return (FALSE);
	}

	return (TRUE);
}

/* this is called when there is no processes left */
void initng_main_when_out(void)
{

	int failing = 0;
	active_db_h *current = NULL;

	while_active_db(current)
	{
		if (IS_FAILED(current))
		{
			failing++;
			printf("\n [%i] service \"%s\" marked \"%s\"\n", failing,
				   current->name, current->current_state->state_name);
		}
	}
	if (failing > 0)
	{
		printf("\n\n All %i services listed above, are marked with a failure.\n", failing);
/* 		printf(" Will sleep for 15 seconds before reboot/halt so you can see them.\n\n"); */
/* 		sleep(15); */
	}



	if (g.i_am == I_AM_INIT && getpid() != 1)
	{
		F_("I AM NOT INIT, THIS CANT BE HAPPENING!\n");
		sleep(3);
		return;
	}

	/* always good to do */
	sync();

	/* none of these calls should return, so the su_login on the end will be a fallback */
	switch (g.when_out)
	{
		case THEN_QUIT:
			P_(" ** Now Quiting **\n");
			initng_main_exit(0);
			break;
		case THEN_SULOGIN:
			P_(" ** Now SuLogin\n");
			/* break here leads to su_login below */
			break;
		case THEN_RESTART:
			P_(" ** Now restarting\n");
			initng_main_restart();
			break;
		case THEN_NEW_INIT:
			P_(" ** Launching new init\n");
			initng_main_new_init();
			break;
		case THEN_REBOOT:
		case THEN_HALT:
		case THEN_POWEROFF:
			initng_hard(g.when_out);
			break;
	}

	/* fallback */
	/* initng_main_su_login(); */
}

static void initng_hard(h_then t)
{
#ifdef CHECK_RO
	FILE *test;
#endif
	int pid;
	int i = 0;

	/* set the sys state */
	switch (t)
	{
		case THEN_REBOOT:
			initng_main_set_sys_state(STATE_REBOOT);
			break;
		case THEN_HALT:
			initng_main_set_sys_state(STATE_HALT);
			break;
		case THEN_POWEROFF:
			initng_main_set_sys_state(STATE_POWEROFF);
			break;
		default:
			F_("initng_hard can only handle STATE_REBOOT, STATE_POWEROFF, or STATE_HALT for now.\n");
			return;
	}


	/* sync data to disk */
	sync();

	/* make sure we are root */
	if (getuid() != 0)
		return;

	/* unload all modules (plugins) found */
	initng_unload_module_unload_all();

	/* make sure all fds but stdin, stdout, stderr is closed */
	/*produce sgfault on our targets
	 * our target have a "ro" rootfs
	for (i = 3; i <= 1013; i++)
	{
		close(i);
	}*/

	/* last sync data to disk */
	sync();

#ifdef CHECK_RO

	/* Mount readonly, youst to be extra sure this is done */
	mount("/dev/root", "/", NULL, MS_RDONLY + MS_REMOUNT, NULL);

	if (errno == EBUSY)
	{
		F_("Failed to remount / ro, EBUSY\n");
	}

	/* check so that / is mounted read only, by trying to open a file */
	if ((test = fopen("/initng_write_testfile", "w")) != NULL)
	{
		fclose(test);
		unlink("/initng_write_testfile");
		F_("/ IS NOT REMOUNTED READ-ONLY, WONT REBOOT/HALT BECAUSE THE FILE SYSTEM CAN BREAK!\n");
		return;
	}

#endif

	/* Under certain unknown circumstances, calling reboot(RB_POWER_OFF) from
	   pid 1 leads to a "Kernel panic - not syncing: Attempted to kill init!".
	   Workaround is to fork a child to do it. See bug #488 for details */
	pid = fork();

	/* if succeded (pid==0) or failed (pid < 0) */
	if (pid <= 0)
	{
		/* child process - shut down the machine */
		switch (t)
		{
			case THEN_REBOOT:
				reboot(RB_AUTOBOOT);
				break;
			case THEN_HALT:
				reboot(RB_HALT_SYSTEM);
				break;
			case THEN_POWEROFF:
				reboot(RB_POWER_OFF);
				break;
			default:
				/* What to do here */
				break;
		}

		/* if in fork, quit it */
		if (pid == 0)
			_exit(0);
	}

	/* parent process waits for child to exit */
	if (pid > 0)
		waitpid(pid, NULL, 0);

	/* idle forever */
	while (1)
		sleep(1);
}

#define TRY_TIMES 2
/*
 *  Launch sulogin and wait it for finish
 */
void initng_main_su_login(void)
{
	pid_t sulogin_pid;
	int status;

	return ;
#ifdef SELINUX
	if (is_selinux_enabled > 0)
	{
		security_context_t *contextlist = NULL;

		if (get_ordered_context_list("root", 0, &contextlist) > 0)
		{
			if (setexeccon(contextlist[0]) != 0)
				fprintf(stderr, "setexeccon failed\n");
			freeconary(contextlist);
		}
	}
#endif
	/* sulogin nicely 2 times */
	if (local_sulogin_count <= TRY_TIMES)
	{
		printf("This is a sulogin offer,\n"
			   "you will be able to login for %i times (now %i),\n"
			   "and on return initng will try continue where it was,\n"
			   "if the times go out, initng will exit on next su_login request.\n\n",
			   TRY_TIMES, local_sulogin_count);
		sulogin_pid = fork();

		if (sulogin_pid == 0)
		{
			const char *sulogin_argv[] = { "/sbin/sulogin", NULL };
			const char *sulogin_env[] = { NULL };
			int i = 0;

			/* make sure all fds but stdin, stdout, stderr is closed */
			for (i = 3; i <= 1013; i++)
			{
				close(i);
			}

			/* launch sulogin */
			execve(sulogin_argv[0], (char **) sulogin_argv,
				   (char **) sulogin_env);

			printf("Unable to execute /sbin/sulogin!\n");
			_exit(1);
		}

		if (sulogin_pid > 0)
		{
			do
			{
				sulogin_pid = waitpid(sulogin_pid, &status, WUNTRACED);
			}
			while (!WIFEXITED(status) && !WIFSIGNALED(status));

			/* increase the sulogin_count */
			local_sulogin_count++;
			return;
		}
	}

	/* newer get here */
	_exit(1);
}

/*
 * Start initial services
 */
void initng_main_start_extra_services(void)
{
	int i;
	int a_count = 0;			/* counts orders from argv to start */

	initng_main_set_sys_state(STATE_STARTING);
	/* check with argv which service to start initiating */
	for (i = 1; i < g.Argc; i++)
	{
		/* start all services that is +service */
		if (g.Argv[i][0] != '+')
			continue;

		/* if succeed to load this service */
		if (initng_handler_start_new_service_named(g.Argv[i] + 1))
			a_count++;
		else
			F_(" Requested service \"%s\", could not be executed!\n",
			   g.Argv[i]);
	}
}

/* This is same as execve() */
void initng_main_new_init(void)
{
	int i;

	initng_main_set_sys_state(STATE_EXECVE);
	for (i = 3; i <= 1013; i++)
	{
		close(i);
	}
	if (!g.new_init || !g.new_init[0])
	{
		F_(" g.new_init is not set!\n");
		return;
	}
	P_("\n\n\n          Launching new init (%s)\n\n", g.new_init[0]);
	execve(g.new_init[0], g.new_init, environ);
}


void initng_main_restart(void)
{
	char **argv = NULL;
	int i;

	initng_main_set_sys_state(STATE_RESTART);
	for (i = 3; i <= 1013; i++)
	{
		close(i);
	}
	argv = (char **) i_calloc(3, sizeof(char *));

	argv[0] = (char *) i_calloc(strlen(g.runlevel) + 12, sizeof(char));
	strcpy(argv[0], "runlevel=");
	strcat(argv[0], g.runlevel);
	argv[1] = NULL;
	P_("\n\n\n          R E S T A R T I N G,  (Really hot reboot)\n\n");
	execve("/sbin/initng", argv, environ);
}

/* this function sets g.sys_state, and call plug-ins that listen on its change */
void initng_main_set_sys_state(h_sys_state state)
{
	/* don't set a state that is */
	if (state == g.sys_state)
		return;

	D_("set_sys_state(): %% Setting state to: %i %% \n", state);
	g.sys_state = state;

	/*
	 * execute all functions in modules that want to
	 * be executed when system state change occurs.
	 */

	initng_plugin_callers_load_module_system_changed(state);
	return;
}


void initng_main_exit(int i)
{
	D_("exit_initng();\n");

	/* First set the system state to exit */
	initng_main_set_sys_state(STATE_EXIT);

	/* Free all global variables */
	initng_global_free();

	/* Then, unload all modules */
	initng_unload_module_unload_all();

	/* And exit with return code */
	exit(i);
}

void initng_main_set_runlevel(const char *runlevel)
{
	/* first free the old_runlevel */
	if (g.old_runlevel)
	{
		free(g.old_runlevel);
		g.old_runlevel = NULL;
	}

	/* then move the last new, to the old */
	if (g.runlevel)
	{
		g.old_runlevel = g.runlevel;
		g.runlevel = NULL;
	}

	/* and set the new */
	g.runlevel = i_strdup(runlevel);

	/* call system state plugins on a change like this */
	initng_plugin_callers_load_module_system_changed(g.sys_state);
}
