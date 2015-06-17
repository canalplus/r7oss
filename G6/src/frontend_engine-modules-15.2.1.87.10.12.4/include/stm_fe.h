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
Source file name : stm_fe.h
Author :           Rahul.V
stm_fe component header
Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V
************************************************************************/

/*
 * This file should be included for RF/IP frontend related function calls.
 * Contents of earlier stm_fe.h have been split into object specific sub headers
 * and all of them are now included in this header file
 */


#ifndef _STM_FE_H
#define _STM_FE_H
#include <stm_event.h>

#include "stm_frontend_demod.h"
#include "stm_frontend_satcr.h"
#include "stm_frontend_fsk.h"
#include "stm_frontend_sif.h"
#include "stm_frontend_rf_matrix.h"
#include "stm_frontend_ip.h"
#include "stm_frontend_ipfec.h"
#include "stm_frontend_diseqc.h"
#include "stm_frontend_lnb.h"

#define FRONTEND_EVENTS	1
#define STM_FE_MAX_SHARED 1

typedef enum {
	STM_FE_NONE,
	STM_FE_DEMOD,
	STM_FE_LNB,
	STM_FE_DISEQC,
	STM_FE_RF_MATRIX,
	STM_FE_SATCR,
	STM_FE_SIF,
	STM_FE_IP,
	STM_FE_IPFEC
} stm_fe_object_type_t;

typedef struct stm_fe_object_info_s {
	char stm_fe_obj[32];
	char member_of[STM_FE_MAX_SHARED][32];
	stm_fe_object_type_t type;
	union {
		stm_fe_demod_caps_t demod;
		stm_fe_diseqc_caps_t diseqc;
		stm_fe_rf_matrix_caps_t rf_matrix;
		stm_fe_ip_caps_t ip;
		stm_fe_ip_fec_caps_t ipfec;
	} u_caps;
} stm_fe_object_info_t;

int32_t __must_check stm_fe_discover(stm_fe_object_info_t **fe, uint32_t *cnt);

#endif /* _STM_FE_H */
