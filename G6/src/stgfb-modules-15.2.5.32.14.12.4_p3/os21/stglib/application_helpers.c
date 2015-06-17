/***********************************************************************
 *
 * File: os21/stglib/application_helpers.cpp
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include "application_helpers.h"

/*
 * Colour bars in RGB, with the range clamped to 16-235 for video output.
 */
unsigned long colourbars100[] = { 0xffebebeb,
                                  0xffebeb10,
                                  0xff10ebeb,
                                  0xff10eb10,
                                  0xffeb10eb,
                                  0xffeb1010,
                                  0xff1010eb,
                                  0xff101010 };

void create_test_pattern(char *pBuf,unsigned long width,unsigned long height, unsigned long stride)
{
unsigned long *pixel;
unsigned long pixelVal;
int nbars = sizeof(colourbars100)/sizeof(unsigned long);
int x,y,i;

  /* Greyscale ramp */
  for(y=0;y<height/2;y++)
  {
    for(x=0;x<width;x++)
    {
      unsigned long tmp = x%256;
      pixel = (unsigned long *)(pBuf+y*stride+x*sizeof(unsigned long));
      pixelVal = (0xff<<24) | (tmp<<16) | (tmp<<8) | tmp;

      *pixel = pixelVal;
    }
  }

  for(i=0;i<nbars;i++)
  {
    int barwidth = width/nbars;
    for(y=height/2;y<height;y++)
    {
      for(x=(barwidth*i);x<(barwidth*(i+1));x++)
      {
    	pixel = (unsigned long *)(pBuf+y*stride+x*sizeof(unsigned long));

    	*pixel = colourbars100[i];
      }
    }
  }
}


volatile unsigned char *hotplug_poll_pio = 0;
int hotplug_pio_pin = 0;



void setup_soc(void)
{
}


void setup_analogue_voltages(stm_display_output_h output)
{
#if defined(CONFIG_xxxxxxxx)
  stm_display_output_set_control(output, OUTPUT_CTRL_DAC123_MAX_VOLTAGE, 1320);
  stm_display_output_set_control(output, OUTPUT_CTRL_DAC456_MAX_VOLTAGE, 1320);
#endif
}

stm_display_output_h get_analog_output(stm_display_device_h dev, int findmain)
{
  int i = 0;
  stm_display_output_h out;
  while(stm_display_device_open_output(dev,i++,&out) == 0)
  {
    uint32_t caps;
    stm_display_output_get_capabilities(out, &caps);
    if((caps & OUTPUT_CAPS_DISPLAY_TIMING_MASTER) != 0)
    {
      /* Note new capabilities to come to make this more obvious */
      if(findmain)
      {
        if((caps & OUTPUT_CAPS_HD_ANALOG) != 0)
          return out;
      }
      else
      {
        if((caps & OUTPUT_CAPS_HD_ANALOG) == 0)
          return out;
      }
    }
    stm_display_output_close(out);
  }

  return 0;
}

/*
 * SOC specific HDMI PHY configuration
 */

stm_display_output_h get_hdmi_output(stm_display_device_h dev)
{
  int i = 0;
  stm_display_output_h out;
  while(stm_display_device_open_output(dev,i++,&out) == 0)
  {
    uint32_t caps;
    stm_display_output_get_capabilities(out, &caps);
    if((caps & OUTPUT_CAPS_HDMI) != 0)
    {
      return out;
    }

    stm_display_output_close(out);
  }

  return 0;
}


stm_display_output_h get_dvo_output(stm_display_device_h dev)
{
  int i = 0;
  stm_display_output_h out;
  while(stm_display_device_open_output(dev,i++,&out) == 0)
  {
    uint32_t caps;
    stm_display_output_get_capabilities(out, &caps);
    if((caps & (OUTPUT_CAPS_DVO_656|OUTPUT_CAPS_DVO_16BIT|OUTPUT_CAPS_DVO_24BIT)) != 0)
    {
      return out;
    }

    stm_display_output_close(out);
  }

  return 0;
}

/*
 * We cannot call the stm_display_device_update_vsync_irq method directly in OS21 from interrupt
 * context, as it will use "free" which can block on a mutex. This is
 * illegal behaviour in OS21 interrupt routines, you cannot de-schedule.
 *
 */

static void displayupdate_task_fn(void *val)
{
  struct display_update_s *d = (struct display_update_s *)val;
  while(1)
  {
    semaphore_wait(d->frameupdate_sem);
    stm_display_device_update_vsync_irq(d->dev, d->timingID);
  }
}


struct display_update_s *start_display_update_thread(stm_display_device_h hDev, stm_display_output_h hOutput)
{
  struct display_update_s *d;
  uint32_t timingID;

  if(stm_display_output_get_timing_identifier(hOutput, &timingID)<0)
    return NULL;

  d = (struct display_update_s *)malloc(sizeof(struct display_update_s));
  if(!d)
    return NULL;


  d->frameupdate_sem  = semaphore_create_fifo(0);
  if(!d->frameupdate_sem)
  {
    free(d);
    return NULL;
  }

  d->dev      = hDev;
  d->output   = hOutput;
  d->timingID = timingID;

  task_create(displayupdate_task_fn, d, OS21_DEF_MIN_STACK_SIZE, MAX_USER_PRIORITY, "displayupdate", 0);
  return d;
}


int hdmi_isr(void *data)
{
  stm_display_output_h o = (stm_display_output_h)data;
  stm_display_output_handle_interrupts(o);
  return OS21_SUCCESS;
}

interrupt_t *get_hdmi_interrupt()
{
    return interrupt_handle(OS21_INTERRUPT_HDMI);
}

interrupt_t *get_main_vtg_interrupt()
{
  return NULL;
}

int get_yesno(void)
{
  int ch=0;
  while(1)
  {
    printf("Did the test succeed y/n?\n");
    ch = getchar();
    switch(ch)
    {
      case 'y':
      case 'Y':
        return 0;
      case 'n':
      case 'N':
        return 1;
    }
  }
}

stm_display_source_h get_and_connect_source_to_plane(stm_display_device_h dev, stm_display_plane_h pPlane)
{
  stm_display_source_h  Source = 0;
  uint32_t              sourceID;
  uint32_t              status;
  int                   ret;

  for(sourceID=0 ; ; sourceID++)  // Iterate available sources
  {
    if(stm_display_device_open_source(dev, sourceID,&Source) != 0)
    {
      printf("Failed to get a source for plane \n");
      return 0;
    }

    /* Check that this source is not already used by someone else */
    ret = stm_display_source_get_status(Source, &status);
    if ( (ret == 0) && (!(status & STM_STATUS_SOURCE_CONNECTED)) )
    {
      // try to connect the source on the prefered plane
      ret = stm_display_plane_connect_to_source(pPlane, Source);
      if (ret == 0)
      {
        printf("Plane %p successfully connected to Source %p\n",pPlane, Source);
        return Source; // break as it is ok
      }
    }

    stm_display_source_close(Source);
    Source = 0;
  }
  printf("Failed to get a source for plane %p\n",pPlane);
  return 0;
}


stm_display_plane_h get_and_connect_gfx_plane_to_output(stm_display_device_h dev, stm_display_output_h hOutput)
{
  stm_display_plane_h Plane = 0;
  uint32_t planeID;
  int ret;

  for(planeID=0 ; ; planeID++)  // Iterate available planes
  {
    stm_plane_capabilities_t caps;
    const char *outname;
    const char *planename;

    if( stm_display_device_open_plane(dev, planeID,&Plane) != 0)
    {
      printf("Failed to find a graphics plane for the output \n");
      return 0;
    }

    ret = stm_display_plane_get_capabilities(Plane, &caps);
    if(ret == 0)
    {
      if(caps & PLANE_CAPS_GRAPHICS)
      {
        // try to connect the source on the prefered plane
        ret = stm_display_plane_connect_to_output(Plane, hOutput);
        if (ret == 0)
        {
          stm_display_output_get_name(hOutput, &outname);
          stm_display_plane_get_name(Plane, &planename);
          printf("Plane \"%s\" successfully connected to Output \"%s\"\n",planename, outname);
          return Plane; // break as it is ok
        }
      }
    }
    stm_display_plane_close(Plane);
    Plane = 0;
  }

  return 0;
}

stm_display_plane_h get_and_connect_video_plane_to_output(stm_display_device_h dev, stm_display_output_h hOutput)
{
  stm_display_plane_h       hPlane = 0;
  uint32_t                  planeID;
  int                       ret;
  int                       matchingPlaneNb = 0;
  stm_plane_capabilities_t  planeCapsValue, planeCapsMask;

  // For every other chipsets, select the Main video plane
  planeCapsValue  = (stm_plane_capabilities_t) (PLANE_CAPS_VIDEO | PLANE_CAPS_VIDEO_BEST_QUALITY | PLANE_CAPS_PRIMARY_PLANE | PLANE_CAPS_PRIMARY_OUTPUT);

  planeCapsMask = planeCapsValue;

  if(stm_display_device_find_planes_with_capabilities(dev, planeCapsValue, planeCapsMask, &planeID, 1) <= 0)
  {
    printf("Failed to find a suitable video plane!\n");
    return 0;
  }

  if( stm_display_device_open_plane(dev, planeID, &hPlane) != 0)
  {
    printf("Failed to get a plane hdl!\n");
    return 0;
  }

  // try to connect the source on the prefered plane
  ret = stm_display_plane_connect_to_output(hPlane, hOutput);
  if (ret == 0)
  {
    const char *outname;
    const char *planename;

    stm_display_output_get_name(hOutput,&outname);
    stm_display_plane_get_name(hPlane, &planename);
    printf("hPlane \"%s\" successfully connected to Output \"%s\"\n", planename, outname);
  }

  return hPlane;
}

stm_display_plane_h get_and_connect_cursor_plane_to_output(stm_display_device_h dev, stm_display_output_h hOutput)
{
  stm_display_plane_h Plane = 0;
  uint32_t planeID;
  int ret;

  for(planeID=0 ; ; planeID++)  // Iterate available planes
  {
    stm_plane_capabilities_t caps;
    const char *outname;
    const char *planename;

    if( stm_display_device_open_plane(dev, planeID,&Plane) != 0)
    {
      printf("Failed to find a cursor plane for the output \n");
      return 0;
    }

    ret = stm_display_plane_get_capabilities(Plane, &caps);
    if(ret == 0)
    {
      if(!(caps & PLANE_CAPS_VIDEO ) && !(caps & PLANE_CAPS_GRAPHICS))
      {
        // try to connect the source on the prefered plane
        ret = stm_display_plane_connect_to_output(Plane, hOutput);
        if (ret == 0)
        {
          stm_display_output_get_name(hOutput,&outname);
          stm_display_plane_get_name(Plane, &planename);
          printf("Plane \"%s\" successfully connected to Output \"%s\"\n",planename, outname);
          return Plane; // break as it is ok
        }
      }
    }
    stm_display_plane_close(Plane);
    Plane = 0;
  }

  return 0;
}
