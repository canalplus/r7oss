/***********************************************************************
 *
 * File: soc/stb7100/stb7100hddisp.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STb7100HDDISP_H
#define STb7100HDDISP_H

#include <Gamma/GammaCompositorDISP.h>

class CSTb7100VideoDisplay: public CGammaCompositorDISP
{
public:
  CSTb7100VideoDisplay(stm_plane_id_t   GDPid,
                       CGammaVideoPlug *videoPlug,
                       ULONG            dispbaseAddr): CGammaCompositorDISP(GDPid, videoPlug, dispbaseAddr)
  {
    m_capabilities.ulControls |= PLANE_CTRL_CAPS_PSI_CONTROLS;
  }

private:
  CSTb7100VideoDisplay (const CSTb7100VideoDisplay &);
  CSTb7100VideoDisplay& operator= (const CSTb7100VideoDisplay &);
};


class CSTb7100HDDisplay: public CSTb7100VideoDisplay
{
public:
  CSTb7100HDDisplay(stm_plane_id_t   GDPid,
                    CGammaVideoPlug *videoPlug,
                    ULONG            dispbaseAddr): CSTb7100VideoDisplay(GDPid, videoPlug, dispbaseAddr)
  {
    /*
     * The 7100 HDDisplay can resize a 1080i HD buffer, independently
     * of the display mode set.
     */
    m_capabilities.ulMaxWidth  = 1920;
    m_capabilities.ulMaxHeight = 1088;
  }

private:
  CSTb7100HDDisplay (const CSTb7100HDDisplay &);
  CSTb7100HDDisplay& operator= (const CSTb7100HDDisplay &);
};

#endif // STb7100HDDISP_H
