/*
 * testout.c
 *
 * Display routines for the libsysfs testsuite
 *
 * Copyright (C) IBM Corp. 2004
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

/**
 * Display routines for test functions
 */

#include <test-defs.h>

static void remove_end_newline(char *value)
{
        char *p = value + (strlen(value) - 1);

        if (p != NULL && *p == '\n')
                *p = '\0';
}

void show_device(struct sysfs_device *device)
{
	if (device != NULL)
		dbg_print("Device is \"%s\" at \"%s\"\n",
				device->name, device->path);
}

void show_driver(struct sysfs_driver *driver)
{
	if (driver != NULL)
		dbg_print("Driver is \"%s\" at \"%s\"\n",
				driver->name, driver->path);
}

void show_device_list(struct dlist *devlist)
{
	if (devlist != NULL) {
		struct sysfs_device *dev = NULL;

		dlist_for_each_data(devlist, dev, struct sysfs_device)
			show_device(dev);
	}
}

void show_driver_list(struct dlist *drvlist)
{
	if (drvlist != NULL) {
		struct sysfs_driver *drv = NULL;

		dlist_for_each_data(drvlist, drv, struct sysfs_driver)
			show_driver(drv);
	}
}

void show_root_device(struct sysfs_root_device *root)
{
	if (root != NULL)
		dbg_print("Device is \"%s\" at \"%s\"\n",
				root->name, root->path);
}

void show_attribute(struct sysfs_attribute *attr)
{
	if (attr != NULL) {
		if (attr->value)
			remove_end_newline(attr->value);
		dbg_print("Attr \"%s\" at \"%s\" has a value \"%s\" \n",
				attr->name, attr->path, attr->value);
	}
}

void show_attribute_list(struct dlist *attrlist)
{
	if (attrlist != NULL) {
		struct sysfs_attribute *attr = NULL;

		dlist_for_each_data(attrlist, attr, struct sysfs_attribute)
			show_attribute(attr);
	}
}

void show_link(struct sysfs_link *ln)
{
	if (ln != NULL)
		dbg_print("Link at \"%s\" points to \"%s\"\n",
				ln->path, ln->target);
}

void show_links_list(struct dlist *linklist)
{
	if (linklist != NULL) {
		struct sysfs_link *ln = NULL;

		dlist_for_each_data(linklist, ln, struct sysfs_link)
			show_link(ln);
	}
}

void show_dir(struct sysfs_directory *dir)
{
	if (dir != NULL)
		dbg_print("Directory \"%s\" is at \"%s\"\n",
				dir->name, dir->path);
}

void show_dir_list(struct dlist *dirlist)
{
	if (dirlist != NULL) {
		struct sysfs_directory *dir = NULL;

		dlist_for_each_data(dirlist, dir, struct sysfs_directory)
			show_dir(dir);
	}
}

void show_directory(struct sysfs_directory *dir)
{
	if (dir != NULL) {
		show_dir(dir);

		if (dir->attributes)
			show_attribute_list(dir->attributes);
		if (dir->links)
			show_links_list(dir->links);
		if (dir->subdirs)
			show_dir_list(dir->subdirs);
	}
}

void show_dir_tree(struct sysfs_directory *dir)
{
	if (dir != NULL) {
		struct sysfs_directory *subdir = NULL;

		if (dir->subdirs) {
			dlist_for_each_data(dir->subdirs, subdir,
						struct sysfs_directory) {
				show_dir(subdir);
				show_dir_tree(subdir);
			}
		}
	}
}

void show_class_device(struct sysfs_class_device *dev)
{
	if (dev != NULL)
		dbg_print("Class device \"%s\" belongs to the \"%s\" class\n",
				dev->name, dev->classname);
}

void show_class_device_list(struct dlist *devlist)
{
	if (devlist != NULL) {
		struct sysfs_class_device *dev = NULL;

		dlist_for_each_data(devlist, dev, struct sysfs_class_device)
			show_class_device(dev);
	}
}

void show_list(struct dlist *list)
{
	if (list != NULL) {
		char *name = NULL;

		dlist_for_each_data(list, name, char)
			dbg_print("%s\n", name);
	}
}

