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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <directfb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <getopt.h>

#include <linux/videodev2.h>

#include "utils/dfb_helpers.h"
#include "utils/v4l2_helper.h"
#include "linux/include/linux/dvb/dvb_v4l2_export.h"

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
#define STREAM_BUFFER_SIZE (64*1024)

#define NUM_TSMUX_BUFFERS 2
#define TSMUX_BUFFER_SIZE ((MUX_BITRATE) / (90000 / PCR_PERIOD))

static unsigned char
    StreamBufferMemory[MAX_STREAMS][MAX_BUFFERS_PER_STREAM][STREAM_BUFFER_SIZE];
static bool StreamBufferInUse[MAX_STREAMS][MAX_BUFFERS_PER_STREAM];
static int StreamNextBuffer[MAX_STREAMS];
static unsigned int StreamTypes[MAX_STREAMS];
static bool StreamUsed[MAX_STREAMS];

static bool TsmuxBufferInUse[NUM_TSMUX_BUFFERS];
static unsigned char TsmuxMemory[NUM_TSMUX_BUFFERS][TSMUX_BUFFER_SIZE];

static const char short_options[] = "d:ho:c:t:";

static const struct option
 long_options[] = {
	{"device", required_argument, NULL, 'd'},
	{"help", no_argument, NULL, 'h'},
	{"output", required_argument, NULL, 'o'},
	{"count", required_argument, NULL, 'c'},
	{"type", required_argument, NULL, 't'},
	{0, 0, 0, 0}
};

static char *dev_name;
static char *out_name;
FILE *outfp;
static int frame_count = 10;
static int fdin = -1;
static int fdout[MAX_STREAMS];
static unsigned char dataval = 0;
enum content_types {
	CONTENT_TYPE_ES = 0,
	CONTENT_TYPE_PES,
};
static enum content_types content_type = CONTENT_TYPE_ES;

/* Function prototypes */
static void SendStreamBuffer(unsigned int StreamId,
			     unsigned int Length,
			     unsigned long long DTS, bool end_of_stream);

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

static unsigned int GetNextStreamBuffer(unsigned int StreamId)
{
	int i;

	i = StreamNextBuffer[StreamId];
	StreamNextBuffer[StreamId]++;
	if (StreamNextBuffer[StreamId] >= MAX_BUFFERS_PER_STREAM)
		StreamNextBuffer[StreamId] = 0;

	return i;
}

static void QueueMuxBuffer(int num)
{
	struct v4l2_buffer buf;

	printf("%s\n", __func__);
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

	if (-1 == xioctl(fdin, VIDIOC_QBUF, &buf))
		errno_exit("VIDIOC_QBUF");

	TsmuxBufferInUse[num] = true;
	return;
}

static void ProcessMuxBuffer(int bufnum, int bytesused)
{
	fwrite(&TsmuxMemory[bufnum][0], bytesused, 1, outfp);
	//fflush(outfp);

}

static void DequeueMuxBuffer(int *bufnum, int *bytesused)
{
	struct v4l2_buffer buf;
	unsigned int i = 0;
	unsigned int found = 0;

	printf("%s\n", __func__);
	*bufnum = -1;
	*bytesused = -1;

	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_DVB_CAPTURE;
	buf.memory = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl(fdin, VIDIOC_DQBUF, &buf)) {
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
	// Check and mark the relevant buffer as no longer in use
	// TODO Can use the buf.index field to find the buffer
	for (i = 0; i < NUM_TSMUX_BUFFERS; i++) {
		if (TsmuxBufferInUse[i]) {
			if (buf.m.userptr == (unsigned long)&TsmuxMemory[i][0]) {
				printf("Dequeued mux buffer %d\n", i);
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

	printf("%s\n", __func__);
	fds.fd = fdin;
	fds.events = (POLLIN);

	/* Mux buffers are ready to be dequeued if read is ready */
	/* NOTE: Using poll instead of select because select returns a valid
	 * set fds value on POLLERR for some reason so when the queue is empty
	 * it looks like there is a buffer to be dequeued.
	 */
	r = poll(&fds, 1, 0);
	//printf("Poll returned %d\n", r);
	if (r < 0)
		errno_exit("Poll\n");
	if (r > 0) {
		//printf("Poll revents = 0x%x\n", fds.revents);
		if ((fds.revents & POLLERR) || (fds.revents & POLLHUP) ||
		    (fds.revents & POLLNVAL)) {
			//printf("Poll errors\n");
			return false;
		}
		if (fds.revents & POLLIN) {
			printf("POLLIN Set\n");
			return true;
		}
		return true;
	} else {
		//printf("Poll is zero\n");
		return false;
	}
}

static void PrepareEndOfStreamBuffer(unsigned int StreamId, unsigned int Index)
{
	unsigned int *Data =
	    (unsigned int *)&StreamBufferMemory[StreamId][Index][0];

	Data[0] = V4L2_STM_TSMUX_END_OF_STREAM;
}

static void TerminateAllStreams(void)
{
	int i;

	for (i = 0; i < MAX_STREAMS; i++) {
		if (StreamUsed[i]) {
			SendStreamBuffer(i, sizeof(unsigned int), 0ULL, true);
		}
	}
	return;
}

static void DequeueRemainingMuxBuffers(void)
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
			printf("Dequeued mux buffer %d bytesoutput %d\n",
			       bufnum, bytesoutput);
			if (bytesoutput == 0) {
				printf("Last mux buffer was 0, stopping\n");
				stop = true;
				if (!flushing)
					ret = -1;
				else
					DequeueRemainingMuxBuffers();
			} else {
				printf("Processing mux buffer with %d bytes\n",
				       bytesoutput);
				ProcessMuxBuffer(bufnum, bytesoutput);
				QueueMuxBuffer(bufnum);
			}
		} else {
			stop = true;
		}
	} while (stop == false);
	return ret;
}

static void QueueStreamBuffer(unsigned int StreamId,
			      unsigned int Index,
			      unsigned int Length, unsigned long long DTS)
{
	struct v4l2_buffer buf;
//      unsigned int *vbuf;

	printf("%s\n", __func__);

	CLEAR(buf);
	buf.index = Index;
	buf.type = V4L2_BUF_TYPE_DVB_OUTPUT;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.m.userptr = (unsigned long)&StreamBufferMemory[StreamId][Index][0];
	buf.length = STREAM_BUFFER_SIZE;
	buf.bytesused = Length;
	buf.timestamp.tv_usec = DTS;

	if (-1 == xioctl(fdout[StreamId], VIDIOC_QBUF, &buf))
		errno_exit("VIDIOC_QBUF");
	return;
}

static void PrepareStreamBuffer(unsigned int StreamId,
				unsigned int Index,
				unsigned int Length, unsigned long long DTS)
{
	unsigned int i;
	unsigned int offset = 0;
	unsigned char *Data = &StreamBufferMemory[StreamId][Index][0];

	printf("%s\n", __func__);
	if (content_type == CONTENT_TYPE_PES) {
		bool Video =
		    (StreamTypes[StreamId] == VIDEO_TYPE ? true : false);
		// Add PES header to buffer
		Data[0] = 0x00;
		Data[1] = 0x00;
		Data[2] = 0x01;
		Data[3] = (Video ? 0xe0 : 0xc0);
		Data[4] = ((Length - 8) >> 8) & 0xff;
		Data[5] = ((Length - 8) & 0xff);
		Data[6] = 0x80;
		Data[7] = 0x80;
		Data[8] = 0x05;
		Data[9] = ((DTS >> 29) & 0x0e) | 1 | 0x20;
		Data[10] = ((DTS >> 22) & 0xff);
		Data[11] = ((DTS >> 14) & 0xfe) | 1;
		Data[12] = ((DTS >> 7) & 0xff);
		Data[13] = ((DTS << 1) & 0xfe) | 1;
		offset = 14;
	}
	/* Fill the buffer with an incrementing count */
	for (i = offset; i < Length; i++) {
		Data[i] = dataval++;
	}

	return;
}

static int DequeueStreamBuffer(unsigned int StreamId, unsigned int Index)
{
	struct v4l2_buffer buf;
	unsigned int i = 0;
	unsigned int found = 0;
	int ret = 0;

	printf("%s\n", __func__);
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
	// Mark the relevant buffer as no longer in use and remove from
	// the StreamBuffersInUse array
	// TODO Can use the buf.index to find the relevant buffer
	for (i = 0; i < MAX_BUFFERS_PER_STREAM; i++) {
		if (StreamBufferInUse[StreamId][i]) {
			if (buf.m.userptr ==
			    (unsigned long)&StreamBufferMemory[StreamId][i][0])
			{
				StreamBufferInUse[StreamId][i] = false;
				found = 1;
			}
		}
	}
	if (buf.index != Index) {
		fprintf(stderr,
			"ERROR: Last dequeued buffer index %d not expected value %d\n",
			buf.index, Index);
		ret = -1;
	}
	if (found == 0) {
		fprintf(stderr,
			"ERROR: Can't find dequeued buffer %d on Stream %d, expected %d\n",
			buf.index, StreamId, Index);
		ret = -1;
	}
	StreamBufferInUse[StreamId][Index] = false;
	return ret;
}

static void SendStreamBuffer(unsigned int StreamId,
			     unsigned int Length,
			     unsigned long long DTS, bool end_of_stream)
{
	unsigned int Index;

	printf("%s\n", __func__);
	/* Get an available buffer */
	Index = GetNextStreamBuffer(StreamId);

	if (StreamBufferInUse[StreamId][Index]) {
		if (DequeueStreamBuffer(StreamId, Index) != 0) {
			fprintf(stderr,
				"ERROR: Can't dequeue previously queued buffer\n");
			exit(EXIT_FAILURE);
		}
	}
	StreamBufferInUse[StreamId][Index] = true;
	if (!end_of_stream) {
		PrepareStreamBuffer(StreamId, Index, Length, DTS);
		printf
		    ("Sending buffer stream=%d, index=%d, addr=0x%lx, Len=%d, DTS = 0x%010llx\n",
		     StreamId, Index,
		     (unsigned long)&StreamBufferMemory[StreamId][Index][0],
		     Length, DTS);
	} else {
		PrepareEndOfStreamBuffer(StreamId, Index);
		printf
		    ("Send end stream buffer stream=%d, index=%d, addr=0x%lx, Len=%d\n",
		     StreamId, Index,
		     (unsigned long)&StreamBufferMemory[StreamId][Index][0],
		     Length);
	}
	QueueStreamBuffer(StreamId, Index, Length, DTS);

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
		 .id = V4L2_CID_STM_TSMUX_STREAM_IS_PES,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is ES */
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
		 .id = V4L2_CID_STM_TSMUX_STREAM_TYPE,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is VIDEO_MPEG2 */
		 .value = Type,
		 },
//                      {
//                                      .id = V4L2_CID_STM_TSMUX_STREAM_DESCRIPTOR,
//                                      .size = V4L2_STM_TSMUX_MAX_STRING,
//                                      .reserved2 = { 0 },
//                                      /* No default */
//                                      .string = "AAAAA",
//                      },
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

	printf("%s\n", __func__);
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
		if (ctrl->id == V4L2_CID_STM_TSMUX_STREAM_IS_PES) {
			switch (content_type) {
			case CONTENT_TYPE_ES:
				ctrl->value = 0;
				break;
			case CONTENT_TYPE_PES:
				ctrl->value = 1;
				break;
			default:
				fprintf(stderr,
					"ERROR: Unknown content_type value!!\n");
				exit(EXIT_FAILURE);
				break;
			}
		}
		printf("Attempting to set stream %d control:-\n", StreamId);
		printf("Setting control id = %d\n", ctrl->id);
		printf("Size = %d\n", ctrl->size);
		if (ctrl->size != 0)
			printf("Value = %s\n", ctrl->string);
		else
			printf("Value = %d\n", ctrl->value);
	}
	if (-1 == xioctl(fdout[StreamId], VIDIOC_S_EXT_CTRLS, &controls))
		errno_exit("VIDIOC_S_EXT_CTRLS");

}

static void InitialiseStreamBuffersInUse(unsigned int StreamId)
{
	int i;

	printf("%s\n", __func__);
	for (i = 0; i < MAX_BUFFERS_PER_STREAM; i++) {
		StreamBufferInUse[StreamId][i] = false;
	}
	StreamNextBuffer[StreamId] = 0;
	return;
}

static void InitialiseStreamOutUserPtr(unsigned int StreamId)
{
	struct v4l2_requestbuffers req;

	printf("%s\n", __func__);
	CLEAR(req);

	req.count = MAX_BUFFERS_PER_STREAM;
	req.type = V4L2_BUF_TYPE_DVB_OUTPUT;
	req.memory = V4L2_MEMORY_USERPTR;

	printf("Doing VIDIOC_REQBUFS for fdout[%d]\n", StreamId);
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

	printf("%s\n", __func__);
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

static void OpenStreamOut(unsigned int StreamId)
{
	struct v4l2_format format;

	printf("%s\n", __func__);
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

	if (ioctl(fdout[StreamId], VIDIOC_S_FMT, &format) < 0) {
		errno_exit("ERROR : VIDIOC_S_FMT --> OUTPUT");
	}
	return;

}

static void AddAStream(unsigned int StreamId,
		       unsigned int Type, unsigned int BitBufferSize)
{
	printf("%s\n", __func__);
	OpenStreamOut(StreamId);
	CheckStreamOutCapabilities(StreamId);
	InitialiseStreamOutUserPtr(StreamId);
	InitialiseStreamBuffersInUse(StreamId);
	SetStreamOutParameters(StreamId, Type, BitBufferSize);
	StartStreamStreaming(StreamId);
	StreamUsed[StreamId] = true;

}

static void AddAProgram(void)
{
	struct v4l2_ext_controls controls;
	struct v4l2_ext_control ctrl_array[] = {
//                      {
//                                      .id = V4L2_CID_STM_TSMUX_PROGRAM_NUMBER,
//                                      .size = 0,
//                                      .reserved2 = { 0 },
//                                      /* Default is 1 */
//                                      .value = 1,
//                      },
//                      {
//                                      .id = V4L2_CID_STM_TSMUX_PMT_PID,
//                                      .size = 0,
//                                      .reserved2 = { 0 },
//                                      /* Default is 0x31 */
//                                      .value = 0x31,
//                      },
//                      {
//                                      .id = V4L2_CID_STM_TSMUX_SDT_PROV_NAME,
//                                      .size = V4L2_STM_TSMUX_MAX_STRING,
//                                      .reserved2 = { 0 },
//                                      /* Default is "Unknown" */
//                                      .string = "BBBBB",
//                      },
//                      {
//                                      .id = V4L2_CID_STM_TSMUX_SDT_SERV_NAME,
//                                      .size = V4L2_STM_TSMUX_MAX_STRING,
//                                      .reserved2 = { 0 },
//                                      /* Default is "TSMux 1" */
//                                      .string = "CCCCC",
//                      },
//                      {
//                                      .id = V4L2_CID_STM_TSMUX_PMT_DESCRIPTOR,
//                                      .size = V4L2_STM_TSMUX_MAX_STRING,
//                                      .reserved2 = { 0 },
//                                      /* There is no default */
//                                      .string = "DDDD",
//                      },
	};
	printf("%s\n", __func__);
	int i;
	struct v4l2_ext_control *ctrl;

	printf("%s\n", __func__);

	controls.ctrl_class = V4L2_CTRL_CLASS_TSMUX;
	controls.count = sizeof(ctrl_array) / sizeof(struct v4l2_ext_control);
	if (controls.count == 0)
		return;
	controls.reserved[0] = 0;
	controls.reserved[1] = 0;
	controls.controls = &ctrl_array[0];

	for (i = 0; i < controls.count; i++) {
		ctrl = &controls.controls[i];
		printf("Attempting to set tsmux control:-\n");
		printf("Setting control id = %d\n", ctrl->id);
		printf("Size = %d\n", ctrl->size);
		if (ctrl->size != 0)
			printf("Value = %s\n", ctrl->string);
		else
			printf("Value = %d\n", ctrl->value);
	}
	if (-1 == xioctl(fdin, VIDIOC_S_EXT_CTRLS, &controls))
		errno_exit("VIDIOC_S_EXT_CTRLS");
	return;
}

static void StartMuxStreaming(void)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_DVB_CAPTURE;

	if (-1 == xioctl(fdin, VIDIOC_STREAMON, &type))
		errno_exit("VIDIOC_STREAMON");

	return;
}

static void StopMuxStreaming(void)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_DVB_CAPTURE;

	if (-1 == xioctl(fdin, VIDIOC_STREAMOFF, &type))
		errno_exit("VIDIOC_STREAMOFF");

	return;
}

static void StartTsmux(void)
{
	int i;

	// Queue the tsmux memory buffers
	for (i = 0; i < NUM_TSMUX_BUFFERS; i++) {
		QueueMuxBuffer(i);
	}
	// Start streaming on the tsmux
	StartMuxStreaming();
}

static void StopTsmux(void)
{
	int i;

	// Stop streaming on all streams
	for (i = 0; i < MAX_STREAMS; i++) {
		if (StreamUsed[i]) {
			StopStreamStreaming(i);
		}
	}
	// Check the tsmux memory buffers are not in use
	for (i = 0; i < NUM_TSMUX_BUFFERS; i++) {
		if (TsmuxBufferInUse[i]) {
			fprintf(stderr, "Warning: Mux buffer %d still in use\n",
				i);
			//exit(EXIT_FAILURE);
		}
	}
	// Stop streaming on the tsmux
	StopMuxStreaming();
}

static void SetTsmuxParameters(void)
{
	struct v4l2_ext_controls controls;
	struct v4l2_ext_control ctrl_array[] = {
		{
		 .id = V4L2_CID_STM_TSMUX_PKT_TYPE,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is DVB */
		 .value = V4L2_STM_TSMUX_PKT_TYPE_DVB,
		 },
		{
		 .id = V4L2_CID_STM_TSMUX_PCR_PERIOD,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 4500 */
		 .value = PCR_PERIOD,
		 },
		{
		 .id = V4L2_CID_STM_TSMUX_GEN_PCR_STREAM,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 1 */
		 .value = 1,
		 },
//                      {
//                                      .id = V4L2_CID_STM_TSMUX_PCR_PID,
//                                      .size = 0,
//                                      .reserved2 = { 0 },
//                                      /* Default is 0x1ffe */
//                                      .value = 0x1ffe,
//                      },
		{
		 .id = V4L2_CID_STM_TSMUX_TABLE_GEN,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is PAT_PMT + SDT */
		 .value = (V4L2_STM_TSMUX_TABLE_GEN_PAT_PMT |
			   V4L2_STM_TSMUX_TABLE_GEN_SDT),
		 },
//                      {
//                                      .id = V4L2_CID_STM_TSMUX_TABLE_PERIOD,
//                                      .size = 0,
//                                      .reserved2 = { 0 },
//                                      /* Default is same as pcr_period */
//                                      .value = 4500,
//                      },
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
	printf("%s\n", __func__);
	int i;
	struct v4l2_ext_control *ctrl;

	printf("%s\n", __func__);

	controls.ctrl_class = V4L2_CTRL_CLASS_TSMUX;
	controls.count = sizeof(ctrl_array) / sizeof(struct v4l2_ext_control);
	if (controls.count == 0)
		return;
	controls.reserved[0] = 0;
	controls.reserved[1] = 0;
	controls.controls = &ctrl_array[0];

	for (i = 0; i < controls.count; i++) {
		ctrl = &controls.controls[i];
		printf("Attempting to set tsmux control:-\n");
		printf("Setting control id = %d\n", ctrl->id);
		printf("Size = %d\n", ctrl->size);
		if (ctrl->size != 0)
			printf("Value = %s\n", ctrl->string);
		else
			printf("Value = %d\n", ctrl->value);
	}
	if (-1 == xioctl(fdin, VIDIOC_S_EXT_CTRLS, &controls))
		errno_exit("VIDIOC_S_EXT_CTRLS");
	return;
}

static void InitialiseTsmuxUserPtr(void)
{
	struct v4l2_requestbuffers req;

	printf("%s\n", __func__);
	CLEAR(req);

	req.count = NUM_TSMUX_BUFFERS;
	req.type = V4L2_BUF_TYPE_DVB_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	printf("Doing VIDIOC_REQBUFS for fdin\n");
	if (-1 == xioctl(fdin, VIDIOC_REQBUFS, &req)) {
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

	printf("%s\n", __func__);
	if (-1 == xioctl(fdin, VIDIOC_QUERYCAP, &cap)) {
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

static void ConfigureTsmux(void)
{
	printf("%s\n", __func__);
	CheckTsmuxCapabilities();
	InitialiseTsmuxUserPtr();
	SetTsmuxParameters();

	return;
}

static void OpenTsmux(void)
{
	struct stat st;
	struct v4l2_format format;

	printf("Opening %s\n", dev_name);
	if (-1 == stat(dev_name, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n",
			dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", dev_name);
		exit(EXIT_FAILURE);
	}

	fdin = open(dev_name, O_RDWR);
	if (-1 == fdin) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n",
			dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Set capture format */
	format.type = V4L2_BUF_TYPE_DVB_CAPTURE;
	format.fmt.dvb.data_type = V4L2_DVB_FMT_TS;
	format.fmt.dvb.buffer_size = TSMUX_BUFFER_SIZE;

	if (xioctl(fdin, VIDIOC_S_FMT, &format) < 0) {
		errno_exit("ERROR: VIDIOC_S_FMT -- > CAPTURE");
	}
	return;

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
		"-d | --device <name>         Video device name [%s]\n"
		"-o | --output <file>         Outputs stream to file [%s]\n"
		"-c | --count  <frames>       Number of frames to grab [%i]\n"
		"-t | --type <ES|PES>         Content type [%s]\n"
		"",
		argv[0], dev_name, out_name, frame_count,
		content_type_to_str(content_type));
}

static int ProcessArguments(int argc, char **argv)
{
	printf("%s\n", __func__);
	for (;;) {
		int idx;
		int c;

		c = getopt_long(argc, argv, short_options, long_options, &idx);

		if (-1 == c)
			break;

		switch (c) {
		case 0:	/* getopt_long() flag */
			break;
		case 'd':
			dev_name = optarg;
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
		default:
			usage(stderr, argc, argv);
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	unsigned int i;
	unsigned long long DTS;
	unsigned long long AudioDTS;
	unsigned long long VideoDTS;
	unsigned int StreamId;
	unsigned int Size;

	dev_name = "/dev/video0";
	out_name = "output.ts";

	ProcessArguments(argc, argv);

	printf("Welcome to tsmux test.\n");

	Initialise();
	OpenOutput();
	OpenTsmux();
	ConfigureTsmux();
	AddAProgram();

	AddAStream(0, VIDEO_TYPE, 18 * 1024 * 1024);
	AddAStream(1, AUDIO_TYPE, 2 * 1024 * 1024);

	/* Can't start Tsmux until all streams are added */
	StartTsmux();

	VideoDTS = 0;
	AudioDTS = 0;

	// 60V + 24A frames per second, 60 seconds worth
//      for(i = 0; i < (84 * 60); i++) {
//      for(i = 0; i < 2; i++) {
	for (i = 0; i < frame_count; i++) {
		/* Determine what we are injecting */
		if (VideoDTS <= AudioDTS) {
			StreamId = 0;
			Size = 36 * 1024;	// 36k
			DTS = VideoDTS;
			VideoDTS += 90000 / 60;
		} else {
			StreamId = 1;
			Size = 10 * 1024;	// 36k
			DTS = AudioDTS;
			AudioDTS += 90000 / 24;
		}

		SendStreamBuffer(StreamId, Size, DTS, false);
		/* Get any output if available */
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

	StopTsmux();
	CloseOutput();

	printf("End of test\n");
	return 0;
}
