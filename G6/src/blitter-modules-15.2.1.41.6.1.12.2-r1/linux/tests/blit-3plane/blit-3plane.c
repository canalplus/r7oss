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

#define BLIT_TEST_AUTHOR "Mohamed Ahmed LAOURINE <mohamed-ahmed.laourine@st.com>"
#define BLIT_TEST_DESC   "A sample blit rectangles test operations using 3 planes format"

struct fb_info       *info;

/* Test module parameters */
static char *file_name1 = "/root/image1280x720.gam";

module_param(file_name1, charp, 0000);
MODULE_PARM_DESC(file_name1, "Source1 GAM File Name");

#define TEST_BLIT_RECT_WIDHT       120
#define TEST_BLIT_RECT_HEIGHT       60

static int __init blit_3plane_init(void)
{
  int res;
  struct bpa2_part                 *bpa2 = NULL;

  stm_blitter_t                    *blitter = NULL;
  stm_blitter_surface_t            *src     = NULL;
  stm_blitter_surface_t            *dest    = NULL;
  stm_blitter_rect_t                src_rects   = { { 100, 50 }, { 200, 200 } };
  stm_blitter_point_t               dst_points  = { 0, 0 };
  stm_blitter_serial_t              serial;
  stm_blitter_color_t              *color_p     = NULL;
  bitmap_t Src;
  bitmap_t Dest;

  printk(KERN_INFO "Starting Blit tests\n");

  printk(KERN_INFO "Loading Source1 Gam file...\n");
  if(load_gam_file( file_name1, &Src) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }

  Dest.size.w = 1024;
  Dest.size.h = 768;

  bpa2 = bpa2_find_part ("bigphysarea");
  if (!bpa2)
  {
    printk ("%s: no bpa2 partition 'bigphysarea'\n", __func__);
    res = -ENOENT;
    return res;
  }

  printk ("using memory from BPA2\n");
  Dest.format = STM_BLITTER_SF_I420;
  Dest.pitch = Dest.size.w;
  Dest.buffer_size = Dest.pitch * Dest.size.h * 3/2;
  Dest.buffer_address.base = bpa2_alloc_pages (bpa2,
                                               ((Dest.buffer_size
                                                 + PAGE_SIZE - 1)
                                                / PAGE_SIZE),
                                               PAGE_SIZE, GFP_KERNEL);
  Dest.buffer_address.cb_offset = Dest.pitch * Dest.size.h;
  Dest.buffer_address.cr_offset = Dest.pitch * Dest.size.h + Dest.pitch * Dest.size.h * 1/4;

  if (!Dest.buffer_address.base)
  {
    printk ("%s: could not allocate %zu bytes from bpa2 partition\n",
            __func__, Dest.buffer_size);
    res = -ENOMEM;
    return res;
  }
  printk ("%s: surface @ %.8lx\n", __func__, Dest.buffer_address.base);

  Dest.memory = phys_to_virt(Dest.buffer_address.base);

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

  /* Create a new surface for source */
  if((src = stm_blitter_surface_new_preallocated(Src.format,Src.colorspace,&Src.buffer_address,Src.buffer_size,&Src.size,Src.pitch)) == NULL)
  {
    printk(KERN_ERR "Failed to get a create new surface for src!\n");
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

  color_p->a = 0xff;
  color_p->r = 0x00;
  color_p->g = 0x00;
  color_p->b = 0xff;
  /* Color fill on 3Plane */
  stm_blitter_surface_clear (blitter, dest, color_p);

  printk(KERN_INFO "Starting Blit tests..\n");
  src_rects.position.x  = dst_points.x = 0;
  src_rects.position.y  = dst_points.y = 0;
  src_rects.size.w  = Src.size.w;
  src_rects.size.h  = Src.size.h;

  /* Conversion from 1Planes to 3Plane */
  if(stm_blitter_surface_blit(blitter, src, &src_rects, dest, &dst_points, 1) < 0)
  {
    printk(KERN_ERR "Simple Blit ops failed!\n");
    stm_blitter_surface_put(src);
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

  /* Conversion from 1Planes to 3Plane */
  if(stm_blitter_surface_blit(blitter, src, &src_rects, dest, &dst_points, 1) < 0)
  {
    printk(KERN_ERR "Simple Blit ops failed!\n");
    stm_blitter_surface_put(src);
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

  /* Conversion from 1Planes to 3Plane */
  if(stm_blitter_surface_blit(blitter, src, &src_rects, dest, &dst_points, 1) < 0)
  {
    printk(KERN_ERR "Simple Blit ops failed!\n");
    stm_blitter_surface_put(src);
    stm_blitter_surface_put(dest);
    stm_blitter_put(blitter);
    goto out_surface;
  }

  /* Copy from 3Planes to 3Plane */
  if(stm_blitter_surface_blit(blitter, dest, &src_rects, dest, &dst_points, 1) < 0)
  {
    printk(KERN_ERR "Simple Blit ops failed!\n");
    stm_blitter_surface_put(src);
    stm_blitter_surface_put(dest);
    stm_blitter_put(blitter);
    goto out_surface;
  }

  /* Conversion from 3Planes to 1Plane */
  if(stm_blitter_surface_stretchblit(blitter, dest, NULL, src, NULL, 1) < 0)
  {
    printk(KERN_ERR "Simple Blit ops failed!\n");
    stm_blitter_surface_put(src);
    stm_blitter_surface_put(dest);
    stm_blitter_put(blitter);
    goto out_surface;
  }

  /* wait for blitter to have finished all operations on this surface. */
  stm_blitter_surface_get_serial (dest, &serial);
  stm_blitter_wait (blitter, STM_BLITTER_WAIT_SERIAL, serial);

  save_gam_file("/root/dumped_dest_3plane.gam", &Dest);

  /* Free the rects memory */
  save_gam_file("/root/dumped_dest_1plane.gam", &Src);

  /* Release the surface */
  stm_blitter_surface_put(src);
  stm_blitter_surface_put(dest);

  /* Release the blitter */
  stm_blitter_put(blitter);

  free_gam_file(&Src);
  if (!info && bpa2 && Dest.buffer_address.base)
    bpa2_free_pages (bpa2, Dest.buffer_address.base);

  /* Free the color data memory */
  if(!color_p)
    kfree((void *)color_p);

  printk(KERN_INFO "Blit tests finished.\n");
  return 0;

out_surface:
  if (!info && bpa2 && Dest.buffer_address.base)
    bpa2_free_pages (bpa2, Dest.buffer_address.base);
  printk(KERN_INFO "Blit tests failed!\n");
  return -1;
}

static void __exit blit_3plane_exit(void)
{
	printk(KERN_INFO " Blit tests module removed.\n");
}

module_init(blit_3plane_init);
module_exit(blit_3plane_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(BLIT_TEST_AUTHOR);
MODULE_DESCRIPTION(BLIT_TEST_DESC);
