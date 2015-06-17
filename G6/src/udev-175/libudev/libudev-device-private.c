/*
 * libudev - interface to udev device information
 *
 * Copyright (C) 2008-2010 Kay Sievers <kay.sievers@vrfy.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "libudev.h"
#include "libudev-private.h"

static void udev_device_tag(struct udev_device *dev, const char *tag, bool add)
{
	const char *id;
	struct udev *udev = udev_device_get_udev(dev);
	char filename[UTIL_PATH_SIZE];

	id = udev_device_get_id_filename(dev);
	if (id == NULL)
		return;
	util_strscpyl(filename, sizeof(filename), udev_get_run_path(udev), "/tags/", tag, "/", id, NULL);

	if (add) {
		int fd;

		util_create_path(udev, filename);
		fd = open(filename, O_WRONLY|O_CREAT|O_CLOEXEC|O_TRUNC|O_NOFOLLOW, 0444);
		if (fd >= 0)
			close(fd);
	} else {
		unlink(filename);
	}
}

int udev_device_tag_index(struct udev_device *dev, struct udev_device *dev_old, bool add)
{
	struct udev_list_entry *list_entry;
	bool found;

	if (add && dev_old != NULL) {
		/* delete possible left-over tags */
		udev_list_entry_foreach(list_entry, udev_device_get_tags_list_entry(dev_old)) {
			const char *tag_old = udev_list_entry_get_name(list_entry);
			struct udev_list_entry *list_entry_current;

			found = false;
			udev_list_entry_foreach(list_entry_current, udev_device_get_tags_list_entry(dev)) {
				const char *tag = udev_list_entry_get_name(list_entry_current);

				if (strcmp(tag, tag_old) == 0) {
					found = true;
					break;
				}
			}
			if (!found)
				udev_device_tag(dev_old, tag_old, false);
		}
	}

	udev_list_entry_foreach(list_entry, udev_device_get_tags_list_entry(dev))
		udev_device_tag(dev, udev_list_entry_get_name(list_entry), add);

	return 0;
}

static bool device_has_info(struct udev_device *udev_device)
{
	struct udev *udev = udev_device_get_udev(udev_device);
	struct udev_list_entry *list_entry;

	if (udev_device_get_devlinks_list_entry(udev_device) != NULL)
		return true;
	if (udev_device_get_devlink_priority(udev_device) != 0)
		return true;
	udev_list_entry_foreach(list_entry, udev_device_get_properties_list_entry(udev_device))
		if (udev_list_entry_get_num(list_entry))
			return true;
	if (udev_device_get_tags_list_entry(udev_device) != NULL)
		return true;
	if (udev_device_get_devnode(udev_device) != NULL) {
		size_t devlen = strlen(udev_get_dev_path(udev))+1;

		if (udev_device_get_knodename(udev_device) == NULL) {
			if (strcmp(&udev_device_get_devnode(udev_device)[devlen], udev_device_get_sysname(udev_device)) != 0)
				return true;
		}
		else {
			if (strcmp(&udev_device_get_devnode(udev_device)[devlen], udev_device_get_knodename(udev_device)) != 0)
				return true;
		}
	}
	if (udev_device_get_watch_handle(udev_device) >= 0)
		return true;
	return false;
}

int udev_device_update_db(struct udev_device *udev_device)
{
	bool has_info;
	const char *id;
	struct udev *udev = udev_device_get_udev(udev_device);
	char filename[UTIL_PATH_SIZE];
	char filename_tmp[UTIL_PATH_SIZE];
	FILE *f;

	id = udev_device_get_id_filename(udev_device);
	if (id == NULL)
		return -1;

	has_info = device_has_info(udev_device);
	util_strscpyl(filename, sizeof(filename), udev_get_run_path(udev), "/data/", id, NULL);

	/* do not store anything for otherwise empty devices */
	if (!has_info &&
	    major(udev_device_get_devnum(udev_device)) == 0 &&
	    udev_device_get_ifindex(udev_device) == 0) {
		unlink(filename);
		return 0;
	}

	/* write a database file */
	util_strscpyl(filename_tmp, sizeof(filename_tmp), filename, ".tmp", NULL);
	util_create_path(udev, filename_tmp);
	f = fopen(filename_tmp, "we");
	if (f == NULL) {
		err(udev, "unable to create temporary db file '%s': %m\n", filename_tmp);
		return -1;
	}

	/*
	 * set 'sticky' bit to indicate that we should not clean the
	 * database when we transition from initramfs to the real root
	 */
	if (udev_device_get_db_persist(udev_device))
		fchmod(fileno(f), 01644);

	if (has_info) {
		size_t devlen = strlen(udev_get_dev_path(udev))+1;
		struct udev_list_entry *list_entry;

		if (udev_device_get_devnode(udev_device) != NULL) {
			fprintf(f, "N:%s\n", &udev_device_get_devnode(udev_device)[devlen]);
			udev_list_entry_foreach(list_entry, udev_device_get_devlinks_list_entry(udev_device))
				fprintf(f, "S:%s\n", &udev_list_entry_get_name(list_entry)[devlen]);
		}
		if (udev_device_get_devlink_priority(udev_device) != 0)
			fprintf(f, "L:%i\n", udev_device_get_devlink_priority(udev_device));
		if (udev_device_get_watch_handle(udev_device) >= 0)
			fprintf(f, "W:%i\n", udev_device_get_watch_handle(udev_device));
		if (udev_device_get_usec_initialized(udev_device) > 0)
			fprintf(f, "I:%llu\n", udev_device_get_usec_initialized(udev_device));
		udev_list_entry_foreach(list_entry, udev_device_get_properties_list_entry(udev_device)) {
			if (!udev_list_entry_get_num(list_entry))
				continue;
			fprintf(f, "E:%s=%s\n",
				udev_list_entry_get_name(list_entry),
				udev_list_entry_get_value(list_entry));
		}
		udev_list_entry_foreach(list_entry, udev_device_get_tags_list_entry(udev_device))
			fprintf(f, "G:%s\n", udev_list_entry_get_name(list_entry));
	}

	fclose(f);
	rename(filename_tmp, filename);
	info(udev, "created %s file '%s' for '%s'\n", has_info ? "db" : "empty", 
	     filename, udev_device_get_devpath(udev_device));
	return 0;
}

int udev_device_delete_db(struct udev_device *udev_device)
{
	const char *id;
	struct udev *udev = udev_device_get_udev(udev_device);
	char filename[UTIL_PATH_SIZE];

	id = udev_device_get_id_filename(udev_device);
	if (id == NULL)
		return -1;
	util_strscpyl(filename, sizeof(filename), udev_get_run_path(udev), "/data/", id, NULL);
	unlink(filename);
	return 0;
}

static int udev_device_sysfs_dev_link_from_id_filename(
    struct udev_device *udev_device, char *sysfs_filename, size_t size)
{
	struct udev *udev = udev_device_get_udev(udev_device);
	const char *id;

	id = udev_device_get_id_filename(udev_device);
	if (id == NULL)
		return -1;
	if (id[0] != 'b' && id[0] != 'c')
		return -1;
	util_strscpyl(sysfs_filename, size, udev_get_sys_path(udev),
				"/dev/", (id[0] == 'b' ? "block" : "char"), "/", &id[1], NULL);
	return 0;
}

static int udev_device_dev_link_from_id_filename(
    struct udev_device *udev_device, char *filename, size_t size)
{
	struct udev *udev = udev_device_get_udev(udev_device);
	const char *id;

	id = udev_device_get_id_filename(udev_device);
	if (id == NULL)
		return -1;
	if (id[0] != 'b' && id[0] != 'c')
		return -1;
	util_strscpyl(filename, UTIL_PATH_SIZE, udev_get_run_path(udev),
				"/dev/", (id[0] == 'b' ? "block" : "char"), "/", &id[1], NULL);
	return 0;
}

int udev_device_need_dev_link(struct udev_device *udev_device)
{
	char sysfs_filename[UTIL_PATH_SIZE];
	struct stat statbuf;

	if (udev_device_sysfs_dev_link_from_id_filename(udev_device, sysfs_filename,
        sizeof(sysfs_filename)) != 0)
		return 0;
	return (stat(sysfs_filename, &statbuf) != 0);
}

int udev_device_create_dev_link(struct udev_device *udev_device)
{
	struct udev *udev = udev_device_get_udev(udev_device);
	char filename[UTIL_PATH_SIZE];
	const char *syspath = udev_device_get_syspath(udev_device);
	struct stat statbuf;

	if (udev_device_dev_link_from_id_filename(udev_device, filename,
        sizeof(filename)) != 0)
		return -1;

	if (lstat(filename, &statbuf) == 0 && S_ISLNK(statbuf.st_mode)) {
		char buf[UTIL_PATH_SIZE];
		ssize_t len;

		dbg(udev, "found existing symlink '%s'\n", filename);
		len = readlink(filename, buf, sizeof(buf));
		if (len > 0 && len < (ssize_t)sizeof(buf)) {
			buf[len] = '\0';
			if (strcmp(buf, syspath) == 0) {
				info(udev, "preserve already existing symlink '%s' to '%s'\n",
						filename, syspath);
				return 0;
			}
		}
	}

	info(udev, "create symlink '%s' to '%s'\n", filename, syspath);
	if (unlink(filename) != 0 && errno != ENOENT) {
		err(udev, "unlink(%s) failed: %m\n", filename);
		return -1;
	}
	if (util_create_path(udev, filename) != 0) {
		err(udev, "util_create_path(%s) failed\n", filename);
		return -1;
	}
	if (symlink(syspath, filename) != 0) {
		err(udev, "symlink(%s,%s) failed: %m\n", syspath, filename);
		return -1;
	}
	return 0;
}

int udev_device_delete_dev_link(struct udev_device *udev_device)
{
	struct udev *udev = udev_device_get_udev(udev_device);
	char filename[UTIL_PATH_SIZE];

	if (udev_device_dev_link_from_id_filename(udev_device, filename,
        sizeof(filename)) != 0)
		return -1;
	if (unlink(filename) != 0 && errno != ENOENT) {
		err(udev, "unlink(%s) failed: %m\n", filename);
		return -1;
	}
	return 0;
}
