/***********************************************************************
 *
 * File: display/ip/tvout/stmtvoutdenc.cpp
 * Copyright (c) 2005-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/Output.h>

#include <display/ip/stmdencregs.h>

#include "stmtvoutdenc.h"
#include "stmtvoutteletext.h"

#define DENC_CFG13_DAC456_CONF_YC_CVBS_MAIN     0x07

#define DENC_CFG13_DAC123_CONF_RGB              (0x2 << 3)
#define DENC_CFG13_DAC123_CONF_YUV_MAIN         (0x3 << 3)
#define DENC_CFG13_DAC123_CONF_NONE             (0x7 << 3)

#define DENC_CFG13_RGB_MAXDYN 0x40

#define DENC_DAC13  0x11  /* Denc gain for dac 1&3 */
#define DENC_DAC34  0x12  /* Denc gain for dac 3&4 */
#define DENC_DAC45  0x13  /* Denc gain for dac 4&5 */
#define DENC_DAC2   0x6A  /* Denc gain for dac 2 */
#define DENC_DAC6   0x6B  /* Denc gain for dac 6 */

CSTmTVOutDENC::CSTmTVOutDENC(CDisplayDevice* pDev, uint32_t baseAddr, CSTmTVOutTeletext *pTeletext): CSTmDENC(pDev, baseAddr, pTeletext)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pDENCSync = 0;
  vibe_os_zero_memory(m_DACScales, sizeof(m_DACScales));

  m_MinRGBScalePercentage = 81250; // 81.25%
  m_RGBScaleStep          = 590;   // 0.59%
  m_MinYScalePercentage   = 75000; // 75%
  m_YScaleStep            = 780;   // 0.78%

  m_C_Multiply = 0; // Chroma scaled up by 100%

  TRCIN( TRC_ID_MAIN_INFO, "" );
}


CSTmTVOutDENC::~CSTmTVOutDENC ()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  delete m_pDENCSync;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmTVOutDENC::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!CSTmDENC::Create())
  {
    TRC( TRC_ID_ERROR, "DENC base class create failed" );
    return false;
  }

  m_pDENCSync = new CSTmDENCSync("denc");
  if(!m_pDENCSync || !m_pDENCSync->Create())
  {
    TRC( TRC_ID_ERROR, "failed to create DENC analog sync object" );
    return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmTVOutDENC::Start(COutput *parent, const stm_display_mode_t *pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * The HD TVOut integration requires a positive H sync and TnB instead of
   * the VSync.
   */
  return CSTmDENC::Start(parent, pModeLine, DENC_CFG0_ODHS_SLV, STVTG_SYNC_POSITIVE, STVTG_SYNC_TOP_NOT_BOT);
}


void CSTmTVOutDENC::WriteDAC123Scale(void)
{
  uint8_t tmp;
  /*
   * This is split out into a separate function to allow subclasses to program
   * the scaling
   */
  TRC( TRC_ID_MAIN_INFO, "CSTmTVOutDENC::WriteDAC123Scale [0x%x,0x%x,0x%x]", (uint32_t)m_DACScales[1],(uint32_t)m_DACScales[2],(uint32_t)m_DACScales[3] );
  WriteDENCReg(DENC_DAC2, m_DACScales[2]);
  WriteDENCReg(DENC_DAC13, (m_DACScales[1]<<2) | ((m_DACScales[3]&0x30)>>4));
  tmp = ReadDENCReg(DENC_DAC34) & 0xF;
  WriteDENCReg(DENC_DAC34, tmp | (m_DACScales[3]<<4));
}


void CSTmTVOutDENC::WriteDAC456Scale(void)
{
  uint8_t tmp;
  /*
   * This is split out into a separate function to allow subclasses to program
   * the scaling
   */
  TRC( TRC_ID_MAIN_INFO, "CSTmTVOutDENC::WriteDAC456Scale [0x%x,0x%x,0x%x]", (uint32_t)m_DACScales[4],(uint32_t)m_DACScales[5],(uint32_t)m_DACScales[6] );
  tmp = ReadDENCReg(DENC_DAC34) & 0xF0;
  WriteDENCReg(DENC_DAC34, tmp | ((m_DACScales[4]&0x3F)>>2));
  WriteDENCReg(DENC_DAC45, (m_DACScales[5]&0x3F) | (m_DACScales[4]<<6));
  WriteDENCReg(DENC_DAC6, m_DACScales[6]);
}


void CSTmTVOutDENC::WriteChromaMultiply(void)
{
  /*
   * Set the Chroma multiplier.
   *
   * This may be called by subclasses that need to override ProgramOutputFormats
   */
  uint8_t val = ReadDENCReg(DENC_CMTTX) & ~DENC_CMTTX_C_MULT_MASK;
  val |= m_C_Multiply << DENC_CMTTX_C_MULT_SHIFT;
  WriteDENCReg(DENC_CMTTX, val);
}


void CSTmTVOutDENC::ProgramDACScalers(void)
{
  DACMult_Config_t syncvalues;
  /*
   * See if we have a loaded firmware configuration for the SD DACs 456.
   * If so then we assume the HD DACs will also have a firmware and set the
   * DENC scaling to 100%. If not then we fall back to the historic internally
   * calculated scales.
   */

  if(m_pDENCSync->GetHWConfiguration(STM_VIDEO_OUT_CVBS, &syncvalues))
  {
    uint8_t defaultScale;

    if(m_mainOutputFormat & STM_VIDEO_OUT_RGB)
      defaultScale = (uint8_t)DIV_ROUNDED((100000 - m_MinRGBScalePercentage), m_RGBScaleStep);
    else
      defaultScale = (uint8_t)DIV_ROUNDED((100000 - m_MinYScalePercentage), m_YScaleStep);

    m_DACScales[1] = defaultScale;
    m_DACScales[2] = defaultScale;
    m_DACScales[3] = defaultScale;

    /*
     * The names of the fields in the structures from the validation sync code
     * are confusing in the DENC case because we are looking at CVBS and S-Video.
     *
     * So we map the structure fields to the DACs by their order.
     */
    m_DACScales[4] = syncvalues.DACMult_Config_Cb;
    m_DACScales[5] = syncvalues.DACMult_Config_Y;
    m_DACScales[6] = syncvalues.DACMult_Config_Cr;
  }
  else
  {
    if(m_mainOutputFormat & STM_VIDEO_OUT_RGB)
    {
      m_DACScales[1] = m_DAC123RGBScale;
      m_DACScales[2] = m_DAC123RGBScale;
      m_DACScales[3] = m_DAC123RGBScale;
    }
    else
    {
      m_DACScales[1] = m_DAC123YScale;
      m_DACScales[2] = m_DAC123YScale;
      m_DACScales[3] = m_DAC123YScale;
    }

    m_DACScales[4] = m_DAC456YScale;
    m_DACScales[5] = m_DAC456YScale;
    m_DACScales[6] = m_DAC456YScale;
  }

  WriteDAC123Scale();
  WriteDAC456Scale();
}


void CSTmTVOutDENC::ProgramOutputFormats(void)
{
  /*
   * Although the HD TVOut integration allows us to switch which set of DENC
   * outputs go to the TVOut aux input (hence the HD DACs) and which go to the
   * "SD DACs", we pick the obvious usage and stick with it.
   *
   * DAC123 is routed back to the TVOut block for the HDFormatter and HD DACs,
   * hence the formats supported are YCrCb or RGB.
   *
   * DACs 456 go directly to the SD DACs and are permanently set to Y/C-CVBS.
   */

  m_pDENCSync->SetCalibrationValues(m_CurrentMode);
  ProgramDACScalers();

  uint8_t Cfg13 = 0;

  if(m_mainOutputFormat & STM_VIDEO_OUT_YUV)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmTVOutDENC::ProgramOutputFormats HD DAC output is YUV" );
    Cfg13 |= DENC_CFG13_DAC123_CONF_YUV_MAIN;
  }
  else if(m_mainOutputFormat & STM_VIDEO_OUT_RGB)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmTVOutDENC::ProgramOutputFormats HD DAC output is RGB" );
    Cfg13 |= DENC_CFG13_DAC123_CONF_RGB;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmTVOutDENC::ProgramOutputFormats HD DAC output is NONE" );
    Cfg13 |= DENC_CFG13_DAC123_CONF_NONE;
  }

  TRC( TRC_ID_MAIN_INFO, "CSTmTVOutDENC::ProgramOutputFormats SD DAC output is Y/C-CVBS" );
  Cfg13 |= DENC_CFG13_DAC456_CONF_YC_CVBS_MAIN;

  WriteDENCReg(DENC_CFG13, Cfg13);

  WriteChromaMultiply();

  TRC( TRC_ID_MAIN_INFO, "CSTmTVOutDENC::ProgramOutputFormats DENC_DAC13 = 0x%02x", (uint32_t)ReadDENCReg(DENC_DAC13) );
  TRC( TRC_ID_MAIN_INFO, "CSTmTVOutDENC::ProgramOutputFormats DENC_DAC2  = 0x%02x", (uint32_t)ReadDENCReg(DENC_DAC2) );
  TRC( TRC_ID_MAIN_INFO, "CSTmTVOutDENC::ProgramOutputFormats DENC_DAC34 = 0x%02x", (uint32_t)ReadDENCReg(DENC_DAC34) );
  TRC( TRC_ID_MAIN_INFO, "CSTmTVOutDENC::ProgramOutputFormats DENC_DAC45 = 0x%02x", (uint32_t)ReadDENCReg(DENC_DAC45) );
  TRC( TRC_ID_MAIN_INFO, "CSTmTVOutDENC::ProgramOutputFormats DENC_DAC6  = 0x%02x", (uint32_t)ReadDENCReg(DENC_DAC6) );
}


bool CSTmTVOutDENC::SetMainOutputFormat(uint32_t format)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  /*
   * The DENC in the HD TVOut context only takes video on its main input, but
   * it can be from the main or aux mixers (the mux is in front of the DENC).
   * The output currently using the DENC will call this during it's setup to
   * switch the DENC usage to its configuration.
   */
  if((format & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV)) == (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV))
    return false;

  m_mainOutputFormat = format;

  ProgramOutputFormats();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmTVOutDENC::SetAuxOutputFormat(uint32_t format)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  /*
   * Auxiliary video path not used on TVOut DENC integration.
   */
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return false;
}


uint32_t CSTmTVOutDENC::SetCompoundControl(stm_output_control_t ctrl, void *newVal)
{
  uint32_t ret = STM_OUT_NO_CTRL;
  TRCIN( TRC_ID_MAIN_INFO, "" );
  switch(ctrl)
  {
    case OUTPUT_CTRL_VIDEO_OUT_CALIBRATION:
    {
      bool RetOk = true;
      const stm_display_analog_calibration_setup_t *f = (stm_display_analog_calibration_setup_t *)newVal;
      if(f->type & DENC_FACTORS)
      {
        RetOk = m_pDENCSync->SetCalibrationValues(m_CurrentMode, &f->denc);
        if(RetOk)
          ProgramDACScalers();
      }
      ret = RetOk?STM_OUT_OK:STM_OUT_INVALID_VALUE;
      break;
    }
    default:
      break;
  }
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return ret;
}


uint32_t CSTmTVOutDENC::GetCompoundControl(stm_output_control_t ctrl, void *val) const
{
  uint32_t ret = STM_OUT_NO_CTRL;
  TRCIN( TRC_ID_MAIN_INFO, "" );
  switch(ctrl)
  {
    case OUTPUT_CTRL_VIDEO_OUT_CALIBRATION:
    {
      bool RetOk = false;
      stm_display_analog_calibration_setup_t *f = (stm_display_analog_calibration_setup_t *)val;
      vibe_os_zero_memory(&f->denc, sizeof(stm_display_analog_factors_t));
      RetOk = m_pDENCSync->GetCalibrationValues(&f->denc);
      if(RetOk)
        f->type |= DENC_FACTORS;
      ret = RetOk?STM_OUT_OK:STM_OUT_INVALID_VALUE;
      break;
    }
    default:
      break;
  }
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return ret;
}

