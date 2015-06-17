/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2011-2014 STMicroelectronics - All Rights Reserved
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

#ifndef _SOURCEINTERFACE_H
#define _SOURCEINTERFACE_H

#include "stm_display_source_queue.h"
#include "DisplayPlane.h"

#define CINTERFACE_MAX_PLANES_PER_SOURCE   10
#define VALID_SRC_INTERFACE_HANDLE         0x01270127

class CDisplaySource;

/*
 * SourceInterface status used to start/stop the HW (ex: IT) when this
 * interface starts to be used.
 */
enum SourceInterfaceStatus
{
  STM_SRC_INTERFACE_NOT_USED,
  STM_SRC_INTERFACE_IN_USE,
  STM_SRC_INTERFACE_STABLE,
  STM_SRC_INTERFACE_UNSTABLE
};

class CSourceInterface
{
public:
    // Construction/Destruction
    CSourceInterface ( uint32_t interfaceID, CDisplaySource * pSource );

    virtual ~CSourceInterface(void);

    // Source Interface Identification
    uint32_t                            GetID(void)                 const { return m_interfaceID; }
    int32_t                             GetHwId(void)               const { return m_interfaceHwId; }
    CDisplaySource                    * GetParentSource(void)       const { return m_pDisplaySource; }

    virtual bool                        Create(void)          = 0;

    virtual stm_display_source_interfaces_t GetInterfaceType(void)                          const = 0;

    // Indicate if a src with such interface can be connected to this plane
    virtual bool                        IsConnectionPossible(CDisplayPlane *pPlane)     const = 0;

    virtual bool                        Flush(bool               bFlushCurrentNode,
                                              const void * const user,
                                              bool               bCheckUser) { return true; }

    // By default a source interface doesn't have a TimingID but this can be overwritten by a specific subclass
    virtual uint32_t                    GetTimingID(void) const = 0;

    virtual void                        UpdateOutputInfo(CDisplayPlane *pPlane) = 0;


    virtual stm_display_timing_events_t GetInterruptStatus(void) { return STM_TIMING_EVENT_NONE; }
    virtual void                        SourceVSyncUpdateHW(bool isInterlaced,
                                                            bool isTopVSync,
                                                            const stm_time64_t &vsyncTime) { return ; }
    virtual void                        OutputVSyncThreadedIrqUpdateHW(bool isInterlaced,
                                                                       bool isTopVSync,
                                                                       const stm_time64_t &vsyncTime) { return ; }

    // Set status of the hardware which is controlled by this interface
    virtual bool                        SetInterfaceStatus(SourceInterfaceStatus status) { return true; }

    virtual uint32_t                    GetControl(stm_display_source_ctrl_t ctrl, uint32_t *ctrl_val) const = 0;
    virtual uint32_t                    SetControl(stm_display_source_ctrl_t ctrl, uint32_t ctrl_val) = 0;

    /*
     * Power Managment stuff
     */
    virtual void                        Freeze(void);
    virtual void                        Suspend(void);
    virtual void                        Resume(void);

protected:
    uint32_t                            m_interfaceID;         // SW Source Interface identifier
    int32_t                             m_interfaceHwId;       // Hardware Id of source interface IP
    CDisplaySource                    * m_pDisplaySource;      // Parent Source access to the interface.

    // Power Managment state
    bool                                m_bIsSuspended; // Has the source been powered off
    bool                                m_bIsFrozen;
};

#endif /* _SOURCEINTERFACE_H */
