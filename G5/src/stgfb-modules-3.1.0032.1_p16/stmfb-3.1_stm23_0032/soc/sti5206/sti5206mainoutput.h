/***********************************************************************
 *
 * File: soc/sti5206/sti5206mainoutput.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi5206MAINOUTPUT_H
#define _STi5206MAINOUTPUT_H

#include <HDTVOutFormatter/stmhdfoutput.h>
#include "sti5206reg.h"

class CSTi5206Device;
class CSTi5206MainMixer;
class CSTi5206ClkDivider;

class CSTi5206MainOutput: public CSTmHDFormatterOutput
{
public:
  CSTi5206MainOutput(CSTi5206Device *,
                     CSTmSDVTG *,
                     CSTmSDVTG *,
                     CSTmTVOutDENC *,
                     CSTi5206MainMixer *,
                     CSTmFSynth *,
                     CSTmHDFormatterAWG *,
                     CSTi5206ClkDivider *);

  virtual ~CSTi5206MainOutput();

  bool ShowPlane(stm_plane_id_t planeID);

protected:
  void StartHDClocks(const stm_mode_line_t*);
  void StartSDInterlacedClocks(const stm_mode_line_t*);
  void StartSDProgressiveClocks(const stm_mode_line_t*);
  void SetMainClockToHDFormatter(void);

  void EnableDACs(void);
  void DisableDACs(void);

private:
  CSTi5206ClkDivider *m_pClkDivider;

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STi5206_SYSCFG_BASE + reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STi5206_SYSCFG_BASE +reg)>>2)); }

  CSTi5206MainOutput(const CSTi5206MainOutput&);
  CSTi5206MainOutput& operator=(const CSTi5206MainOutput&);
};


#endif //_STi5206MAINOUTPUT_H
