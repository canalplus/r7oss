/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

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

Source file name : mxl582_init.c
Author :

Low level driver for mxl582

Date        Modification                                    Name
----        ------------                                    --------
7-May-14   Created						MS

************************************************************************/
/*#include "globaldefs.h"*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <stm_fe.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
#include "mxl582_init.h"
#include "d0mxl582.h"


STCHIP_Register_t DefMXL_HYDRAVal[MXL_HYDRA_NBREGS];

STCHIP_Error_t MXL582_Init(Demod_InitParams_t *InitParams,
				STCHIP_Handle_t *hChipHandle)
{
	STCHIP_Handle_t hChip;
	STCHIP_Error_t error = 0;

	/* Fill elements of external chip data structure */
	InitParams->Chip->NbRegs   = MXL_HYDRA_NBREGS;
	InitParams->Chip->NbFields = MXL_HYDRA_NBFIELDS;
	InitParams->Chip->ChipMode = STCHIP_MODE_SUBADR_16;
	InitParams->Chip->pData    = NULL;

	hChip = (*hChipHandle);

	if (hChip != NULL) {
		/*******************************
	    **   REGISTERS CONFIGURATION
	    **     ----------------------
	    ********************************/
		ChipUpdateDefaultValues(hChip, DefMXL_HYDRAVal);

	} else {
		error = CHIPERR_INVALID_HANDLE;
	}
	return error;
}
