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
   @file   pti_dbg_lite.c
   @brief  PTI debug functions providing a procfs style interface.

   This file implements the functions printing out status information.  It presents a procfs style
   interface to make it trival to implement a procfs under linux.

   It is not only used for procfs, but also for STPTI_Debug.

 */

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"
#include "pti_pdevice_lite.h"
#include "pti_vdevice_lite.h"
#include "pti_swinjector_lite.h"
#include "pti_buffer_lite.h"
#include "pti_slot_lite.h"
#include "pti_filter_lite.h"
#include "pti_dbg_lite.h"
#include "pti_debug_print_lite.h"
/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */

/* Private Variables ------------------------------------------------------- */

/* Functions --------------------------------------------------------------- */

/**
   @brief  Example function for printing out debug information.

   As writing procfs style functions can be complicated, this is an example.

   You try to make your output size consistent upon multiple reads.  So %d is out in a printf,
   instead do %5d.

   @param  vDeviceHandle  Handle to the vDevice under inspection

   @param  Buffer         Buffer to write to
   @param  Limit          Maximum number of characters to output (including terminator)
   @param  Offset         Number of characters to skip before starting to put data into the buffer
   @param  EOF_p          =0 if more data to come, =1 if all the data has been output.

   @return                Characters output (excluding null terminator)
 */
int stptiHAL_DebugAsFileLite(FullHandle_t vDeviceHandle, char *Filename, char *Buffer, int Limit, int Offset, int *EOF_p)
{
	stptiSUPPORT_sprintf_t ctx;

	ctx.p = Buffer;
	ctx.CharsOutput = 0;
	ctx.CharsLeft = Limit - 1;	/* Minus 1 to allow for terminator */
	ctx.CurOffset = 0;
	ctx.StartOffset = Offset;
	ctx.StoppedPrintingEarly = FALSE;

	/* Use stptiSUPPORT_sprintf to guarantee you don't over flow the Buffer and to deal with cases
	   where the printfs are split over several read call (as might happen with a procfs). */

	/* ---------------------------------------------------------------------------------------- */


	if (strcmp(Filename, "Example") == 0) {
		stptiSUPPORT_sprintf(&ctx, "This is an example.\n");
	} else if (strcmp(Filename, "FullObjectTree") == 0) {
		stptiHAL_pDevice_lite_t *pDevice_p =
			stptiHAL_GetObjectpDevice_lite_p(vDeviceHandle);
		stptiHAL_Print_FullObjectTreeLite(&ctx, (debug_print_lite) stptiSUPPORT_sprintf, pDevice_p);
	} else if (strcmp(Filename, "ObjectTreeLite") == 0) {
		stptiHAL_PrintObjectTreeRecurseLite(&ctx, (debug_print_lite) stptiSUPPORT_sprintf, vDeviceHandle, 0);
	}

	if (ctx.StoppedPrintingEarly) {
		*EOF_p = 0;
	} else {
		*EOF_p = 1;
	}
	return (ctx.CharsOutput);
}
