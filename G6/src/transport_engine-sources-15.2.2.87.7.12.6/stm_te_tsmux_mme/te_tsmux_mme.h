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

Source file name : te_tsmux.h

Declares stm_te tsmux_mme functions and utility macros
******************************************************************************/

#ifndef TE_TSMUX_MME_H
#define TE_TSMUX_MME_H

#include "te_tsmux.h"
#include "te_tsg_filter.h"
#include "te_tsg_sec_filter.h"

int te_tsmux_mme_start(struct te_tsmux *tsmux_p);
int te_tsmux_mme_stop(struct te_tsmux *tsmux_p);

int te_tsmux_mme_init(void);
int te_tsmux_mme_term(void);

int te_tsmux_mme_tsg_connect(struct te_tsmux *tsmux_p,
	struct te_tsg_filter *tsg_p);
int te_tsmux_mme_tsg_disconnect(struct te_tsmux *tsmux_p,
	struct te_tsg_filter *tsg_p);

int te_tsmux_mme_send_buffer(struct te_tsmux *tsmx,
		struct te_tsg_filter *tsg,
		struct stm_se_compressed_frame_metadata_s *metadata,
		struct stm_data_block *block_list,
		uint32_t block_count);
int te_tsmux_mme_get_data(struct te_tsmux *tsmx,
		struct stm_data_block *block_list, uint32_t block_count,
		uint32_t *filled_blocks);
int te_tsmux_mme_tsg_pause(struct te_tsmux *tsmx,
		struct te_tsg_filter *tsg);
int te_tsmux_mme_testfordata(struct te_tsmux *tsmx, uint32_t *size);

int te_tsmux_mme_submit_section(struct te_tsmux *tsmx, struct te_tsg_sec_filter *sec);
int te_tsmux_mme_cancel_section(struct te_tsmux *tsmx, struct te_tsg_sec_filter *sec);

void reinit_ftd_para(struct te_tsmux *tsmx,
		struct te_tsg_filter *tsg);


#endif /*TE_TSMUX_MMEH */
