/*
 * surf_utils.c
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * Surface utilities (alloc from BPA2 or framebuffer) for test purposes
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bpa2.h>
#include <linux/fb.h>

#include <linux/stm/blitter.h>

#include "surf_utils.h"

struct blit_fb_bpa2_test_surface_mine {
  struct bpa2_part              *bpa2;
  struct fb_info                *info;
  stm_blitter_surface_address_t  buffer;
};

void
free_fb_bpa2_surf (struct blit_fb_bpa2_test_surface * const surf)
{
  struct blit_fb_bpa2_test_surface_mine * const mine = surf->priv;

  if (surf->surf)
    stm_blitter_surface_put (surf->surf);
  if (mine->bpa2)
    bpa2_free_pages (mine->bpa2, mine->buffer.base);
  if (mine->info) {
    if (mine->info->fbops->fb_release)
      mine->info->fbops->fb_release (mine->info, 0);
    module_put (mine->info->fbops->owner);
  }

  kfree (surf);
}
EXPORT_SYMBOL(free_fb_bpa2_surf);

struct blit_fb_bpa2_test_surface *
get_fb_or_bpa2_surf (stm_blitter_dimension_t       * const dim,
                     stm_blitter_surface_address_t * const buffer,
                     size_t                        * const buffer_size,
                     unsigned long                 * const pitch,
                     stm_blitter_surface_format_t  * const fmt)
{
  struct blit_fb_bpa2_test_surface *surf;
  struct blit_fb_bpa2_test_surface_mine *mine;
  int res;

  surf = kzalloc (sizeof (*surf), GFP_KERNEL);
  mine = kzalloc (sizeof (*mine), GFP_KERNEL);
  if (!surf || !mine) {
    kfree (mine);
    kfree (surf);
    return ERR_PTR (-ENOMEM);
  }
  surf->priv = mine;

  for (res = 0; res < num_registered_fb; ++res) {
    if (registered_fb[res]) {
      mine->info = registered_fb[res];
      printk ("found a registered framebuffer @ %p\n", mine->info);
      break;
    }
  }

  if (mine->info) {
    struct module *owner = mine->info->fbops->owner;

    if (try_module_get (owner)) {
      if (mine->info->fbops->fb_open
          && !mine->info->fbops->fb_open (mine->info, 0)) {
        switch (mine->info->var.bits_per_pixel) {
          case  8: *fmt = STM_BLITTER_SF_LUT8; break;
          case 16: *fmt = STM_BLITTER_SF_RGB565; break;
          case 24: *fmt = STM_BLITTER_SF_RGB24; break;
          case 32: *fmt = STM_BLITTER_SF_ARGB; break;
          default:
            if (mine->info->fbops->fb_release)
              mine->info->fbops->fb_release (mine->info, 0);
            module_put (owner);
            mine->info = NULL;
            break;
        }

        if (mine->info) {
          mine->buffer.base = mine->info->fix.smem_start;
          *buffer_size = mine->info->fix.smem_len;
          *pitch = mine->info->var.xres * mine->info->var.bits_per_pixel / 8;
          dim->w = mine->info->var.xres;
          dim->h = mine->info->var.yres_virtual;
          printk ("fb: %lx %zu %ld\n", mine->buffer.base, *buffer_size, *pitch);
        }
      }
      else {
        module_put (owner);
        printk ("no fb_open()\n");
        mine->info = NULL;
      }
    }
    else {
      printk ("failed try_module_get\n");
      mine->info = NULL;
    }
  }

  if (!mine->info) {
    mine->bpa2 = bpa2_find_part ("bigphysarea");
    if (!mine->bpa2) {
      printk ("%s: no bpa2 partition 'bigphysarea'\n", __func__);
      res = -ENOENT;
      goto out;
    }

    printk ("using memory from BPA2\n");
    *fmt = STM_BLITTER_SF_ARGB;
    *pitch = dim->w * 4;
    *buffer_size = *pitch * dim->h;
    mine->buffer.base = bpa2_alloc_pages (mine->bpa2,
                                          ((*buffer_size + PAGE_SIZE - 1)
                                           / PAGE_SIZE),
                                          PAGE_SIZE, GFP_KERNEL);
    if (!mine->buffer.base) {
      printk ("%s: could not allocate %zu bytes from bpa2 partition\n",
              __func__, *buffer_size);
      res = -ENOMEM;
      goto out;
    }
    printk ("%s: surface @ %.8lx\n", __func__, mine->buffer.base);
  }

  surf->surf = stm_blitter_surface_new_preallocated (*fmt,
                                                     STM_BLITTER_SCS_RGB,
                                                     &mine->buffer,
                                                     *buffer_size, dim, *pitch);
  if (IS_ERR (surf->surf)) {
    res = PTR_ERR (surf->surf);
    printk ("%s: error allocating blitter surface: %lld\n",
            __func__, (long long) res);
    goto out;
  }

  *buffer = mine->buffer;

  return surf;

out:
  free_fb_bpa2_surf (surf);
  return ERR_PTR (res);
}
EXPORT_SYMBOL(get_fb_or_bpa2_surf);
