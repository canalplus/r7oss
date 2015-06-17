/***********************************************************************
 *
 * File: os21/tests/progressive_display/progressive_display.cpp
 * Copyright (c) 2004, 2005, 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * This is the most basic test of the display driver, to bring up
 * an progressive display with a framebuffer filled with a solid colour. It
 * provides sanity checks that the VTG is running at the right speed.
 *
\***********************************************************************/

#include <os21/stglib/application_helpers.h>

// use consts so they are visible in the debugger
static const int FBWIDTH       = 720;
static const int FBHEIGHT      = 480;
static const int FBDEPTH       = 32;
static const SURF_FMT FBPIXFMT = SURF_ARGB8888;
static const int FBSTRIDE      = (FBWIDTH*(FBDEPTH>>3));
static const int FBSIZE        = (FBSTRIDE*FBHEIGHT);

static unsigned long topfields      = 0;
static semaphore_t *frameupdate_sem = 0;
static semaphore_t *framerate_sem   = 0;
static semaphore_t *hotplug_sem     = 0;
static int framerate                = 50;
static stm_display_status_t currentDisplayStatus = STM_DISPLAY_DISCONNECTED;
static int silent_hotplug           = 0;

static stm_display_device_t   *pDev      = 0;
static stm_display_output_t   *pOutput   = 0;
static stm_display_output_t   *pHDMI     = 0;
static stm_display_output_t   *pDVO      = 0;
static const stm_mode_line_t  *pModeLine = 0;

// Note these are not static and can be changed by command line arguments.
stm_display_mode_t      MODE     = STVTG_TIMING_MODE_576P50000_27000;
unsigned long           STANDARD = STM_OUTPUT_STD_SMPTE293M;

static int vsync_isr(void *data)
{

  stm_display_output_handle_interrupts(pOutput);

  topfields++;

  if((topfields % framerate) == 0)
  {
    semaphore_signal(framerate_sem);
  }

  semaphore_signal(frameupdate_sem);

  if(pHDMI)
  {
    stm_display_status_t oldDisplayStatus = currentDisplayStatus;

    stm_display_output_get_status(pHDMI, &currentDisplayStatus);

    if(hotplug_poll_pio)
    {
      unsigned char hotplugstate = *hotplug_poll_pio & hotplug_pio_pin;
      if(currentDisplayStatus == STM_DISPLAY_DISCONNECTED)
      {
        /*
         * If the device has just been plugged in, flag that we now need to
         * start the output.
         */
        if(hotplugstate != 0)
        {
          currentDisplayStatus = STM_DISPLAY_NEEDS_RESTART;
          stm_display_output_set_status(pHDMI, currentDisplayStatus);
        }
      }
      else
      {
        /*
         * We may either be waiting for the output to be started, or already
         * started, so only change the state if the device has now been
         * disconnected.
         */
        if(hotplugstate == 0)
        {
          currentDisplayStatus = STM_DISPLAY_DISCONNECTED;
          stm_display_output_set_status(pHDMI, currentDisplayStatus);
        }
      }
    }

    if(oldDisplayStatus != currentDisplayStatus)
    {
      semaphore_signal(hotplug_sem);
    }
  }

  return OS21_SUCCESS;
}


/*
 * We cannot call the stm_display_update method directly in OS21 from interrupt
 * context, as it will use "free" which can block on a mutex. This is
 * illegal behaviour in OS21 interrupt routines, you cannot de-schedule.
 */
void displayupdate_task_fn(void *val)
{
  while(1)
  {
    semaphore_wait(frameupdate_sem);
    stm_display_update(pDev, pOutput);
  }
}


void hotplug_task_fn(void *val)
{
  while(1)
  {
    semaphore_wait(hotplug_sem);
    switch(currentDisplayStatus)
    {
      case STM_DISPLAY_NEEDS_RESTART:
        if(!silent_hotplug) printf("HDMI TV Starting Link\n");
        if(stm_display_output_start(pHDMI, pModeLine, STM_OUTPUT_STD_CEA861C)<0)
        {
          if(!silent_hotplug) printf("Unable to start HDMI\n");
        }
        break;
      case STM_DISPLAY_CONNECTED:
        if(!silent_hotplug) printf("HDMI TV Successfully Connected\n");
        break;
      case STM_DISPLAY_DISCONNECTED:
        if(!silent_hotplug) printf("HDMI TV Disconnected\n");
        stm_display_output_stop(pHDMI);
        break;
    }
  }
}


void usage(void)
{
    fprintf(stderr,"Usage: progressive_display [C] [d(VI)] [h(D) [50|59|60]] [p [23|24|25|29|30|50|59|60]] \n");
    fprintf(stderr,"\t [r(GB)] [s(D) [50|59|60]] [t secs] [v(GA) [59|60]\n\n");
    fprintf(stderr,"\td      - Select DVI not HDMI signalling\n");
    fprintf(stderr,"\th num  - Select 1280x720p @ 50, 59.94 or 60Hz\n");
    fprintf(stderr,"\tr      - Select RGB (not YPrPb) on analogue component output and HDMI\n");
    fprintf(stderr,"\tp num  - Select 1920x1080p @ 50, 59.94, 60, 30, 29.97, 25, 24 or 23.976Hz\n");
    fprintf(stderr,"\ts num  - Select 720x576p @ 50Hz or 720x483p @ 59.94 or 60Hz\n");
    fprintf(stderr,"\tt secs - Run for specified number of seconds, default 60\n");
    fprintf(stderr,"\tv num  - Select 640x480 @ 59.94Hz or 60Hz (VGA)\n");
    fprintf(stderr,"\tC      - Clip video signal to values 16-235/240\n");
    exit(1);
}


int main(int argc, char **argv)
{
stm_display_plane_t    *pPlane;
stm_display_buffer_t    buffer_setup;
char                   *fbuffer;
void                   *fbufferphys;
interrupt_t            *vsync_interrupt=0;
interrupt_t            *hdmi_interrupt=0;
int                     err;
int                     seconds;
int                     clip;
int                     rgb;
int                     dvi;
stm_plane_id_t          planeid;
osclock_t               lasttime;

  kernel_initialize(NULL);
  kernel_start();
  kernel_timeslice(OS21_TRUE);

  framerate_sem      = semaphore_create_fifo(0);
  frameupdate_sem    = semaphore_create_fifo(0);
  hotplug_sem        = semaphore_create_fifo(0);

  task_create(displayupdate_task_fn, 0, OS21_DEF_MIN_STACK_SIZE, MAX_USER_PRIORITY, "displayupdate", 0);

  pDev = stm_display_get_device(0);
  if(!pDev)
  {
    printf("Unable to create device instance\n");
    return 1;
  }

  if(argc<2)
    usage();

  argc--;
  argv++;

  seconds = 60;
  rgb     = 0;
  dvi     = 0;
  clip    = 0;

  while(argc>0)
  {
    switch(**argv)
    {
      case 'C':
      {
        printf("Clipping video signal selected\n");
        clip = 1;
        break;
      }
      case 'd':
      {
        printf("Setting DVI mode on HDMI output\n");
        dvi = 1;
        break;
      }
      case 'h':
      {
        int refresh;

        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing HD vertical refresh frequency\n");
          usage();
        }

        refresh = atoi(*argv);
        switch(refresh)
        {
          case 50:
            printf("Setting 1280x720-50\n");
            MODE      = STVTG_TIMING_MODE_720P50000_74250;
            STANDARD  = STM_OUTPUT_STD_SMPTE296M;
            framerate = 50;
            break;
          case 59:
            printf("Setting 1280x720-59\n");
            MODE      = STVTG_TIMING_MODE_720P59940_74176;
            STANDARD  = STM_OUTPUT_STD_SMPTE296M;
            framerate = 60;
            break;
          case 60:
            printf("Setting 1280x720-60\n");
            MODE      = STVTG_TIMING_MODE_720P60000_74250;
            STANDARD  = STM_OUTPUT_STD_SMPTE296M;
            framerate = 60;
            break;
          default:
            fprintf(stderr,"Unsupported HD vertical refresh frequency\n");
            usage();
        }
        break;
      }
      case 'r':
      {
        printf("Setting Component RGB Outputs\n");
        rgb = 1;
        break;
      }
      case 'p':
      {
        int refresh;

        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing 1080p vertical refresh frequency\n");
          usage();
        }

        refresh = atoi(*argv);
        switch(refresh)
        {
          case 23:
            printf("Setting 1920x1080-23\n");
            MODE      = STVTG_TIMING_MODE_1080P23976_74176;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 24;
            break;
          case 24:
            printf("Setting 1920x1080-24\n");
            MODE      = STVTG_TIMING_MODE_1080P24000_74250;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 24;
            break;
          case 25:
            printf("Setting 1920x1080-25\n");
            MODE      = STVTG_TIMING_MODE_1080P25000_74250;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 25;
            break;
          case 29:
            printf("Setting 1920x1080-29\n");
            MODE      = STVTG_TIMING_MODE_1080P29970_74176;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 30;
            break;
          case 30:
            printf("Setting 1920x1080-30\n");
            MODE      = STVTG_TIMING_MODE_1080P30000_74250;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 30;
            break;
          case 50:
            printf("Setting 1920x1080-50\n");
            MODE      = STVTG_TIMING_MODE_1080P50000_148500;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 50;
            break;
          case 59:
            printf("Setting 1920x1080-59\n");
            MODE      = STVTG_TIMING_MODE_1080P59940_148352;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 60;
            break;
          case 60:
            printf("Setting 1920x1080-60\n");
            MODE      = STVTG_TIMING_MODE_1080P60000_148500;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 60;
            break;
          default:
            fprintf(stderr,"Unsupported HD vertical refresh frequency\n");
            usage();
        }
        break;
      }
      case 's':
      {
        int refresh;

        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing SD vertical refresh frequency\n");
          usage();
        }

        refresh = atoi(*argv);
        switch(refresh)
        {
          case 50:
            printf("Setting 720x576-50\n");
            MODE      = STVTG_TIMING_MODE_576P50000_27000;
            STANDARD  = STM_OUTPUT_STD_SMPTE293M;
            framerate = 50;
            break;
          case 59:
            printf("Setting 720x480-59\n");
            MODE      = STVTG_TIMING_MODE_480P59940_27000;
            STANDARD  = STM_OUTPUT_STD_SMPTE293M;
            framerate = 60;
            break;
          case 60:
            printf("Setting 720x480-60\n");
            MODE      = STVTG_TIMING_MODE_480P60000_27027;
            STANDARD  = STM_OUTPUT_STD_SMPTE293M;
            framerate = 60;
            break;
          default:
            fprintf(stderr,"Unsupported SD vertical refresh frequency\n");
            usage();
        }
        break;
      }
      case 't':
      {
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing seconds\n");
          usage();
        }

        seconds = atoi(*argv);
        if(seconds<0)
          usage();

        break;
      }
      case 'v':
      {
        int refresh;

        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing vertical refresh frequency\n");
          usage();
        }

        refresh = atoi(*argv);
        switch(refresh)
        {
          case 59:
            printf("Setting 640x480-59\n");
            MODE      = STVTG_TIMING_MODE_480P59940_25180;
            STANDARD  = STM_OUTPUT_STD_VESA;
            framerate = 60;
            break;
          case 60:
            printf("Setting 640x480-60\n");
            MODE      = STVTG_TIMING_MODE_480P60000_25200;
            STANDARD  = STM_OUTPUT_STD_VESA;
            framerate = 60;
            break;
          default:
            fprintf(stderr,"Unsupported vertical refresh frequency\n");
            usage();
        }
        break;
      }
      default:
        fprintf(stderr,"Unknown option '%s'\n",*argv);
        usage();
    }

    argc--;
    argv++;
  }


  planeid  = OUTPUT_GDP1;

  vsync_interrupt = get_main_vtg_interrupt();

  setup_soc();

  if(!vsync_interrupt)
  {
    printf("Cannot find VSYNC interrupt handler\n");
    return 1;
  }

  err  = interrupt_install(vsync_interrupt, vsync_isr, pDev);
  err += interrupt_enable(vsync_interrupt);
  if(err>0)
  {
    printf("Unable to install and enable VSYNC interrupt\n");
    return 1;
  }

  fbuffer = (char *)malloc(FBSIZE);
  if(!fbuffer)
  {
    printf("Unable to allocate framebuffer\n");
    return 1;
  }

  memset(fbuffer, 0x00, FBSIZE);
  create_test_pattern(fbuffer, FBWIDTH, FBHEIGHT, FBSTRIDE);
  cache_purge_data(fbuffer, FBSIZE);

  pOutput = stm_display_get_output(pDev, 0);
  if(!pOutput)
  {
    printf("Unable to get output\n");
    return 1;
  }


  setup_analogue_voltages(pOutput);
  stm_display_output_set_control(pOutput, STM_CTRL_SIGNAL_RANGE, clip?STM_SIGNAL_VIDEO_RANGE:STM_SIGNAL_FILTER_SAV_EAV);

  pDVO = get_dvo_output(pDev);

  {
    pHDMI = get_hdmi_output(pDev);

    if(pHDMI)
    {
      /*
       * Now we have a HDMI output pointer to handle hotplug interrupts,
       * we can enable the interrupt handlers.
       */
      hdmi_interrupt = get_hdmi_interrupt();
      if(hdmi_interrupt)
      {
        err  = interrupt_install(hdmi_interrupt, hdmi_isr, pHDMI);
        err += interrupt_enable(hdmi_interrupt);
      }

      if(err>0)
      {
        printf("Unable to install and enable hdmi interrupts\n");
        return 1;
      }

      stm_display_output_set_control(pHDMI, STM_CTRL_SIGNAL_RANGE, clip?STM_SIGNAL_VIDEO_RANGE:STM_SIGNAL_FILTER_SAV_EAV);

    }
    else
    {
      printf("Hmmm, no HDMI output available\n");
    }
  }


  pModeLine = stm_display_output_get_display_mode(pOutput, MODE);
  if(!pModeLine)
  {
    printf("Unable to use requested display mode\n");
    return 1;
  }

  pPlane = stm_display_get_plane(pDev, planeid);
  if(!pPlane)
  {
    printf("Unable to get graphics plane\n");
    return 1;
  }

  if(stm_display_plane_connect_to_output(pPlane, pOutput)<0)
  {
    printf("Unable to display plane on output\n");
    return 1;
  }

  if(stm_display_plane_lock(pPlane)<0)
  {
    printf("Unable to lock plane's buffer queue\n");
    return 1;
  }

  memset(&buffer_setup, 0, sizeof(buffer_setup));

  vmem_virt_to_phys(fbuffer, &fbufferphys);
  buffer_setup.src.ulVideoBufferAddr = (ULONG)fbufferphys;
  buffer_setup.src.ulVideoBufferSize = FBSIZE;

  buffer_setup.src.ulStride     = FBSTRIDE;
  buffer_setup.src.ulPixelDepth = FBDEPTH;
  buffer_setup.src.ulColorFmt   = FBPIXFMT;
  buffer_setup.src.Rect.width   = FBWIDTH;
  buffer_setup.src.Rect.height  = FBHEIGHT;
  buffer_setup.dst.Rect.width   = FBWIDTH;
  buffer_setup.dst.Rect.height  = FBHEIGHT;

  buffer_setup.info.ulFlags = STM_PLANE_PRESENTATION_PERSISTENT;

  printf("Clock is running at %ld ticks per second\n",(long)time_ticks_per_sec());

  ULONG format=0;

  format = rgb?STM_VIDEO_OUT_RGB:STM_VIDEO_OUT_YUV;

  stm_display_output_set_control(pOutput, STM_CTRL_VIDEO_OUT_SELECT, format);

  if(pHDMI)
  {
    ULONG format = 0;

    format |= rgb?STM_VIDEO_OUT_RGB:STM_VIDEO_OUT_YUV;
    format |= dvi?STM_VIDEO_OUT_DVI:STM_VIDEO_OUT_HDMI;

    stm_display_output_set_control(pHDMI, STM_CTRL_VIDEO_OUT_SELECT, format);
  }


  if(stm_display_output_start(pOutput, pModeLine, STANDARD)<0)
  {
    printf("Unable to start display\n");
    return 1;
  }

  if(pDVO)
  {
    printf("Info: Attempting to start DVO\n");
    if(stm_display_output_start(pDVO, pModeLine, STANDARD)<0)
    {
      printf("Info: Unable to start DVO\n");
    }
  }

  lasttime = time_now(); // VTG Start time (approx)

  if(stm_display_plane_queue_buffer(pPlane, &buffer_setup)<0)
  {
    printf("Unable to queue framebuffer for display on graphics plane\n");
    return 1;
  }

  task_create(hotplug_task_fn, 0, OS21_DEF_MIN_STACK_SIZE, MIN_USER_PRIORITY, "hotplug", 0);

  if(seconds == 0)
  {
    task_delay(time_ticks_per_sec()*5);
    task_priority_set(NULL,MIN_USER_PRIORITY);
    silent_hotplug = 1;
    err = get_yesno();
  }
  else
  {
    while(seconds>0)
    {
    osclock_t now,delta;

      semaphore_wait(framerate_sem);

      now   = time_now();
      delta = time_minus(now,lasttime);
      printf("%d frames took %ld ticks.\n",framerate, (long)delta);

      lasttime = now;
      seconds--;
    }

    err = 0;
  }

  stm_display_plane_flush(pPlane);
  stm_display_plane_disconnect_from_output(pPlane, pOutput);
  stm_display_output_stop(pOutput);

  interrupt_disable(vsync_interrupt);
  interrupt_disable(hdmi_interrupt);

  stm_display_plane_release(pPlane);

  if(pDVO)
    stm_display_output_release(pDVO);

  if(pHDMI)
    stm_display_output_release(pHDMI);

  stm_display_output_release(pOutput);

  stm_display_release_device(pDev);

  return err;
}
