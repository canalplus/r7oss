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
   @file   pti.debug_print_lite.h
   @brief  Generic debug printer helpers
 */

#ifndef _PTI_DEBUG_PRINT_LITE_H_
#define _PTI_DEBUG_PRINT_LITE_H_

#include "pti_pdevice_lite.h"

/* Generic 'printer' prototype - provide one of these to collect debug print */
typedef int (*debug_print_lite) (void *ctx, const char *format, ...);

int stptiHAL_Print_FullObjectTreeLite(void *ctx, debug_print_lite printer, stptiHAL_pDevice_lite_t * pDevice_p);
void stptiHAL_PrintObjectTreeRecurseLite(void *ctx, debug_print_lite printer, FullHandle_t RootObjectHandle, int level);

#endif
