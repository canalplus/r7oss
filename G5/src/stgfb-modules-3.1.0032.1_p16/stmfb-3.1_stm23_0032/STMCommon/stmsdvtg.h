/***********************************************************************
 *
 * File: STMCommon/stmsdvtg.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMSDVTG_H
#define _STMSDVTG_H

#include "stmvtg.h"

class CDisplayDevice;

/*
 * CSTmSDVTG is an encapsulation of the programming of the VTG found in
 * standard definition parts such as STm8000, STi5528, STi5301/STi5100
 *
 * Now extended to support this basic VTG version found on STi7200.
 */
class CSTmSDVTG: public CSTmVTG
{
public:
  CSTmSDVTG(CDisplayDevice* pDev, ULONG ulRegOffset, CSTmFSynth *pFSynth = 0, bool bDoubleClocked = true, stm_vtg_sync_type_t refpol = STVTG_SYNC_TIMING_MODE);
  virtual ~CSTmSDVTG(void);

  bool Start(const stm_mode_line_t*);
  bool Start(const stm_mode_line_t*, int externalid);
  void Stop(void);

  void ResetCounters(void);
  void DisableSyncs(void);
  void RestoreSyncs(void);

  stm_field_t GetInterruptStatus(void);

protected:
  bool m_bSyncEnabled[STMVTG_MAX_SYNC_OUTPUTS];

  void GetHSyncPositions(int                        outputid,
                         const stm_timing_params_t &TimingParams,
                         ULONG                     &ulSync);

  void GetInterlacedVSyncPositions(int                        outputid,
                                   const stm_timing_params_t &TimingParams,
                                   ULONG                      clocksperline,
                                   ULONG                     &ulVSyncLineTop,
                                   ULONG                     &ulVSyncLineBot,
                                   ULONG                     &ulVSyncPixOffTop,
                                   ULONG                     &ulVSyncPixOffBot);

  void GetProgressiveVSyncPositions(int                        outputid,
                                    const stm_timing_params_t &TimingParams,
                                    ULONG                      clocksperline,
                                    ULONG                     &ulVSyncLineTop,
                                    ULONG                     &ulVSyncPixOffTop);

  void ProgramVTGTimings(const stm_mode_line_t*);

private:
  CSTmSDVTG(const CSTmSDVTG&);
  CSTmSDVTG& operator=(const CSTmSDVTG&);
};

#endif // _STMSDVTG_H
