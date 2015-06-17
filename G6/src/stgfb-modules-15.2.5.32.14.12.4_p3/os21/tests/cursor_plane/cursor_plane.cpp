/***********************************************************************
 *
 * File: os21/tests/cursor_plane/cursor_plane.cpp
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *
\***********************************************************************/

#include <os21/stglib/application_helpers.h>

#include <vibe_os.h>
#include <vibe_debug.h>

static semaphore_t *hotplug_sem     = 0;
static semaphore_t *cursor_sem      = 0;

static stm_display_output_connection_status_t currentDisplayStatus = STM_DISPLAY_DISCONNECTED;

static stm_display_output_h hHDMI     = 0;
static stm_display_mode_t  ModeLine = { STM_TIMING_MODE_RESERVED };
static stm_display_mode_t  HDMIModeLine = { STM_TIMING_MODE_RESERVED };

stm_display_mode_id_t  MODE     = STM_TIMING_MODE_1080I60000_74250;
unsigned long          STANDARD = STM_OUTPUT_STD_SMPTE274M;

static interrupt_t *vsync_interrupt=0;

static int vsync_isr(void *data)
{
  struct display_update_s *d = (struct display_update_s *)data;
  uint32_t  timingevent;
  stm_time64_t dummy;

  /* Clear for edge triggered ILC interrupts, this is a NOP on other controllers */
  interrupt_clear(vsync_interrupt);

  stm_display_output_handle_interrupts(d->output);

  stm_display_output_get_last_timing_event(d->output, &timingevent, &dummy);

  if(timingevent == STM_TIMING_EVENT_LINE || timingevent == STM_TIMING_EVENT_NONE)
    return OS21_SUCCESS;

  if(timingevent & STM_TIMING_EVENT_FRAME)
    semaphore_signal(cursor_sem);

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
        printf("HDMI TV Starting Link\n");
        if(stm_display_output_start(hHDMI, &HDMIModeLine)<0)
        {
          printf("Unable to start HDMI\n");
        }
        break;
      case STM_DISPLAY_CONNECTED:
        printf("HDMI TV Successfully Connected\n");
        break;
      case STM_DISPLAY_DISCONNECTED:
        printf("HDMI TV Disconnected\n");
        stm_display_output_stop(hHDMI);
        break;
    }
  }
}


void create_cursor_pattern(char *pBuf)
{
unsigned char *pixel;

  for(int y=0;y<128;y++)
  {
    for(int x=0;x<128;x++)
    {
      pixel = (unsigned char *)(pBuf+y*128+x);

      if(x<4 || x>124 || y == 0 || y == 127)
        *pixel = 0;
      else
        *pixel = 4;//y%8;
    }
  }

}


void usage(void)
{
    fprintf(stderr,"Usage: cursor_plane [i|p]\n");
    fprintf(stderr,"\ti        - Select 1080i@60Hz output (default)\n");
    fprintf(stderr,"\tp        - Select 480p@59.94Hz output\n");
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
unsigned short         *clut;
void                   *clutphys;
int                     err;
int                     x,y;
stm_display_source_interface_params_t iface_params;
stm_rect_t              input_window, output_window;

  kernel_initialize(NULL);
  kernel_start();

  hotplug_sem        = semaphore_create_fifo(0);
  cursor_sem         = semaphore_create_fifo(0);

  argc--;
  argv++;

  while(argc>0)
  {
    switch(**argv)
    {
      case 'i':
      {
        printf("Setting 1920x1080-60i\n");
        MODE      = STM_TIMING_MODE_1080I60000_74250;
        STANDARD  = STM_OUTPUT_STD_SMPTE274M;
        break;
      }
      case 'p':
      {
        printf("Setting 720x480-59.94p\n");
        MODE      = STM_TIMING_MODE_480P59940_27000;
        STANDARD  = STM_OUTPUT_STD_SMPTE293M;
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

  /* OS abstraction is now moved out of stm_display_open_device() */
  if(vibe_os_init()<0)
    return -1;

  if(vibe_debug_init()<0)
    return -1;

  if(stm_display_open_device(0,&hDev) != 0)
  {
    printf("Unable to create device instance\n");
    return 1;
  }

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

  setup_analogue_voltages(hOutput);

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
        fprintf(stderr,"Unable to install and enable hdmi interrupts\n");
        exit(1);
      }
    }
    else
    {
      fprintf(stderr,"No HDMI output available\n");
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

  hPlane = get_and_connect_cursor_plane_to_output(hDev, hOutput);
  if(!hPlane)
  {
    fprintf(stderr,"No Cursor plane available on this platform\n");
    exit(1);
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
  if(stm_display_source_get_interface(hSource, iface_params, (void **)&queue_interface) != 0)
  {
    printf("No interface registred\n");
    return 1;
  }

  if(stm_display_source_queue_lock(queue_interface, false)<0)
  {
    printf("Cannot lock queue\n");
    return 1;
  }

  fbuffer = (char *)malloc((128*128)+16);

  if(!fbuffer)
  {
    printf("Unable to allocate framebuffer\n");
    return 1;
  }

  create_cursor_pattern((char*)(((unsigned long)fbuffer+16) & ~15));
  cache_purge_data(fbuffer, (128*128)+16);
  vmem_virt_to_phys((char*)(((unsigned long)fbuffer+16) & ~15), &fbufferphys);

  clut = (unsigned short *)malloc(256*sizeof(unsigned short));
  clut[0] = 0xffff;
  clut[1] = 0xf000;
  clut[2] = 0xff00;
  clut[3] = 0xf0f0;
  clut[4] = 0xf00f;
  clut[5] = 0xfff0;
  clut[6] = 0xf0ff;
  clut[7] = 0xff0f;
  cache_purge_data(clut, 256*sizeof(unsigned short));
  vmem_virt_to_phys((char*)clut, &clutphys);

  memset(&buffer_setup, 0, sizeof(buffer_setup));

  buffer_setup.src.primary_picture.video_buffer_addr = (uint32_t)fbufferphys;
  buffer_setup.src.primary_picture.video_buffer_size = 128*128;
  buffer_setup.src.clut_bus_address = (uint32_t)clutphys;

  buffer_setup.src.primary_picture.width     = 128;
  buffer_setup.src.primary_picture.height    = 128;
  buffer_setup.src.primary_picture.pitch     = 128;
  buffer_setup.src.primary_picture.pixel_depth = 1;
  buffer_setup.src.primary_picture.color_fmt   = SURF_CLUT8_ARGB4444_ENTRIES;

  buffer_setup.src.visible_area.x = 0;
  buffer_setup.src.visible_area.y = 0;
  buffer_setup.src.visible_area.width = 128;
  buffer_setup.src.visible_area.height = 128;

  buffer_setup.dst.ulFlags  = 0;

  buffer_setup.info.ulFlags = STM_BUFFER_PRESENTATION_PERSISTENT;

  stm_display_output_set_control(hOutput, OUTPUT_CTRL_VIDEO_OUT_SELECT, STM_VIDEO_OUT_YUV);
  stm_display_output_set_control(hOutput, OUTPUT_CTRL_BACKGROUND_ARGB, 0x0);

  if(hHDMI)
  {
    stm_display_output_set_control(hHDMI, OUTPUT_CTRL_VIDEO_OUT_SELECT, (STM_VIDEO_OUT_RGB|STM_VIDEO_OUT_HDMI|STM_VIDEO_OUT_24BIT));
  }

  ModeLine.mode_params.output_standards = STANDARD;
  ModeLine.mode_params.flags = STM_MODE_FLAGS_NONE;

  if(stm_display_output_start(hOutput, &ModeLine)<0)
  {
    printf("Unable to start display\n");
    return 1;
  }

  input_window.x = 0;
  input_window.y = 0;
  input_window.width   = 128;
  input_window.height  = 128;
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
  output_window.width   = 128;
  output_window.height  = 128;
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

  if(stm_display_source_queue_buffer(queue_interface,&buffer_setup)<0)
  {
    printf("Unable to queue framebuffer for display on graphics source\n");
    return 1;
  }

  x = -20;
  y = -20;
  stm_display_plane_set_control(hPlane, PLANE_CTRL_SCREEN_XY, ((uint32_t)x&0xffff) | ((uint32_t)y<<16));

  {
    unsigned long val;
    stm_display_plane_get_control(hPlane, PLANE_CTRL_SCREEN_XY, &val);
    printf("XY CTRL val = 0x%08lx\n",val);
  }

  task_create(hotplug_task_fn, 0, OS21_DEF_MIN_STACK_SIZE, MAX_USER_PRIORITY, "hotplug", 0);

  {
    int i=0;
    while(1)
    {
      semaphore_wait(cursor_sem);
      i = (i+1)%100;

      x = 100+i;
      y = 100+i;
      stm_display_plane_set_control(hPlane, PLANE_CTRL_SCREEN_XY, ((uint32_t)x|((uint32_t)y<<16)));
    }
  }
  return 0;
}
