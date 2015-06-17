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
 * Configuring and starting compo capture
************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <mediactl/mediactl.h>
#include <mediactl/v4l2subdev.h>
#include <linux/v4l2-subdev.h>
#include <pthread.h>
#include <poll.h>

#include "linux/dvb/dvb_v4l2_export.h"
#include "stm_v4l2_helper.h"
#include "linux/stm_v4l2_export.h"

#define AUX_HEIGHT	576
#define AUX_WIDTH	720

struct capture_context {
	struct media_device *media;
	char plane_name[32], compo_name[32], decoder_name[32];
	struct media_entity *compo_entity, *decoder_entity;
	struct media_entity *main_entity, *aux_entity;
}cap_ctx;

/**
 * compo_main_configuration() - configuration for Main display planes
 */
static int compo_main_configuration()
{
	int ret = 0;
	struct v4l2_control ctrl;

	/*
	 * Open the plane subdev
	 */
	ret = stm_v4l2_subdev_open(cap_ctx.media,
			cap_ctx.plane_name, &cap_ctx.main_entity);
	if (ret) {
		goto plane_open_failed;
	}

	/*
	 * Configure input window of main plane in AUTO mode
	 */
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_STM_INPUT_WINDOW_MODE;
	ctrl.value = VCSPLANE_AUTO_MODE;
	ret = ioctl(cap_ctx.main_entity->fd, VIDIOC_S_CTRL, &ctrl);
	if (ret) {
		printf("%s(): failed to set %s input to AUTO mode\n", __func__, cap_ctx.plane_name);
		goto input_auto_mode_failed;
	}

	/*
	 * Configure output window of main plane in AUTO mode
	 */
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_STM_OUTPUT_WINDOW_MODE;
	ctrl.value = VCSPLANE_AUTO_MODE;
	ret = ioctl(cap_ctx.main_entity->fd, VIDIOC_S_CTRL, &ctrl);
	if (ret) {
		printf("%s(): failed to set %s output to AUTO mode\n", __func__, cap_ctx.plane_name);
		goto input_auto_mode_failed;
	}

	return 0;

input_auto_mode_failed:
	stm_v4l2_subdev_close(cap_ctx.main_entity);
	cap_ctx.main_entity = NULL;
plane_open_failed:
	return ret;
}

/**
 * compo_aux_configuration() - compo configuration for aux planes
 */
static int compo_aux_configuration()
{
	int ret = 0;
	struct v4l2_rect rect;
	struct v4l2_mbus_framefmt mbus_fmt;

	/*
	 * Open the plane subdev
	 */
	ret = stm_v4l2_subdev_open(cap_ctx.media,
			cap_ctx.plane_name, &cap_ctx.aux_entity);
	if (ret) {
		printf("%s(): failed to open %s subdev\n", __func__, cap_ctx.plane_name);
		goto plane_open_failed;
	}

	/*
	 * Set the input window dimensions of aux plane
	 */
	memset(&rect, 0, sizeof(rect));
	rect.width = AUX_WIDTH;
	rect.height = AUX_HEIGHT;
	ret = v4l2_subdev_set_crop(cap_ctx.aux_entity, &rect,
					0, V4L2_SUBDEV_FORMAT_ACTIVE);
	if (ret) {
		printf("%s(): failed to set crop on %s\n", __func__, cap_ctx.plane_name);
		goto set_crop_failed;
	}

	/*
	 * Set the output window dimensions of aux plane
	 */
	ret = v4l2_subdev_set_crop(cap_ctx.aux_entity, &rect,
					1, V4L2_SUBDEV_FORMAT_ACTIVE);
	if (ret) {
		printf("%s(): failed to set crop on %s\n", __func__, cap_ctx.plane_name);
		goto set_crop_failed;
	}

	/*
	 * Get the cropping parameters from the aux plane
	 */
	memset(&rect, 0, sizeof(rect));
	ret = v4l2_subdev_get_crop(cap_ctx.aux_entity, &rect,
					0, V4L2_SUBDEV_FORMAT_ACTIVE);
	if (ret) {
		printf("%s(): failed to set crop on %s\n", __func__, cap_ctx.plane_name);
		goto set_crop_failed;
	}

	/*
	 * Open compo subdev
	 */
	ret = stm_v4l2_subdev_open(cap_ctx.media,
			cap_ctx.compo_name,  &cap_ctx.compo_entity);
	if (ret) {
		printf("%s(): failed to open compo subdev\n", __func__);
		goto set_crop_failed;
	}

	/*
	 * Get the format from compo
	 */
	memset(&mbus_fmt, 0, sizeof(mbus_fmt));
	ret = v4l2_subdev_get_format(cap_ctx.compo_entity,
				&mbus_fmt, 1, V4L2_SUBDEV_FORMAT_ACTIVE);
	if (ret) {
		printf("%s(): failed to get compo format\n", __func__);
		goto set_compo_fmt_failed;
	}

	/*
	 * Set the format on compo for the particular output
	 */
	mbus_fmt.width = rect.width;
	mbus_fmt.height = rect.height;
	ret = v4l2_subdev_set_format(cap_ctx.compo_entity,
				&mbus_fmt, 1, V4L2_SUBDEV_FORMAT_ACTIVE);
	if (ret) {
		printf("%s(): failed to set compo format\n", __func__);
		goto set_compo_fmt_failed;
	}

	return 0;

set_compo_fmt_failed:
	stm_v4l2_subdev_close(cap_ctx.compo_entity);
	cap_ctx.compo_entity = NULL;
set_crop_failed:
	stm_v4l2_subdev_close(cap_ctx.aux_entity);
	cap_ctx.aux_entity = NULL;
plane_open_failed:
	return ret;
}

/**
 * compo_capture_start() - configure and start compo capture
 */
static int compo_capture_start()
{
	int ret, exit;

	/*
	 * Open decoder subdev
	 */
	ret = stm_v4l2_subdev_open(cap_ctx.media,
			cap_ctx.decoder_name, &cap_ctx.decoder_entity);
	if (ret) {
		printf("%s(): failed to open decoder subdev\n", __func__);
		goto decoder_open_failed;
	}

	/*
	 * Start the decoder capture
	 */
	ret = ioctl(cap_ctx.decoder_entity->fd,
				VIDIOC_SUBDEV_STI_STREAMON, NULL);
	if (ret) {
		printf("%s(): failed to streamon decoder \n", __func__);
		goto start_failed;
	}

	/*
	 * Wait for user input before stopping streaming
	 */
	printf("Press any key + return to exit and stop streaming\n");
	scanf("%d", &exit);

	/*
	 * Close the decoder subdev as nothing to do for now
	 */
	stm_v4l2_subdev_close(cap_ctx.decoder_entity);
	cap_ctx.decoder_entity = NULL;

start_failed:
	stm_v4l2_subdev_close(cap_ctx.decoder_entity);
decoder_open_failed:
	return ret;
}

/*
 * usage() - help message
 */
static void usage()
{
	printf("compo-capture version-1.0\n");
	printf("Last modified (29 May 2014)\n");
	printf("Compile info:- DATE(%s), TIME(%s)\n", __DATE__, __TIME__);
	printf("Usage: compo-capture [OPTIONS]\n");
	printf("Configure the compo capture.\n");
	printf("\n");
	printf("  -p, --plane=[Main-VID, Aux-GDP2 ... ]\n");
	printf("     Select the plane which will be used to display data captured from compositor\n\n");
	printf("  -d, --device=[0 ... ]\n");
	printf("     Select the compo device to be used for capture\n\n");
	printf("  --dec-id, [0 ... ]\n");
	printf("     Select the decoder device to be used for capture\n\n");
	printf("  --compo-start\n");
	printf("     Configure and start compo capture\n\n");
}

/*
 * main() - main entry point
 */
int main(int argc, char *argv[])
{
	int ret = 0, option, device_id = 0;

	/*
	 * Construct the arguments to be parsed by this application
	 */
	enum {
		COMPO_START,
		DECODER_ID,
	};

	struct option longopt[] = {
		{"compo-start", 0, NULL, COMPO_START},
		{"plane", 1, NULL, 'p'},
		{"dec-id", 1, NULL, DECODER_ID},
		{0, 0, 0, 0}
	};

	/*
	 * Open the media device
	 */
	ret = stm_v4l2_media_open(0, &cap_ctx.media);
	if (ret) {
		printf("%s(): failed to open media device 0\n", __func__);
		goto exit;
	}

	while ((option = getopt_long(argc, argv, "", longopt, NULL)) != -1) {

		switch(option) {
		case COMPO_START:

			if (!strlen(cap_ctx.plane_name)) {
				printf("%s(): no plane specified, cannot start\n", __func__);
				usage();
				goto media_exit;
			}

			ret = compo_capture_start();

			break;

		case DECODER_ID:
			device_id = atoi(optarg);
			snprintf(cap_ctx.decoder_name, sizeof(cap_ctx.decoder_name), "v4l2.video%d", device_id);
			snprintf(cap_ctx.compo_name, sizeof(cap_ctx.compo_name), "compo0");
			break;

		case 'p':

			strncpy(cap_ctx.plane_name, optarg, sizeof(cap_ctx.plane_name) - 1);
			cap_ctx.plane_name[strlen(cap_ctx.plane_name)] = '\0';

			/*
			 * We configure the Main planes and Aux planes
			 * differently, so, we decide here, what we want to do
			 */
			if (!strncmp(cap_ctx.plane_name, "Main", strlen("Main")))
				ret = compo_main_configuration();
			else if (!strncmp(cap_ctx.plane_name, "Aux", strlen("Aux")))
				ret = compo_aux_configuration();
			else
				ret = -EINVAL;

			break;

		default:
			printf("%s(): invalid argument\n", __func__);
			usage();
			break;
		}
	}

media_exit:
	stm_v4l2_media_close(cap_ctx.media);
exit:
	return ret;
}
