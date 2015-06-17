/************************************************************************
Copyright (C) 2008 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : monitor_device.h - monitor device definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Jul-08   Created                                         Julian

************************************************************************/

#ifndef H_MONITOR_DEVICE
#define H_MONITOR_DEVICE

#include <linux/spinlock.h>
#include <linux/smp_lock.h>

#include "monitor_ioctls.h"
#include "monitor_mme.h"

#define MAX_MONITOR_EVENT               128
#define MAX_MONITOR_EVENT_CODE          (MONITOR_EVENT_INFORMATION & MONITOR_EVENT_INDEX_MASK)

struct EventRecord_s
{
    unsigned int                        RecordLength;                   /*! Length of record */
    monitor_event_t                     EventString;                    /*! External event record */
};

struct EventQueue_s
{
    struct EventRecord_s                Event[MAX_MONITOR_EVENT];       /*! Event record */
    unsigned int                        Write;                          /*! Pointer to next event location to write by capture */
    unsigned int                        Read;                           /*! Pointer to next event location to read by user */
    unsigned int                        LostCount;                      /*! Events lost since last read */
    wait_queue_head_t                   EventReceived;                  /*! Queue to wake up readers */
    wait_queue_head_t                   BufferReleased;                 /*! Queue to wake up writers */
    spinlock_t                          Lock;                           /*! Protection for access to Read and Write pointers */
};

struct EventValue_s
{
    unsigned int                        Parameters[MONITOR_PARAMETER_COUNT];
    unsigned int                        Count;
};


struct DeviceContext_s
{
    struct class_device*                ClassDevice;
    struct cdev                         CDev;

    unsigned int                        Id;
    unsigned int                        OpenCount;
    struct monitor_status_s             Status;
    struct EventQueue_s                 EventQueue;
    struct EventValue_s                 StoredEventValues[MAX_MONITOR_EVENT_CODE+1];

    struct MMEContext_s                 MMEContext[MONITOR_MAX_MME_DEVICES];

    struct ModuleContext_s*             ModuleContext;

    unsigned int                       *Timer;
    unsigned int                        TimerPhysical;
};


struct file_operations*                 MonitorInit            (struct DeviceContext_s*         Context);

void                                    MonitorRecordEvent     (struct DeviceContext_s*         Context,
                                                                unsigned int                    SourceId,
                                                                monitor_event_code_t            EventCode,
                                                                unsigned long long              TimeCode,
                                                                unsigned int                    Parameters[MONITOR_PARAMETER_COUNT],
                                                                const char*                     Description);
#endif
