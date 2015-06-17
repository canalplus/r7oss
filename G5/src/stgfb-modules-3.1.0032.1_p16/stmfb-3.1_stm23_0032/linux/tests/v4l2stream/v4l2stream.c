/***********************************************************************
 *
 * File: linux/tests/v4l2stream/v4l2stream.c
 * Copyright (c) 2005 STMicroelectronics Limited.
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

/*
 * This is a simple program to test V4L2 buffer queueing and presentation time.
 * It also tests the 420MB display format, particularly the placement of the chroma
 * buffer when the buffer pixel format dimensions are larger than the image size.
 *
 * The code is deliberately written in a linear, straight forward, way to show
 * the step by step usage of V4L2 to display video frames.
 */
static void usage(void)
{
    fprintf(stderr,"Usage: v4l2stream [options]\n");
    fprintf(stderr,"\ta         - Adjust time to minimise presentation delay\n");
    fprintf(stderr,"\tb num     - Number of V4L buffers to use\n");
    fprintf(stderr,"\td num     - Presentation delay on first buffer by num seconds, default is 1\n");
    fprintf(stderr,"\ti num     - Do num interations, default is 5\n");
    fprintf(stderr,"\tI         - Force Interlaced buffers\n");
    fprintf(stderr,"\th         - Scale the buffer down to half size\n");
    fprintf(stderr,"\th1          - Scale the buffer down to half horizontal size and put it in the left area\n");
    fprintf(stderr,"\th2          - Scale the buffer down to half horizontal size and put it in the right area\n");
    fprintf(stderr,"\tH         - Use a 1920x1080 buffer\n");
    fprintf(stderr,"\tf m|0|v|2|r - Use 420MB, YUV420,YVU420, YUV422P or ARGB instead of 422R\n");
    fprintf(stderr,"\tl         - Use a light/dark horizontal line pattern for raster/progressive formats\n");
    fprintf(stderr,"\tn         - Use a NTSC sized buffer,default is screen display size\n");
    fprintf(stderr,"\tp         - Use a PAL sized buffer, default is screen display size\n");
    fprintf(stderr,"\tP         - Force Progressive buffers\n");
    fprintf(stderr,"\tq         - Scale the buffer down to quarter size\n");
    fprintf(stderr,"\ts         - Use a small (176x144) sized buffer\n");
    fprintf(stderr,"\tv num     - Open V4L2 device /dev/videoNUM, default is 0\n");
    fprintf(stderr,"\tw name    - select plane by name on output, default is first video plane\n");
    fprintf(stderr,"\tt std     - select V4L2_STD_... video standard\n");
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
    fprintf(stderr,"\tx num     - Crop the source image at starting at the given pixel number\n");
    fprintf(stderr,"\ty num     - Crop the source image at starting at the given line number\n");
    fprintf(stderr,"\tz         - Do a dynamic horizontal zoom out\n");
    fprintf(stderr,"\te         - vErbose\n");
    exit(1);
}


void create_macroblock_test_pattern(unsigned char *bufferdata,int width, int height, int chroma_offset, int stride)
{
  int i,flag,mbs;
  unsigned char *ptr = bufferdata;

  /*
   * Round up height to the nearest macroblock, needed for 1080 lines (to 1088)
   */
  if(height%16 != 0)
    height += 16-height%16;

  /*
   * The framebuffer organisation for macroblock format is in no way linear!
   * so while this produces a checkerboard pattern with each square being
   * one macroblock, it isn't the pattern you might at first expect. Also we
   * can't have an odd number of macroblocks and the layout means we need to
   * fill more data than you think to get the bottom line of blocks correct in
   * say 720p.
   */
  height += ((height/16)%2)*16;

  mbs = (stride*height)/(16*16);

  flag = 0;

  for(i=0;i<mbs;i++)
  {
    memset(ptr, flag?0x80:0xFF, 256);
    ptr += 256;
    flag = !flag;
  }

  ptr = bufferdata+chroma_offset;
  flag = 0;

  for(i=0;i<mbs;i++)
  {
    memset(ptr, flag?0x00:0xFF, 128);
    ptr += 128;
    flag = !flag;
  }
}


void create_planar_test_pattern(unsigned char *bufferdata, int width, int height, int chroma_offset, int chroma_height, int chroma_spacing, int stride)
{
  unsigned char *luma    = bufferdata;
  unsigned char *chroma1 = bufferdata+chroma_offset;
  unsigned char *chroma2 = chroma1+(chroma_spacing*stride/2);
  int x,y;

  printf("%dx%d, CbOff = %d, CbHeight = %d, stride = %d\n",width,height,chroma_offset,chroma_height,stride);

  for(y=0;y<height;y++)
  {
    for(x=0;x<width;x++)
    {
      *(luma+x) = x%256;
    }

    luma += stride;
  }

  for(y=0;y<chroma_height;y++)
  {
    for(x=0;x<(width/2);x++)
    {
      /*
       * Produce shades of red for YUV formats and shades
       * of blue for YVU in the top half of the screen, and
       * just the opposite in the bottom half of the screen.
       */
      if(y<(chroma_height/2))
      {
        *(chroma1+x) = 0;
        *(chroma2+x) = 255;
      }
      else
      {
        *(chroma1+x) = 255;
        *(chroma2+x) = 0;
      }
    }

    chroma1 += stride/2;
    chroma2 += stride/2;
  }

}

void create_hline_planar_test_pattern(unsigned char *bufferdata, int width, int height, int chroma_offset, int chroma_height, int chroma_spacing, int stride)
{
  unsigned char *luma    = bufferdata;
  unsigned char *chroma1 = bufferdata+chroma_offset;
  unsigned char *chroma2 = chroma1+(chroma_spacing*stride/2);
  int x,y;

  printf("%dx%d, CbOff = %d, CbHeight = %d, stride = %d\n",width,height,chroma_offset,chroma_height,stride);

  for(y=0;y<height;y++)
  {
    for(x=0;x<width;x++)
    {
      *(luma+x) = (y%2==1)?235:16;
    }

    luma += stride;
  }

  for(y=0;y<chroma_height;y++)
  {
    for(x=0;x<(width/2);x++)
    {
      if(y%2==1)
      {
        *(chroma1+x) = 0;
        *(chroma2+x) = 240;
      }
      else
      {
        *(chroma1+x) = 240;
        *(chroma2+x) = 0;
      }
    }

    chroma1 += stride/2;
    chroma2 += stride/2;
  }

}

void create_422R_test_pattern(unsigned char *bufferdata, int width, int height, int stride)
{
  unsigned short *pixel = (unsigned short *)bufferdata;
  int x,y;

  for(y=0;y<height;y++)
  {
    for(x=0;x<width;x++)
    {
      unsigned chroma=0;

      if((y<(height/2) && (x%2 == 0)) || (y>=(height/2) && (x%2 == 1)))
        chroma = 255;

      *(pixel+x) = (x%256<<8) | chroma;
    }

    pixel += (stride/2);
  }
}

void create_hline_422R_test_pattern(unsigned char *bufferdata, int width, int height, int stride)
{
  unsigned short *pixel = (unsigned short *)bufferdata;
  int x,y;

  for(y=0;y<height;y++)
  {
    for(x=0;x<width;x++)
    {
      unsigned chroma=128;
      unsigned luma= (y%2==1)?16:235;

      *(pixel+x) = (luma<<8) | chroma;
    }

    pixel += (stride/2);
  }
}


void create_rgb_test_pattern(unsigned char *bufferdata, int width, int height, int stride)
{
  unsigned long *pixel = (unsigned long *)bufferdata;
  int x,y;

  for(y=0;y<height;y++)
  {
    for(x=0;x<width;x++)
    {
      unsigned long red   = 0;
      unsigned long green = 0;
      unsigned long blue  = x%256;

      if(y<(height/2))
        green = 255;
      else
        red = 255;

      *(pixel+x) = 0xff000000 | (blue<<16) | (green<<8) | red;
    }

    pixel += (stride/4);
  }
}


void create_hline_rgb_test_pattern(unsigned char *bufferdata, int width, int height, int stride)
{
  unsigned long *pixel = (unsigned long *)bufferdata;
  int x,y;

  for(y=0;y<height;y++)
  {
    for(x=0;x<width;x++)
    {
      *(pixel+x) = (y%2==1)?0xff000000:0xffffffff;
    }

    pixel += (stride/4);
  }
}


int main(int argc, char **argv)
{
  int           verbose;
  int           v4lfd;
  int           viddevice;
  char          vidname[32];
  const char   *plane_name = NULL;
  int           progressive;
  int           iterations;
  int           delay;
  int           pixfmt;
  int           halfsize;
  int           left_half, right_half;
  int           quartersize;
  int           adjusttime;
  int           NBUFS;
  int           i;
  int           image_width;
  int           image_height;
  int           start_x;
  int           start_y;
  int           zoom;
  int           lines;

  unsigned long long frameduration;

  struct v4l2_format         fmt;
  struct v4l2_requestbuffers reqbuf;
  struct v4l2_crop           crop;
  v4l2_std_id                stdid = V4L2_STD_UNKNOWN;
  struct v4l2_standard       standard;
  struct v4l2_cropcap        cropcap;
  struct v4l2_buffer         buffer[20];
  unsigned char             *bufferdata[20];

  struct timespec            currenttime;
  struct timeval             ptime;

  argc--;
  argv++;

  verbose    = 0;
  adjusttime = 0;
  viddevice  = 0;
  pixfmt     = V4L2_PIX_FMT_UYVY;
  halfsize   = 0;
  left_half = right_half = 0;
  quartersize = 0;
  delay      = 1;
  iterations = 5;
  NBUFS      = 5;
  start_x    = 0;
  start_y    = 0;
  zoom       = 0;
  image_width = 0;
  image_height = 0;
  lines       = 0;
  progressive = -1;

  while(argc>0)
  {
    switch(**argv)
    {
      case 'e':
        verbose = 1;
        break;
      case 'a':
        adjusttime = 1;
        break;
      case 'd':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing delay\n");
          usage();
        }

        delay = atoi(*argv);
        if(delay<1)
        {
          fprintf(stderr,"Require a minimum of 1sec delay\n");
          usage();
        }

        break;
      case 'i':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing iterations\n");
          usage();
        }

        iterations = atoi(*argv);
        if(iterations<5)
        {
          fprintf(stderr,"Require a minimum of 5 iterations\n");
          usage();
        }

        break;
      case 'I':
        progressive = 0;
        break;
      case 'h':
        if ((*argv)[1] == '1')
          left_half = 1;
        else if ((*argv)[1] == '2')
          right_half = 1;
        halfsize = 1;
        break;
      case 'H':
        image_width  = 1920;
        image_height = 1080;
        break;
      case 'q':
        quartersize = 1;
        break;
      case 'f':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing format\n");
          usage();
        }

        switch(**argv)
        {
          case 'r':
            pixfmt = V4L2_PIX_FMT_BGR32;
            break;
          case 'm':
            pixfmt = V4L2_PIX_FMT_STM420MB;
            break;
          case '0':
            pixfmt = V4L2_PIX_FMT_YUV420;
            break;
          case 'v':
            pixfmt = V4L2_PIX_FMT_YVU420;
            break;
          case '2':
            pixfmt = V4L2_PIX_FMT_YUV422P;
            break;
          default:
            fprintf(stderr, "Invalid format identifier\n");
            usage();
        }

        break;
      case 'l':
        lines = 1;
        break;
      case 'n':
        image_width  = 720;
        image_height = 480;
        break;
      case 'p':
        image_width  = 720;
        image_height = 576;
        break;
      case 'P':
        progressive = 1;
        break;
      case 's':
        image_width  = 176;
        image_height = 144;
        break;
      case 'v':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing video device number\n");
          usage();
        }

        viddevice = atoi(*argv);
        if(viddevice<0 || viddevice>255)
        {
          fprintf(stderr,"Video device number out of range\n");
          usage();
        }

        break;
      case 'w':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing plane name\n");
          usage();
        }

        plane_name = *argv;

        break;
      case 't':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing standard id\n");
          usage();
        }

        stdid = (v4l2_std_id) strtoull(*argv, NULL, 0);

        break;
      case 'b':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing number of buffers\n");
          usage();
        }

        NBUFS = atoi(*argv);
        if(NBUFS<0 || NBUFS>20)
        {
          fprintf(stderr,"Number of buffers out of range\n");
          usage();
        }

        break;
      case 'x':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing pixel number\n");
          usage();
        }

        start_x = atoi(*argv);
        break;
      case 'y':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing line number\n");
          usage();
        }

        start_y = atoi(*argv);
        break;
      case 'z':
        zoom = 1;
        break;
      default:
        fprintf(stderr,"Unknown option '%s'\n",*argv);
        usage();
    }

    argc--;
    argv++;
  }

  /*
   * Open the requested V4L2 device
   */
  snprintf(vidname, sizeof(vidname), "/dev/video%d", viddevice);
  if((v4lfd = open(vidname, O_RDWR)) < 0)
  {
    perror("Unable to open video plane");
    goto exit;
  }

  v4l2_list_outputs (v4lfd);
  if (!plane_name) {
    if ((i = v4l2_set_output_by_name (v4lfd, "YUV0")) == -1)
      i = v4l2_set_output_by_name (v4lfd, "YUV1");
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

  printf("Current display standard is '%s'\n",standard.name);

  if(progressive == -1)
    progressive = ((stdid & V4L2_STD_PROGRESSIVE) != 0);

  memset(&cropcap, 0, sizeof(cropcap));
  cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  if(LTV_CHECK (ioctl(v4lfd,VIDIOC_CROPCAP,&cropcap))<0)
    goto exit;

  if(image_width == 0 || image_height == 0)
  {
    image_width   = cropcap.bounds.width;
    image_height  = cropcap.bounds.height;
  }

  frameduration = ((double)standard.frameperiod.numerator/(double)standard.frameperiod.denominator)*1000000.0;

  printf("Frameduration = %lluus\n",frameduration);

  /*
   * Set display sized buffer format
   */
  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  fmt.fmt.pix.pixelformat = pixfmt;

  fmt.fmt.pix.colorspace  = V4L2_COLORSPACE_SMPTE170M;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;

  /*
   * Note to readers: the following is to test placing and displaying an image
   * in a buffer that has been created with bigger dimensions than required.
   * e.g. always creating PAL sized buffers ahead of time because you don't
   * know if you are going to be decoding PAL or NTSC content into the buffer.
   */
  fmt.fmt.pix.width     = (cropcap.bounds.width > image_width)  ? cropcap.bounds.width:image_width;
  fmt.fmt.pix.height    = (cropcap.bounds.height > image_height) ? cropcap.bounds.height:image_height;

  fmt.fmt.pix.sizeimage = fmt.fmt.pix.width  * 2 * fmt.fmt.pix.height;

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

    buffer[i].field = progressive?V4L2_FIELD_NONE:V4L2_FIELD_INTERLACED;
    bufferdata[i] = (unsigned char *)mmap (0, buffer[i].length, PROT_READ|PROT_WRITE, MAP_SHARED, v4lfd, buffer[i].m.offset);

    if(bufferdata[i] == ((void*)-1))
    {
      perror("mmap buffer failed\n");
      goto exit;
    }

    if (verbose)
      printf("creating buffer %d\n",i);

    switch(pixfmt)
    {
      case V4L2_PIX_FMT_BGR32:
        if(lines)
          create_hline_rgb_test_pattern(bufferdata[i], image_width, image_height, fmt.fmt.pix.bytesperline);
        else
          create_rgb_test_pattern(bufferdata[i], image_width, image_height, fmt.fmt.pix.bytesperline);

        break;
      case V4L2_PIX_FMT_UYVY:
        if(lines)
          create_hline_422R_test_pattern(bufferdata[i], image_width, image_height, fmt.fmt.pix.bytesperline);
        else
          create_422R_test_pattern(bufferdata[i], image_width, image_height, fmt.fmt.pix.bytesperline);

        break;
      case V4L2_PIX_FMT_STM420MB:
        create_macroblock_test_pattern(bufferdata[i], image_width, image_height, fmt.fmt.pix.priv, fmt.fmt.pix.bytesperline);
        break;
      case V4L2_PIX_FMT_YUV420:
      case V4L2_PIX_FMT_YVU420:
        if(lines)
          create_hline_planar_test_pattern(bufferdata[i], image_width, image_height, fmt.fmt.pix.priv, image_height/2, fmt.fmt.pix.height/2, fmt.fmt.pix.bytesperline);
        else
          create_planar_test_pattern(bufferdata[i], image_width, image_height, fmt.fmt.pix.priv, image_height/2, fmt.fmt.pix.height/2, fmt.fmt.pix.bytesperline);

        break;
      case V4L2_PIX_FMT_YUV422P:
        if(lines)
          create_hline_planar_test_pattern(bufferdata[i], image_width, image_height, fmt.fmt.pix.priv, image_height, fmt.fmt.pix.height, fmt.fmt.pix.bytesperline);
        else
          create_planar_test_pattern(bufferdata[i], image_width, image_height, fmt.fmt.pix.priv, image_height, fmt.fmt.pix.height, fmt.fmt.pix.bytesperline);

        break;
    }
  }

  /*
   * Set the output size and position if downscale requested
   */
  memset(&crop, 0, sizeof(struct v4l2_crop));
  crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  if(halfsize)
  {
    if(left_half)
    {
      crop.c.left   = 0;
      crop.c.top    = 0;
      crop.c.height = cropcap.bounds.height;
    }
    else if(right_half)
    {
      crop.c.left   = cropcap.bounds.width/2;
      crop.c.top    = 0;
      crop.c.height = cropcap.bounds.height;
    }
    else
    {
      crop.c.left   = cropcap.bounds.width/4;
      crop.c.top    = cropcap.bounds.height/4;
      crop.c.height = cropcap.bounds.height/2;
    }
    crop.c.width  = cropcap.bounds.width/2;
  }
  else if(quartersize)
  {
    crop.c.left   = cropcap.bounds.width/3;
    crop.c.top    = cropcap.bounds.height/3;
    crop.c.width  = cropcap.bounds.width/4;
    crop.c.height = cropcap.bounds.height/4;
  }
  else
  {
    crop.c.left   = 0;
    crop.c.top    = 0;
    crop.c.width  = cropcap.bounds.width;
    crop.c.height = cropcap.bounds.height;
  }

  printf("Output Image Dimensions (%d,%d) (%d x %d)\n",crop.c.left, crop.c.top, crop.c.width, crop.c.height);

  /* set output crop */
  if (LTV_CHECK (ioctl(v4lfd,VIDIOC_S_CROP,&crop)) < 0)
    goto exit;

  /*
   * Set the buffer crop to be the full image.
   */
  memset(&crop, 0, sizeof(struct v4l2_crop));
  crop.type     = V4L2_BUF_TYPE_PRIVATE;
  crop.c.left   = start_x;
  crop.c.top    = start_y;
  crop.c.width  = image_width-start_x;
  crop.c.height = image_height-start_y;

  printf("Source Image Dimensions (%d,%d) (%d x %d)\n", start_x, start_y, image_width, image_height);


  /* set source crop */
  if (LTV_CHECK (ioctl(v4lfd,VIDIOC_S_CROP,&crop)) < 0)
    goto exit;

  /*
   * Work out the presentation time of the first buffer to be queued.
   * Note the use of the posix monotomic clock NOT the adjusted wall clock.
   */
  clock_gettime(CLOCK_MONOTONIC,&currenttime);

  if (verbose)
    printf("Current time = %ld.%ld\n",currenttime.tv_sec,currenttime.tv_nsec/1000);
  ptime.tv_sec  = currenttime.tv_sec + delay;
  ptime.tv_usec = currenttime.tv_nsec / 1000;

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

  /* start stream */
  if(LTV_CHECK (ioctl (v4lfd, VIDIOC_STREAMON, &buffer[0].type)) < 0)
    goto exit;


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
    if(adjusttime && unlikely ((dqbuffer.index == (NBUFS-1))
                               && (i >= (NBUFS*2) - 1)))
    {
      if (abs (timediff) > 500)
      {
        printf("Adjusting presentation time by %lld us\n",timediff/2);

        ptime.tv_usec += timediff/2;
      }
    }
    while(ptime.tv_usec >= 1000000L)
    {
      ptime.tv_sec++;
      ptime.tv_usec -= 1000000L;
    }

    buffer[dqbuffer.index].timestamp = ptime;

    if(zoom)
    {
      memset(&crop, 0, sizeof(struct v4l2_crop));
      crop.type     = V4L2_BUF_TYPE_PRIVATE;
      crop.c.left   = i%(image_width/4);
      crop.c.top    = start_y;
      crop.c.width  = image_width-(crop.c.left*2);
      crop.c.height = image_height-start_y;

      /* set source crop */
      if (LTV_CHECK (ioctl(v4lfd,VIDIOC_S_CROP,&crop)) < 0)
        goto streamoff;
    }

    /* queue buffer */
    if(LTV_CHECK (ioctl (v4lfd, VIDIOC_QBUF, &buffer[dqbuffer.index])) < 0)
      goto streamoff;
  }

streamoff:
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
