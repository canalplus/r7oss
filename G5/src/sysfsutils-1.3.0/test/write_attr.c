/*
 * write_attr.c
 *
 * Utility to modify the value of a given class device attribute
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
        fprintf(stdout, "Usage: write_attr [classname] [device] [attribute] [new_value]\n");
}

int main(int argc, char *argv[])
{
	struct sysfs_attribute *attr = NULL;

	if (argc != 5) {
		print_usage();
		return 1;
	}

	attr = sysfs_open_classdev_attr(argv[1], argv[2], argv[3]);
	if (attr == NULL) {
		fprintf(stdout, "Attribute %s not defined for classdev %s\n",
					argv[3], argv[2]);
		return 1;
	}

	fprintf(stdout, "Attribute %s presently has a value %s\n",
					attr->name, attr->value);
	if ((sysfs_write_attribute(attr, argv[4], strlen(argv[4]))) != 0) {
		fprintf(stdout, "Error writing attribute value\n");
		sysfs_close_attribute(attr);
		return 1;
	}
	fprintf(stdout, "Attribute value after write is %s\n", attr->value);
	sysfs_close_attribute(attr);
	return 0;
}

