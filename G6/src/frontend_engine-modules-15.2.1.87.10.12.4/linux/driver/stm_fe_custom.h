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

Source file name : stm_fe_custom.h
Author : HS

Header for custom install functions

Date        Modification                                    Name
----        ------------                                    --------
16-April-13   Created                                         HS

************************************************************************/
#ifndef _STM_FE_CUSTOM_H
#define _STM_FE_CUSTOM_H

struct demod_interface_s {
	enum demod_tsout_config_e ts_out;
	unsigned int demux_tsin_id;
	unsigned int ts_clock;		/*In Hz*/
	unsigned int ts_tag;		/* Tag value used in the ts stream*/
	struct demod_ops_s *ops;
};

struct tuner_interface_s {
	struct tuner_ops_s *ops;
};

struct diseqc_interface_s {
	struct diseqc_ops_s *ops;
};

struct rf_matrix_interface_s {
	struct rf_matrix_ops_s *ops;
};

struct lnb_interface_s {
	struct lnb_ops_s *ops;
};

int stm_fe_custom_demod_attach(int *fe_instance,
			struct demod_interface_s *interface, void *cookie);

int stm_fe_custom_tuner_attach(int *fe_instance,
			struct tuner_interface_s *interface, void *cookie);

int stm_fe_custom_diseqc_attach(int *fe_instance,
			struct diseqc_interface_s *interface, void *cookie);

int stm_fe_custom_rf_matrix_attach(int *fe_instance,
			struct rf_matrix_interface_s *interface, void *cookie);

int stm_fe_custom_lnb_attach(int *fe_instance,
			struct lnb_interface_s *interface, void *cookie);

#endif /* _STM_FE_CUSTOM_H */
