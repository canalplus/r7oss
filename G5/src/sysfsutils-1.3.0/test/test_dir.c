/*
 * test_dir.c
 *
 * Tests for directory related functions for the libsysfs testsuite
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
 ***************************************************************************
 * this will test the directory related functions provided by libsysfs.
 *
 * extern void sysfs_close_attribute(struct sysfs_attribute *sysattr);
 * extern struct sysfs_attribute *sysfs_open_attribute
 * 					(const char *path);
 * extern int sysfs_read_attribute(struct sysfs_attribute *sysattr);
 * extern int sysfs_read_attribute_value(const char *attrpath,
 * 				char *value, size_t vsize);
 * extern int sysfs_write_attribute(struct sysfs_attribute *sysattr,
 * 		const char *new_value, size_t len);
 * extern char *sysfs_get_value_from_attributes(struct dlist *attr,
 * 					const char * name);
 * extern int sysfs_refresh_dir_attributes(struct sysfs_directory *sysdir);
 * extern int sysfs_refresh_dir_links(struct sysfs_directory *sysdir);
 * extern int sysfs_refresh_dir_subdirs(struct sysfs_directory *sysdir);
 * extern void sysfs_close_directory(struct sysfs_directory *sysdir);
 * extern struct sysfs_directory *sysfs_open_directory
 * 					(const char *path);
 * extern int sysfs_read_dir_attributes(struct sysfs_directory *sysdir);
 * extern int sysfs_read_dir_links(struct sysfs_directory *sysdir);
 * extern int sysfs_read_dir_subdirs(struct sysfs_directory *sysdir);
 * extern int sysfs_read_directory(struct sysfs_directory *sysdir);
 * extern int sysfs_read_all_subdirs(struct sysfs_directory *sysdir);
 * extern struct sysfs_directory *sysfs_get_subdirectory
 * 			(struct sysfs_directory *dir, char *subname);
 * extern void sysfs_close_link(struct sysfs_link *ln);
 * extern struct sysfs_link *sysfs_open_link(const char *lnpath);
 * extern struct sysfs_link *sysfs_get_directory_link
 * 			(struct sysfs_directory *dir, char *linkname);
 * extern struct sysfs_link *sysfs_get_subdirectory_link
 * 			(struct sysfs_directory *dir, char *linkname);
 * extern struct sysfs_attribute *sysfs_get_directory_attribute
 * 			(struct sysfs_directory *dir, char *attrname);
 * extern struct dlist *sysfs_get_dir_attributes(struct sysfs_directory *dir);
 * extern struct dlist *sysfs_get_dir_links(struct sysfs_directory *dir);
 * extern struct dlist *sysfs_get_dir_subdirs(struct sysfs_directory *dir);
 ****************************************************************************
 */

#include "test-defs.h"
#include <errno.h>

/**
 * extern void sysfs_close_attribute(struct sysfs_attribute *sysattr);
 *
 * flag:
 * 	0:	sysattr -> valid
 * 	1:	sysattr -> NULL
 */
int test_sysfs_close_attribute(int flag)
{
	struct sysfs_attribute *sysattr = NULL;
	char *path = NULL;

	switch (flag) {
	case 0:
		path = val_file_path;
		sysattr = sysfs_open_attribute(path);
		if (sysattr == NULL) {
			dbg_print("%s: Error opening attribute at %s\n",
					__FUNCTION__, val_file_path);
			return 0;
		}
		break;
	case 1:
		sysattr = NULL;
		break;
	default:
		return -1;
	}
	sysfs_close_attribute(sysattr);

	dbg_print("%s: returns void\n", __FUNCTION__);

	return 0;
}

/**
 * extern struct sysfs_attribute *sysfs_open_attribute
 * 					(const char *path);
 *
 * flag:
 * 	0:	path -> valid
 * 	1:	path -> invalid
 * 	2:	path -> NULL
 */
int test_sysfs_open_attribute(int flag)
{
	char *path = NULL;
	struct sysfs_attribute *sysattr = NULL;

	switch (flag) {
	case 0:
		path = val_file_path;
		break;
	case 1:
		path = inval_path;
		break;
	case 2:
		path = NULL;
		break;
	default:
		return -1;
	}
	sysattr = sysfs_open_attribute(path);

	switch (flag) {
	case 0:
		if (sysattr == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			dbg_print("Attrib name = %s, at %s\n\n",
					sysattr->name, sysattr->path);
		}
		break;
	case 1:
	case 2:
		if (sysattr != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;

	}
	if (sysattr != NULL) {
		sysfs_close_attribute(sysattr);
		sysattr = NULL;
	}

	return 0;
}

/**
 * extern int sysfs_read_attribute(struct sysfs_attribute *sysattr);
 *
 * flag:
 * 	0:	sysattr -> valid
 * 	1:	sysattr -> NULL
 */
int test_sysfs_read_attribute(int flag)
{
	struct sysfs_attribute *sysattr = NULL;
	int ret = 0;

	switch (flag) {
	case 0:
		sysattr = sysfs_open_attribute(val_file_path);
		if (sysattr == NULL) {
			dbg_print("%s: failed opening attribute at %s\n",
					__FUNCTION__, val_file_path);
			return 0;
		}
		break;
	case 1:
		sysattr = NULL;
		break;
	default:
		return -1;
	}
	ret = sysfs_read_attribute(sysattr);

	switch (flag) {
	case 0:
		if (ret != 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_attribute(sysattr);
			dbg_print("\n");
		}
		break;
	case 1:
		if (ret == 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (sysattr != NULL)
		sysfs_close_attribute(sysattr);

	return 0;
}

/**
 * extern int sysfs_read_attribute_value(const char *attrpath,
 *					char *value, size_t vsize);
 *
 * flag:
 * 	0:	attrpath -> valid, value -> valid
 * 	1:	attrpath -> valid, value -> NULL
 * 	2:	attrpath -> NULL, value -> valid
 * 	3:	attrpath -> NULL, value -> NULL
 */
int test_sysfs_read_attribute_value(int flag)
{
	char *attrpath = NULL;
	char *value = NULL;
	size_t vsize = SYSFS_PATH_MAX;
	int ret = 0;

	switch (flag) {
	case 0:
		attrpath = val_file_path;
		value = calloc(1, SYSFS_PATH_MAX);
		break;
	case 1:
		attrpath = val_file_path;
		value = NULL;
		break;
	case 2:
		attrpath = NULL;
		value = calloc(1, SYSFS_PATH_MAX);
		break;
	case 3:
		attrpath = NULL;
		value = NULL;
		break;
	default:
		return -1;
	}
	ret = sysfs_read_attribute_value(attrpath, value, vsize);

	switch (flag) {
	case 0:
		if (ret != 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			dbg_print("Attribute at %s has value %s\n\n",
					attrpath, value);
		}
		break;
	case 1:
	case 2:
	case 3:
		if (ret == 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		return 0;
		break;
	}

	if (value != NULL)
		free(value);

	return 0;
}

/**
 * extern int sysfs_write_attribute(struct sysfs_attribute *sysattr,
 * 			const char *new_value, size_t len);
 *
 * flag:
 * 	0:	sysattr -> valid, new_value -> valid, len -> valid;
 * 	1:	sysattr -> valid, new_value -> invalid, len -> invalid;
 * 	2:	sysattr -> valid, new_value -> NULL, len -> invalid;
 * 	3:	sysattr -> NULL, new_value -> valid, len -> valid;
 * 	4:	sysattr -> NULL, new_value -> invalid, len -> invalid;
 * 	5:	sysattr -> NULL, new_value -> NULL, len -> invalid;
 */
int test_sysfs_write_attribute(int flag)
{
	struct sysfs_attribute *sysattr = NULL;
	char *new_value = NULL;
	size_t len = 0;
	int ret = 0;

	switch (flag) {
	case 0:
		sysattr = sysfs_open_attribute(val_write_attr_path);
		if (sysattr == NULL) {
			dbg_print("%s: failed opening attribute at %s\n",
					__FUNCTION__, val_write_attr_path);
			return 0;
		}
		if (sysfs_read_attribute(sysattr) != 0) {
			dbg_print("%s: failed reading attribute at %s\n",
					__FUNCTION__, val_write_attr_path);
			return 0;
		}
		new_value = calloc(1, sysattr->len + 1);
		strncpy(new_value, sysattr->value, sysattr->len);
		len = sysattr->len;
		break;
	case 1:
		sysattr = sysfs_open_attribute(val_write_attr_path);
		if (sysattr == NULL) {
			dbg_print("%s: failed opening attribute at %s\n",
					__FUNCTION__, val_write_attr_path);
			return 0;
		}
		new_value = calloc(1, SYSFS_PATH_MAX);
		strncpy(new_value, "this should not get copied in the attrib",
				SYSFS_PATH_MAX);
		len = SYSFS_PATH_MAX;
		break;
	case 2:
		sysattr = sysfs_open_attribute(val_write_attr_path);
		if (sysattr == NULL) {
			dbg_print("%s: failed opening attribute at %s\n",
					__FUNCTION__, val_write_attr_path);
			return 0;
		}
		new_value = NULL;
		len = SYSFS_PATH_MAX;
		break;
	case 3:
		sysattr = sysfs_open_attribute(val_write_attr_path);
		if (sysattr == NULL) {
			dbg_print("%s: failed opening attribute at %s\n",
					__FUNCTION__, val_write_attr_path);
			return 0;
		}
		new_value = calloc(1, sysattr->len + 1);
		strncpy(new_value, sysattr->value, sysattr->len);
		len = sysattr->len;
		sysfs_close_attribute(sysattr);
		sysattr = NULL;
		break;
	case 4:
		sysattr = NULL;
		new_value = calloc(1, SYSFS_PATH_MAX);
		strncpy(new_value, "this should not get copied in the attrib",
				SYSFS_PATH_MAX);
		len = SYSFS_PATH_MAX;
		break;
	case 5:
		sysattr = NULL;
		new_value = NULL;
		len = SYSFS_PATH_MAX;
		break;
	default:
		return -1;
	}
	ret = sysfs_write_attribute(sysattr, new_value, len);

	switch (flag) {
	case 0:
		if (ret != 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			dbg_print("Attribute at %s now has value %s\n\n",
					sysattr->path, sysattr->value);
		}
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if (ret == 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}
	if (sysattr != NULL) {
		sysfs_close_attribute(sysattr);
		sysattr = NULL;
	}
	if (new_value != NULL)
		free(new_value);

	return 0;
}

/**
 * extern char *sysfs_get_value_from_attributes(struct dlist *attr,
 * 					const char * name);
 *
 * flag:
 * 	0:	attr -> valid, name -> valid
 * 	1:	attr -> valid, name -> invalid
 * 	2:	attr -> valid, name -> NULL
 * 	3:	attr -> NULL, name -> valid
 * 	4:	attr -> NULL, name -> invalid
 * 	5:	attr -> NULL, name -> NULL
 */
int test_sysfs_get_value_from_attributes(int flag)
{
	struct dlist *attrlist = NULL;
	struct sysfs_device *device = NULL;
	char *name = NULL;
	char *val = NULL;
	char *path = NULL;

	switch (flag) {
	case 0:
		path = val_dev_path;
		device = sysfs_open_device_path(path);
		if (device == NULL) {
			dbg_print("%s: failed opening device at %s\n",
					__FUNCTION__, path);
			return 0;
		}
		attrlist = sysfs_get_device_attributes(device);
		if (attrlist == NULL) {
			dbg_print("%s: failed getting attribs for device %s\n",
					__FUNCTION__, device->name);
			sysfs_close_device(device);
			return 0;
		}
		name = val_dev_attr;
		break;
	case 1:
		path = val_dev_path;
		device = sysfs_open_device_path(path);
		if (device == NULL) {
			dbg_print("%s: failed opening device at %s\n",
					__FUNCTION__, path);
			return 0;
		}
		attrlist = sysfs_get_device_attributes(device);
		if (attrlist == NULL) {
			dbg_print("%s: failed getting attribs for device %s\n",
					__FUNCTION__, device->name);
			sysfs_close_device(device);
			return 0;
		}
		name = inval_name;
		break;
	case 2:
		path = val_dev_path;
		device = sysfs_open_device_path(path);
		if (device == NULL) {
			dbg_print("%s: failed opening device at %s\n",
					__FUNCTION__, path);
			return 0;
		}
		attrlist = sysfs_get_device_attributes(device);
		if (attrlist == NULL) {
			dbg_print("%s: failed getting attribs for device %s\n",
					__FUNCTION__, device->name);
			sysfs_close_device(device);
			return 0;
		}
		name = NULL;
		break;
	case 3:
		attrlist = NULL;
		name = val_dev_attr;
		break;
	case 4:
		attrlist = NULL;
		name = inval_name;
		break;
	case 5:
		attrlist = NULL;
		name = NULL;
		break;
	default:
		return -1;
	}
	val = sysfs_get_value_from_attributes(attrlist, name);

	switch (flag) {
	case 0:
		if (val == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			dbg_print("Attribute %s has value %s\n\n",
					name, val);
		}
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if (val != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (device != NULL) {
		sysfs_close_device(device);
		device = NULL;
	}
	return 0;
}

/**
 * extern int sysfs_refresh_dir_attributes(struct sysfs_directory *sysdir);
 *
 * flag:
 * 	0:	sysdir -> valid
 * 	1:	sysdir -> NULL
 */
int test_sysfs_refresh_dir_attributes(int flag)
{
	struct sysfs_directory *sysdir = NULL;
	int ret = 0;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_dev_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dev_path);
			return 0;
		}
		if (sysfs_read_dir_attributes(sysdir) != 0) {
			dbg_print("%s: failed to read attribs under %s\n",
					__FUNCTION__, sysdir->path);
			sysfs_close_directory(sysdir);
			return 0;
		}
		break;
	case 1:
		sysdir = NULL;
		break;
	default:
		return -1;
	}
	ret = sysfs_refresh_dir_attributes(sysdir);

	switch (flag) {
	case 0:
		if (ret != 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_attribute_list(sysdir->attributes);
			dbg_print("\n");
		}
		break;
	case 1:
		if (ret  == 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}
	if (sysdir != NULL) {
		sysfs_close_directory(sysdir);
		sysdir = NULL;
	}
	return 0;
}

/**
 * extern int sysfs_refresh_dir_links(struct sysfs_directory *sysdir);
 *
 * flag:
 * 	0:	sysdir -> valid
 * 	1:	sysdir -> NULL
 */
int test_sysfs_refresh_dir_links(int flag)
{
	struct sysfs_directory *sysdir = NULL;
	int ret = 0;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_drv_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_drv_path);
			return 0;
		}
		if (sysfs_read_dir_links(sysdir) != 0) {
			dbg_print("%s: failed to read links under %s\n",
					__FUNCTION__, sysdir->path);
			sysfs_close_directory(sysdir);
			return 0;
		}
		break;
	case 1:
		sysdir = NULL;
		break;
	default:
		return -1;
	}
	ret = sysfs_refresh_dir_links(sysdir);

	switch (flag) {
	case 0:
		if (ret != 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_links_list(sysdir->links);
			dbg_print("\n");
		}
		break;
	case 1:
		if (ret  == 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}
	if (sysdir != NULL) {
		sysfs_close_directory(sysdir);
		sysdir = NULL;
	}
	return 0;
}

/**
 * extern int sysfs_refresh_dir_subdirs(struct sysfs_directory *sysdir);
 *
 * flag:
 * 	0:	sysdir -> valid
 * 	1:	sysdir -> NULL
 */
int test_sysfs_refresh_dir_subdirs(int flag)
{
	struct sysfs_directory *sysdir = NULL;
	int ret = 0;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_root_dev_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_root_dev_path);
			return 0;
		}
		if (sysfs_read_dir_subdirs(sysdir) != 0) {
			dbg_print("%s: failed to read subdirs under at %s\n",
					__FUNCTION__, sysdir->path);
			sysfs_close_directory(sysdir);
			return 0;
		}
		break;
	case 1:
		sysdir = NULL;
		break;
	default:
		return -1;
	}
	ret = sysfs_refresh_dir_subdirs(sysdir);

	switch (flag) {
	case 0:
		if (ret != 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_dir_list(sysdir->subdirs);
			dbg_print("\n");
		}
		break;
	case 1:
		if (ret  == 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}
	if (sysdir != NULL) {
		sysfs_close_directory(sysdir);
		sysdir = NULL;
	}
	return 0;
}

/**
 * extern void sysfs_close_directory(struct sysfs_directory *sysdir);
 *
 * flag:
 * 	0:	sysdir -> valid
 * 	1:	sysdir -> invalid
 * 	2:	sysdir -> NULL
 */
int test_sysfs_close_directory(int flag)
{
	struct sysfs_directory *sysdir = NULL;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_dir_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		break;
	case 1:
		sysdir = calloc(1, sizeof(struct sysfs_directory));
		break;
	case 2:
		sysdir = NULL;
		break;
	default:
		return -1;
	}

	sysfs_close_directory(sysdir);
	dbg_print("%s: returns void\n", __FUNCTION__);

	return 0;
}

/**
 * extern struct sysfs_directory *sysfs_open_directory
 * 						(const char *path);
 *
 * flag:
 * 	0:	path -> valid
 * 	1:	path -> invalid
 * 	2:	path -> NULL
 */
int test_sysfs_open_directory(int flag)
{
	char *path = NULL;
	struct sysfs_directory *dir = NULL;

	switch (flag) {
	case 0:
		path = val_dir_path;
		break;
	case 1:
		path = inval_path;
		break;
	case 2:
		path = NULL;
		break;
	default:
		return -1;
	}
	dir = sysfs_open_directory(path);

	switch (flag) {
	case 0:
		if (dir == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			dbg_print("Directory is %s at %s\n\n",
						dir->name, dir->path);
		}
		break;
	case 1:
	case 2:
		if (dir != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (dir != NULL)
		sysfs_close_directory(dir);
	return 0;
}

/**
 * extern int sysfs_read_dir_attributes(struct sysfs_directory *sysdir);
 *
 * flag:
 * 	0:	sysdir -> valid
 * 	1:	sysdir -> NULL
 */
int test_sysfs_read_dir_attributes(int flag)
{
	struct sysfs_directory *sysdir = NULL;
	int ret = 0;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_dev_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dev_path);
			return 0;
		}
		break;
	case 1:
		sysdir = NULL;
		break;
	default:
		return -1;
	}
	ret = sysfs_read_dir_attributes(sysdir);

	switch (flag) {
	case 0:
		if (ret != 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_attribute_list(sysdir->attributes);
			dbg_print("\n");
		}
		break;
	case 1:
		if (ret == 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (sysdir != NULL)
		sysfs_close_directory(sysdir);
	return 0;
}

/**
 * extern int sysfs_read_dir_links(struct sysfs_directory *sysdir);
 *
 * flag:
 * 	0:	sysdir -> valid
 * 	1:	sysdir -> NULL
 */
int test_sysfs_read_dir_links(int flag)
{
	struct sysfs_directory *sysdir = NULL;
	int ret = 0;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_drv_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_drv_path);
			return 0;
		}
		break;
	case 1:
		sysdir = NULL;
		break;
	default:
		return -1;
	}
	ret = sysfs_read_dir_links(sysdir);

	switch (flag) {
	case 0:
		if (ret != 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_links_list(sysdir->links);
			dbg_print("\n");
		}
		break;
	case 1:
		if (ret == 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (sysdir != NULL)
		sysfs_close_directory(sysdir);
	return 0;
}

/**
 * extern int sysfs_read_dir_subdirs(struct sysfs_directory *sysdir);
 *
 * flag:
 * 	0:	sysdir -> valid
 * 	1:	sysdir -> NULL
 */
int test_sysfs_read_dir_subdirs(int flag)
{
	struct sysfs_directory *sysdir = NULL;
	int ret = 0;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_dir_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		break;
	case 1:
		sysdir = NULL;
		break;
	default:
		return -1;
	}

	ret = sysfs_read_dir_subdirs(sysdir);

	switch (flag) {
	case 0:
		if (ret != 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_dir_list(sysdir->subdirs);
			dbg_print("\n");
		}
		break;
	case 1:
		if (ret == 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (sysdir != NULL)
		sysfs_close_directory(sysdir);
	return 0;
}

/**
 * extern int sysfs_read_directory(struct sysfs_directory *sysdir);
 *
 * flag:
 * 	0:	sysdir -> valid
 * 	1:	sysdir -> NULL
 */
int test_sysfs_read_directory(int flag)
{
	struct sysfs_directory *sysdir = NULL;
	int ret = 0;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_drv_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_drv_path);
			return 0;
		}
		break;
	case 1:
		sysdir = NULL;
		break;
	default:
		return -1;
	}
	ret = sysfs_read_directory(sysdir);

	switch (flag) {
	case 0:
		if (ret != 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_directory(sysdir);
			dbg_print("\n");
		}
		break;
	case 1:
		if (ret == 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (flag == 1) {
		free(sysdir);
		sysdir = NULL;
	}

	if (sysdir != NULL)
		sysfs_close_directory(sysdir);
	return 0;
}

/**
 * extern int sysfs_read_all_subdirs(struct sysfs_directory *sysdir);
 *
 * flag:
 * 	0:	sysdir -> valid
 * 	1:	sysdir -> NULLd
 */
int test_sysfs_read_all_subdirs(int flag)
{
	struct sysfs_directory *sysdir = NULL;
	int ret = 0;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_root_dev_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_root_dev_path);
			return 0;
		}
		break;
	case 1:
		sysdir = NULL;
		break;
	default:
		return -1;
	}
	ret = sysfs_read_all_subdirs(sysdir);

	switch (flag) {
	case 0:
		if (ret != 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_dir_tree(sysdir);
			dbg_print("\n");
		}
		break;
	case 1:
		if (ret == 0)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (sysdir != NULL)
		sysfs_close_directory(sysdir);

	return 0;
}

/**
 * extern struct sysfs_directory *sysfs_get_subdirectory
 * 			(struct sysfs_directory *dir, char *subname);
 *
 * flag:
 * 	0:	dir -> valid, subname -> valid
 * 	1:	dir -> valid, subname -> invalid
 * 	2:	dir -> valid, subname -> NULL
 * 	3:	dir -> NULL, subname -> valid
 * 	4:	dir -> NULL, subname -> invalid
 * 	5:	dir -> NULL, subname -> NULL
 */
int test_sysfs_get_subdirectory(int flag)
{
	struct sysfs_directory *dir = NULL, *subdir = NULL;
	char *subname = NULL;

	switch (flag) {
	case 0:
		dir = sysfs_open_directory(val_root_dev_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_root_dev_path);
			return 0;
		}
		subname = val_subdir_name;
		break;
	case 1:
		dir = sysfs_open_directory(val_dir_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		subname = inval_name;
		break;
	case 2:
		dir = sysfs_open_directory(val_dir_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		subname = NULL;
		break;
	case 3:
		dir = NULL;
		subname = val_subdir_name;
		break;
	case 4:
		dir = NULL;
		subname = inval_name;
		break;
	case 5:
		dir = NULL;
		subname = NULL;
		break;
	default:
		return -1;
	}
	subdir = sysfs_get_subdirectory(dir, subname);

	switch (flag) {
	case 0:
		if (subdir == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_dir(subdir);
			dbg_print("\n");
		}
		break;
	default:
		if (subdir != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	}

	if (dir != NULL) {
		sysfs_close_directory(dir);
		dir = NULL;
	}
	return 0;
}

/**
 * extern void sysfs_close_link(struct sysfs_link *ln);
 *
 * flag:
 * 	0:	ln -> valid
 * 	1:	ln -> NULL
 */
int test_sysfs_close_link(int flag)
{
	struct sysfs_link *ln = NULL;

	switch (flag) {
	case 0:
		ln = sysfs_open_link(val_link_path);
		if (ln == NULL)
			return 0;
		break;
	case 1:
		ln = NULL;
		break;
	default:
		return -1;
	}

	sysfs_close_link(ln);
	dbg_print("%s: returns void\n", __FUNCTION__);

	return 0;
}

/**
 * extern struct sysfs_link *sysfs_open_link(const char *lnpath);
 *
 * flag:
 * 	0:	lnpath -> valid;
 * 	1:	lnpath -> invalid;
 * 	2:	lnpath -> NULL;
 */
int test_sysfs_open_link(int flag)
{
	char *lnpath = NULL;
	struct sysfs_link *ln = NULL;

	switch (flag) {
	case 0:
		lnpath = val_link_path;
		break;
	case 1:
		lnpath = inval_path;
		break;
	case 2:
		lnpath = NULL;
		break;
	default:
		return -1;
	}
	ln = sysfs_open_link(lnpath);

	switch (flag) {
	case 0:
		if (ln == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_link(ln);
			dbg_print("\n");
		}
		break;
	default:
		if (ln != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	}

	if (ln != NULL)
		sysfs_close_link(ln);

	return 0;
}

/**
 * extern struct sysfs_link *sysfs_get_directory_link
 * 			(struct sysfs_directory *dir, char *linkname);
 *
 * flag:
 * 	0:	dir -> valid, linkname -> valid
 * 	1:	dir -> valid, linkname -> invalid
 * 	2:	dir -> valid, linkname -> NULL
 * 	3:	dir -> NULL, linkname -> valid
 * 	4:	dir -> NULL, linkname -> invalid
 * 	5:	dir -> NULL, linkname -> NULL
 */
int test_sysfs_get_directory_link(int flag)
{
	struct sysfs_directory *dir = NULL;
	struct sysfs_link *ln = NULL;
	char *linkname = NULL;

	switch (flag) {
	case 0:
		dir = sysfs_open_directory(val_drv_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_drv_path);
			return 0;
		}
		linkname = val_drv_dev_name;
		break;
	case 1:
		dir = sysfs_open_directory(val_dir_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		linkname = inval_name;
		break;
	case 2:
		dir = sysfs_open_directory(val_dir_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		linkname = NULL;
		break;
	case 3:
		dir = NULL;
		linkname = val_drv_dev_name;
		break;
	case 4:
		dir = NULL;
		linkname = inval_name;
		break;
	case 5:
		dir = NULL;
		linkname = NULL;
		break;
	default:
		return -1;
	}
	ln = sysfs_get_directory_link(dir, linkname);

	switch (flag) {
	case 0:
		if (ln == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_link(ln);
			dbg_print("\n");
		}
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if (ln != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (dir != NULL)
		sysfs_close_directory(dir);

	return 0;
}

/**
 * extern struct sysfs_link *sysfs_get_subdirectory_link
 * 			(struct sysfs_directory *dir, char *linkname);
 *
 * flag:
 * 	0:	dir -> valid, linkname -> valid
 * 	1:	dir -> valid, linkname -> invalid
 * 	2:	dir -> valid, linkname -> NULL
 * 	3:	dir -> NULL, linkname -> valid
 * 	4:	dir -> NULL, linkname -> invalid
 * 	5:	dir -> NULL, linkname -> NULL
 */
int test_sysfs_get_subdirectory_link(int flag)
{
	struct sysfs_directory *dir = NULL;
	struct sysfs_link *ln = NULL;
	char *linkname = NULL;

	switch (flag) {
	case 0:
		dir = sysfs_open_directory(val_dir_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		linkname = val_subdir_link_name;
		break;
	case 1:
		dir = sysfs_open_directory(val_dir_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		linkname = inval_name;
		break;
	case 2:
		dir = sysfs_open_directory(val_dir_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		linkname = NULL;
		break;
	case 3:
		dir = NULL;
		linkname = val_subdir_link_name;
		break;
	case 4:
		dir = NULL;
		linkname = inval_name;
		break;
	case 5:
		dir = NULL;
		linkname = NULL;
		break;
	default:
		return -1;
	}
	ln = sysfs_get_subdirectory_link(dir, linkname);

	switch (flag) {
	case 0:
		if (ln == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_link(ln);
			dbg_print("\n");
		}
		break;
	default:
		if (ln != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	}

	if (dir != NULL)
		sysfs_close_directory(dir);

	return 0;
}

/**
 * extern struct sysfs_attribute *sysfs_get_directory_attribute
 * 			(struct sysfs_directory *dir, char *attrname);
 *
 * flag:
 * 	0:	dir -> valid, attrname -> valid
 * 	1:	dir -> valid, attrname -> invalid
 * 	2:	dir -> valid, attrname -> NULL
 * 	3:	dir -> NULL, attrname -> valid
 * 	4:	dir -> NULL, attrname -> invalid
 * 	5:	dir -> NULL, attrname -> NULL
 */
int test_sysfs_get_directory_attribute(int flag)
{
	struct sysfs_directory *dir = NULL;
	struct sysfs_attribute *attr = NULL;
	char *attrname = NULL;

	switch (flag) {
	case 0:
		dir = sysfs_open_directory(val_dev_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dev_path);
			return 0;
		}
		attrname = val_dev_attr;
		break;
	case 1:
		dir = sysfs_open_directory(val_dir_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		attrname = inval_name;
		break;
	case 2:
		dir = sysfs_open_directory(val_dir_path);
		if (dir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		attrname = NULL;
		break;
	case 3:
		dir = NULL;
		attrname = val_dev_attr;
		break;
	case 4:
		dir = NULL;
		attrname = inval_name;
		break;
	case 5:
		dir = NULL;
		attrname = NULL;
		break;
	default:
		return -1;
	}
	attr = sysfs_get_directory_attribute(dir, attrname);

	switch (flag) {
	case 0:
		if (attr == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_attribute(attr);
			dbg_print("\n");
		}
		break;
	default:
		if (attr != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	}

	if (dir != NULL)
		sysfs_close_directory(dir);
	return 0;
}

/**
 * extern struct dlist *sysfs_get_dir_attributes(struct sysfs_directory *dir);
 *
 * flag:
 * 	0:	dir -> valid
 * 	1:	dir -> valid
 */
int test_sysfs_get_dir_attributes(int flag)
{
	struct sysfs_directory *sysdir = NULL;
	struct dlist *list = NULL;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_dev_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dev_path);
			return 0;
		}
		break;
	case 1:
		sysdir = NULL;
		break;
	default:
		return -1;
	}
	list = sysfs_get_dir_attributes(sysdir);

	switch (flag) {
	case 0:
		if (list == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_attribute_list(list);
			dbg_print("\n");
		}
		break;
	case 1:
		if (list != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (sysdir != NULL)
		sysfs_close_directory(sysdir);

	return 0;
}

/**
 * extern struct dlist *sysfs_get_dir_links(struct sysfs_directory *dir);
 *
 * flag:
 * 	0:	dir -> valid
 * 	1:	dir -> valid
 *
 */
int test_sysfs_get_dir_links(int flag)
{
	struct sysfs_directory *sysdir = NULL;
	struct dlist *list = NULL;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_class_dev_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_class_dev_path);
			return 0;
		}
		break;
	case 1:
		sysdir = NULL;
		break;
	default:
		return -1;
	}
	list = sysfs_get_dir_links(sysdir);

	switch (flag) {
	case 0:
		if (list == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_links_list(list);
			dbg_print("\n");
		}
		break;
	case 1:
		if (list != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (sysdir != NULL)
		sysfs_close_directory(sysdir);

	return 0;
}

/**
 * extern struct dlist *sysfs_get_dir_subdirs(struct sysfs_directory *dir);
 *
 * flag:
 * 	0:	dir -> valid
 * 	1:	dir -> valid
 */
int test_sysfs_get_dir_subdirs(int flag)
{
	struct sysfs_directory *sysdir = NULL;
	struct dlist *list = NULL;

	switch (flag) {
	case 0:
		sysdir = sysfs_open_directory(val_dir_path);
		if (sysdir == NULL) {
			dbg_print("%s: failed opening directory at %s\n",
					__FUNCTION__, val_dir_path);
			return 0;
		}
		break;
	case 1:
		sysdir = NULL;
		break;
	default:
		return -1;
	}
	list = sysfs_get_dir_subdirs(sysdir);

	switch (flag) {
	case 0:
		if (list == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
                        dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_dir_list(list);
			dbg_print("\n");
		}
		break;
	case 1:
		if (list != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
                        dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		break;
	default:
		break;
	}

	if (sysdir != NULL)
		sysfs_close_directory(sysdir);

	return 0;
}
