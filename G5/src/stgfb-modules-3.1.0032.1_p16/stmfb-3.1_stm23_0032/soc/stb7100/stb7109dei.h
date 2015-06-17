/***********************************************************************
 *
 * File: soc/stb7100/stb7109dei.h
 * Copyright (c) 2006-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STB7109_DEI_H
#define _STB7109_DEI_H

#include <Gamma/DEIVideoPipe.h>

class CSTb7109DEI: public CDEIVideoPipe
{
public:
  CSTb7109DEI(stm_plane_id_t   planeID,
              CGammaVideoPlug *pVidPlug,
              ULONG            deiBaseAddr): CDEIVideoPipe(planeID, pVidPlug, deiBaseAddr)
  {
    m_ulMotionDataFlag = SDAAF_VIDEO_MEMORY;

    m_nDEIPixelBufferLength = 720;
    m_nDEIPixelBufferHeight = 576;

    m_nMBChunkSize          = 0x3;
    m_nRasterChunkSize      = 0xf;
    m_nPlanarChunkSize      = 0xf;
    m_nRasterOpcodeSize     = 8;
  }

private:
  CSTb7109DEI(const CSTb7109DEI&);
  CSTb7109DEI& operator=(const CSTb7109DEI&);

};

#endif // _STB7109_DEI_H
