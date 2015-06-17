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

#include <vibe_os.h>
#include <vibe_debug.h>

// use consts so they are visible in the debugger
static const int FBWIDTH       = 720;
static const int FBHEIGHT      = 480;
static const int FBDEPTH       = 32;
static const stm_pixel_format_t FBPIXFMT = SURF_ARGB8888;
static const int FBSTRIDE      = (FBWIDTH*(FBDEPTH>>3));
static const int FBSIZE        = (FBSTRIDE*FBHEIGHT);

static unsigned long num_frames     = 0;
static semaphore_t *framerate_sem   = 0;
static semaphore_t *hotplug_sem     = 0;
static int framerate                = 50;
static stm_display_output_connection_status_t currentDisplayStatus = STM_DISPLAY_DISCONNECTED;
static int silent_hotplug           = 0;

static stm_display_output_h hHDMI     = 0;
static stm_display_output_h hDVO      = 0;
static stm_display_mode_t  ModeLine = { STM_TIMING_MODE_RESERVED };
static stm_display_mode_t  HDMIModeLine = { STM_TIMING_MODE_RESERVED };

// Note these are not static and can be changed by command line arguments.
stm_display_mode_id_t   MODE     = STM_TIMING_MODE_576P50000_27000;
unsigned long           STANDARD = STM_OUTPUT_STD_SMPTE293M;

static interrupt_t *vsync_interrupt=0;

static int vsync_isr(void *data)
{
  struct display_update_s *d = (struct display_update_s *)data;
  uint32_t     timingevent;
  stm_time64_t dummy;

  /* Clear for edge triggered ILC interrupts, this is a NOP on other controllers */
  interrupt_clear(vsync_interrupt);

  stm_display_output_handle_interrupts(d->output);

  stm_display_output_get_last_timing_event(d->output, &timingevent, &dummy);

  if(timingevent == STM_TIMING_EVENT_LINE || timingevent == STM_TIMING_EVENT_NONE)
    return OS21_SUCCESS;

  num_frames++;

  if((num_frames % framerate) == 0)
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
struct display_update_s *pDispUpdate = 0;
stm_display_device_h    hDev      = 0;
stm_display_output_h    hOutput   = 0;
stm_display_plane_h     hPlane    = 0;
stm_display_source_h    hSource   = 0;
stm_display_source_queue_h queue_interface=0;
stm_display_buffer_t    buffer_setup;
char                   *fbuffer;
void                   *fbufferphys;
interrupt_t            *hdmi_interrupt=0;
int                     err;
int                     seconds;
int                     clip;
int                     rgb;
int                     dvi;
osclock_t               lasttime;
stm_display_source_interface_params_t iface_params;
stm_rect_t              input_window, output_window;

  kernel_initialize(NULL);
  kernel_start();
  kernel_timeslice(OS21_TRUE);

  framerate_sem      = semaphore_create_fifo(0);
  hotplug_sem        = semaphore_create_fifo(0);

  /* OS abstraction is now moved out of stm_display_open_device() */
  if(vibe_os_init()<0)
    return -1;

  if(vibe_debug_init()<0)
    return -1;

  if(stm_display_open_device(0,&hDev)!=0)
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
            MODE      = STM_TIMING_MODE_720P50000_74250;
            STANDARD  = STM_OUTPUT_STD_SMPTE296M;
            framerate = 50;
            break;
          case 59:
            printf("Setting 1280x720-59\n");
            MODE      = STM_TIMING_MODE_720P59940_74176;
            STANDARD  = STM_OUTPUT_STD_SMPTE296M;
            framerate = 60;
            break;
          case 60:
            printf("Setting 1280x720-60\n");
            MODE      = STM_TIMING_MODE_720P60000_74250;
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
            MODE      = STM_TIMING_MODE_1080P23976_74176;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 24;
            break;
          case 24:
            printf("Setting 1920x1080-24\n");
            MODE      = STM_TIMING_MODE_1080P24000_74250;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 24;
            break;
          case 25:
            printf("Setting 1920x1080-25\n");
            MODE      = STM_TIMING_MODE_1080P25000_74250;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 25;
            break;
          case 29:
            printf("Setting 1920x1080-29\n");
            MODE      = STM_TIMING_MODE_1080P29970_74176;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 30;
            break;
          case 30:
            printf("Setting 1920x1080-30\n");
            MODE      = STM_TIMING_MODE_1080P30000_74250;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 30;
            break;
          case 50:
            printf("Setting 1920x1080-50\n");
            MODE      = STM_TIMING_MODE_1080P50000_148500;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 50;
            break;
          case 59:
            printf("Setting 1920x1080-59\n");
            MODE      = STM_TIMING_MODE_1080P59940_148352;
            STANDARD  = STM_OUTPUT_STD_SMPTE274M;
            framerate = 60;
            break;
          case 60:
            printf("Setting 1920x1080-60\n");
            MODE      = STM_TIMING_MODE_1080P60000_148500;
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
            MODE      = STM_TIMING_MODE_576P50000_27000;
            STANDARD  = STM_OUTPUT_STD_SMPTE293M;
            framerate = 50;
            break;
          case 59:
            printf("Setting 720x480-59\n");
            MODE      = STM_TIMING_MODE_480P59940_27000;
            STANDARD  = STM_OUTPUT_STD_SMPTE293M;
            framerate = 60;
            break;
          case 60:
            printf("Setting 720x480-60\n");
            MODE      = STM_TIMING_MODE_480P60000_27027;
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
            MODE      = STM_TIMING_MODE_480P59940_25180;
            STANDARD  = STM_OUTPUT_STD_VGA;
            framerate = 60;
            break;
          case 60:
            printf("Setting 640x480-60\n");
            MODE      = STM_TIMING_MODE_480P60000_25200;
            STANDARD  = STM_OUTPUT_STD_VGA;
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

  setup_soc();

  vsync_interrupt = get_main_vtg_interrupt();

  if(!vsync_interrupt)
  {
    printf("Cannot find VSYNC interrupt handler\n");
    return 1;
  }

  hOutput = get_analog_output(hDev, TRUE);
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
  create_test_pattern(fbuffer, FBWIDTH, FBHEIGHT, FBSTRIDE);
  cache_purge_data(fbuffer, FBSIZE);

  setup_analogue_voltages(hOutput);
  stm_display_output_set_control(hOutput, OUTPUT_CTRL_CLIP_SIGNAL_RANGE, clip?STM_SIGNAL_VIDEO_RANGE:STM_SIGNAL_FILTER_SAV_EAV);

  hDVO = get_dvo_output(hDev);

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

  /*
   * This assumes all the modes this test app supports are defined by CEA861
   */
  HDMIModeLine = ModeLine;
  HDMIModeLine.mode_params.output_standards = STM_OUTPUT_STD_CEA861;
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
   * Lock the QueueBuffer Interface for our exclusif queue_buffer usage
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

  uint32_t format=0;

  format = rgb?STM_VIDEO_OUT_RGB:STM_VIDEO_OUT_YUV;

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
      printf("%d frames took %ld ticks.\n",framerate, (long)delta);

      lasttime = now;
      seconds--;
    }

    err = 0;
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
