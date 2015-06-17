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

#define BLIT_TEST_AUTHOR "Riadh BEN AMOR <riadh.ben-amor@st.com>"
#define BLIT_TEST_DESC   "Blit tests for double Buffer destinations formats"

#define LOAD_DESTINATION_GAMFILE

/* Test module parameters */
static char *file_dest = "/root/tektro720p_422MB.gam";
static char *file_name = "/root/Joo2_4_2_0_R2B.gam";

module_param(file_name, charp, 0000);
MODULE_PARM_DESC(file_name, "Source2 GAM File Name");

module_param(file_dest, charp, 0000);
MODULE_PARM_DESC(file_dest, "Destination GAM File Name");

#define TEST_BLIT_RECT_WIDHT       120
#define TEST_BLIT_RECT_HEIGHT       60

int stm_blit_test_function(stm_blitter_t     * blitter,
                           bitmap_t            Src2,
                           bitmap_t            Dest,
                           stm_blitter_rect_t  src_rects,
                           stm_blitter_rect_t  dst_rects,
                           stm_blitter_point_t dst_points,
                           bool                stretch_op)
{
  stm_blitter_surface_blitflags_t   flags = STM_BLITTER_SBF_NONE;
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

  stm_blitter_surface_clear (blitter, dest, &color);

  if (stretch_op == true)
  {
    if(stm_blitter_surface_stretchblit(blitter, src2, &src_rects, dest, &dst_rects, 1) < 0)
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
    if(stm_blitter_surface_blit(blitter, src2, &src_rects, dest, &dst_points, 1) < 0)
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
#ifndef LOAD_DESTINATION_GAMFILE
  int res;
  struct bpa2_part                 *bpa2 = NULL;
  unsigned int                      MBRow, MBCol;
#endif
  stm_blitter_t      *blitter    = NULL;
  stm_blitter_rect_t  src_rects  = { { 30, 50 }, { 500, 400 } };
  stm_blitter_rect_t  dst_rects  = { { 50, 30 }, { 570, 350 } };
  stm_blitter_point_t dst_points = { 50, 50 };
  bitmap_t            Src2;
  bitmap_t            Dest;
  bool                stretch_op = true;

  printk(KERN_INFO "Starting Blit tests\n");

  /* Get a blitter handle */
  if((blitter = stm_blitter_get(0)) == NULL)
  {
    printk(KERN_ERR "Failed to get a blitter device!\n");
    return -ENXIO; /* No such device or address */
  }

#ifdef LOAD_DESTINATION_GAMFILE
  if(load_gam_file( file_dest, &Dest) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }
#else
  Dest.size.w = 1280;
  Dest.size.h = 720;

  {
    bpa2 = bpa2_find_part ("bigphysarea");
    if (!bpa2)
    {
      printk ("%s: no bpa2 partition 'bigphysarea'\n", __func__);
      res = -ENOENT;
      return res;
    }

    printk ("using memory from BPA2\n");

    Dest.format = STM_BLITTER_SF_YCBCR422MB;

    MBRow = Dest.size.w / 16 + ((Dest.size.w % 16 !=0)? 1:0);
    MBCol = Dest.size.h / 16 + ((Dest.size.h % 16 !=0)? 1:0);
    if ((MBCol % 2) != 0)
      MBCol++;

    Dest.pitch = MBRow * 16;

    Dest.buffer_size = MBRow * MBCol * (256 + 256);
    Dest.buffer_address.base = bpa2_alloc_pages (bpa2,
                                                 ((Dest.buffer_size
                                                   + PAGE_SIZE - 1)
                                                  / PAGE_SIZE),
                                                 PAGE_SIZE, GFP_KERNEL);

    Dest.buffer_address.cbcr_offset = MBRow * MBCol * 256;

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

  printk(KERN_INFO "Loading Source2 Gam file...\n");
  if(load_gam_file( file_name, &Src2) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }

  if (stm_blit_test_function(blitter, Src2, Dest, src_rects, dst_rects, dst_points, stretch_op) < 0)
  {
    goto out_surface;
  }

  save_gam_file("/root/dumped_dest_t1.gam", &Dest);

  free_gam_file(&Src2);

  /*
   *  Test2: YCbCr422R -> MB422
   */
  file_name = "/root/image1280x720r.gam";
  if(load_gam_file( file_name, &Src2) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }

  stretch_op = false;
  if (stm_blit_test_function(blitter, Src2, Dest, src_rects, dst_rects, dst_points, stretch_op) < 0)
  {
    goto out_surface;
  }

  save_gam_file("/root/dumped_dest_t2.gam", &Dest);
  free_gam_file(&Src2);

#ifndef LOAD_DESTINATION_GAMFILE
  if (bpa2 && Dest.buffer_address.base)
    bpa2_free_pages (bpa2, Dest.buffer_address.base);
#else
    free_gam_file(&Dest);
#endif

  /*
   * Test3: MB420 -> MB420
   */
#ifdef LOAD_DESTINATION_GAMFILE
  file_dest = "/root/image1280x720mb.gam";
  if(load_gam_file( file_dest, &Dest) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }
#else
  Dest.size.w = 1280;
  Dest.size.h = 720;

  {
    bpa2 = bpa2_find_part ("bigphysarea");
    if (!bpa2)
    {
      printk ("%s: no bpa2 partition 'bigphysarea'\n", __func__);
      res = -ENOENT;
      return res;
    }

    printk ("using memory from BPA2\n");
    Dest.format = STM_BLITTER_SF_YCBCR420MB;

    MBRow = Dest.size.w / 16 + ((Dest.size.w % 16 !=0)? 1:0);
    MBCol = Dest.size.h / 16 + ((Dest.size.h % 16 !=0)? 1:0);
    if ((MBCol % 2) != 0)
      MBCol++;

    Dest.pitch = MBRow * 16;

    Dest.buffer_size = MBRow * MBCol * (256 + 128);
    Dest.buffer_address.base = bpa2_alloc_pages (bpa2,
                                                 ((Dest.buffer_size
                                                   + PAGE_SIZE - 1)
                                                  / PAGE_SIZE),
                                                 PAGE_SIZE, GFP_KERNEL);

    Dest.buffer_address.cbcr_offset = MBRow * MBCol * 256;

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

  file_name = "/root/ftb_420_MB.gam";
  printk(KERN_INFO "Loading Source2 Gam file...\n");
  if(load_gam_file( file_name, &Src2) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }

  stretch_op = true;
  if (stm_blit_test_function(blitter, Src2, Dest, src_rects, dst_rects, dst_points, stretch_op) < 0)
  {
    goto out_surface;
  }

  save_gam_file("/root/dumped_dest_t3.gam", &Dest);
  free_gam_file(&Src2);

  /*
   * Test4: R2B420 -> MB420
   */
  file_name = "/root/Joo2_4_2_0_R2B.gam";
  if(load_gam_file( file_name, &Src2) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }

  stretch_op = false;
  if (stm_blit_test_function(blitter, Src2, Dest, src_rects, dst_rects, dst_points, stretch_op) < 0)
  {
    goto out_surface;
  }

  save_gam_file("/root/dumped_dest_t4.gam", &Dest);
  free_gam_file(&Src2);

#ifndef LOAD_DESTINATION_GAMFILE
  if (bpa2 && Dest.buffer_address.base)
    bpa2_free_pages (bpa2, Dest.buffer_address.base);
#else
    free_gam_file(&Dest);
#endif

#ifdef LOAD_DESTINATION_GAMFILE
  /*
   *  Test5: MB420 -> R2B
   */
  file_dest = "/root/Joo2_4_2_0_R2B.gam";
  if(load_gam_file( file_dest, &Dest) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }

  file_name = "/root/ftb_420_MB.gam";
  printk(KERN_INFO "Loading Source2 Gam file...\n");
  if(load_gam_file( file_name, &Src2) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }

  stretch_op = true;
  if (stm_blit_test_function(blitter, Src2, Dest, src_rects, dst_rects, dst_points, stretch_op) < 0)
  {
    goto out_surface;
  }

  save_gam_file("/root/dumped_dest_t5.gam", &Dest);
  free_gam_file(&Src2);

  /*
   *  Test6: R2B420 -> R2B420
   */
  file_name = "/root/Joo2_4_2_0_R2B.gam";
  if(load_gam_file( file_name, &Src2) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }

  stretch_op = false;
  if (stm_blit_test_function(blitter, Src2, Dest, src_rects, dst_rects, dst_points, stretch_op) < 0)
  {
    goto out_surface;
  }

  save_gam_file("/root/dumped_dest_t6.gam", &Dest);

  /* Release the surface */
  free_gam_file(&Src2);
  free_gam_file(&Dest);
#endif

  /* Release the blitter */
  stm_blitter_put(blitter);

  printk(KERN_INFO "Blit tests finished.\n");
  return 0;

out_surface:
#ifndef LOAD_DESTINATION_GAMFILE
  if (bpa2 && Dest.buffer_address.base)
    bpa2_free_pages (bpa2, Dest.buffer_address.base);
#endif
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

