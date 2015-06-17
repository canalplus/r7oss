/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407hqvdplite.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STIH407_HQVDP_LITE_PLANE_H
#define _STIH407_HQVDP_LITE_PLANE_H

#include <display/ip/hqvdplite/HqvdpLitePlane.h>

class CSTiH407HqvdpLitePlane: public CHqvdpLitePlane
{
public:
  CSTiH407HqvdpLitePlane(const CDisplayDevice *pDev,
                         CVideoPlug           *pVideoPlug): CHqvdpLitePlane("Main-VID",
                                                                   STiH407_PLANE_IDX_VID_MAIN,
                                                                   pDev,
                                                                   (stm_plane_capabilities_t)(PLANE_CAPS_VIDEO|PLANE_CAPS_VIDEO_BEST_QUALITY|PLANE_CAPS_PRIMARY_PLANE|PLANE_CAPS_PRIMARY_OUTPUT),
                                                                   pVideoPlug,
                                                                   STiH407_HQVDP_BASE)
  {
    /*
     * TODO: Override this class for HQVDPLite
     */
  }

private:
  CSTiH407HqvdpLitePlane (const CSTiH407HqvdpLitePlane &);
  CSTiH407HqvdpLitePlane& operator= (const CSTiH407HqvdpLitePlane &);
};

#endif // _STIH407_HQVDP_LITE_PLANE_H
