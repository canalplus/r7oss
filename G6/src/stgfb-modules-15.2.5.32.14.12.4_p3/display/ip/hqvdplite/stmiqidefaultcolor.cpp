/***********************************************************************
 *
 * File: display/ip/stmiqi.cpp
 * Copyright (c) 2007-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include "stmiqidefaultcolor.h"

#define CLIP(x,low,up) ( ((x)<(low))?(low):( ((x)>(up))?(up):(x) ) )

CSTmIQIDefaultColor::CSTmIQIDefaultColor (void)
{
    m_ColorFillInfo.RGB_Value = 0;
    m_ColorFillInfo.m_RGB.R = 0;
    m_ColorFillInfo.m_RGB.G = 0;
    m_ColorFillInfo.m_RGB.B = 0;
    m_ColorFillInfo.m_YCbCr.Y = 0;
    m_ColorFillInfo.m_YCbCr.Cb = 0;
    m_ColorFillInfo.m_YCbCr.Cr = 0;
}


CSTmIQIDefaultColor::~CSTmIQIDefaultColor (void)
{

}

void CSTmIQIDefaultColor::setDCbt709(stm_hqvdplite_ColorFill_RGB_t Rgb, stm_hqvdplite_ColorFill_YCbCr_t* YCbCr)
{
    /* RGB 8 bits -> Y Cb Cr 10 bits BT 709 */
    YCbCr->Y = (306 * Rgb.R) + (601 * Rgb.G) + (116 * Rgb.B);
    YCbCr->Cb = (-150 * Rgb.R) - (295 * Rgb.G) + (446 * Rgb.B);
    YCbCr->Cr = (629 * Rgb.R) - (527 * Rgb.G) - (102* Rgb.B);

    YCbCr->Y = YCbCr->Y / 255;
    YCbCr->Cb = (YCbCr->Cb / 255) + 512;
    YCbCr->Cr = (YCbCr->Cr / 255) + 512;

    YCbCr->Y = CLIP(YCbCr->Y, 64, 940);
    YCbCr->Cb = CLIP(YCbCr->Cb, 64, 960);
    YCbCr->Cr = CLIP(YCbCr->Cr, 64, 960);
}

void CSTmIQIDefaultColor::setDCbt601(stm_hqvdplite_ColorFill_RGB_t Rgb, stm_hqvdplite_ColorFill_YCbCr_t* YCbCr)
{
  /* RGB 8 bits -> Y Cb Cr 10 bits BT 601 */
  YCbCr->Y = (306 * Rgb.R) + (601 * Rgb.G) + (116 * Rgb.B);
  YCbCr->Cb = (-172 * Rgb.R) - (339 * Rgb.G) + (512 * Rgb.B);
  YCbCr->Cr = (512 * Rgb.R) - (428 * Rgb.G) - (83* Rgb.B);

  YCbCr->Y = YCbCr->Y / 255;
  YCbCr->Cb = (YCbCr->Cb / 255) + 512;
  YCbCr->Cr = (YCbCr->Cr / 255) + 512;

  YCbCr->Y = CLIP(YCbCr->Y, 64, 940);
  YCbCr->Cb = CLIP(YCbCr->Cb, 64, 940);
  YCbCr->Cr = CLIP(YCbCr->Cr, 64, 940);
}

bool
CSTmIQIDefaultColor::ColorFill ( bool Enable, stm_ycbcr_colorspace_t ColorSpace, uint32_t RgbValue, uint32_t * pdefaultcolor)
{
    TRC( TRC_ID_MAIN_INFO, "Enable = 0x%08X RgbValue=0x%08x", Enable, RgbValue );

    if ( !Enable ) {
        /* set bit IQI_DEFAULT_COLOR_DEFAULT_COLOR_CONTROL_SHIFT to 0 to disable default_color feature */
        *pdefaultcolor = (0 <<  IQI_DEFAULT_COLOR_DEFAULT_COLOR_CONTROL_SHIFT) &  IQI_DEFAULT_COLOR_DEFAULT_COLOR_CONTROL_MASK;
    } else {
      m_ColorFillInfo.m_RGB.R =(RgbValue & 0x00ff0000) >> 16;
      m_ColorFillInfo.m_RGB.G = (RgbValue & 0x0000ff00) >> 8;
      m_ColorFillInfo.m_RGB.B  = (RgbValue & 0x000000ff);

      if (ColorSpace==STM_YCBCR_COLORSPACE_601) {
        setDCbt601(m_ColorFillInfo.m_RGB, &m_ColorFillInfo.m_YCbCr);
      } else  {
        setDCbt709(m_ColorFillInfo.m_RGB, &m_ColorFillInfo.m_YCbCr);
      }
      *pdefaultcolor =   ((m_ColorFillInfo.m_YCbCr.Cr << IQI_DEFAULT_COLOR_CHROMA_RED_SHIFT) & IQI_DEFAULT_COLOR_CHROMA_RED_MASK) |
      ((m_ColorFillInfo.m_YCbCr.Cb << IQI_DEFAULT_COLOR_CHROMA_BLUE_SHIFT) & IQI_DEFAULT_COLOR_CHROMA_BLUE_MASK) |
      ((m_ColorFillInfo.m_YCbCr.Y << IQI_DEFAULT_COLOR_LUMA_DATA_SHIFT ) & IQI_DEFAULT_COLOR_LUMA_DATA_MASK ) |
      ((1 <<  IQI_DEFAULT_COLOR_DEFAULT_COLOR_CONTROL_SHIFT) &  IQI_DEFAULT_COLOR_DEFAULT_COLOR_CONTROL_MASK) ;
    }
    TRC( TRC_ID_MAIN_INFO, "defaultcolor=0x%08x", *pdefaultcolor );

    return true;
}


