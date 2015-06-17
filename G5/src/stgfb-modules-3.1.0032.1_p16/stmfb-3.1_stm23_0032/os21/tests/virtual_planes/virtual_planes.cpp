/***********************************************************************
 *
 * File: os21/tests/virtual_planes/virtual_planes.cpp
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *
\***********************************************************************/

#include <os21/stglib/application_helpers.h>

static const int FBWIDTH       = 720;
static const int FBHEIGHT      = 480;
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
static stm_display_output_t   *pBDispOut = 0;
static const stm_mode_line_t  *pModeLine = 0;

stm_display_mode_t  MODE     = STVTG_TIMING_MODE_1080I60000_74250;
unsigned long       STANDARD = STM_OUTPUT_STD_SMPTE274M;

static int vsync_isr(void *data)
{
  stm_field_t current_field;
  TIME64      dummy;

  stm_display_output_handle_interrupts(pOutput);

  // Some housekeeping to ensure the VTG is in the expected mode.
  stm_display_output_get_last_vsync_info(pOutput, &current_field, &dummy);

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


void usage(void)
{
    fprintf(stderr,"Usage: virtual_planes [i|p] [f (4|5|8)]\n");
    fprintf(stderr,"\tf (4|5|8)- Select ARGB4444,ARGB8565 or ARGB8888 (default) output\n");
    fprintf(stderr,"\ti        - Select 1080i@60Hz output (default)\n");
    fprintf(stderr,"\tp        - Select 720p@60Hz output\n");
    exit(1);
}


int main(int argc, char **argv)
{
stm_display_plane_t    *pPlane,*pPlane2;
stm_display_buffer_t    buffer_setup;
interrupt_t            *vsync_interrupt=0;
interrupt_t            *hdmi_interrupt=0;
char                   *fbuffer;
void                   *fbufferphys;
char                   *gdmabuffer[2];
void                   *gdmabufferphys[2];
unsigned long           gdmabuffersize;
int                     err;
SURF_FMT                outputformat = SURF_ARGB8888;

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
        printf("Setting 1280x720-60p\n");
        MODE      = STVTG_TIMING_MODE_720P60000_74250;
        STANDARD  = STM_OUTPUT_STD_SMPTE296M;
        break;
      }
      case 'f':
      {
        argc--;
        argv++;
        switch(**argv)
        {
          case '4':
            outputformat = SURF_ARGB4444;
            break;
          case '5':
            outputformat = SURF_ARGB8565;
            break;
          case '8':
            outputformat = SURF_ARGB8888;
            break;
          default:
            fprintf(stderr,"Unknown GDMA output format '%s'\n",*argv);
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
  create_test_pattern(fbuffer,FBWIDTH,FBHEIGHT,FBSTRIDE);
  cache_purge_data(fbuffer, FBSIZE);

  gdmabuffersize = 1920*1080*4;
  gdmabuffer[0] = (char *)malloc(gdmabuffersize+256);
  gdmabuffer[1] = (char *)malloc(gdmabuffersize+256);
  if(!gdmabuffer[0] || !gdmabuffer[1])
  {
    printf("Unable to allocate gdma buffers\n");
    return 1;
  }

  memset(gdmabuffer[0], 0xff, gdmabuffersize);
  cache_purge_data(gdmabuffer[0], gdmabuffersize);
  memset(gdmabuffer[1], 0xff, gdmabuffersize);
  cache_purge_data(gdmabuffer[1], gdmabuffersize);

  vmem_virt_to_phys((char *)(((unsigned long)gdmabuffer[0]+255) & ~255), &gdmabufferphys[0]);
  vmem_virt_to_phys((char *)(((unsigned long)gdmabuffer[1]+255) & ~255), &gdmabufferphys[1]);


  pOutput = stm_display_get_output(pDev, 0);
  if(!pOutput)
  {
    printf("Unable to get output\n");
    return 1;
  }


  setup_analogue_voltages(pOutput);

  {
    int i = 0;
    stm_display_output_t *out;
    while((out = stm_display_get_output(pDev,i++)) != 0)
    {
      ULONG caps;
      stm_display_output_get_capabilities(out, &caps);
      if((caps & STM_OUTPUT_CAPS_TMDS) != 0)
      {
        pHDMI = out;
      }
      else if((caps & STM_OUTPUT_CAPS_BLIT_COMPOSITION) != 0)
      {
        pBDispOut = out;
      }
    }

    if(!pBDispOut)
    {
      fprintf(stderr,"No BDisp composition output found huh?\n");
      exit(1);
    }

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

  pPlane = stm_display_get_plane(pDev, OUTPUT_VIRT1);
  if(!pPlane)
  {
    fprintf(stderr,"Unable to get virtual plane\n");
    exit(1);
  }
  stm_display_plane_connect_to_output(pPlane, pBDispOut);
  stm_display_plane_lock(pPlane);

  pPlane2 = stm_display_get_plane(pDev, OUTPUT_VIRT2);
  if(!pPlane2)
  {
    fprintf(stderr,"Unable to get virtual plane\n");
    exit(1);
  }
  stm_display_plane_connect_to_output(pPlane2, pBDispOut);
  stm_display_plane_lock(pPlane2);

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
  buffer_setup.dst.ulFlags      = STM_PLANE_DST_OVERWITE;

  buffer_setup.info.ulFlags = STM_PLANE_PRESENTATION_PERSISTENT;

  stm_display_output_set_control(pOutput, STM_CTRL_VIDEO_OUT_SELECT, STM_VIDEO_OUT_YUV);

  if(pHDMI)
  {
    stm_display_output_set_control(pHDMI, STM_CTRL_VIDEO_OUT_SELECT, (STM_VIDEO_OUT_RGB|STM_VIDEO_OUT_DVI));
  }

  stm_display_output_set_control(pBDispOut, STM_CTRL_GDMA_BUFFER_1, (ULONG)gdmabufferphys[0]);
  stm_display_output_set_control(pBDispOut, STM_CTRL_GDMA_BUFFER_2, (ULONG)gdmabufferphys[1]);
  stm_display_output_set_control(pBDispOut, STM_CTRL_GDMA_SIZE    , gdmabuffersize);
  stm_display_output_set_control(pBDispOut, STM_CTRL_GDMA_FORMAT  , outputformat);
  stm_display_output_set_control(pBDispOut, STM_CTRL_HW_PLANE_ID  , OUTPUT_GDP1);

  if(stm_display_output_start(pOutput, pModeLine, STANDARD)<0)
  {
    printf("Unable to start display\n");
    return 1;
  }

  stm_display_output_enable_background(pBDispOut);

  if(stm_display_output_start(pBDispOut, pModeLine, STANDARD)<0)
  {
    printf("Unable to start BDisp composition\n");
    return 1;
  }

  buffer_setup.dst.Rect.x = -100;
  buffer_setup.dst.Rect.y = -100;
  stm_display_plane_queue_buffer(pPlane, &buffer_setup);

  buffer_setup.dst.Rect.x = pModeLine->ModeParams.ActiveAreaWidth - FBWIDTH + 100;
  buffer_setup.dst.Rect.y = pModeLine->ModeParams.ActiveAreaHeight - FBHEIGHT + 100;

  stm_display_plane_queue_buffer(pPlane2, &buffer_setup);

  task_create(hotplug_task_fn, 0, OS21_DEF_MIN_STACK_SIZE, MAX_USER_PRIORITY, "hotplug", 0);

  while(1) { task_yield(); }

  return 0;
}
