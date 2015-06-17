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
   @file   pti_tango.h
   @brief  The TANGO memory map structures

   This file declares the TANGO TP and SP memory maps for overlaying.  It should not be included
   directly, instead pti_pdevice.h should be used.

 */

#ifndef _PTI_TANGO_H_
#define _PTI_TANGO_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */
#include "stddefs.h"

/* Includes from API level */
#include "../pti_stpti.h" /* include specifically allowed defines from stpti.h */

/* Includes from the HAL / ObjMan level */

/* Exported Types ---------------------------------------------------------- */

/*** STxP70 Device Registers *********************************************************************/

typedef struct {
	volatile U32 Status;
	volatile U32 Set;
	volatile U32 Clear;
	volatile U32 Mask;
} stptiHAL_TangoMailboxDevice_t;

typedef struct {
	volatile U32 Status;
	volatile U32 Set;
	volatile U32 Clear;
	volatile U32 Toggle;
} stptiHAL_TangoGPODevice_t;

typedef struct {
	volatile U32 Status;
	volatile U32 Mask;
} stptiHAL_TangoGPIDevice_t;

typedef struct {
	volatile U32 PageSize;
	volatile U32 MaxOpSize;
	volatile U32 MinOpSize;
	volatile U32 MaxChunkSize;
	volatile U32 MaxMsgSize;
	volatile U32 InterruptStatus;
	volatile U32 InterruptStatusClear;
	volatile U32 InterruptEnable;
	volatile U32 InterruptEnableSet;
	volatile U32 InterruptEnableClear;
	volatile U32 MinimusMsgSpace;
} stptiHAL_TangoSTBusPlug_t;

typedef struct {
	volatile U32 ResetCtrl;
	volatile U32 ResetStatus;
	volatile U32 TrapEnable;
	volatile U32 CoreRunConfig;
	volatile U32 CoreRunStatus;
	volatile U32 PowerdownConfig;
	volatile U32 PowerdownStatus;
	volatile U32 DmemWritelock;
} stptiHAL_TangoCoreCtrl_t;

typedef struct {
	volatile U32 AddrRangeBase[3];
} stptiHAL_TangoT3AddrFilter_t;

#endif /* _PTI_TANGO_H_ */
