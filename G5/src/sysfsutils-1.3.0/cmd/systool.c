/*
 * systool.c
 *
 * Sysfs utility to list buses, classes, and devices
 *
 * Copyright (C) IBM Corp. 2003
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the
 *	Free Software Foundation version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful, but
 *	WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *	General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mntent.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "libsysfs.h"
#include "names.h"

/* Command Options */
static int show_options = 0;		/* bitmask of show options */
static char *attribute_to_show = NULL;	/* show value for this attribute */
static char *device_to_show = NULL;	/* show only this bus device */
struct pci_access *pacc = NULL;
char *show_bus = NULL;

static void show_device(struct sysfs_device *device, int level);
static void show_class_device(struct sysfs_class_device *dev, int level);

#define SHOW_ATTRIBUTES		0x01	/* show attributes command option */
#define SHOW_ATTRIBUTE_VALUE	0x02	/* show an attribute value option */
#define SHOW_DEVICES		0x04	/* show only devices option */
#define SHOW_DRIVERS		0x08	/* show only drivers option */
#define SHOW_ALL_ATTRIB_VALUES	0x10	/* show all attributes with values */
#define SHOW_CHILDREN		0x20	/* show device children */
#define SHOW_PARENT		0x40	/* show device parent */
#define SHOW_PATH		0x80	/* show device/driver path */

#define SHOW_ALL		0xff

static char cmd_options[] = "aA:b:c:CdDhpPr:v";

/*
 * binary_files - defines existing sysfs binary files. These files will be
 * printed in hex.
 */
static char *binary_files[] = {
	"config",
	"data"
};

static int binfiles = 2;

static unsigned int get_pciconfig_word(int offset, unsigned char *buf)
{
        unsigned short val = (unsigned char)buf[offset] |
	                ((unsigned char)buf[offset+1] << 8);
        return val;
}

/**
 * usage: prints utility usage.
 */
static void usage(void)
{
	fprintf(stdout, "Usage: systool [<options> [device]]\n");
	fprintf(stdout, "\t-a\t\t\tShow attributes\n");
	fprintf(stdout, "\t-b <bus_name>\t\tShow a specific bus\n");
	fprintf(stdout, "\t-c <class_name>\t\tShow a specific class\n");
	fprintf(stdout, "\t-d\t\t\tShow only devices\n");
	fprintf(stdout, "\t-h\t\t\tShow usage\n");
	fprintf(stdout, "\t-p\t\t\tShow path to device/driver\n");
	fprintf(stdout,
		"\t-r <root_device>\tShow a specific root device tree\n");
	fprintf(stdout, "\t-v\t\t\tShow all attributes with values\n");
	fprintf(stdout, "\t-A <attribute_name>\tShow attribute value\n");
	fprintf(stdout, "\t-C\t\t\tShow device's children\n");
	fprintf(stdout, "\t-D\t\t\tShow only drivers\n");
	fprintf(stdout, "\t-P\t\t\tShow device's parent\n");
}

/**
 * indent: called before printing a line, it adds indent to the line up to
 *	level passed in.
 * @level: number of spaces to indent.
 */
static void indent(int level)
{
	int i;

	for (i = 0; i < level; i++)
		fprintf(stdout, " ");
}

/**
 * remove_end_newline: removes newline on the end of an attribute value
 * @value: string to remove newline from
 */
static void remove_end_newline(char *value)
{
	char *p = value + (strlen(value) - 1);

	if (p != NULL && *p == '\n')
		*p = '\0';
}

/**
 * show_device_children: prints out device subdirs.
 * @children: dlist of child devices.
 */
static void show_device_children(struct sysfs_device *device, int level)
{
	struct sysfs_device *temp_device = NULL, *child = NULL;
	int flag = 1;

	temp_device = sysfs_open_device_tree(device->path);
	if (temp_device) {
		if (temp_device->children) {
			fprintf(stdout, "\n");
			dlist_for_each_data(temp_device->children, child,
					struct sysfs_device) {
				if (strncmp(child->name, "power", 5) == 0)
					continue;
				if (flag) {
					flag--;
					indent(level);
					fprintf(stdout, "Device \"%s\"'s children\n",
							temp_device->name);
				}
				show_device(child, (level+2));
			}
		}
		sysfs_close_device_tree(temp_device);
	}
}

/**
 * isbinaryvalue: checks to see if attribute is binary or not.
 * @attr: attribute to check.
 * returns 1 if binary, 0 if not.
 */
static int isbinaryvalue(struct sysfs_attribute *attr)
{
	int i;

	if (attr == NULL || attr->value == NULL)
		return 0;

	for (i = 0; i < binfiles; i++)
		if ((strcmp(attr->name, binary_files[i])) == 0)
			return 1;

	return 0;
}

/**
 * show_attribute_value: prints out single attribute value.
 * @attr: attricute to print.
 */
static void show_attribute_value(struct sysfs_attribute *attr, int level)
{
	if (attr == NULL)
		return;

	if (attr->method & SYSFS_METHOD_SHOW) {
		if (isbinaryvalue(attr) != 0) {
			int i;
			for (i = 0; i < attr->len; i++) {
				if (!(i % 16) && (i != 0)) {
					fprintf(stdout, "\n");
					indent(level+22);
				} else if (!(i % 8) && (i != 0))
					fprintf(stdout, " ");
				fprintf(stdout, " %02x",
					(unsigned char)attr->value[i]);
			}
			fprintf(stdout, "\n");

		} else if (attr->value != NULL && strlen(attr->value) > 0) {
			remove_end_newline(attr->value);
			fprintf(stdout, "\"%s\"\n", attr->value);
		} else
			fprintf(stdout, "\n");
	} else {
		fprintf(stdout, "<store method only>\n");
	}
}

/**
 * show_attribute: prints out a single attribute
 * @attr: attribute to print.
 */
static void show_attribute(struct sysfs_attribute *attr, int level)
{
	if (attr == NULL)
		return;

	if (show_options & SHOW_ALL_ATTRIB_VALUES) {
		indent(level);
		fprintf(stdout, "%-20s= ", attr->name);
		show_attribute_value(attr, level);
	} else if ((show_options & SHOW_ATTRIBUTES) || ((show_options
	    & SHOW_ATTRIBUTE_VALUE) && (strcmp(attr->name, attribute_to_show)
	    == 0))) {
		indent(level);
		fprintf (stdout, "%-20s", attr->name);
		if (show_options & SHOW_ATTRIBUTE_VALUE && attr->value
		    != NULL && (strcmp(attr->name, attribute_to_show)) == 0) {
			fprintf(stdout, "= ");
			show_attribute_value(attr, level);
		} else
			fprintf(stdout, "\n");
	}
}

/**
 * show_attributes: prints out a list of attributes.
 * @attributes: print this dlist of attributes/files.
 */
static void show_attributes(struct dlist *attributes, int level)
{
	if (attributes != NULL) {
		struct sysfs_attribute *cur = NULL;

		dlist_for_each_data(attributes, cur,
				struct sysfs_attribute) {
			show_attribute(cur, (level));
		}
	}
}

/**
 * show_device_parent: prints device's parent (if present)
 * @device: sysfs_device whose parent information is needed
 */
static void show_device_parent(struct sysfs_device *device, int level)
{
	struct sysfs_device *parent = NULL;

	parent = sysfs_get_device_parent(device);
	if (parent) {
		fprintf(stdout, "\n");
		indent(level);
		fprintf(stdout, "Device \"%s\"'s parent\n", device->name);
		show_device(parent, (level+2));
	}
}

/**
 * show_device: prints out device information.
 * @device: device to print.
 */
static void show_device(struct sysfs_device *device, int level)
{
	struct dlist *attributes = NULL;
        unsigned int vendor_id, device_id;
        char buf[128], value[256], path[SYSFS_PATH_MAX];

	if (device != NULL) {
		indent(level);
		if (show_bus != NULL && (!(strcmp(show_bus, "pci")))) {
			fprintf(stdout, "%s ", device->bus_id);
			memset(path, 0, SYSFS_PATH_MAX);
			memset(value, 0, SYSFS_PATH_MAX);
			safestrcpy(path, device->path);
			safestrcat(path, "/config");
			if ((sysfs_read_attribute_value(path,
							value, 256)) == 0) {
				vendor_id = get_pciconfig_word
						(PCI_VENDOR_ID, value);
				device_id = get_pciconfig_word
						(PCI_DEVICE_ID, value);
				fprintf(stdout, "%s\n",
					pci_lookup_name(pacc, buf, 128,
					PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
						vendor_id, device_id, 0, 0));
			} else
				fprintf(stdout, "\n");
		} else
			fprintf(stdout, "Device = \"%s\"\n", device->bus_id);

		if (show_options & (SHOW_PATH | SHOW_ALL_ATTRIB_VALUES)) {
			indent(level);
			fprintf(stdout, "Device path = \"%s\"\n",
							device->path);
		}

		if (show_options & (SHOW_ATTRIBUTES | SHOW_ATTRIBUTE_VALUE |
					SHOW_ALL_ATTRIB_VALUES)) {
			attributes = sysfs_get_device_attributes(device);
			if (attributes != NULL)
				show_attributes(attributes, (level+2));
		}
		if ((device_to_show != NULL) &&
				(show_options & SHOW_CHILDREN)) {
			show_options &= ~SHOW_CHILDREN;
			show_device_children(device, (level+2));
		}
		if ((device_to_show != NULL) && (show_options & SHOW_PARENT)) {
			show_options &= ~SHOW_PARENT;
			show_device_parent(device, (level+2));
		}
		if (show_options ^ SHOW_DEVICES)
			if (!(show_options & SHOW_DRIVERS))
				fprintf(stdout, "\n");
	}
}

/**
 * show_driver_attributes: prints out driver attributes .
 * @driver: print this driver's attributes.
 */
static void show_driver_attributes(struct sysfs_driver *driver, int level)
{
	if (driver != NULL) {
		struct dlist *attributes = NULL;

		attributes = sysfs_get_driver_attributes(driver);
		if (attributes != NULL) {
			struct sysfs_attribute *cur = NULL;

			dlist_for_each_data(attributes, cur,
					struct sysfs_attribute) {
				show_attribute(cur, (level));
			}
			fprintf(stdout, "\n");
		}
	}
}

/**
 * show_driver: prints out driver information.
 * @driver: driver to print.
 */
static void show_driver(struct sysfs_driver *driver, int level)
{
	struct dlist *devlist = NULL;

	if (driver != NULL) {
		indent(level);
		fprintf (stdout, "Driver = \"%s\"\n", driver->name);
		if (show_options & (SHOW_PATH | SHOW_ALL_ATTRIB_VALUES)) {
			indent(level);
			fprintf(stdout, "Driver path = \"%s\"\n",
							driver->path);
		}
		if (show_options & (SHOW_ATTRIBUTES | SHOW_ATTRIBUTE_VALUE
			    | SHOW_ALL_ATTRIB_VALUES))
			show_driver_attributes(driver, (level+2));
		devlist = sysfs_get_driver_devices(driver);
		if (devlist != NULL) {
			struct sysfs_device *cur = NULL;

			indent(level+2);
			fprintf(stdout, "Devices using \"%s\" are:\n",
								driver->name);
			dlist_for_each_data(devlist, cur,
					struct sysfs_device) {
				if (show_options & SHOW_DRIVERS) {
					show_device(cur, (level+4));
					fprintf(stdout, "\n");
				} else {
					indent(level+4);
					fprintf(stdout, "\"%s\"\n", cur->name);
				}
			}
		}
		fprintf(stdout, "\n");
	}
}

/**
 * show_device_tree: prints out device tree.
 * @root: root device
 */
static void show_device_tree(struct sysfs_device *root, int level)
{
	if (root != NULL) {
		struct sysfs_device *cur = NULL;

		if (device_to_show == NULL || (strcmp(device_to_show,
			    root->bus_id) == 0)) {
			show_device(root, level);
		}
		if (root->children != NULL) {
			dlist_for_each_data(root->children, cur,
					struct sysfs_device) {
				if (strncmp(cur->name, "power", 5) == 0)
					continue;
				show_device_tree(cur, (level+2));
			}
		}
	}
}

/**
 * show_sysfs_bus: prints out everything on a bus.
 * @busname: bus to print.
 * returns 0 with success or 1 with error.
 */
static int show_sysfs_bus(char *busname)
{
	struct sysfs_bus *bus = NULL;
	struct sysfs_device *curdev = NULL;
	struct sysfs_driver *curdrv = NULL;
	struct dlist *devlist = NULL;
	struct dlist *drvlist = NULL;

	if (busname == NULL) {
		errno = EINVAL;
		return 1;
	}
	bus = sysfs_open_bus(busname);
	if (bus == NULL) {
		fprintf(stderr, "Error opening bus %s\n", busname);
		return 1;
	}

	fprintf(stdout, "Bus = \"%s\"\n", busname);
	if (show_options ^ (SHOW_DEVICES | SHOW_DRIVERS))
		fprintf(stdout, "\n");
	if (show_options & SHOW_DEVICES) {
		devlist = sysfs_get_bus_devices(bus);
		if (devlist != NULL) {
			dlist_for_each_data(devlist, curdev,
						struct sysfs_device) {
				if (device_to_show == NULL ||
						(strcmp(device_to_show,
							curdev->bus_id) == 0))
					show_device(curdev, 2);
			}
		}
	}
	if (show_options & SHOW_DRIVERS) {
		drvlist = sysfs_get_bus_drivers(bus);
		if (drvlist != NULL) {
			dlist_for_each_data(drvlist, curdrv,
					struct sysfs_driver) {
				show_driver(curdrv, 2);
			}
		}
	}
	sysfs_close_bus(bus);
	return 0;
}

/**
 * show_classdev_parent: prints the class device's parent if present
 * @dev: class device whose parent is needed
 */
static void show_classdev_parent(struct sysfs_class_device *dev, int level)
{
	struct sysfs_class_device *parent = NULL;

	parent = sysfs_get_classdev_parent(dev);
	if (parent) {
		fprintf(stdout, "\n");
		indent(level);
		fprintf(stdout, "Class device \"%s\"'s parent is\n",
								dev->name);
		show_class_device(parent, level+2);
	}
}

/**
 * show_class_device: prints out class device.
 * @dev: class device to print.
 */
static void show_class_device(struct sysfs_class_device *dev, int level)
{
	struct dlist *attributes = NULL;
	struct sysfs_device *device = NULL;
	struct sysfs_driver *driver = NULL;

	if (dev != NULL) {
		indent(level);
		fprintf(stdout, "Class Device = \"%s\"\n", dev->name);
		if (show_options & (SHOW_PATH | SHOW_ALL_ATTRIB_VALUES)) {
			indent(level);
			fprintf(stdout, "Class Device path = \"%s\"\n",
								dev->path);
		}
		if (show_options & (SHOW_ATTRIBUTES | SHOW_ATTRIBUTE_VALUE
		    | SHOW_ALL_ATTRIB_VALUES)) {
			attributes = sysfs_get_classdev_attributes(dev);
			if (attributes != NULL)
				show_attributes(attributes, (level+2));
			fprintf(stdout, "\n");
		}
		if (show_options & (SHOW_DEVICES | SHOW_ALL_ATTRIB_VALUES)) {
			device = sysfs_get_classdev_device(dev);
			if (device != NULL) {
				show_device(device, (level+2));
			}
		}
		if (show_options & (SHOW_DRIVERS | SHOW_ALL_ATTRIB_VALUES)) {
			driver = sysfs_get_classdev_driver(dev);
			if (driver != NULL) {
				show_driver(driver, (level+2));
			}
		}
		if ((device_to_show != NULL) && (show_options & SHOW_PARENT)) {
			show_options &= ~SHOW_PARENT;
			show_classdev_parent(dev, level+2);
		}
		if (show_options & ~(SHOW_ATTRIBUTES | SHOW_ATTRIBUTE_VALUE
		    | SHOW_ALL_ATTRIB_VALUES))
			fprintf(stdout, "\n");
	}
}

/**
 * show_sysfs_class: prints out sysfs class and all its devices.
 * @classname: class to print.
 * returns 0 with success and 1 with error.
 */
static int show_sysfs_class(char *classname)
{
	struct sysfs_class *cls = NULL;
	struct sysfs_class_device *cur = NULL;
	struct dlist *clsdevlist = NULL;

	if (classname == NULL) {
		errno = EINVAL;
		return 1;
	}
	cls = sysfs_open_class(classname);
	if (cls == NULL) {
		fprintf(stderr, "Error opening class %s\n", classname);
		return 1;
	}
	fprintf(stdout, "Class = \"%s\"\n\n", classname);
	clsdevlist = sysfs_get_class_devices(cls);
	if (clsdevlist != NULL) {
		dlist_for_each_data(clsdevlist, cur,
				struct sysfs_class_device) {
			if (device_to_show == NULL || (strcmp(device_to_show,
			    cur->name) == 0))
				show_class_device(cur, 2);
		}
	}

	sysfs_close_class(cls);
	return 0;
}

/**
 * show_sysfs_root: prints out sysfs root device tree
 * @rootname: device root to print.
 * returns 0 with success and 1 with error.
 */
static int show_sysfs_root(char *rootname)
{
	struct sysfs_root_device *root = NULL;
	struct sysfs_device *device = NULL;
	struct dlist *devlist = NULL;

	if (rootname == NULL) {
		errno = EINVAL;
		return 1;
	}
	root = sysfs_open_root_device(rootname);
	if (root == NULL) {
		fprintf(stderr, "Error opening root device %s\n", rootname);
		return 1;
	}

	devlist = sysfs_get_root_devices(root);
	if (devlist != NULL) {
		fprintf(stdout, "Root Device = \"%s\"\n\n", rootname);

		if (devlist != NULL) {
			dlist_for_each_data(devlist, device,
						struct sysfs_device) {
				if (strncmp(device->name, "power", 5) == 0)
					continue;
				show_device_tree(device, 2);
			}
		}
	}
	sysfs_close_root_device(root);

	return 0;
}

/**
 * show_default_info: prints current buses, classes, and root devices
 *	supported by sysfs.
 * returns 0 with success or 1 with error.
 */
static int show_default_info(void)
{
	char subsys[SYSFS_NAME_LEN];
	struct dlist *list = NULL;
	char *cur = NULL;
	int retval = 0;

	safestrcpy(subsys, SYSFS_BUS_NAME);
	list = sysfs_open_subsystem_list(subsys);
	if (list != NULL) {
		fprintf(stdout, "Supported sysfs buses:\n");
		dlist_for_each_data(list, cur, char)
			fprintf(stdout, "\t%s\n", cur);
		sysfs_close_list(list);
	}

	safestrcpy(subsys, SYSFS_CLASS_NAME);
	list = sysfs_open_subsystem_list(subsys);
	if (list != NULL) {
		fprintf(stdout, "Supported sysfs classes:\n");
		dlist_for_each_data(list, cur, char)
			fprintf(stdout, "\t%s\n", cur);
		sysfs_close_list(list);
	}

	safestrcpy(subsys, SYSFS_DEVICES_NAME);
	list = sysfs_open_subsystem_list(subsys);
	if (list != NULL) {
		fprintf(stdout, "Supported sysfs devices:\n");
		dlist_for_each_data(list, cur, char)
			fprintf(stdout, "\t%s\n", cur);
		sysfs_close_list(list);
	}

	return retval;
}

/**
 * check_sysfs_mounted: Checks to see if sysfs is mounted.
 * returns 0 if not and 1 if true.
 */
static int check_sysfs_is_mounted(void)
{
	char path[SYSFS_PATH_MAX];

	if (sysfs_get_mnt_path(path, SYSFS_PATH_MAX) != 0)
		return 0;
	return 1;
}

/* MAIN */
int main(int argc, char *argv[])
{
/*	char *show_bus = NULL;*/
	char *show_class = NULL;
	char *show_root = NULL;
	int retval = 0;
	int opt;
        char *pci_id_file = "/usr/local/share/pci.ids";

	while((opt = getopt(argc, argv, cmd_options)) != EOF) {
		switch(opt) {
		case 'a':
			show_options |= SHOW_ATTRIBUTES;
			break;
		case 'A':
			if ((strlen(optarg) + 1) > SYSFS_NAME_LEN) {
				fprintf(stderr,
					"Attribute name %s is too long\n",
					optarg);
				exit(1);
			}
			attribute_to_show = optarg;
			show_options |= SHOW_ATTRIBUTE_VALUE;
			break;
		case 'b':
			show_bus = optarg;
			break;
		case 'c':
			show_class = optarg;
			break;
		case 'C':
			show_options |= SHOW_CHILDREN;
			break;
		case 'd':
			show_options |= SHOW_DEVICES;
			break;
		case 'D':
			show_options |= SHOW_DRIVERS;
			break;
		case 'h':
			usage();
			exit(0);
			break;
		case 'p':
			show_options |= SHOW_PATH;
			break;
		case 'P':
			show_options |= SHOW_PARENT;
			break;
		case 'r':
			show_root = optarg;
			break;
		case 'v':
			show_options |= SHOW_ALL_ATTRIB_VALUES;
			break;
		default:
			usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	switch(argc) {
	case 0:
		break;
	case 1:
		/* get bus to view */
		if ((strlen(*argv)) < SYSFS_NAME_LEN) {
			device_to_show = *argv;
			show_options |= SHOW_DEVICES;
		} else {
			fprintf(stderr,
				"Invalid argument - device name too long\n");
			exit(1);
		}
		break;
	default:
		usage();
		exit(1);
	}

	if (check_sysfs_is_mounted() == 0) {
		fprintf(stderr, "Unable to find sysfs mount point!\n");
		exit(1);
	}

	if ((show_bus == NULL && show_class == NULL && show_root == NULL) &&
			(show_options & (SHOW_ATTRIBUTES |
				SHOW_ATTRIBUTE_VALUE | SHOW_DEVICES |
				SHOW_DRIVERS | SHOW_ALL_ATTRIB_VALUES))) {
		fprintf(stderr,
			"Please specify a bus, class, or root device\n");
		usage();
		exit(1);
	}
	/* default is to print devices */
	if (!(show_options & (SHOW_DEVICES | SHOW_DRIVERS)))
		show_options |= SHOW_DEVICES;

	if (show_bus != NULL) {
	/*	if ((!(strcmp(show_bus, "pci"))) &&
				(show_options & SHOW_DEVICES)) { */
		if ((!(strcmp(show_bus, "pci"))))  {
			pacc = (struct pci_access *)
				calloc(1, sizeof(struct pci_access));
			pacc->pci_id_file_name = pci_id_file;
			pacc->numeric_ids = 0;
		}
		retval = show_sysfs_bus(show_bus);
	}
	if (show_class != NULL)
		retval = show_sysfs_class(show_class);
	if (show_root != NULL)
		retval = show_sysfs_root(show_root);

	if (show_bus == NULL && show_class == NULL && show_root == NULL)
		retval = show_default_info();

	if (show_bus != NULL) {
		/*if ((!(strcmp(show_bus, "pci"))) &&
				(show_options & SHOW_DEVICES)) { */
		if ((!(strcmp(show_bus, "pci"))))  {
			pci_free_name_list(pacc);
			free (pacc);
			pacc = NULL;
		}
	}
	if (!(show_options ^ SHOW_DEVICES))
		fprintf(stdout, "\n");

	exit(retval);
}
