/*
 * get_bus_devices_list.c
 *
 * Utility to get the list of devices on a given bus
 *
 * Copyright (C) IBM Corp. 2003
 *
 *      This program is free software; you can redistribute it and/or modify it
 *      under the terms of the GNU General Public License as published by the
 *      Free Software Foundation version 2 of the License.
 *
 *      This program is distributed in the hope that it will be useful, but
 *      WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "libsysfs.h"

static void print_usage(void)
{
        fprintf(stdout, "Usage: get_bus_devices_list [bus]\n");
}

int main(int argc, char *argv[])
{
	struct dlist *name = NULL;
	char *cur = NULL;

	if (argc != 2) {
		print_usage();
		return 1;
	}
	name = sysfs_open_bus_devices_list(argv[1]);
	if (name != NULL) {
		fprintf(stdout, "Devices on bus \"%s\":\n", argv[1]);
		dlist_for_each_data(name, cur, char) {
			fprintf(stdout, "\t%s\n", cur);
		}
	} else
		fprintf(stdout, "Bus \"%s\" not found\n", argv[1]);
	sysfs_close_list(name);

	return 0;
}

