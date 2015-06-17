/*
 * Control Application for FlexVP stuff (IQI, xp70, DEI)
 *
 * sh4-linux-gcc flexvp_ctrl.c -o flexvp_ctrl -Wall -O0 -ggdb
 */

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

#include "linux/drivers/media/video/stmvout.h"
#include "linux/tests/v4l2_helper.h"


#define X_SCREEN 1280
#define Y_SCREEN 720
#define CROP0_WIDTH 500
#define CROP0_HEIGHT 500
#define CROP1_WIDTH 100
#define CROP1_HEIGHT 100


#define LTV_DEBUG
#ifdef LTV_DEBUG

#define NOCOL      "\033[0m"

#define BLACKCOL   "\033[0;30m"
#define LGRAYCOL   "\033[0;37m"

#define REDCOL     "\033[0;31m"          // dvb warning
#define GREENCOL   "\033[0;32m"          // ui message
#define BROWNCOL   "\033[0;33m"
#define BLUECOL    "\033[0;34m"          // dvb message
#define PURPLECOL  "\033[0;35m"          // ui warning
#define CYANCOL    "\033[0;36m"

#define DGRAYCOL   "\033[1;30m"          // ca message
#define WHITECOL   "\033[1;37m"          // ca warning

#define LREDCOL     "\033[1;31m"
#define LGREENCOL   "\033[1;32m"         // dfb message
#define YELLOWCOL   "\033[1;33m"         // gfx print
#define LBLUECOL    "\033[1;34m"         // osd layer print
#define LPURPLECOL  "\033[1;35m"         // dfb warning
#define LCYANCOL    "\033[1;36m"         // scl layer print



#define DFBCHECK(x...)                                         \
  ( {                                                          \
    DFBResult err = x;                                         \
                                                               \
    if (err != DFB_OK)                                         \
      {                                                        \
        typeof(errno) errno_backup = errno;                    \
        ltv_warning ("%s <%d>:\n\t", __FILE__, __LINE__ );     \
        DirectFBError (#x, err);                               \
        errno = errno_backup;                                  \
      }                                                        \
  } )

#else /* LTV_DEBUG */

#define ltv_print(format, args...)    ( { } )
#define ltv_message(format, args...)  ( { } )
#define ltv_message(format, args...)  ( { } )

#define DFBCHECK(x...)      ( { x; } )

#endif /* LTV_DEBUG */


#define BUILD_BUG_ON_ZERO(e) (sizeof(char[1 - 2 * !!(e)]) - 1)
/* &a[0] degrades to a pointer: a different type from an array */
#define __must_be_array(a) \
  BUILD_BUG_ON_ZERO(__builtin_types_compatible_p(typeof(a), typeof(&a[0])))
#define N_ELEMENTS(array) (sizeof (array) / sizeof ((array)[0]) + __must_be_array (array))



static void
enumerate_menu (int                    fd,
                struct v4l2_queryctrl *queryctrl)
{
  struct v4l2_querymenu querymenu;

  ltv_message ("  Menu items (%x):", queryctrl->id);
  memset (&querymenu, 0, sizeof (querymenu));

  querymenu.id = queryctrl->id;
  for (querymenu.index = queryctrl->minimum;
       querymenu.index <= queryctrl->maximum;
       ++querymenu.index)
    {
      if (LTV_CHECK (ioctl (fd, VIDIOC_QUERYMENU, &querymenu)) == 0)
        ltv_message ("    %s", querymenu.name);
      else
        exit (EXIT_FAILURE);
    }
}


#define GET_CONTROL   (-2)
static void
do_iqiconfig (int fd, int config)
{
  struct v4l2_control control;
  control.id = V4L2_CID_STM_IQI_SET_CONFIG;

  control.value = config;
  if (config == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("iqi config is %d\n", control.value);
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_iqidemo (int fd, int on)
{
  struct v4l2_control control;
  control.id = V4L2_CID_STM_IQI_DEMO;

  control.value = on ? 1 : 0;
  if (on == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("iqi demo is %s\n", (control.value == 0) ? "off" : "on");
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_xvpmode (int fd, int mode)
{
  struct v4l2_control control;
  control.id = V4L2_CID_STM_XVP_SET_CONFIG;

  control.value = mode;
  if (mode == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("xp70 mode is %d\n", control.value);
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_xvptnrnleval (int fd, int val)
{
  struct v4l2_control control;
  control.id = V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE;

  control.value = val;
  if (val == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("tnr NLE override value is %d\n", control.value);
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_xvptnrtopbotswap (int fd, int val)
{
  struct v4l2_control control;
  control.id = V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP;

  control.value = val;
  if (val == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("tnr top/bottom swap is %s\n", control.value ? "on" : "off");
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_fmdmode (int fd, int val)
{
  struct v4l2_control control;
  control.id = V4L2_CID_STM_DEI_SET_FMD_ENABLE;

  control.value = val;
  if (val == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("FMD is turned %s\n", control.value ? "on" : "off");
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_fmdopts (int fd, int vals[12])
{
  struct v4l2_ext_controls controls;
  struct v4l2_ext_control  ctrl[1 + V4L2_CID_STM_FMD_LAST - V4L2_CID_STM_FMD_FIRST];
  int                      i;

  controls.ctrl_class = V4L2_CTRL_CLASS_STMVOUTEXT;
  controls.controls = ctrl;

  if (vals[0] == GET_CONTROL)
    {
      for (i = V4L2_CID_STM_FMD_FIRST; i <= V4L2_CID_STM_FMD_LAST; ++i)
        ctrl[i - V4L2_CID_STM_FMD_FIRST].id = i;

      controls.count = N_ELEMENTS (ctrl);
      LTV_CHECK (ioctl (fd, VIDIOC_G_EXT_CTRLS, &controls));
      for (i = 0; i < N_ELEMENTS (ctrl); ++i) {
        struct v4l2_queryctrl qctrl = { .id = controls.controls[i].id };
        if (LTV_CHECK (ioctl (fd, VIDIOC_QUERYCTRL, &qctrl)) == 0)
          ltv_message ("%s is %d", qctrl.name, controls.controls[i].value);
        else
          ltv_message ("%s is %d", "???", controls.controls[i].value);
      }
      printf ("same as");
      for (i = 0; i < N_ELEMENTS (ctrl); ++i)
        printf ("%c%.d", i ? ':' : ' ', controls.controls[i].value);
      printf ("\n");
    }
  else
    {
      controls.count = 0;

      for (i = V4L2_CID_STM_FMD_FIRST; i <= V4L2_CID_STM_FMD_LAST; ++i)
        if (vals[i - V4L2_CID_STM_FMD_FIRST] >= 0)
          {
            int idx = i - V4L2_CID_STM_FMD_FIRST;

            ctrl[idx].id = i;
            ctrl[idx].value = vals[idx];
            ++controls.count;
          }

      if (LTV_CHECK (ioctl (fd, VIDIOC_S_EXT_CTRLS, &controls)))
        ltv_warning ("failed index: %u\n", controls.error_idx);
    }
}

static void
do_deimode (int fd, int val)
{
  struct v4l2_control control;
  control.id = V4L2_CID_STM_DEI_SET_MODE;

  control.value = val;
  if (val == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("DEI mode is %d\n", control.value);
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_deictrl (int fd, int val)
{
  struct v4l2_control control;
  control.id = V4L2_CID_STM_DEI_SET_CTRLREG;

  control.value = val;
  if (val == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("DEI ctrl reg is %.8x\n", (unsigned int) control.value);
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_ckey (int fd, int val)
{
  struct v4l2_ext_controls controls;
  struct v4l2_ext_control ctrl[7];

  controls.ctrl_class = V4L2_CTRL_CLASS_STMVOUTEXT;
  controls.count = N_ELEMENTS (ctrl);
  controls.controls = ctrl;

  ctrl[0].id = V4L2_CID_STM_CKEY_ENABLE;
  ctrl[0].value = val;

  ctrl[1].id = V4L2_CID_STM_CKEY_FORMAT;
  ctrl[1].value = VCSCFM_CrYCb;

  ctrl[2].id = V4L2_CID_STM_CKEY_R_CR_MODE;
  ctrl[2].value = VCSCCCM_ENABLED;

  ctrl[3].id = V4L2_CID_STM_CKEY_G_Y_MODE;
  ctrl[3].value = VCSCCCM_DISABLED;

  ctrl[4].id = V4L2_CID_STM_CKEY_B_CB_MODE;
  ctrl[4].value = VCSCCCM_DISABLED;

  ctrl[5].id = V4L2_CID_STM_CKEY_MINVAL;
  ctrl[5].value = 0x600000;

  ctrl[6].id = V4L2_CID_STM_CKEY_MAXVAL;
  ctrl[6].value = 0xa00000;

  if (val == GET_CONTROL)
    {
      int i;
      LTV_CHECK (ioctl (fd, VIDIOC_G_EXT_CTRLS, &controls));
      for (i = 0; i < N_ELEMENTS (ctrl); ++i) {
	struct v4l2_queryctrl qctrl = { .id = controls.controls[i].id };
	if (LTV_CHECK (ioctl (fd, VIDIOC_QUERYCTRL, &qctrl)) == 0)
	  ltv_message ("%s is %.8x\n", qctrl.name, controls.controls[i].value);
	else
	  ltv_message ("%s is %.8x\n", "???", controls.controls[i].value);
      }
    }
  else if (LTV_CHECK (ioctl (fd, VIDIOC_S_EXT_CTRLS, &controls)))
    {
      ltv_warning ("failed index: %u\n", controls.error_idx);
    }
}

static void
do_brightness (int fd, int val, int plane_mode)
{
  struct v4l2_control control;
  control.id = plane_mode ? V4L2_CID_STM_PLANE_BRIGTHNESS : V4L2_CID_BRIGHTNESS;

  control.value = val;
  if (val == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("%sbrightness is %.8x\n",
                   plane_mode ? "plane " : "", (unsigned int) control.value);
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_contrast (int fd, int val, int plane_mode)
{
  struct v4l2_control control;
  control.id = plane_mode ? V4L2_CID_STM_PLANE_CONTRAST : V4L2_CID_CONTRAST;

  control.value = val;
  if (val == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("%scontrast is %.8x\n",
                   plane_mode ? "plane " : "", (unsigned int) control.value);
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_saturation (int fd, int val, int plane_mode)
{
  struct v4l2_control control;
  control.id = plane_mode ? V4L2_CID_STM_PLANE_SATURATION : V4L2_CID_SATURATION;

  control.value = val;
  if (val == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("%ssaturation is %.8x\n",
                   plane_mode ? "plane " : "", (unsigned int) control.value);
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_hue (int fd, int val, int plane_mode)
{
  struct v4l2_control control;
  control.id = plane_mode ? V4L2_CID_STM_PLANE_HUE : V4L2_CID_HUE;

  control.value = val;
  if (val == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("%shue is %.8x\n",
                   plane_mode ? "plane " : "", (unsigned int) control.value);
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}

static void
do_hide_show (int                                 fd,
              enum _V4L2_CID_STM_PLANE_HIDE_MODE  mode)
{
  struct v4l2_control control;
  control.id = V4L2_CID_STM_PLANE_HIDE_MODE;

  control.value = mode;
  if (mode == GET_CONTROL)
    {
      LTV_CHECK (ioctl (fd, VIDIOC_G_CTRL, &control));
      ltv_message ("plane show/hide mode is %d (%s)\n", control.value,
                   ((control.value == VCSPHM_MIXER_PULLSDATA)
                    ? "pulldata" : "disabled"));
    }
  else
    LTV_CHECK (ioctl (fd, VIDIOC_S_CTRL, &control));
}


int
main (int   argc,
      char *argv[])
{
  char                  devname[128];
  int                   videofd;
  struct v4l2_queryctrl queryctrl;
  static const char *ctrl_type[] = { "???", "int", "bool", "menu", "button",
                                     "int64", "ctrl_class" };

  int opt;
  int do_list = 0,
      iqiconfig = -1,
      iqidemo = -1,
      xvpmode = -1,
      nleval  = -1,
      topbotswapval = -1,
      fmdmode = -1,
      fmdopts[12],
      deimode = -1,
      deictrl = -1,
      ckey = -1,
      brightness = -1,
      contrast = -1,
      saturation = -1,
      hue = -1,
      bcsh_plane_mode = 0,
      plane_hide_mode = -1
    ;
  for (opt = 0; opt < N_ELEMENTS (fmdopts); ++opt)
    fmdopts[opt] = -1;

  while ((opt = getopt (argc, argv, "Cc:Dd:Xx:Nn:Tt:Ff:Pp:Ii:Rr:Kk:Vv:Bb:Zz:Mm:Aa:2lh")) != -1)
    {
      switch (opt)
        {
        case 'c': /* set iqi _c_onfig */
          iqiconfig = atoi (optarg);
          break;
        case 'C': /* get iqi _c_onfig */
          iqiconfig = GET_CONTROL;
          break;

        case 'd': /* set iqi _d_emo */
          iqidemo = atoi (optarg);
          break;
        case 'D': /* get iqi _d_emo */
          iqidemo = GET_CONTROL;
          break;

        case 'x': /* set _x_VP mode */
          xvpmode = atoi (optarg);
          break;
        case 'X': /* get _x_VP mode */
          xvpmode = GET_CONTROL;
          break;

        case 'n': /* set _N_LE override value to use */
          nleval = atoi (optarg);
          break;
        case 'N': /* get _N_LE override value to use */
          nleval = GET_CONTROL;
          break;

        case 't': /* set _T_op/bottom swap in TNR algo */
          topbotswapval = atoi (optarg);
          break;
        case 'T': /* get _T_op/bottom swap in TNR algo  */
          topbotswapval = GET_CONTROL;
          break;

        case 'f': /* set _f_md mode */
          fmdmode = atoi (optarg);
          break;
        case 'F': /* get _f_md mode */
          fmdmode = GET_CONTROL;
          break;

        case 'p': /* set field mode init _p_arams (and reinit) */
          {
          char *str = strdupa (optarg);
          char *t;

          /* t_mov */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[ 0] = atoi (t);
          /* t_num_mov_pix */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[ 1] = atoi (t);
          /* t_repeat */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[ 2] = atoi (t);
          /* t_scene */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[ 3] = atoi (t);
          /* count_miss */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[ 4] = atoi (t);
          /* count_still */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[ 5] = atoi (t);
          /* t_noise */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[ 6] = atoi (t);
          /* k_cfd1 */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[ 7] = atoi (t);
          /* k_cfd2 */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[ 8] = atoi (t);
          /* k_cfd3 */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[ 9] = atoi (t);
          /* k_scene */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[10] = atoi (t);
          /* d_scene */
          t = strsep (&str, ":,"); if (t && *t) fmdopts[11] = atoi (t);
          }
          break;
        case 'P': /* get field mode init _p_arams */
          fmdopts[0] = GET_CONTROL;
          break;

        case 'i': /* set de_i_ mode */
          deimode = atoi (optarg);
          break;
        case 'I': /* get de_i_ mode */
          deimode = GET_CONTROL;
          break;

        case 'r': /* set dei ctrl _r_eg */
          deictrl = strtoll (optarg, NULL, 0);
          break;
        case 'R': /* get dei ctrl _r_eg */
          deictrl = GET_CONTROL;
          break;

        case 'k': /* set color_k_ey */
          ckey = strtoll (optarg, NULL, 0);
          break;
        case 'K': /* get color_k_ey */
          ckey = GET_CONTROL;
          break;

        case 'v': /* brightness */
          brightness = strtoll (optarg, NULL, 0);
          break;
        case 'V': /* get brightness */
          brightness = GET_CONTROL;
          break;

        case 'b': /* contrast */
          contrast = strtoll (optarg, NULL, 0);
          break;
        case 'B': /* get contrast */
          contrast = GET_CONTROL;
          break;

        case 'z': /* set saturation */
          saturation = strtoll (optarg, NULL, 0);
          break;
        case 'Z': /* get saturation */
          saturation = GET_CONTROL;
          break;

        case 'm': /* set hue */
          hue = strtoll (optarg, NULL, 0);
          break;
        case 'M': /* get hue */
          hue = GET_CONTROL;
          break;

        case '2': /* let brightness/contrast/saturation/hue work on plane */
          bcsh_plane_mode = 1;
          break;

        case 'l': /* list available V4L2 controls */
          do_list = 1;
          break;

        case 'A': /* show plane */
          plane_hide_mode = GET_CONTROL;
          break;
        case 'a': /* hide mode: mixer pulls data */
          plane_hide_mode = strtoll (optarg, NULL, 0);
          break;

        case 'h':
        default:
          fprintf (stderr,
                   "Usage: %s [ OPTION ]\n"
                   "  -h                this help\n"
                   "  -l                list available V4L2 controls\n"
                   "\n"
                   "  [-C|-c CONFIG]    query/enable (1)/disable (0) IQI\n"
                   "  [-D|-d DEMO]      query/enable (1)/disable (0) IQI demo mode\n"
                   "  [-X|-x MODE]      query/set xVP mode: 0 bypass, 1: FilmGrain, 2: TNR, 3: TNR(bypass), 4: TNR(manual NLE)\n"
                   "  [-N|-n value]     query/value to use for NLE in when TNR mode is manual NLE\n"
                   "  [-T|-t 0|1]       query/swap top/bottom bit in TNR algo\n"
                   "  [-F|-f 0|1]       query/enable (1)/disable (0) FMD\n"
                   "  [-P|-p <opts>]    query/set FMD init params\n"
                   "  [-I|-i 0|1|2]     query/set DEI mode: 0: 3D, 1: bypass, 2: median\n"
                   "  [-R|-r value]     query/set DEI ctrl register\n"
                   "  [-K|-k value]     query/set color key\n"
                   "  [-V|-v value]     query/set Brightess\n"
                   "  [-B|-b value]     query/set Contrast\n"
                   "  [-Z|-z value]     query/set Saturation\n"
                   "  [-M|-m value]     query/set Hue\n"
                   "  [-A|-a mode]      query plane hide mode: 0: mixer pulls data. 1: mixer disabled\n"
                   "  [-2]              let Brightness/Contrast/Sat/Hue work on plane (instead of output)\n"
                   "\n"
                   "  fmd options specified as (example == default values)\n"
                   "   10:9:70:15:2:30:10:21:16:6:25:1"
                   "\n",
                   argv[0]);
          exit (EXIT_FAILURE);
          break;
        }
    }

  snprintf (devname, sizeof (devname), "/dev/video0");
  videofd = LTV_CHECK (open (devname, O_RDWR));
  if (videofd < 0)
    return 1;

  {
    struct v4l2_input input;
    __u32 input_idx = -1, input_idx2_read;

    input.index = 0;
    while (!ioctl (videofd, VIDIOC_ENUMINPUT, &input))
      {
	if (do_list)
	  ltv_message ("%s: Found input '%s' w/ idx/std/type/audioset: %d/%.8llx/%d/%d",
		       devname,
		       input.name, input.index, input.std, input.type,
		       input.audioset);
	if (strcasecmp ((char *) input.name, "D1-DVP0") == 0)
	  input_idx = input.index;

	++input.index;
      }

    ltv_print ("Setting input to %u\n", input_idx);
    LTV_CHECK (ioctl (videofd, VIDIOC_S_INPUT, &input_idx));
    LTV_CHECK (ioctl (videofd, VIDIOC_G_INPUT, &input_idx2_read));
    if (input_idx != input_idx2_read)
      ltv_message ("Driver BUG: input index is %d (should be: %d) - ignoring\n",
		   input_idx2_read, input_idx);
  }

  v4l2_list_outputs (videofd);
  v4l2_set_output_by_name (videofd, "YUV0");

  if (do_list)
    {
#if 1
      memset (&queryctrl, 0, sizeof (queryctrl));
      for (queryctrl.id = V4L2_CID_BASE;
           queryctrl.id < V4L2_CID_LASTP1;
           ++queryctrl.id)
        {
          if (ioctl (videofd, VIDIOC_QUERYCTRL, &queryctrl) == 0)
            {
              ltv_message ("%s%s control %s (min/max/def: %d/%d/%d)",
                           (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) ? "disabled " : "",
                           ctrl_type[queryctrl.type], queryctrl.name,
                           queryctrl.minimum, queryctrl.maximum,
                           queryctrl.default_value);

              if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                continue;

              if (queryctrl.type == V4L2_CTRL_TYPE_MENU)
                enumerate_menu (videofd, &queryctrl);
            }
          else
            {
              if (errno == EINVAL)
                continue;

              exit (EXIT_FAILURE);
            }
        }
      for (queryctrl.id = V4L2_CID_PRIVATE_BASE;
           ;
           ++queryctrl.id)
        {
          if (ioctl (videofd, VIDIOC_QUERYCTRL, &queryctrl) == 0)
            {
              ltv_message ("private %s%s control %s (minimum/max/def: %d/%d/%d)",
                           (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) ? "disabled " : "",
                           ctrl_type[queryctrl.type], queryctrl.name,
                           queryctrl.minimum, queryctrl.maximum,
                           queryctrl.default_value);

              if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                continue;

              if (queryctrl.type == V4L2_CTRL_TYPE_MENU)
                enumerate_menu (videofd, &queryctrl);
            }
          else
            {
              if (errno == EINVAL)
                break;

              ltv_warning ("VIDIOC_QUERYCTRL error %d %m", errno);
              exit (EXIT_FAILURE);
            }
        }
#endif

      /* new way */
      {
	queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (0 == ioctl (videofd, VIDIOC_QUERYCTRL, &queryctrl)) {
	  ltv_message ("some %s%s control (%x) %s (minimum/max/def: %d/%d/%d)",
		       (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) ? "disabled " : "",
		       ctrl_type[queryctrl.type], queryctrl.id, queryctrl.name,
		       queryctrl.minimum, queryctrl.maximum,
		       queryctrl.default_value);

	  if (!(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
	      && queryctrl.type == V4L2_CTRL_TYPE_MENU)
	    enumerate_menu (videofd, &queryctrl);

	  queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
      }
    }

  if (iqiconfig != -1)
    do_iqiconfig (videofd, iqiconfig);
  if (iqidemo != -1)
    do_iqidemo (videofd, iqidemo);
  if (xvpmode != -1)
    do_xvpmode (videofd, xvpmode);
  if (nleval != -1)
    do_xvptnrnleval (videofd, nleval);
  if (topbotswapval != -1)
    do_xvptnrtopbotswap (videofd, topbotswapval);
  if (fmdmode != -1)
    do_fmdmode (videofd, fmdmode);
  if (fmdopts[0] != -1)
    do_fmdopts (videofd, fmdopts);
  if (deimode != -1)
    do_deimode (videofd, deimode);
  if (deictrl != -1)
    do_deictrl (videofd, deictrl);
  if (ckey != -1)
    do_ckey (videofd, ckey);

  if (brightness != -1)
    do_brightness (videofd, brightness, bcsh_plane_mode);
  if (contrast != -1)
    do_contrast (videofd, contrast, bcsh_plane_mode);
  if (saturation != -1)
    do_saturation (videofd, saturation, bcsh_plane_mode);
  if (hue != -1)
    do_hue (videofd, hue, bcsh_plane_mode);

  if (plane_hide_mode != -1)
    do_hide_show (videofd, plane_hide_mode);

  LTV_CHECK (close (videofd));

  return 0;
}
