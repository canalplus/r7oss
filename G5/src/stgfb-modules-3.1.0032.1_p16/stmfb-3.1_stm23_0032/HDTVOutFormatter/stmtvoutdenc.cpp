/***********************************************************************
 *
 * File: HDTVOutFormatter/stmtvoutdenc.cpp
 * Copyright (c) 2005-2009 STMicroelectronics Limited.
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

#include <HDTVOutFormatter/stmtvoutdenc.h>
#include <HDTVOutFormatter/stmtvoutteletext.h>

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

CSTmTVOutDENC::CSTmTVOutDENC(CDisplayDevice* pDev, ULONG baseAddr, CSTmTVOutTeletext *pTeletext): CSTmDENC(pDev, baseAddr, pTeletext)
{
  DENTRY();

  m_MinRGBScalePercentage = 81250; // 81.25%
  m_RGBScaleStep          = 590;   // 0.59%
  m_MinYScalePercentage   = 75000; // 75%
  m_YScaleStep            = 780;   // 0.78%

  DENTRY();
}

bool CSTmTVOutDENC::Start(COutput *parent, const stm_mode_line_t *pModeLine, ULONG tvStandard)
{
  DENTRY();

  /*
   * The HD TVOut integration requires a positive H sync and TnB instead of
   * the VSync.
   */
  return CSTmDENC::Start(parent, pModeLine, tvStandard, DENC_CFG0_ODHS_SLV, STVTG_SYNC_POSITIVE, STVTG_SYNC_TOP_NOT_BOT);
}


void CSTmTVOutDENC::WriteDAC123Scale(bool bForRGB)
{
  UCHAR tmp;
  /*
   * This is split out into a separate function to allow subclasses to program
   * the scaling
   */
  if(bForRGB)
  {
    DEBUGF2(2,("CSTmTVOutDENC::WriteDAC123Scale RGB scale set\n"));
    WriteDENCReg(DENC_DAC2, m_DAC123RGBScale);
    WriteDENCReg(DENC_DAC13, (m_DAC123RGBScale<<2) | ((m_DAC123RGBScale&0x30)>>4));
    tmp = ReadDENCReg(DENC_DAC34) & 0xF;
    WriteDENCReg(DENC_DAC34, tmp | (m_DAC123RGBScale<<4));
  }
  else
  {
    DEBUGF2(2,("CSTmTVOutDENC::WriteDAC123Scale YUV scale set\n"));
    WriteDENCReg(DENC_DAC2, m_DAC123YScale);
    WriteDENCReg(DENC_DAC13, (m_DAC123YScale<<2) | ((m_DAC123YScale&0x30)>>4));
    tmp = ReadDENCReg(DENC_DAC34) & 0xF;
    WriteDENCReg(DENC_DAC34, tmp | (m_DAC123YScale<<4));
  }
}


void CSTmTVOutDENC::WriteDAC456Scale(bool bForRGB)
{
  UCHAR tmp;
  /*
   * This is split out into a separate function to allow subclasses to program
   * the scaling
   */
  if(bForRGB)
  {
    DEBUGF2(2,("CSTmTVOutDENC::WriteDAC456Scale RGB scale set\n"));
    tmp = ReadDENCReg(DENC_DAC34) & 0xF0;
    WriteDENCReg(DENC_DAC34, tmp | ((m_DAC456RGBScale&0x3F)>>2));
    WriteDENCReg(DENC_DAC45, (m_DAC456RGBScale&0x3F) | (m_DAC456RGBScale<<6));
    WriteDENCReg(DENC_DAC6, m_DAC456RGBScale);
  }
  else
  {
    DEBUGF2(2,("CSTmTVOutDENC::WriteDAC456Scale YUV scale set\n"));
    tmp = ReadDENCReg(DENC_DAC34) & 0xF0;
    WriteDENCReg(DENC_DAC34, tmp | ((m_DAC456YScale&0x3F)>>2));
    WriteDENCReg(DENC_DAC45, (m_DAC456YScale&0x3F) | (m_DAC456YScale<<6));
    WriteDENCReg(DENC_DAC6, m_DAC456YScale);
  }
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
  UCHAR Cfg13 = 0;

  if(m_mainOutputFormat & STM_VIDEO_OUT_YUV)
  {
    DEBUGF2(2,("CSTmTVOutDENC::ProgramOutputFormats HD DAC output is YUV\n"));
    Cfg13 |= DENC_CFG13_DAC123_CONF_YUV_MAIN;
    WriteDAC123Scale(false);
  }
  else
  {
    DEBUGF2(2,("CSTmTVOutDENC::ProgramOutputFormats HD DAC output is RGB\n"));
    Cfg13 |= DENC_CFG13_DAC123_CONF_RGB;
    WriteDAC123Scale(true);
  }

  DEBUGF2(2,("CSTmTVOutDENC::ProgramOutputFormats SD DAC output is Y/C-CVBS\n"));
  Cfg13 |= DENC_CFG13_DAC456_CONF_YC_CVBS_MAIN;
  WriteDAC456Scale(false);

  WriteDENCReg(DENC_CFG13, Cfg13);

  DEBUGF2(2,("CSTmTVOutDENC::ProgramOutputFormats DENC_DAC13 = 0x%02lx\n", (ULONG)ReadDENCReg(DENC_DAC13)));
  DEBUGF2(2,("CSTmTVOutDENC::ProgramOutputFormats DENC_DAC2  = 0x%02lx\n", (ULONG)ReadDENCReg(DENC_DAC2)));
  DEBUGF2(2,("CSTmTVOutDENC::ProgramOutputFormats DENC_DAC34 = 0x%02lx\n", (ULONG)ReadDENCReg(DENC_DAC34)));
  DEBUGF2(2,("CSTmTVOutDENC::ProgramOutputFormats DENC_DAC45 = 0x%02lx\n", (ULONG)ReadDENCReg(DENC_DAC45)));
  DEBUGF2(2,("CSTmTVOutDENC::ProgramOutputFormats DENC_DAC6  = 0x%02lx\n", (ULONG)ReadDENCReg(DENC_DAC6)));
}


bool CSTmTVOutDENC::SetMainOutputFormat(ULONG format)
{
  /*
   * The DENC in the HD TVOut context only takes video on its main input, but
   * it can be from the main or aux mixers (the mux is in front of the DENC).
   * The output currently using the DENC will call this during it's setup to
   * switch the DENC usage to its configuration.
   */
  m_mainOutputFormat = format;

  ProgramOutputFormats();

  return true;
}


bool CSTmTVOutDENC::SetAuxOutputFormat(ULONG format)
{
  /*
   * Auxiliary video path not used on TVOut DENC integration.
   */
  return false;
}
