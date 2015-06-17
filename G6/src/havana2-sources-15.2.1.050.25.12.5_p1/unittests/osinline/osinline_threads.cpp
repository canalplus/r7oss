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
#include <string.h>

#include "wrap_posix.h"
#include "osinline.h"

/*
 * Tests for thread management interfaces of osinline.h.
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
 * - it is not clear whether JoinThread() should be supported or not.
 * - need to define behavior on wrong arguments such as NULL name or
 *   NULL thread pointer in CreateThread().
 * - it is not clear whether TerminateThread() must be called by the thread
 *   task or if it can be implied. In the userland implementation, it is
 *   mandatory, maybe changed depending on the expected behavior.
 */

/* A reasonable max value for a thread name length. */
#define strdup_thread_name(name) strndup(name, 128)

/* A huge value, such that the actual max number of threads is exceeded. */
#define MAX_THREADS 4096
static OS_Thread_t threads[MAX_THREADS];

/* A reasonable value which should always be allocatable. */
#define N_THREADS 16
static char *thread_names[N_THREADS];
static unsigned int self_ids[N_THREADS];
static char *self_names[N_THREADS];

static OS_TaskEntry(SimpleThreads_task);

static void *SimpleThreads_task(void *Parameter) {
    OS_Status_t status;
    int thread_num = (int)(long)Parameter;
    unsigned int thread_id;
    const char *thread_name;

    thread_id = OS_ThreadID();
    EXPECT_NE(0U, thread_id);

    thread_name = OS_ThreadName();
    EXPECT_NE((char *)0, thread_name);

    status = OS_SetPriority(NULL);
    EXPECT_EQ(OS_NO_ERROR, status);

    self_ids[thread_num] = thread_id;
    self_names[thread_num] = strdup_thread_name(thread_name);

    OS_TerminateThread();

    return (void *)0; /* UNREACHABLE */
}

TEST(OSInlineThreads, SimpleThreads) {
    int status;
    int i;
    char name[128];
    OS_TaskDesc_t taskdesc = {
        "",
        0,
        0,
    };

    for (i = 0; i < N_THREADS; i++) {
        snprintf(name, sizeof(name), "thread-%d", i);
        taskdesc.name = name;
        thread_names[i] = strdup_thread_name(name);
        ASSERT_NE(thread_names[i], (char *)0);
        status = OS_CreateThread(&threads[i],
                                 (OS_TaskEntry_t)SimpleThreads_task,
                                 (OS_TaskParam_t)i,
                                 &taskdesc);
        ASSERT_EQ(OS_NO_ERROR, status);
    }

    for (i = 0; i < N_THREADS; i++) {
        status = OS_JoinThread(threads[i]);
        ASSERT_EQ(OS_NO_ERROR, status);
    }

    for (i = 0; i < N_THREADS; i++) {
        EXPECT_GT(self_ids[i], 0U);
        EXPECT_STREQ(self_names[i], thread_names[i]);
        free(self_names[i]);
        free(thread_names[i]);
    }

}

static OS_TaskEntry(ManyThreads_task);

static void *ManyThreads_task(void *Parameter) {
    OS_SleepMilliSeconds(1000);
    OS_TerminateThread();

    return (void *)0; /* UNREACHABLE */
}

TEST(OSInlineThreads, ManyThreads) {
    int status;
    int i;
    int n_threads_1;
    int n_threads_2;
    OS_TaskDesc_t taskdesc = {
        "",
        0,
        0,
    };

    for (i = 0; i < MAX_THREADS; i++) {
        status = OS_CreateThread(&threads[i],
                                 ManyThreads_task, NULL, &taskdesc);
        if (status == OS_ERROR) { break; }
    }
    n_threads_1 = i;
    ASSERT_GE(n_threads_1, N_THREADS);

    for (i = 0; i < n_threads_1; i++) {
        status = OS_JoinThread(threads[i]);
        ASSERT_EQ(OS_NO_ERROR, status);
    }

    for (i = 0; i < MAX_THREADS; i++) {
        status = OS_CreateThread(&threads[i],
                                 ManyThreads_task, NULL, &taskdesc);
        if (status == OS_ERROR) { break; }
    }
    n_threads_2 = i;
    ASSERT_GE(n_threads_1, N_THREADS);

    for (i = 0; i < n_threads_2; i++) {
        status = OS_JoinThread(threads[i]);
        ASSERT_EQ(OS_NO_ERROR, status);
    }

    /* Expect that after releasing all threads, we get the same number. */
    ASSERT_EQ(n_threads_1, n_threads_2);
}

static void *void_task(void *p) {
    return NULL;
}

TEST(OSInlineThreads, WrappedFailures) {
    OS_Thread_t thread;
    int status;
    OS_TaskDesc_t taskdesc = {
        "",
        0,
        0,
    };

    /* Force lower level thread creation to fail. */
    WRAP_RETURN(pthread_create, -1);
    status = OS_CreateThread(&thread,
                             void_task, NULL, &taskdesc);
    EXPECT_EQ(OS_ERROR, status);
}

TEST(OSInlineThreads, MainThread) {
    /* The main thread name is probably implementation
       dependent. Just check that it is not NULL. */
    EXPECT_NE((char *)0, OS_ThreadName());

    /* The main thread id is implementation dependent.
       Just chack that it is not invalid. */
    EXPECT_NE(OS_INVALID_THREAD, OS_ThreadID());
}
