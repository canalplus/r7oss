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
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/err.h>

#include "st_relayfs_se.h"
#include "st_relay_core.h"

// hash table used to init relay entries; entry names are exposed in st_relay/ debugfs dir
struct st_relay_typename_s  relay_entries_init_se[] = {
	// Ability to capture PES data for only one single audio/video stream (at collator level)
	// When running in multi-instances
	// the captured buffer will be a multiplex of the PES data of all the active audio/video streams
	{ ST_RELAY_TYPE_PES_AUDIO_BUFFER, "PesForCollatorAudio" },
	{ ST_RELAY_TYPE_PES_VIDEO_BUFFER, "PesForCollatorVideo" },
	// Ability to capture coded ES data for 4 audio streams (at frame parser level)
	{ ST_RELAY_TYPE_CODED_AUDIO_BUFFER,  "CodedForFrameParserAudio0" },
	{ ST_RELAY_TYPE_CODED_AUDIO_BUFFER1, "CodedForFrameParserAudio1" },
	{ ST_RELAY_TYPE_CODED_AUDIO_BUFFER2, "CodedForFrameParserAudio2" },
	{ ST_RELAY_TYPE_CODED_AUDIO_BUFFER3, "CodedForFrameParserAudio3" },
	// Ability to capture coded ES data for 4 video streams (at frame parser level)
	{ ST_RELAY_TYPE_CODED_VIDEO_BUFFER,  "CodedForFrameParserVideo0" },
	{ ST_RELAY_TYPE_CODED_VIDEO_BUFFER1, "CodedForFrameParserVideo1" },
	{ ST_RELAY_TYPE_CODED_VIDEO_BUFFER2, "CodedForFrameParserVideo2" },
	{ ST_RELAY_TYPE_CODED_VIDEO_BUFFER3, "CodedForFrameParserVideo3" },
	// Ability to capture aux buffers for 4 audio streams (at codec level))
	{ ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER0, "DecodedForAuxBuffer0" },
	{ ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER1, "DecodedForAuxBuffer1" },
	{ ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER2, "DecodedForAuxBuffer2" },
	{ ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER3, "DecodedForAuxBuffer3" },
	// Ability to capture decoded frame buffers for 4 audio streams (at manifestor level)
	{ ST_RELAY_TYPE_DECODED_AUDIO_BUFFER,  "DecodedForManifestorAudio0" },
	{ ST_RELAY_TYPE_DECODED_AUDIO_BUFFER1, "DecodedForManifestorAudio1" },
	{ ST_RELAY_TYPE_DECODED_AUDIO_BUFFER2, "DecodedForManifestorAudio2" },
	{ ST_RELAY_TYPE_DECODED_AUDIO_BUFFER3, "DecodedForManifestorAudio3" },
	// Ability to capture decoded frame buffers for 4 video streams (at manifestor level)
	{ ST_RELAY_TYPE_DECODED_VIDEO_BUFFER,  "DecodedForManifestorVideo0" },
	{ ST_RELAY_TYPE_DECODED_VIDEO_BUFFER1, "DecodedForManifestorVideo1" },
	{ ST_RELAY_TYPE_DECODED_VIDEO_BUFFER2, "DecodedForManifestorVideo2" },
	{ ST_RELAY_TYPE_DECODED_VIDEO_BUFFER3, "DecodedForManifestorVideo3" },
	//
	{ ST_RELAY_TYPE_SWCRC,  "SW_CRC0" },
	{ ST_RELAY_TYPE_SWCRC1, "SW_CRC1" },
	{ ST_RELAY_TYPE_SWCRC2, "SW_CRC2" },
	{ ST_RELAY_TYPE_SWCRC3, "SW_CRC3" },
	//
	{ ST_RELAY_TYPE_DATA_TO_PCM0, "DataToPCM0" },
	{ ST_RELAY_TYPE_DATA_TO_PCM1, "DataToPCM1" },
	{ ST_RELAY_TYPE_DATA_TO_PCM2, "DataToPCM2" },
	{ ST_RELAY_TYPE_DATA_TO_PCM3, "DataToPCM3" },
	{ ST_RELAY_TYPE_DATA_TO_PCM4, "DataToPCM4" },
	{ ST_RELAY_TYPE_DATA_TO_PCM5, "DataToPCM5" },
	{ ST_RELAY_TYPE_DATA_TO_PCM6, "DataToPCM6" },
	{ ST_RELAY_TYPE_DATA_TO_PCM7, "DataToPCM7" },
	//
	{ ST_RELAY_TYPE_DATA_FROM_PCM0, "DataFromPCM0" },
	//
	{ ST_RELAY_TYPE_AUDIO_MIXER_CRC, "AudioMixerCrc" },
	{ ST_RELAY_TYPE_AUDIO_INTERMIXER_PCM, "InterMixerPcmBuffer" },
	{ ST_RELAY_TYPE_AUDIO_DEC_CRC  , "AudioDecoderCrc" },
	{ ST_RELAY_TYPE_AUDIO_TRANSCODE, "AudioTranscodedBuffer" },
	{ ST_RELAY_TYPE_AUDIO_COMPRESSED_FRAME, "AudioCompressedFrameFromDecoder" },
	//
	// ENCODE path capture points
	//
	// Ability to capture uncompressed data at preproc input level for 4 audio streams
	{ ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT,  "EncoderPreprocInputAudio0" },
	{ ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT1, "EncoderPreprocInputAudio1" },
	{ ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT2, "EncoderPreprocInputAudio2" },
	{ ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT3, "EncoderPreprocInputAudio3" },
	// Ability to capture uncompressed data at preproc input level for 4 video streams
	{ ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT,  "EncoderPreprocInputVideo0" },
	{ ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT1, "EncoderPreprocInputVideo1" },
	{ ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT2, "EncoderPreprocInputVideo2" },
	{ ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT3, "EncoderPreprocInputVideo3" },
	// Ability to capture uncompressed data at coder input level for 4 audio streams
	{ ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT,  "EncoderCoderInputAudio0" },
	{ ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT1, "EncoderCoderInputAudio1" },
	{ ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT2, "EncoderCoderInputAudio2" },
	{ ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT3, "EncoderCoderInputAudio3" },
	// Ability to capture uncompressed data at coder input level for 4 video streams
	{ ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT,  "EncoderCoderInputVideo0" },
	{ ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT1, "EncoderCoderInputVideo1" },
	{ ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT2, "EncoderCoderInputVideo2" },
	{ ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT3, "EncoderCoderInputVideo3" },
	// Ability to capture encoded ES data at transporter level for 4 audio streams
	{ ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT,  "EncoderTransporterInputAudio0" },
	{ ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT1, "EncoderTransporterInputAudio1" },
	{ ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT2, "EncoderTransporterInputAudio2" },
	{ ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT3, "EncoderTransporterInputAudio3" },
	// Ability to capture encoded ES data at transporter level for 4 video streams
	{ ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT,  "EncoderTransporterInputVideo0" },
	{ ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT1, "EncoderTransporterInputVideo1" },
	{ ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT2, "EncoderTransporterInputVideo2" },
	{ ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT3, "EncoderTransporterInputVideo3" },
	//
	{ ST_RELAY_TYPE_CLOCK_RECOV_DATAPOINT, "CRDataPoint" },
	{ ST_RELAY_TYPE_AUDIO_CLOCK_DATAPOINT, "AudioClockDataPoint" },
	{ ST_RELAY_TYPE_VIDEO_CLOCK_DATAPOINT, "VideoClockDataPoint" },
	//
	{ ST_RELAY_TYPE_CONTROL_FWD_DISCONTINUITY, "DetectedControlFwdDiscontinuity" },
	{ ST_RELAY_TYPE_CONTROL_BWD_DISCONTINUITY, "DetectedControlBwdDiscontinuity" },
	{ ST_RELAY_TYPE_CONTROL_BWD_SMOOTH_DISCONTINUITY, "DetectedControlSmoothReverseControlData" },
	{ ST_RELAY_TYPE_CONTROL_SPLICING, "DetectedControlSplicing" },
	//
	{ ST_RELAY_TYPE_CONTROL_REQ_TIME, "DetectedControlReqTime" },
	//
	{ ST_RELAY_TYPE_HEVC_HW_DECODING_TIME, "HwDecodingTime" },
	{ ST_RELAY_TYPE_HEVC_MEMORY_CHECK, "MemoryCheck" },
	//
	{ ST_RELAY_TYPE_MME_TEXT1, "MMELogText1" },
	{ ST_RELAY_TYPE_MME_TEXT2, "MMELogText2" },
	//
	{ ST_RELAY_TYPE_MME_LOG, "MMELog" }
};
struct st_relay_entry_s *relay_entries_se;
struct st_relay_source_type_count_s *relay_source_type_counts_se;

// tables used to store free indexes of independent captures in multi-instances
unsigned int coded_audio_indexes[4]       = { 0, 0, 0, 0 };  // ST_RELAY_TYPE_CODED_AUDIO_BUFFER
unsigned int coded_video_indexes[4]       = { 0, 0, 0, 0 };  // ST_RELAY_TYPE_CODED_VIDEO_BUFFER
unsigned int decoded_audio_aux_indexes[4] = { 0, 0, 0, 0 };  // ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER
unsigned int decoded_audio_indexes[4]     = { 0, 0, 0, 0 };  // ST_RELAY_TYPE_DECODED_AUDIO_BUFFER
unsigned int decoded_video_indexes[4]     = { 0, 0, 0, 0 };  // ST_RELAY_TYPE_DECODED_VIDEO_BUFFER and ST_RELAY_TYPE_SWCRC (reuse)

unsigned int encoder_preproc_audio_indexes[4]       = { 0, 0, 0, 0 };  // ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT
unsigned int encoder_preproc_video_indexes[4]       = { 0, 0, 0, 0 };  // ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT
unsigned int encoder_coder_audio_indexes[4]         = { 0, 0, 0, 0 };  // ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT
unsigned int encoder_coder_video_indexes[4]         = { 0, 0, 0, 0 };  // ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT
unsigned int encoder_transporter_audio_indexes[4]   = { 0, 0, 0, 0 };  // ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT
unsigned int encoder_transporter_video_indexes[4]   = { 0, 0, 0, 0 };  // ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT

unsigned int control_data_indexes[2]      = { 0, 0 };         // ST_RELAY_SOURCE_CONTROL_TEST+

bool is_client_se(int value)
{
	if ((value >= ST_RELAY_OFFSET_SE_START) && (value <= ST_RELAY_OFFSET_SE_END)) {
		return true;
	} else {
		return false;
	}
}

void st_relayfs_write_se(unsigned int type, unsigned int source, unsigned char *buf, unsigned int len,
                         void *info)
{
	struct st_relay_entry_header_s header;
	int res;
	int n;
	int relay_entries_nb = ARRAY_SIZE(relay_entries_init_se);
	struct st_relay_entry_s *relay_entries = relay_entries_se;
	int relay_source_type_counts_nb = NB_ENTRIES_SOURCE_TYPE_SE;
	struct st_relay_source_type_count_s *relay_source_type_counts = relay_source_type_counts_se;
	struct st_relay_entry_s *relay_entry = NULL;
	struct st_relay_source_type_count_s *sourcetype_entry = NULL;

	if (buf == NULL) {
		if (len != 0) {
			pr_warn("warning: %s null buf with len:%d\n", __func__, len);
		}
		return;
	}

	if (relay_entries == NULL) {
		// relay channel was not open; return silently
		return;
	}

	// get entry associated to type
	for (n = 0; n < relay_entries_nb; n++) {
		if (relay_entries[n].type == type) {
			relay_entry = &relay_entries[n];
			break;
		}
	}
	if (relay_entry == NULL) {
		pr_err("Error: %s invalid type:%d - no entry\n", __func__, type);
		return;
	}

	// return silently if not active
	if (relay_entry->active == 0) {
		return;
	}

	// get source-type count associated
	for (n = 0; n < relay_source_type_counts_nb; n++) {
		if ((relay_source_type_counts[n].source == 0) && (relay_source_type_counts[n].type == 0)) {
			// first empty slot : take it
			relay_source_type_counts[n].source = source;  // no control on source validity
			relay_source_type_counts[n].type = type;
			relay_source_type_counts[n].count = 0;
			sourcetype_entry = &relay_source_type_counts[n];
			break;
		} else if ((relay_source_type_counts[n].source == source) && (relay_source_type_counts[n].type == type)) {
			// reuse previously set slot
			sourcetype_entry = &relay_source_type_counts[n];
			break;
		}
	}

	if (sourcetype_entry == NULL) {
		pr_err("Error: %s no source type entry: table full\n", __func__);
		return;
	}

	// prepare header
	strncpy(header.name, relay_entry->name, sizeof(header.name));
	header.name[sizeof(header.name) - 1] = '\0';
	header.x      = 0;	//mini meta data
	header.y      = 0;
	header.z      = 0;
	header.ident  = ST_RELAY_MAGIC_IDENT;
	header.source = source;
	header.count  = sourcetype_entry->count;
	header.len    = len;

	// check for specific header contents
	switch (type) {
	case ST_RELAY_TYPE_PES_VIDEO_BUFFER:
	case ST_RELAY_TYPE_PES_AUDIO_BUFFER:
	case ST_RELAY_TYPE_CLOCK_RECOV_DATAPOINT: {
		if (info) {
			unsigned long long *local_time = (unsigned long long *) info;
			header.y      = (unsigned long)((*local_time) >> 32);
			header.x      = (unsigned long)(*local_time) ;
		}
		break;
	}
	default:
		break;
	}

	res = st_relay_core_write_headerandbuffer(&header, sizeof(header), buf, len);
	if (res != 0) {
		if (sourcetype_entry) { sourcetype_entry->count++; }
	}
}

void st_relayfs_print_se(unsigned int type, unsigned int source, const char *format, ...)
{
	char	buffer[256];
	va_list	list;
	int 	length;

	va_start(list, format);
	length = vsnprintf(buffer, sizeof(buffer), format, list);
	buffer[sizeof(buffer) - 1] = '\0';
	va_end(list);

	if (length > sizeof(buffer)) {
		// vsnprintf returns the length that would have been written if buffer had been big enough
		pr_err("Error: %s buffer can not print %d data (skipped)\n", __func__, length);
		length = sizeof(buffer);
	}

	st_relayfs_write_se(type, source, buffer, length, NULL);
}

unsigned int st_relayfs_getindex_forsource_se(unsigned int source)
{
	unsigned int index = 0;
	if (source == ST_RELAY_SOURCE_CONTROL_TEST) {
		TAKE_FIRST_AVAILABLE_INDEX(index, control_data_indexes);
	} else {
		pr_err("Error: %s invalid source %d\n", __func__, source);
	}
	return index;
}

unsigned int st_relayfs_getindex_fortype_se(unsigned int type)
{
	unsigned int index = 0;

	switch (type) {
	case ST_RELAY_TYPE_CODED_AUDIO_BUFFER:
		TAKE_FIRST_AVAILABLE_INDEX(index, coded_audio_indexes);
		break;
	case ST_RELAY_TYPE_CODED_VIDEO_BUFFER:
		TAKE_FIRST_AVAILABLE_INDEX(index, coded_video_indexes);
		break;
	case ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER0:
		TAKE_FIRST_AVAILABLE_INDEX(index, decoded_audio_aux_indexes);
		break;
	case ST_RELAY_TYPE_DECODED_AUDIO_BUFFER:
		TAKE_FIRST_AVAILABLE_INDEX(index, decoded_audio_indexes);
		break;
	case ST_RELAY_TYPE_DECODED_VIDEO_BUFFER:
		TAKE_FIRST_AVAILABLE_INDEX(index, decoded_video_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT:
		TAKE_FIRST_AVAILABLE_INDEX(index, encoder_preproc_audio_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT:
		TAKE_FIRST_AVAILABLE_INDEX(index, encoder_preproc_video_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT:
		TAKE_FIRST_AVAILABLE_INDEX(index, encoder_coder_audio_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT:
		TAKE_FIRST_AVAILABLE_INDEX(index, encoder_coder_video_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT:
		TAKE_FIRST_AVAILABLE_INDEX(index, encoder_transporter_audio_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT:
		TAKE_FIRST_AVAILABLE_INDEX(index, encoder_transporter_video_indexes);
		break;
	default:
		pr_err("Error: %s invalid type %d\n", __func__, type);
		break;
	}
	return index;
}

void st_relayfs_freeindex_forsource_se(unsigned int source, unsigned int index)
{
	if (source == ST_RELAY_SOURCE_CONTROL_TEST) {
		FREE_INDEX(index, control_data_indexes);
	} else {
		pr_err("Error: %s invalid source %d\n", __func__, source);
	}
}

void st_relayfs_freeindex_fortype_se(unsigned int type, unsigned int index)
{
	switch (type) {
	case ST_RELAY_TYPE_CODED_AUDIO_BUFFER:
		FREE_INDEX(index, coded_audio_indexes);
		break;
	case ST_RELAY_TYPE_CODED_VIDEO_BUFFER:
		FREE_INDEX(index, coded_video_indexes);
		break;
	case ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER0:
		FREE_INDEX(index, decoded_audio_aux_indexes);
		break;
	case ST_RELAY_TYPE_DECODED_AUDIO_BUFFER:
		FREE_INDEX(index, decoded_audio_indexes);
		break;
	case ST_RELAY_TYPE_DECODED_VIDEO_BUFFER:
		FREE_INDEX(index, decoded_video_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT:
		FREE_INDEX(index, encoder_preproc_audio_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT:
		FREE_INDEX(index, encoder_preproc_video_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT:
		FREE_INDEX(index, encoder_coder_audio_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT:
		FREE_INDEX(index, encoder_coder_video_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT:
		FREE_INDEX(index, encoder_transporter_audio_indexes);
		break;
	case ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT:
		FREE_INDEX(index, encoder_transporter_video_indexes);
		break;
	default:
		pr_err("Error: %s invalid type %d\n", __func__, type);
		break;
	}
}

int st_relayfs_open_se(void)
{
	// source-type counts - inits all to 0 => meaning all empty
	relay_source_type_counts_se = kzalloc(sizeof(struct st_relay_source_type_count_s) * NB_ENTRIES_SOURCE_TYPE_SE,
	                                      GFP_KERNEL);
	if (relay_source_type_counts_se == NULL) {
		pr_err("Error: %s failed alloc\n", __func__);
		return -ENOMEM;
	}

	// set up st_relay debugfs top dir and channel
	relay_entries_se = st_relay_core_register(ARRAY_SIZE(relay_entries_init_se), relay_entries_init_se);
	if (IS_ERR(relay_entries_se)) {
		pr_warn("warning: %s st_relay_core not setup\n", __func__); // can happen if debugfs not setup
		relay_entries_se = NULL;
		kfree(relay_source_type_counts_se);
		relay_source_type_counts_se = NULL;
		return 0;  // don't fail
	}
	if (relay_entries_se == NULL) {
		pr_err("Error: %s register failed\n", __func__);
		kfree(relay_source_type_counts_se);
		relay_source_type_counts_se = NULL;
		return -ENOMEM;
	}

	return 0;
}

void st_relayfs_close_se(void)
{
	kfree(relay_source_type_counts_se);
	relay_source_type_counts_se = NULL;

	if (relay_entries_se != NULL) {
		st_relay_core_unregister(ARRAY_SIZE(relay_entries_init_se), relay_entries_se);
		relay_entries_se = NULL;
	}
}

