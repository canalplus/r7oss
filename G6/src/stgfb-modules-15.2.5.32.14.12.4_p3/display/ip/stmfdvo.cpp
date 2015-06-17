/***********************************************************************
 *
 * File: display/ip/stmfdvo.cpp
 * Copyright (c) 2008-2013 STMicroelectronics Limited.
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

#include "stmfdvo.h"

static const int FDVO_AWG_DIGSYNC_CTRL  = 0x0;
static const int FDVO_DOF_CFG           = 0x4;
static const int FDVO_LUT_PROG_LOW      = 0x8;
static const int FDVO_LUT_PROG_MID      = 0xC;
static const int FDVO_LUT_PROG_HIGH     = 0x10;

#define FDVO_DOF_EN_LOWBYTE        (1L<<0)
#define FDVO_DOF_EN_MIDBYTE        (1L<<1)
#define FDVO_DOF_EN_HIGHBYTE       (1L<<2)
#define FDVO_DOF_BYTE_EN_MASK      (FDVO_DOF_EN_LOWBYTE | FDVO_DOF_EN_MIDBYTE | FDVO_DOF_EN_HIGHBYTE)
#define FDVO_DOF_EN_CHROMA_FILTER  (1L<<3)
#define FDVO_DOF_INV_CLOCKOUT      (1L<<4)
#define FDVO_DOF_DOUBLE_CLOCK_DATA (1L<<5)
#define FDVO_DOF_EN                (1L<<6)
#define FDVO_DOF_DROP_SEL          (1L<<7)
#define FDVO_DOF_MOD_COUNT_SHIFT   (8)
#define FDVO_DOF_MOD_COUNT_MASK    (7L<<FDVO_DOF_MOD_COUNT_SHIFT)

#define FDVO_LUT_ZERO              (0)
#define FDVO_LUT_Y_G               (1)
#define FDVO_LUT_Y_G_DEL           (2)
#define FDVO_LUT_CB_B              (3)
#define FDVO_LUT_CB_B_DEL          (4)
#define FDVO_LUT_CR_R              (5)
#define FDVO_LUT_CR_R_DEL          (6)
#define FDVO_LUT_HOLD              (7)


struct fdvo_config
{
  uint32_t flags;
  uint32_t lowbyte;
  uint32_t midbyte;
  uint32_t highbyte;
};

/*
 * There is some confusion in the documentation, but we are informed by
 * validation/design that the mod count is the required modulo-1 .
 */
static const fdvo_config rgb_24bit_config =
{
  ((0L<<FDVO_DOF_MOD_COUNT_SHIFT) |
   FDVO_DOF_EN_LOWBYTE            |
   FDVO_DOF_EN_MIDBYTE            |
   FDVO_DOF_EN_HIGHBYTE),
  FDVO_LUT_CB_B,
  FDVO_LUT_Y_G,
  FDVO_LUT_CR_R
};

static const fdvo_config yuv_444_24bit_config =
{
  ((0L<<FDVO_DOF_MOD_COUNT_SHIFT) |
   FDVO_DOF_EN_LOWBYTE            |
   FDVO_DOF_EN_MIDBYTE            |
  FDVO_DOF_EN_HIGHBYTE),
  FDVO_LUT_CB_B,
  FDVO_LUT_Y_G,
  FDVO_LUT_CR_R
};

static const fdvo_config yuv_444_16bit_config =
{ /* Note the FDVO output clock must be twice the pixel clock in this mode */
  ((1L<<FDVO_DOF_MOD_COUNT_SHIFT) |
   FDVO_DOF_EN_MIDBYTE            |
   FDVO_DOF_EN_HIGHBYTE),
  0,
  (FDVO_LUT_Y_G  | (FDVO_LUT_Y_G<<3)),
  (FDVO_LUT_CB_B | (FDVO_LUT_CR_R<<3))
};

static const fdvo_config yuv_422_16bit_config =
{
  ((1L<<FDVO_DOF_MOD_COUNT_SHIFT) |
   FDVO_DOF_EN_MIDBYTE            |
   FDVO_DOF_EN_HIGHBYTE),
   0,
  (FDVO_LUT_Y_G  | (FDVO_LUT_Y_G<<3)),
  (FDVO_LUT_CB_B | (FDVO_LUT_CR_R_DEL<<3))
};

static const fdvo_config yuv_422_8bit_config =
{ /* Note the FDVO output clock must be twice the pixel clock in this mode */
  ((3L<<FDVO_DOF_MOD_COUNT_SHIFT) |
   FDVO_DOF_EN_MIDBYTE),
  0,
  ( FDVO_LUT_CB_B          | (FDVO_LUT_Y_G << 3) |
    (FDVO_LUT_CR_R_DEL << 6) | (FDVO_LUT_Y_G << 9)),
  0
};

static const fdvo_config yuv_422_8bit_ed_config =
{ /* Note the FDVO output clock must be twice the pixel clock in this mode */
  ((3L<<FDVO_DOF_MOD_COUNT_SHIFT) |
   FDVO_DOF_EN_MIDBYTE),
  0,
  (FDVO_LUT_CR_R_DEL   | (FDVO_LUT_Y_G_DEL << 3) |
  (FDVO_LUT_CB_B << 6) | (FDVO_LUT_Y_G_DEL << 9)),
  0
};

CSTmFDVO::CSTmFDVO(uint32_t        id,
                   CDisplayDevice *pDev,
                   uint32_t        ulFDVOOffset,
                   COutput        *pMasterOutput,
                   const char     *name): COutput(name, id, pDev, pMasterOutput->GetTimingID()),
                                          m_SyncDvo(pDev, ulFDVOOffset)
{
  TRC( TRC_ID_MAIN_INFO, "CSTmFDVO::CSTmFDVO pDev = %p  master output = %p", pDev,pMasterOutput );
  m_pMasterOutput = pMasterOutput;

  m_pFDVORegs     = (uint32_t*)pDev->GetCtrlRegisterBase() + (ulFDVOOffset>>2);

  WriteFDVOReg(FDVO_DOF_CFG, 0); // Ensure it is disabled

  m_ulCapabilities |= (OUTPUT_CAPS_DVO_656   |
                       OUTPUT_CAPS_DVO_16BIT |
                       OUTPUT_CAPS_DVO_24BIT |
                       OUTPUT_CAPS_422_CHROMA_FILTER);

  m_VideoSource = STM_VIDEO_SOURCE_MAIN_COMPOSITOR;

  m_ulOutputFormat = (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422 | STM_VIDEO_OUT_16BIT);

  m_bInvertDataClock = true;

  m_sAwgCodeParams.bAllowEmbeddedSync  = false;
  m_sAwgCodeParams.bEnableData         = false;
  m_sAwgCodeParams.OutputClkDealy      = 1;
  m_u422ChromaFilter = 0;

  TRC( TRC_ID_MAIN_INFO, "CSTmFDVO::CSTmFDVO m_pFDVORegs = %p", m_pFDVORegs );
}


CSTmFDVO::~CSTmFDVO() {}


bool CSTmFDVO::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!COutput::Create())
    return false;

  if(m_pMasterOutput && !m_pMasterOutput->RegisterSlavedOutput(this))
    return false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmFDVO::CleanUp(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsStarted)
    Stop();

  if(m_pMasterOutput)
    m_pMasterOutput->UnRegisterSlavedOutput(this);

  COutput::CleanUp();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

const stm_display_mode_t* CSTmFDVO::SupportedMode(const stm_display_mode_t *mode) const
{
  if(mode->mode_params.output_standards & STM_OUTPUT_STD_NTG5)
  {
    TRCOUT( TRC_ID_MAIN_INFO, "Looking for valid NTG5 mode, pixclock = %u", mode->mode_timing.pixel_clock_freq);
    return mode;
  }

  if(mode->mode_params.output_standards & (STM_OUTPUT_STD_HD_MASK | STM_OUTPUT_STD_XGA))
  {
    TRC( TRC_ID_MAIN_INFO, "Looking for valid HD/XGA mode, pixclock = %u", mode->mode_timing.pixel_clock_freq);

    if(mode->mode_params.output_standards & STM_OUTPUT_STD_AS4933)
    {
      TRCOUT( TRC_ID_MAIN_INFO, "No support for AS4933 yet");
      return 0;
    }

    if(mode->mode_timing.pixel_clock_freq < 65000000 ||
       mode->mode_timing.pixel_clock_freq > 148500000)
    {
      TRCOUT( TRC_ID_MAIN_INFO, "pixel clock out of range");
      return 0;
    }

    return mode;
  }

  /*
   * We support SD interlaced modes based on a 13.5Mhz pixel clock
   * and progressive modes at 27Mhz or 27.027Mhz. We also report support
   * for SD interlaced modes over HDMI where the clock is doubled and
   * pixels are repeated. Finally we support VGA (640x480p@59.94Hz and 60Hz)
   * for digital displays (25.18MHz pixclock)
   */
  if(mode->mode_params.output_standards & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_VGA))
  {
    TRC( TRC_ID_MAIN_INFO, "Looking for valid SD progressive mode");
    if(mode->mode_timing.pixel_clock_freq < 25174800 ||
       mode->mode_timing.pixel_clock_freq > 27027000)
    {
      TRCOUT( TRC_ID_MAIN_INFO, "pixel clock out of range");
      return 0;
    }

    return mode;
  }

  if(mode->mode_params.output_standards == STM_OUTPUT_STD_CEA861)
  {
    /*
     * Support all CEA specific pixel double/quad modes SD and ED for Dolby TrueHD audio
     */
    TRCOUT( TRC_ID_MAIN_INFO, "Looking for valid SD/ED mode");
    return mode;
  }

  if(mode->mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)
  {
    TRC( TRC_ID_MAIN_INFO, "Looking for valid SD interlaced mode");
    if(mode->mode_timing.pixel_clock_freq < 13500000 ||
       mode->mode_timing.pixel_clock_freq > 13513500)
    {
      TRCOUT( TRC_ID_MAIN_INFO, "pixel clock out of range , pixclock = %u", mode->mode_timing.pixel_clock_freq);
      return 0;
    }
    return mode;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "no matching mode found");
  return 0;
}


OutputResults CSTmFDVO::Start(const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  // The DVO is slaved off the timings of a main output, which must have
  // already been started with the mode requested here.
  const stm_display_mode_t *master_mode = m_pMasterOutput->GetCurrentDisplayMode();
  if( master_mode == 0 || !AreModesIdentical(*master_mode, *pModeLine))
  {
    TRC( TRC_ID_ERROR, "Display mode or standard invalid" );
    return STM_OUT_INVALID_VALUE;
  }

  if(m_bIsStarted)
    Stop();

  if(!SetOutputFormat(m_ulOutputFormat,pModeLine))
  {
    TRC( TRC_ID_ERROR, "Invalid output format" );
    Stop();
    return STM_OUT_INVALID_VALUE;
  }

  /*
   * Start EAV/SAV insertion.
   * Note: if no EAV/SAV generation firmware is available for the display mode
   *       then the FDVO will still output the video, syncs and clock signals.
   */
  if (m_sAwgCodeParams.bAllowEmbeddedSync)
  {
    m_SyncDvo.Start(pModeLine,m_ulOutputFormat,m_sAwgCodeParams);
  }
  else
  {
     m_SyncDvo.Stop();
  }

  COutput::Start(pModeLine);

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return STM_OUT_OK;
}


bool CSTmFDVO::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t val = ReadFDVOReg(FDVO_DOF_CFG) & ~FDVO_DOF_EN;
  WriteFDVOReg(FDVO_DOF_CFG, val);

  m_SyncDvo.Stop();
  COutput::Stop();

  DisableClocks();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmFDVO::UpdateOutputMode(const stm_display_mode_t& pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_bIsStarted)
    return;

  Start(&pModeLine);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


uint32_t CSTmFDVO::SetControl(stm_output_control_t ctrl, uint32_t newVal)
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_AUDIO_SOURCE_SELECT:
    {
      /* Just to maintain the API */
      if(newVal != STM_AUDIO_SOURCE_NONE)
        return STM_OUT_INVALID_VALUE;
      break;
    }
    case OUTPUT_CTRL_VIDEO_OUT_SELECT:
    {
      if(SetOutputFormat(newVal,GetCurrentDisplayMode()))
        m_ulOutputFormat = newVal;
      else
        return STM_OUT_INVALID_VALUE;

      break;
    }
    case OUTPUT_CTRL_DVO_INVERT_DATA_CLOCK:
    {
      m_bInvertDataClock = (newVal!=0);
      SetOutputFormat(m_ulOutputFormat,GetCurrentDisplayMode());
      break;
    }
    case OUTPUT_CTRL_DVO_ALLOW_EMBEDDED_SYNCS:
    {
      if (m_sAwgCodeParams.bAllowEmbeddedSync != (newVal!=0))
      {
        m_sAwgCodeParams.bAllowEmbeddedSync = (newVal!=0);
        if (!m_sAwgCodeParams.bAllowEmbeddedSync)
        {
          m_SyncDvo.Stop();
        }
        else
        {
          m_SyncDvo.Start(GetCurrentDisplayMode(),m_ulOutputFormat,m_sAwgCodeParams);
        }
        if (GetCurrentDisplayMode())
          SetVTGSyncs(GetCurrentDisplayMode());
      }
      SetOutputFormat(m_ulOutputFormat,GetCurrentDisplayMode());
      break;
    }
    default:
      return STM_OUT_NO_CTRL;
  }

  return STM_OUT_OK;
}


uint32_t CSTmFDVO::GetControl(stm_output_control_t ctrl, uint32_t *val) const
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_VIDEO_SOURCE_SELECT:
      *val = m_VideoSource;
      break;
    case OUTPUT_CTRL_AUDIO_SOURCE_SELECT:
      *val = m_AudioSource;
      break;
    case OUTPUT_CTRL_VIDEO_OUT_SELECT:
      *val = m_ulOutputFormat;
      break;
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
      *val = m_signalRange;
      break;
    case OUTPUT_CTRL_422_CHROMA_FILTER:
      *val = m_u422ChromaFilter;
      break;
    case OUTPUT_CTRL_DVO_INVERT_DATA_CLOCK:
      *val = m_bInvertDataClock?1:0;
      break;
    case OUTPUT_CTRL_DVO_ALLOW_EMBEDDED_SYNCS:
      *val = m_sAwgCodeParams.bAllowEmbeddedSync?1:0;
      break;
    default:
      *val = 0;
      return STM_OUT_NO_CTRL;
  }

  return STM_OUT_OK;
}


bool CSTmFDVO::SetOutputFormat(uint32_t format,const stm_display_mode_t* pModeLine)
{
  const fdvo_config *config;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  switch(format)
  {
    case (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_24BIT):
      TRC( TRC_ID_MAIN_INFO, "24bit RGB" );
      config = &rgb_24bit_config;
      break;
    case (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_444 | STM_VIDEO_OUT_16BIT):
      TRC( TRC_ID_MAIN_INFO, "16bit YUV 4:4:4 (double clocked)" );
      config = &yuv_444_16bit_config;
      break;
    case (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422 | STM_VIDEO_OUT_16BIT):
      TRC( TRC_ID_MAIN_INFO, "16bit YUV 4:2:2" );
      config = &yuv_422_16bit_config;
      break;
    case (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_ITUR656):
      TRC( TRC_ID_MAIN_INFO, "8bit YUV 4:2:2 (ITUR656)" );
      if(m_bInvertDataClock&&(!(m_sAwgCodeParams.bAllowEmbeddedSync)))         // ED case
        config = &yuv_422_8bit_ed_config;
      else
        config = &yuv_422_8bit_config;
      break;
    case (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_444 | STM_VIDEO_OUT_24BIT):
      TRC( TRC_ID_MAIN_INFO, "24bit YUV 4:4:4" );
      config = &yuv_444_24bit_config;
      break;
    default:
      TRC( TRC_ID_ERROR, "DVO output format (0x%08x) not supported",format );
      return false;
  }

  if(pModeLine)
  {
    if(format & STM_VIDEO_OUT_ITUR656)
    {
      /*
       * 8bit ITUR656 is not defined HD modes
       */
      if((pModeLine->mode_params.output_standards & STM_OUTPUT_STD_HD_MASK) != 0)
      {
        TRC( TRC_ID_ERROR, "8bit DVO not supported in HD modes" );
        return false;
      }
    }

    WriteFDVOReg(FDVO_LUT_PROG_LOW,  config->lowbyte);
    WriteFDVOReg(FDVO_LUT_PROG_MID,  config->midbyte);
    WriteFDVOReg(FDVO_LUT_PROG_HIGH, config->highbyte);

    uint32_t cfg = (config->flags | FDVO_DOF_EN);

    if(!m_sAwgCodeParams.bAllowEmbeddedSync)
    {
      TRC( TRC_ID_MAIN_INFO, "Embedded sync words disabled" );
      cfg &= ~FDVO_DOF_BYTE_EN_MASK;
    }

    if(m_bInvertDataClock)
    {
      TRC( TRC_ID_MAIN_INFO, "Output data clock inverted" );
      cfg |= FDVO_DOF_INV_CLOCKOUT;
    }

    WriteFDVOReg(FDVO_DOF_CFG, cfg);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmFDVO::Suspend(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSuspended)
    return;

  if(m_bIsStarted)
  {
    uint32_t val = ReadFDVOReg(FDVO_DOF_CFG) & ~FDVO_DOF_EN;
    WriteFDVOReg(FDVO_DOF_CFG, val);

    m_SyncDvo.Suspend();
  }

  COutput::Suspend();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CSTmFDVO::Resume(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_bIsSuspended)
    return;

  if(m_bIsStarted)
  {
    m_SyncDvo.Resume();

    if(!SetOutputFormat(m_ulOutputFormat,GetCurrentDisplayMode()))
    {
      TRC( TRC_ID_ERROR, "Invalid output format" );
      this->Stop();
      return;
    }
  }

  COutput::Resume();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

// No interrupts on DVO
bool CSTmFDVO::HandleInterrupts(void) { return false; }
