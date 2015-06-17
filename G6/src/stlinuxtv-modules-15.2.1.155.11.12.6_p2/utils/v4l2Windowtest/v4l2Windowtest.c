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
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
 * Configuring output window dimensions of plane subdevs (Main-VID,
   Main-GDP2) etc.
************************************************************************/

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <mediactl/mediactl.h>
#include <mediactl/v4l2subdev.h>
#include "utils/v4l2_helper.h"
#include <linux/dvb/dvb_v4l2_export.h>
#include <linux/stm/stmedia_export.h>

void usage(int argc, char *argv[])
{
	printf
	    ("Usage: %s [-r] [-l <link>] [-s <pad>] [-h]"\
	    "[<left> <top> <width> <height>]", argv[0]);
	printf("\n");
	printf("-s specify the pad name to trace the connected planes \n");
	printf("<pad> is the pad name in media-ctl format. ex: '\"dvb0.video0\":0' \n");
	printf("\n");
	printf("-l specify the link name to trace the connected planes \n");
	printf("<link> is the link name in media-ctl format. ex:'\"dvb0.video0\":0->\"Main-VID\":0' \n");
	printf("\n");
	printf("-r to restore the auto mode of planes. \n");
	printf("\n");
	printf("<left> <top> <width> <height> to specify the offsets and the size of window \n");
	printf("\n");
	printf("Sample Usage assuming the dvb0.video0 link to Main-VID is enabled : \n");
	printf("ex1: v4l2Windowtest -l '\"dvb0.video0\":0->\"Main-VID\":0' 50 50 360 240\n");
	printf("ex2: v4l2Windowtest -l '\"dvb0.video0\":0->\"Main-VID\":0' -r\n");
	printf("ex3: v4l2Windowtest -s '\"dvb0.video0\":0' 50 50 360 240 \n");
	printf("For the above usage (ex3) user will be prompted to select the plane to control\n");
	printf("ex4: v4l2Windowtest -s '\"dvb0.video0\":0' -r\n");
	printf("\n");
}

struct media_entity* get_conn_links_sel_plane(struct media_device *media, const char *padname)
{
	struct media_pad *source;
	struct media_pad *sink[5];
	struct media_link *link;
	char *end;
	int i, num_of_sinks = 0, index = 0;

	source = media_parse_pad(media, padname, &end);
	if (source == NULL) {
		printf("could not parse source pad \n");
		return NULL;
	}

	for (i = 0; i < source->entity->num_links && num_of_sinks < 5; i++) {
		link = &source->entity->links[i];

		if (link->source == source && (link->flags & MEDIA_LNK_FL_ENABLED)){
			if((link->sink->entity->info.type == MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO) ||
			(link->sink->entity->info.type == MEDIA_ENT_T_V4L2_SUBDEV_PLANE_GRAPHIC))
				sink[num_of_sinks++] = link->sink;
		}
	}

	if(num_of_sinks == 0) {
		printf(" No planes enabled to the entity \n");
		return NULL;
	}

	printf(" The connected planes are : \n");
	for(i = 0; i < num_of_sinks; i++) {
		printf("\t Index: %d -> %s\n", i, sink[i]->entity->info.name);
	}

	do {
		printf("Enter the index : ");
		scanf("%d", &index);
	}while(index >= num_of_sinks);

	return sink[index]->entity;
}

struct media_entity* parse_link_ret_plane(struct media_device *media, const char *linkname)
{
	struct media_link *link;
	char *end = NULL;

	link = media_parse_link(media, linkname, &end);
	if(!link) {
		printf("No link between the the entities\n");
		return NULL;
	}

	if (!(link->flags & MEDIA_LNK_FL_ENABLED)){
		printf(" The link is not enabled \n");
		return NULL;
	}

	return link->sink->entity;
}

int restore_auto_mode(struct media_entity *plane)
{
	struct v4l2_control Control;
	int ret = 0;

	if(v4l2_subdev_open(plane)) {
		printf("error in opening subdev\n");
		v4l2_subdev_close(plane);
	}

	Control.id = V4L2_CID_STM_OUTPUT_WINDOW_MODE;
	Control.value = VCSPLANE_AUTO_MODE;

	ret = ioctl(plane->fd, VIDIOC_S_CTRL, &Control);
	if (ret < 0) {
		printf("error in setting S_CTRL\n");
	}

	printf("set the plane %s to Auto mode \n", plane->info.name);

	v4l2_subdev_close(plane);
	return 0;
}


int set_crop(struct media_entity *plane, struct v4l2_subdev_crop *crop)
{
	struct v4l2_control Control;
	int ret = 0;

	if(v4l2_subdev_open(plane)) {
		printf("error in opening subdev\n");
		goto subdev_close;
	}

	Control.id = V4L2_CID_STM_OUTPUT_WINDOW_MODE;
	Control.value = VCSPLANE_MANUAL_MODE;

	ret = ioctl(plane->fd, VIDIOC_S_CTRL, &Control);
	if (ret < 0) {
		printf("error in setting S_CTRL\n");
		goto subdev_close;
	}

	ret = ioctl(plane->fd, VIDIOC_SUBDEV_S_CROP, crop);
	if (ret < 0) {
		printf("error in setting S_CROP\n");
		goto subdev_close;
	}

	printf("set the plane %s to %d, %d, %d, %d \n", plane->info.name, crop->rect.left,
		crop->rect.top, crop->rect.width, crop->rect.height);

subdev_close:
	v4l2_subdev_close(plane);
	return 0;
}

int main (int argc, char **argv)
{
	struct media_device *media;
	struct media_entity *main_plane;
	struct v4l2_subdev_crop crop;

	int opt, restore = 0;

	media = media_open("/dev/media0");
	if(!media) {
		printf("error in opening media device\n");
		return 0;
	}

	while ((opt = getopt(argc, argv, "hs:rl:")) != -1) {
		switch (opt) {
		case 'l':
			main_plane = parse_link_ret_plane(media, optarg);
			break;
		case 'r':
			restore = 1;
			break;
		case 's':
			main_plane = get_conn_links_sel_plane(media, optarg);
			break;
		case 'h':
		case '?':
		default:
			usage(argc, argv);
			goto exit;
		}
	}

	if(!main_plane){
		printf("Error : Vout entity is NULL\n");
		goto exit;
	}

	if(restore){
		restore_auto_mode(main_plane);
		goto exit;
	}

	if (optind != (argc-4)){
		usage(argc, argv);
		goto exit;
	}

	memset(&crop, 0, sizeof(crop));

	crop.pad = 1;
	crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;

	crop.rect.left = atoi(argv[optind]);
	crop.rect.top = atoi(argv[optind + 1]);
	crop.rect.width = atoi(argv[optind + 2]);
	crop.rect.height = atoi(argv[optind + 3]);
	set_crop(main_plane, &crop);

exit:
	media_close(media);
	exit (EXIT_SUCCESS);
}
