/***********************************************************************
 *
 * File: linux/tests/zorder/zorder.c
 * Copyright (c) 2009 STMicroelectronics Limited.
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

/* This is a simple program to test V4L2 zorder ioctls */
static void
usage (const char * const progname)
{
  fprintf (stderr,
           "%s: outputname [z value]:\n",
           progname);
}


static __u32
zorder_get (int videofd)
{
  struct v4l2_control cntrl;

  cntrl.id = V4L2_CID_STM_Z_ORDER;

  LTV_CHECK (ioctl (videofd, VIDIOC_G_CTRL, &cntrl));
  return cntrl.value;
}

static void
zorder_set (int videofd, int value)
{
  struct v4l2_control cntrl;

  cntrl.id    = V4L2_CID_STM_Z_ORDER;
  cntrl.value = value;

  LTV_CHECK (ioctl (videofd, VIDIOC_S_CTRL, &cntrl));
}


int
main (int    argc,
      char **argv)
{
  int   val_z;
  int   videofd;
  char *endptr;

  if(argc<2 || argc >3)
  {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  videofd = LTV_CHECK (v4l2_open_by_name (V4L2_DISPLAY_DRIVER_NAME, V4L2_DISPLAY_CARD_NAME, O_RDWR));
  v4l2_list_outputs (videofd);

  v4l2_set_output_by_name (videofd, argv[1]);

  printf ("zorder for %s: %u\n",
          argv[1],
          zorder_get (videofd));

  if(argc == 3)
  {
    val_z = strtol (argv[2], &endptr, 10);
    if ((errno == ERANGE && (val_z == LONG_MAX || val_z == LONG_MIN))
        || (errno != 0 && val_z == 0)) {
      perror ("strtol");
      exit (EXIT_FAILURE);
    }

    zorder_set (videofd,  val_z);
  }

  exit (EXIT_SUCCESS);
}
