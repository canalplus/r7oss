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
   @file   pti_dbg.h
   @brief  PTI debug functions providing a procfs style interface.

   This file implements the functions printing out status information.  It presents a procfs style
   interface to make it trival to implement a procfs under linux.

   It is not only used for procfs, but also for STPTI_Debug.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.

 */

#ifndef _PTI_DBG_H_
#define _PTI_DBG_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */

/* Includes from the HAL / ObjMan level */

/* Exported Types ---------------------------------------------------------- */

int stptiHAL_DebugAsFile(FullHandle_t vDeviceHandle, char *Filename, char *Buffer, int Limit, int Offset, int *EOF_p);
int stptiHAL_DebugObjectTree(FullHandle_t RootHandle, char *Buffer, int Limit, int Offset, int *EOF_p);

#endif /* _PTI_DBG_H_ */
