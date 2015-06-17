/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : demod.h
Author :           Rahul.V

Configuration data types for a LNB device

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/
#ifndef _LNB_H
#define _LNB_H

#define STM_FE_LNB_NAME "stm_fe_lnb"

enum lnb_io_route_e {
	LNB_IO_DIRECT,
	LNB_IO_REPEATER
};

enum lnb_io_type_e {
	LNB_IO_I2C,
	LNB_IO_MEMORY_MAPPED
};

/* Default is LNB_LLC_DISABLE and LNB_PROTECTION_STATIC */
enum lnb_customisations_e {
	LNB_CUSTOM_NONE = 0,
	LNB_CUSTOM_LLC_ENABLE = (1 << 0),
	LNB_CUSTOM_PROTECTION_DYNAMIC = (1 << 1)
};

enum lnb_lla_type_e {
	LNB_LLA_FORCED,
	LNB_LLA_AUTO
};

struct lnb_io_config_s {
	enum lnb_io_type_e io;
	enum lnb_io_route_e route;
	/* char name[32]; */
	uint32_t bus;
	uint32_t baud;
	uint32_t address;
};

struct lnb_backend_pio_config_s {
	uint32_t volt_sel;	/* ORed bitfield of PIO port and pin */
	uint32_t volt_en;
	uint32_t tone_sel;
};

#define LNB_NAME_MAX_LEN 32
struct lnb_config_s {
	char lnb_name[LNB_NAME_MAX_LEN];
	char lla[LNB_NAME_MAX_LEN];
	enum lnb_lla_type_e lla_sel;
	uint32_t ctrl;

	/* Not applicable for LNB_DEMOD_IO type LNB */
	struct lnb_io_config_s lnb_io;

	/* If any customisations are desired */
	enum lnb_customisations_e cust_flags;

	/* To be set only for LNB_BACKEND_PIO type LNB */
	struct lnb_backend_pio_config_s be_pio;
};

#endif /* _LNB_H */
