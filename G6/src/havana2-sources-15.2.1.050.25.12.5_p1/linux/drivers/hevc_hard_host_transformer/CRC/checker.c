/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine Library.

Streaming Engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Streaming Engine Library may alternatively be licensed under a proprietary
license from ST.
************************************************************************/

//
// Implementation of the CRC checker component
//

#include <linux/types.h>

#include "osdev_mem.h"

#include "checker.h"
#include "crc.h"
#include "ring.h"

//
// Checker object
//

typedef struct
{
	CRCRing_t references;
	CRCRing_t intermediate_refs;
	CRCRing_t intermediate_computed;
	CRCRing_t computed;

	int       use_count;
	struct semaphore sem;
} Checker_t;

//
// Helper functions
//

static int  CompareCRCs(FrameCRC_t* ref_crc, FrameCRC_t* computed_crc);
static void MergeCRCs  (FrameCRC_t* dest, FrameCRC_t* src);

//
// Upstream API
//

// Allocate a new checker
CheckerStatus_t CHK_Alloc(CheckerHandle_t* handle)
{
	Checker_t*      checker;
	CRCRingStatus_t error;

	checker = OSDEV_Malloc(sizeof(*checker));
	if (checker == NULL)
		return CHECKER_OUT_OF_MEMORY;
	error  = RingAlloc(& checker->references,            1000);
	error |= RingAlloc(& checker->intermediate_refs,     10);   // must be larger than StreamingEngine/HEVC_codec/Configuration.DecodeContextCount
	error |= RingAlloc(& checker->intermediate_computed, 10);   // must be larger than StreamingEngine/HEVC_codec/Configuration.DecodeContextCount
	error |= RingAlloc(& checker->computed,              1000);

	sema_init(&checker->sem, 1);

	if (error != RING_NO_ERROR)
	{
		RingFree(& checker->references);
		RingFree(& checker->intermediate_refs);
		RingFree(& checker->intermediate_computed);
		RingFree(& checker->computed);
		OSDEV_Free(checker);
		return CHECKER_INTERNAL_ERROR;
	}
	checker->use_count = 1;
	*handle = checker;
	return CHECKER_NO_ERROR;
}

//
CheckerStatus_t CHK_ShutdownReferences(CheckerHandle_t handle)
{
	Checker_t* checker = (Checker_t*)handle;
	CRCRingStatus_t status = RingShutdown(& checker->references);

	if (status != RING_NO_ERROR)
		return CHECKER_INTERNAL_ERROR;
	return CHECKER_NO_ERROR;
}

//
CheckerStatus_t CHK_ShutdownComputed(CheckerHandle_t handle)
{
	Checker_t* checker = (Checker_t*)handle;
	CRCRingStatus_t status = RingShutdown(& checker->computed);

	if (status != RING_NO_ERROR)
		return CHECKER_INTERNAL_ERROR;
	return CHECKER_NO_ERROR;
}


//
static CheckerStatus_t CHK_Free(CheckerHandle_t handle)
{
	Checker_t*      checker = (Checker_t*)handle;
	CRCRingStatus_t error;

	error  = RingFree(& checker->references);
	error |= RingFree(& checker->intermediate_refs);
	error |= RingFree(& checker->intermediate_computed);
	error |= RingFree(& checker->computed);

	OSDEV_Free(checker);
	if (error != RING_NO_ERROR)
		return CHECKER_INTERNAL_ERROR;
	return CHECKER_NO_ERROR;
}

// Take & Release
CheckerStatus_t CHK_Take(CheckerHandle_t handle)
{
	Checker_t* checker = (Checker_t*)handle;

	down(&checker->sem);
	checker->use_count ++;
	up(&checker->sem);
	return CHECKER_NO_ERROR;
}

CheckerStatus_t CHK_Release(CheckerHandle_t handle)
{
	Checker_t* checker = (Checker_t*)handle;

	down(&checker->sem);
	checker->use_count --;
	if (checker->use_count == 0)
	{
		CHK_Free(handle);
		// semaphore is destroyed, no need to release it
		return CHECKER_STOPPED;
	}
	up(&checker->sem);
	return CHECKER_NO_ERROR;
}

// Adds a CRC reference
// Note: blocking
CheckerStatus_t CHK_AddRefCRC(CheckerHandle_t handle, FrameCRC_t* crc)
{
	Checker_t*      checker = (Checker_t*)handle;
	CRCRingStatus_t error = RingWriteCRC(& checker->references, crc);

	if (error == RING_STOPPED)
		return CHECKER_STOPPED;
	if (error != RING_NO_ERROR)
		return CHECKER_INTERNAL_ERROR;
	return CHECKER_NO_ERROR;
}

// Checks a computed PP CRC against its ref
// Note: blocking until shutdown
CheckerStatus_t CHK_CheckPPCRC(CheckerHandle_t handle, FrameCRC_t* computed_crc)
{
	Checker_t*      checker = (Checker_t*)handle;
	CRCRingStatus_t ringStatus;
	FrameCRC_t      ref_crc;
	int             mismatch;

	// pr_info(">>> CHK_CheckPPCRC\n");

	ringStatus = RingReadCRC(& checker->references, &ref_crc);
	if (ringStatus == RING_EMPTY)
	{
		RingShutdown(& checker->intermediate_refs);
		RingShutdown(& checker->intermediate_computed);
		return CHECKER_NO_ERROR;
	}
	if (ringStatus != RING_NO_ERROR)
		return CHECKER_INTERNAL_ERROR;

	mismatch = CompareCRCs(&ref_crc, computed_crc);
	// pr_info("CHK_CheckPPCRC: comparison %d\n", mismatch);

	ringStatus = RingWriteCRC(& checker->intermediate_refs, &ref_crc); // to be later used by CHK_CheckDecoderCRC()
	if (ringStatus != RING_NO_ERROR)
		return CHECKER_INTERNAL_ERROR;

	ringStatus = RingWriteCRC(& checker->intermediate_computed, computed_crc); // to be later used by CHK_CheckDecoderCRC()
	if (ringStatus != RING_NO_ERROR)
		return CHECKER_INTERNAL_ERROR;

	if (mismatch)
		return CHECKER_CRC_MISMATCH;

	return CHECKER_NO_ERROR;
}

// Checks a computed decode CRC against its ref
// Note: blocking until shutdown
CheckerStatus_t CHK_CheckDecoderCRC(CheckerHandle_t handle, FrameCRC_t* decoder_computed_crc)
{
	Checker_t*      checker = (Checker_t*)handle;
	CRCRingStatus_t ringStatus;
	FrameCRC_t      ref_crc, computed_pp_crc;
	int             mismatch;

	ringStatus = RingReadCRC(& checker->intermediate_refs, &ref_crc);
	if (ringStatus == RING_EMPTY)
	{
		RingShutdown(& checker->computed);
		return CHECKER_NO_ERROR;
	}
	if (ringStatus != RING_NO_ERROR)
		return CHECKER_INTERNAL_ERROR;

	mismatch = CompareCRCs(&ref_crc, decoder_computed_crc);
	// pr_info("CHK_CheckDecoderCRC: comparison %d\n", mismatch);

	ringStatus = RingReadCRC(& checker->intermediate_computed, &computed_pp_crc);
	if (ringStatus != RING_NO_ERROR)
		return CHECKER_INTERNAL_ERROR;

	MergeCRCs(decoder_computed_crc, &computed_pp_crc);

	ringStatus = RingWriteCRC(& checker->computed, decoder_computed_crc);
	if (ringStatus != RING_NO_ERROR)
		return CHECKER_INTERNAL_ERROR;

	if (mismatch)
		return CHECKER_CRC_MISMATCH;

	return CHECKER_NO_ERROR;
}

//
// Downstream API
//

// Returns a computed CRC
// Note: blocking until shutdown
CheckerStatus_t CHK_GetComputedCRC(CheckerHandle_t handle, FrameCRC_t* computed_crc)
{
	Checker_t*      checker = (Checker_t*)handle;
	CRCRingStatus_t ringStatus;

	ringStatus = RingReadCRC(& checker->computed, computed_crc);
	if (ringStatus == RING_EMPTY)
		return CHECKER_STOPPED;
	if (ringStatus != RING_NO_ERROR)
		return CHECKER_INTERNAL_ERROR;
	return CHECKER_NO_ERROR;
}

//
// CRCS Comparing & Merging
//

static int CompareCRCs(FrameCRC_t* ref_crc, FrameCRC_t* computed_crc)
{
	#define CMP(BIT, CRC) if ((ref_crc->mask & (1llu << BIT)) && (computed_crc->mask & (1llu << BIT)) && ref_crc->CRC != computed_crc->CRC) return 1

	CMP(CRC_BIT_DECODE_INDEX,	decode_index);
	CMP(CRC_BIT_IDR, 		idr);
	CMP(CRC_BIT_POC, 		poc);
	CMP(CRC_BIT_BITSTREAM, 		bitstream);

	CMP(CRC_BIT_SLI_TABLE,	sli_table);
	CMP(CRC_BIT_CTB_TABLE,	ctb_table);
	CMP(CRC_BIT_SLI_HEADER,	sli_header);
	CMP(CRC_BIT_COMMANDS,	commands);
	CMP(CRC_BIT_RESIDUALS,	residuals);

	CMP(CRC_BIT_HWC_CTB_RX,	hwc_ctb_rx);
	CMP(CRC_BIT_HWC_SLI_RX,	hwc_sli_rx);
	CMP(CRC_BIT_HWC_MVP_TX,	hwc_mvp_tx);
	CMP(CRC_BIT_HWC_RED_TX,	hwc_red_tx);
	CMP(CRC_BIT_MVP_PPB_RX,	mvp_ppb_rx);
	CMP(CRC_BIT_MVP_PPB_TX,	mvp_ppb_tx);
	CMP(CRC_BIT_MVP_CMD_TX,	mvp_cmd_tx);
	CMP(CRC_BIT_MAC_CMD_TX,	mac_cmd_tx);
	CMP(CRC_BIT_MAC_DAT_TX,	mac_dat_tx);
	CMP(CRC_BIT_XOP_CMD_TX,	xop_cmd_tx);
	CMP(CRC_BIT_XOP_DAT_TX,	xop_dat_tx);
	CMP(CRC_BIT_RED_DAT_RX,	red_dat_rx);
	CMP(CRC_BIT_RED_CMD_TX,	red_cmd_tx);
	CMP(CRC_BIT_RED_DAT_TX,	red_dat_tx);
	CMP(CRC_BIT_PIP_CMD_TX,	pip_cmd_tx);
	CMP(CRC_BIT_PIP_DAT_TX,	pip_dat_tx);
	CMP(CRC_BIT_IPR_CMD_TX,	ipr_cmd_tx);
	CMP(CRC_BIT_IPR_DAT_TX,	ipr_dat_tx);
	CMP(CRC_BIT_DBK_CMD_TX,	dbk_cmd_tx);
	CMP(CRC_BIT_DBK_DAT_TX,	dbk_dat_tx);
	CMP(CRC_BIT_SAO_CMD_TX,	sao_cmd_tx);
	CMP(CRC_BIT_RSZ_CMD_TX,	rsz_cmd_tx);
	CMP(CRC_BIT_OS_O4_TX,	os_o4_tx);
	CMP(CRC_BIT_OS_R2B_TX,	os_r2b_tx);
	CMP(CRC_BIT_OS_RSZ_TX,	os_rsz_tx);

	CMP(CRC_BIT_OMEGA_LUMA, 		omega_luma);
	CMP(CRC_BIT_OMEGA_CHROMA, 		omega_chroma);
	CMP(CRC_BIT_RASTER_LUMA, 		raster_luma);
	CMP(CRC_BIT_RASTER_CHROMA, 		raster_chroma);
	// CMP(CRC_BIT_RASTER_DECIMATED_LUMA, 	raster_decimated_luma);
	// CMP(CRC_BIT_RASTER_DECIMATED_CHROMA, raster_decimated_chroma);
	return 0;
}

static void MergeCRCs  (FrameCRC_t* dest, FrameCRC_t* src)
{
	#define MERGE(BIT, CRC) if ((!(dest->mask & (1llu << BIT))) && (src->mask & (1llu << BIT))) { dest->CRC = src->CRC; dest->mask |= (1llu << BIT); }

	MERGE(CRC_BIT_DECODE_INDEX, 	decode_index);
	MERGE(CRC_BIT_IDR, 		idr);
	MERGE(CRC_BIT_POC, 		poc);
	MERGE(CRC_BIT_BITSTREAM, 	bitstream);

	MERGE(CRC_BIT_SLI_TABLE,	sli_table);
	MERGE(CRC_BIT_CTB_TABLE,	ctb_table);
	MERGE(CRC_BIT_SLI_HEADER,	sli_header);
	MERGE(CRC_BIT_COMMANDS,		commands);
	MERGE(CRC_BIT_RESIDUALS,	residuals);

	MERGE(CRC_BIT_HWC_CTB_RX,	hwc_ctb_rx);
	MERGE(CRC_BIT_HWC_SLI_RX,	hwc_sli_rx);
	MERGE(CRC_BIT_HWC_MVP_TX,	hwc_mvp_tx);
	MERGE(CRC_BIT_HWC_RED_TX,	hwc_red_tx);
	MERGE(CRC_BIT_MVP_PPB_RX,	mvp_ppb_rx);
	MERGE(CRC_BIT_MVP_PPB_TX,	mvp_ppb_tx);
	MERGE(CRC_BIT_MVP_CMD_TX,	mvp_cmd_tx);
	MERGE(CRC_BIT_MAC_CMD_TX,	mac_cmd_tx);
	MERGE(CRC_BIT_MAC_DAT_TX,	mac_dat_tx);
	MERGE(CRC_BIT_XOP_CMD_TX,	xop_cmd_tx);
	MERGE(CRC_BIT_XOP_DAT_TX,	xop_dat_tx);
	MERGE(CRC_BIT_RED_DAT_RX,	red_dat_rx);
	MERGE(CRC_BIT_RED_CMD_TX,	red_cmd_tx);
	MERGE(CRC_BIT_RED_DAT_TX,	red_dat_tx);
	MERGE(CRC_BIT_PIP_CMD_TX,	pip_cmd_tx);
	MERGE(CRC_BIT_PIP_DAT_TX,	pip_dat_tx);
	MERGE(CRC_BIT_IPR_CMD_TX,	ipr_cmd_tx);
	MERGE(CRC_BIT_IPR_DAT_TX,	ipr_dat_tx);
	MERGE(CRC_BIT_DBK_CMD_TX,	dbk_cmd_tx);
	MERGE(CRC_BIT_DBK_DAT_TX,	dbk_dat_tx);
	MERGE(CRC_BIT_SAO_CMD_TX,	sao_cmd_tx);
	MERGE(CRC_BIT_RSZ_CMD_TX,	rsz_cmd_tx);
	MERGE(CRC_BIT_OS_O4_TX,		os_o4_tx);
	MERGE(CRC_BIT_OS_R2B_TX,	os_r2b_tx);
	MERGE(CRC_BIT_OS_RSZ_TX,	os_rsz_tx);

	MERGE(CRC_BIT_OMEGA_LUMA, 		omega_luma);
	MERGE(CRC_BIT_OMEGA_CHROMA, 		omega_chroma);
	MERGE(CRC_BIT_RASTER_LUMA, 		raster_luma);
	MERGE(CRC_BIT_RASTER_CHROMA, 		raster_chroma);
	MERGE(CRC_BIT_RASTER_DECIMATED_LUMA, 	raster_decimated_luma);
	MERGE(CRC_BIT_RASTER_DECIMATED_CHROMA, 	raster_decimated_chroma);
}

