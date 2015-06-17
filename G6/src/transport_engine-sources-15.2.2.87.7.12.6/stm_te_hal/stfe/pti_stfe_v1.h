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
   @file   pti_stfe_v1.h
   @brief  7108 STFE Peripheral memory map.

   This file declares registers maps customize for first version of STFE.
 */

#ifndef _PTI_STFE_V1_H_
#define _PTI_STFE_V1_H_

#define STFE_MAX_MEMDMA_BLOCK_SIZE     (16)

#define TSDMA_CHANNEL_OFFSET_STFEv1  0x0
#define TSDMA_OUTPUT_OFFSET_STFEv1   0x200

//************************************************/
// PIDFilter
//************************************************/
typedef struct {
	volatile U32 PIDF_LEAK_ENABLE;		/* Not on all devices */
	volatile U32 PIDF_LEAK_COUNT_RESET;	/* Not on all devices */
	volatile U32 PIDF_LEAD_COUNTER;		/* Not on all devices */
	volatile U32 PIDF_LEAK_STATUS;		/* Not on all devices */
} stptiHAL_STFEv1_PidLeakFilter_t;

//************************************************/
// SYSTEM
//************************************************/
typedef struct {
	volatile U32 BUS_BASE;	/* dst */
	volatile U32 BUS_TOP;
	volatile U32 BUS_WP;
	volatile U32 TS_PKT_SIZE;
	volatile U32 BUS_NEXT_BASE;
	volatile U32 BUS_NEXT_TOP;
	volatile U32 MEM_BASE;	/* src */
	volatile U32 MEM_TOP;
} stptiHAL_STFEv1_InputNode_t;

typedef struct {
	volatile U32 STATUS;
	volatile U32 MASK;
	volatile U32 DMA_ROUTE;
	volatile U32 CLK_ENABLE;
} stptiHAL_STFEv1_SystemBlockDevice_t;

//************************************************/
// TSDMA
//************************************************/
typedef struct {
	volatile U32 DEST_ROUTE;
	volatile U32 DEST_FORMAT_STFEv1;
} stptiHAL_STFEv1_TSDmaMixedReg;

//************************************************/
// MEMDMA
//************************************************/
typedef struct {

	volatile U32 DATA_INT_STATUS;
	volatile U32 DATA_INT_MASK;
	volatile U32 ERROR_INT_STATUS;
	volatile U32 ERROR_INT_MASK;
	volatile U32 ERROR_OVERRIDE;
	volatile U32 DMA_ABORT;
	volatile U32 MESSAGE_BOUNDARY;
	volatile U32 IDLE_INT_STATUS;
	volatile U32 IDLE_INT_MASK;
	volatile U32 DMA_MODE;

	volatile U8 padding0[0x18];

	volatile U32 POINTER_BASE;

	volatile U8 padding1[0x3c];

	volatile U32 BUS_BASE;
	volatile U32 BUS_TOP;
	volatile U32 BUS_WP;
	volatile U32 PKT_SIZE;
	volatile U32 BUS_NEXT_BASE;
	volatile U32 BUS_NEXT_TOP;
	volatile U32 MEM_BASE;
	volatile U32 MEM_TOP;
	volatile U32 MEM_RP;

	volatile U8 padding2[0x5c];

	volatile U32 DMA_PKTFIFO_LEVEL;
	volatile U32 DMA_PKTFIFO_PTRS;

	volatile U8 padding3[0x38];

	volatile U32 DMA_INJECT_PKT[STFE_MAX_MEMDMA_BLOCK_SIZE];

	volatile U32 DMA_PKT_FIFO[STFE_MAX_MEMDMA_BLOCK_SIZE];

	volatile U32 padding4[STFE_MAX_MEMDMA_BLOCK_SIZE];

	volatile U32 BUS_SIZE[STFE_MAX_MEMDMA_BLOCK_SIZE];

	volatile U32 BUS_LEVEL[STFE_MAX_MEMDMA_BLOCK_SIZE];

	volatile U32 BUS_THRES[STFE_MAX_MEMDMA_BLOCK_SIZE];
} stptiHAL_STFEv1_DmaBlockDevice_t;

#endif // _PTI_STFE_V1_H_
