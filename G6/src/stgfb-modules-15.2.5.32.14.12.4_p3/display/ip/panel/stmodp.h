/***********************************************************************
 *
 * File: display/ip/panel/stmodp.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_ODP_H
#define _STM_ODP_H

typedef enum
{
    SYNC_UPDATE_REQ_NONE = 0,
    SYNC_UPDATE_REQ_ODP = 0x1,
    SYNC_UPDATE_REQ_END
} SyncUpdateRequestFlags_t;

typedef enum
{
    FORCE_UPDATE_REQ_NONE = 0,
    FORCE_UPDATE_REQ_ODP = 0x1,
    FORCE_UPDATE_REQ_END
} ForceUpdateRequestFlags_t;

class CSTmODP
{
public:
    CSTmODP(CDisplayDevice *pDev);

    virtual ~CSTmODP();

    virtual uint32_t SetPowerTiming(stm_display_panel_power_timing_config_t* pTiming) = 0;

    void SetSyncUpdate(SyncUpdateRequestFlags_t update);
    void ClearSyncUpdate(SyncUpdateRequestFlags_t update);
    virtual uint32_t SetForceUpdate(ForceUpdateRequestFlags_t update) = 0;
    virtual uint32_t HardwareUpdate(void) = 0;

protected:
    uint32_t *m_pDevRegs;

    uint32_t m_SyncUpdateFlag;

private:
    CSTmODP(const CSTmODP&);
    CSTmODP& operator=(const CSTmODP&);
};

#endif //_STM_ODP_H
