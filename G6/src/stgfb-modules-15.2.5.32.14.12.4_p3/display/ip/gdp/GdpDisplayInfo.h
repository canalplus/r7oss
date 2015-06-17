
/***********************************************************************
 *
 * File: display/ip/gdp/GdpDisplayInfo.h
 * Copyright (c) 2014 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef GDP_DISPLAY_INFO_H
#define GDP_DISPLAY_INFO_H


#include "display/generic/DisplayInfo.h"
#include <display/ip/stmviewport.h>

class CDisplayInfo;



class CGdpDisplayInfo: public CDisplayInfo
{
public:
    CGdpDisplayInfo(void):CDisplayInfo()
    {
        Reset();
    }

    // Reset every GdpDisplayInfo
    volatile void Reset(void)
    {
        CDisplayInfo::Reset();

        vibe_os_zero_memory (&m_viewport, sizeof(m_viewport) );
    }

protected:
  const stm_display_mode_t* m_pCurrentMode;

  bool                      m_repeatFirstField;       // Not used yet

  /*
   * Basic information from system and buffer presentation flags
   */
  bool                      m_isDisplayInterlaced;

  BufferNodeType            m_firstFieldType;

  bool                      m_firstFieldOnly;

  /*
   * Scaling information conversion increments
   */
  int32_t                   m_srcFrameRectFixedPointX;  //srcFrame x and y values in n.13 notation
  int32_t                   m_srcFrameRectFixedPointY;

  uint32_t                  m_verticalFilterInputSamples;
  uint32_t                  m_verticalFilterOutputSamples;
  uint32_t                  m_horizontalFilterOutputSamples;
  int                       m_maxYCoordinate;

  uint32_t                  m_hsrcinc;
  uint32_t                  m_vsrcinc;
  uint32_t                  m_line_step;

  /*
   * Destination Viewport
   */
  stm_viewport_t            m_viewport;

// Only CGdpPlane is allowed to access GdpInfo
friend class CGdpPlane;
};

#endif /* GDP_DISPLAY_INFO_H */
