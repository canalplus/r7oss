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
#include <linux/videodev2.h>

#include "linux/include/linux/stmvout.h"
#include "utils/v4l2_helper.h"


static struct option long_options[] = {
        { "delay"              , 1, 0, 'd' },
        { "help"               , 0, 0, 'h' },
        { "iterations"         , 1, 0, 'i' },
        { "vbidev"             , 1, 0, 'v' },
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
  fprintf(stderr,"Usage: v4l2cc [options]\n");
  fprintf(stderr,"\t-d,--delay num       - Delay of first buffer in seconds, default is 0\n");
  fprintf(stderr,"\t-h,--help            - Display this message\n");
  fprintf(stderr,"\t-i,--iterations num  - Do num interations, default is 5\n");
  fprintf(stderr,"\t-v,--vbidev num    - Open V4L2 device /dev/vbiNUM, default is 0\n");
  exit(1);
}

void print_services(int services)
{
  if(services == 0)
    printf("None");

  if(services & V4L2_SLICED_TELETEXT_B)
    printf("TeletextB ");
  if(services & V4L2_SLICED_CAPTION_525)
    printf("CC 525");
  if(services & V4L2_SLICED_VPS)
    printf("VPS ");
  if(services & V4L2_SLICED_WSS_625)
    printf("WSS625 ");

  printf("\n");
}

int main(int argc, char **argv)
{
  int           v4lfd;
  int           vbidevice;
  char          vbiname[32];
  int           iterations;
  int           delay;
  int           i,j;
  int           option;
  int           cc_datat_size;

  /* closed caption text 'I AM TESTING CLOSED CAPTIONS' */
  char cc_data[56]={0x092,0x0E0,0x092,0x0E0,0x049,0x004,0x097,0x0A1,0x097,\
  0x0A1,0x0C1,0x0CD,0x097,0x0A1,0x097,0x0A1,0x004,0x054,0x045,0x0D3,0x054,\
  0x049,0x0CE,0x0C7,0x097,0x0A1,0x097,0x0A1,0x004,0x043,0x04C,0x04F,0x0D3,\
  0x045,0x0C4,0x004,0x097,0x0A1,0x097,0x0A1,0x043,0x0C1,0x0D0,0x054,0x049,\
  0x04F,0x0CE,0x0D3,0x094,0x02C,0x094,0x02C,0x094,0x02F,0x094,0x02F
  };

  struct v4l2_format          fmt;
  struct v4l2_buffer          buffer;
  struct v4l2_sliced_vbi_data bufferdata[1];
  struct v4l2_sliced_vbi_cap  vbicap;
  struct v4l2_requestbuffers  reqbuf;
  struct timespec             currenttime;

  delay      = 0;
  vbidevice  = 0;
  iterations = 100;
  cc_datat_size = sizeof(cc_data);
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
         if(iterations<30)
         {
           fprintf(stderr,"Require a minimum of 30 iterations\n");
           usage();
         }
          break;
       case 'v':
         vbidevice = atoi(optarg);
         if(vbidevice<0 || vbidevice>255)
         {
           fprintf(stderr,"Vbi device number out of range\n");
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
  snprintf(vbiname, sizeof(vbiname), "/dev/vbi%d", vbidevice);
  if((v4lfd = open(vbiname, O_RDWR)) < 0)
  {
    perror("Unable to open vbi device");
    goto exit;
  }

  v4l2_list_outputs(v4lfd);

  /* Select the second mixer (Aux) */
  if ((v4l2_set_output_by_name (v4lfd, "analog_sdout0")) == -1)
    goto exit;

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
  fmt.fmt.sliced.service_lines[0][21] = V4L2_SLICED_CAPTION_525;

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
  reqbuf.count  = 1;

  /* request buffers */
  if(LTV_CHECK (ioctl (v4lfd, VIDIOC_REQBUFS, &reqbuf))<0)
    goto exit;
  memset(&buffer, 0, sizeof(buffer));
  buffer.type      = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
  buffer.memory    = V4L2_MEMORY_USERPTR;
  buffer.m.userptr = (unsigned long)bufferdata;

  memset(bufferdata, 0, sizeof(bufferdata));
  bufferdata[0].line    = 21;
  bufferdata[0].id      = V4L2_SLICED_CAPTION_525;

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

  bufferdata[0].data[0] = cc_data[0];
  bufferdata[0].data[1] = cc_data[1];

  if(LTV_CHECK (ioctl (v4lfd, VIDIOC_QBUF, &buffer)) < 0)
    goto exit;

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

     for(j=0;j<(cc_datat_size/2);j++)
     {
       /* Display closed caption text 'I AM TESTING CLOSED CAPTIONS' */
       bufferdata[0].data[0] = cc_data[2*j];
       bufferdata[0].data[1] = cc_data[2*j+1];
       /* queue buffer */
       if(LTV_CHECK (ioctl (v4lfd, VIDIOC_QBUF, &buffer)) < 0)
         goto streamoff;
     }
   }

 streamoff:
   LTV_CHECK (ioctl(v4lfd, VIDIOC_STREAMOFF, &buffer.type));
   if(close(v4lfd) < 0)
   {
     perror("Unable to close vbi device");
   }
 exit:
   return 0;
 }
