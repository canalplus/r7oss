/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407hdmiphy.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STiH407HDMIPHY_H
#define _STiH407HDMIPHY_H

#include <display/ip/hdmi/stmhdmitx3g4_c28_phy.h>

class CSTiH407HDMIPhy: public CSTmHDMITx3G4_C28_Phy
{
public:
  CSTiH407HDMIPhy(CDisplayDevice *pDev);
  ~CSTiH407HDMIPhy(void);

  bool Start(const stm_display_mode_t*, uint32_t outputFormat);
  void Stop(void);

private:
  uint32_t m_uClockGenA12;

  bool SetupRejectionPLL(const stm_display_mode_t*);
  void DisableRejectionPLL(void);

  void WriteClkGenA12Reg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uClockGenA12+reg), val); }
  uint32_t ReadClkGenA12Reg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_uClockGenA12+reg)); }

  CSTiH407HDMIPhy(const CSTiH407HDMIPhy&);
  CSTiH407HDMIPhy& operator=(const CSTiH407HDMIPhy&);
};

#endif //_STiH407HDMIPHY_H
