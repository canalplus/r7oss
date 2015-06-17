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

#ifndef H_H264PPIO
#define H_H264PPIO

#define H264_PP_IOCTL_QUEUE_BUFFER              1
#define H264_PP_IOCTL_GET_PREPROCESSED_BUFFER   2

#define H264_PP_DEVICE_NAME                     "h264pp"
#define H264_PP_DEVICE                          "/dev/"H264_PP_DEVICE_NAME

typedef struct h264pp_ioctl_queue_s {
	unsigned int     QueueIdentifier;
	void            *InputBufferCachedAddress;
	void            *InputBufferPhysicalAddress;
	void            *OutputBufferCachedAddress;
	void            *OutputBufferPhysicalAddress;
	unsigned int     Field;
	unsigned int     MaxOutputBufferSize;
	unsigned int     InputSize;
	unsigned int     SliceCount;
	unsigned int     CodeLength;
	unsigned int     PicWidth;
	unsigned int     Cfg;
} h264pp_ioctl_queue_t;

typedef struct h264pp_ioctl_dequeue_s {
	unsigned int    QueueIdentifier;
	unsigned int    OutputSize;
	unsigned int    ErrorMask;
} h264pp_ioctl_dequeue_t;

#endif
