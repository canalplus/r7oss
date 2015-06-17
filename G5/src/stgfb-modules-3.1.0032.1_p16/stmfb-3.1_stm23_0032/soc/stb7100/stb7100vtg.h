/***********************************************************************
 *
 * File: soc/stb7100/stb7100vtg.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STB7100VTG_H
#define _STB7100VTG_H

#include <stmdisplay.h>

#include <Generic/IOS.h>

#include <STMCommon/stmvtg.h>

/*
 * On 7100/7109 the sync outputs have hardwired usage.
 */
enum {
  VTG_INT_SYNC_ID = 1,
  VTG_DVO_SYNC_ID,
  VTG_AWG_SYNC_ID
};

class CDisplayDevice;
class CSTmFSynth;

class CSTb7100VTG: public CSTmVTG
{
public:
  CSTb7100VTG(CDisplayDevice* pDev, ULONG ulRegOffset, CSTmFSynth *pFSynth, ULONG its, ULONG itm, ULONG sta);
  virtual ~CSTb7100VTG(void);

  bool Start(const stm_mode_line_t*, stm_vtg_sync_type_t);
  void Stop(void);

  void ResetCounters(void);
  void DisableSyncs(void);
  void RestoreSyncs(void);
  stm_field_t GetInterruptStatus(void);

  bool ArmVsyncTimer(ULONG ulPixels, void (*fpCallback)(void *, int), void *p, int i);
  void WaitForVsync(void);

protected:
  ULONG m_hdo;
  ULONG m_hds;
  ULONG m_vdo;
  ULONG m_vds;

  ULONG m_vtg_its;
  ULONG m_vtg_itm;
  ULONG m_vtg_sta;

  // reading from the interrupt status register automatically clears it so
  // we keep a shadow copy in software.
  ULONG m_ulInterruptStatus;

  void GetVSyncPositions(int outputid, const stm_timing_params_t &TimingParams, ULONG *ulVDO, ULONG *ulVDS);

private:
  CSTb7100VTG(const CSTb7100VTG&);
  CSTb7100VTG& operator=(const CSTb7100VTG&);

  void (*m_fpCallback)(void *, int);
  void *m_pCallbackPointer;
  int m_iCallbackOpcode;

};


class CSTb7100VTG1: public CSTb7100VTG
{
public:
  CSTb7100VTG1(CDisplayDevice* pDev, ULONG ulRegOffset, CSTmFSynth *pFSynth);
  virtual ~CSTb7100VTG1(void);

  bool Start(const stm_mode_line_t*);
  bool Start(const stm_mode_line_t*,int externalid);
  void Stop(void);

  void SetHDMIHSyncDelay(ULONG delay);

private:
  CSTb7100VTG1(const CSTb7100VTG1&);
  CSTb7100VTG1& operator=(const CSTb7100VTG1&);
};


class CSTb7100VTG2: public CSTb7100VTG
{
public:
  CSTb7100VTG2(CDisplayDevice* pDev, ULONG ulRegOffset, CSTmFSynth *pFSynth, CSTb7100VTG1 *pVTG1);
  virtual ~CSTb7100VTG2(void);

  bool Start(const stm_mode_line_t*);
  bool Start(const stm_mode_line_t*,int externalid);
  void Stop(void);

private:
  CSTb7100VTG1 *m_pVTG1; // Used to determine mode params when VTG2 is to be slaved to VTG1

  CSTb7100VTG2(const CSTb7100VTG2&);
  CSTb7100VTG2& operator=(const CSTb7100VTG2&);
};

#endif // _STB7100VTG_H
