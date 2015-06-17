/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
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
extern "C" {
#include <linux/string.h>
}
#undef  true
#undef  false
#undef  bool
#undef NULL
#define NULL 0
#undef  inline
#else
#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/string.h>
#endif
#include <linux/unistd.h>

#ifndef __KERNEL__
#include <stdint.h>
#endif

//
// Error code remapping (and inclusion of errno ABI)
#ifdef __cplusplus
extern "C" {
#endif
#ifndef __KERNEL__
#include <errno.h>
#else
#include <linux/errno.h>
#include <linux/err.h>
#endif
#ifdef __cplusplus
}
#endif

#include "report.h"

/* --- Define bool if we not in c++ --- */

#ifndef __cplusplus
#ifndef bool
#define bool    unsigned char
#endif
/* true/false come from linux/stddef.h */
#endif

/* --- Return codes -- values must be compatible with OSDEV_Status_t enums --- */

#define OS_NO_ERROR             0
#define OS_ERROR                1
#define OS_TIMED_OUT            2
#define OS_INTERRUPTED          3

//

#define OS_NO_PRIORITY          0xffffffff
#define OS_HIGHEST_PRIORITY     99
#define OS_MID_PRIORITY         50
#define OS_LOWEST_PRIORITY      0
#define OS_INFINITE             ((OS_Timeout_t)0xffffffff)
#define OS_INVALID_THREAD       ((OS_Thread_t)0)

/* --- Useful type declarations --- */

typedef int                              OS_Timeout_t;
typedef struct task_struct              *OS_Thread_t;
typedef void                            *OS_TaskParam_t;

typedef struct {
	const char *name;
	int policy;
	int priority;
} OS_TaskDesc_t;

typedef int                              OS_Status_t;

// Use int64_t data to store linux native types
// needed since headers not includable in C++ code.
// Pick conservative sizes to accomodate the many kernel debug options affecting
// structure sizes.
typedef struct os_semaphore { int64_t data[20]; } OS_Semaphore_t;
typedef struct os_rw_lock   { int64_t data[18]; } OS_RwLock_t;
typedef struct os_mutex     { int64_t data[16]; } OS_Mutex_t;
typedef struct os_rt_mutex  { int64_t data[18]; } OS_RtMutex_t;
typedef struct os_spinlock  { int64_t data[18]; unsigned long flags_irq; } OS_SpinLock_t;
typedef struct os_waitqueue { int64_t data[20]; } OS_WaitQueueHead_t;

typedef struct OS_Event_s {
	bool               Valid;
	OS_SpinLock_t      Lock;
	OS_WaitQueueHead_t Queue;
} OS_Event_t;

typedef phys_addr_t                      OS_PhysAddr_t;

/*
 * Per-lock state used by kernel lockdep checker.
 * We can not use directly the original kernel struct (lock_class_key) because
 * we are included from C++ code, so we use a big enough blob instead.
 */
typedef struct {
	uint64_t secret[2];          /* worst-possible alignment */
} OS_OpaqueLockClassKey_t;

typedef void  *(*OS_TaskEntry_t)(void *Parameter);
#define OS_TaskEntry( fn )   void  *fn( void* Parameter )

/* --- Useful macro's --- */

#undef min
#define min( a,b )                      (((a)<(b)) ? (a) : (b))

#undef max
#define max( a,b )                      (((a)<(b)) ? (b) : (a))

#undef inrange
#define inrange( val, lower, upper )    (((val)>=(lower)) && ((val)<=(upper)))

#define abs64(x) ({        \
  s64 __x = (x);           \
  (__x < 0) ? -__x : __x;  \
})

#define samesign( a, b )    ((a)*(b) >= 0)

#undef Clamp
#define Clamp( v, l, u )                {if( v < l ) v = l; else if( v > u ) v = u;}

#undef lengthof
#define lengthof(x) ((int) (sizeof(x) / sizeof(x[0])))

// --------------------------------------------------------------
//      Specialist instruction emulation functions.
//      left these as inlines, since they do not
//      interact with anything else, they are generally
//      required to be fast.

#ifndef H_OSDEV_DEVICE

static inline unsigned int      __swapbw(unsigned int a)        // ByteSwap
{
	unsigned int tmp1 = (a << 8) & 0xFF00FF00;
	unsigned int tmp2 = (a >> 8) & 0x00FF00FF;
	unsigned int tmp3 = tmp1 | tmp2;

	return ((tmp3 >> 16) | (tmp3 << 16));
}

static inline unsigned int      __lzcntw(unsigned int a)        // CountLeadingZeros
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

#endif

// -----------------------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------
//      Memory functions

void         *OS_Malloc(unsigned int             Size);
void          OS_Free(void                    *Address);

void          OS_FlushCacheAll(void);
void          OS_FlushCacheRange(void                  *CPUAddress,
                                 OS_PhysAddr_t          Physical,
                                 unsigned int           size);
void          OS_InvCacheRange(void                    *CPUAddress,
                               OS_PhysAddr_t            Physical,
                               unsigned int             size);
void          OS_PurgeCacheRange(void                  *CPUAddress,
                                 OS_PhysAddr_t          Physical,
                                 unsigned int           size);

// --------------------------------------------------------------
//      Semaphore functions

void          OS_SemaphoreInitialize(OS_Semaphore_t        *Semaphore,
                                     unsigned int           InitialCount);
void          OS_SemaphoreTerminate(OS_Semaphore_t         *Semaphore);
void          OS_SemaphoreWaitNoInt(OS_Semaphore_t         *Semaphore);
OS_Status_t   OS_SemaphoreWaitInterruptible(OS_Semaphore_t *Semaphore);
OS_Status_t   OS_SemaphoreWaitAuto(OS_Semaphore_t          *Semaphore);
OS_Status_t   OS_SemaphoreWaitTimeout(OS_Semaphore_t       *Semaphore,
                                      unsigned int          Timeout);
void          OS_SemaphoreSignal(OS_Semaphore_t            *Semaphore);

// --------------------------------------------------------------
//      Read-write lock functions

// Lockdep requires the key to be statically allocated so must be a macro.
#define OS_InitializeRwLock(Lock) ( { \
    static OS_OpaqueLockClassKey_t __key; \
    __OS_InitializeRwLock(Lock, &__key, #Lock); \
} )
void    __OS_InitializeRwLock(OS_RwLock_t  *Lock, OS_OpaqueLockClassKey_t *Key, const char *Name);
void    OS_TerminateRwLock(OS_RwLock_t     *Lock);
void    OS_LockRead(OS_RwLock_t            *Lock);
void    OS_LockWrite(OS_RwLock_t           *Lock);
void    OS_UnLockRead(OS_RwLock_t          *Lock);
void    OS_UnLockWrite(OS_RwLock_t         *Lock);

// --------------------------------------------------------------
//      Mutex functions

// Lockdep requires the key to be statically allocated so must be a macro..
#define OS_InitializeMutex(Mutex) ( { \
    static OS_OpaqueLockClassKey_t __key; \
    __OS_InitializeMutex(Mutex, &__key, #Mutex); \
} )
void    __OS_InitializeMutex(OS_Mutex_t     *Mutex, OS_OpaqueLockClassKey_t *Key, const char *Name);
void    OS_TerminateMutex(OS_Mutex_t        *Mutex);
void    OS_LockMutex(OS_Mutex_t             *Mutex);
void    OS_UnLockMutex(OS_Mutex_t           *Mutex);

// Return non-zero if Mutex held by *any* thread.
int     OS_MutexIsLocked(OS_Mutex_t         *Mutex);

// Raise assert failure if Mutex is (not) held by *current* thread.
// Do nothing if underlying mutex does not provide such support or assertions
// turned off.
#define	OS_AssertMutexHeld(Mutex) \
	__OS_AssertMutexHeld((Mutex), __FILE__, __LINE__)
#define	OS_AssertMutexNotHeld(Mutex) \
	__OS_AssertMutexNotHeld((Mutex), __FILE__, __LINE__)
void	__OS_AssertMutexHeld(OS_Mutex_t *Mutex, const char *File, unsigned Line);
void	__OS_AssertMutexNotHeld(OS_Mutex_t *Mutex, const char *File, unsigned Line);

// --------------------------------------------------------------
//      RtMutex functions

void    OS_InitializeRtMutex(OS_RtMutex_t     *Mutex);
void    OS_TerminateRtMutex(OS_RtMutex_t      *Mutex);
void    OS_LockRtMutex(OS_RtMutex_t           *Mutex);
void    OS_UnLockRtMutex(OS_RtMutex_t         *Mutex);
int     OS_RtMutexIsLocked(OS_RtMutex_t       *Mutex);

// --------------------------------------------------------------
//      SpinLock functions

// Lockdep requires the key to be statically allocated so must be a macro..
#define OS_InitializeSpinLock(SpinLock) ( { \
    static OS_OpaqueLockClassKey_t __key; \
    __OS_InitializeSpinLock(SpinLock, &__key, #SpinLock); \
} )
void    __OS_InitializeSpinLock(OS_SpinLock_t  *SpinLock, OS_OpaqueLockClassKey_t *Key, const char *Name);
void    OS_TerminateSpinLock(OS_SpinLock_t  *SpinLock);
void    OS_LockSpinLock(OS_SpinLock_t  *SpinLock);
void    OS_UnLockSpinLock(OS_SpinLock_t  *SpinLock);
void    OS_LockSpinLockIRQ(OS_SpinLock_t  *SpinLock);
void    OS_UnLockSpinLockIRQ(OS_SpinLock_t  *SpinLock);
void    OS_LockSpinLockIRQSave(OS_SpinLock_t  *SpinLock);
void    OS_UnLockSpinLockIRQRestore(OS_SpinLock_t  *SpinLock);
int     OS_SpinLockIsLocked(OS_SpinLock_t  *SpinLock);

// --------------------------------------------------------------
//      Memory Barriers functions

void          OS_Smp_Mb(void);

// --------------------------------------------------------------
//      Event functions

// Lockdep requires the key to be statically allocated so must be a macro..
#define OS_InitializeEvent(Event) ( { \
    static OS_OpaqueLockClassKey_t __key; \
    __OS_InitializeEvent(Event, &__key, #Event); \
} )
void          __OS_InitializeEvent(OS_Event_t           *Event, OS_OpaqueLockClassKey_t *Key, const char *Name);
OS_Status_t   OS_WaitForEventNoInt(OS_Event_t           *Event,
                                   OS_Timeout_t          Timeout);
OS_Status_t   OS_WaitForEventInterruptible(OS_Event_t   *Event,
                                           OS_Timeout_t  Timeout);
OS_Status_t   OS_WaitForEventAuto(OS_Event_t   *Event,
                                  OS_Timeout_t  Timeout);
bool          OS_TestEventSet(OS_Event_t          *Event);
void          OS_SetEvent(OS_Event_t              *Event);
void          OS_SetEventFromIRQ(OS_Event_t       *Event);
void          OS_SetEventInterruptible(OS_Event_t *Event);
void          OS_ResetEvent(OS_Event_t            *Event);
void          OS_ReInitializeEvent(OS_Event_t     *Event);
void          OS_TerminateEvent(OS_Event_t        *Event);

// --------------------------------------------------------------
//      Thread functions

OS_Status_t   OS_CreateThread(OS_Thread_t         *Thread,
                              OS_TaskEntry_t       TaskEntry,
                              OS_TaskParam_t       Parameter,
                              const OS_TaskDesc_t *TaskDesc);
void          OS_TerminateThread(void);
OS_Status_t   OS_JoinThread(OS_Thread_t Thread);
char         *OS_ThreadName(void);
unsigned int  OS_ThreadID(void);
OS_Status_t   OS_SetPriority(const OS_TaskDesc_t *TaskDesc);

// --------------------------------------------------------------
//      Signal Interactions

int           OS_SignalPending(void);

// --------------------------------------------------------------
//      Time functions

void          OS_ReSchedule(void);
unsigned int  OS_GetTimeInSeconds(void);
unsigned int  OS_GetTimeInMilliSeconds(void);
unsigned long long  OS_GetTimeInMicroSeconds(void);
void          OS_SleepMilliSeconds(unsigned int           Value);

// --------------------------------------------------------------
//      Tunable functions

void          OS_RegisterTuneable(const char             *Name,
                                  unsigned int           *Address);
void         *OS_RegisterTuneableDir(const char          *Name,
                                     void                *Parent);
void          OS_UnregisterTuneable(const char           *Name);

// ----------------------------------------------------------------------------------------
//      Memory

void OS_Dump_MemCheckCounters(const char *str);

char *OS_StrDup(const char *String);

void *__builtin_new(size_t size);
void *__builtin_vec_new(size_t size);
void __builtin_delete(void *ptr);
void __builtin_vec_delete(void *ptr);
void __pure_virtual(void);
void __cxa_pure_virtual(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
#endif

#endif  // __cplusplus

#endif
