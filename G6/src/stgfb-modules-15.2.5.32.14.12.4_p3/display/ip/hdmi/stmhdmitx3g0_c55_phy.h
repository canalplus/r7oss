/***********************************************************************
 *
 * File: display/ip/hdmi/stmhdmitx3g0_c55_phy.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMHDMITX3G0_C55_PHY_H
#define _STMHDMITX3G0_C55_PHY_H

#include "stmhdmiphy.h"

class CDisplayDevice;

class CSTmHDMITx3G0_C55_Phy: public CSTmHDMIPhy
{
public:
  CSTmHDMITx3G0_C55_Phy(CDisplayDevice *pDev, uint32_t phyoffset);
  ~CSTmHDMITx3G0_C55_Phy(void);

  bool Start(const stm_display_mode_t* ,uint32_t outputFormat);
  void Stop(void);

protected:
  uint32_t *m_pDevRegs;
  bool      m_bMultiplyTMDSClockForPixelRepetition;

private:
  uint32_t  m_uPhyOffset;

  void WritePhyReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uPhyOffset+reg), val); }
  uint32_t ReadPhyReg(uint32_t reg) const { return vibe_os_read_register(m_pDevRegs, (m_uPhyOffset+reg)); }

  CSTmHDMITx3G0_C55_Phy(const CSTmHDMITx3G0_C55_Phy&);
  CSTmHDMITx3G0_C55_Phy& operator=(const CSTmHDMITx3G0_C55_Phy&);
};

#endif //_STMHDMITX3G0_C55_PHY_H
