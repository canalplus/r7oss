/***********************************************************************
 *
 * File: display/ip/hdmi/stmhdmitx2g25_33_c65_phy.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMHDMITX2G25_33_C65_PHY_H
#define _STMHDMITX2G25_33_C65_PHY_H

#include "stmhdmiphy.h"

class CDisplayDevice;

class CSTmHDMITx2G25_33_C65_Phy: public CSTmHDMIPhy
{
public:
  CSTmHDMITx2G25_33_C65_Phy(CDisplayDevice *pDev, uint32_t phyoffset);
  ~CSTmHDMITx2G25_33_C65_Phy(void);

  bool Start(const stm_display_mode_t* ,uint32_t outputFormat);
  void Stop(void);

protected:
  uint32_t *m_pDevRegs;
  bool      m_bMultiplyTMDSClockForPixelRepetition;

private:
  uint32_t  m_uPhyOffset;

  void WritePhyReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uPhyOffset+reg), val); }
  uint32_t ReadPhyReg(uint32_t reg) const { return vibe_os_read_register(m_pDevRegs, (m_uPhyOffset+reg)); }

  CSTmHDMITx2G25_33_C65_Phy(const CSTmHDMITx2G25_33_C65_Phy&);
  CSTmHDMITx2G25_33_C65_Phy& operator=(const CSTmHDMITx2G25_33_C65_Phy&);
};

#endif //_STMHDMITX2G25_33_C65_PHY_H
