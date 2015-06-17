/***********************************************************************
 *
 * File: soc/sti7200/sti7200cut2localmainoutput.cpp
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
#include <HDTVOutFormatter/stmtvoutdenc.h>

#include "sti7200reg.h"
#include "sti7200cut2tvoutreg.h"
#include "sti7200device.h"
#include "sti7200cut2localmainoutput.h"
#include "sti7200hdmi.h"
#include "sti7200mixer.h"


//////////////////////////////////////////////////////////////////////////////
//
// HD, SD Progressive and SD Interlaced Output on main HD Dacs
//
CSTi7200Cut2LocalMainOutput::CSTi7200Cut2LocalMainOutput(
    CDisplayDevice         *pDev,
    CSTmSDVTG              *pVTG1,
    CSTmSDVTG              *pVTG2,
    CSTmTVOutDENC          *pDENC,
    CSTi7200LocalMainMixer *pMixer,
    CSTmFSynth             *pFSynth,
    CSTmHDFormatterAWG     *pAWG): CSTmHDFormatterOutput(pDev,
                                                         STi7200C2_LOCAL_DISPLAY_BASE+STi7200_MAIN_TVOUT_OFFSET,
                                                         STi7200C2_LOCAL_AUX_TVOUT_BASE,
                                                         STi7200C2_LOCAL_FORMATTER_BASE,
                                                         pVTG1, pVTG2, pDENC, pMixer, pFSynth, pAWG)
{
}


CSTi7200Cut2LocalMainOutput::~CSTi7200Cut2LocalMainOutput() {}


void CSTi7200Cut2LocalMainOutput::StartSDInterlacedClocks(const stm_mode_line_t *mode)
{
  ULONG val;

  DENTRY();

  /*
   * The 7200cut2 was supposed to have the same IP as 7111 but it appears that
   * it doesn't have the individual Cb/Cr rescale.
   */
  m_bHasSeparateCbCrRescale = false;

  val = ReadSysCfgReg(SYS_CFG49) | SYS_CFG49_LOCAL_AUX_PIX_HD | SYS_CFG49_LOCAL_SDTVO_HD;
  WriteSysCfgReg(SYS_CFG49,val);

  SetMainClockToHDFormatter();

  /*
   * No other clock divides, are the auto divider settings ok?
   */
  DEXIT();
}


void CSTi7200Cut2LocalMainOutput::StartSDProgressiveClocks(const stm_mode_line_t *mode)
{
  DENTRY();
  /*
   * Nothing to do, are the auto divider settings ok?
   */
  DEXIT();
}


void CSTi7200Cut2LocalMainOutput::StartHDClocks(const stm_mode_line_t *mode)
{
  DENTRY();
  /*
   * Nothing to do, are the auto divider settings ok?
   */
  DEXIT();
}


void CSTi7200Cut2LocalMainOutput::SetMainClockToHDFormatter(void)
{
  DENTRY();
  ULONG val = ReadAuxTVOutReg(TVOUT_GP_OUT) & ~TVOUT_GP_OUT_HD_PIX_SD_N_HD;
  WriteAuxTVOutReg(TVOUT_GP_OUT,val);
  DEXIT();
}


bool CSTi7200Cut2LocalMainOutput::ShowPlane(stm_plane_id_t planeID)
{
  ULONG val;
  DEBUGF2(2,("CSTi7200Cut2LocalMainOutput::ShowPlane %d\n",(int)planeID));

  if(planeID == OUTPUT_GDP3)
  {
    val = ReadSysCfgReg(SYS_CFG49) & ~SYS_CFG49_LOCAL_GDP3_SD;
    WriteSysCfgReg(SYS_CFG49, val);
  }
  else if (planeID == OUTPUT_VID2)
  {
    val = ReadSysCfgReg(SYS_CFG49) & ~SYS_CFG49_LOCAL_VDP_SD;
    WriteSysCfgReg(SYS_CFG49, val);
  }

  return CSTmMasterOutput::m_pMixer->EnablePlane(planeID);
}


void CSTi7200Cut2LocalMainOutput::EnableDACs(void)
{
  ULONG val;

  DENTRY();

  val = ReadSysCfgReg(SYS_CFG1);

  if(m_ulOutputFormat & (STM_VIDEO_OUT_RGB|STM_VIDEO_OUT_YUV))
  {
    DEBUGF2(2,("%s: Enabling HD DACs\n",__PRETTY_FUNCTION__));
    val &= ~(SYS_CFG1_DAC_HD_HZU_DISABLE |
             SYS_CFG1_DAC_HD_HZV_DISABLE |
             SYS_CFG1_DAC_HD_HZW_DISABLE);
  }
  else if(!(ReadAuxTVOutReg(TVOUT_GP_OUT) & TVOUT_GP_OUT_HD_PIX_SD_N_HD))
  {
    /*
     * Only tri-state the HD DAC setup if the SD pipeline is not
     * using the HD formatter for SCART.
     */
    DEBUGF2(2,("%s: Tri-Stating HD DACs\n",__PRETTY_FUNCTION__));
    val |= (SYS_CFG1_DAC_HD_HZU_DISABLE |
            SYS_CFG1_DAC_HD_HZV_DISABLE |
            SYS_CFG1_DAC_HD_HZW_DISABLE);
  }

  if(m_bUsingDENC)
  {
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
  }

  WriteSysCfgReg(SYS_CFG1,val);

  if(!m_bDacHdPowerDisabled)
  {
    val = ReadMainTVOutReg(TVOUT_PADS_CTL) & ~TVOUT_MAIN_PADS_DAC_POFF;
    WriteMainTVOutReg(TVOUT_PADS_CTL, val);
  }

  if(m_bUsingDENC)
  {
    val = ReadAuxTVOutReg(TVOUT_PADS_CTL) & ~TVOUT_AUX_PADS_DAC_POFF;
    WriteAuxTVOutReg(TVOUT_PADS_CTL, val);
  }

  DEXIT();
}


void CSTi7200Cut2LocalMainOutput::DisableDACs(void)
{
  DENTRY();
  ULONG val;

  /*
   *  Power down HD DACs if SD pipeline is not doing SCART
   */
  if(!(ReadAuxTVOutReg(TVOUT_GP_OUT) & TVOUT_GP_OUT_HD_PIX_SD_N_HD))
  {
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
    val = ReadAuxTVOutReg(TVOUT_PADS_CTL) | TVOUT_AUX_PADS_DAC_POFF;
    WriteAuxTVOutReg(TVOUT_PADS_CTL, val);
  }

  DEXIT();
}
