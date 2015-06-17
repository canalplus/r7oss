/***********************************************************************
 *
 * File: soc/sti7111/sti7111mainoutput.cpp
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
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

#include "sti7111reg.h"
#include "sti7111device.h"
#include "sti7111mainoutput.h"
#include "sti7111hdmi.h"
#include "sti7111mixer.h"


//////////////////////////////////////////////////////////////////////////////
//
// HD, SD Progressive and SD Interlaced Output on main HD Dacs
//
CSTi7111MainOutput::CSTi7111MainOutput(
    CDisplayDevice     *pDev,
    CSTmSDVTG          *pVTG1,
    CSTmSDVTG          *pVTG2,
    CSTmTVOutDENC      *pDENC,
    CDisplayMixer      *pMixer,
    CSTmFSynth         *pFSynth,
    CSTmFSynth         *pFSynthAux,
    CSTmHDFormatterAWG *pAWG,
    stm_plane_id_t      sharedVideoPlaneID,
    stm_plane_id_t      sharedGDPPlaneID): CSTmHDFormatterOutput(pDev, STi7111_MAIN_TVOUT_BASE, STi7111_AUX_TVOUT_BASE, STi7111_HD_FORMATTER_BASE, pVTG1, pVTG2, pDENC, pMixer, pFSynth, pAWG)
{
  m_pFSynthAux = pFSynthAux;
  m_sharedVideoPlaneID = sharedVideoPlaneID;
  m_sharedGDPPlaneID = sharedGDPPlaneID;
}


CSTi7111MainOutput::~CSTi7111MainOutput() {}


void CSTi7111MainOutput::StartSDInterlacedClocks(const stm_mode_line_t *mode)
{
  ULONG val = 0;

  DENTRY();

  /*
   * Make sure we unlock the clock registers every time to catch
   * the case where we are coming back from a low power state.
   */
  WriteClkReg(CKGB_LCK, CKGB_LCK_UNLOCK);

  /*
   * Note: this resets the SD aux pipeline clock dividers again to catch the
   * resume from low power case.
   *
   * TODO: support SD on HDMI/DVO without using DENC/Aux pipeline?
   */
  val = CKGB_CFG_TMDS_HDMI(CKGB_CFG_DIV4);
  val |= CKGB_CFG_DISP_HD(CKGB_CFG_DIV8);
  val |= CKGB_CFG_656(CKGB_CFG_DIV4);
  val |= CKGB_CFG_PIX_SD_FS0(CKGB_CFG_DIV4);
  val |= CKGB_CFG_PIX_SD_FS1(CKGB_CFG_DIV4);
  val |= CKGB_CFG_DISP_ID(CKGB_CFG_DIV8);
  WriteClkReg(CKGB_DISPLAY_CFG, val);

  val = ReadClkReg(CKGB_CLK_SRC) & ~CKGB_SRC_PIXSD_FS1_NOT_FS0;
  WriteClkReg(CKGB_CLK_SRC,val);

  // Ensure Aux pipeline clock is at the correct frequency for shared GDP.
  m_pFSynthAux->Start(mode->TimingParams.ulPixelClock);

  DEXIT();
}


void CSTi7111MainOutput::StartSDProgressiveClocks(const stm_mode_line_t *mode)
{
  ULONG val;

  DENTRY();

  WriteClkReg(CKGB_LCK, CKGB_LCK_UNLOCK);

  val = ReadClkReg(CKGB_DISPLAY_CFG);
  /*
   * Preserve the aux pipeline clock configuration.
   */
  val &= (CKGB_CFG_PIX_SD_FS1(CKGB_CFG_MASK) |
          CKGB_CFG_DISP_ID(CKGB_CFG_MASK)    |
          CKGB_CFG_PIX_HD_FS1_N_FS0);

  val |= CKGB_CFG_TMDS_HDMI(CKGB_CFG_DIV4);
  val |= CKGB_CFG_DISP_HD(CKGB_CFG_DIV4);
  val |= CKGB_CFG_656(CKGB_CFG_DIV2);
  val |= CKGB_CFG_PIX_SD_FS0(CKGB_CFG_DIV4);
  WriteClkReg(CKGB_DISPLAY_CFG, val);

  DEXIT();
}


void CSTi7111MainOutput::StartHDClocks(const stm_mode_line_t *mode)
{
  ULONG val = 0;

  DENTRY();

  WriteClkReg(CKGB_LCK, CKGB_LCK_UNLOCK);

  /*
   * Set the clock divides for each block for HD modes
   */
  val = ReadClkReg(CKGB_DISPLAY_CFG);
  /*
   * Preserve the aux pipeline clock configuration.
   */
  val &= (CKGB_CFG_PIX_SD_FS1(CKGB_CFG_MASK) |
          CKGB_CFG_DISP_ID(CKGB_CFG_MASK)    |
          CKGB_CFG_PIX_HD_FS1_N_FS0);

  if(mode->TimingParams.ulPixelClock == 148500000 ||
     mode->TimingParams.ulPixelClock == 148351648)
  {
    /*
     * 1080p 50/60Hz
     */
    val |= CKGB_CFG_TMDS_HDMI(CKGB_CFG_BYPASS);
    val |= CKGB_CFG_DISP_HD(CKGB_CFG_BYPASS);
    val |= CKGB_CFG_656(CKGB_CFG_BYPASS);
    val |= CKGB_CFG_PIX_SD_FS0(CKGB_CFG_BYPASS);

  }
  else
  {
    /*
     * 1080p 25/30Hz, 1080i, 720p
     */
    val |= CKGB_CFG_TMDS_HDMI(CKGB_CFG_DIV2);
    val |= CKGB_CFG_DISP_HD(CKGB_CFG_DIV2);
    val |= CKGB_CFG_656(CKGB_CFG_DIV2);
    val |= CKGB_CFG_PIX_SD_FS0(CKGB_CFG_DIV2);

  }

  WriteClkReg(CKGB_DISPLAY_CFG, val);

  DEXIT();
}


void CSTi7111MainOutput::SetMainClockToHDFormatter(void)
{
  DENTRY();
  ULONG val = ReadClkReg(CKGB_DISPLAY_CFG) & ~CKGB_CFG_PIX_HD_FS1_N_FS0;
  WriteClkReg(CKGB_DISPLAY_CFG,val);
  DEXIT();
}


bool CSTi7111MainOutput::ShowPlane(stm_plane_id_t planeID)
{
  DEBUGF2(2,("CSTi7111MainOutput::ShowPlane %d\n",(int)planeID));

  if(planeID == m_sharedGDPPlaneID)
  {
    if(m_pCurrentMode->TimingParams.ulPixelClock == 135000000 ||
       m_pCurrentMode->TimingParams.ulPixelClock == 135135000)
    {
      /*
       * If the main video path is set for an interlaced SD mode we need
       * to use the SD/Aux divided down clock to get 13.5MHz on the shared
       * GDP.
       */
      ULONG val = ReadClkReg(CKGB_CLK_SRC) | CKGB_SRC_GDP3_FS1_NOT_FS0;
      WriteClkReg(CKGB_CLK_SRC, val);
      DEBUGF2(2,("CSTi7111MainOutput::ShowPlane GDP3 clock set to FS1\n"));
    }
    else
    {
      /*
       * For progressive and all HD modes change the clock source for the
       * shared GDP to the HD/Main video clock
       */
      ULONG val = ReadClkReg(CKGB_CLK_SRC) & ~CKGB_SRC_GDP3_FS1_NOT_FS0;
      WriteClkReg(CKGB_CLK_SRC, val);
      DEBUGF2(2,("CSTi7111MainOutput::ShowPlane GDP3 clock set to FS0\n"));
    }
  }
  else if (planeID == m_sharedVideoPlaneID)
  {
    /*
     * Change the clock source for shared video plane when on Mixer1
     */
    ULONG val = ReadSysCfgReg(SYS_CFG6) | SYS_CFG6_VIDEO2_MAIN_N_AUX;
    WriteSysCfgReg(SYS_CFG6, val);
    DEBUGF2(2,("CSTi7111MainOutput::ShowPlane VID2 clock set to HD pixel clock\n"));
  }

  return CSTmMasterOutput::m_pMixer->EnablePlane(planeID);
}


void CSTi7111MainOutput::EnableDACs(void)
{
  ULONG val;

  DENTRY();

  val = ReadSysCfgReg(SYS_CFG3);

  if(m_ulOutputFormat & (STM_VIDEO_OUT_RGB|STM_VIDEO_OUT_YUV))
  {
    DEBUGF2(2,("%s: Enabling HD DACs\n",__PRETTY_FUNCTION__));
    val &= ~(SYS_CFG3_DAC_HD_HZW_DISABLE |
             SYS_CFG3_DAC_HD_HZV_DISABLE |
             SYS_CFG3_DAC_HD_HZU_DISABLE);
  }
  else if(!(ReadClkReg(CKGB_DISPLAY_CFG) & CKGB_CFG_PIX_HD_FS1_N_FS0))
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


void CSTi7111MainOutput::DisableDACs(void)
{
  DENTRY();
  ULONG val;

  /*
   *  Power down HD DACs if SD pipeline is not doing SCART
   */
  if(!(ReadClkReg(CKGB_DISPLAY_CFG) & CKGB_CFG_PIX_HD_FS1_N_FS0))
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
