/***********************************************************************
 *
 * File: display/ip/tvout/stmtvoutdenc.h
 * Copyright (c) 2009-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STM_TVOUT_DENC_H
#define STM_TVOUT_DENC_H

#include <display/ip/stmdenc.h>
#include <display/ip/analogsync/denc/stmdencsync.h>

class CDisplayDevice;
class COutput;
class CSTmTVOutTeletext;

#define DENC_DELAY                 (2)

class CSTmTVOutDENC : public CSTmDENC
{
public:
  CSTmTVOutDENC(CDisplayDevice* pDev, uint32_t baseAddr, CSTmTVOutTeletext *pTeletext = 0);
  ~CSTmTVOutDENC(void);

  bool Create(void);

  bool Start(COutput *, const stm_display_mode_t *);

  bool SetMainOutputFormat(uint32_t);
  bool SetAuxOutputFormat(uint32_t);

  uint32_t SetCompoundControl(stm_output_control_t ctrl, void *newVal);
  uint32_t GetCompoundControl(stm_output_control_t ctrl, void *val) const;

protected:
  CSTmDENCSync *m_pDENCSync;
  uint8_t       m_DACScales[7]; // Deliberately one element too big so we can match DAC naming

  void  ProgramDACScalers(void);
  void  WriteDAC123Scale();
  void  WriteDAC456Scale();
  void  WriteChromaMultiply();

private:
  void ProgramOutputFormats(void);

  CSTmTVOutDENC(const CSTmTVOutDENC&);
  CSTmTVOutDENC& operator=(const CSTmTVOutDENC&);
};

#endif // STM_TVOUT_DENC_H
