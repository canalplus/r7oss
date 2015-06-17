/***********************************************************************
 *
 * File: soc/stb7100/stb710xgdp.h
 * Copyright (c) 2006 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STb710XGDP_H
#define STb710XGDP_H

#include <Gamma/GammaCompositorGDP.h>

class CSTb710xGDP: public CGammaCompositorGDP
{
public:
  CSTb710xGDP(stm_plane_id_t GDPid, ULONG baseAddr);

private:
  CSTb710xGDP(const CSTb710xGDP&);
  CSTb710xGDP& operator=(const CSTb710xGDP&);
};


class CSTb7109Cut3GDP: public CGammaCompositorGDP
{
public:
  CSTb7109Cut3GDP(stm_plane_id_t GDPid, ULONG baseAddr);

private:
  CSTb7109Cut3GDP(const CSTb7109Cut3GDP&);
  CSTb7109Cut3GDP& operator=(const CSTb7109Cut3GDP&);
};

#endif // STb710XGDP_H
