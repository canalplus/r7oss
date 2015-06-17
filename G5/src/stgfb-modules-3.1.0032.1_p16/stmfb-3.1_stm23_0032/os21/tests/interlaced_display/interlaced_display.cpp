/***********************************************************************
 *
 * File: os21/tests/interlaced_display/interlaced_display.cpp
 * Copyright (c) 2004, 2005, 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * This is the most basic test of the display driver, to bring up
 * an interlaced display with a framebuffer filled with a solid colour. It
 * provides sanity checks that the VTG is running at the right speed
 * and is producing the correct top/bottom field interrupt sequence.
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
static unsigned long botfields      = 0;
static unsigned long duptopfields   = 0;
static unsigned long dupbotfields   = 0;
static semaphore_t *frameupdate_sem = 0;
static semaphore_t *framerate_sem   = 0;
static semaphore_t *hotplug_sem     = 0;
static int framerate                = 50;
static int silent_hotplug           = 0;

static long lastfield = STM_BOTTOM_FIELD;
static stm_display_status_t currentDisplayStatus = STM_DISPLAY_DISCONNECTED;

static stm_display_device_t   *pDev      = 0;
static stm_display_output_t   *pOutput   = 0;
static stm_display_output_t   *pHDMI     = 0;
static stm_display_output_t   *pDVO      = 0;
static const stm_mode_line_t  *pModeLine = 0;
static const stm_mode_line_t  *pHDMIModeLine = 0;
static ULONG                   hdmistandard  = STM_OUTPUT_STD_CEA861C;

stm_display_mode_t  MODE     = STVTG_TIMING_MODE_576I50000_13500;
unsigned long       STANDARD = STM_OUTPUT_STD_PAL_BDGHI;

static int vsync_isr(void *data)
{
  stm_field_t current_field;
  TIME64      dummy;

  stm_display_output_handle_interrupts(pOutput);

  // Some housekeeping to ensure the VTG is in the expected mode.
  stm_display_output_get_last_vsync_info(pOutput, &current_field, &dummy);

  if(current_field == STM_TOP_FIELD)
  {
    if(lastfield == STM_TOP_FIELD)
      duptopfields++;

    lastfield = STM_TOP_FIELD;

    topfields++;
  }
  else
  {
    if(lastfield == STM_BOTTOM_FIELD)
      dupbotfields++;

    botfields++;

    lastfield = STM_BOTTOM_FIELD;
  }

  if(((topfields+botfields) % framerate) == 0)
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
 * We cannot call the  stm_display_update method directly in OS21 from interrupt
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
        if(stm_display_output_start(pHDMI, pHDMIModeLine, hdmistandard)<0)
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
    fprintf(stderr,"Usage: interlaced_display [options]\n");
    fprintf(stderr,"\ta      - Select 720x576 @ 50Hz PAL-N\n");
    fprintf(stderr,"\tc      - Select 720x576 @ 50Hz PAL-Nc\n");
    fprintf(stderr,"\td      - Select DVI instead HDMI signalling\n");
    fprintf(stderr,"\tf      - Enable CVBS trap filtering\n");
    fprintf(stderr,"\th [50|59|60|1250]\n");
    fprintf(stderr,"\t       - Select 1920x1080i @ 50,59.94 or 60Hz, 1250 = Austrailian 50Hz/1250 Lines\n");
    fprintf(stderr,"\tj      - Select 720x480 @ 59.94 NTSC-J\n");
    fprintf(stderr,"\tn [59|60|443]\n");
    fprintf(stderr,"\t       - Select 720x480 @ 59.94 or 60Hz NTSC-M or NTSC-443\n");
    fprintf(stderr,"\tm      - Select 720x480 @ 59.94 PAL-M\n");
    fprintf(stderr,"\to num  - Select output number, 0 - main, 1 - aux\n");
    fprintf(stderr,"\tp      - Select 720x576 @ 50Hz PAL-BDGHI\n");
    fprintf(stderr,"\tr      - Select RGB (not YPrPb) on analogue component output and HDMI\n");
    fprintf(stderr,"\ts      - Select 720x576 @ 50Hz SECAM\n");
    fprintf(stderr,"\tt secs - Run for specified number of seconds, default 60\n");
    fprintf(stderr,"\tx      - Select 720x480 @ 59.94Hz PAL-60\n");
    fprintf(stderr,"\tC      - Clip video signal to values 16-235/240\n");
    fprintf(stderr,"\tD num  - Select display number to use (for STx7200, 0 - local 1 - remote)\n");
    fprintf(stderr,"\tE num  - Set a video clock error adjustment in +/- ppm\n");
    exit(1);
}



int main(int argc, char **argv)
{
stm_display_plane_t    *pPlane;
stm_display_buffer_t    buffer_setup;
interrupt_t            *vsync_interrupt=0;
interrupt_t            *hdmi_interrupt=0;
char                   *fbuffer;
void                   *fbufferphys;
int                     err;
int                     output;
int                     seconds;
int                     clip;
int                     rgb;
int                     dvi;
int                     cvbstrapfilter;
int                     device;
int                     error_ppm;
stm_plane_id_t          planeid;
osclock_t               lasttime;


  kernel_initialize(NULL);
  kernel_start();
  kernel_timeslice(OS21_TRUE);

  framerate_sem      = semaphore_create_fifo(0);
  frameupdate_sem    = semaphore_create_fifo(0);
  hotplug_sem        = semaphore_create_fifo(0);

  task_create(displayupdate_task_fn, 0, OS21_DEF_MIN_STACK_SIZE, MAX_USER_PRIORITY, "displayupdate", 0);


  if(argc<2)
    usage();

  argc--;
  argv++;

  output  = 0;
  seconds = 60;
  rgb     = 0;
  dvi     = 0;
  cvbstrapfilter = 0;
  clip    = 0;
  device  = 0;
  error_ppm = 0;

  while(argc>0)
  {
    switch(**argv)
    {
      case 'a':
      {
        printf("Setting 720x576-50i PAL-N\n");
        MODE      = STVTG_TIMING_MODE_576I50000_13500;
        STANDARD  = STM_OUTPUT_STD_PAL_N;
        framerate = 50;
        break;
      }
      case 'c':
      {
        printf("Setting 720x576-50i PAL-Nc\n");
        MODE      = STVTG_TIMING_MODE_576I50000_13500;
        STANDARD  = STM_OUTPUT_STD_PAL_Nc;
        framerate = 50;
        break;
      }
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
      case 'f':
      {
        printf("Setting CVBS trap filter\n");
        cvbstrapfilter = 1;
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
            printf("Setting 1920x1080-50i\n");
            MODE      = STVTG_TIMING_MODE_1080I50000_74250_274M;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 50;
            break;
          case 1250:
            printf("Setting 1920x1080-50i Australian 1250lines\n");
            MODE      = STVTG_TIMING_MODE_1080I50000_72000;
            STANDARD  = STM_OUTPUT_STD_AS4933;
            framerate = 50;
            break;
          case 59:
            printf("Setting 1920x1080-59i\n");
            MODE      = STVTG_TIMING_MODE_1080I59940_74176;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 60;
            break;
          case 60:
            printf("Setting 1920x1080-60i\n");
            MODE      = STVTG_TIMING_MODE_1080I60000_74250;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 60;
            break;
          default:
            fprintf(stderr,"Unsupported HD vertical refresh frequency\n");
            usage();
        }
        break;
      }
      case 'j':
      {
        printf("Setting 720x480-59i NTSC-J\n");
        MODE      = STVTG_TIMING_MODE_480I59940_13500;
        STANDARD  = STM_OUTPUT_STD_NTSC_J;
        framerate = 60;
        break;
      }
      case 'n':
      {
        int refresh;

        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing NTSC vertical refresh frequency\n");
          usage();
        }

        refresh = atoi(*argv);
        switch(refresh)
        {
          case 59:
            printf("Setting 720x480-59i NTSC-M\n");
            MODE      = STVTG_TIMING_MODE_480I59940_13500;
            STANDARD  = STM_OUTPUT_STD_NTSC_M;
            framerate = 60;
            break;
          case 60:
            printf("Setting 720x480-60i NTSC-M\n");
            MODE      = STVTG_TIMING_MODE_480I60000_13514;
            STANDARD  = STM_OUTPUT_STD_NTSC_M;
            framerate = 60;
            break;
          case 443:
            printf("Setting 720x480-59i NTSC-443\n");
            MODE      = STVTG_TIMING_MODE_480I59940_13500;
            STANDARD  = STM_OUTPUT_STD_NTSC_443;
            framerate = 60;
            break;
          default:
            fprintf(stderr,"Unsupported NTSC vertical refresh frequency\n");
            usage();
        }
        break;
      }
      case 'm':
      {
        printf("Setting 720x480-59i PAL-M\n");
        MODE      = STVTG_TIMING_MODE_480I59940_13500;
        STANDARD  = STM_OUTPUT_STD_PAL_M;
        framerate = 60;
        break;
      }
      case 'o':
      {
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing output number\n");
          usage();
        }

        output = atoi(*argv);
        if(output<0 || output>1)
        {
          fprintf(stderr,"Output number out of range\n");
          usage();
        }

        break;
      }
      case 'p':
      {
        printf("Setting 720x576-50i PAL\n");
        MODE      = STVTG_TIMING_MODE_576I50000_13500;
        STANDARD  = STM_OUTPUT_STD_PAL_BDGHI;
        framerate = 50;
        break;
      }
      case 'r':
      {
        printf("Setting Component RGB Outputs\n");
        rgb = 1;
        break;
      }
      case 's':
      {
        printf("Setting 720x576-50i SECAM\n");
        MODE      = STVTG_TIMING_MODE_576I50000_13500;
        STANDARD  = STM_OUTPUT_STD_SECAM;
        framerate = 50;
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
      case 'x':
      {
        printf("Setting 720x480-59i PAL-60\n");
        MODE      = STVTG_TIMING_MODE_480I59940_13500;
        STANDARD  = STM_OUTPUT_STD_PAL_60;
        framerate = 60;
        break;
      }
      case 'D':
      {
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing display number\n");
          usage();
        }

        device = atoi(*argv);
        if(device<=0)
          usage();

        break;
      }
      case 'E':
      {
        argc--;
        argv++;

        if(argc <= 0)
        {
          fprintf(stderr,"Missing error value\n");
          usage();
        }

        error_ppm = atoi(*argv);

        break;
      }
      default:
        fprintf(stderr,"Unknown option '%s'\n",*argv);
        usage();
    }

    argc--;
    argv++;
  }

  pDev = stm_display_get_device(device);

  if(!pDev)
  {
    fprintf(stderr,"Display device not found\n");
    exit(1);
  }

#if defined(CONFIG_STB7100)
  if(output == 0)
  {
    printf("Selecting STb7100 Main output pipeline\n");
    planeid  = OUTPUT_GDP1;
    vsync_interrupt = interrupt_handle(OS21_INTERRUPT_VTG_1);
  }
  else
  {
    printf("Selecting STb7100 Aux output pipeline\n");
    planeid  = OUTPUT_GDP3;
    vsync_interrupt = interrupt_handle(OS21_INTERRUPT_VTG_2);
  }

#elif defined(CONFIG_STI7111) || defined(CONFIG_STI7105) || defined(CONFIG_STI7106)
  if(output == 0)
  {
    printf("Selecting STx7111/STx7105/STx7106 Main output pipeline\n");
    planeid  = OUTPUT_GDP1;
    vsync_interrupt = interrupt_handle(OS21_INTERRUPT_MAIN_VTG);
  }
  else
  {
    printf("Selecting STx7111/STx7105/STx7106 Aux output pipeline\n");
    planeid  = OUTPUT_GDP3;
    vsync_interrupt = interrupt_handle(OS21_INTERRUPT_AUX_VTG);
  }

#elif defined(CONFIG_STI7141)
  if(output == 0)
  {
    printf("Selecting STx7141 Main output pipeline\n");
    planeid  = OUTPUT_GDP1;
    vsync_interrupt = interrupt_handle(OS21_INTERRUPT_IRQ_MAIN_VTG_0);
  }
  else
  {
    printf("Selecting STx7141 Aux output pipeline\n");
    planeid  = OUTPUT_GDP3;
    vsync_interrupt = interrupt_handle(OS21_INTERRUPT_IRQ_AUX_VTG_0);
  }

#elif defined(CONFIG_STI7200)
  if(device == 0)
  {
    if(output == 0)
    {
      printf("Selecting STx7200 Main output pipeline\n");
      planeid  = OUTPUT_GDP1;
      vsync_interrupt = interrupt_handle(OS21_INTERRUPT_VTG_MAIN0);
    }
    else
    {
      printf("Selecting STx7200 Aux output pipeline\n");
      planeid  = OUTPUT_GDP3;
      vsync_interrupt = interrupt_handle(OS21_INTERRUPT_VTG_AUX0);
    }
  }
  else
  {
    printf("Selecting STx7200 Remote output pipeline\n");
    planeid  = OUTPUT_GDP1;
    vsync_interrupt = interrupt_handle(OS21_INTERRUPT_VTG_SD0);
  }

#elif defined(CONFIG_STI5206)
  if(output == 0)
  {
    printf("Selecting STx5206 Main output pipeline\n");
    planeid  = OUTPUT_GDP2;
    vsync_interrupt = interrupt_handle(OS21_INTERRUPT_MAIN_VTG);
  }
  else
  {
    printf("Selecting STx5206 Aux output pipeline\n");
    planeid  = OUTPUT_GDP1;
    vsync_interrupt = interrupt_handle(OS21_INTERRUPT_AUX_VTG);
  }

#elif defined(CONFIG_STI7108)
  if(output == 0)
  {
    printf("Selecting STx7108 Main output pipeline\n");
    planeid  = OUTPUT_GDP4;
    vsync_interrupt = interrupt_handle(OS21_INTERRUPT_VTG_MAIN_VSYNC);
  }
  else
  {
    printf("Selecting STx7108 Aux output pipeline\n");
    planeid  = OUTPUT_GDP1;
    vsync_interrupt = interrupt_handle(OS21_INTERRUPT_VTG_AUX_VSYNC);
  }

#else
  vsync_interrupt = 0;
#endif

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
  create_test_pattern(fbuffer,FBWIDTH,FBHEIGHT,FBSTRIDE);
  cache_purge_data(fbuffer, FBSIZE);

  pOutput = stm_display_get_output(pDev, output);
  if(!pOutput)
  {
    printf("Unable to get output\n");
    return 1;
  }

  setup_analogue_voltages(pOutput);
  stm_display_output_set_control(pOutput, STM_CTRL_CVBS_FILTER, cvbstrapfilter);
  stm_display_output_set_control(pOutput, STM_CTRL_SIGNAL_RANGE, clip?STM_SIGNAL_VIDEO_RANGE:STM_SIGNAL_FILTER_SAV_EAV);

  stm_display_output_setup_clock_reference(pOutput, STM_CLOCK_REF_30MHZ, error_ppm);

  pDVO = get_dvo_output(pDev);

  if(output == 0)
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

  if(pHDMI && (STANDARD & STM_OUTPUT_STD_SD_MASK))
  {
    hdmistandard |= STM_OUTPUT_STD_TMDS_PIXELREP_2X;
    pHDMIModeLine = stm_display_output_find_display_mode(pOutput,
                                       pModeLine->ModeParams.ActiveAreaWidth*2,
                                       pModeLine->ModeParams.ActiveAreaHeight,
                                       pModeLine->TimingParams.LinesByFrame,
                                       pModeLine->TimingParams.PixelsPerLine*2,
                                       pModeLine->TimingParams.ulPixelClock*2,
                                       pModeLine->ModeParams.ScanType);

    if(!pHDMIModeLine)
    {
      printf("Unable to find HDMI pixel doubled mode for SD mode\n");
      return 1;
    }
  }
  else
  {
    pHDMIModeLine = pModeLine;
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

  ULONG format = 0;
  if(STANDARD & STM_OUTPUT_STD_SD_MASK)
  {
    format = (STM_VIDEO_OUT_YC | STM_VIDEO_OUT_CVBS);

#if !defined(CONFIG_STI7200) || defined(CONFIG_MB671)
    /*
     * Note: 7200Cut1 does not support simultaneous YUV/RGB + CVBS/Y/C, but Cut2
     *       (on MB671) does.
     */
    format |= rgb?STM_VIDEO_OUT_RGB:STM_VIDEO_OUT_YUV;
#endif
  }
  else
  {
    format = rgb?STM_VIDEO_OUT_RGB:STM_VIDEO_OUT_YUV;
  }

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
      printf("%d frames took %ld ticks. topfields = %ld botfields = %ld\n",framerate, (long)delta, topfields, botfields);

      lasttime = now;
      seconds--;
    }

    err = 0;
    printf("Errors: duplicate topfields = %ld botfields = %ld\n",duptopfields, dupbotfields);
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
