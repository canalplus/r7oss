/*
 * Copyright (C) 2006-2009 Kay Sievers <kay@vrfy.org>
 * Copyright (C) 2009 Canonical Ltd.
 * Copyright (C) 2009 Scott James Remnant <scott@netsplit.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <syslog.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "udev.h"

static int adm_settle(struct udev *udev, int argc, char *argv[])
{
	static const struct option options[] = {
		{ "timeout", required_argument, NULL, 't' },
		{ "exit-if-exists", required_argument, NULL, 'E' },
		{ "quiet", no_argument, NULL, 'q' },
		{ "help", no_argument, NULL, 'h' },
		{}
	};
	int quiet = 0;
	const char *exists = NULL;
	unsigned int timeout = 120;
	struct pollfd pfd[1];
	char queue_filename[UTIL_PATH_SIZE];
	int rc = EXIT_FAILURE;

	dbg(udev, "version %s\n", VERSION);

	for (;;) {
		int option;
		int seconds;

		option = getopt_long(argc, argv, "t:E:qh", options, NULL);
		if (option == -1)
			break;

		switch (option) {
		case 't':
			seconds = atoi(optarg);
			if (seconds >= 0)
				timeout = seconds;
			else
				fprintf(stderr, "invalid timeout value\n");
			dbg(udev, "timeout=%i\n", timeout);
			break;
		case 'q':
			quiet = 1;
			break;
		case 'E':
			exists = optarg;
			break;
		case 'h':
			printf("Usage: udevadm settle OPTIONS\n"
			       "  --timeout=<seconds>     maximum time to wait for events\n"
			       "  --exit-if-exists=<file> stop waiting if file exists\n"
			       "  --quiet                 do not print list after timeout\n"
			       "  --help\n\n");
			exit(EXIT_SUCCESS);
		default:
			exit(EXIT_FAILURE);
		}
	}

	/* Not used anymore but kept for future usage. */
	UDEV_IGNORE_VALUE(quiet);

	/* guarantee that the udev daemon isn't pre-processing */
	if (getuid() == 0) {
		struct udev_ctrl *uctrl;

		uctrl = udev_ctrl_new(udev);
		if (uctrl != NULL) {
			if (udev_ctrl_send_ping(uctrl, timeout) < 0) {
				info(udev, "no connection to daemon\n");
				udev_ctrl_unref(uctrl);
				rc = EXIT_SUCCESS;
				goto out;
			}
			udev_ctrl_unref(uctrl);
		}
	}

	pfd[0].events = POLLIN;
	pfd[0].fd = inotify_init1(IN_CLOEXEC);
	if (pfd[0].fd < 0) {
		err(udev, "inotify_init failed: %m\n");
		goto out;
	}

	util_strscpyl(queue_filename, sizeof(queue_filename), udev_get_run_path(udev), "/queue", NULL);
	if (inotify_add_watch(pfd[0].fd, queue_filename, IN_DELETE) < 0) {
		/* If it does not exist, we don't have to wait */
		if (errno == ENOENT)
			rc = EXIT_SUCCESS;
		else
			err(udev, "watching %s failed\n", queue_filename);
		goto out;
	}

	for (;;) {
		if (exists && access(exists, F_OK) == 0) {
			rc = EXIT_SUCCESS;
			break;
		}

		/* exit if queue is empty */
		if (access(queue_filename, F_OK) < 0) {
			rc = EXIT_SUCCESS;
			break;
		}

		/* wake up when "queue" file is deleted */
		if (poll(pfd, 1, 100) > 0 && pfd[0].revents & POLLIN) {
			char buf[sizeof(struct inotify_event) + PATH_MAX];
			UDEV_IGNORE_VALUE(read(pfd[0].fd, buf, sizeof(buf)));
		}
	}

out:
	if (pfd[0].fd >= 0)
		close(pfd[0].fd);
	return rc;
}

const struct udevadm_cmd udevadm_settle = {
	.name = "settle",
	.cmd = adm_settle,
	.help = "wait for pending udev events",
};
