/***********************************************************************
 *
 * File: soc/sti7200/sti7200hdfdvo.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7200HDFDVO_H
#define _STI7200HDFDVO_H

#include <HDTVOutFormatter/stmhdfdvo.h>

class CSTi7200HDFDVO: public CSTmHDFDVO
{
public:
  CSTi7200HDFDVO(CDisplayDevice *,COutput *, ULONG ulDisplayBase, ULONG ulHDFormatterBase);
  virtual ~CSTi7200HDFDVO();

  bool Stop(void);

  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

protected:
  ULONG *m_pTVOutRegs;
  ULONG *m_pSysCfgRegs;
  bool  m_bOutputToDVP;

  bool SetOutputFormat(ULONG format, const stm_mode_line_t* pModeLine);

  void  WriteTVOutReg  (ULONG reg, ULONG value)  { g_pIOS->WriteRegister (m_pTVOutRegs + (reg>>2), value); }
  ULONG ReadTVOutReg   (ULONG reg)               { return g_pIOS->ReadRegister (m_pTVOutRegs + (reg>>2)); }

  void  WriteSysCfgReg (ULONG reg, ULONG value)  { g_pIOS->WriteRegister (m_pSysCfgRegs + (reg>>2), value); }
  ULONG ReadSysCfgReg  (ULONG reg)               { return g_pIOS->ReadRegister (m_pSysCfgRegs + (reg>>2)); }

private:
  CSTi7200HDFDVO(const CSTi7200HDFDVO&);
  CSTi7200HDFDVO& operator=(const CSTi7200HDFDVO&);
};


#endif /* _STI7200HDFDVO_H */
