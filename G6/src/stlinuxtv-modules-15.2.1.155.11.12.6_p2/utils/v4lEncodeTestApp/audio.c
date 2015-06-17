/************************************************************************
 * Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV utilities

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV utilities may alternatively be licensed under a proprietary
license from ST.

Source file name : audio.c

Audio encoder related functions

************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/mman.h>

#include "audio.h"
#include "common.h"

#include <pthread.h>
#include <signal.h>

#include "utils/v4l2_helper.h"

#define AC3_SAMPLES 1536
#define AAC_SAMPLES 1024
#define MP3_SAMPLES 1152

#ifdef AUDIO_PARAMS_VERBOSE
typedef struct {
	int Value;
	char DescriptionText[DEBUG_TEXT_LENGTH];
} tV4L2Description;

typedef struct {
	int Value;
	char DescriptionText[DEBUG_TEXT_LENGTH];
	int bitrate;
} tV4L2Description2;

static tV4L2Description V4L2_Mpeg_Audio_Pcm_format[] = {
	{0, "PCM_FMT_32_S32LE"},
	{1, "PCM_FMT_32_S32BE"},
	{2, "PCM_FMT_24_S24LE"},
	{3, "PCM_FMT_24_S24BE"},
	{4, "PCM_FMT_16_S16LE"},
	{5, "PCM_FMT_16_S16BE"},
	{6, "PCM_FMT_16_U16BE"},
	{7, "PCM_FMT_16_U16LE"},
	{8, "PCM_FMT_8_U8"},
	{9, "PCM_FMT_8_S8"},
	{10, "PCM_FMT_8_ALAW_8"},
	{11, "PCM_FMT_8_ULAW_8"},
};

static tV4L2Description V4L2_Mpeg_Codec[] = {
	{0, "PCM"},
	{1, "LAYER_1"},
	{2, "LAYER_2"},
	{3, "LAYER_3"},
	{4, "AC3"},
	{5, "AAC"},
	{6, "HEAAC"},
	{0, ""}
};

static tV4L2Description V4L2_MPEG_Sampling_Freq[] = {
	{0, "4000"},
	{1, "5000"},
	{2, "6000"},
	{3, "8000"},
	{4, "11000"},
	{5, "12000"},
	{6, "16000"},
	{7, "22000"},
	{8, "24000"},
	{9, "32000"},
	{10, "44100"},
	{11, "48000"},
	{12, "64000"},
	{13, "88000"},
	{14, "96000"},
	{15, "128000"},
	{16, "48000"},
	{17, "176000"},
	{18, "192000"},
	{19, "256000"},
	{20, "352000"},
	{21, "384000"},
	{22, "512000"},
	{23, "705000"},
	{23, "768000"},
	{0, ""}
};

static tV4L2Description V4L2_Bitrate_Mode[] = {
	{0, "CBR"},
	{1, "VBR"},
	{0, ""}
};

static tV4L2Description2 V4L2_MPEG_Bitrate_Freq[] = {
	{0, "32K", 32000},
	{1, "40K", 40000},
	{2, "48K", 48000},
	{3, "56K", 56000},
	{4, "64K", 64000},
	{5, "80K", 80000},
	{6, "96K", 96000},
	{7, "112K", 112000},
	{8, "128K", 128000},
	{9, "160K", 160000},
	{10, "192K", 192000},
	{11, "224K", 224000},
	{12, "256K", 256000},
	{13, "320K", 320000},
	{14, "384K", 384000},
	{15, "448K", 448000},
	{16, "512K", 512000},
	{17, "576K", 576000},
	{18, "640K", 640000},
	{0, ""}
};

#endif

/*****************************************************************************
 * Helper function to initalise video IO using the mmap method
 *****************************************************************************/
static int InitMmapAudio_write(drv_context_t *Context)
{
	int ret;

	ret = alloc_v4l2_buffers(Context, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	if (ret)
		return ret;

	/* set default src meta data */
	memset(&Context->src_meta, 0, sizeof(Context->src_meta));
	Context->src_meta.sample_rate =
	    Context->Params.src_metadata.sample_rate;
	Context->src_meta.sample_format =
	    Context->Params.src_metadata.sample_format;
	Context->src_meta.program_level =
	    Context->Params.src_metadata.program_level;
	Context->src_meta.emphasis = Context->Params.src_metadata.emphasis;
	Context->src_meta.channel_count =
	    Context->Params.src_metadata.channel_count;
	Context->src_meta.channel[0] = Context->Params.src_metadata.channel[0];
	Context->src_meta.channel[1] = Context->Params.src_metadata.channel[1];
	Context->src_meta.channel[2] = Context->Params.src_metadata.channel[2];
	Context->src_meta.channel[3] = Context->Params.src_metadata.channel[3];
	Context->src_meta.channel[4] = Context->Params.src_metadata.channel[4];
	Context->src_meta.channel[5] = Context->Params.src_metadata.channel[5];
	Context->src_meta.channel[6] = Context->Params.src_metadata.channel[6];
	Context->src_meta.channel[7] = Context->Params.src_metadata.channel[7];

	ret = stream_on(Context, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	if (ret)
		return ret;

	encode_print("stream_on_done OUTPUT\n");

	return 0;
}

/*****************************************************************************
 * Helper function to initalise video IO using the mmap method
 *****************************************************************************/
static int InitMmapAudio_read(drv_context_t *Context)
{
	int ret;

	ret = alloc_v4l2_buffers(Context, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (ret)
		return ret;

	memset(&Context->out_meta, 0, sizeof(Context->out_meta));

	ret = stream_on(Context, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (ret)
		return ret;

	encode_print("stream_on_done CAPTURE\n");
	return 0;
}

/*  Lists all v4l2 controls, sets them to the user specified value and create \
 * the relevant variables to enable run-time changes.*/
static int ControlsInitAudio(drv_context_t *ContextPt)
{
	struct v4l2_ext_control ext_ctrl[CONTROLS_NB];
	struct v4l2_ext_controls ext_ctrls;
	int index = 0;
	int i;

	/* manually initialize the control list */
	/* all the controls to set are extended controls from the MPEG class */
	ext_ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
	ext_ctrls.count = 0;
	ext_ctrls.controls = ext_ctrl;

	/* core_format */
	/* sampling freq */
	ext_ctrl[index].id = V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ;
	ext_ctrl[index].value = ContextPt->Params.sampling_freq;
	index++;

	/* channel count */
	ext_ctrl[index].id = V4L2_CID_MPEG_STM_AUDIO_CHANNEL_COUNT;
	ext_ctrl[index].value = ContextPt->Params.channel_count;
	index++;

	/* channel 0..31 */
	for (i = 0; i < ContextPt->Params.channel_count; i++) {
		ext_ctrl[index].id = V4L2_CID_MPEG_STM_AUDIO_CHANNEL_MAP;
		ext_ctrl[index].value = (i << 8) | ContextPt->Params.channel[i];
		index++;
	}

	/* apply core_format */
	ext_ctrl[index].id = V4L2_CID_MPEG_STM_AUDIO_CORE_FORMAT;
	ext_ctrl[index].value = 1;
	index++;

	ext_ctrls.count = index;

	if (ioctl(ContextPt->fd_r, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0) {
		encode_error("Error in extended controls not Supported\n");
		return -1;
	}

	/* bitrate control */
	index = 0;

	ext_ctrl[index].id = V4L2_CID_MPEG_STM_AUDIO_BITRATE_MODE;
	ext_ctrl[index].value = ContextPt->Params.bitrate_mode;	/* CBR, VBR */

	index++;

	if (ContextPt->Params.codec == V4L2_MPEG_AUDIO_STM_ENCODING_AC3)
		ext_ctrl[index].id = V4L2_CID_MPEG_AUDIO_AC3_BITRATE;
	else if (ContextPt->Params.codec == V4L2_MPEG_AUDIO_STM_ENCODING_AAC)
		ext_ctrl[index].id = V4L2_CID_MPEG_AUDIO_AAC_BITRATE;
	else if (ContextPt->Params.codec ==
		 V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_3)
		ext_ctrl[index].id = V4L2_CID_MPEG_AUDIO_L3_BITRATE;
	ext_ctrl[index].value = ContextPt->Params.bitrate;
	index++;

	ext_ctrl[index].id =V4L2_CID_MPEG_STM_AUDIO_VBR_QUALITY_FACTOR;
	ext_ctrl[index].value = ContextPt->Params.vbr_qfactor;	/* 100; */
	index++;

	ext_ctrl[index].id = V4L2_CID_MPEG_STM_AUDIO_BITRATE_CAP;
	ext_ctrl[index].value = ContextPt->Params.bitrate_cap;;	/* 0 */
	index++;

	ext_ctrl[index].id = V4L2_CID_MPEG_STM_AUDIO_BITRATE_CONTROL;
	ext_ctrl[index].value = 1;
	index++;

	ext_ctrls.count = index;

	if (ioctl(ContextPt->fd_r, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0) {
		encode_error("Error in extended controls not Supported\n");
		return -1;
	}

	/* other control */
	index = 0;

	ext_ctrl[index].id = V4L2_CID_MPEG_STM_AUDIO_PROGRAM_LEVEL;
	ext_ctrl[index].value = ContextPt->Params.prog_level;
	index++;

	ext_ctrl[index].id = V4L2_CID_MPEG_STM_AUDIO_SERIAL_CONTROL;
	ext_ctrl[index].value = V4L2_MPEG_AUDIO_NO_COPYRIGHT;
	index++;

	ext_ctrls.count = index;

	if (ioctl(ContextPt->fd_r, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0) {
		encode_error("Error in extended controls not Supported\n");
		return -1;
	}

	encode_print("Setting CONTROLS for Preproc\n");

	return 0;
}

static void *Audio_write(void *ContextPt)
{
	drv_context_t *Context = (drv_context_t *) ContextPt;
	struct v4l2_buffer buf;
	BOOL eof = FALSE;

	while (Context->global_exit != 1) {
		if (fread
		    (Context->input_buffer_p, 1, Context->input_buffer_size,
		     Context->InputFp) != Context->input_buffer_size) {
			encode_print("\nReached end of file\n");
			eof = TRUE;
		}

		if (eof && (Context->loop > 1)) /* We have already completed one loop */
		{
		    /* Reached end of file with a request to loop
		     * Seek back to the beginning of the file and continue
		     */

		    if(fseek(Context->InputFp, 0L, SEEK_SET) == 0)
		    {
			/* Successful rewind */
			Context->loop--;
			eof = FALSE;
			encode_print("Completed loop: %d loops remaining\n", Context->loop);
			continue; /* Reloop to perform a new read operation */
		    }
		    else
			perror("Failed to rewind stream. Can't complete loop request - Finishing Stream\n");
		}

		/* Queue a buffer : write */
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.m.offset = Context->input_buffer_p_offset;
		buf.field = 0;
		buf.length = Context->input_buffer_p_length;
		buf.bytesused = Context->input_buffer_size;
		buf.reserved = (int)&Context->src_meta;

		/*
		 * If end of file is reached, tell the encoder device that
		 * this is going to be the last injection
		 */
		if (eof)
			buf.bytesused = 0;

		if (ioctl(Context->fd_w, VIDIOC_QBUF, &buf) < 0) {
			encode_error("Cannot queue OUTPUT buffer 1\n");
			goto error;
		}

		if (ioctl(Context->fd_w, VIDIOC_DQBUF, &buf) < 0) {
			encode_error("cannot de-queue OUTPUT buffer\n");
			goto error;
		}

		if (eof)
			break;

		encode_print("+");
		fflush(stdout);
	}

	pthread_exit(NULL);
	return NULL;

error:
	encode_error("Failed to properly inject audio\n");
	pthread_exit(NULL);
	return NULL;
}

static void *Audio_read(void *ContextPt)
{
	drv_context_t *Context = (drv_context_t *) ContextPt;
	struct v4l2_buffer bufOut;

	while (Context->global_exit != 1) {
		/* Queue a buffer : read */
		memset(&bufOut, 0, sizeof(struct v4l2_buffer));
		bufOut.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		bufOut.memory = V4L2_MEMORY_MMAP;
		bufOut.m.offset = Context->output_buffer_p_offset;
		bufOut.field = 0;
		bufOut.length = Context->output_buffer_p_length;
		bufOut.reserved = (unsigned int)&Context->out_meta;

		if (ioctl(Context->fd_r, VIDIOC_QBUF, &bufOut) < 0) {
			encode_error("cannot queue CAPTURE buffer\n");
			goto error;
		}

		if (ioctl(Context->fd_r, VIDIOC_DQBUF, &bufOut) < 0) {
			encode_error("cannot de-queue CAPTURE buffer\n");
			goto error;
		}

		/*
		 * Last packet received, so, go out now
		 */
		if (!bufOut.bytesused)
			break;

		encode_print(".");
		fflush(stdout);
		if (fwrite
		    (Context->output_buffer_p, 1, bufOut.bytesused,
		     Context->OutputFp) != bufOut.bytesused) {
			encode_error("Failed to write everything\n");
			goto error;
		}
		memset(&Context->out_meta, 0, sizeof(Context->out_meta));

	}

	pthread_exit(NULL);
	return NULL;

error:
	encode_error("Failed to properly retrieve encoded audio\n");
	pthread_exit(NULL);
	return NULL;
}

static int InitAudio_write(drv_context_t *Context)
{
	struct v4l2_format format;
	struct v4l2_audenc_format *aud_fmt;

	encode_error("InitAudio_write\n");

	/* Select audio output */
	if (ioctl(Context->fd_w, VIDIOC_S_AUDOUT, &Context->io_index) < 0) {
		encode_error("Cannot set encoder context %u\n",
			     Context->io_index);
		return -1;
	}

	/* Set out format */
	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	aud_fmt = (struct v4l2_audenc_format *)format.fmt.raw_data;
	aud_fmt->codec = V4L2_MPEG_AUDIO_STM_ENCODING_PCM;
	aud_fmt->max_channels = Context->Params.src_max_channels;
	if (ioctl(Context->fd_w, VIDIOC_S_FMT, &format) < 0) {
		encode_error("Cannot set requested intput format\n");
		return -1;
	}

	/* Init I/O method */
	if (InitMmapAudio_write(Context)) {
		encode_error("Error in InitMmap\n");
		return -1;
	}
	return 0;
}

static int InitAudio_read(drv_context_t *Context)
{
	int nfmts = 0;
	int i;
	struct v4l2_fmtdesc *formats = NULL;
	struct v4l2_fmtdesc fmt = {.index = 0,
				   .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, };
	struct v4l2_capability cap;
	struct v4l2_format format;

	struct v4l2_audenc_format *aud_fmt;

	encode_error("InitAudio_read\n");

	/* Get device capabilites */
	if (ioctl(Context->fd_r, VIDIOC_QUERYCAP, &cap) < 0) {
		encode_error("cannot get video capabilities of V4L2 device");
		return -1;
	}
#ifdef SPEC_CHECK
	if (!cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
		SPEC_PRINT("V4L2_CAP_VIDEO_CAPTURE should be supported");
		return -1;
	} else {
		SPEC_PRINT("V4L2_CAP_VIDEO_CAPTURE supported!");
	}
	if ((!cap.capabilities) & V4L2_CAP_VIDEO_OUTPUT) {
		SPEC_PRINT("V4L2_CAP_VIDEO_OUTPUT should be supported");
		return -1;
	} else {
		SPEC_PRINT("V4L2_CAP_VIDEO_OUTPUT supported!");
	}
	if ((!cap.capabilities) & V4L2_CAP_STREAMING) {
		SPEC_PRINT("V4L2_CAP_STREAMING should be supported");
		return -1;
	} else {
		SPEC_PRINT("V4L2_CAP_STREAMING supported!");
	}
#endif /* SPEC_CHECK */

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		encode_error("no supported I/O method\n");
		return -1;
	}

	/* Select Audio input */
	encode_print("Using encoder aud-encoder-%02d\n", Context->io_index);
	if (ioctl(Context->fd_r, VIDIOC_S_AUDIO, &Context->io_index) < 0) {
		encode_error("Cannot set input %u\n", Context->io_index);
		return -1;
	}

	/* Enumerate all the supported formats for
	 * V4L2_BUF_TYPE_VIDEO_CAPTURE data stream type */
	while (ioctl(Context->fd_r, VIDIOC_ENUM_FMT, &fmt) >= 0)
		fmt.index = ++nfmts;

	formats = malloc(nfmts * sizeof(*formats));
	if (!formats) {
		encode_error("Failed to allocate memory for V4L2 formats\n");
		return -1;
	}

	encode_print("Encoded Audio Format Supported ");
	for (i = 0; i < nfmts; i++) {
		formats[i].index = i;
		formats[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(Context->fd_r, VIDIOC_ENUM_FMT, &formats[i]) < 0) {
			encode_error
			    ("Cannot get format description for index %u\n",
			     formats[i].index);
			free(formats);
			return -1;
		}
		encode_print("%s ", formats[i].description);
	}
	encode_print("\n");

	/* Check whether the desired format is available */
	i = 0;
	while ((i < nfmts) &&
	       (formats[i].pixelformat != Context->Params.codec))
		i++;

	free(formats);
	if (i == nfmts) {
		encode_error("Desired format for output is not supported\n");
		return -1;
	}

	encode_print("Encoded Audio Format Selected  %s\n",
		     formats[i].description);

	/* Set input format */
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	aud_fmt = (struct v4l2_audenc_format *)format.fmt.raw_data;
	aud_fmt->codec = Context->Params.codec;
	aud_fmt->max_channels = Context->Params.max_channels;
	if (ioctl(Context->fd_r, VIDIOC_S_FMT, &format) < 0) {
		encode_error("Cannot set requested input format\n");
		return -1;
	}

	/* Initialize the controls */
	if (ControlsInitAudio(Context)) {
		encode_error("Error in ControlsInit\n");
		return -1;
	}

	/* Init I/O method */
	if (InitMmapAudio_read(Context)) {
		encode_error("Error in InitMmap\n");
		return -1;
	}

	return 0;
}

int OpenAudio(drv_context_t *ContextPt)
{
	/* Output Data Encoded */
	ContextPt->fd_r =
	    v4l2_open_by_name(V4L2_ENC_DRIVER_NAME, V4L2_ENC_CARD_NAME, O_RDWR);

	if (ContextPt->fd_r < 0) {
		encode_error
		    ("---->>> can't open encoder Device for read AC3\n");
		close(ContextPt->fd_r);
		return -1;
	}

	/* Input Data PCM */
	if (ContextPt->tunneling_mode == 0) {
		ContextPt->fd_w =
		    v4l2_open_by_name(V4L2_ENC_DRIVER_NAME, V4L2_ENC_CARD_NAME,
				      O_RDWR);
		if (ContextPt->fd_w < 0) {
			encode_error("can't open device to write PCM\n");
			close(ContextPt->fd_w);
			return -1;
		}
	}

	return 0;
}

static int CloseAudio(drv_context_t *Context)
{
	struct v4l2_requestbuffers req;

	/* deinit I/O method */
	/* Stop streaming I/O */
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(Context->fd_r, VIDIOC_STREAMOFF, &req.type) < 0) {
		encode_error("cannot stop streaming on capture interface\n");
		return -1;
	}
	if (Context->tunneling_mode == 0) {
		req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (ioctl(Context->fd_w, VIDIOC_STREAMOFF, &req.type) < 0) {
			encode_error
			    ("cannot stop streaming on output interface\n");
			return -1;
		}
		munmap(Context->input_buffer_p, Context->input_buffer_p_length);
	}
	munmap(Context->output_buffer_p, Context->output_buffer_p_length);

	close(Context->fd_r);

	if (Context->tunneling_mode == 0)
		close(Context->fd_w);

	encode_print("V4l Audio Encode Passed\n");
	return 0;
}

static void ParseConfigFileAudio(drv_context_t *Context, FILE *ConfigFp)
{
	char line[MAX_CONFIG_LINE_SIZE];
	char *field;
	char *valuestr;
	char *stripped_line;
	int value;
	char channel[256];
	int i;

	struct AudEnc_Params_t *param = &Context->Params;
	struct v4l2_audenc_src_metadata *metadata =
	    &Context->Params.src_metadata;

	memset(param, 0, sizeof(*param));
	while (fgets(line, MAX_CONFIG_LINE_SIZE, ConfigFp) != NULL) {
		stripped_line = strpbrk(line, "\r\n#");
		if (stripped_line != NULL)
			stripped_line[0] = '\0';

		field = strtok(line, "=");
		if (field == NULL)
			continue;

		valuestr = strtok(NULL, "=");
		value = atoi(valuestr);

		CHECK_PARAM(field, value, "audio_encoder_param",
			    &param->audio_init);
		/* input stream config */
		CHECK_PARAM(field, value, "byte_per_sample",
			    &param->byte_per_sample);
		CHECK_PARAM(field, value, "src_max_channels",
			    &param->src_max_channels);

		/* src meta data */
		CHECK_PARAM(field, value, "sample_rate",
			    &metadata->sample_rate);
		CHECK_PARAM(field, value, "sample_format",
			    &metadata->sample_format);
		CHECK_PARAM(field, value, "program_level",
			    &metadata->program_level);
		CHECK_PARAM(field, value, "emphasis", &metadata->emphasis);

		CHECK_PARAM(field, value, "src_channel_count",
			    &metadata->channel_count);
		/* channel 0..31 */
		for (i = 0; i < V4L2_STM_AUDENC_MAX_CHANNELS; i++) {
			sprintf(channel, "src_channel[%d]", i);
			CHECK_PARAM(field, value, channel,
				    &metadata->channel[i]);
		}

		/* audio controls */
		CHECK_PARAM(field, value, "codec", &param->codec);
		CHECK_PARAM(field, value, "max_channels", &param->max_channels);
		CHECK_PARAM(field, value, "bitrate_mode", &param->bitrate_mode);
		CHECK_PARAM(field, value, "bit_rate", &param->bitrate);
		CHECK_PARAM(field, value, "vbr_qfactor", &param->vbr_qfactor);
		CHECK_PARAM(field, value, "bitrate_cap", &param->bitrate_cap);

		CHECK_PARAM(field, value, "sampling_freq",
			    &param->sampling_freq);
		CHECK_PARAM(field, value, "channel_count",
			    &param->channel_count);

		/* channel 0..31 */
		for (i = 0; i < V4L2_STM_AUDENC_MAX_CHANNELS; i++) {
			sprintf(channel, "channel[%d]", i);
			CHECK_PARAM(field, value, channel, &param->channel[i]);
		}
		CHECK_PARAM(field, value, "prog_level", &param->prog_level);

	}
}

int parse_audio_ini(drv_context_t *ContextPt, char *filename)
{
	drv_context_t *Context = ContextPt;
	FILE *fd;
	int index;
	char channel[256];
	int i;

	fd = fopen(filename, "rb");
	if (fd == NULL) {
		encode_error("can't open config file\n");
		return -1;
	}

	/* updated from config file */
	ParseConfigFileAudio(Context, fd);

	fclose(fd);

#ifdef AUDIO_PARAMS_VERBOSE
	if (Context->Params.audio_init != 1)
		return 0;

	/* Apply audio settings, if applicable */
	encode_print("%s -> %d\n", "byte_per_sample  ",
		     Context->Params.byte_per_sample);

	encode_print("%s -> %d\n", "src_max_channels ",
		     Context->Params.src_max_channels);

	encode_print("%s -> %d\n", "sample_rate      ",
		     Context->Params.src_metadata.sample_rate);

	index = Context->Params.src_metadata.sample_format;
	encode_print("%s -> %s\n", "sample_format    ",
		     V4L2_Mpeg_Audio_Pcm_format[index].DescriptionText);

	encode_print("%s -> %d\n", "program_level    ",
		     Context->Params.src_metadata.program_level);
	encode_print("%s -> %d\n", "emphasis         ",
		     Context->Params.src_metadata.emphasis);

	encode_print("%s -> %d\n", "src_channel_count",
		     Context->Params.src_metadata.channel_count);
	for (i = 0; i < Context->Params.src_metadata.channel_count; i++) {
		sprintf(channel, "src_channel[%d]", i);
		encode_print("%s        -> %d\n", channel,
			     Context->Params.src_metadata.channel[i]);
	}

	index = Context->Params.codec;
	encode_print("%s -> %s\n", "codec            ",
		     V4L2_Mpeg_Codec[index].DescriptionText);

	encode_print("%s -> %d\n", "max_channels     ",
		     Context->Params.max_channels);

	index = Context->Params.bitrate_mode;
	encode_print("%s -> %s\n", "bitrate_mode     ",
		     V4L2_Bitrate_Mode[index].DescriptionText);

	if (Context->Params.codec == V4L2_MPEG_AUDIO_STM_ENCODING_AC3) {
		index = Context->Params.bitrate;
		encode_print("%s -> %s\n", "bitrate          ",
			     V4L2_MPEG_Bitrate_Freq[index].DescriptionText);
	} else if (Context->Params.codec == V4L2_MPEG_AUDIO_STM_ENCODING_AAC) {
		encode_print("%s -> %d\n", "bitrate          ",
			     Context->Params.bitrate);
	} else if (Context->Params.codec ==
		   V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_3) {
		index = Context->Params.bitrate;
		encode_print("%s -> %s\n", "bitrate          ",
			     V4L2_MPEG_Bitrate_Freq[index].DescriptionText);
	}

	encode_print("%s -> %d\n", "vbr_qfactor      ",
		     Context->Params.vbr_qfactor);

	encode_print("%s -> %d\n", "bitrate_cap      ",
		     Context->Params.bitrate_cap);

	index = Context->Params.sampling_freq;
	encode_print("%s -> %s\n", "sampling_freq    ",
		     V4L2_MPEG_Sampling_Freq[index].DescriptionText);

	encode_print("%s -> %d\n", "channel_count    ",
		     Context->Params.channel_count);

	for (i = 0; i < Context->Params.channel_count; i++) {
		sprintf(channel, "channel[%d]", i);
		encode_print("%s        -> %d\n", channel,
			     Context->Params.channel[i]);
	}

	encode_print("%s -> %d\n", "prog_level       ",
		     Context->Params.prog_level);

#endif

	return 0;
}

int encode_audio(drv_context_t *Context)
{
	if (OpenAudio(Context)) {
		CloseAudio(Context);
		encode_error("Error opening Audio\n");
		return 1;
	}

	switch (Context->Params.codec) {
	case V4L2_MPEG_AUDIO_STM_ENCODING_AC3:
		/*  AC3_IN_BUFFER_SIZE; */
		Context->input_buffer_size = AC3_SAMPLES *
				Context->Params.src_metadata.channel_count *
				Context->Params.byte_per_sample;
		break;
	case V4L2_MPEG_AUDIO_STM_ENCODING_AAC:
		/*  AAC_IN_BUFFER_SIZE; */
		Context->input_buffer_size = AAC_SAMPLES *
				Context->Params.src_metadata.channel_count *
				Context->Params.byte_per_sample;
		break;
	case V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_3:
		/*  MP3_IN_BUFFER_SIZE; */
		Context->input_buffer_size = MP3_SAMPLES *
				Context->Params.src_metadata.channel_count *
				Context->Params.byte_per_sample;
		break;
	}

	memset(&Context->output_buffers, 0, sizeof(Context->output_buffers));

	InitAudio_read(Context);

	/* Thread write data to Encoded File */
	pthread_create(&Context->reader_thread, NULL, Audio_read, Context);

	if (Context->tunneling_mode == 0) {
		InitAudio_write(Context);
		/* Thread read data from File */
		pthread_create(&Context->writer_thread, NULL, Audio_write,
			       Context);
	}

	if (Context->tunneling_mode == 0)
		pthread_join(Context->writer_thread, NULL);

	pthread_join(Context->reader_thread, NULL);

	if (CloseAudio(Context)) {
		encode_error("Error closing Audio\n");
		return 1;
	}

	if (Context->tunneling_mode == 0)
		fclose(Context->InputFp);

	return 0;
}
