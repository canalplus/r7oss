/***********************************************************************
 *
 * File: display/ip/stmiqidefaultcolor.h
 * Copyright (c) 2007-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STM_IQI_DEFAULTCOLOR_H
#define _STM_IQI_DEFAULTCOLOR_H

#include <stm_display_plane.h>
#include <vibe_os.h>

#include <display/generic/DisplayInfo.h>
#include <display/ip/hqvdplite/lld/c8fvp3_stddefs.h>
#include <display/ip/hqvdplite/lld/c8fvp3_hqvdplite_api_IQI.h>

#define INIT_DEFAULT_COLOR 128

typedef struct stm_hqvdplite_ColorFill_RGB
{
  uint32_t R;
  uint32_t G;
  uint32_t B;
} stm_hqvdplite_ColorFill_RGB_t;

typedef struct stm_hqvdplite_ColorFill_YCbCr
{
  int32_t Y;
  int32_t Cb;
  int32_t Cr;
} stm_hqvdplite_ColorFill_YCbCr_t;

typedef struct stm_hqvdplite_ColorFill
{
    uint32_t RGB_Value;
    stm_hqvdplite_ColorFill_RGB_t m_RGB;
    stm_hqvdplite_ColorFill_YCbCr_t m_YCbCr;
} stm_hqvdplite_ColorFill_t;



class CSTmIQIDefaultColor
{
public:
  CSTmIQIDefaultColor (void);
  virtual ~CSTmIQIDefaultColor (void);

  bool ColorFill (bool Enable, stm_ycbcr_colorspace_t ColorSpace, uint32_t RgbValue, uint32_t * pdefaultcolor);

private:

  void setDCbt709(stm_hqvdplite_ColorFill_RGB_t Rgb, stm_hqvdplite_ColorFill_YCbCr_t* YCbCr);
  void setDCbt601(stm_hqvdplite_ColorFill_RGB_t Rgb, stm_hqvdplite_ColorFill_YCbCr_t* YCbCr);

  stm_hqvdplite_ColorFill_t m_ColorFillInfo;
};

#endif /* _STM_IQI_DEFAULTCOLOR_H */
