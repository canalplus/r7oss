/***********************************************************************
 *
 * File: linux/tests/v4l2vbi/v4l2vbi.c
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


static struct option long_options[] = {
        { "delay"              , 1, 0, 'd' },
        { "help"               , 0, 0, 'h' },
        { "iterations"         , 1, 0, 'i' },
        { "videodev"           , 1, 0, 'v' },
        { 0, 0, 0, 0 }
};

/*
 * This is a simple program to test V4L2 sliced VBI output of EBU teletext data.
 *
 * The code is deliberately written in a linear, straight forward, way to show
 * the step by step usage of V4L2 to display EBU teletext.
 */
static void usage(void)
{
    fprintf(stderr,"Usage: v4l2vbi [options]\n");
    fprintf(stderr,"\t-d,--delay num       - Delay of first buffer in seconds, default is 0\n");
    fprintf(stderr,"\t-h,--help            - Display this message\n");
    fprintf(stderr,"\t-i,--iterations num  - Do num interations, default is 5\n");
    fprintf(stderr,"\t-v,--videodev num    - Open V4L2 device /dev/videoNUM, default is 0\n");
    exit(1);
}


void print_services(int services)
{
  if(services == 0)
    printf("None");

  if(services & V4L2_SLICED_TELETEXT_B)
    printf("TeletextB ");
  if(services & V4L2_SLICED_CAPTION_525)
    printf("CC ");
  if(services & V4L2_SLICED_VPS)
    printf("VPS ");
  if(services & V4L2_SLICED_WSS_625)
    printf("WSS625 ");

  printf("\n");
}


int main(int argc, char **argv)
{
  int           v4lfd;
  int           viddevice;
  char          vidname[32];
  int           iterations;
  int           delay;
  int           i;
  int           option;

  struct v4l2_format          fmt;
  v4l2_std_id                 stdid = V4L2_STD_UNKNOWN;
  struct v4l2_buffer          buffer;
  struct v4l2_sliced_vbi_data bufferdata[4];
  struct v4l2_sliced_vbi_cap  vbicap;
  struct v4l2_requestbuffers  reqbuf;
  struct timespec             currenttime;

  delay      = 0;
  viddevice  = 0;
  iterations = 5;

  while((option = getopt_long (argc, argv, "d:i:v:", long_options, NULL)) != -1)
  {
    switch(option)
    {
      case 'd':
        delay = atoi(optarg);
        if(delay<0)
          usage();

        break;
      case 'h':
        usage();

      case 'i':
        iterations = atoi(optarg);
        if(iterations<5)
        {
          fprintf(stderr,"Require a minimum of 5 iterations\n");
          usage();
        }

        break;
      case 'v':
        viddevice = atoi(optarg);
        if(viddevice<0 || viddevice>255)
        {
          fprintf(stderr,"Video device number out of range\n");
          usage();
        }

        break;
      default:
        fprintf(stderr,"Unknown option '%s'\n",*argv);
        usage();
    }
  }

  /*
   * Open the requested V4L2 device
   */
  snprintf(vidname, sizeof(vidname), "/dev/video%d", viddevice);
  if((v4lfd = open(vidname, O_RDWR)) < 0)
  {
    perror("Unable to open video device");
    goto exit;
  }

  if ((v4l2_set_output_by_name (v4lfd, "RGB0")) == -1)
    v4l2_set_output_by_name (v4lfd, "RGB2");

  if(LTV_CHECK (ioctl(v4lfd,VIDIOC_G_OUTPUT_STD,&stdid))<0)
    goto exit;

  if (stdid == V4L2_STD_UNKNOWN || (stdid & V4L2_STD_625_50) == 0)
  {
    stdid = V4L2_STD_PAL_G;
    if(LTV_CHECK (ioctl(v4lfd,VIDIOC_S_OUTPUT_STD,&stdid))<0)
      goto exit;
  }

  vbicap.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
  if(LTV_CHECK (ioctl(v4lfd,VIDIOC_G_SLICED_VBI_CAP,&vbicap))<0)
    goto exit;

  printf("VBI Capabilities\n");
  printf("----------------\n");
  printf("VBI services:"); print_services(vbicap.service_set);
  for(i=0;i<24;i++)
  {
    printf("Field0 Line%d:",i); print_services(vbicap.service_lines[0][i]);
  }
  for(i=0;i<24;i++)
  {
    printf("Field1 Line%d:",i); print_services(vbicap.service_lines[1][i]);
  }

  /*
   * Set format for lines 7&22 EBU digital teletext on both fields.
   */
  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
  fmt.fmt.sliced.service_set = 0;
  fmt.fmt.sliced.service_lines[0][7]  = V4L2_SLICED_TELETEXT_B;
  fmt.fmt.sliced.service_lines[0][22] = V4L2_SLICED_TELETEXT_B;
  fmt.fmt.sliced.service_lines[1][7]  = V4L2_SLICED_TELETEXT_B;
  fmt.fmt.sliced.service_lines[1][22] = V4L2_SLICED_TELETEXT_B;

  /* set buffer format */
  if(LTV_CHECK (ioctl(v4lfd,VIDIOC_S_FMT,&fmt))<0)
    goto exit;

  printf("VBI Format\n");
  printf("----------\n");
  printf("VBI services:"); print_services(fmt.fmt.sliced.service_set);
  for(i=0;i<24;i++)
  {
    printf("Field0 Line%d:",i); print_services(fmt.fmt.sliced.service_lines[0][i]);
  }
  for(i=0;i<24;i++)
  {
    printf("Field1 Line%d:",i); print_services(fmt.fmt.sliced.service_lines[1][i]);
  }

  /*
   * Request buffers to be allocated on the V4L2 device
   */
  memset(&reqbuf, 0, sizeof(reqbuf));
  reqbuf.type   = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
  reqbuf.memory = V4L2_MEMORY_USERPTR;

  /* request buffers */
  if(LTV_CHECK (ioctl (v4lfd, VIDIOC_REQBUFS, &reqbuf))<0)
    goto exit;

  memset(&buffer, 0, sizeof(buffer));
  buffer.type      = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
  buffer.memory    = V4L2_MEMORY_USERPTR;
  buffer.m.userptr = (unsigned long)bufferdata;

  /*
   * Note; that the data starts with the framing code, which is not how
   * it is specified in the V4L2 spec. This is because we need to distinguish
   * between normal and inverted EBU Teletext, which have different framing
   * codes. This seemed easier all round rather than adding another set of
   * VBI ids as the user can simply copy all of the bits out of the PES packet.
   */
  memset(bufferdata, 0, sizeof(bufferdata));
  bufferdata[0].line    = 7;
  bufferdata[0].id      = V4L2_SLICED_TELETEXT_B;
  bufferdata[0].data[0] = 0xE4;             /* Framing code */
  memset(&bufferdata[0].data[1], 0xf0, 42); /* 42bytes of data */
  /* Make the last byte more recognizable on a scope for testing purposes */
  bufferdata[0].data[42] = 0x0D;

  bufferdata[1].line    = 22;
  bufferdata[1].id      = V4L2_SLICED_TELETEXT_B;
  bufferdata[1].data[0] = 0xE4;
  memset(&bufferdata[1].data[1], 0x33, 42);
  bufferdata[1].data[42] = 0x0D;

  bufferdata[2].line    = 7;
  bufferdata[2].field   = 1;
  bufferdata[2].id      = V4L2_SLICED_TELETEXT_B;
  bufferdata[2].data[0] = 0xE4;
  memset(&bufferdata[2].data[1], 0xaa, 42);
  bufferdata[2].data[42] = 0x0D;

  bufferdata[3].line    = 22;
  bufferdata[3].field   = 1;
  bufferdata[3].id      = V4L2_SLICED_TELETEXT_B;
  bufferdata[3].data[0] = 0xE4;
  memset(&bufferdata[3].data[1], 0xff, 42);
  bufferdata[3].data[42] = 0x0D;

  if(delay > 0)
  {
    clock_gettime(CLOCK_MONOTONIC,&currenttime);
    buffer.timestamp.tv_sec  = currenttime.tv_sec + delay;
    buffer.timestamp.tv_usec = currenttime.tv_nsec / 1000;
    if(LTV_CHECK (ioctl (v4lfd, VIDIOC_QBUF, &buffer)) < 0)
      goto exit;
  }

  /*
   * Set the time to zero so all the other buffers follow the first one if
   * a delay was set.
   */
  buffer.timestamp.tv_sec  = 0;
  buffer.timestamp.tv_usec = 0;

  /*
   * Queue buffer multiple times so we can keep ahead in the dqueue/queue loop
   */
  for(i=0;i<25;i++)
  {
    if(LTV_CHECK (ioctl (v4lfd, VIDIOC_QBUF, &buffer)) < 0)
      goto exit;
  }

  /* start stream */
  if(LTV_CHECK (ioctl (v4lfd, VIDIOC_STREAMON, &buffer.type)) < 0)
    goto exit;

  for(i=0;i<iterations;i++)
  {
    struct v4l2_buffer dqbuffer;

    /*
     * To dequeue a streaming memory buffer you must set the buffer type
     * AND and the memory type correctly, otherwise the API returns an error.
     */
    dqbuffer.type   = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
    dqbuffer.memory = V4L2_MEMORY_USERPTR;

    /*
     * We didn't open the V4L2 device non-blocking so dequeueing a buffer will
     * sleep until a buffer becomes available.
     */
    if(LTV_CHECK (ioctl (v4lfd, VIDIOC_DQBUF, &dqbuffer)) < 0)
      goto streamoff;

    fprintf(stderr, "Dequeued buffer %d\n",i);

    /* queue buffer */
    if(LTV_CHECK (ioctl (v4lfd, VIDIOC_QBUF, &buffer)) < 0)
      goto streamoff;
  }

streamoff:
  LTV_CHECK (ioctl(v4lfd, VIDIOC_STREAMOFF, &buffer.type));

exit:
  return 0;
}
