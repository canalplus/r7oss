/***********************************************************************
 *
 * File: soc/stb7100/stb7100digitalouts.cpp
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "stb7100reg.h"
#include "stb7100device.h"
#include "stb7100analogueouts.h"
#include "stb7100dvo.h"


CSTb7100DVO::CSTb7100DVO(CDisplayDevice     *pDev,
                         CSTb7100MainOutput *pMainOutput): COutput(pDev,0)
{  
  DEBUGF2(2,("CSTb7100DVO::CSTb7100DVO - in\n"));

  m_pMainOutput  = pMainOutput;
  m_ulTimingID   = pMainOutput->GetTimingID();
  m_pDevRegs     = (ULONG*)pDev->GetCtrlRegisterBase();
  m_ulVOSOffset  = STb7100_VOS_BASE;
  m_ulClkOffset  = STb7100_CLKGEN_BASE;

  // Set input and output configuration to the only thing the output supports.
  m_ulInputSource  = STM_AV_SOURCE_MAIN_INPUT;
  m_ulOutputFormat = STM_VIDEO_OUT_YUV;

  DEBUGF2(2,("CSTb7100DVO::CSTb7100DVO - out\n"));
}


CSTb7100DVO::~CSTb7100DVO() {}


ULONG CSTb7100DVO::GetCapabilities(void) const
{
  return (STM_OUTPUT_CAPS_DVO_656 |
          STM_OUTPUT_CAPS_DVO_HD);
}


bool CSTb7100DVO::Start(const stm_mode_line_t* pModeLine, ULONG tvStandard)
{
  DEBUGF2(2,("CSTb7100DVO::Start - in\n"));

  if(m_pCurrentMode)
    return false;
    
  // The DVO is slaved off the timings of a main output, which must have
  // already been started with the mode requested here.
  if(m_pMainOutput->GetCurrentDisplayMode() == 0         ||
     m_pMainOutput->GetCurrentDisplayMode() != pModeLine)
  {
    DEBUGF2(2,("CSTb7100DVO::Start - main output not started or different mode requested\n"));
    return false;
  }

  // Strip out any auxillary standard in SD progressive modes
  if(tvStandard & STM_OUTPUT_STD_SMPTE293M)
    tvStandard = STM_OUTPUT_STD_SMPTE293M;
    
  if((m_pMainOutput->GetCurrentTVStandard() & tvStandard) == 0)
  {
    DEBUGF2(2,("CSTb7100DVO::Start - tvStandard does not match master output\n"));
    return false;
  }

  /*
   * The DVO output clock is only available via the display clock observation
   * PIO on 710x (it appears). The 656 clock is always setup even for HD/ED
   * modes.
   */
  WriteClkReg(CKGB_CLK_OUT_SEL, CKGB_OUT_SEL_656);

  ULONG cfgdig = ReadVOSReg(DSP_CFG_DIG) & ~(DSP_DIG_AUX_NOT_MAIN | DSP_DIG_OUTPUT_ENABLE);

  if((pModeLine->ModeParams.ScanType == SCAN_I) &&
    ((pModeLine->ModeParams.OutputStandards & STM_OUTPUT_STD_SD_MASK) != 0))
  { 
    /*
     * PAL & NTSC interlaced modes
     */
    switch(pModeLine->TimingParams.LinesByFrame)
    {
      case 625:
        DEBUGF2(2,("CSTb7100DVO::Start - SD 625 line system\n"));
        WriteVOSReg(C656_PAL, C656_PAL_NOT_NTSC);
        WriteVOSReg(DSP_CFG_DIG, cfgdig | (DSP_DIG_AUX_NOT_MAIN | DSP_DIG_OUTPUT_ENABLE));
        break;
      case 525:
        DEBUGF2(2,("CSTb7100DVO::Start - SD 525 line system\n"));
        WriteVOSReg(C656_PAL, ~C656_PAL_NOT_NTSC);
        WriteVOSReg(DSP_CFG_DIG, cfgdig | (DSP_DIG_AUX_NOT_MAIN | DSP_DIG_OUTPUT_ENABLE));
        break;
      default:
        WriteVOSReg(DSP_CFG_DIG, cfgdig);
        break;
    }
    
    /*
     * The mode parameter heights are in frame lines (2x interlaced field lines)
     * hence the divide by 2 here.
     */
    WriteVOSReg(C656_ACTL,   pModeLine->ModeParams.ActiveAreaHeight/2);
    WriteVOSReg(C656_BLANKL, pModeLine->ModeParams.FullVBIHeight/2);
    WriteVOSReg(C656_BACT,   pModeLine->ModeParams.ActiveAreaXStart);
    WriteVOSReg(C656_EACT,   pModeLine->ModeParams.ActiveAreaXStart + pModeLine->ModeParams.ActiveAreaWidth);
  }
  else
  {
    /*
     * HD and SD Progressive modes.
     * 
     * This appears to be it, although the H and V sync signal generation from
     * the VTG will have to be correct for it to work.
     */
    WriteVOSReg(DSP_CFG_DIG, cfgdig | DSP_DIG_OUTPUT_ENABLE);
  }

  COutput::Start(pModeLine, tvStandard);

  DEBUGF2(2,("CSTb7100DVO::Start - out\n"));
  
  return true;
}


bool CSTb7100DVO::Stop(void)
{
  DEBUGF2(2,("CSTb7100DVO::Stop - in\n"));

  ULONG val = ReadVOSReg(DSP_CFG_DIG) & ~DSP_DIG_OUTPUT_ENABLE;
  WriteVOSReg(DSP_CFG_DIG, val);

  COutput::Stop();  

  DEBUGF2(2,("CSTb7100DVO::Stop - out\n"));
  return true;
}


void CSTb7100DVO::Suspend(void)
{
  DEBUGF2(2,("CSTb7100DVO::Suspend - in\n"));

  if(m_bIsSuspended)
    return;
    
  ULONG val = ReadVOSReg(DSP_CFG_DIG) & ~DSP_DIG_OUTPUT_ENABLE;
  WriteVOSReg(DSP_CFG_DIG, val);

  COutput::Suspend();
}


void CSTb7100DVO::Resume(void)
{
  DEBUGF2(2,("CSTb7100DVO::Resume - in\n"));

  if(!m_bIsSuspended)
    return;

  if(m_pCurrentMode)
  {
    ULONG val = ReadVOSReg(DSP_CFG_DIG) | DSP_DIG_OUTPUT_ENABLE;
    WriteVOSReg(DSP_CFG_DIG, val);
  }
  
  COutput::Resume();
}


// DVO has no interrupts of its own.
bool CSTb7100DVO::HandleInterrupts(void) { return false; }

// DVO is a slave output and doesn't explicitly manage plane composition.
bool CSTb7100DVO::CanShowPlane(stm_plane_id_t planeID) { return false; }
bool CSTb7100DVO::ShowPlane   (stm_plane_id_t planeID) { return false; }
void CSTb7100DVO::HidePlane   (stm_plane_id_t planeID) {}
bool CSTb7100DVO::SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate) { return false; }
bool CSTb7100DVO::GetPlaneDepth(stm_plane_id_t planeID, int *depth) const { return false; }

// No controls supported by DVO at the moment
ULONG CSTb7100DVO::SupportedControls(void) const { return 0; }
void  CSTb7100DVO::SetControl(stm_output_control_t, ULONG ulNewVal) {}

ULONG CSTb7100DVO::GetControl(stm_output_control_t ctrl) const
{
  switch(ctrl)
  {
    case STM_CTRL_AV_SOURCE_SELECT:
      return m_ulInputSource;
    case STM_CTRL_VIDEO_OUT_SELECT:
      return m_ulOutputFormat;
    default:
      break;
  }
  return 0;
}



///////////////////////////////////////////////////////////////////////////////
//

#define SYS_CFG3_DVO_TO_DVP_LOOPBACK (1L<<10)
#define DSP_DIG_24BIT_RGB            (1L<<2)

CSTb7109Cut3DVO::CSTb7109Cut3DVO(CDisplayDevice     *pDev,
                                 CSTb7100MainOutput *pMainOutput): CSTb7100DVO(pDev,pMainOutput)
{  
  DEBUGF2(2,("CSTb7109Cut3DVO::CSTb7109Cut3DVO - in\n"));

  m_ulSysCfgOffset = STb7100_SYSCFG_BASE;
  m_bOutputToDVP = false;

  DEBUGF2(2,("CSTb7109Cut3DVO::CSTb7109Cut3DVO - out\n"));
}


CSTb7109Cut3DVO::~CSTb7109Cut3DVO() {}


ULONG CSTb7109Cut3DVO::SupportedControls(void) const
{
  return STM_CTRL_CAPS_VIDEO_OUT_SELECT | STM_CTRL_CAPS_DVO_LOOPBACK;
}


void CSTb7109Cut3DVO::SetControl(stm_output_control_t ctrl, ULONG ulNewVal)
{
  switch(ctrl)
  {
    case STM_CTRL_VIDEO_OUT_SELECT:
    {
      if(ulNewVal != STM_VIDEO_OUT_RGB && ulNewVal != STM_VIDEO_OUT_YUV)
        return;

      if(m_ulOutputFormat != ulNewVal)
      {
        m_ulOutputFormat = ulNewVal;

        ULONG reg = ReadVOSReg(DSP_CFG_DIG) & ~DSP_DIG_24BIT_RGB;
        if(m_ulOutputFormat == STM_VIDEO_OUT_RGB)
          reg |= DSP_DIG_24BIT_RGB;

        WriteVOSReg(DSP_CFG_DIG, reg);
      }

      break;
    }
    case STM_CTRL_DVO_LOOPBACK:
    {
      m_bOutputToDVP = (ulNewVal != 0);
      
      ULONG cfgval = ReadSysCfgReg(SYS_CFG3) & ~SYS_CFG3_DVO_TO_DVP_LOOPBACK;

      if(m_bOutputToDVP)
        cfgval |= SYS_CFG3_DVO_TO_DVP_LOOPBACK;

      WriteSysCfgReg(SYS_CFG3, cfgval);
      break;
    }
    default:
      break;
  }
}


ULONG CSTb7109Cut3DVO::GetControl(stm_output_control_t ctrl) const
{
  switch(ctrl)
  {
    case STM_CTRL_DVO_LOOPBACK:
      return m_bOutputToDVP?1:0;
    default:
      return CSTb7100DVO::GetControl(ctrl);
  }
}
