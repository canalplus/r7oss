/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixelcapturetest/main_to_aux.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/*
App note 3
Determine what the pixel capture capabilities are and
if all negotiations are Ok then attach Display core source to Pixel capture.

Use cases:
Orly_UC4
The system shall be able to pixel_capture the rendered main output and show the same on Aux output. Used as a debug assist
(when HDMITX HDCP authentication fails) in modern STB

*/

#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h>
#include <linux/slab.h>   /* Needed for kmalloc and kfree */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include <asm/io.h>
#include <linux/bpa2.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/random.h>
#include <asm/delay.h>

#include <stm_display.h>
#include <stm_pixel_capture.h>
#include "common.h"
#include "display.h"
#include "hdmirx_test.h"

#include <vibe_debug.h>
#include <thread_vib_settings.h>

#define INPUT_FROM_VIDEO_1              0
#define INPUT_FROM_VIDEO_2              1
#define INPUT_FROM_MAIN_DISPLAY         3
#define INPUT_FROM_AUX_DISPLAY          2
#define CAPTURE_BUFFERS_NUMBER          1
#define TUNNELED_CAPTURE_BUFFERS_NUMBER 3
#define MAX_CAPTURE_BUFFERS_NUMBER      5

/* Capture thread setting */
#define CAPTURE_TEST_THREAD_NUMBER      1
#if defined(CONFIG_STM_VIRTUAL_PLATFORM)
#define CAPTURE_TASK_WAIT_TIME          40 /* Simulate VSync duration for display */
#else
#define CAPTURE_TASK_WAIT_TIME          100
#endif

/* Default Buffer setting */
#define CAPTURE_DEFAULT_BUFFER_WIDTH    720
#define CAPTURE_DEFAULT_BUFFER_HEIGHT   480
#define CAPTURE_DEFAULT_BUFFER_FMT      STM_PIXEL_FORMAT_ARGB8888
#define CAPTURE_DEFAULT_BUFFER_BPP      4
#define CAPTURE_DEFAULT_BUFFER_CS       STM_PIXEL_CAPTURE_RGB

static char *buffer      = "720x480:ARGB8888:4:RGB";
static char *crop_window = "0:0:1280:720";
static char *input_name  = "stm_input_mix1";
static char *plane_name  = "Any";
static int   use_fb      = 1;
static int   main_to_aux = 1;
static int   tunneling   = 0;
static int   stream_inj_ena = 0;
static struct stm_display_io_windows      io_windows;
static stm_pixel_capture_buffer_format_t  buf_format;
static int bytes_per_pixel=3;
static int thread_vib_testcapture[2] = { THREAD_VIB_TESTCAPTURE_POLICY, THREAD_VIB_TESTCAPTURE_PRIORITY };

module_param_array_named(thread_VIB_TestCapture,thread_vib_testcapture, int, NULL, 0644);
MODULE_PARM_DESC(thread_VIB_TestCapture, "VIB-TestCapture thread:s(Mode),p(Priority)");

module_param(input_name, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(input_name, "Capture Input Name");

module_param(plane_name, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(plane_name, "Plane Name where to display captured buffers");

module_param(use_fb, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(use_fb, "Use the plane and memory associated to the framebuffer");

module_param(tunneling, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(tunneling, "Enable/Disable automatic tunneling of captured buffers");

module_param(main_to_aux, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(main_to_aux, "Display Main To Aux : 0 = Aux to Main | 1 Main to Aux");

module_param(buffer, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(buffer, "size:color format:bit per pixel:color space");

module_param(crop_window, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(crop_window, "position_x:position_y:width:height");

#if defined(CONFIG_STM_VIRTUAL_PLATFORM)
module_param(stream_inj_ena, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(stream_inj_ena, "Enable STREAM_INJ on VSOC platform");
#endif /* CONFIG_STM_VIRTUAL_PLATFORM */

typedef struct capture_task_handle_s {
  /* task info */
  void *TaskFunc;
  char  TaskName[32];

  /* capture info */
  unsigned int                        capture_id;
  stm_pixel_capture_h                 pixel_capture;
  stm_pixel_capture_buffer_descr_t   *buffer_descr;
  stm_pixel_capture_input_params_t    input_params;
  int                                 number_buffers;

  struct stm_capture_display_context  *DisplayContext;

  bool CaptureRunning;  // Indicates capture process is running
  bool ThreadTerminated;
} capture_task_handle_t;

typedef enum capture_modules_params_e {
  CAPTURE_MODULE_BUFFER_PARAMS,
  CAPTURE_MODULE_WINDOW_CROP,
} capture_modules_params_t;

static capture_task_handle_t CaptureContext[CAPTURE_TEST_THREAD_NUMBER];
static struct stm_capture_dma_area memory_pool;
static stm_pixel_capture_buffer_descr_t  buffer_descr[MAX_CAPTURE_BUFFERS_NUMBER];

static struct stm_capture_display_context *pDisplayContext=NULL;

static struct fb_info       *info=NULL;

static const char * get_pixel_format_name(stm_pixel_capture_format_t cap_fmt)
{
#define CAP_CMP(val) case val: return #val
  switch (cap_fmt) {
  CAP_CMP(STM_PIXEL_FORMAT_RGB565);
  CAP_CMP(STM_PIXEL_FORMAT_RGB888);
  CAP_CMP(STM_PIXEL_FORMAT_ARGB1555);
  CAP_CMP(STM_PIXEL_FORMAT_ARGB8565);
  CAP_CMP(STM_PIXEL_FORMAT_ARGB8888);
  CAP_CMP(STM_PIXEL_FORMAT_ARGB4444);
  CAP_CMP(STM_PIXEL_FORMAT_YUV_NV12);
  CAP_CMP(STM_PIXEL_FORMAT_YUV_NV16);
  CAP_CMP(STM_PIXEL_FORMAT_YUV);
  CAP_CMP(STM_PIXEL_FORMAT_YCbCr422R);

  case STM_PIXEL_FORMAT_NONE:
  case STM_PIXEL_FORMAT_COUNT:
  default:
    break;
  }

  return "unknown";
#undef CAP_CMP
}

/*************************************************************/
/**                                                         **/
/**  Create an RGB Test Pattern                             **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
static void capture_create_rgb_test_pattern(unsigned char *bufferdata, int width, int height, int stride)
{
  unsigned char *pixel = (unsigned char *)bufferdata;
  int bpp = stride/width;
  int x,y;

  if(!pixel)
    return;

  if(bpp == 0)
      bpp = 1;

  for(y=0;y<height;y++)
  {
    for(x=0;x<(width*bpp);x+=bpp)
    {
      unsigned long red   = 0;
      unsigned long green = 0;
      unsigned long blue  = 0;

      if(y<(height/3))
      {
        red   = x%256;
        green = 255;
        blue  = 0;
      }
      else if(y<(2*height/3))
      {
        red   = 0;
        green = x%256;
        blue  = 255;
      }
      else
      {
        red   = 255;
        green = 0;
        blue  = x%256;
      }

      if(bpp == 4)
      {
        *(pixel+x) = 0xff;
        *(pixel+(x+1)) = blue;
        *(pixel+(x+2)) = green;
        *(pixel+(x+3)) = red;
      }
      if(bpp == 3)
      {
        *(pixel+x) = blue;
        *(pixel+(x+1)) = green;
        *(pixel+(x+2)) = red;
      }
    }

    pixel += (stride);
  }
}

/*************************************************************/
/**                                                         **/
/**  Create an Raster 422 Single Buffer Test Pattern        **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
static void capture_create_422r_test_pattern(unsigned char *bufferdata, int width, int height, int stride)
{
  unsigned short *pixel = (unsigned short *)bufferdata;
  int x,y;

  if(!pixel)
    return;

  for(y=0;y<height;y++)
  {
    for(x=0;x<width;x++)
    {
      unsigned chroma=0;

      if((y<(height/2) && (x%2 == 0)) || (y>=(height/2) && (x%2 == 1)))
        chroma = 255;

      *(pixel+x) = (x%256<<8) | chroma;
    }

    pixel += (stride/2);
  }
}

/*************************************************************/
/**                                                         **/
/**  Create a Raster 422 Double Buffer Test Pattern         **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
static void capture_create_422r2b_test_pattern(unsigned char *bufferdata, int width, int height, int chroma_offset, int chroma_height, int stride)
{
  unsigned char *luma    = bufferdata;
  unsigned char *chroma1 = bufferdata+chroma_offset;
  unsigned char *chroma2 = chroma1+(height*stride/2);
  int x,y;

  for(y=0;y<height;y++)
  {
    for(x=0;x<width;x++)
    {
      *(luma+x) = x%256;
    }

    luma += stride;
  }

  for(y=0;y<chroma_height;y++)
  {
    for(x=0;x<(width/2);x++)
    {
      /*
       * Produce shades of red for YUV formats and shades
       * of blue for YVU in the top half of the screen, and
       * just the opposite in the bottom half of the screen.
       */
      if(y<(chroma_height/2))
      {
        *(chroma1+x) = 0;
        *(chroma2+x) = 255;
      }
      else
      {
        *(chroma1+x) = 255;
        *(chroma2+x) = 0;
      }
    }

    chroma1 += stride/2;
    chroma2 += stride/2;
  }
}

/*************************************************************/
/**                                                         **/
/**  Parse Capture Module Parameters                        **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
static int capture_parse_module_parameters(capture_modules_params_t params)
{
  char *paramstring       = NULL;
  char *copy              = NULL;
  char *tmpstr            = NULL;
  int   ret = 0;

  char *position_x        = NULL;
  char *position_y        = NULL;
  char *width             = NULL;
  char *height            = NULL;

  char *size              = NULL;
  char *fmt               = NULL;

  switch(params)
  {
    case CAPTURE_MODULE_BUFFER_PARAMS:
      paramstring = buffer;
      break;
    case CAPTURE_MODULE_WINDOW_CROP:
      paramstring = crop_window;
      break;
    default:
      break;
  }

  printk("capture: %s parameters = \"%s\"\n",(params? "buffer" : "crop"),(paramstring==NULL)?"<NULL>":paramstring);

  if(paramstring == NULL || *paramstring == '\0')
    return -ENODEV;

  copy = kstrdup(paramstring, GFP_KERNEL);
  if(!copy)
    return -ENOMEM;

  tmpstr         = copy;
  switch(params)
  {
    case CAPTURE_MODULE_BUFFER_PARAMS:
      size        = strsep(&tmpstr,":,");
      fmt         = strsep(&tmpstr,":,");
      printk("buffer parameters: %s-%s\n",size,fmt);
      break;
    case CAPTURE_MODULE_WINDOW_CROP:
      position_x  = strsep(&tmpstr,":,");
      position_y  = strsep(&tmpstr,":,");
      width       = strsep(&tmpstr,":,");
      height      = strsep(&tmpstr,":,");
      printk("cropping window: %sx%s@%s-%s\n",width,height,position_x,position_y);
      break;
  }

  /* TBD : export parameters for main application usage */
  if(size)
  {
    char    *param = NULL;
    unsigned long w = CAPTURE_DEFAULT_BUFFER_WIDTH;
    unsigned long h = CAPTURE_DEFAULT_BUFFER_HEIGHT;

    param = strsep(&size,"x");
    if(param)
      ret = strict_strtoul((const char*)param,10,&w);
    param = strsep(&size,"x");
    if(param)
      ret = strict_strtoul((const char*)param,10,&h);

    if(!ret)
    {
      io_windows.output_window.x = 0;
      io_windows.output_window.y = 0;
      io_windows.output_window.width  = (unsigned int)w;
      io_windows.output_window.height = (unsigned int)h;
      printk("buffer size: %d-%d\n",(unsigned int)w,(unsigned int)h);
    }

    if(!strcmp(fmt, "ARGB8888"))
    {
      buf_format.format = STM_PIXEL_FORMAT_ARGB8888;
      buf_format.color_space = STM_PIXEL_CAPTURE_RGB;
      bytes_per_pixel = 4;
    }
    else if(!strcmp(fmt, "ARGB4444"))
    {
      buf_format.format = STM_PIXEL_FORMAT_ARGB4444;
      buf_format.color_space = STM_PIXEL_CAPTURE_RGB;
      bytes_per_pixel = 2;
    }
    else if(!strcmp(fmt, "ARGB1555"))
    {
      buf_format.format = STM_PIXEL_FORMAT_ARGB1555;
      buf_format.color_space = STM_PIXEL_CAPTURE_RGB;
      bytes_per_pixel = 2;
    }
    else if(!strcmp(fmt, "ARGB8565"))
    {
      buf_format.format = STM_PIXEL_FORMAT_ARGB8565;
      buf_format.color_space = STM_PIXEL_CAPTURE_RGB;
      bytes_per_pixel = 3;
    }
    else if(!strcmp(fmt, "RGB888"))
    {
      buf_format.format = STM_PIXEL_FORMAT_RGB888;
      buf_format.color_space = STM_PIXEL_CAPTURE_RGB;
      bytes_per_pixel = 3;
    }
    else if(!strcmp(fmt, "YUV"))
    {
      buf_format.format = STM_PIXEL_FORMAT_YUV;
      buf_format.color_space = STM_PIXEL_CAPTURE_BT709_FULLRANGE;
      bytes_per_pixel = 3;
    }
    else if(!strcmp(fmt, "YUV_NV12"))
    {
      buf_format.format = STM_PIXEL_FORMAT_YUV_NV12;
      buf_format.color_space = STM_PIXEL_CAPTURE_BT709_FULLRANGE;
      bytes_per_pixel = 1;
    }
    else if(!strcmp(fmt, "YUV_NV16"))
    {
      buf_format.format = STM_PIXEL_FORMAT_YUV_NV16;
      buf_format.color_space = STM_PIXEL_CAPTURE_BT709_FULLRANGE;
      bytes_per_pixel = 1;
    }
    else if(!strcmp(fmt, "YCbCr422R"))
    {
      buf_format.format = STM_PIXEL_FORMAT_YCbCr422R;
      buf_format.color_space = STM_PIXEL_CAPTURE_BT709_FULLRANGE;
      bytes_per_pixel = 2;
    }
    else
    {
      buf_format.format = STM_PIXEL_FORMAT_ARGB8888;
      buf_format.color_space = STM_PIXEL_CAPTURE_RGB;
      bytes_per_pixel = 4;
    }
  }

  if(position_x && position_y && width && height)
  {
    unsigned long pos_x = 0;
    unsigned long pos_y = 0;
    unsigned long w = 0;
    unsigned long h = 0;

    ret  = strict_strtoul((const char*)position_x,10,&pos_x);
    ret |= strict_strtoul((const char*)position_y,10,&pos_y);
    ret |= strict_strtoul((const char*)width,10,&w);
    ret |= strict_strtoul((const char*)height,10,&h);

    if(!ret)
    {
      io_windows.input_window.x = (unsigned int)pos_x;
      io_windows.input_window.y = (unsigned int)pos_y;
      io_windows.input_window.width  = (unsigned int)w;
      io_windows.input_window.height = (unsigned int)h;
      printk("cropping size: %dx%d@%d-%d\n",(unsigned int)w,(unsigned int)h,(unsigned int)pos_x,(unsigned int)pos_y);
    }
  }

  kfree(copy);
  return ret;
}


/*************************************************************/
/**                                                         **/
/**  Thread for Queue/Dequeueing buffers to and from        **/
/**  capture pipe.                                          **/
/**                                                         **/
/*************************************************************/
/*#define STREAM_INJECTION_CONT_ON*/
static int Capture_Thread( void *data )
{
  int res=0;
  int tunneling=0;

  capture_task_handle_t *TaskContext = (capture_task_handle_t *) data;

  printk(KERN_INFO "[%s] \n", TaskContext->TaskName);
  printk(KERN_INFO "******************************************\n");
  printk(KERN_INFO "*** Starting Capture Thread            ***\n");
  printk(KERN_INFO "******************************************\n");
  printk(KERN_INFO "\n");

  stm_pixel_capture_lock(TaskContext->pixel_capture);

  /* attach the display source to the pixel capture */
  if(use_fb == 0)
  {
    if(TaskContext->DisplayContext->tunneling && TaskContext->DisplayContext->hSource)
    {
      stm_pixel_capture_attach(TaskContext->pixel_capture, (stm_object_h)TaskContext->DisplayContext->hSource);
      tunneling = 1;
    }
  }

#if defined(CONFIG_STM_VIRTUAL_PLATFORM)
  if(stream_inj_ena)
  {
    /* Prapare Virtual Stream injection */
    res = stream_init_pattern_injection(TaskContext->input_params);
    if (res < 0)
    {
      printk (KERN_ERR "failed to start capture thread err = %d\n", res);
    }
#ifdef STREAM_INJECTION_CONT_ON
    stream_set_pattern_injection_status(true, 0xFFFFFF);
#endif /* STREAM_INJECTION_CONT_ON */
  }
#endif /* CONFIG_STM_VIRTUAL_PLATFORM */

  /* Now that everything is right the display can start operating */
  stm_pixel_capture_start(TaskContext->pixel_capture);

  TaskContext->CaptureRunning   = true;
  TaskContext->ThreadTerminated = false;
  while (TaskContext->CaptureRunning && !TaskContext->ThreadTerminated)
  {
    unsigned int Jiffies = ((HZ/CAPTURE_TASK_WAIT_TIME)) + 1;
    int i;
    bool do_pan = false;

    if(use_fb)
    {
      do_pan = CAPTURE_BUFFERS_NUMBER > 1 ? true:false;
    }

    for(i=0;i<TaskContext->number_buffers;i++)
    {
      res = stm_pixel_capture_queue_buffer(TaskContext->pixel_capture, &TaskContext->buffer_descr[i]);
      if (res)
      {
        printk (KERN_ERR "[%s] :failed to queue capture buffers %p err = %d\n", TaskContext->TaskName, &TaskContext->buffer_descr[i], res);
        continue;
      }
#if defined(CONFIG_STM_VIRTUAL_PLATFORM)
#ifndef STREAM_INJECTION_CONT_ON
      if(stream_inj_ena)
      {
        /* Start Virtual Stream injection */
        stream_set_pattern_injection_status(true,1);
      }
#endif /* STREAM_INJECTION_CONT_ON */
#endif /* CONFIG_STM_VIRTUAL_PLATFORM */
      if(!tunneling)
      {
        res = stm_pixel_capture_dequeue_buffer(TaskContext->pixel_capture, &TaskContext->buffer_descr[i]);
        if (res)
          printk (KERN_ERR "[%s] : failed to dequeue capture buffers %p err = %d\n", TaskContext->TaskName, &TaskContext->buffer_descr[i], res);
#if defined(CONFIG_STM_VIRTUAL_PLATFORM)
#ifndef STREAM_INJECTION_CONT_ON
        if(stream_inj_ena)
        {
          /* Stop Virtual Stream injection */
          stream_set_pattern_injection_status(false,0);
        }
#endif /* STREAM_INJECTION_CONT_ON */
#endif /* CONFIG_STM_VIRTUAL_PLATFORM */
        if (use_fb && info)
        {
          /* dead code as do_pan is only equal to false
          if(do_pan)
          {
            capture_pan_display(info, &TaskContext->buffer_descr[i]);
          }
          */
        }
        else
        {
          stm_display_buffer_t DisplayBuffer;

          res = display_fill_buffer_descriptor(&DisplayBuffer, TaskContext->buffer_descr[i], TaskContext->input_params);
          if(res)
          {
            printk (KERN_ERR "[%s] :failed to fill display buffers %p err = %d\n", TaskContext->TaskName, &TaskContext->buffer_descr[i], res);
            continue;
          }

          res = stm_display_source_queue_buffer(TaskContext->DisplayContext->hQueue, &DisplayBuffer);
          if(res)
          {
            printk (KERN_ERR "[%s] :failed to queue display buffers %p err = %d\n", TaskContext->TaskName, &DisplayBuffer, res);
            continue;
          }
        }
      }
      /*
       * For Tunneled capture :
       * As all capture buffers were queued then we can now terminate this
       * tread of course without freeing the ressources.
       */
      TaskContext->ThreadTerminated = !!tunneling;
    }
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(Jiffies);
  }

#if defined(CONFIG_STM_VIRTUAL_PLATFORM)
#ifdef STREAM_INJECTION_CONT_ON
  /* Stop Virtual Stream injection */
  stream_set_pattern_injection_status(false,0);
#endif /* STREAM_INJECTION_CONT_ON */
#endif /* CONFIG_STM_VIRTUAL_PLATFORM */

   /*
   * As we are immediatly terminating the capture thread for tunneled capture
   * then we will not proceed to the shutdown of capture and the free of used
   * ressource right now. This will be done just before exiting the test.
   */
  if(!tunneling)
  {
    /*
    Stop capture
    Only after this operation it will be allowed to re-configured the capture memory, or to terminate entirely this capture process
    */
    stm_pixel_capture_stop(TaskContext->pixel_capture);

    stm_pixel_capture_unlock(TaskContext->pixel_capture);
  }

  if (!res)
    printk (KERN_INFO "[%s] - Capture Thread terminated.\n", TaskContext->TaskName);
  else
    printk (KERN_INFO "[%s] - Capture Thread failed err = %d!\n", TaskContext->TaskName, res);

  TaskContext->ThreadTerminated = true;
  return res;
}


/*************************************************************/
/**                                                         **/
/**  Initialize capture thread context                      **/
/**                                                         **/
/*************************************************************/
static int capture_contexts_init (stm_pixel_capture_h pixel_capture,
              stm_pixel_capture_buffer_descr_t *buffer_descr,
              stm_pixel_capture_input_params_t input_params,
              int number_buffers, capture_task_handle_t *Context)
{
  int i;

  printk(KERN_INFO "\n");
  printk(KERN_INFO "*************************************************\n");
  printk(KERN_INFO "*** Initializing Capture Thread Context       ***\n");
  printk(KERN_INFO "*************************************************\n");
  printk(KERN_INFO "\n");

  /* Set capture_id for each thread */
  for(i = 0; i < CAPTURE_TEST_THREAD_NUMBER ; i++)
  {
    snprintf(Context[i].TaskName, sizeof(Context[i].TaskName), "VIB-TestCapture/%d", i);
    Context[i].capture_id     = 1;
    Context[i].pixel_capture  = pixel_capture;
    Context[i].buffer_descr   = buffer_descr;
    Context[i].input_params   = input_params;
    Context[i].number_buffers = number_buffers;
    Context[i].DisplayContext = pDisplayContext;
  }

  /* finally set the task functions */
  Context[0].TaskFunc = (Capture_Thread);

  return 0;
}


/*************************************************************/
/**                                                         **/
/**  Start capture thread context                           **/
/**                                                         **/
/*************************************************************/
static int capture_thread_start (capture_task_handle_t *Context)
{
  struct task_struct *Task = NULL;
  int                 sched_policy = 0;
  struct sched_param  sched_param = {0};
  int i,j;

  for(i = 0; i < CAPTURE_TEST_THREAD_NUMBER ; i++)
  {
    Task = kthread_create(Context[i].TaskFunc, &Context[i], Context[i].TaskName);
    if (IS_ERR(Task)) {
      printk
          ("%s - Unable to start blit thread\n", __func__);
      Context[i].CaptureRunning = false;
      return -EINVAL;
    }

    printk(KERN_INFO "\n");
    printk(KERN_INFO "************************************************************\n");
    printk(KERN_INFO "*** Start Capture Thread %s                               ***\n",Context[i].TaskName);
    printk(KERN_INFO "*************************************************************\n");
    printk(KERN_INFO "\n");

    /* Same scheduling settings for all test capture thread(s) */
    sched_policy               = thread_vib_testcapture[0];
    sched_param.sched_priority = thread_vib_testcapture[1];
    if ( sched_setscheduler(Task, sched_policy, &sched_param) ) {
      TRC(TRC_ID_ERROR, "FAILED to set thread scheduling parameters: name=%s, policy=%d, priority=%d", \
          Context[i].TaskName, sched_policy, sched_param.sched_priority);
      Context[i].CaptureRunning = false;
      return -EINVAL;
    }

    wake_up_process(Task);

    for (j = 0;
         (j < 100) && (!Context[i].CaptureRunning); j++) {
      unsigned int Jiffies = ((HZ) / 1000) + 1;

      set_current_state(TASK_INTERRUPTIBLE);
      schedule_timeout(Jiffies);
    }
  }

  return 0;
}

/*************************************************************/
/**                                                         **/
/**  Stop capture thread context                            **/
/**                                                         **/
/*************************************************************/
static void capture_thread_stop (capture_task_handle_t *Context)
{
  int i;

  for(i = 0 ; i < CAPTURE_TEST_THREAD_NUMBER ; i++)
  {
    if (Context[i].CaptureRunning) {
      Context[i].CaptureRunning = false;

      while (!Context[i].ThreadTerminated) {
        unsigned int Jiffies = ((HZ) / 1000) + 1;

        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(Jiffies);
      }

      if(Context[i].DisplayContext)
      {
        if(Context[i].DisplayContext->tunneling && Context[i].pixel_capture)
        {
          /*
           * Stop capture and to terminate this capture process.
          */
          stm_pixel_capture_stop(Context[i].pixel_capture);

          stm_pixel_capture_unlock(Context[i].pixel_capture);

          stm_pixel_capture_detach(Context[i].pixel_capture, (stm_object_h)Context[i].DisplayContext->hSource);
        }
      }
    }
  }
}

/*************************************************************/
/**                                                         **/
/**  Typical display usage of the pixel capture (Start)     **/
/**                                                         **/
/*************************************************************/
int pixel_capture_display_start(uint32_t number_buffers, stm_pixel_capture_device_type_t type)
{
  stm_pixel_capture_h pixel_capture = (stm_pixel_capture_h)0;

  stm_pixel_capture_buffer_format_t format = {0};
  stm_pixel_capture_input_params_t  params = {0};
  stm_pixel_capture_capabilities_flags_t capabilities_flags = 0;
  stm_pixel_capture_rect_t crop_window;
  const stm_pixel_capture_format_t *capture_formats;
  int                               n_Formats = 0;

  int i,res;

  if(tunneling && use_fb)
  {
    /* Cannot support tunneling with framebuffer display interface */
    printk ("ERROR : Tunneled Capture is not supported with Framebuffer display interface !!\n");
    return -1;
  }

  if(tunneling)
  {
    /* Use 3 buffers for Tunneled Capture */
    number_buffers = TUNNELED_CAPTURE_BUFFERS_NUMBER;
  }
  else
    number_buffers = CAPTURE_BUFFERS_NUMBER;

  info = NULL;

  /* Initialize local variables */
  memset(&format, 0, sizeof(stm_pixel_capture_buffer_format_t));
  memset(&params, 0, sizeof(stm_pixel_capture_input_params_t));
  memset(&io_windows, 0, sizeof(struct stm_display_io_windows));
  memset(&buf_format, 0, sizeof(stm_pixel_capture_buffer_format_t));

  if(capture_parse_module_parameters(CAPTURE_MODULE_BUFFER_PARAMS) < 0)
  {
    /* Cannot query capture handle! */
    printk ("ERROR : Cannot get passed buffer parameters !\n");
    return -1;
  }

  if(capture_parse_module_parameters(CAPTURE_MODULE_WINDOW_CROP) < 0)
  {
    /* Cannot query capture handle! */
    printk ("ERROR : Cannot get passed buffer parameters !\n");
    return -1;
  }

  /* obtain a handle for the first pixel capture device */
  if(stm_pixel_capture_open(type, 0, &pixel_capture ) < 0)
  {
    /* Cannot query capture handle! */
    printk ("ERROR : Cannot get pixel capture handle !\n");
    return -1;
  }

  if(stm_pixel_capture_query_capabilities(pixel_capture, &capabilities_flags) < 0)
  {
    /* Cannot query capture capabilities! */
    printk ("ERROR : No Aux display available!\n");
    stm_pixel_capture_close( pixel_capture );
    return -1;
  }

  n_Formats = stm_pixel_capture_enum_image_formats(pixel_capture, &capture_formats);
  if(n_Formats < 0)
  {
    /* Cannot query capture capabilities! */
    printk ("ERROR : Can't retreive supported buffer formats (err = %d)!\n", n_Formats);
    stm_pixel_capture_close( pixel_capture );
    return -1;
  }

  printk ("***********************************************************\n");
  printk ("*** INFO : Supported buffer formats                     ***\n");
  printk ("***********************************************************\n");
  for(i=0;i< n_Formats;i++)
    printk ("\t --> idx : %d \t- Format = %s\n", i, get_pixel_format_name(capture_formats[i]));
  printk ("***********************************************************\n");

  if(capabilities_flags != STM_PIXEL_DISPLAY)
  {
    /* if the capture mode is not suported then return */
    printk ("ERROR : Wrong Capture Caps!\n");
    stm_pixel_capture_close( pixel_capture );
    return -1;
  }

  capture_list_inputs(pixel_capture);

  if (capture_set_input_by_name(pixel_capture, input_name) !=0 )
  {
    stm_pixel_capture_close( pixel_capture );
    printk ("ERROR : Failed to set Capture Input!\n");
    return -1;
  }

  if(use_fb)
  {
    /* Now get the Main display buffer to be used by capture */
    res = main_to_aux? (num_registered_fb - 2) :(num_registered_fb - 1);
    if (registered_fb[res])
    {
      info = registered_fb[res];
      printk ("found a registered framebuffer @ %p\n", info);
    }
  }

  if(info)
  {
    struct module *owner = info->fbops->owner;

    printk(KERN_INFO "***********************************************************\n");
    printk(KERN_INFO "*** Pixel Capture Test Display Using framebuffer memory ***\n");
    printk(KERN_INFO "***********************************************************\n");

    if(stream_inj_ena)
    {
      /* Fixed scenario for 800x300 input params */
      params.active_window.x                = 16;
      params.active_window.y                = 30;
      params.active_window.width            = 720;
      params.active_window.height           = 240;
      params.pixel_aspect_ratio.numerator   = 16;
      params.pixel_aspect_ratio.denominator = 9;
      params.src_frame_rate                 = 50000;
      params.color_space                    = STM_PIXEL_CAPTURE_RGB;
      params.htotal                         = 800; /* now begin used by dvp driver */
      params.vtotal                         = 300; /* now begin used by dvp driver */
      params.flags                          = 0;
    }
#ifdef HDMIRX_MODE_DETECTION_TEST
    else if(type == STM_PIXEL_CAPTURE_DVP)
    {
      /* hdmirx mode detection */
      res = hdmirx_mode_detection_test_start(&params);
      if(res < 0)
      {
        printk ("ERROR : hdmirx mode detection failed err = %d\n", res);
        printk ("use static 720P@50Hz mode config...\n");
        params.active_window.x                = 370;
        params.active_window.y                = 26;
        params.active_window.width            = 1280;
        params.active_window.height           = 720;
        params.pixel_aspect_ratio.numerator   = 16;
        params.pixel_aspect_ratio.denominator = 9;
        params.src_frame_rate                 = 50000;
        params.color_space                    = STM_PIXEL_CAPTURE_RGB;
        params.htotal                         = 1650; /* now begin used by dvp driver */
        params.vtotal                         = 750; /* now begin used by dvp driver */
        params.flags                          = 0;
      }
    }
#endif
    else
    {
      /*
       * setup input params for 720P display. Should be done according to
       * current Main display mode.
       */
      if (try_module_get (owner)) {
        if (info->fbops->fb_open && !info->fbops->fb_open (info, 0))
        {
          params.active_window.x                = info->var.left_margin;
          params.active_window.y                = info->var.upper_margin;
          params.active_window.width            = info->var.xres;
          params.active_window.height           = info->var.yres;
          params.pixel_aspect_ratio.numerator   = 16;
          params.pixel_aspect_ratio.denominator = 9;
          params.src_frame_rate                 = info->var.pixclock;
          params.color_space                    = STM_PIXEL_CAPTURE_BT601;
          params.vtotal                         = info->var.vsync_len; /* not used by driver for the moment */
          params.htotal                         = info->var.hsync_len; /* not used by driver for the moment */

          switch (info->var.vmode & FB_VMODE_MASK)
          {
            case FB_VMODE_NONINTERLACED:
              params.flags = 0;
              break;
            case FB_VMODE_INTERLACED:
              params.flags = STM_PIXEL_CAPTURE_BUFFER_INTERLACED;
              break;
            case FB_VMODE_DOUBLE:
            default:
              params.flags = STM_PIXEL_CAPTURE_BUFFER_INTERLACED;
              break;
          }
        }
        module_put (owner);
      }
      info = NULL;
    }
  }
  else
  {
    printk(KERN_INFO "***********************************************************\n");
    printk(KERN_INFO "*** Pixel Capture Test Display Using Display STKPI      ***\n");
    printk(KERN_INFO "***********************************************************\n");

    /* use STKPI */
    if(setup_main_display(0, &params, &pDisplayContext, io_windows, plane_name, main_to_aux, stream_inj_ena, tunneling) != 0)
    {
      stm_pixel_capture_close( pixel_capture );
      printk ("ERROR : Failed to retreive Capture Input Params!\n");
      return -1;
    }
#ifdef HDMIRX_MODE_DETECTION_TEST
    /* reconfigure le mode de l'input en cas du DVP */
    if(type == STM_PIXEL_CAPTURE_DVP)
    {
      /* hdmirx mode detection */
      res = hdmirx_mode_detection_test_start(&params);
      if(res < 0)
      {
        return -1;
        //printk ("ERROR : hdmirx mode detection failed err = %d\n", res);
        //printk ("use static 720P@50Hz mode config...\n");
        //params.active_window.x                = 370;
        //params.active_window.y                = 26;
        //params.active_window.width            = 1280;
        //params.active_window.height           = 720;
        //params.pixel_aspect_ratio.numerator   = 16;
        //params.pixel_aspect_ratio.denominator = 9;
        //params.src_frame_rate                 = 50000;
        //params.color_space                    = STM_PIXEL_CAPTURE_RGB;
        //params.htotal                         = 1650; /* now begin used by dvp driver */
        //params.vtotal                         = 750; /* now begin used by dvp driver */
        //params.flags                          = 0;
      }
    }
#endif
  }

  printk ("input_params: %d-%d@%dx%d - flags = %u\n", params.active_window.x, params.active_window.y,
            params.active_window.width, params.active_window.height,
            params.flags);

  if(stm_pixel_capture_set_input_params( pixel_capture, params) != 0)
  {
    stm_pixel_capture_close( pixel_capture );
    printk ("ERROR : Failed to set Capture Input Params!\n");
    return -1;
  }

  if ( capture_get_default_window_rect(pixel_capture, params, &crop_window) )
  {
    stm_pixel_capture_close( pixel_capture );
    printk ("ERROR : Failed to set Capture Cropping!\n");
    return -1;
  }

  if ( capture_set_crop(pixel_capture, crop_window) )
  {
    printk ("ERROR : Failed to set Capture Cropping!\n");
  }

  if(use_fb)
  {
    /* Now get the Aux display buffer to be used by capture */
    res = main_to_aux? (num_registered_fb - 1):(num_registered_fb - 2);
    if (registered_fb[res])
    {
      info = registered_fb[res];
      printk ("found a registered framebuffer @ %p\n", info);
    }
  }

  if (info) {
    struct module *owner = info->fbops->owner;

    if (try_module_get (owner)) {
      if (info->fbops->fb_open
          && !info->fbops->fb_open (info, 0)) {
        switch (info->var.bits_per_pixel) {
          case 16: format.format = STM_PIXEL_FORMAT_RGB565; break;
          case 24: format.format = STM_PIXEL_FORMAT_RGB888; break;
          case 32: format.format = STM_PIXEL_FORMAT_ARGB8888; break;
          default:
            if (info->fbops->fb_release)
              info->fbops->fb_release (info, 0);
            module_put (owner);
            info = NULL;
            break;
        }

        if (info) {
          uint32_t buffer_rounded_size = 0;
          uint32_t buffer_size = 0;

          format.width  = info->var.xres;
          format.height = info->var.yres;
          format.stride = info->var.xres * info->var.bits_per_pixel / 8;
          format.color_space = STM_PIXEL_CAPTURE_RGB;

          switch (info->var.vmode & FB_VMODE_MASK)
          {
            case FB_VMODE_NONINTERLACED:
              format.flags = 0;
              break;
            case FB_VMODE_INTERLACED:
            case FB_VMODE_DOUBLE:
            default:
              format.flags = STM_PIXEL_CAPTURE_BUFFER_INTERLACED;
#if 0
              if(params.flags == 0)
              {
                /*
                 * Current framebuffer is PROGRESSIVE.
                 * As we are not queueing a new interlaced buffer then
                 * we should request to upsize the captured buffer in
                 * order to get correct full screen display
                 */
                format.height *= 2;
              }
#endif
              break;
          }

          buffer_size = format.height * format.stride;
          buffer_rounded_size = (buffer_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

          for (i = 0; i < number_buffers; i++)
          {
            buffer_descr[i].cap_format = format;
            buffer_descr[i].bytesused = buffer_rounded_size;
            buffer_descr[i].length = buffer_rounded_size;
            /* for the moment test RGB formats */
            buffer_descr[i].rgb_address = info->fix.smem_start + (i * buffer_rounded_size);
            memory_pool.memory          = (void *) ioremap(buffer_descr[i].rgb_address, buffer_rounded_size); //virtual address

            printk ("buffer_descr[%d] : %x %u %u %u %u\n", i, (unsigned int)buffer_descr[i].rgb_address, buffer_descr[i].bytesused, buffer_descr[i].cap_format.stride,
                     buffer_descr[i].cap_format.width, buffer_descr[i].cap_format.height);
          }
        }
      }
      else {
        module_put (owner);
        printk ("no fb_open()\n");
        info = NULL;
      }
    }
    else {
      printk ("failed try_module_get\n");
      info = NULL;
    }
  }

  if (!info) {
    /* try if the 480p RGB single plane is supported */
    format.width  = io_windows.output_window.width;
    format.height = io_windows.output_window.height;

    format.stride = bytes_per_pixel*format.width;
    format.stride += format.stride %32;

    format.format = buf_format.format;
    format.color_space = buf_format.color_space;

    format.flags = 0;

    /* Allocate buffers memory for capture*/
    if(allocate_capture_buffers_memory(pixel_capture, number_buffers, format, buffer_descr, &memory_pool) !=0 )
    {
      stm_pixel_capture_close( pixel_capture );
      return -1;
    }
  }

  /* Inject the test Pattern into buffers */
  for (i = 0; i < number_buffers; i++)
  {
    unsigned char *virtual_add =  (unsigned char *)(memory_pool.memory + (i * buffer_descr[i].bytesused));
    if( (buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_RGB888) || (buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_RGB565)
      ||(buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_ARGB8888) || (buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_ARGB1555)
      ||(buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_ARGB4444) || (buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_ARGB8565))
    {
      capture_create_rgb_test_pattern(virtual_add, buffer_descr[i].cap_format.width,
            buffer_descr[i].cap_format.height, buffer_descr[i].cap_format.stride);
    }
    else if((buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_YCbCr422R) || (buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_YUV))
    {
      capture_create_422r_test_pattern(virtual_add, buffer_descr[i].cap_format.width,
            buffer_descr[i].cap_format.height, buffer_descr[i].cap_format.stride);
    }
    else if((buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_YUV_NV12) || (buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_YUV_NV16))
    {
      capture_create_422r2b_test_pattern(virtual_add, buffer_descr[i].cap_format.width,
            buffer_descr[i].cap_format.height, buffer_descr[i].chroma_offset, buffer_descr[i].cap_format.height, buffer_descr[i].cap_format.stride);
    }
    else
    {
      printk ("WARNING : Capture Pixel Format NOT supported?? Ignore test pattern generation for this.\n");
    }
  }

  printk ("buffer_format: w = %d - h = %d - pitch = %d - color_space = %d - flags = %u\n", format.width, format.height,
            format.stride, format.color_space, format.flags);

  if (capture_set_format(pixel_capture, format) !=0 )
  {
    stm_pixel_capture_close( pixel_capture );
    printk ("ERROR : Failed to set Capture Pixel Format!\n");
    return -1;
  }

  res = capture_contexts_init(pixel_capture, buffer_descr, params, number_buffers, CaptureContext);
  if (res) {
    printk ("failed to start capture thread err = %d\n", res);
    if(!info)
      free_capture_buffers_memory(&memory_pool);
    else
      iounmap(memory_pool.memory);
    return res;
  }
  capture_thread_start(CaptureContext);

  return res;
}
EXPORT_SYMBOL(pixel_capture_display_start);

/*************************************************************/
/**                                                         **/
/**  Typical display usage of the pixel capture (Stop)      **/
/**                                                         **/
/*************************************************************/
void pixel_capture_display_stop(void)
{
  capture_thread_stop(CaptureContext);

  if (!info) {
    /* free capture buffers */
    free_capture_buffers_memory(&memory_pool);
  }

  if(!use_fb)
  {
    free_main_display_ressources(pDisplayContext);
  }

  /* close capture handle */
  stm_pixel_capture_close(CaptureContext[0].pixel_capture);

  /* clear internal test data */
  memset(CaptureContext, 0, sizeof(CaptureContext));
  memset(&memory_pool, 0, sizeof(struct stm_capture_dma_area));

#ifdef HDMIRX_MODE_DETECTION_TEST
  hdmirx_mode_detection_test_stop();
#endif

  printk(KERN_INFO "\n");
  printk(KERN_INFO "*************************************************\n");
  printk(KERN_INFO "*** Capture Thread stopped.                   ***\n");
  printk(KERN_INFO "*************************************************\n");
  printk(KERN_INFO "\n");
}
EXPORT_SYMBOL(pixel_capture_display_stop);
