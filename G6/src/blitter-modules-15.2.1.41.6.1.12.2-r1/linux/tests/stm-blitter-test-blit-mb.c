/*
 * stm-blitter-test-blit-mb.c
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * Test to exercise Blit Operations using Macroblock sources
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/stm/blitter.h>

#include "gam_utils/gam_utils.h"
#include "gam_utils/surf_utils.h"

#define TEST_BLIT_RECT_WIDHT       120
#define TEST_BLIT_RECT_HEIGHT       60


/* Test module parameters */
static char *file_name1 = "image1280x720mb.gam";

module_param(file_name1, charp, 0000);
MODULE_PARM_DESC(file_name1, "Source1 GAM File Name");


static stm_blitter_t *blitter;


static int __init
blit_test_init(void)
{
  int res;

  bitmap_t                          Dest;
  struct blit_fb_bpa2_test_surface *dest;

  bitmap_t               Src1;
  stm_blitter_surface_t *src1;

  stm_blitter_color_t color;
  stm_blitter_rect_t  src_rect;
  stm_blitter_point_t dst_point;

  stm_blitter_rect_t  *blit_rects;
  stm_blitter_point_t *blit_points;

  int i, k;

  stm_blitter_serial_t serial;

  printk (KERN_INFO "Starting Blit tests\n");


  /* Get a blitter handle */
  blitter = stm_blitter_get (0);
  if (IS_ERR (blitter))
  {
    printk(KERN_ERR "Failed to get a blitter device: %lld!\n",
           (long long) PTR_ERR (blitter));
    blitter = NULL;
    return -ENXIO; /* No such device or address */
  }

  /* Create a new surface for destination */
  Dest.size.w = 1024;
  Dest.size.h = 768;
  dest = get_fb_or_bpa2_surf (&Dest.size, &Dest.buffer_address,
                              &Dest.buffer_size, &Dest.pitch, &Dest.format);
  if (IS_ERR (dest))
  {
    res = PTR_ERR (dest);
    printk (KERN_ERR "Failed to get a create new surface for dest: %lld!\n",
            (long long) res);
    dest = NULL;
    goto out_dest;
  }
  Dest.memory = phys_to_virt (Dest.buffer_address.base);


  printk (KERN_INFO "Loading Source1 Gam file...\n");
  if ((res = load_gam_file (file_name1, &Src1)) < 0)
  {
    printk (KERN_ERR "Failed to load GAM file!\n");
    goto out_gam;
  }

  /* Create a new surface for source1 */
  src1 = stm_blitter_surface_new_preallocated (Src1.format,
                                               Src1.colorspace,
                                               &Src1.buffer_address,
                                               Src1.buffer_size,
                                               &Src1.size,
                                               Src1.pitch);
  if (IS_ERR (src1))
  {
    res = PTR_ERR (src1);
    printk(KERN_ERR "Failed to get a create new surface for src1: %lld!\n",
           (long long) res);
    src1 = NULL;
    goto out_src1;
  }


  color.a = color.r = color.g = color.b = 0xff;
  stm_blitter_surface_clear (blitter, dest->surf, &color);

  printk (KERN_INFO "Starting Blit tests..\n");
  src_rect.position.x = dst_point.x = 0;
  src_rect.position.y = dst_point.y = 0;
  src_rect.size.w = Dest.size.w;
  src_rect.size.h = Dest.size.h;

  /* Simple Blit ops */
  if ((res = stm_blitter_surface_blit (blitter, src1, &src_rect,
                                       dest->surf, &dst_point, 1)) < 0) {
    printk (KERN_ERR "Simple Blit op (1) failed: %lld\n", (long long) res);
    goto out_src1;
  }
  save_gam_file ("dumped_dest_blit1.gam", &Dest);

  /* Simple Blit ops */
  stm_blitter_surface_clear (blitter, dest->surf, &color);
  src_rect.position.x = 100;
  src_rect.position.y = 100;
  src_rect.size.w = 400;
  src_rect.size.h = 300;
  dst_point.x = 300;
  dst_point.y = 400;
  if ((res = stm_blitter_surface_blit (blitter, src1, &src_rect,
                                       dest->surf, &dst_point, 1)) < 0) {
    printk (KERN_ERR "Simple Blit op (2) failed: %lld\n", (long long) res);
    goto out_src1;
  }
  save_gam_file ("dumped_dest_blit2.gam", &Dest);

  /* Simple Blit ops */
  stm_blitter_surface_clear (blitter, dest->surf, &color);
  src_rect.position.x = 0;
  src_rect.position.y = 0;
  src_rect.size.w = 400;
  src_rect.size.h = 300;
  dst_point.x = 800;
  dst_point.y = 350;
  if ((res = stm_blitter_surface_blit (blitter, src1, &src_rect,
                                       dest->surf, &dst_point, 1)) < 0) {
    printk (KERN_ERR "Simple Blit op (3) failed: %lld\n", (long long) res);
    goto out_src1;
  }
  save_gam_file ("dumped_dest_blit3.gam", &Dest);


  printk (KERN_INFO "Starting batch Blit tests..\n");
  /* Allocate memory for blit rects */
  blit_rects = kmalloc ((Dest.size.h / TEST_BLIT_RECT_HEIGHT) * sizeof (stm_blitter_rect_t),
                        GFP_KERNEL);
  if (!blit_rects) {
    printk (KERN_ERR "Failed to allocate memory!\n");
    res = -ENOMEM;
    goto out_rects;
  }

  blit_points = kmalloc ((Dest.size.h / TEST_BLIT_RECT_HEIGHT) * sizeof(stm_blitter_point_t),
                         GFP_KERNEL);
  if (!blit_points) {
    printk (KERN_ERR "Failed to allocate memory!\n");
    res = -ENOMEM;
    goto out_points;
  }

  color.a = 0xff;
  color.r = 0x00;
  color.g = 0x00;
  color.b = 0xff;
  stm_blitter_surface_clear (blitter, dest->surf, &color);

  for (i = 30;
       i < (Dest.size.h - TEST_BLIT_RECT_HEIGHT);
       i += TEST_BLIT_RECT_HEIGHT) {
    int nb_ops = 0;

    for (k = 100;
         k < (Dest.size.w - TEST_BLIT_RECT_WIDHT);
         k += TEST_BLIT_RECT_WIDHT) {
      blit_rects[k/TEST_BLIT_RECT_WIDHT].size.w = TEST_BLIT_RECT_WIDHT - 10;
      blit_rects[k/TEST_BLIT_RECT_WIDHT].size.h = TEST_BLIT_RECT_HEIGHT - 10;
      blit_rects[k/TEST_BLIT_RECT_WIDHT].position.x = Dest.size.w - (TEST_BLIT_RECT_WIDHT + k + 10);
      blit_rects[k/TEST_BLIT_RECT_WIDHT].position.y = Dest.size.h - (TEST_BLIT_RECT_HEIGHT + i + 10);
      blit_points[k/TEST_BLIT_RECT_WIDHT].x = k;
      blit_points[k/TEST_BLIT_RECT_WIDHT].y = i;
      printk("Blit Rectangle --> (%lld,%lld-%lldx%lld -> %lld,%lld)\n",
              (long long) blit_rects[k / TEST_BLIT_RECT_WIDHT].position.x,
              (long long) blit_rects[k / TEST_BLIT_RECT_WIDHT].position.y,
              (long long) blit_rects[k / TEST_BLIT_RECT_WIDHT].size.w,
              (long long) blit_rects[k / TEST_BLIT_RECT_WIDHT].size.h,
              (long long) blit_points[k / TEST_BLIT_RECT_WIDHT].x,
              (long long) blit_points[k / TEST_BLIT_RECT_WIDHT].y);
      ++nb_ops;
    }

    if ((res = stm_blitter_surface_blit (blitter, src1, blit_rects,
                                         dest->surf, blit_points,
                                         nb_ops)) < 0) {
      printk (KERN_ERR "Batch Blit op (1) failed: %lld\n", (long long) res);
      goto out_points;
    }
  }

  stm_blitter_surface_get_serial (dest->surf, &serial);
  stm_blitter_wait (blitter, STM_BLITTER_WAIT_SERIAL, serial);

  save_gam_file ("dumped_dest_blit4.gam", &Dest);

  color.a = 0xff;
  color.r = 0x00;
  color.g = 0x00;
  color.b = 0xff;
  stm_blitter_surface_clear (blitter, dest->surf, &color);

  for (i = 30;
       i < (Dest.size.h - TEST_BLIT_RECT_HEIGHT);
       i += TEST_BLIT_RECT_HEIGHT) {
    int nb_ops = 0;
    for (k = 100;
         k < (Dest.size.w - TEST_BLIT_RECT_WIDHT);
         k += TEST_BLIT_RECT_WIDHT) {
      blit_rects[k / TEST_BLIT_RECT_WIDHT].size.w = TEST_BLIT_RECT_WIDHT - 10;
      blit_rects[k / TEST_BLIT_RECT_WIDHT].size.h = TEST_BLIT_RECT_HEIGHT - 10;
      blit_rects[k / TEST_BLIT_RECT_WIDHT].position.x = k + 10;
      blit_rects[k / TEST_BLIT_RECT_WIDHT].position.y = i + 10;
      blit_points[k / TEST_BLIT_RECT_WIDHT].x = k;
      blit_points[k / TEST_BLIT_RECT_WIDHT].y = i;
      printk("Blit Rectangle --> (%lld,%lld-%lldx%lld -> %lld,%lld)\n",
              (long long) blit_rects[k / TEST_BLIT_RECT_WIDHT].position.x,
              (long long) blit_rects[k / TEST_BLIT_RECT_WIDHT].position.y,
              (long long) blit_rects[k / TEST_BLIT_RECT_WIDHT].size.w,
              (long long) blit_rects[k / TEST_BLIT_RECT_WIDHT].size.h,
              (long long) blit_points[k / TEST_BLIT_RECT_WIDHT].x,
              (long long) blit_points[k / TEST_BLIT_RECT_WIDHT].y);
      ++nb_ops;
    }

    if ((res = stm_blitter_surface_blit (blitter, src1, blit_rects,
                                         dest->surf, blit_points,
                                         nb_ops)) < 0) {
      printk (KERN_ERR "Batch Blit op (2) failed: %lld\n", (long long) res);
      goto out_points;
    }
  }

  stm_blitter_surface_get_serial (dest->surf, &serial);
  stm_blitter_wait (blitter, STM_BLITTER_WAIT_SERIAL, serial);

  save_gam_file ("dumped_dest_blit5.gam", &Dest);


out_points:
  kfree (blit_points);
out_rects:
  kfree (blit_rects);
out_src1:
  stm_blitter_surface_put (src1);
out_gam:
  free_gam_file (&Src1);
out_dest:
  free_fb_bpa2_surf (dest);

  if (!res)
    printk (KERN_INFO "Blit tests finished.\n");
  else
    printk (KERN_INFO "Blit tests failed!\n");

  return res;
}

static void __exit
blit_test_exit (void)
{
  stm_blitter_put (blitter);
}

module_init (blit_test_init);
module_exit (blit_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Akram BEN BELGACEM <akram.ben-belgacem@st.com>");
MODULE_DESCRIPTION("Macroblock blitting");
