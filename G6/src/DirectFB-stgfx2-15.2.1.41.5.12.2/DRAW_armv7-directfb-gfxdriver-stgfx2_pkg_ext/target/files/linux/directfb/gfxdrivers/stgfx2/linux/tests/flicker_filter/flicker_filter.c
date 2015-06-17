/*
 * flicker_filter.c
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * Test to exercise Blit Operations using the blitter hw
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/stm/blitter.h>
#include <linux/bpa2.h>
#include <linux/fb.h>

#include <gam_utils.h>

#define BLIT_TEST_AUTHOR "Riadh BEN AMOR <riadh.ben-amor@st.com>"
#define BLIT_TEST_DESC   "Blit tests for flicker filter operation"

/* Test module parameters */
static char *file_name = "/root/blackWhite.gam";
struct fb_info       *info;

module_param(file_name, charp, 0000);
MODULE_PARM_DESC(file_name, "Source2 GAM File Name");

int stm_blit_test_function(stm_blitter_t     * blitter,
                           bitmap_t            Src2,
                           bitmap_t            Dest,
                           stm_blitter_rect_t  src_rects,
                           stm_blitter_rect_t  dst_rects,
                           stm_blitter_point_t dst_points,
                           bool                stretch_op,
                           bool                clear,
                           stm_blitter_surface_blitflags_t flags)
{
  stm_blitter_surface_t            *src2  = NULL;
  stm_blitter_surface_t            *dest  = NULL;
  stm_blitter_color_t               color = { .a = 0xff, .r = 0x00, .g = 0x00, .b = 0xff };
  stm_blitter_serial_t              serial;

  /* Create a new surface for source2 */
  if((src2 = stm_blitter_surface_new_preallocated(Src2.format,Src2.colorspace,&Src2.buffer_address,Src2.buffer_size,&Src2.size,Src2.pitch)) == NULL)
  {
    printk(KERN_ERR "Failed to get a create new surface for src2!\n");
    stm_blitter_put(blitter);
    return -1;
  }

  /* Create a new surface for destination */
  if((dest = stm_blitter_surface_new_preallocated(Dest.format,Dest.colorspace,&Dest.buffer_address,Dest.buffer_size,&Dest.size,Dest.pitch)) == NULL)
  {
    printk(KERN_ERR "Failed to get a create new surface for dest!\n");
    stm_blitter_put(blitter);
    return -1;
  }

 /* Setup blit flags in some particular use case (permultiplied surf maybe?) */
  if(stm_blitter_surface_set_blitflags(dest, flags) < 0)
  {
    printk(KERN_ERR "Failed to setup blit flags for the surface !\n");
    stm_blitter_surface_put(src2);
    stm_blitter_surface_put(dest);
    stm_blitter_put(blitter);
    return -1;
  }

  if (clear)
    stm_blitter_surface_clear (blitter, dest, &color);

  stm_blitter_surface_set_ff_mode(dest, STM_BLITTER_FF_ADAPTATIVE | STM_BLITTER_FF_FIELD_NOT_FRAME);
  //~ stm_blitter_surface_set_ff_mode(dest, STM_BLITTER_FF_FIELD_NOT_FRAME);
  //~ stm_blitter_surface_set_ff_mode(dest, STM_BLITTER_FF_ADAPTATIVE);
  //~ stm_blitter_surface_set_ff_mode(dest, STM_BLITTER_FF_SIMPLE);

  if (stretch_op == true)
  {
    if(stm_blitter_surface_stretchblit(blitter, src2, NULL, dest, &dst_rects, 1) < 0)
    {
      printk(KERN_ERR "Simple Blit ops failed!\n");
      stm_blitter_surface_put(src2);
      stm_blitter_surface_put(dest);
      stm_blitter_put(blitter);
      return -1;
    }
  }
  else
  {
    if(stm_blitter_surface_blit(blitter, src2, NULL, dest, &dst_points, 1) < 0)
    {
      printk(KERN_ERR "Simple Blit ops failed!\n");
      stm_blitter_surface_put(src2);
      stm_blitter_surface_put(dest);
      stm_blitter_put(blitter);
      return -1;
    }
  }
  /* wait for the blitter to have finished all operations on this surface. */
  stm_blitter_surface_get_serial(dest, &serial);
  stm_blitter_wait(blitter, STM_BLITTER_WAIT_SERIAL, serial);

  stm_blitter_surface_put(dest);
  stm_blitter_surface_put(src2);

  return 0;
}

static int __init blit_mb_dst_init(void)
{
  int res;
  struct bpa2_part                 *bpa2 = NULL;

  stm_blitter_t      *blitter    = NULL;
  stm_blitter_rect_t  src_rects  = { { 30, 50 }, { 500, 400 } };
  stm_blitter_rect_t  dst_rects  = { { 50, 50 }, { 720, 600 } };
  stm_blitter_point_t dst_points = { 50, 50 };
  bitmap_t            Src2;
  bitmap_t            Dest;
  bool                stretch_op = false;

  printk(KERN_INFO "Starting Blit tests\n");

  /* Get a blitter handle */
  if((blitter = stm_blitter_get(0)) == NULL)
  {
    printk(KERN_ERR "Failed to get a blitter device!\n");
    return -ENXIO; /* No such device or address */
  }

  for (res = 0; res < 2; ++res)
  {
    if (registered_fb[res])
      {
        info = registered_fb[res];
        printk ("found a registered framebuffer @ %p\n", info);
        break;
      }
  }

  Dest.size.w = 1280;
  Dest.size.h = 720;

  if (info)
  {
    struct module *owner = info->fbops->owner;

    if (try_module_get (owner))
      {
        printk ("try_module_get\n");
        if (info->fbops->fb_open && !info->fbops->fb_open (info, 1))
          {
            switch (info->var.bits_per_pixel)
              {
              case  8: Dest.format = STM_BLITTER_SF_LUT8; break;
              case 16: Dest.format = STM_BLITTER_SF_RGB565; break;
              case 24: Dest.format = STM_BLITTER_SF_RGB24; break;
              case 32: Dest.format = STM_BLITTER_SF_ARGB; break;
              default:
                if (info->fbops->fb_release)
                  info->fbops->fb_release (info, 0);
                module_put (owner);
                info = NULL;
                break;
              }

            Dest.buffer_address.base = info->fix.smem_start;
            Dest.pitch = info->var.xres * info->var.bits_per_pixel / 8;
            Dest.size.w = info->var.xres;
            Dest.size.h = info->var.yres_virtual;
            Dest.buffer_size = Dest.pitch * Dest.size.h;
            printk ("fb: %lx %zu %ld\n", Dest.buffer_address.base, Dest.buffer_size, Dest.pitch);
          }
        else
          {
            module_put (owner);
            printk ("no fb_open()\n");
            info = NULL;
          }
      }
  }

  Dest.memory = phys_to_virt(Dest.buffer_address.base);

  printk(KERN_INFO "Loading Source2 Gam file...\n");
  if(load_gam_file( file_name, &Src2) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }

  if (stm_blit_test_function(blitter, Src2, Dest, src_rects, dst_rects, dst_points, stretch_op, true, STM_BLITTER_SBF_FLICKER_FILTER) < 0)
  {
    goto out_surface;
  }

  /* Release the surface */
  free_gam_file(&Src2);

  /* Release the blitter */
  stm_blitter_put(blitter);

  printk(KERN_INFO "Blit tests finished.\n");
  return 0;

out_surface:
  if (bpa2 && Dest.buffer_address.base)
    bpa2_free_pages (bpa2, Dest.buffer_address.base);

  printk(KERN_INFO "Blit tests failed!\n");
  return -1;
}

static void __exit blit_mb_dst_exit(void)
{
    printk(KERN_INFO " Blit tests module removed.\n");
}

module_init(blit_mb_dst_init);
module_exit(blit_mb_dst_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(BLIT_TEST_AUTHOR);
MODULE_DESCRIPTION(BLIT_TEST_DESC);

