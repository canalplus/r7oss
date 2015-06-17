/***********************************************************************
 *
 * File: linux/tests/v4l2stream/yuvplayer.c
 * Copyright (c) 2005-2008 STMicroelectronics Limited.
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

#ifndef MAP_FAILED
#  define MAP_FAILED  ((void *) -1)
#endif

#include <signal.h>
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



#define QUEUE_JUST_SOME 4 // this must be >= 3!
#undef SWAP_FIELDS   // only supported for V4L2_PIX_FMT_UYVY destinations
#undef SOURCE_422P   // only supported for V4L2_PIX_FMT_YUV422P destinations


/*
 * This is a simple program to play back yuv movies through the V4L2 API
 * It's mainly based on v4lstream
 *
 * The expected file format is:
 *      Y plane, Cb plane, Cr plane
 * and such source files can be created using mencoder:
 * mencoder \
            -ss 28 -endpos 1:00 \
            -fps 30000/1001 \
            -nosound -ofps 30000/1001 -of rawvideo -ovc raw \
            -vf format=i420 \
            -o PulpFiction_720x480.yuv420 \
            PulpFiction.mpg

 * mencoder \
            -ss 5 -endpos 10 \
            -fps 30000/1001 \
            -nosound -ofps 30000/1001 -of rawvideo -ovc raw \
            -vf format=i420 \
            -o HQV_DVD_NTSC_Jaggies1.yuv420 \
            HQV_DVD_NTSC_Jaggies1.VOB
 *
 */


#define unlikely(x)     __builtin_expect(!!(x),0)
#define likely(x)       __builtin_expect(!!(x),1)
#define abs(x)  ({				\
		  long long __x = (x);		\
		  (__x < 0) ? -__x : __x;	\
		})

static volatile int terminated;

static void
sig_int_handler (int sig)
{
  terminated = 1;
}


void usage(void)
{
    fprintf(stderr,"Usage: v4l2stream [options]\n");
    fprintf(stderr,"\ta         - Adjust time to minimise presentation delay\n");
    fprintf(stderr,"\tb num     - Number of V4L buffers to use\n");
    fprintf(stderr,"\td num     - Presentation delay on first buffer by num seconds, default is 1\n");
    fprintf(stderr,"\th         - Scale the buffer down to half size\n");
    fprintf(stderr,"\tf m|0|v|2 - Use 420MB, YUV420,YVU420 or YUV422P instead of 422R\n");
    fprintf(stderr,"\tn         - Use a NTSC sized buffer,default is screen display size\n");
    fprintf(stderr,"\tp         - Use a PAL sized buffer, default is screen display size\n");
    fprintf(stderr,"\tq         - Scale the buffer down to quarter size\n");
    fprintf(stderr,"\tr num     - Open framebuffer device /dev/fbNUM, default is 0\n");
    fprintf(stderr,"\ts         - Use a small (176x144) sized buffer\n");
    fprintf(stderr,"\tv num     - Open V4L2 device /dev/videoNUM, default is 0\n");
    fprintf(stderr,"\tw name    - select plane by name on output, default is first video plane\n");
    fprintf(stderr,"\tx num     - Crop the source image at starting at the given pixel number\n");
    fprintf(stderr,"\ty num     - Crop the source image at starting at the given line number\n");
    fprintf(stderr,"\tz         - Do a dynamic horizontal zoom out\n");
    fprintf(stderr,"\tl         - force interLace (use together w/ fbset 1280x720-50 && v4l2stream p l)\n");
    fprintf(stderr,"\tu mUvie   - Play back raw i420 movie file\n");
    fprintf(stderr,"\tt         - startframe inside movie\n");
    fprintf(stderr,"\te         - vErbose\n");
    exit(1);
}




int main(int argc, char **argv)
{
  int           fbfd;
  int           v4lfd;
  int           verbose;
  int           fbdevice;
  int           viddevice;
  char          vidname[32];
  const char   *plane_name = NULL;
  int           progressive = -1;
  int           delay;
  int           pixfmt;
  int           halfsize;
  int           quartersize;
  int           adjusttime;
  int           NBUFS;
  unsigned int  i;
  int           image_width;
  int           image_height;
  int           start_x;
  int           start_y;
  int           zoom;

  const char    *movie_fn = NULL;
  int            movie_startframe = 0;
  int            movie_fd;
  unsigned int   movie_frames = 0;
  const unsigned char *movie = NULL;

  unsigned long long frameduration;

  struct stmfbio_var_screeninfo_ex varEx;

  struct v4l2_format         fmt;
  struct v4l2_requestbuffers reqbuf;
  struct v4l2_crop           crop;
  struct v4l2_buffer        *buffer;
  unsigned char             *bufferdata;

  struct timespec            currenttime;
  struct timeval             ptime;
  struct fb_var_screeninfo   var;

  argc--;
  argv++;

  verbose    = 0;
  adjusttime = 0;
  fbdevice   = 0;
  viddevice  = 0;
  pixfmt     = V4L2_PIX_FMT_UYVY;
  halfsize   = 0;
  quartersize = 0;
  delay      = 1;
  NBUFS      = QUEUE_JUST_SOME ? : 5;
  start_x    = 0;
  start_y    = 0;
  zoom       = 0;
  image_width = 0;
  image_height = 0;

  setvbuf (stdout, NULL, _IONBF, 0);

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

      case 'h':
        halfsize = 1;
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
      case 'n':
        image_width  = 720;
        image_height = 480;
        break;
      case 'p':
        image_width  = 720;
        image_height = 576;
        break;
      case 's':
        image_width  = 176;
        image_height = 144;
        break;
      case 'r':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing framebuffer device number\n");
          usage();
        }

        fbdevice = atoi(*argv);
        if(fbdevice<0 || fbdevice>255)
        {
          fprintf(stderr,"Framebuffer device number out of range\n");
          usage();
        }

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
      case 'b':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing number of buffers\n");
          usage();
        }

        NBUFS = atoi(*argv);
        if (NBUFS <= (QUEUE_JUST_SOME ? (QUEUE_JUST_SOME - 1) : 0))
        {
          fprintf(stderr,"Need at least %d buffer(s)!\n",
                  QUEUE_JUST_SOME ? QUEUE_JUST_SOME : 1);
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

      case 'l':
        progressive = 0;
        break;

      case 'u':
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing filename\n");
          usage();
        }
        movie_fn = *argv;
        break;

      case 't':
        argc--;
        argv++;
        if(argc <= 0)
        {
          fprintf(stderr,"Missing startframe number\n");
          usage();
        }
        movie_startframe = strtoll (*argv, NULL, 0);
        break;

      default:
        fprintf(stderr,"Unknown option '%s'\n",*argv);
        usage();
    }

    argc--;
    argv++;
  }

  buffer = calloc (NBUFS, sizeof (struct v4l2_buffer));
  if (!buffer)
  {
    fprintf (stderr, "Out of memory for buffer\n");
    exit (1);
  }

  /*
   * Open framebuffer device
   */
  sprintf(vidname, "/dev/fb%d", fbdevice);
  if((fbfd = open(vidname, O_RDWR)) < 0)
  {
    perror("Unable to open framebuffer");
    exit(1);
  }

  if(ioctl(fbfd,FBIOGET_VSCREENINFO, &var)<0)
  {
    perror("Unable to query framebuffer mode format");
    exit (1);
  }

  {
    unsigned long long htotal;
    unsigned long long vtotal;
    htotal        = var.hsync_len + var.left_margin + var.xres + var.right_margin;
    vtotal        = var.vsync_len + var.upper_margin + var.yres + var.lower_margin;
    frameduration = ((unsigned long long)var.pixclock*htotal*vtotal)/1000000L;

    /* the var.pixclock is not really accurate, therefore we adjust
       frameduration for some known values. */
    switch (frameduration)
      {
      case 39999: frameduration = 40000; break;
      case 19999: frameduration = 20000; break;
      /* can't do anything about the NTSC cases, though */
      }
  
    if (progressive == 0
        && ((var.vmode & FB_VMODE_INTERLACED) == 0))
    {
      frameduration *= 2;
    }
    else if (progressive == -1)
      progressive = ((var.vmode & FB_VMODE_INTERLACED) == 0);
    if(image_width == 0 || image_height == 0)
    {
      image_width   = var.xres;
      image_height  = var.yres;
  
      if(image_height == 483)
        image_height = 480;
    }
  
    printf("pixclock = %u htotal = %llu vtotal = %llu Frameduration = %lluus\n",var.pixclock,htotal,vtotal,frameduration);
  }

  /*
   * open video file
   */
  if (movie_fn)
  {
    struct stat buf;
    if ((movie_fd = open (movie_fn, O_RDONLY)) == -1)
    {
      perror ("open movie file");
      exit (1);
    }
    if (!fstat (movie_fd, &buf))
    {
#ifdef SOURCE_422P
      movie_frames = buf.st_size / (image_width*image_height + image_width*image_height);
#else
      movie_frames = buf.st_size / (image_width*image_height + image_width*image_height/2);
#endif

      printf("movie has %u frames\n", movie_frames);

      if (movie_frames < (movie_startframe + NBUFS))
      {
        fprintf (stderr, "movie is shorter than specified range to play back: avail: %u\n", movie_frames);
        exit (1);
      }

      /* can only mmap() small files... */
      if (buf.st_size <= 1024*1024*1024)
      {
        movie = mmap (0, buf.st_size, PROT_READ, MAP_SHARED, movie_fd, 0);
        if (movie == MAP_FAILED)
          {
            perror ("mmap movie file");
            movie = NULL;
          }
      }
    }
  }

  /*
   * Open the requested V4L2 device
   */
  sprintf(vidname, "/dev/video%d", viddevice);
  if((v4lfd = open(vidname, O_RDWR)) < 0)
  {
    perror("Unable to open video plane");
    exit (1);
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
  fmt.fmt.pix.width     = (var.xres > image_width)  ? var.xres:image_width;
  fmt.fmt.pix.height    = (var.yres > image_height) ? var.yres:image_height;

  fmt.fmt.pix.sizeimage = fmt.fmt.pix.width  * 2 * fmt.fmt.pix.height;

  if(ioctl(v4lfd,VIDIOC_S_FMT,&fmt)<0)
  {
    perror("Unable to set video buffer format");
    exit (1);
  }

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

  if(ioctl (v4lfd, VIDIOC_REQBUFS, &reqbuf)<0)
  {
    perror("Unable to request buffers");
    exit (1);
  }

  if(reqbuf.count < NBUFS)
  {
    fprintf (stderr, "Unable to allocate all buffers: only %d\n", reqbuf.count);
    exit (1);
  }

  /*
   * Query all the buffers from the driver.
   */
  for(i=0;i<NBUFS;i++)
  {
    buffer[i].index = i;
    buffer[i].type  = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if(ioctl (v4lfd, VIDIOC_QUERYBUF, &buffer[i]) < 0)
    {
      perror("Unable to query buffer");
      goto exit;
    }

    buffer[i].field = progressive?V4L2_FIELD_NONE:V4L2_FIELD_INTERLACED;

    if (verbose)
      printf("creating buffer %d\r",i);
  }
  if (verbose)
    printf("\n");

  if (movie_fn)
  {
    unsigned int         luma_len   = image_width * image_height;
#ifdef SOURCE_422P
    unsigned int         chroma_len = luma_len;
#else
    unsigned int         chroma_len = luma_len / 2;
#endif

    const unsigned char *src_luma;
    const unsigned char *src_chroma;
    unsigned char *tmpbuf = NULL;

    if (movie)
      /* easy, file was mmap()ed */
      src_luma = movie + ((luma_len + chroma_len) * movie_startframe);
    else
    {
      tmpbuf = malloc (luma_len + chroma_len);
      if (!tmpbuf)
      {
        fprintf (stderr, "failed to allocate some temporary buffer memory\n");
        exit (1);
      }
      src_luma = tmpbuf;

      lseek (movie_fd, (luma_len + chroma_len) * movie_startframe, SEEK_SET);
    }

    src_chroma = src_luma + luma_len;

    for (i = 0; i < NBUFS; ++i)
    {
//      buffer[i].flags = (0
//                         | V4L2_BUF_FLAG_REPEAT_FIRST_FIELD
//                         | V4L2_BUF_FLAG_BOTTOM_FIELD_FIRST
//                        );
      bufferdata = mmap (0, buffer[i].length, PROT_READ|PROT_WRITE, MAP_SHARED, v4lfd, buffer[i].m.offset);
      if(bufferdata != MAP_FAILED)
      {
        unsigned char *dst_luma = bufferdata;

        if (verbose)
          printf ("mmap()2 (buf %d/%.8x/%d) to %p\r",i,buffer[i].m.offset,buffer[i].length,bufferdata);

        if ((!movie
             || movie == MAP_FAILED)
            && read (movie_fd, tmpbuf, luma_len + chroma_len) != (luma_len + chroma_len))
        {
	  fflush (stdout);
          fprintf (stderr, "eof?\n");
          exit (1);
        }

        switch (pixfmt)
          {
          case V4L2_PIX_FMT_STM420MB:
          default:
	    fflush (stdout);
            fprintf (stderr, "conversion not supported\n");
            exit (1);
            break;

          case V4L2_PIX_FMT_YUV420:
            {
            /* almost 1:1 mapping between file and expected format */
            unsigned char *dst_chroma = dst_luma + fmt.fmt.pix.priv;
            memcpy (dst_luma, src_luma, luma_len);
            memcpy (dst_chroma, src_chroma, chroma_len);
            }
            break;

          case V4L2_PIX_FMT_YVU420:
            {
            /* Cb plane and Cr plane are swapped */
            unsigned char *dst_chroma = dst_luma + fmt.fmt.pix.priv;
            memcpy (dst_luma, src_luma, luma_len);
            memcpy (dst_chroma,  src_chroma + (chroma_len / 2), chroma_len / 2);
            memcpy (dst_chroma + (chroma_len / 2),  src_chroma, chroma_len / 2);
            }
            break;

          case V4L2_PIX_FMT_YUV422P:
            {
#ifdef SOURCE_422P
            /* almost 1:1 mapping between file and expected format */
            unsigned char *dst_chroma = dst_luma + fmt.fmt.pix.priv;
            memcpy (dst_luma, src_luma, luma_len);
            memcpy (dst_chroma, src_chroma, chroma_len);
#else /* SOURCE_422P */
            /* Luma has a 1:1 mapping, but need to interpolate some chroma */
            unsigned int y;
            unsigned char * const dst_chroma_cb =
              dst_luma + fmt.fmt.pix.priv;
            unsigned char * const dst_chroma_cr =
              dst_chroma_cb + (fmt.fmt.pix.height * fmt.fmt.pix.bytesperline / 2);

            memcpy (dst_luma, src_luma, luma_len);

            for (y = 0; y < image_height; y+=2)
              {
                const unsigned char * const my_src_cb = src_chroma + y/2 * (image_width / 2);
                const unsigned char * const my_src_cr = my_src_cb + (chroma_len / 2);

                unsigned char * const my_dst_cb = dst_chroma_cb + (y * fmt.fmt.pix.bytesperline / 2);
                unsigned char * const my_dst_cr = dst_chroma_cr + (y * fmt.fmt.pix.bytesperline / 2);

                memcpy (my_dst_cb, my_src_cb, image_width / 2);
                /* 'interpolate' a line by using the previous twice */
                memcpy (my_dst_cb + fmt.fmt.pix.bytesperline / 2,
                        my_src_cb, image_width / 2);

                memcpy (my_dst_cr, my_src_cr, image_width / 2);
                /* 'interpolate' a line by using the previous twice */
                memcpy (my_dst_cr + fmt.fmt.pix.bytesperline / 2,
                        my_src_cr, image_width / 2);
              }
#endif /* SOURCE_422P */
            }
            break;

          case V4L2_PIX_FMT_UYVY:
            {
#ifdef SWAP_FIELDS
            /* 0: Y4 Cb0 Y5 Cr0 Y6 Cb1 Y7 Cr1
               1: Y0 Cb0 Y1 Cr0 Y2 Cb1 Y3 Cr1
               2: Yc Cb2 Yd Cr2 Ye Cb3 Yf Cr3
               3: Y8 Cb2 Y9 Cr2 Ya Cb3 Yb Cr3
               etc. */
            unsigned short       *pixel = (unsigned short *) dst_luma;
            const unsigned char  *my_src_luma = src_luma;
            const unsigned char  *my_src_cb   = src_chroma;
            const unsigned char  *my_src_cr   = src_chroma + chroma_len/2;

            /* this can be optimized, but I don't care at the moment! */
            unsigned int x, y;
            pixel += (fmt.fmt.pix.bytesperline / sizeof (unsigned short));
            for (y = 0; y < image_height; ++y)
            {
              for (x = 0; x < image_width; ++x)
              {
                switch (x%2)
                  {
                  case 0:
                    *(pixel + x) = (*my_src_luma++ << 8) | *my_src_cb++;
                    break;
                  case 1:
                    *(pixel + x) = (*my_src_luma++ << 8) | *my_src_cr++;
                    break;
                  }
              }

              if (y % 2 == 0)
                pixel -= (fmt.fmt.pix.bytesperline / sizeof (unsigned short));
              else
                pixel += 3*(fmt.fmt.pix.bytesperline / sizeof (unsigned short));
              /* 'interpolate' a line by using src twice */
              if (y % 2 == 0)
              {
                my_src_cb -= (image_width / 2);
                my_src_cr -= (image_width / 2);
              }
            }
#else /* SWAP_FIELDS */
            /* 0: Y0 Cb0 Y1 Cr0 Y2 Cb1 Y3 Cr1
               1: Y4 Cb0 Y5 Cr0 Y6 Cb1 Y7 Cr1
               2: Y8 Cb2 Y9 Cr2 Ya Cb3 Yb Cr3
               3: Yc Cb2 Yd Cr2 Ye Cb3 Yf Cr3
               etc. */
            unsigned short       *pixel = (unsigned short *) dst_luma;
            const unsigned char  *my_src_luma = src_luma;
            const unsigned char  *my_src_cb   = src_chroma;
            const unsigned char  *my_src_cr   = src_chroma + chroma_len/2;

            /* this can be optimized, but I don't care at the moment! */
            unsigned int x, y;
            for (y = 0; y < image_height; ++y)
            {
              for (x = 0; x < image_width; ++x)
              {
                switch (x%2)
                  {
                  case 0:
                    *(pixel + x) = (*my_src_luma++ << 8) | *my_src_cb++;
                    break;
                  case 1:
                    *(pixel + x) = (*my_src_luma++ << 8) | *my_src_cr++;
                    break;
                  }
              }

              pixel += (fmt.fmt.pix.bytesperline / sizeof (unsigned short));
              /* 'interpolate' a line by using src twice */
              if (y % 2 == 0)
              {
                my_src_cb -= (image_width / 2);
                my_src_cr -= (image_width / 2);
              }
            }
#endif /* SWAP_FIELDS */
            }
            break;
          }

        if (movie)
        {
          src_luma += luma_len + chroma_len;
          src_chroma += luma_len + chroma_len;
        }

        munmap (bufferdata, buffer[i].length);
        bufferdata = NULL;
      }
      else
      {
	fflush (stdout);
        perror("\nmmap buffer failed\n");
        exit (1);
      }
    }

    if (verbose)
      printf ("\n");

    if (tmpbuf)
      free (tmpbuf);
  }

  /*
   * Set the output size and position if downscale requested
   */
  memset(&crop, 0, sizeof(struct v4l2_crop));
  crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  if(halfsize)
  {
    crop.c.left   = var.xres/4;
    crop.c.top    = var.yres/4;
    crop.c.width  = var.xres/2;
    crop.c.height = var.yres/2;
  }
  else if(quartersize)
  {
    crop.c.left   = var.xres/3;
    crop.c.top    = var.yres/3;
    crop.c.width  = var.xres/4;
    crop.c.height = var.yres/4;
  }
  else
  {
    crop.c.left   = 0;
    crop.c.top    = 0;
    crop.c.width  = var.xres;
    crop.c.height = var.yres;
  }

  printf("Output Image Dimensions (%d,%d) (%d x %d)\n",crop.c.left, crop.c.top, crop.c.width, crop.c.height);

  if (ioctl(v4lfd,VIDIOC_S_CROP,&crop) < 0)
  {
    perror("Unable to set output crop");
    exit (1);
  }

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


  if (ioctl(v4lfd,VIDIOC_S_CROP,&crop) < 0)
  {
    perror("Unable to set source crop");
    exit (1);
  }

  /*
   * Hide the framebuffer so we can see the video plane
   */
  varEx.layerid  = 0;
  varEx.caps     = STMFBIO_VAR_CAPS_OPACITY;
  varEx.activate = STMFBIO_ACTIVATE_IMMEDIATE;
  varEx.opacity  = 0;
  ioctl(fbfd, STMFBIO_SET_VAR_SCREENINFO_EX, &varEx);

  /*
   * Work out the presentation time of the first buffer to be queued.
   * Note the use of the posix monotomic clock NOT the adjusted wall clock.
   */
  clock_gettime(CLOCK_MONOTONIC,&currenttime);

  if (verbose)
    printf("Current time = %ld.%ld\n",currenttime.tv_sec,currenttime.tv_nsec/1000);
  ptime.tv_sec  = currenttime.tv_sec + delay;
  ptime.tv_usec = currenttime.tv_nsec / 1000;

#if QUEUE_JUST_SOME == 0
  /* queue all the buffers up for display. */
  for(i=0;i<NBUFS;i++)
  {
    if (i != 0)
      {
        ptime.tv_usec += frameduration;
        while (ptime.tv_usec >= 1000000L)
          {
            ptime.tv_sec++;
            ptime.tv_usec -= 1000000L;
          }
      }

    buffer[i].timestamp = ptime;
    if(ioctl (v4lfd, VIDIOC_QBUF, &buffer[i]) < 0)
      {
        perror ("Unable to queue buffer");
        goto exit;
      }
    if (verbose)
      printf ("queued buffer %d\r", i);
  }
  if (verbose)
    printf ("\n");
#else /* QUEUE_JUST_SOME */
  /* queue just some buffers up for display. */
  for (i = 0; i < QUEUE_JUST_SOME; ++i)
  {
    if (i != 0)
      {
        ptime.tv_usec += frameduration;
        while (ptime.tv_usec >= 1000000L)
          {
            ptime.tv_sec++;
            ptime.tv_usec -= 1000000L;
          }
      }

    buffer[i].timestamp = ptime;
    if (ioctl (v4lfd, VIDIOC_QBUF, &buffer[i]) < 0)
    {
      perror ("Unable to queue buffer");
      goto exit;
    }
    if (verbose)
      printf ("queued buffer %d for display @ %ld.%06ld\n",
              i, ptime.tv_sec, ptime.tv_usec);
  }
#endif /* QUEUE_JUST_SOME */

  if(ioctl (v4lfd, VIDIOC_STREAMON, &buffer[0].type) < 0)
  {
    perror("Unable to start stream");
    goto exit;
  }

  signal (SIGINT, sig_int_handler);

  if (zoom)
    i = 0;

  int first_run = 1;
  while (!terminated)
  {
    struct v4l2_buffer dqbuffer;
    long long time1;
    long long time2;
    long long timediff;

    int buf_to_queue;

    /*
     * To Dqueue a streaming memory buffer you must set the buffer type
     * AND and the memory type correctly, otherwise the API returns an error.
     */
    dqbuffer.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    dqbuffer.memory = V4L2_MEMORY_MMAP;

    /*
     * We didn't open the V4L2 device non-blocking so this will sleep until
     * a buffer becomes available.
     */
    if(ioctl (v4lfd, VIDIOC_DQBUF, &dqbuffer) < 0)
    {
      goto exit;
    }

    time1 = (buffer[dqbuffer.index].timestamp.tv_sec*1000000LL) + buffer[dqbuffer.index].timestamp.tv_usec;
    time2 = (dqbuffer.timestamp.tv_sec*1000000LL) + dqbuffer.timestamp.tv_usec;

    timediff = time2 - time1;

    if (verbose)
      printf ("Buffer%d: wanted %ld.%06ld, actual %ld.%06ld, diff = %lld us\n",
              dqbuffer.index,
              buffer[dqbuffer.index].timestamp.tv_sec,
              buffer[dqbuffer.index].timestamp.tv_usec,
              dqbuffer.timestamp.tv_sec,
              dqbuffer.timestamp.tv_usec,
              timediff);

    buf_to_queue = dqbuffer.index;
#if QUEUE_JUST_SOME
    buf_to_queue += QUEUE_JUST_SOME;
    buf_to_queue %= NBUFS;
#endif

    ptime.tv_usec += frameduration;
    if (unlikely (first_run))
      {
        /* after the first frame came back, we know when exactly the VSync is
           happening, so adjust the presentation time - only once, though! */
        first_run = 0;
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
    while (ptime.tv_usec >= 1000000L)
    {
      ++ptime.tv_sec;
      ptime.tv_usec -= 1000000L;
    }

    buffer[buf_to_queue].timestamp = ptime;

    if(zoom)
    {
      memset(&crop, 0, sizeof(struct v4l2_crop));
      crop.type     = V4L2_BUF_TYPE_PRIVATE;
      crop.c.left   = i++ % (image_width / 4);
      crop.c.top    = start_y;
      crop.c.width  = image_width-(crop.c.left*2);
      crop.c.height = image_height-start_y;

      if (ioctl(v4lfd,VIDIOC_S_CROP,&crop) < 0)
      {
        perror("Unable to set source crop");
        goto exit;
      }
    }

#if 0
    if (movie_fn && i < (iterations - NBUFS))
    {
      unsigned int   luma_len = image_width * image_height;
      unsigned int   chroma_len = luma_len / 2;

      unsigned char *src_luma   = &movie[i * (luma_len + chroma_len)];
      unsigned char *src_chroma = src_luma + luma_len;

      bufferdata = mmap (0, buffer[buf_to_queue].length, PROT_READ|PROT_WRITE, MAP_SHARED, v4lfd, buffer[buf_to_queue].m.offset);
      if(bufferdata != MAP_FAILED)
      {
        unsigned char *dst_luma   = bufferdata;
        unsigned char *dst_chroma = dst_luma + fmt.fmt.pix.priv;

        memcpy (dst_luma, src_luma, luma_len);
        memcpy (dst_chroma, src_chroma, chroma_len);

        munmap (bufferdata, buffer[buf_to_queue].length);
        bufferdata = NULL;
      }
      else
      {
        perror ("mmap3");
        goto exit; do_IRQ ret_from_irq
      }
    }
#endif

    if(ioctl (v4lfd, VIDIOC_QBUF, &buffer[buf_to_queue]) < 0)
    {
      fprintf (stderr, "Unable to queue buffer %d: %d (%m)\n", buf_to_queue, errno);
      goto exit;
    }
//    printf ("  queued buffer %d for display @ %ld.%06ld\n", buf_to_queue,
//            ptime.tv_sec, ptime.tv_usec);
  }

  /*
   * Show the framebuffer again
   */
exit:
  varEx.opacity = 255;
  ioctl(fbfd, STMFBIO_SET_VAR_SCREENINFO_EX, &varEx);

  return 0;
}
