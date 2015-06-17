/***********************************************************************
 *
 * File: linux/tests/picture-cfg/picture-cfg.c
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

#include <sys/types.h>
#include <sys/ioctl.h>

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>

#include <linux/fb.h>

/*
 * This test builds against the version of stmfb.h in this source tree, rather
 * than the one that is shipped as part of the kernel headers package for
 * consistency. Normal user applications should use <linux/stmfb.h>
 */
#include <linux/drivers/video/stmfb.h>

void usage(void)
{
	printf("Usage: picture-cfg [options]\n");
	printf("\t-b,--bar-enable=[yes|no]\n");
	printf("\t-f,--fb=DEVNAME (default=/dev/fb0)\n");
	printf("\t-h,--help\n");
	printf("\t-l,--letterbox-style=[none|center|top|sap14x9|sap4x3]\n");
	printf("\t-p,--picture-aspect-ratio=[unknown|4x3|16x9]\n");
	printf("\t-r,--resize=[none|horizontal|vertical|both]\n");
	printf("\t-v,--video-aspect-ratio=[unknown|4x3|16x9|14x9|gt16x9]\n");
        printf("\t-B,--bars=toplinenum,bottomlinenum,leftpixelnum,rightpixelnum\n");
}

int main(int argc, char **argv)
{
int fbfd;
char *devname       = "/dev/fb0";
int option;

struct stmfbio_picture_configuration pictureConfig = {0};

static struct option long_options[] = {
	{ "bar-enable"             , 1, 0, 'b' },
	{ "fb"                     , 1, 0, 'f' },
	{ "help"                   , 0, 0, 'h' },
	{ "letterbox-style"        , 1, 0, 'l' },
	{ "picture-aspect-ratio"   , 1, 0, 'p' },
	{ "resize"                 , 1, 0, 'r' },
	{ "video-aspect-ratio"     , 1, 0, 'v' },
	{ "bars"                   , 1, 0, 'B' },
	{ 0, 0, 0, 0 }
};


	while((option = getopt_long (argc, argv, "b:f:hl:p:r:v:B:", long_options, NULL)) != -1) {
		switch(option) {
		case 'b':
		{
			switch(optarg[0]) {
			case 'y':
			case 'Y':
				pictureConfig.bar_enable = 1;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_BAR_INFO_ENABLE;
				break;
			case 'n':
			case 'N':
				pictureConfig.bar_enable = 0;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_BAR_INFO_ENABLE;
				break;
			default:
				fprintf(stderr,"Unknown argument\n");
				exit(1);
			}
			break;
		}
		case 'B':
		{
			pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_BAR_INFO;
			if(sscanf(optarg,"%hu,%hu,%hu,%hu", &pictureConfig.top_bar_end,
							&pictureConfig.bottom_bar_start,
							&pictureConfig.left_bar_end,
							&pictureConfig.right_bar_start) != 4)
			{
				fprintf(stderr,"Invalid argument\n");
				exit(1);
			}
			break;
		}
		case 'f':
		{
			devname = optarg;
			break;
		}
		case 'h':
		{
			usage();
			break;
		}
		case 'l':
		{
			if(optarg[0] == 'n' || optarg[0] == 'n') {
				pictureConfig.letterbox_style = STMFBIO_PIC_LETTERBOX_NONE;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_LETTERBOX;
			} else if(optarg[0] == 'c' || optarg[0] == 'C') {
				pictureConfig.letterbox_style = STMFBIO_PIC_LETTERBOX_CENTER;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_LETTERBOX;
			} else if(optarg[0] == 't' || optarg[0] == 'T') {
				pictureConfig.letterbox_style = STMFBIO_PIC_LETTERBOX_TOP;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_LETTERBOX;
			} else if(!strncmp(optarg,"sap14",5)) {
				pictureConfig.letterbox_style = STMFBIO_PIC_LETTERBOX_SAP_14_9;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_LETTERBOX;
			} else if(!strncmp(optarg,"sap4",4)) {
				pictureConfig.letterbox_style = STMFBIO_PIC_LETTERBOX_SAP_4_3;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_LETTERBOX;
			} else {
				fprintf(stderr,"Unknown argument\n");
				exit(1);
			}
			break;
		}
		case 'r':
		{
			switch(optarg[0]) {
			case 'n':
			case 'N':
				pictureConfig.picture_rescale = STMFBIO_PIC_RESCALE_NONE;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_RESCALE_INFO;
				break;
			case 'h':
			case 'H':
				pictureConfig.picture_rescale = STMFBIO_PIC_RESCALE_HORIZONTAL;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_RESCALE_INFO;
				break;
			case 'v':
			case 'V':
				pictureConfig.picture_rescale = STMFBIO_PIC_RESCALE_VERTICAL;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_RESCALE_INFO;
				break;
			case 'b':
			case 'B':
				pictureConfig.picture_rescale = STMFBIO_PIC_RESCALE_BOTH;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_RESCALE_INFO;
				break;
			default:
				fprintf(stderr,"Unknown argument\n");
				exit(1);
			}
			break;
		}
		case 'p':
		{
			switch(optarg[0]) {
			case 'u':
			case 'U':
				pictureConfig.picture_aspect = STMFBIO_PIC_PICTURE_ASPECT_UNKNOWN;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_PICUTRE_ASPECT;
				break;
			case '1':
				pictureConfig.picture_aspect = STMFBIO_PIC_PICTURE_ASPECT_16_9;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_PICUTRE_ASPECT;
				break;
			case '4':
				pictureConfig.picture_aspect = STMFBIO_PIC_PICTURE_ASPECT_4_3;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_PICUTRE_ASPECT;
				break;
			default:
				fprintf(stderr,"Unknown argument\n");
				exit(1);
			}
			break;
		}
		case 'v':
		{
			if(optarg[0] == 'u' || optarg[0] == 'U') {
				pictureConfig.video_aspect = STMFBIO_PIC_VIDEO_ASPECT_UNKNOWN;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_VIDEO_ASPECT;
			} else if(!strncmp(optarg,"14",2)) {
				pictureConfig.video_aspect = STMFBIO_PIC_VIDEO_ASPECT_14_9;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_VIDEO_ASPECT;
			} else if(!strncmp(optarg,"16",2)) {
				pictureConfig.video_aspect = STMFBIO_PIC_VIDEO_ASPECT_16_9;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_VIDEO_ASPECT;
			} else if(optarg[0] == '4') {
				pictureConfig.video_aspect = STMFBIO_PIC_VIDEO_ASPECT_4_3;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_VIDEO_ASPECT;
			} else if(optarg[0] == 'g' || optarg[0] == 'G') {
				pictureConfig.video_aspect = STMFBIO_PIC_VIDEO_ASPECT_GT_16_9;
				pictureConfig.flags |= STMFBIO_PICTURE_FLAGS_VIDEO_ASPECT;
			} else {
				fprintf(stderr,"Unknown argument\n");
				exit(1);
			}
			break;
		}
		default:
		{
			usage();
			exit(1);
		}
	}
}

	if((fbfd = open(devname, O_RDWR)) < 0) {
		fprintf (stderr, "Unable to open %s: %d (%m)\n", devname, errno);
		exit(1);
	}


	if(ioctl(fbfd, STMFBIO_SET_PICTURE_CONFIG, &pictureConfig)<0)
		perror("setting picture configuration failed");

	return 0;
}
