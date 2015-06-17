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

Source file name : v4lEncodeTestApp.h

V4L2 audio / video encoder test application main file

************************************************************************/

#ifndef V4LENCODETESTAPP_H_
#define V4LENCODETESTAPP_H_

#include "linux/include/linux/dvb/dvb_v4l2_export.h"

#define encode_print(format, args...) \
		({ fprintf(stdout, format, ## args); })
#define encode_error(format, args...) \
		({ fprintf(stderr, format, ## args); })

#define V4L2_ENC_DRIVER_NAME	"Encoder"
#define V4L2_ENC_CARD_NAME "STMicroelectronics"

#define MAX_CONFIG_LINE_SIZE 256

#define PRINT_CHECK 0

#define CHECK_PARAM(field, value, param, element) \
	{ if (strncmp(field, param, strlen(param)) == 0) { \
		if (PRINT_CHECK) \
			encode_print("<%s> <%d>\n", field, value); \
			*element = value; \
	} \
	}
#define CHECK_PARAM1(field, value, param, element) \
	{ if (strncmp(field, param, strlen(param)) == 0) { \
		if (PRINT_CHECK) \
			encode_print("<%s> <%d>\n", field, value); \
			element = value; \
	} \
	}

typedef unsigned int BOOL;
#define TRUE (1 == 1)
#define FALSE (!TRUE)

/*enable this flag to check driver compatibility vs. SDK2 specification*/
/* #define SPEC_CHECK */

#define PROFILE_NB 10
#define CONTROLS_NB 35
#define MAX_OUTPUT_BUFFERS	6	/* 6 output buffers */

#define DEBUG_TEXT_LENGTH 48

#ifdef SPEC_CHECK
#define SPEC_PRINT(msg)	{  encode_print("SPEC_CHECK: <%s>\n", msg); }
#endif

/* V4L2 driver parameters - start*/
typedef struct drv_ctrl_s {
	int fd;
	unsigned int id;
	unsigned char type;
	char name[32];
	unsigned int default_value;
	struct drv_ctrl_s *next;
} drv_ctrl_t;

struct AudEnc_EncodedInfo {
	int out_buffer_nums;
	struct {
		int offset;
		int size;
	} out_buffer[MAX_OUTPUT_BUFFERS];
};

struct AudEnc_Params_t {
	int audio_init;

	/* input stream config */
	int byte_per_sample;

	/* src meta data */
	int src_max_channels;
	struct v4l2_audenc_src_metadata src_metadata;

	/* audio controls */
	int codec;
	int max_channels;

	int bitrate_mode;
	int bitrate;
	int vbr_qfactor;
	int bitrate_cap;

	int sampling_freq;
	int channel_count;
	int channel[V4L2_STM_AUDENC_MAX_CHANNELS];

	int prog_level;
};

typedef enum IntraRefreshType_e {
	HVA_INTRA_REFRESH_DISABLED = 0,
	HVA_ADAPTATIVE_INTRA_REFRESH = 1,
	HVA_CYCLE_INTRA_REFRESH = 2
} IntraRefreshType_t;

typedef enum {
	HVA_YUV420_SEMI_PLANAR = 0,
	HVA_YUV422_RASTER = 1
} SamplingMode_t;

typedef struct HVAEnc_SequenceParams_s {
	enum v4l2_mpeg_video_h264_level LevelIDC;
	enum v4l2_mpeg_video_h264_profile ProfileIDC;
	/* Random Acces Point periodicity */
	unsigned int IntraPeriod;
	/* Intra-coded picture periodicity */
	unsigned int GOPSize;
	/* all Intra pictures are random acess points (SPS+PPS+IDR) */
	BOOL IDRIntraEnable;
	unsigned int FrameRate_Num;
	unsigned int FrameRate_Den;
	unsigned int TargetFrameRate_Num;
	unsigned int TargetFrameRate_Den;
	/* frame width in pixel, should be a multiple of 16.
	 * It shouldn't change within one sequence.
	 * (if it changes, a new sequence should be triggered) */
	unsigned short width;
	/* frame width in pixel, should be a multiple of 16 */
	unsigned short height;
	enum v4l2_mpeg_video_bitrate_mode BitRateControlType;
	SamplingMode_t SamplingMode;
	enum v4l2_mpeg_video_multi_slice_mode SliceMode;
	/* The maximum size of a slice in bytes */
	unsigned int ByteSliceSize;
	/* The maximum number of macroblocks in a slice */
	unsigned int MbSliceSize;
	IntraRefreshType_t IntraRefreshType;
	/* adaptative intra refresh MB number
	* (HVA_ADAPTATIVE_INTRA_REFRESH only) */
	unsigned short AirMbNum;
	/* target bitrate for BCR  */
	unsigned int BitRate;
	/* target cpb size (constrained by level selected) */
	unsigned int CpbBufferSize;
	/* delay for CBR. */
	unsigned int CbrDelay;
	/* initial value for VBR */
	unsigned int VBRInitialQuant;
	unsigned short qpmin;
	unsigned short qpmax;
	unsigned int V4L2SamplingMode;

	unsigned int field;
	unsigned int colorspace;
	unsigned int AspectRatio;
	/*** H264 Encode Additional Parameters options ***/
	/* to enable output of SEI messages */
	BOOL AddSEIMessages;
	BOOL AddHrdParams;
	BOOL AddVUIParams;
	/* TEMP? */
	/* HVAEnc_VUIParams_t  VUIParams; */
} HVAEnc_SequenceParams_t;

typedef struct drv_context_s {
	int fd_r;
	int fd_w;
	int io_index;

	unsigned int tunneling_mode;
	unsigned int global_exit;
	unsigned int loop; /* Loop Requests on InputData */

	drv_ctrl_t *DrvControls;

	int input_buffer_size;

	struct v4l2_audenc_src_metadata src_meta;
	struct v4l2_audenc_dst_metadata out_meta;

	void *input_buffer_p;
	void *output_buffer_p;
	int input_buffer_p_offset;
	int input_buffer_p_length;
	int output_buffer_p_offset;
	int output_buffer_p_length;

	struct AudEnc_EncodedInfo output_buffers;
	/* Current profile */
	int index_profile;
	/* Enable HVA or Audio encode with dynamic controls */
	BOOL Profile_Encode;
	/* nb Frame to switch of Profile */
	unsigned int Profile_Frames;
	/* nb of Define Profile */
	unsigned int Profile_nb;

	struct AudEnc_Params_t Params;
	struct HVAEnc_SequenceParams_s SequenceParams[PROFILE_NB];

	char *input_file_name;
	char *output_file_name;

	FILE *OutputFp;
	FILE *InputFp;

	int validation;

	pthread_t writer_thread;
	pthread_t reader_thread;
} drv_context_t;

#define HVA_VERSION 1		/* cut 1 */

#endif /* HVATESTAPP_H_ */
