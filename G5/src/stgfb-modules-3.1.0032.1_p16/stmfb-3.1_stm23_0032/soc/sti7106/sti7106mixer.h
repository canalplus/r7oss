/***********************************************************************
 *
 * File: soc/sti7106/sti7106mixer.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7106_MIXER_H
#define _STi7106_MIXER_H

#include <Gamma/GammaMixer.h>
#include <Gamma/GenericGammaReg.h>
#include <Gamma/GenericGammaDefs.h>

class CDisplayDevice;

/*
 * Note: like the 5206 and 7108 VID1 is now the shared "PiP" video plane not
 * the main video plane.
 */

class CSTi7106MainMixer: public CGammaMixer
{
public:
  CSTi7106MainMixer(CDisplayDevice *pDev, ULONG offset): CGammaMixer(pDev,offset)
  {
    m_validPlanes = (OUTPUT_BKG  |
                     OUTPUT_GDP1 |
                     OUTPUT_GDP2 |
                     OUTPUT_GDP3 |
                     OUTPUT_VID1 |
                     OUTPUT_VID2 |
                     OUTPUT_NULL |
                     OUTPUT_CUR);

    /*
     * Fix the default plane order to BKG -> VID2 -> GDP2 -> VID1 -> GDP1 -> GDP3
     * VID2 = main video program
     * GDP2 = e.g. DVD subpicture presentation for VID1
     * VID1 = PIP second video program
     * GDP1 = "floating" extra plane between main and aux pipelines
     * GDP3 = primary framebuffer for application UI
     */
    ULONG ulCRBReg = (MIX_CRB_VID2 << MIX_CRB_DEPTH1_SHIFT) |
                     (MIX_CRB_GDP2 << MIX_CRB_DEPTH2_SHIFT) |
                     (MIX_CRB_VID1 << MIX_CRB_DEPTH3_SHIFT) |
                     (MIX_CRB_GDP1 << MIX_CRB_DEPTH4_SHIFT) |
                     (MIX_CRB_GDP3 << MIX_CRB_DEPTH5_SHIFT);

    WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
    m_crossbarSize = 5;
  }

private:
  CSTi7106MainMixer(const CSTi7106MainMixer&);
  CSTi7106MainMixer& operator=(const CSTi7106MainMixer&);
};


class CSTi7106AuxMixer: public CGammaMixer
{
public:
  CSTi7106AuxMixer(CDisplayDevice *pDev, ULONG offset): CGammaMixer(pDev,offset)
  {
    m_validPlanes = (OUTPUT_BKG  |
                     OUTPUT_GDP1 |
                     OUTPUT_VID1);

    ULONG ulCRBReg = (MIX_CRB_VID1 << MIX_CRB_DEPTH1_SHIFT) |
                     (MIX_CRB_GDP1 << MIX_CRB_DEPTH2_SHIFT);

    WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
    m_crossbarSize = 3;
  }

private:
  CSTi7106AuxMixer(const CSTi7106AuxMixer&);
  CSTi7106AuxMixer& operator=(const CSTi7106AuxMixer&);
};

#endif /* _STi7106_MIXER_H */
