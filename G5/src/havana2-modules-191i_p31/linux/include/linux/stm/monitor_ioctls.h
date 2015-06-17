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

Source file name : monitor_ioctls.h - external interface definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Jul-08   Created                                         Julian

************************************************************************/

#ifndef H_MONITOR_IOCTLS
#define H_MONITOR_IOCTLS

#include "monitor_types.h"

#define MONITOR_TIME_UNKNOWN                    0xfedcba9876543210ull

#define MONITOR_EVENT_RECORD_SIZE               128

typedef unsigned int                            monitor_subsystem_mask_t;

typedef enum monitor_status_code_e
{
    MONITOR_STATUS_IDLE,
    MONITOR_STATUS_RUNNING
} monitor_status_code_t;

struct monitor_status_s
{
    monitor_status_code_t       status_code;
    monitor_subsystem_mask_t    subsystem_mask;
    unsigned int                event_count;
    unsigned int                events_lost;
};

#define MONITOR_EVENT_INDEX_MASK                0x000000ff

typedef char                    monitor_event_t[MONITOR_EVENT_RECORD_SIZE];

struct monitor_event_request_s
{
    monitor_event_code_t        event_code;
    unsigned int                parameters[MONITOR_PARAMETER_COUNT];
    unsigned int                count;
    unsigned int                reset;
};


#define MONITOR_ENABLE          _IOW('o', 10, monitor_subsystem_mask_t)
#define MONITOR_DISABLE         _IOW('o', 11, monitor_subsystem_mask_t)
#define MONITOR_GET_STATUS      _IOR('o', 12, struct monitor_status_s)
#define MONITOR_REQUEST_EVENT   _IOW('o', 12, struct monitor_event_request_s)

#endif
