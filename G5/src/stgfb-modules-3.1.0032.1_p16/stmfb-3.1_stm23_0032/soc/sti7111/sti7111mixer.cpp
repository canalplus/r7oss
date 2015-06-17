/***********************************************************************
 *
 * File: soc/sti7111/sti7111mixer.cpp
 * Copyright (c) 2008,2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/DisplayDevice.h>

#include <Gamma/GenericGammaReg.h>
#include <Gamma/GenericGammaDefs.h>

#include "sti7111mixer.h"


CSTi7111MainMixer::CSTi7111MainMixer(CDisplayDevice *pDev,
                                     ULONG           offset): CGammaMixer(pDev,
                                                                          offset)
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
   * Fix the default plane order to BKG -> VID1 -> GDP2 -> VID2 -> GDP3 -> GDP1
   * VID1 = main video program
   * GDP2 = e.g. DVD subpicture presentation for VID1
   * VID2 = PIP second video program
   * GDP3 = "floating" extra plane between main and aux pipelines
   * GDP1 = primary framebuffer for application UI
   */
  ULONG ulCRBReg = (MIX_CRB_VID1 << MIX_CRB_DEPTH1_SHIFT) |
                   (MIX_CRB_GDP2 << MIX_CRB_DEPTH2_SHIFT) |
                   (MIX_CRB_VID2 << MIX_CRB_DEPTH3_SHIFT) |
                   (MIX_CRB_GDP3 << MIX_CRB_DEPTH4_SHIFT) |
                   (MIX_CRB_GDP1 << MIX_CRB_DEPTH5_SHIFT);

  WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
  m_crossbarSize = 5;
}


///////////////////////////////////////////////////////////////////////////////
//

CSTi7111AuxMixer::CSTi7111AuxMixer(CDisplayDevice *pDev,
                                   ULONG           offset): CGammaMixer(pDev,
                                                                        offset)
{
  m_validPlanes = (OUTPUT_BKG  |
                   OUTPUT_GDP3 |
                   OUTPUT_VID2);

  ULONG ulCRBReg = (MIX_CRB_VID2 << MIX_CRB_DEPTH1_SHIFT) |
                   (MIX_CRB_GDP3 << MIX_CRB_DEPTH2_SHIFT);

  WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
  m_crossbarSize = 2;
}
