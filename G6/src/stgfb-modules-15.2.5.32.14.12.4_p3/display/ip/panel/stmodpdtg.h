/***********************************************************************
 *
 * File: display/ip/panel/stmodpdtg.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMODPDTG_H
#define _STMODPDTG_H

#include <display/ip/displaytiming/stmvtg.h>
#include <display/ip/panel/stmodp.h>

#define STMDTG_MAX_SYNC_OUTPUTS 1

typedef enum
{
    DTG_LOCK_MAIN,
    DTG_LOCK_PIP,
    DTG_LOCK_NONE
} DTG_DTGLockSource_t;


typedef enum
{
    DTG_FREERUN  = (1 << 0),
    DTG_FIXED_FRAME_LOCK = (1 << 1),
    DTG_DYNAMIC_FRAME_LOCK = (1 << 2),
    DTG_LOCK_LOAD = (1 << 3),
    DTG_DFL_VERTICAL_ONLY = (1 << 4)
} DTG_DTGLockScheme_t;

typedef struct DTGLockParams_s
{
    DTG_DTGLockSource_t InputSource;
    DTG_DTGLockScheme_t LockMethod;
    stm_scan_type_t     InputScanType;
    uint32_t            PanelHTotal;
    uint32_t            PanelVTotal;
    uint32_t            RefreshRateToLock;
    uint32_t            OutputRefreshRate;
    } DTGLockParams_t;

class CDisplayDevice;

/*
 * CSTmVTG is an encapsulation of the programming of the VTG found in
 * standard definition parts
 *
 * Now extended to support this basic DTG version found on ODP HW.
 */
class CSTmDTGOdp: public CSTmVTG
{
public:
    CSTmDTGOdp(CDisplayDevice* pDev, CSTmODP *pODP, CSTmFSynth *pFSynth = 0, bool bDoubleClocked = true, stm_vtg_sync_type_t refpol = STVTG_SYNC_TIMING_MODE);
    virtual ~CSTmDTGOdp(void);

    bool Start(const stm_display_mode_t*);
    bool Start(const stm_display_mode_t*, int externalid) { return false; }
    void Stop(void);

    void ResetCounters(void);
    void DisableSyncs(void);
    void RestoreSyncs(void);
    int SetInputLockScheme(stm_display_panel_timing_config_t lockparam);
protected:
    uint32_t         *m_pDevRegs;
    CSTmODP       *m_pODP;
    CSTmFSynth       *m_pFSynth;
    uint32_t          m_InterruptMask;

    uint32_t          m_FrameRate;
    bool              m_bInvertedField;
    DTGLockParams_t   m_LockParams;

    stm_display_mode_t    m_CurrentMode;
    stm_display_mode_t    m_PendingMode;

    virtual void EnableDTG(void) = 0;
    virtual void DisableDTG(void) = 0;
    virtual void EnableInterrupts(void) = 0;
    virtual void DisableInterrupts(void) = 0;
    virtual void DTGSetModeParams(void) = 0;
    virtual void ProgramDTGSyncsParams(void) = 0;
    virtual void ProgramDTGTimingsParams(void) = 0;
    virtual int  ProgramDTGInputLockingParams(void) = 0;

private:
    CSTmDTGOdp(const CSTmDTGOdp&);
    CSTmDTGOdp& operator=(const CSTmDTGOdp&);
};

#endif // _STMODPDTG_H
