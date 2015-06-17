/************************************************************************
Copyright (C) 2011,2012 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_engine Library.

Crypto_engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : helpers.h

Declares a number of helper functions for internal use in stm_ce
************************************************************************/
#ifndef __HELPERS_H
#define __HELPERS_H
#include "stm_ce.h"
#include "stm_ce_osal.h"

/* Object manipulation functions */
int foreach_child_object(stm_object_h parent, int (*operator)(stm_object_h));

/* DMA scatterlist functions (OS-specific) */
int get_dma_scatterlist(const stm_ce_buffer_t *kbuf, int direction,
		struct scatterlist **dma_sgl, uint32_t *n_elems);
int free_dma_scatterlist(struct scatterlist *dma_sgl, uint32_t n_elems,
		int direction);
int dma_scatterlist_to_buffers(struct scatterlist *sgl, uint32_t n_elems,
		stm_ce_buffer_t *buf, uint32_t *n_bufs);
#endif
