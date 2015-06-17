/***********************************************************************
 *
 * File: display/ip/stmdenc.cpp
 * Copyright (c) 2000-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include <display/generic/DisplayDevice.h>
#include <display/generic/Output.h>
#include <display/generic/MetaDataQueue.h>

#include "stmdencregs.h"
#include "stmdenc.h"
#include "stmteletext.h"

/*
 * DENC subcarrier frequency synthesizer setup for various modes where the
 * default DENC setup is not sufficient.
 */
static const stm_denc_idfs ntsc443    = { 0x2A, 0x09, 0x8B };
static const stm_denc_idfs palnc_sq   = { 0x1F, 0x15, 0xC0 };

/*
 * Conversion table between STKPI Chroma delay enumeration and the DENC
 * values.
 */
static const uint8_t chroma_delay_table[] = {
/* [STM_CHROMA_DELAY_M_2_0] */ DENC_CHROMA_DEL_M_2_0 ,
/* [STM_CHROMA_DELAY_M_1_5] */ DENC_CHROMA_DEL_M_1_5 ,
/* [STM_CHROMA_DELAY_M_1_0] */ DENC_CHROMA_DEL_M_1_0 ,
/* [STM_CHROMA_DELAY_M_0_5] */ DENC_CHROMA_DEL_M_0_5 ,
/* [STM_CHROMA_DELAY_0] */     DENC_CHROMA_DEL_ZERO  ,
/* [STM_CHROMA_DELAY_0_5] */   DENC_CHROMA_DEL_P_0_5 ,
/* [STM_CHROMA_DELAY_1_0] */   DENC_CHROMA_DEL_P_1_0 ,
/* [STM_CHROMA_DELAY_1_5] */   DENC_CHROMA_DEL_P_1_5 ,
/* [STM_CHROMA_DELAY_2_0] */   DENC_CHROMA_DEL_P_2_0 ,
/* [STM_CHROMA_DELAY_2_5] */   DENC_CHROMA_DEL_P_2_5 ,
};


/*
 * We need to calculate DAC scaling percentages for the target max output given
 * the maximum DAC output voltage and saturation point based on a 10bit DAC range.
 *
 * For PAL/SECAM Y signal, white has value 816 and the sync level is 16.
 * The difference between the two (800) must equal 1v change on the DACs.
 * Hence the full DAC range is 1023/800 = 1.279v
 *
 * NTSC is very slightly different, white level = 800 and sync level is 16.
 * Hence the full DAC range is 1023/784 = 1.305v
 *
 * Note that the above values hold even if a 7.5IRE pedestal level is
 * added (e.g. NTSC-M), as the DENC automatically scales the video signal
 * to accomodate this without us having to do anything else.
 *
 * What about RGB SCART output? The DENC is supposed to scale RGB to 70%
 * of Y, so using the same value above will end up with a total range of ~0.9v
 * including syncs (which is a little low, should be 1vpp) and a video data
 * range of 0.7vpp (which is correct).
 *
 */
static const uint32_t PAL_SECAM_MAX_VOLTAGE = 1279;
static const uint32_t NTSC_MAX_VOLTAGE      = 1305;

/////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSTmDENC::CSTmDENC(CDisplayDevice *pDev,
                   uint32_t        ulDencOffset,
                   CSTmTeletext   *pTeletext)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pDev      = pDev;
  m_pDENCReg  = ((uint32_t*)m_pDev->GetCtrlRegisterBase()) + (ulDencOffset>>2);
  m_lock      = 0;
  m_pParent   = 0;
  m_pTeletext             = pTeletext;

  m_ulCapabilities        = OUTPUT_CAPS_HW_CC|((m_pTeletext != 0)?OUTPUT_CAPS_HW_TELETEXT:0);

  m_mainOutputFormat      = 0;
  m_auxOutputFormat       = 0;

  vibe_os_zero_memory(&m_CurrentMode, sizeof(stm_display_mode_t));
  m_CurrentMode.mode_id   = STM_TIMING_MODE_RESERVED;

  vibe_os_zero_memory(&m_chromaFilterConfig, sizeof(stm_display_denc_filter_setup_t));

  vibe_os_zero_memory(&m_lumaFilterConfig, sizeof(stm_display_denc_filter_setup_t));

  /*
   * Some or all of these might be overridden for specific chips and boards
   */
  m_DAC123RGBScale         = 0;
  m_DAC123YScale           = 0;
  m_DAC456RGBScale         = 0;
  m_DAC456YScale           = 0;

  m_MinRGBScalePercentage  = 0 ;
  m_RGBScaleStep           = 0 ;
  m_MinYScalePercentage    = 0 ;
  m_YScaleStep             = 0 ;

  m_maxDAC123Voltage         = NTSC_MAX_VOLTAGE;
  m_maxDAC456Voltage         = NTSC_MAX_VOLTAGE;
  m_DAC123Saturation         = 1023;  // Full 10bit DAC range by default.
  m_DAC456Saturation         = 1023;  // Full 10bit DAC range by default.

  m_DisplayStandardMaxVoltage= NTSC_MAX_VOLTAGE;

  m_C_MultiplyStep = 3125;   // 3.125%
  m_C_MultiplyMin  = 100000; // 100%
  m_C_MultiplyMax  = m_C_MultiplyMin+(15 * m_C_MultiplyStep); // 146.875%
  m_C_Multiply     = 0; // Hardware value for 100%

  /*
   * By default disable the CVBS luma trap filter. This can be turned on
   * using the STG_CTRL_CVBS_TRAP_FILTER control.
   */
  m_bEnableTrapFilter      = false;
  m_bEnableLumaNotchFilter = false;
  m_bEnableIFLumaDelay     = false;
  m_bEnableLumaFilter      = false;
  m_bEnableChromaFilter    = false;
  m_bFreerunEnabled        = false;

  /*
   * Default chroma/luma delay for V13 DENC and YCrCb444 input is -1
   */
  m_bEnableCVBSChromaDelay      = true;
  m_CVBSChromaDelay             = DENC_CHROMA_DEL_M_1_0;
  m_bEnableComponentChromaDelay = true;
  m_ComponentChromaDelay        = DENC_CHROMA_DEL_M_1_0;
  m_RGBDelay                    = DENC_CFG4_SYOUT_0;

  vibe_os_zero_memory(&m_currentPictureInfo, sizeof(stm_picture_format_info_t));
  m_currentPictureInfo.video_aspect_ratio    = STM_WSS_ASPECT_RATIO_UNKNOWN;
  m_currentPictureInfo.picture_aspect_ratio  = STM_WSS_ASPECT_RATIO_4_3;
  m_currentPictureInfo.letterbox_style = STM_LETTERBOX_NONE;
  m_pPictureInfoQueue   = 0;

  m_pClosedCaptionQueue = 0;
  m_bEnableCCField      = 0;

  m_bEnableWSSInsertion = false; /* default is not to use the HW WSS insertion, refer to Bug 22570 */
  m_CGMSAnalog          = 0;
  m_bUpdateWSS          = false;
  m_teletextStandard    = DENC_TTXCFG_SYSTEM_A;
  m_teletextLineSize    = 0; /* depends on output standard */

  m_ulBrightness        = 0x80;
  m_ulSaturation        = 0x80;
  m_ulContrast          = 0x80;
  m_bHasHueControl      = true;
  m_ulHue               = 0x80;

  m_signalRange         = STM_SIGNAL_FILTER_SAV_EAV;

  vibe_os_zero_memory(&m_currentCfg,sizeof(struct stm_denc_cfg));

  /*
   * Ensure the DENC output is initially disabled, in case anything survived
   * a chip reset or we previously had something else initialize the display
   * for a splash screen.
   */
  WriteDENCReg(DENC_CFG5, DENC_CFG5_DIS_DAC1 |
                          DENC_CFG5_DIS_DAC2 |
                          DENC_CFG5_DIS_DAC3 |
                          DENC_CFG5_DIS_DAC4 |
                          DENC_CFG5_DIS_DAC5 |
                          DENC_CFG5_DIS_DAC6);

  WriteDENCReg(DENC_CFG6, DENC_CFG6_RST_SOFT);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmDENC::~CSTmDENC()
{
  if(m_lock)
    vibe_os_delete_resource_lock(m_lock);

  delete m_pPictureInfoQueue;
  delete m_pClosedCaptionQueue;
}


bool CSTmDENC::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_lock = vibe_os_create_resource_lock();
  if(m_lock == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create object lock" );
    return false;
  }

  /*
   * Create a 2 element metadata queue for picture information that returns its
   * data 1 vsync before its presentation time.
   */
  m_pPictureInfoQueue = new CMetaDataQueue(STM_METADATA_TYPE_PICTURE_INFO,2,1);
  if(!m_pPictureInfoQueue || !m_pPictureInfoQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create picture info queue" );
    return false;
  }

  /*
   * Create a 2 element metadata queue for closed caption that returns its
   * data 1 vsync before its presentation time.
   */
  m_pClosedCaptionQueue = new CMetaDataQueue(STM_METADATA_TYPE_CLOSED_CAPTION,2,1);
  if(!m_pClosedCaptionQueue || !m_pClosedCaptionQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create closed caption queue" );
    return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmDENC::SetMainOutputFormat(uint32_t format)
{
  static const uint32_t componentFormats = (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV);

  // If we clash with the current AUX format, fail
  if((m_auxOutputFormat & format) != 0)
    return false;

  // RGB & YUV are mutually exclusive.
  if((format & componentFormats) == componentFormats)
    return false;

  // Check there isn't a YUV/RGB clash between main and aux
  if((format & componentFormats) != 0 &&
     (m_auxOutputFormat & componentFormats) != 0)
  {
    return false;
  }

  m_mainOutputFormat = format;

  ProgramOutputFormats();

  return true;
}


bool CSTmDENC::SetAuxOutputFormat(uint32_t format)
{
  static const uint32_t componentFormats = (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV);

  // If we clash with the current main format, fail
  if((m_mainOutputFormat & format) != 0)
    return false;

  // RGB & YUV are mutually exclusive.
  if((format & componentFormats) == componentFormats)
    return false;

  // Check there isn't a YUV/RGB clash between main and aux
  if((format & componentFormats) != 0 &&
     (m_mainOutputFormat & componentFormats) != 0)
  {
    return false;
  }

  m_auxOutputFormat = format;

  ProgramOutputFormats();

  return true;
}


void CSTmDENC::ProgramPSIControls()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "brightness = 0x%x", m_ulBrightness );
  WriteDENCReg(DENC_BRIGHT,m_ulBrightness);

  TRC( TRC_ID_MAIN_INFO, "saturation = 0x%x", m_ulSaturation );
  WriteDENCReg(DENC_SATURA,m_ulSaturation);

  /*
   * Convert input 0-255 to 8bit signed -128 to 127
   */
  signed char val = (int)m_ulContrast - 128;
  WriteDENCReg(DENC_CONTRA,(uint8_t)val);

  TRC( TRC_ID_MAIN_INFO, "contrast = 0x%x", (uint32_t)val );

  /*
   * HUE control is not available in SECAM, the Phase registers that
   * contain the enable bit have a different meaning when SECAM is enabled.
   */
  if(m_bHasHueControl && (m_CurrentMode.mode_params.output_standards != STM_OUTPUT_STD_SECAM))
  {
    uint8_t hue;
    /*
     * The DENC Hue control control programming is slightly strange, it
     * is a 7 bit number (i.e. 0-127) with bit 8 indicating sign, but
     * contrary to all convention bit eight set to 1 indicates a _positive_
     * number!
     */
    if(m_ulHue >= 128)
      hue = (m_ulHue-128) | 0x80;
    else
      hue = 128 - m_ulHue;

    WriteDENCReg(DENC_HUE, hue);
    TRC( TRC_ID_MAIN_INFO, "hue = 0x%x", (uint32_t)hue );

    uint8_t reg = ReadDENCReg(DENC_PDFS0) | DENC_DFS_PHASE0_HUE_EN;

    /*
     * We only need to set HUE_EN, it gets automatically reset to zero by
     * the HW once the value has been accepted.
     */
    WriteDENCReg(DENC_PDFS0, reg);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmDENC::SetPSIControl(uint32_t *ctrl, uint32_t newVal)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(newVal>255)
    newVal = 255;

  if(*ctrl != newVal)
  {
    *ctrl = newVal;
    ProgramPSIControls();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


uint32_t CSTmDENC::SetControl(stm_output_control_t ctrl, uint32_t newVal)
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_BRIGHTNESS:
    {
      SetPSIControl(&m_ulBrightness,newVal);
      break;
    }
    case OUTPUT_CTRL_SATURATION:
    {
      SetPSIControl(&m_ulSaturation,newVal);
      break;
    }
    case OUTPUT_CTRL_CONTRAST:
    {
      SetPSIControl(&m_ulContrast,newVal);
      break;
    }
    case OUTPUT_CTRL_HUE:
    {
      /*
       * The hue input value is ranged from 1 to 255, with the centre
       * (0 phase shift) at 128.
       */
      if(newVal < 1)
        newVal = 1;

      SetPSIControl(&m_ulHue,newVal);
      break;
    }
    case OUTPUT_CTRL_CVBS_TRAP_FILTER:
    {
      m_bEnableTrapFilter = (newVal != 0);
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetControl Setting trap filter %s", m_bEnableTrapFilter?"on":"off" );

      if(IsStarted())
      {
        vibe_os_lock_resource(m_lock);
        {
          if(m_bEnableTrapFilter)
            m_currentCfg.cfg[3] |= DENC_CFG3_ENA_TRFLT;
          else
            m_currentCfg.cfg[3] &= ~DENC_CFG3_ENA_TRFLT;

          WriteDENCReg(DENC_CFG3, m_currentCfg.cfg[3]);
        }
        vibe_os_unlock_resource(m_lock);
      }
      break;
    }
    case OUTPUT_CTRL_LUMA_NOTCH_FILTER:
    {
      m_bEnableLumaNotchFilter = (newVal != 0);
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetControl Setting notch filter %s", m_bEnableLumaNotchFilter?"on":"off" );

      if(IsStarted())
      {
        vibe_os_lock_resource(m_lock);
        {
          if(m_bEnableLumaNotchFilter)
            m_currentCfg.cfg[12] |= DENC_CFG12_ENNOTCH;
          else
            m_currentCfg.cfg[12] &= ~DENC_CFG12_ENNOTCH;

          WriteDENCReg(DENC_CFG12, m_currentCfg.cfg[12]);
        }
        vibe_os_unlock_resource(m_lock);
      }
      break;
    }
    case OUTPUT_CTRL_IF_LUMA_DELAY:
    {
      m_bEnableIFLumaDelay = (newVal != 0);
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetControl Setting IF Luma delay %s", m_bEnableIFLumaDelay?"on":"off" );

      if(IsStarted())
      {
        vibe_os_lock_resource(m_lock);
        {
          if(m_bEnableIFLumaDelay)
            m_currentCfg.cfg[11] |= DENC_CFG11_MAIN_IF_DELAY;
          else
            m_currentCfg.cfg[11] &= ~DENC_CFG11_MAIN_IF_DELAY;

          WriteDENCReg(DENC_CFG11, m_currentCfg.cfg[11]);
        }
        vibe_os_unlock_resource(m_lock);
      }
      break;
    }
    case OUTPUT_CTRL_WSS_INSERTION:
    {
      vibe_os_lock_resource(m_lock);
      {
        m_bEnableWSSInsertion = (newVal != 0);
        m_bUpdateWSS = true;
      }
      vibe_os_unlock_resource(m_lock);
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetControl Setting WSS Insertion %s", m_bEnableWSSInsertion?"on":"off" );
      break;
    }
    case OUTPUT_CTRL_CC_INSERTION_ENABLE:
    {
      vibe_os_lock_resource(m_lock);
      {
        m_bEnableCCField = newVal;
        m_currentCfg.cfg[1] &= DENC_CFG1_MASK_CC;
        m_currentCfg.cfg[1] |= (m_bEnableCCField & DENC_CFG1_CC_ENA_BOTH);
        WriteDENCReg(DENC_CFG1, m_currentCfg.cfg[1]);
      }
      vibe_os_unlock_resource(m_lock);
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetControl Setting Closed Caption Insertion %s", m_bEnableCCField?"on":"off" );
      break;
    }
    case OUTPUT_CTRL_SD_CGMS:
    {
      vibe_os_lock_resource(m_lock);
      {
        m_CGMSAnalog = (newVal & 0x1F);
        m_bUpdateWSS = true;
      }
      vibe_os_unlock_resource(m_lock);

      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetControl Setting CGMS 0x%x", m_CGMSAnalog );
      break;
    }
    case OUTPUT_CTRL_DAC123_MAX_VOLTAGE:
    {
      m_maxDAC123Voltage = newVal;
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetControl DAC123 MaxV = %u", m_maxDAC123Voltage );
      CalculateDACScaling();
      if(IsStarted())
        ProgramOutputFormats();

      break;
    }
    case OUTPUT_CTRL_DAC456_MAX_VOLTAGE:
    {
      m_maxDAC456Voltage = newVal;
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetControl DAC456 MaxV = %u", m_maxDAC456Voltage );
      CalculateDACScaling();
      if(IsStarted())
        ProgramOutputFormats();

      break;
    }
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
    {
      if(newVal> STM_SIGNAL_VIDEO_RANGE)
        return STM_OUT_INVALID_VALUE;

      if(newVal == STM_SIGNAL_FULL_RANGE)
        m_signalRange = STM_SIGNAL_FILTER_SAV_EAV;
      else
        m_signalRange = (stm_display_signal_range_t)newVal;

      if(IsStarted())
      {
        vibe_os_lock_resource(m_lock);
        {
          if(m_signalRange == STM_SIGNAL_FILTER_SAV_EAV)
          {
            TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetControl Filter SAV/EAV values" );
            m_currentCfg.cfg[6]  |= DENC_CFG6_MAX_DYN;
            m_currentCfg.cfg[10] &= ~DENC_CFG10_RGBSAT_EN;
          }
          else
          {
            TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetControl Clamp input to 16-235/240" );
            m_currentCfg.cfg[6]  &= ~DENC_CFG6_MAX_DYN;
            m_currentCfg.cfg[10] |= DENC_CFG10_RGBSAT_EN;
          }

          WriteDENCReg(DENC_CFG6,  m_currentCfg.cfg[6]);
          WriteDENCReg(DENC_CFG10, m_currentCfg.cfg[10]);
        }
        vibe_os_unlock_resource(m_lock);
      }
      break;
    }
    case OUTPUT_CTRL_TELETEXT_SYSTEM:
    {
      if(!m_pTeletext)
        return STM_OUT_NO_CTRL;

      if(newVal>STM_TELETEXT_SYSTEM_D)
        return STM_OUT_INVALID_VALUE;

      m_pTeletext->Stop();

      vibe_os_lock_resource(m_lock);
      {
        m_teletextStandard  = (uint8_t)(newVal << DENC_TTXCFG_SYSTEM_SHIFT);
        m_currentCfg.ttxcfg &= ~DENC_TTXCFG_SYSTEM_MASK;
        m_currentCfg.ttxcfg |= m_teletextStandard;

        if(IsStarted())
        {
          SetTeletextLineSize();
          WriteDENCReg(DENC_TTXCFG, m_currentCfg.ttxcfg);
        }
      }
      vibe_os_unlock_resource(m_lock);

      if(IsStarted())
        m_pTeletext->Start(m_teletextLineSize, m_pParent);

      break;
    }
    case OUTPUT_CTRL_CHROMA_SCALE:
    {
      if(newVal < m_C_MultiplyMin)
        newVal = m_C_MultiplyMin;

      if(newVal > m_C_MultiplyMax)
        newVal = m_C_MultiplyMax;

      newVal -= m_C_MultiplyMin;

      m_C_Multiply = (newVal+(m_C_MultiplyStep/2)) / m_C_MultiplyStep;

      if(IsStarted())
        ProgramOutputFormats();

      break;
    }
    case OUTPUT_CTRL_CHROMA_DELAY:
    {
      if(newVal > STM_CHROMA_DELAY_2_5)
        return STM_OUT_INVALID_VALUE;

      /*
       * At the moment we are setting both chroma delays with the same delay
       * value; is this sufficient?
       */
      m_CVBSChromaDelay      = chroma_delay_table[newVal];
      m_ComponentChromaDelay = chroma_delay_table[newVal];
      if(IsStarted())
      {
        m_currentCfg.cfg[9] &= ~DENC_CFG9_DEL_MASK;
        m_currentCfg.cfg[9] |= DENC_CFG9_DEL(m_CVBSChromaDelay);
        WriteDENCReg(DENC_CFG9, m_currentCfg.cfg[9]);
        WriteDENCReg(DENC_CHROMA_DELAY, m_ComponentChromaDelay);
        TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetControl Changing chroma delays DENC_CFG9 = 0x%x DENC_CHROMA_DELAY = 0x%x", (unsigned)m_currentCfg.cfg[9], (unsigned)m_ComponentChromaDelay );
      }

      break;
    }
    default:
      return STM_OUT_NO_CTRL;
  }

  return STM_OUT_OK;
}


uint32_t CSTmDENC::GetControl(stm_output_control_t ctrl, uint32_t *val) const
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_BRIGHTNESS:
      *val = m_ulBrightness;
      break;
    case OUTPUT_CTRL_SATURATION:
      *val = m_ulSaturation;
      break;
    case OUTPUT_CTRL_CONTRAST:
      *val = m_ulContrast;
      break;
    case OUTPUT_CTRL_HUE:
      *val = m_ulHue;
      break;
    case OUTPUT_CTRL_CVBS_TRAP_FILTER:
      *val = m_bEnableTrapFilter?1:0;
      break;
    case OUTPUT_CTRL_LUMA_NOTCH_FILTER:
      *val = m_bEnableLumaNotchFilter?1:0;
      break;
    case OUTPUT_CTRL_IF_LUMA_DELAY:
      *val = m_bEnableIFLumaDelay?1:0;
      break;
    case OUTPUT_CTRL_WSS_INSERTION:
      *val = m_bEnableWSSInsertion?1:0;
      break;
    case OUTPUT_CTRL_SD_CGMS:
      *val = m_CGMSAnalog;
      break;
    case OUTPUT_CTRL_DAC123_MAX_VOLTAGE:
      *val = m_maxDAC123Voltage;
      break;
    case OUTPUT_CTRL_DAC456_MAX_VOLTAGE:
      *val = m_maxDAC456Voltage;
      break;
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
      *val = (uint32_t)m_signalRange;
      break;
    case OUTPUT_CTRL_TELETEXT_SYSTEM:
    {
      if(m_pTeletext)
        *val = (uint32_t)(m_teletextStandard >> DENC_TTXCFG_SYSTEM_SHIFT);
      else
      {
        *val = 0;
        return STM_OUT_NO_CTRL;
      }

      break;
    }
    case OUTPUT_CTRL_CHROMA_SCALE:
      *val = m_C_MultiplyMin + ((uint32_t)m_C_Multiply * m_C_MultiplyStep);
      break;
    case OUTPUT_CTRL_CHROMA_DELAY:
    {
      uint32_t i;
      for(i=0;i<N_ELEMENTS(chroma_delay_table);i++)
      {
        if(chroma_delay_table[i] == m_CVBSChromaDelay)
        {
          *val = i;
          break;
        }
      }
      ASSERTF((i<N_ELEMENTS(chroma_delay_table)),(""));
      break;
    }
    default:
      *val = 0;
      return STM_OUT_NO_CTRL;
  }

  return STM_OUT_OK;
}

bool CSTmDENC::SetVPS(stm_display_vps_t *vpsdata)
{
  bool ret = true;
  TRCIN( TRC_ID_MAIN_INFO, "" );
  if (vpsdata->vps_enable)
  {
    m_currentCfg.cfg[7] |= (DENC_CFG7_ENA_VPS);
    WriteDENCReg(DENC_VPS0, vpsdata->vps_data[0]);
    WriteDENCReg(DENC_VPS1, vpsdata->vps_data[1]);
    WriteDENCReg(DENC_VPS2, vpsdata->vps_data[2]);
    WriteDENCReg(DENC_VPS3, vpsdata->vps_data[3]);
    WriteDENCReg(DENC_VPS4, vpsdata->vps_data[4]);
    WriteDENCReg(DENC_VPS5, vpsdata->vps_data[5]);
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetVPS Enabled: %#.2x, %#.2x, %#.2x, %#.2x, %#.2x, %#.2x", vpsdata->vps_data[0], vpsdata->vps_data[1], vpsdata->vps_data[2], vpsdata->vps_data[3], vpsdata->vps_data[4], vpsdata->vps_data[5] );
  }
  else
  {
    m_currentCfg.cfg[7] &= ~(DENC_CFG7_ENA_VPS);
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::SetVPS Disabled" );
  }

  WriteDENCReg(DENC_CFG7, m_currentCfg.cfg[7]);
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return ret;
}

bool CSTmDENC::SetFilterCoefficients(const stm_display_filter_setup_t *f)
{
  bool ret = true;
  TRCIN( TRC_ID_MAIN_INFO, "" );
  switch(f->type)
  {
    case DENC_COEFF_LUMA:
    {
      if(f->denc.div != STM_FLT_DISABLED)
      {
        TRC( TRC_ID_MAIN_INFO, "Enabling Luma Filter" );
        m_lumaFilterConfig = f->denc;

#if defined(DEBUG)
        int sum = 0;
        for(int i=0;i<9;i++)
        {
          TRC( TRC_ID_MAIN_INFO, "luma filter coeff[%d] = %d", i,m_lumaFilterConfig.coeff[i] );
          sum += 2*m_lumaFilterConfig.coeff[i];
        }

        TRC( TRC_ID_MAIN_INFO, "luma filter coeff[9] = %d", m_lumaFilterConfig.coeff[9] );
        sum += m_lumaFilterConfig.coeff[9];

        TRC( TRC_ID_MAIN_INFO, "luma filter sum = %d", sum );
#endif

        m_bEnableLumaFilter = true;
      }
      else
      {
        TRC( TRC_ID_MAIN_INFO, "Disabling Luma Filter" );
        m_bEnableLumaFilter = false;
      }

      if(IsStarted())
        ProgramLumaFilter();

      break;
    }
    case DENC_COEFF_CHROMA:
    {
      if(f->denc.div != STM_FLT_DISABLED)
      {
        TRC( TRC_ID_MAIN_INFO, "Enabling Chroma Filter" );
        m_chromaFilterConfig = f->denc;

#if defined(DEBUG)
        int sum = 0;
        for(int i=0;i<8;i++)
        {
          TRC( TRC_ID_MAIN_INFO, "chroma filter coeff[%d] = %d", i,m_chromaFilterConfig.coeff[i] );
          sum += 2*m_chromaFilterConfig.coeff[i];
        }

        TRC( TRC_ID_MAIN_INFO, "chroma filter coeff[8] = %d", m_chromaFilterConfig.coeff[8] );
        sum += m_chromaFilterConfig.coeff[8];

        TRC( TRC_ID_MAIN_INFO, "chroma filter sum = %d", sum );
#endif

        m_bEnableChromaFilter = true;
      }
      else
      {
        TRC( TRC_ID_MAIN_INFO, "Disabling Chroma Filter" );
        m_bEnableChromaFilter = false;
      }

      if(IsStarted())
        ProgramChromaFilter();

      break;
    }
    default:
      ret = false;
      break;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return ret;
}


bool CSTmDENC::Start(COutput *parent, const stm_display_mode_t *pModeLine)
{
  return Start(parent, pModeLine, DENC_CFG0_ODHS_SLV);
}


bool CSTmDENC::Start(COutput                  *parent,
                     const stm_display_mode_t *pModeLine,
                     uint8_t                   syncMode,
                     stm_vtg_sync_type_t       href,
                     stm_vtg_sync_type_t       vref)
{
  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start - DENC ID = %#.2x", ReadDENCReg(DENC_CID) );

  if(!parent)
    return false;

  /*
   * Strip out SMPTE293M bit to get the mode for the re-interlaced DENC output
   */
  uint32_t tvStandard = pModeLine->mode_params.output_standards & STM_OUTPUT_STD_SD_MASK;

  if(tvStandard == 0)
    return false;

  /*
   * If this is a standard change, stop the DENC first so we don't have any
   * conflicts between the start sequence and the vsync UpdateHW processing
   * currently going on.
   */
  if(IsStarted())
    this->Stop();

  bool bIsSquarePixel = (pModeLine->mode_params.pixel_aspect_ratios[STM_AR_INDEX_4_3].numerator == 1 &&
                         pModeLine->mode_params.pixel_aspect_ratios[STM_AR_INDEX_4_3].denominator == 1);

  m_currentCfg.cfg[0] = GetSyncConfiguration(pModeLine, syncMode, href, vref);
  m_currentCfg.cfg[1] = (m_bEnableCCField & DENC_CFG1_CC_ENA_BOTH);
  m_currentCfg.cfg[2] = DENC_CFG2_ENA_BURST | DENC_CFG2_ENA_RST;
  m_currentCfg.cfg[3] = m_bEnableCVBSChromaDelay?DENC_CFG3_DELAY_ENABLE:0;
  m_currentCfg.cfg[4] = m_RGBDelay | DENC_CFG4_SYIN_0 | DENC_CFG4_ALINE;
  m_currentCfg.cfg[5] = DENC_CFG5_DAC_NONINV;

  /*
   * The Cfg6 Man mode bit is default/reserved and should not to be changed on
   * modern DENCS according to the datasheet.
   */
  m_currentCfg.cfg[6] = DENC_CFG6_MAN_MODE;

  if(m_signalRange != STM_SIGNAL_VIDEO_RANGE)
    m_currentCfg.cfg[6] |= DENC_CFG6_MAX_DYN;

  m_currentCfg.cfg[7]  = 0;
  m_currentCfg.cfg[8]  = DENC_CFG8_TTX_NOTMV | DENC_CFG8_RESRV_BIT; /* Reserved bit must be set according to datasheet */
  m_currentCfg.cfg[10] = (m_signalRange == STM_SIGNAL_VIDEO_RANGE)?DENC_CFG10_RGBSAT_EN:0;

  m_currentCfg.ttxcfg = DENC_TTXCFG_RESERVED_BIT_4 | m_teletextStandard; /* Reserved bit must be set according to datasheets */

  /*
   * Set the auxilary path chroma delay to be the same as the main path,
   * but see SECAM settings below.
   *
   * Thankfully the Cfg9 delay bits are identical to the Cfg11 delay bits
   * Additonally the Cfg11 reserved bit is set by default in the datasheets
   */
  m_currentCfg.cfg[11] = DENC_CFG11_RESERVED | DENC_CFG9_DEL(m_CVBSChromaDelay);
  m_currentCfg.cfg[12] = m_bEnableCVBSChromaDelay?DENC_CFG12_AUX_DEL_EN:0;

  m_currentCfg.idfs    = 0;
  m_currentCfg.pdfs    = 0;

  if(m_bEnableTrapFilter)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start Enabling Trap Filter" );
    m_currentCfg.cfg[3]  |= DENC_CFG3_ENA_TRFLT;
    m_currentCfg.cfg[12] |= DENC_CFG12_AUX_ENTRAP;
  }

  if(m_bEnableLumaNotchFilter)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start Enabling Notch Filter" );
    m_currentCfg.cfg[12] |= DENC_CFG12_ENNOTCH;
  }

  if(m_bEnableIFLumaDelay)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start Enabling IF Luma delay" );
    m_currentCfg.cfg[11] |= DENC_CFG11_MAIN_IF_DELAY;
  }

  switch(tvStandard)
  {
  case STM_OUTPUT_STD_PAL_BDGHI:
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start TV Standard is PAL-BDGHI" );
    m_currentCfg.cfg[0]  |= DENC_CFG0_PAL_BDGHI;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_19;
    m_currentCfg.cfg[2]  |= DENC_CFG2_RST_2F;
    m_currentCfg.cfg[3]  |= DENC_CFG3_TRAP_443;
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_19_FLT;

	ProgramPDFSPALConfig();

    m_DisplayStandardMaxVoltage = PAL_SECAM_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_PAL_N:
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start TV Standard is PAL-N" );
    /*
     * For the purposes of the DENC hardware this is the same as
     * PAL_BDGHI but with a 7.5IRE setup like NTSC and 1.6MHz
     * UV filter bandwidth. This information was obtained from
     * Video Demystified.
     */
    m_currentCfg.cfg[0]  |= DENC_CFG0_PAL_BDGHI;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_16 | DENC_CFG1_SETUP;
    m_currentCfg.cfg[2]  |= DENC_CFG2_RST_2F;
    m_currentCfg.cfg[3]  |= DENC_CFG3_TRAP_443;
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_16_FLT;

    m_DisplayStandardMaxVoltage = PAL_SECAM_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_PAL_Nc:
    /*
     * This is Argentian PAL-N, no IRE setup and a 3.582MHz subcarrier
     * instead of 4.43MHz
     */
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start TV Standard is PAL-Nc" );
    m_currentCfg.cfg[0]  |= DENC_CFG0_PAL_N;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_16;
    m_currentCfg.cfg[2]  |= DENC_CFG2_RST_2F;
    // Note:No 443_TRAP for PAL_Nc trap filtering as the subcarrier is ~3.58MHz
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_16_FLT;

    if(bIsSquarePixel)
    {
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start Programming Colour Carrier for square pixel" );
      m_currentCfg.idfs = &palnc_sq;
      m_currentCfg.cfg[5] |= DENC_CFG5_SEL_INC;
    }

    m_DisplayStandardMaxVoltage = PAL_SECAM_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_PAL_M:
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start TV Standard is PAL-M" );
    m_currentCfg.cfg[0]  |= DENC_CFG0_PAL_M;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_16 | DENC_CFG1_SETUP;
    m_currentCfg.cfg[2]  |= DENC_CFG2_RST_2F;
    // Note:No 443_TRAP for PAL_M trap filtering as the subcarrier is ~3.57MHz
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_16_FLT;

    m_DisplayStandardMaxVoltage = PAL_SECAM_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_PAL_60:
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start TV Standard is PAL-60" );
    m_currentCfg.cfg[0]  |= DENC_CFG0_PAL_M;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_19; // Note no IRE setup as we are pretending to be PAL-BDGHI
    m_currentCfg.cfg[2]  |= DENC_CFG2_RST_2F;
    m_currentCfg.cfg[3]  |= DENC_CFG3_TRAP_443;
    m_currentCfg.cfg[5]  |= DENC_CFG5_SEL_INC;
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_19_FLT;

    /*
     * Change the carrier frequency to 4.43MHz
     */
    m_currentCfg.idfs = &ntsc443;

    m_DisplayStandardMaxVoltage = PAL_SECAM_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_NTSC_M:
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start TV Standard is NTSC-M" );
    m_currentCfg.cfg[0]  |= DENC_CFG0_NTSC_M;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_16 | DENC_CFG1_SETUP;
    m_currentCfg.cfg[2]  |= DENC_CFG2_SEL_RST | DENC_CFG2_RST_OSC | DENC_CFG2_RST_EVE;
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_16_FLT;

    /*
     * Change the subcarrier to Horizontal sync phase to 180°
     */
    m_currentCfg.pdfs = 180;

    m_DisplayStandardMaxVoltage = NTSC_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_NTSC_443:
    /*
     * NTSC but with the colour on a 4.43MHz subcarrier frequency instead of
     * 3.579MHz.
     */
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start TV Standard is NTSC-443" );
    m_currentCfg.cfg[0]  |= DENC_CFG0_NTSC_M;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_16 | DENC_CFG1_SETUP;
    m_currentCfg.cfg[2]  ^= DENC_CFG2_ENA_RST;
    m_currentCfg.cfg[3]  |= DENC_CFG3_TRAP_443;
    m_currentCfg.cfg[5]  |= DENC_CFG5_SEL_INC;
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_16_FLT;

    /*
     * Note that we do not support NTSC with a 4.43MHz subcarrier in square
     * pixel mode.
     */
    m_currentCfg.idfs = &ntsc443;

    m_DisplayStandardMaxVoltage = NTSC_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_NTSC_J:
    /*
     * NTSC, but without the 7.5IRE setup
     */
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start TV Standard is NTSC-J" );
    m_currentCfg.cfg[0]  |= DENC_CFG0_NTSC_M;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_16;
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_16_FLT;

    m_DisplayStandardMaxVoltage = NTSC_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_SECAM:
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start TV Standard is SECAM" );
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_19;
    m_currentCfg.cfg[2]   = 0; // Not used in SECAM
    // Force trap filter enable for SECAM
    m_bEnableTrapFilter = true;
    m_currentCfg.cfg[3]  |= DENC_CFG3_TRAP_443 | DENC_CFG3_ENA_TRFLT;
    m_currentCfg.cfg[7]  |= DENC_CFG7_SECAM;
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_19_FLT;
    // Force SECAM chroma delay for auxilary path.
    m_currentCfg.cfg[11]  = DENC_CFG11_RESERVED | DENC_CFG11_DEL_P_0_5;
    m_currentCfg.cfg[12] |= DENC_CFG12_AUX_ENTRAP | DENC_CFG12_AUX_DEL_EN;

    m_DisplayStandardMaxVoltage = PAL_SECAM_MAX_VOLTAGE;
    break;

  default :
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start invalid TV Standard" );
    vibe_os_zero_memory(&m_currentCfg,sizeof(struct stm_denc_cfg));
    return false;
  }

  if(bIsSquarePixel)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Start Square pixel mode enabled" );
    m_currentCfg.cfg[7] |= DENC_CFG7_SQ_PIX;
  }

  m_pParent = parent;

  ProgramDENCConfig();
  ProgramLumaFilter();
  ProgramChromaFilter();

  /*
   * Now the DENC is basically alive, set the modeline and standard so
   * the rest of the config and interrupt processing will know what to do.
   *
   * Interrupt processing (UpdateHW) is alive after this point.
   */
  m_CurrentMode = *pModeLine;

  m_pPictureInfoQueue->Start(m_pParent);
  m_pClosedCaptionQueue->Start(m_pParent);

  ProgramPSIControls();
  SetTeletextLineSize();
  if(m_pTeletext)
    m_pTeletext->Start(m_teletextLineSize, m_pParent);

  CalculateDACScaling();
  ProgramOutputFormats();

  m_bUpdateWSS = true;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmDENC::Stop(void)
{
  m_pPictureInfoQueue->Stop();
  m_pClosedCaptionQueue->Stop();
  if(m_pTeletext)
    m_pTeletext->Stop();

  vibe_os_lock_resource(m_lock);
  {
    /*
     * Blank all DACs, actually only DENC_CFG5_DIS_DAC4 appears to have an
     * effect on the 8K and setting this blanks _all_ the DACs.
     */
    WriteDENCReg(DENC_CFG5, DENC_CFG5_DIS_DAC1 |
                            DENC_CFG5_DIS_DAC2 |
                            DENC_CFG5_DIS_DAC3 |
                            DENC_CFG5_DIS_DAC4 |
                            DENC_CFG5_DIS_DAC5 |
                            DENC_CFG5_DIS_DAC6);

    WriteDENCReg(DENC_CFG6, DENC_CFG6_RST_SOFT);

    vibe_os_zero_memory(&m_currentCfg,sizeof(struct stm_denc_cfg));

    m_CurrentMode.mode_id = STM_TIMING_MODE_RESERVED;
    m_pParent = 0;
  }
  vibe_os_unlock_resource(m_lock);
}


uint8_t CSTmDENC::GetSyncConfiguration(const stm_display_mode_t *pModeLine,
                                       uint8_t                syncMode,
                                       stm_vtg_sync_type_t    href,
                                       stm_vtg_sync_type_t    vref)
{
  /*
   * Helper for Start() method to construct the sync mode configuration in Cfg0
   */
  uint8_t cfg0 = syncMode;

  if(syncMode == DENC_CFG0_ODDE_SLV || syncMode == DENC_CFG0_FRM_SLV)
    cfg0 |= m_bFreerunEnabled?DENC_CFG0_FREE_RUN:0;

  switch(href)
  {
    case STVTG_SYNC_TIMING_MODE:
      if(pModeLine->mode_timing.hsync_polarity)
        cfg0 |= DENC_CFG0_HSYNC_POL_HIGH;

      break;
    case STVTG_SYNC_INVERSE_TIMING_MODE:
      if(!pModeLine->mode_timing.hsync_polarity)
        cfg0 |= DENC_CFG0_HSYNC_POL_HIGH;

      break;
    case STVTG_SYNC_POSITIVE:
    case STVTG_SYNC_TOP_NOT_BOT:
    case STVTG_SYNC_BOT_NOT_TOP:
      cfg0 |= DENC_CFG0_HSYNC_POL_HIGH;
      break;
    case STVTG_SYNC_NEGATIVE:
      break;
  }

  switch(vref)
  {
    case STVTG_SYNC_TIMING_MODE:
      if(pModeLine->mode_timing.vsync_polarity)
        cfg0 |= DENC_CFG0_VSYNC_POL_HIGH;
      break;
    case STVTG_SYNC_INVERSE_TIMING_MODE:
      if(!pModeLine->mode_timing.vsync_polarity)
        cfg0 |= DENC_CFG0_VSYNC_POL_HIGH;
      break;
    case STVTG_SYNC_POSITIVE:
    case STVTG_SYNC_TOP_NOT_BOT:
      cfg0 |= DENC_CFG0_VSYNC_POL_HIGH;
      break;
    case STVTG_SYNC_NEGATIVE:
    case STVTG_SYNC_BOT_NOT_TOP:
      break;
  }

  return cfg0;
}

void CSTmDENC::ProgramPDFSPALConfig(void)
{

	uint8_t pdfs0, pdfs1;

	TRCIN( TRC_ID_MAIN_INFO, "" );
	pdfs0 = 0x76;
	pdfs1 = 0xe0;
	m_currentCfg.cfg[2] =0x6d;

    WriteDENCReg(DENC_PDFS0, pdfs0);
    WriteDENCReg(DENC_PDFS1, pdfs1);

    TRC( TRC_ID_MAIN_INFO, "DENC_PDFS0 = %02X", ReadDENCReg(DENC_PDFS0) );
    TRC( TRC_ID_MAIN_INFO, "DENC_PDFS1 = %02X", ReadDENCReg(DENC_PDFS1) );
	}

void CSTmDENC::ProgramPDFSConfig(void)
{
  uint32_t pdfs;
  uint8_t pdfs0, pdfs1;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if((m_currentCfg.pdfs > 0) && (m_currentCfg.pdfs <= 360))
  {
    pdfs  = (((m_currentCfg.pdfs * 100) / 35) + 1);
    pdfs0 = (pdfs & 0xFF);
    pdfs1 = (((pdfs >> 8) & 0xFF) | 0x80);

    TRC( TRC_ID_MAIN_INFO, "Setting the subcarrier to Horizontal sync phase (phase = %d°)" , m_currentCfg.pdfs );

    WriteDENCReg(DENC_PDFS0, pdfs0);
    WriteDENCReg(DENC_PDFS1, pdfs1);

    TRC( TRC_ID_MAIN_INFO, "DENC_PDFS0 = %02X", ReadDENCReg(DENC_PDFS0) );
    TRC( TRC_ID_MAIN_INFO, "DENC_PDFS1 = %02X", ReadDENCReg(DENC_PDFS1) );
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CSTmDENC::ProgramDENCConfig(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_currentCfg.idfs)
  {
    WriteDENCReg(DENC_IDFS0, m_currentCfg.idfs->idfs0);
    WriteDENCReg(DENC_IDFS1, m_currentCfg.idfs->idfs1);
    WriteDENCReg(DENC_IDFS2, m_currentCfg.idfs->idfs2);
  }

  ProgramPDFSPALConfig();

  WriteDENCReg(DENC_CFG7, m_currentCfg.cfg[7]);
  WriteDENCReg(DENC_CFG0, m_currentCfg.cfg[0]);
  WriteDENCReg(DENC_CFG1, m_currentCfg.cfg[1]);
  WriteDENCReg(DENC_CFG2, m_currentCfg.cfg[2]);
  WriteDENCReg(DENC_CFG3, m_currentCfg.cfg[3]);
  WriteDENCReg(DENC_CFG4, m_currentCfg.cfg[4]);
  WriteDENCReg(DENC_CFG5, m_currentCfg.cfg[5]);
  WriteDENCReg(DENC_CFG8, m_currentCfg.cfg[8]);

  /*
   * Softreset the DENC; this is to make sure various settings are correctly
   * loaded. But the documentation claims that only Cfg0-8 are preserved
   * across reset. You might think that was a cut and paste error from a
   * previous version of the DENC (this one has 13 config registers), but
   * empirical evidence suggests otherwise. In particular the DAC output
   * settings (Cfg13) do not set reliably if done before the reset.
   */
  WriteDENCReg(DENC_CFG6,  m_currentCfg.cfg[6] | DENC_CFG6_RST_SOFT);
  vibe_os_stall_execution(10);

  WriteDENCReg(DENC_CFG10, m_currentCfg.cfg[10]);
  WriteDENCReg(DENC_CFG11, m_currentCfg.cfg[11]);
  WriteDENCReg(DENC_CFG12, m_currentCfg.cfg[12]);

  WriteDENCReg(DENC_CHROMA_DELAY, m_ComponentChromaDelay);
  WriteDENCReg(DENC_CHROMA_DELAY_EN, m_bEnableComponentChromaDelay?1:0);

  /*
   * Reset clears the VBI priority order bit for some reason, so we have to set
   * it again after reset, and we do want to set it to make sure Teletext gets
   * priority.
   */
  WriteDENCReg(DENC_CFG8, m_currentCfg.cfg[8]);

  WriteDENCReg(DENC_TTXCFG, m_currentCfg.ttxcfg);

  /*
   * Turn on brightness control for the MAIN video input only, disabling
   * it on the AUX input for those parts that have it connected. However
   * don't disturb the top bits of this register that effect the DAC2/Chroma
   * multiplying factor, as this is documented to have different scaling factors
   * and defaults on the STb7100 to both the STm8000 and STi5528.
   */
  uint8_t val = ReadDENCReg(DENC_CMTTX) & ~DENC_CMTTX_BCS_AUX_EN;
  val |= DENC_CMTTX_BCS_MAIN_EN;
  /*
   * Disable masking of some of the teletext framing code bits, to support
   * inverted EBU teletext output. The framing code must be correct in the
   * teletext data packet.
   */
  val |= DENC_CMTTX_FCODE_MASK_DISABLE;

  WriteDENCReg(DENC_CMTTX, val);

  /*
   * Set Teletext line configuration registers to default
   */
  WriteDENCReg(DENC_TTXL1, (0x4<<DENC_TTXL1_DEL_SHIFT));
  WriteDENCReg(DENC_TTXL2, 0);
  WriteDENCReg(DENC_TTXL3, 0);
  WriteDENCReg(DENC_TTXL4, 0);
  WriteDENCReg(DENC_TTXL5, 0);

  /*
   * Clear Closed Caption Registers
   */
  WriteDENCReg(DENC_CC10,0);
  WriteDENCReg(DENC_CC11,0);
  WriteDENCReg(DENC_CC20,0);
  WriteDENCReg(DENC_CC21,0);


  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmDENC::ProgramChromaFilter(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bEnableChromaFilter)
  {
    TRC( TRC_ID_MAIN_INFO, "Setting user defined filters" );

    WriteDENCReg(DENC_CHRM1, ((m_chromaFilterConfig.coeff[1]+32) & DENC_CHRM1_MASK) |
                             ((m_chromaFilterConfig.coeff[8] & (0x1<<8))>>2));

    WriteDENCReg(DENC_CHRM2, (m_chromaFilterConfig.coeff[2]+64) & DENC_CHRM2_MASK);
    WriteDENCReg(DENC_CHRM3, (m_chromaFilterConfig.coeff[3]+32) & DENC_CHRM3_MASK);
    WriteDENCReg(DENC_CHRM4, (m_chromaFilterConfig.coeff[4]+32) & DENC_CHRM4_MASK);
    WriteDENCReg(DENC_CHRM5, (m_chromaFilterConfig.coeff[5]+32) & DENC_CHRM5_MASK);
    WriteDENCReg(DENC_CHRM6, m_chromaFilterConfig.coeff[6]      & DENC_CHRM6_MASK);
    WriteDENCReg(DENC_CHRM7, m_chromaFilterConfig.coeff[7]      & DENC_CHRM7_MASK);
    WriteDENCReg(DENC_CHRM8, m_chromaFilterConfig.coeff[8]      & DENC_CHRM8_MASK);

    WriteDENCReg(DENC_CHRM0, DENC_CHRM0_FLT_S |
                             (((m_chromaFilterConfig.div>>1)&0x3)<<5) |
                             ((m_chromaFilterConfig.coeff[0]+16) & DENC_CHRM0_MASK));

    TRC( TRC_ID_MAIN_INFO, "DENC_CHRM0 = %x", ReadDENCReg(DENC_CHRM0) );
    TRC( TRC_ID_MAIN_INFO, "DENC_CHRM1 = %x", ReadDENCReg(DENC_CHRM1) );
    TRC( TRC_ID_MAIN_INFO, "DENC_CHRM2 = %x", ReadDENCReg(DENC_CHRM2) );
    TRC( TRC_ID_MAIN_INFO, "DENC_CHRM3 = %x", ReadDENCReg(DENC_CHRM3) );
    TRC( TRC_ID_MAIN_INFO, "DENC_CHRM4 = %x", ReadDENCReg(DENC_CHRM4) );
    TRC( TRC_ID_MAIN_INFO, "DENC_CHRM5 = %x", ReadDENCReg(DENC_CHRM5) );
    TRC( TRC_ID_MAIN_INFO, "DENC_CHRM6 = %x", ReadDENCReg(DENC_CHRM6) );
    TRC( TRC_ID_MAIN_INFO, "DENC_CHRM7 = %x", ReadDENCReg(DENC_CHRM7) );
    TRC( TRC_ID_MAIN_INFO, "DENC_CHRM8 = %x", ReadDENCReg(DENC_CHRM8) );
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Setting default filters" );
    /*
     * We must restore the default divisor here, otherwise the default filters
     * do not work correctly.
     */
    WriteDENCReg(DENC_CHRM0, DENC_CHRM0_DIV_1024);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmDENC::ProgramLumaFilter(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_currentCfg.cfg[9] = DENC_CFG9_DEL(m_CVBSChromaDelay);

  if(m_bEnableLumaFilter)
  {
    TRC( TRC_ID_MAIN_INFO, "Setting user defined filter" );

    WriteDENCReg(DENC_LUMA0, (m_lumaFilterConfig.coeff[0] & DENC_LUMA0_MASK) |
                             ((m_lumaFilterConfig.coeff[8] & (0x1<<8))>>3));
    WriteDENCReg(DENC_LUMA1, (m_lumaFilterConfig.coeff[1] & DENC_LUMA1_MASK) |
                             ((m_lumaFilterConfig.coeff[9] & (0x3<<8))>>2));
    WriteDENCReg(DENC_LUMA2, (m_lumaFilterConfig.coeff[2] & DENC_LUMA2_MASK) |
                             ((m_lumaFilterConfig.coeff[6] & (0x1<<8))>>1));
    WriteDENCReg(DENC_LUMA3, (m_lumaFilterConfig.coeff[3] & DENC_LUMA3_MASK) |
                             ((m_lumaFilterConfig.coeff[7] & (0x1<<8))>>1));

    WriteDENCReg(DENC_LUMA4, m_lumaFilterConfig.coeff[4] & DENC_LUMA4_MASK);
    WriteDENCReg(DENC_LUMA5, m_lumaFilterConfig.coeff[5] & DENC_LUMA5_MASK);
    WriteDENCReg(DENC_LUMA6, m_lumaFilterConfig.coeff[6] & DENC_LUMA6_MASK);
    WriteDENCReg(DENC_LUMA7, m_lumaFilterConfig.coeff[7] & DENC_LUMA7_MASK);
    WriteDENCReg(DENC_LUMA8, m_lumaFilterConfig.coeff[8] & DENC_LUMA8_MASK);
    WriteDENCReg(DENC_LUMA9, m_lumaFilterConfig.coeff[9] & DENC_LUMA9_MASK);

    TRC( TRC_ID_MAIN_INFO, "DENC_LUMA0 = %x", ReadDENCReg(DENC_LUMA0) );
    TRC( TRC_ID_MAIN_INFO, "DENC_LUMA1 = %x", ReadDENCReg(DENC_LUMA1) );
    TRC( TRC_ID_MAIN_INFO, "DENC_LUMA2 = %x", ReadDENCReg(DENC_LUMA2) );
    TRC( TRC_ID_MAIN_INFO, "DENC_LUMA3 = %x", ReadDENCReg(DENC_LUMA3) );
    TRC( TRC_ID_MAIN_INFO, "DENC_LUMA4 = %x", ReadDENCReg(DENC_LUMA4) );
    TRC( TRC_ID_MAIN_INFO, "DENC_LUMA5 = %x", ReadDENCReg(DENC_LUMA5) );
    TRC( TRC_ID_MAIN_INFO, "DENC_LUMA6 = %x", ReadDENCReg(DENC_LUMA6) );
    TRC( TRC_ID_MAIN_INFO, "DENC_LUMA7 = %x", ReadDENCReg(DENC_LUMA7) );
    TRC( TRC_ID_MAIN_INFO, "DENC_LUMA8 = %x", ReadDENCReg(DENC_LUMA8) );
    TRC( TRC_ID_MAIN_INFO, "DENC_LUMA9 = %x", ReadDENCReg(DENC_LUMA9) );

    m_currentCfg.cfg[9] |= (m_lumaFilterConfig.div<<1) | DENC_CFG9_FLT_YS;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Setting default filters" );
    m_currentCfg.cfg[9] |= DENC_CFG9_PLG_DIV_Y_512;
  }

  WriteDENCReg(DENC_CFG9,  m_currentCfg.cfg[9]);

  TRC( TRC_ID_MAIN_INFO, "DENC_CFG9 = 0x%x", (int)ReadDENCReg(DENC_CFG9) );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmDENC::Program625LineWSS(void)
{
  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program625LineWSS" );

  uint8_t wss1reg = 0;
  uint8_t wss0reg = 0;

    if(m_currentPictureInfo.teletext_subtitles == STM_TELETEXT_SUBTITLES_PRESENT)
      wss0reg |= DENC_WSS0_SUB_TELETEXT;

    wss0reg |= (uint8_t)m_currentPictureInfo.open_subtitles << DENC_WSS0_OPEN_SUBTITLE_SHIFT;

  switch (m_currentPictureInfo.video_aspect_ratio)
  {
    case STM_WSS_ASPECT_RATIO_UNKNOWN:
    {
      wss1reg = DENC_WSS1_NO_AFD;
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program625LineWSS OFF" );
      break;
    }
    case STM_WSS_ASPECT_RATIO_4_3:
    {
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program625LineWSS 4x3" );
      switch(m_currentPictureInfo.letterbox_style)
      {
        case STM_LETTERBOX_NONE:
        case STM_LETTERBOX_CENTER:
        case STM_LETTERBOX_TOP:
        case STM_SHOOT_AND_PROTECT_4_3:
          wss1reg = DENC_WSS1_4_3_FULL;
          break;
        case STM_SHOOT_AND_PROTECT_14_9:
          wss1reg = DENC_WSS1_4_3_SAP_14_9;
          break;
      }
      wss1reg = DENC_WSS1_4_3_FULL;
      break;
    }
    case STM_WSS_ASPECT_RATIO_16_9:
    {
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program625LineWSS 16x9" );
      switch(m_currentPictureInfo.letterbox_style)
      {
        case STM_LETTERBOX_NONE:
        case STM_SHOOT_AND_PROTECT_14_9:
        case STM_SHOOT_AND_PROTECT_4_3:
          wss1reg = DENC_WSS1_16_9_FULL;
          break;
        case STM_LETTERBOX_CENTER:
          wss1reg = DENC_WSS1_16_9_CENTER;
          break;
        case STM_LETTERBOX_TOP:
          wss1reg = DENC_WSS1_16_9_TOP;
          break;
      }
      break;
    }
    case STM_WSS_ASPECT_RATIO_14_9:
    {
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program625LineWSS 14x9" );
      switch(m_currentPictureInfo.letterbox_style)
      {
        case STM_LETTERBOX_NONE:
        case STM_SHOOT_AND_PROTECT_14_9:
        case STM_SHOOT_AND_PROTECT_4_3:
          wss1reg = DENC_WSS1_16_9_FULL;
          break;
        case STM_LETTERBOX_CENTER:
          wss1reg = DENC_WSS1_14_9_CENTER;
          break;
        case STM_LETTERBOX_TOP:
          wss1reg = DENC_WSS1_14_9_TOP;
          break;
      }
      break;
    }
    case STM_WSS_ASPECT_RATIO_GT_16_9:
    {
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program625LineWSS >16x9" );
      switch(m_currentPictureInfo.letterbox_style)
      {
        case STM_LETTERBOX_NONE:
        case STM_LETTERBOX_TOP:
        case STM_SHOOT_AND_PROTECT_14_9:
        case STM_SHOOT_AND_PROTECT_4_3:
          wss1reg = DENC_WSS1_16_9_FULL;
          break;
        case STM_LETTERBOX_CENTER:
          wss1reg = DENC_WSS1_GT_16_9_CENTER;
          break;
      }
      break;
    }
  }

  vibe_os_lock_resource(m_lock);
  {
    /*
     * 625line WSS only uses the bottom 2 bits of the Analogue CGMS value
     */
    if(m_CGMSAnalog)
      wss0reg |= (uint8_t)(m_CGMSAnalog & 0x3) << DENC_WSS0_CP_SHIFT;

    WriteDENCReg(DENC_WSS1, wss1reg);
    WriteDENCReg(DENC_WSS0, wss0reg);

    /*
     * Firstly make sure both 525 and 625 line WSS/CGMS insertion is disabled
     */
    m_currentCfg.cfg[3] &= ~(DENC_CFG3_ENA_CGMS | DENC_CFG3_ENA_WSS);

    /*
     * Only actually enable the HW insertion if we have been requested to
     * do WSS or if the CGMSA flags are non zero.
     */
    if(m_bEnableWSSInsertion || (m_CGMSAnalog != 0))
      m_currentCfg.cfg[3] |= DENC_CFG3_ENA_WSS;

    WriteDENCReg(DENC_CFG3, m_currentCfg.cfg[3]);
  }
  vibe_os_unlock_resource(m_lock);

  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program625LineWSS WSS bytes 0x%02x 0x%02x", (unsigned)wss0reg,(unsigned)wss1reg );
  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program625LineWSS DENC Cfg3 = 0x%02x", (unsigned)m_currentCfg.cfg[3] );
}


void CSTmDENC::Program525LineWSS(void)
{
  uint8_t cgms0=0,cgms1=0,cgms2=0;

  vibe_os_lock_resource(m_lock);
  {
    /*
     * Firstly make sure both 525 and 625 line WSS/CGMS insertion is disabled
     */
    m_currentCfg.cfg[3] &= ~(DENC_CFG3_ENA_CGMS | DENC_CFG3_ENA_WSS);

    /*
     * Only actually enable the HW insertion if we have been requested to
     * do WSS or if the CGMSA flags are non zero.
     */
    if(m_bEnableWSSInsertion || (m_CGMSAnalog != 0))
      m_currentCfg.cfg[3] |= DENC_CFG3_ENA_CGMS;

    if(m_CGMSAnalog)
    {
      cgms1 |= (m_CGMSAnalog      & 0x1) << DENC_CGMS1_IEC61880_B7_SHIFT;
      cgms1 |= ((m_CGMSAnalog>>1) & 0x1) << DENC_CGMS1_IEC61880_B8_SHIFT;
      cgms1 |= ((m_CGMSAnalog>>2) & 0x1) << DENC_CGMS1_IEC61880_B9_SHIFT;
      cgms1 |= ((m_CGMSAnalog>>3) & 0x1) << DENC_CGMS1_IEC61880_B10_SHIFT;
      cgms1 |= ((m_CGMSAnalog>>4) & 0x1) << DENC_CGMS1_IEC61880_B11_SHIFT;
    }
  }
  vibe_os_unlock_resource(m_lock);

  switch (m_currentPictureInfo.video_aspect_ratio)
  {
    case STM_WSS_ASPECT_RATIO_UNKNOWN:
    {
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program525LineWSS OFF" );
      break;
    }
    case STM_WSS_ASPECT_RATIO_4_3:
    {
      cgms0 = DENC_CGMS0_4_3 | ((m_currentPictureInfo.letterbox_style>STM_LETTERBOX_NONE)?DENC_CGMS0_LETTERBOX:DENC_CGMS0_NORMAL);
      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program525LineWSS 4x3" );
      break;
    }
    case STM_WSS_ASPECT_RATIO_16_9:
    case STM_WSS_ASPECT_RATIO_14_9:
    case STM_WSS_ASPECT_RATIO_GT_16_9:
    {
      cgms0 = DENC_CGMS0_16_9 | DENC_CGMS0_NORMAL;

      TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program525LineWSS >=16x9 or 14x9" );
      break;
    }
  }


  /*
   * CRC generation. X^6 + X + 1, all preset to 1.
   */
  uint32_t crcreg = 0x3f;

  TRC( TRC_ID_MAIN_INFO, "CRC: reg = 0x%x", crcreg );

  for(int bit=3;bit>=0;bit--)
  {
    int newbit = (crcreg & 0x1) ^ ((cgms0 >> bit) & 0x1);

    crcreg >>= 1;
    crcreg ^= newbit << 4;
    crcreg |= newbit << 5;

    TRC( TRC_ID_MAIN_INFO, "CRC: reg = 0x%x", crcreg );
  }

  for(int bit=7;bit>=0;bit--)
  {
    int newbit = (crcreg & 0x1) ^ ((cgms1 >> bit) & 0x1);

    crcreg >>= 1;
    crcreg ^= newbit << 4;
    crcreg |= newbit << 5;

    TRC( TRC_ID_MAIN_INFO, "CRC: reg = 0x%x", crcreg );
  }

  for(int bit=7;bit>=6;bit--)
  {
    int newbit = (crcreg & 0x1) ^ ((cgms2 >> bit) & 0x1);

    crcreg >>= 1;
    crcreg ^= newbit << 4;
    crcreg |= newbit << 5;

    TRC( TRC_ID_MAIN_INFO, "CRC: reg = 0x%x", crcreg );
  }

  for(int bit=5;bit>=0;bit--)
  {
    int newbit = crcreg & 0x1;

    crcreg >>= 1;
    cgms2 |= (newbit << bit);
  }

  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program525LineWSS CGMS bytes 0x%02x 0x%02x 0x%02x", (unsigned)cgms0,(unsigned)cgms1,(unsigned)cgms2 );
  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::Program525LineWSS DENC Cfg3 = 0x%02x", (unsigned)m_currentCfg.cfg[3] );

  vibe_os_lock_resource(m_lock);
  {
    WriteDENCReg(DENC_CGMS0, cgms0);
    WriteDENCReg(DENC_CGMS1, cgms1);
    WriteDENCReg(DENC_CGMS2, cgms2);
    WriteDENCReg(DENC_CFG3,  m_currentCfg.cfg[3]);
  }
  vibe_os_unlock_resource(m_lock);
}


inline uint8_t xds_data_parity(uint8_t data)
{
  /*
   * The xds data transmission is 7bit data + odd parity bit.
   */
  uint8_t parity = 1;
  for(int i=0;i<7;i++)
  {
    parity ^= (data>>i) & 0x1;
  }
  return parity<<7;
}


void CSTmDENC::SetTeletextLineSize(void)
{
  /*
   * Numbers from video demystified, includes the clock run in and framing code
   */
  switch(m_teletextStandard)
  {
    case DENC_TTXCFG_SYSTEM_A:
      /*
       * System A is only valid in 625 line modes.
       */
      m_teletextLineSize = (m_CurrentMode.mode_timing.lines_per_frame==625)?40:0;
      break;
    case DENC_TTXCFG_SYSTEM_B:
      m_teletextLineSize = (m_CurrentMode.mode_timing.lines_per_frame==625)?45:37;
      break;
    case DENC_TTXCFG_SYSTEM_C:
      m_teletextLineSize = 36;
      break;
    case DENC_TTXCFG_SYSTEM_D:
      m_teletextLineSize = 37;
      break;
  }
}


void CSTmDENC::CalculateDACScaling(void)
{
  const uint32_t max_10bit_value = 1023;
  uint32_t DAC_target = DIV_ROUNDED_UP((m_DAC123Saturation * m_DisplayStandardMaxVoltage), m_maxDAC123Voltage);
  uint32_t percentage_of_max_voltage = DIV_ROUNDED_UP((DAC_target*100000),max_10bit_value); // scale up to give 3 decimal places of precision

  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::CalculateDACScaling DAC123 MaxV = %u Saturation = %u", m_maxDAC123Voltage,m_DAC123Saturation );
  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::CalculateDACScaling DAC123 Tv = %u.%u%%", percentage_of_max_voltage/1000,percentage_of_max_voltage%1000 );

  if(percentage_of_max_voltage > m_MinRGBScalePercentage)
    m_DAC123RGBScale = (uint8_t)DIV_ROUNDED((percentage_of_max_voltage - m_MinRGBScalePercentage), m_RGBScaleStep);
  else
    m_DAC123RGBScale = 0;

  if(percentage_of_max_voltage > m_MinYScalePercentage)
    m_DAC123YScale = (uint8_t)DIV_ROUNDED((percentage_of_max_voltage - m_MinYScalePercentage), m_YScaleStep);
  else
    m_DAC123YScale = 0;

  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::CalculateDACScaling DAC123 RGBScale = 0x%02x YScale = 0x%02x", (uint32_t)m_DAC123RGBScale,(uint32_t)m_DAC123YScale );

  DAC_target = DIV_ROUNDED((m_DAC456Saturation * m_DisplayStandardMaxVoltage), m_maxDAC456Voltage);
  percentage_of_max_voltage = DIV_ROUNDED((DAC_target*100000),max_10bit_value); // scale up to give 3 decimal places of precision

  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::CalculateDACScaling DAC456 MaxV = %u Saturation = %u", m_maxDAC456Voltage,m_DAC456Saturation );
  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::CalculateDACScaling DAC456 Tv = %u.%u%%", percentage_of_max_voltage/1000,percentage_of_max_voltage%1000 );

  if(percentage_of_max_voltage > m_MinRGBScalePercentage)
    m_DAC456RGBScale = (uint8_t)DIV_ROUNDED((percentage_of_max_voltage - m_MinRGBScalePercentage), m_RGBScaleStep);
  else
    m_DAC456RGBScale = 0;

  if(percentage_of_max_voltage > m_MinYScalePercentage)
    m_DAC456YScale = (uint8_t)DIV_ROUNDED((percentage_of_max_voltage - m_MinYScalePercentage), m_YScaleStep);
  else
    m_DAC456YScale = 0;

  TRC( TRC_ID_MAIN_INFO, "CSTmDENC::CalculateDACScaling DAC456 RGBScale = 0x%02x YScale = 0x%02x", (uint32_t)m_DAC456RGBScale,(uint32_t)m_DAC456YScale );
}


stm_display_metadata_result_t CSTmDENC::QueueMetadata(stm_display_metadata_t *m)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  switch(m->type)
  {
    case STM_METADATA_TYPE_PICTURE_INFO:
      return m_pPictureInfoQueue->Queue(m);

    case STM_METADATA_TYPE_CLOSED_CAPTION:
      return m_pClosedCaptionQueue->Queue(m);

    case STM_METADATA_TYPE_TELETEXT:
      if(m_pTeletext)
        return m_pTeletext->QueueMetadata(m);

      break;

    default:
      break;
  }
  return STM_METADATA_RES_UNSUPPORTED_TYPE;
}


void CSTmDENC::FlushMetadata(stm_display_metadata_type_t type)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  switch(type)
  {
    case STM_METADATA_TYPE_PICTURE_INFO:
      m_pPictureInfoQueue->Flush();
      break;

    case STM_METADATA_TYPE_TELETEXT:
      if(m_pTeletext)
        m_pTeletext->FlushMetadata(type);

      break;

    default:
      break;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmDENC::UpdateHW(uint32_t timingevent)
{
  stm_display_metadata_t    *m;
  stm_picture_format_info_t *p;
  stm_closed_caption_t      *c;
  uint8_t buffer_status = 0;
  int lines_per_frame;

  vibe_os_lock_resource(m_lock);
  {
    if(m_CurrentMode.mode_id == STM_TIMING_MODE_RESERVED)
    {
      vibe_os_unlock_resource(m_lock);
      return;
    }

    lines_per_frame = m_CurrentMode.mode_timing.lines_per_frame;
  }
  vibe_os_unlock_resource(m_lock);
  if((m = m_pPictureInfoQueue->Pop()) != 0)
  {
    p = (stm_picture_format_info_t*)&m->data[0];
    if(p->flags & STM_PIC_INFO_LETTERBOX_STYLE)
    {
      m_currentPictureInfo.letterbox_style = p->letterbox_style;
      m_bUpdateWSS = true;
    }
    if(p->flags & STM_PIC_INFO_PICTURE_ASPECT)
    {
      m_currentPictureInfo.picture_aspect_ratio = p->picture_aspect_ratio;
      m_bUpdateWSS = true;
    }
    if(p->flags & STM_PIC_INFO_VIDEO_ASPECT)
    {
      m_currentPictureInfo.video_aspect_ratio = p->video_aspect_ratio;
      m_bUpdateWSS = true;
    }
    if(p->flags & STM_PIC_INFO_TELETEXT_SUBTITLES)
    {
      m_currentPictureInfo.teletext_subtitles = p->teletext_subtitles;
      m_bUpdateWSS = true;
    }
    if(p->flags & STM_PIC_INFO_OPEN_SUBTITLES)
    {
      m_currentPictureInfo.open_subtitles = p->open_subtitles;
      m_bUpdateWSS = true;
    }

    stm_meta_data_release(m);
  }

  if(m_bEnableCCField)
  {
    buffer_status = ReadDENCReg(DENC_STA);
    if(((buffer_status & DENC_STA_BUFF1_FREED) && (m_bEnableCCField & DENC_CFG1_CC_ENA_F1))
    || ((buffer_status & DENC_STA_BUFF2_FREED) && (m_bEnableCCField & DENC_CFG1_CC_ENA_F2)))
    {
      if((m = m_pClosedCaptionQueue->Pop()) != 0)
      {
        c = (stm_closed_caption_t*)&m->data[0];
        if(c->field == 0)
        {
          WriteDENCReg(DENC_CFL1,c->lines_field);
          WriteDENCReg(DENC_CC10,c->data[0]);
          WriteDENCReg(DENC_CC11,c->data[1]);
        }

        if(c->field == 1)
        {
          WriteDENCReg(DENC_CFL2,c->lines_field);
          WriteDENCReg(DENC_CC20,c->data[0]);
          WriteDENCReg(DENC_CC21,c->data[1]);
        }

        stm_meta_data_release(m);
      }
    }
  }

  /*
   * Remember that other controls may also cause an update.
   */
  if(m_bUpdateWSS)
  {
    m_bUpdateWSS = false;
    if(lines_per_frame == 625)
    {
      Program625LineWSS();
    }
    else
    {
      Program525LineWSS();
    }
  }

  /*
   * If we have a Teletext processor attached to this DENC, let it update too.
   */
  if(m_pTeletext)
    m_pTeletext->UpdateHW();
}
