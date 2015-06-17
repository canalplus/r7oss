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

Source file name : monitor_mme.c
Author :           Julian

Implementation of linux monitor device

Date        Modification                                    Name
----        ------------                                    --------
07-Aug-08   Created                                         Julian

************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/bpa2.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>
#include <asm/timer.h>
#include <asm/clock.h>
#include <linux/delay.h>

#include "mme.h"
#include "EVENT_Log_TransformerTypes.h"
#include "monitor_inline.h"
#include "monitor_module.h"
#include "monitor_device.h"

#define MONITOR_MME_THREAD_NAME         "MonitorMMEThread"

static int      MonitorMMEThread               (void*                   Param);
static int      TransformerInitialize          (struct MMEContext_s*    Context);
static int      TransformerTerminate           (struct MMEContext_s*    Context);
static void     TransformerCallback            (MME_Event_t             Event,
                                                MME_Command_t*          CallbackData,
                                                void*                   UserData);

/*{{{  MonitorMMEInit*/
int MonitorMMEInit                     (struct DeviceContext_s*         DeviceContext,
                                        struct MMEContext_s*            Context,
                                        unsigned int                    Id)
{
    struct sched_param          Param;
    struct task_struct*         Taskp;
    int                         Status;
    struct clk*                 Tmu1Clock       = clk_get(NULL, "tmu1_clk");
    unsigned long long          TimeStamp;
    unsigned int                TimeValue;

    if (Tmu1Clock == NULL)
    {
        MONITOR_ERROR("Unable to access working clock\n");
        return -ENODEV;
    }

    Context->DeviceContext              = DeviceContext;

    Context->ClockAddress               = DeviceContext->TimerPhysical;
    Context->ClockMaxValue              = 0xffffffff;

    TimeValue                           = *DeviceContext->Timer;
    TimeStamp                           = ktime_to_us (ktime_get ());

    msleep(10); // Sleep for some random amount of time

    TimeValue                           = *DeviceContext->Timer - TimeValue;
    TimeStamp                           = ktime_to_us (ktime_get ()) - TimeStamp;

    // If my maths is correct you don't need to cope with rap case....
    Context->TicksPerSecond             = ((((unsigned long long)TimeValue) * 1000000ull) / (unsigned long long)TimeStamp);

    MONITOR_TRACE("TicksPerSecond %lld\n",Context->TicksPerSecond);

    Context->Id                         = Id;
    Context->TransformerInitialized     = 0;

    sprintf (Context->TransformerName, "%s%d", EVENT_LOG_MME_TRANSFORMER_NAME, Context->Id-1);
    Status                              = TransformerInitialize (Context);
    if (Status != 0)
        return Status;

    sema_init (&(Context->ThreadTerminated), 0);
    sema_init (&(Context->EventReceived), 0);
    Context->Monitoring                 = true;
    Context->MonitorMMEThread           = NULL;

    Taskp                               = kthread_run (MonitorMMEThread, Context, MONITOR_MME_THREAD_NAME);
    if (!Taskp)
    {
        Context->Monitoring             = false;
        MONITOR_ERROR("Unable to create transformer monitor thread\n");
        return -ENODEV;
    }


    Param.sched_priority                = 40;
    sched_setscheduler (Taskp, SCHED_RR, &Param);

    return 0;
}
/*}}}  */
/*{{{  MonitorMMETerminate*/
int MonitorMMETerminate                (struct MMEContext_s*    Context)
{
    if (Context->TransformerInitialized)
        TransformerTerminate (Context);

    if (Context->MonitorMMEThread != NULL)
    {
        Context->Monitoring     = false;

        down_interruptible (&(Context->ThreadTerminated));
    }

    return 0;
}
/*}}}  */

/*{{{  VerifyCapabilities*/
static int VerifyCapabilities       (struct MMEContext_s*    Context)
{
    EVENT_LOG_TransformerCapability_t   EventLogCapability      = {0};
    MME_TransformerCapability_t         MMECapability           = {0};
    MME_ERROR                           MMEStatus;

    MMECapability.StructSize            = sizeof (MME_TransformerCapability_t);
    MMECapability.TransformerInfoSize   = sizeof (EVENT_LOG_TransformerCapability_t);
    MMECapability.TransformerInfo_p     = &EventLogCapability;

    MMEStatus                           = MME_GetTransformerCapability (Context->TransformerName, &MMECapability );
    if (MMEStatus == MME_UNKNOWN_TRANSFORMER)
    {
        MONITOR_DEBUG ("%s: Transformer not found.  This event source will not be monitored.\n", Context->TransformerName);
        return -EFAULT;
    }
    if (MMEStatus != MME_SUCCESS)
    {
        MONITOR_ERROR ("%s: Failed to retrieve transformer capabilities - Error 0x%08x.\n", Context->TransformerName, MMEStatus);
        return -EFAULT;
    }

    MONITOR_TRACE ("Found %s transformer (version %x)\n", Context->TransformerName, MMECapability.Version);

    return 0;
}
/*}}}  */
/*{{{  TransformerInitialize*/
static int TransformerInitialize       (struct MMEContext_s*    Context)
{
    EVENT_LOG_InitTransformerParam_t    EventLogInitParams      = {0};
    MME_TransformerInitParams_t         MMEInitParams           = {0};
    MME_ERROR                           MMEStatus;
    int                                 Status;

    Status                                      = VerifyCapabilities (Context);
    if (Status != 0)
        return Status;

    EventLogInitParams.StructSize               = sizeof (EVENT_LOG_InitTransformerParam_t);
    EventLogInitParams.TimeCodeMemoryAddress    = Context->ClockAddress;

    MMEInitParams.Priority                      = MME_PRIORITY_LOWEST;
    MMEInitParams.StructSize                    = sizeof (MME_TransformerInitParams_t);
    MMEInitParams.Callback                      = &TransformerCallback;
    MMEInitParams.CallbackUserData              = Context;

    MMEInitParams.TransformerInitParamsSize     = sizeof (EVENT_LOG_InitTransformerParam_t);
    MMEInitParams.TransformerInitParams_p       = (MME_GenericParams_t)(&EventLogInitParams);


    MMEStatus   = MME_InitTransformer (Context->TransformerName, &MMEInitParams, &Context->MMEHandle);
    if (MMEStatus != MME_SUCCESS)
    {
        MONITOR_ERROR ("%s: Failed to initialize transformer - Error 0x%08x.\n", Context->TransformerName, MMEStatus);
        return -EFAULT;
    }

    Context->TransformerInitialized             = true;

    return 0;

}
/*}}}  */
/*{{{  TransformerTerminate*/
static int TransformerTerminate        (struct MMEContext_s*    Context)
{
    MME_ERROR           MMEStatus;

    if (Context->TransformerInitialized)
    {
        Context->TransformerInitialized         = false;

        MMEStatus                               = MME_TermTransformer (Context->MMEHandle);
        if (MMEStatus != MME_SUCCESS)
        {
            MONITOR_ERROR ("%s: Failed to terminate mme transformer - Error 0x%08x.\n", Context->TransformerName, MMEStatus);
            return -EFAULT;
        }
    }

    return 0;
}
/*}}}  */
/*{{{  TransformerGetLogEvent*/
static int TransformerGetLogEvent              (struct MMEContext_s*    Context)
{
    MME_Command_t                       MMECommand;
    MME_ERROR                           MMEStatus;
    /*EVENT_LOG_TransformParam_t          TransformParams;*/

    memset (&MMECommand, 0x00, sizeof(MME_Command_t));

    MMECommand.CmdStatus.AdditionalInfoSize     = sizeof(EVENT_LOG_CommandStatus_t);
    MMECommand.CmdStatus.AdditionalInfo_p       = (MME_GenericParams_t)(&(Context->MMECommandStatus));
    MMECommand.StructSize                       = sizeof (MME_Command_t);
    MMECommand.CmdCode                          = MME_TRANSFORM;
    MMECommand.CmdEnd                           = MME_COMMAND_END_RETURN_NOTIFY;
    MMECommand.DueTime                          = (MME_Time_t)0;
    MMECommand.NumberInputBuffers               = 0;
    MMECommand.NumberOutputBuffers              = 0;

    MMECommand.DataBuffers_p                    = NULL;

    MMECommand.ParamSize                        = 0;/*sizeof(EVENT_LOG_TransformParam_t);*/
    MMECommand.Param_p                          = NULL; /*(MME_GenericParams_t)&TransformParams;*/

    MMEStatus                                   = MME_SendCommand (Context->MMEHandle, &MMECommand);
    if (MMEStatus != MME_SUCCESS)
    {
        MONITOR_ERROR ("%s: Failed to send command - Error 0x%08x.\n", Context->TransformerName, MMEStatus);
        return -EFAULT;
    }
    return 0;
}
/*}}}  */
/*{{{  TransformerCallback*/
static void     TransformerCallback            (MME_Event_t             Event,
                                                MME_Command_t*          CallbackData,
                                                void*                   UserData)
{
    struct MMEContext_s*        Context = (struct MMEContext_s*)UserData;

    if (CallbackData == NULL)
    {
        MONITOR_ERROR ("%s: ####################### No CallbackData #######################\n", Context->TransformerName);
        return;
    }

    switch (CallbackData->CmdCode)
    {
        case MME_TRANSFORM:
        {

            up (&(Context->EventReceived));
            break;
        }

        default:
            break;
    }

}
/*}}}  */

/*{{{  MonitorMMEThread*/
static int MonitorMMEThread (void* Param)
{
    struct MMEContext_s*        Context = (struct MMEContext_s*)Param;
    MME_ERROR                   MMEStatus;
    unsigned long long          TimeStamp;
    unsigned int                TimeValue;

    daemonize(MONITOR_MME_THREAD_NAME);

    MONITOR_DEBUG("Starting\n");

    while (Context->Monitoring)
    {
        MMEStatus               = TransformerGetLogEvent (Context);
        if (MMEStatus != MME_SUCCESS)
            break;

        if (down_interruptible (&(Context->EventReceived)) != 0)
            break;

        TimeValue               = *Context->DeviceContext->Timer;
        TimeStamp               = ktime_to_us (ktime_get ());

        if (Context->MMECommandStatus.TimeCode != 0)
        {
            unsigned long long  TimeDiff;
            /* This assumes that the timer is counting down from ClockMaxValue to 0 */
            if (Context->MMECommandStatus.TimeCode > TimeValue)
                TimeDiff        = (unsigned long long)(Context->MMECommandStatus.TimeCode - TimeValue);
            else
                TimeDiff        = ((unsigned long long)Context->MMECommandStatus.TimeCode + Context->ClockMaxValue + 1) - (unsigned long long)TimeValue;

            TimeStamp          -= ((unsigned long long)TimeDiff * 1000000ull) / Context->TicksPerSecond;
        }
        if (Context->Monitoring)
            MonitorRecordEvent         (Context->DeviceContext,
                                        Context->Id,
                                        Context->MMECommandStatus.EventID,
                                        TimeStamp,
                                        Context->MMECommandStatus.Parameters,
                                        Context->MMECommandStatus.Message);

    }

    MONITOR_DEBUG("Terminating\n");

    up (&(Context->ThreadTerminated));

    return 0;
}
/*}}}  */

