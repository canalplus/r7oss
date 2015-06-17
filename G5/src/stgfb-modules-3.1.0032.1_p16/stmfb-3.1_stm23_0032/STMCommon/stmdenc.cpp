/***********************************************************************
 *
 * File: STMCommon/stmdenc.cpp
 * Copyright (c) 2000-2008 STMicroelectronics Limited.
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
#include <Generic/MetaDataQueue.h>

#include "stmdencregs.h"
#include "stmdenc.h"
#include "stmteletext.h"

/*
 * DENC subcarrier frequency synthesizer setup for various modes where the
 * default DENC setup is not sufficient.
 */
static const stm_denc_idfs ntsc443    = { 0x2A, 0x09, 0x8B };
static const stm_denc_idfs ntsc60hz   = { 0x21, 0xFC, 0x7C };
static const stm_denc_idfs palnc_sq   = { 0x1F, 0x15, 0xC0 };


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
static const ULONG PAL_SECAM_MAX_VOLTAGE = 1279;
static const ULONG NTSC_MAX_VOLTAGE      = 1305;

/////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSTmDENC::CSTmDENC(CDisplayDevice *pDev,
                   ULONG           ulDencOffset,
                   CSTmTeletext   *pTeletext)
{
  DENTRY();

  m_pDev      = pDev;
  m_pDENCReg  = ((ULONG*)m_pDev->GetCtrlRegisterBase()) + (ulDencOffset>>2);

  m_pTeletext             = pTeletext;
  m_mainOutputFormat      = 0;
  m_auxOutputFormat       = 0;

  m_pCurrentMode          = 0;
  m_ulTVStandard          = 0;

  /*
   * Some or all of these might be overridden for specific chips and boards
   */
  m_maxDAC123Voltage         = NTSC_MAX_VOLTAGE;
  m_maxDAC456Voltage         = NTSC_MAX_VOLTAGE;
  m_DAC123Saturation         = 1023;  // Full 10bit DAC range by default.
  m_DAC456Saturation         = 1023;  // Full 10bit DAC range by default.

  m_DisplayStandardMaxVoltage= NTSC_MAX_VOLTAGE;

  /*
   * By default disable the CVBS luma trap filter. This can be turned on
   * using the STG_CTRL_CVBS_FILTER control.
   */
  m_bEnableTrapFilter   = false;
  m_bEnableLumaFilter   = false;
  m_bEnableChromaFilter = false;
  m_bFreerunEnabled     = false;

  /*
   * Default chroma/luma delay for V13 DENC and YCrCb444 input is -1
   */
  m_bUseCVBSDelay       = true;
  m_CVBSDelay           = DENC_CFG9_DEL_M_1_0;
  m_RGBDelay            = DENC_CFG4_SYOUT_0;

  g_pIOS->ZeroMemory(&m_currentPictureInfo, sizeof(stm_picture_format_info_t));
  m_currentPictureInfo.videoAspect    = STM_WSS_OFF;
  m_currentPictureInfo.pictureAspect  = STM_WSS_4_3;
  m_currentPictureInfo.letterboxStyle = STM_LETTERBOX_NONE;
  m_pPictureInfoQueue   = 0;

  m_OpenSubtitles       = STM_SUBTITLES_NONE;
  m_bTeletextSubtitles  = false;
  m_CGMSAnalog          = 0;
  m_bPrerecordedAnalogueSource = false;
  m_bUpdateWSS          = false;
  m_teletextStandard    = DENC_TTXCFG_SYSTEM_A;
  m_teletextLineSize    = 0; /* depends on output standard */

  /*
   * Specific support for CGMS-A over closed caption XDS channel.
   */
  m_CGMSA_XDS_Packet[0] = 0x1; // Start XDS packet for current program
  m_CGMSA_XDS_Packet[1] = 0x8; // CGMSA packet type
  m_CGMSA_XDS_Packet[2] = 0x0; // CGMSA data
  m_CGMSA_XDS_Packet[3] = 0x40; // From CEA-608-C with RCD set to 0
  m_CGMSA_XDS_Packet[4] = 0x8F; // End XDS packet + odd parity bit
  m_CGMSA_XDS_Packet[5] = 0x0; // Checksum
  m_CGMSA_XDS_SendIndex = 0;
  m_bEnableCCXDS        = true;

  m_ulBrightness        = 0x80;
  m_ulSaturation        = 0x80;
  m_ulContrast          = 0x80;
  m_bHasHueControl      = true;
  m_ulHue               = 0x80;

  m_signalRange         = STM_SIGNAL_FILTER_SAV_EAV;

  g_pIOS->ZeroMemory(&m_currentCfg,sizeof(struct stm_denc_cfg));

  DEXIT();
}


CSTmDENC::~CSTmDENC()
{
  delete m_pPictureInfoQueue;
}


bool CSTmDENC::Create(void)
{
  DENTRY();
  /*
   * Create a 2 element metadata queue for picture information that returns its
   * data 1 vsync before its presentation time.
   */
  m_pPictureInfoQueue = new CMetaDataQueue(STM_METADATA_TYPE_PICTURE_INFO,2,1);
  if(!m_pPictureInfoQueue || !m_pPictureInfoQueue->Create())
  {
    DERROR("Unable to create picture info queue\n");
    return false;
  }

  DEXIT();
  return true;
}


bool CSTmDENC::SetMainOutputFormat(ULONG format)
{
  static const ULONG componentFormats = (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV);

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


bool CSTmDENC::SetAuxOutputFormat(ULONG format)
{
  static const ULONG componentFormats = (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV);

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


ULONG CSTmDENC::SupportedControls(void) const
{
  ULONG caps = (STM_CTRL_CAPS_BRIGHTNESS     |
                STM_CTRL_CAPS_SATURATION     |
                STM_CTRL_CAPS_CONTRAST       |
                STM_CTRL_CAPS_DENC_FILTERS   |
                STM_CTRL_CAPS_625LINE_VBI_23 |
                STM_CTRL_CAPS_CGMS           |
                STM_CTRL_CAPS_DAC_VOLTAGE    |
                STM_CTRL_CAPS_SIGNAL_RANGE);

  if(m_pTeletext)
    caps |= STM_CTRL_CAPS_TELETEXT;

  if(m_bHasHueControl)
    caps |= STM_CTRL_CAPS_HUE;

  return caps;
}


void CSTmDENC::ProgramPSIControls()
{
  DENTRY();

  DEBUGF2(2,("%s: brightness = 0x%lx\n", __PRETTY_FUNCTION__, m_ulBrightness));
  WriteDENCReg(DENC_BRIGHT,m_ulBrightness);

  DEBUGF2(2,("%s: saturation = 0x%lx\n", __PRETTY_FUNCTION__, m_ulSaturation));
  WriteDENCReg(DENC_SATURA,m_ulSaturation);

  /*
   * Convert input 0-255 to 8bit signed -128 to 127
   */
  signed char val = (int)m_ulContrast - 128;
  WriteDENCReg(DENC_CONTRA,(UCHAR)val);

  DEBUGF2(2,("%s: contrast = 0x%lx\n",   __PRETTY_FUNCTION__, (ULONG)val));

  /*
   * HUE control is not available in SECAM, the Phase registers that
   * contain the enable bit have a different meaning when SECAM is enabled.
   */
  if(m_bHasHueControl && (m_ulTVStandard != STM_OUTPUT_STD_SECAM))
  {
    UCHAR hue;
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
    DEBUGF2(2,("%s: hue = 0x%lx\n",   __PRETTY_FUNCTION__, (ULONG)hue));

    UCHAR reg = ReadDENCReg(DENC_PDFS0) | DENC_DFS_PHASE0_HUE_EN;

    /*
     * We only need to set HUE_EN, it gets automatically reset to zero by
     * the HW once the value has been accepted.
     */
    WriteDENCReg(DENC_PDFS0, reg);
  }

  DEXIT();
}


void CSTmDENC::SetPSIControl(ULONG *ctrl, ULONG ulNewVal)
{
  DENTRY();

  if(ulNewVal>255)
    ulNewVal = 255;

  if(*ctrl != ulNewVal)
  {
    *ctrl = ulNewVal;
    ProgramPSIControls();
  }

  DEXIT();
}


void CSTmDENC::SetControl(stm_output_control_t ctrl, ULONG ulNewVal)
{
  switch(ctrl)
  {
    case STM_CTRL_BRIGHTNESS:
    {
      SetPSIControl(&m_ulBrightness,ulNewVal);
      break;
    }
    case STM_CTRL_SATURATION:
    {
      SetPSIControl(&m_ulSaturation,ulNewVal);
      break;
    }
    case STM_CTRL_CONTRAST:
    {
      SetPSIControl(&m_ulContrast,ulNewVal);
      break;
    }
    case STM_CTRL_HUE:
    {
      /*
       * The hue input value is ranged from 1 to 255, with the centre
       * (0 phase shift) at 128.
       */
      if(ulNewVal < 1)
        ulNewVal = 1;

      SetPSIControl(&m_ulHue,ulNewVal);
      break;
    }
    case STM_CTRL_CVBS_FILTER:
    {
      m_bEnableTrapFilter = (ulNewVal != 0);
      DEBUGF2(2,("CSTmDENC::SetControl Setting trap filter %s\n",m_bEnableTrapFilter?"on":"off"));

      if(m_pCurrentMode)
      {
        if(m_bEnableTrapFilter)
          m_currentCfg.cfg[3] |= DENC_CFG3_ENA_TRFLT;
        else
          m_currentCfg.cfg[3] &= ~DENC_CFG3_ENA_TRFLT;

        WriteDENCReg(DENC_CFG3, m_currentCfg.cfg[3]);
      }
      break;
    }
    case STM_CTRL_TELETEXT_SUBTITLES:
    {
      DEBUGF2(2,("CSTmDENC::SetControl Setting 625line WSS teletext subtitles  %s\n",m_bTeletextSubtitles?"on":"off"));
      m_bTeletextSubtitles = (ulNewVal != 0);
      m_bUpdateWSS = true;
      break;
    }
    case STM_CTRL_OPEN_SUBTITLES:
    {
      if(ulNewVal > STM_SUBTITLES_OUTSIDE_ACTIVE_VIDEO)
      {
        DEBUGF2(1,("CSTmDENC::SetControl Open subtitle mode out of range (%lu)\n",ulNewVal));
        return;
      }

      DEBUGF2(2,("CSTmDENC::SetControl Setting Open Subtitles %d\n",m_OpenSubtitles));
      m_OpenSubtitles = (stm_open_subtitles_t)ulNewVal;
      m_bUpdateWSS = true;
      break;
    }
    case STM_CTRL_CGMS_ANALOG:
    {
      DEBUGF2(2,("CSTmDENC::SetControl Setting CGMS 0x%lx\n",m_CGMSAnalog));
      m_CGMSAnalog = (ulNewVal & 0xF);
      m_bUpdateWSS = true;
      break;
    }
    case STM_CTRL_PRERECORDED_ANALOGUE_SOURCE:
    {
      DEBUGF2(2,("CSTmDENC::SetControl Setting CGMS prerecorded analogue source %s\n",m_bPrerecordedAnalogueSource?"on":"off"));
      m_bPrerecordedAnalogueSource = (ulNewVal != 0);
      m_bUpdateWSS = true;
      break;
    }
    case STM_CTRL_DAC123_MAX_VOLTAGE:
    {
      m_maxDAC123Voltage = ulNewVal;
      DEBUGF2(2,("CSTmDENC::SetControl DAC123 MaxV = %lu\n",m_maxDAC123Voltage));
      CalculateDACScaling();
      break;
    }
    case STM_CTRL_DAC123_SATURATION_POINT:
    {
      m_DAC123Saturation = ulNewVal;
      DEBUGF2(2,("CSTmDENC::SetControl DAC123 Saturation = %lu\n",m_DAC123Saturation));
      CalculateDACScaling();
      break;
    }
    case STM_CTRL_DAC456_MAX_VOLTAGE:
    {
      m_maxDAC456Voltage = ulNewVal;
      DEBUGF2(2,("CSTmDENC::SetControl DAC456 MaxV = %lu\n",m_maxDAC456Voltage));
      CalculateDACScaling();
      break;
    }
    case STM_CTRL_DAC456_SATURATION_POINT:
    {
      m_DAC456Saturation = ulNewVal;
      DEBUGF2(2,("CSTmDENC::SetControl DAC456 Saturation = %lu\n",m_DAC456Saturation));
      CalculateDACScaling();
      break;
    }
    case STM_CTRL_SIGNAL_RANGE:
    {
      if(ulNewVal> STM_SIGNAL_VIDEO_RANGE)
        break;

      if(ulNewVal == STM_SIGNAL_FULL_RANGE)
        m_signalRange = STM_SIGNAL_FILTER_SAV_EAV;
      else
        m_signalRange = (stm_display_signal_range_t)ulNewVal;

      if(m_pCurrentMode)
      {
        if(m_signalRange == STM_SIGNAL_FILTER_SAV_EAV)
        {
          DEBUGF2(2,("CSTmDENC::SetControl Filter SAV/EAV values\n"));
          m_currentCfg.cfg[6] |= DENC_CFG6_MAX_DYN;
        }
        else
        {
          DEBUGF2(2,("CSTmDENC::SetControl Clamp input to 16-235/240\n"));
          m_currentCfg.cfg[6] &= ~DENC_CFG6_MAX_DYN;
        }

        WriteDENCReg(DENC_CFG6, m_currentCfg.cfg[6]);
      }
      break;
    }
    case STM_CTRL_TELETEXT_SYSTEM:
    {
      if(ulNewVal>STM_TELETEXT_SYSTEM_D)
        break;

      if(m_pTeletext)
      {
        m_teletextStandard  = (UCHAR)(ulNewVal << DENC_TTXCFG_SYSTEM_SHIFT);
        m_currentCfg.ttxcfg &= ~DENC_TTXCFG_SYSTEM_MASK;
        m_currentCfg.ttxcfg |= m_teletextStandard;

        if(m_pCurrentMode)
        {
          SetTeletextLineSize();
          m_pTeletext->Stop();
          WriteDENCReg(DENC_TTXCFG, m_currentCfg.ttxcfg);
          m_pTeletext->Start(m_teletextLineSize, m_pParent);
        }
      }
      break;
    }
    default:
      break;
  }
}


ULONG CSTmDENC::GetControl(stm_output_control_t ctrl) const
{
  ULONG	ulVal = 0;

  switch(ctrl)
  {
    case STM_CTRL_BRIGHTNESS:
      ulVal = m_ulBrightness;
      break;
    case STM_CTRL_SATURATION:
      ulVal = m_ulSaturation;
      break;
    case STM_CTRL_CONTRAST:
      ulVal = m_ulContrast;
      break;
    case STM_CTRL_HUE:
      ulVal = m_ulHue;
      break;
    case STM_CTRL_CVBS_FILTER:
      ulVal = m_bEnableTrapFilter?1:0;
      break;
    case STM_CTRL_OPEN_SUBTITLES:
      ulVal = (ULONG)m_OpenSubtitles;
      break;
    case STM_CTRL_TELETEXT_SUBTITLES:
      ulVal = m_bTeletextSubtitles?1:0;
      break;
    case STM_CTRL_CGMS_ANALOG:
      ulVal = m_CGMSAnalog;
      break;
    case STM_CTRL_PRERECORDED_ANALOGUE_SOURCE:
      ulVal = m_bPrerecordedAnalogueSource?1:0;
      break;
    case STM_CTRL_DAC123_MAX_VOLTAGE:
      ulVal = m_maxDAC123Voltage;
      break;
    case STM_CTRL_DAC123_SATURATION_POINT:
      ulVal = m_DAC123Saturation;
      break;
    case STM_CTRL_DAC456_MAX_VOLTAGE:
      ulVal = m_maxDAC456Voltage;
      break;
    case STM_CTRL_DAC456_SATURATION_POINT:
      ulVal = m_DAC456Saturation;
      break;
    case STM_CTRL_SIGNAL_RANGE:
      ulVal = (ULONG)m_signalRange;
      break;
    case STM_CTRL_TELETEXT_SYSTEM:
      if(m_pTeletext)
        ulVal = (ULONG)(m_teletextStandard >> DENC_TTXCFG_SYSTEM_SHIFT);
    default:
      break;
  }

  return ulVal;
}


bool CSTmDENC::SetFilterCoefficients(const stm_display_filter_setup_t *f)
{
  bool ret = true;
  DENTRY();
  switch(f->type)
  {
    case DENC_COEFF_LUMA:
    {
      if(f->denc.div != STM_FLT_DISABLED)
      {
        DEBUGF2(2,("%s: Enabling Luma Filter\n",__PRETTY_FUNCTION__));
        m_lumaFilterConfig = f->denc;

#if defined(DEBUG)
        int sum = 0;
        for(int i=0;i<9;i++)
        {
          DEBUGF2(2,("luma filter coeff[%d] = %d\n",i,m_lumaFilterConfig.coeff[i]));
          sum += 2*m_lumaFilterConfig.coeff[i];
        }

        DEBUGF2(2,("luma filter coeff[9] = %d\n",m_lumaFilterConfig.coeff[9]));
        sum += m_lumaFilterConfig.coeff[9];

        DEBUGF2(2,("luma filter sum = %d\n",sum));
#endif

        m_bEnableLumaFilter = true;
      }
      else
      {
        DEBUGF2(2,("%s: Disabling Luma Filter\n",__PRETTY_FUNCTION__));
        m_bEnableChromaFilter = false;
      }

      if(m_pCurrentMode)
        ProgramLumaFilter();

      break;
    }
    case DENC_COEFF_CHROMA:
    {
      if(f->denc.div != STM_FLT_DISABLED)
      {
        DEBUGF2(2,("%s: Enabling Chroma Filter\n",__PRETTY_FUNCTION__));
        m_chromaFilterConfig = f->denc;

#if defined(DEBUG)
        int sum = 0;
        for(int i=0;i<8;i++)
        {
          DEBUGF2(2,("chroma filter coeff[%d] = %d\n",i,m_chromaFilterConfig.coeff[i]));
          sum += 2*m_chromaFilterConfig.coeff[i];
        }

        DEBUGF2(2,("chroma filter coeff[8] = %d\n",m_chromaFilterConfig.coeff[8]));
        sum += m_chromaFilterConfig.coeff[8];

        DEBUGF2(2,("chroma filter sum = %d\n",sum));
#endif

        m_bEnableChromaFilter = true;
      }
      else
      {
        DEBUGF2(2,("%s: Disabling Chroma Filter\n",__PRETTY_FUNCTION__));
        m_bEnableChromaFilter = false;
      }

      if(m_pCurrentMode)
        ProgramChromaFilter();

      break;
    }
    default:
      ret = false;
      break;
  }

  DEXIT();
  return ret;
}


bool CSTmDENC::Start(COutput *parent,const stm_mode_line_t *pModeLine, ULONG tvStandard)
{
  return Start(parent, pModeLine, tvStandard, DENC_CFG0_ODHS_SLV);
}


bool CSTmDENC::Start(COutput               *parent,
                     const stm_mode_line_t *pModeLine,
                     ULONG                  tvStandard,
                     UCHAR                  syncMode,
                     stm_vtg_sync_type_t    href,
                     stm_vtg_sync_type_t    vref)
{
  DEBUGF2(2,("CSTmDENC::Start - DENC ID = %#.2x\n", ReadDENCReg(DENC_CID)));

  if(!parent)
    return false;

  m_pParent = parent;

  /*
   * Strip out SMPTE293M bit to get the mode for the re-interlaced DENC output
   * This was mostly for the STm8000 (no longer supported), but other chips
   * could do this in the future.
   */
  tvStandard = tvStandard & STM_OUTPUT_STD_SD_MASK;

  if(tvStandard == 0)
    return false;

  if((pModeLine->ModeParams.OutputStandards & tvStandard) != tvStandard)
  {
    DEBUGF2(2,("CSTmDENC::Start the requested TV standard is not valid\n"));
    return false;
  }

  bool bIsSquarePixel = (pModeLine->ModeParams.PixelAspectRatios[AR_INDEX_4_3].numerator == 1 &&
                         pModeLine->ModeParams.PixelAspectRatios[AR_INDEX_4_3].denominator == 1);

  m_currentCfg.cfg[0] = GetSyncConfiguration(pModeLine, syncMode, href, vref);
  m_currentCfg.cfg[1] = 0;
  m_currentCfg.cfg[2] = DENC_CFG2_ENA_BURST | DENC_CFG2_ENA_RST;
  m_currentCfg.cfg[3] = m_bUseCVBSDelay?DENC_CFG3_DELAY_ENABLE:0;
  m_currentCfg.cfg[4] = m_RGBDelay | DENC_CFG4_SYIN_0 | DENC_CFG4_ALINE;
  m_currentCfg.cfg[5] = DENC_CFG5_DAC_NONINV;

  /*
   * The Cfg6 Man mode bit is default/reserved and should not to be changed on
   * modern DENCS according to the datasheet.
   */
  m_currentCfg.cfg[6] = DENC_CFG6_MAN_MODE;

  if(m_signalRange == STM_SIGNAL_FILTER_SAV_EAV)
    m_currentCfg.cfg[6] |= DENC_CFG6_MAX_DYN;

  m_currentCfg.cfg[7]  = 0;
  m_currentCfg.cfg[8]  = DENC_CFG8_TTX_NOTMV | DENC_CFG8_RESRV_BIT; /* Reserved bit must be set according to datasheet */
  m_currentCfg.cfg[10] = DENC_CFG10_RGBSAT_EN;

  m_currentCfg.ttxcfg = DENC_TTXCFG_RESERVED_BIT_4 | m_teletextStandard; /* Reserved bit must be set according to datasheets */

  /*
   * Set the auxilary path chroma delay to be the same as the main path,
   * but see SECAM settings below.
   *
   * Thankfully the Cfg9 delay bits are identical to the Cfg11 delay bits
   * Additonally the Cfg11 reserved bit is set by default in the datasheets
   */
  m_currentCfg.cfg[11] = DENC_CFG11_RESERVED | m_CVBSDelay;
  m_currentCfg.cfg[12] = m_bUseCVBSDelay?DENC_CFG12_AUX_DEL_EN:0;

  m_currentCfg.idfs    = 0;

  if(m_bEnableTrapFilter)
  {
    DEBUGF2(2,("CSTmDENC::Start Enabling Trap Filter\n"));
    m_currentCfg.cfg[3]  |= DENC_CFG3_ENA_TRFLT;
    m_currentCfg.cfg[12] |= DENC_CFG12_AUX_ENTRAP;
  }

  switch(tvStandard)
  {
  case STM_OUTPUT_STD_PAL_BDGHI:
    DEBUGF2(2,("CSTmDENC::Start TV Standard is PAL-BDGHI\n"));
    m_currentCfg.cfg[0]  |= DENC_CFG0_PAL_BDGHI;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_19;
    m_currentCfg.cfg[2]  |= DENC_CFG2_RST_2F;
    m_currentCfg.cfg[3]  |= DENC_CFG3_TRAP_443;
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_19_FLT;

    m_DisplayStandardMaxVoltage = PAL_SECAM_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_PAL_N:
    DEBUGF2(2,("CSTmDENC::Start TV Standard is PAL-N\n"));
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
    DEBUGF2(2,("CSTmDENC::Start TV Standard is PAL-Nc\n"));
    m_currentCfg.cfg[0]  |= DENC_CFG0_PAL_N;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_16;
    m_currentCfg.cfg[2]  |= DENC_CFG2_RST_2F;
    // Note:No 443_TRAP for PAL_Nc trap filtering as the subcarrier is ~3.58MHz
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_16_FLT;

    if(bIsSquarePixel)
    {
      DEBUGF2(2,("CSTmDENC::Start Programming Colour Carrier for square pixel\n"));
      m_currentCfg.idfs = &palnc_sq;
      m_currentCfg.cfg[5] |= DENC_CFG5_SEL_INC;
    }

    m_DisplayStandardMaxVoltage = PAL_SECAM_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_PAL_M:
    DEBUGF2(2,("CSTmDENC::Start TV Standard is PAL-M\n"));
    m_currentCfg.cfg[0]  |= DENC_CFG0_PAL_M;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_16 | DENC_CFG1_SETUP;
    m_currentCfg.cfg[2]  |= DENC_CFG2_RST_2F;
    // Note:No 443_TRAP for PAL_M trap filtering as the subcarrier is ~3.57MHz
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_16_FLT;

    m_DisplayStandardMaxVoltage = PAL_SECAM_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_PAL_60:
    DEBUGF2(2,("CSTmDENC::Start TV Standard is PAL-60\n"));
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
    DEBUGF2(2,("CSTmDENC::Start TV Standard is NTSC-M\n"));
    m_currentCfg.cfg[0]  |= DENC_CFG0_NTSC_M;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_16 | DENC_CFG1_SETUP;
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_16_FLT;

    if(pModeLine->Mode == STVTG_TIMING_MODE_480I60000_13514)
    {
      DEBUGF2(2,("CSTmDENC::Start Programming Colour Carrier for 60Hz NTSC\n"));
      m_currentCfg.idfs = &ntsc60hz;
      m_currentCfg.cfg[5] |= DENC_CFG5_SEL_INC;
      m_currentCfg.cfg[2] ^= DENC_CFG2_ENA_RST;
    }

    m_DisplayStandardMaxVoltage = NTSC_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_NTSC_443:
    /*
     * NTSC but with the colour on a 4.43MHz subcarrier frequency instead of
     * 3.579MHz.
     */
    DEBUGF2(2,("CSTmDENC::Start TV Standard is NTSC-443\n"));
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
    DEBUGF2(2,("CSTmDENC::Start TV Standard is NTSC-J\n"));
    m_currentCfg.cfg[0]  |= DENC_CFG0_NTSC_M;
    m_currentCfg.cfg[1]  |= DENC_CFG1_FLT_16;
    m_currentCfg.cfg[10] |= DENC_CFG10_AUX_16_FLT;

    m_DisplayStandardMaxVoltage = NTSC_MAX_VOLTAGE;
    break;

  case STM_OUTPUT_STD_SECAM:
    DEBUGF2(2,("CSTmDENC::Start TV Standard is SECAM\n"));
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
    DEBUGF2(2,( "CSTmDENC::Start invalid TV Standard\n"));
    g_pIOS->ZeroMemory(&m_currentCfg,sizeof(struct stm_denc_cfg));
    return false;
  }

  if(bIsSquarePixel)
  {
    DEBUGF2(2,("CSTmDENC::Start Square pixel mode enabled\n"));
    m_currentCfg.cfg[7] |= DENC_CFG7_SQ_PIX;
  }

  if(m_bEnableCCXDS && (pModeLine->TimingParams.LinesByFrame == 525))
    m_currentCfg.cfg[1] |= DENC_CFG1_CC_ENA_F2;

  ProgramDENCConfig();
  ProgramLumaFilter();
  ProgramChromaFilter();

  /*
   * Reset the closed caption XDS data pointer before the interrupt processing
   * is allowed to do anything.
   */
  m_CGMSA_XDS_SendIndex = 0;

  /*
   * Now the DENC is basically alive, set the modeline and standard so
   * the rest of the config and interrupt processing will know what to do.
   *
   * Interrupt processing (UpdateHW) is alive after this point.
   */
  m_pCurrentMode = pModeLine;
  m_ulTVStandard = tvStandard;

  m_pPictureInfoQueue->Start(m_pParent);

  ProgramPSIControls();
  SetTeletextLineSize();
  if(m_pTeletext)
    m_pTeletext->Start(m_teletextLineSize, m_pParent);

  CalculateDACScaling();
  ProgramOutputFormats();

  m_bUpdateWSS = true;

  DEXIT();
  return true;
}


void CSTmDENC::Stop(void)
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

  g_pIOS->ZeroMemory(&m_currentCfg,sizeof(struct stm_denc_cfg));

  m_pCurrentMode = 0;
  m_ulTVStandard = 0;
  m_pParent      = 0;
  m_pPictureInfoQueue->Stop();
  if(m_pTeletext)
    m_pTeletext->Stop();
}


UCHAR CSTmDENC::GetSyncConfiguration(const stm_mode_line_t *pModeLine,
                                     UCHAR syncMode,
                                     stm_vtg_sync_type_t href,
                                     stm_vtg_sync_type_t vref)
{
  /*
   * Helper for Start() method to construct the sync mode configuration in Cfg0
   */
  UCHAR cfg0 = syncMode;

  if(syncMode == DENC_CFG0_ODDE_SLV || syncMode == DENC_CFG0_FRM_SLV)
    cfg0 |= m_bFreerunEnabled?DENC_CFG0_FREE_RUN:0;

  switch(href)
  {
    case STVTG_SYNC_TIMING_MODE:
      if(pModeLine->TimingParams.HSyncPolarity)
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
      if(pModeLine->TimingParams.VSyncPolarity)
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


void CSTmDENC::ProgramDENCConfig(void)
{
  DENTRY();

  if(m_currentCfg.idfs)
  {
    WriteDENCReg(DENC_IDFS0, m_currentCfg.idfs->idfs0);
    WriteDENCReg(DENC_IDFS1, m_currentCfg.idfs->idfs1);
    WriteDENCReg(DENC_IDFS2, m_currentCfg.idfs->idfs2);
  }

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
  g_pIOS->StallExecution(10);

  WriteDENCReg(DENC_CFG10, m_currentCfg.cfg[10]);
  WriteDENCReg(DENC_CFG11, m_currentCfg.cfg[11]);
  WriteDENCReg(DENC_CFG12, m_currentCfg.cfg[12]);

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
  UCHAR val = ReadDENCReg(DENC_CMTTX) & ~DENC_CMTTX_BCS_AUX_EN;
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


  DEXIT();
}


void CSTmDENC::ProgramChromaFilter(void)
{
  DENTRY();

  if(m_bEnableChromaFilter)
  {
    DEBUGF2(2,("%s: Setting user defined filters\n",__PRETTY_FUNCTION__));

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

    DEBUGF2(2,("DENC_CHRM0 = %x\n",ReadDENCReg(DENC_CHRM0)));
    DEBUGF2(2,("DENC_CHRM1 = %x\n",ReadDENCReg(DENC_CHRM1)));
    DEBUGF2(2,("DENC_CHRM2 = %x\n",ReadDENCReg(DENC_CHRM2)));
    DEBUGF2(2,("DENC_CHRM3 = %x\n",ReadDENCReg(DENC_CHRM3)));
    DEBUGF2(2,("DENC_CHRM4 = %x\n",ReadDENCReg(DENC_CHRM4)));
    DEBUGF2(2,("DENC_CHRM5 = %x\n",ReadDENCReg(DENC_CHRM5)));
    DEBUGF2(2,("DENC_CHRM6 = %x\n",ReadDENCReg(DENC_CHRM6)));
    DEBUGF2(2,("DENC_CHRM7 = %x\n",ReadDENCReg(DENC_CHRM7)));
    DEBUGF2(2,("DENC_CHRM8 = %x\n",ReadDENCReg(DENC_CHRM8)));
  }
  else
  {
    DEBUGF2(2,("%s: Setting default filters\n",__PRETTY_FUNCTION__));
    /*
     * We must restore the default divisor here, otherwise the default filters
     * do not work correctly.
     */
    WriteDENCReg(DENC_CHRM0, DENC_CHRM0_DIV_1024);
  }

  DEXIT();
}


void CSTmDENC::ProgramLumaFilter(void)
{
  DENTRY();

  m_currentCfg.cfg[9] = m_CVBSDelay;

  if(m_bEnableLumaFilter)
  {
    DEBUGF2(2,("%s: Setting user defined filter\n",__PRETTY_FUNCTION__));

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

    DEBUGF2(2,("DENC_LUMA0 = %x\n",ReadDENCReg(DENC_LUMA0)));
    DEBUGF2(2,("DENC_LUMA1 = %x\n",ReadDENCReg(DENC_LUMA1)));
    DEBUGF2(2,("DENC_LUMA2 = %x\n",ReadDENCReg(DENC_LUMA2)));
    DEBUGF2(2,("DENC_LUMA3 = %x\n",ReadDENCReg(DENC_LUMA3)));
    DEBUGF2(2,("DENC_LUMA4 = %x\n",ReadDENCReg(DENC_LUMA4)));
    DEBUGF2(2,("DENC_LUMA5 = %x\n",ReadDENCReg(DENC_LUMA5)));
    DEBUGF2(2,("DENC_LUMA6 = %x\n",ReadDENCReg(DENC_LUMA6)));
    DEBUGF2(2,("DENC_LUMA7 = %x\n",ReadDENCReg(DENC_LUMA7)));
    DEBUGF2(2,("DENC_LUMA8 = %x\n",ReadDENCReg(DENC_LUMA8)));
    DEBUGF2(2,("DENC_LUMA9 = %x\n",ReadDENCReg(DENC_LUMA9)));

    m_currentCfg.cfg[9] |= (m_lumaFilterConfig.div<<1) | DENC_CFG9_FLT_YS;
  }
  else
  {
    DEBUGF2(2,("%s: Setting default filters\n",__PRETTY_FUNCTION__));
    m_currentCfg.cfg[9] |= DENC_CFG9_PLG_DIV_Y_512;
  }

  WriteDENCReg(DENC_CFG9,  m_currentCfg.cfg[9]);

  DEBUGF2(2,("DENC_CFG9 = 0x%x\n",(int)ReadDENCReg(DENC_CFG9)));

  DEXIT();
}


void CSTmDENC::Program625LineWSS(void)
{
  DEBUGF2(2,("CSTmDENC::Program625LineWSS\n"));

  UCHAR wss1reg = 0;
  UCHAR wss0reg = 0;

  m_currentCfg.cfg[3] &= ~DENC_CFG3_ENA_CGMS;
  m_currentCfg.cfg[3] |= DENC_CFG3_ENA_WSS;

  switch (m_currentPictureInfo.videoAspect)
  {
    case STM_WSS_OFF:
    {
      wss1reg = DENC_WSS1_NO_AFD;
      DEBUGF2(2,("CSTmDENC::Program625LineWSS OFF\n"));
      break;
    }
    case STM_WSS_4_3:
    {
      DEBUGF2(2,("CSTmDENC::Program625LineWSS 4x3\n"));
      switch(m_currentPictureInfo.letterboxStyle)
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
      break;
    }
    case STM_WSS_16_9:
    {
      DEBUGF2(2,("CSTmDENC::Program625LineWSS 16x9\n"));
      switch(m_currentPictureInfo.letterboxStyle)
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
    case STM_WSS_14_9:
    {
      DEBUGF2(2,("CSTmDENC::Program625LineWSS 14x9\n"));
      switch(m_currentPictureInfo.letterboxStyle)
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
    case STM_WSS_GT_16_9:
    {
      DEBUGF2(2,("CSTmDENC::Program625LineWSS >16x9\n"));
      switch(m_currentPictureInfo.letterboxStyle)
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

  /*
   * 625line WSS only uses the bottom 2 bits of the Analogue CGMS value
   */
  if(m_CGMSAnalog)
    wss0reg = (UCHAR)(m_CGMSAnalog & 0x3) << DENC_WSS0_CP_SHIFT;

  if(m_bTeletextSubtitles)
    wss0reg |= DENC_WSS0_SUB_TELETEXT;

  wss0reg |= (UCHAR)m_OpenSubtitles << DENC_WSS0_OPEN_SUBTITLE_SHIFT;

  DEBUGF2(2,("CSTmDENC::Program625LineWSS WSS bytes 0x%02x 0x%02x\n",(unsigned)wss0reg,(unsigned)wss1reg));
  DEBUGF2(2,("CSTmDENC::Program625LineWSS DENC Cfg3 = 0x%02x\n",(unsigned)m_currentCfg.cfg[3]));

  WriteDENCReg(DENC_WSS1, wss1reg);
  WriteDENCReg(DENC_WSS0, wss0reg);
  WriteDENCReg(DENC_CFG3, m_currentCfg.cfg[3]);
}


void CSTmDENC::Program525LineWSS(void)
{
  UCHAR cgms0=0,cgms1=0,cgms2=0;

  m_currentCfg.cfg[3] &= ~DENC_CFG3_ENA_WSS;
  m_currentCfg.cfg[3] |= DENC_CFG3_ENA_CGMS;

  switch (m_currentPictureInfo.videoAspect)
  {
    case STM_WSS_OFF:
    {
      DEBUGF2(2,("CSTmDENC::Program525LineWSS OFF\n"));
      break;
    }
    case STM_WSS_4_3:
    {
      cgms0 = DENC_CGMS0_4_3 | ((m_currentPictureInfo.letterboxStyle>STM_LETTERBOX_NONE)?DENC_CGMS0_LETTERBOX:DENC_CGMS0_NORMAL);
      DEBUGF2(2,("CSTmDENC::Program525LineWSS 4x3\n"));
      break;
    }
    case STM_WSS_16_9:
    case STM_WSS_14_9:
    case STM_WSS_GT_16_9:
    {
      cgms0 = DENC_CGMS0_16_9 | DENC_CGMS0_NORMAL;

      DEBUGF2(2,("CSTmDENC::Program525LineWSS >=16x9 or 14x9\n"));
      break;
    }
  }

  if(m_CGMSAnalog)
  {
    cgms1 |= (m_CGMSAnalog&0x1)        << DENC_CGMS1_IEC61880_B6_SHIFT;
    cgms1 |= ((m_CGMSAnalog>>1) & 0x1) << DENC_CGMS1_IEC61880_B7_SHIFT;
    cgms1 |= ((m_CGMSAnalog>>2) & 0x1) << DENC_CGMS1_IEC61880_B8_SHIFT;
    cgms1 |= ((m_CGMSAnalog>>3) & 0x1) << DENC_CGMS1_IEC61880_B9_SHIFT;
  }

  if(m_bPrerecordedAnalogueSource)
    cgms1 |= DENC_CGMS1_PRERECORDED_ANALOGUE_SOURCE;

  /*
   * CRC generation. X^6 + X + 1, all preset to 1.
   */
  ULONG crcreg = 0x3f;

  DEBUGF2(2,("CRC: reg = 0x%lx\n",crcreg));

  for(int bit=3;bit>=0;bit--)
  {
    int newbit = (crcreg & 0x1) ^ ((cgms0 >> bit) & 0x1);

    crcreg >>= 1;
    crcreg ^= newbit << 4;
    crcreg |= newbit << 5;

    DEBUGF2(2,("CRC: reg = 0x%lx\n",crcreg));
  }

  for(int bit=7;bit>=0;bit--)
  {
    int newbit = (crcreg & 0x1) ^ ((cgms1 >> bit) & 0x1);

    crcreg >>= 1;
    crcreg ^= newbit << 4;
    crcreg |= newbit << 5;

    DEBUGF2(2,("CRC: reg = 0x%lx\n",crcreg));
  }

  for(int bit=7;bit>=6;bit--)
  {
    int newbit = (crcreg & 0x1) ^ ((cgms2 >> bit) & 0x1);

    crcreg >>= 1;
    crcreg ^= newbit << 4;
    crcreg |= newbit << 5;

    DEBUGF2(2,("CRC: reg = 0x%lx\n",crcreg));
  }

  for(int bit=5;bit>=0;bit--)
  {
    int newbit = crcreg & 0x1;

    crcreg >>= 1;
    cgms2 |= (newbit << bit);
  }

  DEBUGF2(2,("CSTmDENC::Program525LineWSS CGMS bytes 0x%02x 0x%02x 0x%02x\n",(unsigned)cgms0,(unsigned)cgms1,(unsigned)cgms2));
  DEBUGF2(2,("CSTmDENC::Program525LineWSS DENC Cfg3 = 0x%02x\n",(unsigned)m_currentCfg.cfg[3]));

  WriteDENCReg(DENC_CGMS0, cgms0);
  WriteDENCReg(DENC_CGMS1, cgms1);
  WriteDENCReg(DENC_CGMS2, cgms2);
  WriteDENCReg(DENC_CFG3,  m_currentCfg.cfg[3]);
}


inline UCHAR xds_data_parity(UCHAR data)
{
  /*
   * The xds data transmission is 7bit data + odd parity bit.
   */
  UCHAR parity = 1;
  for(int i=0;i<7;i++)
  {
    parity ^= (data>>i) & 0x1;
  }
  return parity<<7;
}


void CSTmDENC::ProgramCGMSAXDS(void)
{
  DENTRY();

  m_CGMSA_XDS_Packet[2] = 0x40; // Bit6 always 1
  m_CGMSA_XDS_Packet[2] |= (m_CGMSAnalog&0x1) << 4;
  m_CGMSA_XDS_Packet[2] |= ((m_CGMSAnalog>>1) & 0x1) << 3;
  m_CGMSA_XDS_Packet[2] |= ((m_CGMSAnalog>>2) & 0x1) << 2;
  m_CGMSA_XDS_Packet[2] |= ((m_CGMSAnalog>>3) & 0x1) << 1;

  if(m_bPrerecordedAnalogueSource)
    m_CGMSA_XDS_Packet[2] |= 1;

  m_CGMSA_XDS_Packet[2] |= xds_data_parity(m_CGMSA_XDS_Packet[2]);
  DEBUGF2(2,("%s: XDS CGMSA data = 0x%02x\n",__PRETTY_FUNCTION__,(int)m_CGMSA_XDS_Packet[2]));

  /*
   * Calculate checksum, sum of all data == 0 (ignoring parity bits)
   */
  m_CGMSA_XDS_Packet[5] = 0;
  for(int i = 0; i<5; i++)
  {
    m_CGMSA_XDS_Packet[5] += m_CGMSA_XDS_Packet[i] & 0x7F;
    if(m_CGMSA_XDS_Packet[5]>127)
      m_CGMSA_XDS_Packet[5] -= 128;
  }

  m_CGMSA_XDS_Packet[5]  = 128 - m_CGMSA_XDS_Packet[5];
  m_CGMSA_XDS_Packet[5] |= xds_data_parity(m_CGMSA_XDS_Packet[5]);
  DEBUGF2(2,("%s: XDS checksum = 0x%02x\n",__PRETTY_FUNCTION__,(int)m_CGMSA_XDS_Packet[5]));
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
      m_teletextLineSize = (m_pCurrentMode->TimingParams.LinesByFrame==625)?40:0;
      break;
    case DENC_TTXCFG_SYSTEM_B:
      m_teletextLineSize = (m_pCurrentMode->TimingParams.LinesByFrame==625)?45:37;
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
  ULONG DAC_target = (m_DAC123Saturation * m_DisplayStandardMaxVoltage) / m_maxDAC123Voltage;
  ULONG percentage_of_max_voltage = DAC_target*100000/1024; // scale up to give 3 decimal places of precision

  DEBUGF2(2,("CSTmDENC::CalculateDACScaling DAC123 MaxV = %lu Saturation = %lu\n",m_maxDAC123Voltage,m_DAC123Saturation));
  DEBUGF2(2,("CSTmDENC::CalculateDACScaling DAC123 Tv = %lu.%lu%%\n",percentage_of_max_voltage/1000,percentage_of_max_voltage%1000));

  if(percentage_of_max_voltage > m_MinRGBScalePercentage)
    m_DAC123RGBScale = (UCHAR)((percentage_of_max_voltage - m_MinRGBScalePercentage) / m_RGBScaleStep);
  else
    m_DAC123RGBScale = 0;

  if(percentage_of_max_voltage > m_MinYScalePercentage)
    m_DAC123YScale = (UCHAR)((percentage_of_max_voltage - m_MinYScalePercentage) / m_YScaleStep);
  else
    m_DAC123YScale = 0;

  DEBUGF2(2,("CSTmDENC::CalculateDACScaling DAC123 RGBScale = 0x%02lx YScale = 0x%02lx\n",(ULONG)m_DAC123RGBScale,(ULONG)m_DAC123YScale));

  DAC_target = (m_DAC456Saturation * m_DisplayStandardMaxVoltage) / m_maxDAC456Voltage;
  percentage_of_max_voltage = DAC_target*100000/1024; // scale up to give 3 decimal places of precision

  DEBUGF2(2,("CSTmDENC::CalculateDACScaling DAC456 MaxV = %lu Saturation = %lu\n",m_maxDAC456Voltage,m_DAC456Saturation));
  DEBUGF2(2,("CSTmDENC::CalculateDACScaling DAC456 Tv = %lu.%lu%%\n",percentage_of_max_voltage/1000,percentage_of_max_voltage%1000));

  if(percentage_of_max_voltage > m_MinRGBScalePercentage)
    m_DAC456RGBScale = (UCHAR)((percentage_of_max_voltage - m_MinRGBScalePercentage) / m_RGBScaleStep);
  else
    m_DAC456RGBScale = 0;

  if(percentage_of_max_voltage > m_MinYScalePercentage)
    m_DAC456YScale = (UCHAR)((percentage_of_max_voltage - m_MinYScalePercentage) / m_YScaleStep);
  else
    m_DAC456YScale = 0;

  DEBUGF2(2,("CSTmDENC::CalculateDACScaling DAC456 RGBScale = 0x%02lx YScale = 0x%02lx\n",(ULONG)m_DAC456RGBScale,(ULONG)m_DAC456YScale));
}


stm_meta_data_result_t CSTmDENC::QueueMetadata(stm_meta_data_t *m)
{
  DENTRY();

  /*
   * This looks like overkill, but we might want to add a queue for closed
   * caption at some later date.
   */
  switch(m->type)
  {
    case STM_METADATA_TYPE_PICTURE_INFO:
      return m_pPictureInfoQueue->Queue(m);

    case STM_METADATA_TYPE_EBU_TELETEXT:
      if(m_pTeletext)
        return m_pTeletext->QueueMetadata(m);

      break;

    default:
      break;
  }
  return STM_METADATA_RES_UNSUPPORTED_TYPE;
}


void CSTmDENC::FlushMetadata(stm_meta_data_type_t type)
{
  DENTRY();

  switch(type)
  {
    case STM_METADATA_TYPE_PICTURE_INFO:
      m_pPictureInfoQueue->Flush();
      break;

    case STM_METADATA_TYPE_EBU_TELETEXT:
      if(m_pTeletext)
        m_pTeletext->FlushMetadata(type);

      break;

    default:
      break;
  }

  DEXIT();
}


void CSTmDENC::UpdateHW(stm_field_t sync)
{
  stm_meta_data_t *m;
  stm_picture_format_info_t *p;

  if(!m_pCurrentMode)
    return;

  if((m = m_pPictureInfoQueue->Pop()) != 0)
  {
    p = (stm_picture_format_info_t*)&m->data[0];
    if(p->flags & STM_PIC_INFO_LETTERBOX)
    {
      m_currentPictureInfo.letterboxStyle = p->letterboxStyle;
      m_bUpdateWSS = true;
    }
    if(p->flags & STM_PIC_INFO_PICTURE_ASPECT)
    {
      m_currentPictureInfo.pictureAspect = p->pictureAspect;
      m_bUpdateWSS = true;
    }
    if(p->flags & STM_PIC_INFO_VIDEO_ASPECT)
    {
      m_currentPictureInfo.videoAspect = p->videoAspect;
      m_bUpdateWSS = true;
    }

    stm_meta_data_release(m);
  }

  /*
   * Remember that other controls may also cause an update.
   */
  if(m_bUpdateWSS)
  {
    m_bUpdateWSS = false;
    if(m_pCurrentMode->TimingParams.LinesByFrame == 625)
    {
      Program625LineWSS();
    }
    else
    {
      Program525LineWSS();
      if(m_bEnableCCXDS && (sync == STM_TOP_FIELD) && (m_CGMSA_XDS_Packet == 0))
          ProgramCGMSAXDS();
    }
  }

  if(m_bEnableCCXDS &&
     (sync == STM_TOP_FIELD) &&
     (m_pCurrentMode->TimingParams.LinesByFrame == 525))
  {
    WriteDENCReg(DENC_CC20,m_CGMSA_XDS_Packet[m_CGMSA_XDS_SendIndex++]);
    WriteDENCReg(DENC_CC21,m_CGMSA_XDS_Packet[m_CGMSA_XDS_SendIndex++]);

    if(m_CGMSA_XDS_SendIndex == 6)
      m_CGMSA_XDS_SendIndex = 0;
  }

  /*
   * If we have a Teletext processor attached to this DENC, let it update too.
   */
  if(m_pTeletext)
    m_pTeletext->UpdateHW();
}
