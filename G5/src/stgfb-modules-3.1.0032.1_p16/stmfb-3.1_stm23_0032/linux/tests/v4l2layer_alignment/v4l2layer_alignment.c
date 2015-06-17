/***********************************************************************
 *
 * File: linux/tests/v4l2layer_alignment/v4l2layer_alignment.c
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>

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

#define printf_standard(std)\
          fprintf(stderr,"\t                 %#.16llx - %s\n", std, #std)

static struct option long_options[] = {
        { "alpha"              , 1, 0, 'a' },
        { "help"               , 0, 0, 'h' },
        { "ntsc-buffer"        , 0, 0, 'n' },
        { "pal-buffer"         , 0, 0, 'p' },
        { "720p"               , 0, 0, '7' },
        { "1080p"              , 0, 0, '1' },
        { "output-standard"    , 1, 0, 't' },
        { 0, 0, 0, 0 }
};

/*
 * This is a simple program to test the registration of buffers queued on two
 * display layers (V4L2 outputs) possibly when scaled
 *
 * The code is deliberately written in a linear, straight forward, way to show
 * the step by step usage of V4L2 to display content on the screen.
 */
static void usage(void)
{
    fprintf(stderr,"Usage: v4l2layer_alignment [options]\n");
    fprintf(stderr,"\t-a,--alpha [0-255]       global alpha for graphics layer\n");
    fprintf(stderr,"\t-h,--help\n");
    fprintf(stderr,"\t-n,--ntsc-buffer         Use a NTSC sized buffer,default is screen display size\n");
    fprintf(stderr,"\t-p,--pal-buffer          Use a PAL sized buffer, default is screen display size\n");
    fprintf(stderr,"\t-7,--720p                Use a 1280x720 sized buffer\n");
    fprintf(stderr,"\t-1,--1080p               Use a 1920x1080 sized buffer\n");
    fprintf(stderr,"\t-t,--output-standard std select V4L2_STD_... video standard\n");
    fprintf(stderr,"\t            here's a selection of some possible standards:\n");
      printf_standard(V4L2_STD_VGA_60);
      printf_standard(V4L2_STD_VGA_59_94);
      printf_standard(V4L2_STD_480P_60);
      printf_standard(V4L2_STD_480P_59_94);
      printf_standard(V4L2_STD_576P_50);
      printf_standard(V4L2_STD_1080P_60);
      printf_standard(V4L2_STD_1080P_59_94);
      printf_standard(V4L2_STD_1080P_50);
      printf_standard(V4L2_STD_1080P_23_98);
      printf_standard(V4L2_STD_1080P_24);
      printf_standard(V4L2_STD_1080P_25);
      printf_standard(V4L2_STD_1080P_29_97);
      printf_standard(V4L2_STD_1080P_30);
      printf_standard(V4L2_STD_1080I_60);
      printf_standard(V4L2_STD_1080I_59_94);
      printf_standard(V4L2_STD_1080I_50);
      printf_standard(V4L2_STD_720P_60);
      printf_standard(V4L2_STD_720P_59_94);
      printf_standard(V4L2_STD_720P_50);
    exit(1);
}


void create_test_pattern(unsigned char *bufferdata, int width, int height, int stride, unsigned chroma)
{
  unsigned short *pixel = (unsigned short *)bufferdata;
  int x,y;

  for(y=0;y<height;y++)
  {
    for(x=0;x<width;x++)
    {
      int blank = ((y/8)%2==1) || ((x/8)%2==1);
      unsigned luma= blank?16:235;

      *(pixel+x) = (luma<<8) | (blank?128:chroma);
    }

    pixel += (stride/2);
  }
}

int main(int argc, char **argv)
{
  int v4lfd1;
  int v4lfd2;
  int image_width;
  int image_height;
  int option;
  unsigned int graphicsalpha;

  struct v4l2_format         fmt;
  struct v4l2_requestbuffers reqbuf;
  v4l2_std_id                stdid = V4L2_STD_UNKNOWN;
  struct v4l2_standard       standard;
  struct v4l2_cropcap        cropcap;
  struct v4l2_crop           crop;
  struct v4l2_buffer         buffer[2];
  unsigned char             *bufferdata[2];

  image_width   = 0;
  image_height  = 0;
  graphicsalpha = 128;

  while((option = getopt_long (argc, argv, "a:hnp71t:", long_options, NULL)) != -1)
  {
    switch(option)
    {
      case 'a':
        graphicsalpha = (unsigned int) strtoul(optarg, NULL, 0);
        if(graphicsalpha >255)
          usage();

        break;
      case 'h':
        usage();
        break;
      case 'n':
        image_width  = 720;
        image_height = 480;
        break;
      case 'p':
        image_width  = 720;
        image_height = 576;
        break;
      case '7':
        image_width  = 1280;
        image_height = 720;
        break;
      case '1':
        image_width  = 1920;
        image_height = 1080;
        break;
      case 't':
        stdid = (v4l2_std_id) strtoull(optarg, NULL, 0);

        break;
      default:
        fprintf(stderr,"Unknown option '%s'\n",*argv);
        usage();
    }
  }

  if((v4lfd1 = open("/dev/video0", O_RDWR)) < 0)
  {
    perror("Unable to open video device");
    goto exit;
  }

  v4l2_list_outputs (v4lfd1);
  if (v4l2_set_output_by_name (v4lfd1, "YUV0") == -1)
  {
    perror("Unable to set video output");
    goto exit;
  }

  if((v4lfd2 = open("/dev/video0", O_RDWR)) < 0)
  {
    perror("Unable to open video device");
    goto exit;
  }

  if (v4l2_set_output_by_name (v4lfd2, "RGB2") == -1)
  {
    perror("Unable to set graphics output");
    goto exit;
  }

  if (stdid == V4L2_STD_UNKNOWN)
  {
    /* if no standard was requested... */
    if(LTV_CHECK (ioctl(v4lfd1,VIDIOC_G_OUTPUT_STD,&stdid))<0)
      goto exit;
    if (stdid == V4L2_STD_UNKNOWN)
    {
      /* and no standard is currently set on the display pipeline, we just
         pick the first standard listed, whatever that might be. */
      standard.index = 0;
      if(LTV_CHECK (ioctl(v4lfd1,VIDIOC_ENUM_OUTPUT_STD,&standard))<0)
        goto exit;
      stdid = standard.id;
    }
  }
  if(LTV_CHECK (ioctl(v4lfd1,VIDIOC_S_OUTPUT_STD,&stdid))<0)
    goto exit;

  standard.index = -1;
  standard.id = -1;
  while(standard.id != stdid) {
    ++standard.index;
    if(LTV_CHECK (ioctl(v4lfd1,VIDIOC_ENUM_OUTPUT_STD,&standard))<0)
      goto exit;
  }
  printf("Current display standard is '%s'\n",standard.name);

  memset(&cropcap, 0, sizeof(cropcap));
  cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  if(LTV_CHECK (ioctl(v4lfd1,VIDIOC_CROPCAP,&cropcap))<0)
    goto exit;

  if(image_width == 0 || image_height == 0)
  {
    image_width   = cropcap.bounds.width;
    image_height  = cropcap.bounds.height;
  }

  /*
   * Set display sized buffer format
   */
  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;

  fmt.fmt.pix.colorspace  = V4L2_COLORSPACE_SMPTE170M;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;

  fmt.fmt.pix.width     = image_width;
  fmt.fmt.pix.height    = image_height;

  fmt.fmt.pix.sizeimage = fmt.fmt.pix.width  * 2 * fmt.fmt.pix.height;

  /* set buffer format */
  if(LTV_CHECK (ioctl(v4lfd1,VIDIOC_S_FMT,&fmt))<0)
    goto exit;

  if(LTV_CHECK (ioctl(v4lfd2,VIDIOC_S_FMT,&fmt))<0)
    goto exit;

  printf("Selected V4L2 pixel format:\n");
  printf("Width                = %d\n",fmt.fmt.pix.width);
  printf("Height               = %d\n",fmt.fmt.pix.height);
  printf("BytesPerLine         = %d\n",fmt.fmt.pix.bytesperline);
  printf("SizeImage            = %d\n",fmt.fmt.pix.sizeimage);
  printf("Priv (Chroma Offset) = %d\n\n",fmt.fmt.pix.priv);

  /*
   * Request buffers to be allocated on the V4L2 device
   */
  memset(&reqbuf, 0, sizeof(reqbuf));
  reqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  reqbuf.memory = V4L2_MEMORY_MMAP;
  reqbuf.count  = 1;

  /* request buffers */
  if(LTV_CHECK (ioctl (v4lfd1, VIDIOC_REQBUFS, &reqbuf))<0)
    goto exit;

  if(LTV_CHECK (ioctl (v4lfd2, VIDIOC_REQBUFS, &reqbuf))<0)
    goto exit;

  memset(&buffer[0], 0, sizeof(struct v4l2_buffer));
  buffer[0].index = 0;
  buffer[0].type  = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  /* query buffer */
  if(LTV_CHECK (ioctl (v4lfd1, VIDIOC_QUERYBUF, &buffer[0])) < 0)
    goto exit;

  buffer[0].field = V4L2_FIELD_NONE;
  bufferdata[0] = (unsigned char *)mmap (0, buffer[0].length, PROT_READ|PROT_WRITE, MAP_SHARED, v4lfd1, buffer[0].m.offset);

  if(bufferdata[0] == ((void*)-1))
  {
    perror("mmap buffer failed\n");
    goto exit;
  }

  memset(&buffer[1], 0, sizeof(struct v4l2_buffer));
  buffer[1].index = 0;
  buffer[1].type  = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  if(LTV_CHECK (ioctl (v4lfd2, VIDIOC_QUERYBUF, &buffer[1])) < 0)
    goto exit;

  buffer[1].field = V4L2_FIELD_NONE;
  bufferdata[1] = (unsigned char *)mmap (0, buffer[1].length, PROT_READ|PROT_WRITE, MAP_SHARED, v4lfd2, buffer[1].m.offset);

  if(bufferdata[1] == ((void*)-1))
  {
    perror("mmap buffer failed\n");
    goto exit;
  }

  create_test_pattern(bufferdata[0], image_width, image_height, fmt.fmt.pix.bytesperline, 230);
  create_test_pattern(bufferdata[1], image_width, image_height, fmt.fmt.pix.bytesperline, 16);

  memset(&crop, 0, sizeof(struct v4l2_crop));
  crop.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  crop.c.width  = cropcap.bounds.width;
  crop.c.height = cropcap.bounds.height;

  if (LTV_CHECK (ioctl(v4lfd1,VIDIOC_S_CROP,&crop)) < 0)
    goto exit;

  if (LTV_CHECK (ioctl(v4lfd2,VIDIOC_S_CROP,&crop)) < 0)
    goto exit;


  if(LTV_CHECK (ioctl (v4lfd1, VIDIOC_QBUF, &buffer[0])) < 0)
    goto exit;

  if(LTV_CHECK (ioctl (v4lfd1, VIDIOC_STREAMON, &buffer[0].type)) < 0)
    goto exit;


  if(LTV_CHECK (ioctl (v4lfd2, VIDIOC_S_OUTPUT_ALPHA, &graphicsalpha)) < 0)
    goto exit;

  if(LTV_CHECK (ioctl (v4lfd2, VIDIOC_QBUF, &buffer[1])) < 0)
    goto exit;

  if(LTV_CHECK (ioctl (v4lfd2, VIDIOC_STREAMON, &buffer[1].type)) < 0)
    goto exit;

  while(1);

exit:

  /*
   * Try and turn the display off (this will fail if the display is owned
   * by the framebuffer driver)
   */
  stdid = V4L2_STD_UNKNOWN;
  if(LTV_CHECK (ioctl(v4lfd1, VIDIOC_S_OUTPUT_STD,&stdid))<0)
    perror("Unable to turn display pipeline off");

  return 0;
}
