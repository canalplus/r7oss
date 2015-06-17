/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407clkdivider.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STIH407_CLK_DIVIDER_H
#define _STIH407_CLK_DIVIDER_H

#include <display/ip/tvout/stmtvoutclkdivider.h>

class CDisplayDevice;

class CSTiH407ClkDivider: public CSTmTVOutClkDivider
{
public:
  CSTiH407ClkDivider( CDisplayDevice *pDev );

  virtual bool Enable(stm_clk_divider_output_name_t,
                      stm_clk_divider_output_source_t,
                      stm_clk_divider_output_divide_t);

  virtual bool Disable(stm_clk_divider_output_name_t);
  virtual bool isEnabled(stm_clk_divider_output_name_t) const;
  virtual bool getDivide(stm_clk_divider_output_name_t, stm_clk_divider_output_divide_t *) const;
  virtual bool getSource(stm_clk_divider_output_name_t, stm_clk_divider_output_source_t *) const;

private:
  uint32_t *m_pFlexClkGenReg;

  void WriteFlexClkGenReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pFlexClkGenReg, reg, val); }
  uint32_t ReadFlexClkGenReg(uint32_t reg) const { return vibe_os_read_register(m_pFlexClkGenReg, reg); }

  CSTiH407ClkDivider(const CSTiH407ClkDivider&);
  CSTiH407ClkDivider& operator=(const CSTiH407ClkDivider&);
};

#endif // _STIH407_CLK_DIVIDER_H
