/***********************************************************************
 *
 * File: soc/sti7111/sti7111hdmi.h
 * Copyright (c) 2008,2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7111HDMI_H
#define _STI7111HDMI_H

#include <HDTVOutFormatter/stmhdfhdmi.h>

#include "sti7111reg.h"

class CSTmSDVTG;
class COutput;

class CSTi7111HDMI: public CSTmHDFormatterHDMI
{
public:
  CSTi7111HDMI(CDisplayDevice *pDev,
               COutput        *pMainOutput,
               CSTmSDVTG      *pVTG,
#ifdef CONFIG_SH_TC_DCI804RMU
               ULONG           clockgenCInput              = AUD_FSYN_CFG_SYSA_IN,
#else
               ULONG           clockgenCInput              = AUD_FSYN_CFG_SYSB_IN,
#endif
               stm_hdmi_hardware_version_t     hdmiVersion = STM_HDMI_HW_V_1_4,
               stm_hdf_hdmi_hardware_version_t hdfRevision = STM_HDF_HDMI_REV2);

  virtual ~CSTi7111HDMI(void);

  bool Create(void);

  bool Stop(void);

protected:
  bool SetInputSource(ULONG  source);
  bool PreConfiguration(const stm_mode_line_t*, ULONG tvStandard);

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulSysCfgOffset+reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulSysCfgOffset+reg)>>2)); }

  ULONG ReadTVOutReg(ULONG reg)   { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulTVOutOffset+reg)>>2)); }
  void WriteTVOutReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulTVOutOffset+reg)>>2), val); }

private:
  int  GetAudioFrequency(void);
  void SetupRejectionPLL(const stm_mode_line_t*, ULONG tvStandard);
  void StartAudioClocks(void);

  ULONG m_ulSysCfgOffset;
  ULONG m_ulAudioOffset;
  ULONG m_ulTVOutOffset;
  ULONG m_ulClkOffset;

  ULONG m_ulClockgenCInput;

  void WriteClkReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulClkOffset + reg)>>2), val); }
  ULONG ReadClkReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulClkOffset +reg)>>2)); }

  ULONG ReadAUDReg(ULONG reg)   { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulAudioOffset+reg)>>2)); }
  void WriteAUDReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulAudioOffset+reg)>>2), val); }

  CSTi7111HDMI(const CSTi7111HDMI&);
  CSTi7111HDMI& operator=(const CSTi7111HDMI&);

};

#endif //_STI7111HDMI_H
