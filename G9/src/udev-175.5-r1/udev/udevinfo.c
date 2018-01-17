/*
 * Copyright (C) 2007-2009 Kay Sievers <kay.sievers@vrfy.org>
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "udev.h"

static bool debug;

static void log_fn(struct udev *udev, int priority,
		   const char *file, int line, const char *fn,
		   const char *format, va_list args)
{
	if (debug) {
		fprintf(stderr, "%s: ", fn);
		vfprintf(stderr, format, args);
	} else {
		va_list args2;

		va_copy(args2, args);
		vfprintf(stderr, format, args2);
		va_end(args2);
		vsyslog(priority, format, args);
	}
}

static int run_command(struct udev *udev, const struct udevadm_cmd *cmd, int argc, char *argv[])
{
	if (cmd->debug) {
		debug = true;
		if (udev_get_log_priority(udev) < LOG_INFO)
			udev_set_log_priority(udev, LOG_INFO);
	}
	info(udev, "calling: %s\n", cmd->name);
	return cmd->cmd(udev, argc, argv);
}

int main(int argc, char *argv[])
{
	struct udev *udev;
	unsigned int i;
	int rc = 1;

	udev = udev_new();
	if (udev == NULL)
		goto out;

	udev_log_init("udevadm");
	udev_set_log_fn(udev, log_fn);
	udev_selinux_init(udev);

	info(udev, "runtime dir '%s'\n", udev_get_run_path(udev));

	rc = run_command(udev, &udevadm_info, argc, argv);
	goto out;

	fprintf(stderr, "missing or unknown command\n\n");
	adm_help(udev, argc, argv);
	rc = 2;
out:
	udev_selinux_exit(udev);
	udev_unref(udev);
	udev_log_close();
	return rc;
}
