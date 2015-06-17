/***********************************************************************
 *
 * File: soc/stb7100/stb7100denc.cpp
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

#include <STMCommon/stmdencregs.h>

#include "stb7100reg.h"
#include "stb7100denc.h"

#define DENC_CFG13_DAC456_CONF_YC_CVBS_MAIN     0x07

#define DENC_CFG13_DAC123_CONF_RGB              (0x2 << 3)
#define DENC_CFG13_DAC123_CONF_YUV_MAIN         (0x3 << 3)

#define DENC_CFG13_RGB_MAXDYN 0x40

/*
 * Note that these are different from the older STi5528/STm8000 devices.
 */
#define DENC_DAC13  0x11  /* Denc gain for dac 1&3 */
#define DENC_DAC34  0x12  /* Denc gain for dac 3&4 */
#define DENC_DAC45  0x13  /* Denc gain for dac 4&5 */
#define DENC_DAC2   0x6A  /* Denc gain for dac 2 */
#define DENC_DAC6   0x6B  /* Denc gain for dac 6 */

static stm_display_denc_filter_setup_t default_luma_filter_config = {
  STM_FLT_DIV_1024,
  {0x03, 0x01, 0xfffffff7, 0xfffffffe, 0x1E, 0x05, 0xffffffa4, 0xfffffff9, 0x144, 0x206}
};


CSTb7100DENC::CSTb7100DENC(CDisplayDevice* pDev, ULONG baseAddr, CSTmTeletext *pTeletext): CSTmDENC(pDev, baseAddr, pTeletext)
{
  /*
   * Recommended STb7100 design is for a 1.4v max output voltage
   * from the video DACs.
   */
  m_maxDAC123Voltage = 1400;
  m_maxDAC456Voltage = 1400;

  /*
   * The 7100 has finer DAC scaling control than the 5528/8K
   */
  m_MinRGBScalePercentage = 81250; // 81.25%
  m_RGBScaleStep          = 590;   // 0.59%
  m_MinYScalePercentage   = 75000; // 75%
  m_YScaleStep            = 780;   // 0.78%

  m_bEnableLumaFilter = true;
  m_lumaFilterConfig = default_luma_filter_config;
}

CSTb7100DENC::~CSTb7100DENC() {}


bool CSTb7100DENC::Start(COutput *parent, const stm_mode_line_t *pModeLine, ULONG tvStandard)
{
  DEBUGF2(2,("CSTb7100DENC::Start\n"));

  /*
   * The polarity of the href and vref from the VTGs on the 7100 is not
   * programmable (unlike the STm8000 and STi5528) as it uses a different
   * version of the VTG block. So we have to hard code the DENC to expect
   * the correct reference signal polarities instead of using the polarity
   * in the modeline.
   */
  return CSTmDENC::Start(parent, pModeLine, tvStandard, DENC_CFG0_ODHS_SLV, STVTG_SYNC_POSITIVE, STVTG_SYNC_NEGATIVE);
}


void CSTb7100DENC::WriteDAC123Scale(bool bForRGB)
{
  UCHAR tmp;
  /*
   * This is split out into a separate function to allow subclasses to program
   * the scaling
   */
  if(bForRGB)
  {
    DEBUGF2(2,("CSTb7100DENC::WriteDAC123Scale RGB scale set\n"));
    WriteDENCReg(DENC_DAC2, m_DAC123RGBScale);
    WriteDENCReg(DENC_DAC13, (m_DAC123RGBScale<<2) | ((m_DAC123RGBScale&0x30)>>4));
    tmp = ReadDENCReg(DENC_DAC34) & 0xF;
    WriteDENCReg(DENC_DAC34, tmp | (m_DAC123RGBScale<<4));
  }
  else
  {
    DEBUGF2(2,("CSTb7100DENC::WriteDAC123Scale YUV scale set\n"));
    WriteDENCReg(DENC_DAC2, m_DAC123YScale);
    WriteDENCReg(DENC_DAC13, (m_DAC123YScale<<2) | ((m_DAC123YScale&0x30)>>4));
    tmp = ReadDENCReg(DENC_DAC34) & 0xF;
    WriteDENCReg(DENC_DAC34, tmp | (m_DAC123YScale<<4));
  }
}


void CSTb7100DENC::WriteDAC456Scale(bool bForRGB)
{
  UCHAR tmp;
  /*
   * This is split out into a separate function to allow subclasses to program
   * the scaling
   */
  if(bForRGB)
  {
    DEBUGF2(2,("CSTb7100DENC::WriteDAC456Scale RGB scale set\n"));
    tmp = ReadDENCReg(DENC_DAC34) & 0xF0;
    WriteDENCReg(DENC_DAC34, tmp | ((m_DAC456RGBScale&0x3F)>>2));
    WriteDENCReg(DENC_DAC45, (m_DAC456RGBScale&0x3F) | (m_DAC456RGBScale<<6));
    WriteDENCReg(DENC_DAC6, m_DAC456RGBScale);
  }
  else
  {
    DEBUGF2(2,("CSTb7100DENC::WriteDAC456Scale YUV scale set\n"));
    tmp = ReadDENCReg(DENC_DAC34) & 0xF0;
    WriteDENCReg(DENC_DAC34, tmp | ((m_DAC456YScale&0x3F)>>2));
    WriteDENCReg(DENC_DAC45, (m_DAC456YScale&0x3F) | (m_DAC456YScale<<6));
    WriteDENCReg(DENC_DAC6, m_DAC456YScale);
  }
}


void CSTb7100DENC::ProgramOutputFormats(void)
{
  /*
   * On stb7100 DAC123 is routed to the main HD analogue outs via
   * the upsizer and must be either YCrCb or RGB. DACs 456 go to the SD
   * analogue outputs and are permanently set to Y/C-CVBS.
   */
  UCHAR Cfg13 = 0;

  if(m_mainOutputFormat & STM_VIDEO_OUT_YUV)
  {
    DEBUGF2(2,("CSTb7100DENC::ProgramOutputFormats HD DAC output is YUV\n"));
    Cfg13 |= DENC_CFG13_DAC123_CONF_YUV_MAIN;
    WriteDAC123Scale(false);
  }
  else
  {
    DEBUGF2(2,("CSTb7100DENC::ProgramOutputFormats HD DAC output is RGB\n"));
    Cfg13 |= DENC_CFG13_DAC123_CONF_RGB;
    WriteDAC123Scale(true);
  }

  DEBUGF2(2,("CSTb7100DENC::ProgramOutputFormats SD DAC output is Y/C-CVBS\n"));
  Cfg13 |= DENC_CFG13_DAC456_CONF_YC_CVBS_MAIN;
  WriteDAC456Scale(false);

  WriteDENCReg(DENC_CFG13, Cfg13);

  DEBUGF2(2,("CSTb7100DENC::ProgramOutputFormats DENC_DAC13 = 0x%02lx\n", (ULONG)ReadDENCReg(DENC_DAC13)));
  DEBUGF2(2,("CSTb7100DENC::ProgramOutputFormats DENC_DAC2  = 0x%02lx\n", (ULONG)ReadDENCReg(DENC_DAC2)));
  DEBUGF2(2,("CSTb7100DENC::ProgramOutputFormats DENC_DAC34 = 0x%02lx\n", (ULONG)ReadDENCReg(DENC_DAC34)));
  DEBUGF2(2,("CSTb7100DENC::ProgramOutputFormats DENC_DAC45 = 0x%02lx\n", (ULONG)ReadDENCReg(DENC_DAC45)));
  DEBUGF2(2,("CSTb7100DENC::ProgramOutputFormats DENC_DAC6  = 0x%02lx\n", (ULONG)ReadDENCReg(DENC_DAC6)));
}


bool CSTb7100DENC::SetMainOutputFormat(ULONG format)
{
  /*
   * This is a little different from previous chips as the DENC only takes
   * video on its main input, but it can be from the main or aux video pipelines
   * (the mux is in front of the DENC). The output currently using the DENC will
   * call this during it's setup to switch the DENC usage to its configuration.
   */
  m_mainOutputFormat = format;

  ProgramOutputFormats();

  return true;
}


bool CSTb7100DENC::SetAuxOutputFormat(ULONG format)
{
  /*
   * No auxillary video path on 710x DENC
   */
  return false;
}
