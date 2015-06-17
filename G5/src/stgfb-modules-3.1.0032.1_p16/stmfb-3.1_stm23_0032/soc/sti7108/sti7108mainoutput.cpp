/***********************************************************************
 *
 * File: soc/sti7108/sti7108mainoutput.cpp
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <STMCommon/stmhdmi.h>
#include <STMCommon/stmfsynth.h>
#include <STMCommon/stmsdvtg.h>

#include <HDTVOutFormatter/stmhdfawg.h>
#include <HDTVOutFormatter/stmtvoutreg.h>
#include <HDTVOutFormatter/stmtvoutdenc.h>

#include "sti7108reg.h"
#include "sti7108device.h"
#include "sti7108mainoutput.h"
#include "sti7108mixer.h"
#include "sti7108clkdivider.h"

//////////////////////////////////////////////////////////////////////////////
//
// HD, SD Progressive and SD Interlaced Output on main HD Dacs
//
CSTi7108MainOutput::CSTi7108MainOutput(
    CSTi7108Device     *pDev,
    CSTmSDVTG          *pVTG1,
    CSTmSDVTG          *pVTG2,
    CSTmTVOutDENC      *pDENC,
    CSTi7108MainMixer  *pMixer,
    CSTmFSynth         *pFSynth,
    CSTmHDFormatterAWG *pAWG,
    CSTi7108ClkDivider *pClkDivider): CSTmHDFormatterOutput(pDev, STi7108_MAIN_TVOUT_BASE, STi7108_AUX_TVOUT_BASE, STi7108_HD_FORMATTER_BASE, pVTG1, pVTG2, pDENC, pMixer, pFSynth, pAWG)
{
  DENTRY();
  m_pClkDivider = pClkDivider;
  DEXIT();
}


CSTi7108MainOutput::~CSTi7108MainOutput() {}


void CSTi7108MainOutput::StartSDInterlacedClocks(const stm_mode_line_t *mode)
{
  DENTRY();

  m_pClkDivider->Enable(STM_CLK_PIX_HD, STM_CLK_SRC_0, STM_CLK_DIV_1);
  m_pClkDivider->Enable(STM_CLK_DISP_HD, STM_CLK_SRC_0, STM_CLK_DIV_8);
  m_pClkDivider->Enable(STM_CLK_656, STM_CLK_SRC_0, STM_CLK_DIV_4);
  m_pClkDivider->Enable(STM_CLK_PIX_SD, STM_CLK_SRC_0, STM_CLK_DIV_4);
  m_pClkDivider->Enable(STM_CLK_DISP_SD, STM_CLK_SRC_0, STM_CLK_DIV_8);

  DEXIT();
}


void CSTi7108MainOutput::StartSDProgressiveClocks(const stm_mode_line_t *mode)
{
  DENTRY();

  m_pClkDivider->Enable(STM_CLK_PIX_HD, STM_CLK_SRC_0, STM_CLK_DIV_1);
  m_pClkDivider->Enable(STM_CLK_DISP_HD, STM_CLK_SRC_0, STM_CLK_DIV_4);
  m_pClkDivider->Enable(STM_CLK_656, STM_CLK_SRC_0, STM_CLK_DIV_2);

  DEXIT();
}


void CSTi7108MainOutput::StartHDClocks(const stm_mode_line_t *mode)
{
  DENTRY();

  m_pClkDivider->Enable(STM_CLK_PIX_HD, STM_CLK_SRC_0, STM_CLK_DIV_1);

  if(mode->TimingParams.ulPixelClock >= 148000000)
  {
    m_pClkDivider->Enable(STM_CLK_DISP_HD, STM_CLK_SRC_0, STM_CLK_DIV_1);
    m_pClkDivider->Enable(STM_CLK_656, STM_CLK_SRC_0, STM_CLK_DIV_1);
  }
  else
  {
    m_pClkDivider->Enable(STM_CLK_DISP_HD, STM_CLK_SRC_0, STM_CLK_DIV_2);
    m_pClkDivider->Enable(STM_CLK_656, STM_CLK_SRC_0, STM_CLK_DIV_2);
  }

  DEXIT();
}


void CSTi7108MainOutput::SetMainClockToHDFormatter(void)
{
  DENTRY();
  m_pClkDivider->Enable(STM_CLK_PIX_HD, STM_CLK_SRC_0, STM_CLK_DIV_1);
  DEXIT();
}


bool CSTi7108MainOutput::ShowPlane(stm_plane_id_t planeID)
{
  DEBUGF2(2,("CSTi7108MainOutput::ShowPlane %d\n",(int)planeID));

  stm_clk_divider_output_divide_t div;

  if(!m_pClkDivider->getDivide(STM_CLK_DISP_HD,&div))
  {
    DERROR("Cannot get display clock divider\n");
    return false;
  }

  if(planeID == OUTPUT_GDP1)
  {
    m_pClkDivider->Enable(STM_CLK_GDP1, STM_CLK_SRC_0, div);
    DEBUGF2(2,("%s: GDP1 clock set to HD disp clock\n",__PRETTY_FUNCTION__));
  }
  else if(planeID == OUTPUT_GDP2)
  {
    m_pClkDivider->Enable(STM_CLK_GDP2, STM_CLK_SRC_0, div);
    DEBUGF2(2,("%s: GDP2 clock set to HD disp clock\n",__PRETTY_FUNCTION__));
  }
  else if (planeID == OUTPUT_VID1)
  {
    ULONG val = ReadSysCfgReg(SYS_CFG2) | SYS_CFG2_PIP_MODE;
    WriteSysCfgReg(SYS_CFG2,val);

    m_pClkDivider->Enable(STM_CLK_DISP_PIP, STM_CLK_SRC_0, div);
    DEBUGF2(2,("%s: PIP clock set to HD disp clock\n",__PRETTY_FUNCTION__));
  }

  return CSTmMasterOutput::m_pMixer->EnablePlane(planeID);
}


void CSTi7108MainOutput::EnableDACs(void)
{
  DENTRY();

  ULONG val = ReadSysCfgReg(SYS_CFG3);

  stm_clk_divider_output_source_t hdpixsrc;
  m_pClkDivider->getSource(STM_CLK_PIX_HD,&hdpixsrc);

  if(m_ulOutputFormat & (STM_VIDEO_OUT_RGB|STM_VIDEO_OUT_YUV))
  {
    DEBUGF2(2,("%s: Enabling HD DACs\n",__PRETTY_FUNCTION__));
    val &= ~(SYS_CFG3_DAC_HD_HZW_DISABLE |
             SYS_CFG3_DAC_HD_HZV_DISABLE |
             SYS_CFG3_DAC_HD_HZU_DISABLE);
  }
  else if(hdpixsrc == STM_CLK_SRC_0)
  {
    /*
     * Only tri-state the HD DAC setup if the SD pipeline is not
     * using the HD formatter for SCART.
     */
    DEBUGF2(2,("%s: Tri-Stating HD DACs\n",__PRETTY_FUNCTION__));
    val |=  (SYS_CFG3_DAC_HD_HZW_DISABLE |
             SYS_CFG3_DAC_HD_HZV_DISABLE |
             SYS_CFG3_DAC_HD_HZU_DISABLE);
  }

  if(m_bUsingDENC)
  {
    if(m_ulOutputFormat & STM_VIDEO_OUT_CVBS)
    {
      DEBUGF2(2,("%s: Enabling CVBS DAC\n",__PRETTY_FUNCTION__));
      val &= ~SYS_CFG3_DAC_SD_HZU_DISABLE;
    }
    else
    {
      DEBUGF2(2,("%s: Tri-Stating CVBS DAC\n",__PRETTY_FUNCTION__));
      val |=  SYS_CFG3_DAC_SD_HZU_DISABLE;
    }

    if(m_ulOutputFormat & STM_VIDEO_OUT_YC)
    {
      DEBUGF2(2,("%s: Enabling Y/C DACs\n",__PRETTY_FUNCTION__));
      val &= ~(SYS_CFG3_DAC_SD_HZV_DISABLE |
               SYS_CFG3_DAC_SD_HZW_DISABLE);
    }
    else
    {
      DEBUGF2(2,("%s: Tri-Stating Y/C DACs\n",__PRETTY_FUNCTION__));
      val |=  (SYS_CFG3_DAC_SD_HZV_DISABLE |
               SYS_CFG3_DAC_SD_HZW_DISABLE);
    }
  }

  WriteSysCfgReg(SYS_CFG3,val);

  if(!m_bDacHdPowerDisabled)
  {
    DEBUGF2(2,("%s: Powering Up HD DACs\n",__PRETTY_FUNCTION__));
    val = ReadMainTVOutReg(TVOUT_PADS_CTL) & ~TVOUT_MAIN_PADS_DAC_POFF;
    WriteMainTVOutReg(TVOUT_PADS_CTL, val);
  }

  if(m_bUsingDENC)
  {
    DEBUGF2(2,("%s: Powering Up SD DACs\n",__PRETTY_FUNCTION__));
    val = ReadAuxTVOutReg(TVOUT_PADS_CTL) & ~TVOUT_AUX_PADS_DAC_POFF;
    WriteAuxTVOutReg(TVOUT_PADS_CTL, val);
  }

  DEXIT();
}


void CSTi7108MainOutput::DisableDACs(void)
{
  DENTRY();
  ULONG val;

  /*
   *  Power down HD DACs if SD pipeline is not doing SCART
   */
  stm_clk_divider_output_source_t hdpixsrc;
  m_pClkDivider->getSource(STM_CLK_PIX_HD,&hdpixsrc);

  if(hdpixsrc == STM_CLK_SRC_0)
  {
    DEBUGF2(2,("%s: Powering Down HD DACs\n",__PRETTY_FUNCTION__));
    val = ReadMainTVOutReg(TVOUT_PADS_CTL) | TVOUT_MAIN_PADS_DAC_POFF;
    WriteMainTVOutReg(TVOUT_PADS_CTL, val);
    /*
     * Disable the HDF sample rate converter as well while we are at it.
     */
    SetUpsampler(0,0);
  }

  if(m_bUsingDENC)
  {
    /*
     * Power down SD DACs
     */
    DEBUGF2(2,("%s: Powering Down SD DACs\n",__PRETTY_FUNCTION__));
    val = ReadAuxTVOutReg(TVOUT_PADS_CTL) | TVOUT_AUX_PADS_DAC_POFF;
    WriteAuxTVOutReg(TVOUT_PADS_CTL, val);
  }

  DEXIT();
}
