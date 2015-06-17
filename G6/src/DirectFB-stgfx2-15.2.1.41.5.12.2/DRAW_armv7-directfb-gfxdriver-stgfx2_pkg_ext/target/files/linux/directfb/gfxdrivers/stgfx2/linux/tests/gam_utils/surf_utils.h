/*
 * surf_utils.h
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * Surface utilities (alloc from BPA2 or framebuffer) for test purposes
 */

#ifndef __STM_BLITTER_SURF_UTILS_H__
#define __STM_BLITTER_SURF_UTILS_H__

#include <linux/stm/blitter.h>


struct blit_fb_bpa2_test_surface {
  stm_blitter_surface_t *surf;
  void                  *priv;
};


void free_fb_bpa2_surf (struct blit_fb_bpa2_test_surface *surf);
struct blit_fb_bpa2_test_surface *
get_fb_or_bpa2_surf (stm_blitter_dimension_t       *dim,
                     stm_blitter_surface_address_t *buffer,
                     size_t                        *buffer_size,
                     unsigned long                 *pitch,
                     stm_blitter_surface_format_t  *fmt);


#endif /* __STM_BLITTER_SURF_UTILS_H__) */
