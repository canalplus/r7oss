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

Source file name : video.c

Video encoder related functions

************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/mman.h>

#include "video.h"
#include "common.h"

#include <pthread.h>

#include "crc.h"

#include "utils/v4l2_helper.h"

/* check timestamp */
/* #define PTS_CHECK */
/* check time discontinuity */
/* #define TD_CHECK */
/* #define GOP_START_CHECK */
/* #define FRAME_TYPE_CHECK */

/* HVA encoding parameters (set by user) - start*/
typedef enum H264ProfileIDC_e {
	PROFILE_BASELINE = 66,
	PROFILE_MAIN = 77,
	PROFILE_EXTENDED = 88,
	PROFILE_HIGH = 100,
	PROFILE_HIGH10 = 110,
	PROFILE_MULTIVIEW_HIGH = 118,
	PROFILE_HIGH422 = 122,
	PROFILE_STEREO_HIGH = 128,
	PROFILE_HIGH444 = 144
} H264ProfileIDC_t;

enum {
	HVAENC_NO_ERROR = 0,
	HVAENC_ERROR
};

typedef enum BRCType_e {
	HVA_NO_BRC = 0,		/* fixed Qp */
	HVA_CBR = 1,
	HVA_VBR = 2
} BRCType_t;

/* Auto initialized to NULL */
static ControlNode_t *ControlsHead;
static ControlNode_t *ControlsEnd;

/*****************************************************************************
 * Helper function to initialize video IO using the mmap method
 *****************************************************************************/
static int InitMmapVideo_write(drv_context_t *ContextPt)
{
	int ret;

	ret = alloc_v4l2_buffers(ContextPt, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	if (ret)
		return ret;

	return 0;
}

/*****************************************************************************
 * Helper function to initialize video IO using the mmap method
 *****************************************************************************/
static int InitMmapVideo_read(drv_context_t *ContextPt)
{
	int ret;

	ret = alloc_v4l2_buffers(ContextPt, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (ret)
		return ret;

	return 0;
}

/*  Lists all v4l2 controls, sets them to the user specified value and create \
 * the relevant variables to enable run-time changes.*/
static int ControlsInitVideo(drv_context_t *ContextPt)
{
	struct v4l2_ext_control ext_ctrl[CONTROLS_NB];
	struct v4l2_ext_controls ext_ctrls;
	struct v4l2_control ctrl_p;
	ControlNode_t *ControlsPtr;
	int index, value;
	int count = 0;

	static const unsigned int lookup_203[] = {
		/*V4L2_CID_MPEG_VIDEO_GOP_SIZE - 203 */
		offsetof(HVAEnc_SequenceParams_t, IntraPeriod),
		0x0,
		0x0,
		/*V4L2_CID_MPEG_VIDEO_BITRATE_MODE - 206 */
		offsetof(HVAEnc_SequenceParams_t, BitRateControlType),
		/*V4L2_CID_MPEG_VIDEO_BITRATE - 207 */
		offsetof(HVAEnc_SequenceParams_t, BitRate),
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		/*V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES - 219 */
		offsetof(HVAEnc_SequenceParams_t, ByteSliceSize),
		/*V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB - 220 */
		offsetof(HVAEnc_SequenceParams_t, MbSliceSize),
		/*V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE - 221 */
		offsetof(HVAEnc_SequenceParams_t, SliceMode)
	};

	static const unsigned int lookup_350[] = {
		/*V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP - 350 */
		0x0,
		/*V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP - 351 */
		0x0,
		/*V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP - 352 */
		0x0,
		/*V4L2_CID_MPEG_VIDEO_H264_MIN_QP - 353 */
		0x0,
		/*V4L2_CID_MPEG_VIDEO_H264_MAX_QP - 354 */
		0x0,
		/*V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM - 355 */
		0x0,
		/*V4L2_CID_MPEG_VIDEO_H264_CPB_SIZE - 356 */
		offsetof(HVAEnc_SequenceParams_t, CpbBufferSize),
		/*V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE - 357 */
		0x0,
		/*V4L2_CID_MPEG_VIDEO_H264_I_PERIOD - 358 */
		0x0,
		/*V4L2_CID_MPEG_VIDEO_H264_LEVEL - 359 */
		offsetof(HVAEnc_SequenceParams_t, LevelIDC),
		/*V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA - 360 */
		0x0,
		/*V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA - 361 */
		0x0,
		/*V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE - 362 */
		0x0,
		/*V4L2_CID_MPEG_VIDEO_H264_PROFILE - 363 */
		offsetof(HVAEnc_SequenceParams_t, ProfileIDC)
	};

	struct v4l2_queryctrl query;
	query.id = V4L2_CID_MPEG_BASE + 203 /*V4L2_CID_BASE */ ;
	encode_print("index_profile = %d\n", ContextPt->index_profile);

	while (query.id < (V4L2_CID_MPEG_BASE + 400)) {

		/* young */
		if (query.id == V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE ||
		    query.id == V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB ||
		    query.id == V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES) {
			query.id++;
			continue;
		}

		if (ioctl(ContextPt->fd_r, VIDIOC_QUERYCTRL, &query) >= 0) {

			if ((query.id - V4L2_CID_MPEG_BASE) >= 350) {
				index = query.id - V4L2_CID_MPEG_BASE - 350;
				value =
				    *(((int *)&ContextPt->
				       SequenceParams[ContextPt->
						      index_profile] +
				       lookup_350[index] / 4));
			} else {
				index = query.id - V4L2_CID_MPEG_BASE - 203;
				value =
				    *(((int *)&ContextPt->
				       SequenceParams[ContextPt->
						      index_profile] +
				       lookup_203[index] / 4));
			}
			ControlsPtr = InitControlNode("NA", query.id, value);
			AddControlNode(&ControlsHead, &ControlsEnd,
				       ControlsPtr);
			encode_print
			    ("Added control \"%s\"; initial value = %d\n",
			     query.name, value);
		}

		/*query.id |= V4L2_CTRL_FLAG_NEXT_CTRL; NOT SUPPORTED YET */
		query.id++;
	}

	/* Parse the controls linked list */
	ControlsPtr = ControlsHead;
	while (ControlsPtr != NULL) {
		ext_ctrl[count].id = ControlsPtr->ControlID;
		ext_ctrl[count].size = 0;
		ext_ctrl[count].reserved2[0] = 0;
		ext_ctrl[count].value = ControlsPtr->ControlValue;
		count++;
		ControlsPtr = ControlsPtr->NextControl;
	}
	ext_ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
	ext_ctrls.count = count;
	ext_ctrls.reserved[0] = 0;
	ext_ctrls.reserved[1] = 0;
	ext_ctrls.controls = ext_ctrl;

	if (ioctl(ContextPt->fd_r, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0) {
		encode_error("Error in extended controls not Supported\n");
		return -1;
	}

	encode_print("Setting CONTROLS for Preproc\n");

	ctrl_p.id = V4L2_CID_MPEG_STM_VIDEO_H264_DEI_SET_MODE;
	ctrl_p.value = 1;

	if (ioctl(ContextPt->fd_r, VIDIOC_S_CTRL, &ctrl_p) < 0) {
		encode_error("error in setting controls\n");
		return -1;
	}

	ctrl_p.id = V4L2_CID_MPEG_STM_VIDEO_H264_NOISE_FILTERING;
	ctrl_p.value = 0;

	if (ioctl(ContextPt->fd_r, VIDIOC_S_CTRL, &ctrl_p) < 0) {
		encode_error("error in setting controls\n");
		return -1;
	}

	ctrl_p.id = V4L2_CID_MPEG_STM_VIDEO_H264_ASPECT_RATIO;
	ctrl_p.value =
	    ContextPt->SequenceParams[ContextPt->index_profile].AspectRatio;

	if (ioctl(ContextPt->fd_r, VIDIOC_S_CTRL, &ctrl_p) < 0) {
		encode_error("error in setting controls\n");
		return -1;
	}

	return 0;
}

static void *Video_write(void *Context)
{
	drv_context_t *ContextPt = (drv_context_t *) Context;
	struct v4l2_buffer buf;
	/* Auto initialized to 0 */
	static long int PTS_sec;
	static long int PTS_usec = 0x100;
	BOOL eof = FALSE;
	/* Auto initialized to 0 */
	static int frame_counter;

	while (ContextPt->global_exit != 1) {
		if (fread
		    (ContextPt->input_buffer_p, 1, ContextPt->input_buffer_size,
		     ContextPt->InputFp) != ContextPt->input_buffer_size) {
			encode_print("\nReached end of file\n");
			eof = TRUE;
		}

		if (eof && (ContextPt->loop > 1)) /* We have already completed one loop */
		{
		    /* Reached end of file with a request to loop
		     * Seek back to the beginning of the file and continue
		     */

		    if(fseek(ContextPt->InputFp, 0L, SEEK_SET) == 0)
		    {
			/* Successful rewind */
			ContextPt->loop--;
			eof = FALSE;
			encode_print("Completed loop: %d loops remaining\n", ContextPt->loop);
			continue; /* Reloop to perform a new read operation */
		    }
		    else
			perror("Failed to rewind stream. Can't complete loop request - Finishing Stream\n");
		}

		/* Refill these parameters as 'buf' is not retaining the
		 * 'timestamp' values after the above VIDIOC_QUERYBUF call */
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = 0;
		buf.timestamp.tv_sec = PTS_sec;
		buf.timestamp.tv_usec = PTS_usec;

		buf.m.offset = ContextPt->input_buffer_p_offset;
		buf.field = 0;
		buf.length = ContextPt->input_buffer_p_length;
		buf.bytesused = ContextPt->input_buffer_size;
		buf.reserved = (int)&ContextPt->src_meta;

		/*
		 * If end of file is reached, the same needs to be communicated
		 * to encoder device, so that the encoded output is
		 * read without block */
		if (eof)
			buf.bytesused = 0;

		if (ioctl(ContextPt->fd_w, VIDIOC_QBUF, &buf) < 0) {
			encode_error("cannot queue OUTPUT buffer\n");
			goto error;
		}

		if (ioctl(ContextPt->fd_w, VIDIOC_DQBUF, &buf) < 0) {
			encode_error("cannot de-queue OUTPUT buffer\n");
			goto error;
		}

		/*
		 * Injected the last data, now, go out
		 */
		if (eof)
			break;
		if (ContextPt->validation == 0) {
			encode_print("+");
			fflush(stdout);
		}

		PTS_sec++;
		PTS_usec++;
		frame_counter++;
	}

	pthread_exit(NULL);
	return NULL;

error:
	encode_error("Failed to properly inject video\n");
	pthread_exit(NULL);
	return NULL;
}

static int Update_VIDIOC_S_EXT_CTRLS(drv_context_t *ContextPt)
{
	struct v4l2_ext_control ext_ctrl[CONTROLS_NB];
	struct v4l2_ext_controls ext_ctrls;
	int count = 0;
	struct v4l2_control ctrl_p;

	count = 0;
	/* Update BitRateControlType with value in new Profile */
	ext_ctrl[count].id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE;
	ext_ctrl[count].size = 0;
	ext_ctrl[count].reserved2[0] = 0;
	ext_ctrl[count].value =
	    ContextPt->SequenceParams[ContextPt->index_profile].
	    BitRateControlType;
	count++;

	/* Update GOPSize with value in new Profile */
	ext_ctrl[count].id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
	ext_ctrl[count].size = 0;
	ext_ctrl[count].reserved2[0] = 0;
	ext_ctrl[count].value =
	    ContextPt->SequenceParams[ContextPt->index_profile].GOPSize;
	count++;

	/* Update LevelIDC with value in new Profile */
	ext_ctrl[count].id = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
	ext_ctrl[count].size = 0;
	ext_ctrl[count].reserved2[0] = 0;
	ext_ctrl[count].value =
	    ContextPt->SequenceParams[ContextPt->index_profile].LevelIDC;
	count++;

	/* Update ProfileIDC with value in new Profile */
	ext_ctrl[count].id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
	ext_ctrl[count].size = 0;
	ext_ctrl[count].reserved2[0] = 0;
	ext_ctrl[count].value =
	    ContextPt->SequenceParams[ContextPt->index_profile].ProfileIDC;
	count++;

	/* Update BitRate with value in new Profile */
	ext_ctrl[count].id = V4L2_CID_MPEG_VIDEO_BITRATE;
	ext_ctrl[count].size = 0;
	ext_ctrl[count].reserved2[0] = 0;
	ext_ctrl[count].value =
	    ContextPt->SequenceParams[ContextPt->index_profile].BitRate;
	count++;

#if 0
	/* Update SliceMode with value in new Profile */
	ext_ctrl[count].id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE;
	ext_ctrl[count].size = 0;
	ext_ctrl[count].reserved2[0] = 0;
	ext_ctrl[count].value =
	    ContextPt->SequenceParams[ContextPt->index_profile].SliceMode;
	count++;

	/* Update MbSliceSize with value in new Profile */
	ext_ctrl[count].id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB;
	ext_ctrl[count].size = 0;
	ext_ctrl[count].reserved2[0] = 0;
	ext_ctrl[count].value =
	    ContextPt->SequenceParams[ContextPt->index_profile].MbSliceSize;
	count++;

	/* Update ByteSliceSize with value in new Profile */
	ext_ctrl[count].id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES;
	ext_ctrl[count].size = 0;
	ext_ctrl[count].reserved2[0] = 0;
	ext_ctrl[count].value =
	    ContextPt->SequenceParams[ContextPt->index_profile].ByteSliceSize;
	count++;
#endif

	/* Update CpbBufferSize with value in new Profile */
	ext_ctrl[count].id = V4L2_CID_MPEG_VIDEO_H264_CPB_SIZE;
	ext_ctrl[count].size = 0;
	ext_ctrl[count].reserved2[0] = 0;
	ext_ctrl[count].value =
	    ContextPt->SequenceParams[ContextPt->index_profile].CpbBufferSize;
	count++;

	ext_ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
	ext_ctrls.count = count;
	ext_ctrls.reserved[0] = 0;
	ext_ctrls.reserved[1] = 0;
	ext_ctrls.controls = ext_ctrl;

	if (ioctl(ContextPt->fd_r, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0) {
		encode_error("Error in extended controls not Supported\n");
		return -1;
	}

	ctrl_p.id = V4L2_CID_MPEG_STM_VIDEO_H264_DEI_SET_MODE;
	ctrl_p.value = 1;

	if (ioctl(ContextPt->fd_r, VIDIOC_S_CTRL, &ctrl_p) < 0) {
		encode_error("error in setting controls\n");
		return -1;
	}

	ctrl_p.id = V4L2_CID_MPEG_STM_VIDEO_H264_NOISE_FILTERING;
	ctrl_p.value = 0;

	if (ioctl(ContextPt->fd_r, VIDIOC_S_CTRL, &ctrl_p) < 0) {
		encode_error("error in setting controls\n");
		return -1;
	}

	ctrl_p.id = V4L2_CID_MPEG_STM_VIDEO_H264_ASPECT_RATIO;
	ctrl_p.value =
	    ContextPt->SequenceParams[ContextPt->index_profile].AspectRatio;

	if (ioctl(ContextPt->fd_r, VIDIOC_S_CTRL, &ctrl_p) < 0) {
		encode_error("error in setting controls\n");
		return -1;
	}
	return 0;
}

static int CloseVideo(drv_context_t *Context)
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
	close(Context->fd_w);
	return 0;
}

static int Update_Profile(drv_context_t *ContextPt)
{
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;

	int nfmts = 0;
	struct v4l2_fmtdesc *formats = NULL;
	struct v4l2_format format;
	struct v4l2_streamparm param;

	struct v4l2_fmtdesc fmt = {.index = 0,
				   .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, };
	struct v4l2_capability cap;
	int i;

	CloseVideo(ContextPt);

	/* Output Data Encoded */
	ContextPt->fd_r =
	    v4l2_open_by_name(V4L2_ENC_DRIVER_NAME, V4L2_ENC_CARD_NAME, O_RDWR);
	if (ContextPt->fd_r < 0) {
		encode_error
		    ("---->>> can't open encoder Device for read h234\n");
		close(ContextPt->fd_r);
		return -1;
	}
	/* Input Data PCM */
	if (ContextPt->tunneling_mode == 0) {
		ContextPt->fd_w =
		    v4l2_open_by_name(V4L2_ENC_DRIVER_NAME, V4L2_ENC_CARD_NAME,
				      O_RDWR);
		if (ContextPt->fd_w < 0) {
			encode_error
			    (">>> can't open encoder Device for write YUV\n");
			close(ContextPt->fd_w);
			return -1;
		}
	}

	/* Get device capabilites */
	if (ioctl(ContextPt->fd_r, VIDIOC_QUERYCAP, &cap) < 0) {
		encode_error("cannot get video capabilities of V4L2 device");
		return -1;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		encode_error("no supported I/O method\n");
		return -1;
	}

	/* Select Video input */
	if (ioctl(ContextPt->fd_r, VIDIOC_S_INPUT, &ContextPt->io_index) < 0) {
		encode_error("cannot set input %u\n", ContextPt->io_index);
		return -1;
	}

	/* Enumerate all the supported formats for
	 * V4L2_BUF_TYPE_VIDEO_CAPTURE data stream type */
	while (ioctl(ContextPt->fd_r, VIDIOC_ENUM_FMT, &fmt) >= 0)
		fmt.index = ++nfmts;

	formats = malloc(nfmts * sizeof(*formats));
	if (!formats) {
		encode_error("Failed to allocate memory for V4L2 formats\n");
		return -1;
	}
	/* Check whether the desired format is available */
	i = 0;
	while ((i < nfmts) && (formats[i].pixelformat != V4L2_PIX_FMT_H264))
		i++;

	if (i == nfmts) {
		encode_error
		    ("desired format for capture queue is not supported\n");
		return -1;
	}

	/* Set input format */
	free(formats);
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width =
	    ContextPt->SequenceParams[ContextPt->index_profile].width;
	format.fmt.pix.height =
	    ContextPt->SequenceParams[ContextPt->index_profile].height;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
	if (ioctl(ContextPt->fd_r, VIDIOC_S_FMT, &format) < 0) {
		encode_error("cannot set requested input format\n");
		return -1;
	}

	/* Set the input streaming parameters */
	param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	param.parm.capture.timeperframe.numerator =
	    ContextPt->SequenceParams[ContextPt->index_profile].
	    TargetFrameRate_Den;
	param.parm.capture.timeperframe.denominator =
	    ContextPt->SequenceParams[ContextPt->index_profile].
	    TargetFrameRate_Num;
	if (ioctl(ContextPt->fd_r, VIDIOC_S_PARM, &param) < 0) {
		encode_error("cannot set requested input parameters\n");
		return -1;
	}

	/* Initialize the controls */
	Update_VIDIOC_S_EXT_CTRLS(ContextPt);

	/* mmap for CAPTURE interface */
	memset(&req, 0, sizeof(req));
	req.count = 1;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(ContextPt->fd_r, VIDIOC_REQBUFS, &req) < 0) {
		encode_error
		    ("device does not support user mmap i/o for capture\n");
		return -1;
	}

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
	if (ioctl(ContextPt->fd_r, VIDIOC_QUERYBUF, &buf) < 0) {
		encode_error("VIDIOC_QUERYBUF for capture interface failed!\n");
		return -1;
	}

	ContextPt->output_buffer_p_length = buf.length;
	ContextPt->output_buffer_p =
	    mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
		 ContextPt->fd_r, buf.m.offset);
	if (ContextPt->output_buffer_p == MAP_FAILED) {
		encode_error("mmap for capture interface failed!\n");
		return -1;
	}

	/* Start streaming I/O */
	memset(&buf, 0, sizeof(struct v4l2_buffer));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(ContextPt->fd_r, VIDIOC_STREAMON, &buf.type) < 0) {
		encode_error("cannot start streaming on capture interface\n");
		return -1;
	}
	return 0;
}

static void *Video_read(void *Context)
{
	drv_context_t *ContextPt = (drv_context_t *) Context;
	struct v4l2_buffer buf;
	unsigned int frames = 0;
	int Profile_switch = ContextPt->Profile_Frames;

	while (ContextPt->global_exit != 1) {
		/* Queue a buffer : read */
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = 0;

		buf.m.offset = ContextPt->output_buffer_p_offset;
		buf.field = 0;
		buf.length = ContextPt->output_buffer_p_length;
		buf.reserved = (unsigned int)&ContextPt->out_meta;

		if (ioctl(ContextPt->fd_r, VIDIOC_QBUF, &buf) < 0) {
			encode_error("cannot queue CAPTURE buffer\n");
			goto error;
		}

		if (ioctl(ContextPt->fd_r, VIDIOC_DQBUF, &buf) < 0) {
			encode_error("cannot de-queue CAPTURE buffer\n");
			goto error;
		}

		/*
		 * Last encoded output comes with bytesused=0, so, exit
		 */
		if (!buf.bytesused)
			break;

		if (ContextPt->validation == 0) {
			encode_print(".");
		} else {
			unsigned long frame_crc =
			    crc(ContextPt->output_buffer_p, buf.bytesused);

			encode_print
			    ("[%4ld.%06ld] Received Encoded Frame: "\
			     "%5d %5d bytes  CRC: 0x%08lx\n",
			     buf.timestamp.tv_sec, buf.timestamp.tv_usec,
			     frames, buf.bytesused, frame_crc);

			if (ContextPt->validation > 1)
				FrameWriter(ContextPt, frames, frame_crc,
					    ContextPt->output_buffer_p,
					    buf.bytesused);

			/* Delay if we are REALLY wanting to cause a strain */
			if (ContextPt->validation > 2)
				usleep(1000 * 1000);
		}
		fflush(stdout);

		if (fwrite
		    (ContextPt->output_buffer_p, 1, buf.bytesused,
		     ContextPt->OutputFp) != buf.bytesused) {
			encode_error("Failed to write everything\n");
			goto error;
		}

		memset(&ContextPt->out_meta, 0, sizeof(ContextPt->out_meta));

		frames++;
		if (ContextPt->Profile_Encode) {
			if (frames % Profile_switch == 0) {
				ContextPt->index_profile++;
				if (ContextPt->index_profile >=
				    ContextPt->Profile_nb) {
					ContextPt->index_profile = 0;
				}
				Update_Profile(ContextPt);
				encode_print("P");
				fflush(stdout);
			}
		}
	}

	pthread_exit(NULL);
	return NULL;

error:
	encode_error("Failed to properly retrieve encoded video\n");
	pthread_exit(NULL);
	return NULL;
}

static int InitVideo_write(drv_context_t *ContextPt)
{
	/* Enumerate all the supported formats for
	 * V4L2_BUF_TYPE_VIDEO_CAPTURE data stream type */
	int nfmts = 0;
	struct v4l2_fmtdesc *formats = NULL;
	struct v4l2_fmtdesc fmt = {.index = 0,
				   .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, };
	struct v4l2_format format;
	struct v4l2_streamparm param;
	int i;

	encode_error("InitVideo_write\n");

	/* Select ouput */
	if (ioctl(ContextPt->fd_w, VIDIOC_S_OUTPUT, &ContextPt->io_index) < 0) {
		encode_error("cannot set output %u\n", ContextPt->io_index);
		return -1;
	}

	/* Enumerate all the supported formats for
	 * V4L2_BUF_TYPE_VIDEO_OUTPUT data stream type */
	memset(&fmt, 0, sizeof(struct v4l2_fmtdesc));
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	while (ioctl(ContextPt->fd_w, VIDIOC_ENUM_FMT, &fmt) >= 0)
		fmt.index = ++nfmts;

	formats = malloc(nfmts * sizeof(*formats));
	for (i = 0; i < nfmts; i++) {
		formats[i].index = i;
		formats[i].type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (ioctl(ContextPt->fd_w, VIDIOC_ENUM_FMT, &formats[i]) < 0) {
			encode_error
			    ("cannot get format description for index %u\n",
			     formats[i].index);
			return -1;
		}
	}
	/* Check whether the desired format is available */
	i = 0;
	while ((i < nfmts)
	       && (formats[i].pixelformat !=
		   ((ContextPt->SequenceParams[ContextPt->index_profile].
		     SamplingMode ==
		     HVA_YUV422_RASTER) ? V4L2_PIX_FMT_UYVY :
		    V4L2_PIX_FMT_NV12))) {
		i++;
	}
	if (i == nfmts) {
		encode_error
		    ("desired format for output queue is not supported\n");
		return -1;
	} else {
		/* Set out format */
		free(formats);
		format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		format.fmt.pix.width =
		    ContextPt->SequenceParams[ContextPt->index_profile].width;
		format.fmt.pix.height =
		    ContextPt->SequenceParams[ContextPt->index_profile].height;
		if (ContextPt->SequenceParams[ContextPt->index_profile].
		    SamplingMode == HVA_YUV422_RASTER)
			format.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
		else
			format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;

		format.fmt.pix.field =
		    ContextPt->SequenceParams[ContextPt->index_profile].field;
		format.fmt.pix.colorspace =
		    ContextPt->SequenceParams[ContextPt->index_profile].
		    colorspace;

		if (ioctl(ContextPt->fd_w, VIDIOC_S_FMT, &format) < 0) {
			encode_error("cannot set requested output format\n");
			return -1;
		}
	}

	/* Set the output streaming parameters */
	param.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	param.parm.output.timeperframe.numerator =
	    ContextPt->SequenceParams[ContextPt->index_profile].FrameRate_Den;
	param.parm.output.timeperframe.denominator =
	    ContextPt->SequenceParams[ContextPt->index_profile].FrameRate_Num;
	if (ioctl(ContextPt->fd_w, VIDIOC_S_PARM, &param) < 0) {
		encode_error("cannot set requested output parameters\n");
		return -1;
	}

	/* Init I/O method */
	if (InitMmapVideo_write(ContextPt)) {
		encode_error("Error in InitMmap\n");
		return -1;
	}

	return 0;
}

static int InitVideo_read(drv_context_t *ContextPt)
{
	/* Enumerate all the supported formats for
	 * V4L2_BUF_TYPE_VIDEO_CAPTURE data stream type */
	int nfmts = 0;
	struct v4l2_fmtdesc *formats = NULL;
	struct v4l2_fmtdesc fmt = {.index = 0,
				   .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, };
	struct v4l2_capability cap;
	struct v4l2_format format;
	struct v4l2_streamparm param;
	int i;

	encode_error("InitVideo_read\n");

	/* Get device capabilites */
	if (ioctl(ContextPt->fd_r, VIDIOC_QUERYCAP, &cap) < 0) {
		encode_error("cannot get video capabilities of V4L2 device");
		return -1;
	}
#ifdef SPEC_CHECK
	if ((!cap.capabilities) & V4L2_CAP_VIDEO_CAPTURE) {
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

	/* Select Video input */
	encode_print("Using encoder vid-encoder-%02d\n", ContextPt->io_index);
	if (ioctl(ContextPt->fd_r, VIDIOC_S_INPUT, &ContextPt->io_index) < 0) {
		encode_error("cannot set input %u\n", ContextPt->io_index);
		return -1;
	}

	/* Enumerate all the supported formats for
	 * V4L2_BUF_TYPE_VIDEO_CAPTURE data stream type */
	while (ioctl(ContextPt->fd_r, VIDIOC_ENUM_FMT, &fmt) >= 0)
		fmt.index = ++nfmts;

	formats = malloc(nfmts * sizeof(*formats));
	if (!formats) {
		encode_error("Failed to allocate memory for V4L2 formats\n");
		return -1;
	}

	encode_print("Encoded Video Format Supported ");
	for (i = 0; i < nfmts; i++) {
		formats[i].index = i;
		formats[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(ContextPt->fd_r, VIDIOC_ENUM_FMT, &formats[i]) < 0) {
			encode_error
			    ("cannot get format description for index %u\n",
			     formats[i].index);
			free(formats);
			return -1;
		}
		encode_print("%s ", formats[i].description);
	}
	encode_print("\n");

	/* Check whether the desired format is available */
	i = 0;
	while ((i < nfmts) && (formats[i].pixelformat != V4L2_PIX_FMT_H264))
		i++;

	if (i == nfmts) {
		encode_error
		    ("desired format for capture queue is not supported\n");
		return -1;
	}
	encode_print("Encoded Video Format Selected  %s\n",
		     formats[i].description);

	/* Set input format */
	free(formats);
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width =
	    ContextPt->SequenceParams[ContextPt->index_profile].width;
	format.fmt.pix.height =
	    ContextPt->SequenceParams[ContextPt->index_profile].height;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
	if (ioctl(ContextPt->fd_r, VIDIOC_S_FMT, &format) < 0) {
		encode_error("cannot set requested input format\n");
		return -1;
	}

	/* Set the input streaming parameters */
	param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	param.parm.capture.timeperframe.numerator =
	    ContextPt->SequenceParams[ContextPt->index_profile].
	    TargetFrameRate_Den;
	param.parm.capture.timeperframe.denominator =
	    ContextPt->SequenceParams[ContextPt->index_profile].
	    TargetFrameRate_Num;
	if (ioctl(ContextPt->fd_r, VIDIOC_S_PARM, &param) < 0) {
		encode_error("cannot set requested input parameters\n");
		return -1;
	}

	/* initialize the controls */
	if (ControlsInitVideo(ContextPt)) {
		encode_error("Error in ControlsInit\n");
		return -1;
	}

	/* Init I/O method */
	if (InitMmapVideo_read(ContextPt)) {
		encode_error("Error in InitMmap\n");
		return -1;
	}

	return 0;
}

static int OpenVideo(drv_context_t *ContextPt)
{
	int ret;

	/* Output Data Encoded */
	ContextPt->fd_r =
	    v4l2_open_by_name(V4L2_ENC_DRIVER_NAME, V4L2_ENC_CARD_NAME, O_RDWR);
	if (ContextPt->fd_r < 0) {
		encode_error
		    ("---->>> can't open encoder Device for read h234\n");
		close(ContextPt->fd_r);
		return -1;
	}

	/* Input Data PCM */
	if (ContextPt->tunneling_mode == 0) {
		ContextPt->fd_w =
		    v4l2_open_by_name(V4L2_ENC_DRIVER_NAME, V4L2_ENC_CARD_NAME,
				      O_RDWR);
		if (ContextPt->fd_w < 0) {
			encode_error
			    (">>> can't open encoder Device for write YUV\n");
			close(ContextPt->fd_w);
			return -1;
		}
	}

	/* Should respect following order for S_FMT: 'output' S_FMT IOCTL , then 'capture' S_FMT */
	if (ContextPt->tunneling_mode == 0)
		InitVideo_write(ContextPt);
	InitVideo_read(ContextPt);

	/* Should respect following order for STREAM_ON: 'capture' STREAM_ON IOCTL , then 'output' STREAM_ON */
	ret = stream_on(ContextPt, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (ret) {
		encode_error(" STREAM_ON error on V4L2_BUF_TYPE_VIDEO_CAPTURE\n");
		return ret;
	}
	encode_print("stream_on_done CAPTURE\n");

	if (ContextPt->tunneling_mode == 0) {
		ret = stream_on(ContextPt, V4L2_BUF_TYPE_VIDEO_OUTPUT);
		if (ret) {
			encode_error(" STREAM_ON error on V4L2_BUF_TYPE_VIDEO_OUTPUT\n");
			return ret;
		}
		encode_print("stream_on_done OUTPUT\n");
	}
	return 0;
}

static void ParseVideoProfile(drv_context_t *Context, char *field, int value,
			      int index)
{
	struct HVAEnc_SequenceParams_s *seqparam =
	    &Context->SequenceParams[index];

	CHECK_PARAM(field, value, "LevelIDC", &seqparam->LevelIDC);
	CHECK_PARAM(field, value, "ProfileIDC", &seqparam->ProfileIDC);
	/* CHECK_PARAM(field, value, "IntraPeriod", &seqparam->IntraPeriod); */
	CHECK_PARAM(field, value, "GOPSize", &seqparam->GOPSize);
	CHECK_PARAM(field, value, "IDRIntraEnable", &seqparam->IDRIntraEnable);
	CHECK_PARAM(field, value, "FrameRate_Num", &seqparam->FrameRate_Num);
	CHECK_PARAM(field, value, "FrameRate_Den", &seqparam->FrameRate_Den);

	CHECK_PARAM(field, value, "TargetFrameRate_Num",
		    &seqparam->TargetFrameRate_Num);
	CHECK_PARAM(field, value, "TargetFrameRate_Den",
		    &seqparam->TargetFrameRate_Den);

	CHECK_PARAM(field, value, "width", &seqparam->width);
	CHECK_PARAM(field, value, "height", &seqparam->height);
	CHECK_PARAM(field, value, "BitRateControlType",
		    &seqparam->BitRateControlType);
	CHECK_PARAM(field, value, "SamplingMode", &seqparam->SamplingMode);
	CHECK_PARAM(field, value, "SliceMode", &seqparam->SliceMode);
	CHECK_PARAM(field, value, "ByteSliceSize", &seqparam->ByteSliceSize);
	CHECK_PARAM(field, value, "MbSliceSize", &seqparam->MbSliceSize);
	CHECK_PARAM(field, value, "IntraRefreshType",
		    &seqparam->IntraRefreshType);
	CHECK_PARAM(field, value, "AirMbNum", &seqparam->AirMbNum);
	CHECK_PARAM(field, value, "BitRate", &seqparam->BitRate);
	CHECK_PARAM(field, value, "CpbBufferSize", &seqparam->CpbBufferSize);
	CHECK_PARAM(field, value, "CbrDelay", &seqparam->CbrDelay);
	CHECK_PARAM(field, value, "VBRInitialQuant",
		    &seqparam->VBRInitialQuant);
	CHECK_PARAM(field, value, "qpmin", &seqparam->qpmin);
	CHECK_PARAM(field, value, "qpmax", &seqparam->qpmax);
	CHECK_PARAM(field, value, "AddSEIMessages", &seqparam->AddSEIMessages);
	CHECK_PARAM(field, value, "AddHrdParams", &seqparam->AddHrdParams);
	CHECK_PARAM(field, value, "AddVUIParams", &seqparam->AddVUIParams);

	CHECK_PARAM(field, value, "field", &seqparam->field);
	CHECK_PARAM(field, value, "colorspace", &seqparam->colorspace);
	CHECK_PARAM(field, value, "AspectRatio", &seqparam->AspectRatio);
	seqparam->IntraPeriod = seqparam->GOPSize;

#ifdef DISABLED
	CHECK_PARAM(field, value, "aspect_ratio_info_present_flag",
			&seqparam->VUIParams.aspect_ratio_info_present_flag);
	CHECK_PARAM(field, value, "aspect_ratio_idc",
			&seqparam->VUIParams.aspect_ratio_idc);
	CHECK_PARAM(field, value, "sar_width",
			&seqparam->VUIParams.sar_width);
	CHECK_PARAM(field, value, "sar_height",
			&seqparam->VUIParams.sar_height);
	CHECK_PARAM(field, value, "video_signal_type_present_flag",
			&seqparam->VUIParams.video_signal_type_present_flag);
	CHECK_PARAM(field, value, "video_format",
			&seqparam->VUIParams.video_format);
	CHECK_PARAM(field, value, "video_full_range_flag",
			&seqparam->VUIParams.video_full_range_flag);
	CHECK_PARAM(field, value, "colour_description_present_flag",
			&seqparam->VUIParams.colour_description_present_flag);
	CHECK_PARAM(field, value, "colour_primaries",
			&seqparam->VUIParams.colour_primaries);
	CHECK_PARAM(field, value, "transfer_characteristics",
			&seqparam->VUIParams.transfer_characteristics);
	CHECK_PARAM(field, value, "matrix_coefficients",
			&seqparam->VUIParams.matrix_coefficients);
#endif
}

static void ParseConfigFile(drv_context_t *Context, FILE *ConfigFp)
{
	char line[MAX_CONFIG_LINE_SIZE];
	char *field;
	char *valuestr;
	char *stripped_line;
	int value;
	int index_profile = 0;

	while (fgets(line, MAX_CONFIG_LINE_SIZE, ConfigFp) != NULL) {
		stripped_line = strpbrk(line, "\r\n#");
		if (stripped_line != NULL)
			stripped_line[0] = '\0';
		field = strtok(line, "=");
		if (field == NULL)
			continue;

		valuestr = strtok(NULL, "=");
		value = atoi(valuestr);

		CHECK_PARAM(field, value, "Profile_Encode",
			    &Context->Profile_Encode);
		if (Context->Profile_Encode) {
			CHECK_PARAM(field, value, "Profile_Frames",
				    &Context->Profile_Frames);
			CHECK_PARAM(field, value, "Profile_nb",
				    &Context->Profile_nb);
		}
	}
	if (Context->Profile_Encode)
		encode_print("Nb Profile Define:%d\n", Context->Profile_nb);

	fseek(ConfigFp, 0, SEEK_SET);	/* reset reading position */

	while (fgets(line, MAX_CONFIG_LINE_SIZE, ConfigFp) != NULL) {
		stripped_line = strpbrk(line, "\r\n#");
		if (stripped_line != NULL)
			stripped_line[0] = '\0';
		field = strtok(line, "=");
		if (field == NULL)
			continue;

		valuestr = strtok(NULL, "=");
		value = atoi(valuestr);

		if (strncmp(field, "Profile_Start", strlen("Profile_Start")) ==
		    0) {
			encode_print("Beginning of Profile: %d\n",
				     index_profile);
		}
		if (strncmp(field, "Profile_End", strlen("Profile_End")) == 0) {
			encode_print("Ending of Profile: %d\n", index_profile);
			index_profile++;
		}
		ParseVideoProfile(Context, field, value, index_profile);
	}
}

int parse_video_ini(drv_context_t *ContextPt, char *filename)
{
	drv_context_t *Context = ContextPt;
	FILE *fd;

	fd = fopen(filename, "rb");
	if (fd == NULL) {
		encode_error("can't open config file\n");
		return -1;
	}

	/* set default values */
	Context->SequenceParams[ContextPt->index_profile].LevelIDC = 31;
	Context->SequenceParams[ContextPt->index_profile].ProfileIDC =
	    PROFILE_BASELINE;
	Context->SequenceParams[ContextPt->index_profile].IntraPeriod = 12;
	Context->SequenceParams[ContextPt->index_profile].GOPSize = 12;
	Context->SequenceParams[ContextPt->index_profile].IDRIntraEnable = TRUE;
	Context->SequenceParams[ContextPt->index_profile].FrameRate_Num = 25;
	Context->SequenceParams[ContextPt->index_profile].FrameRate_Den = 1;
	Context->SequenceParams[ContextPt->index_profile].TargetFrameRate_Num =
	    25;
	Context->SequenceParams[ContextPt->index_profile].TargetFrameRate_Den =
	    1;
	Context->SequenceParams[ContextPt->index_profile].width = 720;
	Context->SequenceParams[ContextPt->index_profile].height = 576;
	Context->SequenceParams[ContextPt->index_profile].BitRateControlType =
	    HVA_VBR;
	Context->SequenceParams[ContextPt->index_profile].SamplingMode =
	    HVA_YUV422_RASTER;
	Context->SequenceParams[ContextPt->index_profile].SliceMode = 0;
	Context->SequenceParams[ContextPt->index_profile].ByteSliceSize = 0;
	Context->SequenceParams[ContextPt->index_profile].MbSliceSize = 0;
	Context->SequenceParams[ContextPt->index_profile].IntraRefreshType =
	    HVA_INTRA_REFRESH_DISABLED;
	/* adaptative intra refresh MB number
	 * (HVA_ADAPTATIVE_INTRA_REFRESH only) */
	Context->SequenceParams[ContextPt->index_profile].AirMbNum = 0;
	Context->SequenceParams[ContextPt->index_profile].BitRate = 4000000;
	Context->SequenceParams[ContextPt->index_profile].CpbBufferSize =
	    2000000;
	Context->SequenceParams[ContextPt->index_profile].CbrDelay = 0;
	Context->SequenceParams[ContextPt->index_profile].VBRInitialQuant = 28;
	Context->SequenceParams[ContextPt->index_profile].qpmin = 5;
	Context->SequenceParams[ContextPt->index_profile].qpmax = 51;
	Context->SequenceParams[ContextPt->index_profile].AspectRatio = 0;
	Context->SequenceParams[ContextPt->index_profile].AddSEIMessages =
	    FALSE;
	Context->SequenceParams[ContextPt->index_profile].AddHrdParams = FALSE;
	Context->SequenceParams[ContextPt->index_profile].AddVUIParams = FALSE;

	ParseConfigFile(Context, fd);
	fclose(fd);

	return 0;
}

int encode_video(drv_context_t *Context)
{
	if (OpenVideo(Context)) {
		CloseVideo(Context);
		encode_error("Error opening Video\n");
		return 1;
	}

	if (Context->SequenceParams[Context->index_profile].SamplingMode ==
	    HVA_YUV422_RASTER) {
		Context->input_buffer_size =
		    Context->SequenceParams[Context->index_profile].width *
		    Context->SequenceParams[Context->index_profile].height * 2;
	} else {
		Context->input_buffer_size =
		    Context->SequenceParams[Context->index_profile].width *
		    Context->SequenceParams[Context->index_profile].height * 3 /
		    2;
	}

	memset(&Context->output_buffers, 0, sizeof(Context->output_buffers));

	/* Thread write data to Encoded File */
	pthread_create(&Context->reader_thread, NULL, Video_read, Context);

	if (Context->tunneling_mode == 0) {
		/* Thread read data from File */
		pthread_create(&Context->writer_thread, NULL, Video_write,
			       Context);
	}

	if (Context->tunneling_mode == 0)
		pthread_join(Context->writer_thread, NULL);

	pthread_join(Context->reader_thread, NULL);

	if (CloseVideo(Context)) {
		encode_error("Error closing Video\n");
		return 1;
	}
	if (Context->tunneling_mode == 0)
		fclose(Context->InputFp);

	return 0;
}
