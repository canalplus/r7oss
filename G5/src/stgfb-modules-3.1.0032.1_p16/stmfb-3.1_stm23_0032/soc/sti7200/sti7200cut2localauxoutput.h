/***********************************************************************
 *
 * File: soc/sti7200/sti7200cut2localauxoutput.h
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7200CUT2LOCALAUXOUTPUT_H
#define _STi7200CUT2LOCALAUXOUTPUT_H

#include <HDTVOutFormatter/stmauxtvoutput.h>

class CSTi7200Cut2LocalDevice;
class CSTi7200LocalAuxMixer;

class CSTi7200Cut2LocalAuxOutput: public CSTmAuxTVOutput
{
public:
  CSTi7200Cut2LocalAuxOutput(CSTi7200Cut2LocalDevice *,
                             CSTmSDVTG *,
                             CSTmTVOutDENC *,
                             CSTi7200LocalAuxMixer *,
                             CSTmFSynthType2 *,
                             CSTmHDFormatterAWG *,
                             CSTmHDFormatterOutput *);

  bool ShowPlane(stm_plane_id_t planeID);

private:
  void StartClocks(const stm_mode_line_t*);
  void SetAuxClockToHDFormatter(void);

  void EnableDACs(void);
  void DisableDACs(void);

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STi7200_SYSCFG_BASE + reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STi7200_SYSCFG_BASE +reg)>>2)); }

  void WriteClkReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STi7200_CLKGEN_BASE + reg)>>2), val); }
  ULONG ReadClkReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STi7200_CLKGEN_BASE +reg)>>2)); }

  CSTi7200Cut2LocalAuxOutput(const CSTi7200Cut2LocalAuxOutput&);
  CSTi7200Cut2LocalAuxOutput& operator=(const CSTi7200Cut2LocalAuxOutput&);
};


#endif //_STi7200CUT2LOCALAUXOUTPUT_H
