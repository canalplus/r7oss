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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <sys/time.h>

#include "osinline.h"

/*
 * Tests for read-write lock interfaces of osinline.h.
 *
 * Note that these tests will actually validate the
 * userland implementation of osinline in this test framework,
 * not the actual kernel implementation in the player.
 * Hence, consider that:
 * - it validates/documents the interface usage,
 * - it validates the osinline part of the test framework,
 * - it does not validate the actual implementation in kernel mode.
 */

/*
 * TODO:
 * - a number of erroneous usages are undefined and thus can't be
 *   tested extensively, should be clarified in the interface:
 *   - what happens if a lock has been destroyed?
 *   - what happens is a lock is initialized twice?
 */

/* A reasonable value which should always be allocatable. */
#define N_THREADS 16
#define N_COUNTS 1000
static OS_Thread_t threads[N_THREADS];
static OS_Semaphore_t semaphores[N_THREADS];

static OS_RwLock_t counter_lock;
static unsigned int counter;

static OS_TaskDesc_t taskdesc = {
    "",
    0,
    0,
};

static OS_TaskEntry(IncrementCounter_task);

static void *IncrementCounter_task(void *Parameter) {
    int i;

    for (i = 0; i < N_COUNTS; i++) {
        OS_LockWrite(&counter_lock);
        counter++;
        OS_UnLockWrite(&counter_lock);
    }

    OS_TerminateThread();

    return (void *)0; /* UNREACHABLE */
}

TEST(OSInlineRwLocks, IncrementCounter) {
    int i;
    int status;

    OS_InitializeRwLock(&counter_lock);

    for (i = 0; i < N_THREADS; i++) {
        status = OS_CreateThread(&threads[i], IncrementCounter_task,
                                 NULL, &taskdesc);
        ASSERT_EQ(status, OS_NO_ERROR);
    }

    for (i = 0; i < N_THREADS; i++) {
        status = OS_JoinThread(threads[i]);
        ASSERT_EQ(status, OS_NO_ERROR);
    }

    OS_TerminateRwLock(&counter_lock);

    EXPECT_EQ(N_THREADS * N_COUNTS, counter);
}

static OS_TaskEntry(SignalAndWait_task);

static void *SignalAndWait_task(void *Parameter) {
    int i = (int)Parameter;

    OS_LockRead(&counter_lock);

    /* Wake-up next thread. */
    OS_SemaphoreSignal(&semaphores[(i + 1) % N_THREADS]);

    /*
     * Wait for previous thread to wake us up.  As we are in critical section
     * and the previous one will be in critical section to wake us up, this
     * proves several threads can share a read critical section.
     */
    OS_SemaphoreWait(&semaphores[i]);

    OS_UnLockRead(&counter_lock);

    OS_TerminateThread();

    return (void *)0; /* UNREACHABLE */
}

TEST(OSInlineRwLocks, SeveralThreadsCanBeInReadCriticalSection) {
    int i;
    int status;

    OS_InitializeRwLock(&counter_lock);

    for (i = 0; i < N_THREADS; i++) {
        OS_SemaphoreInitialize(&semaphores[i], 0);
        status = OS_CreateThread(&threads[i], SignalAndWait_task,
                                 (void *)i, &taskdesc);
        ASSERT_EQ(status, OS_NO_ERROR);
    }

    for (i = 0; i < N_THREADS; i++) {
        status = OS_JoinThread(threads[i]);
        ASSERT_EQ(status, OS_NO_ERROR);
    }

    for (i = 0; i < N_THREADS; i++) {
        OS_SemaphoreTerminate(&semaphores[i]);
    }

    OS_TerminateRwLock(&counter_lock);
}

TEST(OSInlineRwLocks, ReadCriticalSectionsAreNestable) {
    OS_RwLock_t lock;

    OS_InitializeRwLock(&lock);

    OS_LockRead(&lock);
    OS_LockRead(&lock);

    OS_UnLockRead(&lock);
    OS_UnLockRead(&lock);

    OS_TerminateRwLock(&lock);
}
