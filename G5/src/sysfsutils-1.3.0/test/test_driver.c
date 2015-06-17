/*
 * test_driver.c
 *
 * Tests for driver related functions for the libsysfs testsuite
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
 * this will test the driver related functions provided by libsysfs.
 *
 * extern void sysfs_close_driver(struct sysfs_driver *driver);
 * extern struct sysfs_driver *sysfs_open_driver
 * 	(const char *bus_name, const char *drv_name);
 * extern struct sysfs_driver *sysfs_open_driver_path
 * 					(const char *path);
 * extern struct sysfs_attribute *sysfs_get_driver_attr
 * 		(struct sysfs_driver *drv, const char *name);
 * extern struct dlist *sysfs_get_driver_attributes
 * 					(struct sysfs_driver *driver);
 * extern struct dlist *sysfs_get_driver_devices(struct sysfs_driver *driver);
 * extern struct dlist *sysfs_refresh_driver_devices
 * 					(struct sysfs_driver *driver);
 * extern struct dlist *sysfs_get_driver_links(struct sysfs_driver *driver);
 * extern struct sysfs_device *sysfs_get_driver_device
 * 	(struct sysfs_driver *driver, const char *name);
 * extern struct dlist *sysfs_refresh_driver_attributes
 * 					(struct sysfs_driver *driver);
 * extern struct sysfs_attribute *sysfs_open_driver_attr
 * 		(const char *bus, const char *drv,
 * 					const char *attrib);
 *
 ******************************************************************************
 */

#include "test-defs.h"
#include <errno.h>

/**
 * extern void sysfs_close_driver(struct sysfs_driver *driver);
 *
 * flag:
 * 	0:	driver -> valid
 * 	1:	driver -> NULL
 */
int test_sysfs_close_driver(int flag)
{
	struct sysfs_driver *driver = NULL;

	switch (flag) {
	case 0:
		driver = sysfs_open_driver_path(val_drv_path);
		if (driver == NULL)
			return 0;
		break;
	case 1:
		driver = NULL;
		break;
	default:
		return -1;
	}
	sysfs_close_driver(driver);

	dbg_print("%s: returns void\n", __FUNCTION__);
	return 0;
}

/**
 * extern struct sysfs_driver *sysfs_open_driver_path
 * 					(const char *path);
 *
 * flag:
 * 	0:	path -> valid
 * 	1:	path -> invalid
 * 	2:	path -> NULL
 */
int test_sysfs_open_driver_path(int flag)
{
	struct sysfs_driver *driver = NULL;
	char *path = NULL;

	switch (flag) {
	case 0:
		path = val_drv_path;
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
	driver = sysfs_open_driver_path(path);

	switch (flag) {
	case 0:
		if (driver == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
			dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_driver(driver);
			dbg_print("\n");
		}
		break;
	case 1:
	case 2:
		if (driver != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
	default:
		break;
	}
	if (driver != NULL)
		sysfs_close_driver(driver);

	return 0;
}

/**
 * extern struct sysfs_driver *sysfs_open_driver
 * 	(const char *bus_name, const char *drv_name);
 *
 * flag:
 * 	0:	path -> valid, name -> valid
 * 	1:	path -> valid, name -> invalid
 * 	2:	path -> valid, name -> NULL
 * 	3:	path -> invalid, name -> valid
 * 	4:	path -> invalid, name -> invalid
 * 	5:	path -> invalid, name -> NULL
 * 	6:	path -> NULL, name -> valid
 * 	7:	path -> NULL, name -> invalid
 * 	8:	path -> NULL, name -> NULL
 */
int test_sysfs_open_driver(int flag)
{
	struct sysfs_driver *driver = NULL;
	char *bus_name = NULL;
	char *drv_name = NULL;

	switch (flag) {
	case 0:
		bus_name = val_drv_bus_name;
		drv_name = val_drv_name;
		dbg_print("bus_name = %s, drv_name = %s\n", bus_name, drv_name);
		break;
	case 1:
		bus_name = val_drv_bus_name;
		drv_name = inval_name;
		break;
	case 2:
		bus_name = val_drv_bus_name;
		drv_name = NULL;
		break;
	case 3:
		bus_name = inval_name;
		drv_name = val_drv_name;
		break;
	case 4:
		bus_name = inval_name;
		drv_name = inval_name;
		break;
	case 5:
		bus_name = inval_name;
		drv_name = NULL;
		break;
	case 6:
		bus_name = NULL;
		drv_name = val_drv_name;
		break;
	case 7:
		bus_name = NULL;
		drv_name = inval_name;
		break;
	case 8:
		bus_name = NULL;
		drv_name = NULL;
		break;

	default:
		return -1;

	}
	driver = sysfs_open_driver(bus_name, drv_name);
	switch (flag) {
	case 0:
		if (driver == NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else {
			dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_driver(driver);
			dbg_print("\n");
		}
		break;
	case 1:
	case 2:
		if (driver != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
	default:
		break;
	}
	if (driver != NULL)
		sysfs_close_driver(driver);

	return 0;
}

/**
 * extern struct sysfs_attribute *sysfs_get_driver_attr
 * 		(struct sysfs_driver *drv, const char *name);
 *
 * flag:
 * 	0:	drv -> valid, name -> valid
 * 	1:	drv -> valid, name -> invalid
 * 	2:	drv -> valid, name -> NULL
 * 	3:	drv -> NULL, name -> valid
 * 	4:	drv -> NULL, name -> invalid
 * 	5:	drv -> NULL, name -> NULL
 */
int test_sysfs_get_driver_attr(int flag)
{
	char *name = NULL;
	char *attrname = NULL;
	struct sysfs_driver *drv = NULL;
	struct sysfs_attribute *attr = NULL;

	switch (flag) {
	case 0:
		name = val_drv_path;
		drv = sysfs_open_driver_path(name);
		if (drv == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, name);
			return 0;
		}
		attrname = val_drv_attr_name;
		break;
	case 1:
		name = val_drv_path;
		drv = sysfs_open_driver_path(name);
		if (drv == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, name);
			return 0;
		}
		attrname = inval_name;
		break;
	case 2:
		name = val_drv_path;
		drv = sysfs_open_driver_path(name);
		if (drv == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, name);
			return 0;
		}
		attrname = NULL;
		break;
	case 3:
		drv = NULL;
		attrname = val_drv_attr_name;
		break;
	case 4:
		drv = NULL;
		attrname = inval_name;
		break;
	case 5:
		drv = NULL;
		attrname = NULL;
		break;
	default:
		return -1;
	}

	attr = sysfs_get_driver_attr(drv, attrname);
	switch (flag) {
	case 0:
		if (attr == NULL) {
			if (errno == EACCES)
				dbg_print("%s: attribute %s does not support READ\n",
						__FUNCTION__, attrname);
			else if (errno == ENOENT)
				dbg_print("%s: attribute %s not defined for driver at %s\n",
						__FUNCTION__, attrname, name);
			else if (errno == 0)
				dbg_print("%s: driver at %s does not export attributes\n",
						__FUNCTION__, val_drv_path);
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
		else
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
	default:
		break;
	}
	if (drv != NULL)
		sysfs_close_driver(drv);

	return 0;
}

/**
 * extern struct dlist *sysfs_get_driver_attributes
 * 				(struct sysfs_driver *driver);
 *
 * flag:
 * 	0:	driver -> valid
 * 	1:	driver -> NULL
 */
int test_sysfs_get_driver_attributes(int flag)
{
	struct sysfs_driver *driver = NULL;
	struct dlist *list = NULL;
	char *drv = NULL;

	switch (flag) {
	case 0:
		drv = val_drv_path;
		driver = sysfs_open_driver_path(drv);
		if (driver == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, val_drv_path);
			return 0;
		}
		break;
	case 1:
		driver = NULL;
		break;
	case 2:
		drv = val_drv1_path;
		driver = sysfs_open_driver_path(drv);
		if (driver == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, val_drv1_path);
			return 0;
		}
		break;
	default:
		return -1;
	}
	list = sysfs_get_driver_attributes(driver);

	switch (flag) {
	case 0:
	case 2:
		if (list == NULL) {
			if (errno == 0)
				dbg_print("%s: No attributes are defined for the driver at %s\n",
						__FUNCTION__, drv);
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
		else
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
	default:
		break;
	}
	if (driver != NULL)
		sysfs_close_driver(driver);

	return 0;
}

/**
 * extern struct dlist *sysfs_get_driver_devices(struct sysfs_driver *driver);
 *
 * flag:
 * 	0:	driver -> valid
 * 	1:	driver -> NULL
 */
int test_sysfs_get_driver_devices(int flag)
{
	struct sysfs_driver *driver = NULL;
	struct dlist *list = NULL;
	char *drv = NULL;

	switch (flag) {
	case 0:
		drv = val_drv_path;
		driver = sysfs_open_driver_path(drv);
		if (driver == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, val_drv_path);
			return 0;
		}
		break;
	case 1:
		driver = NULL;
		break;
	case 2:
		drv = val_drv1_path;
		driver = sysfs_open_driver_path(drv);
		if (driver == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, val_drv1_path);
			return 0;
		}
		break;
	default:
		return -1;
	}
	list = sysfs_get_driver_devices(driver);

	switch (flag) {
	case 0:
	case 2:
		if (list == NULL) {
			if (errno == 0)
				dbg_print("%s: No devices are using the driver at %s\n",
						__FUNCTION__, drv);
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
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
	default:
		break;

	}
	if (driver != NULL)
		sysfs_close_driver(driver);

	return 0;
}

/**
 * extern struct dlist *sysfs_refresh_driver_devices
 * 					(struct sysfs_driver *driver);
 *
 * flag:
 * 	0:	driver -> valid
 * 	1:	driver -> NULL
 */
int test_sysfs_refresh_driver_devices(int flag)
{
	struct sysfs_driver *driver = NULL;
	struct dlist *list = NULL;

	switch (flag) {
	case 0:
		driver = sysfs_open_driver_path(val_drv_path);
		if (driver == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, val_drv_path);
			return 0;
		}
		list = sysfs_get_driver_devices(driver);
		if (list == NULL) {
			dbg_print("%s: Error getting driver devices for %s\n",
					__FUNCTION__, driver->name);
			sysfs_close_driver(driver);
			return 0;
		}
		break;
	case 1:
		driver = NULL;
		break;
	default:
		return -1;
	}
	list = sysfs_refresh_driver_devices(driver);

	switch (flag) {
	case 0:
		if (list == NULL) {
			if (errno == 0)
				dbg_print("%s: No devices are using the driver at %s\n",
						__FUNCTION__, val_drv_path);
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
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
	default:
		break;

	}
	if (driver != NULL)
		sysfs_close_driver(driver);

	return 0;
}

/**
 * extern struct dlist *sysfs_get_driver_links(struct sysfs_driver *driver);
 *
 * flag:
 * 	0:	driver -> valid
 * 	1:	driver -> NULL
 */
int test_sysfs_get_driver_links(int flag)
{
	struct sysfs_driver *driver = NULL;
	struct dlist *list = NULL;

	switch (flag) {
	case 0:
		driver = sysfs_open_driver_path(val_drv_path);
		if (driver == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, val_drv_path);
			return 0;
		}
		break;
	case 1:
		driver = NULL;
		break;
	default:
		return -1;
	}
	list = sysfs_get_driver_links(driver);

	switch (flag) {
	case 0:
		if (list == NULL) {
			if (errno == 0)
				dbg_print("%s: Driver at %s does not have any links\n",
						__FUNCTION__, val_drv_path);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		} else {
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
	default:
		break;

	}
	if (driver != NULL)
		sysfs_close_driver(driver);

	return 0;
}

/**
 * extern struct sysfs_device *sysfs_get_driver_device
 * 	(struct sysfs_driver *driver, const char *name);
 *
 * flag:
 * 	0:	driver -> valid, name -> valid
 * 	1:	driver -> valid, name -> invalid
 * 	2:	driver -> valid, name -> NULL
 * 	3:	driver -> NULL, name -> valid
 * 	4:	driver -> NULL, name -> invalid
 * 	5:	driver -> NULL, name -> NULL
 */
int test_sysfs_get_driver_device(int flag)
{
	struct sysfs_driver *driver = NULL;
	struct sysfs_device *dev = NULL;
	char *name = NULL;

	switch (flag) {
	case 0:
		driver = sysfs_open_driver_path(val_drv_path);
		if (driver == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, val_drv_path);
			return 0;
		}
		name = val_drv_dev_name;
		break;
	case 1:
		driver = sysfs_open_driver_path(val_drv_path);
		if (driver == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, val_drv_path);
			return 0;
		}
		name = inval_name;
		break;
	case 2:
		driver = sysfs_open_driver_path(val_drv_path);
		if (driver == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, val_drv_path);
			return 0;
		}
		name = NULL;
		break;
	case 3:
		driver = NULL;
		name = val_drv_dev_name;
		break;
	case 4:
		driver = NULL;
		name = inval_name;
		break;
	case 5:
		driver = NULL;
		name = NULL;
		break;
	default:
		return -1;
	}
	dev = sysfs_get_driver_device(driver, name);

	switch (flag) {
	case 0:
		if (dev == NULL) {
			if (errno == 0)
				dbg_print("%s: Device with name %s does not use the driver at %s\n",
						__FUNCTION__, name, val_drv_path);
			else
				dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		} else {
			dbg_print("%s: SUCCEEDED with flag = %d\n\n",
						__FUNCTION__, flag);
			show_device(dev);
			dbg_print("\n");
		}
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if (dev != NULL)
			dbg_print("%s: FAILED with flag = %d errno = %d\n",
						__FUNCTION__, flag, errno);
		else
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
	default:
		break;

	}
	if (driver != NULL)
		sysfs_close_driver(driver);

	return 0;
}

/**
 * extern struct dlist *sysfs_refresh_driver_attributes
 * 					(struct sysfs_driver *driver);
 * flag:
 * 	0:	driver -> valid
 * 	1:	driver -> NULL
 */
int test_sysfs_refresh_driver_attributes(int flag)
{
	struct sysfs_driver *driver = NULL;
	struct dlist *list = NULL;

	switch (flag) {
	case 0:
		driver = sysfs_open_driver_path(val_drv_path);
		if (driver == NULL) {
			dbg_print("%s: failed opening driver at %s\n",
						__FUNCTION__, val_drv_path);
			return 0;
		}
		break;
	case 1:
		driver = NULL;
		break;
	default:
		return -1;
	}
	list = sysfs_refresh_driver_attributes(driver);

	switch (flag) {
	case 0:
		if (list == NULL) {
			if (errno == 0)
				dbg_print("%s: driver at %s does not export attributes\n",
						__FUNCTION__, val_drv_path);
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
		else
			dbg_print("%s: SUCCEEDED with flag = %d\n",
						__FUNCTION__, flag);
	default:
		break;

	}
	if (driver != NULL)
		sysfs_close_driver(driver);

	return 0;
}

/**
 * extern struct sysfs_attribute *sysfs_open_driver_attr
 * 			(const char *bus, const char *drv,
 * 			const char *attrib);
 *
 * flag:
 * 	0: bus -> valid, drv -> valid, attrib -> valid
 * 	1: bus -> valid, drv -> valid, attrib -> invalid
 * 	2: bus -> valid, drv -> valid, attrib -> NULL
 * 	3: bus -> valid, drv -> invalid, attrib -> valid
 * 	4: bus -> valid, drv -> invalid, attrib -> invalid
 * 	5: bus -> valid, drv -> invalid, attrib -> NULL
 * 	6: bus -> valid, drv -> NULL, attrib -> valid
 * 	7: bus -> valid, drv -> NULL, attrib -> invalid
 * 	8: bus -> valid, drv -> NULL, attrib -> NULL
 * 	9: bus -> invalid, drv -> valid, attrib -> valid
 * 	10: bus -> invalid, drv -> valid, attrib -> invalid
 * 	11: bus -> invalid, drv -> valid, attrib -> NULL
 * 	12: bus -> invalid, drv -> invalid, attrib -> valid
 * 	13: bus -> invalid, drv -> invalid, attrib -> invalid
 * 	14: bus -> invalid, drv -> invalid, attrib -> NULL
 * 	15: bus -> invalid, drv -> NULL, attrib -> valid
 * 	16: bus -> invalid, drv -> NULL, attrib -> invalid
 * 	17: bus -> invalid, drv -> NULL, attrib -> NULL
 * 	18: bus -> NULL, drv -> valid, attrib -> valid
 * 	19: bus -> NULL, drv -> valid, attrib -> invalid
 * 	20: bus -> NULL, drv -> valid, attrib -> NULL
 * 	21: bus -> NULL, drv -> invalid, attrib -> valid
 * 	22: bus -> NULL, drv -> invalid, attrib -> invalid
 * 	23: bus -> NULL, drv -> invalid, attrib -> NULL
 * 	24: bus -> NULL, drv -> NULL, attrib -> valid
 * 	25: bus -> NULL, drv -> NULL, attrib -> invalid
 * 	26: bus -> NULL, drv -> NULL, attrib -> NULL
 */
int  test_sysfs_open_driver_attr(int flag)
{

	char *bus = NULL;
	char *drv = NULL;
	char *attrib = NULL;
	struct sysfs_attribute *attr = NULL;

	switch (flag) {
	case 0:
		bus = val_drv_bus_name;
		drv = val_drv_name;
		attrib = val_drv_attr_name;
		break;
	case 1:
		bus = val_drv_bus_name;
		drv = val_drv_name;
		attrib = inval_name;
		break;
	case 2:
		bus = val_drv_bus_name;
		drv = val_drv_name;
		attrib = NULL;
		break;
	case 3:
		bus = val_drv_bus_name;
		drv = inval_name;
		attrib = val_drv_attr_name;
		break;
	case 4:
		bus = val_drv_bus_name;
		drv = inval_name;
		attrib = inval_name;
		break;
	case 5:
		bus = val_drv_bus_name;
		drv = inval_name;
		attrib = NULL;
		break;
	case 6:
		bus = val_drv_bus_name;
		drv = NULL;
		attrib = val_drv_attr_name;
		break;
	case 7:
		bus = val_drv_bus_name;
		drv = NULL;
		attrib = inval_name;
		break;
	case 8:
		bus = val_drv_bus_name;
		drv = NULL;
		attrib = NULL;
		break;
	case 9:
		bus = inval_name;
		drv = val_drv_name;
		attrib = val_drv_attr_name;
		break;
	case 10:
		bus = inval_name;
		drv = val_drv_name;
		attrib = inval_name;
		break;
	case 11:
		bus = inval_name;
		drv = val_drv_name;
		attrib = NULL;
		break;
	case 12:
		bus = inval_name;
		drv = inval_name;
		attrib = val_drv_attr_name;
		break;
	case 13:
		bus = inval_name;
		drv = inval_name;
		attrib = inval_name;
		break;
	case 14:
		bus = inval_name;
		drv = inval_name;
		attrib = NULL;
		break;
	case 15:
		bus = inval_name;
		drv = NULL;
		attrib = val_drv_attr_name;
		break;
	case 16:
		bus = inval_name;
		drv = NULL;
		attrib = inval_name;
		break;
	case 17:
		bus = inval_name;
		drv = NULL;
		attrib = NULL;
		break;
	case 18:
		bus = NULL;
		drv = val_drv_name;
		attrib = val_drv_attr_name;
		break;
	case 19:
		bus = NULL;
		drv = val_drv_name;
		attrib = inval_name;
		break;
	case 20:
		bus = NULL;
		drv = val_drv_name;
		attrib = NULL;
		break;
	case 21:
		bus = NULL;
		drv = inval_name;
		attrib = val_drv_attr_name;
		break;
	case 22:
		bus = NULL;
		drv = inval_name;
		attrib = inval_name;
		break;
	case 23:
		bus = NULL;
		drv = inval_name;
		attrib = NULL;
		break;
	case 24:
		bus = NULL;
		drv = NULL;
		attrib = val_drv_attr_name;
		break;
	case 25:
		bus = NULL;
		drv = NULL;
		attrib = inval_name;
		break;
	case 26:
		bus = NULL;
		drv = NULL;
		attrib = NULL;
		break;
	case 27:
		bus = val_drv1_bus_name;
		drv = val_drv1_name;
		attrib = val_drv1_attr_name;
		break;
	default:
		return -1;
	}
	attr = sysfs_open_driver_attr(bus, drv, attrib);

	switch (flag) {
	case 0:
	case 27:
		if (attr == NULL) {
			if (errno == EACCES)
				dbg_print("%s: attribute %s does not support READ\n",
						__FUNCTION__, attrib);
			else if (errno == ENOENT)
				dbg_print("%s: attribute %s not defined for driver %s\n",
						__FUNCTION__, attrib, drv);
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
