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
 * Tests for events interfaces of osinline.h.
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
 * - complete userland implementation and tests for the
 *   remaining of the events interface, currently missing:
 *   - OS_WaitForEventInterruptible,
 *   - OS_SetEventInterruptible,
 *   - OS_ReInitializeEvent
 * - a number of erroneous usages are undefined and thus can't be
 *   tested extensively, should be clarified in the interface:
 *   - what happens if an event have been destroyed?
 *   - what happens is an event is initialized twice?
 */

static void *SimpleEvent_task(void *Parameter) {
    int status;
    OS_Event_t *event_ptr = (OS_Event_t *)Parameter;

    status = OS_TestEventSet(event_ptr);
    EXPECT_EQ(0, status);

    OS_SetEvent(event_ptr);

    status = OS_TestEventSet(event_ptr);
    EXPECT_EQ(1, status);

    OS_TerminateThread();

    return (void *)0; /* UNREACHABLE */
}

TEST(OSInlineEvents, SimpleEvent) {
    int status;
    OS_Thread_t thread;
    OS_Event_t event;
    OS_TaskDesc_t taskdesc = {
        "",
        0,
        0,
    };

    status = OS_InitializeEvent(&event);
    ASSERT_EQ(OS_NO_ERROR, status);

    status = OS_TestEventSet(&event);
    EXPECT_EQ(0, status);

    status = OS_CreateThread(&thread, SimpleEvent_task,
                             &event, &taskdesc);
    ASSERT_EQ(OS_NO_ERROR, status);

    status = OS_WaitForEvent(&event, OS_INFINITE);
    ASSERT_EQ(OS_NO_ERROR, status);

    status = OS_TestEventSet(&event);
    EXPECT_EQ(1, status);

    status = OS_JoinThread(thread);
    ASSERT_EQ(OS_NO_ERROR, status);

    OS_TerminateEvent(&event);
}

#define MIN_DELAY 10
#define AVG_LOOPS 10
#define MIN_LOOPS 3
#define MAX_LOOPS 30

static void *DelayedEvent_task(void *Parameter) {
    OS_Event_t *event_ptr = (OS_Event_t *)Parameter;

    OS_SleepMilliSeconds(MIN_DELAY * AVG_LOOPS);

    OS_SetEvent(event_ptr);

    OS_TerminateThread();

    return (void *)0; /* UNREACHABLE */
}

TEST(OSInlineEvents, DelayedEvent) {
    int status;
    int wait_num;
    OS_Thread_t thread;
    OS_Event_t event;
    OS_TaskDesc_t taskdesc = {
        "",
        0,
        0,
    };

    status = OS_InitializeEvent(&event);
    ASSERT_EQ(OS_NO_ERROR, status);

    status = OS_TestEventSet(&event);
    EXPECT_EQ(0, status);

    status = OS_CreateThread(&thread, DelayedEvent_task,
                             &event, &taskdesc);
    ASSERT_EQ(OS_NO_ERROR, status);

    wait_num = 0;
    do {
        status = OS_WaitForEvent(&event, MIN_DELAY);
        wait_num++;
        if (wait_num > MAX_LOOPS) { break; }
    } while (status == OS_TIMED_OUT);

    /* We expect a reasonable range of loop iterations, given the delay. */
    EXPECT_GE(wait_num, MIN_LOOPS);
    EXPECT_LE(wait_num, MAX_LOOPS);

    EXPECT_EQ(OS_NO_ERROR, status);

    status = OS_TestEventSet(&event);
    EXPECT_EQ(1, status);

    status = OS_JoinThread(thread);
    ASSERT_EQ(OS_NO_ERROR, status);

    OS_TerminateEvent(&event);
}

#define N_THREADS 16
static OS_Thread_t threads[N_THREADS];
static OS_Event_t start_event;
static OS_Mutex_t start_event_mutex;
static unsigned int started_count;
static OS_Mutex_t started_count_mutex;
static OS_Event_t running_event;

static void *StartEvent_task(void *Parameter) {
    int started;
    int status;

    OS_SleepMilliSeconds(10);

    status = OS_TestEventSet(&running_event);
    EXPECT_EQ(0, status);

    OS_LockMutex(&start_event_mutex);
    OS_LockMutex(&started_count_mutex);
    started_count++;
    OS_UnLockMutex(&started_count_mutex);
    OS_SetEvent(&start_event);
    OS_UnLockMutex(&start_event_mutex);

    OS_WaitForEvent(&running_event, OS_INFINITE);

    /* At this point, it is expected to have all
       threads started. */
    OS_LockMutex(&started_count_mutex);
    started = started_count;
    OS_UnLockMutex(&started_count_mutex);

    EXPECT_EQ(N_THREADS, started);

    OS_TerminateThread();

    return (void *)0; /* UNREACHABLE */
}

TEST(OSInlineEvents, StartEvent) {
    int status;
    int i;
    int started;
    OS_TaskDesc_t taskdesc = {
        "",
        0,
        0,
    };

    started_count = 0;

    OS_InitializeMutex(&start_event_mutex);
    OS_InitializeMutex(&started_count_mutex);
    OS_InitializeEvent(&start_event);

    status = OS_InitializeEvent(&running_event);
    ASSERT_EQ(OS_NO_ERROR, status);

    for (i = 0; i < N_THREADS; i++) {
        status = OS_CreateThread(&threads[i], StartEvent_task,
                                 NULL, &taskdesc);
        ASSERT_EQ(OS_NO_ERROR, status);
    }

    do {
        status = OS_WaitForEvent(&start_event, OS_INFINITE);
        ASSERT_EQ(OS_NO_ERROR, status);
        OS_LockMutex(&start_event_mutex);
        OS_ResetEvent(&start_event);
        OS_LockMutex(&started_count_mutex);
        started = started_count;
        OS_UnLockMutex(&started_count_mutex);
        OS_UnLockMutex(&start_event_mutex);
    } while (started < N_THREADS);

    EXPECT_EQ(N_THREADS, started);

    OS_SetEvent(&running_event);

    for (i = 0; i < N_THREADS; i++) {
        status = OS_JoinThread(threads[i]);
        ASSERT_EQ(OS_NO_ERROR, status);
    }

    OS_TerminateEvent(&running_event);
    OS_TerminateEvent(&start_event);

    OS_TerminateMutex(&started_count_mutex);
    OS_TerminateMutex(&start_event_mutex);
}

TEST(OSInlineEvents, InvalidUsage) {
    EXPECT_EXIT({
        OS_InitializeEvent(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_TerminateEvent(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_WaitForEvent(NULL, 0);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_SetEvent(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_TestEventSet(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");

    EXPECT_EXIT({
        OS_ResetEvent(NULL);
    }, ::testing::KilledBySignal(SIGABRT), "Assertion");
}

TEST(OSInlineEvents, WrappedFailures) {
    OS_Event_t event;
    int status;

    /* Force posix initializations to fail. */
    WRAP_RETURN(pthread_cond_init, -1);
    status = OS_InitializeEvent(&event);
    EXPECT_EQ(OS_ERROR, status);

    WRAP_RETURN(pthread_mutex_init, -1);
    status = OS_InitializeEvent(&event);
    EXPECT_EQ(OS_ERROR, status);
}
