/*
    Wyplay Asia s5h1423 DVB OFDM demodulator driver

    Copyright (C) 2006 ST Microelectronics
	Peter Bennett <peter.bennett@st.com>
	
    Copyright (C) 2009 Wyplay Asia 
	Hu Gang <hug@wyplayasia.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef MXL5007CE6353_H
#define MXL5007CE6353_H

#include <linux/init.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
	#include <linux/stm/pio.h>
#else
	#include <linux/stpio.h>
#endif

#include <linux/dvb/frontend.h>
#include "dvb_frontend.h"

struct intel_config
{
	/* the tuners i2c address / adapter */
	u8 tuner_address;
	struct i2c_adapter* tuner_i2c;
	int serial_not_parallel;
};

struct intel_state
{
	u8 demod_address;
	u8 type;
	u32 version;
	struct intel_config * config;
	struct i2c_adapter * demod_i2c;
	struct dvb_frontend_ops ops;
	struct dvb_frontend frontend;
};


extern struct dvb_frontend* intel_attach(const struct intel_config* config,
					   struct i2c_adapter* i2c, u8 demod_address);
extern struct i2c_adapter *tuner_adapter;
extern struct i2c_adapter *mxl5007_adapter;
#endif // S5H1423_H
