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

Configuration data types for a DISEQC device

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/
#ifndef _DISEQC_H
#define _DISEQC_H

#define STM_FE_DISEQC_NAME "stm_fe_diseqc"

enum diseqc_version_e {
	DISEQC_VERSION_1_2,
	DISEQC_VERSION_2_0
};

enum diseqc_customisations_e {
	DISEQC_CUSTOM_NONE = 0,
	DISEQC_CUSTOM_ENVELOPE_MODE = (1 << 0)
};

enum diseqc_lla_type_e {
	DISEQC_LLA_FORCED,
	DISEQC_LLA_AUTO
};
#define DISEQC_NAME_MAX_LEN 32
struct diseqc_config_s {
	char diseqc_name[DISEQC_NAME_MAX_LEN];
	char lla[DISEQC_NAME_MAX_LEN];
	enum diseqc_lla_type_e lla_sel;
	uint32_t ctrl;
	enum diseqc_version_e ver;

	enum diseqc_customisations_e cust_flags;
};

#endif /* _DISEQC_H */
