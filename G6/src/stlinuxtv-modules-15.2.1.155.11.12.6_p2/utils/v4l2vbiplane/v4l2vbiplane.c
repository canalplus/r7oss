/***********************************************************************
 *
 * File: linux/tests/v4l2vbi/v4l2vbiplane.c
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

#include "cgmshd.h"

//#define USE_AUTO_MODE_FOR_IOWINDOWS

#define unlikely(x)     __builtin_expect(!!(x),0)
#define likely(x)       __builtin_expect(!!(x),1)

#define printf_standard(std)\
          fprintf(stderr,"\t                 %#.16llx - %s\n", std, #std)

static const struct option long_options[] = {
        { "help"               , no_argument      , 0, 'h' },
        { "verbose"            , no_argument      , 0, 'e' },
        { "iterations"         , required_argument, 0, 'i' },
        { "videodev"           , required_argument, 0, 'v' },
        { "plane"              , required_argument, 0, 'w' },
        { "std"                , required_argument, 0, 't' },
        { "data"               , required_argument, 0, 'd' },
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
    fprintf(stderr,"Usage: v4l2stream [options]\n");
    fprintf(stderr,"\t-h         - Show this message\n");
    fprintf(stderr,"\t-e         - vErbose\n");
    fprintf(stderr,"\t-i num     - Do num interations, default is 5\n");
    fprintf(stderr,"\t-v num     - Open V4L2 device /dev/videoNUM, default is 0\n");
    fprintf(stderr,"\t-w name    - select plane by name on output, default is first plane\n");
    fprintf(stderr,"\t-t std     - select V4L2_STD_... video standard\n");
    fprintf(stderr,"\t             here's a selection of some possible standards:\n");
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
    fprintf(stderr,"\t-d data     - CGMS data (TypeA needs 3 bytes / TypeB needs 17 bytes)\n");
    exit(1);
}


int main(int argc, char **argv)
{
  int           verbose;
  int           v4lfd;
  int           viddevice;
  char          vidname[32];
  const char   *plane_name = "Main-VBI";
  int           iterations;
  int           NBUFS;
  int           i;
  int           image_width;
  int           image_height;
  int           Interlaced = 0;
  int           option;
  int           option_index;

  unsigned long long frameduration;

  struct v4l2_format         fmt;
  struct v4l2_requestbuffers reqbuf;
  struct v4l2_crop           crop;
  v4l2_std_id                current_stdid = V4L2_STD_UNKNOWN;
  v4l2_std_id                stdid = V4L2_STD_UNKNOWN;
  struct v4l2_standard       standard;
  struct v4l2_cropcap        cropcap;
  struct v4l2_buffer         buffer[20];
  unsigned char             *bufferdata[20];

  struct timespec            currenttime;
  struct timeval             ptime;

  cgms_standard_t   cgms_standard;
  cgms_type_t       cgms_type = CGMS_TYPE_A;
  unsigned char     cgms_data[17] = {0x45, 0x08, 0x0a, 0xaa, 0x00, 0x55, 0x00, 0xff, 0xf7, 0x00, 0xaa, 0xdd, 0xdd, 0xff, 0xff, 0xaa, 0x01};
  short int         input_data[17];
  int               cgms_data_len = 0;

  verbose    = 0;
  viddevice  = 1; /* Default stmvout device is now video1 */
  iterations = 5;
  NBUFS      = 2;


  while((option = getopt_long (argc, argv, "hebi:v:w:t:d:", long_options, &option_index)) != -1)
  {
    switch(option)
    {
      fprintf(stdout,"option %s", long_options[option_index].name);
      case 'h':
        usage();

      case 'e':
        verbose = 1;
        break;
      case 'i':
        if(optarg == 0)
        {
          fprintf(stderr,"Missing iterations\n");
          usage();
        }

        iterations = atoi(optarg);
        if(iterations<5)
        {
          fprintf(stderr,"Require a minimum of 5 iterations\n");
          usage();
        }

        break;
      case 'b':
        cgms_type = CGMS_TYPE_B;
        break;
      case 'v':
        if(optarg == 0)
        {
          fprintf(stderr,"Missing video device number\n");
          usage();
        }

        viddevice = atoi(optarg);
        if(viddevice<0 || viddevice>255)
        {
          fprintf(stderr,"Video device number out of range\n");
          usage();
        }

        break;
      case 'w':
        if(optarg == 0)
        {
          fprintf(stderr,"Missing plane name\n");
          usage();
        }

        plane_name = optarg;

        break;
      case 't':
        if(optarg == 0)
        {
          fprintf(stderr,"Missing standard id\n");
          usage();
        }

        stdid = (v4l2_std_id) strtoull(optarg, NULL, 0);
        fprintf(stdout,"standard id is %#.16llx\n", stdid);

        break;
      case 'd':
        if(optarg == 0)
        {
          fprintf(stderr,"Missing CGMS data\n");
          usage();
        }

        memset(cgms_data, 0, sizeof(cgms_data));

        cgms_data_len = sscanf(optarg,"%hi,%hi,%hi,%hi,%hi,%hi,%hi,%hi,%hi,%hi,%hi,%hi,%hi,%hi,%hi,%hi,%hi", &input_data[0] , &input_data[1] , &input_data[2] , &input_data[3] , &input_data[4] , &input_data[5]
                                                                                                         , &input_data[6] , &input_data[7] , &input_data[8] , &input_data[9] , &input_data[10], &input_data[11]                                                                                                        , &input_data[12], &input_data[13], &input_data[14], &input_data[15], &input_data[16]);
        for (i=0;i<17;i++)
          cgms_data[i] = (unsigned char)input_data[i];
        break;

      default:
        fprintf(stderr,"Unknown option '%s'\n",(char *)option);
        usage();
    }
  }

  if(cgms_data_len == 0)
  {
    fprintf(stderr,"No CGMS data ?!\n");
    usage();
  }

  printf("\nCGMS Test:\nSelected CGMS Type is CGMS_TYPE_%c, data len=%d\n\n",
                          (cgms_type == CGMS_TYPE_A ? 'A':'B'), cgms_data_len);

  if( ((cgms_type == CGMS_TYPE_A) && (cgms_data_len !=  3))
   || ((cgms_type == CGMS_TYPE_B) && (cgms_data_len != 17)) )
  {
    fprintf(stderr,"Wrong CGMS data (cgms_data_len != %d)\n", cgms_type == CGMS_TYPE_A ? 3:17);
    exit(1);
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

  v4l2_list_outputs (v4lfd);
  if (!plane_name) {
      i = v4l2_set_output_by_index (v4lfd, 0);
  }
  else
    i = v4l2_set_output_by_name (v4lfd, plane_name);

  if (i == -1)
  {
    perror("Unable to set video output");
    goto exit;
  }

  if (stdid == V4L2_STD_UNKNOWN)
  {
    /* if no standard was requested... */
    if(LTV_CHECK (ioctl(v4lfd,VIDIOC_G_OUTPUT_STD,&stdid))<0)
      goto exit;
    if (stdid == V4L2_STD_UNKNOWN)
    {
      /* and no standard is currently set on the display pipeline, we just
         pick the first standard listed, whatever that might be. */
      standard.index = 0;
      if(LTV_CHECK (ioctl(v4lfd,VIDIOC_ENUM_OUTPUT_STD,&standard))<0)
        goto exit;
      stdid = standard.id;
    }
  }
  if(LTV_CHECK (ioctl(v4lfd,VIDIOC_S_OUTPUT_STD,&stdid))<0)
    goto exit;

  standard.index = 0;
  standard.id    = V4L2_STD_UNKNOWN;
  do {
    if(LTV_CHECK (ioctl(v4lfd,VIDIOC_ENUM_OUTPUT_STD,&standard))<0)
      goto exit;

    ++standard.index;
  } while((standard.id & stdid) != stdid);

  memset(&cropcap, 0, sizeof(cropcap));
  cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  if(LTV_CHECK (ioctl(v4lfd,VIDIOC_CROPCAP,&cropcap))<0)
    goto exit;

  image_width   = cropcap.bounds.width;

  if(image_width == 720)
  {
    stdid = (V4L2_STD_480P_59_94 | V4L2_STD_480P_60);
    cgms_standard = (cgms_type == CGMS_TYPE_A? CGMS_HD_480P60000 : CGMS_TYPE_B_HD_480P60000);
  }
  else if(image_width == 1280)
  {
    stdid = (V4L2_STD_720P_59_94 | V4L2_STD_720P_60);
    cgms_standard = (cgms_type == CGMS_TYPE_A? CGMS_HD_720P60000 : CGMS_TYPE_B_HD_720P60000);
    image_width += 28;
  }
  else if(image_width == 1920)
  {
    stdid = (V4L2_STD_1080I_59_94 | V4L2_STD_1080I_60);
    cgms_standard = (cgms_type == CGMS_TYPE_A? CGMS_HD_1080I60000 : CGMS_TYPE_B_HD_1080I60000);
  }
  else
  {
    perror("Invalid display mode??\n");
    goto exit;
  }

  if(LTV_CHECK (ioctl(v4lfd,VIDIOC_G_OUTPUT_STD,&current_stdid))<0)
    goto exit;

  if((stdid & current_stdid) == 0)
  {
    perror("No CGMS std for this display mode\n");
    goto exit;
  }

  if (current_stdid == V4L2_STD_UNKNOWN)
  {
    if(LTV_CHECK (ioctl(v4lfd,VIDIOC_S_OUTPUT_STD,&stdid))<0)
      goto exit;
  }

  standard.index = 0;
  standard.id    = V4L2_STD_UNKNOWN;
  do {
    if(LTV_CHECK (ioctl(v4lfd,VIDIOC_ENUM_OUTPUT_STD,&standard))<0)
      goto exit;

    ++standard.index;
  } while((standard.id & stdid) == 0);
  printf("Current display standard is '%s'\n",standard.name);

  Interlaced = ((standard.id & V4L2_STD_INTERLACED) != 0);
  printf("%s content\n",(Interlaced?"Interlaced":"Progressive"));

  frameduration = ((double)standard.frameperiod.numerator/(double)standard.frameperiod.denominator)*1000000.0;
  printf("Frameduration = %lluus\n",frameduration);

  image_height  = 1;
  if(Interlaced)
    image_height *= 2;

  /*
   * Set display sized buffer format
   */
  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR32;

  fmt.fmt.pix.colorspace  = V4L2_COLORSPACE_SMPTE170M;
  fmt.fmt.pix.field       = (Interlaced? V4L2_FIELD_INTERLACED: V4L2_FIELD_NONE);
  fmt.fmt.pix.width     = image_width;
  fmt.fmt.pix.height    = image_height;
  fmt.fmt.pix.sizeimage = fmt.fmt.pix.width  * 4 * fmt.fmt.pix.height;

  /* set buffer format */
  if(LTV_CHECK (ioctl(v4lfd,VIDIOC_S_FMT,&fmt))<0)
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
  reqbuf.count  = NBUFS;

  /* request buffers */
  if(LTV_CHECK (ioctl (v4lfd, VIDIOC_REQBUFS, &reqbuf))<0)
    goto exit;

  if(reqbuf.count < NBUFS)
  {
    perror("Unable to allocate all buffers");
    goto exit;
  }

  /*
   * Work out the presentation time of the first buffer to be queued.
   * Note the use of the posix monotomic clock NOT the adjusted wall clock.
   */
  clock_gettime(CLOCK_MONOTONIC,&currenttime);

  if (verbose)
    printf("Current time = %ld.%ld\n",currenttime.tv_sec,currenttime.tv_nsec/1000);
  ptime.tv_sec  = currenttime.tv_sec;
  ptime.tv_usec = currenttime.tv_nsec / 1000;


  /*
   * Query all the buffers from the driver.
   */
  for(i=0;i<NBUFS;i++)
  {
    memset(&buffer[i], 0, sizeof(struct v4l2_buffer));
    buffer[i].index = i;
    buffer[i].type  = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    /* query buffer */
    if(LTV_CHECK (ioctl (v4lfd, VIDIOC_QUERYBUF, &buffer[i])) < 0)
      goto exit;

    buffer[i].field = (Interlaced? V4L2_FIELD_INTERLACED:V4L2_FIELD_NONE);
    bufferdata[i] = (unsigned char *)mmap (0, buffer[i].length, PROT_READ|PROT_WRITE, MAP_SHARED, v4lfd, buffer[i].m.offset);

    if(bufferdata[i] == ((void*)-1))
    {
      perror("mmap buffer failed\n");
      goto exit;
    }

    if (verbose)
      printf("creating buffer %d\n",i);

    CGMSHD_DrawWaveform(cgms_standard, (unsigned char *)cgms_data, fmt.fmt.pix.bytesperline, (unsigned long *)bufferdata[i]);
  }

  /* start stream */
  if(verbose)
      printf("Starting stream \n");

  if(LTV_CHECK (ioctl (v4lfd, VIDIOC_STREAMON, &buffer[0].type)) < 0)
    goto exit;

#ifdef USE_AUTO_MODE_FOR_IOWINDOWS
  printf("Using Automatic MODE for Input/Output Windows:\n");
#endif

  /*
   * Set the buffer crop to be the full image.
   */
  memset(&crop, 0, sizeof(struct v4l2_crop));
  crop.type     = V4L2_BUF_TYPE_PRIVATE;
#if !defined(USE_AUTO_MODE_FOR_IOWINDOWS)
  crop.c.left   = 0;
  crop.c.top    = 0;
  crop.c.width  = image_width;
  crop.c.height = image_height;
#endif
  if (LTV_CHECK (ioctl(v4lfd,VIDIOC_S_CROP,&crop)) < 0)
    goto streamoff;

  /* get the buffer crop */
  if (LTV_CHECK (ioctl(v4lfd,VIDIOC_G_CROP,&crop)) < 0)
    goto streamoff;

  printf("Source Image Dimensions (%d,%d) (%d x %d)\n",crop.c.left, crop.c.top, crop.c.width, crop.c.height);

  /*
   * Set the output crop to be the full screen
   */
  memset(&crop, 0, sizeof(struct v4l2_crop));
  crop.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT;
#if !defined(USE_AUTO_MODE_FOR_IOWINDOWS)
  if((cgms_standard == CGMS_HD_720P60000) || (cgms_standard == CGMS_TYPE_B_HD_720P60000))
    crop.c.left = -28;
  else
    crop.c.left = 0;

  if(cgms_type == CGMS_TYPE_B)
    crop.c.top  = -3;
  else
    crop.c.top  = -2;

  if(Interlaced)
    crop.c.top  *= 2;

  crop.c.width  = image_width;
  crop.c.height = image_height;
#endif
  if (LTV_CHECK (ioctl(v4lfd,VIDIOC_S_CROP,&crop)) < 0)
    goto streamoff;

  /* get output crop */
  if (LTV_CHECK (ioctl(v4lfd,VIDIOC_G_CROP,&crop)) < 0)
    goto streamoff;

  printf("Output Image Dimensions (%d,%d) (%d x %d)\n",crop.c.left, crop.c.top, crop.c.width, crop.c.height);

  /*
   * Queue all the buffers up for display.
   */
  for(i=0;i<NBUFS;i++)
  {
    buffer[i].timestamp = ptime;

    /* queue buffer */
    if(LTV_CHECK (ioctl (v4lfd, VIDIOC_QBUF, &buffer[i])) < 0)
      goto exit;

    ptime.tv_usec += frameduration;
    while(ptime.tv_usec >= 1000000L)
    {
      ptime.tv_sec++;
      ptime.tv_usec -= 1000000L;
    }
  }

  for(i=0;i<iterations;i++)
  {
    struct v4l2_buffer dqbuffer;
    long long time1;
    long long time2;
    long long timediff;

    /*
     * To Dqueue a streaming memory buffer you must set the buffer type
     * AND and the memory type correctly, otherwise the API returns an error.
     */
    dqbuffer.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    dqbuffer.memory = V4L2_MEMORY_MMAP;

    /*
     * We didn't open the V4L2 device non-blocking so dequeueing a buffer will
     * sleep until a buffer becomes available.
     */
    if(LTV_CHECK (ioctl (v4lfd, VIDIOC_DQBUF, &dqbuffer)) < 0)
      goto streamoff;

    time1 = (buffer[dqbuffer.index].timestamp.tv_sec*1000000LL) + buffer[dqbuffer.index].timestamp.tv_usec;
    time2 = (dqbuffer.timestamp.tv_sec*1000000LL) + dqbuffer.timestamp.tv_usec;

    timediff = time2 - time1;

    if (verbose)
      printf ("Buffer%d: wanted %ld.%06ld, actual %ld.%06ld, diff = %lld us\n",
              dqbuffer.index, buffer[dqbuffer.index].timestamp.tv_sec,
              buffer[dqbuffer.index].timestamp.tv_usec,
              dqbuffer.timestamp.tv_sec, dqbuffer.timestamp.tv_usec, timediff);

    ptime.tv_usec += frameduration;
    if (unlikely(i == 0))
    {
      /* after the first frame came back, we know when exactly the VSync is
         happening, so adjust the presentation time - only once, though! */
      ptime.tv_usec += timediff;
    }
    while(ptime.tv_usec >= 1000000L)
    {
      ptime.tv_sec++;
      ptime.tv_usec -= 1000000L;
    }

    buffer[dqbuffer.index].timestamp = ptime;

    /* queue buffer */
    if(LTV_CHECK (ioctl (v4lfd, VIDIOC_QBUF, &buffer[dqbuffer.index])) < 0)
      goto streamoff;
  }

streamoff:
  if(verbose)
      printf("Stream off \n");

  LTV_CHECK (ioctl(v4lfd, VIDIOC_STREAMOFF, &buffer[0].type));

exit:

  /*
   * Try and turn the display off (this will fail if the display is owned
   * by the framebuffer driver)
   */
  stdid = V4L2_STD_UNKNOWN;
  if(LTV_CHECK (ioctl(v4lfd, VIDIOC_S_OUTPUT_STD,&stdid))<0)
    perror("Unable to turn display pipeline off");

  return 0;
}
