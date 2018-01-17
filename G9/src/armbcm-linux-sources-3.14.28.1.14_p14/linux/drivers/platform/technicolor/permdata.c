/*
 *  permdata.c - Driver that export Technicolor permdata fom Flash
 m
 *  Author : Cyril TOSTIVINT <cyril.tostivint@technicolor.com>
 *
 *  Copyright (C) 2011-2016 Technicolor
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <linux/mtd/mtd.h>

#include "plt_id.h"
#include "plt/mapping.h"
#include "plt/individual_data_mapping.h"


struct kobj_permdata_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj, struct kobj_permdata_attr *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct kobj_permdata_attr *attr,
			 const char *buf, size_t count);
	int address;
	ssize_t size;
};

#define __ATTR_PERM(_name, _address, _size) {					\
	.attr	= { .name = __stringify(_name), .mode = S_IRUGO },	\
	.show	= permdata_show,					\
	.address = _address,						\
	.size = _size,								\
}

struct kobj_mac_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj, struct kobj_mac_attr *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct kobj_mac_attr *attr,
			 const char *buf, size_t count);
	unsigned short index;
};

#define __ATTR_MAC(_name, _index) {								\
	.attr	= { .name = __stringify(_name), .mode = S_IRUGO },	\
	.show	= mac_show_gen,			\
	.index = _index,			\
}

static inline void read_product_ids(struct kobject *kobj,
		struct product_ids *p_ids_ptr)
{
	struct tch_plt_id_desc *desc = container_of(kobj,
		   struct tch_plt_id_desc, kobj_perm);
	ssize_t read_len;

	desc->mtd->_read(desc->mtd, FLASH_INDIVIDUAL_DATA_PRODUCT_IDS,
			sizeof(struct product_ids), &read_len,
			(void *) p_ids_ptr);
}

static ssize_t ptype_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct product_ids p_ids;

	read_product_ids(kobj, &p_ids);

	return sprintf(buf, "0x%04X", p_ids.pt);
}

static ssize_t pversion_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct product_ids p_ids;

	read_product_ids(kobj, &p_ids);

	return sprintf(buf, "0x%04X", p_ids.pv);
}

static ssize_t pid_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct product_ids p_ids;

	read_product_ids(kobj, &p_ids);

	return sprintf(buf, "0x%04X", p_ids.pr);
}

static ssize_t oui_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct product_ids p_ids;

	read_product_ids(kobj, &p_ids);

	return sprintf(buf, "0x%08X", p_ids.oui);
}

static ssize_t permdata_show(struct kobject *kobj,
		struct kobj_permdata_attr *attr,
		char *buf)
{
	struct tch_plt_id_desc *desc = container_of(kobj,
		   	struct tch_plt_id_desc, kobj_perm);
	size_t read_len;

	desc->mtd->_read(desc->mtd, attr->address,
			attr->size, &read_len,
			buf);

	// call sprintf to cut strings after \0
	return sprintf(buf, "%s", buf);
}

// Read callback for /sys/plt/macX
static inline ssize_t mac_show_gen(struct kobject *kobj,
		struct kobj_mac_attr *attr, char *buf)
{
	struct tch_plt_id_desc *desc = container_of(kobj,
		   	struct tch_plt_id_desc, kobj_perm);
	size_t read_len;
	u_char read_buf[MAC_SIZE];
	u_int address = FLASH_INDIVIDUAL_DATA_MAC0 
        + attr->index*FLASH_INDIVIDUAL_DATA_MAC_SIZE;

	desc->mtd->_read(desc->mtd, address, MAC_SIZE, &read_len,
			read_buf);
	if (read_len != MAC_SIZE)
		printk("Failed to read the 6 bytes of mac, (read only %d)\n", read_len);

	return sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
			read_buf[0], read_buf[1], read_buf[2],
			read_buf[3], read_buf[4], read_buf[5]);
}

/*---------------- SysFS structures ----------------*/
static struct kobj_permdata_attr mbserial_attr = __ATTR_PERM(mbserial,
		FLASH_INDIVIDUAL_DATA_MB_SERIAL_NB, FLASH_INDIVIDUAL_DATA_MB_SERIAL_NB_SIZE);
static struct kobj_permdata_attr serial_attr = __ATTR_PERM(serial,
		FLASH_INDIVIDUAL_DATA_BOX_SERIAL_NB, FLASH_INDIVIDUAL_DATA_BOX_SERIAL_NB_SIZE);
static struct kobj_permdata_attr tocom_attr = __ATTR_PERM(tocom,
		FLASH_INDIVIDUAL_DATA_TOCOM_NB, FLASH_INDIVIDUAL_DATA_TOCOM_NB_SIZE);
static struct kobj_attribute ptype_attr = __ATTR_RO(ptype);
static struct kobj_attribute pversion_attr = __ATTR_RO(pversion);
static struct kobj_attribute pid_attr = __ATTR_RO(pid);
static struct kobj_attribute oui_attr = __ATTR_RO(oui);
static struct kobj_mac_attr mac1_attr = __ATTR_MAC(mac1, 0);
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 2
static struct kobj_mac_attr mac2_attr = __ATTR_MAC(mac2, 1);
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 3
static struct kobj_mac_attr mac3_attr = __ATTR_MAC(mac3, 2);
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 4
static struct kobj_mac_attr mac4_attr = __ATTR_MAC(mac4, 3);
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 5
static struct kobj_mac_attr mac5_attr = __ATTR_MAC(mac5, 4);
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 6
static struct kobj_mac_attr mac6_attr = __ATTR_MAC(mac6, 5);
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 7
static struct kobj_mac_attr mac7_attr = __ATTR_MAC(mac7, 6);
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 8
static struct kobj_mac_attr mac8_attr = __ATTR_MAC(mac8, 7);
#endif

static struct attribute *plt_permdata_attrs[] = {
	&mbserial_attr.attr,
	&serial_attr.attr,
	&tocom_attr.attr,
	&ptype_attr.attr,
	&pversion_attr.attr,
	&pid_attr.attr,
	&oui_attr.attr,
	&mac1_attr.attr,
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 2
	&mac2_attr.attr,
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 3
	&mac3_attr.attr,
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 4
	&mac4_attr.attr,
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 5
	&mac5_attr.attr,
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 6
	&mac6_attr.attr,
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 7
	&mac7_attr.attr,
#endif
#if CONFIG_TECHNICOLOR_PLT_ID_PERMDATA_MAC >= 8
	&mac8_attr.attr,
#endif
	NULL
};

static struct kobj_type plt_permdata_ktype = {
	.sysfs_ops = &kobj_sysfs_ops,
	.default_attrs = plt_permdata_attrs,
};

void plt_permdata_init(struct device *dev, struct tch_plt_id_desc *desc)
{
	int ret;

	desc->mtd = get_mtd_device_nm("flash0.permdata");
	if (IS_ERR(desc->mtd))
		dev_err(dev, "Didn't found permdata mtd device\n");
	else {
		dev_info(dev, "found %d:%s", desc->mtd->index, desc->mtd->name);

		ret = kobject_init_and_add(&desc->kobj_perm,
				&plt_permdata_ktype, &desc->kobj, "permdata");
		if (ret)
			dev_err(dev, "Unable to create sysfs permdata: %d", ret);

		kobject_uevent(&desc->kobj_perm, KOBJ_ADD);
	}
}
