/***********************************************************************
 *
 * File: soc/sti5206/sti5206mainoutput.cpp
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

#include "sti5206reg.h"
#include "sti5206device.h"
#include "sti5206mainoutput.h"
#include "sti5206mixer.h"
#include "sti5206clkdivider.h"

//////////////////////////////////////////////////////////////////////////////
//
// HD, SD Progressive and SD Interlaced Output on main HD Dacs
//
CSTi5206MainOutput::CSTi5206MainOutput(
    CSTi5206Device     *pDev,
    CSTmSDVTG          *pVTG1,
    CSTmSDVTG          *pVTG2,
    CSTmTVOutDENC      *pDENC,
    CSTi5206MainMixer  *pMixer,
    CSTmFSynth         *pFSynth,
    CSTmHDFormatterAWG *pAWG,
    CSTi5206ClkDivider *pClkDivider): CSTmHDFormatterOutput(pDev, STi5206_MAIN_TVOUT_BASE, STi5206_AUX_TVOUT_BASE, STi5206_HD_FORMATTER_BASE, pVTG1, pVTG2, pDENC, pMixer, pFSynth, pAWG)
{
  DENTRY();
  m_pClkDivider = pClkDivider;
  DEXIT();
}


CSTi5206MainOutput::~CSTi5206MainOutput() {}


void CSTi5206MainOutput::StartSDInterlacedClocks(const stm_mode_line_t *mode)
{
  DENTRY();

  m_pClkDivider->Enable(STM_CLK_PIX_ED, STM_CLK_SRC_0, STM_CLK_DIV_1);
  m_pClkDivider->Enable(STM_CLK_DISP_ED, STM_CLK_SRC_0, STM_CLK_DIV_8);
  m_pClkDivider->Enable(STM_CLK_656, STM_CLK_SRC_0, STM_CLK_DIV_4);
  m_pClkDivider->Enable(STM_CLK_PIX_SD, STM_CLK_SRC_0, STM_CLK_DIV_4);
  m_pClkDivider->Enable(STM_CLK_DISP_SD, STM_CLK_SRC_0, STM_CLK_DIV_8);

  DEXIT();
}


void CSTi5206MainOutput::StartSDProgressiveClocks(const stm_mode_line_t *mode)
{
  DENTRY();

  m_pClkDivider->Enable(STM_CLK_PIX_ED, STM_CLK_SRC_0, STM_CLK_DIV_1);
  m_pClkDivider->Enable(STM_CLK_DISP_ED, STM_CLK_SRC_0, STM_CLK_DIV_4);
  m_pClkDivider->Enable(STM_CLK_656, STM_CLK_SRC_0, STM_CLK_DIV_2);

  DEXIT();
}


void CSTi5206MainOutput::StartHDClocks(const stm_mode_line_t *mode)
{
  DENTRY();

  m_pClkDivider->Enable(STM_CLK_PIX_ED, STM_CLK_SRC_0, STM_CLK_DIV_1);

  if(mode->TimingParams.ulPixelClock >= 148000000)
  {
    m_pClkDivider->Enable(STM_CLK_DISP_ED, STM_CLK_SRC_0, STM_CLK_DIV_1);
    m_pClkDivider->Disable(STM_CLK_656);
  }
  else
  {
    m_pClkDivider->Enable(STM_CLK_DISP_ED, STM_CLK_SRC_0, STM_CLK_DIV_2);
    m_pClkDivider->Enable(STM_CLK_656, STM_CLK_SRC_0, STM_CLK_DIV_1);
  }

  DEXIT();
}


void CSTi5206MainOutput::SetMainClockToHDFormatter(void)
{
  DENTRY();
  m_pClkDivider->Enable(STM_CLK_PIX_ED, STM_CLK_SRC_0, STM_CLK_DIV_1);
  DEXIT();
}


bool CSTi5206MainOutput::ShowPlane(stm_plane_id_t planeID)
{
  DEBUGF2(2,("CSTi5206MainOutput::ShowPlane %d\n",(int)planeID));

  stm_clk_divider_output_divide_t div;

  if(!m_pClkDivider->getDivide(STM_CLK_DISP_ED,&div))
  {
    DERROR("Cannot get display clock divider\n");
    return false;
  }

  if(planeID == OUTPUT_GDP1)
  {
    m_pClkDivider->Enable(STM_CLK_GDP1, STM_CLK_SRC_0, div);
    DEBUGF2(2,("CSTi5206MainOutput::ShowPlane GDP1 clock set to ED disp clock\n"));
  }
  else if (planeID == OUTPUT_VID1)
  {
    m_pClkDivider->Enable(STM_CLK_PIP, STM_CLK_SRC_0, div);
    DEBUGF2(2,("CSTi5206MainOutput::ShowPlane PIP clock set to ED disp clock\n"));
  }

  return CSTmMasterOutput::m_pMixer->EnablePlane(planeID);
}


void CSTi5206MainOutput::EnableDACs(void)
{
  DENTRY();

  ULONG val = ReadSysCfgReg(SYS_CFG3);

  stm_clk_divider_output_source_t edpixsrc;
  m_pClkDivider->getSource(STM_CLK_PIX_ED,&edpixsrc);

  if(m_ulOutputFormat & (STM_VIDEO_OUT_RGB|STM_VIDEO_OUT_YUV))
  {
    DEBUGF2(2,("%s: Enabling HD DACs\n",__PRETTY_FUNCTION__));
    val &= ~(SYS_CFG3_DAC_HD_HZW_DISABLE |
             SYS_CFG3_DAC_HD_HZV_DISABLE |
             SYS_CFG3_DAC_HD_HZU_DISABLE);
  }
  else if(edpixsrc == STM_CLK_SRC_0)
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


void CSTi5206MainOutput::DisableDACs(void)
{
  DENTRY();
  ULONG val;

  /*
   *  Power down HD DACs if SD pipeline is not doing SCART
   */
  stm_clk_divider_output_source_t edpixsrc;
  m_pClkDivider->getSource(STM_CLK_PIX_ED,&edpixsrc);

  if(edpixsrc == STM_CLK_SRC_0)
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
