/*
 * fill-test.c
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

#define BLIT_TEST_AUTHOR "Akram BEN BELGACEM <akram.ben-belgacem@st.com>"
#define BLIT_TEST_DESC   "A sample blit rectangles test operations using the blitter driver"

struct fb_info       *info;

/* Test module parameters */
static char *file_name1 = "/root/image1280x720r.gam";
static char *file_name2 = "/root/image1280x720.gam";
static char *file_name3 = "/root/ftb_420_R2B.gam";

module_param(file_name1, charp, 0000);
MODULE_PARM_DESC(file_name1, "Source1 GAM File Name");

module_param(file_name2, charp, 0000);
MODULE_PARM_DESC(file_name2, "Source2 GAM File Name");

module_param(file_name3, charp, 0000);
MODULE_PARM_DESC(file_name3, "Destination GAM File Name");

#define TEST_BLIT_RECT_WIDHT       120
#define TEST_BLIT_RECT_HEIGHT       60

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
  stm_blitter_surface_blitflags_t   flags   = STM_BLITTER_SBF_NONE;
  stm_blitter_rect_t                src_rects   = { { 100, 50 }, { 200, 200 } };
  stm_blitter_point_t               dst_points  = { 0, 0 };
  stm_blitter_serial_t              serial;
  stm_blitter_rect_t               *blit_rects  = NULL;
  stm_blitter_point_t              *blit_points = NULL;
  stm_blitter_color_t              *color_p     = NULL;
  bitmap_t Src1;
  bitmap_t Src2;
  bitmap_t Dest;
  int                               i=0, k=0;

  printk(KERN_INFO "Starting Blit tests\n");

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

  /* Setup blit flags in some particular use case (permultiplied surf maybe?) */
  if(stm_blitter_surface_set_blitflags(dest, flags) < 0)
  {
    printk(KERN_ERR "Failed to setup blit flags for the surface !\n");
    stm_blitter_surface_put(src1);
    stm_blitter_surface_put(src2);
    stm_blitter_surface_put(dest);
    stm_blitter_put(blitter);
    goto out_surface;
  }
  color_p->a = 0xff;
  color_p->r = 0xff;
  color_p->g = 0xff;
  color_p->b = 0xff;
  stm_blitter_surface_clear (blitter, dest, color_p);

  printk(KERN_INFO "Starting Blit tests..\n");
  src_rects.position.x  = dst_points.x = 0;
  src_rects.position.y  = dst_points.y = 0;
  src_rects.size.w  = Dest.size.w;
  src_rects.size.h  = Dest.size.h;

  /* Simple Blit ops */
  if(stm_blitter_surface_blit(blitter, src1, &src_rects, dest, &dst_points, 1) < 0)
  {
    printk(KERN_ERR "Simple Blit ops failed!\n");
    stm_blitter_surface_put(src1);
    stm_blitter_surface_put(src2);
    stm_blitter_surface_put(dest);
    stm_blitter_put(blitter);
    goto out_surface;
  }

  flags   = STM_BLITTER_SBF_BLEND_ALPHACHANNEL | STM_BLITTER_SBF_SRC_PREMULTIPLY;
  /* Setup blit flags in some particular use case (permultiplied surf maybe?) */
  if(stm_blitter_surface_set_blitflags(dest, flags) < 0)
  {
    printk(KERN_ERR "Failed to setup blit flags for the surface !\n");
    stm_blitter_surface_put(src1);
    stm_blitter_surface_put(src2);
    stm_blitter_surface_put(dest);
    stm_blitter_put(blitter);
    goto out_surface;
  }

  src_rects.position.x  = 100;
  src_rects.position.y  = 100;
  src_rects.size.w  = 400;
  src_rects.size.h  = 300;
  dst_points.x = 300;
  dst_points.y = 400;
  /* Simple Blit ops */
  if(stm_blitter_surface_blit(blitter, src2, &src_rects, dest, &dst_points, 1) < 0)
  {
    printk(KERN_ERR "Simple Blit ops failed!\n");
    stm_blitter_surface_put(src1);
    stm_blitter_surface_put(src2);
    stm_blitter_surface_put(dest);
    stm_blitter_put(blitter);
    goto out_surface;
  }

  flags   = STM_BLITTER_SBF_BLEND_ALPHACHANNEL;
  /* Setup blit flags in some particular use case (permultiplied surf maybe?) */
  if(stm_blitter_surface_set_blitflags(dest, flags) < 0)
  {
    printk(KERN_ERR "Failed to setup blit flags for the surface !\n");
    stm_blitter_surface_put(src1);
    stm_blitter_surface_put(src2);
    stm_blitter_surface_put(dest);
    stm_blitter_put(blitter);
    goto out_surface;
  }

  src_rects.position.x  = 0;
  src_rects.position.y  = 0;
  src_rects.size.w  = 400;
  src_rects.size.h  = 300;
  dst_points.x = 800;
  dst_points.y = 350;
  /* Simple Blit ops */
  if(stm_blitter_surface_blit(blitter, src2, &src_rects, dest, &dst_points, 1) < 0)
  {
    printk(KERN_ERR "Simple Blit ops failed!\n");
    stm_blitter_surface_put(src1);
    stm_blitter_surface_put(src2);
    stm_blitter_surface_put(dest);
    stm_blitter_put(blitter);
    goto out_surface;
  }

  flags   = STM_BLITTER_SBF_NONE;
  /* Setup blit flags in some particular use case (permultiplied surf maybe?) */
  if(stm_blitter_surface_set_blitflags(dest, flags) < 0)
  {
    printk(KERN_ERR "Failed to setup blit flags for the surface !\n");
    stm_blitter_surface_put(src1);
    stm_blitter_surface_put(src2);
    stm_blitter_surface_put(dest);
    stm_blitter_put(blitter);
    goto out_surface;
  }
  save_gam_file("/root/dumped_dest_t1.gam", &Dest);

  printk(KERN_INFO "Starting batch Blit tests..\n");

  /* Allocate memory for blit rects */
  blit_rects = kmalloc((Dest.size.h/TEST_BLIT_RECT_HEIGHT)*sizeof(stm_blitter_rect_t), GFP_KERNEL);
  if(!blit_rects)
  {
    printk(KERN_ERR "Failed to allocate memory!\n");
    goto out_surface;
  }

  blit_points = kmalloc((Dest.size.h/TEST_BLIT_RECT_HEIGHT)*sizeof(stm_blitter_point_t), GFP_KERNEL);
  if(!blit_points)
  {
    printk(KERN_ERR "Failed to allocate memory!\n");
    goto out_surface;
  }

  color_p->a = 0xff;
  color_p->r = 0x00;
  color_p->g = 0x00;
  color_p->b = 0xff;
  stm_blitter_surface_clear (blitter, dest, color_p);

  for (i = 30;i < (Dest.size.h-TEST_BLIT_RECT_HEIGHT); i+=TEST_BLIT_RECT_HEIGHT)
  {
    int nb_ops = 0;
    for (k = 100; k < (Dest.size.w-TEST_BLIT_RECT_WIDHT); k+=TEST_BLIT_RECT_WIDHT)
    {
      blit_rects[k/TEST_BLIT_RECT_WIDHT].size.w = TEST_BLIT_RECT_WIDHT-10;
      blit_rects[k/TEST_BLIT_RECT_WIDHT].size.h = TEST_BLIT_RECT_HEIGHT-10;
      blit_rects[k/TEST_BLIT_RECT_WIDHT].position.x = Dest.size.w - (TEST_BLIT_RECT_WIDHT + k + 10);
      blit_rects[k/TEST_BLIT_RECT_WIDHT].position.y = Dest.size.h - (TEST_BLIT_RECT_HEIGHT + i + 10);
      blit_points[k/TEST_BLIT_RECT_WIDHT].x = k;
      blit_points[k/TEST_BLIT_RECT_WIDHT].y = i;
      printk("Blit Rectangle --> (%d,%d-%dx%d -> %d,%d)\n",
              (int)blit_rects[k/TEST_BLIT_RECT_WIDHT].position.x,
              (int)blit_rects[k/TEST_BLIT_RECT_WIDHT].position.y,
              (int)blit_rects[k/TEST_BLIT_RECT_WIDHT].size.w,
              (int)blit_rects[k/TEST_BLIT_RECT_WIDHT].size.h,
              (int)blit_points[k/TEST_BLIT_RECT_WIDHT].x,
              (int)blit_points[k/TEST_BLIT_RECT_WIDHT].y);
      nb_ops++;
    }

    if(stm_blitter_surface_blit(blitter, src2, blit_rects, dest, blit_points, nb_ops) < 0)
    {
      printk(KERN_ERR "Batch Blit ops failed!\n");
      stm_blitter_surface_put(src1);
      stm_blitter_surface_put(src2);
      stm_blitter_surface_put(dest);
      stm_blitter_put(blitter);
      goto out_surface;
    }
  }

  /* wait for blitter to have finished all operations on this surface. */
  stm_blitter_surface_get_serial (dest, &serial);
  stm_blitter_wait (blitter, STM_BLITTER_WAIT_SERIAL, serial);

  save_gam_file("/root/dumped_dest_t2.gam", &Dest);

  color_p->a = 0xff;
  color_p->r = 0x00;
  color_p->g = 0x00;
  color_p->b = 0xff;
  stm_blitter_surface_clear (blitter, dest, color_p);

  for (i = 30;i < (Dest.size.h-TEST_BLIT_RECT_HEIGHT); i+=TEST_BLIT_RECT_HEIGHT)
  {
    int nb_ops = 0;
    for (k = 100; k < (Dest.size.w-TEST_BLIT_RECT_WIDHT); k+=TEST_BLIT_RECT_WIDHT)
    {
      blit_rects[k/TEST_BLIT_RECT_WIDHT].size.w = TEST_BLIT_RECT_WIDHT-10;
      blit_rects[k/TEST_BLIT_RECT_WIDHT].size.h = TEST_BLIT_RECT_HEIGHT-10;
      blit_rects[k/TEST_BLIT_RECT_WIDHT].position.x = k + 10;
      blit_rects[k/TEST_BLIT_RECT_WIDHT].position.y = i + 10;
      blit_points[k/TEST_BLIT_RECT_WIDHT].x = k;
      blit_points[k/TEST_BLIT_RECT_WIDHT].y = i;
      printk("Blit Rectangle --> (%d,%d-%dx%d -> %d,%d)\n",
              (int)blit_rects[k/TEST_BLIT_RECT_WIDHT].position.x,
              (int)blit_rects[k/TEST_BLIT_RECT_WIDHT].position.y,
              (int)blit_rects[k/TEST_BLIT_RECT_WIDHT].size.w,
              (int)blit_rects[k/TEST_BLIT_RECT_WIDHT].size.h,
              (int)blit_points[k/TEST_BLIT_RECT_WIDHT].x,
              (int)blit_points[k/TEST_BLIT_RECT_WIDHT].y);
      nb_ops++;
    }

    if(stm_blitter_surface_blit(blitter, src2, blit_rects, dest, blit_points, nb_ops) < 0)
    {
      printk(KERN_ERR "Batch Blit ops failed!\n");
      stm_blitter_surface_put(src1);
      stm_blitter_surface_put(src2);
      stm_blitter_surface_put(dest);
      stm_blitter_put(blitter);
      goto out_surface;
    }
  }

  /* wait for blitter to have finished all operations on this surface. */
  stm_blitter_surface_get_serial (dest, &serial);
  stm_blitter_wait (blitter, STM_BLITTER_WAIT_SERIAL, serial);

  save_gam_file("/root/dumped_dest_t3.gam", &Dest);

  /* Free the rects memory */
  kfree(blit_rects);
  kfree(blit_points);
  save_gam_file("/root/dumped_src1.gam", &Src1);
  save_gam_file("/root/dumped_src2.gam", &Src2);

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

  printk(KERN_INFO "Blit tests finished.\n");
  return 0;

out_surface:
#ifndef LOAD_DESTINATION_GAMFILE
  if (!info && bpa2 && Dest.buffer_address.base)
    bpa2_free_pages (bpa2, Dest.buffer_address.base);
#endif
  printk(KERN_INFO "Blit tests failed!\n");
  return -1;
}

static void __exit blit_test_exit(void)
{
	printk(KERN_INFO " Blit tests module removed.\n");
}

module_init(blit_test_init);
module_exit(blit_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(BLIT_TEST_AUTHOR);
MODULE_DESCRIPTION(BLIT_TEST_DESC);
