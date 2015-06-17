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
   @file   pti_stfe_v2.h
   @brief  7108 STFE Peripheral memory map.

   This file declares registers maps customize for second version of STFE.
 */

/* Be careful if you change this value - always check its impact on the memory map */

#ifndef _PTI_STFE_V2_H_
#define _PTI_STFE_V2_H_

#define TSDMA_CHANNEL_OFFSET_STFEv2  0x0
#define TSDMA_OUTPUT_OFFSET_STFEv2   0x800

// only for this kind
#define PID_LEAK_SIZE 			(2)

#define STFEv2_MAX_SYS_BLOCK_SIZE        (16)

#define   STFEv2_CFG_BLOCK_MASK     (0xFF)

#define   STFEv2_STREAM_NUM_SHIFT   (0)
#define   STFEv2_RAM_NUM_SHIFT   (8)
#define   STFEv2_MEMDMA_STREAM_NUM_SHIFT   (0)
#define   STFEv2_TSDMA_STREAM_NUM_SHIFT   (8)
#define   STFEv2_NUM_SUBSTREAMS_SHIFT   (16)

//************************************************/
// PIDFilter
//************************************************/
typedef struct {
	volatile U32 PIDF_LEAK_ENABLE[2];	/* Not on all devices */
	volatile U32 PIDF_LEAK_STATUS[2];	/* Not on all devices */
	volatile U32 PIDF_LEAK_COUNT_RESET;	/* Not on all devices */
	volatile U32 PIDF_LEAD_COUNTER;		/* Not on all devices */
} stptiHAL_STFEv2_PidLeakFilter_t;

//************************************************/
// SYSTEM
//************************************************/

typedef struct {
	volatile U32 INFO;
	volatile U32 DMA_INFO;
	U32 padding[2];
} stptiHAL_STFE_InfoBlockDevice_t;

typedef struct {
	volatile U32 RAM_ADDR;
	volatile U32 RAM_SIZE;
	U32 padding[2];
} stptiHAL_STFE_RAMInfoBlockDevice_t;

typedef struct {
	volatile U32 INPUT_STATUS[2];
	volatile U32 OTHER_STATUS[2];
	volatile U32 INPUT_MASK[2];
	volatile U32 OTHER_MASK[2];
	volatile U32 DMA_ROUTE[4];
	volatile U32 INPUT_CLK_ENABLE[2];
	volatile U32 OTHER_CLK_ENABLE[2];

	volatile U8 padding0[0x1C0];

	volatile U32 NUM_IB;
	volatile U32 NUM_MIB;
	volatile U32 NUM_SWTS;
	volatile U32 NUM_TSDMA;
	volatile U32 NUM_CCSC;
	volatile U32 NUM_RAM;
	volatile U32 NUM_TP;

	volatile U8 padding1[0x1E4];

	stptiHAL_STFE_InfoBlockDevice_t IB_CFG[STFEv2_MAX_SYS_BLOCK_SIZE];
	volatile U8 padding2[0x300];

	stptiHAL_STFE_InfoBlockDevice_t SWTS_CFG[STFEv2_MAX_SYS_BLOCK_SIZE];
	volatile U8 padding3[0x300];

	stptiHAL_STFE_InfoBlockDevice_t MIB_CFG[STFEv2_MAX_SYS_BLOCK_SIZE];

	stptiHAL_STFE_RAMInfoBlockDevice_t RAM_CFG[STFEv2_MAX_SYS_BLOCK_SIZE];
} stptiHAL_STFEv2_SystemBlockDevice_t;

//************************************************/
// TSDMA
//************************************************/
typedef struct {
	volatile U32 DEST_ROUTE[2];
} stptiHAL_STFEv2_TSDmaMixedReg;

//************************************************/
// MEMDMA
//************************************************/
typedef struct {
	volatile U32 TP_BASE;
	volatile U32 TP_TOP;
	volatile U32 TP_WP;
	volatile U32 TP_RP;
} stptiHAL_STFE_bufferTp_t;

typedef struct {
	volatile U32 MEM_BASE;
	volatile U32 MEM_TOP;
	volatile U32 TS_PKT_SIZE;
	volatile U32 ENABLED_TP;
	volatile stptiHAL_STFE_bufferTp_t TP_BUFFER_p[];
} stptiHAL_STFEv2_InputNode_t;

typedef struct {
	volatile U32 MEMDMA_FW_V;
	volatile U32 MEMDMA_PTRREC_BASE;
	volatile U32 MEMDMA_PTRREC_OFFSET;
	volatile U32 MEMDMA_ERR_BASE;
	volatile U32 MEMDMA_IDLE_IRQ;
	volatile U32 MEMDMA_FW_CFG;
} stptiHAL_STFE_DmaSWDevice_t;

typedef struct {
	volatile U32 MEMDMA_CPU_ID;
	volatile U32 MEMDMA_CPU_VCR;
	volatile U32 MEMDMA_CPU_RUN;
	volatile U32 MEMDMA_CPU_OTHERS[13];
} stptiHAL_STFE_DmaCPUDevice_t;

typedef struct {
	volatile U8 padding0[0xE00];
	volatile U32 MEMDMA_PER_TP_DREQ[0x20];		/* 0xe00 */
	volatile U32 MEMDMA_PER_TP_DACK[0x20];		/* 0xe80 */
	volatile U8 padding1[0x88];			/* 0xf00 */
	volatile U32 MEMDMA_PER_STBUS_SYNC;		/* 0xf88 */
	volatile U32 MEMDMA_PER_STBUS_ACCESS;
	volatile U32 MEMDMA_PER_STBUS_ADDRESS;
	volatile U8 padding2[0x14];
	volatile U32 MEMDMA_PER_STBUS_IDLE_INT;		/* 0xfa8 */
	volatile U32 MEMDMA_PER_PRIORITY;		/* 0xfac */
	volatile U32 MEMDMA_PER_MAX_OPCODE;		/* 0xfb0 */
	volatile U32 MEMDMA_PER_MAX_CHUNCK;		/* 0xfb4 */
	volatile U8 padding3;
	volatile U32 MEMDMA_PER_PAGE_SIZE;		/* 0xfbc */
	volatile U32 MEMDMA_PER_MBOX_STATUS[2];		/* 0xfc0 */
	volatile U32 MEMDMA_PER_MBOX_SET[2];		/* 0xfc8 */
	volatile U32 MEMDMA_PER_MBOX_CLEAR[2];		/* 0xfd0 */
	volatile U32 MEMDMA_PER_MBOX_MASK[2];		/* 0xfd8 */
	volatile U32 MEMDMA_PER_INJECT_PKT_SRC;		/* 0xfe0 */
	volatile U32 MEMDMA_PER_INKECT_PKT_DEST;	/* 0xfe4 */
	volatile U32 MEMDMA_PER_INJECT_PKT_ADDR;	/* 0xfe8 */
	volatile U32 MEMDMA_PER_INJECT_PKT;		/* 0xfec */
	volatile U32 MEMDMA_PER_PAT_PTR_INIT;		/* 0xff0 */
	volatile U32 MEMDMA_PER_PAT_PTR;		/* 0xff4 */
	volatile U32 MEMDMA_PER_SLEEP_MASK;		/* 0xff8 */
	volatile U32 MEMDMA_PER_SLEEP_COUNTER;		/* 0xffc */
} stptiHAL_STFE_DmaPERDevice_t;

typedef struct {
	U8 MEMDMA_IMEM[0x2000];
	U8 padding0[0x2000];

	union {
		stptiHAL_STFE_DmaSWDevice_t MEMDMA_SW;
		U8 MEMDMA_DMEM[0x1000];
	} MEMDMA_DMEM;
	U8 padding1[0x3000];

	/*0x8000 */
	union {
		stptiHAL_STFE_DmaCPUDevice_t MEMDMA_CPU;
		U8 padding[0x1000];
	} MEMDMA_CPU;

	/*0x9000 */
	U8 MEMDMA_RF[0x1000];
	U8 padding3[0x1000];

	/*0xb000 */
	stptiHAL_STFE_DmaPERDevice_t MEMDMA_PER;
} stptiHAL_STFEv2_DmaBlockDevice_t;

#endif // _PTI_STFE_V2_H_
