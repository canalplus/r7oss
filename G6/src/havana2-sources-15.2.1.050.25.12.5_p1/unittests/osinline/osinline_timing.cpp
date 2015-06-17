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
 * Tests for time related interfaces of osinline.h.
 *
 * Note that these tests will actually validate the
 * userland implementation of osinline in this test framework,
 * not the actual kernel implementation in the player.
 * Hence, consider that:
 * - it validates/documents the interface usage,
 * - it validates the osinline part of the test framework,
 * - it does not validate the actual implementation in kernel mode.
 */

TEST(OSInlineTiming, TimeSec) {

    unsigned int time_secs;

    time_secs = OS_GetTimeInSeconds();
    sleep(1);
    time_secs = OS_GetTimeInSeconds() - time_secs;

    EXPECT_GE(time_secs, 1);
}

TEST(OSInlineTiming, TimeMSec) {

    unsigned int time_msecs;

    time_msecs = OS_GetTimeInMilliSeconds();
    sleep(1);
    time_msecs = OS_GetTimeInMilliSeconds() - time_msecs;

    EXPECT_GE(time_msecs, 1000);
}

TEST(OSInlineTiming, TimeUSec) {

    unsigned long long time_usecs;

    time_usecs = OS_GetTimeInMicroSeconds();
    sleep(1);
    time_usecs = OS_GetTimeInMicroSeconds() - time_usecs;

    EXPECT_GE(time_usecs, 1000000);
}

TEST(OSInlineTiming, Reschedule) {

    unsigned long long time_usecs;

    time_usecs = OS_GetTimeInMicroSeconds();

    OS_ReSchedule();

    time_usecs = OS_GetTimeInMicroSeconds() - time_usecs;

    /* Obviously implementation dependent, though we can
       expect at least 1 microsecond as it is the shortest
       time interval computable by the osinline interface. */
    EXPECT_GE(time_usecs, 1);
}

TEST(OSInlineTiming, Sleep) {

    unsigned long long time_usecs;

    time_usecs = OS_GetTimeInMicroSeconds();

    OS_SleepMilliSeconds(1000);

    time_usecs = OS_GetTimeInMicroSeconds() - time_usecs;

    EXPECT_GE(time_usecs, 1000000);
}

TEST(OSInlineTiming, Sleep0) {

    unsigned long long time_usecs;

    time_usecs = OS_GetTimeInMicroSeconds();

    OS_SleepMilliSeconds(0);

    time_usecs = OS_GetTimeInMicroSeconds() - time_usecs;

    /* Expect that at least a reschedule occured.
       Ref to the comments in the OSInlineTiming/Reschedule test. */
    EXPECT_GE(time_usecs, 1);
}
