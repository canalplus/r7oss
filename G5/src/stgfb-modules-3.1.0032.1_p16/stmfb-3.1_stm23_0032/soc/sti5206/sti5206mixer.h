/***********************************************************************
 *
 * File: soc/sti5206/sti5206mixer.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi5206_MIXER_H
#define _STi5206_MIXER_H

#include <Gamma/GammaMixer.h>
#include <Gamma/GenericGammaReg.h>
#include <Gamma/GenericGammaDefs.h>

class CDisplayDevice;

/*
 * Yes note how this is irritating different to every other chip we have
 * made in the past 10 years, in particular the fact that VID1 is now the shared
 * "PiP" video plane. Sigh...
 */

class CSTi5206MainMixer: public CGammaMixer
{
public:
  CSTi5206MainMixer(CDisplayDevice *pDev, ULONG offset): CGammaMixer(pDev,offset)
  {
    m_validPlanes = (OUTPUT_BKG  |
                     OUTPUT_GDP1 |
                     OUTPUT_GDP2 |
                     OUTPUT_GDP3 |
                     OUTPUT_NULL |
                     OUTPUT_VID1 |
                     OUTPUT_VID2);

    /*
     * Fix the default plane order to BKG -> VID2 -> GDP3 -> VID1 -> GDP1 -> GDP2
     * VID2 = main video program
     * GDP3 = e.g. DVD subpicture or Subtitles/CC presentation for VID2
     * VID1 = PIP second video program
     * GDP1 = "floating" extra plane between main and aux pipelines
     * GDP2 = primary framebuffer for application UI
     */
    ULONG ulCRBReg = (MIX_CRB_VID2 << MIX_CRB_DEPTH1_SHIFT) |
                     (MIX_CRB_GDP3 << MIX_CRB_DEPTH2_SHIFT) |
                     (MIX_CRB_VID1 << MIX_CRB_DEPTH3_SHIFT) |
                     (MIX_CRB_GDP1 << MIX_CRB_DEPTH4_SHIFT) |
                     (MIX_CRB_GDP2 << MIX_CRB_DEPTH5_SHIFT);

    WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
    m_crossbarSize = 5;
  }

private:
  CSTi5206MainMixer(const CSTi5206MainMixer&);
  CSTi5206MainMixer& operator=(const CSTi5206MainMixer&);
};


class CSTi5206AuxMixer: public CGammaMixer
{
public:
  CSTi5206AuxMixer(CDisplayDevice *pDev, ULONG offset): CGammaMixer(pDev,offset)
  {
    m_validPlanes = (OUTPUT_BKG  |
                     OUTPUT_GDP1 |
                     OUTPUT_VID1);

    ULONG ulCRBReg = (MIX_CRB_VID1 << MIX_CRB_DEPTH1_SHIFT) |
                     (MIX_CRB_GDP1 << MIX_CRB_DEPTH2_SHIFT);

    WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
    m_crossbarSize = 2;
  }

private:
  CSTi5206AuxMixer(const CSTi5206AuxMixer&);
  CSTi5206AuxMixer& operator=(const CSTi5206AuxMixer&);
};

#endif /* _STi5206_MIXER_H */
