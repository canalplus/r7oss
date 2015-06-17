/***********************************************************************
 *
 * File: soc/stb7100/stb7100dvo.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STb7100DVO_H
#define _STb7100DVO_H

#include <Generic/Output.h>

class CDisplayDevice;
class CSTb7100MainOutput;

class CSTb7100DVO: public COutput
{
public:
  CSTb7100DVO(CDisplayDevice *, CSTb7100MainOutput *);
  virtual ~CSTb7100DVO(void);

  ULONG GetCapabilities(void) const;

  bool Start(const stm_mode_line_t*, ULONG tvStandard);
  bool Stop(void);
  void Suspend(void);
  void Resume(void);

  bool HandleInterrupts(void);
  
  bool CanShowPlane(stm_plane_id_t planeID);
  bool ShowPlane   (stm_plane_id_t planeID);
  void HidePlane   (stm_plane_id_t planeID);
  bool SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate);
  bool GetPlaneDepth(stm_plane_id_t planeID, int *depth) const;
 
  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

protected:
  CSTb7100MainOutput *m_pMainOutput;
  
  ULONG *m_pDevRegs;
  ULONG m_ulVOSOffset;
  ULONG m_ulClkOffset;

  void WriteVOSReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulVOSOffset+reg)>>2), val); }
  ULONG ReadVOSReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulVOSOffset+reg)>>2)); }

  void WriteClkReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulClkOffset+reg)>>2), val); }
  ULONG ReadClkReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulClkOffset+reg)>>2)); }

};


class CSTb7109Cut3DVO: public CSTb7100DVO
{
public:
  CSTb7109Cut3DVO(CDisplayDevice *, CSTb7100MainOutput *);
  virtual ~CSTb7109Cut3DVO(void);

  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

private:
  ULONG m_ulSysCfgOffset;
  bool  m_bOutputToDVP;

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulSysCfgOffset+reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulSysCfgOffset+reg)>>2)); }
};

#endif //_STb7100DVO_H
