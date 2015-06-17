/***********************************************************************
 *
 * File: HDTVOutFormatter/stmauxtvoutput.cpp
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
#include <Generic/DisplayDevice.h>
#include <Generic/DisplayMixer.h>

#include <STMCommon/stmsdvtg.h>
#include <STMCommon/stmfsynth.h>

#include <HDTVOutFormatter/stmhdfawg.h>
#include <HDTVOutFormatter/stmhdfoutput.h>
#include <HDTVOutFormatter/stmtvoutreg.h>
#include <HDTVOutFormatter/stmtvoutdenc.h>
#include <HDTVOutFormatter/stmauxtvoutput.h>


CSTmAuxTVOutput::CSTmAuxTVOutput(
    CDisplayDevice        *pDev,
    ULONG                  ulMainTVOut,
    ULONG                  ulAuxTVOut,
    ULONG                  ulTVFmt,
    CSTmSDVTG             *pVTG,
    CSTmTVOutDENC         *pDENC,
    CDisplayMixer         *pMixer,
    CSTmFSynth            *pFSynth,
    CSTmHDFormatterAWG    *pAWG,
    CSTmHDFormatterOutput *pHDFOutput): CSTmMasterOutput(pDev, pDENC, pVTG, pMixer, pFSynth)
{
  DENTRY();

  m_ulMainTVOut = ulMainTVOut;
  m_ulAuxTVOut  = ulAuxTVOut;
  m_ulTVFmt     = ulTVFmt;

  m_bHasSeparateCbCrRescale = true;

  m_ulDENCSyncOffset = -4;

  m_pAWG = pAWG;
  m_pHDFOutput = pHDFOutput;

  m_ulInputSource = STM_AV_SOURCE_MAIN_INPUT;
  CSTmMasterOutput::SetControl(STM_CTRL_VIDEO_OUT_SELECT,(STM_VIDEO_OUT_YC | STM_VIDEO_OUT_CVBS));

  /*
   * Default sync selection, note the HD Formatter sync will change depending
   * on which pipeline is actually using the HD DACs.
   *
   * Sync1  - Positive H sync, V sync is actually a top not bot, needed by the
   *          DENC. Yes you might think it might use the ref bnott signal but it
   *          doesn't.
   * Sync2  - External syncs, polarity as specified in standards, could be moved
   *          independently to Sync1 to compensate for signal delays.
   * Sync3  - Internal signal generator sync, again movable relative to Sync1
   *          to allow for delays in the generator block.
   */
  WriteAuxTVOutReg(TVOUT_SYNC_SEL, (TVOUT_AUX_SYNC_DENC_SYNC1      |
                                    TVOUT_AUX_SYNC_VTG_SLAVE_SYNC1 |
                                    TVOUT_AUX_SYNC_AWG_SYNC3       |
                                    TVOUT_AUX_SYNC_HDF_VTG1_REF));

  m_pVTG->SetSyncType(1, STVTG_SYNC_TOP_NOT_BOT);
  m_pVTG->SetSyncType(2, STVTG_SYNC_TIMING_MODE);
  m_pVTG->SetSyncType(3, STVTG_SYNC_POSITIVE);

  /*
   * Default pad configuration
   */
  WriteAuxTVOutReg(TVOUT_PADS_CTL,(TVOUT_AUX_PADS_HSYNC_SYNC2 |
                                   TVOUT_PADS_VSYNC_SYNC2     |
                                   TVOUT_AUX_PADS_DAC_POFF    |
                                   TVOUT_AUX_PADS_SD_DAC_456_N_123 ));

  /*
   * CVBS delay for RGB SCART due to the additional latency of routing RGB
   * through the HD Formatter to the HD DACs
   */
  WriteAuxTVOutReg(TVOUT_SD_DEL,2);

  m_maxDACVoltage  = 1400; // in mV
  m_DACSaturation  = 1023;
  RecalculateDACSetup();

  DEXIT();
}


const stm_mode_line_t* CSTmAuxTVOutput::SupportedMode(const stm_mode_line_t *mode) const
{
  if((mode->ModeParams.OutputStandards & STM_OUTPUT_STD_SD_MASK) == 0)
    return 0;

  if(mode->ModeParams.ScanType == SCAN_P)
    return 0;

  if(mode->TimingParams.ulPixelClock == 13500000)
    return mode;

  return 0;
}


bool CSTmAuxTVOutput::Start(const stm_mode_line_t *mode, ULONG tvStandard)
{
  ULONG tmp;
  DENTRY();

  if(!m_pCurrentMode && (m_pVTG->GetCurrentMode() != 0))
  {
    DEBUGF2(1,("CSTmAuxTVOutput::Start - hardware in use by main output\n"));
    return false;
  }

  this->StartClocks(mode);

  /*
   * Ensure the DENC is getting the video from the Aux mixer
   */
  tmp = ReadTVFmtReg(TVFMT_ANA_CFG) & ~ANA_CFG_SEL_MAIN_TO_DENC;
  WriteTVFmtReg(TVFMT_ANA_CFG,tmp);

  m_pDENC->SetMainOutputFormat(m_ulOutputFormat);

  /*
   * Make sure the VTG ref signals are +ve for master mode
   */
  m_pVTG->SetSyncType(0, STVTG_SYNC_POSITIVE);

  /*
   * Make sure the offsets are reset in case the VTG has previously been in use
   * by the main display output.
   */
  m_pVTG->SetHSyncOffset(1, m_ulDENCSyncOffset);
  m_pVTG->SetVSyncHOffset(1, m_ulDENCSyncOffset);

  if(!CSTmMasterOutput::Start(mode, tvStandard))
  {
    DEBUGF2(2,("CSTmAuxTVOutput::Start - failed\n"));
    return false;
  }

  DEXIT();

  return true;
}


void CSTmAuxTVOutput::Resume(void)
{
  DENTRY();

  if(!m_bIsSuspended)
    return;

  if(m_pCurrentMode)
  {
    /*
     * We need to go and make sure all the clock setup is correct again
     */
    this->StartClocks(m_pCurrentMode);

    if(m_ulOutputFormat & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV))
      this->SetAuxClockToHDFormatter();
  }

  CSTmMasterOutput::Resume();

  DEXIT();
}


ULONG CSTmAuxTVOutput::GetCapabilities(void) const
{
  ULONG caps = (STM_OUTPUT_CAPS_SD_ANALOGUE       |
                STM_OUTPUT_CAPS_PLANE_MIXER       |
                STM_OUTPUT_CAPS_MODE_CHANGE       |
                STM_OUTPUT_CAPS_SD_RGB_CVBS_YC    |
                STM_OUTPUT_CAPS_SD_YPrPb_CVBS_YC);

  return caps;
}


bool CSTmAuxTVOutput::SetOutputFormat(ULONG format)
{
  DEBUGF2(2,("CSTmAuxTVOutput::SetOutputFormat - format = 0x%lx\n",format));

  /*
   * Cannot support RGB+YUV together
   */
  if((format & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV)) == (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV))
    return false;

  if(m_pCurrentMode)
  {
    m_pDENC->SetMainOutputFormat(format);

    /*
     * Switch HD formatter for RGB(SCART) or YPrPb output from the aux pipeline
     */
    if(format & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV))
    {
      ULONG val;
      /*
       * We must disable any sync program running as this will corrupt the
       * signal from the DENC.
       */
      m_pAWG->Disable();

      /*
       * We have to clear this bit to get output on the HD DACs.
       */
      val = ReadAuxTVOutReg(TVOUT_CLK_SEL) & ~TVOUT_AUX_CLK_SEL_SD_N_HD;
      WriteAuxTVOutReg(TVOUT_CLK_SEL, val);

      this->SetAuxClockToHDFormatter();

      val = ReadAuxTVOutReg(TVOUT_SYNC_SEL) & ~TVOUT_AUX_SYNC_HDF_MASK;
      val |= TVOUT_AUX_SYNC_HDF_VTG2_SYNC1;
      WriteAuxTVOutReg(TVOUT_SYNC_SEL,val);

      ULONG cfgana = ReadTVFmtReg(TVFMT_ANA_CFG) & ~(ANA_CFG_INPUT_MASK |
                                                     ANA_CFG_CLIP_EN);
      cfgana |= ANA_CFG_INPUT_AUX;
      WriteTVFmtReg(TVFMT_ANA_CFG, cfgana);

      WriteTVFmtReg(TVFMT_ANA_Y_SCALE, 0x400);
      WriteTVFmtReg(TVFMT_ANA_C_SCALE, 0x400);
      if(m_bHasSeparateCbCrRescale)
        WriteTVFmtReg(TVFMT_ANA_CR_SCALE, 0x400);

      /*
       * Set up the HD formatter SRC configuration
       */
      m_pHDFOutput->SetUpsampler(4, format);

      val  = ReadMainTVOutReg(TVOUT_SYNC_SEL) & ~TVOUT_MAIN_SYNC_DCSED_MASK;
      val |= TVOUT_MAIN_SYNC_DCSED_VTG2_SYNC3;
      WriteMainTVOutReg(TVOUT_SYNC_SEL, val);
    }
  }

  return true;
}


