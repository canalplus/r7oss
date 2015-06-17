/*
   ST Microelectronics BDispII driver - hardware acceleration

   (c) Copyright 2007-2009  STMicroelectronics Ltd.

   All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __BDISP_ACCEL_H__
#define __BDISP_ACCEL_H__


#include <linux/stm/blitter.h>
#include "bdisp2/bdispII_aq_state.h"
#include "bdisp2/bdispII_device_features.h"

int
bdisp2_init_features_by_type (struct bdisp2_features    *features,
                              struct stm_plat_blit_data *plat_blit);

int
bdisp2_initialize (struct stm_bdisp2_driver_data *stdrv,
                   struct stm_bdisp2_device_data *stdev);
void
bdisp2_cleanup (struct stm_bdisp2_driver_data *stdrv,
                struct stm_bdisp2_device_data *stdev);


void
bdisp2_engine_reset (struct stm_bdisp2_driver_data *stdrv,
                     struct stm_bdisp2_device_data *stdev);

void
bdisp2_wait_event (struct stm_bdisp2_driver_data       *stdrv,
                   const struct stm_bdisp2_device_data *stdev,
                   const stm_blitter_serial_t          *serial);
int
bdisp2_engine_sync (struct stm_bdisp2_driver_data       *stdrv,
                    const struct stm_bdisp2_device_data *stdev);

int
bdisp2_fence (struct stm_bdisp2_driver_data *stdrv,
              struct stm_bdisp2_device_data *stdev);

void
bdisp2_emit_commands (struct stm_bdisp2_driver_data       *stdrv,
                      const struct stm_bdisp2_device_data *stdev,
                      bool                                 is_locked);

void
bdisp2_get_serial (const struct stm_bdisp2_driver_data *stdrv,
                   const struct stm_bdisp2_device_data *stdev,
                   stm_blitter_serial_t                *serial);

void
bdisp2_wait_serial (struct stm_bdisp2_driver_data *stdrv,
                    struct stm_bdisp2_device_data *stdev,
                    stm_blitter_serial_t           serial);


int
bdisp2_fill_draw_nop (struct stm_bdisp2_driver_data       *stdrv,
                      const struct stm_bdisp2_device_data *stdev,
                      const stm_blitter_rect_t            *dst);

int
bdisp2_fill_rect_simple (struct stm_bdisp2_driver_data       *stdrv,
                         const struct stm_bdisp2_device_data *stdev,
                         const stm_blitter_rect_t            *dst);
int
bdisp2_fill_rect (struct stm_bdisp2_driver_data       *stdrv,
                  const struct stm_bdisp2_device_data *stdev,
                  const stm_blitter_rect_t            *dst);

int
bdisp2_draw_rect_simple (struct stm_bdisp2_driver_data       *stdrv,
                         const struct stm_bdisp2_device_data *stdev,
                         const stm_blitter_rect_t            *dst);
int
bdisp2_draw_rect (struct stm_bdisp2_driver_data       *stdrv,
                  const struct stm_bdisp2_device_data *stdev,
                  const stm_blitter_rect_t            *dst);

int
bdisp2_blit_simple (struct stm_bdisp2_driver_data *stdrv,
                    struct stm_bdisp2_device_data *stdev,
                    const stm_blitter_rect_t      *src,
                    const stm_blitter_point_t     *dst_pt);
int
bdisp2_blit_simple_YCbCr422r (struct stm_bdisp2_driver_data *stdrv,
                              struct stm_bdisp2_device_data *stdev,
                              const stm_blitter_rect_t      *src,
                              const stm_blitter_point_t     *dst_pt);
int
bdisp2_blit_swap_YCbCr422r (struct stm_bdisp2_driver_data *stdrv,
                            struct stm_bdisp2_device_data *stdev,
                            const stm_blitter_rect_t      *src,
                            const stm_blitter_point_t     *dst_pt);
int
bdisp2_blit_simple_NV422 (struct stm_bdisp2_driver_data *stdrv,
                          struct stm_bdisp2_device_data *stdev,
                          const stm_blitter_rect_t      *src,
                          const stm_blitter_point_t     *dst_pt);
int
bdisp2_blit_simple_NV420 (struct stm_bdisp2_driver_data *stdrv,
                          struct stm_bdisp2_device_data *stdev,
                          const stm_blitter_rect_t      *src,
                          const stm_blitter_point_t     *dst_pt);
int
bdisp2_blit_simple_YV444 (struct stm_bdisp2_driver_data *stdrv,
                          struct stm_bdisp2_device_data *stdev,
                          const stm_blitter_rect_t      *src,
                          const stm_blitter_point_t     *dst_pt);
int
bdisp2_blit_simple_YV422 (struct stm_bdisp2_driver_data *stdrv,
                          struct stm_bdisp2_device_data *stdev,
                          const stm_blitter_rect_t      *src,
                          const stm_blitter_point_t     *dst_pt);
int
bdisp2_blit_simple_YV420 (struct stm_bdisp2_driver_data *stdrv,
                          struct stm_bdisp2_device_data *stdev,
                          const stm_blitter_rect_t      *src,
                          const stm_blitter_point_t     *dst_pt);


int
bdisp2_blit (struct stm_bdisp2_driver_data  *stdrv,
             struct stm_bdisp2_device_data  *stdev,
             const stm_blitter_rect_t       *src,
             const stm_blitter_point_t      *dst_pt);
int
bdisp2_stretch_blit (struct stm_bdisp2_driver_data *stdrv,
                     struct stm_bdisp2_device_data *stdev,
                     const stm_blitter_rect_t      *src,
                     const stm_blitter_rect_t      *dst);
int
bdisp2_stretch_blit_MB(struct stm_bdisp2_driver_data *stdrv,
                       struct stm_bdisp2_device_data *stdev,
                       const stm_blitter_rect_t      *src,
                       const stm_blitter_rect_t      *dst);
int
bdisp2_blit_as_stretch (struct stm_bdisp2_driver_data *stdrv,
                        struct stm_bdisp2_device_data *stdev,
                        const stm_blitter_rect_t      *src,
                        const stm_blitter_point_t     *dst_pt);
int
bdisp2_blit_as_stretch_MB (struct stm_bdisp2_driver_data *stdrv,
                           struct stm_bdisp2_device_data *stdev,
                           const stm_blitter_rect_t      *src,
                           const stm_blitter_point_t     *dst_pt);
int
bdisp2_stretch_blit_planar_4xx (struct stm_bdisp2_driver_data *stdrv,
                                struct stm_bdisp2_device_data *stdev,
                                const stm_blitter_rect_t      *src,
                                const stm_blitter_rect_t      *dst);
int
bdisp2_stretch_blit_MB4xx (struct stm_bdisp2_driver_data *stdrv,
                           struct stm_bdisp2_device_data *stdev,
                           const stm_blitter_rect_t      *src,
                           const stm_blitter_rect_t      *dst);
int
bdisp2_blit_as_stretch_planar_4xx (struct stm_bdisp2_driver_data *stdrv,
                                   struct stm_bdisp2_device_data *stdev,
                                   const stm_blitter_rect_t      *src,
                                   const stm_blitter_point_t     *dst_pt);
int
bdisp2_blit_as_stretch_MB4xx (struct stm_bdisp2_driver_data *stdrv,
                              struct stm_bdisp2_device_data *stdev,
                              const stm_blitter_rect_t      *src,
                              const stm_blitter_point_t     *dst_pt);
int
bdisp2_blit2_as_stretch_MB (struct stm_bdisp2_driver_data * const stdrv,
                            struct stm_bdisp2_device_data * const stdev,
                            const stm_blitter_rect_t      * const src1,
                            const stm_blitter_point_t     * const src2_pt,
                            const stm_blitter_point_t     * const dst_pt);

int
bdisp2_blit2_as_stretch (struct stm_bdisp2_driver_data * const stdrv,
                         struct stm_bdisp2_device_data * const stdev,
                         const stm_blitter_rect_t      * const src1,
                         const stm_blitter_point_t     * const src2_pt,
                         const stm_blitter_point_t     * const dst_pt);
int
bdisp2_blit2 (struct stm_bdisp2_driver_data *stdrv,
              struct stm_bdisp2_device_data *stdev,
              const stm_blitter_rect_t      *src1,
              const stm_blitter_point_t     *src2_pt,
              const stm_blitter_point_t     *dst_pt);

int
bdisp2_blit_shortcut (struct stm_bdisp2_driver_data *stdrv,
                      struct stm_bdisp2_device_data *stdev,
                      const stm_blitter_rect_t      *src,
                      const stm_blitter_point_t     *dst_pt);
int
bdisp2_stretch_blit_shortcut (struct stm_bdisp2_driver_data *stdrv,
                              struct stm_bdisp2_device_data *stdev,
                              const stm_blitter_rect_t      *src,
                              const stm_blitter_rect_t      *dst);
int
bdisp2_blit2_shortcut (struct stm_bdisp2_driver_data *stdrv,
                       struct stm_bdisp2_device_data *stdev,
                       const stm_blitter_rect_t      *src1,
                       const stm_blitter_point_t     *src2_pt,
                       const stm_blitter_point_t     *dst_pt);

int
bdisp2_blit_shortcut_YCbCr422r (struct stm_bdisp2_driver_data *stdrv,
                                struct stm_bdisp2_device_data *stdev,
                                const stm_blitter_rect_t      *src,
                                const stm_blitter_point_t     *dst_pt);
int
bdisp2_stretch_blit_shortcut_YCbCr422r (struct stm_bdisp2_driver_data *stdrv,
                                        struct stm_bdisp2_device_data *stdev,
                                        const stm_blitter_rect_t      *src,
                                        const stm_blitter_rect_t      *dst);
int
bdisp2_blit2_shortcut_YCbCr422r (struct stm_bdisp2_driver_data *stdrv,
                                 struct stm_bdisp2_device_data *stdev,
                                 const stm_blitter_rect_t      *src1,
                                 const stm_blitter_point_t     *src2_pt,
                                 const stm_blitter_point_t     *dst_pt);

int
bdisp2_blit_shortcut_rgb32 (struct stm_bdisp2_driver_data *stdrv,
                            struct stm_bdisp2_device_data *stdev,
                            const stm_blitter_rect_t      *src,
                            const stm_blitter_point_t     *dst_pt);
int
bdisp2_stretch_blit_shortcut_rgb32 (struct stm_bdisp2_driver_data *stdrv,
                                    struct stm_bdisp2_device_data *stdev,
                                    const stm_blitter_rect_t      *src,
                                    const stm_blitter_rect_t      *dst);
int
bdisp2_blit2_shortcut_rgb32 (struct stm_bdisp2_driver_data *stdrv,
                             struct stm_bdisp2_device_data *stdev,
                             const stm_blitter_rect_t      *src1,
                             const stm_blitter_point_t     *src2_pt,
                             const stm_blitter_point_t     *dst_pt);

int
bdisp2_blit_rotate_90_270 (struct stm_bdisp2_driver_data *stdrv,
                           struct stm_bdisp2_device_data *stdev,
                           const stm_blitter_rect_t      *src,
                           const stm_blitter_point_t     *dst_pt);

int
bdisp2_blit_nop (struct stm_bdisp2_driver_data *stdrv,
                 struct stm_bdisp2_device_data *stdev,
                 const stm_blitter_rect_t      *src,
                 const stm_blitter_point_t     *dst_pt);
int
bdisp2_stretch_blit_nop (struct stm_bdisp2_driver_data *stdrv,
                         struct stm_bdisp2_device_data *stdev,
                         const stm_blitter_rect_t      *src,
                         const stm_blitter_rect_t      *dst);
int
bdisp2_blit2_nop (struct stm_bdisp2_driver_data *stdrv,
                  struct stm_bdisp2_device_data *stdev,
                  const stm_blitter_rect_t      *src1,
                  const stm_blitter_point_t     *src2_pt,
                  const stm_blitter_point_t     *dst_pt);


void
bdisp_aq_setup_blit_operation (struct stm_bdisp2_driver_data *stdrv,
                               struct stm_bdisp2_device_data *stdev);
void
bdisp2_prepare_upload_palette_hw (struct stm_bdisp2_driver_data *stdrv,
                                  struct stm_bdisp2_device_data *stdev);


int
bdisp2_stretch_blit_RLE (struct stm_bdisp2_driver_data *stdrv,
                         struct stm_bdisp2_device_data *stdev,
                         unsigned long                  src_address,
                         unsigned long                  src_length,
                         unsigned long                  dst_address,
                         stm_blitter_surface_format_t   dst_format,
                         uint16_t                       dst_pitch,
                         const stm_blitter_rect_t      *drect);


void
bdisp2_RGB32_init (struct stm_bdisp2_driver_data *stdrv,
                   struct stm_bdisp2_device_data *stdev,
                   uint32_t                       blt_tba,
                   uint16_t                       pitch,
                   const stm_blitter_rect_t      *dst);

void
bdisp2_RGB32_fixup (struct stm_bdisp2_driver_data *stdrv,
                    struct stm_bdisp2_device_data *stdev,
                    uint32_t                       blt_tba,
                    uint16_t                       pitch,
                    const stm_blitter_rect_t      *dst);


#endif /* __BDISP_ACCEL_H__ */
