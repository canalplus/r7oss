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

#ifndef _QUEUEBUFFERINTERFACE_H
#define _QUEUEBUFFERINTERFACE_H

#include "DisplayDevice.h"
#include "DisplayPlane.h"
#include "DisplaySource.h"
#include "SourceInterface.h"


class CQueueBufferInterface: public CSourceInterface
{
public:
    // Construction/Destruction
    CQueueBufferInterface ( uint32_t interfaceID, CDisplaySource  *pSource );

    virtual ~CQueueBufferInterface(void);
    bool     Create(void);

    virtual uint32_t                    GetTimingID(void) const = 0;

    virtual void                        UpdateOutputInfo(CDisplayPlane *pPlane) = 0;

    // Interface Queue Buffer Identification
    stm_display_source_interfaces_t     GetInterfaceType(void)      const { return STM_SOURCE_QUEUE_IFACE; }

    virtual bool                        LockUse(void *user, bool force) = 0;
    virtual void                        Unlock (void *user) = 0;

    virtual bool                        QueueBuffer(const stm_display_buffer_t * const ,
                                                    const void                 * const ) = 0;
    virtual bool                        Flush(bool               bFlushCurrentNode,
                                              const void * const user,
                                              bool               bCheckUser) = 0;

    virtual int                         GetBufferFormats(const stm_pixel_format_t **) const = 0;

    virtual uint32_t                    GetControl(stm_display_source_ctrl_t ctrl, uint32_t *ctrl_val) const;
    virtual uint32_t                    SetControl(stm_display_source_ctrl_t ctrl, uint32_t ctrl_val);

    virtual bool                        RegisterStatistics(void);

    virtual TuningResults               SetTuning( uint16_t service, void *inputList, uint32_t inputListSize, void *outputList, uint32_t outputListSize);

protected:
    const CDisplayDevice               *m_pDisplayDevice;        // Parent Device access .

    void                              * m_lock;         // Queue lock
    struct SourceStatistics             m_Statistics;   // Source statistics
};

#endif /* _QUEUEBUFFERINTERFACE_H */
