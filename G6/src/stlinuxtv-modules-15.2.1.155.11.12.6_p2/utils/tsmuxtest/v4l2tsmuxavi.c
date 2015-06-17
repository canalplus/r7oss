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

/******************************
* 		Start of AVI header bits
******************************/
#define AVI_HASINDEX        0x00000010	/* Index at end of file? */
#define AVI_MUSTUSEINDEX    0x00000020
#define AVI_ISINTERLEAVED   0x00000100
#define AVI_TRUSTCKTYPE     0x00000800	/* Use CKType to find key frames */
#define AVI_WASCAPTUREFILE  0x00010000
#define AVI_COPYRIGHTED     0x00020000

typedef struct AviHeader_s {
	/* 32 bits - 'avih' */
	/* 32 bits - Size of this structure (bytes) */
	unsigned int MicroSecondsPerFrame;	/* 32 bits - frame display rate (or 0) */
	unsigned int MaxBytesPerSec;	/* 32 bits - max. transfer rate */
	unsigned int PaddingGranularity;	/* 32 bits - pad to multiples of this size; normally 2K. */
	unsigned int Flags;	/* 32 bits - flags (AVI_abc) */
	unsigned int TotalFrames;	/* 32 bits - # frames in first movi list */
	unsigned int InitialFrames;
	unsigned int Streams;
	unsigned int SuggestedBufferSize;
	unsigned int Width;
	unsigned int Height;
	unsigned int Reserved[4];
} AviHeader_t;

#define SH_DISABLED          0x00000001
#define SH_VIDEO_PALCHANGES  0x00010000

typedef struct StreamHeader_s {
	/* 32 bits - 'avih' */
	/* 32 bits - Size of this structure (bytes) */
	unsigned int Type;	/* stream type codes */
	unsigned int Handler;
	unsigned int Flags;
	unsigned short int Priority;
	unsigned short int Language;
	unsigned int InitialFrames;
	unsigned int Scale;
	unsigned int Rate;	/* dwRate/dwScale is stream tick rate in ticks/sec */
	unsigned int Start;
	unsigned int Length;
	unsigned int SuggestedBufferSize;
	unsigned int Quality;
	unsigned int SampleSize;
	struct {
		unsigned short int Left;
		unsigned short int Top;
		unsigned short int Right;
		unsigned short int Bottom;
	} Frame;
} StreamHeader_t;

typedef struct StreamFormatAudio_s {
	unsigned short FormatTag;
	unsigned short NumberOfChannels;
	unsigned int SamplesPerSecond;
	unsigned int AverageBytesPerSecond;
	unsigned short BlockAlign;
	unsigned short BitsPerSample;
	unsigned short Size;
} StreamFormatAudio_t;

typedef struct StreamFormatVideo_s {
	unsigned int Size;
	unsigned int Width;
	unsigned int Height;
	unsigned short Planes;
	unsigned short BitCount;
	unsigned int Compression;
	unsigned int SizeImage;
	unsigned int XPelsPerMeter;
	unsigned int YPelsPerMeter;
	unsigned int ClrUsed;
	unsigned int ClrImportant;
} StreamFormatVideo_t;

AviHeader_t AviHeader;

#define CodeToInteger(a,b,c,d)	((a << 0) | (b << 8) | (c << 16) | (d << 24))

/******************************
* 		Start of tsmux header bits
******************************/

/* Global variables*/

#define MAX_SUPPORTED_STREAMS	8
#define PES_HEADER_SIZE 14

#define MAX_TSMUXES 2

#define MAX_BUFFERS_PER_STREAM 32

#define STREAM_BUFFER_SIZE	(1024 * 1024)

#define MUX_BITRATE 48*1024*1024
#define PCR_PERIOD		4500

#define NUM_TSMUX_BUFFERS 2
#define TSMUX_BUFFER_SIZE ((MUX_BITRATE) / (90000 / PCR_PERIOD))

static FILE *Input;
static FILE *Output;

typedef struct Stream_s {
	StreamHeader_t StreamHeader;
	StreamFormatVideo_t StreamFormatVideo;
	StreamFormatAudio_t StreamFormatAudio;

	unsigned int BitBufferSize;
	unsigned int Type;

	unsigned int CurrentBuffer;
	unsigned int BufferCount;

	unsigned int FrameCount;
	unsigned long long DTS;
	unsigned long long AdjustedDTS;
	unsigned long long DTSAdjustment;
} Stream_t;

static unsigned int StreamCount;
static Stream_t Streams[MAX_SUPPORTED_STREAMS];

static unsigned char
    StreamBufferMemory[MAX_SUPPORTED_STREAMS][MAX_BUFFERS_PER_STREAM]
    [STREAM_BUFFER_SIZE];
static bool StreamBufferInUse[MAX_SUPPORTED_STREAMS][MAX_BUFFERS_PER_STREAM];
static int StreamNextBuffer[MAX_SUPPORTED_STREAMS];
static bool StreamUsed[MAX_SUPPORTED_STREAMS];

static bool TsmuxBufferInUse[NUM_TSMUX_BUFFERS];
static unsigned char TsmuxMemory[NUM_TSMUX_BUFFERS][TSMUX_BUFFER_SIZE];
static bool Tsmux_Started = false;

static char *dev_name;
static int fdin = -1;
static int fdout[MAX_SUPPORTED_STREAMS];

static void SendStreamBuffer(unsigned int StreamId,
			     unsigned int Length,
			     unsigned long long DTS, bool end_of_stream);
static void StartTsmux(void);

/******************************
* 		Start of tsmux code bits
******************************/

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

	if (-1 == xioctl(fdin, VIDIOC_QBUF, &buf))
		errno_exit("VIDIOC_QBUF");

	TsmuxBufferInUse[num] = true;
	return;
}

static void ProcessMuxBuffer(int bufnum, int bytesused)
{
	fwrite(&TsmuxMemory[bufnum][0], bytesused, 1, Output);
}

static void DequeueMuxBuffer(int *bufnum, int *bytesused)
{
	struct v4l2_buffer buf;
	unsigned int i = 0;
	unsigned int found = 0;

	PRINT_DEBUG("%s\n", __func__);
	*bufnum = -1;
	*bytesused = -1;

	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_DVB_CAPTURE;
	buf.memory = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl(fdin, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
		case EAGAIN:
			fprintf(stderr, "EAGAIN : Not ready to dequeue mux\n");
			return;
		case EPERM:
			fprintf(stderr, "EPERM :  Not ready to dequeue mux\n");
			return;
		case EIO:
			/* Could ignore EIO, see spec. */
			/* fall through */
		default:
			errno_exit("VIDIOC_DQBUF");
			break;
		}
	}

	/* Check and mark the relevant buffer as no longer in use
	   TODO Can use the buf.index field to find the buffer */
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

	PRINT_DEBUG("%s\n", __func__);
	fds.fd = fdin;
	fds.events = (POLLIN);

	/* Mux buffers are ready to be dequeued if read is ready */
	/* NOTE: Using poll instead of select because select returns a valid
	 * set fds value on POLLERR for some reason so when the queue is empty
	 * it looks like there is a buffer to be dequeued.
	 */
	r = poll(&fds, 1, 0);
	/*PRINT_DEBUG("Poll returned %d\n", r); */
	if (r < 0)
		errno_exit("Poll\n");
	if (r > 0) {
		/*PRINT_DEBUG("Poll revents = 0x%x\n", fds.revents); */
		if ((fds.revents & POLLERR) || (fds.revents & POLLHUP) ||
		    (fds.revents & POLLNVAL)) {
			/*PRINT_DEBUG("Poll errors\n"); */
			return false;
		}
		if (fds.revents & POLLIN) {
			PRINT_DEBUG("POLLIN Set\n");
			return true;
		}
		return true;
	} else {
		/*PRINT_DEBUG("Poll is zero\n"); */
		return false;
	}
}

static void TerminateAllStreams(void)
{
	int i;

	for (i = 0; i < MAX_SUPPORTED_STREAMS; i++) {
		if (StreamUsed[i]) {
			SendStreamBuffer(i, sizeof(unsigned int), 0ULL, true);
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
			stop = true;
		}
	} while (stop == false);
	return ret;
}

static void QueueStreamBuffer(unsigned int StreamId,
			      unsigned int Index,
			      unsigned int Length,
			      unsigned long long DTS, bool end_of_stream)
{
	struct v4l2_buffer buf;
	struct v4l2_tsmux_src_es_metadata es_meta;
	struct v4l2_format format;

	PRINT_DEBUG("%s\n", __func__);

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
	    (format.fmt.dvb.data_type == V4L2_DVB_FMT_PES)) {
		es_meta.DTS = DTS;
		es_meta.dit_transition = 0;

		buf.reserved = (int)&es_meta;
	}

	if (-1 == xioctl(fdout[StreamId], VIDIOC_QBUF, &buf))
		errno_exit("VIDIOC_QBUF");
	return;
}

static void PrepareStreamBuffer(unsigned int StreamId,
				unsigned int Index,
				unsigned int Length, unsigned long long DTS)
{
	unsigned char *Data = &StreamBufferMemory[StreamId][Index][0];

	PRINT_DEBUG("%s\n", __func__);
	bool Video = Streams[StreamId].StreamHeader.Type
	    == CodeToInteger('v', 'i', 'd', 's');
	/* Add PES header to buffer */
	Data[0] = 0x00;
	Data[1] = 0x00;
	Data[2] = 0x01;
	Data[3] = (Video ? 0xe0 : 0xc0);
	Data[4] = ((Length + 8) >> 8) & 0xff;
	Data[5] = ((Length + 8) & 0xff);
	Data[6] = 0x80;
	Data[7] = 0x80;
	Data[8] = 0x05;
	Data[9] = ((DTS >> 29) & 0x0e) | 1 | 0x20;
	Data[10] = ((DTS >> 22) & 0xff);
	Data[11] = ((DTS >> 14) & 0xfe) | 1;
	Data[12] = ((DTS >> 7) & 0xff);
	Data[13] = ((DTS << 1) & 0xfe) | 1;
	fread(&StreamBufferMemory[StreamId][Index][14], Length, 1, Input);

	return;
}

static int DequeueStreamBuffer(unsigned int StreamId, unsigned int Index)
{
	struct v4l2_buffer buf;
	unsigned int i = 0;
	unsigned int found = 0;
	int ret = 0;

	PRINT_DEBUG("%s\n", __func__);
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
	/* Mark the relevant buffer as no longer in use and remove from
	   the StreamBuffersInUse array
	   TODO Can use the buf.index to find the relevant buffer */
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

	PRINT_DEBUG("%s\n", __func__);
	/* Check if we have started the tsmux yet, if not then start it */
	if (!Tsmux_Started) {
		StartTsmux();
		Tsmux_Started = true;
	}

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
		PRINT_DEBUG
		    ("Sending buffer stream=%d, index=%d, addr=0x%lx, Len=%d, DTS = 0x%010llx\n",
		     StreamId, Index,
		     (unsigned long)&StreamBufferMemory[StreamId][Index][0],
		     Length, DTS);
		QueueStreamBuffer(StreamId, Index, (Length + 14), DTS,
				  end_of_stream);
	} else {
		Length = 0;
		PRINT_DEBUG
		    ("Send end stream buffer stream=%d, index=%d, addr=0x%lx, Len=%d\n",
		     StreamId, Index,
		     (unsigned long)&StreamBufferMemory[StreamId][Index][0],
		     Length);
		QueueStreamBuffer(StreamId, Index, Length, DTS, end_of_stream);
	}

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
		 .id = V4L2_CID_STM_TSMUX_BIT_BUFFER_SIZE,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 2*1024*1024 */
		 .value = BitBufferSize,
		 },
	};

	PRINT_DEBUG("%s\n", __func__);

	controls.ctrl_class = V4L2_CTRL_CLASS_TSMUX;
	controls.count = sizeof(ctrl_array) / sizeof(struct v4l2_ext_control);
	if (controls.count == 0)
		return;
	controls.reserved[0] = 0;
	controls.reserved[1] = 0;
	controls.controls = &ctrl_array[0];

	if (-1 == xioctl(fdout[StreamId], VIDIOC_S_EXT_CTRLS, &controls))
		errno_exit("VIDIOC_S_EXT_CTRLS");

}

static void InitialiseStreamBuffersInUse(unsigned int StreamId)
{
	int i;

	PRINT_DEBUG("%s\n", __func__);
	for (i = 0; i < MAX_BUFFERS_PER_STREAM; i++) {
		StreamBufferInUse[StreamId][i] = false;
	}
	StreamNextBuffer[StreamId] = 0;
	return;
}

static void InitialiseStreamOutUserPtr(unsigned int StreamId)
{
	struct v4l2_requestbuffers req;

	PRINT_DEBUG("%s\n", __func__);
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
	format.fmt.dvb.data_type = V4L2_DVB_FMT_PES;
	format.fmt.dvb.buffer_size = STREAM_BUFFER_SIZE;
	format.fmt.dvb.codec = Type;
	if (ioctl(fdout[StreamId], VIDIOC_S_FMT, &format) < 0) {
		errno_exit("ERROR : VIDIOC_S_FMT --> OUTPUT");
	}

}

static void AddAStream(unsigned int StreamId,
		       unsigned int Type, unsigned int BitBufferSize)
{
	PRINT_DEBUG("%s\n", __func__);
	OpenStreamOut(StreamId, Type);
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
	};
	PRINT_DEBUG("%s\n", __func__);
	int i;
	struct v4l2_ext_control *ctrl;

	PRINT_DEBUG("%s\n", __func__);

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

	PRINT_DEBUG("Starting TSMUX\n");
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
	for (i = 0; i < MAX_SUPPORTED_STREAMS; i++) {
		if (StreamUsed[i]) {
			StopStreamStreaming(i);
		}
	}
	/* Check the tsmux memory buffers are not in use */
	for (i = 0; i < NUM_TSMUX_BUFFERS; i++) {
		if (TsmuxBufferInUse[i]) {
			fprintf(stderr, "Warning: Mux buffer %d still in use\n",
				i);
			//exit(EXIT_FAILURE);
		}
	}
	/* Stop streaming on the tsmux */
	StopMuxStreaming();
}

static void SetTsmuxParameters(void)
{
	struct v4l2_ext_controls controls;
	struct v4l2_ext_control ctrl_array[] = {
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
		{
		 .id = V4L2_CID_STM_TSMUX_PCR_PID,
		 .size = 0,
		 .reserved2 = {0},
		 /* Default is 0x1ffe */
		 .value = 0x1ffe,
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
	PRINT_DEBUG("%s\n", __func__);
	int i;
	struct v4l2_ext_control *ctrl;

	PRINT_DEBUG("%s\n", __func__);

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
	if (-1 == xioctl(fdin, VIDIOC_S_EXT_CTRLS, &controls))
		errno_exit("VIDIOC_S_EXT_CTRLS");
	return;
}

static void InitialiseTsmuxUserPtr(void)
{
	struct v4l2_requestbuffers req;

	PRINT_DEBUG("%s\n", __func__);
	CLEAR(req);

	req.count = NUM_TSMUX_BUFFERS;
	req.type = V4L2_BUF_TYPE_DVB_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	PRINT_DEBUG("Doing VIDIOC_REQBUFS for fdin\n");
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
	PRINT_DEBUG("%s\n", __func__);
	CheckTsmuxCapabilities();
	InitialiseTsmuxUserPtr();
	SetTsmuxParameters();

	return;
}

static void OpenTsmux(void)
{
	struct v4l2_format format;

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
}

/*******************************
* 		Start of avi code bits
******************************/

static char TheString[5];
char *String(unsigned int Code)
{
	memcpy(TheString, &Code, 4);
	TheString[4] = 0;
	return TheString;
}

static void Initialise(void)
{
	memset(StreamBufferInUse, 0x00,
	       MAX_SUPPORTED_STREAMS * MAX_BUFFERS_PER_STREAM * sizeof(bool));
	memset(StreamBufferMemory, 0xa5,
	       MAX_SUPPORTED_STREAMS * MAX_BUFFERS_PER_STREAM
	       * STREAM_BUFFER_SIZE * sizeof(unsigned char));
	memset(StreamNextBuffer, 0x00, MAX_SUPPORTED_STREAMS * sizeof(int));
	memset(TsmuxMemory, 0x00,
	       NUM_TSMUX_BUFFERS * TSMUX_BUFFER_SIZE * sizeof(unsigned char));
	memset(StreamUsed, 0x00, MAX_SUPPORTED_STREAMS * sizeof(bool));
	memset(Streams, 0x00, MAX_SUPPORTED_STREAMS * sizeof(Stream_t));

}

/*******************************
* 		Main program
******************************/

int main(int argc, char *argv[])
{
	unsigned int Size;
	char Identifier[5];
	unsigned int End;
	unsigned int Current;
	unsigned int ReadOnlyOne;

	unsigned int Stream;
	bool Video;
	unsigned int Count;
	unsigned int Scale;
	unsigned int Rate;
	unsigned int Align;
	unsigned long long NextDTS;

	printf("======>Starting the AVI test \n\n\n");

	dev_name = "/dev/tsmux/tsmux0";

	if (argc < 2) {
		printf
		    ("Usage should be testavi <input filename> [<output filename>]\n");
		exit(1);
	}

	Initialise();

	Input = fopen(argv[1], "rb");
	if (Input == NULL)
		fprintf(stderr, "Failed to open input file '%s'\n", argv[1]);

	Output = fopen(((argc > 2) ? argv[2] : "avi.ts"), "wb");
	if (Output == NULL)
		fprintf(stderr, "Failed to open output file '%s'\n",
			((argc > 2) ? argv[2] : "avi.ts"));

	OpenTsmux();
	ConfigureTsmux();
	AddAProgram();

	Identifier[4] = 0;
	StreamCount = 0;
	ReadOnlyOne = false;
	Current = 0;

	while (!feof(Input)) {

		Current = (Current >> 8) | (getc(Input) << 24);

		if (!ReadOnlyOne) {
			Current = (Current >> 8) | (getc(Input) << 24);
			Current = (Current >> 8) | (getc(Input) << 24);
			Current = (Current >> 8) | (getc(Input) << 24);
		}

		ReadOnlyOne = 0;

		switch (Current) {
		case CodeToInteger('R', 'I', 'F', 'F'):
			PRINT_DEBUG("%08x - ", (unsigned int)ftell(Input) - 4);
			fread(&Size, sizeof(unsigned int), 1, Input);
			End = ftell(Input) + Size;
			fread(&Identifier, sizeof(unsigned int), 1, Input);
			PRINT_DEBUG
			    ("RIFF - Size %08x, End %08x - Identifier '%s'\n",
			     Size, End, Identifier);
			break;

		case CodeToInteger('L', 'I', 'S', 'T'):
			PRINT_DEBUG("%08x - ", (unsigned int)ftell(Input) - 4);
			fread(&Size, sizeof(unsigned int), 1, Input);
			End = ftell(Input) + Size;
			fread(&Identifier, sizeof(unsigned int), 1, Input);
			PRINT_DEBUG
			    ("LIST - Size %08x, End %08x - Identifier '%s'\n",
			     Size, End, Identifier);
			break;

		case CodeToInteger('J', 'U', 'N', 'K'):
			PRINT_DEBUG("%08x - ", (unsigned int)ftell(Input) - 4);
			fread(&Size, sizeof(unsigned int), 1, Input);
			End = ftell(Input) + Size;
			fseek(Input, Size, SEEK_CUR);
			PRINT_DEBUG("JUNK - Size %08x, End %08x\n", Size, End);
			break;

		case CodeToInteger('a', 'v', 'i', 'h'):
			PRINT_DEBUG("%08x - ", (unsigned int)ftell(Input) - 4);
			fread(&Size, sizeof(unsigned int), 1, Input);
			End = ftell(Input) + Size;
			if (Size != sizeof(AviHeader_t)) {
				PRINT_DEBUG("avih - Size %04x, expected %04x\n",
					    Size, sizeof(AviHeader_t));
				exit(0);
			}
			fread(&AviHeader, sizeof(AviHeader_t), 1, Input);
			PRINT_DEBUG("avih - Size %08x, End %08x\n", Size, End);
			PRINT_DEBUG("        MicroSecondsPerFrame    %6d\n",
				    AviHeader.MicroSecondsPerFrame);
			PRINT_DEBUG("        MaxBytesPerSec          %6d\n",
				    AviHeader.MaxBytesPerSec);
			PRINT_DEBUG("        PaddingGranularity      %6d\n",
				    AviHeader.PaddingGranularity);
			PRINT_DEBUG("        Flags                   %06x\n",
				    AviHeader.Flags);
			PRINT_DEBUG("        TotalFrames             %6d\n",
				    AviHeader.TotalFrames);
			PRINT_DEBUG("        InitialFrames           %6d\n",
				    AviHeader.InitialFrames);
			PRINT_DEBUG("        Streams                 %6d\n",
				    AviHeader.Streams);
			PRINT_DEBUG("        SuggestedBufferSize     %6d\n",
				    AviHeader.SuggestedBufferSize);
			PRINT_DEBUG("        Width                   %6d\n",
				    AviHeader.Width);
			PRINT_DEBUG("        Height                  %6d\n",
				    AviHeader.Height);
			break;

		case CodeToInteger('s', 't', 'r', 'h'):
			PRINT_DEBUG("%08x - ", (unsigned int)ftell(Input) - 4);
			fread(&Size, sizeof(unsigned int), 1, Input);
			End = ftell(Input) + Size;
			if (Size != sizeof(StreamHeader_t)) {
				PRINT_DEBUG("strh - Size %04x, expected %04x\n",
					    Size, sizeof(StreamHeader_t));
				exit(0);
			}
			fread(&Streams[StreamCount].StreamHeader,
			      sizeof(StreamHeader_t), 1, Input);
			PRINT_DEBUG("strh - Size %08x, End %08x\n", Size, End);
			PRINT_DEBUG("        Type                    %6s\n",
				    String(Streams[StreamCount].StreamHeader.
					   Type));
			PRINT_DEBUG("        Handler                 %6s\n",
				    String(Streams[StreamCount].StreamHeader.
					   Handler));
			PRINT_DEBUG("        Flags                   %06x\n",
				    Streams[StreamCount].StreamHeader.Flags);
			PRINT_DEBUG("        Priority                %6d\n",
				    Streams[StreamCount].StreamHeader.Priority);
			PRINT_DEBUG("        Language                %6d\n",
				    Streams[StreamCount].StreamHeader.Language);
			PRINT_DEBUG("        InitialFrames           %6d\n",
				    Streams[StreamCount].StreamHeader.
				    InitialFrames);
			PRINT_DEBUG("        Scale                   %6d\n",
				    Streams[StreamCount].StreamHeader.Scale);
			PRINT_DEBUG("        Rate                    %6d\n",
				    Streams[StreamCount].StreamHeader.Rate);
			PRINT_DEBUG("        Start                   %6d\n",
				    Streams[StreamCount].StreamHeader.Start);
			PRINT_DEBUG("        Length                %8d\n",
				    Streams[StreamCount].StreamHeader.Length);
			PRINT_DEBUG("        SuggestedBufferSize     %6d\n",
				    Streams[StreamCount].StreamHeader.
				    SuggestedBufferSize);
			PRINT_DEBUG("        Quality                 %6d\n",
				    Streams[StreamCount].StreamHeader.Quality);
			PRINT_DEBUG("        SampleSize              %6d\n",
				    Streams[StreamCount].StreamHeader.
				    SampleSize);
			PRINT_DEBUG("        Frame.Left              %6d\n",
				    Streams[StreamCount].StreamHeader.Frame.
				    Left);
			PRINT_DEBUG("        Frame.Top               %6d\n",
				    Streams[StreamCount].StreamHeader.Frame.
				    Top);
			PRINT_DEBUG("        Frame.Right             %6d\n",
				    Streams[StreamCount].StreamHeader.Frame.
				    Right);
			PRINT_DEBUG("        Frame.Bottom            %6d\n",
				    Streams[StreamCount].StreamHeader.Frame.
				    Bottom);
			break;

		case CodeToInteger('s', 't', 'r', 'f'):
			PRINT_DEBUG("%08x - ", (unsigned int)ftell(Input) - 4);
			fread(&Size, sizeof(unsigned int), 1, Input);
			End = ftell(Input) + Size;
			if (Streams[StreamCount].StreamHeader.Type
			    == CodeToInteger('v', 'i', 'd', 's')) {
				if (Size < sizeof(StreamFormatVideo_t)) {
					PRINT_DEBUG
					    ("strf - Size %08x, expected at least %08x (for vids)\n",
					     Size, sizeof(StreamFormatVideo_t));
					exit(0);
				}
				fread(&Streams[StreamCount].StreamFormatVideo,
				      sizeof(StreamFormatVideo_t), 1, Input);
				PRINT_DEBUG
				    ("strf(vids) - Size %08x, End %08x\n", Size,
				     End);
				PRINT_DEBUG
				    ("        Size                    %6d\n",
				     Streams[StreamCount].StreamFormatVideo.
				     Size);
				PRINT_DEBUG
				    ("        Width                   %6d\n",
				     Streams[StreamCount].StreamFormatVideo.
				     Width);
				PRINT_DEBUG
				    ("        Height                  %6d\n",
				     Streams[StreamCount].StreamFormatVideo.
				     Height);
				PRINT_DEBUG
				    ("        Planes                  %6d\n",
				     Streams[StreamCount].StreamFormatVideo.
				     Planes);
				PRINT_DEBUG
				    ("        BitCount                %6d\n",
				     Streams[StreamCount].StreamFormatVideo.
				     BitCount);
				PRINT_DEBUG
				    ("        Compression             %6d\n",
				     Streams[StreamCount].StreamFormatVideo.
				     Compression);
				PRINT_DEBUG
				    ("        SizeImage               %6d\n",
				     Streams[StreamCount].StreamFormatVideo.
				     SizeImage);
				PRINT_DEBUG
				    ("        XPelsPerMeter           %6d\n",
				     Streams[StreamCount].StreamFormatVideo.
				     XPelsPerMeter);
				PRINT_DEBUG
				    ("        YPelsPerMeter           %6d\n",
				     Streams[StreamCount].StreamFormatVideo.
				     YPelsPerMeter);
				PRINT_DEBUG
				    ("        ClrUsed                 %6d\n",
				     Streams[StreamCount].StreamFormatVideo.
				     ClrUsed);
				PRINT_DEBUG
				    ("        ClrImportant            %6d\n",
				     Streams[StreamCount].StreamFormatVideo.
				     ClrImportant);
				if (Size > sizeof(StreamFormatVideo_t))
					fseek(Input,
					      Size
					      - sizeof(StreamFormatVideo_t),
					      SEEK_CUR);

				Scale = Streams[StreamCount].StreamHeader.Scale;
				Rate = Streams[StreamCount].StreamHeader.Rate;
				Streams[StreamCount].BitBufferSize = Streams[StreamCount].StreamHeader.SuggestedBufferSize * 16 * 8;	// 16 of them 8 bits
				Streams[StreamCount].BitBufferSize = 4000000;
				Streams[StreamCount].FrameCount =
				    Streams[StreamCount].StreamHeader.
				    InitialFrames;

				Streams[StreamCount].Type =
				    V4L2_STM_TSMUX_STREAM_TYPE_VIDEO_MPEG4;
				Streams[StreamCount].DTS =
				    (Streams[StreamCount].FrameCount
				     * 90000ull * Scale) / Rate;
				Streams[StreamCount].DTSAdjustment =
				    0xffffffffffffffffull;
				Streams[StreamCount].AdjustedDTS =
				    0xffffffffffffffffull;
				Streams[StreamCount].BufferCount = 0;
				Streams[StreamCount].CurrentBuffer =
				    StreamCount;

				AddAStream(StreamCount,
					   Streams[StreamCount].Type,
					   Streams[StreamCount].BitBufferSize);
				StreamCount++;
			} else if (Streams[StreamCount].StreamHeader.Type
				   == CodeToInteger('a', 'u', 'd', 's')) {
				if (Size < sizeof(StreamFormatAudio_t)) {
					PRINT_DEBUG
					    ("strf - Size %08x, expected at least %08x (for auds)\n",
					     Size, sizeof(StreamFormatAudio_t));
					exit(0);
				}
				fread(&Streams[StreamCount].StreamFormatAudio,
				      sizeof(StreamFormatAudio_t), 1, Input);
				PRINT_DEBUG
				    ("strf(auds) - Size %08x, End %08x\n", Size,
				     End);
				PRINT_DEBUG
				    ("        FormatTag               %6d\n",
				     Streams[StreamCount].StreamFormatAudio.
				     FormatTag);
				PRINT_DEBUG
				    ("        NumberOfChannels        %6d\n",
				     Streams[StreamCount].StreamFormatAudio.
				     NumberOfChannels);
				PRINT_DEBUG
				    ("        SamplesPerSecond        %6d\n",
				     Streams[StreamCount].StreamFormatAudio.
				     SamplesPerSecond);
				PRINT_DEBUG
				    ("        AverageBytesPerSecond   %6d\n",
				     Streams[StreamCount].StreamFormatAudio.
				     AverageBytesPerSecond);
				PRINT_DEBUG
				    ("        BlockAlign              %6d\n",
				     Streams[StreamCount].StreamFormatAudio.
				     BlockAlign);
				PRINT_DEBUG
				    ("        BitsPerSample           %6d\n",
				     Streams[StreamCount].StreamFormatAudio.
				     BitsPerSample);
				PRINT_DEBUG
				    ("        Size                    %6d\n",
				     Streams[StreamCount].StreamFormatAudio.
				     Size);
				if (Size > sizeof(StreamFormatAudio_t))
					fseek(Input,
					      Size
					      - sizeof(StreamFormatAudio_t),
					      SEEK_CUR);

				Scale = Streams[StreamCount].StreamHeader.Scale;
				Rate = Streams[StreamCount].StreamHeader.Rate;
				Streams[StreamCount].BitBufferSize = Streams[StreamCount].StreamHeader.SuggestedBufferSize * 16 * 8;	// 16 of them 8 bits
				Streams[StreamCount].BitBufferSize = 256000;

				Streams[StreamCount].Type =
				    V4L2_STM_TSMUX_STREAM_TYPE_AUDIO_MPEG2;
				Streams[StreamCount].DTS =
				    (Streams[StreamCount].StreamHeader.
				     InitialFrames * Scale * 90000)
				    / Rate;
				Streams[StreamCount].DTSAdjustment =
				    0xffffffffffffffffull;
				Streams[StreamCount].AdjustedDTS =
				    0xffffffffffffffffull;
				Streams[StreamCount].FrameCount = 0;
				Streams[StreamCount].BufferCount = 0;
				Streams[StreamCount].CurrentBuffer =
				    StreamCount;

				AddAStream(StreamCount,
					   Streams[StreamCount].Type,
					   Streams[StreamCount].BitBufferSize);
				StreamCount++;
			}
			break;

		case CodeToInteger('i', 'd', 'x', '1'):
			PRINT_DEBUG("%08x - ", (unsigned int)ftell(Input) - 4);
			fread(&Size, sizeof(unsigned int), 1, Input);
			End = ftell(Input) + Size;
			fseek(Input, Size, SEEK_CUR);
			PRINT_DEBUG("vids - Size %08x, End %08x\n", Size, End);
			break;

		case CodeToInteger('d', 'm', 'l', 'h'):
			PRINT_DEBUG("%08x - ", (unsigned int)ftell(Input) - 4);
			fread(&Size, sizeof(unsigned int), 1, Input);
			End = ftell(Input) + Size;
			fseek(Input, Size, SEEK_CUR);
			PRINT_DEBUG("vids - Size %08x, End %08x\n", Size, End);
			break;

		case CodeToInteger('0', '0', 'w', 'b'):
		case CodeToInteger('0', '0', 'd', 'b'):
		case CodeToInteger('0', '0', 'd', 'c'):
		case CodeToInteger('0', '1', 'w', 'b'):
		case CodeToInteger('0', '1', 'd', 'b'):
		case CodeToInteger('0', '1', 'd', 'c'):
			fread(&Size, sizeof(unsigned int), 1, Input);
			if (Size > (STREAM_BUFFER_SIZE - PES_HEADER_SIZE)) {
				PRINT_DEBUG
				    ("Record too large for buffer (%s:%08x)\n",
				     String(Current), Size);
				exit(0);
			}

			Stream = ((Current >> 8) & 0xff) - '0';

			Video = Streams[Stream].StreamHeader.Type
			    == CodeToInteger('v', 'i', 'd', 's');
			Count = ++Streams[Stream].FrameCount;
			Scale = Streams[Stream].StreamHeader.Scale;
			Rate = Streams[Stream].StreamHeader.Rate;
			Align = Streams[Stream].StreamFormatAudio.BlockAlign;

			if (Video)
				NextDTS = (Count * 90000ull * Scale) / Rate;
			else
				NextDTS = (Streams[Stream].DTS
					   + ((((Size + Align - 1) / Align)
					       * Scale * 90000)
					      / Rate));
			PRINT_DEBUG("DTS %d - %010llx\n", Stream,
				    Streams[Stream].DTS);
			SendStreamBuffer(Stream, Size, Streams[Stream].DTS,
					 false);

			/* Get any output if available */
			if (GetMultiplexedData(false)) {
				fprintf(stderr,
					"Error: multiplexer failed!!\n");
				exit(EXIT_FAILURE);
			}

			Streams[Stream].DTS = NextDTS;

			break;

		default:
			ReadOnlyOne = 1;
			PRINT_DEBUG("Nothing at %08x ---- %02x\n",
				    (unsigned int)ftell(Input) - 4,
				    ((Current) & 0xff));
			break;
		}
	}

	/* Flush any remaining output */
	if (GetMultiplexedData(true)) {
		fprintf(stderr, "Error: Multiplexer failed while flushing!!\n");
		exit(EXIT_FAILURE);
	}

	StopTsmux();

	fclose(Input);
	fclose(Output);

	printf("======>Completed the AVI test\n");

	return 0;
}
