/************************************************************************
Copyright (C) 2007, 2009, 2010 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
 * Testing for linux v4l2 tsmux test device
************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <linux/videodev2.h>

#include <signal.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <pthread.h>
#include <assert.h>
#include <getopt.h>		/* getopt_long() */
#include <fcntl.h>		/* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "linux/include/linux/dvb/dvb_v4l2_export.h"
#include "utils/v4l2_helper.h"

#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG	0
#endif

#define PRINT_DEBUG(fmt, args...) \
	((void) (ENABLE_DEBUG && \
		(printf("%s: " fmt, __FUNCTION__, ##args), 0)))

#ifndef bool
#define bool unsigned char
#endif
#ifndef false
#define false 0
#define true  1
#endif

#define CLEAR(x) memset(&(x), 0, sizeof(x))

/* Global variables */
#define VIDEO_TYPE 1
#define AUDIO_TYPE 2
#define PCR_PERIOD 4500
#define MUX_BITRATE 48*1024*1024

#define MAX_TSMUXES 2

#define MAX_STREAMS 2
#define MAX_BUFFERS_PER_STREAM 32
#define STREAM_BUFFER_SIZE (512*1024)

#define NUM_TSMUX_BUFFERS 4
#define TSMUX_BUFFER_SIZE ((MUX_BITRATE) / (90000 / PCR_PERIOD))

static unsigned char
 StreamBufferMemory[MAX_STREAMS][MAX_BUFFERS_PER_STREAM][STREAM_BUFFER_SIZE];
static bool StreamBufferInUse[MAX_STREAMS][MAX_BUFFERS_PER_STREAM];
static int StreamNextBuffer[MAX_STREAMS];
static unsigned int StreamTypes[MAX_STREAMS];
static bool StreamUsed[MAX_STREAMS];

static bool TsmuxBufferInUse[NUM_TSMUX_BUFFERS];
static unsigned char TsmuxMemory[NUM_TSMUX_BUFFERS][TSMUX_BUFFER_SIZE];

static const char short_options[] = "b:d:ho:c:t:r:v:ia";

static const struct option
 long_options[] = {
	{"bad", required_argument, NULL, 'b'},
	{"device", required_argument, NULL, 'd'},
	{"help", no_argument, NULL, 'h'},
	{"output", required_argument, NULL, 'o'},
	{"count", required_argument, NULL, 'c'},
	{"type", required_argument, NULL, 't'},
	{"raw", required_argument, NULL, 'r'},
	{"raw", required_argument, NULL, 'v'},
	{"raw", required_argument, NULL, 'a'},
	{"raw", required_argument, NULL, 'i'},
	{0, 0, 0, 0}
};

static char *dev_name;
static char *out_name;
static char *in_name = NULL;
FILE *outfp;
static int frame_count;
static int fdin[2] = { -1, -1 };

static int fdout[MAX_STREAMS];
static int insert_bad_every;
bool running = true;

static int video_stream = 0;
static int audio_stream = 0;
static int check_indexing = 0;

enum content_types {
	CONTENT_TYPE_ES = 0,
	CONTENT_TYPE_PES,
};
static enum content_types content_type = CONTENT_TYPE_ES;

/* Function prototypes */
static void SendStreamBuffer(unsigned int StreamId,
			     unsigned int Index,
			     unsigned int Length,
			     unsigned long long DTS, bool end_of_stream);
static unsigned int GetNextStreamBuffer(unsigned int StreamId);

/* Functions */
static void errno_exit(const char *s)
{
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
	int r;

	do {
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

static void QueueMuxBuffer(int num)
{
	struct v4l2_buffer buf;

	PRINT_DEBUG("%s\n", __func__);
	if (TsmuxBufferInUse[num]) {
		fprintf(stderr, "ERROR: Tsmux buffer %d in use\n", num);
		exit(EXIT_FAILURE);
	}
	CLEAR(buf);

	buf.index = num;
	buf.type = V4L2_BUF_TYPE_DVB_CAPTURE;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.m.userptr = (unsigned long)&TsmuxMemory[num][0];
	buf.length = TSMUX_BUFFER_SIZE;
	buf.bytesused = 0;

	memset(&TsmuxMemory[num][0], 0xab, TSMUX_BUFFER_SIZE);

	if (-1 == xioctl(fdin[0], VIDIOC_QBUF, &buf))
		errno_exit("VIDIOC_QBUF");

	PRINT_DEBUG("DONE\n");

	TsmuxBufferInUse[num] = true;
	return;
}

static void ProcessMuxBuffer(int bufnum, int bytesused)
{
	fwrite(&TsmuxMemory[bufnum][0], bytesused, 1, outfp);
}

static void DequeueMuxBuffer(int *bufnum, int *bytesused)
{
	struct v4l2_buffer buf;
	unsigned int i = 0;
	unsigned int found = 0;

	*bufnum = -1;
	*bytesused = -1;

	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_DVB_CAPTURE;
	buf.memory = V4L2_MEMORY_USERPTR;
	if (-1 == xioctl(fdin[0], VIDIOC_DQBUF, &buf)) {
		switch (errno) {
		case EAGAIN:
			fprintf(stderr, "Not ready to dequeue mux\n");
			return;
		case EIO:
			/* Could ignore EIO, see spec. */
			/* fall through */
		default:
			errno_exit("VIDIOC_DQBUF");
			break;
		}
	}
	/* Check and mark the relevant buffer as no longer in use */
	/* TODO Can use the buf.index field to find the buffer */
	for (i = 0; i < NUM_TSMUX_BUFFERS; i++) {
		if (TsmuxBufferInUse[i]) {
			if (buf.m.userptr == (unsigned long)&TsmuxMemory[i][0]) {
				PRINT_DEBUG("Dequeued mux buffer %d\n", i);
				TsmuxBufferInUse[i] = false;
				*bufnum = i;
				*bytesused = buf.bytesused;
				found = 1;
			}
		}
	}
	if (found == 0) {
		fprintf(stderr, "ERROR: Can't find dequeued tsmux buffer\n");
		exit(EXIT_FAILURE);
	}
	if (buf.flags & V4L2_BUF_FLAG_ERROR) {
		fprintf(stderr, "WARN: Dequeued mux buffer has error\n");
	}
	return;
}

static bool ReadyToDequeueMux(void)
{
	struct pollfd fds;
	int r;

	fds.fd = fdin[0];
	fds.events = (POLLIN);

	/* Mux buffers are ready to be dequeued if read is ready */
	/* NOTE: Using poll instead of select because select returns a valid
	 * set fds value on POLLERR for some reason so when the queue is empty
	 * it looks like there is a buffer to be dequeued.
	 */
	r = poll(&fds, 1, 100);
	if (r < 0)
		errno_exit("Poll\n");
	if (r > 0) {
		if ((fds.revents & POLLERR) || (fds.revents & POLLHUP) ||
		    (fds.revents & POLLNVAL)) {
			return false;
		}
		if (fds.revents & POLLIN) {
			return true;
		}
		return true;
	} else {
		return false;
	}
}

static void TerminateAllStreams(void)
{
	int i;
	unsigned int Index;

	for (i = 0; i < MAX_STREAMS; i++) {
		if (StreamUsed[i]) {
			Index = GetNextStreamBuffer(i);
			SendStreamBuffer(i, Index, sizeof(unsigned int), 0ULL,
					 true);
		}
	}
	return;
}

void DequeueRemainingMuxBuffers(void)
{
	int bufnum, bytesused;

	while (ReadyToDequeueMux()) {
		DequeueMuxBuffer(&bufnum, &bytesused);
		if (bytesused != 0) {
			fprintf(stderr,
				"Warning: Remaining mux buffer with %d bytes\n",
				bytesused);
		}
	}
}

static int GetMultiplexedData(bool flushing)
{
	bool stop = false;
	int bufnum;
	int bytesoutput = 0;
	int ret = 0;

	if (flushing) {
		TerminateAllStreams();
	}
	do {
		if (flushing || ReadyToDequeueMux()) {
			/* Dequeue will block if no buffer available */
			DequeueMuxBuffer(&bufnum, &bytesoutput);
			PRINT_DEBUG("Dequeued mux buffer %d bytesoutput %d\n",
				    bufnum, bytesoutput);
			if (bytesoutput == 0) {
				PRINT_DEBUG
				    ("Last mux buffer was 0, stopping\n");
				stop = true;
				if (!flushing)
					ret = -1;
				else
					DequeueRemainingMuxBuffers();
			} else {
				PRINT_DEBUG
				    ("Processing mux buffer with %d bytes\n",
				     bytesoutput);
				ProcessMuxBuffer(bufnum, bytesoutput);
				QueueMuxBuffer(bufnum);
			}
		} else {
			PRINT_DEBUG
			    ("Returning from GetMultiplexedData() without putting any data \n");
			stop = true;
		}
	} while (stop == false);
	return ret;
}

static void QueueStreamBuffer(unsigned int StreamId,
			      unsigned int Index,
			      unsigned int Length, unsigned long long DTS,
			      bool end_of_stream)
{
	struct v4l2_buffer buf;
	struct v4l2_format format;
	struct v4l2_tsmux_src_es_metadata es_meta;

	format.type = V4L2_BUF_TYPE_DVB_OUTPUT;
	if (-1 == xioctl(fdout[StreamId], VIDIOC_G_FMT, &format))
		errno_exit("VIDIOC_G_FMT");

	CLEAR(buf);
	buf.index = Index;
	buf.type = V4L2_BUF_TYPE_DVB_OUTPUT;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.m.userptr = (unsigned long)&StreamBufferMemory[StreamId][Index][0];
	buf.length = STREAM_BUFFER_SIZE;
	buf.bytesused = Length;
	buf.timestamp.tv_usec = DTS;

	/*TODO :: Need to fill proper values here.. */
	if ((format.fmt.dvb.data_type == V4L2_DVB_FMT_ES) ||
		(format.fmt.dvb.data_type == V4L2_DVB_FMT_PES)){
		es_meta.DTS = DTS;
		es_meta.dit_transition = 0;

		buf.reserved = (int)&es_meta;
	}

	if (-1 == xioctl(fdout[StreamId], VIDIOC_QBUF, &buf))
		errno_exit("VIDIOC_QBUF");

	return;
}

static int DequeueStreamBuffer(unsigned int StreamId, unsigned int *Index)
{
	struct v4l2_buffer buf;
	int ret = 0;

	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_DVB_OUTPUT;
	buf.memory = V4L2_MEMORY_USERPTR;
	if (-1 == xioctl(fdout[StreamId], VIDIOC_DQBUF, &buf)) {
		switch (errno) {
		case EAGAIN:
			fprintf(stderr, "Not ready to dequeue %d\n", StreamId);
			ret = -1;
		case EIO:
			/* Could ignore EIO, see spec. */
			/* fall through */
		default:
			errno_exit("VIDIOC_DQBUF");
			break;
		}
	}
	/* Check for errors in the dequeued buffers, but just issue
	 * a warning and continue. */
	if (buf.flags & V4L2_BUF_FLAG_ERROR) {
		fprintf(stderr, "WARNING: Error seen in buffer %d\n",
			buf.index);
	}
	/* Check that the returned buffer index matches the stream buffer */
	if (buf.m.userptr !=
	    (unsigned long)&StreamBufferMemory[StreamId][buf.index][0]) {
		fprintf(stderr,
			"ERROR: Dequeued stream buffer %d address mismatch\n",
			buf.index);
		exit(EXIT_FAILURE);
	}
	/* Check that the dequeued buffer was marked as in use */
	if (StreamBufferInUse[StreamId][buf.index]) {
		StreamBufferInUse[StreamId][buf.index] = false;
	} else {
		fprintf(stderr,
			"ERROR: Dequeued stream buffer %d not currently in use\n",
			buf.index);
		exit(EXIT_FAILURE);
	}
	/* Set the index to the newly dequeued buffer index */
	*Index = buf.index;

	return ret;
}

static void SendStreamBuffer(unsigned int StreamId,
			     unsigned int Index,
			     unsigned int Length,
			     unsigned long long DTS, bool end_of_stream)
{
	if (!end_of_stream) {
		PRINT_DEBUG
		    ("Sending buffer stream=%d, index=%d, addr=%ld, Len=%d, DTS = 0x%010llx\n",
		     StreamId, Index,
		     (unsigned long)&StreamBufferMemory[StreamId][Index][0],
		     Length, DTS);
	} else {
		Length = 0;
		PRINT_DEBUG
		    ("Send end stream buffer stream=%d, index=%d, addr=%ld, Len=%d\n",
		     StreamId, Index,
		     (unsigned long)&StreamBufferMemory[StreamId][Index][0],
		     Length);
	}
	QueueStreamBuffer(StreamId, Index, Length, DTS, end_of_stream);

}

static unsigned int GetNextStreamBuffer(unsigned int StreamId)
{
	unsigned int i;

	i = StreamNextBuffer[StreamId];
	StreamNextBuffer[StreamId]++;
	if (StreamNextBuffer[StreamId] >= MAX_BUFFERS_PER_STREAM)
		StreamNextBuffer[StreamId] = 0;
	/* If the next buffer is marked as in use then dequeue a free buffer */
	if (StreamBufferInUse[StreamId][i]) {
		/* Note that the dequeue will return the index the next */
		/* free buffer, which may not be the expected one!! */
		if (DequeueStreamBuffer(StreamId, &i) != 0) {
			fprintf(stderr,
				"ERROR: Can't dequeue previously queued buffer\n");
			exit(EXIT_FAILURE);
		}
	}
	StreamBufferInUse[StreamId][i] = true;

	return i;
}

static void StartStreamStreaming(unsigned int StreamId)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_DVB_OUTPUT;

	if (-1 == xioctl(fdout[StreamId], VIDIOC_STREAMON, &type))
		errno_exit("VIDIOC_STREAMON");

	return;
}

static void StopStreamStreaming(unsigned int StreamId)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_DVB_OUTPUT;

	if (-1 == xioctl(fdout[StreamId], VIDIOC_STREAMOFF, &type))
		errno_exit("VIDIOC_STREAMOFF");

	return;
}

static void SetStreamOutParameters(unsigned int StreamId,
				   unsigned int Type,
				   unsigned int BitBufferSize)
{
	struct v4l2_ext_controls controls;
	struct v4l2_ext_control ctrl_array[] = {
		{
		 .id = V4L2_CID_STM_TSMUX_INCLUDE_PCR,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 0 */
		 .value = 0,
		 },
		{
		 .id = V4L2_CID_STM_TSMUX_STREAM_PID,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 0x100*(ProgId+1)+streamId */
		 .value = (0x150 + StreamId),
		 },
		{
		 .id = V4L2_CID_STM_TSMUX_BIT_BUFFER_SIZE,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 2*1024*1024 */
		 .value = BitBufferSize,
		 },
	};
	int i;
	struct v4l2_ext_control *ctrl;

	StreamTypes[StreamId] = Type;

	controls.ctrl_class = V4L2_CTRL_CLASS_TSMUX;
	controls.count = sizeof(ctrl_array) / sizeof(struct v4l2_ext_control);
	if (controls.count == 0)
		return;
	controls.reserved[0] = 0;
	controls.reserved[1] = 0;
	controls.controls = &ctrl_array[0];

	for (i = 0; i < controls.count; i++) {
		ctrl = &controls.controls[i];
		PRINT_DEBUG("Attempting to set stream %d control:-\n",
			    StreamId);
		PRINT_DEBUG("Setting control id = %d\n", ctrl->id);
		PRINT_DEBUG("Size = %d\n", ctrl->size);
		if (ctrl->size != 0)
			PRINT_DEBUG("Value = %s\n", ctrl->string);
		else
			PRINT_DEBUG("Value = %d\n", ctrl->value);
	}
	if (-1 == xioctl(fdout[StreamId], VIDIOC_S_EXT_CTRLS, &controls))
		errno_exit("VIDIOC_S_EXT_CTRLS");

}

static void InitialiseStreamBuffersInUse(unsigned int StreamId)
{
	int i;

	for (i = 0; i < MAX_BUFFERS_PER_STREAM; i++) {
		StreamBufferInUse[StreamId][i] = false;
	}
	StreamNextBuffer[StreamId] = 0;
	return;
}

static void InitialiseStreamOutUserPtr(unsigned int StreamId)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = MAX_BUFFERS_PER_STREAM;
	req.type = V4L2_BUF_TYPE_DVB_OUTPUT;
	req.memory = V4L2_MEMORY_USERPTR;

	PRINT_DEBUG("Doing VIDIOC_REQBUFS for fdout[%d]\n", StreamId);
	if (-1 == xioctl(fdout[StreamId], VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s output does not support "
				"user pointer i/o\n", dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	return;
}

static void CheckStreamOutCapabilities(unsigned int StreamId)
{
	struct v4l2_capability cap;

	if (-1 == xioctl(fdout[StreamId], VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n", dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_DVB_OUTPUT)) {
		fprintf(stderr, "%s is no video output device\n", dev_name);
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n",
			dev_name);
		exit(EXIT_FAILURE);
	}
	return;
}

static void SetIndexingOnOutputStream(unsigned int StreamId,
				      unsigned int Enable)
{
	struct v4l2_ext_controls controls;
	struct v4l2_ext_control ctrl_array[] = {
		{
		 .id = V4L2_CID_STM_TSMUX_INDEXING,
		 .size = 0,
		 .reserved2 = {0},
		 .value = Enable,
		 },
	};
	int i;
	struct v4l2_ext_control *ctrl;

	controls.ctrl_class = V4L2_CTRL_CLASS_TSMUX;
	controls.count = sizeof(ctrl_array) / sizeof(struct v4l2_ext_control);
	if (controls.count == 0)
		return;
	controls.reserved[0] = 0;
	controls.reserved[1] = 0;
	controls.controls = &ctrl_array[0];

	for (i = 0; i < controls.count; i++) {
		ctrl = &controls.controls[i];
		PRINT_DEBUG("Attempting to set tsmux control:-\n");
		PRINT_DEBUG("Setting control id = %d\n", ctrl->id);
		PRINT_DEBUG("Size = %d\n", ctrl->size);
		if (ctrl->size != 0)
			PRINT_DEBUG("Value = %s\n", ctrl->string);
		else
			PRINT_DEBUG("Value = %d\n", ctrl->value);
	}
	if (-1 == xioctl(fdout[StreamId], VIDIOC_S_EXT_CTRLS, &controls))
		errno_exit("VIDIOC_S_EXT_CTRLS");
	return;
}

static void SetTsmuxIndexingControls(unsigned int TsmuxId)
{
	struct v4l2_ext_controls controls;
	struct v4l2_ext_control ctrl_array[] = {
		{
		 .id = V4L2_CID_STM_TSMUX_INDEXING_MASK,
		 .size = 0,
		 .reserved2 = {0},
		 .value = V4L2_STM_TSMUX_INDEX_PUSI | V4L2_STM_TSMUX_INDEX_PTS,
		 },
	};
	int i;
	struct v4l2_ext_control *ctrl;

	controls.ctrl_class = V4L2_CTRL_CLASS_TSMUX;
	controls.count = sizeof(ctrl_array) / sizeof(struct v4l2_ext_control);
	if (controls.count == 0)
		return;
	controls.reserved[0] = 0;
	controls.reserved[1] = 0;
	controls.controls = &ctrl_array[0];

	for (i = 0; i < controls.count; i++) {
		ctrl = &controls.controls[i];
		PRINT_DEBUG("Attempting to set tsmux control:-\n");
		PRINT_DEBUG("Setting control id = %d\n", ctrl->id);
		PRINT_DEBUG("Size = %d\n", ctrl->size);
		if (ctrl->size != 0)
			PRINT_DEBUG("Value = %s\n", ctrl->string);
		else
			PRINT_DEBUG("Value = %d\n", ctrl->value);
	}
	if (-1 == xioctl(fdin[TsmuxId], VIDIOC_S_EXT_CTRLS, &controls))
		errno_exit("VIDIOC_S_EXT_CTRLS");
	return;
}

static void OpenStreamOut(unsigned int StreamId, unsigned int Type)
{
	struct v4l2_format format;

	fdout[StreamId] = -1;
	/* We need BLOCKING ON */
	fdout[StreamId] = open(dev_name, O_RDWR);
	if (-1 == fdout[StreamId]) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n",
			dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Set output format */
	format.type = V4L2_BUF_TYPE_DVB_OUTPUT;
	format.fmt.dvb.data_type = V4L2_DVB_FMT_ES;
	format.fmt.dvb.buffer_size = STREAM_BUFFER_SIZE;
	format.fmt.dvb.codec = Type;
	if (ioctl(fdout[StreamId], VIDIOC_S_FMT, &format) < 0) {
		errno_exit("ERROR : VIDIOC_S_FMT --> OUTPUT");
	}

	if (check_indexing) {
		SetIndexingOnOutputStream(StreamId, 1);
		SetTsmuxIndexingControls(0);
	}

}

static void AddAStream(unsigned int StreamId,
		       unsigned int Type, unsigned int BitBufferSize)
{
	OpenStreamOut(StreamId, Type);
	CheckStreamOutCapabilities(StreamId);
	InitialiseStreamOutUserPtr(StreamId);
	InitialiseStreamBuffersInUse(StreamId);
	SetStreamOutParameters(StreamId, Type, BitBufferSize);
	StartStreamStreaming(StreamId);
	StreamUsed[StreamId] = true;

}

static void StartMuxStreaming(void)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_DVB_CAPTURE;

	if (-1 == xioctl(fdin[0], VIDIOC_STREAMON, &type))
		errno_exit("VIDIOC_STREAMON");

	return;
}

static void StopMuxStreaming(void)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_DVB_CAPTURE;

	if (-1 == xioctl(fdin[0], VIDIOC_STREAMOFF, &type))
		errno_exit("VIDIOC_STREAMOFF");

	return;
}

static void StartTsmux(void)
{
	int i;

	/* Queue the tsmux memory buffers */
	for (i = 0; i < NUM_TSMUX_BUFFERS; i++) {
		QueueMuxBuffer(i);
	}

	/* Start streaming on the tsmux */
	StartMuxStreaming();
}

static void StopTsmux(void)
{
	int i;

	/* Stop streaming on all streams */
	for (i = 0; i < MAX_STREAMS; i++) {
		if (StreamUsed[i]) {
			StopStreamStreaming(i);
		}
	}
	/* Check the tsmux memory buffers are not in use */
	for (i = 0; i < NUM_TSMUX_BUFFERS; i++) {
		if (TsmuxBufferInUse[i]) {
			fprintf(stderr, "Warning: Mux buffer %d still in use\n",
				i);
			/*exit(EXIT_FAILURE); */
		}
	}
	/* Stop streaming on the tsmux */
	StopMuxStreaming();

	for (i = 0; i < MAX_STREAMS; i++) {
		if (StreamUsed[i]) {
			if (check_indexing)
				SetIndexingOnOutputStream(i, 0);
		}
	}
}

static void SetTsmuxParameters(unsigned int TsmuxId)
{
	struct v4l2_ext_controls controls;
	struct v4l2_ext_control ctrl_array[] = {
		{
		 .id = V4L2_CID_STM_TSMUX_PCR_PERIOD,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 4500 */
		 .value = 4000,//PCR_PERIOD,
		 },
		{
		 .id = V4L2_CID_STM_TSMUX_GEN_PCR_STREAM,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 1 */
		 .value = 1,
		 },
		{
		 .id = V4L2_CID_STM_TSMUX_TABLE_GEN,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is PAT_PMT + SDT */
		 .value = (V4L2_STM_TSMUX_TABLE_GEN_PAT_PMT |
			   V4L2_STM_TSMUX_TABLE_GEN_SDT),
		 },
		{
		 .id = V4L2_CID_STM_TSMUX_TS_ID,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 0 */
		 .value = 0,
		 },
		{
		 .id = V4L2_CID_STM_TSMUX_BITRATE_IS_CONSTANT,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 0 i.e. variable */
		 .value = 0,
		 },
		{
		 .id = V4L2_CID_STM_TSMUX_BITRATE,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 48*1024*1024 */
		 .value = MUX_BITRATE,
		 },
	};
	int i;
	struct v4l2_ext_control *ctrl;

	controls.ctrl_class = V4L2_CTRL_CLASS_TSMUX;
	controls.count = sizeof(ctrl_array) / sizeof(struct v4l2_ext_control);
	if (controls.count == 0)
		return;
	controls.reserved[0] = 0;
	controls.reserved[1] = 0;
	controls.controls = &ctrl_array[0];

	for (i = 0; i < controls.count; i++) {
		ctrl = &controls.controls[i];
		PRINT_DEBUG("Attempting to set tsmux control:-\n");
		PRINT_DEBUG("Setting control id = %d\n", ctrl->id);
		PRINT_DEBUG("Size = %d\n", ctrl->size);
		if (ctrl->size != 0)
			PRINT_DEBUG("Value = %s\n", ctrl->string);
		else
			PRINT_DEBUG("Value = %d\n", ctrl->value);
	}
	if (-1 == xioctl(fdin[TsmuxId], VIDIOC_S_EXT_CTRLS, &controls))
		errno_exit("VIDIOC_S_EXT_CTRLS");
	return;
}

static void InitialiseTsmuxUserPtr(unsigned int TsmuxId)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = NUM_TSMUX_BUFFERS;
	req.type = V4L2_BUF_TYPE_DVB_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	PRINT_DEBUG("Doing VIDIOC_REQBUFS for fdin\n");
	if (-1 == xioctl(fdin[TsmuxId], VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s input does not support "
				"user pointer i/o\n", dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	return;
}

static void CheckTsmuxCapabilities(void)
{
	struct v4l2_capability cap;

	if (-1 == xioctl(fdin[0], VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n", dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_DVB_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n", dev_name);
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n",
			dev_name);
		exit(EXIT_FAILURE);
	}
	return;
}

static void ConfigureTsmux(unsigned int TsmuxId)
{
	CheckTsmuxCapabilities();
	if (TsmuxId == 0) {
		InitialiseTsmuxUserPtr(TsmuxId);
		SetTsmuxParameters(TsmuxId);
	}

	return;
}

static void OpenTsmux(unsigned int TsmuxId, unsigned int Type)
{
	struct v4l2_format format;

	fdin[TsmuxId] = open(dev_name, O_RDWR);
	if (-1 == fdin[TsmuxId]) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n",
			dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (Type == V4L2_DVB_FMT_TS || Type == V4L2_DVB_FMT_DLNA_TS) {
		/* Set capture format */
		format.type = V4L2_BUF_TYPE_DVB_CAPTURE;
		format.fmt.dvb.data_type = Type;
		format.fmt.dvb.buffer_size = TSMUX_BUFFER_SIZE;
		if (xioctl(fdin[TsmuxId], VIDIOC_S_FMT, &format) < 0) {
			errno_exit("ERROR: VIDIOC_S_FMT -- > CAPTURE");
		}
	}

	return;
}

static void CloseOutput(void)
{
	fclose(outfp);
}

static void OpenOutput(void)
{
	outfp = fopen(out_name, "wb");
	if (outfp == NULL) {
		fprintf(stderr, "Can't open output file %s\n", out_name);
		exit(EXIT_FAILURE);
	}
}

static void Initialise(void)
{
	memset(StreamBufferInUse, 0x00, MAX_STREAMS *
	       MAX_BUFFERS_PER_STREAM * sizeof(bool));
	memset(StreamBufferMemory, 0xa5, MAX_STREAMS *
	       MAX_BUFFERS_PER_STREAM * STREAM_BUFFER_SIZE *
	       sizeof(unsigned char));
	memset(StreamNextBuffer, 0x00, MAX_STREAMS * sizeof(int));
	memset(TsmuxMemory, 0x00, NUM_TSMUX_BUFFERS * TSMUX_BUFFER_SIZE *
	       sizeof(unsigned char));
	memset(StreamUsed, 0x00, MAX_STREAMS * sizeof(bool));
}

static char *content_type_to_str(enum content_types content_type)
{
	char *ret;

	switch (content_type) {
	case CONTENT_TYPE_ES:
		ret = "ES";
		break;
	case CONTENT_TYPE_PES:
		ret = "PES";
		break;
	default:
		fprintf(stderr, "ERROR: Invalid content type %d\n",
			content_type);
		ret = "";
		break;
	}
	return ret;
}

static void usage(FILE * fp, int argc, char **argv)
{
	fprintf(fp,
		"Usage: %s [options]\n\n"
		"Options:\n"
		"-h | --help                  Print this message\n"
		"-b | --bad <skip>            Insert bad frames every [%d] frames\n"
		"-d | --device <name>         Video device name [%s]\n"
		"-o | --output <file>         Outputs stream to file [%s]\n"
		"-c | --count  <frames>       Number of frames to grab [%i]\n"
		"-t | --type <ES|PES>         Content type [%s]\n"
		"-v | --video file name         video file name [%s]\n"
		"-a | --audio file name         video file name [%s]\n"
		"-i | --check indexing        	Run Indexing \n"
		"",
		argv[0], insert_bad_every, dev_name, out_name,
		frame_count, content_type_to_str(content_type), in_name,
		in_name);
}

static int ProcessArguments(int argc, char **argv)
{
	for (;;) {
		int idx;
		int c;

		c = getopt_long(argc, argv, short_options, long_options, &idx);

		if (-1 == c)
			break;

		switch (c) {
		case 0:	/* getopt_long() flag */
			break;
		case 'a':
			/*in_name = optarg; */
			audio_stream = 0;
			printf("audio not yet supported in this testapp %s \n",
			       in_name);
			break;
		case 'd':
			dev_name = optarg;
			break;
		case 'b':
			errno = 0;
			insert_bad_every = strtol(optarg, NULL, 0);
			if (errno)
				errno_exit(optarg);
			break;
		case 'h':
			usage(stdout, argc, argv);
			exit(EXIT_SUCCESS);
		case 'o':
			out_name = optarg;
			break;
		case 'c':
			errno = 0;
			frame_count = strtol(optarg, NULL, 0);
			if (errno)
				errno_exit(optarg);
			break;
		case 't':
			if (strcasecmp(optarg, "ES") == 0)
				content_type = CONTENT_TYPE_ES;
			else if (strcasecmp(optarg, "PES") == 0)
				content_type = CONTENT_TYPE_PES;
			else {
				fprintf(stderr,
					"ERROR: Invalid content type %s\n",
					optarg);
				usage(stderr, argc, argv);
				exit(EXIT_FAILURE);
			}
			break;
		case 'v':
			in_name = optarg;
			video_stream = 1;
			break;
		case 'i':
			check_indexing = 1;
			break;
		default:
			usage(stderr, argc, argv);
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}

void *IndexReader(void *arg)
{
	struct v4l2_enc_idx tsmux_idx;
	int j = 0;
	int i = 0;

	do {
		/*Avoid calling it continously.. */
		if (j == 10000) {
			memset(&tsmux_idx, 0, sizeof(struct v4l2_enc_idx));
			ioctl(fdin[1], VIDIOC_G_ENC_INDEX, &tsmux_idx);
			for (i = 0; i < tsmux_idx.entries; i++) {
				PRINT_DEBUG
				    ("entries=%d entries_cap = %d entry[%d].offset = %lld entry[%d].reserved[0] = %d entry[%d].flags = %d  entry[%d].pts = 0x%llx\n",
				     tsmux_idx.entries, tsmux_idx.entries_cap,
				     i, tsmux_idx.entry[i].offset, i,
				     tsmux_idx.entry[i].reserved[0], i,
				     tsmux_idx.entry[i].flags, i,
				     tsmux_idx.entry[i].pts);
			}
			j = 0;
		}
		j++;
	} while (running);
	return NULL;
}

void *Reader(void *arg)
{
	while (running) {
		if (GetMultiplexedData(false)) {
			fprintf(stderr, "Error: multiplexer failed!!\n");
			exit(EXIT_FAILURE);
		}
	}
	/* Flush any remaining output */
	if (GetMultiplexedData(true)) {
		fprintf(stderr, "Error: Multiplexer failed while flushing!!\n");
		exit(EXIT_FAILURE);
	}
	return NULL;
}

int main(int argc, char **argv)
{
	unsigned long long DTS;
	unsigned int StreamId;
	unsigned int Size;
	pthread_t readThread;
	pthread_t IndexThread;
	unsigned int Index;
	FILE *rawfp = NULL;
	int ret = 0;
	unsigned long long bytes_following = 0;
	unsigned int framenum = 0;

	ProcessArguments(argc, argv);

	printf("=======>Starting the tsmux test.\n");

	rawfp = fopen(in_name, "rb");
	if (!rawfp) {
		fprintf(stderr, "Error: open of %s failed\n", in_name);
		exit(EXIT_FAILURE);
	}

	Initialise();
	OpenOutput();
	OpenTsmux(0, V4L2_DVB_FMT_TS);
	if (check_indexing) {
		printf("=======>Check Indexing...\n");
		OpenTsmux(1, 0);
	}
	ConfigureTsmux(0);

	if (video_stream) {
		StreamId = 0;
		AddAStream(0, V4L2_STM_TSMUX_STREAM_TYPE_VIDEO_H264, 14000000);
	} else if (audio_stream) {
		/*StreamId = 1; */
		/*AddAStream(1, V4L2_STM_TSMUX_STREAM_TYPE_PS_AUDIO_AC3, 2*1024*1024); */
	} else {
		fprintf(stderr, "Error: no stream added \n");
		exit(EXIT_FAILURE);
	}

	/* Can't start Tsmux until all streams are added */
	StartTsmux();

	DTS = 0;

	ret = pthread_create(&readThread, NULL, Reader, NULL);
	if (ret != 0) {
		fprintf(stderr, "Unable to spin demux reader thread %d\n", ret);
		goto done;
	}

	if (check_indexing) {
		ret = pthread_create(&IndexThread, NULL, IndexReader, NULL);
		if (ret != 0) {
			fprintf(stderr,
				"Unable to spin demux Indexreader thread %d\n",
				ret);
			goto done;
		}
	}
	/* Video only */

	while (1) {
		ret = fread(&DTS, sizeof(DTS), 1, rawfp);
		if (ret != 1)
			break;

		/* Convert DTS at 1000kHz (usec) to DTS at 90kHz */
		DTS *= 9;
		DTS /= 100;

		ret = fread(&bytes_following, sizeof(bytes_following), 1,
			    rawfp);
		if (ret != 1)
			break;

		if (bytes_following > STREAM_BUFFER_SIZE) {
			fprintf(stderr,
				"ERROR: Frame is larger than stream buffer %llu > %d\n",
				bytes_following, STREAM_BUFFER_SIZE);
			exit(EXIT_FAILURE);
		}

		/* Get an available buffer */
		Index = GetNextStreamBuffer(StreamId);

		/*PrepareStreamBuffer(StreamId, Index, Length, DTS); */
		memset(&StreamBufferMemory[StreamId][Index][0], 0xde,
		       bytes_following);

		ret = fread(&StreamBufferMemory[StreamId][Index][0], 1,
			    bytes_following, rawfp);
		if (ret == EOF || ret < bytes_following) {
			fprintf(stderr,
				"!!! error reading data (ret=%d, bytes_following=%llu)\n",
				ret, bytes_following);
			break;
		}

		Size = bytes_following;

		/* Inject a BAD DTS value every specified number of frames,
		 * ignoring the first */
		if (insert_bad_every && framenum &&
		    ((framenum % insert_bad_every) == 0))
			DTS = 0x1ffffffff;

		SendStreamBuffer(StreamId, Index, Size, DTS, false);

		framenum++;
		if (frame_count && (framenum == frame_count))
			break;

	}

	fprintf(stderr,
		"!!! Out of file reading loop, stopping demux reader!\n");
	running = false;
	pthread_join(readThread, NULL);
	if (check_indexing)
		pthread_join(IndexThread, NULL);
	printf("======>pthread_join-Done<====== \n");

done:
	StopTsmux();
	CloseOutput();

	printf("===>End of test<===\n");
	return 0;
}
