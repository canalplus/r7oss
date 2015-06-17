/*
   ST Microelectronics BDispII driver - distinguishing hardware

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

#include "bdisp_features.h"

#include "bdisp_registers.h"

#include <direct/util.h>
#include <string.h>

/* the following are hardware features and they will be different depending
   on the BDisp implementation we're running on. */
#define BDISP_HW_FEATURES_BLIT_OPS (0                                        \
                                    | STM_BLITTER_FLAGS_FLICKERFILTER        \
                                   )

#define FORMAT_YUV444P      (1 << 3)
#define FORMAT_YCBCR42XMBN  (1 << 4)

/* rotation and mirroring */
#define STM_BLITTER_FLAGS_ROTATE90   (0x00000040)
#define STM_BLITTER_FLAGS_ROTATE270  (0x00000100)

/* this is the value for pre-7108 */
#define BDISP_DEFAULT_MAX_SPANWIDTH   (128)
#define BDISP_DEFAULT_MAX_ROTATEWIDTH ( 16)


static struct _bdisp_hw_features bdisp_hw_features[] = {
  /* other versions are not supported! */
  [STM_BLITTER_VERSION_7109c2] =
    { .name = "STb7109c2",
      .blitflags = BDISP_HW_FEATURES_BLIT_OPS },
  [STM_BLITTER_VERSION_7109c3] =
    { .name = "STb7109c3",
      .blitflags = BDISP_HW_FEATURES_BLIT_OPS },
  [STM_BLITTER_VERSION_7200c1] =
    { .name = "STi7200c1",
      .blitflags = (BDISP_HW_FEATURES_BLIT_OPS
                    | STM_BLITTER_FLAGS_RLE_DECODE) },
  [STM_BLITTER_VERSION_7200c2_7111_7141_7105] =
    { .name = "STi7200c2/7111/7141/7105",
      .blitflags = (BDISP_HW_FEATURES_BLIT_OPS
                    | STM_BLITTER_FLAGS_RLE_DECODE
                    | STM_BLITTER_FLAGS_ROTATE90
                    | STM_BLITTER_FLAGS_ROTATE270),
      .extra_src_formats = (FORMAT_YUV444P
                            | FORMAT_YCBCR42XMBN) },
  [STM_BLITTER_VERSION_5197] =
    /* don't know if destination color key works here - just a safe guess */
    { .name = "STb5197",
      .blitflags = (BDISP_HW_FEATURES_BLIT_OPS
                    | STM_BLITTER_FLAGS_ROTATE90
                    | STM_BLITTER_FLAGS_ROTATE270),
      .extra_src_formats = (FORMAT_YUV444P
                            | FORMAT_YCBCR42XMBN) },
  [STM_BLITTER_VERSION_5206] =
    { .name = "STi5206",
      .line_buffer_length = 720,
      .rotate_buffer_length = 64,
      .blitflags = (BDISP_HW_FEATURES_BLIT_OPS
                    | STM_BLITTER_FLAGS_ROTATE90
                    | STM_BLITTER_FLAGS_ROTATE270),
      .extra_src_formats = FORMAT_YUV444P },
  [STM_BLITTER_VERSION_7106_7108] =
    { .name = "STi7106/7108",
      .line_buffer_length = 720,
      .rotate_buffer_length = 64,
      .blitflags = (BDISP_HW_FEATURES_BLIT_OPS
                    | STM_BLITTER_FLAGS_RLE_DECODE
                    | STM_BLITTER_FLAGS_ROTATE90
                    | STM_BLITTER_FLAGS_ROTATE270),
      .extra_src_formats = FORMAT_YUV444P },
};


static const struct _bdisp_pixelformat_table dspf_to_bdisp[DFB_NUM_PIXELFORMATS] = {
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB1555)] = { BLIT_COLOR_FORM_ARGB1555,                              true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB16)]    = { BLIT_COLOR_FORM_RGB565,                                true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB24)]    = { BLIT_COLOR_FORM_RGB888,                                true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB32)]    = { BLIT_COLOR_FORM_ARGB8888 | BLIT_TY_FULL_ALPHA_RANGE,   true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB)]     = { BLIT_COLOR_FORM_ARGB8888 | BLIT_TY_FULL_ALPHA_RANGE,   true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_A8)]       = { BLIT_COLOR_FORM_A8 | BLIT_TY_FULL_ALPHA_RANGE,         true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_YUY2)]     = { BLIT_COLOR_FORM_YCBCR422R | BLIT_TY_BIG_ENDIAN,        true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB332)]   = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_UYVY)]     = { BLIT_COLOR_FORM_YCBCR422R,                             true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_I420)]     = { BLIT_COLOR_FORM_YUV444P,                               true, false },
  [DFB_PIXELFORMAT_INDEX (DSPF_YV12)]     = { BLIT_COLOR_FORM_YUV444P,                               true, false },
  [DFB_PIXELFORMAT_INDEX (DSPF_LUT8)]     = { BLIT_COLOR_FORM_CLUT8,                                 true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_ALUT44)]   = { BLIT_COLOR_FORM_ACLUT44,                               true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_AiRGB)]    = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_A1)]       = { BLIT_COLOR_FORM_A1,                                    true, false },
/* Right now, JPEGDECHW/DeltaMu output an Omega2 MB YCbCr buffer. But future versions of JPEGDECHW
   will/can be made (depends on the platform?) to output a raster buffer instead. DSPF_NV12 is actually
   YCBCR420R2B, NV16 is YCBCR422R2B. But let's fake it for the moment. */
#ifndef JPEGDECHW_DELTAMU_RASTER_BUFFERS /* Not defined. */
  [DFB_PIXELFORMAT_INDEX (DSPF_NV12)]     = { BLIT_COLOR_FORM_YCBCR42XMB,                            true, false },
  [DFB_PIXELFORMAT_INDEX (DSPF_NV16)]     = { BLIT_COLOR_FORM_YCBCR42XMB,                            true, false },
#else
  [DFB_PIXELFORMAT_INDEX (DSPF_NV12)]     = { BLIT_COLOR_FORM_YCBCR42XR2B,                           true, false },
  [DFB_PIXELFORMAT_INDEX (DSPF_NV16)]     = { BLIT_COLOR_FORM_YCBCR42XR2B,                           true, false },
#endif
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB2554)] = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB4444)] = { BLIT_COLOR_FORM_ARGB4444,                              true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_RGBA4444)] = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_NV21)]     = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_AYUV)]     = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_A4)]       = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB1666)] = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB6666)] = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB18)]    = { -1, },
  /* CLUT2 is not supported as a target type on BDisp at the moment */
  [DFB_PIXELFORMAT_INDEX (DSPF_LUT2)]     = { BLIT_COLOR_FORM_CLUT2,                                 true, false },
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB444)]   = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB555)]   = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_BGR555)]   = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_RGBA5551)] = { -1, },
  [DFB_PIXELFORMAT_INDEX (DSPF_YUV444P)]  = { BLIT_COLOR_FORM_YUV444P,                               true, false },
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB8565)] = { BLIT_COLOR_FORM_ARGB8565 | BLIT_TY_FULL_ALPHA_RANGE,   true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_AVYU)]     = { BLIT_COLOR_FORM_AYCBCR8888 | BLIT_TY_FULL_ALPHA_RANGE, true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_VYU)]      = { BLIT_COLOR_FORM_YCBCR888,                              true, true },
  [DFB_PIXELFORMAT_INDEX (DSPF_A1_LSB)]   = { -1 },
  [DFB_PIXELFORMAT_INDEX (DSPF_YV16)]     = { BLIT_COLOR_FORM_YUV444P,                               true, false },
#if 0
  /* unsupported for now */
  [DFB_PIXELFORMAT_INDEX (DSPF_BGRA)]     = { BLIT_COLOR_FORM_ARGB8888 | BLIT_TY_FULL_ALPHA_RANGE | BLIT_TY_BIG_ENDIAN,
  [DFB_PIXELFORMAT_INDEX (DSPF_LUT1)]     = { BLIT_COLOR_FORM_CLUT1,
  [DFB_PIXELFORMAT_INDEX (DSPF_LUT4)]     = { BLIT_COLOR_FORM_CLUT4,
  [DFB_PIXELFORMAT_INDEX (DSPF_ALUT88)]   = { BLIT_COLOR_FORM_ACLUT88 | BLIT_TY_FULL_ALPHA_RANGE,
#endif
};


bool
bdisp_hw_features_set_bdisp (enum stm_blitter_device    device,
                             struct _bdisp_hw_features *ret_features)
{
  struct _bdisp_hw_features *features = &bdisp_hw_features[device];

  if (device >= D_ARRAY_SIZE (bdisp_hw_features))
    return false;

  strncpy (ret_features->name, features->name, sizeof (ret_features->name));

  ret_features->line_buffer_length = features->line_buffer_length;
  ret_features->rotate_buffer_length = features->rotate_buffer_length;
  if (!ret_features->line_buffer_length)
    ret_features->line_buffer_length = BDISP_DEFAULT_MAX_SPANWIDTH;
  if (!ret_features->rotate_buffer_length)
    ret_features->rotate_buffer_length = BDISP_DEFAULT_MAX_ROTATEWIDTH;

  ret_features->blitflags = features->blitflags;

  ret_features->dfb_drawflags = STGFX2_VALID_DRAWINGFLAGS;
  ret_features->dfb_blitflags = STGFX2_VALID_BLITTINGFLAGS;
  ret_features->dfb_renderopts = STGFX2_VALID_RENDEROPTS;

  /* rotation is not available on all platforms */
  if (D_FLAGS_IS_SET (features->blitflags, STM_BLITTER_FLAGS_ROTATE90))
    ret_features->dfb_blitflags |= DSBLIT_ROTATE90;
  if (D_FLAGS_IS_SET (features->blitflags, STM_BLITTER_FLAGS_ROTATE270))
    ret_features->dfb_blitflags |= DSBLIT_ROTATE270;

  memcpy (ret_features->dfb_dspf_to_bdisp, dspf_to_bdisp,
          sizeof (dspf_to_bdisp));
  if (!D_FLAGS_IS_SET (features->extra_src_formats, FORMAT_YUV444P))
    {
      ret_features->dfb_dspf_to_bdisp[DFB_PIXELFORMAT_INDEX (DSPF_YUV444P)].supported_as_src = false;
      ret_features->dfb_dspf_to_bdisp[DFB_PIXELFORMAT_INDEX (DSPF_I420)].supported_as_src = false;
      ret_features->dfb_dspf_to_bdisp[DFB_PIXELFORMAT_INDEX (DSPF_YV12)].supported_as_src = false;
      ret_features->dfb_dspf_to_bdisp[DFB_PIXELFORMAT_INDEX (DSPF_YV16)].supported_as_src = false;
    }
//  if (D_FLAGS_IS_SET (features->extra_src_formats, FORMAT_YCBCR42XMBN))
//    ret_features->dfb_dspf_to_bdisp[DFB_PIXELFORMAT_INDEX (DSPF_YCBCR42XMBN)].supported_as_src = true;

  return true;
}
