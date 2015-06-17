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
#include "encoder_test.h"
#include "common.h"

int TestFrameInjection(void);

#define TEST_ITERATIONS 50

int repeat_frame_injection(EncoderTestArg *arg)
{
    int result = 0;
    int x = 0;

    while (x++ < TEST_ITERATIONS)
    {
        result = TestFrameInjection();

        if (0 != result)
        {
            pr_err("Error: %s Frame Injection test failed on iteration %d\n", __func__, x);
            return result;
        }
    }

    // Success
    return result;
}

// Max 40 chars on Test Description
ENCODER_TEST(repeat_frame_injection, "Repeat Frame Injection test 50 times", BASIC_TEST);
