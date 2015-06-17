/***********************************************************************
 *
 * File: soc/stb7100/stb7100analogueouts.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STb7100ANALOGUEOUTS_H
#define _STb7100ANALOGUEOUTS_H

#include <STMCommon/stmmasteroutput.h>

class CDisplayDevice;
class CSTmFSynth;
class CSTb7100VTG1;
class CSTb7100VTG2;
class CSTb7100MainMixer;
class CSTb7100AuxMixer;
class CSTb7100DENC;
class CSTb7100AWGAnalog;

class CSTb7100MainOutput: public CSTmMasterOutput
{
public:
  CSTb7100MainOutput(CDisplayDevice *,
                     CSTb7100VTG1 *,
                     CSTb7100VTG2 *,
                     CSTb7100DENC *,
                     CGammaMixer *,
                     CSTmFSynth *,
                     CSTmFSynth *,
                     CSTb7100AWGAnalog *,
                     stm_plane_id_t sharedPlane);

  virtual ~CSTb7100MainOutput();

  bool Start(const stm_mode_line_t*, ULONG tvStandard);
  bool Stop(void);

  bool ShowPlane(stm_plane_id_t planeID);

  ULONG GetCapabilities(void) const;

  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

  const stm_mode_line_t* SupportedMode(const stm_mode_line_t *) const;

  void SetSlaveOutputs(COutput *dvo, COutput *hdmi);

  void Enable108MHzClockForSD(void) { m_bUse108MHzForSD = true; }

private:
  CSTb7100VTG2      *m_pVTG2; // Slave VTG for SD modes on HD display outputs
  COutput           *m_pDVO;
  COutput           *m_pHDMI;
  CSTmFSynth        *m_pFSynthAux;
  CSTb7100AWGAnalog *m_pAWGAnalog;

  bool m_bUse108MHzForSD;
  bool m_bDacHdPower;

  stm_plane_id_t m_sharedGDP;

  stm_ycbcr_colorspace_t m_colorspaceMode;

  bool StartHDDisplay(const stm_mode_line_t*);
  bool StartSDDisplay(const stm_mode_line_t*, ULONG tvStandard);
  bool StartSDInterlacedDisplay(const stm_mode_line_t*, ULONG tvStandard);
  bool StartSDProgressiveDisplay(const stm_mode_line_t*, ULONG tvStandard);

  bool SetOutputFormat(ULONG);
  void EnableDACs(void);
  void DisableDACs(void);

  void WriteVOSReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STb7100_VOS_BASE + reg)>>2), val); }
  ULONG ReadVOSReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STb7100_VOS_BASE +reg)>>2)); }

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STb7100_SYSCFG_BASE + reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STb7100_SYSCFG_BASE +reg)>>2)); }

  void WriteClkReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STb7100_CLKGEN_BASE + reg)>>2), val); }
  ULONG ReadClkReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STb7100_CLKGEN_BASE +reg)>>2)); }

  CSTb7100MainOutput(const CSTb7100MainOutput&);
  CSTb7100MainOutput& operator=(const CSTb7100MainOutput&);
};


class CSTb7100AuxOutput: public CSTmMasterOutput
{
public:
  CSTb7100AuxOutput(CDisplayDevice *,
                    CSTb7100VTG2 *,
                    CSTb7100DENC *,
                    CGammaMixer *,
                    CSTmFSynth *,
                    stm_plane_id_t sharedPlane);

  virtual ~CSTb7100AuxOutput();

  bool Start(const stm_mode_line_t*, ULONG tvStandard);

  bool ShowPlane(stm_plane_id_t planeID);

  ULONG GetCapabilities(void) const;

  const stm_mode_line_t* SupportedMode(const stm_mode_line_t *) const;

private:
  stm_plane_id_t m_sharedGDP;

  bool SetOutputFormat(ULONG);
  void EnableDACs(void);
  void DisableDACs(void);

  void WriteVOSReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STb7100_VOS_BASE + reg)>>2), val); }
  ULONG ReadVOSReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STb7100_VOS_BASE + reg)>>2)); }

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STb7100_SYSCFG_BASE + reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STb7100_SYSCFG_BASE +reg)>>2)); }

  void WriteClkReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STb7100_CLKGEN_BASE + reg)>>2), val); }
  ULONG ReadClkReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STb7100_CLKGEN_BASE +reg)>>2)); }

  CSTb7100AuxOutput(const CSTb7100AuxOutput&);
  CSTb7100AuxOutput& operator=(const CSTb7100AuxOutput&);
};


#endif //_STb7100ANALOGUEOUTS_H
