/***********************************************************************
 *
 * File: soc/sti7108/sti7108vdp.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STI7108_VDP_H
#define _STI7108_VDP_H

#include <Gamma/DEIVideoPipe.h>

class CSTi7108VDP: public CDEIVideoPipe
{
public:
  CSTi7108VDP(stm_plane_id_t   planeID,
              CGammaVideoPlug *plug,
              ULONG            vdpBaseAddr): CDEIVideoPipe(planeID,plug,vdpBaseAddr)
  {
    m_bHaveDeinterlacer = false;
    m_keepHistoricBufferForDeinterlacing = false;
  }

private:
  CSTi7108VDP (const CSTi7108VDP &);
  CSTi7108VDP& operator= (const CSTi7108VDP &);
};

#endif // _STI7108_VDP_H
