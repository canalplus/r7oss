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


#undef LOAD_DESTINATION_GAMFILE

static char *S2_File_Name  = "/root/Test_Display_1280x720.gam";
static char *DST_File_Name = "/root/b1_MB_1920x1080.gam";

module_param(S2_File_Name, charp, 0000);
MODULE_PARM_DESC(S2_File_Name, "Source2 GAM File Name");

module_param(DST_File_Name, charp, 0000);
MODULE_PARM_DESC(DST_File_Name, "Destination GAM File Name");


static stm_blitter_t *blitter;


static int __init
test_init(void)
{
  long res;

  bitmap_t               Src2;
  stm_blitter_surface_t *src2;

  bitmap_t                          Dest;
#if !defined(LOAD_DESTINATION_GAMFILE)
  struct blit_fb_bpa2_test_surface *dest;
#endif
  stm_blitter_surface_t            *dst;

  stm_blitter_rect_t  src_rects;
  stm_blitter_point_t dst_points;

  stm_blitter_serial_t serial;


  printk(KERN_INFO "%s\n", __func__);

  /* get a blitter handle */
  blitter = stm_blitter_get(0);
  if (IS_ERR(blitter)) {
    printk(KERN_ERR "Failed to get a blitter device: %lld!\n",
           (long long) PTR_ERR(blitter));
    return -ENXIO; /* No such device or address */
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

  /* destination */
#ifdef LOAD_DESTINATION_GAMFILE
  printk(KERN_DEBUG "Loading Destination Gam file %s\n", DST_File_Name);
  res = load_gam_file(DST_File_Name, &Dest);
  if (res < 0) {
    printk(KERN_ERR "Failed to load Dest GAM file: %lld!\n",
           (long long) res);
    goto out_gam_d;
  }
  dst = stm_blitter_surface_new_preallocated(Dest.format,
                                             Dest.colorspace,
                                             &Dest.buffer_address,
                                             Dest.buffer_size,
                                             &Dest.size,
                                             Dest.pitch);
  if (IS_ERR(dst)) {
    res = PTR_ERR(dst);
    printk(KERN_ERR "Failed to create a surface for dest: %lld!\n",
           (long long) res);
    dst = NULL;
    goto out_dst;
  }
#else
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
  /* this is not good (TM)! */
  Dest.memory = phys_to_virt(Dest.buffer_address.base);
#endif


#if defined (CLEAR_DST_SURFACE)
  {
    stm_blitter_color_t color;

    color.a = 0xff;
    color.r = 0x00;
    color.g = 0x00;
    color.b = 0x00;

    printk(KERN_DEBUG "clear destination\n");
    stm_blitter_surface_clear(blitter, dst, &color);
  }
#endif

  stm_blitter_surface_set_blitflags(dst, STM_BLITTER_SBF_NONE);

  printk(KERN_DEBUG "1st copy operation\n");
  src_rects = (stm_blitter_rect_t) { .position = { .x = 0, .y = 0 },
                                     .size = { .w = Dest.size.w / 2,
                                               .h = Dest.size.h / 2 } };
  dst_points = (stm_blitter_point_t) { .x = 0, .y = 0 };

  res = stm_blitter_surface_blit(blitter,
                                 src2, &src_rects, dst, &dst_points, 1);
  if (res < 0) {
    printk(KERN_ERR "simple blit op (1) failed: %lld!\n", (long long) res);
    goto out_blit1;
  }

  /* wait for the blitter operation to have finished */
  stm_blitter_surface_get_serial(dst, &serial);
  stm_blitter_wait(blitter, STM_BLITTER_WAIT_SERIAL, serial);

  save_gam_file("Direct_Copy_Dst_0", &Dest);


  printk(KERN_DEBUG "2nd copy operation\n");
  src_rects = (stm_blitter_rect_t) { .position = { .x = 0, .y = 0 },
                                     .size = { .w = Dest.size.w / 2,
                                               .h = Dest.size.h / 2 } };
  dst_points = (stm_blitter_point_t) { .x = Dest.size.w / 4,
                                       .y = Dest.size.h / 4 };

  res = stm_blitter_surface_blit(blitter,
                                 src2, &src_rects, dst, &dst_points, 1);
  if (res < 0) {
    printk(KERN_ERR "simple blit op (2) failed: %lld!\n", (long long) res);
    goto out_blit2;
  }

  /* wait for the blitter operation to have finished */
  stm_blitter_surface_get_serial(dst, &serial);
  stm_blitter_wait(blitter, STM_BLITTER_WAIT_SERIAL, serial);

  save_gam_file("Direct_Copy_Dst_1", &Dest);


  printk(KERN_DEBUG "3rd copy operation\n");
  src_rects = (stm_blitter_rect_t) { .position = { .x = 0, .y = 0 },
                                     .size = { .w = Dest.size.w,
                                               .h = Dest.size.h } };
  dst_points = (stm_blitter_point_t) { .x = 0, .y = 0 };

  res = stm_blitter_surface_blit(blitter,
                                 src2, &src_rects, dst, &dst_points, 1);
  if (res < 0) {
    printk(KERN_ERR "simple blit op (3) failed: %lld!\n", (long long) res);
    goto out_blit3;
  }

  /* wait for the blitter operation to have finished */
  stm_blitter_surface_get_serial(dst, &serial);
  stm_blitter_wait(blitter, STM_BLITTER_WAIT_SERIAL, serial);

  save_gam_file("Direct_Copy_Dst_2", &Dest);

  printk(KERN_INFO "all tests finished\n");

out_blit3:
out_blit2:
out_blit1:
#ifdef LOAD_DESTINATION_GAMFILE
  stm_blitter_surface_put(dst);
out_dst:
  free_gam_file(&Dest);
out_gam_d:
#else
  dst = NULL;
  free_fb_bpa2_surf(dest);
out_dest:
#endif
  stm_blitter_surface_put(src2);
out_src2:
  free_gam_file(&Src2);
out_gam_s2:

  return res;
}

static void __exit
test_exit(void)
{
  stm_blitter_put(blitter);
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aymen Laarafa <aymen.laarafa@st.com>");
MODULE_DESCRIPTION("A sample blitter direct copy operation");
