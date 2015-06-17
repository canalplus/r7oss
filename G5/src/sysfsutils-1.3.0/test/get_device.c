/*
 * get_device.c
 *
 * Utility to get details of a given device
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
        fprintf(stdout, "Usage: get_device [bus] [device]\n");
}

int main(int argc, char *argv[])
{
	struct sysfs_device *device = NULL;
	struct sysfs_attribute *attr = NULL;
	struct dlist *attrlist = NULL;

	if (argc != 3) {
		print_usage();
		return 1;
	}

	device = sysfs_open_device(argv[1], argv[2]);
	if (device == NULL) {
		fprintf(stdout, "Device \"%s\" not found on bus \"%s\"\n",
					argv[2], argv[1]);
		return 1;
	}

	attrlist = sysfs_get_device_attributes(device);
	if (attrlist != NULL) {
		dlist_for_each_data(attrlist, attr,
					struct sysfs_attribute)
			fprintf(stdout, "\t%-20s : %s",
					attr->name, attr->value);
	}
	fprintf(stdout, "\n");

	sysfs_close_device(device);
	return 0;
}

