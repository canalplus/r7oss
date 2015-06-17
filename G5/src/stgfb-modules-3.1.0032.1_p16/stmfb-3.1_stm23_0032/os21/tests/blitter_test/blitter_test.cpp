/***********************************************************************
 *
 * File: os21/tests/blitter_test/blitter_test.cpp
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * This is a simple bdisp blitter test, that also lets us look at STBus plug
 * settings on the 7200.
 *
\***********************************************************************/

#include <os21/stglib/application_helpers.h>

// use consts so they are visible in the debugger
static const int FBWIDTH       = 1280;
static const int FBHEIGHT      = 720;
static const int FBDEPTH       = 32;
static const SURF_FMT FBPIXFMT = SURF_ARGB8888;
static const int FBSTRIDE      = (FBWIDTH*(FBDEPTH>>3));
static const int FBSIZE        = (FBSTRIDE*FBHEIGHT);

static semaphore_t *frameupdate_sem = 0;
static semaphore_t *hotplug_sem     = 0;
static stm_display_status_t currentDisplayStatus = STM_DISPLAY_DISCONNECTED;

static stm_display_device_t   *pDev      = 0;
static stm_display_output_t   *pOutput   = 0;
static stm_display_output_t   *pHDMI     = 0;
static struct stm_display_blitter_s  *pBlitter  = 0;
static const stm_mode_line_t  *pModeLine = 0;

static stm_display_mode_t MODE      = STVTG_TIMING_MODE_720P60000_74250;
static unsigned long      STANDARD  = STM_OUTPUT_STD_SMPTE296M;


static int vsync_isr(void *data)
{

  stm_display_output_handle_interrupts(pOutput);

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


static int blitter_isr(void *data)
{
  stm_display_blitter_handle_interrupt(pBlitter);

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


void usage(void)
{
  fprintf(stderr,"Usage: blitter_test \n");
  exit(1);
}

static unsigned long colours[] = {
    0xff000000,
    0xffff0000,
    0xff00ff00,
    0xff0000ff,
    0xffff00ff,
    0xffffff00,
    0xff00ffff,
    0xffffffff
};

int main(int argc, char **argv)
{
stm_display_plane_t    *pPlane;
stm_display_buffer_t    buffer_setup;
char                   *fbuffer;
void                   *fbufferphys;
interrupt_t            *vsync_interrupt=0;
interrupt_t            *hdmi_interrupt=0;
interrupt_t            *blitter_interrupt=0;
int                     err;
stm_plane_id_t          planeid;

  kernel_initialize(NULL);
  kernel_start();

  frameupdate_sem    = semaphore_create_fifo(0);
  hotplug_sem        = semaphore_create_fifo(0);

  task_create(displayupdate_task_fn, 0, OS21_DEF_MIN_STACK_SIZE, MAX_USER_PRIORITY, "displayupdate", 0);

  pDev = stm_display_get_device(0);
  if(!pDev)
  {
    printf("Unable to create device instance\n");
    return 1;
  }

  planeid  = OUTPUT_GDP1;

  vsync_interrupt = get_main_vtg_interrupt();
  blitter_interrupt = get_bdisp_interrupt();

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

  err = interrupt_install(blitter_interrupt,blitter_isr,NULL);
  err += interrupt_enable(blitter_interrupt);
  if(err>0)
  {
    printf("Unable to install and enable blitter interrupt\n");
    return 1;
  }

  fbuffer = (char *)malloc(FBSIZE+4096);
  if(!fbuffer)
  {
    printf("Unable to allocate framebuffer\n");
    return 1;
  }

  memset(fbuffer, 0x00, FBSIZE+4096);
  cache_purge_data(fbuffer, FBSIZE+4096);

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
        printf("Unable to install and enable hdmi interrupts\n");
        return 1;
      }
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

  vmem_virt_to_phys((char*)(((ULONG)fbuffer+4095)&~4095), &fbufferphys);
  printf("Physical start addr of destination = 0x%08lx\n",(ULONG)fbufferphys);

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

  if(pHDMI)
  {
    stm_display_output_set_control(pHDMI, STM_CTRL_VIDEO_OUT_SELECT, (STM_VIDEO_OUT_HDMI | STM_VIDEO_OUT_YUV));
  }


  if(stm_display_output_start(pOutput, pModeLine, STANDARD)<0)
  {
    printf("Unable to start display\n");
    return 1;
  }

  if(stm_display_plane_queue_buffer(pPlane, &buffer_setup)<0)
  {
    printf("Unable to queue framebuffer for display on graphics plane\n");
    return 1;
  }

  task_create(hotplug_task_fn, 0, OS21_DEF_MIN_STACK_SIZE, MIN_USER_PRIORITY, "hotplug", 0);

  /*
   * Let hotplug do its stuff before running the test.
   */

  task_delay(time_ticks_per_sec()*3);

  {
    osclock_t time1,time2;
    stm_blitter_operation_t op;
    stm_rect_t r;
    int i;

    pBlitter = stm_display_get_blitter(pDev,0);
    if(!pBlitter)
    {
      printf("Unable to get blitter instance\n");
      return 1;
    }

    memset(&op,0,sizeof(op));
    op.dstSurface.ulMemory = (ULONG)fbufferphys;
    op.dstSurface.ulSize = FBSIZE;
    op.dstSurface.ulWidth = FBWIDTH;
    op.dstSurface.ulHeight = FBHEIGHT;
    op.dstSurface.ulStride = FBSTRIDE;
    op.dstSurface.format = SURF_ARGB8888;
    op.colourFormat = SURF_ARGB8888;

    r.top    = 100;
    r.bottom = 611;
    r.left   = 100;
    r.right  = 1123;

    time1 = time_now();

    for(i=0;i<3000;i++)
    {
      op.ulColour = colours[i%8];
      stm_display_blitter_fill_rect(pBlitter,&op,&r);
    }

    stm_display_blitter_sync(pBlitter);

    time2 = time_now();

    time1 = time_minus(time2,time1);
    printf("Blitter operations took %lld ticks\n",time1);
  }

  stm_display_plane_flush(pPlane);
  stm_display_plane_disconnect_from_output(pPlane, pOutput);
  stm_display_output_stop(pOutput);

  interrupt_disable(vsync_interrupt);
  interrupt_disable(hdmi_interrupt);

  stm_display_plane_release(pPlane);

  if(pHDMI)
    stm_display_output_release(pHDMI);

  stm_display_output_release(pOutput);

  stm_display_release_device(pDev);

  return 0;
}
