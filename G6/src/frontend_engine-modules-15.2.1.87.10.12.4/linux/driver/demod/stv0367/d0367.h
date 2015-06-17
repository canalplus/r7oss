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

Source file name : d0367.h
Author :           Ankur

wrapper for stv090x LLA

Date        Modification                                    Name
----        ------------                                    --------
30-Jul-11   Created                                         Ankur

************************************************************************/
#ifndef _D0367_H
#define _D0367_H

#include "stv0367qam_drv.h"
#include "stv0367ofdm_drv.h"
typedef FE_TS_Config_t FE_STV0367_TSConfig_t;
typedef FE_367qam_InternalParams_t FE_STV0367_InternalParams_t;
typedef FE_LLA_Error_t FE_STV0367_Error_t;
/****************************************************************
	SEARCH STRUCTURES
****************************************************************/
typedef FE_CAB_SearchParams_t FE_STV0367_SearchParams_t;
typedef FE_CAB_SearchResult_t FE_STV0367_SearchResult_t;
/************************
	INFO STRUCTURE
************************/
typedef FE_CAB_SignalInfo_t FE_STV0367_SignalInfo_t;

int stm_fe_stv0367_attach(struct stm_fe_demod_s *priv);
int stm_fe_stv0367_detach(struct stm_fe_demod_s *priv);

#endif /* _D0367_H */
