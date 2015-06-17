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

Source file name : monitor_mme.h - monitor mme definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
08_Aug-08   Created                                         Julian

************************************************************************/

#ifndef H_MONITOR_MME
#define H_MONITOR_MME

#include <linux/sched.h>
#include <asm/semaphore.h>

#include "mme.h"
#include "EVENT_Log_TransformerTypes.h"

#define MONITOR_MAX_MME_DEVICES         4

struct MMEContext_s
{
    unsigned int                        Id;
    char                                TransformerName[MME_MAX_TRANSFORMER_NAME];

    unsigned int                        ClockAddress;
    unsigned long long                  ClockMaxValue;
    unsigned long long                  TicksPerSecond;

    struct task_struct*                 MonitorMMEThread;
    struct semaphore                    EventReceived;
    unsigned int                        Monitoring;
    struct semaphore                    ThreadTerminated;

    MME_TransformerHandle_t             MMEHandle;
    EVENT_LOG_CommandStatus_t           MMECommandStatus;

    struct DeviceContext_s*             DeviceContext;

    unsigned int                        TransformerInitialized;
};


int MonitorMMEInit                     (struct DeviceContext_s*         DeviceContext,
                                        struct MMEContext_s*            Context,
                                        unsigned int                    Id);
int MonitorMMETerminate                (struct MMEContext_s*            Context);


#endif
