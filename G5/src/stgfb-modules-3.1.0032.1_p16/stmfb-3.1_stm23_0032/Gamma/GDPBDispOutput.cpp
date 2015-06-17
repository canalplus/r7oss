/***********************************************************************
 *
 * File: Gamma/GDPBDispOutput.cpp
 * Copyright (c) 2007 STMicroelectronics Limited.
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

#include "GDPBDispOutput.h"
#include "GammaCompositorGDP.h"
#include "GenericGammaDefs.h"

CGDPBDispOutput::CGDPBDispOutput(CDisplayDevice *pDev,
                                 COutput        *pMasterOutput,
                                 ULONG          *pBlitterBaseAddr,
                                 ULONG           BDispPhysAddr,
                                 ULONG           CQOffset,
                                 int             qNumber,
                                 int             qPriority): CSTmBDispOutput(pDev, pMasterOutput, pBlitterBaseAddr, BDispPhysAddr, CQOffset, qNumber, qPriority)
{
  DENTRY();
  m_hwPlaneID     = OUTPUT_NONE;
  m_pHWPlane      = 0;
  m_ulBrightness  = 255;
  m_ulOpacity     = 255;
  m_bHWEnabled    = false;

  m_pGDPRegisters[0] = 0;
  m_pGDPRegisters[1] = 0;

  DEXIT();
}


CGDPBDispOutput::~CGDPBDispOutput() {}


bool CGDPBDispOutput::Start(const stm_mode_line_t* pModeLine, ULONG tvStandard)
{
  DENTRY();

  if(m_pMasterOutput->GetCurrentDisplayMode() == 0         ||
     m_pMasterOutput->GetCurrentDisplayMode() != pModeLine ||
     m_pMasterOutput->GetCurrentTVStandard()  != tvStandard)
  {
    return false;
  }

  if(m_hwPlaneID == OUTPUT_NONE || m_buffers[0] == 0 || m_buffers[1] == 0 || m_bufferSize == 0)
  {
    DERROR("failed, controls not configured");
    return false;
  }

  /*
   * Setup the GDP and output we want to use, doing much of what a user
   * of the core driver code would do to use a hardware display plane.
   */
  CGammaCompositorGDP *pHWPlane = static_cast<CGammaCompositorGDP *>(m_pDisplayDevice->GetPlane(m_hwPlaneID));
  if(!pHWPlane)
  {
    DERROR("requested GDP not available");
    return false;
  }

  if(!pHWPlane->LockUse(this))
  {
    DERROR("failed, cannot lock GDP, already in use");
    return false;
  }

  if(!pHWPlane->ConnectToOutput(m_pMasterOutput))
  {
    DERROR("failed, cannot connect GDP to master output");
    pHWPlane->Unlock(this);
    return false;
  }

  m_pGDPRegisters[0] = pHWPlane->GetGDPRegisters(0);
  m_pGDPRegisters[1] = pHWPlane->GetGDPRegisters(1);

  if(!SetGDPConfiguration(pModeLine))
  {
    pHWPlane->Unlock(this);
    pHWPlane->DisconnectFromOutput(m_pMasterOutput);
    return false;
  }

  CSTmBDispOutput::Start(pModeLine, tvStandard);

  /*
   * Now update the class member which will start processing in UpdateHW
   */
  g_pIOS->LockResource(m_lock);
  m_pHWPlane = pHWPlane;
  g_pIOS->UnlockResource(m_lock);

  DEXIT();
  return true;
}


bool CGDPBDispOutput::Stop(void)
{
  DENTRY();
  /*
   * Shutdown GDP
   */
  if(m_pHWPlane)
  {
    CGammaCompositorGDP *pHWPlane;

    g_pIOS->LockResource(m_lock);
    pHWPlane = (CGammaCompositorGDP *)m_pHWPlane;
    m_pHWPlane = 0;
    g_pIOS->UnlockResource(m_lock);

    pHWPlane->Unlock(this);
    m_pMasterOutput->HidePlane(m_hwPlaneID);
    pHWPlane->DisconnectFromOutput(m_pMasterOutput);
    m_bHWEnabled = false;
  }

  CSTmBDispOutput::Stop();

  DEXIT();
  return true;
}


void CGDPBDispOutput::UpdateHW(void)
{
  if(!m_pHWPlane)
    return;

  bool isTopFieldOnDisplay = (m_pMasterOutput->GetCurrentVTGVSync() == STM_TOP_FIELD);
  bool isDisplayInterlaced = (m_pCurrentMode->ModeParams.ScanType == SCAN_I);

  CSTmBDispOutput::UpdateHW();

  if(!isDisplayInterlaced || !isTopFieldOnDisplay)
    m_nFrontBuffer = (m_nFrontBuffer+1) % 2;

  ULONG pml = m_buffers[m_nFrontBuffer];

  if(m_bHWEnabled)
  {
    if(isTopFieldOnDisplay)
      pml += m_ulFieldOffset;

  }
  else if(m_bBuffersValid && (!isDisplayInterlaced || !isTopFieldOnDisplay))
  {
    DEBUGF2(2,("%s: starting GDP image buffer pointer = 0x%08lx\n",__PRETTY_FUNCTION__,pml));
    /*
     * Enable GDP and show it on the mixer of the output it is connected to
     */
    m_pHWPlane->EnableHW();
    m_bHWEnabled = true;
  }
  else
  {
    /*
     * Nothing to do yet
     */
    return;
  }

  /*
   * Work out the GDP node list to update for the next vsync now we have
   * definitely got the hardware enabled.
   */
  int index = m_pHWPlane->isNVNInNodeList(m_pHWPlane->GetHWNVN(),0)?1:0;
  g_pIOS->MemcpyToDMAArea(m_pGDPRegisters[index], OFFSETOF(GENERIC_GDP_LLU_NODE,GDPn_PML), &pml, sizeof(ULONG));

}


ULONG CGDPBDispOutput::SupportedControls(void) const
{
  return (CSTmBDispOutput::SupportedControls() | STM_CTRL_CAPS_BRIGHTNESS |
                                                 STM_CTRL_CAPS_SELECT_HW_PLANE);
}


void CGDPBDispOutput::SetControl(stm_output_control_t ctrl, ULONG ulNewVal)
{
  DEBUGF2(2,(FENTRY ": ctrl = %d ulNewVal = %lu (0x%08lx)\n",__PRETTY_FUNCTION__,(int)ctrl,ulNewVal,ulNewVal));

  switch(ctrl)
  {
    case STM_CTRL_BRIGHTNESS:
    {
      if(ulNewVal>255)
        ulNewVal = 255;

      m_ulBrightness = ulNewVal;
      if(m_pCurrentMode)
      {
        ULONG agc = (((m_ulBrightness+1)/2)<<GDP_AGC_GAIN_SHIFT) | ((m_ulOpacity+1)/2);
        g_pIOS->MemcpyToDMAArea(m_pGDPRegisters[0], (ULONG)&((GENERIC_GDP_LLU_NODE *)0)->GDPn_AGC, &agc, sizeof(ULONG));
        g_pIOS->MemcpyToDMAArea(m_pGDPRegisters[1], (ULONG)&((GENERIC_GDP_LLU_NODE *)0)->GDPn_AGC, &agc, sizeof(ULONG));
      }
      break;
    }
    case STM_CTRL_HW_PLANE_OPACITY:
    {
      if(ulNewVal>255)
        ulNewVal = 255;

      m_ulOpacity = ulNewVal;
      if(m_pCurrentMode)
      {
        ULONG agc = (((m_ulBrightness+1)/2)<<GDP_AGC_GAIN_SHIFT) | ((m_ulOpacity+1)/2);
        g_pIOS->MemcpyToDMAArea(m_pGDPRegisters[0], (ULONG)&((GENERIC_GDP_LLU_NODE *)0)->GDPn_AGC, &agc, sizeof(ULONG));
        g_pIOS->MemcpyToDMAArea(m_pGDPRegisters[1], (ULONG)&((GENERIC_GDP_LLU_NODE *)0)->GDPn_AGC, &agc, sizeof(ULONG));
      }
      break;
    }
    case STM_CTRL_HW_PLANE_ID:
    {
      if(ulNewVal<OUTPUT_GDP1 || ulNewVal>OUTPUT_GDP4)
    	return;

      m_hwPlaneID = (stm_plane_id_t)ulNewVal;
      break;
    }
    default:
      CSTmBDispOutput::SetControl(ctrl,ulNewVal);
  }
}


ULONG CGDPBDispOutput::GetControl(stm_output_control_t ctrl) const
{
  DEBUGF2(2,(FENTRY ": ctrl = %d\n",__PRETTY_FUNCTION__,(int)ctrl));

  switch(ctrl)
  {
    case STM_CTRL_BRIGHTNESS:
      return m_ulBrightness;
    case STM_CTRL_HW_PLANE_OPACITY:
      return m_ulOpacity;
    case STM_CTRL_HW_PLANE_ID:
      return (ULONG)m_hwPlaneID;
    default:
      return CSTmBDispOutput::GetControl(ctrl);
  }

  return 0;
}


bool CGDPBDispOutput::SetGDPConfiguration(const stm_mode_line_t* pModeLine)
{
  struct GENERIC_GDP_LLU_NODE GDPSetup;

  DENTRY();

  /*
   * Create GDP node config for GDMA like buffers. This is a pretty simple
   * node setup, which can be shared between both fields for an interlaced
   * display (except for the base address of each field) as there is no
   * scaling applied.
   */
  int bytesperpixel;

  g_pIOS->ZeroMemory(&GDPSetup,sizeof(GDPSetup));

  GDPSetup.GDPn_CTL = GDP_CTL_PREMULT_FORMAT;

  if(m_pHWPlane->IsLastGDPNodeOwner())
    GDPSetup.GDPn_CTL |= GDP_CTL_WAIT_NEXT_VSYNC;

  switch(m_bufferFormat)
  {
    case SURF_ARGB8888:
      GDPSetup.GDPn_CTL |= GDP_CTL_ARGB_8888 | GDP_CTL_ALPHA_RANGE;
      bytesperpixel = 4;
      break;
    case SURF_ARGB8565:
      GDPSetup.GDPn_CTL |= GDP_CTL_ARGB_8565 | GDP_CTL_ALPHA_RANGE;
      bytesperpixel = 3;
      break;
    case SURF_ARGB4444:
      GDPSetup.GDPn_CTL |= GDP_CTL_ARGB_4444;
      bytesperpixel = 2;
      break;
    default:
      DERROR("Unsupported destination format");
      return false;
  }

  GDPSetup.GDPn_AGC = (((m_ulBrightness+1)/2)<<GDP_AGC_GAIN_SHIFT) | ((m_ulOpacity+1)/2);;

  ULONG x = STCalculateViewportPixel(pModeLine, 0);
  ULONG y = STCalculateViewportLine(pModeLine,  0);

  GDPSetup.GDPn_VPO = x | (y << 16);

  x = STCalculateViewportPixel(pModeLine, pModeLine->ModeParams.ActiveAreaWidth - 1);
  y = STCalculateViewportLine(pModeLine, pModeLine->ModeParams.ActiveAreaHeight - 1);

  GDPSetup.GDPn_VPS = x | (y << 16);

  x = pModeLine->ModeParams.ActiveAreaWidth;
  y = pModeLine->ModeParams.ActiveAreaHeight;

  m_ulFieldOffset = GDPSetup.GDPn_PMP = x*bytesperpixel;

  if(m_ulFieldOffset*y > m_bufferSize)
  {
    DERROR("Provided buffer sizes too small for display mode");
    return false;
  }

  if(pModeLine->ModeParams.ScanType == SCAN_I)
  {
    GDPSetup.GDPn_PMP *= 2;
    y /= 2;
  }

  GDPSetup.GDPn_SIZE = x | (y << 16);

  DEBUGF2(DEBUG_GDPNODE_LEVEL,("----------------- GDP Node -----------------------\n"));
  DEBUGGDP(&GDPSetup);
  DEBUGF2(DEBUG_GDPNODE_LEVEL,("--------------------------------------------------\n"));

  GDPSetup.GDPn_NVN = ((struct GENERIC_GDP_LLU_NODE*)m_pGDPRegisters[0]->pData)->GDPn_NVN;
  g_pIOS->MemcpyToDMAArea(m_pGDPRegisters[0],0,&GDPSetup,sizeof(GDPSetup));

  GDPSetup.GDPn_NVN = ((struct GENERIC_GDP_LLU_NODE*)m_pGDPRegisters[1]->pData)->GDPn_NVN;
  g_pIOS->MemcpyToDMAArea(m_pGDPRegisters[1],0,&GDPSetup,sizeof(GDPSetup));

  DEXIT();
  return true;
}
