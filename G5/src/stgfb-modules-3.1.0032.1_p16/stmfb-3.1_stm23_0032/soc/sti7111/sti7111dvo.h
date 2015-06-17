/***********************************************************************
 *
 * File: soc/sti7111/sti7111dvo.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7111DVO_H
#define _STI7111DVO_H

#include <HDTVOutFormatter/stmhdfdvo.h>

class CSTi7111DVO: public CSTmHDFDVO
{
public:
  CSTi7111DVO(CDisplayDevice *,COutput *, ULONG ulDVOBase, ULONG ulHDFormatterBase);
  virtual ~CSTi7111DVO();

protected:
  ULONG *m_pTVOutRegs;
  ULONG *m_pClkGenBRegs;

  bool SetOutputFormat(ULONG format, const stm_mode_line_t* pModeLine);

  void  WriteTVOutReg  (ULONG reg, ULONG value)  { g_pIOS->WriteRegister (m_pTVOutRegs + (reg>>2), value); }
  ULONG ReadTVOutReg   (ULONG reg)               { return g_pIOS->ReadRegister (m_pTVOutRegs + (reg>>2)); }

  void  WriteClkGenBReg (ULONG reg, ULONG value)  { g_pIOS->WriteRegister (m_pClkGenBRegs + (reg>>2), value); }
  ULONG ReadClkGenBReg  (ULONG reg)               { return g_pIOS->ReadRegister (m_pClkGenBRegs + (reg>>2)); }

private:
  CSTi7111DVO(const CSTi7111DVO&);
  CSTi7111DVO& operator=(const CSTi7111DVO&);
};


#endif /* _STI7111DVO_H */
