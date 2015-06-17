/***********************************************************************
 *
 * File: soc/sti7111/sti7111dei.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STI7111_DEI_H
#define _STI7111_DEI_H

#include <Gamma/DEIVideoPipeV2.h>
#include <Gamma/DEIReg.h>

class CSTi7111DEI: public CDEIVideoPipeV2
{
public:
  CSTi7111DEI (stm_plane_id_t   GDPid,
               CGammaVideoPlug *plug,
               ULONG            baseAddr): CDEIVideoPipeV2 (GDPid,
                                                            plug,
                                                            baseAddr)
  {
    /*
     * motion data allocated from system partition, there is only one LMI on
     * sti7111.
     */
    m_ulMotionDataFlag = SDAAF_NONE;

    /*
     * (mostly) validation recommended T3I plug settings.
     */
    m_nMBChunkSize          = 0x3; // Validation actually recommend 0xf, this is better for extreme downscales
    m_nRasterOpcodeSize     = 32;
    m_nRasterChunkSize      = 0x7;
    m_nPlanarChunkSize      = 0x3;// Validation do not test this mode so do not have a recommendation

    m_nMotionOpcodeSize     = 32;
    WriteVideoReg (DEI_T3I_MOTION_CTL, (DEI_T3I_CTL_OPCODE_SIZE_32 |
                                        (0x3 << DEI_T3I_CTL_CHUNK_SIZE_SHIFT)));
  }

private:
  CSTi7111DEI (const CSTi7111DEI &);
  CSTi7111DEI& operator= (const CSTi7111DEI &);
};

#endif // _STI7111_DEI_H
