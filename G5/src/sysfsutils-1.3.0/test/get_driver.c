/*
 * get_driver.c
 *
 * Utility to get details of the given driver
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
#include <string.h>

#include "libsysfs.h"

static void print_usage(void)
{
        fprintf(stdout, "Usage: get_driver [driver]\n");
}

int main(int argc, char *argv[])
{
	char *bus = NULL, path[SYSFS_PATH_MAX];
	struct sysfs_driver *driver = NULL;
	struct sysfs_device *device = NULL;
	struct dlist *devlist = NULL;

	if (argc != 2) {
		print_usage();
		return 1;
	}

	memset(path, 0, SYSFS_PATH_MAX);
	bus = (char *)calloc(1, SYSFS_NAME_LEN);
	if ((sysfs_find_driver_bus(argv[1], bus, SYSFS_NAME_LEN)) < 0) {
		fprintf(stdout, "Driver %s not found\n", argv[1]);
		free(bus);
		return 1;
	}
	fprintf(stdout, "Driver %s is a member of bus %s\n", argv[1], bus);

	if ((sysfs_get_mnt_path(path, SYSFS_PATH_MAX)) != 0) {
		fprintf(stdout, "Sysfs not mounted?\n");
		return 1;
	}
	strcat(path, "/");
	strcat(path, SYSFS_BUS_NAME);
	strcat(path, "/");
	strcat(path, bus);
	strcat(path, "/");
	strcat(path, SYSFS_DRIVERS_NAME);
	strcat(path, "/");
	strcat(path, argv[1]);
	driver = sysfs_open_driver_path(path);
	if (driver == NULL) {
		fprintf(stdout, "Device %s not found\n", argv[1]);
		free(bus);
		return 1;
	}
	devlist = sysfs_get_driver_devices(driver);
	if (devlist != NULL) {
		fprintf(stdout, "%s is used by:\n", argv[1]);
		dlist_for_each_data(devlist, device, struct sysfs_device)
			fprintf(stdout, "\t\t%s\n", device->bus_id);
	} else
		fprintf(stdout, "%s is presently not used by any device\n", argv[1]);

	sysfs_close_driver(driver);
	free(bus);
	return 0;
}

