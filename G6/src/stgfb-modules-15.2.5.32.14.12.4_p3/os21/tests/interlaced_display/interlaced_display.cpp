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

#include <vibe_os.h>
#include <vibe_debug.h>

// use consts so they are visible in the debugger
static const int FBWIDTH       = 720;
static const int FBHEIGHT      = 480;
static const int FBDEPTH       = 32;
static const stm_pixel_format_t FBPIXFMT = SURF_ARGB8888;
static const int FBSTRIDE      = (FBWIDTH*(FBDEPTH>>3));
static const int FBSIZE        = (FBSTRIDE*FBHEIGHT);

static unsigned long topfields      = 0;
static unsigned long botfields      = 0;
static unsigned long duptopfields   = 0;
static unsigned long dupbotfields   = 0;
static semaphore_t *framerate_sem   = 0;
static semaphore_t *hotplug_sem     = 0;
static int framerate                = 50;
static int silent_hotplug           = 0;

static stm_display_timing_events_t lastfield = STM_TIMING_EVENT_NONE;
static stm_display_output_connection_status_t currentDisplayStatus = STM_DISPLAY_DISCONNECTED;

static stm_display_output_h hHDMI      = 0;
static stm_display_output_h hDVO       = 0;
static stm_display_mode_t ModeLine     = { STM_TIMING_MODE_RESERVED };
static stm_display_mode_t HDMIModeLine = { STM_TIMING_MODE_RESERVED };
static uint32_t           hdmistandard = STM_OUTPUT_STD_CEA861;
static uint32_t           hdmiflags    = STM_MODE_FLAGS_NONE;

stm_display_mode_id_t  MODE     = STM_TIMING_MODE_576I50000_13500;
unsigned long          STANDARD = STM_OUTPUT_STD_PAL_BDGHI;

static interrupt_t *vsync_interrupt=0;

static int vsync_isr(void *data)
{
  struct display_update_s *d = (struct display_update_s *)data;
  uint32_t     timingevent;
  stm_time64_t dummy;

  /* Clear for edge triggered ILC interrupts, this is a NOP on other controllers */
  interrupt_clear(vsync_interrupt);

  stm_display_output_handle_interrupts(d->output);

  // Some housekeeping to ensure the VTG is in the expected mode.
  stm_display_output_get_last_timing_event(d->output, &timingevent, &dummy);

  if(timingevent == STM_TIMING_EVENT_LINE || timingevent == STM_TIMING_EVENT_NONE)
    return OS21_SUCCESS;

  if(timingevent & STM_TIMING_EVENT_TOP_FIELD)
  {
    if(lastfield == STM_TIMING_EVENT_TOP_FIELD)
      duptopfields++;

    lastfield = STM_TIMING_EVENT_TOP_FIELD;

    topfields++;
  }
  else
  {
    if(lastfield == STM_TIMING_EVENT_BOTTOM_FIELD)
      dupbotfields++;

    botfields++;

    lastfield = STM_TIMING_EVENT_BOTTOM_FIELD;
  }

  if(((topfields+botfields) % framerate) == 0)
  {
    semaphore_signal(framerate_sem);
  }

  semaphore_signal(d->frameupdate_sem);

  if(hHDMI)
  {
    stm_display_output_connection_status_t oldDisplayStatus = currentDisplayStatus;

    stm_display_output_get_connection_status(hHDMI, &currentDisplayStatus);

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
          stm_display_output_set_connection_status(hHDMI, currentDisplayStatus);
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
          stm_display_output_set_connection_status(hHDMI, currentDisplayStatus);
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


void hotplug_task_fn(void *val)
{
  while(1)
  {
    semaphore_wait(hotplug_sem);
    switch(currentDisplayStatus)
    {
      case STM_DISPLAY_NEEDS_RESTART:
        if(!silent_hotplug) printf("HDMI TV Starting Link\n");
        if(stm_display_output_start(hHDMI, &HDMIModeLine)<0)
        {
          if(!silent_hotplug) printf("Unable to start HDMI\n");
        }
        break;
      case STM_DISPLAY_CONNECTED:
        if(!silent_hotplug) printf("HDMI TV Successfully Connected\n");
        break;
      case STM_DISPLAY_DISCONNECTED:
        if(!silent_hotplug) printf("HDMI TV Disconnected\n");
        stm_display_output_stop(hHDMI);
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
    fprintf(stderr,"\tn [59|443]\n");
    fprintf(stderr,"\t       - Select 720x480 @ 59.94 NTSC-M or NTSC-443\n");
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
struct display_update_s *pDispUpdate = 0;
stm_display_device_h    hDev      = 0;
stm_display_output_h    hOutput   = 0;
stm_display_plane_h     hPlane    = 0;
stm_display_source_h    hSource   = 0;
stm_display_source_queue_h queue_interface=0;
stm_display_buffer_t    buffer_setup;
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
osclock_t               lasttime;
stm_display_source_interface_params_t iface_params;
stm_rect_t              input_window, output_window;

  kernel_initialize(NULL);
  kernel_start();
  kernel_timeslice(OS21_TRUE);

  framerate_sem      = semaphore_create_fifo(0);
  hotplug_sem        = semaphore_create_fifo(0);

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
        MODE      = STM_TIMING_MODE_576I50000_13500;
        STANDARD  = STM_OUTPUT_STD_PAL_N;
        framerate = 50;
        break;
      }
      case 'c':
      {
        printf("Setting 720x576-50i PAL-Nc\n");
        MODE      = STM_TIMING_MODE_576I50000_13500;
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
            MODE      = STM_TIMING_MODE_1080I50000_74250_274M;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 50;
            break;
          case 1250:
            printf("Setting 1920x1080-50i Australian 1250lines\n");
            MODE      = STM_TIMING_MODE_1080I50000_72000;
            STANDARD  = STM_OUTPUT_STD_AS4933;
            framerate = 50;
            break;
          case 59:
            printf("Setting 1920x1080-59i\n");
            MODE      = STM_TIMING_MODE_1080I59940_74176;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 60;
            break;
          case 60:
            printf("Setting 1920x1080-60i\n");
            MODE      = STM_TIMING_MODE_1080I60000_74250;
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
        MODE      = STM_TIMING_MODE_480I59940_13500;
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
            MODE      = STM_TIMING_MODE_480I59940_13500;
            STANDARD  = STM_OUTPUT_STD_NTSC_M;
            framerate = 60;
            break;
          case 443:
            printf("Setting 720x480-59i NTSC-443\n");
            MODE      = STM_TIMING_MODE_480I59940_13500;
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
        MODE      = STM_TIMING_MODE_480I59940_13500;
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
        MODE      = STM_TIMING_MODE_576I50000_13500;
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
        MODE      = STM_TIMING_MODE_576I50000_13500;
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
        MODE      = STM_TIMING_MODE_480I59940_13500;
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

  /* OS abstraction is now moved out of stm_display_open_device() */
  if(vibe_os_init()<0)
    return -1;

  if(vibe_debug_init()<0)
    return -1;

  if(stm_display_open_device(device,&hDev)!=0)
  {
    fprintf(stderr,"Display device not found\n");
    exit(1);
  }

  vsync_interrupt = 0;

  setup_soc();

  if(!vsync_interrupt)
  {
    printf("Cannot find VSYNC interrupt handler\n");
    return 1;
  }

  hOutput = get_analog_output(hDev, (output == 0));
  if(!hOutput)
  {
    printf("Unable to get output\n");
    return 1;
  }

  if((pDispUpdate = start_display_update_thread(hDev, hOutput)) == 0)
  {
    printf("Unable to start display update thread\n");
    return 1;
  }

  err  = interrupt_install(vsync_interrupt, vsync_isr, pDispUpdate);
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

  setup_analogue_voltages(hOutput);
  stm_display_output_set_control(hOutput, OUTPUT_CTRL_CVBS_TRAP_FILTER, cvbstrapfilter);
  stm_display_output_set_control(hOutput, OUTPUT_CTRL_CLIP_SIGNAL_RANGE, clip?STM_SIGNAL_VIDEO_RANGE:STM_SIGNAL_FILTER_SAV_EAV);

  stm_display_output_set_clock_reference(hOutput, STM_CLOCK_REF_30MHZ, error_ppm);

  hDVO = get_dvo_output(hDev);

  if(output == 0)
  {
    hHDMI = get_hdmi_output(hDev);

    if(hHDMI)
    {
      /*
       * Now we have a HDMI output pointer to handle hotplug interrupts,
       * we can enable the interrupt handlers.
       */
      hdmi_interrupt = get_hdmi_interrupt();
      if(hdmi_interrupt)
      {
        err  = interrupt_install(hdmi_interrupt, hdmi_isr, hHDMI);
        err += interrupt_enable(hdmi_interrupt);
      }

      if(err>0)
      {
        printf("Unable to install and enable hdmi interrupts\n");
        return 1;
      }

      stm_display_output_set_control(hHDMI, OUTPUT_CTRL_CLIP_SIGNAL_RANGE, clip?STM_SIGNAL_VIDEO_RANGE:STM_SIGNAL_FILTER_SAV_EAV);

    }
    else
    {
      printf("Hmmm, no HDMI output available\n");
    }
  }

  if(stm_display_output_get_display_mode(hOutput, MODE, &ModeLine)<0)
  {
    printf("Unable to use requested display mode\n");
    return 1;
  }

  if(hHDMI && (STANDARD & STM_OUTPUT_STD_SD_MASK))
  {
    hdmiflags |= STM_MODE_FLAGS_HDMI_PIXELREP_2X;
    if(stm_display_output_find_display_mode(hOutput,
                                       ModeLine.mode_params.active_area_width*2,
                                       ModeLine.mode_params.active_area_height,
                                       ModeLine.mode_timing.lines_per_frame,
                                       ModeLine.mode_timing.pixels_per_line*2,
                                       ModeLine.mode_timing.pixel_clock_freq*2,
                                       ModeLine.mode_params.scan_type,
                                       &HDMIModeLine)<0)
    {
      printf("Unable to find HDMI pixel doubled mode for SD mode\n");
      return 1;
    }
  }
  else
  {
    HDMIModeLine = ModeLine;
  }

  HDMIModeLine.mode_params.output_standards = hdmistandard;
  HDMIModeLine.mode_params.flags = STM_MODE_FLAGS_NONE;

  hPlane = get_and_connect_gfx_plane_to_output(hDev, hOutput);
  if(!hPlane)
  {
    printf("Unable to get graphics plane\n");
    return 1;
  }

  /*
   * Get a Source and try to connect it on created plane
   */
  hSource = get_and_connect_source_to_plane(hDev ,hPlane);
  if (!hSource)
  {
    printf("Unable to get a source for plane %p\n", hPlane);
    return 1;
  }

  /*
   * Lock the QueueBuffer Interface for our exclusive queue_buffer usage
   */
  iface_params.interface_type = STM_SOURCE_QUEUE_IFACE;
  if(stm_display_source_get_interface(hSource, iface_params, (void**)&queue_interface) != 0)
  {
    printf("No interface registred\n");
    return 1;
  }

  if(stm_display_source_queue_lock(queue_interface, false)<0)
  {
    printf("Cannot lock queue\n");
    return 1;
  }

  memset(&buffer_setup, 0, sizeof(buffer_setup));

  vmem_virt_to_phys(fbuffer, &fbufferphys);
  buffer_setup.src.primary_picture.video_buffer_addr = (uint32_t)fbufferphys;
  buffer_setup.src.primary_picture.video_buffer_size = FBSIZE;

  buffer_setup.src.primary_picture.width     = FBWIDTH;
  buffer_setup.src.primary_picture.height    = FBHEIGHT;
  buffer_setup.src.primary_picture.pitch     = FBSTRIDE;
  buffer_setup.src.primary_picture.pixel_depth = FBDEPTH;
  buffer_setup.src.primary_picture.color_fmt   = FBPIXFMT;

  buffer_setup.src.visible_area.x = 0;
  buffer_setup.src.visible_area.y = 0;
  buffer_setup.src.visible_area.width = FBWIDTH;
  buffer_setup.src.visible_area.height = FBHEIGHT;

  buffer_setup.info.ulFlags = STM_BUFFER_PRESENTATION_PERSISTENT;

  printf("Clock is running at %ld ticks per second\n",(long)time_ticks_per_sec());

  uint32_t format = 0;
  if(STANDARD & STM_OUTPUT_STD_SD_MASK)
  {
    format = (STM_VIDEO_OUT_YC | STM_VIDEO_OUT_CVBS);
    format |= rgb?STM_VIDEO_OUT_RGB:STM_VIDEO_OUT_YUV;
  }
  else
  {
    format = rgb?STM_VIDEO_OUT_RGB:STM_VIDEO_OUT_YUV;
  }

  stm_display_output_set_control(hOutput, OUTPUT_CTRL_VIDEO_OUT_SELECT, format);

  if(hHDMI)
  {
    uint32_t format = 0;

    format |= rgb?STM_VIDEO_OUT_RGB:STM_VIDEO_OUT_YUV;
    format |= dvi?STM_VIDEO_OUT_DVI:STM_VIDEO_OUT_HDMI;
    format |= STM_VIDEO_OUT_24BIT;
    stm_display_output_set_control(hHDMI, OUTPUT_CTRL_VIDEO_OUT_SELECT, format);
  }

  ModeLine.mode_params.output_standards = STANDARD;
  ModeLine.mode_params.flags = STM_MODE_FLAGS_NONE;

  if(stm_display_output_start(hOutput, &ModeLine)<0)
  {
    printf("Unable to start display\n");
    return 1;
  }

  if(hDVO)
  {
    printf("Info: Attempting to start DVO\n");
    if(stm_display_output_start(hDVO, &ModeLine)<0)
    {
      printf("Info: Unable to start DVO\n");
    }
  }

  input_window.x = 0;
  input_window.y = 0;
  input_window.width   = FBWIDTH;
  input_window.height  = FBHEIGHT;
  if(stm_display_plane_set_compound_control(hPlane, PLANE_CTRL_INPUT_WINDOW_VALUE, &input_window)<0)
  {
    printf("Unable to set Input window rect\n");
    return 1;
  }
  if(stm_display_plane_set_control(hPlane, PLANE_CTRL_INPUT_WINDOW_MODE, MANUAL_MODE)<0)
  {
    printf("Unable to set Input window mode\n");
    return 1;
  }

  output_window.x = 0;
  output_window.y = 0;
  output_window.width   = FBWIDTH;
  output_window.height  = FBHEIGHT;
  if(stm_display_plane_set_compound_control(hPlane, PLANE_CTRL_OUTPUT_WINDOW_VALUE, &output_window)<0)
  {
    printf("Unable to set Output window rect\n");
    return 1;
  }
  if(stm_display_plane_set_control(hPlane, PLANE_CTRL_OUTPUT_WINDOW_MODE, MANUAL_MODE)<0)
  {
    printf("Unable to set Output window mode\n");
    return 1;
  }

  lasttime = time_now(); // VTG Start time (approx)

  if(stm_display_source_queue_buffer(queue_interface,&buffer_setup)<0)
  {
    printf("Unable to queue framebuffer for display on graphics source\n");
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

  stm_display_plane_disconnect_from_output(hPlane, hOutput);
  stm_display_output_stop(hOutput);

  interrupt_disable(vsync_interrupt);
  interrupt_disable(hdmi_interrupt);

  if(hSource)
  {
    // Unlock this queue for our exclusive usage
    stm_display_source_queue_unlock(queue_interface);
    stm_display_source_queue_release(queue_interface);

    stm_display_plane_disconnect_from_source(hPlane, hSource);
    stm_display_source_close(hSource);
  }

  stm_display_plane_close(hPlane);

  stm_display_output_close(hDVO);
  stm_display_output_close(hHDMI);
  stm_display_output_close(hOutput);

  stm_display_device_close(hDev);

  /* vibe_os destruction is now moved out of stm_display_device_close() */
  vibe_debug_release();
  vibe_os_release();

  return err;
}
