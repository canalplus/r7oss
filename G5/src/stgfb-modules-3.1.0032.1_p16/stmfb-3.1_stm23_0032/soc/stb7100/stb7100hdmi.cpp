/***********************************************************************
 *
 * File: soc/stb7100/stb7100hdmi.cpp
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
#include <STMCommon/stmhdmiregs.h>
#include <STMCommon/stmiframemanager.h>
#include <STMCommon/stmdmaiframes.h>

#include "stb7100reg.h"
#include "stb7100device.h"
#include "stb7100analogueouts.h"
#include "stb7100vtg.h"
#include "stb7100hdmi.h"


/*
 * DLL Rejection PLL setup
 */
#define SYS_CFG3_PLL_S_HDMI_PDIV_DEFAULT (2L   << SYS_CFG3_PLL_S_HDMI_PDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_NDIV_DEFAULT (0x64 << SYS_CFG3_PLL_S_HDMI_NDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_MDIV_DEFAULT (0x32 << SYS_CFG3_PLL_S_HDMI_MDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_MDIV_2XREF   (0x19 << SYS_CFG3_PLL_S_HDMI_MDIV_SHIFT)

/*
 * Some offsets into the audio hardware so we can determine the HDMI audio
 * setup without making a dependency between the display and audio drivers.
 */
#define AUD_FSYN_CFG   0x0
#define AUD_FSYN0_MD   0x10
#define AUD_FSYN0_PE   0x14
#define AUD_FSYN0_SDIV 0x18
#define AUD_FSYN0_EN   0x1C
#define AUD_FSYN1_MD   0x20
#define AUD_FSYN1_PE   0x24
#define AUD_FSYN1_SDIV 0x28
#define AUD_FSYN1_EN   0x2C
#define AUD_FSYN2_MD   0x30
#define AUD_FSYN2_PE   0x34
#define AUD_FSYN2_SDIV 0x38
#define AUD_FSYN2_EN   0x3C
#define AUD_IO_CTRL    0x200
#define AUD_HDMI_MUX   0x204

#define AUD_FSYN_CFG_SPDIF_EN (1L<<8)

#define MD_48KHZ_256FS   0xF3
#define PE_48KHZ_256FS   0x3C00
#define SDIV_48KHZ_256FS 0x4

#define AUD_SPDIF_CTRL   0x1C
#define AUD_PRCONV_CTRL  0x200

#define AUD_SPDIF_CTRL_MODE_MASK    0x7
#define AUD_SPDIF_CTRL_CLKDIV_SHIFT 5
#define AUD_SPDIF_CTRL_CLKDIV_MASK  (0xFF << AUD_SPDIF_CTRL_CLKDIV_SHIFT)

#define STM_VTG1_ITS 0x2c
#define STM_VTG_ITS_VSB (1L<<1)
#define STM_VTG_ITS_VST (1L<<2)
#define STM_HDMI_CTRL2 (-0x400 + 0x04)
#define STM_HDMI_CTRL2_AUTH    (1 << 1 | 1 << 0)
#define STM_HDMI_CTRL2_AV_MUTE (1 << 2)

#define GNBvd47682_USE_VSYNC_WORKAROUND 0
#define GNBvd47682_USE_TIMED_UNMUTE_WORKAROUND 1

enum
{
  GNBvd47682_WORKAROUND_AFTER_VSYNC,
  GNBvd47682_WORKAROUND_BEFORE_VSYNC
};

////////////////////////////////////////////////////////////////////////////////
//

CSTb710xHDMI::CSTb710xHDMI(CDisplayDevice     *pDev,
                           CSTb7100MainOutput *pMainOutput,
                           CSTb7100VTG1       *pVTG): CSTmHDMI(pDev, STM_HDMI_HW_V_1_2, STb7100_HDMI_BASE, pMainOutput, pVTG)
{
  ULONG val;

  DEBUGF2(2,("CSTb710xHDMI::CSTb710xHDMI pDev = %p  main output = %p\n",pDev,pMainOutput));
  m_pVTG1          = pVTG;

  m_ulVOSOffset    = STb7100_VOS_BASE;
  m_ulSysCfgOffset = STb7100_SYSCFG_BASE;
  m_ulClkGBOffset  = STb7100_CLKGEN_BASE;
  m_ulAudioOffset  = STb7100_AUDIO_BASE;
  m_ulPRCONVOffset = STb7100_PRCONV_BASE;
  m_ulSPDIFOffset  = STb7100_SPDIF_BASE;

  /*
   * Ensure  TMDS and HDMI PIX clocks are on
   */
  val = ReadClkReg(CKGB_CLK_EN);
  WriteClkReg(CKGB_CLK_EN, (val | CKGB_EN_TMDS_HDMI | CKGB_EN_PIX_HDMI));

  /*
   * HDMI Serializer must be powered down to meet the leakage
   * current constraints of HDMI certification. However this may be too
   * late, given the time from switching on a box to initialising the
   * display driver, hence this may need to be setup in any bootloader.
   */
  val = ReadVOSReg(DSP_CFG_CLK) | DSP_CLK_HDMI_OFF_NOT_ON;
  WriteVOSReg(DSP_CFG_CLK, val);

  val = ReadAUDReg(AUD_FSYN_CFG);
  if((val & AUD_FSYN_CFG_SPDIF_EN) == 0)
  {
    /*
     * We don't appear to have started any audio clocks,
     * so set some up, without an SPDIF clock the HDMI
     * will not correctly reset. Of cource if we have clocks
     * already, don't change them!
     */
    DEBUGF2(2,("CSTb710xHDMI::CSTb710xHDMI - Setting up default SPDIF clock, aud cfg = %lx\n",val));
    WriteAUDReg(AUD_FSYN_CFG, 0x1);        /* Reset                          */
    g_pIOS->StallExecution(10);
    WriteAUDReg(AUD_FSYN_CFG, 0x00005ddc); /* Enable and power everything up */

    WriteAUDReg(AUD_FSYN0_EN,   0);
    WriteAUDReg(AUD_FSYN0_SDIV, SDIV_48KHZ_256FS);
    WriteAUDReg(AUD_FSYN0_MD,   MD_48KHZ_256FS);
    WriteAUDReg(AUD_FSYN0_PE,   PE_48KHZ_256FS);
    WriteAUDReg(AUD_FSYN0_EN,   1);

    WriteAUDReg(AUD_FSYN1_EN,   0);
    WriteAUDReg(AUD_FSYN1_SDIV, SDIV_48KHZ_256FS);
    WriteAUDReg(AUD_FSYN1_MD,   MD_48KHZ_256FS);
    WriteAUDReg(AUD_FSYN1_PE,   PE_48KHZ_256FS);
    WriteAUDReg(AUD_FSYN1_EN,   1);

    WriteAUDReg(AUD_FSYN2_EN,   0);
    WriteAUDReg(AUD_FSYN2_SDIV, SDIV_48KHZ_256FS);
    WriteAUDReg(AUD_FSYN2_MD,   MD_48KHZ_256FS);
    WriteAUDReg(AUD_FSYN2_PE,   PE_48KHZ_256FS);
    WriteAUDReg(AUD_FSYN2_EN,   1);

    WriteAUDReg(AUD_IO_CTRL, 7);  /* All signals to be outputs */
  }


  m_bUse2xRefForSDRejectionPLL = false;

  DEBUGF2(2,("CSTb710xHDMI::CSTb710xHDMI - out\n"));
}


CSTb710xHDMI::~CSTb710xHDMI()
{
}


void CSTb710xHDMI::SetupRejectionPLL(const stm_mode_line_t *mode,
                                     ULONG                  tvStandard)
{
  ULONG val;

  /*
   * The main HDMI clocks have been setup when the main output video mode
   * was started. However there is an extra PLL, fed by the main output
   * FSynth which needs to be enabled. This unfortunately lives in the sys
   * config registers, not the clockgen B registers. The startup sequence is
   * quite complex....
   *
   * Note the excessive use of delays in the code, this is to match the
   * ST reference drivers and to workaround a reliability issue with the
   * sequence which aflicts some chips.
   *
   * First Power down the BCH PLL
   */
  val  = ReadSysCfgReg(SYS_CFG3);
  val &= ~(SYS_CFG3_PLL_S_HDMI_EN | SYS_CFG3_S_HDMI_RST_N);
  WriteSysCfgReg(SYS_CFG3, val);

  DEBUGF2(2,("CSTb710xHDMI::SetupRejectionPLL - waiting for BCH powerdown\n"));

  do
  {
    g_pIOS->StallExecution(1000);
  } while((ReadVOSReg(HDMI_PHY_LCK_STA) & (HDMI_PHY_RPLL_LCK|HDMI_PHY_DLL_LCK)) != 0);

  g_pIOS->StallExecution(1000);

  DEBUGF2(2,("CSTb710xHDMI::SetupRejectionPLL - got BCH powerdown\n"));

  /*
   * Power up the BCH PLL
   */
  val = ReadSysCfgReg(SYS_CFG3) & ~(SYS_CFG3_PLL_S_HDMI_PDIV_MASK |
                                    SYS_CFG3_PLL_S_HDMI_NDIV_MASK |
                                    SYS_CFG3_PLL_S_HDMI_MDIV_MASK);

  val |= (SYS_CFG3_PLL_S_HDMI_PDIV_DEFAULT |
          SYS_CFG3_PLL_S_HDMI_NDIV_DEFAULT |
          SYS_CFG3_PLL_S_HDMI_EN);

  if((mode->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK) != 0 || !m_bUse2xRefForSDRejectionPLL)
  {
    DEBUGF2(2,("CSTb710xHDMI::SetupRejectionPLL - Default Rejection PLL for 1xRef Clock\n"));
    val |= SYS_CFG3_PLL_S_HDMI_MDIV_DEFAULT;
  }
  else
  {
    DEBUGF2(2,("CSTb710xHDMI::SetupRejectionPLL - Rejection PLL for 2xRef Clock\n"));
    val |= SYS_CFG3_PLL_S_HDMI_MDIV_2XREF;
  }

  WriteSysCfgReg(SYS_CFG3, val);

  DEBUGF2(2,("CSTb710xHDMI::SetupRejectionPLL - waiting for rejection PLL Lock\n"));

  do
  {
    g_pIOS->StallExecution(1000);
  } while((ReadVOSReg(HDMI_PHY_LCK_STA) & HDMI_PHY_RPLL_LCK) == 0);

  g_pIOS->StallExecution(1000);

  DEBUGF2(2,("CSTb710xHDMI::SetupRejectionPLL - got rejection PLL Lock\n"));

  /* The HW documentation says we should bring the serializer out of reset
     here and now, but if we do that at this point, something seems to be
     going wrong sometimes, resulting in signals which can not be displayed by
     the connected sink. Relatively easy to observe when switching between VGA
     and 720p50 a couple of times.
     Experimentation has shown that moving the code to bring the serializer
     out of reset into the PostConfiguration() call solves these issues with
     no adverse effect. */
}


bool CSTb710xHDMI::PreConfiguration(const stm_mode_line_t *mode,
                                    ULONG                  tvStandard)
{
  ULONG val;

  DEBUGF2(2,("CSTb710xHDMI::PreConfiguration\n"));

  /*
   * Turn on HDMI in the VOS
   */
  val = ReadVOSReg(DSP_CFG_CLK) & ~DSP_CLK_HDMI_OFF_NOT_ON;
  WriteVOSReg(DSP_CFG_CLK, val);

  SetupRejectionPLL(mode,tvStandard);

  return true;
}


bool CSTb710xHDMI::PostConfiguration(const stm_mode_line_t *mode,
                                     ULONG                  tvStandard)
{
  DEBUGF2(2,("CSTb710xHDMI::PostConfiguration\n"));

  /* Bring the serializer out of reset. See comment above at end of
     SetupRejectionPLL() about why this is done here. */
  ULONG val = ReadSysCfgReg(SYS_CFG3) | SYS_CFG3_S_HDMI_RST_N;
  g_pIOS->StallExecution(1000);
  WriteSysCfgReg(SYS_CFG3, val);

  DEBUGF2(2,("%s - waiting for HDMI DLL Lock\n", __PRETTY_FUNCTION__));
  do
  {
    g_pIOS->StallExecution(1000);
  } while((ReadVOSReg(HDMI_PHY_LCK_STA) & HDMI_PHY_DLL_LCK) == 0);
  DEBUGF2(2,("%s - got HDMI DLL Lock\n", __PRETTY_FUNCTION__));

  /*
   * Sync delay calibrated using "just scan" mode on an LG325700 so that
   * all edge pixels were visible.
   */
  m_pVTG1->SetHDMIHSyncDelay(4);

  DEBUGF2(2,("CSTb710xHDMI::PostConfiguration - out\n"));

  return true;
}


bool CSTb710xHDMI::Stop(void)
{
  ULONG val;

  DEBUGF2(2,("CSTb710xHDMI::Stop\n"));

  CSTmHDMI::Stop();

  val = ReadSysCfgReg(SYS_CFG3);
  val &= ~(SYS_CFG3_PLL_S_HDMI_EN | SYS_CFG3_S_HDMI_RST_N);
  WriteSysCfgReg(SYS_CFG3, val);

  val = ReadVOSReg(DSP_CFG_CLK) | DSP_CLK_HDMI_OFF_NOT_ON;
  WriteVOSReg(DSP_CFG_CLK, val);

  DEBUGF2(2,("CSTb710xHDMI::Stop - out\n"));

  return true;
}


int CSTb710xHDMI::GetAudioFrequency(void)
{
  ULONG sdiv;
  long  md; /* Note this is signed */
  ULONG pe;

  if(m_ulInputSource & STM_AV_SOURCE_SPDIF_INPUT)
  {
    sdiv = ReadAUDReg(AUD_FSYN2_SDIV);
    md   = ReadAUDReg(AUD_FSYN2_MD);
    pe   = ReadAUDReg(AUD_FSYN2_PE);
  }
  else
  {
    sdiv = ReadAUDReg(AUD_FSYN0_SDIV);
    md   = ReadAUDReg(AUD_FSYN0_MD);
    pe   = ReadAUDReg(AUD_FSYN0_PE);
  }

  /*
   * The 710x uses a 30MHz reference clock to the audio FSynths,
   * the same clock as for video.
   */
  return stm_fsynth_frequency(STM_CLOCK_REF_30MHZ, sdiv, md, pe);
}


void CSTb710xHDMI::GetAudioHWState(stm_hdmi_audio_state_t *state)
{
  ULONG audctrl;

  /*
   * First update the audio info frame state, the HDMI can take its audio input
   * from either the SPDIF player or PCM Player 0 via an I2S to SPDIF converter.
   */
  if(m_ulInputSource & STM_AV_SOURCE_SPDIF_INPUT)
    audctrl = ReadSPDIFReg(AUD_SPDIF_CTRL);
  else
    audctrl = ReadPRCONVReg(AUD_PRCONV_CTRL);

  state->status = ((audctrl & AUD_SPDIF_CTRL_MODE_MASK) != 0)?STM_HDMI_AUDIO_RUNNING:STM_HDMI_AUDIO_STOPPED;
  state->clock_divide = (audctrl & AUD_SPDIF_CTRL_CLKDIV_MASK) >> AUD_SPDIF_CTRL_CLKDIV_SHIFT;
}


ULONG CSTb710xHDMI::SupportedControls(void) const
{
  ULONG caps = CSTmHDMI::SupportedControls();

  caps |= STM_CTRL_CAPS_AV_SOURCE_SELECT;

  return caps;
}


void CSTb710xHDMI::SetControl(stm_output_control_t ctrl, ULONG ulNewVal)
{
  DEBUGF2(2,("CSTb710xHDMI::SetControl: 0x%.8x/%lu\n", ctrl, ulNewVal));

  switch (ctrl)
  {
    case STM_CTRL_AV_SOURCE_SELECT:
      if(SetInputSource(ulNewVal))
        m_ulInputSource = ulNewVal;
      break;
    default:
      CSTmHDMI::SetControl(ctrl, ulNewVal);
      break;
  }
}


ULONG CSTb710xHDMI::GetControl(stm_output_control_t ctrl) const
{
  DEBUGF2(2,("CSTb710xHDMI::GetControl\n"));

  switch (ctrl)
  {
    case STM_CTRL_AV_SOURCE_SELECT:
      return m_ulInputSource;
    default:
      return CSTmHDMI::GetControl(ctrl);
  }

  return 0;
}


bool CSTb710xHDMI::SetOutputFormat(ULONG format)
{
  ULONG val = ReadVOSReg(DSP_CFG_CLK);

  DEBUGF2(2,("CSTb710xHDMI::SetOutputFormat\n"));

  switch (format & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422))
  {
    case STM_VIDEO_OUT_RGB:
      DEBUGF2(2,("CSTb710xHDMI::SetOutputFormat - Set RGB Output\n"));
      val &= ~DSP_CLK_HDMI_YCBCR_NOT_RGB_IN;
      break;
    case STM_VIDEO_OUT_422:
    case STM_VIDEO_OUT_YUV:
      DEBUGF2(2,("CSTb710xHDMI::SetOutputFormat - Set YUV Output\n"));
      val |= DSP_CLK_HDMI_YCBCR_NOT_RGB_IN;
      break;
    default:
      return false;
  }

  WriteVOSReg(DSP_CFG_CLK, val);

  return CSTmHDMI::SetOutputFormat(format);
}


bool CSTb710xHDMI::SetInputSource(ULONG src)
{
  DEBUGF2(2,("CSTb710xHDMI::SetInputSource\n"));

  /*
   * HDMI is only fed from the main video path.
   */
  if(!(src & STM_AV_SOURCE_MAIN_INPUT))
    return false;

  /*
   * No multichannel audio on 7100/7109
   */
  if(src & STM_AV_SOURCE_8CH_I2S_INPUT)
    return false;

  /*
   * On the STb7100 HDMI we can switch the audio input to be either the output
   * of the S/PDIF player or the first I2S channel of PCMPlayer0 via the
   * I2S->SPDIF Protocol converter. Note we only reset the hardware if the
   * input wasn't the new option OR has not yet been set up. This causes
   * the audio running state to be false causing the UpdateHW handler
   * to re-setup the HDMI audio state.
   */
  switch(src & (STM_AV_SOURCE_SPDIF_INPUT | STM_AV_SOURCE_2CH_I2S_INPUT))
  {
    case STM_AV_SOURCE_SPDIF_INPUT:
      if(!(m_ulInputSource & STM_AV_SOURCE_SPDIF_INPUT))
      {
        InvalidateAudioPackets();
        WriteAUDReg(AUD_HDMI_MUX, 1);
        m_audioState.status = STM_HDMI_AUDIO_STOPPED;
        DEBUGF2(2,("CSTb710xHDMI::SetInputSource - Set SPDIF Input\n"));
      }
      break;
    case STM_AV_SOURCE_2CH_I2S_INPUT:
      if(!(m_ulInputSource & STM_AV_SOURCE_2CH_I2S_INPUT))
      {
        InvalidateAudioPackets();
        WriteAUDReg(AUD_HDMI_MUX, 0);
        m_audioState.status = STM_HDMI_AUDIO_STOPPED;
        DEBUGF2(2,("CSTb710xHDMI::SetInputSource - Set I2S Input\n"));
      }
      break;
    default:
      InvalidateAudioPackets();
      WriteAUDReg(AUD_HDMI_MUX, 0);
      m_audioState.status = STM_HDMI_AUDIO_DISABLED;
      DEBUGF2(2,("CSTb710xHDMI::SetInputSource - Audio Disabled\n"));
      break;
  }

  m_ulInputSource = src;
  return true;
}


bool CSTb710xHDMI::PreAuth() const
{
  if (GNBvd47682_USE_VSYNC_WORKAROUND)
    if (ReadHDMIReg(STM_HDMI_CFG) & STM_HDMI_CFG_CP_EN) {
      m_pVTG1->DisableSyncs();
      return true;
    } else {
      return false;
    }

  return true;
}


static void __CSTb710xHDMI_HandleCallback(void *p, int opcode)
{
  CSTb710xHDMI *pHDMI = static_cast<CSTb710xHDMI *>(p);
  pHDMI->HandleCallback(opcode);
}


void CSTb710xHDMI::PostAuth(bool auth)
{
  if (auth) {
    if (GNBvd47682_USE_TIMED_UNMUTE_WORKAROUND)
    {
      // get the VTG to callback almost immediately after Vsync
      m_pVTG1->ArmVsyncTimer(1, __CSTb710xHDMI_HandleCallback, this, GNBvd47682_WORKAROUND_AFTER_VSYNC);
    }
    else
    {
      // perform the unmute directly
      ULONG ctrl2 = ReadHDMIReg(STM_HDMI_CTRL2);
      ctrl2 |= STM_HDMI_CTRL2_AUTH;
      ctrl2 &= ~STM_HDMI_CTRL2_AV_MUTE;
      WriteHDMIReg(STM_HDMI_CTRL2, ctrl2);
    }
  }

  if (GNBvd47682_USE_VSYNC_WORKAROUND) {
    m_pVTG1->RestoreSyncs();
  }
}


void CSTb710xHDMI::HandleCallback(int opcode)
{
  if (GNBvd47682_USE_TIMED_UNMUTE_WORKAROUND)
  {
    switch (opcode)
    {
    case GNBvd47682_WORKAROUND_AFTER_VSYNC:
      {
	ULONG ctrl2 = ReadHDMIReg(STM_HDMI_CTRL2);
	ctrl2 |= STM_HDMI_CTRL2_AUTH | STM_HDMI_CTRL2_AV_MUTE;
	WriteHDMIReg(STM_HDMI_CTRL2, ctrl2);

	// get the VTG to callback 5 lines before the next Vsync
	const stm_mode_line_t *mode = m_pVTG->GetCurrentMode();
	ULONG delay = mode->TimingParams.PixelsPerLine * (mode->TimingParams.LinesByFrame - 5) /
		      (mode->ModeParams.ScanType == SCAN_I ? 2 : 1);
	m_pVTG1->ArmVsyncTimer(delay, __CSTb710xHDMI_HandleCallback, this, GNBvd47682_WORKAROUND_BEFORE_VSYNC);
	break;
      }

    case GNBvd47682_WORKAROUND_BEFORE_VSYNC:
      {
	// pre-calculate the value to be written to CTRL2, wait for Vsync then do it
	ULONG ctrl2 = ReadHDMIReg(STM_HDMI_CTRL2) & ~STM_HDMI_CTRL2_AV_MUTE;
	m_pVTG1->WaitForVsync();
	WriteHDMIReg(STM_HDMI_CTRL2, ctrl2);
	break;
      }
    }
  }
}

#if GNBvd47682_USE_VSYNC_WORKAROUND && GNBvd47682_USE_TIMED_UNMUTE_WORKAROUND
#error Cannot deploy both GNBvd47682 workarounds at the same time!!!
#endif



///////////////////////////////////////////////////////////////////////////////
//
//
CSTb7100HDMI::CSTb7100HDMI(CDisplayDevice     *pDev,
                           CSTb7100MainOutput *pMainOutput,
                           CSTb7100VTG1       *pVTG): CSTb710xHDMI(pDev, pMainOutput, pVTG)
{
  DEBUGF2(2,("CSTb7100HDMI::CSTb7100HDMI pDev = %p  main output = %p\n",pDev,pMainOutput));

  m_bInitedTwice = false;

  /*
   * On 7100 the hotplug interrupt is not connected to the HDMI block, we have
   * to poll a PIO instead.
   */
  m_bUseHotplugInt = false;

  /*
   * Change the rejection PLL setup for SD/ED modes from the standard,
   * to cope with a 54MHz, instead of a 108MHz reference clock.
   */
  m_bUse2xRefForSDRejectionPLL = true;

  m_ulInputSource = STM_AV_SOURCE_MAIN_INPUT | STM_AV_SOURCE_2CH_I2S_INPUT;
  CSTb7100HDMI::SetInputSource(m_ulInputSource);
}


CSTb7100HDMI::~CSTb7100HDMI()
{
}


bool CSTb7100HDMI::PreConfiguration(const stm_mode_line_t *mode,
                                    ULONG                  tvStandard)
{
  bool success;

  DENTRY();

  if ((success = CSTb710xHDMI::PreConfiguration(mode,tvStandard)) == true)
  {
    if (UNLIKELY (!m_bInitedTwice))
    {
      /* This appears to be wrong, but it isn't quite. This is to work
         around an apparent issue with certain chips, that do not correctly
         initialise the rejection PLL the first after power on. Doing it
         twice just makes these chips work. Ho Hum.... */
      SetupRejectionPLL(mode,tvStandard);
      m_bInitedTwice = true;
    }
  }

  DEXIT();

  return success;
}

///////////////////////////////////////////////////////////////////////////////
//
//

/*
 * New 7109Cut3 HDMI register
 */
#define STM_SYNC_HOTPLUG_EN       (1L<<0)
#define STM_SYNC_HDMI_HSYNC_DIS   (1L<<1)
#define STM_SYNC_HDMI_VSYNC_DIS   (1L<<2)
#define STM_SYNC_HDCP_HSYNC_DIS   (1L<<3)
#define STM_SYNC_HDCP_VSYNC_DIS   (1L<<4)
#define STM_SYNC_HDMI_HSYNC_INV   (1L<<5)
#define STM_SYNC_HDMI_VSYNC_INV   (1L<<6)
#define STM_SYNC_HDCP_HSYNC_INV   (1L<<7)
#define STM_SYNC_HDCP_VSYNC_INV   (1L<<8)


CSTb7109Cut3HDMI::CSTb7109Cut3HDMI(CDisplayDevice     *pDev,
                                   CSTb7100MainOutput *pMainOutput,
                                   CSTb7100VTG1       *pVTG): CSTb710xHDMI(pDev, pMainOutput, pVTG)
{
  DEBUGF2(2,("CSTb7109Cut3HDMI::CSTb7109Cut3HDMI pDev = %p  main output = %p\n",pDev,pMainOutput));

  /*
   * Make sure all syncs are on and not inverted.
   */
  WriteHDMIReg(STM_HDMI_SYNC_CFG, 0);

  m_ulInputSource = STM_AV_SOURCE_MAIN_INPUT | STM_AV_SOURCE_2CH_I2S_INPUT;
  CSTb7109Cut3HDMI::SetInputSource(m_ulInputSource);

  /*
   * As we have no boards with the hotplug line connected to the correct
   * PIO pin to use the hotplug interrupt yet, default to it being off
   * and poll PIO2 pin 2 instead.
   */
  CSTb7109Cut3HDMI::SetControl(STM_CTRL_HDMI_USE_HOTPLUG_INTERRUPT, 0);
}


CSTb7109Cut3HDMI::~CSTb7109Cut3HDMI()
{
}


bool CSTb7109Cut3HDMI::Create(void)
{
  DENTRY();

  /*
   * Try and create an FDMA driver InfoFrame manager before we call the
   * base class Create().
   */
  m_pIFrameManager = new CSTmDMAIFrames(m_pDisplayDevice, m_ulHDMIOffset, STb7100_REGISTER_BASE);
  if(!m_pIFrameManager || !m_pIFrameManager->Create(this,m_pMasterOutput))
  {
    DEBUGF2(2,("Unable to create a DMA based Info Frame manager\n"));
    /*
     * Reset m_pIFrameManager so the base class will create the CPU driven
     * version instead.
     */
    delete m_pIFrameManager;
    m_pIFrameManager = 0;
  }

  if(!CSTmHDMI::Create())
    return false;

  DEXIT();
  return true;
}


void CSTb7109Cut3HDMI::SetControl(stm_output_control_t ctrl, ULONG ulNewVal)
{
  switch (ctrl)
  {
    case STM_CTRL_HDMI_USE_HOTPLUG_INTERRUPT:
    {
      m_bUseHotplugInt = (ulNewVal != 0);

      ULONG val = ReadHDMIReg(STM_HDMI_SYNC_CFG);
      if(m_bUseHotplugInt)
        val |= STM_SYNC_HOTPLUG_EN;
      else
        val &= ~STM_SYNC_HOTPLUG_EN;

      WriteHDMIReg(STM_HDMI_SYNC_CFG, val);
      break;
    }
    default:
    {
      CSTb710xHDMI::SetControl(ctrl, ulNewVal);
      break;
    }
  }
}


bool CSTb7109Cut3HDMI::PreConfiguration(const stm_mode_line_t *mode,
                                        ULONG                  tvStandard)
{
  DEBUGF2(2,("CSTb7109Cut3HDMI::PreConfiguration\n"));

  /*
   * For 7109C3 we need to set the new BCH clock divider for SD/ED or HD
   * modes so we can use a 108MHz reference clock.
   */
  if((mode->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK) != 0)
  {
    DEBUGF2(2,("CSTb7109Cut3HDMI::PreConfiguration HD BCH = ref clock\n"));
    WriteSysCfgReg(SYS_CFG34, 0);
  }
  else
  {
    DEBUGF2(2,("CSTb7109Cut3HDMI::PreConfiguration SD BCH = ref clock/2\n"));
    WriteSysCfgReg(SYS_CFG34, SYS_CFG34_CLK_BCH_HDMI_DIVSEL);
  }

  return CSTb710xHDMI::PreConfiguration(mode,tvStandard);
}
