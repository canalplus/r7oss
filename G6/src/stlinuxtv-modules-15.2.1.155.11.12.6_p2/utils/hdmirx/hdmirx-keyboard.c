/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

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
 * Keyboard thread to accept runtime options
************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <poll.h>
#include <mediactl/v4l2subdev.h>
#include <linux/v4l2-subdev.h>

#include "linux/stm_v4l2_hdmi_export.h"
#include "linux/stm_v4l2_export.h"
#include "linux/stm_v4l2_decoder_export.h"
#include "hdmirx-capture.h"
#include "stm_v4l2_helper.h"

#define COMMAND_LINE_SIZE	128

/**
 * hdmirx_reszie_plane() - resize the plane
 */
static int hdmirx_reszie_plane(struct hdmirx_context *hdmirx_ctx, char *plane_name)
{
	int ret;
	FILE *fp;
	char buffer[COMMAND_LINE_SIZE];
	struct v4l2_control ctrl;
	struct v4l2_subdev_crop crop;

	/*
	 * Open plane subdev
	 */
	strncpy(hdmirx_ctx->plane_name, plane_name, sizeof(hdmirx_ctx->plane_name));
	ret = stm_v4l2_subdev_open(hdmirx_ctx->media, plane_name,
						&hdmirx_ctx->plane_entity);
	if (ret) {
		printf("%s(): failed to open plane subdev\n", __func__);
		goto resize_done;
	}

	/*
	 * Move the plane to MANUAL mode
	 */
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_STM_OUTPUT_WINDOW_MODE;
	ctrl.value = VCSPLANE_MANUAL_MODE;
	ret = ioctl(hdmirx_ctx->plane_entity->fd, VIDIOC_S_CTRL, &ctrl);
	if (ret) {
		printf("%s(): failed to set %s output to MANUAL mode\n", __func__, plane_name);
		goto set_manual_failed;
	}

	/*
	 * Find out the plane parameters from plane.conf
	 */
	memset(&crop, 0, sizeof(crop));
	fp = fopen("plane.conf", "r");
	if (!fp) {
		printf("%s(): no plane conf file present\n", __func__);
		goto set_manual_failed;
	}

	while (fgets(buffer, sizeof(buffer), fp)) {

		char *ptr;

		/*
		 * Skip comments from the conf file
		 */
		if (strchr(buffer, '#'))
			continue;

		ptr = strrchr(buffer, ' ');
		if (!strncmp(buffer, "left", strlen("left")))
			sscanf(ptr + 1, "%d", &crop.rect.left);
		else if (!strncmp(buffer, "top", strlen("top")))
			sscanf(ptr + 1, "%d", &crop.rect.top);
		else if (!strncmp(buffer, "width", strlen("width")))
			sscanf(ptr + 1, "%d", &crop.rect.width);
		else if (!strncmp(buffer, "height", strlen("height")))
			sscanf(ptr + 1, "%d", &crop.rect.height);
	}

	printf("Setting window with: (x: %d, y: %d), (width: %d, height: %d)\n",
				crop.rect.left, crop.rect.top, crop.rect.width, crop.rect.height);

	/*
	 * Set cropping on the output window
	 */
	crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	crop.pad = 1;
	ret = ioctl(hdmirx_ctx->plane_entity->fd, VIDIOC_SUBDEV_S_CROP, &crop);
	if (ret) {
		printf("%s(): cropping output window failed\n", __func__);
		goto set_manual_failed;
	}

	return 0;

set_manual_failed:
	stm_v4l2_subdev_close(hdmirx_ctx->plane_entity);
	hdmirx_ctx->plane_entity = NULL;
resize_done:
	return ret;
}

/**
 * keyboard_help() - keyboard help
 */
void hdmirx_keyboard_help()
{
	printf("  resize\n");
	printf("   The plane which you want to reconfigure, the input comes from plane.conf file\n\n");
}

/**
 * find_command() - finds out if the command is supported
 */
static int find_command(char *command)
{
	if (strncmp(command, "resize", strlen("resize")))
		return false;

	return true;
}

/**
 * keyboard_monitor() - keyboard monitor thread
 */
void *keyboard_monitor(void *arg)
{
	char command_line[COMMAND_LINE_SIZE], *ptr;
	struct hdmirx_context *hdmirx_ctx = arg;

	/*
	 * Read the command line parse it for the options
	 */
	while (fgets(command_line, sizeof(command_line), stdin)) {

		if (!find_command(command_line)) {
			hdmirx_keyboard_help();
			continue;
		}

		printf("What? Main-VID, Main-PIP ...?");
		fgets(command_line, sizeof(command_line), stdin);

		ptr = strchr(command_line, '\n');
		*ptr = '\0';

		hdmirx_reszie_plane(hdmirx_ctx, command_line);
	}

	return NULL;
}
