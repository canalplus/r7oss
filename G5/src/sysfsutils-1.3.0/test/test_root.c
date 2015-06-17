/*
 * test_root.c
 *
 * Tests for device/root device related functions for the libsysfs testsuite
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
 ******************************************************************************
 * this will test the device/root device related functions provided by libsysfs
 *
 * extern void sysfs_close_root_device(struct sysfs_root_device *root);
 * extern struct sysfs_root_device *sysfs_open_root_device
 * 						(const char *name);
 * extern struct dlist *sysfs_get_root_devices(struct sysfs_root_device *root);
 * extern void sysfs_close_device(struct sysfs_device *dev);
 * extern struct sysfs_device *sysfs_open_device
 * 		(const char *bus, const char *bus_id);
 * extern struct sysfs_device *sysfs_get_device_parent
 * 					(struct sysfs_device *dev);
 * extern struct sysfs_device *sysfs_open_device_path
 * 					(const char *path);
 * extern struct sysfs_attribute *sysfs_get_device_attr
 * 			(struct sysfs_device *dev, const char *name);
 * extern struct dlist *sysfs_get_device_attributes
 * 					(struct sysfs_device *device);
 * extern struct dlist *sysfs_refresh_device_attributes
 * 					(struct sysfs_device *device);
 * extern struct sysfs_attribute *sysfs_open_device_attr
 * 				(const char *bus,
 * 				const char *bus_id,
 * 				const char *attrib);
 *
 ******************************************************************************
 */

#include "test-defs.h"
#include <errno.h>

/**
 * extern void sysfs_close_root_device(struct sysfs_root_device *root);
 *
 * flag:
 * 	0:	root -> valid
 * 	1:	root -> NULL
 */
int test_sysfs_close_root_device(int flag)
{
	struct sysfs_root_device *root = NULL;
	char *root_name = NULL;

	switch (flag) {
	case 0:
		root_name = val_root_name;
		root = sysfs_open_root_device(root_name);
		if (root == NULL) {
			dbg_print("%s: failed to open root device %s\n",
					__FUNCTION__, root_name);
			return 0;
		}
		break;
	case 1:
		root = NULL;
		break;
	default:
		return -1;

	}
	sysfs_close_root_device(root);

	dbg_print("%s: returns void\n", __FUNCTION__);
	return 0;
}

/**
 * extern struct sysfs_root_device *sysfs_open_root_device
 * 						(const char *name);
 *
 * flag:
 * 	0:	name -> valid
 * 	1:	name -> invalid
 * 	2:	name -> NULL
 */
int test_sysfs_open_root_device(int flag)
{
	struct sysfs_root_device *root = NULL;
	char *name = NULL;

	switch (flag) {
	case 0:
		name = val_root_name;
		break;
	case 1:
		name = inval_name;
		break;
	case 2:
		name = NULL;
		break;
	default:
		return -1;
	}
	root = sysfs_open_root_device(name);

	switch (flag) {
	case 0:
		if (root == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
			dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_root_device(root);
			dbg_print("\n");
		}
		break;
	case 1:
	case 2:
		if (root != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
			if (errno == EINVAL)
				dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		}
	default:
		break;
	}
	if (root != NULL)
		sysfs_close_root_device(root);
	return 0;
}

/**
 * extern struct dlist *sysfs_get_root_devices(struct sysfs_root_device *root);
 *
 * flag:
 * 	0:	root -> valid
 * 	1:	root -> NULL
 */
int test_sysfs_get_root_devices(int flag)
{
	struct sysfs_root_device *root = NULL;
	struct dlist *list = NULL;
	char *root_name = NULL;

	switch (flag) {
	case 0:
		root_name = val_root_name;
		root = sysfs_open_root_device(root_name);
		if (root == NULL) {
			dbg_print("%s: failed to open root device %s\n",
					__FUNCTION__, root_name);
			return 0;
		}
		break;
	case 1:
		root = NULL;
		break;
	default:
		return -1;

	}
	list = sysfs_get_root_devices(root);

	switch (flag) {
	case 0:
		if (list == NULL) {
			if (errno == 0)
				dbg_print("%s: Root device %s does not have any devices under it\n",
						__FUNCTION__, root_name);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		} else {
			dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_device_list(list);
			dbg_print("\n");
		}
		break;
	case 1:
		if (list != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
			if (errno == EINVAL)
				dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
	default:
		break;
	}

	if (root != NULL)
		sysfs_close_root_device(root);

	return 0;
}

/**
 * extern void sysfs_close_device(struct sysfs_device *dev);
 *
 * flag:
 * 	0:	dev -> valid
 * 	1:	dev -> NULL
 */
int test_sysfs_close_device(int flag)
{
	struct sysfs_device *dev = NULL;
	char *path = NULL;

	switch (flag) {
	case 0:
		path = val_dev_path;
		dev = sysfs_open_device_path(path);
		if (dev == NULL) {
			dbg_print("%s: failed to open device at %s\n",
						__FUNCTION__, path);
			return 0;
		}
		break;
	case 1:
		dev = NULL;
		break;
	default:
		return -1;
	}
	sysfs_close_device(dev);

	dbg_print("%s: returns void\n", __FUNCTION__);
	return 0;
}

/**
 * extern struct sysfs_device *sysfs_open_device
 * 		(const char *bus, const char *bus_id);
 *
 * flag:
 * 	0:	bus -> valid, bus_id -> valid
 * 	1:	bus -> valid, bus_id -> invalid
 * 	2:	bus -> valid, bus_id -> invalid
 * 	3:	bus -> invalid, bus_id -> valid
 * 	4:	bus -> invalid, bus_id -> invalid
 * 	5:	bus -> invalid, bus_id -> invalid
 * 	6:	bus -> NULL, bus_id -> valid
 * 	7:	bus -> NULL, bus_id -> invalid
 * 	8:	bus -> NULL, bus_id -> invalid
 */
int test_sysfs_open_device(int flag)
{
	struct sysfs_device *dev = NULL;
	char *bus = NULL;
	char *bus_id = NULL;

	switch (flag) {
	case 0:
		bus = val_bus_name;
		bus_id = val_bus_id;
		break;
	case 1:
		bus = val_bus_name;
		bus_id = inval_name;
		break;
	case 2:
		bus = val_bus_name;
		bus_id = NULL;
		break;
	case 3:
		bus = inval_name;
		bus_id = val_bus_id;
		break;
	case 4:
		bus = inval_name;
		bus_id = inval_name;
		break;
	case 5:
		bus = inval_name;
		bus_id = NULL;
		break;
	case 6:
		bus = NULL;
		bus_id = val_bus_id;
		break;
	case 7:
		bus = NULL;
		bus_id = inval_name;
		break;
	case 8:
		bus = NULL;
		bus_id = NULL;
		break;
	default:
		return -1;
	}
	dev = sysfs_open_device(bus, bus_id);

	switch (flag) {
	case 0:
		if (dev == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
			dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_device(dev);
			dbg_print("\n");
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
		if (dev == NULL)
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		else
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		break;
	}

	if (dev != NULL)
		sysfs_close_device(dev);
	return 0;
}

/**
 * extern struct sysfs_device *sysfs_get_device_parent
 * 					(struct sysfs_device *dev);
 *
 * flag:
 * 	0:	dev -> valid
 * 	1:	dev -> NULL
 */
int test_sysfs_get_device_parent(int flag)
{
	struct sysfs_device *pdev = NULL;
	struct sysfs_device *dev = NULL;
	char *dev_path = NULL;

	switch (flag) {
	case 0:
		dev_path = val_dev_path;
		dev = sysfs_open_device_path(dev_path);
		if (dev == NULL) {
			dbg_print("%s: failed to open device at %s\n",
					__FUNCTION__, dev_path);
			return 0;
		}
		break;
	case 1:
		dev = NULL;
		break;
	default:
		return -1;

	}
	pdev = sysfs_get_device_parent(dev);

	switch (flag) {
	case 0:
		if (pdev == NULL) {
			if (errno == 0)
				dbg_print("%s: Device at %s does not have a parent\n",
						__FUNCTION__, dev_path);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		} else {
			dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_device(pdev);
			dbg_print("\n");
		}
		break;
	case 1:
		if (pdev == NULL)
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		else
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
	default:
		break;
	}
	if (dev != NULL) {
		sysfs_close_device(dev);
	}
	return 0;
}

/**
 * extern struct sysfs_device *sysfs_open_device_path
 * 					(const char *path);
 *
 * flag:
 * 	0:	path -> valid
 * 	1:	path -> invalid
 * 	2:	path -> NULL
 *
 */
int test_sysfs_open_device_path(int flag)
{
	struct sysfs_device *dev = NULL;
	char *path = NULL;

	switch (flag) {
	case 0:
		path = val_dev_path;
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
	dev = sysfs_open_device_path(path);

	switch (flag) {
	case 0:
		if (dev == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
			dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_device(dev);
			dbg_print("\n");
		}
		break;
	case 1:
	case 2:
		if (dev == NULL)
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
		else
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
	default:
		break;
	}
	if (dev != NULL)
		sysfs_close_device(dev);
	return 0;
}

/**
 * extern struct sysfs_attribute *sysfs_get_device_attr
 * 		(struct sysfs_device *dev, const char *name);
 *
 * flag:
 * 	0:	dev -> valid, name -> valid
 * 	1:	dev -> valid, name -> invalid
 * 	2:	dev -> valid, name -> NULL
 * 	3:	dev -> NULL, name -> valid
 * 	4:	dev -> NULL, name -> invalid
 * 	5:	dev -> NULL, name -> NULL
 */
int test_sysfs_get_device_attr(int flag)
{
	struct sysfs_device *dev = NULL;
	char *name = NULL;
	char *path = NULL;
	struct sysfs_attribute *attr = NULL;

	switch (flag) {
	case 0:
		path = val_dev_path;
		dev = sysfs_open_device_path(path);
		if (dev == NULL) {
			dbg_print("%s: failed to open device at %s\n",
					__FUNCTION__, path);
			return 0;
		}
		name = val_dev_attr;
		break;
	case 1:
		dev = sysfs_open_device_path(path);
		name = inval_name;
		break;
	case 2:
		dev = sysfs_open_device_path(path);
		name = NULL;
		break;
	case 3:
		dev = NULL;
		name = val_dev_attr;
		break;
	case 4:
		dev = NULL;
		name = inval_name;
		break;
	case 5:
		dev = NULL;
		name = NULL;
		break;
	default:
		return -1;
	}
	attr = sysfs_get_device_attr(dev, name);

	switch (flag) {
	case 0:
		if (attr == NULL) {
			if (errno == EACCES)
				dbg_print("%s: attribute %s does not support READ\n",
						__FUNCTION__, name);
			else if (errno == ENOENT)
				dbg_print("%s: attribute %s not defined for device at %s\n",
						__FUNCTION__, name, path);
			else if (errno == 0)
				dbg_print("%s: device at %s does not export attributes\n",
						__FUNCTION__, path);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);

		} else {
			dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_attribute(attr);
			dbg_print("\n");
		}
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if (attr != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
			if (errno == EINVAL)
				dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		}
	default:
		break;
	}

	if (dev != NULL)
		sysfs_close_device(dev);
	return 0;
}

/**
 * extern struct dlist *sysfs_get_device_attributes
 * 					(struct sysfs_device *device);
 *
 * flag:
 * 	0:	device -> valid
 * 	1:	device -> NULL
 */
int test_sysfs_get_device_attributes(int flag)
{
	struct sysfs_device *device = NULL;
	struct dlist *list = NULL;
	char *path = NULL;

	switch (flag) {
	case 0:
		path = val_dev_path;
		device = sysfs_open_device_path(path);
		if (device == NULL) {
			dbg_print("%s: failed to open device at %s\n",
					__FUNCTION__, path);
			return 0;
		}
		break;
	case 1:
		device = NULL;
		break;
	default:
		return -1;
	}
	list = sysfs_get_device_attributes(device);

	switch (flag) {
	case 0:
		if (list == NULL) {
			if (errno == 0)
				dbg_print("%s: device at %s does not export attributes\n",
						__FUNCTION__, path);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		} else {
			dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_attribute_list(list);
			dbg_print("\n");
		}
		break;
	case 1:
		if (errno != EINVAL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
	default:
		break;
	}
	if (device != NULL)
		sysfs_close_device(device);
	return 0;
}

/**
 * extern struct dlist *sysfs_refresh_device_attributes
 * 					(struct sysfs_device *device);
 *
 * flag:
 * 	0:	device -> valid
 * 	1:	device -> NULL
 *
 */
int test_sysfs_refresh_device_attributes(int flag)
{
	struct sysfs_device *device = NULL;
	struct dlist *list = NULL;
	char *path = NULL;

	switch (flag) {
	case 0:
		path = val_dev_path;
		device = sysfs_open_device_path(path);
		if (device == NULL) {
			dbg_print("%s: failed to open device at %s\n",
					__FUNCTION__, path);
			return 0;
		}
		list = sysfs_get_device_attributes(device);
		if (list == NULL) {
			dbg_print("%s: failed to get device attribs for %s\n",
					__FUNCTION__, device->name);
			sysfs_close_device(device);
			return 0;
		}
		break;
	case 1:
		device = NULL;
		break;
	default:
		return -1;
	}
	list = sysfs_refresh_device_attributes(device);

	switch (flag) {
	case 0:
		if (list == NULL) {
			if (errno == 0)
				dbg_print("%s: device at %s does not export attributes\n",
						__FUNCTION__, path);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		} else {
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
		else {
			if (errno == EINVAL)
				dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		}
	default:
		break;
	}
	if (device != NULL)
		sysfs_close_device(device);
	return 0;
}

/**
 * extern struct sysfs_attribute *sysfs_open_device_attr
 * 				(const char *bus,
 * 				const char *bus_id,
 * 				const char *attrib);
 *
 * flag:
 * 	0: bus -> valid, bus_id -> valid, attrib -> valid
 * 	1: bus -> valid, bus_id -> valid, attrib -> invalid
 * 	2: bus -> valid, bus_id -> valid, attrib -> NULL
 * 	3: bus -> valid, bus_id -> invalid, attrib -> valid
 * 	4: bus -> valid, bus_id -> invalid, attrib -> invalid
 * 	5: bus -> valid, bus_id -> invalid, attrib -> NULL
 * 	6: bus -> valid, bus_id -> NULL, attrib -> valid
 * 	7: bus -> valid, bus_id -> NULL, attrib -> invalid
 * 	8: bus -> valid, bus_id -> NULL, attrib -> NULL
 * 	9: bus -> invalid, bus_id -> valid, attrib -> valid
 * 	10: bus -> invalid, bus_id -> valid, attrib -> invalid
 * 	11: bus -> invalid, bus_id -> valid, attrib -> NULL
 * 	12: bus -> invalid, bus_id -> invalid, attrib -> valid
 * 	13: bus -> invalid, bus_id -> invalid, attrib -> invalid
 * 	14: bus -> invalid, bus_id -> invalid, attrib -> NULL
 * 	15: bus -> invalid, bus_id -> NULL, attrib -> valid
 * 	16: bus -> invalid, bus_id -> NULL, attrib -> invalid
 * 	17: bus -> invalid, bus_id -> NULL, attrib -> NULL
 * 	18: bus -> NULL, bus_id -> valid, attrib -> valid
 * 	19: bus -> NULL, bus_id -> valid, attrib -> invalid
 * 	20: bus -> NULL, bus_id -> valid, attrib -> NULL
 * 	21: bus -> NULL, bus_id -> invalid, attrib -> valid
 * 	22: bus -> NULL, bus_id -> invalid, attrib -> invalid
 * 	23: bus -> NULL, bus_id -> invalid, attrib -> NULL
 * 	24: bus -> NULL, bus_id -> NULL, attrib -> valid
 * 	25: bus -> NULL, bus_id -> NULL, attrib -> invalid
 * 	26: bus -> NULL, bus_id -> NULL, attrib -> NULL
 */
int  test_sysfs_open_device_attr(int flag)
{

	char *bus = NULL;
	char *bus_id = NULL;
	char *attrib = NULL;
	struct sysfs_attribute *attr = NULL;

	switch (flag) {
	case 0:
		bus = val_bus_name;
		bus_id = val_bus_id;
		attrib = val_dev_attr;
		break;
	case 1:
		bus = val_bus_name;
		bus_id = val_bus_id;
		attrib = inval_name;
		break;
	case 2:
		bus = val_bus_name;
		bus_id = val_bus_id;
		attrib = NULL;
		break;
	case 3:
		bus = val_bus_name;
		bus_id = inval_name;
		attrib = val_dev_attr;
		break;
	case 4:
		bus = val_bus_name;
		bus_id = inval_name;
		attrib = inval_name;
		break;
	case 5:
		bus = val_bus_name;
		bus_id = inval_name;
		attrib = NULL;
		break;
	case 6:
		bus = val_bus_name;
		bus_id = NULL;
		attrib = val_dev_attr;
		break;
	case 7:
		bus = val_bus_name;
		bus_id = NULL;
		attrib = inval_name;
		break;
	case 8:
		bus = val_bus_name;
		bus_id = NULL;
		attrib = NULL;
		break;
	case 9:
		bus = inval_name;
		bus_id = val_bus_id;
		attrib = val_dev_attr;
		break;
	case 10:
		bus = inval_name;
		bus_id = val_bus_id;
		attrib = inval_name;
		break;
	case 11:
		bus = inval_name;
		bus_id = val_bus_id;
		attrib = NULL;
		break;
	case 12:
		bus = inval_name;
		bus_id = inval_name;
		attrib = val_dev_attr;
		break;
	case 13:
		bus = inval_name;
		bus_id = inval_name;
		attrib = inval_name;
		break;
	case 14:
		bus = inval_name;
		bus_id = inval_name;
		attrib = NULL;
		break;
	case 15:
		bus = inval_name;
		bus_id = NULL;
		attrib = val_dev_attr;
		break;
	case 16:
		bus = inval_name;
		bus_id = NULL;
		attrib = inval_name;
		break;
	case 17:
		bus = inval_name;
		bus_id = NULL;
		attrib = NULL;
		break;
	case 18:
		bus = NULL;
		bus_id = val_bus_id;
		attrib = val_dev_attr;
		break;
	case 19:
		bus = NULL;
		bus_id = val_bus_id;
		attrib = inval_name;
		break;
	case 20:
		bus = NULL;
		bus_id = val_bus_id;
		attrib = NULL;
		break;
	case 21:
		bus = NULL;
		bus_id = inval_name;
		attrib = val_dev_attr;
		break;
	case 22:
		bus = NULL;
		bus_id = inval_name;
		attrib = inval_name;
		break;
	case 23:
		bus = NULL;
		bus_id = inval_name;
		attrib = NULL;
		break;
	case 24:
		bus = NULL;
		bus_id = NULL;
		attrib = val_dev_attr;
		break;
	case 25:
		bus = NULL;
		bus_id = NULL;
		attrib = inval_name;
		break;
	case 26:
		bus = NULL;
		bus_id = NULL;
		attrib = NULL;
		break;
	default:
		return -1;
	}
	attr = sysfs_open_device_attr(bus, bus_id, attrib);

	switch (flag) {
	case 0:
		if (attr == NULL) {
			if (errno == EACCES)
				dbg_print("%s: attribute %s does not support READ\n",
						__FUNCTION__, attrib);
			else if (errno == ENOENT)
				dbg_print("%s: attribute %s not defined for device %s\n",
						__FUNCTION__, attrib, bus_id);
			else if (errno == 0)
				dbg_print("%s: device %s does not export attributes\n",
						__FUNCTION__, bus_id);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);

		} else {
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
	if (attr != NULL)
		sysfs_close_attribute(attr);
	return 0;
}
