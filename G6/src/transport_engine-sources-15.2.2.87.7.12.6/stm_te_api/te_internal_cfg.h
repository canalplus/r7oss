/******************************************************************************
Copyright (C) 2011, 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

Transport Engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Transport Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : te_internal_cfg.h

Defines runtime configuration
******************************************************************************/

#ifndef __TE_INTERNAL_CFG_H
#define __TE_INTERNAL_CFG_H

#include "stm_te.h"
#include "pti_tshal_api.h"
#include "te_constants.h"
#include "te_tsmux.h"
#include <linux/types.h>
#include <asm/sizes.h>

struct te_module_config {
	unsigned int max_queued_output_data;
	unsigned int max_section_filters;
	unsigned int ts_buffer_size;
	unsigned int section_buffer_size;
	unsigned int pes_buffer_size;
	unsigned int pcr_buffer_size;
	unsigned int index_buffer_size;
	unsigned int tsg_index_buffer_size;
	unsigned int multiplex_ahead_limit;
	unsigned int dts_integrity_threshold;
	unsigned int dont_wait_limit;
	unsigned int dts_duration;
};

extern struct te_module_config te_cfg;

#define TE_DEFAULT_CONFIG {                                   \
	.max_queued_output_data  = SZ_512K,                   \
	.ts_buffer_size          = SZ_16K * DVB_PACKET_SIZE,  \
	.section_buffer_size     = 4 * TE_MAX_SECTION_SIZE,   \
	.pes_buffer_size         = SZ_16K * DVB_PACKET_SIZE,  \
	.pcr_buffer_size         = SZ_1K,                     \
	.index_buffer_size       = SZ_4K,                     \
	.tsg_index_buffer_size   = 512,                       \
	.multiplex_ahead_limit   = 30000,                     \
	.dts_integrity_threshold = 10 * 90000,                \
	.dont_wait_limit		= 130000,		\
	.dts_duration			= 1000			\
}

#endif
