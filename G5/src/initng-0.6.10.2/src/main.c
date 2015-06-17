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

#include <unistd.h>
#include <time.h>							/* time() */
#include <fcntl.h>							/* fcntl() */
#include <linux/kd.h>						/* KDSIGACCEPT */
#include <stdlib.h>							/* free() exit() */
#include <termios.h>
#include <stdio.h>
#include <dirent.h>
#include <fnmatch.h>
#include <errno.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/klog.h>
#include <sys/reboot.h>						/* reboot() RB_DISABLE_CAD */
#include <sys/ioctl.h>						/* ioctl() */
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/un.h>							/* memmove() strcmp() */
#include <sys/wait.h>						/* waitpid() sa */
#include <sys/mount.h>
#include <initng-paths.h>

#include "initng_global.h"
#include "initng_signal.h"
#include "initng_handler.h"
#include "initng_execute.h"					/* new_environ() */
#include "initng_active_db.h"
#include "initng_load_module.h"
#include "initng_plugin_callers.h"
#include "initng_toolbox.h"
#include "initng_interrupt.h"
#include "initng_main.h"
#include "initng_service_data_types.h"
#include "initng_fd.h"
//#include "initng_service_cache.h"
#include "initng_process_db.h"
#include "initng_global.h"
#include "initng_static_event_types.h"

#include <string.h>
#ifdef SELINUX
#include <stdlib.h>
#include <string.h>
#include <selinux/selinux.h>
#include <selinux/context.h>
#include <sepol/sepol.h>
#endif
#define TIMEOUT 60000


/*older code no longer needed on FC5 and FCX (X>=5) */
#ifdef OLDSELINUX
/* Mount point for selinuxfs. */
#define SELINUXMNT "/selinux/"
int enforcing = -1;

static int load_policy(int *enforce)
{
	int fd = -1, ret = -1;
	size_t data_size;
	int rc = 0, orig_enforce;
	struct stat sb;
	void *map, *data;
	char policy_file[PATH_MAX];
	int policy_version = 0;
	FILE *cfg;
	char buf[4096];
	int seconfig = -2;
	char *nonconst;				//Ugly hack!

	selinux_getenforcemode(&seconfig);

	mount("none", "/proc", "proc", 0, 0);
	cfg = fopen("/proc/cmdline", "r");
	if (cfg)
	{
		char *tmp;

		if (fgets(buf, 4096, cfg) && (tmp = strstr(buf, "enforcing=")))
		{
			if (tmp == buf || isspace(*(tmp - 1)))
			{
				enforcing = atoi(tmp + 10);
			}
		}
		fclose(cfg);
	}
#define MNT_DETACH 2
	umount2("/proc", MNT_DETACH);

	if (enforcing >= 0)
		*enforce = enforcing;
	else if (seconfig == 1)
		*enforce = 1;

	if (mount("none", SELINUXMNT, "selinuxfs", 0, 0) < 0)
	{
		if (errno == ENODEV)
		{
			fprintf(stderr, "SELinux not supported by kernel: %s\n",
					strerror(errno));
			*enforce = 0;
		}
		else
		{
			fprintf(stderr, "Failed to mount %s: %s\n", SELINUXMNT,
					strerror(errno));
		}
		return ret;
	}

	nonconst = malloc(sizeof(SELINUXMNT));
	strcpy(nonconst, SELINUXMNT);
	set_selinuxmnt(nonconst);				/* set manually since we mounted it */
	free(nonconst);

	policy_version = security_policyvers();
	if (policy_version < 0)
	{
		fprintf(stderr, "Can't get policy version: %s\n", strerror(errno));
		goto UMOUNT;
	}

	orig_enforce = rc = security_getenforce();
	if (rc < 0)
	{
		fprintf(stderr, "Can't get SELinux enforcement flag: %s\n",
				strerror(errno));
		goto UMOUNT;
	}
	if (enforcing >= 0)
	{
		*enforce = enforcing;
	}
	else if (seconfig == -1)
	{
		*enforce = 0;
		rc = security_disable();
		if (rc == 0)
			umount(SELINUXMNT);
		if (rc < 0)
		{
			rc = security_setenforce(0);
			if (rc < 0)
			{
				fprintf(stderr, "Can't disable SELinux: %s\n",
						strerror(errno));
				goto UMOUNT;
			}
		}
		ret = 0;
		goto UMOUNT;
	}
	else if (seconfig >= 0)
	{
		*enforce = seconfig;
		if (orig_enforce != *enforce)
		{
			rc = security_setenforce(seconfig);
			if (rc < 0)
			{
				fprintf(stderr, "Can't set SELinux enforcement flag: %s\n",
						strerror(errno));
				goto UMOUNT;
			}
		}
	}

	snprintf(policy_file, sizeof(policy_file), "%s.%d",
			 selinux_binary_policy_path(), policy_version);
	fd = open(policy_file, O_RDONLY);
	if (fd < 0)
	{
		/* Check previous version to see if old policy is available
		 */
		snprintf(policy_file, sizeof(policy_file), "%s.%d",
				 selinux_binary_policy_path(), policy_version - 1);
		fd = open(policy_file, O_RDONLY);
		if (fd < 0)
		{
			fprintf(stderr, "Can't open '%s.%d':  %s\n",
					selinux_binary_policy_path(), policy_version,
					strerror(errno));
			goto UMOUNT;
		}
	}

	if (fstat(fd, &sb) < 0)
	{
		fprintf(stderr, "Can't stat '%s':  %s\n",
				policy_file, strerror(errno));
		goto UMOUNT;
	}

	map = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
	{
		fprintf(stderr, "Can't map '%s':  %s\n",
				policy_file, strerror(errno));
		goto UMOUNT;
	}


	/* Set SELinux users based on a local.users configuration file. */
	ret = sepol_genusers(map, sb.st_size, selinux_users_path(), &data,
						 &data_size);
	if (ret < 0)
	{
		fprintf(stderr,
				"Warning!  Error while reading user configuration from %s/{local.users,system.users}:  %s\n",
				selinux_users_path(), strerror(errno));
		data = map;
		data_size = sb.st_size;
	}

	/* Set booleans based on a booleans configuration file. */
	nonconst = malloc(sizeof(selinux_booleans_path()));
	strcpy(nonconst, selinux_booleans_path());
	ret = sepol_genbools(data, data_size, nonconst);
	free(nonconst);
	if (ret < 0)
	{
		if (errno == ENOENT || errno == EINVAL)
		{
			/* No booleans file or stale booleans in the file; non-fatal. */
			fprintf(stderr, "Warning!  Error while setting booleans:  %s\n",
					strerror(errno));
		}
		else
		{
			fprintf(stderr, "Error while setting booleans:  %s\n",
					strerror(errno));
			goto UMOUNT;
		}
	}
	fprintf(stderr, "Loading security policy\n");
	ret = security_load_policy(data, data_size);
	if (ret < 0)
	{
		fprintf(stderr, "security_load_policy failed\n");
	}

  UMOUNT:
	/*umount(SELINUXMNT); */
	if (fd >= 0)
		close(fd);

	return (ret);
}
#endif


static void setup_console(void)
{
	int fd;						/* /dev/console */
	struct termios tty;

	D_("MAIN_SET_I_AM_INIT_STUFF\n");

	/* change dir to / */
	while (chdir("/") < 0)
	{
		F_("Cant chdir to \"/\"\n");
		initng_main_su_login();
	}

	/* set console loglevel */
	klogctl(8, NULL, 1);


	/* enable generation of core files */
	{
		struct rlimit c = { RLIM_INFINITY, RLIM_INFINITY };
		setrlimit(RLIMIT_CORE, &c);
	}

	reboot(RB_DISABLE_CAD);					/* Disable Ctrl + Alt + Delete */

	/* open the console */
	if (g.dev_console)
	{
		fd = open(g.dev_console, O_RDWR | O_NOCTTY);
	}
	else
	{
		fd = open(INITNG_CONSOLE, O_RDWR | O_NOCTTY);
	}

	/* Try to open the console, but don't control it */
	if (fd > 0)
	{
		D_("Opened " INITNG_CONSOLE ". Setting terminal options.\n");
		ioctl(fd, KDSIGACCEPT, SIGWINCH);	/* Accept signals from 'kbd' */
		close(fd);							/* Like Ctrl + Alt + Delete signal? */
	}
	else
	{
		D_("Failed to open " INITNG_CONSOLE ". Setting options anyway.\n");
		ioctl(0, KDSIGACCEPT, SIGWINCH);	/* Accept signals from 'kbd' */
	}

	/* TODO: this block may be incorrect or incomplete */
	/* Q: What does this block really do? */

	/*
	 * TODO: /dev/console may still be open from before. Also, if it
	 * fails to open, why try to finish the block?
	 */
	if ((fd = open("/dev/console", O_RDWR | O_NOCTTY)) < 0)
	{
		F_("main(): can't open /dev/console.\n");
	}

	(void) tcgetattr(fd, &tty);

	tty.c_cflag &= CBAUD | CBAUDEX | CSIZE | CSTOPB | PARENB | PARODD;
	tty.c_cflag |= HUPCL | CLOCAL | CREAD;

	tty.c_cc[VINTR] = 3;					/* ctrl('c') */
	tty.c_cc[VQUIT] = 28;					/* ctrl('\\') */
	tty.c_cc[VERASE] = 127;
	tty.c_cc[VKILL] = 24;					/* ctrl('x') */
	tty.c_cc[VEOF] = 4;						/* ctrl('d') */
	tty.c_cc[VTIME] = 0;
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VSTART] = 17;					/* ctrl('q') */
	tty.c_cc[VSTOP] = 19;					/* ctrl('s') */
	tty.c_cc[VSUSP] = 26;					/* ctrl('z') */

	/*
	 *      Set pre and post processing
	 */
	tty.c_iflag = IGNPAR | ICRNL | IXON | IXANY;
	tty.c_oflag = OPOST | ONLCR;
	tty.c_lflag = ISIG | ICANON | ECHO | ECHOCTL | ECHOPRT | ECHOKE;

	/*
	 *      Now set the terminal line.
	 *      We don't care about non-transmitted output data
	 *      and non-read input data.
	 */
	(void) tcsetattr(fd, TCSANOW, &tty);
	(void) tcflush(fd, TCIOFLUSH);
	(void) close(fd);

}


/*
 * %%%%%%%%%%%%%%%%%%%%   main ()   %%%%%%%%%%%%%%%%%%%%
 */
#ifdef BUSYBOX
int init_main(int argc, char *argv[], char *env[])
{
#else
int main(int argc, char *argv[], char *env[])
{
#endif
	struct timespec last;		/* save the time here */
	int retval;

#ifdef DEBUG
	int loop_counter = 0;		/* counts how many times the main_loop has run */
#endif
	S_;

	/* maby initng is launched only for getting the version */
	if (argv[1] && strcmp(argv[1], "--version") == 0)
	{
		fprintf(stderr, INITNG_VERSION "\n");
		usleep(100);
		fprintf(stdout, "api_ver=%i.\n", API_VERSION);
		fprintf(stdout, "created by " INITNG_CREATOR "\n\n");
		exit(0);
	}

	/* get the time */
	clock_gettime(CLOCK_MONOTONIC, &last);

	/* initialize global variables */
	initng_global_new(argc, argv, env);

	if (getuid() != 0)
	{
		W_("Initng is designed to run as root user, a lot of functionality will not work correctly.\n");
	}

	/* if this is real init */
	if (g.i_am == I_AM_INIT)
	{
		/*load selinux policy and rexec*/
#ifdef SELINUX
		FILE *tmp_f;
		if ((tmp_f = fopen("/selinux/enforce", "r")) != NULL)
		{
			fclose(tmp_f);
			goto BOOT;
		}

		int enforce = -1;
		char *nonconst;

		if (getenv("SELINUX_INIT") == NULL)
		{
			nonconst = malloc(sizeof("SELINUX_INIT=YES"));
			strcpy(nonconst, "SELINUX_INIT=YES");
			putenv(nonconst);
			free(nonconst);
#ifdef OLDSELINX
			if (load_policy(&enforce) == 0)
#else
			if (selinux_init_load_policy(&enforce) == 0)
#endif
			{
				execv(g.Argv[0], g.Argv);
			}
			else
			{
				if (enforce > 0)
				{
					/* SELinux in enforcing mode but load_policy failed */
					/* At this point, we probably can't open /dev/console, so log() won't work */
					fprintf(stderr,
						"Enforcing mode requested but no policy loaded. Halting now.\n");
					exit(1);
				}
			}
		}

		BOOT:
#endif

		/* when last service stopped, offer a sulogin */
		g.when_out = THEN_REBOOT;
		if (!g.runlevel)
			initng_main_set_runlevel(RUNLEVEL_PREFIX "suspend");

		if (!g.hot_reload)
		{
			/* static function above, initialize the system */
			setup_console();
		}

	}
	else if (g.i_am == I_AM_FAKE_INIT)
	{
		/* when last service stopped, quit initng */
		W_("Initng is running in fake-mode, fake-default runlevel will be started instead.\n");
		g.when_out = THEN_QUIT;
		initng_main_set_runlevel(RUNLEVEL_PREFIX "fake-default");
	}

	D_("MAIN_LOAD_MODULES\n");
	/* Load modules, if fails - launch sulogin and then try again */
	if (!initng_load_module_load_all(INITNG_PLUGIN_DIR)
		&& !initng_load_module_load_all("/lib/initng")
		&& !initng_load_module_load_all("/lib32/initng")
		&& !initng_load_module_load_all("/lib64/initng"))
	{
		/* if we can't load modules, revert to single-user login */
		initng_main_su_login();
	}

	if (g.hot_reload)
	{
		/* Mark service as up */
		retval = initng_plugin_callers_reload_active_db();
		if (retval != TRUE)
		{
			if (retval == FALSE)
				F_("No plugin handled hot_reload!\n");
			else
				F_("Hot reload failed!\n");

			/* if the hot reload failed, we're probably in big trouble */
			initng_main_su_login();
		}

		/* Hopefully no-one will try a hot reload when the system isn't up... */
		initng_main_set_sys_state(STATE_UP);
	}

	/* set title of initng, can be watched in ps -ax */
	initng_toolbox_set_proc_title("initng [%s]", g.runlevel);

	/* Configure signal handlers */
	(void) initng_signal_enable();


	/* make sure this is not a hot reload */
	if (!g.hot_reload)
	{
		/* change system state */
		initng_main_set_sys_state(STATE_STARTING);

		/* first start all services, prompted at boot with +daemon/test */
		initng_main_start_extra_services();

		/* try load the default service, if it fails - launch sulogin and try again */
		if (!initng_handler_start_new_service_named(g.runlevel))
		{
			F_("Failed to load runlevel (%s)!\n", g.runlevel);
			initng_main_su_login();
		}
	}

	D_("MAIN_GOING_MAIN_LOOP\n");
	/* %%%%%%%%%%%%%%%   MAIN LOOP   %%%%%%%%%%%%%%% */
	for (;;)
	{
		D_("MAIN_LOOP: %i\n", loop_counter++);	
		int interrupt = FALSE;

		/* Update current time, save this in global so we don't need to call time() that often. */
		clock_gettime(CLOCK_MONOTONIC, &g.now);

		/*
		 * If it has gone more then 1h since last
		 * main loop, or if clock has gone backwards,
		 * reset it.
		 */
		if ((g.now.tv_sec - last.tv_sec) >= 3600
			|| (g.now.tv_sec - last.tv_sec) < 0)
		{
			D_(" Clock skew, time have changed over one hour, in one mainloop, compensating %i seconds.\n", (g.now.tv_sec - last.tv_sec));
			initng_active_db_compensate_time(g.now.tv_sec - last.tv_sec);
			initng_plugin_callers_compensate_time(g.now.tv_sec - last.tv_sec);
			last.tv_sec += (g.now.tv_sec - last.tv_sec);
		}
		/* put last time, to current time last = g.now; */
		memcpy(&last, &g.now, sizeof(struct timespec));

		/* if a sleep is set */
		g.sleep_seconds = 0;

		/* run state alarm if timeout */
		/*
		 * Run all alarm state callers.
		 * If there is more later alarms, this will set new ones.
		 */
		if (g.next_alarm && g.next_alarm <= g.now.tv_sec)
			initng_handler_run_alarm();

		/* handle signals */
		{
			int i;

			for (i = 0; i < SIGNAL_STACK; i++)
			{
				if (signals_got[i] == -1)
					continue;

				initng_plugin_callers_signal(signals_got[i]);

				switch (signals_got[i])
				{
						/* dead children */
					case SIGCHLD:
						initng_signal_handle_sigchild();
						break;
					case SIGALRM:
						/*initng_plugin_callers_alarm(); */
						initng_handler_run_alarm();
						break;
					default:
						break;
				}
				/* reset the signal */
				signals_got[i] = -1;
			}

		}


		/* If there is modules to unload, handle this */
		if (g.modules_to_unload == TRUE)
		{
			g.modules_to_unload = FALSE;
			D_("There is modules to unload!\n");
			initng_unload_module_unload_marked();
		}


		/*
		 * After here i put things that should be done every main
		 * loop, g.interrupt or not ...
		 */

		/*
		 * Trigger MAIN_EVENT event.
		 * OBS! This part in mainloop is expensive.
		 */
		{
			s_event event;
			event.event_type = &EVENT_MAIN;
			initng_event_send(&event);
		}


		/*
		 * Check if there are any running processes left, otherwise quit, when_out try to do something when this happens.
		 *
		 * This check is also expensive.
		 */
		if (initng_main_ready_to_quit() == TRUE)
		{
			initng_main_set_sys_state(STATE_ASE);
			P_(" *** Last service has quit. ***\n");
			initng_main_when_out();
			continue;
		}

		/*
		 * Run this to clean the active_db out from down services.
		 */
		initng_active_db_clean_down();

		/* LAST: check for service state changes in a special function */
		interrupt = initng_interrupt();

		/* figure out how long we can sleep */
		{
			/* set it to default */
			int closest_timeout = TIMEOUT;

			/* if sleepseconds are set */
			if (g.sleep_seconds && g.sleep_seconds < closest_timeout)
				closest_timeout = g.sleep_seconds;

			/* Check how many seconds there are to the state alarm */
			if (g.next_alarm && g.next_alarm > g.now.tv_sec)
			{
				int time_to_next = g.next_alarm - g.now.tv_sec;

				D_("g.next_alarm is set!, will trigger in %i sec.\n",
				   time_to_next);
				if (time_to_next < closest_timeout)
					closest_timeout = time_to_next;
			}

			/* if we got some seconds */
			if (closest_timeout > 0 && interrupt == FALSE)
			{
				/* do a poll == the same as sleep but also watch fds */
				D_("Will sleep for %i seconds.\n", closest_timeout);
				initng_fd_plugin_poll(closest_timeout);
			}
		}
	}										/* End main loop */
}
