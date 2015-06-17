/***********************************************************************
 *
 * File: utils/v4l2std/v4l2std.c
 * Copyright (c) 2005-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

#include <sys/types.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <linux/videodev2.h>

#include <mediactl/mediactl.h>
#include <mediactl/v4l2subdev.h>

#include "linux/include/linux/dvb/dvb_v4l2_export.h"
#include "linux/include/linux/stmvout.h"
#include "utils/v4l2_helper.h"

#define V4L2_DISPLAY_DRIVER_NAME	"Planes"
#define V4L2_DISPLAY_CARD_NAME		"STMicroelectronics"

#define MAIN_OUTPUT "main"
#define MAIN_OUTPUT_NAME "analog_hdout0"
#define AUX_OUTPUT "aux"
#define AUX_OUTPUT_NAME "analog_sdout0"

struct media_entity *find_source_of(struct media_device *md,
				      const char *entity)
{
	struct media_entity *me;
	struct media_pad *sink_pad = NULL;
	struct media_link *link;
	int i;

	/* Search for the entity */
	me = media_get_entity_by_name(md, entity, strlen(entity));
	if (!me) {
		printf("ERROR: Couldn't find such entity\n");
		return NULL;
	}

	/* Check if it has a sink pad */
	for (i = 0; i < me->info.pads; i++) {
		if (me->pads[i].flags == MEDIA_PAD_FL_SINK)
			sink_pad = &me->pads[i];
	}

	if (!sink_pad) {
		printf("ERROR: This entity doesn't have a sink pad\n");
		return NULL;
	}

	/* Go through links */
	for (i = 0; i < me->num_links; i++) {
		link = &me->links[i];

		/* Skip all links not related to that sink pad */
		if (link->sink != sink_pad)
			continue;

		if (link->flags & MEDIA_LNK_FL_ENABLED)
			return link->source->entity;
	}

	return NULL;
}

static void usage(void)
{
	fprintf(stderr, "Usage: v4l2std [options] resolution\n");
	fprintf(stderr, "[options]\n");
	fprintf(stderr, "\t--help, -h:	Display this message\n");
	fprintf(stderr, "\t--output, -o [%s, %s]: output to control\n",
				MAIN_OUTPUT, AUX_OUTPUT);
	fprintf(stderr, "\t\tDefault: %s\n", MAIN_OUTPUT);
	fprintf(stderr, "\t--list, -l: List supported resolutions\n");

	exit(1);
}

static struct option long_options[] = {
	{ "output"	, 1, 0, 'o' },
	{ "help"	, 0, 0, 'h' },
	{ "list"	, 0, 0, 'l' },
	{ 0, 0, 0, 0 }
};

void list_standards(int v4lfd)
{
	struct v4l2_standard standard;

	printf("Supported resolutions\n");
	standard.index = 0;
	standard.id = V4L2_STD_UNKNOWN;
	while (1) {
		if (ioctl(v4lfd, VIDIOC_ENUM_OUTPUT_STD, &standard) < 0)
			break;

		printf("\t%s\n", standard.name);
		standard.index++;
	}
}

int main(int argc, char **argv)
{
	int v4lfd;
	const char *output_name = MAIN_OUTPUT_NAME;
	int option;
	struct media_device *md;
	struct media_entity *me;
	unsigned int list_only = 0;
	unsigned int new_std = 0;

	v4l2_std_id stdid_c = V4L2_STD_UNKNOWN;
	v4l2_std_id stdid = V4L2_STD_UNKNOWN;
	struct v4l2_standard standard;
	unsigned int graphicsalpha;

	graphicsalpha = 255;	/* Default gfx alpha is 255 for full opaque */

	while ((option = getopt_long (argc, argv,
				      "o:hl", long_options,
				      NULL)) != -1) {
		switch (option) {
		case 'o':
			if (!strcmp(optarg, MAIN_OUTPUT))
				output_name = MAIN_OUTPUT_NAME;
			else if (!strcmp(optarg, AUX_OUTPUT))
				output_name = AUX_OUTPUT_NAME;
			else {
				fprintf(stderr, "Unknown output %s\n", optarg);
				usage();
			}
			break;
		case 'h':
			usage();
			break;
		case 'l':
			list_only = 1;
			break;
		default:
			fprintf(stderr, "Unknown option\n");
			usage();
		}
	}

	if (!list_only && (optind <= argc - 1)) {
		/* Contain a new standard information */
		new_std = 1;
	}

	/*
	 * Open the V4L2 device
	 */
	v4lfd = v4l2_open_by_name(V4L2_DISPLAY_DRIVER_NAME,
				  V4L2_DISPLAY_CARD_NAME, O_RDWR);
	if (v4lfd < 0) {
		perror("Unable to open video device");
		goto exit;
	}

	/*
	 * Open the media controller device
	 */
	md = media_open("/dev/media0");
	if (!md) {
		perror("Unable to open the media controller device");
		goto exit;
	}

	/*
	 * Search for a plane attached to the requested output
	 */
	me = find_source_of(md, output_name);
	if (!me) {
		fprintf(stderr, "Unable to access the output\n");
		goto exit;
	}

	if (v4l2_set_output_by_name(v4lfd, me->info.name) < 0) {
		perror("Unable to select a plane for control");
		goto exit;
	}

	if (list_only) {
		list_standards(v4lfd);
		exit(0);
	}

	/* Get Current display standard */
	if (LTV_CHECK(ioctl(v4lfd, VIDIOC_G_OUTPUT_STD, &stdid_c)) < 0)
		goto exit;

	standard.index = 0;
	standard.id = V4L2_STD_UNKNOWN;
	do {
		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_ENUM_OUTPUT_STD, &standard)) <
		    0)
			goto exit;

		++standard.index;
	} while ((standard.id & stdid_c) != stdid_c);

	printf("Current display standard is '%s'\n", standard.name);

	/* TODO: That is ugly */
	/* Don't do anything if we have nothing to set */
	if (!new_std)
		goto exit;

	/* Search for the standard ID of the requested standard */
	standard.index = 0;
	standard.id = V4L2_STD_UNKNOWN;
	while (1) {
		if (ioctl(v4lfd, VIDIOC_ENUM_OUTPUT_STD, &standard) < 0)
			break;

		if (!strcmp(argv[optind], (char *)standard.name)) {
			stdid = standard.id;
			break;
		}
		standard.index++;
	}

	if (stdid == V4L2_STD_UNKNOWN) {
		fprintf(stderr, "Unsupported resolution\n");
		exit(0);
	}

	/* Set New display standard */
	if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_OUTPUT_STD, &stdid)) < 0)
		goto exit;

	if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_OUTPUT_ALPHA, &graphicsalpha)) < 0)
		goto exit;

	/* Get New display standard */
	standard.index = 0;
	standard.id = V4L2_STD_UNKNOWN;
	do {
		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_ENUM_OUTPUT_STD, &standard)) <
		    0)
			goto exit;

		++standard.index;
	} while ((standard.id & stdid) != stdid);

	printf("New display standard is '%s'\n", standard.name);

exit:
	close(v4lfd);
	return 0;
}
