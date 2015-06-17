/************************************************************************
Copyright (C) 2000 STMicroelectronics. All Rights Reserved.

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

Source file name : osinline.c (kernel version)
Author :           Nick

Contains the useful operating system inline functions/types
that we need to port our wince code to linux.

Date        Modification                                    Name
----        ------------                                    --------
07-Mar-05   Created                                         Julian

************************************************************************/

#include "osdev_device.h"
#include "osinline.h"
#include "linux/debugfs.h"


struct OS_Event_s
{
	bool                    Valid;
	OS_Mutex_t              Mutex;
	OSDEV_WaitQueue_t       Queue;
};

/* --- Taskname table --- */

#define OS_MAX_NAME_SIZE    32

typedef struct OS_NameEntry_s
{
    OSDEV_Thread_t       Thread;
    char                 Name[OS_MAX_NAME_SIZE];
} OS_NameEntry_t;

#define OS_MAX_TASKS    32

static OS_NameEntry_t          OS_TaskNameTable[OS_MAX_TASKS];
// --------------------------------------------------------------
// Malloc and free functions

void            *OS_Malloc(     unsigned int             Size )
{
    return OSDEV_Malloc( Size );
}

// --------

OS_Status_t      OS_Free(       void                    *Address )
{
    return OSDEV_Free( Address );
}

// --------

void OS_InvalidateCacheRange( void* CPUAddress, unsigned int size )
{
    OSDEV_InvalidateCacheRange(CPUAddress,size);
}

// --------

void OS_FlushCacheRange( void* CPUAddress, unsigned int size )
{
    OSDEV_FlushCacheRange(CPUAddress,size);
}

// --------

void            *OS_PeripheralAddress( void* CPUAddress )
{
    return (void*)OSDEV_PeripheralAddress( (unsigned int)CPUAddress );
}


// --------

void OS_PurgeCacheRange( void* CPUAddress, unsigned int size )
{
    OSDEV_PurgeCacheRange(CPUAddress,size);
}


// --------------------------------------------------------------
//      The Semaphore functions

OS_Status_t   OS_SemaphoreInitialize( OS_Semaphore_t  *Semaphore,
				      unsigned int     InitialCount )
{
    return OSDEV_InitializeSemaphore( (OSDEV_Semaphore_t*)Semaphore, InitialCount );
}

//

OS_Status_t   OS_SemaphoreTerminate( OS_Semaphore_t  *Semaphore )
{
    OSDEV_DeInitializeSemaphore( (OSDEV_Semaphore_t*)Semaphore );
    return OS_NO_ERROR;
}

//

OS_Status_t   OS_SemaphoreWait( OS_Semaphore_t  *Semaphore )
{
    OSDEV_ClaimSemaphore( (OSDEV_Semaphore_t*)Semaphore );
    return OS_NO_ERROR;
}

//

OS_Status_t   OS_SemaphoreSignal( OS_Semaphore_t  *Semaphore )
{
    OSDEV_ReleaseSemaphore( (OSDEV_Semaphore_t*)Semaphore );
    return OS_NO_ERROR;
}

// --------------------------------------------------------------
//      The Mutex functions

OS_Status_t   OS_InitializeMutex( OS_Mutex_t  *Mutex )
{
    return OSDEV_InitializeSemaphore( (OSDEV_Semaphore_t*)Mutex, 1 );
}

//

OS_Status_t   OS_TerminateMutex( OS_Mutex_t  *Mutex )
{
    OSDEV_DeInitializeSemaphore( (OSDEV_Semaphore_t*)Mutex );
    return OS_NO_ERROR;
}

//

OS_Status_t   OS_LockMutex( OS_Mutex_t  *Mutex )
{
    OSDEV_ClaimSemaphore( (OSDEV_Semaphore_t*)Mutex );
    return OS_NO_ERROR;
}

//

OS_Status_t   OS_UnLockMutex( OS_Mutex_t  *Mutex )
{
    OSDEV_ReleaseSemaphore( (OSDEV_Semaphore_t*)Mutex );
    return OS_NO_ERROR;
}

// -----------------------------------------------------------------------------------------------
// The event functions
OS_Status_t   OS_InitializeEvent (OS_Event_t* Event)
{
    *Event = (OS_Event_t)OSDEV_Malloc( sizeof(struct OS_Event_s) );
    if( *Event != NULL )
    {
	OS_InitializeMutex        (&((*Event)->Mutex));
	if (OSDEV_InitializeWaitQueue (&((*Event)->Queue) ) == OSDEV_NoError)
	{
	    (*Event)->Valid   = false;
	    return OS_NO_ERROR;
	}
	else
	    OSDEV_Free (*Event);
    }

    return OS_ERROR;
}

OS_Status_t   OS_TerminateEvent (OS_Event_t* Event)
{
    OS_TerminateMutex( &((*Event)->Mutex));
    OSDEV_DeInitializeWaitQueue( (*Event)->Queue);
    OSDEV_Free( *Event );
    return OS_NO_ERROR;
}

OS_Status_t   OS_WaitForEvent (OS_Event_t* Event, OS_Timeout_t Timeout)
{
    if (!((*Event)->Valid))
    {
	OS_LockMutex (&((*Event)->Mutex));

	if (!((*Event)->Valid))                 // NOTE this may have changed before we locked access
	{
	    OS_UnLockMutex (&((*Event)->Mutex));
	    OSDEV_WaitForQueue( (*Event)->Queue, &((*Event)->Valid), (Timeout == OS_INFINITE) ? OSDEV_INFINITE : Timeout );
	    return ((*Event)->Valid) ? OS_NO_ERROR : OS_TIMED_OUT;
	}
	else
	    OS_UnLockMutex (&((*Event)->Mutex));
    }

    return OS_NO_ERROR;
}

bool OS_TestEventSet( OS_Event_t* Event )
{
    return (*Event)->Valid;
}

OS_Status_t OS_SetEvent (OS_Event_t* Event)
{
    OS_LockMutex (&((*Event)->Mutex));

    (*Event)->Valid     = true;
    OSDEV_WakeUpQueue ((*Event)->Queue);

    OS_UnLockMutex (&((*Event)->Mutex));
    return OS_NO_ERROR;
}

OS_Status_t OS_ResetEvent( OS_Event_t *Event )
{
    OS_LockMutex (&((*Event)->Mutex));

    (*Event)->Valid     = false;

    OS_UnLockMutex (&((*Event)->Mutex));
    return OS_NO_ERROR;
}

OS_Status_t OS_ReInitializeEvent( OS_Event_t *Event )
{
    OS_LockMutex (&((*Event)->Mutex));

    (*Event)->Valid = false;
    OSDEV_ReInitializeWaitQueue ((*Event)->Queue);

    OS_UnLockMutex (&((*Event)->Mutex));
    return OS_NO_ERROR;
}

// --------------------------------------------------------------
//      The Thread functions

OS_Status_t   OS_CreateThread( OS_Thread_t        *Thread,
			       OS_TaskEntry_t      TaskEntry,
			       OS_TaskParam_t      Parameter,
			       const char         *Name,
			       OS_TaskPriority_t   Priority )
{
unsigned int        FreeEntry;
OSDEV_Status_t      Status;

//

    for( FreeEntry=0; FreeEntry<(OS_MAX_TASKS-1); FreeEntry++ )
	if( OS_TaskNameTable[FreeEntry].Name[0] == '\0' )
	    break;

    strncpy( OS_TaskNameTable[FreeEntry].Name, Name, OS_MAX_NAME_SIZE );
    OS_TaskNameTable[FreeEntry].Name[OS_MAX_NAME_SIZE-1]        = '\0';

//

    Status = OSDEV_CreateThread( &(OS_TaskNameTable[FreeEntry].Thread),
				 (OSDEV_ThreadFn_t)TaskEntry,
				 (OSDEV_ThreadParam_t)Parameter,
				 Name,
				 (OSDEV_ThreadPriority_t)Priority );

    if( Status != OSDEV_NoError )
    {
	OS_TaskNameTable[FreeEntry].Name[0] = '\0';
	return OS_ERROR;
    }

    /* detach */

    *Thread     = OS_TaskNameTable[FreeEntry].Thread;
    return OS_NO_ERROR;
}

//

void   OS_TerminateThread( void )
{
unsigned int        i;

    for( i=0; i<OS_MAX_TASKS; i++ )
	if( (OS_TaskNameTable[i].Name[0] != '\0') && (OS_TaskNameTable[i].Thread == current) )
	{
	    OS_TaskNameTable[i].Name[0] = '\0';
	}

    OSDEV_ExitThread();
}

//

OS_Status_t OS_SetPriority( OS_TaskPriority_t Priority )
{
OSDEV_Status_t      Status;

    Status = OSDEV_SetPriority( (OSDEV_ThreadPriority_t) Priority );

    return (Status == OSDEV_NoError ? OS_NO_ERROR : OS_ERROR); 
}

//

OS_Status_t   OS_JoinThread( OS_Thread_t     Thread )
{
    return OS_NO_ERROR;
}

//

char   *OS_ThreadName( void )
{
unsigned int        i;

    for( i=0; i<OS_MAX_TASKS; i++ )
	if( (OS_TaskNameTable[i].Name != '\0') &&(OS_TaskNameTable[i].Thread == current) )
	    return OS_TaskNameTable[i].Name;

    return OSDEV_ThreadName();
}

// --------------------------------------------------------------
//      The Message Functions


OS_Status_t   OS_InitializeMessageQueue( OS_MessageQueue_t   *Queue )
{
    OSDEV_Print( "%s not implemented\n", __FUNCTION__ );
    return OS_ERROR;
}

//

OS_Status_t   OS_SendMessageCode( OS_MessageQueue_t   Queue,
						unsigned int        Code )
{
    OSDEV_Print( "%s not implemented\n", __FUNCTION__ );
    return OS_ERROR;
}

//

OS_Status_t   OS_GetMessageCode( OS_MessageQueue_t   Queue,
					       unsigned int       *Code,
					       bool                Blocking )
{
    OSDEV_Print( "%s not implemented\n", __FUNCTION__ );
    return OS_ERROR;
}

//

OS_Status_t   OS_GetMessage( OS_MessageQueue_t   Queue,
					   void               *Message,
					   unsigned int        MaxSizeOfMessage,
					   bool                Blocking )
{
    OSDEV_Print( "%s not implemented\n", __FUNCTION__ );
    return OS_ERROR;
}


// --------------------------------------------------------------
//      The Miscelaneous functions


void OS_ReSchedule( void )
{
    schedule();
}

unsigned int OS_GetTimeInSeconds( void )
{
struct timeval  Now;

#if !defined (CONFIG_USE_SYSTEM_CLOCK)
ktime_t         Ktime   = ktime_get();

    Now                 = ktime_to_timeval(Ktime);

#else
    do_gettimeofday( &Now );

#endif

    return Now.tv_sec;
}

unsigned int OS_GetTimeInMilliSeconds( void )
{
    return OSDEV_GetTimeInMilliSeconds();
}


unsigned long long OS_GetTimeInMicroSeconds( void )
{
    return OSDEV_GetTimeInMicroSeconds();
}


void OS_SleepMilliSeconds( unsigned int Value )
{
    OSDEV_SleepMilliSeconds( Value );
}

void OS_RegisterTuneable( const char *Name, unsigned int *Address )
{
    static struct dentry *root = NULL;
    struct dentry *dentry;

    if (NULL == root)
	root = debugfs_create_dir("havana", NULL);

    if (NULL != root)
	dentry = debugfs_create_u32( Name, 0600, root, Address );
}

// ----------------------------------------------------------------------------------------
//
// Initialization function, sets up task deleter for terminated tasks

OS_Status_t   OS_Initialize( void )
{
    memset( OS_TaskNameTable, 0x00, sizeof(OS_TaskNameTable) );

    OS_TaskNameTable[0].Thread  = current;
    strcpy( OS_TaskNameTable[0].Name, "Root Task" );

    return OS_NO_ERROR;
}

char* strdup (const char* String)
{
    char*  Copy = (char*)OS_Malloc (strlen (String) + 1);

    if (Copy != NULL)
	strcpy (Copy, String);
    return Copy;
}

// -----------------------------------------------------------------------------------------------
//
// Access to hardware watch points
//
#if defined (CONFIG_WATCHPOINTS)

#include <asm/watchpoint.h>

asmlinkage void user_watch_trap (unsigned long r4, unsigned long r5, unsigned long r6, unsigned long r7, struct pt_regs *regs)
{
    printk("User watch trap at 0x%.8x\n", (unsigned int)regs->pc);
    while (1);
}

void OS_Add_Hardware_Watchpoint (unsigned int* WatchedAddress, OS_WatchAccessType_t AccessType)
{
    struct hw_watchpoint        WatchPoint;

    hw_watch_init (&WatchPoint);
    WatchPoint.addr             = (unsigned int)WatchedAddress;
    WatchPoint.oneshot          = 1;
    switch (AccessType)
    {
	case OS_WatchRead:      WatchPoint.rw   = WP_READ;      break;
	case OS_WatchWrite:     WatchPoint.rw   = WP_WRITE;     break;
	default:                WatchPoint.rw   = WP_ACCESS;    break;
    }
    add_hw_watch (&WatchPoint);
}

void OS_Disable_Hardware_Watchpoint (void)
{
    disable_hw_watch ();
}

void OS_Reenable_Hardware_Watchpoint (void)
{
    reenable_hw_watch ();
}
#endif

//////////////////////////////////////////////////////////////////////////////
// Services required by C++ code. These are typically part of the C++ run time
// but we dont have a run time!
#include <linux/vmalloc.h>

void* __builtin_new(size_t size)
{
    if (size > 0)
    {
	void* p;

	if (likely(size <= (4*PAGE_SIZE)))
	    p   = kmalloc (size, GFP_KERNEL);
	else
	    p   = vmalloc (size);


#ifdef ENABLE_MALLOC_POISONING
	if (p)
	    memset(p, 0xcc, size);
#else
	if (p)
	    memset(p, 0, size);
#endif

	return p;
    }

    return NULL;
}


void* __builtin_vec_new(size_t size)
{
    return __builtin_new(size);
}


void __builtin_delete(void *ptr)
{
    if (ptr)
    {
	if (unlikely((ptr >= (void*)VMALLOC_START) && (ptr < (void*)VMALLOC_END)))
	    vfree (ptr);
	else
	    kfree (ptr);
    }
}


void __builtin_vec_delete(void* ptr)
{
    __builtin_delete(ptr);
}


void __pure_virtual(void)
{
}

void __cxa_pure_virtual(void)
{
}

