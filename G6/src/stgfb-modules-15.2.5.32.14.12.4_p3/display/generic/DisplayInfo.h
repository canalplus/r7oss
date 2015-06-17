/***********************************************************************
 *
 * File: display/generic/DisplayInfo.h
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef DISPLAY_INFO_H
#define DISPLAY_INFO_H

#include "stm_display.h"


// All the sizes and positions in this structure are related to the real picture (main or decimated) that is going to be read by the HW
struct SelectedPicture
{
    uint32_t                width;
    uint32_t                height;
    uint32_t                pitch;

    stm_pixel_format_t      colorFmt ;
    uint32_t                pixelDepth;

    bool                    isSrcMacroBlock;
    bool                    isSrc420;
    bool                    isSrc422;
    bool                    isSrcOn10bits;

    // With a Progressive source: srcHeight = srcFrameRect.Height
    // With an Interlaced source: srcHeight = srcFrameRect.Height / 2
    stm_rect_t              srcFrameRect;   // Computed source rectangle in Frame coordinates (x and y are in sixteenth of pixel unit)
    uint32_t                srcHeight;      // Computed source height in Field or Frame coordinates (depending of source ScanType)
};



class CDisplayInfo
{
public:
    CDisplayInfo(void)
    {
        Reset();
    }

    // Reset every DisplayInfo
    volatile void   Reset(void)
    {
        m_isSrcInterlaced             = false;
        m_srcVSyncFreq                = 0;
        m_areIOWindowsValid           = false;
        m_isSecondaryPictureSelected  = false;
        m_horizontalDecimationFactor  = 1;
        m_verticalDecimationFactor    = 1;

        vibe_os_zero_memory (&m_primarySrcFrameRect, sizeof(m_primarySrcFrameRect) );
        vibe_os_zero_memory (&m_dstFrameRect, sizeof(m_dstFrameRect) );
        m_dstHeight                   = 0;

        vibe_os_zero_memory( &m_selectedPicture, sizeof( m_selectedPicture ));
    }

    bool           m_isSrcInterlaced;
    uint32_t       m_srcVSyncFreq;

    bool           m_areIOWindowsValid;
    bool           m_isSecondaryPictureSelected;  // Indicates if the secondary picture is used

    uint32_t       m_horizontalDecimationFactor;
    uint32_t       m_verticalDecimationFactor;

    // m_primarySrcFrameRect is ALWAYS related to the PRIMARY picture
    stm_rect_t     m_primarySrcFrameRect;         // Computed source rectangle in Frame coordinates (x and y are in sixteenth of pixel unit)

    stm_rect_t     m_dstFrameRect;                // Computed destination rectangle in Frame coordinates
    uint32_t       m_dstHeight;                   // Computed destination height in Field or Frame coordinates (depending of display ScanType)

    // Structure containing information related to the real picture (main or decimated) that is going to be read by the HW
    SelectedPicture   m_selectedPicture;
};


#endif /* DISPLAY_INFO_H */
