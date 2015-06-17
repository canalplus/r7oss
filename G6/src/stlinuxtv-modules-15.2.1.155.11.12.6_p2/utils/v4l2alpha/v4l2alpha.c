/***********************************************************************
 *
 * File: apps/STLinuxTV/utils/v4l2alpha/v4l2alpha.c
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

#include <getopt.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/fb.h>
#include <linux/videodev2.h>

#include "linux/include/linux/stmvout.h"
#include "utils/v4l2_helper.h"

#define V4L2_DISPLAY_DRIVER_NAME	"Planes"
#define V4L2_DISPLAY_CARD_NAME		"STMicroelectronics"

/* This is a simple program to set V4L2 VIDIOC_S_OUTPUT_ALPHA ioctl
 * (transparency for a video plane).
 */

static int
alpha_set(int videofd, int value)
{
  unsigned int alpha = value;
  int r = 0;

  r = LTV_CHECK(ioctl(videofd, VIDIOC_S_OUTPUT_ALPHA, &alpha));
  return r;
}


int
main(int    argc,
      char **argv)
{
  int   val_alpha = 255;
  int   videofd;
  char *endptr;
  int r = 0;

  videofd = LTV_CHECK(v4l2_open_by_name(V4L2_DISPLAY_DRIVER_NAME, V4L2_DISPLAY_CARD_NAME, O_RDWR));
  if (argc != 3)
  {
    printf("usage: %s outputname alpha(0..255)\n", argv[0]);
    printf("outputs:\n");
    v4l2_list_outputs(videofd);
    close(videofd);
    exit(EXIT_FAILURE);
  }

  r = v4l2_set_output_by_name(videofd, argv[1]);
  if ( r != 0)
  {
      printf("error %d opening output.\n", r);
      return r;
  }

  errno = 0;
  val_alpha = strtol(argv[2], &endptr, 10);
  if ((errno == ERANGE && (val_alpha == LONG_MAX || val_alpha == LONG_MIN))
        || (errno != 0 && val_alpha == 0)) {
      perror("strtol");
      exit (EXIT_FAILURE);
  }

  r = alpha_set(videofd, val_alpha);

  close(videofd);

  return r;
}
