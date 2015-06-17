/***********************************************************************
 *
 * File: soc/sti7108/sti7108mainoutput.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7108MAINOUTPUT_H
#define _STi7108MAINOUTPUT_H

#include <HDTVOutFormatter/stmhdfoutput.h>
#include "sti7108reg.h"

class CSTi7108Device;
class CSTi7108MainMixer;
class CSTi7108ClkDivider;

class CSTi7108MainOutput: public CSTmHDFormatterOutput
{
public:
  CSTi7108MainOutput(CSTi7108Device *,
                     CSTmSDVTG *,
                     CSTmSDVTG *,
                     CSTmTVOutDENC *,
                     CSTi7108MainMixer *,
                     CSTmFSynth *,
                     CSTmHDFormatterAWG *,
                     CSTi7108ClkDivider *);

  virtual ~CSTi7108MainOutput();

  bool ShowPlane(stm_plane_id_t planeID);

protected:
  void StartHDClocks(const stm_mode_line_t*);
  void StartSDInterlacedClocks(const stm_mode_line_t*);
  void StartSDProgressiveClocks(const stm_mode_line_t*);
  void SetMainClockToHDFormatter(void);

  void EnableDACs(void);
  void DisableDACs(void);

private:
  CSTi7108ClkDivider *m_pClkDivider;

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STi7108_SYSCFG_BANK3_BASE + reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STi7108_SYSCFG_BANK3_BASE +reg)>>2)); }

  CSTi7108MainOutput(const CSTi7108MainOutput&);
  CSTi7108MainOutput& operator=(const CSTi7108MainOutput&);
};


#endif //_STi7108MAINOUTPUT_H
