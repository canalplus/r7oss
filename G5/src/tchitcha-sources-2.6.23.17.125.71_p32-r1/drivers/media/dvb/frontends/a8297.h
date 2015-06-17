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

#ifndef _A8297_H_
#define _A8297_H_

#include <linux/dvb/frontend.h>

struct a8297_config {
	u8 i2c_addr;		/* 7-bit I2C chip address, from 0x08 to 0x0b */
	u8 section;		/* 0 for LNB1, or 1 for LNB2 */
};

#if defined(CONFIG_DVB_A8297) || \
	(defined(CONFIG_DVB_A8297_MODULE) && defined(MODULE))
extern struct dvb_frontend *a8297_attach(struct dvb_frontend *fe,
	struct i2c_adapter *i2c, const struct a8297_config *cfg);
#else
static inline struct dvb_frontend *a8297_attach(struct dvb_frontend *fe,
	struct i2c_adapter *i2c, const struct a8297_config *cfg)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif
