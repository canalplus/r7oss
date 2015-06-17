/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : mxl584_init.h
Author :

Header for low level initialisation driver of STV0900

Date        Modification                                    Name
----        ------------                                    --------
8-April-12   Created						HS

************************************************************************/
#ifndef _MXL_HYDRAINIT
#define _MXL_HYDRAINIT

#include <i2c_wrapper.h>
/* functions --------------------------------------------------------------- */

/* create instance of the register mappings */
STCHIP_Error_t
	MXL584_Init(Demod_InitParams_t *InitParams, STCHIP_Handle_t *hChip);

#endif
