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

int dual_stream_test(EncoderTestArg *arg)
{
    EncoderContext context = {0};
    int result = 0;
    // Test creating and destroying of a single EncodeStream
    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new Encode\n", __func__);
        return result;
    }

    // Create Streams
    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL, &context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL EncodeStream\n", __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_new("EncodeStream1", context.encode, STM_SE_ENCODE_STREAM_ENCODING_AUDIO_NULL, &context.audio_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_AUDIO_NULL EncodeStream\n", __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    // Destroy Streams
    // We are testing object deletion in this test as well
    //- so its important we distinctly remove our stream ourselves
    result = stm_se_encode_stream_delete(context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to delete STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL EncodeStream %d\n", __func__, result);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_delete(context.audio_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to delete STM_SE_ENCODE_STREAM_ENCODING_AUDIO_NULL EncodeStream %d\n", __func__, result);
        EncoderTestCleanup(&context);
        return result;
    }

    // Tidy up
    result = stm_se_encode_delete(context.encode);

    if (result < 0)
    {
        pr_err("Error: %s stm_se_encode_delete returned %d\n", __func__, result);
        return result;
    }

    // Success
    return result;
}

// Max 40 chars on Test Description
ENCODER_TEST(dual_stream_test, "Dual Stream Object create and delete", BASIC_TEST);
