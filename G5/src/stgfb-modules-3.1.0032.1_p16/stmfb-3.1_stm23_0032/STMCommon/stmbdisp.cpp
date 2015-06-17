/***********************************************************************
 *
 * File: STMCommon/stmbdisp.cpp
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "stmbdispregs.h"
#include "stmbdisp.h"


CSTmBDisp::CSTmBDisp (ULONG                   *pBDispBaseAddr,
                      stm_blitter_device_priv  eDevice,
                      ULONG                    drawOps,
                      ULONG                    copyOps,
                      ULONG                    nAQs,
                      ULONG                    nCQs,
                      ULONG                    nLineBufferLength)
{
  DEBUGF2 (2, (FENTRY ": %p: pBDispBaseAddr: %p, caps: %.8lx/%.8lx, AQs/CQs: %lu/%lu\n",
               __PRETTY_FUNCTION__, this, pBDispBaseAddr, drawOps, copyOps,
               nAQs, nCQs));

  m_pBDispReg = pBDispBaseAddr;

  for (unsigned int i = 0; i < N_ELEMENTS (m_pAQs); ++i)
    m_pAQs[i] = 0;

//  for (unsigned int i = 0; i < N_ELEMENTS (m_pCQs); ++i)
//    m_pCQs[i] = 0;

  m_BDispAQdrawOps = drawOps;
  m_BDispAQcopyOps = copyOps;

  m_nAQs = nAQs;
  m_nCQs = nCQs;

  m_eBDispDevice = eDevice;

  m_nLineBufferLength = nLineBufferLength;

  DEBUGF2 (2, (FEXIT ": %p\n", __PRETTY_FUNCTION__, this));
}


CSTmBDisp::~CSTmBDisp (void)
{
  DEBUGF2 (2, (FENTRY ": %p\n", __PRETTY_FUNCTION__, this));

  for (unsigned int i = 0; i < m_nAQs; ++i)
    delete m_pAQs[i];

/*
 * For the future
 *
  for (unsigned int i = 0; i < m_nCQs; ++i)
    delete m_pCQs[i];
 */

  DEBUGF2 (2, (FEXIT ": %p\n", __PRETTY_FUNCTION__, this));
}


bool CSTmBDisp::Create (void)
{
  DEBUGF2 (2, (FENTRY ": %p - doing reset\n", __PRETTY_FUNCTION__, this));

  /* Do a BDisp global soft reset here. */
  WriteBDispReg (BDISP_CTL, BDISP_CTL_GLOBAL_SOFT_RESET);
  g_pIOS->StallExecution (10);
  WriteBDispReg (BDISP_CTL, 0);

  while ((ReadBDispReg (BDISP_STA) & 0x1) == 0)
    g_pIOS->StallExecution (10);

  DEBUGF2 (2, ("%s: %p - BDisp reset complete\n", __PRETTY_FUNCTION__, this));

  /*
   * PLUG register setup, copied from 7109 for the moment but we may need
   * to tweak it for performance on 7200.
   */
  ConfigurePlugs ();

  DEBUGF2 (2, (FEXIT ": %p\n", __PRETTY_FUNCTION__, this));

  return m_nAQs <= N_ELEMENTS (m_pAQs);
}


void CSTmBDisp::ConfigurePlugs (void)
{
  DEBUGF2 (2, (FENTRY ": %p\n", __PRETTY_FUNCTION__, this));

  /* PLUG register setup, for 7109. May need to be tweaked for performance
     for other platforms. */
  WriteBDispReg (BDISP_S1_MAX_OPCODE,0x5);
  WriteBDispReg (BDISP_S1_CHUNK,0x0);
  WriteBDispReg (BDISP_S1_MESSAGE,0x3);
  WriteBDispReg (BDISP_S1_PAGE,0x2);

  WriteBDispReg (BDISP_S2_MAX_OPCODE,0x5);
  WriteBDispReg (BDISP_S2_CHUNK,0x0);
  WriteBDispReg (BDISP_S2_MESSAGE,0x3);
  WriteBDispReg (BDISP_S2_PAGE,0x2);

  WriteBDispReg (BDISP_S3_MAX_OPCODE,0x5);
  WriteBDispReg (BDISP_S3_CHUNK,0x0);
  WriteBDispReg (BDISP_S3_MESSAGE,0x3);
  WriteBDispReg (BDISP_S3_PAGE,0x2);

  WriteBDispReg (BDISP_TARGET_MAX_OPCODE,0x5);
  WriteBDispReg (BDISP_TARGET_CHUNK,0x0);
  WriteBDispReg (BDISP_TARGET_MESSAGE,0x3);
  WriteBDispReg (BDISP_TARGET_PAGE,0x2);

  DEBUGF2 (2, (FEXIT ": %p\n", __PRETTY_FUNCTION__, this));
}


CSTmBDispAQ *CSTmBDisp::GetAQ (unsigned int qNumber)
{
  DEBUGF2 (2, (FENTRY ": %p (aq %u of %u)\n", __PRETTY_FUNCTION__, this, qNumber, m_nAQs));

  if (qNumber == 0 || qNumber > m_nAQs || qNumber > N_ELEMENTS (m_pAQs))
  {
    DEBUGF2 (1, ("%s: invalid qNumber %u\n", __PRETTY_FUNCTION__, qNumber));
    return 0;
  }

  unsigned int index = qNumber - 1;
  if (!m_pAQs[index])
  {
    m_pAQs[index] = new CSTmBDispAQ (m_pBDispReg,
                                     BDISP_AQ1_BASE + 0x10 * index,
                                     qNumber,  STM_BDISP_MAX_AQs - qNumber,
                                     m_BDispAQdrawOps, m_BDispAQcopyOps,
                                     m_eBDispDevice, m_nLineBufferLength);
    if (!m_pAQs[index])
      DEBUGF2 (1, ("%s: failed to allocate BDisp application queue %u\n", __PRETTY_FUNCTION__, qNumber));
    else
    {
      if (!m_pAQs[index]->Create ())
      {
        DEBUGF2 (1, ("%s: failed to create BDisp application queue %u\n", __PRETTY_FUNCTION__, qNumber));
        delete m_pAQs[index];
        m_pAQs[index] = 0;
      }
    }
  }

  DEBUGF2 (2, (FEXIT ": %p (aq %u of %u @ %p)\n", __PRETTY_FUNCTION__, this, qNumber, m_nAQs, m_pAQs[index]));

  return m_pAQs[index];
}
