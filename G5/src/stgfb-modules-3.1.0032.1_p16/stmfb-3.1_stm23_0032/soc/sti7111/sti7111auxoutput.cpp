/***********************************************************************
 *
 * File: soc/sti7111/sti7111auxoutput.cpp
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


#include <STMCommon/stmsdvtg.h>
#include <STMCommon/stmfsynth.h>

#include <HDTVOutFormatter/stmhdfawg.h>
#include <HDTVOutFormatter/stmhdfoutput.h>
#include <HDTVOutFormatter/stmtvoutreg.h>
#include <HDTVOutFormatter/stmtvoutdenc.h>

#include "sti7111reg.h"
#include "sti7111device.h"
#include "sti7111auxoutput.h"
#include "sti7111mixer.h"

CSTi7111AuxOutput::CSTi7111AuxOutput(
    CSTi7111Device        *pDev,
    CSTmSDVTG             *pVTG,
    CSTmTVOutDENC         *pDENC,
    CDisplayMixer         *pMixer,
    CSTmFSynth            *pFSynth,
    CSTmHDFormatterAWG    *pAWG,
    CSTmHDFormatterOutput *pHDFOutput,
    stm_plane_id_t         sharedVideoPlaneID,
    stm_plane_id_t         sharedGDPPlaneID): CSTmAuxTVOutput(pDev,
                                                        STi7111_MAIN_TVOUT_BASE,
                                                        STi7111_AUX_TVOUT_BASE,
                                                        STi7111_HD_FORMATTER_BASE,
                                                        pVTG,
                                                        pDENC,
                                                        pMixer,
                                                        pFSynth,
                                                        pAWG,
                                                        pHDFOutput)
{
  DENTRY();

  m_sharedVideoPlaneID = sharedVideoPlaneID;
  m_sharedGDPPlaneID = sharedGDPPlaneID;

  /*
   * We set up SD to be divided down from 108MHz, so we can run the
   * HD DACs at the same speed as when used from the main output.
   */
  m_pFSynth->SetDivider(8);

  CSTi7111AuxOutput::DisableDACs();

  DEXIT();
}


void CSTi7111AuxOutput::StartClocks(const stm_mode_line_t *mode)
{
  DENTRY();

  /*
   * Make sure we unlock the clock registers and setup the clock dividers
   * every time to catch the case where we are coming back from a low power
   * state.
   */
  WriteClkReg(CKGB_LCK, CKGB_LCK_UNLOCK);

  ULONG val = ReadClkReg(CKGB_DISPLAY_CFG);

  val &= ~(CKGB_CFG_PIX_SD_FS1(CKGB_CFG_MASK) | CKGB_CFG_DISP_ID(CKGB_CFG_MASK));
  val |=  (CKGB_CFG_PIX_SD_FS1(CKGB_CFG_DIV4) | CKGB_CFG_DISP_ID(CKGB_CFG_DIV8));

  WriteClkReg(CKGB_DISPLAY_CFG, val);

  m_pFSynth->SetAdjustment(0);
  m_pFSynth->Start(mode->TimingParams.ulPixelClock);

  /*
   * Set the SD pixel clock to be sourced from the SD FSynth.
   */
  val = ReadClkReg(CKGB_CLK_SRC) | CKGB_SRC_PIXSD_FS1_NOT_FS0;
  WriteClkReg(CKGB_CLK_SRC, val);

  /*
   * Configure TVOut clocks driven from SD pixel clock from FS1,
   * which will be 27MHz due to the ClockGenB dividers.
   */
  WriteAuxTVOutReg(TVOUT_CLK_SEL, (TVOUT_AUX_CLK_SEL_PIX1X(TVOUT_CLK_DIV_2) |
                                   TVOUT_AUX_CLK_SEL_DENC(TVOUT_CLK_BYPASS) |
                                   TVOUT_AUX_CLK_SEL_SD_N_HD));


  DEXIT();
}


void CSTi7111AuxOutput::SetAuxClockToHDFormatter(void)
{
  DENTRY();
  /*
   * Switch the HD DAC clock to the SD clock
   */
  ULONG val = ReadClkReg(CKGB_DISPLAY_CFG) | CKGB_CFG_PIX_HD_FS1_N_FS0;
  WriteClkReg(CKGB_DISPLAY_CFG,val);

  DEXIT();
}


bool CSTi7111AuxOutput::ShowPlane(stm_plane_id_t planeID)
{
  ULONG val;
  DEBUGF2(2,("CSTi7111LocalMainOutput::ShowPlane %d\n",(int)planeID));

  if(planeID == m_sharedGDPPlaneID)
  {
    // Change the clock source for the shared GDP to the SD/Aux video clock
    ULONG val = ReadClkReg(CKGB_CLK_SRC) | CKGB_SRC_GDP3_FS1_NOT_FS0;
    WriteClkReg(CKGB_CLK_SRC, val);
  }
  else if (planeID == m_sharedVideoPlaneID)
  {
    val = ReadSysCfgReg(SYS_CFG6) & ~SYS_CFG6_VIDEO2_MAIN_N_AUX;
    WriteSysCfgReg(SYS_CFG6, val);
  }

  return CSTmMasterOutput::m_pMixer->EnablePlane(planeID);
}


void CSTi7111AuxOutput::EnableDACs(void)
{
  ULONG val;

  DENTRY();

  val = ReadSysCfgReg(SYS_CFG3);

  if(m_ulOutputFormat & (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_RGB))
  {
    /*
     * We can blindly enable the HD DACs if we need them, it doesn't
     * matter if they were already in use by the HD pipeline.
     */
    DEBUGF2(2,("%s: Enabling HD DACs\n",__PRETTY_FUNCTION__));
    val &= ~(SYS_CFG3_DAC_HD_HZU_DISABLE |
             SYS_CFG3_DAC_HD_HZV_DISABLE |
             SYS_CFG3_DAC_HD_HZW_DISABLE);

    ULONG hddacs = ReadMainTVOutReg(TVOUT_PADS_CTL) & ~TVOUT_MAIN_PADS_DAC_POFF;
    WriteMainTVOutReg(TVOUT_PADS_CTL, hddacs);
  }

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

  WriteSysCfgReg(SYS_CFG3,val);

  ULONG sddacs = ReadAuxTVOutReg(TVOUT_PADS_CTL) & ~TVOUT_AUX_PADS_DAC_POFF;
  WriteAuxTVOutReg(TVOUT_PADS_CTL, sddacs);

  DEXIT();
}


void CSTi7111AuxOutput::DisableDACs(void)
{
  DENTRY();
  ULONG val;

  if(m_ulOutputFormat & (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_RGB))
  {
    /*
     * Disable HD DACs only if this pipeline using them
     */
    if(ReadClkReg(CKGB_DISPLAY_CFG) & CKGB_CFG_PIX_HD_FS1_N_FS0)
    {
      DEBUGF2(2,("%s: Disabling HD DACs\n",__PRETTY_FUNCTION__));
      val = ReadMainTVOutReg(TVOUT_PADS_CTL) | TVOUT_MAIN_PADS_DAC_POFF;
      WriteMainTVOutReg(TVOUT_PADS_CTL,val);
    }
  }

  val = ReadAuxTVOutReg(TVOUT_PADS_CTL) | TVOUT_AUX_PADS_DAC_POFF;
  WriteAuxTVOutReg(TVOUT_PADS_CTL, val);

  DEXIT();
}
