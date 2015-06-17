/***********************************************************************
 *
 * File: soc/stb7100/stb7100hdmi.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STb7100HDMI_H
#define _STb7100HDMI_H

#include <STMCommon/stmhdmi.h>

class CSTb710xHDMI: public CSTmHDMI
{
public:
  CSTb710xHDMI(CDisplayDevice *, CSTb7100MainOutput *, CSTb7100VTG1 *);
  virtual ~CSTb710xHDMI(void);

  bool Stop(void);

  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

  void HandleCallback(int opcode);

protected:
  bool PreConfiguration(const stm_mode_line_t*, ULONG tvStandard);
  bool PostConfiguration(const stm_mode_line_t*, ULONG tvStandard);
  bool PreAuth() const;
  void PostAuth(bool auth);

  void GetAudioHWState(stm_hdmi_audio_state_t *state);
  int  GetAudioFrequency(void);

  void SetupRejectionPLL(const stm_mode_line_t*, ULONG tvStandard);

  bool SetOutputFormat(ULONG format);
  bool SetInputSource(ULONG  source);

  CSTb7100VTG1 *m_pVTG1;

  /*
   * To correctly drive HDMI on the 7100 requires us to touch many
   * different bits of the system.
   */
  ULONG m_ulVOSOffset;
  ULONG m_ulSysCfgOffset;
  ULONG m_ulClkGBOffset;
  ULONG m_ulAudioOffset;
  ULONG m_ulPRCONVOffset;
  ULONG m_ulSPDIFOffset;

  bool  m_bUse2xRefForSDRejectionPLL;

  ULONG ReadAUDReg(ULONG reg)   { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulAudioOffset+reg)>>2)); }
  void WriteAUDReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulAudioOffset+reg)>>2), val); }

  ULONG ReadPRCONVReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulPRCONVOffset+reg)>>2)); }
  ULONG ReadSPDIFReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulSPDIFOffset+reg)>>2)); }

  void WriteClkReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulClkGBOffset+reg)>>2), val); }
  ULONG ReadClkReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulClkGBOffset+reg)>>2)); }

  void WriteVOSReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulVOSOffset+reg)>>2), val); }
  ULONG ReadVOSReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulVOSOffset+reg)>>2)); }

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulSysCfgOffset+reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulSysCfgOffset+reg)>>2)); }
};


class CSTb7100HDMI: public CSTb710xHDMI
{
public:
  CSTb7100HDMI(CDisplayDevice *, CSTb7100MainOutput *, CSTb7100VTG1 *);
  virtual ~CSTb7100HDMI(void);

  bool PreConfiguration(const stm_mode_line_t*, ULONG tvStandard);

private:
  bool m_bInitedTwice;
};


class CSTb7109Cut3HDMI: public CSTb710xHDMI
{
public:
  CSTb7109Cut3HDMI(CDisplayDevice *, CSTb7100MainOutput *, CSTb7100VTG1 *);
  virtual ~CSTb7109Cut3HDMI(void);

  bool Create(void);

  bool PreConfiguration(const stm_mode_line_t*, ULONG tvStandard);
  void SetControl(stm_output_control_t, ULONG ulNewVal);
};

#endif //_STb7100HDMI_H
