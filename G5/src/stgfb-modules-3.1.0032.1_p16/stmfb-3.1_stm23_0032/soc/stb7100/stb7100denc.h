/***********************************************************************
 *
 * File: soc/stb7100/stb7100denc.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STb7100DENC_H
#define STb7100DENC_H

#include <STMCommon/stmdenc.h>

class CDisplayDevice;
class CGenericGammaVTG;
class COutput;
class CSTmTeletext;

class CSTb7100DENC : public CSTmDENC
{
public:
  CSTb7100DENC(CDisplayDevice* pDev, ULONG baseAddr, CSTmTeletext *pTeletext = 0);
  ~CSTb7100DENC(void);

  bool Start(COutput *, const stm_mode_line_t *, ULONG tvStandard);

  bool SetMainOutputFormat(ULONG);
  bool SetAuxOutputFormat(ULONG);

protected:
  void  WriteDAC123Scale(bool bForRGB);
  void  WriteDAC456Scale(bool bForRGB);

private:
  void ProgramOutputFormats(void);
};

#endif // STb7100DENC_H
