/***********************************************************************
 *
 * File: soc/sti7200/sti7200vdp.h
 * Copyright (c) 2007-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STB7200_VDP_H
#define _STB7200_VDP_H

#include <Gamma/DEIVideoPipe.h>

/*
 * The 7200VDP is a DEI but without the de-interlacing capability, sigh...
 */
class CSTi7200VDP: public CDEIVideoPipe
{
public:
  CSTi7200VDP(stm_plane_id_t   GDPid,
              CGammaVideoPlug *plug,
              ULONG            vdpBaseAddr): CDEIVideoPipe(GDPid, plug, vdpBaseAddr)
  {
    m_bHaveDeinterlacer = false;
    m_keepHistoricBufferForDeinterlacing = false;
  }

private:
  CSTi7200VDP (const CSTi7200VDP &);
  CSTi7200VDP& operator= (const CSTi7200VDP &);
};

#endif // _STB7200_VDP_H
