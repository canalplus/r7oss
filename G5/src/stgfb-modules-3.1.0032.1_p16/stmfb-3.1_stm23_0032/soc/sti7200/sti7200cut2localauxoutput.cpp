/***********************************************************************
 *
 * File: soc/sti7200/sti7200cut2localauxoutput.cpp
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
#include <STMCommon/stmsdvtg.h>
#include <STMCommon/stmfsynth.h>

#include <HDTVOutFormatter/stmhdfawg.h>
#include <HDTVOutFormatter/stmhdfoutput.h>
#include <HDTVOutFormatter/stmtvoutdenc.h>

#include "sti7200reg.h"
#include "sti7200cut2tvoutreg.h"
#include "sti7200cut2localdevice.h"
#include "sti7200cut2localauxoutput.h"
#include "sti7200mixer.h"

CSTi7200Cut2LocalAuxOutput::CSTi7200Cut2LocalAuxOutput(
    CSTi7200Cut2LocalDevice   *pDev,
    CSTmSDVTG                 *pVTG,
    CSTmTVOutDENC             *pDENC,
    CSTi7200LocalAuxMixer     *pMixer,
    CSTmFSynthType2           *pFSynth,
    CSTmHDFormatterAWG        *pAWG,
    CSTmHDFormatterOutput     *pHDFOutput): CSTmAuxTVOutput(pDev,
                                                            STi7200C2_LOCAL_DISPLAY_BASE+STi7200_MAIN_TVOUT_OFFSET,
                                                            STi7200C2_LOCAL_AUX_TVOUT_BASE,
                                                            STi7200C2_LOCAL_FORMATTER_BASE,
                                                            pVTG,
                                                            pDENC,
                                                            pMixer,
                                                            pFSynth,
                                                            pAWG,
                                                            pHDFOutput)
{
  DENTRY();

  /*
   * We setup the TVOut clock to be 108MHz with a /2 from 216MHz. The
   * pixel clock will be from the fixed /8.
   */
  ULONG tmp = ReadClkReg(CLKB_OUT_MUX_CFG) & ~CLKBC2_OUT_MUX_SD0_EXT;
  tmp |= CLKBC2_OUT_MUX_SD0_DIV2;
  WriteClkReg(CLKB_OUT_MUX_CFG,tmp);

  m_pFSynth->SetDivider(16);
  m_pFSynth->Start(13500000);

  m_bHasSeparateCbCrRescale = false;

  CSTi7200Cut2LocalAuxOutput::DisableDACs();

  DEXIT();
}


void CSTi7200Cut2LocalAuxOutput::StartClocks(const stm_mode_line_t *mode)
{
  DENTRY();

  /*
   * There is no need to start the clock as the base frequency is fixed,
   * however reset any clock recovery adjustments to zero.
   */
  m_pFSynth->SetAdjustment(0);

  /*
   * Ensure we have the right clock routing to start SD mode from. This may
   * have been previously changed by the main output pipeline so don't be
   * tempted to put this into the constructor.
   */
  ULONG val = ReadSysCfgReg(SYS_CFG49) & ~(SYS_CFG49_LOCAL_SDTVO_HD | SYS_CFG49_LOCAL_AUX_PIX_HD);
  WriteSysCfgReg(SYS_CFG49, val);

  /*
   * Configure TVOut sub-clocks, driven from an 108MHz input.
   */
  WriteAuxTVOutReg(TVOUT_CLK_SEL, (TVOUT_AUX_CLK_SEL_PIX1X(TVOUT_CLK_DIV_8) |
                                   TVOUT_AUX_CLK_SEL_DENC(TVOUT_CLK_DIV_4)  |
                                   TVOUT_AUX_CLK_SEL_SD_N_HD));


  DEXIT();
}


void CSTi7200Cut2LocalAuxOutput::SetAuxClockToHDFormatter(void)
{
  DENTRY();

  /*
   * Switch the HD DAC clock to the SD clock
   */
  ULONG val = ReadAuxTVOutReg(TVOUT_GP_OUT) | TVOUT_GP_OUT_HD_PIX_SD_N_HD;
  WriteAuxTVOutReg(TVOUT_GP_OUT,val);

  DEXIT();
}


bool CSTi7200Cut2LocalAuxOutput::ShowPlane(stm_plane_id_t planeID)
{
  ULONG val;
  DEBUGF2(2,("CSTi7200LocalMainOutput::ShowPlane %d\n",(int)planeID));

  if(planeID == OUTPUT_GDP3)
  {
    val = ReadSysCfgReg(SYS_CFG49) | SYS_CFG49_LOCAL_GDP3_SD;
    WriteSysCfgReg(SYS_CFG49, val);
  }
  else if (planeID == OUTPUT_VID2)
  {
    val = ReadSysCfgReg(SYS_CFG49) | SYS_CFG49_LOCAL_VDP_SD;
    WriteSysCfgReg(SYS_CFG49, val);
  }

  return CSTmMasterOutput::m_pMixer->EnablePlane(planeID);
}


void CSTi7200Cut2LocalAuxOutput::EnableDACs(void)
{
  ULONG val;

  DENTRY();

  val = ReadSysCfgReg(SYS_CFG1);

  if(m_ulOutputFormat & (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_RGB))
  {
    /*
     * We can blindly enable the HD DACs if we need them, it doesn't
     * matter if they were already in use by the HD pipeline.
     */
    DEBUGF2(2,("%s: Enabling HD DACs\n",__PRETTY_FUNCTION__));
    val &= ~(SYS_CFG1_DAC_HD_HZU_DISABLE |
             SYS_CFG1_DAC_HD_HZV_DISABLE |
             SYS_CFG1_DAC_HD_HZW_DISABLE);

    ULONG hddacs = ReadMainTVOutReg(TVOUT_PADS_CTL) & ~TVOUT_MAIN_PADS_DAC_POFF;
    WriteMainTVOutReg(TVOUT_PADS_CTL, hddacs);
  }

  if(m_ulOutputFormat & STM_VIDEO_OUT_CVBS)
  {
    DEBUGF2(2,("%s: Enabling CVBS DAC\n",__PRETTY_FUNCTION__));
    val &= ~SYS_CFG1_DAC_SD0_HZU_DISABLE;
  }
  else
  {
    DEBUGF2(2,("%s: Tri-Stating CVBS DAC\n",__PRETTY_FUNCTION__));
    val |=  SYS_CFG1_DAC_SD0_HZU_DISABLE;
  }

  if(m_ulOutputFormat & STM_VIDEO_OUT_YC)
  {
    DEBUGF2(2,("%s: Enabling Y/C DACs\n",__PRETTY_FUNCTION__));
    val &= ~(SYS_CFG1_DAC_SD0_HZV_DISABLE |
             SYS_CFG1_DAC_SD0_HZW_DISABLE);
  }
  else
  {
    DEBUGF2(2,("%s: Tri-Stating Y/C DACs\n",__PRETTY_FUNCTION__));
    val |=  (SYS_CFG1_DAC_SD0_HZV_DISABLE |
             SYS_CFG1_DAC_SD0_HZW_DISABLE);
  }

  WriteSysCfgReg(SYS_CFG1,val);

  ULONG sddacs = ReadAuxTVOutReg(TVOUT_PADS_CTL) & ~TVOUT_AUX_PADS_DAC_POFF;
  WriteAuxTVOutReg(TVOUT_PADS_CTL, sddacs);

  DEXIT();
}


void CSTi7200Cut2LocalAuxOutput::DisableDACs(void)
{
  ULONG val;
  DENTRY();

  if(m_ulOutputFormat & (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_RGB))
  {
    /*
     * Disable HD DACs only if this pipeline using them
     */
    if(ReadAuxTVOutReg(TVOUT_GP_OUT) & TVOUT_GP_OUT_HD_PIX_SD_N_HD)
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
