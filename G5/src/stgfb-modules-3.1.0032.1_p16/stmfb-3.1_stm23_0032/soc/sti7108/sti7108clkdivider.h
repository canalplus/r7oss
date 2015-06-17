/***********************************************************************
 *
 * File:soc/sti7108/sti7108clkdivider.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7108_CLK_DIVIDER_H
#define _STI7108_CLK_DIVIDER_H

#include <HDTVOutFormatter/stmtvoutclkdivider.h>

#include "sti7108reg.h"

class CSTi7108Device;

class CSTi7108ClkDivider: public CSTmTVOutClkDivider
{
public:
  CSTi7108ClkDivider(CSTi7108Device *pDev);

  virtual bool Enable(stm_clk_divider_output_name_t,
                      stm_clk_divider_output_source_t,
                      stm_clk_divider_output_divide_t);

  virtual bool Disable(stm_clk_divider_output_name_t);
  virtual bool isEnabled(stm_clk_divider_output_name_t) const;
  virtual bool getDivide(stm_clk_divider_output_name_t, stm_clk_divider_output_divide_t *) const;
  virtual bool getSource(stm_clk_divider_output_name_t, stm_clk_divider_output_source_t *) const;

private:
  ULONG *m_pSysCfgReg;

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pSysCfgReg + (reg>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) const { return g_pIOS->ReadRegister(m_pSysCfgReg + (reg>>2)); }

  CSTi7108ClkDivider(const CSTi7108ClkDivider&);
  CSTi7108ClkDivider& operator=(const CSTi7108ClkDivider&);
};

#endif // _STI7108_CLK_DIVIDER_H
