/******************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : stm_te_if_tsmux.h

Declares the private interface between stm_te tsmux objects and other
objects
******************************************************************************/
#ifndef _STM_TE_IF_TSMUX_H
#define _STM_TE_IF_TSMUX_H

#include "stm_common.h"
#include "stm_data_interface.h"
#include "stm_te.h"
#include "stm_se.h"

#define STM_TE_ASYNC_DATA_INTERFACE "stm-te-async-data-interface"

/* Supplied by a source object to the sink object in the sources
 * call to connect for an async buffer interface*/
typedef struct stm_te_async_data_interface_src {
        int __must_check (*release_buffer)(stm_object_h src_object,
                                   struct stm_data_block *block_list,
                                   uint32_t block_count);
} stm_te_async_data_interface_src_t;

/* When only the PTS is valid for the buffer, set the DTS to this value */
#define TSMUX_DTS_UNSPECIFIED 0xffffffffffffffffull

typedef enum stm_te_tsmux_frame_type_e {
	STM_TSMUX_FRAME_TYPE_UNKNOWN = 0,
	STM_TSMUX_FRAME_TYPE_I,
	STM_TSMUX_FRAME_TYPE_P,
	STM_TSMUX_FRAME_TYPE_B,
} stm_te_tsmux_frame_type_t;

/* Metadata passed to tsmux tsg filter with every buffer queued */
/*
struct stm_te_tsmux_stream_metadata {
	unsigned long long PTS;
	unsigned long long DTS;
	int dit_transition;
	int rap_transition;
	stm_te_tsmux_frame_type_t frame_type;
	int end_of_stream;
};
*/

/* Interface that is registered by an object implementing the async
 * abstraction. The source will use these functions to connect (supplying
 * it's own function that will be used to release a buffer) and disconnect,
 * It is up to the connecting source to ensure that buffers made available via
 * the queue_buffer method meet the requirement of the sink */
typedef struct stm_te_async_data_interface_sink {
        int __must_check (*connect)(stm_object_h src_object,
                                stm_object_h sink_object,
                                struct stm_te_async_data_interface_src *async_src);
	int (*disconnect)(stm_object_h src_object,
                                   stm_object_h sink_object);
        int __must_check (*queue_buffer)(stm_object_h sink_object,
			struct stm_se_compressed_frame_metadata_s *metadata,
                        struct stm_data_block *block_list,
                        uint32_t block_count,
                        uint32_t *data_blocks);
	enum stm_data_mem_type mem_type;
        stm_memory_iomode_t mode;
	uint32_t alignment;
        uint32_t max_transfer;
        uint32_t paketized;
} stm_te_async_data_interface_sink_t;


#endif /*_STM_TE_IF_TSMUX_H*/
