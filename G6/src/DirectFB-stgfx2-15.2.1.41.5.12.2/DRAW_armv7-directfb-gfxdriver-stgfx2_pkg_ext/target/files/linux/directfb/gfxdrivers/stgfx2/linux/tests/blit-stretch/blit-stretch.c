/*
 * blit-stretch.c
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * Test to exercise Blit Stretch Operations using the blitter hw
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

#define BLIT_TEST_AUTHOR "Akram BEN BELGACEM <akram.ben-belgacem@st.com>"
#define BLIT_TEST_DESC   "A sample blit stretch test using the blitter driver"

struct fb_info       *info;

/* Test module parameters */
static char *file_name1 = "/root/image1280x720r.gam";
static char *file_name2 = "/root/image1280x720.gam";

module_param(file_name1, charp, 0000);
MODULE_PARM_DESC(file_name1, "Source1 GAM File Name");

module_param(file_name2, charp, 0000);
MODULE_PARM_DESC(file_name2, "Source2 GAM File Name");

static int __init blit_test_init(void)
{
#ifndef LOAD_DESTINATION_GAMFILE
  int res;
  struct bpa2_part                 *bpa2 = NULL;
#endif
  stm_blitter_t                    *blitter = NULL;
  stm_blitter_surface_t            *src1    = NULL;
  stm_blitter_surface_t            *src2    = NULL;
  stm_blitter_surface_t            *dest    = NULL;
  stm_blitter_color_t              *color_p = NULL;
  stm_blitter_rect_t                src_rects   = { { 0, 0 }, { 1280, 720 } };
  stm_blitter_rect_t                dst_rects   = { { 100, 100 }, { 640, 360 } };
  stm_blitter_serial_t              serial;
  bitmap_t Src1;
  bitmap_t Src2;
  bitmap_t Dest;
  int i,j;

  printk(KERN_INFO "Starting tests\n");

  printk(KERN_INFO "Loading Source1 Gam file...\n");
  if(load_gam_file( file_name1, &Src1) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }

  printk(KERN_INFO "Loading Source2 Gam file...\n");
  if(load_gam_file( file_name2, &Src2) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }
#ifdef LOAD_DESTINATION_GAMFILE
  printk(KERN_INFO "Loading Destination Gam file...\n");
  if(load_gam_file( file_name3, &Dest) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }
#else
  for (res = 0; res < num_registered_fb; ++res)
  {
    if (registered_fb[res])
      {
        info = registered_fb[res];
        printk ("found a registered framebuffer @ %p\n", info);
        break;
      }
  }

  Dest.size.w = 1024;
  Dest.size.h = 768;

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
              case  8: Dest.format = STM_BLITTER_SF_LUT8; break;
              case 16: Dest.format = STM_BLITTER_SF_RGB565/*STM_BLITTER_SF_UYVY*/; break;
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
            /*Dest.buffer_size = info->fix.smem_len;*/
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
    else
      {
        printk ("failed try_module_get\n");
        info = NULL;
      }
  }

  if (!info)
  {
    bpa2 = bpa2_find_part ("bigphysarea");
    if (!bpa2)
    {
      printk ("%s: no bpa2 partition 'bigphysarea'\n", __func__);
      res = -ENOENT;
      return res;
    }

    printk ("using memory from BPA2\n");
    Dest.format = STM_BLITTER_SF_ARGB;
    Dest.pitch = (Dest.size.w + 5) * 4;
    Dest.buffer_size = Dest.pitch * (Dest.size.h + 7);
    Dest.buffer_address.base = bpa2_alloc_pages (bpa2,
                                                 ((Dest.buffer_size
                                                   + PAGE_SIZE - 1)
                                                  / PAGE_SIZE),
                                                 PAGE_SIZE, GFP_KERNEL);
    if (!Dest.buffer_address.base)
    {
      printk ("%s: could not allocate %zu bytes from bpa2 partition\n",
              __func__, Dest.buffer_size);
      res = -ENOMEM;
      return res;
    }
    printk ("%s: surface @ %.8lx\n", __func__, Dest.buffer_address.base);
  }
  Dest.memory = phys_to_virt(Dest.buffer_address.base);
#endif

  /* Allocate memory for color data */
  color_p = (stm_blitter_color_t *)kmalloc((size_t)sizeof(stm_blitter_color_t), GFP_KERNEL);
  if(!color_p)
  {
    printk(KERN_ERR "Failed to allocate memory!\n");
    return -ENXIO; /* No such device or address */
  }

  /* Get a blitter handle */
  if((blitter = stm_blitter_get(0)) == NULL)
  {
    printk(KERN_ERR "Failed to get a blitter device!\n");
    return -ENXIO; /* No such device or address */
  }

  /* Create a new surface for source1 */
  if((src1 = stm_blitter_surface_new_preallocated(Src1.format,Src1.colorspace,&Src1.buffer_address,Src1.buffer_size,&Src1.size,Src1.pitch)) == NULL)
  {
    printk(KERN_ERR "Failed to get a create new surface for src1!\n");
    stm_blitter_put(blitter);
    goto out_surface;
  }

  /* Create a new surface for source2 */
  if((src2 = stm_blitter_surface_new_preallocated(Src2.format,Src2.colorspace,&Src2.buffer_address,Src2.buffer_size,&Src2.size,Src2.pitch)) == NULL)
  {
    printk(KERN_ERR "Failed to get a create new surface for src2!\n");
    stm_blitter_put(blitter);
    goto out_surface;
  }

  /* Create a new surface for destination */
  if((dest = stm_blitter_surface_new_preallocated(Dest.format,Dest.colorspace,&Dest.buffer_address,Dest.buffer_size,&Dest.size,Dest.pitch)) == NULL)
  {
    printk(KERN_ERR "Failed to get a create new surface for dest!\n");
    stm_blitter_put(blitter);
    goto out_surface;
  }

  /* clear the destination surface */
  color_p->a = 0xff;
  color_p->r = 0xff;
  color_p->g = 0xff;
  color_p->b = 0xff;
  stm_blitter_surface_clear (blitter, dest, color_p);
  printk(KERN_INFO "Starting Blit Stretch tests..\n");

  for (i = 50; i < (Dest.size.w -70); i+=10)
  {
    for (j = 50; j < (Dest.size.h -70); j+=10)
    {
      dst_rects.size.w = i;
      dst_rects.size.h = j;
      if(stm_blitter_surface_stretchblit(blitter, src1, &src_rects, dest, &dst_rects, 1) < 0)
      {
        printk(KERN_ERR "Blit ops failed!\n");
        stm_blitter_surface_put(src1);
        stm_blitter_surface_put(src2);
        stm_blitter_surface_put(dest);
        stm_blitter_put(blitter);
        goto out_surface;
      }
    }
    stm_blitter_surface_clear (blitter, dest, color_p);
  }

  /* wait for blitter to have finished all operations on this surface. */
  stm_blitter_surface_get_serial (dest, &serial);
  stm_blitter_wait (blitter, STM_BLITTER_WAIT_SERIAL, serial);

  save_gam_file("/root/dumped_dest_stretch_ts", &Dest);

  /* Release the surface */
  stm_blitter_surface_put(src1);
  stm_blitter_surface_put(src2);
  stm_blitter_surface_put(dest);

  /* Release the blitter */
  stm_blitter_put(blitter);

  free_gam_file(&Src1);
  free_gam_file(&Src2);
#ifdef LOAD_DESTINATION_GAMFILE
  free_gam_file(&Dest);
#else
  if (!info && bpa2 && Dest.buffer_address.base)
    bpa2_free_pages (bpa2, Dest.buffer_address.base);
#endif

  /* Free the color data memory */
  if(!color_p)
    kfree((void *)color_p);

  printk(KERN_INFO " Blit Stretch tests finished.\n");
  return 0;

out_surface:
#ifndef LOAD_DESTINATION_GAMFILE
  if (!info && bpa2 && Dest.buffer_address.base)
    bpa2_free_pages (bpa2, Dest.buffer_address.base);
#endif
  printk(KERN_INFO "Blit Stretch tests failed!\n");
  return -1;
}

static void __exit blit_test_exit(void)
{
  printk(KERN_INFO " Blit Stretch tests module removed.\n");
}

module_init(blit_test_init);
module_exit(blit_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(BLIT_TEST_AUTHOR);
MODULE_DESCRIPTION(BLIT_TEST_DESC);
