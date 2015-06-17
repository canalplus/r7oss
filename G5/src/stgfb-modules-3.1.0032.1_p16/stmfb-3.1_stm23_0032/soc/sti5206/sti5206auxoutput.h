/***********************************************************************
 *
 * File: soc/sti5206/sti5206auxoutput.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi5206AUXOUTPUT_H
#define _STi5206AUXOUTPUT_H

#include <HDTVOutFormatter/stmauxtvoutput.h>
#include "sti5206reg.h"

class CSTi5206Device;
class CSTi5206AuxMixer;
class CSTi5206ClkDivider;

class CSTi5206AuxOutput: public CSTmAuxTVOutput
{
public:
  CSTi5206AuxOutput(CSTi5206Device        *pDev,
                    CSTmSDVTG             *pVTG,
                    CSTmTVOutDENC         *pDENC,
                    CSTi5206AuxMixer      *pMixer,
                    CSTmFSynth            *pFSynth,
                    CSTmHDFormatterAWG    *pAWG,
                    CSTmHDFormatterOutput *pHDFOutput,
                    CSTi5206ClkDivider    *pClkDivider);

  bool ShowPlane(stm_plane_id_t planeID);

private:
  CSTi5206ClkDivider  *m_pClkDivider;

  void StartClocks(const stm_mode_line_t*);
  void SetAuxClockToHDFormatter(void);

  void EnableDACs(void);
  void DisableDACs(void);

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STi5206_SYSCFG_BASE + reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STi5206_SYSCFG_BASE +reg)>>2)); }

  CSTi5206AuxOutput(const CSTi5206AuxOutput&);
  CSTi5206AuxOutput& operator=(const CSTi5206AuxOutput&);
};


#endif //_STi5206AUXOUTPUT_H
