/***********************************************************************
 *
 * File: STMCommon/stmvtg.h
 * Copyright (c) 2000, 2004, 2005, 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMVTG_H
#define _STMVTG_H

class CDisplayDevice;
class CSTmFSynth;

#define STMVTG_MAX_SYNC_OUTPUTS 3

/*
 * Base class for all VTGs present in ST devices
 */
class CSTmVTG
{
public:
  CSTmVTG(CDisplayDevice* pDev, ULONG ulRegOffset, CSTmFSynth *pFSynth, bool bDoubleClocked = false);
  virtual ~CSTmVTG(void);

  virtual bool Start(const stm_mode_line_t*) = 0;
  virtual bool Start(const stm_mode_line_t*, int externalid) = 0;
  virtual void Stop(void) = 0;

  virtual bool RequestModeUpdate(const stm_mode_line_t*);

  /*
   * Reset all of the counter generation and immediately start a new top field
   */
  virtual void ResetCounters(void) = 0;

  /*
   * Temporary disabling of the sync pulse generation
   */
  virtual void DisableSyncs(void) = 0;
  virtual void RestoreSyncs(void) = 0;

  virtual stm_field_t GetInterruptStatus(void) = 0;

  const stm_mode_line_t* GetCurrentMode(void) const { return m_pCurrentMode; }

  /*
   * Set an offset from href for the hsync signals to be generated, this can
   * be negative. Note that outputid 0 is deliberately ignored.
   */
  void SetHSyncOffset(int outputid, int offset)
  {
    if(outputid>0 && outputid<=STMVTG_MAX_SYNC_OUTPUTS)
      m_HSyncOffsets[outputid] = offset;
  }

  /*
   * Set an offset in pixels from vref for the vsync signals to be generated,
   * this can be negative. Note that outputid 0 is deliberately ignored.
   */
  void SetVSyncHOffset(int outputid, int offset)
  {
    if(outputid>0 && outputid<=STMVTG_MAX_SYNC_OUTPUTS)
      m_VSyncHOffsets[outputid] = offset;
  }

  /*
   * Set the type of a particular sync output, which can include the
   * polarity of the ref signals.
   */
  void SetSyncType(int outputid, stm_vtg_sync_type_t type)
  {
    if(outputid>=0 && outputid<=STMVTG_MAX_SYNC_OUTPUTS)
      m_syncType[outputid] = type;
  }

protected:
  ULONG*  m_pDevRegs;
  ULONG   m_ulVTGOffset;

  CSTmFSynth *m_pFSynth;

  ULONG   m_lock;

  bool    m_bDoubleClocked;
  bool    m_bDisabled;
  bool    m_bSuppressMissedInterruptMessage;

  int     m_HSyncOffsets[STMVTG_MAX_SYNC_OUTPUTS+1];
  int     m_VSyncHOffsets[STMVTG_MAX_SYNC_OUTPUTS+1];

  stm_vtg_sync_type_t m_syncType[STMVTG_MAX_SYNC_OUTPUTS+1];

  volatile bool m_bUpdateClockFrequency;

  const stm_mode_line_t* volatile m_pCurrentMode;
  const stm_mode_line_t* volatile m_pPendingMode;

  void GetHSyncPositions(int                        outputid,
                         const stm_timing_params_t &TimingParams,
                         ULONG                     *ulStart,
                         ULONG                     *ulStop);

  void WriteVTGReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulVTGOffset+reg)>>2), val); }
  ULONG ReadVTGReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulVTGOffset+reg)>>2)); }

private:
  CSTmVTG(const CSTmVTG&);
  CSTmVTG& operator=(const CSTmVTG&);
};

#endif // _STMVTG_H
