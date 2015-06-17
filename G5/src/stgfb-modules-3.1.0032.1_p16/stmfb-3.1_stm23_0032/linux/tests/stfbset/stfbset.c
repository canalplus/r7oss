/***********************************************************************
 *
 * File: linux/tests/stfbcontrol/stfbset.c
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

#include <sys/types.h>
#include <sys/ioctl.h>

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>

#include <linux/fb.h>

/*
 * This test builds against the version of stmfb.h in this source tree, rather
 * than the one that is shipped as part of the kernel headers package for
 * consistency. Normal user applications should use <linux/stmfb.h>
 */
#include <linux/drivers/video/stmfb.h>

void usage(void)
{
  printf("Usage: stfbset [options]\n");
  printf("\t-a,--transparency=[0-255]\n");
  printf("\t-b,--hdmi-deepcolour-bitdepth=[24|30|36|48]\n");
  printf("\t-d,--disable-output=[cvbs|svideo|yuv|rgb|hdmi|dvo]\n");
  printf("\t-e,--enable-output=[cvbs|svideo|yuv|rgb|hdmi|dvo]\n");
  printf("\t-f,--fb=DEVNAME (default=/dev/fb0)\n");
  printf("\t-g,--gain=[0-255]\n");
  printf("\t-h,--help\n");
  printf("\t-p,--premultipled-alpha\n");
  printf("\t-n,--non-premultiplied-alpha\n");
  printf("\t-s,--show\n");
  printf("\t-C,--ycbcr-colourspace=[auto|601|709]\n");
  printf("\t-D,--dvo-outputmode=[rgb|yuv|422|itu656|24bit_yuv]\n");
  printf("\t-F,--flicker-filter=[off|simple|adaptive]\n");
  printf("\t-H,--hdmi-colourspace=[rgb|yuv|422]\n");
  printf("\t-M,--mixer-background=0xAARRGGBB\n");
  printf("\t-R,--rescale-colour=[fullrange|videorange]\n");
  printf("\t-U,--full-range=[analogue|hdmi|dvo]\n");
  printf("\t-V,--video-range=[analogue|hdmi|dvo]\n");
}

void show_info(const struct stmfbio_var_screeninfo_ex *varEx,
               const struct stmfbio_output_configuration *outputConfig)
{
  printf("Flicker Filter: ");
  if(varEx->caps & STMFBIO_VAR_CAPS_FLICKER_FILTER)
  {
    switch(varEx->ff_state)
    {
      case STMFBIO_FF_OFF:
        printf("Off\n");
        break;
      case STMFBIO_FF_SIMPLE:
        printf("Simple\n");
        break;
      case STMFBIO_FF_ADAPTIVE:
        printf("Adaptive\n");
        break;
    }
  }
  else
  {
    printf("Unsupported\n");
  }

  printf("Mixer Colour  : ");
  if(outputConfig->caps & STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND)
  {
    printf("0x%08x\n",outputConfig->mixer_background);
  }
  else
  {
    printf("None\n");
  }


  printf("Transparency  : ");
  if(varEx->caps & STMFBIO_VAR_CAPS_OPACITY)
    printf("%d\n",varEx->opacity);
  else
    printf("Unsupported\n");

  printf("Gain          : ");
  if(varEx->caps & STMFBIO_VAR_CAPS_GAIN)
    printf("%d\n",varEx->gain);
  else
    printf("Unsupported\n");

  printf("Blend Mode    : %s\n",varEx->premultiplied_alpha?"Premultiplied":"Non-premultipled");
  printf("Rescale Mode  : %s\n",varEx->rescale_colour_to_video_range?"Video range":"Full range");

  printf("Analogue Out  : ");
  if(outputConfig->caps & STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG)
  {
    if(outputConfig->analogue_config & STMFBIO_OUTPUT_ANALOGUE_RGB)
      printf("RGB ");
    if(outputConfig->analogue_config & STMFBIO_OUTPUT_ANALOGUE_YPrPb)
      printf("YUV ");
    if(outputConfig->analogue_config & STMFBIO_OUTPUT_ANALOGUE_YC)
      printf("S-Video ");
    if(outputConfig->analogue_config & STMFBIO_OUTPUT_ANALOGUE_CVBS)
      printf("CVBS ");

    if((outputConfig->analogue_config & STMFBIO_OUTPUT_ANALOGUE_MASK)==0)
      printf("Disabled ");

    if(outputConfig->analogue_config & STMFBIO_OUTPUT_ANALOGUE_CLIP_FULLRANGE)
      printf("(Full range");
    else
      printf("(Video range");

    switch(outputConfig->analogue_config & STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_MASK)
    {
      case STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_AUTO:
        printf(", Auto colourspace");
        break;
      case STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_601:
        printf(", 601 colourspace");
        break;
      case STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_709:
        printf(", 709 colourspace");
        break;
    }

    printf(")\n");
  }
  else
  {
    printf("None\n");
  }

  printf("HDMI Out      : ");
  if(outputConfig->caps & STMFBIO_OUTPUT_CAPS_HDMI_CONFIG)
  {
    printf("%s",outputConfig->hdmi_config&STMFBIO_OUTPUT_HDMI_DISABLED?"Disabled ":"Enabled ");

    if(!(outputConfig->hdmi_config & STMFBIO_OUTPUT_HDMI_422))
    {
      switch(outputConfig->hdmi_config & STMFBIO_OUTPUT_HDMI_COLOURDEPTH_MASK)
      {
        case STMFBIO_OUTPUT_HDMI_COLOURDEPTH_30BIT:
          printf("(30bit ");
          break;
        case STMFBIO_OUTPUT_HDMI_COLOURDEPTH_36BIT:
          printf("(36bit ");
          break;
        case STMFBIO_OUTPUT_HDMI_COLOURDEPTH_48BIT:
          printf("(48bit ");
          break;
        default:
          printf("(24bit ");
          break;
      }

      if(outputConfig->hdmi_config & STMFBIO_OUTPUT_HDMI_YUV)
        printf(" YUV 4:4:4) ");
      else
        printf(" RGB) ");
    }
    else
    {
      printf("(YUV 4:2:2) ");
    }

    if(outputConfig->hdmi_config & STMFBIO_OUTPUT_HDMI_CLIP_FULLRANGE)
      printf("(Full range)");
    else
      printf("(Video range)");

    printf("\n");
  }
  else
  {
    printf("None\n");
  }

  printf("DVO Out       : ");
  if(outputConfig->caps & STMFBIO_OUTPUT_CAPS_DVO_CONFIG)
  {
    printf("%s",outputConfig->dvo_config&STMFBIO_OUTPUT_DVO_DISABLED?"Disabled ":"Enabled ");
    switch(outputConfig->dvo_config & STMFBIO_OUTPUT_DVO_MODE_MASK)
    {
      case STMFBIO_OUTPUT_DVO_YUV_444_16BIT:
        printf("(YUV 16bit 4:4:4) ");
        break;
      case STMFBIO_OUTPUT_DVO_YUV_444_24BIT:
        printf("(YUV 24bit 4:4:4) ");
        break;
      case STMFBIO_OUTPUT_DVO_YUV_422_16BIT:
        printf("(YUV 16bit 4:2:2) ");
        break;
      case STMFBIO_OUTPUT_DVO_ITUR656:
        printf("(ITU-R 656) ");
        break;
      case STMFBIO_OUTPUT_DVO_RGB_24BIT:
        printf("(RGB 24bit) ");
        break;
    }
    if(outputConfig->dvo_config & STMFBIO_OUTPUT_DVO_CLIP_FULLRANGE)
      printf("(Full range)");
    else
      printf("(Video range)");

    printf("\n");
  }
  else
  {
    printf("None\n");
  }

}

#define OUT_CVBS   (1<<0)
#define OUT_SVIDEO (1<<1)
#define OUT_YUV    (1<<2)
#define OUT_RGB    (1<<3)
#define OUT_HDMI   (1<<4)
#define OUT_DVO    (1<<5)


int main(int argc, char **argv)
{
  int fbfd;
  int change_alpha    = 0,     alpha       = 0;
  int change_gain     = 0,     gain        = 0;
  int change_blend    = 0,     blend       = 0;
  int change_dvo_mode = 0,     dvo_mode    = 0;
  int change_hdmi_colour = 0,  hdmi_colour = 0;
  int change_hdmi_bitdepth = 0, hdmi_bitdepth = 0;
  int change_rescale  = 0,     rescale     = 0;
  int change_cgms     = 0,     cgms        = 0;
  int change_hd_filter= 0,     hd_filter   = 0;
  int change_colourspace = 0,  colourspace = 0;
  int show            = 0;
  char *devname       = "/dev/fb0";
  int enables         = 0;
  int disables        = 0;
  int fullrange       = 0;
  int videorange      = 0;
  int change_ff_state = 0;
  stmfbio_ff_state ff_state      = STMFBIO_FF_OFF;
  int change_mixer_background    = 0;
  unsigned long mixer_background = 0;
  int option;

  struct stmfbio_var_screeninfo_ex varEx = {0};
  struct stmfbio_output_configuration outputConfig = {0};

  static struct option long_options[] = {
          { "transparency"           , 1, 0, 'a' },
          { "hdmi-deepcolour-bitdepth",1, 0, 'b' },
          { "disable-output"         , 1, 0, 'd' },
          { "enable-output"          , 1, 0, 'e' },
          { "fb"                     , 1, 0, 'f' },
          { "gain"                   , 1, 0, 'g' },
          { "help"                   , 0, 0, 'h' },
          { "premultiplied-alpha"    , 0, 0, 'p' },
          { "non-premultiplied-alpha", 0, 0, 'n' },
          { "show"                   , 0, 0, 's' },
          { "ycbcr-colourspace"      , 1, 0, 'C' },
          { "dvo-outputmode"         , 1, 0, 'D' },
          { "flicker-filter"         , 1, 0, 'F' },
          { "hdmi-colourspace"       , 1, 0, 'H' },
          { "mixer-background"       , 1, 0, 'M' },
          { "rescale-colour"         , 1, 0, 'R' },
          { "full-range"             , 1, 0, 'U' },
          { "video-range"            , 1, 0, 'V' },
          { 0, 0, 0, 0 }
  };


  if(argc < 2)
  {
    show = 1;
  }

  while((option = getopt_long (argc, argv, "a:b:d:e:f:g:hpnsB:C:D:F:H:M:R:U:V:x:", long_options, NULL)) != -1)
  {
    switch(option)
    {
      case 'a':
      {
        alpha = atoi(optarg);
        if(alpha<0 || alpha>255)
        {
          fprintf(stderr,"Alpha value out of range\n");
          exit(1);
        }
        else
        {
          change_alpha = 1;
        }

        break;
      }
      case 'b':
      {
        hdmi_bitdepth = atoi(optarg);
        switch(hdmi_bitdepth)
        {
          case 24:
            hdmi_bitdepth = STMFBIO_OUTPUT_HDMI_COLOURDEPTH_24BIT;
            break;
          case 30:
            hdmi_bitdepth = STMFBIO_OUTPUT_HDMI_COLOURDEPTH_30BIT;
            break;
          case 36:
            hdmi_bitdepth = STMFBIO_OUTPUT_HDMI_COLOURDEPTH_36BIT;
            break;
          case 48:
            hdmi_bitdepth = STMFBIO_OUTPUT_HDMI_COLOURDEPTH_48BIT;
            break;
          default:
            fprintf(stderr,"Invalid HDMI deepcolour mode\n");
            exit(1);
        }
        change_hdmi_bitdepth = 1;
        break;
      }
      case 'B':
      {
        switch(optarg[0])
        {
          case 'y':
          case 'Y':
            change_hd_filter = 1;
            hd_filter = 1;
            break;
          case 'n':
          case 'N':
            change_hd_filter = 1;
            hd_filter = 0;
            break;
          default:
            fprintf(stderr,"Unknown argument\n");
            exit(1);
        }
        break;
      }
      case 'd':
      {
        switch(optarg[0])
        {
          case 'c':
          case 'C':
            disables |= OUT_CVBS;
            break;
          case 'd':
          case 'D':
            disables |= OUT_DVO;
            break;
          case 'h':
          case 'H':
            disables |= OUT_HDMI;
            break;
          case 'r':
          case 'R':
            disables |= OUT_RGB;
            break;
          case 's':
          case 'S':
            disables |= OUT_SVIDEO;
            break;
          case 'y':
          case 'Y':
            disables |= OUT_YUV;
            break;
          default:
            fprintf(stderr,"Unknown disable output argument\n");
            exit(1);
        }
        break;
      }
      case 'C':
      {
        switch(optarg[0])
        {
          case 'a':
          case 'A':
            change_colourspace = 1;
            colourspace = STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_AUTO;
            break;
          case '6':
            change_colourspace = 1;
            colourspace = STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_601;
            break;
          case '7':
            change_colourspace = 1;
            colourspace = STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_709;
            break;
          default:
            fprintf(stderr,"Unknown YCbCr colourspace mode argument\n");
            exit(1);
        }
        break;
      }
      case 'D':
      {
        switch(optarg[0])
        {
          case 'i':
          case 'I':
            change_dvo_mode = 1;
            dvo_mode = STMFBIO_OUTPUT_DVO_ITUR656;
            break;
          case 'r':
          case 'R':
            change_dvo_mode = 1;
            dvo_mode = STMFBIO_OUTPUT_DVO_RGB_24BIT;
            break;
          case 'y':
          case 'Y':
            change_dvo_mode = 1;
            dvo_mode = STMFBIO_OUTPUT_DVO_YUV_444_16BIT;
            break;
          case '4':
            change_dvo_mode = 1;
            dvo_mode = STMFBIO_OUTPUT_DVO_YUV_422_16BIT;
            break;
          case '2':
            change_dvo_mode = 1;
            dvo_mode = STMFBIO_OUTPUT_DVO_YUV_444_24BIT;
            break;
          default:
            fprintf(stderr,"Unknown dvo output mode argument\n");
            exit(1);
        }
        break;
      }
      case 'e':
      {
        switch(optarg[0])
        {
          case 'c':
          case 'C':
            enables |= OUT_CVBS;
            break;
          case 'd':
          case 'D':
            enables |= OUT_DVO;
            break;
          case 'h':
          case 'H':
            enables |= OUT_HDMI;
            break;
          case 'r':
          case 'R':
            enables |= OUT_RGB;
            break;
          case 's':
          case 'S':
            enables |= OUT_SVIDEO;
            break;
          case 'y':
          case 'Y':
            enables |= OUT_YUV;
            break;
          default:
            fprintf(stderr,"Unknown enable output argument\n");
            exit(1);
        }
        break;
      }
      case 'f':
      {
        devname = optarg;
        break;
      }
      case 'F':
      {
        switch(optarg[0])
        {
          case 'a':
          case 'A':
            change_ff_state = 1;
            ff_state = STMFBIO_FF_ADAPTIVE;
            break;
          case 's':
          case 'S':
            change_ff_state = 1;
            ff_state = STMFBIO_FF_SIMPLE;
            break;
          case 'o':
          case 'O':
            change_ff_state = 1;
            ff_state = STMFBIO_FF_OFF;
            break;
          default:
            fprintf(stderr,"Unknown flicker filter argument\n");
            exit(1);
        }
        break;
      }
      case 'g':
      {
        gain = atoi(optarg);
        if(gain<0 || gain>255)
        {
          fprintf(stderr,"gain value out of range\n");
          exit(1);
        }
        else
        {
          change_gain = 1;
        }

        break;
      }
      case 'h':
      {
        usage();
        break;
      }
      case 'H':
      {
        switch(optarg[0])
        {
          case 'r':
          case 'R':
            change_hdmi_colour = 1;
            hdmi_colour = 0;
            break;
          case 'y':
          case 'Y':
            change_hdmi_colour = 1;
            hdmi_colour = 1;
            break;
          case '4':
            change_hdmi_colour = 1;
            hdmi_colour = 2;
            break;
          default:
            fprintf(stderr,"Unknown hdmi colourspace argument\n");
            exit(1);
        }
        break;
      }
      case 'p':
      {
        change_blend = 1;
        blend = 1;
        break;
      }
      case 'n':
      {
        change_blend = 1;
        blend = 0;
        break;
      }
      case 's':
      {
        show = 1;
        break;
      }
      case 'R':
      {
        switch(optarg[0])
        {
          case 'f':
          case 'F':
            change_rescale = 1;
            rescale = 0;
            break;
          case 'v':
          case 'V':
            change_rescale = 1;
            rescale = 1;
            break;
          default:
            fprintf(stderr,"Unknown rescale argument\n");
            exit(1);
            break;
        }
        break;
      }
      case 'M':
      {
        errno = 0;
        mixer_background = strtoul(optarg,NULL,16);
        if(errno != 0)
        {
          perror("Invalid mixer background colour argument");
          exit(1);
        }
        else
        {
          change_mixer_background = 1;
        }

        break;
      }
      case 'x':
      {
        change_cgms = 1;
        cgms = atoi(optarg);
        break;
      }
      case 'U':
      {
        switch(optarg[0])
        {
          case 'a':
          case 'A':
            fullrange |= OUT_CVBS;
            break;
          case 'd':
          case 'D':
            fullrange |= OUT_DVO;
            break;
          case 'h':
          case 'H':
            fullrange |= OUT_HDMI;
            break;
          default:
            fprintf(stderr,"Unknown full-range output argument\n");
            exit(1);
        }
        break;
      }
      case 'V':
      {
        switch(optarg[0])
        {
          case 'a':
          case 'A':
            videorange |= OUT_CVBS;
            break;
          case 'd':
          case 'D':
            videorange |= OUT_DVO;
            break;
          case 'h':
          case 'H':
            videorange |= OUT_HDMI;
            break;
          default:
            fprintf(stderr,"Unknown video-range output argument\n");
            exit(1);
        }
        break;
      }
      default:
      {
        usage();
        exit(1);
      }
    }
  }

  if((fbfd = open(devname, O_RDWR)) < 0)
  {
    fprintf (stderr, "Unable to open %s: %d (%m)\n", devname, errno);
    exit(1);
  }

  if(change_cgms)
  {
    ioctl(fbfd, STMFBIO_SET_CGMS_ANALOG, cgms);
  }

  if(change_hd_filter)
  {
    ioctl(fbfd, STMFBIO_SET_DAC_HD_FILTER, hd_filter);
  }

  varEx.layerid  = 0;
  varEx.caps     = 0;
  varEx.activate = STMFBIO_ACTIVATE_IMMEDIATE;

  outputConfig.outputid = STMFBIO_OUTPUTID_MAIN;
  if(ioctl(fbfd, STMFBIO_GET_OUTPUT_CONFIG, &outputConfig)<0)
    perror("Getting current output configuration failed");

  outputConfig.caps = 0;
  outputConfig.activate = STMFBIO_ACTIVATE_IMMEDIATE;

  if(change_alpha)
  {
    varEx.caps |= STMFBIO_VAR_CAPS_OPACITY;
    varEx.opacity = alpha;
  }

  if(change_gain)
  {
    varEx.caps |= STMFBIO_VAR_CAPS_GAIN;
    varEx.gain  = gain;
  }

  if(change_blend)
  {
    varEx.caps |= STMFBIO_VAR_CAPS_PREMULTIPLIED;
    varEx.premultiplied_alpha = blend;
  }

  if(change_rescale)
  {
    varEx.caps |= STBFBIO_VAR_CAPS_RESCALE_COLOUR_TO_VIDEO_RANGE;
    varEx.rescale_colour_to_video_range = rescale;
  }

  if(change_ff_state)
  {
    varEx.caps |= STMFBIO_VAR_CAPS_FLICKER_FILTER;
    varEx.ff_state = ff_state;
  }

  if(change_mixer_background)
  {
    outputConfig.caps |= STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND;
    outputConfig.mixer_background = mixer_background;
  }

  if(change_hdmi_colour)
  {
    outputConfig.caps |= STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
    outputConfig.hdmi_config &= ~(STMFBIO_OUTPUT_HDMI_YUV|STMFBIO_OUTPUT_HDMI_422);

    switch(hdmi_colour)
    {
      case 1:
        outputConfig.hdmi_config |= STMFBIO_OUTPUT_HDMI_YUV;
        break;
      case 2:
        outputConfig.hdmi_config |= (STMFBIO_OUTPUT_HDMI_YUV|STMFBIO_OUTPUT_HDMI_422);
        break;
      default:
        break;
    }
  }

  if(change_hdmi_bitdepth)
  {
    outputConfig.caps |= STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
    outputConfig.hdmi_config &= ~STMFBIO_OUTPUT_HDMI_COLOURDEPTH_MASK;
    outputConfig.hdmi_config |= hdmi_bitdepth;
  }

  if(change_dvo_mode)
  {
    outputConfig.caps |= STMFBIO_OUTPUT_CAPS_DVO_CONFIG;
    outputConfig.dvo_config &= ~STMFBIO_OUTPUT_DVO_MODE_MASK;
    outputConfig.dvo_config |= dvo_mode;
  }

  if(change_colourspace)
  {
    outputConfig.caps |= STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
    outputConfig.analogue_config &= ~STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_MASK;
    outputConfig.analogue_config |= colourspace;
  }

  if(disables)
  {
    if(disables&OUT_CVBS)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
      outputConfig.analogue_config &= ~STMFBIO_OUTPUT_ANALOGUE_CVBS;
    }
    if(disables&OUT_SVIDEO)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
      outputConfig.analogue_config &= ~STMFBIO_OUTPUT_ANALOGUE_YC;
    }
    if(disables&OUT_YUV)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
      outputConfig.analogue_config &= ~STMFBIO_OUTPUT_ANALOGUE_YPrPb;
    }
    if(disables&OUT_RGB)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
      outputConfig.analogue_config &= ~STMFBIO_OUTPUT_ANALOGUE_RGB;
    }
    if(disables&OUT_HDMI)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
      outputConfig.hdmi_config |= STMFBIO_OUTPUT_HDMI_DISABLED;
    }
    if(disables&OUT_DVO)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_DVO_CONFIG;
      outputConfig.dvo_config |= STMFBIO_OUTPUT_DVO_DISABLED;
    }
  }

  if(enables)
  {
    if(enables&OUT_CVBS)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
      outputConfig.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_CVBS;
    }
    if(enables&OUT_SVIDEO)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
      outputConfig.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_YC;
    }
    if(enables&OUT_YUV)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
      outputConfig.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_YPrPb;
    }
    if(enables&OUT_RGB)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
      outputConfig.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_RGB;
    }
    if(enables&OUT_HDMI)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
      outputConfig.hdmi_config &= ~STMFBIO_OUTPUT_HDMI_DISABLED;
    }
    if(enables&OUT_DVO)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_DVO_CONFIG;
      outputConfig.dvo_config &= ~STMFBIO_OUTPUT_DVO_DISABLED;
    }
  }


  if(fullrange)
  {
    if(fullrange&OUT_CVBS)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
      outputConfig.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_CLIP_FULLRANGE;
    }
    if(fullrange&OUT_HDMI)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
      outputConfig.hdmi_config |= STMFBIO_OUTPUT_HDMI_CLIP_FULLRANGE;
    }
    if(fullrange&OUT_DVO)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_DVO_CONFIG;
      outputConfig.dvo_config |= STMFBIO_OUTPUT_DVO_CLIP_FULLRANGE;
    }
  }


  if(videorange)
  {
    if(videorange&OUT_CVBS)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
      outputConfig.analogue_config &= ~STMFBIO_OUTPUT_ANALOGUE_CLIP_FULLRANGE;
    }
    if(videorange&OUT_HDMI)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
      outputConfig.hdmi_config &= ~STMFBIO_OUTPUT_HDMI_CLIP_FULLRANGE;
    }
    if(videorange&OUT_DVO)
    {
      outputConfig.caps |= STMFBIO_OUTPUT_CAPS_DVO_CONFIG;
      outputConfig.dvo_config &= ~STMFBIO_OUTPUT_DVO_CLIP_FULLRANGE;
    }
  }


  if(varEx.caps != STMFBIO_VAR_CAPS_NONE)
  {
    if(ioctl(fbfd, STMFBIO_SET_VAR_SCREENINFO_EX, &varEx)<0)
      perror("setting extended screen info failed");
  }

  if(outputConfig.caps != STMFBIO_OUTPUT_CAPS_NONE)
  {
    if(ioctl(fbfd, STMFBIO_SET_OUTPUT_CONFIG, &outputConfig)<0)
      perror("setting output configuration failed");
  }

  if(show)
  {
    if(ioctl(fbfd, STMFBIO_GET_OUTPUT_CONFIG, &outputConfig)<0)
      perror("Getting current output configuration failed");
    if(ioctl(fbfd, STMFBIO_GET_VAR_SCREENINFO_EX, &varEx)<0)
      perror("setting extended screen info failed");

    show_info(&varEx,&outputConfig);
  }

  return 0;
}
