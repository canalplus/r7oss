/***********************************************************************
 *
 * File: display/ip/vdp/VdpDisplayInfo.h
 * Copyright (c) 2014 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public License.
 * See the file COPYING in the main directory of this archive formore details.
 *
\***********************************************************************/
#ifndef _VDP_DISPLAY_INFO_H_
#define _VDP_DISPLAY_INFO_H_

class CVdpDisplayInfo : public CDisplayInfo
{
 public:

  CVdpDisplayInfo( void )
    {
      Reset();
    }

  // Reset every VdpDisplayInfo
  void Reset( void )
  {
    m_srcFrameRectFixedPointX = 0;
    m_srcFrameRectFixedPointY = 0;
    m_verticalFilterInputSamples = 0;
    m_verticalFilterOutputSamples = 0;
    m_horizontalFilterOutputSamples = 0;
    m_maxYCoordinate = 0;
    m_hsrcinc = 0;
    m_chroma_hsrcinc = 0;
    m_vsrcinc = 0;
    m_chroma_vsrcinc = 0;
    m_line_step = 0;
    vibe_os_zero_memory( &m_viewport, sizeof( m_viewport ));
    vibe_os_zero_memory( &m_videoPlugSetup, sizeof( m_videoPlugSetup ));
    m_pixelsPerLine = 0;
    m_video_buffer_addr = 0;
    m_chroma_buffer_offset = 0;
    m_pCurrentMode = 0;
  }

  // Information specific to Vdp

  // Scaling information conversion increments
  int32_t m_srcFrameRectFixedPointX; // srcFrame x and y values in n.13 notation
  int32_t m_srcFrameRectFixedPointY;
  uint32_t m_verticalFilterInputSamples;
  uint32_t m_verticalFilterOutputSamples;
  uint32_t m_horizontalFilterOutputSamples;
  int m_maxYCoordinate;
  uint32_t m_hsrcinc;
  uint32_t m_chroma_hsrcinc;
  uint32_t m_vsrcinc;
  uint32_t m_chroma_vsrcinc;
  uint32_t m_line_step;

  stm_viewport_t m_viewport; // Destination Viewport

  VideoPlugSetup m_videoPlugSetup;
  uint32_t m_pixelsPerLine;
  uint32_t m_video_buffer_addr;
  uint32_t m_chroma_buffer_offset;
  const stm_display_mode_t * m_pCurrentMode;
};

#endif
