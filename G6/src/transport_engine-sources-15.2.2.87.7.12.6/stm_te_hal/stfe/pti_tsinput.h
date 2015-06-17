/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

 ************************************************************************/
/**
   @file   pti_tsinput.h
   @brief  TSInput Object Initialisation, Termination and Manipulation Functions.

   This file declares the object structure and access mechanism for the tsinput
   Object.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.

 */

#ifndef _PTI_TSINPUT_H_
#define _PTI_TSINPUT_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */
#include "../pti_osal.h"
#include "../pti_tshal_api.h"

/* Includes from the HAL / ObjMan level */

/* Exported Types ---------------------------------------------------------- */

typedef enum stptiHAL_TSInputStfeType_e {
	stptiTSHAL_TSINPUT_STFE_IB,
	stptiTSHAL_TSINPUT_STFE_MIB,
	stptiTSHAL_TSINPUT_STFE_SWTS,
	stptiTSHAL_TSINPUT_STFE_NONE
} stptiHAL_TSInputStfeType_t;

/* Exported Function Prototypes -------------------------------------------- */

extern const stptiTSHAL_TSInputAPI_t stptiTSHAL_TSInputAPI;

#endif /* _PTI_TSINPUT_H_ */
