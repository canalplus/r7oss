/***********************************************************************
 *
 * File: soc/sti7200/sti7200mainvideopipe.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STI7200_MAINVIDEOPIPE_H
#define _STI7200_MAINVIDEOPIPE_H

#include <Gamma/DEIVideoPipeV2.h>
#include <Gamma/DEIReg.h>


/*
 * The 7200c2 main video pipe contains a 7109DEI with 1920 pixel line buffers
 * and a new IQI as well as a P2I block.
 */
class CSTi7200Cut2MainVideoPipe: public CDEIVideoPipeV2
{
public:
  CSTi7200Cut2MainVideoPipe (stm_plane_id_t   GDPid,
                             CGammaVideoPlug *plug,
                             ULONG            dei_base): CDEIVideoPipeV2 (GDPid,
                                                                          plug,
                                                                          dei_base)
  {
    /*
    * Keep the motion buffers in LMI0 for the moment, so they will be
    * separate from the actual video buffers in LMI1 and so will balance
    * the memory bandwidth requirements for de-interlacing 1080i.
    */
    m_ulMotionDataFlag = SDAAF_NONE;

    /*
    * The following T3I plug configuration is completely against the
    * validation recommendations, but this setup works for more cases
    * particularly when working with de-interlaced 1080i or 1080p content.
    * Validation are aware of this issue and are expected to provide new
    * plug setup recommendations shortly. For the moment we are going with
    * what works in practice.
    *
    */
    m_nMBChunkSize          = 0x3;// Validation recommend 0xf
    /*
    * Validation recommendation is LD32. Note that when the video pipe is used
    * on an empty system LD8 appears slightly better, but once the DVP is
    * enabled you appear to get mutual interference between the two IPs unless
    * the video plane is using LD32.
    */
    m_nRasterOpcodeSize     = 32;
    m_nRasterChunkSize      = 0x3;// Validation recommend 0x7
    m_nPlanarChunkSize      = 0x3;// Validation do not test this mode so do not have a recommendation

    /*
    * Setup memory plug configuration for the motion buffers, which is
    * now separate from the video configuration
    */
    m_nMotionOpcodeSize     = 8; // Validation recommend LD32, but it seems less efficient (TO BE UNDERSTOOD)
    WriteVideoReg (DEI_T3I_MOTION_CTL, (DEI_T3I_CTL_OPCODE_SIZE_8 |
                                        (0x3 << DEI_T3I_CTL_CHUNK_SIZE_SHIFT)));

  }

private:
  CSTi7200Cut2MainVideoPipe(const CSTi7200Cut2MainVideoPipe&);
  CSTi7200Cut2MainVideoPipe& operator=(const CSTi7200Cut2MainVideoPipe&);

};


#endif /* _STI7200_MAINVIDEOPIPE_H */
