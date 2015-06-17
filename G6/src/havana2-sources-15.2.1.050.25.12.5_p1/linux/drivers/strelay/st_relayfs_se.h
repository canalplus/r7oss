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
#ifndef _ST_RELAYFS_SE_H_
#define _ST_RELAYFS_SE_H_

#include "st_relayfs.h"

#ifdef __cplusplus
extern "C" {
#endif

//////// SE sources & types
enum {
	ST_RELAY_SOURCE_SE = ST_RELAY_OFFSET_SE_START,

	ST_RELAY_SOURCE_CONTROL_TEST,
	ST_RELAY_SOURCE_CONTROL_TEST_DISCONTINUITY,
};

enum {
	ST_RELAY_TYPE_PES_AUDIO_BUFFER = ST_RELAY_OFFSET_SE_START,
	ST_RELAY_TYPE_PES_VIDEO_BUFFER,

	ST_RELAY_TYPE_CODED_AUDIO_BUFFER,
	ST_RELAY_TYPE_CODED_AUDIO_BUFFER1,
	ST_RELAY_TYPE_CODED_AUDIO_BUFFER2,
	ST_RELAY_TYPE_CODED_AUDIO_BUFFER3,

	ST_RELAY_TYPE_CODED_VIDEO_BUFFER,
	ST_RELAY_TYPE_CODED_VIDEO_BUFFER1,
	ST_RELAY_TYPE_CODED_VIDEO_BUFFER2,
	ST_RELAY_TYPE_CODED_VIDEO_BUFFER3,

	ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER0,
	ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER1,
	ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER2,
	ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER3,

	ST_RELAY_TYPE_DECODED_AUDIO_BUFFER,
	ST_RELAY_TYPE_DECODED_AUDIO_BUFFER1,
	ST_RELAY_TYPE_DECODED_AUDIO_BUFFER2,
	ST_RELAY_TYPE_DECODED_AUDIO_BUFFER3,

	ST_RELAY_TYPE_DECODED_VIDEO_BUFFER,
	ST_RELAY_TYPE_DECODED_VIDEO_BUFFER1,
	ST_RELAY_TYPE_DECODED_VIDEO_BUFFER2,
	ST_RELAY_TYPE_DECODED_VIDEO_BUFFER3,

	ST_RELAY_TYPE_SWCRC,
	ST_RELAY_TYPE_SWCRC1,
	ST_RELAY_TYPE_SWCRC2,
	ST_RELAY_TYPE_SWCRC3,

	ST_RELAY_TYPE_DATA_TO_PCM0,
	ST_RELAY_TYPE_DATA_TO_PCM1,
	ST_RELAY_TYPE_DATA_TO_PCM2,
	ST_RELAY_TYPE_DATA_TO_PCM3,
	ST_RELAY_TYPE_DATA_TO_PCM4,
	ST_RELAY_TYPE_DATA_TO_PCM5,
	ST_RELAY_TYPE_DATA_TO_PCM6,
	ST_RELAY_TYPE_DATA_TO_PCM7,

	ST_RELAY_TYPE_DATA_FROM_PCM0,
	ST_RELAY_TYPE_AUDIO_INTERMIXER_PCM,

	ST_RELAY_TYPE_AUDIO_MIXER_CRC,
	ST_RELAY_TYPE_AUDIO_DEC_CRC,
	ST_RELAY_TYPE_AUDIO_TRANSCODE,
	ST_RELAY_TYPE_AUDIO_COMPRESSED_FRAME,

	ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT,
	ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT1,
	ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT2,
	ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT3,

	ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT,
	ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT1,
	ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT2,
	ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT3,

	ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT,
	ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT1,
	ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT2,
	ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT3,

	ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT,
	ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT1,
	ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT2,
	ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT3,

	ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT,
	ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT1,
	ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT2,
	ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT3,

	ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT,
	ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT1,
	ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT2,
	ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT3,

	ST_RELAY_TYPE_CLOCK_RECOV_DATAPOINT,
	ST_RELAY_TYPE_AUDIO_CLOCK_DATAPOINT,
	ST_RELAY_TYPE_VIDEO_CLOCK_DATAPOINT,

	ST_RELAY_TYPE_CONTROL_FWD_DISCONTINUITY,
	ST_RELAY_TYPE_CONTROL_BWD_DISCONTINUITY,
	ST_RELAY_TYPE_CONTROL_BWD_SMOOTH_DISCONTINUITY,
	ST_RELAY_TYPE_CONTROL_SPLICING,
	// Ktm module
	ST_RELAY_TYPE_CONTROL_REQ_TIME,

	// hevc module
	ST_RELAY_TYPE_HEVC_HW_DECODING_TIME,
	ST_RELAY_TYPE_HEVC_MEMORY_CHECK,

	// hevc custom logs.. only if HEVC_CODEC_DUMP_MME defined (not by default)
	ST_RELAY_TYPE_MME_TEXT1, // human-readable text (codec specific)
	ST_RELAY_TYPE_MME_TEXT2, // human-readable text (codec specific)

	// mme log module
	ST_RELAY_TYPE_MME_LOG,   // binary dump

	// dont add types beyond this point
	ST_RELAY_TYPE_LAST_SE
};

// nb entries source-type for SE: includes SE proper (1 source) + control data (5 types * 2 sources)
#define NB_ENTRIES_SOURCE_TYPE_SE  ((ST_RELAY_TYPE_LAST_SE - ST_RELAY_OFFSET_SE_START) + (5 * 2))

// depends on RELAY and DEBUG_FS
#if defined(CONFIG_RELAY) && defined(CONFIG_DEBUG_FS)
bool is_client_se(int value);
int st_relayfs_open_se(void);
void st_relayfs_close_se(void);
//
void st_relayfs_write_se(unsigned int type, unsigned int source, unsigned char *buf, unsigned int len, void *info);
void st_relayfs_print_se(unsigned int type, unsigned int source, const char *format, ...);
//
unsigned int st_relayfs_getindex_forsource_se(unsigned int source);
void st_relayfs_freeindex_forsource_se(unsigned int source, unsigned int index);
//
unsigned int st_relayfs_getindex_fortype_se(unsigned int type);
void st_relayfs_freeindex_fortype_se(unsigned int type, unsigned int index);
#else
static inline bool is_client_se(int value) { return false; }
static inline int st_relayfs_open_se(void) { return 0; }
static inline void st_relayfs_close_se(void) {}
//
static inline void st_relayfs_write_se(unsigned int id, unsigned int source,
                                       unsigned char *buf, unsigned int len,
                                       void *info) {}
static inline void st_relayfs_print_se(unsigned int id, unsigned int source, const char *format, ...) {}
//
static inline unsigned int st_relayfs_getindex_forsource_se(unsigned int source) { return 0; }
static inline void st_relayfs_freeindex_forsource_se(unsigned int source, unsigned int index) {}
//
static inline unsigned int st_relayfs_getindex_fortype_se(unsigned int type) { return 0; }
static inline void st_relayfs_freeindex_fortype_se(unsigned int type, unsigned int index) {}
#endif  // defined(CONFIG_RELAY) && defined(CONFIG_DEBUG_FS)

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* _ST_RELAYFS_SE_H_ */

