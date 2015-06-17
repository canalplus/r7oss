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

Source file name : common.c

Common function used by both audio & video encoders

************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <linux/videodev2.h>
#include <sys/mman.h>

#include "common.h"

#include "utils/v4l2_helper.h"

ControlNode_t *InitControlNode(char *name, int id, int value)
{
	ControlNode_t *ptr;
	ptr = (ControlNode_t *) malloc(sizeof(ControlNode_t));
	if (ptr == NULL)
		return (ControlNode_t *) NULL;
	else {
		strcpy(ptr->ControlName, name);
		ptr->ControlID = id;
		ptr->ControlValue = value;
		return ptr;
	}
}

void AddControlNode(ControlNode_t **head, ControlNode_t **end,
		    ControlNode_t *new)
{				/* adding to end of list */
	if (*head == NULL)
		*head = new;
	if (*end != NULL)
		(*end)->NextControl = new;
	new->NextControl = NULL;
	*end = new;
}

int FrameWriter(drv_context_t *Context, unsigned int frame_no,
		unsigned long crc, void *data, unsigned int size)
{
	FILE *frame;
	char filename[1024];

	sprintf(filename, "%s.%04d.%08lx", Context->output_file_name, frame_no,
		crc);

	frame = fopen(filename, "wb");
	if (frame == NULL) {
		encode_error("can't open output file %s\n", filename);
		return -1;
	}

	if (fwrite(data, 1, size, frame) != size) {
		encode_error("Failed to write everything\n");
		fclose(frame);
		return -1;
	}

	fclose(frame);

	return 0;
}

/*
 * V4L2 backend helper
 */
int alloc_v4l2_buffers(drv_context_t *ContextPt, unsigned int type)
{
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;
	int fd;
	void **buffer_p;
	int *offset;
	int *length;

	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		fd = ContextPt->fd_w;
		buffer_p = &ContextPt->input_buffer_p;
		offset = &ContextPt->input_buffer_p_offset;
		length = &ContextPt->input_buffer_p_length;
	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		fd = ContextPt->fd_r;
		buffer_p = &ContextPt->output_buffer_p;
		offset = &ContextPt->output_buffer_p_offset;
		length = &ContextPt->output_buffer_p_length;
	} else {
		encode_error("Unsupported buffer type\n");
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.count = 1;
	req.type = type;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
		encode_error("failed to reqbuf\n");
		return -1;
	}

	memset(&buf, 0, sizeof(buf));
	buf.type = type;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
	if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
		encode_error("VIDIOC_QUERYBUF failed!\n");
		return -1;
	}

	*length = buf.length;
	*offset = buf.m.offset;
	*buffer_p =
	    mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		 buf.m.offset);
	if (*buffer_p == MAP_FAILED) {
		encode_error("mmap failed!\n");
		return -1;
	}

	encode_print("===>> buffer      : mmap=%p, length=%x, offset=%x\n",
		     *buffer_p, *length, *offset);

	return 0;
}

int stream_on(drv_context_t *ContextPt, unsigned int type)
{
	struct v4l2_buffer buf;
	int fd;

	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		fd = ContextPt->fd_w;
	else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		fd = ContextPt->fd_r;
	else {
		encode_error("unsupported buffer type\n");
		return -1;
	}

	/* Start streaming I/O */
	memset(&buf, 0, sizeof(struct v4l2_buffer));
	buf.type = type;
	if (ioctl(fd, VIDIOC_STREAMON, &buf.type) < 0) {
		encode_error("cannot start streaming\n");
		return -1;
	}

	return 0;
}
