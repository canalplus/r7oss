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
   @file   pti_isr.h
   @brief  Interrupt Service Routine (called on interrupt).

   This file implements the interrupt service routine.  In most cases semaphores are signalled
   for tasks to take the appropriate action.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.

 */

#ifndef _PTI_ISR_H_
#define _PTI_ISR_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* OS includes */
#include <linux/irqreturn.h>

/* Includes from API level */

/* Includes from the HAL / ObjMan level */

/* Exported Types ---------------------------------------------------------- */

/* Exported Function Prototypes -------------------------------------------- */

irqreturn_t stptiHAL_TPInterruptHandler(int irq, void *pDeviceHandle);
irqreturn_t stptiHAL_SPInterruptHandler(int irq, void *pDeviceHandle);

#endif /* _PTI_ISR_H_ */
