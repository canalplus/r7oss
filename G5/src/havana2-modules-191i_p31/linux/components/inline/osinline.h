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

Source file name : osinline.h (kernel version)
Author :           Nick

Contains the useful operating system inline functions/types
that we need to port our wince code to linux.

Date        Modification                                    Name
----        ------------                                    --------
07-Mar-05   Created                                         Julian

************************************************************************/

#ifndef H_OSINLINE
#define H_OSINLINE

#ifdef __cplusplus
#undef  true
#undef  false
#define true linux_stddef_true
#define false linux_stddef_false
typedef bool _Bool;
#define bool  linux_types_bool
#include <linux/stddef.h>
#include <linux/types.h>
#undef  true
#undef  false
#undef  bool
#else
#include <linux/stddef.h>
#include <linux/types.h>
#endif
#include <linux/unistd.h>
#include <linux/string.h>

#include "report.h"

#define __st220pft(X,Y) ((void) ((X) + (Y)))

/* --- Define bool if we not in c++ --- */

#ifndef __cplusplus
#ifndef bool
#define bool    unsigned char
#endif
/* true/false come from linux/stddef.h */
#endif

#ifndef boolean
#define boolean bool
#endif

/* --- Return codes --- */

#define OS_NO_ERROR             0
#define OS_ERROR                1
#define OS_TIMED_OUT            2
#define OS_NO_MESSAGE           3

//

#define OS_NO_PRIORITY          0xffffffff
#define OS_HIGHEST_PRIORITY     99
#define OS_MID_PRIORITY         50
#define OS_LOWEST_PRIORITY      0
#define OS_INFINITE             ((OS_Timeout_t)0xffffffff)
#define OS_INVALID_THREAD       ((OS_Thread_t)0)

/* --- Useful type declarations --- */

/* --- not implemented in kernel version --- */
typedef unsigned int         OS_MessageQueue_t;

//

typedef int                              OS_Timeout_t;
typedef struct task_struct              *OS_Thread_t;
typedef void                            *OS_TaskParam_t;
typedef unsigned int                     OS_TaskPriority_t;
typedef int                              OS_Status_t;
typedef struct semaphore                *OS_Semaphore_t;
typedef struct semaphore                *OS_Mutex_t;
typedef struct OS_Event_s               *OS_Event_t;

typedef void  *(*OS_TaskEntry_t)( void* Parameter );
#define OS_TaskEntry( fn )   void  *fn( void* Parameter )

/* --- Useful macro's --- */

#define strerror( x )                   "Unknown error"

#undef min
#define min( a,b )                      (((a)<(b)) ? (a) : (b))

#undef max
#define max( a,b )                      (((a)<(b)) ? (b) : (a))

#undef SignExtend
#define SignExtend( val,bits )          (((val)&(0xffff<<((bits)-1))) ? ((val) | (0xffff<<((bits)-1))) : (val))

#undef inrange
#define inrange( val, lower, upper )    (((val)>=(lower)) && ((val)<=(upper)))

#undef Clamp
#define Clamp( v, l, u )                {if( v < l ) v = l; else if( v > u ) v = u;}

// --------------------------------------------------------------
//      Specialist st200 intruction emulation functions.
//      I have left these as inlines, since they do not
//      interact with anything else, they are generally
//      required to be fast.

#ifndef H_OSDEV_DEVICE

static inline unsigned int      __swapbw( unsigned int a )      // ByteSwap
{
unsigned int tmp1 = (a << 8) & 0xFF00FF00;
unsigned int tmp2 = (a >> 8) & 0x00FF00FF;
unsigned int tmp3 = tmp1 | tmp2;

    return ((tmp3 >> 16) | (tmp3 << 16));
}

static inline unsigned int      __lzcntw( unsigned int a )      // CountLeadingZeros
{
unsigned int    i = 0;
unsigned int    b;

    i = (a ? 0 : 1);

    b  = a & 0xffff0000;
    a  = (b ? b : a);
    i += (b ? 0 : 16);

    b  = a & 0xff00ff00;
    a = (b ? b : a);
    i += (b ? 0 : 8);

    b  = a & 0xf0f0f0f0;
    a  = (b ? b : a);
    i += (b ? 0 : 4);

    b  = a & 0xcccccccc;
    a  = (b ? b : a);
    i += (b ? 0 : 2);

    b  = a & 0xaaaaaaaa;
    i += (b ? 0 : 1);

    return i;
}

//

static inline unsigned int      __gethw( unsigned long long a )
{
    return (unsigned int)(a >> 32);
}

//

static inline unsigned int      __getlw( unsigned long long a )
{
    return (unsigned int)(a & 0xffffffff);
}

#endif

// -----------------------------------------------------------------------------------------------


#ifdef __cplusplus
extern "C" {
#endif
// --------------------------------------------------------------
//      The Memeory functions

void         *OS_Malloc(                        unsigned int             Size );
OS_Status_t   OS_Free(                          void                    *Address );

void          OS_InvalidateCacheRange(          void                    *CPUAddress,
						unsigned int             size );
void          OS_FlushCacheRange(               void                    *CPUAddress,
						unsigned int             size );
void          OS_PurgeCacheRange(               void                    *CPUAddress,
						unsigned int             size );
void         *OS_PeripheralAddress(             void                    *CPUAddress );

// --------------------------------------------------------------
//      The Semaphore functions

OS_Status_t   OS_SemaphoreInitialize(           OS_Semaphore_t          *Semaphore,
						unsigned int             InitialCount );
OS_Status_t   OS_SemaphoreTerminate(            OS_Semaphore_t          *Semaphore );
OS_Status_t   OS_SemaphoreWait(                 OS_Semaphore_t          *Semaphore );
OS_Status_t   OS_SemaphoreSignal(               OS_Semaphore_t          *Semaphore );

// --------------------------------------------------------------
//      The Mutex functions

OS_Status_t   OS_InitializeMutex(               OS_Mutex_t             *Mutex );
OS_Status_t   OS_TerminateMutex(                OS_Mutex_t             *Mutex );
OS_Status_t   OS_LockMutex(                     OS_Mutex_t             *Mutex );
OS_Status_t   OS_UnLockMutex(                   OS_Mutex_t             *Mutex );

// --------------------------------------------------------------
//      The Event functions - not implemented

OS_Status_t   OS_InitializeEvent(               OS_Event_t             *Event );
OS_Status_t   OS_WaitForEvent(                  OS_Event_t             *Event,
						OS_Timeout_t            Timeout );
OS_Status_t   OS_WaitForEventInterruptible(     OS_Event_t             *Event );
bool          OS_TestEventSet(                  OS_Event_t             *Event );
OS_Status_t   OS_SetEvent(                      OS_Event_t             *Event );
OS_Status_t   OS_ResetEvent(                    OS_Event_t             *Event );
OS_Status_t   OS_ReInitializeEvent(             OS_Event_t             *Event );
OS_Status_t   OS_TerminateEvent(                OS_Event_t             *Event );

// --------------------------------------------------------------
//      The Thread functions

OS_Status_t   OS_CreateThread(                  OS_Thread_t            *Thread,
						OS_TaskEntry_t          TaskEntry,
						OS_TaskParam_t          Parameter,
						const char              *Name,
						OS_TaskPriority_t       Priority );
void          OS_TerminateThread(               void );
OS_Status_t   OS_JoinThread(                    OS_Thread_t             Thread );
char         *OS_ThreadName(                    void );
OS_Status_t   OS_SetPriority(                   OS_TaskPriority_t       Priority );

// --------------------------------------------------------------
//      The Message Functions - not implemented

OS_Status_t   OS_InitializeMessageQueue(        OS_MessageQueue_t      *Queue );
OS_Status_t   OS_SendMessageCode(               OS_MessageQueue_t       Queue,
						unsigned int            Code );
OS_Status_t   OS_GetMessageCode(                OS_MessageQueue_t       Queue,
						unsigned int           *Code,
						bool                    Blocking );
OS_Status_t   OS_GetMessage(                    OS_MessageQueue_t       Queue,
						void                   *Message,
						unsigned int            MaxSizeOfMessage,
						bool                    Blocking );

// --------------------------------------------------------------
//      The Miscelaneous functions


void          OS_ReSchedule(                    void );
unsigned int  OS_GetTimeInSeconds(              void );
unsigned int  OS_GetTimeInMilliSeconds(         void );
unsigned long long  OS_GetTimeInMicroSeconds(   void );
void          OS_SleepMilliSeconds(             unsigned int            Value );
void          OS_RegisterTuneable(              const char             *Name,
						unsigned int           *Address );

// ----------------------------------------------------------------------------------------
//
// Initialization function, sets up task deleter for terminated tasks

OS_Status_t   OS_Initialize(                    void );

char* strdup (const char* String);
void* __builtin_new(size_t size);
void* __builtin_vec_new(size_t size);
void __builtin_delete(void *ptr);
void __builtin_vec_delete(void* ptr);
void __pure_virtual(void);
void __cxa_pure_virtual(void);

// -----------------------------------------------------------------------------------------------
//
// Methods to access hardware watch points
//
typedef enum
{
    OS_WatchRead        = 1,
    OS_WatchWrite       = 2,
    OS_WatchAccess      = 3
} OS_WatchAccessType_t;

void OS_Add_Hardware_Watchpoint(                unsigned int*           WatchedAddress,
						OS_WatchAccessType_t    AccessType );
void OS_Disable_Hardware_Watchpoint(            void );
void OS_Reenable_Hardware_Watchpoint(           void );

#ifdef __cplusplus
}

///////////////////////////////////////////////////////////////////////////
///
/// Mutex wrapper to ensure the mutex is automatically released.
///
/// This uses an odd (but suprisingly common) C++ trick to ensure
/// the mutex is not left locked by accident. Basically the constructor
/// takes the mutex and the destructor, which is called automatically when
/// the variable goes out of scope (e.g. by a method return), releases
/// it.
///
/// Usage: OS_AutoLockMutex AutoLock( &NameOfMutex );
///
class OS_AutoLockMutex
{
public:
	inline OS_AutoLockMutex(OS_Mutex_t *M) : Mutex(M) { OS_LockMutex(M); }
	inline ~OS_AutoLockMutex() { OS_UnLockMutex(Mutex); }
private:
	OS_Mutex_t *Mutex;
};
#endif

#endif
