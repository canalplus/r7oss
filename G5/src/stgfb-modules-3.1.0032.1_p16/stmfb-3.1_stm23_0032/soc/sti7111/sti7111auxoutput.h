/***********************************************************************
 *
 * File: soc/sti7111/sti7111auxoutput.h
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7111AUXOUTPUT_H
#define _STi7111AUXOUTPUT_H

#include <HDTVOutFormatter/stmauxtvoutput.h>

class CSTi7111Device;

class CSTi7111AuxOutput: public CSTmAuxTVOutput
{
public:
  CSTi7111AuxOutput(CSTi7111Device        *pDev,
                    CSTmSDVTG             *pVTG,
                    CSTmTVOutDENC         *pDENC,
                    CDisplayMixer         *pMixer,
                    CSTmFSynth            *pFSynth,
                    CSTmHDFormatterAWG    *pAWG,
                    CSTmHDFormatterOutput *pHDFOutput,
                    stm_plane_id_t         sharedVideoPlaneID,
                    stm_plane_id_t         sharedGDPPlaneID);

  bool ShowPlane(stm_plane_id_t planeID);

private:
  stm_plane_id_t m_sharedVideoPlaneID;
  stm_plane_id_t m_sharedGDPPlaneID;

  void StartClocks(const stm_mode_line_t*);
  void SetAuxClockToHDFormatter(void);

  void EnableDACs(void);
  void DisableDACs(void);

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STi7111_SYSCFG_BASE + reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STi7111_SYSCFG_BASE +reg)>>2)); }

  void WriteClkReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STi7111_CLKGEN_BASE + reg)>>2), val); }
  ULONG ReadClkReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STi7111_CLKGEN_BASE +reg)>>2)); }

  CSTi7111AuxOutput(const CSTi7111AuxOutput&);
  CSTi7111AuxOutput& operator=(const CSTi7111AuxOutput&);
};


#endif //_STi7111AUXOUTPUT_H
