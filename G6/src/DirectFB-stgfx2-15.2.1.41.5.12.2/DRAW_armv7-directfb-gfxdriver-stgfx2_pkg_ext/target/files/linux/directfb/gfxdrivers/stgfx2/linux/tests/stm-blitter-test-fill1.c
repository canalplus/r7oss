#include <linux/module.h>
#include <linux/err.h>
#include <linux/stm/blitter.h>

#include "gam_utils/surf_utils.h"


static stm_blitter_t *blitter[8];

static int __init
test_init (void)
{
  int res;

  struct blit_fb_bpa2_test_surface *surf;
  stm_blitter_dimension_t           dimension;
  stm_blitter_surface_address_t     buffer;
  size_t                            buffer_size;
  unsigned long                     stride;
  stm_blitter_surface_format_t      fmt;

  stm_blitter_color_t      white;
  stm_blitter_serial_t     serial;
  stm_blitter_rect_t       rect;

  printk ("%s\n", __func__);

  white.a = 0xff;
  white.r = 0xff;
  white.g = 0xff;
  white.b = 0xff;

  for (res = 0; res < ARRAY_SIZE (blitter); ++res)
    {
      blitter[res] = stm_blitter_get (res);
      if (IS_ERR (blitter[res]))
        {
          printk ("%s: could not get blitter %d\n", __func__, res);
          blitter[res] = NULL;
          if (res == 0)
            {
              res = -ENODEV;
              goto out_blitter;
            }
        }
    }

  dimension.w = 1024;
  dimension.h = 768;

  surf = get_fb_or_bpa2_surf (&dimension, &buffer, &buffer_size, &stride, &fmt);
  if (IS_ERR (surf))
    {
      res = PTR_ERR (surf);
      printk ("%s: error allocating blitter surface: %lld\n",
              __func__, (long long) res);
      surf = NULL;
      goto out_surface;
    }

  stm_blitter_surface_clear (blitter[0], surf->surf, &white);
  for (res = 0; res < ARRAY_SIZE (blitter); ++res)
    {
      stm_blitter_color_t color;

      if (!blitter[res])
        continue;

      color.a = 0xff;
      color.r = 0x00;
      color.g = 0x20 * res;
      color.b = 0x00;
      stm_blitter_surface_set_color (surf->surf, &color);

      rect.size.w = 10;
      rect.size.h = 10;
      rect.position.x = 10 + 1;
      rect.position.y = 10 + res * rect.size.h;
      printk ("rect: %ld %ld %ld %ld\n", rect.position.x, rect.position.y,
              rect.size.w, rect.size.h);
      stm_blitter_surface_fill_rect (blitter[res], surf->surf, &rect);
    }

  stm_blitter_surface_get_serial (surf->surf, &serial);
  while (--res)
    ;
  stm_blitter_wait (blitter[res], STM_BLITTER_WAIT_SERIAL, serial);

  free_fb_bpa2_surf (surf);

  return 0;

out_surface:
  free_fb_bpa2_surf (surf);
out_blitter:
  {
  int i;
  for (i = 0; i < ARRAY_SIZE (blitter); ++i)
    if (blitter[i]) {
      stm_blitter_put (blitter[i]);
      blitter[i] = NULL;
    }
  }

  return res;
}

static void __exit
test_exit (void)
{
  int i;

  printk ("%s\n", __func__);

  for (i = 0; i < ARRAY_SIZE (blitter); ++i)
    if (blitter[i])
      stm_blitter_put (blitter[i]);

  return;
}


module_init (test_init);
module_exit (test_exit);

MODULE_AUTHOR("André Draszik <andre.draszik@st.com>");
MODULE_LICENSE("GPL");
