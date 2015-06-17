/***********************************************************************
 *
 * File: HDTVOutFormatter/stmhdfoutput.cpp
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

#include <STMCommon/stmhdmi.h>
#include <STMCommon/stmfsynth.h>
#include <STMCommon/stmsdvtg.h>
#include <STMCommon/stmdenc.h>

#include "stmhdfawg.h"
#include "stmhdfoutput.h"
#include "stmtvoutreg.h"


#define VTG_AWG_SYNC_ID  1
#define VTG_HDMI_SYNC_ID 2
#define VTG_PADS_SYNC_ID 3


#define HDMI_DELAY          (0)
#define PADS_DELAY          (6) // TODO: Find out this value.
#define MAIN_TO_DENC_DELAY  (-9)

/*
 * Default Analogue HD formatter SRC coefficients.
 */
static stm_display_hdf_filter_setup_t default_2x_filter = {
  STM_FLT_DIV_256,
  {
    0x00FE83FB,
    0x1F900401,
    0x0113,
    0x00000000,
    0x00000000,
    0x00F408F9,
    0x055F7C25,
    0x00000000,
    0x00000000
  }
};


static stm_display_hdf_filter_setup_t default_alt_2x_luma_filter = {
  STM_FLT_DIV_512,
  {
    0x00FD7BFD,
    0x03A88613,
    0x08b,
    0x00000000,
    0x00000000,
    0x0001FBFA,
    0x0428BC29,
    0x00000000,
    0x00000000
  }
};


static stm_display_hdf_filter_setup_t default_4x_filter = {
  STM_FLT_DIV_512,
  {
    0x00fc827f,
    0x008fe20b,
    0x01ed,
    0x00f684fc,
    0x050f7c24,
    0x00f4857c,
    0x0a1f402e,
    0x00fa027f,
    0x0e076e1d
  }
};


//////////////////////////////////////////////////////////////////////////////
//
// HD, SD Progressive and SD Interlaced Output on main HD Dacs
//
CSTmHDFormatterOutput::CSTmHDFormatterOutput(
    CDisplayDevice     *pDev,
    ULONG               ulMainTVOut,
    ULONG               ulAuxTVOut,
    ULONG               ulTVFmt,
    CSTmSDVTG          *pVTG1,
    CSTmSDVTG          *pVTG2,
    CSTmDENC           *pDENC,
    CDisplayMixer      *pMixer,
    CSTmFSynth         *pFSynth,
    CSTmHDFormatterAWG *pAWG): CSTmMasterOutput(pDev, pDENC, pVTG1, pMixer, pFSynth)
{
  DENTRY();

  m_pVTG2      = pVTG2;
  m_pAWGAnalog = pAWG;
  m_pDVO       = 0;
  m_pHDMI      = 0;

  m_ulMainTVOut = ulMainTVOut;
  m_ulAuxTVOut  = ulAuxTVOut;
  m_ulTVFmt     = ulTVFmt;

  DEBUGF2(2,("CSTmHDFormatterOutput::CSTmHDFormatterOutput - m_ulMainTVOut = 0x%lx\n", m_ulMainTVOut));
  DEBUGF2(2,("CSTmHDFormatterOutput::CSTmHDFormatterOutput - m_ulAuxTVOut  = 0x%lx\n", m_ulAuxTVOut));
  DEBUGF2(2,("CSTmHDFormatterOutput::CSTmHDFormatterOutput - m_ulTVFmt     = 0x%lx\n", m_ulTVFmt));

  m_ulAWGLock   = g_pIOS->CreateResourceLock();

  m_bHasSeparateCbCrRescale = true;
  m_bDacHdPowerDisabled = false;

  /*
   * Recommended STi7111 design is for a 1.4v max output voltage
   * from the video DACs. By default we set the saturation point
   * to the full range of the 10bit DAC. See RecalculateDACSetup for
   * more details.
   */
  m_maxDACVoltage  = 1400; // in mV
  m_DACSaturation  = 1023;
  RecalculateDACSetup();

  DEBUGF2(2,("CSTmHDFormatterOutput::CSTmHDFormatterOutput - m_pVTG   = %p\n", m_pVTG));
  DEBUGF2(2,("CSTmHDFormatterOutput::CSTmHDFormatterOutput - m_pVTG2  = %p\n", m_pVTG2));
  DEBUGF2(2,("CSTmHDFormatterOutput::CSTmHDFormatterOutput - m_pDENC  = %p\n", m_pDENC));
  DEBUGF2(2,("CSTmHDFormatterOutput::CSTmHDFormatterOutput - m_pMixer = %p\n", m_pMixer));

  /* Select Main compositor input and output defaults */
  m_ulInputSource = STM_AV_SOURCE_MAIN_INPUT;
  CSTmHDFormatterOutput::SetControl(STM_CTRL_VIDEO_OUT_SELECT, STM_VIDEO_OUT_YUV);

  /*
   * Default DAC routing and analogue output config
   */
  WriteTVFmtReg(TVFMT_ANA_CFG, (ANA_CFG_RCTRL_R << ANA_CFG_REORDER_RSHIFT) |
                               (ANA_CFG_RCTRL_G << ANA_CFG_REORDER_GSHIFT) |
                               (ANA_CFG_RCTRL_B << ANA_CFG_REORDER_BSHIFT));

  /*
   * Default pad configuration.
   */
  WriteMainTVOutReg(TVOUT_PADS_CTL, TVOUT_MAIN_PADS_HSYNC_SYNC3 |
                                    TVOUT_PADS_VSYNC_SYNC3      |
                                    TVOUT_MAIN_PADS_DAC_POFF);

  /*
   * Default sync selection to subblocks, note that the HD formatter syncs
   * are configured in the Aux TVOUT block.
   */
  WriteMainTVOutReg(TVOUT_SYNC_SEL, TVOUT_MAIN_SYNC_FDVO_REF    |
                                    TVOUT_MAIN_SYNC_HDMI_SYNC2  |
                                    TVOUT_MAIN_SYNC_DCSED_VTG1_SYNC1_PROGRESSIVE);


  /*
   * The syncs to the HDMI and HDFormatter blocks must be positive regardless
   * of the display standard.
   */
  m_pVTG->SetSyncType(VTG_AWG_SYNC_ID,  STVTG_SYNC_POSITIVE);
  m_pVTG->SetSyncType(VTG_HDMI_SYNC_ID, STVTG_SYNC_POSITIVE);
  m_pVTG->SetSyncType(VTG_PADS_SYNC_ID, STVTG_SYNC_TIMING_MODE);

  m_pVTG->SetHSyncOffset(VTG_HDMI_SYNC_ID, HDMI_DELAY);
  m_pVTG->SetVSyncHOffset(VTG_HDMI_SYNC_ID, HDMI_DELAY);
  m_pVTG->SetHSyncOffset(VTG_PADS_SYNC_ID, PADS_DELAY);
  m_pVTG->SetVSyncHOffset(VTG_PADS_SYNC_ID, PADS_DELAY);

  m_pAWGAnalog->CacheFirmwares(this);

  m_bUseAlternate2XFilter = false;

  m_filters[HDF_COEFF_2X_LUMA]       = default_2x_filter;
  m_filters[HDF_COEFF_2X_CHROMA]     = default_2x_filter;
  m_filters[HDF_COEFF_2X_ALT_LUMA]   = default_alt_2x_luma_filter;
  m_filters[HDF_COEFF_2X_ALT_CHROMA] = default_2x_filter;
  m_filters[HDF_COEFF_4X_LUMA]       = default_4x_filter;
  m_filters[HDF_COEFF_4X_CHROMA]     = default_4x_filter;

  DEXIT();
}


CSTmHDFormatterOutput::~CSTmHDFormatterOutput() {}


void CSTmHDFormatterOutput::SetSlaveOutputs(COutput *dvo, COutput *hdmi)
{
  m_pDVO  = dvo;
  m_pHDMI = hdmi;
}


const stm_mode_line_t* CSTmHDFormatterOutput::SupportedMode(const stm_mode_line_t *mode) const
{
  if(mode->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK)
  {
    DEBUGF2(3,("CSTmHDFormatterOutput::SupportedMode Looking for valid HD mode, pixclock = %lu\n",mode->TimingParams.ulPixelClock));

    if(mode->ModeParams.OutputStandards & STM_OUTPUT_STD_AS4933)
    {
      DEBUGF2(3,("CSTmHDFormatterOutput::SupportedMode No support for AS4933 yet\n"));
      return 0;
    }

    return mode;
  }
  else
  {
    /*
     * We support SD interlaced modes based on a 13.5Mhz pixel clock
     * and progressive modes at 27Mhz or 27.027Mhz. We also report support
     * for SD interlaced modes over HDMI where the clock is doubled and
     * pixels are repeated. Finally we support VGA (640x480p@59.94Hz and 60Hz)
     * for digital displays (25.18MHz pixclock)
     */
    if(mode->ModeParams.OutputStandards & (STM_OUTPUT_STD_SMPTE293M | STM_OUTPUT_STD_VESA))
    {
      DEBUGF2(3,("CSTmHDFormatterOutput::SupportedMode Looking for valid SD progressive mode\n"));
      if(mode->TimingParams.ulPixelClock == 27000000 ||
         mode->TimingParams.ulPixelClock == 27027000 ||
         mode->TimingParams.ulPixelClock == 25174800 ||
         mode->TimingParams.ulPixelClock == 25200000)
        return mode;
    }
    else if(mode->ModeParams.OutputStandards & (STM_OUTPUT_STD_TMDS_PIXELREP_2X|STM_OUTPUT_STD_TMDS_PIXELREP_4X))
    {
      /*
       * Support all pixel double/quad modes SD and ED for Dolby TrueHD audio
       */
      DEBUGF2(3,("CSTmHDFormatterOutput::SupportedMode Looking for valid SD/ED HDMI mode\n"));
      return mode;
    }
    else
    {
      DEBUGF2(3,("CSTmHDFormatterOutput::SupportedMode Looking for valid SD interlaced mode\n"));
      if(mode->TimingParams.ulPixelClock == 13500000 ||
         mode->TimingParams.ulPixelClock == 13513500)
        return mode;
    }
  }

  return 0;
}


ULONG CSTmHDFormatterOutput::GetCapabilities(void) const
{
  ULONG caps =  (STM_OUTPUT_CAPS_SD_ANALOGUE       |
                 STM_OUTPUT_CAPS_ED_ANALOGUE       |
                 STM_OUTPUT_CAPS_HD_ANALOGUE       |
                 STM_OUTPUT_CAPS_PLANE_MIXER       |
                 STM_OUTPUT_CAPS_MODE_CHANGE       |
                 STM_OUTPUT_CAPS_CVBS_YC_EXCLUSIVE |
                 STM_OUTPUT_CAPS_YPrPb_EXCLUSIVE   |
                 STM_OUTPUT_CAPS_RGB_EXCLUSIVE     |
                 STM_OUTPUT_CAPS_SD_RGB_CVBS_YC    |
                 STM_OUTPUT_CAPS_SD_YPrPb_CVBS_YC);

  return caps;
}


bool CSTmHDFormatterOutput::SetOutputFormat(ULONG format)
{
  DEBUGF2(2,("CSTmHDFormatterOutput::SetOutputFormat - format = 0x%lx\n",format));

  /*
   * Don't change the VOS setup unless the display is running.
   */
  if(!m_pCurrentMode)
  {
    DEBUGF2(2,("CSTmHDFormatterOutput::SetOutputFormat - No display mode, nothing to do\n"));
    return true;
  }

  if(m_bUsingDENC)
  {
    DEBUGF2(2,("CSTmHDFormatterOutput::SetOutputFormat - configuring DENC\n"));
    /*
     * Defer this to the DENC for interlaced modes on the HD DACs.
     */
    if(!m_pDENC->SetMainOutputFormat(format))
    {
      DEBUGF2(2,("CSTmHDFormatterOutput::SetOutputFormat - format unsupported on DENC\n"));
      return false;
    }

    ULONG cfgana = ReadTVFmtReg(TVFMT_ANA_CFG) & ~(ANA_CFG_INPUT_MASK|ANA_CFG_PRPB_SYNC_OFFSET_MASK);
    cfgana |= ANA_CFG_INPUT_AUX;
    WriteTVFmtReg(TVFMT_ANA_CFG, cfgana);
    WriteTVFmtReg(TVFMT_ANA_Y_SCALE, 0x400);
    WriteTVFmtReg(TVFMT_ANA_C_SCALE, 0x400);
    if(m_bHasSeparateCbCrRescale)
      WriteTVFmtReg(TVFMT_ANA_CR_SCALE, 0x400);

    SetUpsampler(4,format);

    return true;
  }

  DEBUGF2(2,("CSTmHDFormatterOutput::SetOutputFormat - configuring HDFormatter\n"));

  /*
   * If we have analogue HD output configured we switch the
   * PIX HD clock to the main clock source and the HD formatter syncs to VTG1.
   * Otherwise we leave it alone in case the aux pipeline is already using
   * the HD formatter for SCART output.
   */
  if((format & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV)) == 0)
    return true;

  SetMainClockToHDFormatter();

  ULONG val = ReadAuxTVOutReg(TVOUT_SYNC_SEL) & ~TVOUT_AUX_SYNC_HDF_MASK;
  if(m_pCurrentMode->ModeParams.ScanType == SCAN_I)
    val |= TVOUT_AUX_SYNC_HDF_VTG1_SYNC1_INTERLACED;
  else
    val |= TVOUT_AUX_SYNC_HDF_VTG1_SYNC1_PROGRESSIVE;

  WriteAuxTVOutReg(TVOUT_SYNC_SEL,val);

  val  = ReadMainTVOutReg(TVOUT_SYNC_SEL) & ~TVOUT_MAIN_SYNC_DCSED_MASK;
  if(m_pCurrentMode->ModeParams.ScanType == SCAN_I)
    val |= TVOUT_MAIN_SYNC_DCSED_VTG1_SYNC1_INTERLACED;
  else
    val |= TVOUT_MAIN_SYNC_DCSED_VTG1_SYNC1_PROGRESSIVE;

  WriteMainTVOutReg(TVOUT_SYNC_SEL, val);

  /*
   * This bit shouldn't effect the main pipeline's analogue output, but it
   * does on 7200cut2 and it must be set for HD modes :-( The 7111 doesn't
   * seem to care either way.
   */
  val = ReadAuxTVOutReg(TVOUT_CLK_SEL) | TVOUT_AUX_CLK_SEL_SD_N_HD;
  WriteAuxTVOutReg(TVOUT_CLK_SEL, val);

  if(m_ulTVStandard & STM_OUTPUT_STD_HD_MASK)
  {
    if(m_pCurrentMode->TimingParams.ulPixelClock != 148500000 &&
       m_pCurrentMode->TimingParams.ulPixelClock != 148351648)
      SetUpsampler(2,format);
    else
      SetUpsampler(1,format);
  }
  else
    SetUpsampler(4,format);

  /*
   * Make sure syncs (if programmed) are enabled.
   */
  m_pAWGAnalog->Enable();

  ULONG cfgana = ReadTVFmtReg(TVFMT_ANA_CFG) & ~(ANA_CFG_INPUT_MASK |
                                                 ANA_CFG_PRPB_SYNC_OFFSET_MASK);

  /*
   * Select the colour format for use with progressive/HD modes. We also
   * need to change the Chroma/Red/Blue DAC level scaling. Note we only
   * support sync on Luma/Green for consumer applications at the moment.
   */
  switch(format & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV))
  {
    case STM_VIDEO_OUT_RGB:
    {
      cfgana |= ANA_CFG_INPUT_RGB | ANA_CFG_CLIP_GB_N_CRCB;

      if(m_ulTVStandard & STM_OUTPUT_STD_VESA)
      {
        /*
         * For VESA modes use the full RGB range mapped to 0-0.7v
         */
        DEBUGF2(2,("CSTmLocalHDFormatterOutput::SetOutputFormat VESA\n"));
        WriteTVFmtReg(TVFMT_ANA_Y_SCALE, m_DAC_RGB_Scale);
        WriteTVFmtReg(TVFMT_ANA_C_SCALE, m_DAC_RGB_Scale);
        if(m_bHasSeparateCbCrRescale)
          WriteTVFmtReg(TVFMT_ANA_CR_SCALE, m_DAC_RGB_Scale);
      }
      else
      {
       /*
        * For TV modes using component RGB use the same scaling and offset
        * as Luma (Y) for all three signals.
        */
        DEBUGF2(2,("CSTmLocalHDFormatterOutput::SetOutputFormat RGB\n"));
        WriteTVFmtReg(TVFMT_ANA_Y_SCALE, m_DAC_Y_Scale | (m_DAC_Y_Offset<<ANA_SCALE_OFFSET_SHIFT));
        WriteTVFmtReg(TVFMT_ANA_C_SCALE, m_DAC_Y_Scale | (m_DAC_Y_Offset<<ANA_SCALE_OFFSET_SHIFT));
        if(m_bHasSeparateCbCrRescale)
          WriteTVFmtReg(TVFMT_ANA_CR_SCALE, m_DAC_Y_Scale | (m_DAC_Y_Offset<<ANA_SCALE_OFFSET_SHIFT));
      }
      break;
    }
    case STM_VIDEO_OUT_YUV:
    {
      cfgana |= ANA_CFG_INPUT_YCBCR;
      cfgana &= ~ANA_CFG_CLIP_GB_N_CRCB;

      if(m_ulTVStandard & STM_OUTPUT_STD_VESA)
      {
        /*
         * VESA modes not defined in YUV so blank the signal at 0V
         */
        DEBUGF2(2,("CSTmLocalHDFormatterOutput::SetOutputFormat Blank VESA in YUV\n"));
        WriteTVFmtReg(TVFMT_ANA_Y_SCALE, 0);
        WriteTVFmtReg(TVFMT_ANA_C_SCALE, 0);
        if(m_bHasSeparateCbCrRescale)
          WriteTVFmtReg(TVFMT_ANA_CR_SCALE, 0);
      }
      else
      {
        DEBUGF2(2,("CSTmLocalHDFormatterOutput::SetOutputFormat YUV\n"));
        WriteTVFmtReg(TVFMT_ANA_Y_SCALE, m_DAC_Y_Scale | (m_DAC_Y_Offset<<ANA_SCALE_OFFSET_SHIFT));
        WriteTVFmtReg(TVFMT_ANA_C_SCALE, m_DAC_C_Scale | (m_DAC_C_Offset<<ANA_SCALE_OFFSET_SHIFT));
        if(m_bHasSeparateCbCrRescale)
          WriteTVFmtReg(TVFMT_ANA_CR_SCALE, m_DAC_C_Scale | (m_DAC_C_Offset<<ANA_SCALE_OFFSET_SHIFT));

        cfgana |= (m_AWG_Y_C_Offset  << ANA_CFG_PRPB_SYNC_OFFSET_SHIFT) & ANA_CFG_PRPB_SYNC_OFFSET_MASK;
      }

      break;
    }
    default:
    {
      DEBUGF2(1,("%s: Invalid Output Format RGB+YUV\n",__PRETTY_FUNCTION__));
      break;
    }
  }

  DEBUGF2(2,("CSTmLocalHDFormatterOutput::SetOutputFormat TVFMT_YSCALE\t\t%#.8lx\n", ReadTVFmtReg(TVFMT_ANA_Y_SCALE)));
  DEBUGF2(2,("CSTmLocalHDFormatterOutput::SetOutputFormat TVFMT_CSCALE\t\t%#.8lx\n", ReadTVFmtReg(TVFMT_ANA_C_SCALE)));

  if(m_signalRange == STM_SIGNAL_VIDEO_RANGE)
    cfgana |= ANA_CFG_CLIP_EN;
  else
    cfgana &= ~ANA_CFG_CLIP_EN;

  WriteTVFmtReg(TVFMT_ANA_CFG, cfgana);

  DEBUGF2(2,("CSTmLocalHDFormatterOutput::SetOutputFormat TVFMT_ANA_CFG\t\t%#.8lx\n", ReadTVFmtReg(TVFMT_ANA_CFG)));

  DEXIT();
  return true;
}


void CSTmHDFormatterOutput::SetControl(stm_output_control_t ctrl, ULONG val)
{
  DEBUGF2(2,("CSTmHDFormatterOutput::SetControl ctrl = %u val = %lu\n",(unsigned)ctrl,val));
  switch(ctrl)
  {
    case STM_CTRL_SIGNAL_RANGE:
    {
      CSTmMasterOutput::SetControl(ctrl,val);
      /*
       * Enable the change in the HD formatter
       */
      SetOutputFormat(m_ulOutputFormat);
      break;
    }
    case STM_CTRL_DAC_HD_POWER:
    {
      m_bDacHdPowerDisabled = (val != 0);
      if (m_pCurrentMode)
      {
        if(!m_bDacHdPowerDisabled)
          EnableDACs();
        else
        {
          val = ReadMainTVOutReg(TVOUT_PADS_CTL) | TVOUT_MAIN_PADS_DAC_POFF;
          WriteMainTVOutReg(TVOUT_PADS_CTL, val);
        }
      }
      break;
    }
    case STM_CTRL_DAC_HD_FILTER:
    {
      m_bUseAlternate2XFilter = (val != 0);
      if(m_pCurrentMode && (m_ulTVStandard & STM_OUTPUT_STD_HD_MASK))
      {
        if(m_pCurrentMode->TimingParams.ulPixelClock <= 74250000)
          SetUpsampler(2, m_ulOutputFormat);
      }
      break;
    }
    case STM_CTRL_YCBCR_COLORSPACE:
    {
      if(m_pHDMI)
        m_pHDMI->SetControl(ctrl,val);
      // Fallthrough to base class in order to configure the mixer
    }
    default:
      CSTmMasterOutput::SetControl(ctrl,val);
      break;
  }
}


ULONG CSTmHDFormatterOutput::GetControl(stm_output_control_t ctrl) const
{
  switch(ctrl)
  {
    case STM_CTRL_DAC_HD_POWER:
      return m_bDacHdPowerDisabled?1:0;
    case STM_CTRL_DAC_HD_FILTER:
      return m_bUseAlternate2XFilter?1:0;
    default:
      return CSTmMasterOutput::GetControl(ctrl);
  }

  return 0;
}


bool CSTmHDFormatterOutput::SetFilterCoefficients(const stm_display_filter_setup_t *f)
{
  bool ret = true;
  DENTRY();
  switch(f->type)
  {
    case HDF_COEFF_2X_LUMA ... HDF_COEFF_4X_CHROMA:
    {
      if(f->hdf.div < STM_FLT_DIV_4096)
      {
        DEBUGF2(2,("%s: Setting filter %d\n",__PRETTY_FUNCTION__,f->type));
        m_filters[f->type] = f->hdf;
        ret = true;
      }
      else
      {
        DEBUGF2(1,("%s: Unsupported filter divide %d\n",__PRETTY_FUNCTION__,f->hdf.div));
        ret = false;
      }

      break;
    }
    default:
      ret = CSTmMasterOutput::SetFilterCoefficients(f);
      break;
  }

  DEXIT();
  return ret;
}


bool CSTmHDFormatterOutput::Start(const stm_mode_line_t *mode, ULONG tvStandard)
{
  DEBUGF2(2,("CSTmHDFormatterOutput::Start - in\n"));

  /*
   * Strip any secondary mode flags out when SMPTE293M is defined.
   * Other chips need additional modes for simultaneous re-interlaced DENC
   * output. Stripping that out lets us use tvStandard more sensibly throughout,
   * making the code more readable.
   */
  if(tvStandard & STM_OUTPUT_STD_SMPTE293M)
    tvStandard = STM_OUTPUT_STD_SMPTE293M;

  if((mode->ModeParams.OutputStandards & tvStandard) != tvStandard)
  {
    DEBUGF2(1,("CSTmHDFormatterOutput::Start - requested standard not supported by mode\n"));
    return false;
  }

  if(m_bIsSuspended)
  {
    DEBUGF2(1,("CSTmHDFormatterOutput::Start output is suspended\n"));
    return false;
  }

  /*
   * First try to change the display mode on the fly, if that works there is
   * nothing else to do.
   */
  if(TryModeChange(mode, tvStandard))
  {
    DEBUGF2(2,("CSTmHDFormatterOutput::Start - mode change successfull\n"));
    return true;
  }

  if(m_pCurrentMode)
  {
    DEBUGF2(1,("CSTmHDFormatterOutput::Start - failed, output is active\n"));
    return false;
  }

  /*
   * Set the new mixer viewport registers before we start the VTG.
   */
  if(!m_pMixer->Start(mode))
  {
    DEBUGF2(1,("CSTmHDFormatterOutput::Start Mixer start failed\n"));
    return false;
  }

  if(tvStandard & STM_OUTPUT_STD_HD_MASK)
  {
    if(!StartHDDisplay(mode))
      goto stop_and_exit;
  }
  else if(tvStandard & STM_OUTPUT_STD_SD_MASK)
  {
    if(!StartSDInterlacedDisplay(mode, tvStandard))
      goto stop_and_exit;
  }
  else if(tvStandard & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_VESA))
  {
    /*
     * Note that this path also deals with VESA (VGA) modes.
     */
    if(!StartSDProgressiveDisplay(mode, tvStandard))
      goto stop_and_exit;
  }
  else
  {
    DEBUGF2(1,("CSTmHDFormatterOutput::Start Unsupported Output Standard\n"));
    goto stop_and_exit;
  }

  /*
   * We don't want anything from CSTmMasterOutput::Start, but we do
   * need to call the base class Start.
   */
  COutput::Start(mode, tvStandard);

  SetOutputFormat(m_ulOutputFormat);
  EnableDACs();

  DEXIT();
  return true;

stop_and_exit:
  /*
   * Cleanup any partially successful hardware setup.
   */
  this->Stop();
  DEXIT();
  return false;
}


bool CSTmHDFormatterOutput::StartSDInterlacedDisplay(const stm_mode_line_t *mode, ULONG tvStandard)
{
  ULONG val;

  DENTRY();

  /*
   * Just make sure someone hasn't specified an SD progressive mode
   * without specifying OUTPUT_CAPS_SD_SMPTE293M
   */
  if(mode->ModeParams.ScanType != SCAN_I)
    return false;

  if(m_pVTG2->GetCurrentMode() != 0)
  {
    DEBUGF2(1,("CSTmLocalHDFormatterOutput::StartSDInterlacedDisplay - cannot set interlaced SD mode, SD path in use\n"));
    return false;
  }

  StartSDInterlacedClocks(mode);

  WriteMainTVOutReg(TVOUT_CLK_SEL, (TVOUT_MAIN_CLK_SEL_PIX(TVOUT_CLK_DIV_8)   |
                                    TVOUT_MAIN_CLK_SEL_TMDS(TVOUT_CLK_DIV_4)  |
                                    TVOUT_MAIN_CLK_SEL_FDVO(TVOUT_CLK_DIV_4)));

  /*
   * Change the HD formatter syncs to the slaved VTG2 outputs.
   */
  val = ReadAuxTVOutReg(TVOUT_SYNC_SEL) & ~TVOUT_AUX_SYNC_HDF_MASK;
  val |= TVOUT_AUX_SYNC_HDF_VTG2_SYNC1;
  WriteAuxTVOutReg(TVOUT_SYNC_SEL,val);

  val = ReadMainTVOutReg(TVOUT_SYNC_SEL) & ~TVOUT_MAIN_SYNC_DCSED_MASK;
  val |= TVOUT_MAIN_SYNC_DCSED_VTG1_SYNC1_INTERLACED;
  WriteMainTVOutReg(TVOUT_SYNC_SEL, val);

  m_pFSynth->SetDivider(8);

  /*
   * Set Aux TVOut divides for 108MHz from FS0, not 27MHz from FS1
   */
  WriteAuxTVOutReg(TVOUT_CLK_SEL, (TVOUT_AUX_CLK_SEL_PIX1X(TVOUT_CLK_DIV_8) | /* 13.5MHz */
                                   TVOUT_AUX_CLK_SEL_DENC(TVOUT_CLK_DIV_4)));  /* 27MHz */

  val = ReadTVFmtReg(TVFMT_ANA_CFG) & ~(ANA_CFG_INPUT_MASK           |
                                        ANA_CFG_SYNC_FILTER_DEL_MASK |
                                        ANA_CFG_SYNC_EN              |
                                        ANA_CFG_CLIP_EN              |
                                        ANA_CFG_PREFILTER_EN         |
                                        ANA_CFG_SYNC_ON_PRPB);

  val |= ANA_CFG_SEL_MAIN_TO_DENC;

  WriteTVFmtReg(TVFMT_ANA_CFG, val);

  /*
   * We are going to reuse the AWG syncs here to generate the signals that will
   * drive VTG2 and then the DENC. The auxiliary output must be configured to
   * receive the correct sync set.
   */
  m_pVTG->SetHSyncOffset(VTG_AWG_SYNC_ID, 0);
  m_pVTG->SetVSyncHOffset(VTG_AWG_SYNC_ID,0);

  /*
   * Setup sync signals sent to VTG2 for it to lock to.
   *
   * Note: using TopNotBot produces a much more reliable startup of
   * the slave VTG than BotNotTop, possibly because you get an immediate
   * rising edge as soon as VTG1 starts.
   */
  m_pVTG->SetSyncType(VTG_AWG_SYNC_ID, STVTG_SYNC_TOP_NOT_BOT);
  m_pVTG2->SetSyncType(0, STVTG_SYNC_TOP_NOT_BOT);

  m_pVTG2->SetHSyncOffset(1, MAIN_TO_DENC_DELAY);
  m_pVTG2->SetVSyncHOffset(1, MAIN_TO_DENC_DELAY);

  /*
   * Mark the DENC as in use now as it allows Stop() to clean things
   * up if one of the subsequent calls fails.
   */
  m_bUsingDENC = true;

  /*
   *  Start VTG2 in slave mode, this should leave it waiting for the
   *  first input sync before it starts counting.
   */
  if(!m_pVTG2->Start(mode,0))
  {
    DEBUGF2(1,("CSTmHDFormatterOutput::StartSDInterlacedDisplay VTG2 start failed\n"));
    return false;
  }

  /*
   * If we start the DENC after VTG1 then the vertical sync is not
   * reliable. However we are still not sure if this is VTG2 not producing
   * the sync or the DENC getting confused. But this is the more natural
   * startup order anyway.
   */
  if(!m_pDENC->Start(this, mode, tvStandard))
  {
    DEBUGF2(1,("CSTmHDFormatterOutput::StartSDInterlacedDisplay DENC start failed\n"));
    return false;
  }

  /*
   * Now everything else is set, start up the master sync generator.
   */
  if(!m_pVTG->Start(mode))
  {
    DEBUGF2(1,("CSTmHDFormatterOutput::StartSDInterlacedDisplay VTG start failed\n"));
    return false;
  }

  /*
   * Now we have ownership of the DENC, set this output's PSI control setup.
   */
  ProgramPSIControls();

  m_pDENC->SetControl(STM_CTRL_SIGNAL_RANGE, (ULONG)m_signalRange);

  return true;
}


bool CSTmHDFormatterOutput::StartSDProgressiveDisplay(const stm_mode_line_t *mode, ULONG tvStandard)
{
  ULONG val;

  DEBUGF2(2,("CSTmHDFormatterOutput::StartSDProgressiveDisplay\n"));

  StartSDProgressiveClocks(mode);

  WriteMainTVOutReg(TVOUT_CLK_SEL, (TVOUT_MAIN_CLK_SEL_PIX(TVOUT_CLK_DIV_4)   |
                                    TVOUT_MAIN_CLK_SEL_TMDS(TVOUT_CLK_DIV_4)  |
                                    TVOUT_MAIN_CLK_SEL_FDVO(TVOUT_CLK_DIV_4)));


  m_pFSynth->SetDivider(4);

  val = ReadTVFmtReg(TVFMT_ANA_CFG) & ~(ANA_CFG_SEL_MAIN_TO_DENC     |
                                        ANA_CFG_SYNC_FILTER_DEL_MASK |
                                        ANA_CFG_SYNC_EN              |
                                        ANA_CFG_SYNC_HREF_IGNORED    |
                                        ANA_CFG_SYNC_FIELD_N_FRAME   |
                                        ANA_CFG_PREFILTER_EN         |
                                        ANA_CFG_SYNC_ON_PRPB);

  WriteTVFmtReg(TVFMT_ANA_CFG, val);

  /*
   * Enable sync generation for ED modes but not VESA (VGA) modes.
   * Try and start a sync program for the mode. If one doesn't exist then
   * this isn't a failure because we may only support the mode on slaved
   * digital outputs (e.g. HDMI).
   */
  if((tvStandard & STM_OUTPUT_STD_ED_MASK) != 0)
    m_pAWGAnalog->Start(mode);

  m_pVTG->SetHSyncOffset(VTG_AWG_SYNC_ID, AWG_DELAY_ED);
  m_pVTG->SetVSyncHOffset(VTG_AWG_SYNC_ID, AWG_DELAY_ED);
  m_pVTG->SetSyncType(VTG_AWG_SYNC_ID, STVTG_SYNC_POSITIVE);

  if(!m_pVTG->Start(mode))
  {
    DEBUGF2(1,("CSTmHDFormatterOutput::StartSDProgressiveDisplay VTG start failed\n"));
    return false;
  }

  return true;
}


bool CSTmHDFormatterOutput::StartHDDisplay(const stm_mode_line_t *mode)
{
  ULONG val = 0;
  DEBUGF2(2,("CSTmHDFormatterOutput::StartHDdisplay\n"));

  StartHDClocks(mode);

  if(mode->TimingParams.ulPixelClock == 148500000 ||
     mode->TimingParams.ulPixelClock == 148351648)
  {
    /*
     * 1080p 50/60Hz
     */
    m_pFSynth->SetDivider(1);

    WriteMainTVOutReg(TVOUT_CLK_SEL, (TVOUT_MAIN_CLK_SEL_PIX(TVOUT_CLK_BYPASS)   |
                                      TVOUT_MAIN_CLK_SEL_TMDS(TVOUT_CLK_BYPASS)  |
                                      TVOUT_MAIN_CLK_SEL_FDVO(TVOUT_CLK_BYPASS)));
  }
  else
  {
    /*
     * 1080p 25/30Hz, 1080i, 720p
     */
    m_pFSynth->SetDivider(2);

    WriteMainTVOutReg(TVOUT_CLK_SEL, (TVOUT_MAIN_CLK_SEL_PIX(TVOUT_CLK_DIV_2)   |
                                      TVOUT_MAIN_CLK_SEL_TMDS(TVOUT_CLK_DIV_2)  |
                                      TVOUT_MAIN_CLK_SEL_FDVO(TVOUT_CLK_DIV_2)));

  }

  val = ReadTVFmtReg(TVFMT_ANA_CFG) & ~(ANA_CFG_SEL_MAIN_TO_DENC     |
                                        ANA_CFG_SYNC_FILTER_DEL_MASK |
                                        ANA_CFG_SYNC_EN              |
                                        ANA_CFG_SYNC_HREF_IGNORED    |
                                        ANA_CFG_SYNC_FIELD_N_FRAME   |
                                        ANA_CFG_PREFILTER_EN         |
                                        ANA_CFG_SYNC_ON_PRPB);

  WriteTVFmtReg(TVFMT_ANA_CFG, val);

  /*
   * Try and start a sync program for the mode. If one doesn't exist then
   * this isn't a failure because we may only support the mode on slaved
   * digital outputs (e.g. HDMI).
   */
  m_pAWGAnalog->Start(mode);

  m_pVTG->SetHSyncOffset(VTG_AWG_SYNC_ID, AWG_DELAY_HD);
  /*
   * For 1080i we cannot move the VSync backwards, that would put the top field
   * vsync active edge in the bottom field (as the VTG sees it). This doesn't
   * work because the AWG is set up in frame mode for this and only sees the
   * active edge when the VTG reference signal bot_not_top indicates a top field.
   *
   * TODO: Investigate if we can use the progressive version of sync1 instead
   * and only program the top field waveform, does this mode force BnT to Top?
   * As this goes to the whole HDFormatter, not just the AWG would this have
   * other implications? If we could do this we wouldn't need to modify the
   * AWG firmware to know about the delay at the beginning of the first line.
   */
  if(mode->ModeParams.ScanType == SCAN_I)
    m_pVTG->SetVSyncHOffset(VTG_AWG_SYNC_ID, 0);
  else
    m_pVTG->SetVSyncHOffset(VTG_AWG_SYNC_ID, AWG_DELAY_HD);

  m_pVTG->SetSyncType(VTG_AWG_SYNC_ID, STVTG_SYNC_POSITIVE);


  if(!m_pVTG->Start(mode))
  {
    DEBUGF2(1,("CSTmHDFormatterOutput::StartHDdisplay VTG start failed\n"));
    return false;
  }

  return true;
}


bool CSTmHDFormatterOutput::Stop(void)
{
  DENTRY();

  /*
   * Lock out interrupt processing while we test and shutdown the mixer, to
   * prevent a plane's vsync update processing enabling itself on
   * the mixer between the test and the Stop.
   */
  g_pIOS->LockResource(m_ulLock);

  ULONG planes = m_pMixer->GetActivePlanes();

  if((planes & ~(ULONG)OUTPUT_BKG) != 0)
  {
    DEBUGF2(1, ("CSTmHDFormatterOutput::Stop error - mixer has active planes\n"));
    g_pIOS->UnlockResource(m_ulLock);
    return false;
  }

  DisableDACs();

  // Stop slaved digital outputs.
  if(m_pHDMI)
    m_pHDMI->Stop();

  if(m_pDVO)
    m_pDVO->Stop();

  if(m_bUsingDENC)
  {
    // Stop SD Path when slaved to HD
    m_pDENC->Stop();
    m_pVTG2->Stop();
    m_bUsingDENC = false;
  }

  // Stop HD Path.
  m_pAWGAnalog->Stop();

  // Disable all planes on the mixer.
  m_pMixer->Stop();

  g_pIOS->UnlockResource(m_ulLock);

  if(m_pCurrentMode)
  {
    /*
     * If the output was properly running it is necessary to wait for vsyncs
     * in order to ensure the AWG has actually stopped. It also allows the
     * mixer enable register to latch 0 (all planes disabled) into the hardware.
     *
     * So we just busy wait here... but waiting for only one VSync is not always
     * enough, so we wait for 3 VSyncs :-(
     */
    for (int i = 0; i < 3; ++i)
    {
      TIME64 lastVSync = m_LastVSyncTime;
      while (lastVSync == m_LastVSyncTime)
        // would like to have some sort of cpu_relax() here
        ;
    }

    m_pVTG->Stop();
    COutput::Stop();
  }

  DEXIT();
  return true;
}


void CSTmHDFormatterOutput::Resume(void)
{
  DENTRY();

  if(!m_bIsSuspended)
    return;

  if(m_pCurrentMode)
  {
    /*
     * We need to go and make sure all the clock setup is correct again
     */
    if(m_ulTVStandard & STM_OUTPUT_STD_HD_MASK)
    {
      this->StartHDClocks(m_pCurrentMode);
    }
    else if(m_ulTVStandard & STM_OUTPUT_STD_SD_MASK)
    {
      this->StartSDInterlacedClocks(m_pCurrentMode);
      m_pDENC->Stop();
      /*
       * Reset slave VTG, should wait again for the first VTG1 sync.
       */
      m_pVTG2->ResetCounters();
      m_pDENC->Start(this,m_pCurrentMode,m_ulTVStandard);
    }
    else if(m_ulTVStandard & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_VESA))
    {
      this->StartSDProgressiveClocks(m_pCurrentMode);
    }

    if(!m_bUsingDENC && (m_ulOutputFormat & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV)))
      this->SetMainClockToHDFormatter();

  }

  CSTmMasterOutput::Resume();

  DEXIT();
}


void CSTmHDFormatterOutput::SetUpsampler(int multiple, ULONG format)
{
  ULONG val,srccfg;
  stm_filter_coeff_types_t luma,chroma;
  bool isRGB = (format & STM_VIDEO_OUT_RGB) != 0;

  switch(multiple)
  {
    case 2:
      DEBUGF2(2,("%s 2X\n",__PRETTY_FUNCTION__));
      if(!m_bUseAlternate2XFilter)
      {
        luma   = HDF_COEFF_2X_LUMA;
        chroma = isRGB?HDF_COEFF_2X_LUMA:HDF_COEFF_2X_CHROMA;
      }
      else
      {
        luma   = HDF_COEFF_2X_ALT_LUMA;
        chroma = isRGB?HDF_COEFF_2X_ALT_LUMA:HDF_COEFF_2X_ALT_CHROMA;
      }
      srccfg = ANA_SRC_CFG_2X;
      break;
    case 4:
      DEBUGF2(2,("%s 4X\n",__PRETTY_FUNCTION__));
      luma   = HDF_COEFF_4X_LUMA;
      chroma = isRGB?HDF_COEFF_4X_LUMA:HDF_COEFF_4X_CHROMA;
      srccfg = ANA_SRC_CFG_4X;
      break;
    case 1:
    default:
      DEBUGF2(2,("%s disabled/bypass\n",__PRETTY_FUNCTION__));
      WriteTVFmtReg(TVFMT_LUMA_SRC_CFG, (ANA_SRC_CFG_DISABLE|ANA_SRC_CFG_DIV_512|ANA_SRC_CFG_BYPASS));
      WriteTVFmtReg(TVFMT_CHROMA_SRC_CFG, ANA_SRC_CFG_DIV_512);
      return;
  }

  DEBUGF2(2,("%s: luma filter index = %d chroma filter index = %d\n",__PRETTY_FUNCTION__, luma, chroma));

  val = srccfg | (m_filters[luma].coeff[HDF_COEFF_PHASE1_7] << 16) |
                 (m_filters[luma].div << ANA_SRC_CFG_DIV_SHIFT);

  WriteTVFmtReg(TVFMT_LUMA_SRC_CFG, val);

  val = srccfg | (m_filters[chroma].coeff[HDF_COEFF_PHASE1_7] << 16) |
                 (m_filters[chroma].div << ANA_SRC_CFG_DIV_SHIFT);

  WriteTVFmtReg(TVFMT_CHROMA_SRC_CFG, val);

  DEBUGF2(3,("Luma P1: 0x%lx",m_filters[luma].coeff[HDF_COEFF_PHASE1_123]&0x7f));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE1_123]>>7)&0xff));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE1_123]>>15)&0x1ff));

  DEBUGF2(3,(" 0x%lx",m_filters[luma].coeff[HDF_COEFF_PHASE1_456]&0x1ff));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE1_456]>>9)&0x3ff));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE1_456]>>19)&0x3ff));

  DEBUGF2(3,(" 0x%lx\n",m_filters[luma].coeff[HDF_COEFF_PHASE1_7]&0x3ff));

  DEBUGF2(3,("Luma P2: 0x%lx",m_filters[luma].coeff[HDF_COEFF_PHASE2_123]&0x7f));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE2_123]>>7)&0xff));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE2_123]>>15)&0x1ff));

  DEBUGF2(3,(" 0x%lx",m_filters[luma].coeff[HDF_COEFF_PHASE2_456]&0x1ff));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE2_456]>>9)&0x3ff));
  DEBUGF2(3,(" 0x%lx\n",(m_filters[luma].coeff[HDF_COEFF_PHASE2_456]>>19)&0x3ff));

  DEBUGF2(3,("Luma P3: 0x%lx",m_filters[luma].coeff[HDF_COEFF_PHASE3_123]&0x7f));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE3_123]>>7)&0xff));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE3_123]>>15)&0x1ff));

  DEBUGF2(3,(" 0x%lx",m_filters[luma].coeff[HDF_COEFF_PHASE3_456]&0x1ff));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE3_456]>>9)&0x3ff));
  DEBUGF2(3,(" 0x%lx\n",(m_filters[luma].coeff[HDF_COEFF_PHASE3_456]>>19)&0x3ff));

  DEBUGF2(3,("Luma P4: 0x%lx",m_filters[luma].coeff[HDF_COEFF_PHASE4_123]&0x7f));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE4_123]>>7)&0xff));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE4_123]>>15)&0x1ff));

  DEBUGF2(3,(" 0x%lx",m_filters[luma].coeff[HDF_COEFF_PHASE4_456]&0x1ff));
  DEBUGF2(3,(" 0x%lx",(m_filters[luma].coeff[HDF_COEFF_PHASE4_456]>>9)&0x3ff));
  DEBUGF2(3,(" 0x%lx\n",(m_filters[luma].coeff[HDF_COEFF_PHASE4_456]>>19)&0x3ff));

  WriteTVFmtReg(TVFMT_LUMA_COEFF_P1_T123, m_filters[luma].coeff[HDF_COEFF_PHASE1_123]);
  WriteTVFmtReg(TVFMT_LUMA_COEFF_P1_T456, m_filters[luma].coeff[HDF_COEFF_PHASE1_456]);
  WriteTVFmtReg(TVFMT_LUMA_COEFF_P2_T123, m_filters[luma].coeff[HDF_COEFF_PHASE2_123]);
  WriteTVFmtReg(TVFMT_LUMA_COEFF_P2_T456, m_filters[luma].coeff[HDF_COEFF_PHASE2_456]);
  WriteTVFmtReg(TVFMT_LUMA_COEFF_P3_T123, m_filters[luma].coeff[HDF_COEFF_PHASE3_123]);
  WriteTVFmtReg(TVFMT_LUMA_COEFF_P3_T456, m_filters[luma].coeff[HDF_COEFF_PHASE3_456]);
  WriteTVFmtReg(TVFMT_LUMA_COEFF_P4_T123, m_filters[luma].coeff[HDF_COEFF_PHASE4_123]);
  WriteTVFmtReg(TVFMT_LUMA_COEFF_P4_T456, m_filters[luma].coeff[HDF_COEFF_PHASE4_456]);

  DEBUGF2(3,("Chroma P1: 0x%lx",m_filters[chroma].coeff[HDF_COEFF_PHASE1_123]&0x7f));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE1_123]>>7)&0xff));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE1_123]>>15)&0x1ff));

  DEBUGF2(3,(" 0x%lx",m_filters[chroma].coeff[HDF_COEFF_PHASE1_456]&0x1ff));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE1_456]>>9)&0x3ff));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE1_456]>>19)&0x3ff));

  DEBUGF2(3,(" 0x%lx\n",m_filters[chroma].coeff[HDF_COEFF_PHASE1_7]&0x3ff));

  DEBUGF2(3,("Chroma P2: 0x%lx",m_filters[chroma].coeff[HDF_COEFF_PHASE2_123]&0x7f));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE2_123]>>7)&0xff));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE2_123]>>15)&0x1ff));

  DEBUGF2(3,(" 0x%lx",m_filters[chroma].coeff[HDF_COEFF_PHASE2_456]&0x1ff));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE2_456]>>9)&0x3ff));
  DEBUGF2(3,(" 0x%lx\n",(m_filters[chroma].coeff[HDF_COEFF_PHASE2_456]>>19)&0x3ff));

  DEBUGF2(3,("Chroma P3: 0x%lx",m_filters[chroma].coeff[HDF_COEFF_PHASE3_123]&0x7f));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE3_123]>>7)&0xff));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE3_123]>>15)&0x1ff));

  DEBUGF2(3,(" 0x%lx",m_filters[chroma].coeff[HDF_COEFF_PHASE3_456]&0x1ff));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE3_456]>>9)&0x3ff));
  DEBUGF2(3,(" 0x%lx\n",(m_filters[chroma].coeff[HDF_COEFF_PHASE3_456]>>19)&0x3ff));

  DEBUGF2(3,("Chroma P4: 0x%lx",m_filters[chroma].coeff[HDF_COEFF_PHASE4_123]&0x7f));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE4_123]>>7)&0xff));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE4_123]>>15)&0x1ff));

  DEBUGF2(3,(" 0x%lx",m_filters[chroma].coeff[HDF_COEFF_PHASE4_456]&0x1ff));
  DEBUGF2(3,(" 0x%lx",(m_filters[chroma].coeff[HDF_COEFF_PHASE4_456]>>9)&0x3ff));
  DEBUGF2(3,(" 0x%lx\n",(m_filters[chroma].coeff[HDF_COEFF_PHASE4_456]>>19)&0x3ff));

  WriteTVFmtReg(TVFMT_CHROMA_COEFF_P1_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE1_123]);
  WriteTVFmtReg(TVFMT_CHROMA_COEFF_P1_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE1_456]);
  WriteTVFmtReg(TVFMT_CHROMA_COEFF_P2_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE2_123]);
  WriteTVFmtReg(TVFMT_CHROMA_COEFF_P2_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE2_456]);
  WriteTVFmtReg(TVFMT_CHROMA_COEFF_P3_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE3_123]);
  WriteTVFmtReg(TVFMT_CHROMA_COEFF_P3_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE3_456]);
  WriteTVFmtReg(TVFMT_CHROMA_COEFF_P4_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE4_123]);
  WriteTVFmtReg(TVFMT_CHROMA_COEFF_P4_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE4_456]);
}
