/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

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

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
- v4l2 AV grab application with mmap'ped buffers
**************************************************************************/
#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "utils/v4l2_helper.h"

#define MMAP_MAX_BUFFERS	2
#define PIXEL_SIZE		4
#define V4L2_RENDER_WIDTH	720
#define V4L2_RENDER_HEIGHT	480

static int app_exit = 0;


/*
 * Set the video format
 */
static int v4l2_set_video_format(int fd, enum v4l2_buf_type buf_type)
{

	struct v4l2_format fmt;
	int ret = 0;

	/*
	 * Set the format
	 */
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = buf_type;
	fmt.fmt.pix.width = V4L2_RENDER_WIDTH;
	fmt.fmt.pix.height = V4L2_RENDER_HEIGHT;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
	fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	fmt.fmt.pix.field = V4L2_FIELD_ANY;
	fmt.fmt.pix.bytesperline = fmt.fmt.pix.width * PIXEL_SIZE;
	ret = ioctl(fd, VIDIOC_S_FMT, &fmt);
	if (ret)
		printf("set format failed\n");

	return ret;
}

/*
 * A common function for requesting buffers
 */
static int v4l2_request_buffers(int fd, enum v4l2_buf_type type,
				enum v4l2_memory mem, int count)
{
	struct v4l2_requestbuffers buf;
	int ret = 0;

	memset(&buf, 0, sizeof(buf));
	buf.count = count;
	buf.type = type;
	buf.memory = mem;
	ret = ioctl (fd, VIDIOC_REQBUFS, &buf);
	if (ret)
		printf("Request for buffers failed\n");

	return ret;
}

/*
 * Initialize the video decoder and set the input
 */
static int video_decoder_init(int *dec_fd)
{
	int ret = 0;

	/*
	 * Open v4l2 av decoder
	 */
	*dec_fd = v4l2_open_by_name("AV Decoder", "STMicroelectronics", O_RDWR | O_NONBLOCK);
	if (*dec_fd < 0) {
		printf("Failed to open AV decoder\n");
		goto decoder_init_failed;
	}

	/*
	 * Set the input
	 */
	ret = v4l2_set_input_by_name(*dec_fd, "dvb0.video3");
	if (ret) {
		printf("Failed to set the input\n");
		goto set_input_failed;
	}

	return 0;

set_input_failed:
	close(*dec_fd);
decoder_init_failed:
	return ret;
}

/*
 * Initializer the display driver
 */
static int video_output_init(int *disp_fd)
{
	int ret = 0;

	*disp_fd = v4l2_open_by_name("Planes", "STMicroelectronics", O_RDWR | O_NONBLOCK);
	if (ret < 0) {
		printf("Display device open failed\n");
		ret = -1;
		goto display_open_failed;
	}

	ret = v4l2_set_output_by_name(*disp_fd, "Main-GDP2");
	if (ret) {
		printf("Display device output cannot be set\n");
		goto set_output_failed;
	}

	return 0;

set_output_failed:
	close(*disp_fd);
display_open_failed:
	return ret;
}

void *mmap_video_grab_thread(void *arg)
{
	int dec_fd, disp_fd, buf_index;
	struct v4l2_buffer dec_buf[MMAP_MAX_BUFFERS];
	struct v4l2_buffer disp_buf[MMAP_MAX_BUFFERS];
	struct v4l2_crop crop;
	int ret = 0;

	/*
	 * Prepare v4l2 av capture driver
	 */
	ret = video_decoder_init(&dec_fd);
	if (ret) {
		printf("Video decoder init failed\n");
		goto decoder_init_failed;
	}

	ret = v4l2_set_video_format(dec_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (ret) {
		printf("Video decoder set format failed\n");
		goto decoder_init_failed;
	}

	ret = v4l2_request_buffers(dec_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_MMAP, 2);
	if (ret) {
		printf("Video decoder request buffer failed\n");
		goto decoder_init_failed;
	}

	/*
	 * Preapre Display driver
	 */
	ret = video_output_init(&disp_fd);
	if (ret) {
		printf("Video display init failed\n");
		goto decoder_init_failed;
	}

	ret = v4l2_set_video_format(disp_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	if (ret) {
		printf("Video display set format failed\n");
		goto display_set_format_failed;
	}

	memset(&crop, 0, sizeof(crop));
	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	crop.c.left = 0;
	crop.c.top = 0;
	crop.c.width = V4L2_RENDER_WIDTH;
	crop.c.height = V4L2_RENDER_HEIGHT;
	ret = ioctl(disp_fd, VIDIOC_S_CROP, &crop);
	if (ret < 0) {
		printf("Video display set crop failed\n");
		goto display_set_format_failed;
	}

	ret = v4l2_request_buffers(disp_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, V4L2_MEMORY_USERPTR, 2);
	if (ret) {
		printf("Video decoder request buffer failed\n");
		goto decoder_init_failed;
	}

	/*
	 * Pass on the first capture buffer to display driver to initialize
	 * the display queue
	 */
	memset(&dec_buf, 0, sizeof(dec_buf));
	memset(&disp_buf, 0, sizeof(disp_buf));

	for (buf_index = 0; buf_index < MMAP_MAX_BUFFERS; buf_index++) {
		dec_buf[buf_index].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		dec_buf[buf_index].memory = V4L2_MEMORY_MMAP;
		dec_buf[buf_index].index = buf_index;

		disp_buf[buf_index].type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		disp_buf[buf_index].memory = V4L2_MEMORY_USERPTR;
		disp_buf[buf_index].index = buf_index;
		disp_buf[buf_index].field = V4L2_FIELD_NONE;

		ret = ioctl(dec_fd, VIDIOC_QUERYBUF, &dec_buf[buf_index]);
		if (ret) {
			printf("Video decoder query-buf failed\n");
			goto display_set_format_failed;
		}
		disp_buf[buf_index].m.userptr = (unsigned long) mmap(NULL, dec_buf[buf_index].length,
							PROT_READ, MAP_SHARED, dec_fd,
							dec_buf[buf_index].m.offset);
		if (disp_buf[buf_index].m.userptr == (unsigned long)MAP_FAILED) {
			printf("Display device userptr failed\n");
			goto display_set_format_failed;
		}
	}

	/*
	 * Now buf_index will be used to queue/dqbuf buffers sequentially
	 */
	buf_index = 0;
	ret = ioctl(disp_fd, VIDIOC_QBUF, &disp_buf[buf_index]);
	if (ret) {
		printf("Display device first qbuf failed\n");
		goto display_set_format_failed;
	}

	ret = ioctl(disp_fd, VIDIOC_STREAMON, &disp_buf[buf_index].type);
	if (ret) {
		printf("Display device streamon failed\n");
		goto display_set_format_failed;
	}
	buf_index++;

	/*
	 * Start the decoder, get the decoded data and push to display
	 */
	do {
		ret = ioctl(dec_fd, VIDIOC_STREAMON, &dec_buf[buf_index].type);
		if (ret) {
			printf("Video decoder streamon failed\n");
			goto display_set_format_failed;
		}

		/*
		 * Queue/DQBUF
		 */
		do {
			ret = ioctl(dec_fd, VIDIOC_QBUF, &dec_buf[buf_index]);
			if (ret) {
				printf("Video decoder QBUF failed\n");
				goto display_set_format_failed;
			}

			do {
				ret = ioctl(dec_fd, VIDIOC_DQBUF, &dec_buf[buf_index]);
				if (ret) {
					if (errno != EAGAIN) {
						printf("Video decoder DQBUF failed\n");
						goto display_set_format_failed;
					} else
					continue;
				}
			} while(ret);

			ret = ioctl(disp_fd, VIDIOC_QBUF, &disp_buf[buf_index]);
			if (ret) {
				printf("Display device QBUF failed\n");
				goto display_set_format_failed;
			}

			buf_index++;
			if (buf_index >= MMAP_MAX_BUFFERS)
				buf_index = 0;

			do {
				ret = ioctl(disp_fd, VIDIOC_DQBUF, &disp_buf[buf_index]);
				if (ret) {
					if (errno != EAGAIN) {
						printf("Display device DQBUF failed\n");
						goto display_set_format_failed;
					} else
						continue;
				}
			} while(ret);


		} while (!app_exit);

	} while(!app_exit);

display_set_format_failed:
	close(disp_fd);
decoder_init_failed:
	close(dec_fd);
	return NULL;
}


int main(int argc, char *argv[])
{
	pthread_t tid_video;
	int ret = 0;

	ret = pthread_create(&tid_video, NULL, mmap_video_grab_thread, NULL);
	if (ret) {
		perror("Failed to create video thread\n");
		goto thread_failed;
	}

	pthread_join(tid_video, NULL);

	return 0;

thread_failed:
	perror("error:");
	return ret;
}
