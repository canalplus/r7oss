/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef _HEVCPPIO_H_
#define _HEVCPPIO_H_

#include <hevc_video_transformer_types.h>

#define HEVCPP_DEVICE_NAME   "hevcpp"

#define HEVCPP_DEVICE        "/dev/"HEVCPP_DEVICE_NAME

#define HEVC_PP_IOCTL_QUEUE_BUFFER              1
#define HEVC_PP_IOCTL_GET_PREPROCESSED_BUFFER   2
#define HEVC_PP_IOCTL_GET_PREPROC_AVAILABILITY  3

#define HEVCPP_ERROR_STATUS_END_OF_PROCESS       (0x1)
#define HEVCPP_ERROR_STATUS_PICTURE_COMPLETED    (0x2)
#ifdef HEVC_HADES_CUT1
/** pp error status register's bits:
 * [31-10] reserved, [9] entropy_decode_error,
 * [8] range_error, [7] EBR_forbidden_4bytes, [6] EBR_forbidden_3bytes,
 * [5] move_out_of_dma, [4] end_of_dma , [3] startcode_detected,
 * [2] end_of_stream_issue, [1] picture_completed, [0] end_of_process
 * Allow to decode the picture even if the preprocessor returns particular
 * errors (processed as warnings) without hanging the decoder. */
#define HEVCPP_ERROR_STATUS_END_OF_STREAM_ISSUE  (0x4)
#define HEVCPP_ERROR_STATUS_STARTCODE_DETECTED   (0x8)
#define HEVCPP_ERROR_STATUS_END_OF_DMA           (0x10)
#define HEVCPP_ERROR_STATUS_MOVE_OUT_OF_DMA      (0x20)
#define HEVCPP_ERROR_STATUS_EBR_FORBIDDEN_3BYTES (0x40)
#define HEVCPP_ERROR_STATUS_EBR_FORBIDDEN_4BYTES (0x80)
#define HEVCPP_ERROR_STATUS_RANGE_ERROR          (0x100)
#define HEVCPP_ERROR_STATUS_ENTROPY_DECODE_ERROR (0x200)
#else
#define HEVCPP_ERROR_STATUS_END_OF_DMA             (1U<<2)
#define HEVCPP_ERROR_STATUS_MOVE_OUT_OF_DMA        (1U<<3)
#define HEVCPP_ERROR_STATUS_ERROR_ON_HEADER_FLAG   (1U<<4)
#define HEVCPP_ERROR_STATUS_ERROR_ON_DATA_FLAG     (1U<<5)
#define HEVCPP_ERROR_STATUS_UNEXPECTED_SC          (1U<<6)
#define HEVCPP_ERROR_STATUS_SLICE_TABLE_OVERFLOW   (1U<<7)
#define HEVCPP_ERROR_STATUS_SLICE_HEADER_OVERFLOW  (1U<<8)
#define HEVCPP_ERROR_STATUS_CTB_TABLE_OVERFLOW     (1U<<9)
#define HEVCPP_ERROR_STATUS_CTB_CMD_OVERFLOW       (1U<<10)
#define HEVCPP_ERROR_STATUS_RES_CMD_OVERFLOW       (1U<<11)
#define HEVCPP_ERROR_STATUS_BUFFER1_DONE           (1U<<16)
#define HEVCPP_ERROR_STATUS_BUFFER2_DONE           (1U<<17)
#define HEVCPP_ERROR_STATUS_BUFFER3_DONE           (1U<<18)
#define HEVCPP_ERROR_STATUS_BUFFER4_DONE           (1U<<19)
#define HEVCPP_ERROR_STATUS_BUFFER5_DONE           (1U<<20)
#define HEVCPP_ERROR_STATUS_DISABLE_OVHD_VIOLATION (1U<<30)
#define HEVCPP_ERROR_STATUS_DISABLE_HEVC_VIOLATION (1U<<31)
#endif

#define HEVCPP_NO_ERROR  (HEVCPP_ERROR_STATUS_END_OF_DMA | \
		HEVCPP_ERROR_STATUS_PICTURE_COMPLETED | HEVCPP_ERROR_STATUS_END_OF_PROCESS)

#define HEVCPP_ERC_NO_ERROR HEVCPP_ERROR_STATUS_END_OF_PROCESS

/* 64 bytes alignement */
#define HEVCPP_BUFFER_ALIGNMENT               0x3FU

// HEVC intermediate buffer indices
#define HEVC_SLICE_TABLE                    0
#define HEVC_CTB_TABLE                      1
#define HEVC_SLICE_HEADERS                  2
#define HEVC_CTB_COMMANDS                   3
#define HEVC_CTB_RESIDUALS                  4
#define HEVC_NUM_INTERMEDIATE_BUFFERS       5

typedef enum {
	hevcpp_ok = 0,
	hevcpp_timeout,
	hevcpp_unrecoverable_error,
	hevcpp_recoverable_error,
	hevcpp_error
} hevcpp_status_t;

struct hevcpp_ioctl_queue_t {
	hevcdecpreproc_transform_param_t iCmd;
	// Cached address of base address of circular bit buffer stream (mapped to
	// physical address referred by iCmd.bit_buffer.base_addr)
	void *iCodedBufferCachedAddress;
	// Cached address of intermediate buffers
	void *iIntermediateBufferCachedAddress[HEVC_NUM_INTERMEDIATE_BUFFERS];
	// Decode index of picture used to reference intermediate buffer CRCs
	unsigned int iDecodeIndex;
};

struct hevcpp_ioctl_dequeue_t {
	hevcdecpreproc_command_status_t iStatus;  //task status
	hevcpp_status_t hwStatus;                 //HW status
};

#endif
