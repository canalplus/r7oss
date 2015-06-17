/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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
   @file   pti.debug_print.h
   @brief  Generic debug printer helpers
 */

#ifndef _PTI_DEBUG_PRINT_H_
#define _PTI_DEBUG_PRINT_H_

/* Generic 'printer' prototype - provide one of these to collect debug print */
typedef int (*debug_print) (void *ctx, const char *format, ...);

/* The printer helpers */
int stptiHAL_Print_pDevice(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_pDeviceDEM(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_pDeviceSharedMemory(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_pDeviceTrigger(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p, int Offset);
int stptiHAL_Print_TPCycleCount(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_UtilisationTP(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_vDeviceInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_PIDTable(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_SlotInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_FullSlotInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_BufferInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_BufferOverflowInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_IndexerInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_CAMData(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_FullObjectTree(void *ctx, debug_print printer, stptiHAL_pDevice_t *pDevice_p);
int stptiHAL_Print_PrintBuffer(void *ctx, debug_print printer, void *ignoreme);
int stptiHAL_Print_TSInput(void *ctx, debug_print printer, void *streamID);

void stptiHAL_PrintObjectTreeRecurse(void *ctx, debug_print printer, FullHandle_t RootObjectHandle, int level);

#endif
