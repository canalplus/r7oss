/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixelcapturetest/capture_test.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif

#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_INFO */
#include <linux/slab.h>   /* Needed for kmalloc and kfree */
#include <asm/io.h>

#include <linux/bpa2.h>
#include <linux/fb.h>
#include <linux/io.h>

#include <stm_pixel_capture.h>

/*#include <capture.h>*/

static int   cap_type      = 0;

module_param(cap_type, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(cap_type, "Set the capture device type : 0 = COMPO | 1 = FVDP | 2 = DVP");

extern int  pixel_capture_display_start(uint32_t number_buffers, stm_pixel_capture_device_type_t type);
extern void pixel_capture_display_stop(void);

/* Simple test case to capture main mixer content to memory */

static int __init stm_capturetest_init(void)
{
  struct fb_info *info = NULL;
  stm_pixel_capture_device_type_t  type;
  stm_pixel_capture_buffer_descr_t c_buffer;
  stm_pixel_capture_h pixel_capture = NULL;
  const char *input_name;
  uint32_t input_id = 0;
  int res=0;

  memset(&c_buffer, 0, sizeof(stm_pixel_capture_buffer_descr_t));

  switch (cap_type)
  {
    case 1:
      type = STM_PIXEL_CAPTURE_FVDP;
      break;
    case 2:
      type = STM_PIXEL_CAPTURE_DVP;
      break;
    case 0:
    default:
      type = STM_PIXEL_CAPTURE_COMPO;
      break;
  }

  /* Now get the Aux display buffer to be used by capture */
  res = num_registered_fb - 1;
  if (registered_fb[res])
  {
    info = registered_fb[res];
    printk ("found a registered framebuffer @ %p\n", info);
  }

  if (info)
  {
    struct module *owner = info->fbops->owner;

    if (try_module_get (owner))
    {
      printk ("try_module_get\n");
      if (info->fbops->fb_open && !info->fbops->fb_open (info, 0))
      {
        switch (info->var.bits_per_pixel)
        {
          case 16: c_buffer.cap_format.format = STM_PIXEL_FORMAT_RGB565; break;
          case 24: c_buffer.cap_format.format = STM_PIXEL_FORMAT_RGB888; break;
          case 32: c_buffer.cap_format.format = STM_PIXEL_FORMAT_ARGB8888; break;
          default:
            if (info->fbops->fb_release)
              info->fbops->fb_release (info, 0);
            module_put (owner);
            info = NULL;
            break;
        }

        if ((c_buffer.cap_format.format != STM_PIXEL_FORMAT_NONE) && ( info != NULL ))
        {
          c_buffer.rgb_address = info->fix.smem_start;
          /*buffer_size = info->fix.smem_len;*/
          c_buffer.cap_format.stride = info->var.xres * info->var.bits_per_pixel / 8;
          c_buffer.cap_format.width = info->var.xres;
          c_buffer.cap_format.height = info->var.yres_virtual;
          c_buffer.bytesused = c_buffer.cap_format.stride * c_buffer.cap_format.height;
          c_buffer.length = c_buffer.cap_format.stride * c_buffer.cap_format.height;
          printk ("fb: %x %u %u %u %u\n", c_buffer.rgb_address, c_buffer.bytesused, c_buffer.cap_format.stride,
                   c_buffer.cap_format.width, c_buffer.cap_format.height);
        }
      }
      else
      {
        printk ("no fb_open()\n");
        info = NULL;
      }
      module_put (owner);
    }
    else
    {
      printk ("failed try_module_get\n");
      info = NULL;
    }
  }

  if(info == NULL)
  {
    printk ("Failed to Start Pixel Capture tests...\n");
    return -EINVAL;
  }

  printk ("Starting Pixel Capture STKPI tests...\n");

  /* STKPI Basic test */
  res = stm_pixel_capture_open(type, 0, &pixel_capture);
  if(res < 0)
    printk ("ERROR : can't get a pixel capture handle with id = %d | err = %d\n",0, res);

  printk("stm_pixel_capture_set_input --> %d\n",input_id);
  res = stm_pixel_capture_set_input(pixel_capture, input_id);
  if(res < 0)
    printk ("ERROR : can't set a capture input with id = %d | err = %d\n",input_id, res);

  res = stm_pixel_capture_get_input(pixel_capture, &input_id);
  if(res < 0)
    printk ("ERROR : can't get a capture input with id = %d | err = %d\n",input_id, res);
  printk("stm_pixel_capture_get_input <-- %d\n",input_id);

  res = stm_pixel_capture_enum_inputs(pixel_capture, input_id, &input_name);
  if(res < 0)
    printk ("ERROR : can't get a capture input name with id = %d | err = %d\n",input_id, res);

  printk("Capture Input %d name is (%s)\n",input_id, input_name);

  stm_pixel_capture_close(pixel_capture);

  res = pixel_capture_display_start(0, type);
  if(res < 0)
    printk ("ERROR : can't execute extra tests err = %d\n", res);

  printk(KERN_INFO "Pixel Capture Test Display Started err = %d\n", res);
  return res;
}

static void __exit stm_capturetest_exit(void)
{
  printk ("Unloading Pixel Capture Test Module...\n");
  pixel_capture_display_stop();
}

/******************************************************************************
 *  Modularization
 */

#ifdef MODULE
module_init(stm_capturetest_init);
module_exit(stm_capturetest_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Pixel Capture Test Module.");
MODULE_AUTHOR("Akram BEN BELGACEM <akram.ben-belgacem@st.com>");
#endif /* MODULE */
