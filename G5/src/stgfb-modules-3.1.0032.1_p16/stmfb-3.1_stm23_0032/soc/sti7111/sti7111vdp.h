/***********************************************************************
 *
 * File: soc/sti7111/sti7111vdp.h
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STI7111_VDP_H
#define _STI7111_VDP_H

#include <Gamma/DEIVideoPipe.h>

class CSTi7111VDP: public CDEIVideoPipe
{
public:
  CSTi7111VDP(stm_plane_id_t   planeID,
              CGammaVideoPlug *plug,
              ULONG            vdpBaseAddr): CDEIVideoPipe(planeID,plug,vdpBaseAddr)
  {
    m_bHaveDeinterlacer = false;
    m_keepHistoricBufferForDeinterlacing = false;
  }

private:
  CSTi7111VDP (const CSTi7111VDP &);
  CSTi7111VDP& operator= (const CSTi7111VDP &);
};

#endif // _STI7111_VDP_H
