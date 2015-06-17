/***********************************************************************
 *
 * File: display/ip/hdmi/stmhdmitx3g4_c28_phy.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMHDMITX3G4_C28_PHY_H
#define _STMHDMITX3G4_C28_PHY_H

#include "stmhdmiphy.h"

class CDisplayDevice;

class CSTmHDMITx3G4_C28_Phy: public CSTmHDMIPhy
{
public:
  CSTmHDMITx3G4_C28_Phy(CDisplayDevice *pDev, uint32_t phyoffset);
  ~CSTmHDMITx3G4_C28_Phy(void);

  bool Start(const stm_display_mode_t* ,uint32_t outputFormat);
  void Stop(void);

protected:
  uint32_t *m_pDevRegs;

private:
  uint32_t  m_uPhyOffset;

  void WritePhyReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uPhyOffset+reg), val); }
  uint32_t ReadPhyReg(uint32_t reg) const { return vibe_os_read_register(m_pDevRegs, (m_uPhyOffset+reg)); }

  CSTmHDMITx3G4_C28_Phy(const CSTmHDMITx3G4_C28_Phy&);
  CSTmHDMITx3G4_C28_Phy& operator=(const CSTmHDMITx3G4_C28_Phy&);
};

#endif //_STMHDMITX3G4_C28_PHY_H
