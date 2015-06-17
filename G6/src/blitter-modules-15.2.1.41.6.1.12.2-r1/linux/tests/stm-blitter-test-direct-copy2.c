#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/bpa2.h>
#include <linux/io.h>
#include <linux/stm/blitter.h>

#include "gam_utils/surf_utils.h"
#include "gam_utils/gam_utils.h"


static unsigned int id = 0;
module_param(id, uint, 0000);
MODULE_PARM_DESC(id, "blitter id");

static struct bpa2_part *bpa2;

static int __init
test_init (void)
{
  long res;
  stm_blitter_t *blitter;

  struct blit_fb_bpa2_test_surface *screen;
  stm_blitter_dimension_t           dimension;
  stm_blitter_surface_address_t     buffer;
  size_t                            buffer_size;
  unsigned long                     stride;
  stm_blitter_surface_format_t   fmt;


  bitmap_t Src;
  stm_blitter_surface_t *src;
  stm_blitter_surface_t *dst;

  const stm_blitter_color_t white = { .a = 0xff,
                                      .r = 0xff, .g = 0xff, .b = 0xff };
  const stm_blitter_color_t black = { .a = 0xff,
                                      .r = 0x00, .g = 0x00, .b = 0x00 };

  printk ("%s\n", __func__);

  bpa2 = bpa2_find_part("blitter");
  if (!bpa2) {
    pr_warning("%s: no bpa2 partition 'blitter'\n", __func__);
    return -ENODEV;
  }

  blitter = stm_blitter_get(id);
  if (IS_ERR(blitter)) {
    res = PTR_ERR(blitter);
    blitter = NULL;
    printk("%s: could not get blitter: %lld\n", __func__, (long long) res);
    goto out_blitter;
  }

  dimension.w = 1024;
  dimension.h = 768;

  screen = get_fb_or_bpa2_surf(&dimension, &buffer, &buffer_size, &stride, &fmt);
  if (IS_ERR(screen))
    {
      res = PTR_ERR(screen);
      screen = NULL;
      printk ("%s: error allocating blitter surface: %lld\n",
              __func__, (long long) res);
      goto out_screen;
    }
  stm_blitter_surface_clear(blitter, screen->surf, &black);

//  res = load_gam_file("/tmp/bateau_420yuv.gam", &Src);
  res = load_gam_file("/tmp/hell_420r2b.gam", &Src);
//  res = load_gam_file("/tmp/hell_422r2b.gam", &Src);
  if (res != 0)
    goto out_src_gam;
  src = stm_blitter_surface_new_preallocated(Src.format,
                                             Src.colorspace,
                                             &Src.buffer_address,
                                             Src.buffer_size,
                                             &Src.size,
                                             Src.pitch);
  if (IS_ERR(src)) {
    res = PTR_ERR(src);
    goto out_src_surf;
  }

  buffer.base = bpa2_alloc_pages(bpa2, 2000, 0, GFP_KERNEL);
  buffer.cb_offset = Src.buffer_address.cb_offset;
  buffer.cr_offset = Src.buffer_address.cr_offset;
  if (!buffer.base)
    goto out_dst_bpa2;
  buffer_size = 2000 * PAGE_SIZE;
  dst = stm_blitter_surface_new_preallocated(Src.format,
                                             Src.colorspace,
                                             &buffer,
                                             buffer_size,
                                             &Src.size,
                                             Src.pitch);
  if (IS_ERR(dst)) {
    res = PTR_ERR(dst);
    goto out_dst_surf;
  }
  {
    uint8_t *buf = ioremap(buffer.base, buffer_size);
    memset(buf, 0, buffer_size);
    iounmap(buf);
  }
  stm_blitter_surface_clear(blitter, dst, &white);


  {
    stm_blitter_rect_t src_rect;
    stm_blitter_point_t dst_pt;
    src_rect.position.x = src_rect.position.y = 64;
//    src_rect.position.x = src_rect.position.y = 0;
    src_rect.size.w = src_rect.size.h = 64;
    dst_pt.x = 3;
    dst_pt.y = 3;
    stm_blitter_surface_set_blitflags(dst, STM_BLITTER_SBF_NONE);
//    stm_blitter_surface_blit(blitter, src, &src_rect, dst, NULL, 1);
    stm_blitter_surface_blit(blitter, src, &src_rect, dst, &dst_pt, 1);
//    stm_blitter_surface_blit(blitter, src, NULL, dst, &dst_pt, 1);
//    stm_blitter_surface_blit(blitter, src, NULL, dst, NULL, 1);
  }

  stm_blitter_surface_set_blitflags(screen->surf, STM_BLITTER_SBF_NONE);
  stm_blitter_surface_blit(blitter, dst, NULL, screen->surf, NULL, 1);

  stm_blitter_wait(blitter, STM_BLITTER_WAIT_SYNC, 0);

  {
    bitmap_t gam;
    gam.format = Src.format;
    gam.colorspace = Src.colorspace;
    gam.buffer_size = Src.buffer_size;
    gam.size = Src.size;
    gam.pitch = Src.pitch;
    gam.buffer_address = buffer;
    gam.memory = ioremap(buffer.base, buffer_size);
    save_gam_file("/tmp/copy_save.gam", &gam);
    iounmap(gam.memory);
  }

  res = -ENODEV;

  stm_blitter_surface_put(dst);
out_dst_surf:
  bpa2_free_pages(bpa2, buffer.base);
out_dst_bpa2:
  stm_blitter_surface_put(src);
out_src_surf:
  free_gam_file(&Src);
out_src_gam:
  free_fb_bpa2_surf(screen);
out_screen:
  stm_blitter_put(blitter);
out_blitter:

  return res;
}

static void __exit
test_exit (void)
{
}


module_init(test_init);
module_exit(test_exit);

MODULE_AUTHOR("André Draszik <andre.draszik@st.com>");
MODULE_LICENSE("GPL");
