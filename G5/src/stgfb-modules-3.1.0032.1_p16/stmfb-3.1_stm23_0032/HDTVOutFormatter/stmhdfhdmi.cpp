/***********************************************************************
 *
 * File: HDTVOutFormatter/stmhdfhdmi.cpp
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
#include <STMCommon/stmhdmiregs.h>
#include <STMCommon/stmiframemanager.h>

#include "stmhdfhdmi.h"

#define STM_HDMI_CTRL2         (0x800 + 0x04)
#define STM_HDMI_CTRL2_AUTH    (1 << 1)
#define STM_HDMI_CTRL2_AV_MUTE (1 << 2)

#define HDFMT_DIG1_CFG      0x100

#define HDFMT_CFG_INPUT_YCBCR      (0L)
#define HDFMT_CFG_INPUT_RGB        (1L)
#define HDFMT_CFG_INPUT_AUX        (2L)
#define HDFMT_CFG_INPUT_MASK       (3L)
#define HDFMT_CFG_RCTRL_G          (0L)
#define HDFMT_CFG_RCTRL_B          (1L)
#define HDFMT_CFG_RCTRL_R          (2L)
#define HDFMT_CFG_RCTRL_MASK       (3L)
#define HDFMT_CFG_REORDER_RSHIFT   (2L)
#define HDFMT_CFG_REORDER_GSHIFT   (4L)
#define HDFMT_CFG_REORDER_BSHIFT   (6L)
#define HDFMT_CFG_CLIP_EN          (1L<<8)
#define HDFMT_CFG_CLIP_GB_N_CRCB   (1L<<9)
#define HDFMT_CFG_CLIP_SAV_EAV     (1L<<10)

////////////////////////////////////////////////////////////////////////////////
//
CSTmHDFormatterHDMI::CSTmHDFormatterHDMI(CDisplayDevice *pDev,
                         stm_hdmi_hardware_version_t     hdmiHWVer,
                         stm_hdf_hdmi_hardware_version_t revision,
                                         ULONG           ulHDMIOffset,
                                         ULONG           ulFormatterOffset,
                                         ULONG           ulPCMPOffset,
                                         ULONG           ulSPDIFOffset,
                                         COutput        *pMasterOutput,
                                         CSTmVTG        *pVTG): CSTmHDMI(pDev, hdmiHWVer, ulHDMIOffset, pMasterOutput, pVTG)
{
  DENTRY();

  m_ulFmtOffset    = ulFormatterOffset;
  m_ulPCMPOffset   = ulPCMPOffset;
  m_ulSPDIFOffset  = ulSPDIFOffset;
  m_Revision       = revision;

  m_bUseHotplugInt = true;

  DEXIT();
}


CSTmHDFormatterHDMI::~CSTmHDFormatterHDMI() {}


bool CSTmHDFormatterHDMI::Create(void)
{
  ULONG val;

  DENTRY();

  if(!CSTmHDMI::Create())
    return false;

  /*
   * Default digital configuration
   */
  val = (HDFMT_CFG_RCTRL_R << HDFMT_CFG_REORDER_RSHIFT) |
        (HDFMT_CFG_RCTRL_G << HDFMT_CFG_REORDER_GSHIFT) |
        (HDFMT_CFG_RCTRL_B << HDFMT_CFG_REORDER_BSHIFT) |
         HDFMT_CFG_CLIP_SAV_EAV;

  WriteHDFmtReg(HDFMT_DIG1_CFG, val);

  /*
   * Default HDFormatter/HDMI glue configuration
   */
  val = ReadHDMIReg(STM_HDMI_GPOUT);

  switch(m_Revision)
  {
    case STM_HDF_HDMI_REV1:
    {
      val &= ~(STM_HDMI_GPOUT_EXTERNAL_PCMPLAYER_EN   |
               STM_HDMI_GPOUT_DTS_FORMAT_EN           |
               STM_HDMI_GPOUT_CTS_CLK_DIV_BYPASS      |
               STM_HDMI_GPOUT_DTS_SPDIF_MODE_EN);

      val |= (STM_HDMI_GPOUT_I2S_EN           |
              STM_HDMI_GPOUT_SPDIF_EN         |
              STM_HDMI_GPOUT_I2S_N_SPDIF_DATA |
              0);
      break;
    }
    case STM_HDF_HDMI_REV2:
    {
      val &= ~(STM_HDMI_GPOUT_EXTERNAL_PCMPLAYER_EN   |
               STM_HDMI_GPOUT_DTS_SPDIF_MODE_EN       |
               STM_HDMI_GPOUT_DROP_CB_N_CR            |
               STM_HDMI_GPOUT_DROP_EVEN_N_ODD_SAMPLES |
               STM_HDMI_GPOUT_IN_MASK);

      val |= (STM_HDMI_GPOUT_I2S_EN            |
              STM_HDMI_GPOUT_SPDIF_EN          |
              STM_HDMI_GPOUT_I2S_N_SPDIF_DATA  |
              STM_HDMI_GPOUT_I2S_N_SPDIF_CLOCK |
              STM_HDMI_GPOUT_IN_HDFORMATTER    |
              0);
      break;
    }
    case STM_HDF_HDMI_REV3:
    {
      val &= ~STM_HDMI_GPOUT_EXTERNAL_PCMPLAYER_EN;

      val |= (STM_HDMI_GPOUT_I2S_EN            |
              STM_HDMI_GPOUT_SPDIF_EN          |
              STM_HDMI_GPOUT_I2S_N_SPDIF_DATA  |
              STM_HDMI_GPOUT_I2S_N_SPDIF_CLOCK |
              0);
      break;
    }
  }

  WriteHDMIReg(STM_HDMI_GPOUT, val);

  SetInputSource((STM_AV_SOURCE_MAIN_INPUT | STM_AV_SOURCE_2CH_I2S_INPUT));

  DEXIT();
  return true;
}


bool CSTmHDFormatterHDMI::PostConfiguration(const stm_mode_line_t *mode,
                                            ULONG                  tvStandard)
{
  DENTRY();

  DEXIT();

  return true;
}


ULONG CSTmHDFormatterHDMI::SupportedControls(void) const
{
  ULONG caps = CSTmHDMI::SupportedControls();

  caps |= STM_CTRL_CAPS_AV_SOURCE_SELECT;

  return caps;
}


void CSTmHDFormatterHDMI::SetControl(stm_output_control_t ctrl, ULONG ulNewVal)
{
  DEBUGF2(3,("CSTmHDFormatterHDMI::SetControl\n"));

  switch (ctrl)
  {
    case STM_CTRL_AV_SOURCE_SELECT:
    {
      SetInputSource(ulNewVal);
      break;
    }
    case STM_CTRL_SIGNAL_RANGE:
    {
      if(ulNewVal> STM_SIGNAL_VIDEO_RANGE)
        break;

      m_signalRange = (stm_display_signal_range_t)ulNewVal;

      ULONG val = ReadHDFmtReg(HDFMT_DIG1_CFG) & ~(HDFMT_CFG_CLIP_SAV_EAV |
                                                   HDFMT_CFG_CLIP_EN);

      switch(m_signalRange)
      {
        case STM_SIGNAL_FILTER_SAV_EAV:
          DEBUGF2(2,("CSTmHDFormatterHDMI::SetControl - Filter SAV/EAV\n"));
          val |= HDFMT_CFG_CLIP_SAV_EAV;
          break;
        case STM_SIGNAL_VIDEO_RANGE:
          DEBUGF2(2,("CSTmHDFormatterHDMI::SetControl - Clip input to video range\n"));
          val |= HDFMT_CFG_CLIP_EN;
          break;
        default:
          DEBUGF2(2,("CSTmHDFormatterHDMI::SetControl - Full signal range\n"));
          break;
      }

      WriteHDFmtReg(HDFMT_DIG1_CFG, val);
      m_pIFrameManager->ForceAVIUpdate();

      break;
    }
    case STM_CTRL_HDMI_AUDIO_OUT_SELECT:
    {
      /*
       * We are not supporting anything other than basic audio on Rev1, i.e.
       * 7200cut1.
       */
      if(m_Revision == STM_HDF_HDMI_REV1)
        return;

      /*
       * Don't do anything if the value isn't changing
       */
      if(m_ulAudioOutputType == (stm_hdmi_audio_output_type_t)ulNewVal)
        return;

      /*
       * Check the validity of the setup
       */
      switch(ulNewVal)
      {
        case STM_HDMI_AUDIO_TYPE_NORMAL:
        case STM_HDMI_AUDIO_TYPE_ONEBIT:
          break;
        case STM_HDMI_AUDIO_TYPE_DST:
        case STM_HDMI_AUDIO_TYPE_DST_DOUBLE:
          if(m_ulInputSource & STM_AV_SOURCE_8CH_I2S_INPUT)
            return;
          break;
        case STM_HDMI_AUDIO_TYPE_HBR:
          if((m_ulInputSource & STM_AV_SOURCE_2CH_I2S_INPUT) ||
             ((m_Revision == STM_HDF_HDMI_REV3) && (m_ulInputSource & STM_AV_SOURCE_SPDIF_INPUT)))
          {
            return;
          }
          break;
        default:
          return;
      }

      DEBUGF2(2,("%s: change audio output type to %lu\n",__PRETTY_FUNCTION__,ulNewVal));

      /*
       * We are being told there is a change to the audio stream, so stop the
       * current audio processing and get the audio update interrupt handling
       * to sort it out.
       */
      g_pIOS->LockResource(m_statusLock);

      InvalidateAudioPackets();

      m_ulAudioOutputType = (stm_hdmi_audio_output_type_t)ulNewVal;
      m_audioState.status = STM_HDMI_AUDIO_STOPPED;

      SetSPDIFPlayerMode();

      g_pIOS->UnlockResource(m_statusLock);
      break;
    }
    default:
    {
      CSTmHDMI::SetControl(ctrl, ulNewVal);
      break;
    }
  }
}


ULONG CSTmHDFormatterHDMI::GetControl(stm_output_control_t ctrl) const
{
  DENTRY();

  switch (ctrl)
  {
    case STM_CTRL_AV_SOURCE_SELECT:
      return m_ulInputSource;
    default:
      return CSTmHDMI::GetControl(ctrl);
  }

  return 0;
}


void CSTmHDFormatterHDMI::SetSPDIFPlayerMode(void)
{
  ULONG gpout = ReadHDMIReg(STM_HDMI_GPOUT) & ~STM_HDMI_GPOUT_DTS_SPDIF_MODE_EN;
  if((m_ulInputSource & STM_AV_SOURCE_SPDIF_INPUT) &&
     (m_ulAudioOutputType == STM_HDMI_AUDIO_TYPE_HBR))
  {
    DEBUGF2(2,("%s: enabling HBR mode on HDMI SPDIF Player\n",__PRETTY_FUNCTION__));
    gpout |= STM_HDMI_GPOUT_DTS_SPDIF_MODE_EN;
  }

  WriteHDMIReg(STM_HDMI_GPOUT,gpout);
}


bool CSTmHDFormatterHDMI::SetOutputFormat(ULONG format)
{
  ULONG val = ReadHDFmtReg(HDFMT_DIG1_CFG) & ~(HDFMT_CFG_INPUT_MASK | HDFMT_CFG_CLIP_GB_N_CRCB);

  DENTRY();

  switch (format & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422))
  {
    case STM_VIDEO_OUT_RGB:
      DEBUGF2(2,("%s: Set RGB Output\n",__PRETTY_FUNCTION__));
      val |= (HDFMT_CFG_INPUT_RGB | HDFMT_CFG_CLIP_GB_N_CRCB);
      break;
    case STM_VIDEO_OUT_422:
    case STM_VIDEO_OUT_YUV:
      DEBUGF2(2,("%s: Set YUV Output\n",__PRETTY_FUNCTION__));
      val |= HDFMT_CFG_INPUT_YCBCR;
      break;
    default:
      return false;
  }

  WriteHDFmtReg(HDFMT_DIG1_CFG, val);

  return CSTmHDMI::SetOutputFormat(format);
}


bool CSTmHDFormatterHDMI::SetInputSource(ULONG src)
{
  DENTRY();

  /*
   * We only support HDMI fed from the main video path.
   */
  if(!(src & STM_AV_SOURCE_MAIN_INPUT))
    return false;

  /*
   * Lock the updating of the audio config registers against the
   * HDMI interrupt audio update handling.
   */
  g_pIOS->LockResource(m_statusLock);

  switch(src & (STM_AV_SOURCE_SPDIF_INPUT   |
                STM_AV_SOURCE_2CH_I2S_INPUT |
                STM_AV_SOURCE_8CH_I2S_INPUT))
  {
    case STM_AV_SOURCE_SPDIF_INPUT:
    {
      if(m_Revision == STM_HDF_HDMI_REV3 && m_ulAudioOutputType == STM_HDMI_AUDIO_TYPE_HBR)
      {
        /*
         * The HBR configuration for the SPDIF player to be quad clocked and
         * get the channel status bits correct doesn't exist on Rev3.
         */
        g_pIOS->UnlockResource(m_statusLock);
        return false;
      }

      if(!(m_ulInputSource & STM_AV_SOURCE_SPDIF_INPUT))
      {
        InvalidateAudioPackets();

        ULONG val = ReadHDMIReg(STM_HDMI_GPOUT) & ~STM_HDMI_GPOUT_I2S_N_SPDIF_DATA;
        if(m_Revision != STM_HDF_HDMI_REV1)
          val &= ~STM_HDMI_GPOUT_I2S_N_SPDIF_CLOCK;

        WriteHDMIReg(STM_HDMI_GPOUT,val);

        val = ReadHDMIReg(STM_HDMI_AUDIO_CFG) & ~STM_HDMI_AUD_CFG_8CH_NOT_2CH;
        WriteHDMIReg(STM_HDMI_AUDIO_CFG, val);

        m_audioState.status = STM_HDMI_AUDIO_STOPPED;

        DEBUGF2(2,("%s - Set SPDIF Input\n",__PRETTY_FUNCTION__));
      }
      break;
    }
    case STM_AV_SOURCE_2CH_I2S_INPUT:
    {
      if(m_ulAudioOutputType == STM_HDMI_AUDIO_TYPE_HBR)
      {
        g_pIOS->UnlockResource(m_statusLock);
        return false;
      }

      if(!(m_ulInputSource & STM_AV_SOURCE_2CH_I2S_INPUT))
      {
        InvalidateAudioPackets();

        ULONG val = ReadHDMIReg(STM_HDMI_GPOUT) | STM_HDMI_GPOUT_I2S_N_SPDIF_DATA;
        if(m_Revision != STM_HDF_HDMI_REV1)
          val |= STM_HDMI_GPOUT_I2S_N_SPDIF_CLOCK;

        WriteHDMIReg(STM_HDMI_GPOUT,val);

        val = ReadHDMIReg(STM_HDMI_AUDIO_CFG) & ~STM_HDMI_AUD_CFG_8CH_NOT_2CH;
        WriteHDMIReg(STM_HDMI_AUDIO_CFG, val);

        m_audioState.status = STM_HDMI_AUDIO_STOPPED;

        DEBUGF2(2,("%s - Set I2S Input\n",__PRETTY_FUNCTION__));
      }
      break;
    }
    case STM_AV_SOURCE_8CH_I2S_INPUT:
    {
      if((m_Revision == STM_HDF_HDMI_REV1)                           ||
         (m_ulAudioOutputType == STM_HDMI_AUDIO_TYPE_DST)        ||
         (m_ulAudioOutputType == STM_HDMI_AUDIO_TYPE_DST_DOUBLE))
      {
        g_pIOS->UnlockResource(m_statusLock);
        return false;
      }

      if(!(m_ulInputSource & STM_AV_SOURCE_8CH_I2S_INPUT))
      {
        InvalidateAudioPackets();

        ULONG val = ReadHDMIReg(STM_HDMI_GPOUT);
        val |= STM_HDMI_GPOUT_I2S_N_SPDIF_DATA | STM_HDMI_GPOUT_I2S_N_SPDIF_CLOCK;
        WriteHDMIReg(STM_HDMI_GPOUT,val);

        val = ReadHDMIReg(STM_HDMI_AUDIO_CFG) | STM_HDMI_AUD_CFG_8CH_NOT_2CH;
        WriteHDMIReg(STM_HDMI_AUDIO_CFG, val);

        m_audioState.status = STM_HDMI_AUDIO_STOPPED;

        DEBUGF2(2,("%s - Set Multichannel I2S Input\n",__PRETTY_FUNCTION__));
      }
      break;
    }
    default:
    {
      InvalidateAudioPackets();

      ULONG val = ReadHDMIReg(STM_HDMI_GPOUT) | STM_HDMI_GPOUT_I2S_N_SPDIF_DATA;
      if(m_Revision != STM_HDF_HDMI_REV1)
        val |= STM_HDMI_GPOUT_I2S_N_SPDIF_CLOCK;

      WriteHDMIReg(STM_HDMI_GPOUT,val);

      val = ReadHDMIReg(STM_HDMI_AUDIO_CFG) & ~STM_HDMI_AUD_CFG_8CH_NOT_2CH;
      WriteHDMIReg(STM_HDMI_AUDIO_CFG, val);

      m_audioState.status = STM_HDMI_AUDIO_DISABLED;

      DEBUGF2(2,("%s - Audio Disabled\n",__PRETTY_FUNCTION__));
      break;
    }
  }

  m_ulInputSource = src;

  // Must be done after changing m_ulInputSource and still under interrupt lock
  SetSPDIFPlayerMode();

  g_pIOS->UnlockResource(m_statusLock);

  DEXIT();
  return true;
}


bool CSTmHDFormatterHDMI::PreAuth() const
{
  return true;
}


void CSTmHDFormatterHDMI::PostAuth(bool auth)
{
  g_pIOS->CallBios (auth ? 33 : 34, 2, 1, STM_HDMI_CTRL2_AUTH);
}


#define AUD_PLAYER_CTRL   0x1C

#define AUD_PCMP_CTRL_MODE_MASK    0x3
#define AUD_PCMP_CTRL_CLKDIV_SHIFT 4
#define AUD_PCMP_CTRL_CLKDIV_MASK  (0xFF << AUD_PCMP_CTRL_CLKDIV_SHIFT)

#define AUD_SPDIF_CTRL_MODE_MASK    0x7
#define AUD_SPDIF_CTRL_CLKDIV_SHIFT 5
#define AUD_SPDIF_CTRL_CLKDIV_MASK  (0xFF << AUD_SPDIF_CTRL_CLKDIV_SHIFT)

void CSTmHDFormatterHDMI::GetAudioHWState(stm_hdmi_audio_state_t *state)
{
  ULONG audctrl;

  /*
   * First update the audio info frame state, the HDMI can take its audio input
   * from either the SPDIF player or PCM Player 0 via an I2S to SPDIF converter.
   */
  if(m_ulInputSource & STM_AV_SOURCE_SPDIF_INPUT)
  {
    audctrl = ReadSPDIFReg(AUD_PLAYER_CTRL);
    state->status = ((audctrl & AUD_SPDIF_CTRL_MODE_MASK) != 0)?STM_HDMI_AUDIO_RUNNING:STM_HDMI_AUDIO_STOPPED;
    state->clock_divide = (audctrl & AUD_SPDIF_CTRL_CLKDIV_MASK) >> AUD_SPDIF_CTRL_CLKDIV_SHIFT;
  }
  else
  {
    audctrl = ReadPCMPReg(AUD_PLAYER_CTRL);
    state->status = ((audctrl & AUD_PCMP_CTRL_MODE_MASK) != 0)?STM_HDMI_AUDIO_RUNNING:STM_HDMI_AUDIO_STOPPED;
    state->clock_divide = (audctrl & AUD_PCMP_CTRL_CLKDIV_MASK) >> AUD_PCMP_CTRL_CLKDIV_SHIFT;
  }
}

