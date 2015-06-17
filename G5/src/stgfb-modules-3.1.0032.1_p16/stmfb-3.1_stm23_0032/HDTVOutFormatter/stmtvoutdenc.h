/***********************************************************************
 *
 * File: HDTVOutFormatter/stmtvoutdenc.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STM_TVOUT_DENC_H
#define STM_TVOUT_DENC_H

#include <STMCommon/stmdenc.h>

class CDisplayDevice;
class COutput;
class CSTmTVOutTeletext;

class CSTmTVOutDENC : public CSTmDENC
{
public:
  CSTmTVOutDENC(CDisplayDevice* pDev, ULONG baseAddr, CSTmTVOutTeletext *pTeletext = 0);

  bool Start(COutput *, const stm_mode_line_t *, ULONG tvStandard);

  bool SetMainOutputFormat(ULONG);
  bool SetAuxOutputFormat(ULONG);

protected:
  void  WriteDAC123Scale(bool bForRGB);
  void  WriteDAC456Scale(bool bForRGB);

private:
  void ProgramOutputFormats(void);

  CSTmTVOutDENC(const CSTmTVOutDENC&);
  CSTmTVOutDENC& operator=(const CSTmTVOutDENC&);
};

#endif // STM_TVOUT_DENC_H
