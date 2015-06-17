/*
 * fill-test.c
 *
 * Copyright (C) STMicroelectronics Limited 2012. All rights reserved.
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

#include "gam_utils/gam_utils.h"
#include "gam_utils/surf_utils.h"


#undef BLIT_INITIAL_DEST_IMAGE
#define BLIT_USE_DIFFERENT_COLOR_BLENDING
#define DO_BLIT2


/* Test module parameters */
static char *file_name1 = "/root/image1280x720clut8.gam";
static char *file_name2 = "/root/bool_1280_720_1.gam";
static char *file_name3 = "/root/ARGB8888_1280_720_1.gam";

module_param(file_name1, charp, 0000);
MODULE_PARM_DESC(file_name1, "Source1 GAM File Name");

module_param(file_name2, charp, 0000);
MODULE_PARM_DESC(file_name2, "Source2 GAM File Name");

module_param(file_name3, charp, 0000);
MODULE_PARM_DESC(file_name3, "Destination GAM File Name");


static int __init
test_init(void)
{
  int res;

  stm_blitter_t *blitter;

  bitmap_t                          Dest;
  struct blit_fb_bpa2_test_surface *dest;

  bitmap_t               Src1;
  stm_blitter_surface_t *src1;
  bitmap_t               Src2;
  stm_blitter_surface_t *src2;
  bitmap_t               Src3;
  stm_blitter_surface_t *src3;

  stm_blitter_color_t color;
  stm_blitter_rect_t  src_rect;
#if defined(DO_BLIT2)
  stm_blitter_point_t src2_point;
#endif /* DO_BLIT2 */
  stm_blitter_point_t dst_point;

  stm_blitter_serial_t serial;

  printk(KERN_INFO "%s\n", __func__);

  /* get a blitter handle */
  blitter = stm_blitter_get(0);
  if (IS_ERR(blitter)) {
    printk(KERN_ERR "Failed to get a blitter device: %lld!\n",
           (long long) PTR_ERR(blitter));
    res = -ENXIO;
    goto out_blitter;
  }

  /* destination */
  printk(KERN_DEBUG "Creating destination surface\n");
  Dest.size.w = 1024;
  Dest.size.h = 768;
  dest = get_fb_or_bpa2_surf(&Dest.size, &Dest.buffer_address,
                             &Dest.buffer_size, &Dest.pitch, &Dest.format);
  if (IS_ERR(dest)) {
    res = PTR_ERR(dest);
    printk (KERN_ERR "Failed to get a create new surface for dest: %lld!\n",
            (long long) res);
    dest = NULL;
    goto out_dest;
  }
  /* this is not good (TM)! */
  Dest.memory = phys_to_virt (Dest.buffer_address.base);


  /* source 1 */
  printk(KERN_DEBUG "Loading Source1 GAM file %s\n", file_name1);
  res = load_gam_file(file_name1, &Src1);
  if (res < 0) {
    printk(KERN_ERR "Failed to load Source1 GAM file: %lld!\n",
           (long long) res);
    goto out_gam1;
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
  printk(KERN_DEBUG "Loading Source2 GAM file %s\n", file_name2);
  res = load_gam_file(file_name2, &Src2);
  if (res < 0) {
    printk (KERN_ERR "Failed to load Source2 GAM file: %lld!\n",
            (long long) res);
    goto out_gam2;
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


  /* source 3 */
  printk(KERN_DEBUG "Loading Source3 GAM file %s\n", file_name3);
  res = load_gam_file(file_name3, &Src3);
  if (res < 0) {
    printk (KERN_ERR "Failed to load Source3 GAM file: %lld!\n",
            (long long) res);
    goto out_gam3;
  }
  /* Create a new surface for source3 */
  src3 = stm_blitter_surface_new_preallocated(Src3.format,
                                              Src3.colorspace,
                                              &Src3.buffer_address,
                                              Src3.buffer_size,
                                              &Src3.size,
                                              Src3.pitch);
  if (IS_ERR(src3)) {
    res = PTR_ERR(src3);
    printk(KERN_ERR "Failed to create a surface for src3: %lld!\n",
           (long long) res);
    src3 = NULL;
    goto out_src3;
  }

  /* clear the destination surface (blue colour) */
  color = (stm_blitter_color_t) { .a = 0xff,
                                  .r = 0x00,
                                  .g = 0x00,
                                  .b = 0xff };
  stm_blitter_surface_clear(blitter, dest->surf, &color);

#if defined(BLIT_INITIAL_DEST_IMAGE)
  /* blit something to the destination (preparing next scenario) */
  stm_blitter_surface_set_blitflags(dest->surf, STM_BLITTER_SBF_NONE);
  src_rect = (stm_blitter_rect_t) { .position = { .x = 100, .y  = 100 },
                                    .size = { .w = 1080, .h = 520 } };
  dst_point = (stm_blitter_point_t) { .x = 100, .y = 100 };
  res = stm_blitter_surface_blit(blitter,
                                 src3, &src_rect,
                                 dest->surf, &dst_point,
                                 1);
  if (res < 0) {
    printk(KERN_ERR "stm_blitter_surface_blit (1) failed: %lld!\n",
           (long long) res);
    goto out_blit1;
  }
#endif

  /*
   * A) Resize/Copy operation(with the option of Global Alhpa Blending).
   *
   *  1) for Plain copy i'd use (STM_BLITTER_SBF_SRC_PREMULTIPLY )
   *  copy FG to BG and Premultiply RGB(fg) with A(fg)
   *
   *  2) for copy with global Alpha blending
   *  (STM_BLITTER_SBF_SRC_PREMULTIPLY | STM_BLITTER_SBF_BLEND_COLORALPHA )
   *  Premultiply RGB(fg) with A(fg) copy Foreground to background and also
   *  blend using the alpha value from the color.
   */

  /*
   * B) Blend operation (with the option of Global Alhpa Blending).
   * Here Premultipication is mandatory as this is a blend operation
   * therfore.
   *
   *  1) for a plain blend
   *  (STM_BLITTER_SBF_SRC_PREMULTIPLY | STM_BLITTER_SBF_BLEND_ALPHACHANNEL)
   *  Premultiply RGB(fg) with A(fg) and modulate src color with (modulated)
   *  alpha from src while blending.
   *
   *  2) for blend with global Alpha blending
   *  (STM_BLITTER_SBF_SRC_PREMULTIPLY | STM_BLITTER_SBF_BLEND_COLORALPHA |
   *  STM_BLITTER_SBF_BLEND_ALPHACHANNEL).
   *  Premultiply RGB(fg) with A(fg) blend Foreground onto background use
   *  the alpha value from the color and modulate src color with (modulated)
   *  alpha from src.
   */


  /* Part [A-1] */

  /* setup blit operation */
  stm_blitter_surface_set_blitflags(dest->surf,
                                    STM_BLITTER_SBF_SRC_PREMULTIPLY);

  src_rect = (stm_blitter_rect_t) { .position = { .x = 100, .y  = 100 },
                                    .size = { .w = 400, .h = 250 } };
  dst_point = (stm_blitter_point_t) { .x = 100, .y = 100 };
  res = stm_blitter_surface_blit(blitter,
                                 src1, &src_rect,
                                 dest->surf, &dst_point,
                                 1);
  if (res < 0) {
    printk(KERN_ERR "stm_blitter_surface_blit (2) failed: %lld!\n",
           (long long) res);
    goto out_blit2;
  }

  /* wait for operation on dest to complete */
  stm_blitter_surface_get_serial(dest->surf, &serial);
  stm_blitter_wait(blitter, STM_BLITTER_WAIT_SERIAL, serial);


  /* Part [A-2] */
  /* setup blit operation */
#if defined(BLIT_USE_DIFFERENT_COLOR_BLENDING)
  color = (stm_blitter_color_t) { .a = 0x80,
                                  .r = 0xff,
                                  .g = 0x00,
                                  .b = 0x00 };
#endif
  stm_blitter_surface_set_color(dest->surf, &color);

  stm_blitter_surface_set_blitflags(dest->surf,
                                    (STM_BLITTER_SBF_SRC_PREMULTIPLY
                                     | STM_BLITTER_SBF_BLEND_COLORALPHA));

  src_rect = (stm_blitter_rect_t) { .position = { .x = 100, .y  = 100 },
                                    .size = { .w = 400, .h = 250 } };
  dst_point = (stm_blitter_point_t) { .x = 100, .y = 400 };

  res = stm_blitter_surface_blit(blitter,
                                 src1, &src_rect,
                                 dest->surf, &dst_point,
                                 1);
  if (res < 0) {
    printk(KERN_ERR "stm_blitter_surface_blit (3) failed: %lld!\n",
           (long long) res);
    goto out_blit3;
  }

  /* wait for operation on dest to complete */
  stm_blitter_surface_get_serial(dest->surf, &serial);
  stm_blitter_wait (blitter, STM_BLITTER_WAIT_SERIAL, serial);


  /* Part [B-1] */
  stm_blitter_surface_set_blitflags(dest->surf,
                                    (STM_BLITTER_SBF_SRC_PREMULTIPLY
                                     | STM_BLITTER_SBF_BLEND_ALPHACHANNEL));

  src_rect = (stm_blitter_rect_t) { .position = { .x = 100, .y  = 100 },
                                    .size = { .w = 400, .h = 250 } };
  dst_point = (stm_blitter_point_t) { .x = 550, .y = 100 };

#ifndef DO_BLIT2
  res = stm_blitter_surface_blit(blitter,
                                 src1, &src_rect,
                                 dest->surf, &dst_point,
                                 1);
#else /* DO_BLIT2 */
  src2_point = (stm_blitter_point_t) { .x = 0, .y = 0 };
  res = stm_blitter_surface_blit_two(blitter,
                                     src1, &src_rect,
                                     src2, &src2_point,
                                     dest->surf, &dst_point,
                                     1);
#endif /* DO_BLIT2 */
  if (res < 0) {
    printk(KERN_ERR "stm_blitter_surface_blit (4) failed: %lld!\n",
           (long long) res);
    goto out_blit4;
  }

  /* wait for operation on dest to complete */
  stm_blitter_surface_get_serial(dest->surf, &serial);
  stm_blitter_wait(blitter, STM_BLITTER_WAIT_SERIAL, serial);


  /* Part [B-2] */
#if defined(BLIT_USE_DIFFERENT_COLOR_BLENDING)
  color = (stm_blitter_color_t) { .a = 0x80,
                                  .r = 0xff,
                                  .g = 0x00,
                                  .b = 0x00 };
#endif
  stm_blitter_surface_set_color(src1, &color);
  stm_blitter_surface_set_color(dest->surf, &color);

  stm_blitter_surface_set_blitflags(dest->surf,
                                    (STM_BLITTER_SBF_SRC_PREMULTIPLY
                                     | STM_BLITTER_SBF_BLEND_COLORALPHA
                                     | STM_BLITTER_SBF_BLEND_ALPHACHANNEL));

  src_rect = (stm_blitter_rect_t) { .position = { .x = 100, .y  = 100 },
                                    .size = { .w = 400, .h = 250 } };
  dst_point = (stm_blitter_point_t) { .x = 550, .y = 400 };
#ifndef DO_BLIT2
  res = stm_blitter_surface_blit(blitter,
                                 src1, &src_rect,
                                 dest->surf, &dst_point,
                                 1);
#else /* DO_BLIT2 */
  res = stm_blitter_surface_blit_two(blitter,
                                     src1, &src_rect,
                                     src2, &src2_point,
                                     dest->surf, &dst_point,
                                     1);
#endif /* DO_BLIT2 */
  if (res < 0) {
    printk(KERN_ERR "stm_blitter_surface_blit (5) failed: %lld!\n",
           (long long) res);
    goto out_blit5;
  }

  /* wait for operation on dest to complete */
  stm_blitter_surface_get_serial(dest->surf, &serial);
  stm_blitter_wait(blitter, STM_BLITTER_WAIT_SERIAL, serial);


  save_gam_file("/root/dumped_dest_clut8.gam", &Dest);


out_blit5:
out_blit4:
out_blit3:
out_blit2:
#if defined(BLIT_INITIAL_DEST_IMAGE)
out_blit1:
#endif
  stm_blitter_surface_put(src3);
out_src3:
  free_gam_file(&Src3);
out_gam3:
  stm_blitter_surface_put(src2);
out_src2:
  free_gam_file(&Src2);
out_gam2:
  stm_blitter_surface_put(src1);
out_src1:
  free_gam_file(&Src1);
out_gam1:
  free_fb_bpa2_surf(dest);
out_dest:
  stm_blitter_put(blitter);
out_blitter:

  if (!res)
    printk (KERN_INFO "Blit Two + Blending tests finished.\n");
  else
    printk (KERN_INFO "Blit Two + Blending tests failed!\n");

  return res;
}

static void __exit
test_exit(void)
{
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Akram BEN BELGACEM <akram.ben-belgacem@st.com>");
MODULE_DESCRIPTION("A blit using clut8 source and other two sources operation"
                   " test with blending enabled");
