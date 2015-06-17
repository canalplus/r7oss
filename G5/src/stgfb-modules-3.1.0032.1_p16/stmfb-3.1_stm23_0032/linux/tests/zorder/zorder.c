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
#include <linux/videodev.h>

#include "linux/drivers/video/stmfb.h"
#include "linux/drivers/media/video/stmvout.h"
#include "linux/tests/v4l2_helper.h"
 
#define unlikely(x)     __builtin_expect(!!(x),0)
#define likely(x)       __builtin_expect(!!(x),1)
#define abs(x)  ({				\
		  long long __x = (x);		\
		  (__x < 0) ? -__x : __x;	\
		})

/* Force a compilation error if condition is true, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define BUILD_BUG_ON_ZERO(e) (sizeof(char[1 - 2 * !!(e)]) - 1)
/* &a[0] degrades to a pointer: a different type from an array */
#define __must_be_array(a) \
  BUILD_BUG_ON_ZERO(__builtin_types_compatible_p(typeof(a), typeof(&a[0])))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

struct _zorder_map {
  int         value;
  const char *name;
};

/* This is a simple program to test V4L2 zorder ioctls */
static void
usage (const char * const progname)
{
  fprintf (stderr,
	   "%s: available options:\n"
	   "    --rgb0 z       z value of RGB0 plane\n"
	   "    --rgb1 z       z value of RGB1 plane\n"
	   "    --rgb2 z       z value of RGB2 plane\n"
	   "    --vid0 z       z value of VID0 plane\n"
	   "    --vid1 z       z value of VID1 plane\n"
	   "\n"
	   "    --help         this message\n",
	   progname);
}


enum _short_opts {
  opt_rgb0  = 256,
  opt_rgb1,
  opt_rgb2,
  opt_vid0,
  opt_vid1,
  opt_help  = 'h'
};

static __u32
zorder_get (int videofd, int id)
{
  struct v4l2_control cntrl;

  cntrl.id = id;

  LTV_CHECK (ioctl (videofd, VIDIOC_G_CTRL, &cntrl));
  return cntrl.value;
}

static void
zorder_set (int videofd, int id, int value)
{
  struct v4l2_control cntrl;

  cntrl.id    = id;
  cntrl.value = value;

  LTV_CHECK (ioctl (videofd, VIDIOC_S_CTRL, &cntrl));
}


int
main (int    argc,
      char **argv)
{
  int   c;
  char *endptr;
  int   z_ = -1;
  int   have_z[5] = { 0 };
  int   val_z[5];
  int   videofd;
  static const struct _zorder_map idx_to_v4l2[5] = {
    { V4L2_CID_STM_Z_ORDER_RGB0, "rgb0" },
    { V4L2_CID_STM_Z_ORDER_RGB1, "rgb1" },
    { V4L2_CID_STM_Z_ORDER_RGB2, "rgb2" },
    { V4L2_CID_STM_Z_ORDER_VID0, "vid0" },
    { V4L2_CID_STM_Z_ORDER_VID1, "vid1" }
  };

  while (1) {
    int option_index = 0;
    static const struct option long_options[] = {
      { "rgb0", required_argument, NULL, opt_rgb0 },
      { "rgb1", required_argument, NULL, opt_rgb1 },
      { "rgb2", required_argument, NULL, opt_rgb2 },
      { "vid0", required_argument, NULL, opt_vid0 },
      { "vid1", required_argument, NULL, opt_vid1 },
      { "help", required_argument, NULL, opt_help },
      { 0, 0, 0, 0}
    };

    c = getopt_long (argc, argv, "", long_options, NULL);
    if (c == -1)
      break;

    switch (c) {
    case opt_rgb0:
    case opt_rgb1:
    case opt_rgb2:
    case opt_vid0:
    case opt_vid1:
      errno = 0;
      z_ = strtol (optarg, &endptr, 10);
      if ((errno == ERANGE && (z_ == LONG_MAX || z_ == LONG_MIN))
	  || (errno != 0 && z_ == 0)) {
	perror ("strtol");
	exit (EXIT_FAILURE);
      }
      if (endptr == optarg) {
	fprintf (stderr, "No digits were found\n");
	exit (EXIT_FAILURE);
      }

      have_z[c - opt_rgb0] = 1;
      val_z[c - opt_rgb0] = z_;
      break;

    case '?':
    case opt_help:
    default:
      usage (argv[0]);
      exit (EXIT_FAILURE);
      break;
    }
  }

  videofd = LTV_CHECK (open ("/dev/video0", O_RDWR));
  v4l2_list_outputs (videofd);

  /* setting the output shouldn't be necessary, but because of stm_v4l2.ko
     this is necessary now */
  v4l2_set_output_by_name (videofd, "RGB0");

  for (c = 0; c < ARRAY_SIZE (idx_to_v4l2); ++c) {
    printf ("zorder for %s: %u\n",
	    idx_to_v4l2[c].name,
	    zorder_get (videofd, idx_to_v4l2[c].value));
  }

  if (z_ != -1) {
    /* set if at least one option was recognised */
    for (c = 0; c < ARRAY_SIZE (idx_to_v4l2); ++c) {
      if (have_z[c]) {
	printf ("setting zorder of %s to %d - press return\n",
		idx_to_v4l2[c].name, val_z[c]);
	zorder_set (videofd, idx_to_v4l2[c].value, val_z[c]);
	getchar ();
      }
    }
  }

  printf ("done\n");


  exit (EXIT_SUCCESS);
}
