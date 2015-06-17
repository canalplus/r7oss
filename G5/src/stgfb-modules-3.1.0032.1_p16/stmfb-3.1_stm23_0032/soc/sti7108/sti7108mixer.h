/***********************************************************************
 *
 * File: soc/sti7108/sti7108mixer.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7108_MIXER_H
#define _STi7108_MIXER_H

#include <Gamma/GammaMixer.h>
#include <Gamma/GenericGammaReg.h>
#include <Gamma/GenericGammaDefs.h>

class CDisplayDevice;

/*
 * Note: like the 5206 VID1 is now the shared "PiP" video plane not the main
 * video plane. Sigh...
 */

class CSTi7108MainMixer: public CGammaMixer
{
public:
  CSTi7108MainMixer(CDisplayDevice *pDev, ULONG offset): CGammaMixer(pDev,offset)
  {
    m_validPlanes = (OUTPUT_BKG  |
                     OUTPUT_GDP1 |
                     OUTPUT_GDP2 |
                     OUTPUT_GDP3 |
                     OUTPUT_GDP4 |
                     OUTPUT_NULL |
                     OUTPUT_CUR  |
                     OUTPUT_VID1 |
                     OUTPUT_VID2);

    /*
     * Fix the default plane order to BKG -> VID2 -> GDP3 -> VID1 -> GDP1 -> GDP2 -> GDP4
     * VID2 = main video program
     * GDP3 = e.g. DVD subpicture or Subtitles/CC presentation for VID2
     * VID1 = PIP second video program
     * GDP1 = "floating" extra plane between main and aux pipelines
     * GDP2 = "floating" extra plane between main and aux pipelines
     * GDP4 = primary framebuffer for application UI
     */
    ULONG ulCRBReg = (MIX_CRB_VID2 << MIX_CRB_DEPTH1_SHIFT) |
                     (MIX_CRB_GDP3 << MIX_CRB_DEPTH2_SHIFT) |
                     (MIX_CRB_VID1 << MIX_CRB_DEPTH3_SHIFT) |
                     (MIX_CRB_GDP1 << MIX_CRB_DEPTH4_SHIFT) |
                     (MIX_CRB_GDP2 << MIX_CRB_DEPTH5_SHIFT) |
                     (MIX_CRB_GDP4 << MIX_CRB_DEPTH6_SHIFT);

    WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
    m_crossbarSize = 6;
  }

private:
  CSTi7108MainMixer(const CSTi7108MainMixer&);
  CSTi7108MainMixer& operator=(const CSTi7108MainMixer&);
};


class CSTi7108AuxMixer: public CGammaMixer
{
public:
  CSTi7108AuxMixer(CDisplayDevice *pDev, ULONG offset): CGammaMixer(pDev,offset)
  {
    m_validPlanes = (OUTPUT_BKG  |
                     OUTPUT_GDP1 |
                     OUTPUT_GDP2 |
                     OUTPUT_VID1);

    ULONG ulCRBReg = (MIX_CRB_VID1 << MIX_CRB_DEPTH1_SHIFT) |
                     (MIX_CRB_GDP1 << MIX_CRB_DEPTH2_SHIFT) |
                     (MIX_CRB_GDP2 << MIX_CRB_DEPTH3_SHIFT);

    WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
    m_crossbarSize = 3;
  }

private:
  CSTi7108AuxMixer(const CSTi7108AuxMixer&);
  CSTi7108AuxMixer& operator=(const CSTi7108AuxMixer&);
};

#endif /* _STi7108_MIXER_H */
