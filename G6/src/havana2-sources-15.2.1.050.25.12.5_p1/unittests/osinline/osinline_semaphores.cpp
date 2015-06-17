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

#include "wrap_posix.h"
#include "osinline.h"

/*
 * Tests for semaphores interfaces of osinline.h.
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
 *   - what happens if a semaphore have been destroyed?
 *   - what happens is a semaphore is initialized twice?
 */

#define N_THREADS 16
#define N_COUNTS 1000
static OS_Thread_t threads[N_THREADS];
static OS_Mutex_t counter_mutex;
static OS_Semaphore_t completion_sem;
static unsigned int counter;

static OS_TaskEntry(IncrementCounter_task);

static void *IncrementCounter_task(void *Parameter) {
    int i;

    for (i = 0; i < N_COUNTS; i++) {
        OS_LockMutex(&counter_mutex);
        counter++;
        OS_UnLockMutex(&counter_mutex);
    }

    /* Add some delay for Testing Timeout. */
    OS_SleepMilliSeconds(100);

    OS_SemaphoreSignal(&completion_sem);

    OS_TerminateThread();

    return (void *)0; /* UNREACHABLE */
}

TEST(OSInlineSemaphores, IncrementCounter) {
    int status;
    int i;
    int run_count = 0;
    OS_TaskDesc_t taskdesc = {
        "",
        0,
        0,
    };

    counter = 0;
    OS_SemaphoreInitialize(&completion_sem, 0);

    OS_InitializeMutex(&counter_mutex);

    for (i = 0; i < N_THREADS; i++) {
        status = OS_CreateThread(&threads[i], IncrementCounter_task,
                                 NULL, &taskdesc);
        ASSERT_EQ(OS_NO_ERROR, status);
        run_count++;
    }

    while (run_count > 0) {
        if (run_count % 2 == 0) {
            OS_SemaphoreWait(&completion_sem);
            run_count--;
        } else {
            status = OS_SemaphoreWaitInterruptible(&completion_sem);
            if (status == OS_NO_ERROR) {
                run_count--;
            }
        }
    }

    for (i = 0; i < N_THREADS; i++) {
        status = OS_JoinThread(threads[i]);
        ASSERT_EQ(OS_NO_ERROR, status);
    }

    EXPECT_EQ(N_THREADS * N_COUNTS, counter);

    OS_TerminateMutex(&counter_mutex);
    OS_SemaphoreTerminate(&completion_sem);
}

TEST(OSInlineSemaphores, IncrementCounterTimeOut) {
    int status;
    int i;
    int run_count = 0;
    int timeout = 0;
    OS_TaskDesc_t taskdesc = {
        "",
        0,
        0,
    };

    counter = 0;
    OS_SemaphoreInitialize(&completion_sem, 0);

    OS_InitializeMutex(&counter_mutex);

    for (i = 0; i < N_THREADS; i++) {
        status = OS_CreateThread(&threads[i], IncrementCounter_task,
                                 NULL, &taskdesc);
        ASSERT_EQ(OS_NO_ERROR, status);
        run_count++;
    }

    while (run_count > 0) {
        do {
            /* At least some timeout should occur as the delay is
               100 ms in IncrementCounter_task().
            */
            status = OS_SemaphoreWaitTimeout(&completion_sem, 10);
            if (status == OS_TIMED_OUT) { timeout += 1; }
        } while (status == OS_TIMED_OUT);
        run_count--;
    }

    for (i = 0; i < N_THREADS; i++) {
        status = OS_JoinThread(threads[i]);
        ASSERT_EQ(OS_NO_ERROR, status);
    }

    EXPECT_EQ(N_THREADS * N_COUNTS, counter);

    EXPECT_GT(timeout, 0);

    OS_TerminateMutex(&counter_mutex);
    OS_SemaphoreTerminate(&completion_sem);
}

static OS_TaskEntry(ParallelCounter_task);
static int parallel_counter;
static int max_parallel_counter;
static OS_Mutex_t parallel_counter_mutex;
static OS_Semaphore_t parallel_sem;

static void *ParallelCounter_task(void *Parameter) {
    OS_SemaphoreWait(&parallel_sem);

    OS_LockMutex(&parallel_counter_mutex);
    parallel_counter++;
    if (parallel_counter > max_parallel_counter) {
        max_parallel_counter = parallel_counter;
    }
    OS_UnLockMutex(&parallel_counter_mutex);

    /* Add some delay for increasing parallelism. */
    OS_SleepMilliSeconds(100);

    OS_LockMutex(&parallel_counter_mutex);
    parallel_counter--;
    OS_UnLockMutex(&parallel_counter_mutex);

    OS_SemaphoreSignal(&parallel_sem);

    OS_TerminateThread();

    return (void *)0; /* UNREACHABLE */
}

TEST(OSInlineSemaphores, ParallelCounter) {
    int i;
    int status;
    int max_parallel;
    OS_TaskDesc_t taskdesc = {
        "",
        0,
        0,
    };

    max_parallel = 4;
    ASSERT_GT(N_THREADS, max_parallel);

    OS_SemaphoreInitialize(&parallel_sem, max_parallel);

    OS_InitializeMutex(&parallel_counter_mutex);

    parallel_counter = max_parallel_counter = 0;
    for (i = 0; i < N_THREADS; i++) {
        status = OS_CreateThread(&threads[i], ParallelCounter_task,
                                 NULL, &taskdesc);
        ASSERT_EQ(OS_NO_ERROR, status);
    }
    for (i = 0; i < N_THREADS; i++) {
        status = OS_JoinThread(threads[i]);
        ASSERT_EQ(OS_NO_ERROR, status);
    }

    /* Check that running tasks counted down to 0. */
    EXPECT_EQ(0, parallel_counter);

    /* Check that at most max_parallel tasks ran in parallel. */
    EXPECT_LE(max_parallel_counter, max_parallel);

    /* We can expect that the max_parallel count has been reached
       as we are sleeping some amount of time in each task. */
    EXPECT_EQ(max_parallel_counter, max_parallel);

    /* Increment max parallel tasks. */
    max_parallel += 1;
    ASSERT_GT(N_THREADS, max_parallel);

    OS_TerminateMutex(&parallel_counter_mutex);
    OS_SemaphoreTerminate(&parallel_sem);
}

TEST(OSInlineSemaphores, InvalidUsage) {
    EXPECT_EXIT({
        OS_SemaphoreInitialize(NULL, 0);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_SemaphoreTerminate(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_SemaphoreWait(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_SemaphoreWaitTimeout(NULL, 0);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_SemaphoreWaitInterruptible(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_SemaphoreSignal(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

}

TEST(OSInlineSemaphores, WrappedInterruptible) {
    OS_Semaphore_t sema;
    int status;

    OS_SemaphoreInitialize(&sema, 0);

    OS_SemaphoreSignal(&sema);
    /* Simulate interruption of sem_wait() for the
       interruptible call. */
    WRAP_RETURN_ERRNO(sem_wait, -1, EINTR);
    status = OS_SemaphoreWaitInterruptible(&sema);
    EXPECT_EQ(OS_INTERRUPTED, status);
    /* The second call should be ok. */
    status = OS_SemaphoreWaitInterruptible(&sema);
    EXPECT_EQ(OS_NO_ERROR, status);

    OS_SemaphoreSignal(&sema);
    /* The first call will be interrupted, but the
       blocking wait should return only when done. */
    WRAP_RETURN_ERRNO(sem_wait, -1, EINTR);
    OS_SemaphoreWait(&sema);
    EXPECT_EQ(OS_NO_ERROR, status);

    OS_SemaphoreSignal(&sema);
    /* The first call will be interrupted, but the
       blocking timeout wait should return only when done. */
    WRAP_RETURN_ERRNO(sem_wait, -1, EINTR);
    status = OS_SemaphoreWaitTimeout(&sema, 100);
    EXPECT_EQ(OS_NO_ERROR, status);

    OS_SemaphoreTerminate(&sema);
}
