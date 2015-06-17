/***********************************************************************
 *
 * File: STMCommon/stmdvo656.cpp
 * Copyright (c) 2004, 2005 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 * 
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <Generic/DisplayDevice.h>

#include <STMCommon/stmviewport.h>

#include "stmdvo656.h"

CSTmDVO656::CSTmDVO656(CDisplayDevice *pDev,
                       ULONG           ulRegOffset,
                       COutput        *pMasterOutput): COutput(pDev,0)
{
  DEBUGF2(2,("CSTmDVO656::CSTmDVO656 pDev = 0x%x  master output = 0x%x\n",pDev,pMasterOutput));
  m_pMasterOutput = pMasterOutput;
  m_ulTimingID    = pMasterOutput->GetTimingID();
  m_pDevRegs      = (ULONG*)pDev->GetCtrlRegisterBase();
  m_ulDVOOffset   = ulRegOffset;
  DEBUGF2(2,("CSTmDVO656::CSTmDVO656 m_pDevRegs = 0x%x  m_ulDVOOffset = 0x%x\n",m_pDevRegs,m_ulDVOOffset));  
}


CSTmDVO656::~CSTmDVO656()
{
  CSTmDVO656::Stop();
}


bool CSTmDVO656::Start(const stm_mode_line_t* pModeLine, ULONG tvStandard)
{
  // The DVO is slaved off the timings of a main output, which must have
  // already been started with the mode requested here.
  if(m_pMasterOutput->GetCurrentDisplayMode() == 0         ||
     m_pMasterOutput->GetCurrentDisplayMode() != pModeLine ||
     m_pMasterOutput->GetCurrentTVStandard()  != tvStandard)
  {
    return false;
  }

  Stop();
    
  switch(pModeLine->TimingParams.LinesByFrame)
  {
    case 625:
      // Program for ITU-R 656 625 line system
      WriteDVOReg(STM_DVO_CTL_656, (DVO_SF_625_LINE | DVO_CTL656_EN656_NOT_CCIR601));
      break;
    case 525:
      // Program for ITU-R 656 525 line system
      WriteDVOReg(STM_DVO_CTL_656, (DVO_SF_525_LINE   |
                                    DVO_CTL656_POE    |
                                    DVO_CTL656_ADDYDS |
                                    DVO_CTL656_EN656_NOT_CCIR601));
                        
      break;
    default:
      // Program for CCIR-601
      WriteDVOReg(STM_DVO_CTL_656, 0);
  }
  
  ULONG xpo = STCalculateViewportPixel(pModeLine,0);
  ULONG xps = STCalculateViewportPixel(pModeLine, pModeLine->ModeParams.ActiveAreaWidth);

  /*
   * Setup the vertical viewport for the bottom field, the hardware works out
   * the top field from this.
   */
  ULONG ypo = STCalculateViewportLine(pModeLine, 0);
  ULONG yps = STCalculateViewportLine(pModeLine, pModeLine->ModeParams.ActiveAreaHeight-1);
  
  WriteDVOReg(STM_DVO_VPO_656,((ypo << 16) | xpo));
  WriteDVOReg(STM_DVO_VPS_656,((yps << 16) | xps));
  
  COutput::Start(pModeLine, tvStandard);
  
  return true;
}


bool CSTmDVO656::Stop(void)
{
  WriteDVOReg(STM_DVO_CTL_656,0);

  COutput::Stop();  
  
  return true;
}


// Chip specific implementations will probably add to this for input sources
ULONG CSTmDVO656::GetCapabilities(void) const
{
  return STM_OUTPUT_CAPS_DVO_656;
}

// No interrupts on DVO
bool CSTmDVO656::HandleInterrupts(void) { return false; }

// DVO is a slave output and doesn't explicitly manage plane composition.
bool CSTmDVO656::CanShowPlane(stm_plane_id_t planeID) { return false; }
bool CSTmDVO656::ShowPlane   (stm_plane_id_t planeID) { return false; }
void CSTmDVO656::HidePlane   (stm_plane_id_t planeID) {}
bool CSTmDVO656::SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate) { return false; }
bool CSTmDVO656::GetPlaneDepth(stm_plane_id_t planeID, int *depth) const { return false; }

// No controls supported by DVO
ULONG CSTmDVO656::SupportedControls(void) const {return 0; }
void  CSTmDVO656::SetControl(stm_output_control_t, ULONG ulNewVal) {}
ULONG CSTmDVO656::GetControl(stm_output_control_t) const { return 0; }
