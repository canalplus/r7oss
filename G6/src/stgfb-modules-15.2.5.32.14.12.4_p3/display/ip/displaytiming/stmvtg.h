/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2000-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#ifndef _STMVTG_H
#define _STMVTG_H

#include <display/ip/displaytiming/stmfsynth.h>

class CDisplayDevice;

/*! \enum  stm_vtg_sync_type_t
 *  \brief Types of sync output waveforms that can be programmed on a VTG
 */
typedef enum
{
  STVTG_SYNC_TIMING_MODE, /*!< Sync polarity follows the actual display mode        */
  STVTG_SYNC_INVERSE_TIMING_MODE, /*!< Sync polarity is inverse of display mode     */
  STVTG_SYNC_POSITIVE,    /*!< Force H and V syncs positive                         */
  STVTG_SYNC_NEGATIVE,    /*!< Force H and V syncs negative                         */
  STVTG_SYNC_TOP_NOT_BOT, /*!< Positive H sync but V sync programmed as Top not Bot */
  STVTG_SYNC_BOT_NOT_TOP  /*!< Positive H sync but V sync programmed as Bot not Top */
} stm_vtg_sync_type_t;


typedef enum stm_display_sync_id_e
{
    STM_SYNC_SEL_REF
  , STM_SYNC_SEL_1
  , STM_SYNC_SEL_2
  , STM_SYNC_SEL_3
  , STM_SYNC_SEL_4
  , STM_SYNC_SEL_5
  , STM_SYNC_SEL_6
} stm_display_sync_id_t;


typedef enum
{
    STVTG_SYNC_BNOTTOP_NOT_INVERTED
  , STVTG_SYNC_BNOTTOP_INVERTED
} stm_vtg_sync_Bnottop_type_t;


typedef struct stm_vtg_sync_params_s
{
  stm_vtg_sync_type_t   m_syncType;
  int                   m_HSyncOffsets;
  int                   m_VSyncHOffsets;
} stm_vtg_sync_params_t;

/*
 * Base class for all VTGs present in ST devices
 */
class CSTmVTG
{
public:
  CSTmVTG(CDisplayDevice* pDev, CSTmFSynth *pFSynth, bool bDoubleClocked, unsigned nbr_sync_outputs);
  virtual ~CSTmVTG(void);

  virtual bool Start(const stm_display_mode_t*);
  virtual void Stop(void);

  bool RegisterSlavedVTG(CSTmVTG *,
                         stm_vtg_sync_type_t,
                         int ExternalSyncSourceID,
                         int VSyncHDelay,
                         int HSyncDelay);

  void UnRegisterSlavedVTG(void);

  int GetNumSyncOutputs(void) const { return m_max_sync_outputs; };

  virtual bool RequestModeUpdate(const stm_display_mode_t*);

  /*
   * Reset all of the counter generation and immediately start a new top field
   */
  virtual void ResetCounters(void) = 0;

  /*
   * Temporary disabling of the sync pulse generation
   */
  virtual void DisableSyncs(void) = 0;
  virtual void RestoreSyncs(void) = 0;

  virtual uint32_t GetInterruptStatus(void) = 0;

  stm_display_mode_t GetCurrentMode(void) const { return m_CurrentMode; }
  bool IsStarted(void) { return m_CurrentMode.mode_id != STM_TIMING_MODE_RESERVED; }
  bool IsPending(void) { return m_PendingMode.mode_id != STM_TIMING_MODE_RESERVED; }

  void SetHSyncOffset(int outputid, int offset);
  void SetVSyncHOffset(int outputid, int offset);
  void SetSlaveHSyncOffset(int outputid, int offset);
  void SetSlaveVSyncHOffset(int outputid, int offset);

  void SetSyncType(int outputid, stm_vtg_sync_type_t type);
  void SetSlaveSyncType(int outputid, stm_vtg_sync_type_t type);
  void SetSlavedVTGInputSyncType(stm_vtg_sync_type_t type);

  virtual bool SetBnottopType(stm_display_sync_id_t sync, stm_vtg_sync_Bnottop_type_t type) { return false; }
  bool SetSlaveBnottopType(stm_display_sync_id_t sync, stm_vtg_sync_Bnottop_type_t type);

  void SetIsSlave(bool isSlave) { m_bIsSlaveVTG = isSlave; }

  void SetClockReference(stm_clock_ref_frequency_t refClock, int error_ppm)
  {
    m_pFSynth->SetClockReference(refClock,error_ppm);
  }
  bool SetAdjustment(int ppm) { return(m_pFSynth->SetAdjustment(ppm)); }
  int  GetAdjustment(void) { return(m_pFSynth->GetAdjustment()); }

  /*
   * The following is to be used by VTG subclasses to implement updates of
   * slaved VTGs. It is not for general use.
   */
  virtual void ProgramVTGTimings(const stm_display_mode_t*);
  virtual void ProgramOutputWindow(const stm_display_mode_t* pModeLine);

  bool SetOutputWindowRectangle(const uint16_t XStart, const uint16_t YStart, const uint32_t Width, const uint32_t Height);

protected:
  void                  * m_lock;

  CSTmFSynth           *m_pFSynth;

  CSTmVTG              *m_pSlavedVTG;
  bool                  m_bIsSlaveVTG;
  stm_vtg_sync_type_t   m_SlavedVTGInputSyncType;
  int                   m_SlavedVTGVSyncHDelay;
  int                   m_SlavedVTGHSyncDelay;
  int                   m_SlavedVTGExternalSyncSource;

  bool                  m_bDoubleClocked;
  bool                  m_bDisabled;
  bool                  m_bSuppressMissedInterruptMessage;

  int                   m_max_sync_outputs;

  stm_vtg_sync_params_t *m_syncParams;
  uint32_t              m_pendingSync;
  /*
   * Output window offsets: Used by MISR module
   */
  bool                  m_UpdateOutputRect;
  stm_rect_t            m_OutputWindowRect;

  bool                  m_bUpdateOnlyClockFrequency;

  stm_display_mode_t    m_CurrentMode;
  stm_display_mode_t    m_PendingMode;

  virtual bool Start(const stm_display_mode_t*, int ExternalSyncSourceID) { return false; }

  void GetHSyncPositions(int                        outputid,
                   const stm_display_mode_timing_t &mode_timing,
                         uint32_t                  *HStart,
                         uint32_t                  *HStop);

  /*
   * Allow VTG master to directly access the hardware interrupt status of a
   * slave for designated subclass implementations.
   */
  friend class CSTmC8VTG_V8_4;
  virtual uint32_t ReadHWInterruptStatus(void);

private:
  CSTmVTG(const CSTmVTG&);
  CSTmVTG& operator=(const CSTmVTG&);
};

#endif // _STMVTG_H
