/***********************************************************************
 *
 * File: soc/sti7108/sti7108hdmi.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7108HDMI_H
#define _STI7108HDMI_H

#include <HDTVOutFormatter/stmhdfhdmi.h>
#include <HDTVOutFormatter/stmtvoutclkdivider.h>

class CSTmSDVTG;
class COutput;

class CSTi7108HDMI: public CSTmHDFormatterHDMI
{
public:
  CSTi7108HDMI(CDisplayDevice      *pDev,
               COutput             *pMainOutput,
               CSTmSDVTG           *pVTG,
               CSTmTVOutClkDivider *pClkDivider);

  virtual ~CSTi7108HDMI(void);

  bool Create(void);

  bool Stop(void);

private:
  ULONG m_ulSysCfg0Offset;
  ULONG m_ulSysCfg3Offset;
  ULONG m_ulSysCfg4Offset;

  CSTmTVOutClkDivider *m_pClkDivider;

  int  GetAudioFrequency(void);

  bool PreConfiguration(const stm_mode_line_t*, ULONG tvStandard);
  bool SetupRejectionPLL(const stm_mode_line_t*, ULONG tvStandard, stm_clk_divider_output_divide_t divide);
  bool PostV29PhyConfiguration(const stm_mode_line_t*, ULONG tvStandard);

  void StartAudioClocks(void);

  void WriteSysCfg0Reg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulSysCfg0Offset+reg)>>2), val); }
  ULONG ReadSysCfg0Reg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulSysCfg0Offset+reg)>>2)); }

  void WriteSysCfg3Reg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulSysCfg3Offset+reg)>>2), val); }
  ULONG ReadSysCfg3Reg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulSysCfg3Offset+reg)>>2)); }

  void WriteSysCfg4Reg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulSysCfg4Offset+reg)>>2), val); }
  ULONG ReadSysCfg4Reg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulSysCfg4Offset+reg)>>2)); }

  CSTi7108HDMI(const CSTi7108HDMI&);
  CSTi7108HDMI& operator=(const CSTi7108HDMI&);

};

#endif //_STI7108HDMI_H
