/*
 * pd-tests.c
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * Test to exercise Blit Operations with different Porter/Duff settings using the blitter hw
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
#include "crc_utils/crc_utils.h"
#include "gam_utils/surf_utils.h"


/* Test module parameters */
static char *file_name1 = "/root/ARGB8888_1280_720_1.gam";
static bool  use_crc = 0;
static bool  generate_ref = 0;

module_param(file_name1, charp, 0000);
module_param(use_crc, bool, 0644);
module_param(generate_ref, bool, 0644);
MODULE_PARM_DESC(file_name1, "Source1 GAM File Name");


struct pd_rule_entry {
  stm_blitter_porter_duff_rule_t  rule;
  const char                     *name;
};
#define E(pd) \
  { pd, #pd }

static const struct pd_rule_entry pd_rules[] = {
  E(STM_BLITTER_PD_CLEAR),
  E(STM_BLITTER_PD_SOURCE),
  E(STM_BLITTER_PD_DEST),
  E(STM_BLITTER_PD_SOURCE_OVER),
  E(STM_BLITTER_PD_DEST_OVER),
  E(STM_BLITTER_PD_SOURCE_IN),
  E(STM_BLITTER_PD_DEST_IN),
  E(STM_BLITTER_PD_SOURCE_OUT),
  E(STM_BLITTER_PD_DEST_OUT),
  E(STM_BLITTER_PD_SOURCE_ATOP),
  E(STM_BLITTER_PD_DEST_ATOP),
  E(STM_BLITTER_PD_XOR),
};
#undef E

static int __init
test_init(void)
{
  long res;

  stm_blitter_t *blitter;

  bitmap_t               Src1;
  stm_blitter_surface_t *src1;

  bitmap_t                          Dest;
  struct blit_fb_bpa2_test_surface *dest;

  stm_blitter_color_t color;
  stm_blitter_serial_t serial;

  int i;
  int step;


  printk(KERN_INFO "%s\n", __func__);
  if(use_crc)
    init_crc("porterduff.ref", generate_ref);

  /* get a blitter handle */
  blitter = stm_blitter_get(0);
  if (IS_ERR(blitter)) {
    printk(KERN_ERR "Failed to get a blitter device: %lld!\n",
           (long long) PTR_ERR(blitter));
    res = -ENXIO;
    goto out_blitter;
  }

  /* source 1 */
  printk(KERN_DEBUG "Loading Source 1 Gam file %s\n", file_name1);
  res = load_gam_file(file_name1, &Src1);
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

  /* destination */
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
  /* this is not good (TM)! */
  Dest.memory = phys_to_virt(Dest.buffer_address.base);


  /* clear the destination surface */
  color = (stm_blitter_color_t) { .a = 0x00,
                                  .r = 0x00, .g = 0x00, .b = 0x00 };
  stm_blitter_surface_clear(blitter, dest->surf, &color);


  /* Setup drawing flags in some particular use case (permultiplied surf maybe?) */
  stm_blitter_surface_set_drawflags(dest->surf,
                                    (STM_BLITTER_SDF_SRC_PREMULTIPLY
                                     | STM_BLITTER_SDF_BLEND));

  step = Dest.size.w / 5;

  for (i = 0; i < ARRAY_SIZE(pd_rules); ++i) {
    int x = (1 + i % 4) * step;
    int y = (0 + i / 4) * 150;
    const char * const str = pd_rules[i].name;
    stm_blitter_rect_t dst_rect;

    printk(KERN_DEBUG "Porter/Duff rule: %s\n", str);

    stm_blitter_surface_set_porter_duff(dest->surf, STM_BLITTER_PD_SOURCE);

    color = (stm_blitter_color_t) { .a = 140,
                                    .r = 255, .g = 0, .b = 0 };
    stm_blitter_surface_set_color(dest->surf, &color);

    dst_rect = (stm_blitter_rect_t) { .position = { .x = x - 50,
                                                    .y = y + 100 },
                                      .size = { .w = 80,
                                                .h = 70 } };
    res = stm_blitter_surface_fill_rect(blitter, dest->surf, &dst_rect);


    stm_blitter_surface_set_porter_duff(dest->surf, pd_rules[i].rule);
    color = (stm_blitter_color_t) { .a = 200,
                                    .r = 0, .g = 0, .b = 255 };
    stm_blitter_surface_set_color(dest->surf, &color);
    dst_rect.position = (stm_blitter_point_t) { .x = x - 30,
                                                .y = y + 130 };
    res = stm_blitter_surface_fill_rect(blitter, dest->surf, &dst_rect);
    if (res < 0)
      printk(KERN_ERR "fill rect (2) failed (PD: %s, res: %lld)\n", str,
             (long long) res);
  }
  check_crc(Dest.memory, Dest.buffer_size);

  /* Blit the background bitmap */
  stm_blitter_surface_set_blitflags(dest->surf,
                                    (STM_BLITTER_SBF_BLEND_ALPHACHANNEL
                                     | STM_BLITTER_SBF_SRC_COLORIZE));
  stm_blitter_surface_set_porter_duff(dest->surf, STM_BLITTER_PD_DEST_OVER);
  color = (stm_blitter_color_t) { .a = 0x0,
                                  .r = 0x00, .g = 0x88, .b = 0x00 };
  stm_blitter_surface_set_color(dest->surf, &color);
  res = stm_blitter_surface_stretchblit(blitter,
                                        src1, NULL,
                                        dest->surf, NULL,
                                        1);
  if (res) {
    printk(KERN_ERR
           "failed to blit background using P/D DEST_OVER: %lld!\n",
           (long long) res);
    res = 0;
  }

  /* wait for the blitter to have finished all operations on this surface. */
  stm_blitter_surface_get_serial(dest->surf, &serial);
  stm_blitter_wait(blitter, STM_BLITTER_WAIT_SERIAL, serial);
  if(use_crc)
    check_crc(Dest.memory, Dest.buffer_size);

  save_gam_file("/root/dumped_dest_pd_ts", &Dest);


  free_fb_bpa2_surf(dest);
out_dest:
  stm_blitter_surface_put(src1);
out_src1:
  free_gam_file(&Src1);
out_gam_s1:
  stm_blitter_put(blitter);
out_blitter:

  if (!res)
    printk(KERN_DEBUG "Porter/Duff tests finished.\n");
  else
    printk(KERN_ERR "Porter/Duff tests failed!\n");

  if(use_crc)
    term_crc();
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
MODULE_DESCRIPTION("Test different Porter/Duff rules");
