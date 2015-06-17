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
 * Tests for mutexes interfaces of osinline.h.
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
 *   - what happens if a mutex have been destroyed?
 *   - what happens if a mutex is recursively locked?
 *   - what happens is a mutex is initialized twice?
 */

/* A reasonable value which should always be allocatable. */
#define N_THREADS 16
#define N_COUNTS 1000
static OS_Thread_t threads[N_THREADS];

static OS_Mutex_t counter_mutex;
static unsigned int counter;

static OS_TaskEntry(IncrementCounter_task);

static void *IncrementCounter_task(void *Parameter) {
    int i;

    for (i = 0; i < N_COUNTS; i++) {

        while (OS_MutexIsLocked(&counter_mutex)) {
            (void)0;
        }

        OS_LockMutex(&counter_mutex);
        counter++;
        OS_UnLockMutex(&counter_mutex);
    }

    OS_TerminateThread();

    return (void *)0; /* UNREACHABLE */
}

TEST(OSInlineMutextes, IncrementCounter) {
    int i;
    int status;
    OS_TaskDesc_t taskdesc = {
        "",
        0,
        0,
    };

    OS_InitializeMutex(&counter_mutex);

    for (i = 0; i < N_THREADS; i++) {
        status = OS_CreateThread(&threads[i], IncrementCounter_task,
                                 NULL, &taskdesc);
        ASSERT_EQ(status, OS_NO_ERROR);
    }

    for (i = 0; i < N_THREADS; i++) {
        status = OS_JoinThread(threads[i]);
        ASSERT_EQ(status, OS_NO_ERROR);
    }

    OS_TerminateMutex(&counter_mutex);

    EXPECT_EQ(N_THREADS * N_COUNTS, counter);
}

TEST(OSInlineMutextes, InvalidUsage) {
    EXPECT_EXIT({
        OS_InitializeMutex(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_TerminateMutex(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_LockMutex(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_UnLockMutex(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_MutexIsLocked(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_MutexIsLocked(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

}
