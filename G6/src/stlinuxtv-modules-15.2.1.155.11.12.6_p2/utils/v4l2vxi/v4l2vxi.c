#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <linux/videodev2.h>

#include "utils/v4l2_helper.h"

#define V4L2_CARD_NAME  "STMicroelectronics"
#define V4L2_CAPTURE_DEVICE_NAME "AV Decoder"

typedef struct dv_timings_s {
char *name;
struct v4l2_dv_timings timing;
struct v4l2_framebuffer fb;
} dv_timings_t;

dv_timings_t timings[] = {
             {"480i59",{ .bt={  720, 240, 1, 0, 13500000, 119, 19, 0, 19, 26, 0, 0, 0, 0}},
                       { .capability = V4L2_FBUF_CAP_EXTERNOVERLAY, .flags = 0,.fmt={ 720, 240, V4L2_PIX_FMT_UYVY, V4L2_FIELD_INTERLACED, 0, 0, V4L2_COLORSPACE_SMPTE170M, 0}}},
             {"576i50",{ .bt={  720, 288, 1, 0, 13500000, 132, 12, 0, 23, 26, 0, 0, 0, 0}},
                       { .capability = V4L2_FBUF_CAP_EXTERNOVERLAY, .flags = 0,.fmt={ 720, 288, V4L2_PIX_FMT_UYVY, V4L2_FIELD_INTERLACED, 0, 0, V4L2_COLORSPACE_SMPTE170M, 0}}},
             {"480p59",{ .bt={  720, 480, 0, 0, 27000000, 122, 16, 0, 37,  8, 0, 0, 0, 0}},
                       { .capability = V4L2_FBUF_CAP_EXTERNOVERLAY, .flags = 0,.fmt={ 720, 480, V4L2_PIX_FMT_UYVY, V4L2_FIELD_TOP,        0, 0, V4L2_COLORSPACE_SMPTE240M, 0}}},
             {"576p50",{ .bt={  720, 576, 0, 0, 27000000, 132, 12, 0, 23, 26, 0, 0, 0, 0}},
                       { .capability = V4L2_FBUF_CAP_EXTERNOVERLAY, .flags = 0,.fmt={ 720, 576, V4L2_PIX_FMT_UYVY, V4L2_FIELD_TOP,        0, 0, V4L2_COLORSPACE_SMPTE240M, 0}}},
             {"720p50",{ .bt={ 1280, 720, 0, 1, 74250000, 260,440, 0, 26,  4, 0, 0, 0, 0}},
                       { .capability = V4L2_FBUF_CAP_EXTERNOVERLAY, .flags = 0,.fmt={1280, 720, V4L2_PIX_FMT_UYVY, V4L2_FIELD_TOP,        0, 0, V4L2_COLORSPACE_SMPTE240M, 0}}},
             {"720p60",{ .bt={ 1280, 720, 0, 1, 74250000, 260,110, 0, 26,  4, 0, 0, 0, 0}},
                       { .capability = V4L2_FBUF_CAP_EXTERNOVERLAY, .flags = 0,.fmt={1280, 720, V4L2_PIX_FMT_UYVY, V4L2_FIELD_TOP,        0, 0, V4L2_COLORSPACE_SMPTE240M, 0}}},
            {"1080i50",{ .bt={ 1920, 540, 1, 1, 74250000, 192,528, 0, 21, 24, 0, 0, 0, 0}},
                       { .capability = V4L2_FBUF_CAP_EXTERNOVERLAY, .flags = 0,.fmt={1920, 540, V4L2_PIX_FMT_UYVY, V4L2_FIELD_INTERLACED, 0, 0, V4L2_COLORSPACE_SMPTE240M, 0}}},
            {"1080i60",{ .bt={ 1920, 540, 1, 1, 74250000, 192, 89, 0, 21, 24, 0, 0, 0, 0}},
                       { .capability = V4L2_FBUF_CAP_EXTERNOVERLAY, .flags = 0,.fmt={1920, 540, V4L2_PIX_FMT_NV16, V4L2_FIELD_INTERLACED, 0, 0, V4L2_COLORSPACE_SMPTE240M, 0}}}
};

static void usage(void)
{
    printf("Usage: v4l2vxi [options]\n");
    printf("\t-h,--help          - Display this help\n");
    printf("\t-v,--video-timing  - Set video timing \n");
    printf("\t-p,--pixel-format  - Set pixel format \n");
    exit(1);
}

int main(int argc, char **argv)
{
  unsigned int i = 0;
  int error = 0;
  int option = 0;
  int v4lfd  = 0;
  unsigned int change_video_mode   = 0;
  unsigned int change_pixel_format = 0;
  unsigned int pixel_format = 0;
  struct v4l2_dv_timings  get_timing;
  struct v4l2_framebuffer get_fb;
  static struct option long_options[] = {
          { "help"                   , 0, 0, 'h' },
          { "video-timing"           , 1, 0, 'v' },
          { "pixel-format"           , 1, 0, 'p' },
          { 0, 0, 0, 0 }
  };

  if((v4lfd = v4l2_open_by_name(V4L2_CAPTURE_DEVICE_NAME, V4L2_CARD_NAME,O_RDWR)) < 0)
  {
    perror("Unable to open video device");
    goto exit;
  }

  i = v4l2_set_input_by_name (v4lfd, "vxi0");

  if (i == -1)
  {
    perror("Unable to set video input");
    error = 1;
    goto exit;
  }

  while((option = getopt_long (argc, argv, "h:v:p:", long_options, NULL)) != -1)
  {
    switch(option)
    {
      case 'h':
        usage();
        break;
      case 'v':
      {
        i = 0;
        while ((strcmp(optarg, timings[i].name)!= 0) && (i<sizeof(timings)/sizeof(dv_timings_t)))
        {
          i++;
        }

        if (i == (sizeof(timings)/sizeof(dv_timings_t)))
        {
          perror("Timings not supported it should be [480i59|576i50|480p59|576p50|720p50|720p60|1080i50|1080i60]\n");
          error = -1;
          goto exit;
        }
        else
        {
          change_video_mode = 1;
        }
        break;
      }
      case 'p':
      {
        change_pixel_format = 0;
        if (strcmp(optarg, "YUV422") == 0)
        {
          change_pixel_format = 1;
          pixel_format = V4L2_PIX_FMT_UYVY;
        }
        else if (strcmp(optarg, "YUV444") == 0)
             {
                change_pixel_format = 1;
                pixel_format = V4L2_PIX_FMT_YUV444;
             }
        else if (strcmp(optarg, "RGB444") == 0)
             {
               change_pixel_format = 1;
               pixel_format = V4L2_PIX_FMT_RGB24;
             }
        else
        {
          perror("Pixel format not supported it should be: [YUV422|YUV444|RGB444]\n");
          error = -1;
          goto exit;
        }
        break;
      }
      default:
        break;
    }
  }
  if (change_video_mode == 1)
  {
    if((error=LTV_CHECK(ioctl(v4lfd,VIDIOC_S_DV_TIMINGS,&(timings[i].timing))))<0)
      goto exit;
  }

  if((error=LTV_CHECK(ioctl(v4lfd,VIDIOC_G_DV_TIMINGS,&get_timing)))<0)
      goto exit;

  printf("-------- %s selected --------\n", timings[i].name);
  printf("width          = %d \n", get_timing.bt.width);
  printf("height         = %d \n", get_timing.bt.height );
  printf("interlaced     = %d \n", get_timing.bt.interlaced);
  printf("polarities     = %d \n", get_timing.bt.polarities);
  printf("pixelclock     = %d \n", (int)get_timing.bt.pixelclock);
  printf("hfrontporch    = %d \n", get_timing.bt.hfrontporch);
  printf("hsync          = %d \n", get_timing.bt.hsync);
  printf("hbackporch     = %d \n", get_timing.bt.hbackporch);
  printf("vfrontporch    = %d \n", get_timing.bt.vfrontporch);
  printf("vsync          = %d \n", get_timing.bt.vsync);
  printf("vbackporch     = %d \n", get_timing.bt.vbackporch);
  printf("il_vfrontporch = %d \n", get_timing.bt.il_vfrontporch);
  printf("il_vsync       = %d \n", get_timing.bt.il_vsync);
  printf("il_vbackporch  = %d \n", get_timing.bt.il_vbackporch);

  if (change_pixel_format == 1)
  {
    timings[i].fb.fmt.pixelformat = pixel_format;
    if((error=LTV_CHECK (ioctl(v4lfd,VIDIOC_S_FBUF,&(timings[i].fb))))<0)
      goto exit;
  }

  if((error=LTV_CHECK (ioctl(v4lfd,VIDIOC_G_FBUF,&get_fb)))<0)
      goto exit;

  printf("------- Pixelstream format ------\n");
  printf("capability      = %d \n", get_fb.capability);
  printf("flags           = %d \n", get_fb.flags);
  printf("fmt.width       = %d \n", get_fb.fmt.width);
  printf("fmt.height      = %d \n", get_fb.fmt.height);
  printf("fmt.pixelformat = %d \n", get_fb.fmt.pixelformat);
  printf("fmt.field       = %d \n", get_fb.fmt.field );
  printf("fmt.colorspace  = %d \n", get_fb.fmt.colorspace);
  printf("fmt.sizeimage   = %d \n", get_fb.fmt.sizeimage);

exit:
  close(v4lfd);
  return error;
}
