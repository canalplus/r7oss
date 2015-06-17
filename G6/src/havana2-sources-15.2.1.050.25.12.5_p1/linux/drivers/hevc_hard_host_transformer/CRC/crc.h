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
// List of CRCs
//

#ifndef H_CRC_DEF
#define H_CRC_DEF

#include <linux/types.h>

// Specifies which fields are available in the ref or the computed frame CRC

typedef enum
{
	CRC_BIT_DECODE_INDEX = 0,
	CRC_BIT_IDR,
	CRC_BIT_POC,

	CRC_BIT_BITSTREAM,
	CRC_BIT_SLI_TABLE,
	CRC_BIT_CTB_TABLE,
	CRC_BIT_SLI_HEADER,
	CRC_BIT_COMMANDS,
	CRC_BIT_RESIDUALS,

	CRC_BIT_HWC_CTB_RX,
	CRC_BIT_HWC_SLI_RX,
	CRC_BIT_HWC_MVP_TX,
	CRC_BIT_HWC_RED_TX,
	CRC_BIT_MVP_PPB_RX,
	CRC_BIT_MVP_PPB_TX,
	CRC_BIT_MVP_CMD_TX,
	CRC_BIT_MAC_CMD_TX,
	CRC_BIT_MAC_DAT_TX,
	CRC_BIT_XOP_CMD_TX,
	CRC_BIT_XOP_DAT_TX,
	CRC_BIT_RED_DAT_RX,
	CRC_BIT_RED_CMD_TX,
	CRC_BIT_RED_DAT_TX,
	CRC_BIT_PIP_CMD_TX,
	CRC_BIT_PIP_DAT_TX,
	CRC_BIT_IPR_CMD_TX,
	CRC_BIT_IPR_DAT_TX,
	CRC_BIT_DBK_CMD_TX,
	CRC_BIT_DBK_DAT_TX,
	CRC_BIT_SAO_CMD_TX,
	CRC_BIT_RSZ_CMD_TX,
	CRC_BIT_OS_O4_TX,
	CRC_BIT_OS_R2B_TX,
	CRC_BIT_OS_RSZ_TX,

	CRC_BIT_OMEGA_LUMA,
	CRC_BIT_OMEGA_CHROMA,
	CRC_BIT_RASTER_LUMA,
	CRC_BIT_RASTER_CHROMA,
	CRC_BIT_RASTER_DECIMATED_LUMA,
	CRC_BIT_RASTER_DECIMATED_CHROMA,
} CRCMask_t;

// All the CRCs computed by the Hades IP (see crc_*.log files)
// Same order as in the log files

typedef struct
{
	// Bit Mask
	uint64_t mask;

	//
	// picture info
	//

	uint32_t decode_index;      // in decode order, starting at 0
	int      idr;               // flags IDR frames
	int32_t  poc;               // picture order count

	//
	// CRCs
	//

	uint32_t bitstream;         // preprocessor input

	// intermediate buffer: preprocessor output, decoder input

	uint32_t sli_table;
	uint32_t ctb_table;
	uint32_t sli_header;
	uint32_t commands;
	uint32_t residuals;

	// decoder internals

	uint32_t hwc_ctb_rx;
	uint32_t hwc_sli_rx;
	uint32_t hwc_mvp_tx;
	uint32_t hwc_red_tx;
	uint32_t mvp_ppb_rx;
	uint32_t mvp_ppb_tx;
	uint32_t mvp_cmd_tx;
	uint32_t mac_cmd_tx;
	uint32_t mac_dat_tx;
	uint32_t xop_cmd_tx;
	uint32_t xop_dat_tx;
	uint32_t red_dat_rx;
	uint32_t red_cmd_tx;
	uint32_t red_dat_tx;
	uint32_t pip_cmd_tx;
	uint32_t pip_dat_tx;
	uint32_t ipr_cmd_tx;
	uint32_t ipr_dat_tx;
	uint32_t dbk_cmd_tx;
	uint32_t dbk_dat_tx;
	uint32_t sao_cmd_tx;
	uint32_t rsz_cmd_tx;
	uint32_t os_o4_tx;
	uint32_t os_r2b_tx;
	uint32_t os_rsz_tx;

	// output files

	uint32_t omega_luma;               // decoder reference output
	uint32_t omega_chroma;
	uint32_t raster_luma;              // decoder main display output
	uint32_t raster_chroma;
	uint32_t raster_decimated_luma;	   // decoder decimated display output
	uint32_t raster_decimated_chroma;

} FrameCRC_t;

#define CRC_INIT 0xffffffffu

uint32_t crc_update(uint32_t word, uint8_t bits, uint32_t result);
uint32_t crc_rot(uint8_t *base, uint32_t start, uint32_t stop, uint32_t end);
uint32_t crc_rot_swap(uint8_t *base, uint32_t start, uint32_t stop, uint32_t end);
uint32_t crc_1d(uint8_t *buffer, uint32_t size);
uint32_t crc_2d(uint8_t *buffer, uint16_t stride, uint16_t h_size, uint16_t v_size);
uint32_t crc_3d(uint8_t *buffer1, uint8_t *buffer2, uint16_t stride, uint16_t h_size, uint16_t v_size);

#endif // H_CRC_DEF
