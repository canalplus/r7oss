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
 * Tests for barriers interfaces of osinline.h.
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
 * Note that we test the memory barrier only for the store/load
 * reordering case which is the most subject to failure.
 * Experiments on x86_64 show that the issue arrives on average 1
 * time over 3000 transactions, hence we run 100000 transactions
 * which should be sufficient to exhibit the problem frequently.
 *
 * In the case where no error is reported, one can't actually conclude
 * anything as the race condition may not have been exhibited.
 */

#ifndef DELAY_HINT
#define DELAY_HINT 8
#endif

static OS_Semaphore_t start_sem1;
static OS_Semaphore_t start_sem2;
static OS_Semaphore_t stop_sem;
static OS_Event_t stop_event;

static unsigned int seed1 = 1;
static unsigned int seed2 = 2;

static volatile int X, Y;
static volatile int r1, r2;

/* One may uncomment this to observe the bug actually. */
//#define SHOW_BUG
#ifndef SHOW_BUG
#define STORELOAD_BARRIER() OS_Smp_Mb()
#else
#define STORELOAD_BARRIER() (void)0
#endif

#define ITERATIONS 100000

static void *StoreLoadReordering_task1(void *param) {
    while (!OS_TestEventSet(&stop_event)) {
        OS_SemaphoreWait(&start_sem1);
        // Add a short, random delay
        while (rand_r(&seed1) % DELAY_HINT != 0) {}

        // ----- THE TRANSACTION! -----
        X = 1;
        STORELOAD_BARRIER();
        r1 = Y;

        OS_SemaphoreSignal(&stop_sem);
    }

    OS_TerminateThread();

    return NULL;  // UNREACHABLE
}

static void *StoreLoadReordering_task2(void *param) {
    while (!OS_TestEventSet(&stop_event)) {
        OS_SemaphoreWait(&start_sem2);
        // Add a short, random delay
        while (rand_r(&seed2) % DELAY_HINT != 0) {}

        // ----- THE TRANSACTION! -----
        Y = 1;
        STORELOAD_BARRIER();
        r2 = X;

        OS_SemaphoreSignal(&stop_sem);
    }

    OS_TerminateThread();

    return NULL;  // UNREACHABLE
};

TEST(OSInlineBarriers, StoreLoadReordering) {
    int status;
    OS_Thread_t thread1, thread2;
    int iterations;
    OS_TaskDesc_t taskdesc = {
        "",
        0,
        0,
    };

    OS_SemaphoreInitialize(&start_sem1, 0);
    OS_SemaphoreInitialize(&start_sem2, 0);
    OS_SemaphoreInitialize(&stop_sem, 0);

    status = OS_CreateThread(&thread1, StoreLoadReordering_task1,
                             NULL, &taskdesc);
    ASSERT_EQ(OS_NO_ERROR, status);
    status = OS_CreateThread(&thread2, StoreLoadReordering_task2,
                             NULL, &taskdesc);
    ASSERT_EQ(OS_NO_ERROR, status);

    // Repeat the experiment for some amount of time
    int perlines = 80;
    int nerrors = 0;
    for (iterations = 1; iterations < ITERATIONS; iterations++) {
        // Reset X and Y
        X = 0;
        Y = 0;

        // Signal both threads to start their transaction
        OS_SemaphoreSignal(&start_sem1);
        OS_SemaphoreSignal(&start_sem2);

        // Both threads execute their transaction

        // Wait for both threads to complete their transaction
        OS_SemaphoreWait(&stop_sem);
        OS_SemaphoreWait(&stop_sem);

        // Check if there was a simultaneous reorder such that
        // both r1 and r2 are 0 at the end of the transaction,
        // i.e. no sequential consistency is enforced and
        // thus we can't conclude whether task1 (resp. task2)
        // executed first the store to X (resp. Y).
        if (r1 == 0 && r2 == 0) {
            nerrors++;
            fprintf(stderr, "!");
            if (nerrors % perlines == 0) { fprintf(stderr, "\n"); }
        }
    }

    if (nerrors > 0) { fprintf(stderr, "\n"); }

    /* Actually, if no error is observed, one can't conclude that
       the implementation is correct, though we can't do much better. */
    EXPECT_EQ(0, nerrors);

    // Set termination flag and signal both threads
    OS_SetEvent(&stop_event);
    OS_SemaphoreSignal(&start_sem1);
    OS_SemaphoreSignal(&start_sem2);

    OS_JoinThread(thread1);
    OS_JoinThread(thread2);

    OS_SemaphoreTerminate(&start_sem1);
    OS_SemaphoreTerminate(&start_sem2);
    OS_SemaphoreTerminate(&stop_sem);
    OS_TerminateEvent(&stop_event);
}
