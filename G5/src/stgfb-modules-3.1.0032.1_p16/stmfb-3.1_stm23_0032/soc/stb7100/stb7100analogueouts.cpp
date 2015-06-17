/***********************************************************************
 *
 * File: soc/stb7100/stb7100analogueouts.cpp
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

#include <STMCommon/stmfsynth.h>

#include "stb7100reg.h"
#include "stb7100device.h"
#include "stb7100analogueouts.h"
#include "stb7100dvo.h"
#include "stb7100hdmi.h"
#include "stb7100mixer.h"
#include "stb7100vtg.h"
#include "stb7100denc.h"
#include "stb7100AWGAnalog.h"

/*  SD Upsampler Coefficients */
#define  SD_COEFF_SET1_1  0x3F9013FF
#define  SD_COEFF_SET1_2  0x011FC40B
#define  SD_COEFF_SET1_3  0x3F1045ED
#define  SD_COEFF_SET1_4  0x004FE40B
#define  SD_COEFF_SET2_1  0x3ED027FC
#define  SD_COEFF_SET2_2  0x0A1EF824
#define  SD_COEFF_SET2_3  0x01DEDDC0
#define  SD_COEFF_SET2_4  0x3FF013F4
#define  SD_COEFF_SET3_1  0x3E902BFC
#define  SD_COEFF_SET3_2  0x143E802E
#define  SD_COEFF_SET3_3  0x02EE8143
#define  SD_COEFF_SET3_4  0x3FC02BE9
#define  SD_COEFF_SET4_1  0x3F4013FF
#define  SD_COEFF_SET4_2  0x1C0EDC1D
#define  SD_COEFF_SET4_3  0x024EF8A1
#define  SD_COEFF_SET4_4  0x3FC027ED

/*  HD Upsampler Coefficients */
#define  HD_COEFF_SET1_1  0x3EC027FC
#define  HD_COEFF_SET1_2  0x056F0823
#define  HD_COEFF_SET1_3  0x01DEDD0C
#define  HD_COEFF_SET1_4  0x3FC027EF
#define  HD_COEFF_SET2_1  0x3EF027FC
#define  HD_COEFF_SET2_2  0x10CEDC1D
#define  HD_COEFF_SET2_3  0x023F0856
#define  HD_COEFF_SET2_4  0x3FC027EC
#define  HD_COEFF_SET3_1  0x00000000
#define  HD_COEFF_SET3_2  0x00000000
#define  HD_COEFF_SET3_3  0x00000000
#define  HD_COEFF_SET3_4  0x00000000
#define  HD_COEFF_SET4_1  0x00000000
#define  HD_COEFF_SET4_2  0x00000000
#define  HD_COEFF_SET4_3  0x00000000
#define  HD_COEFF_SET4_4  0x00000000

/*
 * Delay (in pixel clocks) to apply to the DVO hsync and vsync signals so
 * they are aligned with the DVO or analogue D-SUB video output signals.
 */
#define DVO_SYNCS_DELAY     0

//////////////////////////////////////////////////////////////////////////////
//
// HD, SD Progressive and SD Interlaced Output on main HD Dacs
//
CSTb7100MainOutput::CSTb7100MainOutput(CDisplayDevice    *pDev,
                                       CSTb7100VTG1      *pVTG1,
                                       CSTb7100VTG2      *pVTG2,
                                       CSTb7100DENC      *pDENC,
                                       CGammaMixer       *pMixer,
                                       CSTmFSynth        *pFSynth,
                                       CSTmFSynth        *pFSynthAux,
                                       CSTb7100AWGAnalog *pAWG,
                                       stm_plane_id_t     sharedPlane): CSTmMasterOutput(pDev, pDENC, pVTG1, pMixer, pFSynth)
{
  m_pVTG2      = pVTG2;
  m_pAWGAnalog = pAWG;
  m_pDVO       = 0;
  m_pHDMI      = 0;

  m_pFSynthAux = pFSynthAux;
  m_sharedGDP  = sharedPlane;

  m_bUse108MHzForSD = false;
  m_bDacHdPower     = false;

  m_colorspaceMode  = STM_YCBCR_COLORSPACE_AUTO_SELECT;

  /*
   * Recommended STb7100 design is for a 1.4v max output voltage
   * from the video DACs. By default we set the saturation point
   * to the full range of the 10bit DAC. See RecalculateDACSetup for
   * more details.
   */
  m_maxDACVoltage  = 1400; // in mV
  m_DACSaturation  = 1023;
  RecalculateDACSetup();

  DEBUGF2(2,("CSTb7100MainOutput::CSTb7100MainOutput - m_pVTG   = %p\n", m_pVTG));
  DEBUGF2(2,("CSTb7100MainOutput::CSTb7100MainOutput - m_pVTG2  = %p\n", m_pVTG2));
  DEBUGF2(2,("CSTb7100MainOutput::CSTb7100MainOutput - m_pDENC  = %p\n", m_pDENC));
  DEBUGF2(2,("CSTb7100MainOutput::CSTb7100MainOutput - m_pMixer = %p\n", m_pMixer));

  /* Select Main compositor input and output defaults */
  m_ulInputSource = STM_AV_SOURCE_MAIN_INPUT;
  CSTb7100MainOutput::SetControl(STM_CTRL_VIDEO_OUT_SELECT, STM_VIDEO_OUT_YUV);

  m_pVTG->SetSyncType(VTG_INT_SYNC_ID,  STVTG_SYNC_POSITIVE);    // Internal display IP
  m_pVTG->SetSyncType(VTG_DVO_SYNC_ID, STVTG_SYNC_TIMING_MODE); // DVO/D-Sub external syncs
  m_pVTG->SetSyncType(VTG_AWG_SYNC_ID,  STVTG_SYNC_POSITIVE);    // AWG

  m_pVTG->SetHSyncOffset(VTG_DVO_SYNC_ID, DVO_SYNCS_DELAY);
  m_pVTG->SetVSyncHOffset(VTG_DVO_SYNC_ID, DVO_SYNCS_DELAY);

}


CSTb7100MainOutput::~CSTb7100MainOutput() {}


void CSTb7100MainOutput::SetSlaveOutputs(COutput *dvo, COutput *hdmi)
{
  DEBUGF2(2,(FENTRY "dvo = %p hdmi = %p\n",__PRETTY_FUNCTION__,dvo,hdmi));
  m_pDVO  = dvo;
  m_pHDMI = hdmi;
  DEXIT();
}


const stm_mode_line_t* CSTb7100MainOutput::SupportedMode(const stm_mode_line_t *mode) const
{
  DEBUGF2(3,("CSTb7100MainOutput::SupportedMode\n"));
  /*
   * The 7100 HD/Main path can support HD and SD progressive modes independently
   * of the SD/Aux path, or can route through the SD path, if it isn't already
   * in use, for SD interlaced modes with the SD DENC output routed back to
   * the HD DACs. However SD interlaced modes will be unavailable if
   * CSTb7100AuxOutput is active.
   */

  if(mode->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK)
  {
    DEBUGF2(3,("CSTb7100MainOutput::SupportedMode Looking for valid HD mode, pixclock = %lu\n",mode->TimingParams.ulPixelClock));

    if(mode->ModeParams.OutputStandards & STM_OUTPUT_STD_AS4933)
    {
      DEBUGF2(3,("CSTb7100MainOutput::SupportedMode AS4933 not supported\n"));
      return 0;
    }

    if(mode->TimingParams.ulPixelClock <= 74250000)
    {
      DEBUGF2(3,("CSTb7100MainOutput::SupportedMode Found valid HD mode\n"));
      return mode;
    }
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
      DEBUGF2(3,("CSTb7100MainOutput::SupportedMode Looking for valid SD progressive mode\n"));
      if(mode->TimingParams.ulPixelClock == 27000000 ||
         mode->TimingParams.ulPixelClock == 27027000 ||
         mode->TimingParams.ulPixelClock == 25174800 ||
         mode->TimingParams.ulPixelClock == 25200000)
        return mode;
    }
    else if(mode->ModeParams.OutputStandards & STM_OUTPUT_STD_TMDS_PIXELREP_2X)
    {
      DEBUGF2(3,("CSTb7100MainOutput::SupportedMode Looking for valid SD HDMI mode\n"));
      if(mode->TimingParams.ulPixelClock == 27000000)
        return mode;
    }
    else
    {
      DEBUGF2(3,("CSTb7100MainOutput::SupportedMode Looking for valid SD interlaced mode\n"));
      if(mode->TimingParams.ulPixelClock == 13500000 ||
         mode->TimingParams.ulPixelClock == 13513500)
        return mode;
    }
  }

  return 0;
}


ULONG CSTb7100MainOutput::GetCapabilities(void) const
{
  ULONG caps =  (STM_OUTPUT_CAPS_SD_ANALOGUE       |
                 STM_OUTPUT_CAPS_ED_ANALOGUE       |
                 STM_OUTPUT_CAPS_HD_ANALOGUE       |
                 STM_OUTPUT_CAPS_PLANE_MIXER       |
                 STM_OUTPUT_CAPS_MODE_CHANGE       |
                 STM_OUTPUT_CAPS_CVBS_YC_EXCLUSIVE | /* Component DACs disabled                                            */
                 STM_OUTPUT_CAPS_YPrPb_EXCLUSIVE   | /* ED/HD component YPrPb, or SD component with CVBS&Y/C DACs disabled */
                 STM_OUTPUT_CAPS_RGB_EXCLUSIVE     | /* VGA/D-Sub, RGB on component output with no embedded syncs          */
                 STM_OUTPUT_CAPS_SD_RGB_CVBS_YC    | /* SCART, RGB+CVBS Y/C optionally disabled                            */
                 STM_OUTPUT_CAPS_SD_YPrPb_CVBS_YC);  /* Component YPrPb and composite outputs supported together           */

  return caps;
}


bool CSTb7100MainOutput::SetOutputFormat(ULONG format)
{
  DEBUGF2(2,("CSTb7100MainOutput::SetOutputFormat - format = 0x%lx\n",format));

  /*
   * Don't change the VOS setup unless the display is running.
   */
  if(!m_pCurrentMode)
    return true;

  if((m_colorspaceMode == STM_YCBCR_COLORSPACE_601) ||
     (m_colorspaceMode == STM_YCBCR_COLORSPACE_AUTO_SELECT && (m_pCurrentMode->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK) == 0))
  {
    WriteVOSReg(DHDO_COLOR, DHDO_COLOR_ITUR_601_NOT_709);
  }
  else
  {
    WriteVOSReg(DHDO_COLOR, 0);
  }

  DEBUGF2(2,("CSTb7100MainOutput::SetOutputFormat DHDO_COLOR\t\t%#.8lx\n", ReadVOSReg(DHDO_COLOR)));

  if(m_bUsingDENC)
  {
    /*
     * Defer this to the DENC for interlaced modes on the HD DACs.
     */
    if(!m_pDENC->SetMainOutputFormat(format))
    {
      DEBUGF2(2,("CSTb7100MainOutput::SetOutputFormat - format unsupported on DENC\n"));
      return false;
    }

    return true;
  }


  /*
   * Select the colour format for use with progressive/HD modes. We also
   * need to change the Chroma/Red/Blue DAC level scaling. Note we only
   * support sync on Luma/Green for consumer applications at the moment.
   */
  ULONG cfgana = ReadVOSReg(DSP_CFG_ANA);
  ULONG acfg = ReadVOSReg(DHDO_ACFG);
  switch(format & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV))
  {
    case STM_VIDEO_OUT_RGB:
    {
      cfgana |= DSP_ANA_RGB_NOT_YCBCR_OUT;
      acfg &= ~DHDO_ACFG_SYNC_CHROMA_ENABLE;

      if(m_ulTVStandard & STM_OUTPUT_STD_VESA)
      {
      	/*
      	 * For VESA modes use the full RGB range mapped to 0-0.7v
      	 */
        WriteVOSReg(DHDO_YOFF, 0);
        WriteVOSReg(DHDO_COFF, 0);
        WriteVOSReg(DHDO_YMLT, m_DAC_RGB_Scale);
        WriteVOSReg(DHDO_CMLT, m_DAC_RGB_Scale);
      }
      else
      {
       /*
        * For TV modes using component RGB use the same scaling and offset
        * as Luma (Y) for all three signals.
        */
        WriteVOSReg(DHDO_YOFF, m_DAC_Y_Offset);
        WriteVOSReg(DHDO_COFF, m_DAC_Y_Offset);
        WriteVOSReg(DHDO_YMLT, m_DAC_Y_Scale);
        WriteVOSReg(DHDO_CMLT, m_DAC_Y_Scale);
      }
      break;
    }
    case STM_VIDEO_OUT_YUV:
    {
      cfgana &= ~DSP_ANA_RGB_NOT_YCBCR_OUT;
      acfg   &= ~DHDO_ACFG_SYNC_CHROMA_ENABLE;

      if(m_ulTVStandard & STM_OUTPUT_STD_VESA)
      {
      	/*
      	 * VESA modes not defined in YUV so blank the signal at 0V
      	 */
        WriteVOSReg(DHDO_YMLT, 0);
        WriteVOSReg(DHDO_CMLT, 0);
        WriteVOSReg(DHDO_YOFF, 0);
        WriteVOSReg(DHDO_COFF, 0);
      }
      else
      {
        WriteVOSReg(DHDO_YMLT, m_DAC_Y_Scale);
        WriteVOSReg(DHDO_CMLT, m_DAC_C_Scale);
        WriteVOSReg(DHDO_YOFF, m_DAC_Y_Offset);
        WriteVOSReg(DHDO_COFF, m_DAC_C_Offset);
      }

      break;
    }
    default:
    {
      WriteVOSReg(DHDO_YOFF, 0);
      WriteVOSReg(DHDO_COFF, 0);
      WriteVOSReg(DHDO_YMLT, 0);
      WriteVOSReg(DHDO_CMLT, 0);
      break;
    }
  }

  DEBUGF2(2,("CSTb7100MainOutput::SetOutputFormat DHDO_YMLT\t\t%#.8lx\n", ReadVOSReg(DHDO_YMLT)));
  DEBUGF2(2,("CSTb7100MainOutput::SetOutputFormat DHDO_YOFF\t\t%#.8lx\n", ReadVOSReg(DHDO_YOFF)));
  DEBUGF2(2,("CSTb7100MainOutput::SetOutputFormat DHDO_CMLT\t\t%#.8lx\n", ReadVOSReg(DHDO_CMLT)));
  DEBUGF2(2,("CSTb7100MainOutput::SetOutputFormat DHDO_COFF\t\t%#.8lx\n", ReadVOSReg(DHDO_COFF)));

  WriteVOSReg(DSP_CFG_ANA, cfgana);
  WriteVOSReg(DHDO_ACFG,   acfg);

  DEBUGF2(2,("CSTb7100MainOutput::SetOutputFormat - out\n"));
  return true;
}


void CSTb7100MainOutput::SetControl(stm_output_control_t ctrl, ULONG val)
{
  DEBUGF2(2,("CSTb7100MainOutput::SetControl ctrl = %u val = %lu\n",(unsigned)ctrl,val));
  switch(ctrl)
  {
    case STM_CTRL_DAC_HD_POWER:
    {
      m_bDacHdPower = (val != 0);
      if (m_pCurrentMode)
      {
        if(!m_bDacHdPower)
          EnableDACs();
        else
        {
          val = ReadVOSReg(DSP_CFG_DAC) & ~DSP_DAC_HD_POWER_ON;
          WriteVOSReg(DSP_CFG_DAC, val);
        }
      }
      break;
    }
    case STM_CTRL_YCBCR_COLORSPACE:
    {
      if(val > STM_YCBCR_COLORSPACE_709)
        return;

      m_colorspaceMode = static_cast<stm_ycbcr_colorspace_t>(val);

      DEBUGF2(2,("%s: new colourspace mode = %d\n", __PRETTY_FUNCTION__, (int)m_colorspaceMode));

      /*
       * On this device the colourspace conversion is done by the VOS not by
       * the mixer, this is setup in SetOutputFormat so just call it with the
       * current format to make the change if the display is already active.
       */
      SetOutputFormat(m_ulOutputFormat);

      /*
       * Now inform the HDMI slave output of the change for the AVI infoframe.
       */
      if(m_pHDMI)
        m_pHDMI->SetControl(ctrl, val);

      break;
    }
    default:
      CSTmMasterOutput::SetControl(ctrl,val);
      break;
  }
}


ULONG CSTb7100MainOutput::GetControl(stm_output_control_t ctrl) const
{
  switch(ctrl)
  {
    case STM_CTRL_DAC_HD_POWER:
      return m_bDacHdPower?1:0;
    case STM_CTRL_YCBCR_COLORSPACE:
      return m_colorspaceMode;
    default:
      return CSTmMasterOutput::GetControl(ctrl);
  }

  return 0;
}


bool CSTb7100MainOutput::Start(const stm_mode_line_t *mode, ULONG tvStandard)
{
  ULONG val;
  DEBUGF2(2,("CSTb7100MainOutput::Start - in\n"));

  /*
   * On the 7100, strip any secondary mode flags out when SMPTE293M is defined.
   * Other chips need additional modes for simultaneous re-interlaced DENC
   * output. Stripping that out lets us use tvStandard more sensibly throughout,
   * making the code more readable.
   */
  if(tvStandard & STM_OUTPUT_STD_SMPTE293M)
    tvStandard = STM_OUTPUT_STD_SMPTE293M;

  if((mode->ModeParams.OutputStandards & tvStandard) != tvStandard)
  {
    DEBUGF2(1,("CSTb7100MainOutput::Start - requested standard not supported by mode\n"));
    return false;
  }

  if(m_bIsSuspended)
  {
    DEBUGF2(1,("CSTb7100MainOutput::Start output is suspended\n"));
    return false;
  }

  /*
   * First try to change the display mode on the fly, if that works there is
   * nothing else to do.
   */
  if(TryModeChange(mode, tvStandard))
  {
    DEBUGF2(2,("CSTb7100MainOutput::Start - mode change successfull\n"));
    return true;
  }

  if(m_pCurrentMode)
  {
    DEBUGF2(1,("CSTb7100MainOutput::Start - failed, output is active\n"));
    return false;
  }

  if(tvStandard & STM_OUTPUT_STD_SD_MASK)
  {
    if(m_pVTG2->GetCurrentMode() != 0)
    {
      DEBUGF2(1,("CSTb7100MainOutput::Start - cannot set interlaced SD mode, SD path in use\n"));
      return false;
    }
  }

  /*
   * Setup the common HD/SD Progressive timing and sync pulse insertion.
   *
   * We do this for all modes to simplify the logic, however they
   * are ignored for SD interlaced modes. In that case it is the
   * DENC which formats the video lines.
   */
  WriteVOSReg(DHDO_ABROAD,  mode->TimingParams.HSyncPulseWidth); // Also AACTIVE
  WriteVOSReg(DHDO_BACTIVE, mode->ModeParams.ActiveAreaXStart);
  WriteVOSReg(DHDO_CACTIVE, mode->ModeParams.ActiveAreaXStart+mode->ModeParams.ActiveAreaWidth);

  /*
   * We don't blank the VBI any more because we may want to add IEC 61880-2 or
   * CEA-805 VBI lines.
   */
  WriteVOSReg(DHDO_BLANKL, 0);
  if(mode->ModeParams.ScanType == SCAN_I)
  {
    WriteVOSReg(DHDO_ACTIVEL, ((mode->ModeParams.ActiveAreaHeight / 2) +
                               (mode->ModeParams.FullVBIHeight    / 2) -
                               mode->TimingParams.VSyncPulseWidth));
  }
  else
  {
    WriteVOSReg(DHDO_ACTIVEL, (mode->ModeParams.ActiveAreaHeight +
                               mode->ModeParams.FullVBIHeight    -
                               mode->TimingParams.VSyncPulseWidth));
  }

  WriteVOSReg(DHDO_BROADL, mode->TimingParams.VSyncPulseWidth);

  val = ReadVOSReg(DSP_CFG_ANA);

  if(tvStandard & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_HD_MASK))
  {
    /*
     * SD progressive or HD modes need sync insertion. By defult enable 770.x
     * standard waveforms, special user defined waveform generation will be
     * used as a last resort and the 770 sync insertion turned off again later.
     */
    DEBUGF2(2,("CSTb7100MainOutput::Start - Enabling 770.x sync insertion\n"));
    val |= (DSP_ANA_HD_SYNC_ENABLE | DSP_ANA_RESCALE_FOR_SYNC);

  }
  else if(tvStandard & STM_OUTPUT_STD_VESA)
  {
    /*
     * VESA (VGA) modes need rescaling enabled to get the correct signal
     * levels out, but no syncs are embedded in the signal.
     */
    DEBUGF2(2,("CSTb7100MainOutput::Start - Enabling rescale for VESA modes\n"));
    val &= ~DSP_ANA_HD_SYNC_ENABLE;
    val |= DSP_ANA_RESCALE_FOR_SYNC;
  }
  else
  {
    /*
     * When showing interlaced content produced by the
     * DENC then turn sync insertion and rescale off.
     */
    val &= ~(DSP_ANA_HD_SYNC_ENABLE | DSP_ANA_RESCALE_FOR_SYNC);
  }

  WriteVOSReg(DSP_CFG_ANA, val);

  if(mode->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK)
  {
    if(!StartHDDisplay(mode))
      return false;
  }
  else
  {
    /*
     * Note that this path also deals with VESA (VGA) modes at the moment.
     */
    if(!StartSDDisplay(mode, tvStandard))
      return false;
  }

  if(!m_pMixer->Start(mode))
  {
    DEBUGF2(1,("CSTb7100MainOutput::Start Mixer start failed\n"));
    return false;
  }


  /*
   * We don't want anything from CSTmMasterOutput::Start, but we do
   * need to call the base class Start.
   */
  COutput::Start(mode, tvStandard);

  /*
   * Now update the output DAC scaling and colour format, which is dependent
   * on exactly which display standard is set.
   */
  SetOutputFormat(m_ulOutputFormat);

  EnableDACs();

  DEBUGF2(2,("CSTb7100MainOutput::Start DSP_CFG_CLK\t\t%#.8lx\n",        ReadVOSReg(DSP_CFG_CLK)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DSP_CFG_DIG\t\t%#.8lx\n",        ReadVOSReg(DSP_CFG_DIG)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DSP_CFG_ANA\t\t%#.8lx\n",        ReadVOSReg(DSP_CFG_ANA)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_ACFG\t\t%#.8lx\n",          ReadVOSReg(DHDO_ACFG)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_ABROAD\t\t%#.8lx (%lu)\n",  ReadVOSReg(DHDO_ABROAD),  ReadVOSReg(DHDO_ABROAD)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_BBROAD\t\t%#.8lx (%lu)\n",  ReadVOSReg(DHDO_BBROAD),  ReadVOSReg(DHDO_BBROAD)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_CBROAD\t\t%#.8lx (%lu)\n",  ReadVOSReg(DHDO_CBROAD),  ReadVOSReg(DHDO_CBROAD)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_BACTIVE\t\t%#.8lx (%lu)\n", ReadVOSReg(DHDO_BACTIVE), ReadVOSReg(DHDO_BACTIVE)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_CACTIVE\t\t%#.8lx (%lu)\n", ReadVOSReg(DHDO_CACTIVE), ReadVOSReg(DHDO_CACTIVE)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_BROADL\t\t%#.8lx (%lu)\n",  ReadVOSReg(DHDO_BROADL),  ReadVOSReg(DHDO_BROADL)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_BLANKL\t\t%#.8lx (%lu)\n",  ReadVOSReg(DHDO_BLANKL),  ReadVOSReg(DHDO_BLANKL)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_ACTIVEL\t\t%#.8lx (%lu)\n", ReadVOSReg(DHDO_ACTIVEL), ReadVOSReg(DHDO_ACTIVEL)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_LO\t\t%#.8lx (%lu)\n",      ReadVOSReg(DHDO_LO),      ReadVOSReg(DHDO_LO)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_MIDLO\t\t%#.8lx (%lu)\n",   ReadVOSReg(DHDO_MIDLO),   ReadVOSReg(DHDO_MIDLO)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_ZEROL\t\t%#.8lx (%lu)\n",   ReadVOSReg(DHDO_ZEROL),   ReadVOSReg(DHDO_ZEROL)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_MIDL\t\t%#.8lx (%lu)\n",    ReadVOSReg(DHDO_MIDL),    ReadVOSReg(DHDO_MIDL)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_HI\t\t%#.8lx (%lu)\n",      ReadVOSReg(DHDO_HI),      ReadVOSReg(DHDO_HI)));
  DEBUGF2(2,("CSTb7100MainOutput::Start DHDO_COLOR\t\t%#.8lx\n",         ReadVOSReg(DHDO_COLOR)));

  DEBUGF2(2,("CSTb7100MainOutput::Start - out\n"));
  return true;
}


bool CSTb7100MainOutput::StartSDDisplay(const stm_mode_line_t *mode, ULONG tvStandard)
{
  DEBUGF2(2,("CSTb7100MainOutput::StartSDDisplay\n"));

  // Upsampler coefficients for all SD modes
  WriteVOSReg(COEFF_SET1_1, SD_COEFF_SET1_1);
  WriteVOSReg(COEFF_SET1_2, SD_COEFF_SET1_2);
  WriteVOSReg(COEFF_SET1_3, SD_COEFF_SET1_3);
  WriteVOSReg(COEFF_SET1_4, SD_COEFF_SET1_4);
  WriteVOSReg(COEFF_SET2_1, SD_COEFF_SET2_1);
  WriteVOSReg(COEFF_SET2_2, SD_COEFF_SET2_2);
  WriteVOSReg(COEFF_SET2_3, SD_COEFF_SET2_3);
  WriteVOSReg(COEFF_SET2_4, SD_COEFF_SET2_4);
  WriteVOSReg(COEFF_SET3_1, SD_COEFF_SET3_1);
  WriteVOSReg(COEFF_SET3_2, SD_COEFF_SET3_2);
  WriteVOSReg(COEFF_SET3_3, SD_COEFF_SET3_3);
  WriteVOSReg(COEFF_SET3_4, SD_COEFF_SET3_4);
  WriteVOSReg(COEFF_SET4_1, SD_COEFF_SET4_1);
  WriteVOSReg(COEFF_SET4_2, SD_COEFF_SET4_2);
  WriteVOSReg(COEFF_SET4_3, SD_COEFF_SET4_3);
  WriteVOSReg(COEFF_SET4_4, SD_COEFF_SET4_4);

  if(tvStandard & STM_OUTPUT_STD_SD_MASK)
  {
    /*
     * Just make sure someone hasn't specified an SD progressive mode
     * without specifying OUTPUT_CAPS_SD_SMPTE293M
     */
    if(mode->ModeParams.ScanType != SCAN_I)
      return false;

    if(!StartSDInterlacedDisplay(mode, tvStandard))
      return false;
  }
  else
  {
    if(!StartSDProgressiveDisplay(mode, tvStandard))
      return false;
  }

  return true;
}


bool CSTb7100MainOutput::StartSDInterlacedDisplay(const stm_mode_line_t *mode, ULONG tvStandard)
{
  ULONG val;

  DEBUGF2(2,("CSTb7100MainOutput::StartSDInterlacedDisplay\n"));

  /*
   * Set the output upsampler to 4x.
   * This doesn't make a lot of sense with the pixel clock now at 54MHz instead
   * of 108MHz, but it just doesn't work if you use 2x .
   */
  val  = ReadVOSReg(DSP_CFG_CLK) & ~(DSP_CLK_UPSAMPLE_4X_SD_PROGRESSIVE | DSP_CLK_UPSAMPLE_COEFF_MASK);
  val |= (DSP_CLK_UPSAMPLE_4X_NOT_2X | DSP_CLK_UPSAMPLER_ENABLE | DSP_CLK_UPSAMPLE_COEFF_512);
  WriteVOSReg(DSP_CFG_CLK, val);

  if(m_bUse108MHzForSD)
  {
    /*
     * Set the clock divides for each block for SD interlaced timings
     * based on a 108MHz input clock.
     */
    DEBUGF2(2,("CSTb7100MainOutput::StartSDInterlacedDisplay clock setup for 108MHz ref\n"));
    WriteClkReg(CKGB_DISP_CFG, CKGB_CFG_TMDS_HDMI_FS0_DIV4 | // 27MHz
                               CKGB_CFG_PIX_HDMI_FS0_DIV8  | // 13.5MHz
                               CKGB_CFG_DISP_HD_FS0_DIV8   | // 13.5MHz
                               CKGB_CFG_656_FS0_DIV4       | // 27MHz
                               CKGB_CFG_DISP_ID_FS1_DIV2   | // 13.5MHz (always)
                               CKGB_CFG_PIX_SD_FS0_DIV4);    // 27MHz (for SD PIX)

    m_pFSynth->SetDivider(8);
  }
  else
  {
    /*
     * Set the clock divides for each block for SD interlaced timings
     * based on a 54MHz input clock.
     */
    DEBUGF2(2,("CSTb7100MainOutput::StartSDInterlacedDisplay clock setup for 54MHz ref\n"));
    WriteClkReg(CKGB_DISP_CFG, CKGB_CFG_TMDS_HDMI_FS0_DIV2 | // 27MHz
                               CKGB_CFG_PIX_HDMI_FS0_DIV4  | // 13.5MHz
                               CKGB_CFG_DISP_HD_FS0_DIV4   | // 13.5MHz
                               CKGB_CFG_656_FS0_DIV2       | // 27MHz (for 8bit 4:2:2 DVO output)
                               CKGB_CFG_DISP_ID_FS1_DIV2   | // 13.5MHz (always)
                               CKGB_CFG_PIX_SD_FS0_DIV2);    // 27MHz (for SD PIX)

    m_pFSynth->SetDivider(4);
  }

  // Set the SD pixel clock to be sourced from the HD FSynth.
  val = ReadClkReg(CKGB_CLK_SRC) & ~CKGB_SRC_PIXSD_FS1_NOT_FS0;
  WriteClkReg(CKGB_CLK_SRC, val);

  // Set the DENC routing to take the main video as its source.
  val = ReadVOSReg(DSP_CFG_CLK) | DSP_CLK_DENC_MAIN_NOT_AUX;
  WriteVOSReg(DSP_CFG_CLK, val);
  DEBUGF2(2,("CSTb7100MainOutput::StartSDInterlacedDisplay DENC switched to main video\n"));

  val = ReadVOSReg(DHDO_ACFG) & ~(DHDO_ACFG_PROGRESSIVE_SMPTE_295M | DHDO_ACFG_PROGRESSIVE_SMPTE_293M);
  WriteVOSReg(DHDO_ACFG, val);

  if(!m_pVTG->Start(mode))
  {
    DEBUGF2(1,("CSTb7100MainOutput::StartSDInterlacedDisplay VTG start failed\n"));
    return false;
  }

  // Ensure Aux pipeline clock is at the correct frequency for shared GDP.
  m_pFSynthAux->Start(mode->TimingParams.ulPixelClock);

  // Start VTG2 in slave mode for SD Interlaced modes.
  if(!m_pVTG2->Start(0))
  {
    DEBUGF2(1,("CSTb7100MainOutput::StartSDInterlacedDisplay VTG2 start failed\n"));
    return false;
  }

  if(!m_pDENC->Start(this, mode, tvStandard))
  {
    DEBUGF2(1,("CSTb7100MainOutput::StartSDInterlacedDisplay DENC start failed\n"));
    return false;
  }

  m_bUsingDENC = true;

  /*
   * Now we have ownership of the DENC, set this output's PSI control setup.
   */
  ProgramPSIControls();

  m_pDENC->SetControl(STM_CTRL_SIGNAL_RANGE, (ULONG)m_signalRange);

  return true;
}


bool CSTb7100MainOutput::StartSDProgressiveDisplay(const stm_mode_line_t *mode, ULONG tvStandard)
{
  ULONG val;

  DEBUGF2(2,("CSTb7100MainOutput::StartSDProgressiveDisplay\n"));

  /*
   * Set the upsampler to the 4x SD Progressive input path.
   * This doesn't make a lot of sense with the pixel clock now at 54MHz instead
   * of 108MHz, but there is no way of picking 2x with the SD progressive route.
   */
  val  = ReadVOSReg(DSP_CFG_CLK) & ~(DSP_CLK_UPSAMPLE_4X_NOT_2X | DSP_CLK_UPSAMPLE_COEFF_MASK);
  val |= (DSP_CLK_UPSAMPLE_4X_SD_PROGRESSIVE | DSP_CLK_UPSAMPLER_ENABLE | DSP_CLK_UPSAMPLE_COEFF_512);
  WriteVOSReg(DSP_CFG_CLK, val);

  if(m_bUse108MHzForSD)
  {
    /*
     * Set the clock divides for each block for SD progressive modes
     * based on a 108MHz input clock.
     */
    DEBUGF2(2,("CSTb7100MainOutput::StartSDProgressiveDisplay clock setup for 108MHz ref\n"));
    WriteClkReg(CKGB_DISP_CFG, CKGB_CFG_TMDS_HDMI_FS0_DIV4 | // 27MHz
                               CKGB_CFG_PIX_HDMI_FS0_DIV4  | // 27MHz
                               CKGB_CFG_DISP_HD_FS0_DIV4   | // 27MHz
                               CKGB_CFG_656_FS0_DIV4       | // 27MHz (for 16bit 4:2:2 external DVO clock)
                               CKGB_CFG_DISP_ID_FS1_DIV2   | // 13.5MHz (always)
                               CKGB_CFG_PIX_SD_FS0_DIV4);    // 27MHz (for shared GDP)

    m_pFSynth->SetDivider(4);
  }
  else
  {
    /*
     * Set the clock divides for each block for SD progressive modes
     * based on a 54MHz input clock.
     *
     * Note that it looks like the 656 clock is broken as it should need
     * a 54MHz clock but there is no way of generating this starting from
     * a 54MHz reference as there is at least a divide by 2 involved.
     */
    DEBUGF2(2,("CSTb7100MainOutput::StartSDProgressiveDisplay clock setup for 54MHz ref\n"));
    WriteClkReg(CKGB_DISP_CFG, CKGB_CFG_TMDS_HDMI_FS0_DIV2 | // 27MHz
                               CKGB_CFG_PIX_HDMI_FS0_DIV2  | // 27MHz
                               CKGB_CFG_DISP_HD_FS0_DIV2   | // 27MHz
                               CKGB_CFG_656_FS0_DIV2       | // 27MHz (for 16bit 4:2:2 external DVO clock)
                               CKGB_CFG_DISP_ID_FS1_DIV2   | // 13.5MHz (always)
                               CKGB_CFG_PIX_SD_FS0_DIV2);    // 27MHz (for shared GDP)

    m_pFSynth->SetDivider(2);
  }

  if(tvStandard & STM_OUTPUT_STD_VESA)
  {
    WriteVOSReg(DHDO_BBROAD, 0);
    WriteVOSReg(DHDO_CBROAD, 0);
    WriteVOSReg(DHDO_LO,     0);
    WriteVOSReg(DHDO_MIDLO,  0);
    WriteVOSReg(DHDO_ZEROL,  0);
    WriteVOSReg(DHDO_MIDL,   0);
    WriteVOSReg(DHDO_HI  ,   0);
  }
  else
  {
    // Set Sync insertion for SMPTE293M Progressive (Bi-Level)
    val = ReadVOSReg(DHDO_ACFG) & ~(DHDO_ACFG_PROGRESSIVE_SMPTE_295M | DHDO_ACFG_PROGRESSIVE_SMPTE_293M);
    WriteVOSReg(DHDO_ACFG, val | DHDO_ACFG_PROGRESSIVE_SMPTE_293M);

    // Define broad line pulse length for VSync
    WriteVOSReg(DHDO_BBROAD, mode->TimingParams.PixelsPerLine - mode->TimingParams.HSyncPulseWidth);
    WriteVOSReg(DHDO_CBROAD, mode->TimingParams.PixelsPerLine);

    // Setup sync levels for Luma/Green only.
    WriteVOSReg(DHDO_LO,    m_DAC_321mV - m_DAC_43IRE);
    WriteVOSReg(DHDO_MIDLO, m_DAC_321mV - (m_DAC_43IRE/2));
    WriteVOSReg(DHDO_ZEROL, m_DAC_321mV);
    WriteVOSReg(DHDO_MIDL,  m_DAC_321mV + (m_DAC_700mV/2));
    WriteVOSReg(DHDO_HI  ,  m_DAC_321mV + m_DAC_700mV);
  }

  if(!m_pVTG->Start(mode))
  {
    DEBUGF2(1,("CSTb7100MainOutput::StartSDProgressiveDisplay VTG start failed\n"));
    return false;
  }

  return true;
}


bool CSTb7100MainOutput::StartHDDisplay(const stm_mode_line_t *mode)
{
  ULONG val = 0;
  DEBUGF2(2,("CSTb7100MainOutput::StartHDdisplay\n"));

  // Set the clock divides for each block for HD modes
  WriteClkReg(CKGB_DISP_CFG, CKGB_CFG_TMDS_HDMI_FS0_DIV2 |
                             CKGB_CFG_PIX_HDMI_FS0_DIV2  |
                             CKGB_CFG_DISP_HD_FS0_DIV2   |
                             CKGB_CFG_656_FS0_DIV2       | // for DVO external clock
                             CKGB_CFG_DISP_ID_FS1_DIV2   | // 13.5MHz (always)
                             CKGB_CFG_PIX_SD_FS0_DIV2);    // for shared GDP

  m_pFSynth->SetDivider(2);

  // Note: we do not support any SMPTE 295M Progressive modes
  val = ReadVOSReg(DHDO_ACFG) & ~(DHDO_ACFG_PROGRESSIVE_SMPTE_295M | DHDO_ACFG_PROGRESSIVE_SMPTE_293M);
  WriteVOSReg(DHDO_ACFG, val);

  // Upsample 2x for HD as the compositor is running at half the pixel clock
  val  = ReadVOSReg(DSP_CFG_CLK) & ~(DSP_CLK_UPSAMPLE_4X_NOT_2X | DSP_CLK_UPSAMPLE_4X_SD_PROGRESSIVE | DSP_CLK_UPSAMPLE_COEFF_MASK);
  val |= (DSP_CLK_UPSAMPLER_ENABLE | DSP_CLK_UPSAMPLE_COEFF_256);
  WriteVOSReg(DSP_CFG_CLK, val);

  // Upsampler coefficients for HD modes
  WriteVOSReg(COEFF_SET1_1, HD_COEFF_SET1_1);
  WriteVOSReg(COEFF_SET1_2, HD_COEFF_SET1_2);
  WriteVOSReg(COEFF_SET1_3, HD_COEFF_SET1_3);
  WriteVOSReg(COEFF_SET1_4, HD_COEFF_SET1_4);
  WriteVOSReg(COEFF_SET2_1, HD_COEFF_SET2_1);
  WriteVOSReg(COEFF_SET2_2, HD_COEFF_SET2_2);
  WriteVOSReg(COEFF_SET2_3, HD_COEFF_SET2_3);
  WriteVOSReg(COEFF_SET2_4, HD_COEFF_SET2_4);
  WriteVOSReg(COEFF_SET3_1, HD_COEFF_SET3_1);
  WriteVOSReg(COEFF_SET3_2, HD_COEFF_SET3_2);
  WriteVOSReg(COEFF_SET3_3, HD_COEFF_SET3_3);
  WriteVOSReg(COEFF_SET3_4, HD_COEFF_SET3_4);
  WriteVOSReg(COEFF_SET4_1, HD_COEFF_SET4_1);
  WriteVOSReg(COEFF_SET4_2, HD_COEFF_SET4_2);
  WriteVOSReg(COEFF_SET4_3, HD_COEFF_SET4_3);
  WriteVOSReg(COEFF_SET4_4, HD_COEFF_SET4_4);

  // Define broad line pulse length for VSync
  if(mode->ModeParams.ScanType == SCAN_P)
  {
    /*
     * For progressive modes position the broad pulse in the
     * same position as the active video.
     *
     * Note: This would not be true for SMPTE295M Progressive
     * but that is not a mode supported by this hardware
     */
    DEBUGF2(2,("CSTb7100MainOutput::StartHDdisplay Progressive Sync\n"));
    WriteVOSReg(DHDO_BBROAD, mode->ModeParams.ActiveAreaXStart);
    WriteVOSReg(DHDO_CBROAD, mode->ModeParams.ActiveAreaXStart + mode->ModeParams.ActiveAreaWidth);
  }
  else
  {
    if(mode->ModeParams.OutputStandards & STM_OUTPUT_STD_SMPTE274M)
    {
      DEBUGF2(2,("CSTb7100MainOutput::StartHDdisplay SMPTE 274M/EIA 770.3 Sync\n"));
      /*
       * For SMPTE 274M/EIA 770.3 modes we need to set up the broad pulse in
       * a half line, in order to accomodate a mid line tri-level sync pulse.
       */
      WriteVOSReg(DHDO_CBROAD, (mode->TimingParams.PixelsPerLine/2) - (mode->TimingParams.HSyncPulseWidth*2));
      WriteVOSReg(DHDO_BBROAD, mode->TimingParams.HSyncPulseWidth*3);
    }
    else
    {
      /*
       * Any other interlaced standards, such as AS4933 need to be generated
       * using the AWG and we don't support that on the 710x chips at this time.
       */
      WriteVOSReg(DHDO_BBROAD, 0);
      WriteVOSReg(DHDO_CBROAD, 0);
    }
  }

  if(ReadVOSReg(DHDO_CBROAD) != 0)
  {
    // Setup Tri-Level sync insertion, Luma/Green only
    WriteVOSReg(DHDO_LO,    m_DAC_321mV - m_DAC_43IRE);
    WriteVOSReg(DHDO_MIDLO, m_DAC_321mV - (m_DAC_43IRE/2));
    WriteVOSReg(DHDO_ZEROL, m_DAC_321mV);
    WriteVOSReg(DHDO_MIDL,  m_DAC_321mV + (m_DAC_43IRE/2));
    WriteVOSReg(DHDO_HI  ,  m_DAC_321mV + m_DAC_43IRE);
  }
  else
  {
    // Disable sync insertion
    WriteVOSReg(DHDO_LO,     0);
    WriteVOSReg(DHDO_MIDLO,  0);
    WriteVOSReg(DHDO_ZEROL,  0);
    WriteVOSReg(DHDO_MIDL,   0);
    WriteVOSReg(DHDO_HI  ,   0);

    val = ReadVOSReg(DSP_CFG_ANA) & ~DSP_ANA_HD_SYNC_ENABLE;
    WriteVOSReg(DSP_CFG_ANA,val);
  }

  if(!m_pVTG->Start(mode))
  {
    DEBUGF2(1,("CSTb7100MainOutput::StartHDdisplay VTG start failed\n"));
    return false;
  }

  return true;
}


bool CSTb7100MainOutput::Stop(void)
{
  DEBUGF2(2,("CSTb7100MainOutput::Stop - in\n"));

  if(m_pCurrentMode)
  {
    ULONG planes = m_pMixer->GetActivePlanes();

    if((planes & ~(ULONG)OUTPUT_BKG) != 0)
    {
      DEBUGF2(1, ("CSTb7100MainOutput::Stop error - mixer has active planes\n"));
      return false;
    }

    DisableDACs();

    // Stop slaved digital outputs.
    if(m_pHDMI)
      m_pHDMI->Stop();

    m_pDVO->Stop();

    if(m_bUsingDENC)
    {
      // Stop SD Path when slaved to HD
      m_pDENC->Stop();
      m_pVTG2->Stop();
      m_bUsingDENC = false;
    }

    // Stop HD Path.
    m_pMixer->Stop();
    m_pVTG->Stop();

    COutput::Stop();
  }

  DEBUGF2(2,("CSTb7100MainOutput::Stop - out\n"));
  return true;
}


bool CSTb7100MainOutput::ShowPlane(stm_plane_id_t planeID)
{
  DEBUGF2(2,("CSTb7100MainOutput::ShowPlane %d\n",(int)planeID));

  if(planeID == m_sharedGDP)
  {
    if(m_pCurrentMode->TimingParams.ulPixelClock == 135000000 ||
       m_pCurrentMode->TimingParams.ulPixelClock == 135135000)
    {
      /*
       * If the main video path is set for an interlaced SD mode we need
       * to use the SD/Aux divided down clock to get 13.5MHz on the shared
       * GDP.
       */
      ULONG val = ReadClkReg(CKGB_CLK_SRC) | CKGB_SRC_GDPx_FS1_NOT_FS0;
      WriteClkReg(CKGB_CLK_SRC, val);
      DEBUGF2(2,("CSTb7100MainOutput::ShowPlane shared GDP clock set to FS1\n"));
    }
    else
    {
      /*
       * For progressive and all HD modes change the clock source for the
       * shared GDP to the HD/Main video clock
       */
      ULONG val = ReadClkReg(CKGB_CLK_SRC) & ~CKGB_SRC_GDPx_FS1_NOT_FS0;
      WriteClkReg(CKGB_CLK_SRC, val);
      DEBUGF2(2,("CSTb7100MainOutput::ShowPlane shared GDP clock set to FS0\n"));
    }
  }
  else if (planeID == OUTPUT_VID2)
  {
    /*
     * Change the clock source for VID2 when on Mixer1. Note this is
     * only available on 7109Cut3 and we wouldn't be here unless the plane
     * was supported by the mixer object.
     */
    ULONG val = ReadVOSReg(DSP_CFG_CLK);
    WriteVOSReg(DSP_CFG_CLK, (val | DSP_CLK_7109C3_VID2_HD_CLK_SEL));
    DEBUGF2(2,("CSTb7100MainOutput::ShowPlane VID2 clock set to HD pixel clock\n"));
  }

  return CSTmMasterOutput::m_pMixer->EnablePlane(planeID);
}


void CSTb7100MainOutput::EnableDACs(void)
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
  else
  {
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
      val &= ~SYS_CFG3_DAC_SD_HZV_DISABLE;
    }
    else
    {
      DEBUGF2(2,("%s: Tri-Stating CVBS DAC\n",__PRETTY_FUNCTION__));
      val |=  SYS_CFG3_DAC_SD_HZV_DISABLE;
    }

    if(m_ulOutputFormat & STM_VIDEO_OUT_YC)
    {
      DEBUGF2(2,("%s: Enabling Y/C DACs\n",__PRETTY_FUNCTION__));
      val &= ~(SYS_CFG3_DAC_SD_HZW_DISABLE |
               SYS_CFG3_DAC_SD_HZU_DISABLE);
    }
    else
    {
      DEBUGF2(2,("%s: Tri-Stating Y/C DACs\n",__PRETTY_FUNCTION__));
      val |=  (SYS_CFG3_DAC_SD_HZW_DISABLE |
               SYS_CFG3_DAC_SD_HZU_DISABLE);
    }
  }

  WriteSysCfgReg(SYS_CFG3,val);

  val = ReadVOSReg(DSP_CFG_DAC);
  if(!m_bDacHdPower)
    val |= DSP_DAC_HD_POWER_ON;

  if(m_bUsingDENC)
    val |= DSP_DAC_SD_POWER_ON;

  WriteVOSReg(DSP_CFG_DAC, val);

  DEXIT();
}


void CSTb7100MainOutput::DisableDACs(void)
{
  DENTRY();

  // Power down HD DACs
  ULONG val = ReadVOSReg(DSP_CFG_DAC) & ~DSP_DAC_HD_POWER_ON;
  WriteVOSReg(DSP_CFG_DAC, val);

  if(m_bUsingDENC)
  {
    // Power down SD DACs
    val = ReadVOSReg(DSP_CFG_DAC) & ~DSP_DAC_SD_POWER_ON;
    WriteVOSReg(DSP_CFG_DAC, val);
  }

  DEXIT();
}


//////////////////////////////////////////////////////////////////////////////
//
// SD Interlaced Output on SD Dacs
//
CSTb7100AuxOutput::CSTb7100AuxOutput(CDisplayDevice   *pDev,
                                     CSTb7100VTG2     *pVTG,
                                     CSTb7100DENC     *pDENC,
                                     CGammaMixer      *pMixer,
                                     CSTmFSynth       *pFSynth,
                                     stm_plane_id_t    sharedPlane): CSTmMasterOutput(pDev, pDENC, pVTG, pMixer, pFSynth)
{

  DEBUGF2(2,("CSTb7100AuxOutput::CSTb7100AuxOutput - m_pVTG   = %p\n", m_pVTG));
  DEBUGF2(2,("CSTb7100AuxOutput::CSTb7100AuxOutput - m_pDENC  = %p\n", m_pDENC));
  DEBUGF2(2,("CSTb7100AuxOutput::CSTb7100AuxOutput - m_pMixer = %p\n", m_pMixer));

  m_pFSynth->SetDivider(2);

  /* Select AUX compositor input and output defaults */
  m_ulInputSource = STM_AV_SOURCE_AUX_INPUT;
  CSTb7100AuxOutput::SetControl(STM_CTRL_VIDEO_OUT_SELECT,(STM_VIDEO_OUT_YC | STM_VIDEO_OUT_CVBS));

  /*
   * Start up the SD FSynth as a 27MHz clock, as this is needed for clock recovery
   * even if the auxiliary output isn't being used.
   */
  m_pFSynth->Start(13500000);

  m_sharedGDP = sharedPlane;

  m_pVTG->SetSyncType(VTG_INT_SYNC_ID,STVTG_SYNC_POSITIVE);
}


CSTb7100AuxOutput::~CSTb7100AuxOutput() {}


const stm_mode_line_t* CSTb7100AuxOutput::SupportedMode(const stm_mode_line_t *mode) const
{
  // The 7100 SD/AUX path can support SD interlaced modes only.
  if((mode->ModeParams.OutputStandards & STM_OUTPUT_STD_SD_MASK) == 0)
    return 0;

  if(mode->ModeParams.ScanType == SCAN_P)
    return 0;

  if(mode->TimingParams.ulPixelClock == 13500000 ||
     mode->TimingParams.ulPixelClock == 13513500)
    return mode;

  return 0;
}


bool CSTb7100AuxOutput::Start(const stm_mode_line_t *mode, ULONG tvStandard)
{
  DEBUGF2(2,("CSTb7100AuxOutput::Start - in\n"));

  /*
   * Test if VTG2 and DENC is in use by the main output, if so don't
   * continue.
   */
  if(!m_pCurrentMode && m_pVTG->GetCurrentMode() != 0)
  {
    DEBUGF2(1,("CSTb7100AuxOutput::Start - hardware in use by main output\n"));
    return false;
  }

  // Set the SD pixel clock to be sourced from the SD FSynth.
  ULONG val = ReadClkReg(CKGB_CLK_SRC) | CKGB_SRC_PIXSD_FS1_NOT_FS0;
  WriteClkReg(CKGB_CLK_SRC, val);

  // Set DENC routing to the AUX video input source
  val = ReadVOSReg(DSP_CFG_CLK) & ~DSP_CLK_DENC_MAIN_NOT_AUX;
  WriteVOSReg(DSP_CFG_CLK, val);

  m_pDENC->SetMainOutputFormat(m_ulOutputFormat);

  if(!CSTmMasterOutput::Start(mode, tvStandard))
  {
    DEBUGF2(2,("CSTb7100AuxOutput::Start - failed\n"));
    return false;
  }

  DEBUGF2(2,("CSTb7100AuxOutput::Start - out\n"));

  return true;
}


bool CSTb7100AuxOutput::ShowPlane(stm_plane_id_t planeID)
{
  DEBUGF2(2,("CSTb7100AuxOutput::ShowPlane %d\n",(int)planeID));

  if(planeID == m_sharedGDP)
  {
    // Change the clock source for the shared GDP to the SD/Aux video clock
    ULONG val = ReadClkReg(CKGB_CLK_SRC) | CKGB_SRC_GDPx_FS1_NOT_FS0;
    WriteClkReg(CKGB_CLK_SRC, val);
  }
  else if (planeID == OUTPUT_VID2)
  {
    /*
     * Change the clock source for VID2 when on Mixer2 to the SD clock.
     * This is only necessary for 7109Cut3, but as we are clearing an otherwise
     * reserved bit it doesn't matter if we do this on all the 710x chips.
     */
    ULONG val = ReadVOSReg(DSP_CFG_CLK) & ~DSP_CLK_7109C3_VID2_HD_CLK_SEL;
    WriteVOSReg(DSP_CFG_CLK, val);
  }

  return CSTmMasterOutput::m_pMixer->EnablePlane(planeID);
}


ULONG CSTb7100AuxOutput::GetCapabilities(void) const
{
  ULONG caps = (STM_OUTPUT_CAPS_SD_ANALOGUE       |
                STM_OUTPUT_CAPS_PLANE_MIXER       |
                STM_OUTPUT_CAPS_MODE_CHANGE       |
                STM_OUTPUT_CAPS_CVBS_YC_EXCLUSIVE);

  return caps;
}


bool CSTb7100AuxOutput::SetOutputFormat(ULONG format)
{
  DEBUGF2(2,("CSTb7100AuxOutput::SetOutputFormat - format = 0x%lx\n",format));

  switch(format)
  {
    case 0:
    case STM_VIDEO_OUT_YC:
    case STM_VIDEO_OUT_CVBS:
    case (STM_VIDEO_OUT_YC | STM_VIDEO_OUT_CVBS):
      break;
    default:
      return false;
  }

  if(m_pCurrentMode)
    m_pDENC->SetMainOutputFormat(format);

  return true;
}


void CSTb7100AuxOutput::EnableDACs(void)
{
  ULONG val;

  DENTRY();

  val = ReadSysCfgReg(SYS_CFG3);

  if(m_ulOutputFormat & STM_VIDEO_OUT_CVBS)
  {
    DEBUGF2(2,("%s: Enabling CVBS DAC\n",__PRETTY_FUNCTION__));
    val &= ~SYS_CFG3_DAC_SD_HZV_DISABLE;
  }
  else
  {
    DEBUGF2(2,("%s: Tri-Stating CVBS DAC\n",__PRETTY_FUNCTION__));
    val |=  SYS_CFG3_DAC_SD_HZV_DISABLE;
  }

  if(m_ulOutputFormat & STM_VIDEO_OUT_YC)
  {
    DEBUGF2(2,("%s: Enabling Y/C DACs\n",__PRETTY_FUNCTION__));
    val &= ~(SYS_CFG3_DAC_SD_HZW_DISABLE |
             SYS_CFG3_DAC_SD_HZU_DISABLE);
  }
  else
  {
    DEBUGF2(2,("%s: Tri-Stating Y/C DACs\n",__PRETTY_FUNCTION__));
    val |=  (SYS_CFG3_DAC_SD_HZW_DISABLE |
             SYS_CFG3_DAC_SD_HZU_DISABLE);
  }

  WriteSysCfgReg(SYS_CFG3,val);

  val = ReadVOSReg(DSP_CFG_DAC) | DSP_DAC_SD_POWER_ON;
  WriteVOSReg(DSP_CFG_DAC, val);

  DEXIT();
}


void CSTb7100AuxOutput::DisableDACs(void)
{
  DENTRY();

  ULONG val = ReadVOSReg(DSP_CFG_DAC) & ~DSP_DAC_SD_POWER_ON;
  WriteVOSReg(DSP_CFG_DAC, val);

  DEXIT();
}
