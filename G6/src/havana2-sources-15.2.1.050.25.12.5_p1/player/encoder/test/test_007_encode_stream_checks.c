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

int encode_stream_parameter_tests(EncoderTestArg *arg)
{
    EncoderContext context = {0};
    int result = 0;
    int failures = 0;
    // We need a valid Encode Object to test the Encode Stream calls against
    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new Encode\n", __func__);
        return result;
    }

    // STKPI Calls should be defensive against invalid input parameters
    // We expect all of these calls to FAIL. If they pass - then this test has failed.
    // Validate stm_se_encode_stream_new
    result = stm_se_encode_stream_new(NULL, context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL, &context.video_stream[0]);

    if (result >= 0)
    {
        pr_err("Error: %s Created an Invalid new Encode Stream with NULL Context pointer\n", __func__);
        stm_se_encode_stream_delete(context.video_stream[0]);
        failures++;
    }

    result = stm_se_encode_stream_new("EncodeStream0", NULL, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL, &context.video_stream[0]);

    if (result >= 0)
    {
        pr_err("Error: %s Created an Invalid new Encode Stream with NULL encode context pointer\n", __func__);
        // Impossible scenario really - we can't delete the stream that has been 'created'
        failures++;
    }

    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL, NULL);

    if (result >= 0)
    {
        pr_err("Error: %s Created an Invalid new Encode Stream with NULL encode stream context pointer\n", __func__);
        // we can't delete stream as we have no reference of it
        failures++;
    }

    // Tidy up our Encode Object
    result = stm_se_encode_delete(context.encode);

    if (result < 0)
    {
        pr_err("Error: %s stm_se_encode_delete returned %d\n", __func__, result);
        return result;
    }

    // Return the number of failures. 0 failures == Success
    return failures;
}

// Max 40 chars on Test Description
ENCODER_TEST(encode_stream_parameter_tests, "Encode Stream Object Parameter Validity Checks", BASIC_TEST);
