/***********************************************************************
 *
 * File: display/ip/displaytiming/stmc8vtg_v8_4.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMGENVTG_H
#define _STMGENVTG_H

#include "stmc8vtg.h"

class CDisplayDevice;

/*
 * CSTmC8VTG_V8_4 is an encapsulation of the programming of the C8VTG IP
 * version 8.4 as found in STiH416/STiH407
 */
class CSTmC8VTG_V8_4: public CSTmC8VTG
{
public:
  CSTmC8VTG_V8_4(CDisplayDevice     *pDev,
                 uint32_t            uRegOffset,
                 uint32_t            uNumSyncOutputs,
                 CSTmFSynth         *pFSynth,
                 bool                bDoubleClocked,
                 stm_vtg_sync_type_t refpol,
                 bool                bDisableSyncsOnStop,
                 bool                bUseSlaveInterrupts);

  virtual ~CSTmC8VTG_V8_4(void);

  void ResetCounters(void);
  void DisableSyncs(void);

  bool SetBnottopType(stm_display_sync_id_t sync, stm_vtg_sync_Bnottop_type_t type);

  uint32_t GetInterruptStatus(void);

protected:
  uint32_t m_InterruptMask;

  void     ProgramVTGTimings(const stm_display_mode_t*);

  void     EnableInterrupts(void);
  void     DisableInterrupts(void);
  uint32_t ReadHWInterruptStatus(void);

  void     EnableSyncs(void);

private:
  uint32_t     m_dbg_count;
  uint32_t     m_mismatched_sync_dbg_count;
  stm_time64_t m_lastvsync;

  void ProgramOutputWindow(const stm_display_mode_t* pModeLine);

  CSTmC8VTG_V8_4(const CSTmC8VTG_V8_4&);
  CSTmC8VTG_V8_4& operator=(const CSTmC8VTG_V8_4&);
};

#endif // _STMC8VTG_V8_4_H
