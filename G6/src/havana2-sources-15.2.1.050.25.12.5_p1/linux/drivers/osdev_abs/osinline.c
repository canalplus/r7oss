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

#include <linux/debugfs.h>
#include <linux/smp.h>
#include <linux/bug.h>
#include <linux/rwsem.h>

#include "osdev_mem.h"
#include "osdev_cache.h"
#include "osdev_time.h"
#include "osdev_sched.h"

#include "osinline.h"

/* --- memory activity tracking --- */
void OS_Dump_MemCheckCounters(const char *str)
{
	return OSDEV_Dump_MemCheckCounters(str);
}

/* --- Taskname table --- */

#define OS_MAX_NAME_SIZE    32

typedef struct OS_NameEntry_s {
	OSDEV_Thread_t       Thread;
	char                 Name[OS_MAX_NAME_SIZE];
} OS_NameEntry_t;

#define OS_MAX_TASKS    128

static OS_NameEntry_t          OS_TaskNameTable[OS_MAX_TASKS];

/* --- Tuneable entries --- */
#ifdef CONFIG_DEBUG_FS
typedef struct OS_TuneableEntry_s {
	struct dentry       *Dentry;
	char                 Name[OS_MAX_NAME_SIZE];
} OS_TuneableEntry_t;

static struct dentry       *TuneableRoot = NULL;
#define OS_MAX_TUNEABLES    128
static OS_TuneableEntry_t   Tuneable[OS_MAX_TUNEABLES];
#endif  // DEBUG_FS

// --------------------------------------------------------------
// Memory functions

void *OS_Malloc(unsigned int Size)
{
	return OSDEV_Malloc(Size);
}

void OS_Free(void *Address)
{
	return OSDEV_Free(Address);
}

void OS_Smp_Mb(void)
{
	// full r/w memory barrier
	smp_mb();
}

// --------------------------------------------------------------
// Cache functions

void OS_FlushCacheAll(void)
{
	OSDEV_FlushCacheAll();
}

void OS_FlushCacheRange(void *CPUAddress, OS_PhysAddr_t Physical, unsigned int size)
{
	OSDEV_FlushCacheRange(CPUAddress, Physical, size);
}

void OS_InvCacheRange(void *CPUAddress, OS_PhysAddr_t Physical, unsigned int size)
{
	OSDEV_InvCacheRange(CPUAddress, Physical, size);
}

void OS_PurgeCacheRange(void *CPUAddress, OS_PhysAddr_t Physical, unsigned int size)
{
	OSDEV_PurgeCacheRange(CPUAddress, Physical, size);
}

// --------------------------------------------------------------
// Semaphore functions

void OS_SemaphoreInitialize(OS_Semaphore_t  *Semaphore,
                            unsigned int     InitialCount)
{
	BUILD_BUG_ON(sizeof(struct semaphore) > sizeof(OS_Semaphore_t));

	sema_init((struct semaphore *)Semaphore, InitialCount);
}

void OS_SemaphoreTerminate(OS_Semaphore_t  *Semaphore)
{
	// nothing to do
}

void OS_SemaphoreWaitNoInt(OS_Semaphore_t  *Semaphore)
{
	down((struct semaphore *)Semaphore);
}

OS_Status_t   OS_SemaphoreWaitInterruptible(OS_Semaphore_t *Semaphore)
{
	int res = down_interruptible((struct semaphore *)Semaphore);
	return (res == 0) ? OS_NO_ERROR : OS_INTERRUPTED;
}

OS_Status_t   OS_SemaphoreWaitAuto(OS_Semaphore_t          *Semaphore)
{
	OS_Status_t WaitStatus = OS_SemaphoreWaitInterruptible(Semaphore);
	if (WaitStatus == OS_INTERRUPTED) {
		// auto mode means hack mode for the time being
		// for codes that can not handle return from wait interrupt:
		// switch to non interruptible wait..
		SE_WARNING("switching from interrupt to non interrupt wait\n");
		OS_SemaphoreWaitNoInt(Semaphore);
		WaitStatus = OS_NO_ERROR;
	}
	return WaitStatus;
}

OS_Status_t   OS_SemaphoreWaitTimeout(OS_Semaphore_t  *Semaphore,
                                      unsigned int     Timeout)
{
	int res = down_timeout((struct semaphore *)Semaphore, msecs_to_jiffies(Timeout));
	return (res == 0) ? OS_NO_ERROR : OS_TIMED_OUT;
}

void OS_SemaphoreSignal(OS_Semaphore_t *Semaphore)
{
	up((struct semaphore *)Semaphore);
}

// --------------------------------------------------------------
// RtMutex functions

void OS_InitializeRtMutex(OS_RtMutex_t  *RtMutex)
{
	BUILD_BUG_ON(sizeof(struct rt_mutex) > sizeof(OS_RtMutex_t));
	rt_mutex_init((struct rt_mutex *)RtMutex);
}

void OS_TerminateRtMutex(OS_RtMutex_t   *RtMutex)
{
	// check that at terminate time lock is released
	rt_mutex_destroy((struct rt_mutex *)RtMutex);
}

void OS_LockRtMutex(OS_RtMutex_t    *RtMutex)
{
	rt_mutex_lock((struct rt_mutex *)RtMutex);
}

void OS_UnLockRtMutex(OS_RtMutex_t  *RtMutex)
{
	rt_mutex_unlock((struct rt_mutex *)RtMutex);
}

int OS_RtMutexIsLocked(OS_RtMutex_t *RtMutex)
{
	return rt_mutex_is_locked((struct rt_mutex *)RtMutex);
}

// --------------------------------------------------------------
// Mutex functions

// Do not call directly.  Use OS_InitializeMutex() instead.
void __OS_InitializeMutex(OS_Mutex_t  *Mutex, OS_OpaqueLockClassKey_t *Key, const char *Name)
{
#ifdef CONFIG_LOCKDEP
	// Ensure our C++-compatible version of lock_class_key is big enough
	BUILD_BUG_ON(sizeof(struct lock_class_key) > sizeof(OS_OpaqueLockClassKey_t));
#endif
	BUILD_BUG_ON(sizeof(struct mutex) > sizeof(OS_Mutex_t));

	mutex_init((struct mutex *)Mutex);
	lockdep_set_class_and_name((struct mutex *)Mutex, (struct lock_class_key *)Key, Name);
}

void OS_TerminateMutex(OS_Mutex_t  *Mutex)
{
	// check that at terminate time lock is released
	mutex_destroy((struct mutex *)Mutex);
}

void OS_LockMutex(OS_Mutex_t    *Mutex)
{
	mutex_lock((struct mutex *)Mutex);
}

void OS_UnLockMutex(OS_Mutex_t  *Mutex)
{
	mutex_unlock((struct mutex *)Mutex);
}

int OS_MutexIsLocked(OS_Mutex_t *Mutex)
{
	return mutex_is_locked((struct mutex *)Mutex);
}

/*
 * Mutex assertions.
 *
 * The implementation of mutex assertions depends on the enabled kernel
 * features:
 *
 * - The preferred option is to use lockdep facilities.  This avoids relying on
 *   kernel internals.
 *
 * - Unfortunately we hardly ever enable lockdep.  So the fallback is to have a
 *   peek at mutex internals and test struct mutex::owner if it is enabled in
 *   kernel.  If the current thread owns the mutex, owner == current.  If the
 *   current thread does not own the mutex, we can not rely on owner to know the
 *   exact state of the mutex because there are no memory barriers protecting
 *   access to it in mutex_[un]lock().  However we have the guarantee that owner
 *   != current because only the current thread can set it to current and reset
 *   it from current to NULL.  Note: These invariants do not hold when lockdep
 *   is enabled.
 *
 * - Finally, when the kernel does not enable mutex::owner, for example in
 *   single core builds without kernel-level mutex debugging, these assertions
 *   turn into no-op.
 */
void __OS_AssertMutexHeld(OS_Mutex_t *Mutex, const char *File, unsigned Line)
{
	struct mutex *m = (struct mutex *)Mutex;

#if defined(CONFIG_LOCKDEP)
	lockdep_assert_held(m);
#elif defined(CONFIG_DEBUG_MUTEXES) || defined(CONFIG_SMP)
	struct task_struct *owner = m->owner;
	struct task_struct *curtd = current;

	/*
	 * TODO(theryn): A false positive was observed (bug 61416).  Should this
	 * occur again, inspect dumped task pointers to figure out if the
	 * following test is flawed.
	 */
	if (owner != curtd) {
		/*
		 * It is tempting to print owner->comm and owner->pid here but
		 * owner could have released the mutex and died by the time we
		 * do this.
		 */
		SE_FATAL("current thread should hold mutex @ %s:%u (owner=0x%p current=0x%p)\n",
		         File, Line, owner, curtd);
	}
#else
	(void)m;
#endif
}

void __OS_AssertMutexNotHeld(OS_Mutex_t *Mutex, const char *File, unsigned Line)
{
	struct mutex *m = (struct mutex *)Mutex;

#if defined(CONFIG_LOCKDEP)
	/*
	 * The kernel does not expose lockdep_assert_not_held() so roll out our
	 * own version.
	 */
	WARN_ON(debug_locks && lockdep_is_held(m));
#elif (defined(CONFIG_DEBUG_MUTEXES) || defined(CONFIG_SMP))
	struct task_struct *owner = m->owner;

	if (owner == current) {
		SE_FATAL("current thread should not hold mutex @ %s:%u\n", File, Line);
	}
#else
	(void)m;
#endif
}

// --------------------------------------------------------------
// Read-write lock functions

void __OS_InitializeRwLock(OS_RwLock_t *RwLock, OS_OpaqueLockClassKey_t *Key, const char *Name)
{
	BUILD_BUG_ON(sizeof(struct rw_semaphore) > sizeof(OS_RwLock_t));

	init_rwsem((struct rw_semaphore *)RwLock);
	lockdep_set_class_and_name((struct rw_semaphore *)RwLock, (struct lock_class_key *)Key, Name);
}

void OS_TerminateRwLock(OS_RwLock_t *RwLock)
{
	/* The kernel exposes no destructor. */
}

void OS_LockRead(OS_RwLock_t *RwLock)
{
	down_read((struct rw_semaphore *)RwLock);
}

void OS_LockWrite(OS_RwLock_t *RwLock)
{
	down_write((struct rw_semaphore *)RwLock);
}

void OS_UnLockRead(OS_RwLock_t *RwLock)
{
	up_read((struct rw_semaphore *)RwLock);
}

void OS_UnLockWrite(OS_RwLock_t *RwLock)
{
	up_write((struct rw_semaphore *)RwLock);
}

// --------------------------------------------------------------
// SpinLock functions

// Do not call directly.  Use OS_InitializeSpinLock() instead.
void __OS_InitializeSpinLock(OS_SpinLock_t  *SpinLock, OS_OpaqueLockClassKey_t *Key, const char *Name)
{
	BUILD_BUG_ON(sizeof(spinlock_t) > sizeof(SpinLock->data));
	spin_lock_init((spinlock_t *)&SpinLock->data);
	lockdep_set_class_and_name((spinlock_t *)&SpinLock->data, (struct lock_class_key *)Key, Name);
}

void OS_TerminateSpinLock(OS_SpinLock_t  *SpinLock)
{
	// deleting a locked spinlock is not allowed. if you don't *know* it is unlocked and
	// that it will never be locked then you are committing a use-after-free ;-)
	BUG_ON(OS_SpinLockIsLocked(SpinLock));
}

void OS_LockSpinLock(OS_SpinLock_t  *SpinLock)
{
	spin_lock((spinlock_t *)&SpinLock->data);
}

void OS_UnLockSpinLock(OS_SpinLock_t  *SpinLock)
{
	spin_unlock((spinlock_t *)&SpinLock->data);
}

void OS_LockSpinLockIRQ(OS_SpinLock_t  *SpinLock)
{
	spin_lock_irq((spinlock_t *)&SpinLock->data);
}

void OS_UnLockSpinLockIRQ(OS_SpinLock_t  *SpinLock)
{
	spin_unlock_irq((spinlock_t *)&SpinLock->data);
}

void OS_LockSpinLockIRQSave(OS_SpinLock_t  *SpinLock)
{
	unsigned long flags_irq;
	spin_lock_irqsave((spinlock_t *)&SpinLock->data, flags_irq);
	SpinLock->flags_irq = flags_irq;
}

void OS_UnLockSpinLockIRQRestore(OS_SpinLock_t  *SpinLock)
{
	unsigned long flags_irq = SpinLock->flags_irq;
	spin_unlock_irqrestore((spinlock_t *)&SpinLock->data, flags_irq);
}

int OS_SpinLockIsLocked(OS_SpinLock_t       *SpinLock)
{
	return spin_is_locked((spinlock_t *)&SpinLock->data);
}

// -----------------------------------------------------------------------------------------------
// event functions

// Do not call directly.  Use OS_InitializeEvent() instead.
void __OS_InitializeEvent(OS_Event_t *Event, OS_OpaqueLockClassKey_t *Key, const char *Name)
{
	BUILD_BUG_ON(sizeof(wait_queue_head_t) > sizeof(OS_WaitQueueHead_t));

	__OS_InitializeSpinLock(&Event->Lock, Key, Name);
	init_waitqueue_head((wait_queue_head_t *)&Event->Queue);
	Event->Valid  = false;

	OS_Smp_Mb();
}

void OS_TerminateEvent(OS_Event_t *Event)
{
	OS_TerminateSpinLock(&Event->Lock);
	// TODO could check no pending wait for queue ?
}

OS_Status_t OS_WaitForEventNoInt(OS_Event_t *Event, OS_Timeout_t Timeout)
{
	OS_Status_t Status = OS_NO_ERROR;
	int RemJiff;

	/* Ensure any writes from before SetEvent() are visible to this CPU */
	OS_Smp_Mb();
	OS_LockSpinLockIRQSave(&Event->Lock);

	if (!Event->Valid) {
		OS_UnLockSpinLockIRQRestore(&Event->Lock);

		if (Timeout == OS_INFINITE) {
			wait_event((*(wait_queue_head_t *)&Event->Queue), Event->Valid);
		} else {
			RemJiff = wait_event_timeout((*(wait_queue_head_t *)&Event->Queue), Event->Valid,
			                             msecs_to_jiffies((unsigned int)Timeout));
			Status = (RemJiff > 0) ? OS_NO_ERROR : OS_TIMED_OUT;
		}

		/* valid could be set after unlock and thus WaitForQueue would not sleep and return
		   - if sleeping a full memory barrier will happen by wait_event during a schedule*/
		OS_Smp_Mb();

		OS_LockSpinLockIRQSave(&Event->Lock);
		/* Protective sanity error check - if this fires something maybe wrong */
		if ((Status == OS_NO_ERROR) && !Event->Valid) {
			SE_WARNING("woke up (no timeout) but the condition is still false\n");
		}

		if (Status != OS_NO_ERROR && Event->Valid) {
			// overwrite in case of time out
			Status = OS_NO_ERROR;
		}
		OS_UnLockSpinLockIRQRestore(&Event->Lock);

		return Status;
	} else {
		OS_UnLockSpinLockIRQRestore(&Event->Lock);
		return OS_NO_ERROR;
	}
}

OS_Status_t __must_check OS_WaitForEventInterruptible(OS_Event_t *Event, OS_Timeout_t Timeout)
{
	OS_Status_t Status = OS_NO_ERROR;
	int RemJiff;
	int res;

	/* Ensure any writes from before SetEvent() are visible to this CPU */
	OS_Smp_Mb();
	OS_LockSpinLockIRQSave(&Event->Lock);

	if (!Event->Valid) {
		OS_UnLockSpinLockIRQRestore(&Event->Lock);

		if (Timeout == OS_INFINITE) {
			res = wait_event_interruptible((*(wait_queue_head_t *)&Event->Queue), Event->Valid);
			if (res != 0) {
				// -ERESTARTSYS received
				Status = OS_INTERRUPTED;
			}
		} else {
			RemJiff = wait_event_interruptible_timeout((*(wait_queue_head_t *)&Event->Queue), Event->Valid,
			                                           msecs_to_jiffies((unsigned int)Timeout));
			if (RemJiff == -ERESTARTSYS) {
				Status = OS_INTERRUPTED;
			} else if (RemJiff == 0) {
				Status = OS_TIMED_OUT;
			}
		}

		/* valid could be set after unlock and thus WaitForQueue would not sleep and return
		   - if sleeping a full memory barrier will happen by wait_event during a schedule*/
		OS_Smp_Mb();

		if (Status != OS_NO_ERROR && Event->Valid) {
			// overwrite in case of time out or interrupt
			Status = OS_NO_ERROR;
		}

		return Status;
	} else {
		OS_UnLockSpinLockIRQRestore(&Event->Lock);
		return OS_NO_ERROR;
	}
}

OS_Status_t OS_WaitForEventAuto(OS_Event_t   *Event,
                                OS_Timeout_t  Timeout)
{
	OS_Status_t WaitStatus = OS_WaitForEventInterruptible(Event, Timeout);
	if (WaitStatus == OS_INTERRUPTED) {
		// auto mode means hack mode for the time being
		// for codes that can not handle return from wait interrupt:
		// switch to non interruptible wait..
		SE_WARNING("switching from interrupt to non interrupt wait\n");
		WaitStatus = OS_WaitForEventNoInt(Event, Timeout);
	}
	return WaitStatus;
}

bool OS_TestEventSet(OS_Event_t *Event)
{
	OS_Smp_Mb();
	return Event->Valid;
}

void OS_SetEvent(OS_Event_t *Event)
{
	/*ensure any writes before SetEvent are written before waking
	  up the other task */
	OS_Smp_Mb();

	OS_LockSpinLockIRQSave(&Event->Lock);

	Event->Valid = true;

	/* no barrier is required here as WakeUp will have an implicit memory barrier */
	wake_up((wait_queue_head_t *)&Event->Queue);

	OS_UnLockSpinLockIRQRestore(&Event->Lock);
}

void OS_SetEventFromIRQ(OS_Event_t *Event)
{
	/*ensure any writes before SetEvent are written before waking
	  up the other task */
	OS_Smp_Mb();

	OS_LockSpinLock(&Event->Lock);

	Event->Valid = true;

	/* no barrier is required here as WakeUp will have an implicit memory barrier */
	wake_up((wait_queue_head_t *)&Event->Queue);

	OS_UnLockSpinLock(&Event->Lock);
}

void OS_SetEventInterruptible(OS_Event_t *Event)
{
	/*ensure any writes before SetEvent are written before waking
	  up the other task */
	OS_Smp_Mb();

	OS_LockSpinLockIRQSave(&Event->Lock);

	Event->Valid = true;
	wake_up_interruptible((wait_queue_head_t *)&Event->Queue);

	OS_UnLockSpinLockIRQRestore(&Event->Lock);
}

void OS_ResetEvent(OS_Event_t *Event)
{
	OS_LockSpinLockIRQSave(&Event->Lock);

	Event->Valid = false;

	OS_UnLockSpinLockIRQRestore(&Event->Lock);
}

void OS_ReInitializeEvent(OS_Event_t *Event)
{
	OS_LockSpinLockIRQSave(&Event->Lock);

	Event->Valid = false;
	init_waitqueue_head((wait_queue_head_t *)&Event->Queue);

	OS_UnLockSpinLockIRQRestore(&Event->Lock);
}

// --------------------------------------------------------------
// Thread functions

OS_Status_t   OS_CreateThread(OS_Thread_t         *Thread,
                              OS_TaskEntry_t       TaskEntry,
                              OS_TaskParam_t       Parameter,
                              const OS_TaskDesc_t *TaskDesc)
{
	unsigned int        FreeEntry;
	OSDEV_Status_t      Status;
	OSDEV_ThreadDesc_t  ThDesc;

	if (TaskDesc == NULL) {
		return OS_ERROR;
	}

	for (FreeEntry = 0; FreeEntry < OS_MAX_TASKS; FreeEntry++)
		if (OS_TaskNameTable[FreeEntry].Name[0] == '\0') {
			break;
		}

	if (FreeEntry == OS_MAX_TASKS) {
		pr_err("Error: %s No more thread entry available for %s\n", __func__, TaskDesc->name);
		return OS_ERROR;
	}

	strncpy(OS_TaskNameTable[FreeEntry].Name, TaskDesc->name, sizeof(OS_TaskNameTable[FreeEntry].Name));
	OS_TaskNameTable[FreeEntry].Name[sizeof(OS_TaskNameTable[FreeEntry].Name) - 1] = '\0';

	ThDesc.name     = TaskDesc->name;
	ThDesc.policy   = TaskDesc->policy;
	ThDesc.priority = TaskDesc->priority;
	Status = OSDEV_CreateThread(&(OS_TaskNameTable[FreeEntry].Thread),
	                            (OSDEV_ThreadFn_t)TaskEntry,
	                            (OSDEV_ThreadParam_t)Parameter,
	                            &ThDesc);
	if (Status != OSDEV_NoError) {
		OS_TaskNameTable[FreeEntry].Name[0] = '\0';
		return OS_ERROR;
	}

	*Thread = OS_TaskNameTable[FreeEntry].Thread;
	_DUMPMEMCHK_UP(DMC_THREAD, OS_TaskNameTable[FreeEntry].Name, Thread);
	return OS_NO_ERROR;
}

void   OS_TerminateThread(void)
{
	unsigned int        i;

	for (i = 0; i < OS_MAX_TASKS; i++)
		if ((OS_TaskNameTable[i].Name[0] != '\0') && (OS_TaskNameTable[i].Thread == current)) {
			_DUMPMEMCHK_DOWN(DMC_THREAD, "thread", current);
			OS_TaskNameTable[i].Name[0] = '\0';
		}
}

OS_Status_t OS_SetPriority(const OS_TaskDesc_t *TaskDesc)
{
	OSDEV_ThreadDesc_t ThDesc;
	if (TaskDesc == NULL) {
		return OS_ERROR;
	}

	ThDesc.name     = TaskDesc->name;
	ThDesc.policy   = TaskDesc->policy;
	ThDesc.priority = TaskDesc->priority;
	return OSDEV_SetPriority(&ThDesc);
}

OS_Status_t   OS_JoinThread(OS_Thread_t     Thread)
{
	pr_warn("%s not implemented\n", __func__);
	return OS_NO_ERROR;
}

char   *OS_ThreadName(void)
{
	unsigned int        i;

	for (i = 0; i < OS_MAX_TASKS; i++)
		if ((OS_TaskNameTable[i].Name[0] != '\0') && (OS_TaskNameTable[i].Thread == current)) {
			return OS_TaskNameTable[i].Name;
		}

	return OSDEV_ThreadName();
}

unsigned int OS_ThreadID(void)
{
	return task_pid_nr(current);
}

// --------------------------------------------------------------
// Signal Interactions

int OS_SignalPending(void)
{
	return OSDEV_SignalPending();
}

// --------------------------------------------------------------
// time functions

void OS_ReSchedule(void)
{
	schedule();
}

unsigned int OS_GetTimeInSeconds(void)
{
	ktime_t kt;
	struct timeval tv;

	kt = OSDEV_GetKtime();
	tv = ktime_to_timeval(kt);

	return tv.tv_sec;
}

unsigned int OS_GetTimeInMilliSeconds(void)
{
	return OSDEV_GetTimeInMilliSeconds();
}


unsigned long long OS_GetTimeInMicroSeconds(void)
{
	return OSDEV_GetTimeInMicroSeconds();
}

void OS_SleepMilliSeconds(unsigned int Value)
{
#if defined(CONFIG_STM_VIRTUAL_PLATFORM)
	/* VSOC - Sleep is the ennemy of the VSOC platform as time is simulated time...reduce all to 1ms */
	SE_DEBUG(group_misc, "sleep:%d - VSOC => 1ms\n", Value);
	Value = 1;
#endif
	OSDEV_SleepMilliSeconds(Value);
}

// --------------------------------------------------------------
// tunable functions

#ifdef CONFIG_DEBUG_FS
static int checkAndCreateTuneableRoot(void)
{
	if (TuneableRoot == NULL) {
		TuneableRoot = debugfs_create_dir("havana", NULL);
		if (IS_ERR_OR_NULL(TuneableRoot)) {
			pr_err("Error: %s Cannot create debugfs directory havana\n", __func__);
			return -1;
		} else {
			memset(Tuneable, 0 , sizeof(Tuneable));
			return 0;
		}
	}
	return 0;
}

static OS_TuneableEntry_t *getAvailableTuneableEntry(const char *Name)
{
	unsigned int    i;

	/* Find first NULL entry in Tuneable static table */
	for (i = 0; i < OS_MAX_TUNEABLES; i ++) {
		if (Tuneable[i].Dentry == NULL) {
			break;
		}
	}

	if (i < OS_MAX_TUNEABLES) {
		return &Tuneable[i];
	} else {
		pr_err("Error: %s No more tuneable entry available for %s\n", __func__, Name);
		BUG();
		return NULL;
	}
}
#endif  // DEBUG_FS

void OS_RegisterTuneable(const char *Name, unsigned int *Address)
{
#ifdef CONFIG_DEBUG_FS
	OS_TuneableEntry_t *tuneableEntry;

	if (checkAndCreateTuneableRoot()) {
		return; // error case
	}

	if (TuneableRoot != NULL) {
		tuneableEntry = getAvailableTuneableEntry(Name);
		if (tuneableEntry == NULL) {
			return;
		}
		strncpy(tuneableEntry->Name, Name, sizeof(tuneableEntry->Name));
		tuneableEntry->Name[sizeof(tuneableEntry->Name) - 1] = '\0';
		tuneableEntry->Dentry = debugfs_create_u32(tuneableEntry->Name, 0600, TuneableRoot, Address);
		_DUMPMEMCHK_UP(DMC_TUNE, tuneableEntry->Name, tuneableEntry->Dentry);
	}
#endif  // DEBUG_FS
}

void *OS_RegisterTuneableDir(const char *Name, void *Parent)
{
#ifdef CONFIG_DEBUG_FS
	OS_TuneableEntry_t *tuneableEntry;
	void               *retPtr = NULL;

	if (Name == NULL) {
		pr_err("Error: %s NULL input Name parameter\n", __func__);
		return NULL;
	}

	if (checkAndCreateTuneableRoot()) {
		return NULL; // error case
	}

	if (TuneableRoot != NULL) {
		tuneableEntry = getAvailableTuneableEntry(Name);
		if (tuneableEntry == NULL) {
			return NULL;
		}

		retPtr = debugfs_create_dir(Name, Parent ? (struct dentry *) Parent : TuneableRoot);
		if (IS_ERR_OR_NULL(retPtr)) {
			pr_err("Error: %s Cannot create debugfs directory:%s\n", __func__, Name);
			return NULL;
		}

		tuneableEntry->Dentry = retPtr;
		_DUMPMEMCHK_UP(DMC_TUNE, Name, tuneableEntry->Dentry);
		strncpy(tuneableEntry->Name, Name, sizeof(tuneableEntry->Name));
		tuneableEntry->Name[sizeof(tuneableEntry->Name) - 1] = '\0';
	}
	return retPtr;
#else
	return NULL;
#endif  // DEBUG_FS
}

void OS_UnregisterTuneable(const char *Name)
{
#ifdef CONFIG_DEBUG_FS
	unsigned int    i;

	if (TuneableRoot != NULL) {
		for (i = 0; i < OS_MAX_TUNEABLES; i ++) {
			if ((Tuneable[i].Dentry != NULL) &&
			    (strncmp(Tuneable[i].Name, Name, OS_MAX_NAME_SIZE - 1) == 0)) {
				break;
			}
		}

		if (i < OS_MAX_TUNEABLES) {
			_DUMPMEMCHK_DOWN(DMC_TUNE, Name, Tuneable[i].Dentry);
			debugfs_remove(Tuneable[i].Dentry);
			Tuneable[i].Dentry = NULL;

			for (i = 0; i < OS_MAX_TUNEABLES; i ++) {
				if (Tuneable[i].Dentry != NULL) {
					break;
				}
			}

			if (i == OS_MAX_TUNEABLES) {
				debugfs_remove(TuneableRoot);
				TuneableRoot = NULL;
			}
		}
	}
#endif  // DEBUG_FS
}

// ----------------------------------------------------------------------------------------

char *OS_StrDup(const char *String)
{
	size_t namelen = strlen(String) + 1;
	char  *Copy = (char *)OS_Malloc(namelen);

	if (Copy != NULL) {
		strncpy(Copy, String, namelen);
		Copy[namelen - 1] = '\0';
	}
	return Copy;
}

//////////////////////////////////////////////////////////////////////////////
// Services required by C++ code. These are typically part of the C++ run time
// but we dont have a run time!
#include <linux/vmalloc.h>

//#define ENABLE_NEW_POISONING
void *__builtin_new(size_t size)
{
	if (size > 0) {
		void *p;

		if (likely(size <= (4 * PAGE_SIZE))) {
			p   = kmalloc(size, GFP_KERNEL);
			_DUMPMEMCHK_UP(DMC_NEWK, "newk", p);
		} else {
			p   = vmalloc(size);
			_DUMPMEMCHK_UP(DMC_NEWV, "newv", p);
		}

#ifdef ENABLE_NEW_POISONING
		if (p) {
			memset(p, 0xcc, size);
		}
#endif
		return p;
	}

	return NULL;
}

void *__builtin_vec_new(size_t size)
{
	return __builtin_new(size);
}

void __builtin_delete(void *ptr)
{
	if (ptr) {
		if (unlikely((ptr >= (void *)VMALLOC_START) && (ptr < (void *)VMALLOC_END))) {
			_DUMPMEMCHK_DOWN(DMC_NEWV, "newv", ptr);
			vfree(ptr);
		} else {
			_DUMPMEMCHK_DOWN(DMC_NEWK, "newk", ptr);
			kfree(ptr);
		}
	}
}

void __builtin_vec_delete(void *ptr)
{
	__builtin_delete(ptr);
}

void __pure_virtual(void)
{
}

void __cxa_pure_virtual(void)
{
}

