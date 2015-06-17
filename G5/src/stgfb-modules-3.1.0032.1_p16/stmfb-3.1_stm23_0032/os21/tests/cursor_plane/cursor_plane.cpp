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

static semaphore_t *frameupdate_sem = 0;
static semaphore_t *hotplug_sem     = 0;
static semaphore_t *cursor_sem      = 0;

static stm_display_status_t currentDisplayStatus = STM_DISPLAY_DISCONNECTED;

static stm_display_device_t   *pDev      = 0;
static stm_display_output_t   *pOutput   = 0;
static stm_display_output_t   *pHDMI     = 0;
static const stm_mode_line_t  *pModeLine = 0;

stm_display_mode_t  MODE     = STVTG_TIMING_MODE_1080I60000_74250;
unsigned long       STANDARD = STM_OUTPUT_STD_SMPTE274M;

static int vsync_isr(void *data)
{
  stm_field_t current_field;
  TIME64      dummy;

  stm_display_output_handle_interrupts(pOutput);

  stm_display_output_get_last_vsync_info(pOutput, &current_field, &dummy);

  if(current_field == STM_TOP_FIELD)
    semaphore_signal(cursor_sem);

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
        printf("HDMI TV Starting Link\n");
        if(stm_display_output_start(pHDMI, pModeLine, STM_OUTPUT_STD_CEA861C)<0)
        {
          printf("Unable to start HDMI\n");
        }
        break;
      case STM_DISPLAY_CONNECTED:
        printf("HDMI TV Successfully Connected\n");
        break;
      case STM_DISPLAY_DISCONNECTED:
        printf("HDMI TV Disconnected\n");
        stm_display_output_stop(pHDMI);
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
stm_display_plane_t    *pPlane;
stm_display_buffer_t    buffer_setup;
interrupt_t            *vsync_interrupt=0;
interrupt_t            *hdmi_interrupt=0;
char                   *fbuffer;
void                   *fbufferphys;
unsigned short         *clut;
void                   *clutphys;
int                     err;
int                     x,y;

  kernel_initialize(NULL);
  kernel_start();

  frameupdate_sem    = semaphore_create_fifo(0);
  hotplug_sem        = semaphore_create_fifo(0);
  cursor_sem         = semaphore_create_fifo(0);

  task_create(displayupdate_task_fn, 0, OS21_DEF_MIN_STACK_SIZE, MAX_USER_PRIORITY, "displayupdate", 0);

  argc--;
  argv++;

  while(argc>0)
  {
    switch(**argv)
    {
      case 'i':
      {
        printf("Setting 1920x1080-60i\n");
        MODE      = STVTG_TIMING_MODE_1080I60000_74250;
        STANDARD  = STM_OUTPUT_STD_SMPTE274M;
        break;
      }
      case 'p':
      {
        printf("Setting 720x480-59.94p\n");
        MODE      = STVTG_TIMING_MODE_480P59940_27000;
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

  pDev = stm_display_get_device(0);

  if(!pDev)
  {
    printf("Unable to create device instance\n");
    return 1;
  }

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

  pOutput = stm_display_get_output(pDev, 0);
  if(!pOutput)
  {
    printf("Unable to get output\n");
    return 1;
  }


  setup_analogue_voltages(pOutput);

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
        fprintf(stderr,"Unable to install and enable hdmi interrupts\n");
        exit(1);
      }
    }
    else
    {
      fprintf(stderr,"No HDMI output available\n");
    }
  }

  pModeLine = stm_display_output_get_display_mode(pOutput, MODE);
  if(!pModeLine)
  {
    fprintf(stderr,"Unable to use requested display mode\n");
    exit(1);
  }

  pPlane = stm_display_get_plane(pDev, OUTPUT_CUR);
  if(!pPlane)
  {
    fprintf(stderr,"No Cursor plane available on this platform\n");
    exit(1);
  }
  stm_display_plane_connect_to_output(pPlane, pOutput);
  stm_display_plane_lock(pPlane);

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

  buffer_setup.src.ulVideoBufferAddr = (ULONG)fbufferphys;
  buffer_setup.src.ulVideoBufferSize = 128*128;
  buffer_setup.src.ulClutBusAddress = (ULONG)clutphys;

  buffer_setup.src.ulStride     = 128;
  buffer_setup.src.ulPixelDepth = 1;
  buffer_setup.src.ulColorFmt   = SURF_CLUT8_ARGB4444_ENTRIES;
  buffer_setup.src.Rect.width   = 128;
  buffer_setup.src.Rect.height  = 128;

  buffer_setup.dst.Rect.width   = 128;
  buffer_setup.dst.Rect.height  = 128;
  buffer_setup.dst.ulFlags      = STM_PLANE_DST_USE_SCREEN_XY;

  buffer_setup.info.ulFlags = STM_PLANE_PRESENTATION_PERSISTENT;

  stm_display_output_set_control(pOutput, STM_CTRL_VIDEO_OUT_SELECT, STM_VIDEO_OUT_YUV);
  stm_display_output_set_control(pOutput, STM_CTRL_BACKGROUND_ARGB, 0x0);

  if(pHDMI)
  {
    stm_display_output_set_control(pHDMI, STM_CTRL_VIDEO_OUT_SELECT, (STM_VIDEO_OUT_RGB|STM_VIDEO_OUT_HDMI));
  }

  if(stm_display_output_start(pOutput, pModeLine, STANDARD)<0)
  {
    printf("Unable to start display\n");
    return 1;
  }

  stm_display_plane_queue_buffer(pPlane, &buffer_setup);

  x = -20;
  y = -20;
  stm_display_plane_set_control(pPlane, PLANE_CTRL_SCREEN_XY, ((ULONG)x&0xffff) | ((ULONG)y<<16));

  {
    unsigned long val;
    stm_display_plane_get_control(pPlane, PLANE_CTRL_SCREEN_XY, &val);
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
      stm_display_plane_set_control(pPlane, PLANE_CTRL_SCREEN_XY, ((ULONG)x|((ULONG)y<<16)));
    }
  }
  return 0;
}
