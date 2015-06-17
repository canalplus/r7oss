/**
 *****************************************************************************
 * @file plt_sysfs.c
 * @author Kevin PETIT <kevin.petit@technicolor.com>
 *
 * @brief Create the /sys/plt files
 *
 * Copyright (C) 2012 Technicolor
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA  02110-1301  USA.
 *
 * Contact Information:
 * technicolor.com
 * 1, rue Jeanne d´Arc
 * 92443 Issy-les-Moulineaux Cedex
 * Paris
 * France
 *
 ******************************************************************************/
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <plt/plt_id.h>
/* #include <plt/plt_femap.h> */

#define PLT_DISPLAY_TIMEON_CNT

#define PLT_SYSFS_MAX_DESC 64
#define PLT_SYSFS_MAX_LINE_DESC 32

struct plt_sysfs_attribute{
	struct attribute attr;
	char desc[PLT_SYSFS_MAX_DESC];
    int value;
};

#define DEFINE_PLT_KOBJ(__name, __file_name)							\
		struct plt_sysfs_attribute plt_sysfs_attrs_##__name = {			\
			.attr = {													\
				.name = __file_name,									\
				.mode = 0444											\
			},															\
			.desc = { '\0' },											\
            .value = 0                                                  \
        }

#define DEFINE_PLT_KOBJ_CONTAINER(__name, __file_name)					\
		struct plt_sysfs_attribute plt_sysfs_attrs_##__name = {			\
			.attr = {													\
				.name = __file_name,									\
				.mode = 0644											\
			},															\
			.desc = { '\0' },											\
            .value = 0                                                  \
		}

DEFINE_PLT_KOBJ(id,         "id");
DEFINE_PLT_KOBJ(id_board,   "id_board");
DEFINE_PLT_KOBJ(id_hw,      "id_hw");
DEFINE_PLT_KOBJ(name,       "name");
DEFINE_PLT_KOBJ(core,       "core");

#ifdef PLT_DISPLAY_TIMEON_CNT
DEFINE_PLT_KOBJ_CONTAINER(timeon,       "timeon");
DEFINE_PLT_KOBJ_CONTAINER(wrcnt,        "wrcnt");
#endif

#ifdef PLT_FEMAP_KERNEL_CREATE_FILE_SYS_PLT_FEMAP /* from plt/plt_femap.h */
DEFINE_PLT_KOBJ(femap,      "femap");
#endif

struct kobject plt_sysfs_kobject;
#ifdef PLT_DISPLAY_TIMEON_CNT
struct kobject plt_sysfs_kobject_container_display;
#endif

/*******************************************************************************
 * Kobject hanling
 ******************************************************************************/
void plt_sysfs_kobject_release(struct kobject *kobj)
{
	kfree(kobj);
}

#define to_plt_attr(_attr) container_of(_attr, struct plt_sysfs_attribute, attr)

static ssize_t plt_sysfs_kobject_attr_show(struct kobject *kobj,
											  struct attribute *attr, char *buf)
{
	struct plt_sysfs_attribute *plt_attr = to_plt_attr(attr);
	ssize_t ret = -EIO;
	ret = snprintf(buf, PLT_SYSFS_MAX_DESC, "%s", plt_attr->desc);
	return ret;
}

const struct sysfs_ops plt_sysfs_kobject_sysfs_ops = {
	.show	= plt_sysfs_kobject_attr_show,
	.store	= NULL,
};

struct kobj_type plt_sysfs_kobj_type = {
	.release	= plt_sysfs_kobject_release,
	.sysfs_ops	= &plt_sysfs_kobject_sysfs_ops,
};

/********************************************************************/

static ssize_t plt_sysfs_kobject_container_attr_show(struct kobject *kobj,
											  struct attribute *attr, char *buf)
{
	struct plt_sysfs_attribute *plt_attr = to_plt_attr(attr);
	ssize_t ret = -EIO;
	ret = snprintf(buf, PLT_SYSFS_MAX_DESC, "%d\n", plt_attr->value);
	return ret;
}

static ssize_t plt_sysfs_kobject_container_attr_store(struct kobject *kobj,
                                            struct attribute *attr, const char *buf, size_t count)
{
	struct plt_sysfs_attribute *plt_attr = to_plt_attr(attr);
	unsigned long value;
	int rc;
    
	rc = kstrtoul(buf, 0, &value);
	if (rc == 0) {
        plt_attr->value = value;
        rc = count;
    }
    return rc;
}

const struct sysfs_ops plt_sysfs_kobject_container_sysfs_ops = {
	.show	= plt_sysfs_kobject_container_attr_show,
	.store	= plt_sysfs_kobject_container_attr_store,
};

struct kobj_type plt_sysfs_kobj_container_type = {
	.release	= plt_sysfs_kobject_release,
	.sysfs_ops	= &plt_sysfs_kobject_container_sysfs_ops,
};

/*******************************************************************************
 *                 INIT /sys/plt                                               *
 ******************************************************************************/
int __init plt_sysfs_init(void)
{
	/* FIXME handle errors */
	int ret;
    
	/* Init the plt_features kobject */
	kobject_init(&plt_sysfs_kobject, &plt_sysfs_kobj_type);
	/* Add it to the hierarchy */
	ret = kobject_add(&plt_sysfs_kobject, NULL, "plt");

	/* Create "id" entry */
	snprintf(plt_sysfs_attrs_id.desc, PLT_SYSFS_MAX_DESC, "%d\n", plt_id_get());
	ret = sysfs_create_file(&plt_sysfs_kobject, &plt_sysfs_attrs_id.attr);
	/* Create "id_board" entry */
	snprintf(plt_sysfs_attrs_id_board.desc, PLT_SYSFS_MAX_DESC, "%d\n", plt_id_get_board_id());
	ret = sysfs_create_file(&plt_sysfs_kobject, &plt_sysfs_attrs_id_board.attr);
	/* Create "id_hw" entry */
	snprintf(plt_sysfs_attrs_id_hw.desc, PLT_SYSFS_MAX_DESC, "%d\n", plt_id_get_hw_id());
	ret = sysfs_create_file(&plt_sysfs_kobject, &plt_sysfs_attrs_id_hw.attr);
	/* Create "name" entry */
	snprintf(plt_sysfs_attrs_name.desc, PLT_SYSFS_MAX_DESC, "%s\n", plt_id_get_name());
	ret = sysfs_create_file(&plt_sysfs_kobject, &plt_sysfs_attrs_name.attr);
	/* Create "core" entry */
	snprintf(plt_sysfs_attrs_core.desc, PLT_SYSFS_MAX_DESC, "%d\n", plt_id_get_core());
	ret = sysfs_create_file(&plt_sysfs_kobject, &plt_sysfs_attrs_core.attr);


#ifdef PLT_DISPLAY_TIMEON_CNT
	/* Init the plt_features display container*/
	kobject_init(&plt_sysfs_kobject_container_display, &plt_sysfs_kobj_container_type);
	/* Add it to the hierarchy */
	ret = kobject_add(&plt_sysfs_kobject_container_display, &plt_sysfs_kobject, "display");

    /* Create "display" entries */
	snprintf(plt_sysfs_attrs_timeon.desc, PLT_SYSFS_MAX_DESC, "%d\n", 0);
	ret = sysfs_create_file(&plt_sysfs_kobject_container_display, &plt_sysfs_attrs_timeon.attr);
	snprintf(plt_sysfs_attrs_wrcnt.desc, PLT_SYSFS_MAX_DESC, "%d\n", 0);
	ret = sysfs_create_file(&plt_sysfs_kobject_container_display, &plt_sysfs_attrs_wrcnt.attr);
#endif
    
#ifdef PLT_FEMAP_KERNEL_CREATE_FILE_SYS_PLT_FEMAP /* from plt/plt_femap.h */
    {
        int i,j;

        /* Create "femap" entry */
        strcpy(plt_sysfs_attrs_femap.desc, "");
        for(i=0; i<PLT_FEMAP_GetNbOfEntries(); i++) {
            j = strlen(plt_sysfs_attrs_femap.desc);
            if(j < PLT_SYSFS_MAX_DESC) {
                char string[PLT_SYSFS_MAX_LINE_DESC];
                snprintf(string, sizeof(string), "%s %d TSIN %d", PLT_FEMAP_GetTypeName(i), i, PLT_FEMAP_GetTSInput(i));
                snprintf(&plt_sysfs_attrs_femap.desc[j], PLT_SYSFS_MAX_DESC - j, "%s\n", string);
            }
        }
        ret = sysfs_create_file(&plt_sysfs_kobject, &plt_sysfs_attrs_femap.attr);
    }
#endif
    
	return ret;
}

late_initcall(plt_sysfs_init);

