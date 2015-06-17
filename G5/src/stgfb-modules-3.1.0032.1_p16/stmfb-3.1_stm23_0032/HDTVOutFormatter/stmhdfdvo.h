/***********************************************************************
 *
 * File: HDTVOutFormatter/stmhdfdvo.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_HDFDVO_H
#define _STM_HDFDVO_H

#include <STMCommon/stmfdvo.h>


class CSTmHDFDVO: public CSTmFDVO
{
public:
  CSTmHDFDVO(CDisplayDevice *, ULONG ulFDVOOffset, ULONG ulHDFOffset, COutput *);
  virtual ~CSTmHDFDVO();

  void  SetControl(stm_output_control_t, ULONG ulNewVal);

protected:
  ULONG *m_pHDFRegs;

  bool SetOutputFormat(ULONG format, const stm_mode_line_t* pModeLine);

  void  WriteHDFmtReg (ULONG reg, ULONG value)  { g_pIOS->WriteRegister (m_pHDFRegs + (reg>>2), value); }
  ULONG ReadHDFmtReg  (ULONG reg)               { return g_pIOS->ReadRegister (m_pHDFRegs + (reg>>2)); }


private:
  CSTmHDFDVO(const CSTmHDFDVO&);
  CSTmHDFDVO& operator=(const CSTmHDFDVO&);
};


#endif /* _STM_HDFDVO_H */
