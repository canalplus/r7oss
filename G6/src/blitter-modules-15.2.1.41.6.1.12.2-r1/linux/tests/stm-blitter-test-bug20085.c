/*
 * blend test
 *
 * Copyright (C) STMicroelectronics Limited 2012. All rights reserved.
 *
 * Test to exercise Blit Operations using the blitter hw
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>

#include <linux/stm/blitter.h>

#include "gam_utils/gam_utils.h"
#include "gam_utils/surf_utils.h"


/* Test module parameters */
static char *S2_File_Name  = "/root/gam/football_AlRGB.gam";
static char *S1_File_Name  = "/root/gam/hell_ARGB.gam";

module_param(S1_File_Name, charp, 0000);
MODULE_PARM_DESC(S1_File_Name, "Source1 GAM File Name");

module_param(S2_File_Name, charp, 0000);
MODULE_PARM_DESC(S2_File_Name, "Source2 GAM File Name");


static int __init
test_init(void)
{
  long res;
  stm_blitter_t *blitter;

  bitmap_t Src1;
  stm_blitter_surface_t *src1;

  bitmap_t Src2;
  stm_blitter_surface_t *src2;

  bitmap_t Dest;
  struct blit_fb_bpa2_test_surface *dest;
  stm_blitter_surface_t *dst;

  stm_blitter_rect_t src_rects[3];
  stm_blitter_point_t src2_points[3];
  stm_blitter_point_t dst_points[3];

  stm_blitter_serial_t serial;

  static const stm_blitter_color_t white = { .a = 0xff,
                                             .r = 0xff,
                                             .g = 0xff,
                                             .b = 0xff };

  printk(KERN_INFO "%s\n", __func__);

  /* get a blitter handle */
  blitter = stm_blitter_get(0);
  if (IS_ERR(blitter)) {
    printk(KERN_ERR "Failed to get a blitter device: %lld!\n",
           (long long) PTR_ERR(blitter));
    res = PTR_ERR(blitter);
    goto out_blitter;
  }

  /* source 1 */
  printk(KERN_DEBUG "Loading Source 1 Gam file %s\n", S1_File_Name);
  res = load_gam_file(S1_File_Name, &Src1);
  if (res < 0) {
    printk(KERN_ERR "Failed to load Src1 GAM file: %lld!\n",
           (long long) res);
    goto out_gam_s1;
  }
  src1 = stm_blitter_surface_new_preallocated(Src1.format,
                                              Src1.colorspace,
                                              &Src1.buffer_address,
                                              Src1.buffer_size,
                                              &Src1.size,
                                              Src1.pitch);
  if (IS_ERR(src1)) {
    res = PTR_ERR(src1);
    printk(KERN_ERR "Failed to create a surface for src1: %lld!\n",
           (long long) res);
    src1 = NULL;
    goto out_src1;
  }

  /* source 2 */
  printk(KERN_DEBUG "Loading Source 2 Gam file %s\n", S2_File_Name);
  res = load_gam_file(S2_File_Name, &Src2);
  if (res < 0) {
    printk(KERN_ERR "Failed to load Src2 GAM file: %lld!\n",
           (long long) res);
    goto out_gam_s2;
  }
  src2 = stm_blitter_surface_new_preallocated(Src2.format,
                                              Src2.colorspace,
                                              &Src2.buffer_address,
                                              Src2.buffer_size,
                                              &Src2.size,
                                              Src2.pitch);
  if (IS_ERR(src2)) {
    res = PTR_ERR(src2);
    printk(KERN_ERR "Failed to create a surface for src2: %lld!\n",
           (long long) res);
    src2 = NULL;
    goto out_src2;
  }


  Dest.size.w = 1024;
  Dest.size.h = 768;
  dest = get_fb_or_bpa2_surf(&Dest.size, &Dest.buffer_address,
                             &Dest.buffer_size, &Dest.pitch, &Dest.format);
  if (IS_ERR(dest)) {
    res = PTR_ERR(dest);
    printk(KERN_ERR "Failed to get memory for dest: %lld!\n",
           (long long) res);
    dest = NULL;
    goto out_dest;
  }
  dst = dest->surf;

  stm_blitter_surface_clear(blitter, dst, &white);

  /* gam files are not pre-multiplied, hence we need to pre-multiply the
     pixel data during (before) the blend operation. */
  stm_blitter_surface_set_blitflags(dst,
                                    (0
                                     | STM_BLITTER_SBF_BLEND_ALPHACHANNEL
                                     | STM_BLITTER_SBF_SRC_PREMULTIPLY
                                    )
                                   );

  stm_blitter_surface_set_porter_duff(dst, STM_BLITTER_PD_SOURCE_OVER);

  src_rects[0] = (stm_blitter_rect_t) { .position = { .x = 0, .y = 0 },
                                        .size = { .w = Dest.size.w,
                                                  .h = Dest.size.h } };
  src2_points[0] = (stm_blitter_point_t) { .x = 0, .y = 30 };
  dst_points[0] = (stm_blitter_point_t) { .x = Dest.size.w / 2,
                                          .y = Dest.size.h / 2 };
  src_rects[1] = (stm_blitter_rect_t) { .position = { .x = 50, .y = 50 },
                                        .size = { .w = Dest.size.w,
                                                  .h = Dest.size.h } };
  src2_points[1] = (stm_blitter_point_t) { .x = 1250, .y = 30 };
  dst_points[1] = (stm_blitter_point_t) { .x = 500, .y = 600 };
  src_rects[2] = (stm_blitter_rect_t) { .position = { .x = 230, .y = 50 },
                                        .size = { .w = Dest.size.w,
                                                  .h = Dest.size.h } };
  src2_points[2] = (stm_blitter_point_t) { .x = 257, .y = 30 };
  dst_points[2] = (stm_blitter_point_t) { .x = 500, .y = -100 };

  /* Start blitter blend operation */
  res = stm_blitter_surface_blit_two(blitter,
                                     src1, src_rects,
                                     src2, src2_points,
                                     dst, dst_points,
                                     ARRAY_SIZE(dst_points));
  if (res < 0) {
    printk(KERN_ERR "stm_blitter_surface_blit2 failed: %lld!\n",
           (long long) res);
    goto out_blit2;
  }

  /* wait for the blitter operation to have finished */
  stm_blitter_surface_get_serial(dst, &serial);
  stm_blitter_wait(blitter, STM_BLITTER_WAIT_SERIAL, serial);

out_blit2:
  dst = NULL;
  free_fb_bpa2_surf(dest);
out_dest:
  stm_blitter_surface_put(src2);
out_src2:
  free_gam_file(&Src2);
out_gam_s2:
  stm_blitter_surface_put(src1);
out_src1:
  free_gam_file(&Src1);
out_gam_s1:

  stm_blitter_put(blitter);
out_blitter:

  if (!res) {
    printk(KERN_DEBUG "Blending tests finished.\n");
    res = -ENODEV;
  } else
    printk(KERN_ERR "Blending tests failed!\n");

  return res;
}

module_init(test_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andre Draszik <andre.draszik@st.com>");
MODULE_DESCRIPTION("A simple (blend) test for blit_two() clipping");
