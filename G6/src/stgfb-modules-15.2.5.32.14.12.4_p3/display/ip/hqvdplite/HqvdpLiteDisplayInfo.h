/***********************************************************************
 *
 * File: display/ip/hqvdplite/HqvdpLiteDisplayInfo.h
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef HQVDP_LITE_DISPLAY_INFO_H
#define HQVDP_LITE_DISPLAY_INFO_H


#include "display/generic/DisplayInfo.h"
#include <display/ip/stmviewport.h>

class CDisplayInfo;



class CHqvdpLiteDisplayInfo: public CDisplayInfo
{
public:
    CHqvdpLiteDisplayInfo(void):CDisplayInfo()
    {
        Reset();
    }

    // Reset every HqvdpLiteDisplayInfo
    volatile void Reset(void)
    {
        CDisplayInfo::Reset();

        m_isHWDeinterlacing = false;
        m_isUsingProgressive2InterlacedHW = false;
        vibe_os_zero_memory (&m_videoPlugSetup, sizeof(m_videoPlugSetup) );
        m_is3DDeinterlacingPossible = false;
    }

protected:
    // Information specific to HQVDP_Lite
    bool                                m_isHWDeinterlacing;
    bool                                m_isUsingProgressive2InterlacedHW;
    VideoPlugSetup                      m_videoPlugSetup;
    bool                                m_is3DDeinterlacingPossible;

// Only CHqvdpLitePlane is allowed to access to the hqvdpInfo
friend class CHqvdpLitePlane;
};


#endif /* HQVDP_LITE_DISPLAY_INFO_H */


