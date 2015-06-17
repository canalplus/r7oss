/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407gdp.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STiH407GDP_H
#define _STiH407GDP_H

#include <display/ip/gdp/GdpPlane.h>

class CSTiH407GDP: public CGdpPlane
{
public:
  CSTiH407GDP(const char                    *name,
              uint32_t                       id,
              const CDisplayDevice          *pDev,
              const stm_plane_capabilities_t caps,
              uint32_t                       baseAddr): CGdpPlane(name,
                                                                  id,
                                                                  pDev,
                                                                  static_cast<stm_plane_capabilities_t>(caps | PLANE_CAPS_GRAPHICS_BEST_QUALITY),
                                                                  baseAddr,
                                                                 true)
  {
    m_bHasVFilter       = true;
    m_bHasFlickerFilter = true;
    m_b4k2k             = true;
    m_ulMaxHSrcInc = m_fixedpointONE*4; // downscale to 1/4
    m_ulMinHSrcInc = m_fixedpointONE/8; // upscale 8x
    /*
     * As per other SoCs we are using line skip via changing the pitch for
     * downscales more than 1/2.
     */
    m_ulMaxVSrcInc = m_fixedpointONE*2; // downscale to 1/2
    m_ulMinVSrcInc = m_fixedpointONE/8;

    m_bHas4_13_precision = true;
  }

private:
  CSTiH407GDP(const CSTiH407GDP&);
  CSTiH407GDP& operator=(const CSTiH407GDP&);
};


class CSTiH407GDPLite: public CGdpPlane
{
public:
  CSTiH407GDPLite(const char                *name,
              uint32_t                       id,
              const CDisplayDevice          *pDev,
              const stm_plane_capabilities_t caps,
              uint32_t                       baseAddr): CGdpPlane(name,
                                                                  id,
                                                                  pDev,
                                                                  caps,
                                                                  baseAddr,
                                                                 true)
  {
    /*
     * No vertical or horizontal rescale or flicker filter.
     */
    m_hasAScaler = false;
  }

private:
  CSTiH407GDPLite(const CSTiH407GDPLite&);
  CSTiH407GDPLite& operator=(const CSTiH407GDPLite&);
};

#endif // _STiH407GDP_H
