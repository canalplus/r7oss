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
 * Configuring and starting dvp capture
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
#include <signal.h>
#include <mediactl/v4l2subdev.h>
#include <linux/v4l2-subdev.h>

#include "linux/stm_v4l2_hdmi_export.h"
#include "linux/stm_v4l2_export.h"
#include "linux/stm_v4l2_decoder_export.h"
#include "hdmirx-edid.h"
#include "hdmirx-capture.h"
#include "stm_v4l2_helper.h"

#define EDID_READ_BLOCKS	2
#define EDID_BLOCK_SIZE		128

struct hdmirx_monitor_data {
	bool dvp_start;
	bool aud_start;
}monitor_data;

static struct hdmirx_context hdmirx_ctx;
static pthread_t hdmirx_thread, keyboard_thread;

/**
 * hdmirx_edid_read_test() - edid read test
 */
static int hdmirx_edid_read_test()
{
	int ret = 0, i, j;
	FILE *dump_file;
	struct media_entity *hdmirx_entity;
	struct v4l2_subdev_edid edid;

	/*
	 * Open the file where to dump edid data
	 */
	dump_file = fopen("edid.dump", "w+");
	if (!dump_file) {
		printf("%s(): failed to open edid dump file\n", __func__);
		ret = errno;
		goto dump_open_failed;
	}

	/*
	 * Open hdmirx subdev
	 */
	ret = stm_v4l2_subdev_open(hdmirx_ctx.media,
				"hdmirx0", &hdmirx_ctx.hdmirx_entity);
	if (ret) {
		printf("%s(): failed to open hdmirx\n", __func__);
		goto hdmirx_open_failed;
	}
	hdmirx_entity = hdmirx_ctx.hdmirx_entity;

	/*
	 * Start the hdmirx test sequence
	 */
	memset(&edid, 0, sizeof(edid));
	edid.pad = 0;
	edid.start_block = 0;
	edid.blocks = EDID_READ_BLOCKS;
	edid.edid = malloc(edid.blocks * 128);
	if (!edid.edid) {
		printf("%s(): out of memory for edid data\n", __func__);
		ret = -ENOMEM;
		goto alloc_failed;
	}

	/*
	 * Get EDID now
	 */
	ret = ioctl(hdmirx_entity->fd, VIDIOC_SUBDEV_G_EDID, &edid);
	if (ret) {
		printf("%s(): failed to retrieve edid\n", __func__);
		goto g_edid_failed;
	}

	/*
	 * Dump EDID to the file
	 */
	fprintf(dump_file, "--------------------------------------------------------------------------\n");
	fprintf(dump_file, "Test Case 1\n");
	fprintf(dump_file, "Input Parameters : start_block = %d, blocks = %d\n", edid.start_block, EDID_READ_BLOCKS);
	fprintf(dump_file, "Output Parameters: start_block: %d, blocks = %d\n", edid.start_block, edid.blocks);
	fprintf(dump_file, "--------------------------------------------------------------------------\n");
	for (i = 0; i < edid.blocks; i++) {
		unsigned char *ptr = &edid.edid[i * 128];
		fprintf(dump_file, "--> Data for block: %d <--\n", i);
		for (j = 0 ; j < 128;) {
			fprintf(dump_file, "0x%x \t 0x%x \t  0x%x \t  0x%x \t 0x%x \t 0x%x \t 0x%x \t 0x%x\n",
				ptr[j], ptr[j+1], ptr[j+2], ptr[j+3], ptr[j+4], ptr[j+5], ptr[j+6], ptr[j+7]);
			j += 8;
		}
	}

	/*
	 * Get EDID from block 1
	 */
	edid.start_block = 1;

	ret = ioctl(hdmirx_entity->fd, VIDIOC_SUBDEV_G_EDID, &edid);
	if (ret) {
		printf("%s(): failed to retrieve edid\n", __func__);
		goto g_edid_failed;
	}

	/*
	 * Dump EDID to the file
	 */
	fprintf(dump_file, "--------------------------------------------------------------------------\n");
	fprintf(dump_file, "Test Case 2\n");
	fprintf(dump_file, "Input Parameters : start_block = %d, blocks = %d\n", edid.start_block, EDID_READ_BLOCKS);
	fprintf(dump_file, "Output Parameters: start_block: %d, blocks = %d\n", edid.start_block, edid.blocks);
	fprintf(dump_file, "--------------------------------------------------------------------------\n");
	for (i = 0; i < edid.blocks; i++) {
		unsigned char *ptr = &edid.edid[i * 128];
		fprintf(dump_file, "--> Data for block: %d <--\n", i);
		for (j = 0 ; j < 128;) {
			fprintf(dump_file, "0x%x \t 0x%x \t  0x%x \t  0x%x \t 0x%x \t 0x%x \t 0x%x \t 0x%x\n",
				ptr[j], ptr[j+1], ptr[j+2], ptr[j+3], ptr[j+4], ptr[j+5], ptr[j+6], ptr[j+7]);
			j += 8;
		}
	}

g_edid_failed:
	free(edid.edid);
alloc_failed:
	stm_v4l2_subdev_close(hdmirx_ctx.hdmirx_entity);
	hdmirx_ctx.hdmirx_entity = NULL;
hdmirx_open_failed:
	fclose(dump_file);
dump_open_failed:
	return ret;
}

/**
 * hdmirx_edid_write_test() - do an edid write on hdmirx
 */
static int hdmirx_edid_write_test()
{
	int ret = 0;
	struct media_entity *hdmirx_entity;
	struct v4l2_subdev_edid set_edid, get_edid;

	/*
	 * Open hdmirx subdev
	 */
	ret = stm_v4l2_subdev_open(hdmirx_ctx.media,
				"hdmirx0", &hdmirx_ctx.hdmirx_entity);
	if (ret) {
		printf("%s(): failed to open hdmirx\n", __func__);
		goto hdmirx_open_failed;
	}
	hdmirx_entity = hdmirx_ctx.hdmirx_entity;

	/*
	 * We want to set the EDID and test
	 */
	memset(&set_edid, 0, sizeof(set_edid));
	set_edid.pad = 0;
	set_edid.start_block = 0;
	set_edid.blocks = sizeof(hdmirx_edid)/EDID_BLOCK_SIZE;
	set_edid.edid = hdmirx_edid;
	ret = ioctl(hdmirx_entity->fd, VIDIOC_SUBDEV_S_EDID, &set_edid);
	if (ret) {
		printf("%s(): cannot set edid on block: %d\n", __func__, set_edid.blocks);
		goto s_edid_done;
	}

	/*
	 * Read back the edid and comapre
	 */
	memset(&get_edid, 0, sizeof(get_edid));
	get_edid.pad = 0;
	get_edid.start_block = 0;
	get_edid.blocks = sizeof(hdmirx_edid)/EDID_BLOCK_SIZE;
	get_edid.edid = malloc(get_edid.blocks * 128);
	if (!get_edid.edid) {
		printf("%s(): memory allocation for edid blocks\n", __func__);
		ret = -ENOMEM;
		goto s_edid_done;
	}
	ret = ioctl(hdmirx_entity->fd, VIDIOC_SUBDEV_G_EDID, &get_edid);
	if (ret) {
		printf("%s(): cannot get edid \n", __func__);
		goto s_edid_done;
	}

	/*
	 * Compare now
	 */
	if (!memcmp(get_edid.edid, set_edid.edid, (get_edid.blocks * 128) - 1))
		printf("%s(): Comparison okay, and test passed\n", __func__);

s_edid_done:
	stm_v4l2_subdev_close(hdmirx_ctx.hdmirx_entity);
	hdmirx_ctx.hdmirx_entity = NULL;
hdmirx_open_failed:
	return ret;
}

/**
 * hdmirx_configure_video_repeater() - configure video decoder for a specific repeater mode
 */
static int hdmirx_configure_decoder(char *decoder_name, int mode)
{
	int ret = 0, is_vid = 0;
	struct v4l2_control ctrl;
	struct media_entity *decoder_entity;

	/*
	 * Get the decoder subdev
	 */
	if (!strncmp(decoder_name, "v4l2.video", strlen("v4l2.video"))) {

		ret = stm_v4l2_subdev_open(hdmirx_ctx.media,
					decoder_name, &hdmirx_ctx.viddec_entity);
		if (ret) {
			printf("%s(): failed to open %s decoder\n", __func__, decoder_name);
			goto decoder_open_failed;
		}
		decoder_entity = hdmirx_ctx.viddec_entity;
		is_vid = 1;

	} else if (!strncmp(decoder_name, "v4l2.audio", strlen("v4l2.audio"))) {

		ret = stm_v4l2_subdev_open(hdmirx_ctx.media,
						decoder_name, &hdmirx_ctx.auddec_entity);
		if (ret) {
			printf("%s(): failed to open %s decoder\n", __func__, decoder_name);
			goto decoder_open_failed;
		}

		decoder_entity = hdmirx_ctx.auddec_entity;
	} else {

		printf("\nUse --dec-id or -d option before asking for repeater mode setting\n\n");
		ret = -EINVAL;
		goto decoder_open_failed;
	}

	/*
	 * Set the requested control
	 */
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_MPEG_STI_HDMI_REPEATER_MODE;
	ctrl.value = mode;

	ret = ioctl(decoder_entity->fd, VIDIOC_S_CTRL, &ctrl);
	if (ret) {
		printf("%s(): failed to set control\n", __func__);
		goto s_ctrl_failed;
	}

	return 0;

s_ctrl_failed:
	if (is_vid) {
		stm_v4l2_subdev_close(hdmirx_ctx.viddec_entity);
		hdmirx_ctx.viddec_entity = NULL;
	} else {
		stm_v4l2_subdev_close(hdmirx_ctx.auddec_entity);
		hdmirx_ctx.auddec_entity = NULL;
	}
decoder_open_failed:
	return ret;
}

/**
 * hdmirx_start_dvp_capture() - configure and start video capture
 */
static int hdmirx_start_dvp_capture(bool start)
{
	int ret = 0;
	struct v4l2_mbus_framefmt dvp_fmt;
	struct media_entity *decoder_entity;

	if (start)
		printf("Configure and start DVP capture\n");
	else
		printf("Configure DVP capture\n");

	/*
	 * Open dvp subdevice
	 */
	ret = stm_v4l2_subdev_open(hdmirx_ctx.media,
					"dvp0", &hdmirx_ctx.dvp_entity);
	if (ret) {
		printf("%s(): failed to open dvp0 subdevice\n", __func__);
		goto dvp_open_failed;
	}

	if (hdmirx_ctx.dvp_mbus_code) {

		/*
		 * Get the format and modify the mbus code.
		 */
		ret = v4l2_subdev_get_format(hdmirx_ctx.dvp_entity,
				&dvp_fmt, 1, V4L2_SUBDEV_FORMAT_ACTIVE);
		if (ret) {
			printf("%s(): failed to get dvp capture format\n", __func__);
			goto get_fmt_failed;
		}

		/*
		 * If there's no valid dvp fmt, ignore it, else try setting it
		 */
		dvp_fmt.code = hdmirx_ctx.dvp_mbus_code;
		ret = v4l2_subdev_set_format(hdmirx_ctx.dvp_entity,
				&dvp_fmt, 1, V4L2_SUBDEV_FORMAT_ACTIVE);
		if (ret) {
			printf("%s(): failed to set dvp format and start capture\n", __func__);
			goto get_fmt_failed;
		}
	}

	/*
	 * Open video decoder subdevice
	 */
	ret = stm_v4l2_subdev_open(hdmirx_ctx.media,
				hdmirx_ctx.viddec_name, &hdmirx_ctx.viddec_entity);
	if (ret) {
		printf("%s(): failed to open video decoder\n", __func__);
		goto get_fmt_failed;
	}
	decoder_entity = hdmirx_ctx.viddec_entity;

	/*
	 * Start the video decoder
	 */
	if (start) {
		ret = ioctl(decoder_entity->fd, VIDIOC_SUBDEV_STI_STREAMON, NULL);
		if (ret) {
			printf("%s(): failed to streamon\n", __func__);
			goto s_ctrl_failed;
		}
	}

	return 0;

s_ctrl_failed:
	stm_v4l2_subdev_close(hdmirx_ctx.viddec_entity);
	hdmirx_ctx.viddec_entity = NULL;
get_fmt_failed:
	stm_v4l2_subdev_close(hdmirx_ctx.dvp_entity);
	hdmirx_ctx.dvp_entity = NULL;
dvp_open_failed:
	return ret;
}

static int hdmirx_start_audio_capture(bool start)
{
	int ret = 0;
	struct media_entity *decoder_entity;

	if (start)
		printf("Configure and start Audio capture\n");
	else
		printf("Configure Audio capture\n");

	/*
	 * Open audio decoder
	 */
	ret = stm_v4l2_subdev_open(hdmirx_ctx.media,
			hdmirx_ctx.auddec_name, &hdmirx_ctx.auddec_entity);
	if (ret) {
		printf("%s(): failed to open %s subdevice\n", __func__, hdmirx_ctx.auddec_name);
		goto auddec_open_failed;
	}

	decoder_entity = hdmirx_ctx.auddec_entity;

	/*
	 * Start the audio decoder
	 */
	if (start) {
		ret = ioctl(decoder_entity->fd, VIDIOC_SUBDEV_STI_STREAMON, NULL);
		if (ret) {
			printf("%s(): failed to streamon\n", __func__);
			goto s_ctrl_failed;
		}
	}

	return 0;

s_ctrl_failed:
	stm_v4l2_subdev_close(hdmirx_ctx.auddec_entity);
	hdmirx_ctx.auddec_entity = NULL;
auddec_open_failed:
	return ret;
}

/**
 * hdmirx_capture_start() - start hdmirx capture
 */
static int hdmirx_capture_start()
{
	int ret = 0;

	/*
	 * Start Video capture
	 */
	ret = hdmirx_start_dvp_capture(false);
	if (ret) {
		printf("%s(): failed to configure dvp for capture\n", __func__);
		goto start_done;
	}

	/*
	 * Start audio capture
	 */
	ret = hdmirx_start_audio_capture(false);
	if (ret)
		printf("%s(): failed to configure for audio capture\n", __func__);

start_done:
	return ret;
}

/**
 * hdmirx_monitor() - hdmirx monitor thread
 */
static void *hdmirx_monitor(void *arg)
{
	struct media_entity *hdmirx_entity;
	struct v4l2_event_subscription subs;
	struct v4l2_event event;
	struct pollfd hdmirx_pollfd;
	int ret;

	/*
	 * Get the hdmirx entity
	 */
	hdmirx_entity = media_get_entity_by_name(hdmirx_ctx.media,
					"hdmirx0", strlen("hdmirx0"));
	if (!hdmirx_entity) {
		printf("%s(): No hdmirx0 entity found\n", __func__);
		goto entity_search_failed;
	}

	/*
	 * Open the hdmirx subdev
	 */
	ret = v4l2_subdev_open(hdmirx_entity);
	if (ret) {
		printf("%s(): failed to open the dvp subdevice\n", __func__);
		goto entity_search_failed;
	}

	/*
	 * Subscribe to hdmirx events
	 * a. Hotplug (pad = 0, means subs.id = 0)
	 * b. Video format change (pad = 1, means subs.id = 1)
	 * c. Audio format change (pad = 2, means subs.id = 2)
	 */
	memset(&subs, 0, sizeof(subs));
	subs.type = V4L2_EVENT_STI_HOTPLUG;
	subs.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL;
	ret = ioctl(hdmirx_entity->fd, VIDIOC_SUBSCRIBE_EVENT, &subs);
	if (ret) {
		printf("%s(): failed to subscribe to hdmirx video property event change\n", __func__);
		goto subs_failed;
	}

	memset(&subs, 0, sizeof(subs));
	subs.id = 0;
	subs.type = V4L2_EVENT_STI_HDMI_SIG;
	subs.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL;
	ret = ioctl(hdmirx_entity->fd, VIDIOC_SUBSCRIBE_EVENT, &subs);
	if (ret) {
		printf("%s(): failed to subscribe to hdmirx signal stability event\n", __func__);
		goto subs_failed;
	}

	if (monitor_data.dvp_start) {
		memset(&subs, 0, sizeof(subs));
		subs.id = 1;
		subs.type = V4L2_EVENT_SOURCE_CHANGE;
		subs.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL;
		ret = ioctl(hdmirx_entity->fd, VIDIOC_SUBSCRIBE_EVENT, &subs);
		if (ret) {
			printf("%s(): failed to subscribe to hdmirx video property event change\n", __func__);
			goto subs_failed;
		}
	}

	if (monitor_data.aud_start) {
		memset(&subs, 0, sizeof(subs));
		subs.id = 2;
		subs.type = V4L2_EVENT_SOURCE_CHANGE;
		subs.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL;
		ret = ioctl(hdmirx_entity->fd, VIDIOC_SUBSCRIBE_EVENT, &subs);
		if (ret) {
			printf("%s(): failed to subscribe to hdmirx video property event change\n", __func__);
			goto subs_failed;
		}
	}

	while (1) {

		memset(&hdmirx_pollfd, 0, sizeof(hdmirx_pollfd));
		hdmirx_pollfd.fd = hdmirx_entity->fd;
		hdmirx_pollfd.events = POLLIN | POLLPRI;

		if (poll(&hdmirx_pollfd, 1, 100)) {

			if (hdmirx_pollfd.revents & POLLERR) {
				goto subs_failed;
			}
		} else
			continue;

		memset(&event, 0, sizeof(event));
		ret = ioctl(hdmirx_entity->fd, VIDIOC_DQEVENT, &event);
		if (ret) {
			printf("%s(): failed to dq hdmirx event\n", __func__);
			goto subs_failed;
		}

		if (event.type == V4L2_EVENT_STI_HOTPLUG) {
			if (event.u.data[0] == 1)
				printf("Hotplug detected: Status IN\n");
			else
				printf("Hotplug detected: Status OUT\n");
		}

		if ((event.id == 0) && (event.type == V4L2_EVENT_STI_HDMI_SIG)) {
			if (event.u.data[0] == 1)
				printf("Signal Detected... ~~~~~~\n");
			else
				printf("Signal Lost... XXXXXXX\n");
		}

		if ((event.id == 1) && (event.type == V4L2_EVENT_SOURCE_CHANGE) &&
			(event.u.src_change.changes == V4L2_EVENT_SRC_CH_RESOLUTION)) {
			ret = hdmirx_start_dvp_capture(true);
			if (ret) {
				printf("%s(): failed to reconfigure dvp\n", __func__);
				goto subs_failed;
			}
		}

		if ((event.id == 2) && (event.type == V4L2_EVENT_SOURCE_CHANGE) &&
			(event.u.src_change.changes == V4L2_EVENT_SRC_CH_RESOLUTION)) {
			ret = hdmirx_start_audio_capture(true);
			if (ret) {
				printf("%s(): failed to reconfigure audio capture\n", __func__);
				goto subs_failed;
			}
		}
	}

subs_failed:
	v4l2_subdev_close(hdmirx_entity);
entity_search_failed:
	return NULL;
}

static void hdmirx_restore_plane()
{
	int ret;
	struct v4l2_control ctrl;

	/*
	 * Find out if we want to move the plane to AUTO mode
	 */
	if (strncmp("Main", hdmirx_ctx.plane_name, strlen("Main")))
		goto restore_done;

	/*
	 * Open plane subdev
	 */
	ret = stm_v4l2_subdev_open(hdmirx_ctx.media,
			hdmirx_ctx.plane_name, &hdmirx_ctx.plane_entity);
	if (ret) {
		printf("%s(): failed to open plane subdev\n", __func__);
		goto restore_done;
	}

	/*
	 * Move to AUTO mode
	 */
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_STM_OUTPUT_WINDOW_MODE;
	ctrl.value = VCSPLANE_AUTO_MODE;
	ret = ioctl(hdmirx_ctx.plane_entity->fd, VIDIOC_S_CTRL, &ctrl);
	if (ret)
		printf("%s(): failed to set %s output to AUTO mode\n", __func__, hdmirx_ctx.plane_name);

	stm_v4l2_subdev_close(hdmirx_ctx.plane_entity);

restore_done:
	return;
}

/**
 * hdmirx_sighandler() - signal handler to manage clean exit
 */
static void hdmirx_sighandler(int signum, siginfo_t *siginfo, void *data)
{
	switch (signum) {
	case SIGINT:
		pthread_cancel(keyboard_thread);
		pthread_cancel(hdmirx_thread);
		hdmirx_restore_plane();
		printf("\n");
		break;

	default:
		printf("Unhandled signal number: %d\n", signum);
	}
}

/**
 * usage() - help to tbe displayed
 */
static void usage()
{
	printf("hdmirx-capture version-1.0\n");
	printf("Last modified (03 June 2014)\n");
	printf("Compile info:- DATE(%s), TIME(%s)\n", __DATE__, __TIME__);
	printf("Usage: hdmirx-capture [OPTIONS]\n");
	printf("Configure and start hdmirx capture.\n");
	printf("\n");
	printf("  -d, --dec-id\n");
	printf("     The decoder id to use, it is used for audio/video\n\n");
	printf("  --start\n");
	printf("     Start the hdmirx capture, this will start both audio and video capture\n\n");
	printf("  --dvp-fmt\n");
	printf("     Set the user defined capture format for video (DVP) capture\n\n");
	printf("  --vid-repeater\n");
	printf("     Configure the mode of video decoder, 0 -> Disabled, 1 -> Repeater, 2 -> Non-repeater\n");
	printf("     NOTE: since, audio and video are in same playback, so, setting only once the repeater policy is suffice.\n");
	printf("     Two separate options are given, so, as to realize the single stream playback with specific policy\n\n");
	printf("  --aud-repeater\n");
	printf("     Configure the mode of audio decoder, 0 -> Disabled, 1 -> Repeater, 2 -> Non-repeater\n\n");
	printf("  --dvp-start\n");
	printf("     Configure and start dvp capture\n\n");
	printf("  --aud-start\n");
	printf("     Configure and start hdmirx audio capture\n\n");
	printf("  --edid-read\n");
	printf("     Do an edid read test, and dump the contents in a file (edid.dump)\n");
	printf("     (start_block = 0, blocks = %d; start_block = 1, blocks = %d\n", EDID_READ_BLOCKS, EDID_READ_BLOCKS);
	printf("     This is a test only option and must not be used in conjuction with any other\n\n");
	printf("  --edid-write\n");
	printf("     Do an edid write test, 0 for no exit and 1 for exit after the test\n");
	printf("     (start_block = 0, blocks = 2\n");
	printf("     This is a test only option with 1 value, else, it can be used in conjuction with any other\n\n");
	printf("     -----------------------------------------------------------------------------------------------\n");
	printf("     NOTE: This app is now supporting resize of the output window after the application is started.\n");
	printf("     The app follows the following syntax, for example: resize<enter>. Enter the plane name.\n");
	printf("     App will read plane.conf file placed in /root/ and will resize based on the params provided\n");
	printf("     -----------------------------------------------------------------------------------------------\n\n");
}

/*
 * main() - main entry point
 */
int main(int argc, char *argv[])
{
	int ret = -EINVAL, option;
	struct sigaction hdmirx_sigaction;

	/*
	 * Construct the arguments to be parsed by this application
	 */
	enum {
		HDMIRX_START,
		DVP_START,
		AUDIO_START,
		DVP_FORMAT,
		VIDEO_REPEATER,
		AUDIO_REPEATER,
		EDID_READ_TEST,
		EDID_WRITE_TEST,
	};

	struct option longopt[] = {
		{"dec-id", 1, NULL, 'd'},
		{"start", 0, NULL, HDMIRX_START},
		{"dvp-start", 0, NULL, DVP_START},
		{"aud-start", 0, NULL, AUDIO_START},
		{"dvp-fmt", 1, NULL, DVP_FORMAT},
		{"vid-repeater", 1, NULL, VIDEO_REPEATER},
		{"aud-repeater", 1, NULL, AUDIO_REPEATER},
		{"edid-read", 0, NULL, EDID_READ_TEST},
		{"edid-write", 1, NULL, EDID_WRITE_TEST},
		{0, 0, 0, 0}
	};

	memset(&hdmirx_sigaction, 0, sizeof(hdmirx_sigaction));
	hdmirx_sigaction.sa_sigaction = hdmirx_sighandler;
	hdmirx_sigaction.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &hdmirx_sigaction, NULL);

	/*
	 * Opens the media device
	 */
	ret = stm_v4l2_media_open(0, &hdmirx_ctx.media);
	if (ret) {
		printf("%s(): failed to open media device\n", __func__);
		goto exit;
	}

	/*
	 * Parse the arguments
	 */
	while ((option = getopt_long(argc, argv, "d:", longopt, NULL)) != -1) {

		switch(option) {
		case HDMIRX_START:
			ret = hdmirx_capture_start();
			monitor_data.dvp_start = true;
			monitor_data.aud_start = true;
			break;

		case DVP_START:

			/*
			 * It is expected that, before this application is invoked
			 * the basic connections are already done. These basic
			 * connection include:
			 * vidextin -> hdmirx (done at boot up)
			 * hdmirx->dvp (done at boot up)
			 * dvp->v4l2.video (done at boot up)
			 * v4l2.videoX->Main/Aux-Plane (Just before the application)
			 */
			ret = hdmirx_start_dvp_capture(false);
			monitor_data.dvp_start = true;
			break;

		case DVP_FORMAT:
			hdmirx_ctx.dvp_mbus_code = atoi(optarg);
			break;

		case AUDIO_START:
			ret = hdmirx_start_audio_capture(false);
			monitor_data.aud_start = true;
			break;

		case 'd':
		{
			int size;

			size = sizeof(hdmirx_ctx.viddec_name) - 1;
			snprintf(hdmirx_ctx.viddec_name, size, "v4l2.video%d", atoi(optarg));
			size = sizeof(hdmirx_ctx.auddec_name) - 1;
			snprintf(hdmirx_ctx.auddec_name, size, "v4l2.audio%d", atoi(optarg));
			break;
		}

		case VIDEO_REPEATER:
		case AUDIO_REPEATER:
		{
			int mode = atoi(optarg);

			if ((mode < 0) || (mode > 2)) {
				printf("%s(): invalide mode value\n", __func__);
				break;
			}

			switch (option) {
			case VIDEO_REPEATER:
				ret = hdmirx_configure_decoder(hdmirx_ctx.viddec_name, mode);
				break;

			case AUDIO_REPEATER:
				ret = hdmirx_configure_decoder(hdmirx_ctx.auddec_name, mode);
				break;
			}

			break;
		}

		case EDID_READ_TEST:

			ret = hdmirx_edid_read_test();
			goto media_exit;

		case EDID_WRITE_TEST:

			ret = hdmirx_edid_write_test(atoi(optarg));
			if (optarg[0] == '1')
				goto media_exit;
			break;

		default:
			printf("%s(): invalid argument\n", __func__);
			usage();
			goto media_exit;
		}
	}

	if (ret) {
		printf("%s(): make sure the relevant media-ctl commands are executed\n", __func__);
		goto media_exit;
	}

	/*
	 * Create an hdmirx monitor task to subscribe to hdmirx events
	 * and then reprogram dvp
	 */
	ret = pthread_create(&hdmirx_thread, NULL, hdmirx_monitor, NULL);
	if (ret) {
		printf("%s(): failed to create monitor thread\n", __func__);
		goto media_exit;
	}

	/*
	 * Create the keyboard thread to process the runtime options
	 */
	ret = pthread_create(&keyboard_thread, NULL, keyboard_monitor, &hdmirx_ctx);
	if (ret) {
		printf("%s(): failed to create keyboard thread\n", __func__);
		goto media_exit;
	}

	pthread_join(hdmirx_thread, NULL);
	pthread_join(keyboard_thread, NULL);

media_exit:
	stm_v4l2_media_close(hdmirx_ctx.media);
exit:
	return ret;
}
