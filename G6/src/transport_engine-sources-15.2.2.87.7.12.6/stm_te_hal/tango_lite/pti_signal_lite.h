/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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
   @file   pti_signal_lite.h
   @brief  Signal Object Initialisation, Termination and Manipulation Functions.

   Dummy functions without TP functionality

 */

#ifndef _PTI_SIGNAL_LITE_H_
#define _PTI_SIGNAL_LITE_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */
/* Includes from the HAL / ObjMan level */
#include "../pti_hal_api.h"

/* Exported Types ---------------------------------------------------------- */
typedef struct {
	Object_t ObjectHeader;				/* Must be first in structure */
} stptiHAL_Signal_lite_t;

/* Exported Function Prototypes -------------------------------------------- */
extern const stptiHAL_SignalAPI_t stptiHAL_SignalAPI_Lite;

#define stptiHAL_GetObjectSignal_lite_p(ObjectHandle)   ((stptiHAL_Signal_lite_t *)stptiOBJMAN_HandleToObjectPointer(ObjectHandle,OBJECT_TYPE_SIGNAL))

#endif /* _PTI_SIGNAL_LITE_H_ */
