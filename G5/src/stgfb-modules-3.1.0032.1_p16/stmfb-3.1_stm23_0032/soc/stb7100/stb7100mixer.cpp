/***********************************************************************
 *
 * File: soc/stb7100/stb7100mixer.cpp
 * Copyright (c) 2005,2009 STMicroelectronics Limited.
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

#include "stb7100reg.h"
#include "stb7100mixer.h"

///////////////////////////////////////////////////////////////////////////////
//
// 7100 Main video mixer

CSTb7100MainMixer::CSTb7100MainMixer(CDisplayDevice* pDev):
  CGammaMixer(pDev, STb7100_MIXER1_OFFSET+STb7100_COMPOSITOR_BASE)
{
  m_validPlanes = (OUTPUT_BKG  |
                   OUTPUT_GDP1 |
                   OUTPUT_GDP2 |
                   OUTPUT_VID1 |
                   OUTPUT_NULL |
                   OUTPUT_CUR);

  /* Fix the plane order to BKG -> VID1 -> GDP2-> GDP1
   * This allows the main framebuffer on GDP1 and for GDP2 to float between
   * the Main and AUX mixers as required.
   */
  ULONG ulCRBReg = (MIX_CRB_VID1 << MIX_CRB_DEPTH1_SHIFT) |
                   (MIX_CRB_GDP2 << MIX_CRB_DEPTH2_SHIFT) |
                   (MIX_CRB_GDP1 << MIX_CRB_DEPTH3_SHIFT);

  WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
  m_crossbarSize = 3;

  m_bHasYCbCrMatrix = false;
}


///////////////////////////////////////////////////////////////////////////////
//
// 7109Cut3 Main video mixer

CSTb7109Cut3MainMixer::CSTb7109Cut3MainMixer(CDisplayDevice* pDev):
  CGammaMixer(pDev, STb7100_MIXER1_OFFSET+STb7100_COMPOSITOR_BASE)
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
   * Fix the plane order to BKG -> VID1 -> GDP2 -> VID2 -> GDP3 -> GDP1
   * VID1 = main video program, GDP2 = e.g. DVD subpicture presentation for VID1
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
// 7100 Aux video mixer
//
// Aux mixer crossbar has a fixed order
bool CSTb7100AuxMixer::GetPlaneDepth(stm_plane_id_t planeID, int *depth) const
{
  switch(planeID)
  {
    case OUTPUT_VID1:
      *depth = 1;
      break;

    case OUTPUT_GDP2:
      *depth = 2;
      break;

    default:
      return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// 7109Cut3 Aux video mixer

CSTb7109Cut3AuxMixer::CSTb7109Cut3AuxMixer(CDisplayDevice* pDev):
  CGammaMixer(pDev, STb7100_MIXER2_OFFSET+STb7100_COMPOSITOR_BASE)
{
  /*
   * Note that we now have a background colour on mixer2 in 7109Cut3.
   */
  m_validPlanes = (OUTPUT_BKG  |
                   OUTPUT_GDP3 |
                   OUTPUT_VID2);

  /*
   * We need to program the CRB register on mixer2 for 7109cut3, unlike
   * 7100 and 7109Cut2 where the crossbar doesn't exist. So as with
   * everything else we put the GDP on top of the video plane.
   */
  ULONG ulCRBReg = (MIX_CRB_VID2 << MIX_CRB_DEPTH1_SHIFT) |
                   (MIX_CRB_GDP3 << MIX_CRB_DEPTH2_SHIFT);

  WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
  m_crossbarSize = 2;
}
