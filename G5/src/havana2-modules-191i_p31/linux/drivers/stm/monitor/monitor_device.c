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

Source file name : monitor_device.c
Author :           Julian

Implementation of linux monitor device

Date        Modification                                    Name
----        ------------                                    --------
24-Jul-08   Created                                         Julian

************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/bpa2.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#include "monitor_ioctls.h"
#include "monitor_device.h"
#include "monitor_module.h"
#include "monitor_inline.h"

/*{{{  #defines and macros*/
/* Convert the event name into human readable form. */
static const char* EventName (monitor_event_code_t Event)
{
#define E(e) case MONITOR_EVENT_ ## e: return #e
    switch(Event)
    {
        E(AUDIO_SIGNAL_ACQUIRED);
        E(AUDIO_SIGNAL_LOST);
        E(AUDIO_FORMAT_DETECTED);
        E(AUDIO_FORMAT_UNKNOWN);
        E(AUDIO_INITIALIZATION_START);
        E(AUDIO_SHUTDOWN_COMPLETE);
        E(AUDIO_SAMPLING_RATE_CHANGE);
        E(AUDIO_SAMPLES_PROCESSED);
        E(AUDIO_POTENTIAL_ARTIFACT);
        E(VIDEO_FIRST_FIELD_ACQUIRED);
        E(VIDEO_ACQUISITION_STOPPED);
        E(VIDEO_SIGNAL_ACQUIRED);
        E(VIDEO_SIGNAL_LOST);
        E(VIDEO_FIELDS_EXPECTED);
        E(VIDEO_FIELDS_DROPPED);
        E(VIDEO_TNR_SETTING);
        E(VIDEO_DEINTERLACER_SETTING);
        E(INFORMATION);
        default: return "Illegal Event";
    }
#undef E
}
/*}}}*/
/*{{{  prototypes*/
static int MonitorOpen                 (struct inode           *Inode,
                                        struct file            *File);
static int MonitorRelease              (struct inode           *Inode,
                                        struct file            *File);
static ssize_t MonitorRead             (struct file            *File,
                                        char __user            *Buffer,
                                        size_t                  Count,
                                        loff_t                 *ppos);
static ssize_t MonitorWrite            (struct file            *File,
                                        const char __user      *Buffer,
                                        size_t                  Count,
                                        loff_t                 *ppos);
static int MonitorIoctl                (struct inode           *Inode,
                                        struct file            *File,
                                        unsigned int            IoctlCode,
                                        unsigned long           Parameter);
static unsigned int MonitorPoll        (struct file*            File,
                                        poll_table*             Wait);

/*}}}*/
/*{{{  static data*/
static struct file_operations MonitorFops =
{
        owner:          THIS_MODULE,
        ioctl:          MonitorIoctl,
        open:           MonitorOpen,
        release:        MonitorRelease,
        read:           MonitorRead,
        write:          MonitorWrite,
        poll:           MonitorPoll
};

/*}}}*/

/*{{{  MonitorInit*/
struct file_operations* MonitorInit (struct DeviceContext_s* Context)
{
    unsigned int        i;

    MONITOR_DEBUG("\n");

    init_waitqueue_head (&(Context->EventQueue.EventReceived));
    init_waitqueue_head (&(Context->EventQueue.BufferReleased));
    spin_lock_init (&(Context->EventQueue.Lock));

    Context->OpenCount                          = 0;
    Context->Status.status_code                 = MONITOR_STATUS_IDLE;
    Context->Status.subsystem_mask              = MONITOR_SYSTEM_GENERIC;     /* Allow all system wide events */
    Context->EventQueue.Write                   = 0;
    Context->EventQueue.Read                    = 0;
    Context->EventQueue.LostCount               = 0;

    for (i = 0; i <= MAX_MONITOR_EVENT_CODE; i++)
    {
        memset (Context->StoredEventValues[i].Parameters, 0, sizeof(unsigned int)*MONITOR_PARAMETER_COUNT);
        Context->StoredEventValues[i].Count     = 0;
    }

    for (i = 0; i < MONITOR_MAX_MME_DEVICES; i++)
    {
        MonitorMMEInit (Context, &(Context->MMEContext[i]), i+1);
    }

    return &MonitorFops;
}
/*}}}*/

/*{{{  MonitorIoctlEnable*/
static int MonitorIoctlEnable (struct DeviceContext_s* Context, monitor_subsystem_mask_t SubsystemMask)
{
    MONITOR_DEBUG("\n");

    if ((SubsystemMask & MONITOR_ENABLE_ALL) == 0)
    {
        MONITOR_ERROR("Invalid subsystem mask %08x\n", SubsystemMask);
        return -EINVAL;
    }
    Context->Status.subsystem_mask     |= (SubsystemMask & MONITOR_ENABLE_ALL);

    return 0;
}
/*}}}*/
/*{{{  MonitorIoctlDisable*/
static int MonitorIoctlDisable (struct DeviceContext_s* Context, monitor_subsystem_mask_t SubsystemMask)
{
    MONITOR_DEBUG("\n");

    if ((SubsystemMask & MONITOR_ENABLE_ALL) == 0)
    {
        MONITOR_ERROR("Invalid subsystem mask %08x\n", SubsystemMask);
        return -EINVAL;
    }
    Context->Status.subsystem_mask     &= (~(SubsystemMask & MONITOR_ENABLE_ALL));

    return 0;
}
/*}}}*/
/*{{{  MonitorIoctlGetStatus*/
static int MonitorIoctlGetStatus (struct DeviceContext_s* Context, struct monitor_status_s* Status)
{
    struct EventQueue_s*        EventList       = &Context->EventQueue;

    MONITOR_DEBUG("\n");

    if (!access_ok (VERIFY_WRITE, Status,  sizeof (struct monitor_status_s)))
    {
        MONITOR_ERROR ("Invalid status address = %p\n", Status);
        return -EFAULT;
    }

    if (EventList->Read >= EventList->Write)
        Context->Status.event_count     = EventList->Read - EventList->Write;
    else
        Context->Status.event_count     = (EventList->Read + MAX_MONITOR_EVENT) - EventList->Write;

    Context->Status.events_lost         = EventList->LostCount;

    memcpy (Status, &Context->Status, sizeof(struct monitor_status_s));

    return 0;
}
/*}}}*/
/*{{{  MonitorIoctlRequestEvent*/
static int MonitorIoctlRequestEvent (struct DeviceContext_s* Context, struct monitor_event_request_s* EventRequest)
{
    struct EventValue_s*   StoredEvent;

    MONITOR_DEBUG("\n");

    if (!access_ok (VERIFY_WRITE, EventRequest,  sizeof (struct monitor_event_request_s)))
    {
        MONITOR_ERROR ("Invalid status address = %p\n", EventRequest);
        return -EFAULT;
    }

    if ((EventRequest->event_code & MONITOR_EVENT_INDEX_MASK) > MAX_MONITOR_EVENT_CODE)
    {
        MONITOR_ERROR ("Invalid event requested (%x)\n", EventRequest->event_code);
        return -EINVAL;
    }

    StoredEvent                 = &(Context->StoredEventValues[EventRequest->event_code & MONITOR_EVENT_INDEX_MASK]);

    memcpy (EventRequest->parameters, StoredEvent->Parameters, sizeof(StoredEvent->Parameters));
    EventRequest->count         = StoredEvent->Count;
    if (EventRequest->reset)
        StoredEvent->Count      = 0;

    return 0;
}
/*}}}*/

/*{{{  MonitorOpen*/
static int MonitorOpen         (struct inode*   Inode,
                                struct file*    File)
{
    struct DeviceContext_s*     Context         = container_of(Inode->i_cdev, struct DeviceContext_s, CDev);
    struct ModuleContext_s*     ModuleContext   = Context->ModuleContext;

    MONITOR_DEBUG ("\n");
    File->private_data                          = Context;

    mutex_lock (&(ModuleContext->Lock));
    if (Context->Status.status_code == MONITOR_STATUS_IDLE)
    {
        Context->Status.status_code             = MONITOR_STATUS_RUNNING;
        Context->Status.subsystem_mask          = MONITOR_SYSTEM_GENERIC;     /* Allow all system wide events */
        /*
        Context->EventQueue.Write               = 0;
        Context->EventQueue.Read                = 0;
        Context->EventQueue.LostCount           = 0;
        */
    }
    Context->OpenCount++;
    mutex_unlock (&(ModuleContext->Lock));

    return 0;
}
/*}}}*/
/*{{{  MonitorRelease*/
static int MonitorRelease      (struct inode*   Inode,
                                struct file*    File)
{
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)File->private_data;
    struct ModuleContext_s*     ModuleContext   = Context->ModuleContext;

    MONITOR_DEBUG("\n");

    mutex_lock (&(ModuleContext->Lock));
    Context->OpenCount--;
    if (Context->OpenCount == 0)
    {
        Context->Status.status_code       = MONITOR_STATUS_IDLE;
        Context->Status.subsystem_mask    = MONITOR_SYSTEM_GENERIC;     /* Allow all system wide events */
        /*
        Context->EventQueue.Write         = 0;
        Context->EventQueue.Read          = 0;
        Context->EventQueue.LostCount     = 0;
        */
    }
    mutex_unlock (&(ModuleContext->Lock));

    return 0;
}
/*}}}*/
/*{{{  MonitorRead*/
static ssize_t MonitorRead (struct file *File, char __user* Buffer, size_t Count, loff_t* ppos)
{
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)File->private_data;
    struct ModuleContext_s*     ModuleContext   = Context->ModuleContext;
    int                         Result          = 0;
    struct EventQueue_s*        EventList       = &Context->EventQueue;
    unsigned int                Flags;
    unsigned int                RecordLength    = 0;

    /*MONITOR_DEBUG ("EventList: Read %d, Write %d\n", EventList->Read, EventList->Write);*/

    mutex_lock (&(ModuleContext->Lock));

    /*
    if (EventList->LostCount != 0)
    {
        EventList->LostCount    = 0;
        mutex_unlock (&(ModuleContext->Lock));
        return -EOVERFLOW;
    }
    */
    if (EventList->Write == EventList->Read)
    {
        mutex_unlock (&(ModuleContext->Lock));
        if ((File->f_flags & O_NONBLOCK) != 0)
            return -EWOULDBLOCK;

        Result  = wait_event_interruptible (EventList->EventReceived, EventList->Write != EventList->Read);
        if (Result != 0)
            return -ERESTARTSYS;
        mutex_lock (&(ModuleContext->Lock));
    }

    if (Context->Status.status_code == MONITOR_STATUS_RUNNING)
    {
        spin_lock_irqsave (&EventList->Lock, Flags);

        RecordLength            = EventList->Event[EventList->Read].RecordLength;
        Result                  = copy_to_user (Buffer, &(EventList->Event[EventList->Read].EventString), RecordLength);
        if (Result == 0)
            EventList->Read     = (EventList->Read + 1) % MAX_MONITOR_EVENT;

        spin_unlock_irqrestore (&EventList->Lock, Flags);

        if (Result == 0)
            wake_up_interruptible (&EventList->BufferReleased);         /* Wake up any writers waiting for space */
    }

    mutex_unlock (&(ModuleContext->Lock));

    if (Result != 0)
        return -EFAULT;

    return RecordLength;

}

/*}}}*/
/*{{{  MonitorWrite*/
static ssize_t MonitorWrite    (struct file *File, const char __user* Buffer, size_t Count, loff_t* ppos)
{
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)File->private_data;
    struct ModuleContext_s*     ModuleContext   = Context->ModuleContext;
    int                         Result          = 0;
    struct EventQueue_s*        EventList       = &Context->EventQueue;
    unsigned int                Flags;
    unsigned int                Next;
    struct EventRecord_s*       EventRecord;

    mutex_lock (&(ModuleContext->Lock));

    EventList                   = &Context->EventQueue;
    Next                        = (EventList->Write + 1) % MAX_MONITOR_EVENT;

    if (Next == EventList->Read)
    {
        mutex_unlock (&(ModuleContext->Lock));
        if ((File->f_flags & O_NONBLOCK) != 0)
            return -EWOULDBLOCK;

        Result  = wait_event_interruptible (EventList->BufferReleased, Next != EventList->Read);
        if (Result != 0)
            return -ERESTARTSYS;
        mutex_lock (&(ModuleContext->Lock));
    }

    if (Count > MONITOR_EVENT_RECORD_SIZE)
        Count                   = MONITOR_EVENT_RECORD_SIZE;

    spin_lock_irqsave (&EventList->Lock, Flags);

    EventRecord                 = &(EventList->Event[EventList->Write]);
    EventRecord->RecordLength   = Count;
    Result                      = copy_from_user (EventRecord->EventString, Buffer, Count);
    if (Result == 0)
    {
        EventRecord->EventString[MONITOR_EVENT_RECORD_SIZE-1]   = '\n';
        EventList->Write        = Next;
    }
    spin_unlock_irqrestore (&EventList->Lock, Flags);

    if (Result == 0)
        wake_up_interruptible (&EventList->EventReceived);

    mutex_unlock (&(ModuleContext->Lock));

    if (Result != 0)
        return -EFAULT;

    return Count;

}
/*}}}*/
/*{{{  MonitorIoctl*/
static int MonitorIoctl        (struct inode*   Inode,
                                struct file*    File,
                                unsigned int    IoctlCode,
                                unsigned long   Parameter)
{
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)File->private_data;
    struct ModuleContext_s*     ModuleContext   = Context->ModuleContext;
    int                         Result          = 0;

    /*MONITOR_DEBUG("MonitorIoctl : Ioctl %08x\n", IoctlCode);*/

    if (((File->f_flags & O_ACCMODE) == O_RDONLY) &&
        ((IoctlCode != MONITOR_GET_STATUS) && (IoctlCode != MONITOR_REQUEST_EVENT)))
        return -EPERM;

    mutex_lock (&(ModuleContext->Lock));

    switch (IoctlCode)
    {
        case MONITOR_DISABLE:       Result = MonitorIoctlDisable       (Context, (monitor_subsystem_mask_t)Parameter);          break;
        case MONITOR_ENABLE:        Result = MonitorIoctlEnable        (Context, (monitor_subsystem_mask_t)Parameter);          break;
        case MONITOR_GET_STATUS:    Result = MonitorIoctlGetStatus     (Context, (struct monitor_status_s*)Parameter);          break;
        case MONITOR_REQUEST_EVENT: Result = MonitorIoctlRequestEvent  (Context, (struct monitor_event_request_s*)Parameter);   break;
        default:
            MONITOR_ERROR("Error - invalid ioctl %08x\n", IoctlCode);
            Result      = -ENOIOCTLCMD;
    }

    mutex_unlock (&(ModuleContext->Lock));

    return Result;
}
/*}}}*/
/*{{{  MonitorPoll*/
static unsigned int MonitorPoll (struct file* File, poll_table* Wait)
{
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)File->private_data;
    unsigned int                Mask            = 0;

    /*MONITOR_DEBUG("\n");*/

    poll_wait (File, &Context->EventQueue.EventReceived, Wait);

    poll_wait (File, &Context->EventQueue.BufferReleased, Wait);

    if (Context->EventQueue.Write != Context->EventQueue.Read)
        Mask    = POLLIN | POLLRDNORM;

    if (((Context->EventQueue.Write + 1) % MAX_MONITOR_EVENT) != Context->EventQueue.Read)
        Mask   |= POLLOUT | POLLWRNORM;

    return Mask;
}
/*}}}*/

/*{{{  MonitorRecordEvent*/
void MonitorRecordEvent        (struct DeviceContext_s*         Context,
                                unsigned int                    SourceId,
                                monitor_event_code_t            EventCode,
                                unsigned long long              TimeStamp,
                                unsigned int                    Parameters[MONITOR_PARAMETER_COUNT],
                                const char*                     Description)
{
    struct EventQueue_s*                EventList;
    unsigned int                        Next;
    unsigned int                        EventReceived   = false;
    unsigned int                        Flags;
    struct EventRecord_s*               EventRecord;
    struct EventValue_s*                StoredEvent;

    StoredEvent                 = &(Context->StoredEventValues[EventCode & MONITOR_EVENT_INDEX_MASK]);

    if (Parameters)
        memcpy (StoredEvent->Parameters, Parameters, sizeof(Parameters));
    StoredEvent->Count++;

    if ((EventCode & MONITOR_EVENT_REPORT_ON_REQUEST) != 0)
        return;

    if ((Context->Status.subsystem_mask & EventCode) == 0)
        return;

    EventList                   = &Context->EventQueue;

    spin_lock_irqsave (&EventList->Lock, Flags);

    Next                        = (EventList->Write + 1) % MAX_MONITOR_EVENT;
    if (Next == EventList->Read)
    {
        EventList->LostCount++;
        EventList->Read         = (EventList->Read + 1) % MAX_MONITOR_EVENT;
    }

    EventRecord                 = &(EventList->Event[EventList->Write]);
    if (Parameters)
        EventRecord->RecordLength       = snprintf ((char*)&(EventRecord->EventString), MONITOR_EVENT_RECORD_SIZE,
                                                "%s at %lluus source %d (0x%x, 0x%x, 0x%x, 0x%x) \"%s\"\n",
                                                EventName(EventCode), TimeStamp, SourceId,
                                                Parameters[0], Parameters[1], Parameters[2], Parameters[3], Description);
    else
        EventRecord->RecordLength       = snprintf ((char*)&(EventRecord->EventString), MONITOR_EVENT_RECORD_SIZE,
                                                "%s at %lluus source %d \"%s\"\n",
                                                EventName(EventCode), TimeStamp, SourceId, Description);
    EventRecord->EventString[MONITOR_EVENT_RECORD_SIZE-1]       = '\n';
    if (EventRecord->RecordLength > MONITOR_EVENT_RECORD_SIZE)
        EventRecord->RecordLength       = MONITOR_EVENT_RECORD_SIZE;

    EventList->Write                    = Next;
    EventReceived                       = true;

    spin_unlock_irqrestore (&EventList->Lock, Flags);

    /*MONITOR_DEBUG ("Write:%d, Read:%d: Message:%s\n", EventList->Write, EventList->Read, (char*)(EventRecord->EventString));*/

    if (EventReceived)
        wake_up_interruptible (&EventList->EventReceived);

}

/*}}}*/
/*{{{  MonitorSignalEvent*/
#if defined (CONFIG_MONITOR)

void MonitorSignalEvent        (monitor_event_code_t    EventCode,
                                unsigned int            Parameters[MONITOR_PARAMETER_COUNT],
                                const char*             Description)
{
    unsigned int                DeviceId        = 0;
    struct DeviceContext_s*     Context         = GetDeviceContext (DeviceId);

    // If no context means the driver has not been installed.
    if (!Context)
    {
            //MONITOR_ERROR("Invalid monitor device %d\n", DeviceId);
        return;
    }

    MonitorRecordEvent         (Context,
                                0,
                                EventCode,
                                (unsigned long long)ktime_to_us (ktime_get ()),
                                Parameters,
                                Description);

}

EXPORT_SYMBOL (MonitorSignalEvent);

#endif
/*}}}*/

