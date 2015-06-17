/*
 * Allegro A8297 dual LNBR SEC driver
 *
 * (C) Copyright 2012 WyPlay SAS.
 * Aubin Constans <aconstans@wyplay.com>
 *
 * Copyright (C) 2011 Antti Palosaari <crope@iki.fi>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of
 * the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/list.h>

#include "dvb_frontend.h"
#include "a8297.h"

#define LOG_PREFIX			"a8297"
#define PRINTK(fmt, args...)						\
	do {								\
		if (debug >= 1)						\
			printk(KERN_DEBUG "%s %s: " fmt,		\
			       LOG_PREFIX, __FUNCTION__, ##args);	\
	} while (0)

#define A8297_SFT_CTRL_LNB1		0
#define A8297_SFT_CTRL_LNB2		4

#define A8297_SFT_CTRL_VSEL		0
#define A8297_MSK_CTRL_VSEL		0x7
#define A8297_SFT_CTRL_ENB		3
#define A8297_MSK_CTRL_ENB		0x1
#define A8297_MSK_CTRL_LNB		0xf

#define A8297_VSEL_13_3			0
#define A8297_VSEL_13_6			1
#define A8297_VSEL_14_0			2
#define A8297_VSEL_14_3			3
#define A8297_VSEL_18_6			4
#define A8297_VSEL_19_0			5
#define A8297_VSEL_19_6			6
#define A8297_VSEL_20_0			7

#define A8297_ENB_OFF			0
#define A8297_ENB_ON			1

/* ----------------------------------------------------------------------- */
struct a8297_chip {
	struct mutex lock;
	struct list_head node;
	struct i2c_adapter *i2c;
	u8 i2c_addr;
	u8 ctrl;
	u8 use_count;
};

struct a8297_section {
	struct a8297_config cfg;
	struct a8297_chip *chip;
	bool highervoltage;
};

struct a8297_base {
	struct mutex lock;
	struct list_head chips;
};

static const char* const a8297_voltage_names[9] =
{
	"off",
	"13.3V",
	"13.6V",
	"14V",
	"14.3V",
	"18.6V",
	"19V",
	"19.6V",
	"20V",
};

static struct a8297_base a8297_mod =
{
	.lock                        = __MUTEX_INITIALIZER(a8297_mod.lock),
	.chips                       = LIST_HEAD_INIT(a8297_mod.chips),
};

static int debug = 0;
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Debug level 0/1");

/* ----------------------------------------------------------------------- */
static int a8297_send_data(struct a8297_chip *chip, const u8 data)
{
	u8 buf[2] = {	0,		/* Control Register Address */
			data };		/* Control Data */
	struct i2c_msg msg = {	.addr = chip->i2c_addr, .flags = 0,
				.len = sizeof(buf), .buf = buf };
	int rc;

	rc = i2c_transfer(chip->i2c, &msg, 1);
	if (rc == 1)
		rc = 0;
	else if (rc >= 0)
		rc = -EREMOTEIO;
	if (rc == 0)
		PRINTK("wrote %02x\n", data);
	else
		PRINTK("failed to write (%02x), error %d\n", data, -rc);
	return rc;
}

static int a8297_get_data(struct a8297_chip *chip, const u8 subaddress,
                          const size_t datalen, u8 *target)
{
	u8 subaddr = subaddress;	/* Status Register Address */
	struct i2c_msg msg[2] = {
		{	.addr = chip->i2c_addr, .flags = 0,
			.len = 1, .buf = &subaddr
		},
		{	.addr = chip->i2c_addr, .flags = I2C_M_RD,
			.len = 1, .buf = target
		}
	};
	int rc;

	if (subaddress > 1 || subaddress + datalen > 2)
		return -EINVAL;
	if (datalen == 0)
		return 0;
	/* note: the A8297 does not support the repeated START I2C condition */
	rc = i2c_transfer(chip->i2c, &msg[0], 1);
	if (rc == 1)
		rc = i2c_transfer(chip->i2c, &msg[1], 1);
	if (rc == 1)
		rc = 0;
	else if (rc >= 0)
		rc = -EREMOTEIO;
	if (datalen > 1 && rc == 0) {
		subaddr++;
		msg[1].buf = target + 1;
		rc = i2c_transfer(chip->i2c, &msg[0], 1);
		if (rc == 1)
			rc = i2c_transfer(chip->i2c, &msg[1], 1);
		if (rc == 1)
			rc = 0;
		else if (rc >= 0)
			rc = -EREMOTEIO;
	}
	if (debug && rc == 0) {
		if (datalen > 1)
			PRINTK("read from %x: %02x %02x\n", subaddress,
				target[0], target[1]);
		else
			PRINTK("read from %x: %02x\n", subaddress, target[0]);
	}
	else
		PRINTK("failed to read %zu byte(s) from %x, error %d\n",
			datalen, subaddress, -rc);
	return rc;
}

static int a8297_set_voltage_int(struct a8297_section *section,
                             fe_sec_voltage_t fe_sec_voltage)
{
	struct a8297_chip *chip = section->chip;
	const u8 offset = section->cfg.section ?
		A8297_SFT_CTRL_LNB2 : A8297_SFT_CTRL_LNB1;
	u8 level = 0;
	int rc = 0;

	switch (fe_sec_voltage) {
	case SEC_VOLTAGE_OFF:
		level = 0;	/* power the LNB off */
		break;
	case SEC_VOLTAGE_13:
		level = 1 + (section->highervoltage ? A8297_VSEL_14_3 :
			A8297_VSEL_13_3);
		break;
	case SEC_VOLTAGE_18:
		level = 1 + (section->highervoltage ? A8297_VSEL_19_6 :
			A8297_VSEL_18_6);
		break;
	default:
		rc = -EINVAL;
		goto Exit;
	};
	chip->ctrl &= ~(A8297_MSK_CTRL_VSEL
		<< (A8297_SFT_CTRL_VSEL + offset));
	if (level == 0)
		chip->ctrl &= ~(A8297_MSK_CTRL_ENB
			<< (A8297_SFT_CTRL_ENB + offset));
	else {
		chip->ctrl |= (level - 1) << (A8297_SFT_CTRL_VSEL + offset);
		chip->ctrl |= A8297_ENB_ON << (A8297_SFT_CTRL_ENB + offset);
	}
	rc = a8297_send_data(chip, chip->ctrl);

Exit:
	if (rc == 0)
		PRINTK("%d-0x%02x-LNB%u power supply is %s\n",
			i2c_adapter_id(chip->i2c), chip->i2c_addr,
			section->cfg.section + 1, a8297_voltage_names[level]);
	else
		PRINTK("%d-0x%02x-LNB%u: failed to switch power supply (%d), "
			"error %d\n",
			i2c_adapter_id(chip->i2c), chip->i2c_addr,
			section->cfg.section + 1, fe_sec_voltage, -rc);
	return rc;
}

/* ----------------------------------------------------------------------- */
static int a8297_set_voltage(struct dvb_frontend *fe,
                             fe_sec_voltage_t fe_sec_voltage)
{
	struct a8297_section *section = (struct a8297_section*)fe->sec_priv;
	int rc = 0;

	if (mutex_lock_interruptible(&section->chip->lock) != 0)
		return -ERESTARTSYS;
	rc = a8297_set_voltage_int(section, fe_sec_voltage);
	mutex_unlock(&section->chip->lock);
	return rc;
}

static int a8297_enable_high_lnb_voltage(struct dvb_frontend *fe, long arg)
{
	struct a8297_section *section = (struct a8297_section*)fe->sec_priv;
	const u8 offset = section->cfg.section ?
		A8297_SFT_CTRL_LNB2 : A8297_SFT_CTRL_LNB1;
	const bool highervoltage = arg ? true : false;
	fe_sec_voltage_t fe_sec_voltage;
	int rc = 0;

	if (section->highervoltage == highervoltage)
		return 0;
	if (mutex_lock_interruptible(&section->chip->lock) != 0)
		return -ERESTARTSYS;
	section->highervoltage = highervoltage;
	if (((section->chip->ctrl >> (A8297_SFT_CTRL_ENB + offset))
		& A8297_MSK_CTRL_ENB) == A8297_ENB_ON) {
		if (((section->chip->ctrl >> (A8297_SFT_CTRL_VSEL + offset))
			& A8297_MSK_CTRL_VSEL) >= A8297_VSEL_18_6)
			fe_sec_voltage = SEC_VOLTAGE_18;
		else
			fe_sec_voltage = SEC_VOLTAGE_13;
		rc = a8297_set_voltage_int(section, fe_sec_voltage);
	}
	mutex_unlock(&section->chip->lock);
	return rc;
}

static void a8297_release_sec(struct dvb_frontend *fe)
{
	struct a8297_section *section = (struct a8297_section*)fe->sec_priv;
	struct a8297_chip *chip = NULL;

	a8297_set_voltage(fe, SEC_VOLTAGE_OFF);

	/* if the chip entry has no section attached anymore, destroy it */
	mutex_lock(&a8297_mod.lock);
	list_for_each_entry(chip, &a8297_mod.chips, node)
	{
		if (chip == section->chip)
			break;
	}
	if (chip == section->chip) {
		mutex_lock(&chip->lock);
		chip->use_count--;
		if (chip->use_count == 0) {
			PRINTK("%d-0x%02x: chip instance destroyed (0x%p)\n",
			       i2c_adapter_id(chip->i2c), chip->i2c_addr, chip);
			list_del(&chip->node);
			mutex_destroy(&chip->lock);
			kfree(chip);
		}
		else
			mutex_unlock(&chip->lock);
	}
	mutex_unlock(&a8297_mod.lock);

	fe->sec_priv = NULL;
	kfree(section);
}

struct dvb_frontend *a8297_attach(struct dvb_frontend *fe,
	struct i2c_adapter *i2c, const struct a8297_config *cfg)
{
	struct a8297_chip *chip = NULL;
	struct a8297_section *section = NULL;
	u8 buf[2];
	int rc = 0;
	bool locked = false, newchip = false;

	if (cfg->section > 1) {
		rc = -EINVAL;
		goto Error;
	}
	section = kzalloc(sizeof(*section), GFP_KERNEL);
	if (section == NULL) {
		rc = -ENOMEM;
		goto Error;
	}

	/* search whether the underlying chip has been instanciated already */
	if (mutex_lock_interruptible(&a8297_mod.lock) != 0) {
		rc = -ERESTARTSYS;
		goto Error;
	}
	locked = true;
	rc = ENODEV;
	list_for_each_entry(chip, &a8297_mod.chips, node)
	{
		if (chip->i2c_addr == cfg->i2c_addr && chip->i2c == i2c) {
			rc = 0;
			break;
		}
	}
	if (rc == ENODEV) {
		/* chip not instanciated yet, creating new chip entry */
		chip = kzalloc(sizeof(*chip), GFP_KERNEL);
		if (chip == NULL) {
			rc = -ENOMEM;
			goto Error;
		}
		newchip = true;
		mutex_init(&chip->lock);
		INIT_LIST_HEAD(&chip->node);
		chip->i2c = i2c;
		chip->i2c_addr = cfg->i2c_addr;
		chip->ctrl = 0;
		chip->use_count = 0;
		PRINTK("%d-0x%02x: chip instance created (0x%p)\n",
		       i2c_adapter_id(chip->i2c), chip->i2c_addr, chip);
		/* check whether the chip is there */
		rc = a8297_get_data(chip, 0, 2, buf);
		if (rc)
			goto Error;
		/* power both LNB's off */
		rc = a8297_send_data(chip, chip->ctrl);
		if (rc)
			goto Error;
		list_add(&chip->node, &a8297_mod.chips);
	}
	chip->use_count++;
	mutex_unlock(&a8297_mod.lock);
	locked = false;

	/* link the new section with the chip entry */
	section->cfg = *cfg;
	section->chip = chip;
	section->highervoltage = false;
	fe->sec_priv = section;

	/* install release callback */
	fe->ops.release_sec = a8297_release_sec;
	/* override frontend ops */
	fe->ops.set_voltage = a8297_set_voltage;
	fe->ops.enable_high_lnb_voltage = a8297_enable_high_lnb_voltage;

	printk(KERN_INFO LOG_PREFIX ": LNB%u attached - chip at 0x%02x on "
		"adapter %s\n", section->cfg.section + 1,
		section->chip->i2c_addr, section->chip->i2c->name);
	return fe;

Error:
	if (locked)
		mutex_unlock(&a8297_mod.lock);
	printk(KERN_ERR LOG_PREFIX ": failed to attach %d-0x%02x-LNB%u, error "
		"%d\n", i2c_adapter_id(i2c), cfg->i2c_addr, cfg->section + 1,
		-rc);
	if (newchip) {
		mutex_destroy(&chip->lock);
		kfree(chip);
	}
	if (section != NULL)
		kfree(section);
	return NULL;
}
EXPORT_SYMBOL(a8297_attach);

MODULE_DESCRIPTION("Allegro A8297 SEC driver");
MODULE_AUTHOR("Aubin Constans <aconstans@wyplay.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
