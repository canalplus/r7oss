/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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

Source file name : rf_matrix.h
Author :           Ashish Gandhi

Configuration data types for a RF_MATRIX device

Date        Modification                                    Name
----        ------------                                    --------
07-Aug-13   Created                                         Ashish Gandhi

************************************************************************/
#ifndef _RF_MATRIX_H
#define _RF_MATRIX_H

#define STM_FE_RF_MATRIX_NAME "stm_fe_rf_matrix"

#define RF_MATRIX_NAME_MAX_LEN 32

enum rf_matrix_lla_type_e {
	RF_MATRIX_LLA_FORCED,
	RF_MATRIX_LLA_AUTO
};

struct rf_matrix_io_config_s {
	/* char name[32]; */
	uint32_t bus;
	uint32_t baud;
	uint32_t address;
};

struct rf_matrix_config_s {
	char name[RF_MATRIX_NAME_MAX_LEN];
	char lla[RF_MATRIX_NAME_MAX_LEN];
	enum rf_matrix_lla_type_e lla_sel;
	uint32_t ctrl;
	uint32_t grp_id;
	uint32_t max_input;
	uint32_t input_id;
	struct rf_matrix_io_config_s rf_matrix_io;
};

#endif /* _RF_MATRIX_H */
