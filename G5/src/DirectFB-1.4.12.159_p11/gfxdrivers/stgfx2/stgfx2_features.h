/*
   ST Microelectronics BDispII driver - features of the driver

   (c) Copyright 2008       STMicroelectronics Ltd.

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

#ifndef __STGFX2_FEATURES_H__
#define __STGFX2_FEATURES_H__


/**************************** performance flags *****************************/
/* GraphicsDeviceFuncs::GetSerial() and  GraphicsDeviceFuncs::WaitSerial
   give DirectFB some more fine grained control around flipping, which is
   useful in windowing environments.
   Unfortunately, CPU usage in the stretch blit tests in df_dok goes up from
   sth around 2-3% to > 60% when using a size of 512x512.
   Also, at the moment this is not correctly implemented for multiple
   independent DirectFB instances (on different framebuffers), so disabled
   for now! */
#undef STGFX2_IMPLEMENT_WAITSERIAL


/* use bsearch() when trying to figure out the filter table's base address
   we need to use when scaling (instead of iterating over the list).
   Seems to be faster. */
#define STGFX2_USE_BSEARCH


/***************************** some debug/stats *****************************/
/* define to measure the blitter load. Only makes sense if stmfb
   framebuffer driver's STMCommon/stmbdispaq.cpp was compiled with
   STM_BDISP_MEASURE_BLITLOAD #define'd (see top of that file), too.

   Necessary, if you want df_bltload to display something useful! */
#define STGFX2_MEASURE_BLITLOAD


/* should we printf() the time we had to wait for an entry in the nodelist
   to be available? */
#undef STGFX2_PRINT_NODE_WAIT_TIME


/* should we printf() all the CheckState and SetState operations? */
#undef STGFX2_DUMP_CHECK_STATE
#undef STGFX2_DUMP_CHECK_STATE_FAILED
#undef STGFX2_DUMP_SET_STATE


/************************** implementation details **************************/
/* the following are basically implementation details of this driver and you
   should not need to touch them. */
#define STGFX2_VALID_DRAWINGFLAGS (0                        \
                                   | DSDRAW_BLEND           \
                                   | DSDRAW_XOR             \
                                   | DSDRAW_SRC_PREMULTIPLY \
                                  )

/* we used to accelerate DRAWLINE just for being able to draw horizontal
   and vertical lines in hardware, which is what happens in a
   cairo/gtk/webkit environment. But nowadays DirectFB is intelligent enough
   to draw rectangles instead in this case, so that code was removed. */
#define STGFX2_VALID_DRAWINGFUNCTIONS (0 \
                                       | DFXL_FILLRECTANGLE \
                                       | DFXL_DRAWRECTANGLE \
                                      )

#define STGFX2_VALID_BLITTINGFLAGS (0                            \
                                    | DSBLIT_INDEX_TRANSLATION   \
                                    | DSBLIT_SRC_COLORKEY        \
                                    | DSBLIT_BLEND_COLORALPHA    \
                                    | DSBLIT_BLEND_ALPHACHANNEL  \
                                    | DSBLIT_SRC_PREMULTIPLY     \
                                    | DSBLIT_XOR                 \
                                    | DSBLIT_COLORIZE            \
                                    | DSBLIT_SRC_PREMULTCOLOR    \
                                    \
                                    | DSBLIT_SOURCE2             \
                                    \
                                    | DSBLIT_FIXEDPOINT          \
                                    \
                                    | DSBLIT_ROTATE180           \
                                    | DSBLIT_FLIP_HORIZONTAL     \
                                    | DSBLIT_FLIP_VERTICAL       \
                                   )

#define STGFX2_VALID_BLITTINGFUNCTIONS (DFXL_BLIT \
                                        | DFXL_STRETCHBLIT \
                                        | DFXL_BLIT2)

/* we always interpolate stretch blits */
#define STGFX2_VALID_RENDEROPTS (0                        \
                                 | DSRO_SMOOTH_UPSCALE    \
                                 | DSRO_SMOOTH_DOWNSCALE  \
                                 | DSRO_MATRIX            \
                                )

/* We can support DirectFB's DSRO_SMOOTH_UPSCALE | DSRO_SMOOTH_DOWNSCALE render
   options.
   As this is a (relatively) new flag in DirectFB, we used to behave as if it
   was set to give best image quality.
   Now if we honor this flag applications will actually have to specify whether
   or not they want good looking rescales, which is a major change in default
   behaviour and as such can be tweaked.

   Non-smooth rescale is not well tested and therefore disabled. */
#define STGFX2_FORCE_SMOOTH_SCALE

/* Do we want clipping to be done in hardware? This is needed for better
   looking stretch blit support (at the edges) but it can significantly
   increase CPU and blitter usage because operations are still executed,
   just the result is not written.
   On current SoCs this might also lead to the BDisp accessing memory beyond
   a 64MB bank boundary, thus this should never be enabled! */
#undef STGFX2_SUPPORT_HW_CLIPPING

/* If this is defined, the CLUT will be uploaded only once into the BDisp
   and will be assumed not to change anytime after outside our control.
   So if multiple applications using different CLUTs on different BDisp AQs
   are active (or if somebody is using the kernel interface in parallel) the
   CLUT feature will not work correctly. This feature is therefore disabled
   by default. In earlier versions of this driver we gained a little
   performance increase, but this is not the case anymore, so there's no
   point in enabling this option... */
#undef STGFX2_CLUT_UNSAFE_MULTISESSION


#endif /* __STGFX2_FEATURES_H__ */
