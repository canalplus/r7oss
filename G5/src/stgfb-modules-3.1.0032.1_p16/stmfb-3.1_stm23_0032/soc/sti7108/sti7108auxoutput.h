/***********************************************************************
 *
 * File: soc/sti7108/sti7108auxoutput.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7108AUXOUTPUT_H
#define _STi7108AUXOUTPUT_H

#include <HDTVOutFormatter/stmauxtvoutput.h>
#include "sti7108reg.h"

class CSTi7108Device;
class CSTi7108AuxMixer;
class CSTi7108ClkDivider;

class CSTi7108AuxOutput: public CSTmAuxTVOutput
{
public:
  CSTi7108AuxOutput(CSTi7108Device        *pDev,
                    CSTmSDVTG             *pVTG,
                    CSTmTVOutDENC         *pDENC,
                    CSTi7108AuxMixer      *pMixer,
                    CSTmFSynth            *pFSynth,
                    CSTmHDFormatterAWG    *pAWG,
                    CSTmHDFormatterOutput *pHDFOutput,
                    CSTi7108ClkDivider    *pClkDivider);

  bool ShowPlane(stm_plane_id_t planeID);

private:
  CSTi7108ClkDivider  *m_pClkDivider;

  void StartClocks(const stm_mode_line_t*);
  void SetAuxClockToHDFormatter(void);

  void EnableDACs(void);
  void DisableDACs(void);

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STi7108_SYSCFG_BANK3_BASE + reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STi7108_SYSCFG_BANK3_BASE +reg)>>2)); }

  CSTi7108AuxOutput(const CSTi7108AuxOutput&);
  CSTi7108AuxOutput& operator=(const CSTi7108AuxOutput&);
};


#endif //_STi7108AUXOUTPUT_H
