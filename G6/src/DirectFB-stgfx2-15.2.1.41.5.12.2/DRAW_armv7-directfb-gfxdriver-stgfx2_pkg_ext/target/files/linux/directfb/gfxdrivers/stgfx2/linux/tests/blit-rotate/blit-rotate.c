/*
 * blit-rotate.c
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * Test to exercise Blit with rotation and flip Operations using the blitter hw
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

#include "gam_utils/gam_utils.h"
#include "gam_utils/surf_utils.h"


/* Test module parameters */
static char *file_name1 = "/root/gam/football_AlRGB.gam";
static char *file_name2 = "/root/gam/football_AlRGB_r.gam";

module_param(file_name1, charp, 0000);
MODULE_PARM_DESC(file_name1, "Source1 GAM File Name");

module_param(file_name2, charp, 0000);
MODULE_PARM_DESC(file_name2, "Source2 GAM File Name");

#undef LOAD_DESTINATION_GAMFILE

static int __init
test_init(void)
{
  int res;
  stm_blitter_t                    *blitter = NULL;
  stm_blitter_color_t color;
  stm_blitter_rect_t                src_rects   = { { 100, 50 }, { 200, 200 } };
  stm_blitter_point_t               dst_points  = { 0, 0 };
  stm_blitter_serial_t              serial;
  bitmap_t               Src1;
  stm_blitter_surface_t *src1;

  bitmap_t                            Dest;
  stm_blitter_surface_t              *d_surf;
#if !defined(LOAD_DESTINATION_GAMFILE)
  struct blit_fb_bpa2_test_surface   *dest;
  stm_blitter_surface_address_t       d_buffer_address;
  size_t                              d_buffer_size;
  unsigned long                       d_pitch;
  stm_blitter_surface_format_t        d_format;
#endif

  pr_info("Starting rotation tests\n");

  /* get a blitter handle */
  blitter = stm_blitter_get(0);
  if (IS_ERR(blitter)) {
      pr_err("Failed to get a blitter device: %lld!\n",
             (long long) PTR_ERR(blitter));
      res = PTR_ERR(blitter);
      goto out_blitter;
  }

  res = load_gam_file(file_name1, &Src1);
  if (res < 0)
    goto out_gam_s1;
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

#if defined(LOAD_DESTINATION_GAMFILE)
  res = load_gam_file(file_name2, &Dest);
  if (res < 0)
    goto out_gam_d;
  d_surf = stm_blitter_surface_new_preallocated(Dest.format,
                                                Dest.colorspace,
                                                &Dest.buffer_address,
                                                Dest.buffer_size,
                                                &Dest.size,
                                                Dest.pitch);
#else
  Dest.size.w = 1024;
  Dest.size.h = 768;
  dest = get_fb_or_bpa2_surf(&Dest.size, &d_buffer_address, &d_buffer_size,
                             &d_pitch, &d_format);
  if (IS_ERR(dest)) {
    res = PTR_ERR(dest);
    pr_err("Failed to get memory for dest: %lld!\n", (long long) res);
    goto out_dest;
  }
  Dest.format = d_format;
  Dest.colorspace = STM_BLITTER_SCS_RGB;
  Dest.buffer_size = d_buffer_size;
  Dest.pitch = d_pitch;
  Dest.buffer_address = d_buffer_address;
  d_surf = stm_blitter_surface_ref(dest->surf);
  /* this is not good (TM)! */
  Dest.memory = phys_to_virt(d_buffer_address.base);
#endif
  if (IS_ERR(d_surf)) {
    res = PTR_ERR(d_surf);
    printk(KERN_ERR "Failed to create a surface for dest: %lld!\n",
           (long long) res);
    d_surf = NULL;
    goto out_d_surf;
  }


  /* clear the destination surface */
  color.a = 0xff;
  color.r = 0xff;
  color.g = 0xff;
  color.b = 0xff;
  stm_blitter_surface_clear(blitter, d_surf, &color);


  /* Blit the Original Bitmap */
  src_rects.position.x  = 100;
  src_rects.position.y  = 100;
  src_rects.size.w  = 464;
  src_rects.size.h  = 208;
  dst_points.x = 720;
  dst_points.y = 22;
  /* Simple Blit ops */
  res = stm_blitter_surface_blit(blitter,
                                 src1, &src_rects,
                                 d_surf, &dst_points, 1);
  if (res < 0) {
    pr_err("simple blit failed!\n");
    goto out_blit1;
  }

  /* Rotate 90 Test */
  res = stm_blitter_surface_set_blitflags(d_surf,
                                          STM_BLITTER_SBF_ROTATE90);
  if (res < 0) {
    pr_err("failed to setup blit flags 90 for the surface!\n");
    goto out_blit1;
  }
  dst_points.x = 720;
  dst_points.y = 235;
  res = stm_blitter_surface_blit(blitter,
                                 src1, &src_rects,
                                 d_surf, &dst_points, 1);
  if (res < 0) {
    pr_err("blit Rotate 90 failed!\n");
    goto out_blit1;
  }

  /* Rotate 270 Test */
  res = stm_blitter_surface_set_blitflags(d_surf,
                                          STM_BLITTER_SBF_ROTATE270);
  if (res < 0) {
    pr_err("failed to setup blit flags 270 for the surface!\n");
    goto out_blit1;
  }
  dst_points.x = 464+720-208;
  dst_points.y = 235;
  res = stm_blitter_surface_blit(blitter,
                                 src1, &src_rects,
                                 d_surf, &dst_points, 1);
  if (res < 0) {
    pr_err("blit Rotate 270 failed!\n");
    goto out_blit1;
  }

  /* Blit the original bitmap for next tests */
  res = stm_blitter_surface_set_blitflags(d_surf,
                                          STM_BLITTER_SBF_NONE);
  if (res < 0) {
    pr_err("failed to setup empty blit flags for the surface!\n");
    goto out_blit1;
  }
  src_rects.position.x  = 460;
  src_rects.position.y  = 350;
  src_rects.size.w  = 304;
  src_rects.size.h  = 240;
  dst_points.x = 50;
  dst_points.y = 100;
  /* Simple Blit ops */
  res = stm_blitter_surface_blit(blitter,
                                 src1, &src_rects,
                                 d_surf, &dst_points, 1);
  if (res < 0) {
    pr_err("simple blit failed!\n");
    goto out_blit1;
  }

  /* Vertical Flip Tests */
  res = stm_blitter_surface_set_blitflags(d_surf,
                                          STM_BLITTER_SBF_FLIP_VERTICAL);
  if (res < 0) {
    pr_err("failed to setup vertical flip for the surface!\n");
    goto out_blit1;
  }
  dst_points.x = 50;
  dst_points.y = 350;
  res = stm_blitter_surface_blit(blitter,
                                 src1, &src_rects,
                                 d_surf, &dst_points, 1);
  if (res < 0) {
    pr_err("blit with vertical flip failed!\n");
    goto out_blit1;
  }

  /* Horizontal Flip Tests */
  res = stm_blitter_surface_set_blitflags(d_surf,
                                          STM_BLITTER_SBF_FLIP_HORIZONTAL);
  if (res < 0) {
    pr_err("failed to setup horizontal flip for the surface!\n");
    goto out_blit1;
  }
  dst_points.x = 365;
  dst_points.y = 100;
  res = stm_blitter_surface_blit(blitter,
                                 src1, &src_rects,
                                 d_surf, &dst_points, 1);
  if (res < 0) {
    pr_err("blit with horizontal flip failed!\n");
    goto out_blit1;
  }

  /* Rotate 180 Test */
  res = stm_blitter_surface_set_blitflags(d_surf,
                                          STM_BLITTER_SBF_ROTATE180);
  if (res < 0) {
    pr_err("failed to setup blit flags 180 for the surface!\n");
    goto out_blit1;
  }
  dst_points.x = 365;
  dst_points.y = 350;
  res = stm_blitter_surface_blit(blitter,
                                 src1, &src_rects,
                                 d_surf, &dst_points, 1);
  if (res < 0) {
    pr_err("blit rotate 180 failed!\n");
    goto out_blit1;
  }

  /* wait for blitter to have finished all operations on this surface. */
  stm_blitter_surface_get_serial(d_surf, &serial);
  stm_blitter_wait(blitter, STM_BLITTER_WAIT_SERIAL, serial);

  save_gam_file("/root/dumped_dest_rot.gam", &Dest);


out_blit1:
  stm_blitter_surface_put(d_surf);
out_d_surf:
#if defined(LOAD_DESTINATION_GAMFILE)
  free_gam_file(&Dest);
out_gam_d:
#else
  free_fb_bpa2_surf(dest);
out_dest:
#endif

  stm_blitter_surface_put(src1);
out_src1:
  free_gam_file(&Src1);
out_gam_s1:

  stm_blitter_put(blitter);
out_blitter:

  if (!res) {
    pr_debug("rotate tests finished.\n");
    res = -ENODEV;
  } else
    pr_err("rotate tests failed!\n");

  return res;
}


module_init(test_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Akram BEN BELGACEM <akram.ben-belgacem@st.com>");
MODULE_DESCRIPTION("A sample blit rotate and flipping test operations using the blitter driver");
