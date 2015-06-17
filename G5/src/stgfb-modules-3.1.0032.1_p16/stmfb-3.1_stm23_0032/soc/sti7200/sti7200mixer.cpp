/***********************************************************************
 *
 * File: soc/sti7200/sti7200mixer.cpp
 * Copyright (c) 2007,2009 STMicroelectronics Limited.
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

#include "sti7200reg.h"
#include "sti7200mixer.h"



///////////////////////////////////////////////////////////////////////////////
//
// 7200 Local display main video mixer

CSTi7200LocalMainMixer::CSTi7200LocalMainMixer(CDisplayDevice* pDev):
  CGammaMixer(pDev, STi7200_MIXER1_OFFSET+STi7200_LOCAL_COMPOSITOR_BASE)
{
  m_validPlanes = OUTPUT_BKG  |
                  OUTPUT_GDP1 |
                  OUTPUT_GDP2 |
                  OUTPUT_GDP3 |
                  OUTPUT_NULL |
                  OUTPUT_VID1 |
                  OUTPUT_VID2 |
                  OUTPUT_CUR;

  /*
   * Default plane order to BKG -> VID1 -> VID2-> GDP3 -> GDP2-> GDP1
   */
  ULONG ulCRBReg = (MIX_CRB_VID1 << MIX_CRB_DEPTH1_SHIFT) |
                   (MIX_CRB_VID2 << MIX_CRB_DEPTH2_SHIFT) |
                   (MIX_CRB_GDP3 << MIX_CRB_DEPTH3_SHIFT) |
                   (MIX_CRB_GDP2 << MIX_CRB_DEPTH4_SHIFT) |
                   (MIX_CRB_GDP1 << MIX_CRB_DEPTH5_SHIFT);

  WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
  m_crossbarSize = 5;
}


///////////////////////////////////////////////////////////////////////////////
//
// 7200 Local display aux video mixer

CSTi7200LocalAuxMixer::CSTi7200LocalAuxMixer(CDisplayDevice* pDev):
  CGammaMixer(pDev, STi7200_MIXER2_OFFSET+STi7200_LOCAL_COMPOSITOR_BASE)
{
  m_validPlanes = (OUTPUT_BKG  |
                   OUTPUT_GDP3 |
                   OUTPUT_GDP4 |
                   OUTPUT_VID2);

  /*
   * Default plane order to BKG -> VID2 -> GDP3 -> GDP4
   */
  ULONG ulCRBReg = (MIX_CRB_VID2 << MIX_CRB_DEPTH1_SHIFT) |
                   (MIX_CRB_GDP3 << MIX_CRB_DEPTH2_SHIFT) |
                   (MIX_CRB_GDP4 << MIX_CRB_DEPTH3_SHIFT);

  WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
  m_crossbarSize = 3;
}


///////////////////////////////////////////////////////////////////////////////
//
// 7200Cut2 Remote display mixer

CSTi7200Cut2RemoteMixer::CSTi7200Cut2RemoteMixer(CDisplayDevice* pDev):
  CGammaMixer(pDev, STi7200_MIXER1_OFFSET+STi7200_REMOTE_COMPOSITOR_BASE)
{
  m_validPlanes = (OUTPUT_BKG  |
                   OUTPUT_GDP1 |
                   OUTPUT_GDP2 |
                   OUTPUT_NULL |
                   OUTPUT_VID1);

  /*
   * Default plane order to BKG -> VID1 -> GDP1 -> GDP2
   */
  ULONG ulCRBReg = (MIX_CRB_VID1 << MIX_CRB_DEPTH1_SHIFT) |
                   (MIX_CRB_GDP1 << MIX_CRB_DEPTH2_SHIFT) |
                   (MIX_CRB_GDP2 << MIX_CRB_DEPTH3_SHIFT);

  WriteMixReg(MXn_CRB_REG_OFFSET, ulCRBReg);
  m_crossbarSize = 3;
}
