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

#include <stdio.h>

/* --- Include the os specific headers we will need --- */

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <errno.h>

/* At present none of the cache flush constants are available in the
 * user kernel headers. Until they make it that far we must
 * reproduce them ourselves.
 */
#define __NR_cacheflush         123
#define CACHEFLUSH_D_INVAL      0x1     /* invalidate (without write back) */
#define CACHEFLUSH_D_WB         0x2     /* write back (without invalidate) */
#define CACHEFLUSH_D_PURGE      0x3     /* writeback and invalidate */
#define CACHEFLUSH_I            0x4


#include <pthread.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <sys/time.h>

#include "report.h"

/* --- Define bool if we not in c++ --- */

#ifndef __cplusplus
#ifndef bool
#define bool    unsigned char
#endif
#ifndef false
#define false   0
#define true    1
#endif
#endif

/* --- Return codes --- */

#define OS_NO_ERROR             0
#define OS_ERROR                1
#define OS_TIMED_OUT            2
#define OS_NO_MESSAGE           3
#define OS_INTERRUPTED          4

//

#define OS_NO_PRIORITY          0xffffffff
/*
  #define OS_HIGHEST_PRIORITY     OS_NO_PRIORITY
  #define OS_MID_PRIORITY         OS_NO_PRIORITY
  #define OS_LOWEST_PRIORITY      OS_NO_PRIORITY
*/
#define OS_HIGHEST_PRIORITY     99
#define OS_MID_PRIORITY         50
#define OS_LOWEST_PRIORITY      0
#define OS_INFINITE             (~((OS_Timeout_t)0))
#define OS_INVALID_THREAD       (~((OS_Thread_t)0))

/* --- Useful type declarations --- */

typedef struct {
    pthread_cond_t          Cond;
    pthread_mutex_t         Mutex;
    bool                    Valid; /* SHARED, PROTECTED_BY: Mutex */
} OS_Event_t;

//

typedef int                  OS_Timeout_t;
typedef pthread_t            OS_Thread_t;
typedef void                *OS_TaskParam_t;
typedef struct {
    const char *name;
    int policy;
    int priority;
} OS_TaskDesc_t;
typedef int                  OS_Status_t;
typedef pthread_mutex_t      OS_Mutex_t;
typedef pthread_mutex_t      OS_SpinLock_t;
typedef pthread_rwlock_t     OS_RwLock_t;
typedef sem_t                OS_Semaphore_t;

typedef void  *(*OS_TaskEntry_t)(void *Parameter);
#define OS_TaskEntry( fn )   void  *fn( void* Parameter )

/* --- Useful macro's --- */

#undef min
#define min( a,b )                      (((a)<(b)) ? (a) : (b))

#undef max
#define max( a,b )                      (((a)<(b)) ? (b) : (a))

#undef inrange
#define inrange( val, lower, upper )    (((val)>=(lower)) && ((val)<=(upper)))

#undef Clamp
#define Clamp( v, l, u )                {if( v < l ) v = l; else if( v > u ) v = u;}

#ifndef unlikely
#define unlikely
#endif

/* --- Taskname table --- */

typedef struct OS_NameEntry_s {
    pthread_t            Thread;
    char                *Name;
} OS_NameEntry_t;

#define OS_MAX_TASKS    32

#ifdef OSINLINE_INSTANTIATE_DATA
OS_NameEntry_t          OS_TaskNameTable[OS_MAX_TASKS];
#else
extern OS_NameEntry_t   OS_TaskNameTable[OS_MAX_TASKS];
#endif

/*
 * If OSINLINE_ASSERT is not defined before inclusion of this file
 * if will fall back to a bare assert().
 */
#if !defined(OSINLINE_ASSERT)
#define OSINLINE_ASSERT(x) assert(x)
#endif

/*
 * If OSINLINE_INLINE is defined (the default) the implementation
 * will be inlined in modules including this file.
 * If OSINLINE_NOINLINE is defined either through compilation
 * flag or before any inclusion of osinline.h, the implementation
 * will not be inlined.
 * In order to provide support for the NOINLINE case one of the
 * compiled file must define OSINLINE_IN_IMPL and include this header.
 */
#if defined(OSINLINE_NOINLINE) && defined(OSINLINE_INLINE)
#error "OSINLINE_NOINLINE and OSINLINE_INLINE are incompatible"
#endif
#if !defined(OSINLINE_NOINLINE) && !defined(OSINLINE_INLINE)
#define OSINLINE_INLINE
#endif

#define OSINLINE_EXTERN_DECL_ extern
#define OSINLINE_EXTERN_IMPL_ /* global visibility */
#define OSINLINE_INLINE_DECL_ static inline
#define OSINLINE_INLINE_IMPL_ static inline
#if defined(OSINLINE_INLINE)
#define OSINLINE_DECL_ OSINLINE_INLINE_DECL_
#define OSINLINE_IMPL_ OSINLINE_INLINE_IMPL_
#else  /* !defined(OSINLINE_INLINE) */
#define OSINLINE_DECL_ OSINLINE_EXTERN_DECL_
#ifdef OSINLINE_IN_IMPL
#define OSINLINE_IMPL_ OSINLINE_EXTERN_IMPL_
#endif
#endif /* !defined(OSINLINE_INLINE) */

/* --- The actual functions declarations/implementations--- */

// --------------------------------------------------------------
// Malloc and free functions

OSINLINE_DECL_ void             *OS_Malloc(unsigned int             Size);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void             *OS_Malloc(unsigned int             Size) {
    /* Return NULL (failure) for 0-sized alloc. */
    if (Size == 0) {
        return NULL;
    }

    return malloc(Size);
}
#endif

// --------

OSINLINE_DECL_ void              OS_Free(void                    *Address);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void              OS_Free(void                    *Address) {
    /* Define NULL address as a no-op, same as kfree() does. */
    if (Address == NULL) {
        return;
    }

    free(Address);
}
#endif

// --------

/* UNTESTED_START: cache primitives not tested. */
/* TODO: is it really useful for SE, shouldn't it be removed? */
OSINLINE_DECL_ void OS_InvalidateCacheRange(void *CPUAddress, unsigned int size);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_InvalidateCacheRange(void *CPUAddress, unsigned int size) {
    syscall(__NR_cacheflush, CPUAddress, size, CACHEFLUSH_D_INVAL);
}
#endif

// --------

OSINLINE_DECL_ void OS_FlushCacheRange(void *CPUAddress, unsigned int size);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_FlushCacheRange(void *CPUAddress, unsigned int size) {
    syscall(__NR_cacheflush, CPUAddress, size, CACHEFLUSH_D_WB);
}
#endif

// --------

OSINLINE_DECL_ void OS_PurgeCacheRange(void *CPUAddress, unsigned int size);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_PurgeCacheRange(void *CPUAddress, unsigned int size) {
    syscall(__NR_cacheflush, CPUAddress, size, CACHEFLUSH_D_PURGE);
}
#endif

/* UNTESTED_STOP */

// --------------------------------------------------------------
//      Specialist intruction emulation functions

/* UNTESTED_START: builtins not tested. */

OSINLINE_DECL_ unsigned int      __swapbw(unsigned int a);        // ByteSwap
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ unsigned int      __swapbw(unsigned int a) {
    unsigned int tmp1 = (a << 8) & 0xFF00FF00;
    unsigned int tmp2 = (a >> 8) & 0x00FF00FF;
    unsigned int tmp3 = tmp1 | tmp2;

    return ((tmp3 >> 16) | (tmp3 << 16));
}
#endif

//

OSINLINE_DECL_ unsigned int      __lzcntw(unsigned int a);       // CountLeadingZeros
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ unsigned int      __lzcntw(unsigned int a) {
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

/* UNTESTED_STOP */

// --------------------------------------------------------------
//      The Mutex functions

OSINLINE_DECL_ void OS_InitializeMutex(OS_Mutex_t  *Mutex);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_InitializeMutex(OS_Mutex_t  *Mutex) {
    OSINLINE_ASSERT(Mutex != NULL);

    pthread_mutex_init(Mutex, NULL);
}
#endif

//

OSINLINE_DECL_ void OS_TerminateMutex(OS_Mutex_t  *Mutex);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_TerminateMutex(OS_Mutex_t  *Mutex) {
    OSINLINE_ASSERT(Mutex != NULL);

    pthread_mutex_destroy(Mutex);
}
#endif

//

OSINLINE_DECL_ void          OS_LockMutex(OS_Mutex_t  *Mutex);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void          OS_LockMutex(OS_Mutex_t  *Mutex) {
    int Status;
    OSINLINE_ASSERT(Mutex != NULL);

    Status = pthread_mutex_lock(Mutex);
    OSINLINE_ASSERT(Status == 0);
}
#endif

//

OSINLINE_DECL_ void          OS_UnLockMutex(OS_Mutex_t  *Mutex);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void          OS_UnLockMutex(OS_Mutex_t  *Mutex) {
    int Status;
    OSINLINE_ASSERT(Mutex != NULL);

    Status = pthread_mutex_unlock(Mutex);
    OSINLINE_ASSERT(Status == 0);
}
#endif

//

OSINLINE_DECL_ int           OS_MutexIsLocked(OS_Mutex_t  *Mutex);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ int           OS_MutexIsLocked(OS_Mutex_t  *Mutex) {
    OSINLINE_ASSERT(Mutex != NULL);

    /* Pthread does not provide such an interface.
       It is a valid implementation to always return false. */
    return 0;
}
#endif

OSINLINE_DECL_ void         OS_AssertMutexHeld(OS_Mutex_t *Mutex);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void         OS_AssertMutexHeld(OS_Mutex_t *Mutex) {
    /* Pthread does not provide such an interface so do nothing. */
}
#endif

OSINLINE_DECL_ void         OS_AssertMutexNotHeld(OS_Mutex_t *Mutex);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void         OS_AssertMutexNotHeld(OS_Mutex_t *Mutex) {
    /* Pthread does not provide such an interface so do nothing. */
}
#endif

OSINLINE_DECL_ void    OS_InitializeRwLock(OS_RwLock_t *Lock);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void    OS_InitializeRwLock(OS_RwLock_t *Lock) {
    int Status;

    Status = pthread_rwlock_init(Lock, NULL);
    OSINLINE_ASSERT(Status == 0);
}
#endif

OSINLINE_DECL_ void    OS_TerminateRwLock(OS_RwLock_t *Lock);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void    OS_TerminateRwLock(OS_RwLock_t *Lock) {
    int Status;

    Status = pthread_rwlock_destroy(Lock);
    OSINLINE_ASSERT(Status == 0);
}
#endif

OSINLINE_DECL_ void    OS_LockRead(OS_RwLock_t *Lock);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void    OS_LockRead(OS_RwLock_t *Lock) {
    int Status;

    Status = pthread_rwlock_rdlock(Lock);
    OSINLINE_ASSERT(Status == 0);
}
#endif

OSINLINE_DECL_ void    OS_LockWrite(OS_RwLock_t *Lock);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void    OS_LockWrite(OS_RwLock_t *Lock) {
    int Status;

    Status = pthread_rwlock_wrlock(Lock);
    OSINLINE_ASSERT(Status == 0);
}
#endif

OSINLINE_DECL_ void    OS_UnLockRead(OS_RwLock_t *Lock);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void    OS_UnLockRead(OS_RwLock_t *Lock) {
    int Status;

    Status = pthread_rwlock_unlock(Lock);
    OSINLINE_ASSERT(Status == 0);
}
#endif

OSINLINE_DECL_ void    OS_UnLockWrite(OS_RwLock_t *Lock);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void    OS_UnLockWrite(OS_RwLock_t *Lock) {
    int Status;

    Status = pthread_rwlock_unlock(Lock);
    OSINLINE_ASSERT(Status == 0);
}
#endif


OSINLINE_DECL_ void OS_Smp_Mb(void);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_Smp_Mb(void) {
#if defined(__GNUC__) && (__GNUC__ * 10 + __GNUC_MINOR__) >= 47
    /* Use GCC builtin for gcc4.7+. */
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#else
    /* Force yield, implies a barrier. */
    usleep(1);    /* UNTESTED: only tested for gcc 4.7+ */
#endif
}
#endif

#define OS_InitializeSpinLock       OS_InitializeMutex
#define OS_TerminateSpinLock        OS_TerminateMutex
#define OS_LockSpinLockIRQSave      OS_LockMutex
#define OS_UnLockSpinLockIRQ        OS_UnLockMutex
#define OS_UnLockSpinLockIRQRestore OS_UnLockMutex
#define OS_LockSpinLock             OS_LockMutex
#define OS_UnLockSpinLock           OS_UnLockMutex

// --------------------------------------------------------------
//      The Event functions

OSINLINE_DECL_ OS_Status_t   OS_InitializeEvent(OS_Event_t  *Event);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ OS_Status_t   OS_InitializeEvent(OS_Event_t  *Event) {
    int result;
    OSINLINE_ASSERT(Event != 0);

    result = pthread_cond_init(&Event->Cond,  NULL);
    if (result != 0) { goto cond_fail; }
    result = pthread_mutex_init(&Event->Mutex, NULL);
    if (result != 0) { goto mutex_fail; }
    Event->Valid   = false;
    return OS_NO_ERROR;

mutex_fail:
    pthread_cond_destroy(&Event->Cond);
cond_fail:
    return OS_ERROR;
}
#endif

//

#define OS_WaitForEventInterruptible OS_WaitForEvent
#define OS_WaitForEventAuto OS_WaitForEvent
#define OS_SetEventInterruptible OS_SetEvent

OSINLINE_DECL_ OS_Status_t   OS_WaitForEvent(OS_Event_t *Event, OS_Timeout_t Timeout);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ OS_Status_t   OS_WaitForEvent(OS_Event_t *Event, OS_Timeout_t Timeout) {
    int result;
    struct timespec ts;
    OSINLINE_ASSERT(Event != 0);

    if (Timeout != OS_INFINITE) {
        result = clock_gettime(CLOCK_REALTIME, &ts);
        OSINLINE_ASSERT(result == 0);
        /* Timeout is specified in milliseconds. */
        ts.tv_sec += Timeout / 1000;
        ts.tv_nsec += (Timeout % 1000) * 1000000;
        ts.tv_sec += ts.tv_nsec / 1000000000;
        ts.tv_nsec = ts.tv_nsec % 1000000000;
    }

    pthread_mutex_lock(&Event->Mutex);
    result = 0;
    while (!Event->Valid) {
        if (Timeout == OS_INFINITE) {
            result = pthread_cond_wait(&Event->Cond, &Event->Mutex);
        } else {
            result = pthread_cond_timedwait(&Event->Cond, &Event->Mutex, &ts);
        }
        if (result != 0) { break; }
    }
    pthread_mutex_unlock(&Event->Mutex);
    OSINLINE_ASSERT(result == 0 || result == ETIMEDOUT);

    return result == 0 ? OS_NO_ERROR : OS_TIMED_OUT;
}
#endif

//

OSINLINE_DECL_ bool OS_TestEventSet(OS_Event_t *Event);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ bool OS_TestEventSet(OS_Event_t *Event) {
    bool Valid;
    OSINLINE_ASSERT(Event != 0);

    pthread_mutex_lock(&Event->Mutex);
    Valid = Event->Valid;
    pthread_mutex_unlock(&Event->Mutex);
    return Valid;
}
#endif

//

OSINLINE_DECL_ void OS_SetEvent(OS_Event_t *Event);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_SetEvent(OS_Event_t *Event) {
    OSINLINE_ASSERT(Event != 0);

    pthread_mutex_lock(&Event->Mutex);
    if (Event->Valid == false) {
        Event->Valid = true;
        pthread_cond_broadcast(&Event->Cond);
    }
    pthread_mutex_unlock(&Event->Mutex);
}
#endif

//

OSINLINE_DECL_ void OS_ResetEvent(OS_Event_t *Event);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_ResetEvent(OS_Event_t *Event) {
    OSINLINE_ASSERT(Event != 0);

    pthread_mutex_lock(&Event->Mutex);
    Event->Valid = false;
    pthread_mutex_unlock(&Event->Mutex);
}
#endif

//

OSINLINE_DECL_ void OS_TerminateEvent(OS_Event_t  *Event);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_TerminateEvent(OS_Event_t  *Event) {
    OSINLINE_ASSERT(Event != 0);

    pthread_mutex_destroy(&Event->Mutex);
    pthread_cond_destroy(&Event->Cond);
}
#endif

// --------------------------------------------------------------
//      The Thread functions

OSINLINE_DECL_ OS_Status_t   OS_CreateThread(OS_Thread_t     *Thread,
                                             OS_TaskEntry_t   TaskEntry,
                                             OS_TaskParam_t   Parameter,
                                             OS_TaskDesc_t   *TaskDesc);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ OS_Status_t   OS_CreateThread(OS_Thread_t     *Thread,
                                             OS_TaskEntry_t   TaskEntry,
                                             OS_TaskParam_t   Parameter,
                                             OS_TaskDesc_t   *TaskDesc) {
    unsigned int        FreeEntry;
    int                 Status;

    /* UNTESTED_START: test code. */
    if (TaskDesc == NULL) {
        return OS_ERROR;
    }
    /* UNTESTED_STOP */

    *Thread = OS_INVALID_THREAD;

    for (FreeEntry = 0; FreeEntry < OS_MAX_TASKS; FreeEntry++)
        if (OS_TaskNameTable[FreeEntry].Name == NULL) {
            break;
        }

    if (FreeEntry == OS_MAX_TASKS) {
        return OS_ERROR;
    }

    OS_TaskNameTable[FreeEntry].Name    = strdup(TaskDesc->name);

    Status = pthread_create(&OS_TaskNameTable[FreeEntry].Thread, NULL, TaskEntry, Parameter);
    if (Status != 0) {
        free(OS_TaskNameTable[FreeEntry].Name);
        OS_TaskNameTable[FreeEntry].Name        = NULL;
        return OS_ERROR;
    }

    /* Should not be detached as we want to support JoinThread(). */
    /* pthread_detach( OS_TaskNameTable[FreeEntry].Thread ); */

    *Thread = OS_TaskNameTable[FreeEntry].Thread;
    return OS_NO_ERROR;
}
#endif

//

OSINLINE_DECL_ void   OS_TerminateThread(void);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void   OS_TerminateThread(void) {
    unsigned int i;
    static int   OS_ExitValue = 0;

    for (i = 0; i < OS_MAX_TASKS; i++)
        if ((OS_TaskNameTable[i].Name != NULL) && pthread_equal(OS_TaskNameTable[i].Thread, pthread_self())) {
            free(OS_TaskNameTable[i].Name);
            OS_TaskNameTable[i].Name    = NULL;
        }

    pthread_exit((void *)&OS_ExitValue);
}
#endif

//

OSINLINE_DECL_ OS_Status_t   OS_JoinThread(OS_Thread_t     Thread);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ OS_Status_t   OS_JoinThread(OS_Thread_t     Thread) {
    int Status;
    void *ReturnCode;

    Status = pthread_join(Thread, &ReturnCode);
    (void)ReturnCode; /* UNUSED */

    return Status == 0 ? OS_NO_ERROR : OS_ERROR;
}
#endif

//

OSINLINE_DECL_ char   *OS_ThreadName(void);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ char   *OS_ThreadName(void) {
    unsigned int        i;

    for (i = 0; i < OS_MAX_TASKS; i++) {
        if ((OS_TaskNameTable[i].Name != NULL) && pthread_equal(OS_TaskNameTable[i].Thread, pthread_self())) {
            return OS_TaskNameTable[i].Name;
        }
    }


    return (char *) "Main Thread";
}
#endif

OSINLINE_DECL_ unsigned int OS_ThreadID(void);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ unsigned int OS_ThreadID(void) {
    return (unsigned int) pthread_self();
}
#endif

OSINLINE_DECL_ OS_Status_t OS_SetPriority(OS_TaskDesc_t  *TaskDesc);
#ifdef OSINLINE_IMPL_
OSINLINE_DECL_ OS_Status_t OS_SetPriority(OS_TaskDesc_t  *TaskDesc) {
    /* No-op in this implementation. */
    return OS_NO_ERROR;
}
#endif

// --------------------------------------------------------------
//      Semaphore functions

OSINLINE_DECL_ void          OS_SemaphoreInitialize(OS_Semaphore_t  *Semaphore,
                                                    unsigned int     InitialCount);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void          OS_SemaphoreInitialize(OS_Semaphore_t  *Semaphore,
                                                    unsigned int     InitialCount) {
    OSINLINE_ASSERT(Semaphore != NULL);

    sem_init(Semaphore, 0, InitialCount);
}
#endif

OSINLINE_DECL_ void          OS_SemaphoreTerminate(OS_Semaphore_t  *Semaphore);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void          OS_SemaphoreTerminate(OS_Semaphore_t  *Semaphore) {
    OSINLINE_ASSERT(Semaphore != NULL);

    sem_destroy(Semaphore);
}
#endif

OSINLINE_DECL_ void OS_SemaphoreWait(OS_Semaphore_t  *Semaphore);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_SemaphoreWait(OS_Semaphore_t  *Semaphore) {
    int result;
    OSINLINE_ASSERT(Semaphore != NULL);

    /* This is a non interruptible wait.
       Hence restarts while interrupted by signal. */
    do {
        result = sem_wait(Semaphore);
    } while (result != 0 && errno == EINTR);
    OSINLINE_ASSERT(result == 0);
}
#endif

#define OS_SemaphoreWaitAuto OS_SemaphoreWait

OSINLINE_DECL_ void OS_SemaphoreSignal(OS_Semaphore_t  *Semaphore);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_SemaphoreSignal(OS_Semaphore_t  *Semaphore) {
    int result;
    OSINLINE_ASSERT(Semaphore != NULL);

    result = sem_post(Semaphore);
    OSINLINE_ASSERT(result == 0);
}
#endif

OSINLINE_DECL_ OS_Status_t OS_SemaphoreWaitTimeout(OS_Semaphore_t  *Semaphore, unsigned int Timeout);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ OS_Status_t OS_SemaphoreWaitTimeout(OS_Semaphore_t  *Semaphore, unsigned int Timeout) {
    int result;
    struct timespec ts;
    OSINLINE_ASSERT(Semaphore != NULL);

    result = clock_gettime(CLOCK_REALTIME, &ts);
    OSINLINE_ASSERT(result == 0);
    /* Timeout is specified in milliseconds. */
    ts.tv_sec += Timeout / 1000;
    ts.tv_nsec += (Timeout % 1000) * 1000000;
    ts.tv_sec += ts.tv_nsec / 1000000000;
    ts.tv_nsec = ts.tv_nsec % 1000000000;

    /* Restart while interrupted by signal. */
    do {
        result = sem_timedwait(Semaphore, &ts);
    } while (result != 0 && errno == EINTR);

    OSINLINE_ASSERT(result == 0 || errno == ETIMEDOUT);

    return result == 0 ? OS_NO_ERROR : OS_TIMED_OUT;
}
#endif

OSINLINE_DECL_ OS_Status_t OS_SemaphoreWaitInterruptible(OS_Semaphore_t  *Semaphore);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ OS_Status_t OS_SemaphoreWaitInterruptible(OS_Semaphore_t  *Semaphore) {
    int result;
    OSINLINE_ASSERT(Semaphore != NULL);

    result = sem_wait(Semaphore);
    OSINLINE_ASSERT(result == 0 || errno == EINTR);

    return result == 0 ? OS_NO_ERROR : OS_INTERRUPTED;
}
#endif

// --------------------------------------------------------------
//      The Miscelaneous functions

OSINLINE_DECL_ void OS_ReSchedule(void);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_ReSchedule(void) {
    usleep(10);
}
#endif

OSINLINE_DECL_ unsigned int OS_GetTimeInSeconds(void);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ unsigned int OS_GetTimeInSeconds(void) {
    struct timeval  Now;

    gettimeofday(&Now, NULL);
    return Now.tv_sec;
}
#endif

OSINLINE_EXTERN_DECL_ unsigned int OS_GetTimeInMilliSeconds(void);

OSINLINE_EXTERN_DECL_ unsigned long long OS_GetTimeInMicroSeconds(void);

OSINLINE_DECL_ void OS_SleepMilliSeconds(unsigned int Value);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_SleepMilliSeconds(unsigned int Value) {
    /* Enforce yield as usleep(0) would imply a no-op.  */
    if (Value == 0) {
        OS_ReSchedule();
    } else {
        usleep(Value * 1000);
    }
}
#endif

OSINLINE_DECL_ void OS_RegisterTuneable(const char *Name, unsigned int *Address);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_RegisterTuneable(const char *Name, unsigned int *Address) {
    /* UNTESTED_START: test code. */
    // tweak the tuneables to fit the framework better
    if (0 == strcmp("fatal_error_on_assertfail", Name)) {
        // Eventually the assertion failures will be turned on by
        // default. Thus when this assert starts to fire then we can
        // simply remove this whole branch.
        assert(0 == *Address);
        *Address = 1;
    }
    /* UNTESTED_STOP */
}
#endif

OSINLINE_DECL_ void OS_UnregisterTuneable(const char *Name);
#ifdef OSINLINE_IMPL_
OSINLINE_IMPL_ void OS_UnregisterTuneable(const char *Name) {
}
#endif

#ifdef __cplusplus
// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName)      \
    TypeName(const TypeName&);                  \
    void operator=(const TypeName&)
#endif
#endif



#endif
