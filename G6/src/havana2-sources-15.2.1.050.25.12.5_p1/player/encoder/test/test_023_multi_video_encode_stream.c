/************************************************************************
Copyright (C) 2003-2013 STMicroelectronics. All Rights Reserved.

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

int multi_video_encode_stream_test(EncoderTestArg *arg)
{
    EncoderContext context = {0};
    int result = 0;

    // Test creating and destroying of multiple EncodeStream
    result = stm_se_encode_new("Encode0", &context.encode);
    if (result < 0)
    {
        pr_err("%s: Failed to create a new Encode\n", __FUNCTION__);
        return result;
    }

    // Create Video Encode Stream 0: should succeed whatever the platform
    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264, &context.video_stream[0]);
    if (result < 0)
    {
        pr_err("%s: Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream\n", __FUNCTION__);
        //context video_stream is not relevant in that case
        context.video_stream[0] = NULL;
        MultiEncoderTestCleanup(&context);
        return result;
    }

    // Create Video Encode Stream 1: should fail on Cannes but succeed on Monaco platforms
    result = stm_se_encode_stream_new("EncodeStream1", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264, &context.video_stream[1]);
    if (result < 0)
    {
        pr_warn("%s: Failed to create a 2nd STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream\n", __FUNCTION__);
        pr_warn("%s: This is an expected behavior in case of Cannes platforms but not on Monaco-\n", __FUNCTION__);
        //context video_stream is not relevant in that case
        context.video_stream[1] = NULL;
        MultiEncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_delete(context.video_stream[0]);
    if (result < 0)
    {
        pr_err("%s: Failed to delete STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream %d\n", __FUNCTION__, result);
        MultiEncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_delete(context.video_stream[1]);
    if (result < 0)
    {
        pr_err("%s: Failed to delete STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream %d\n", __FUNCTION__, result);
        MultiEncoderTestCleanup(&context);
        return result;
    }

    // Tidy up
    result = stm_se_encode_delete(context.encode);
    if (result < 0)
    {
        pr_err("%s: stm_se_encode_delete returned %d\n", __FUNCTION__, result);
        return result;
    }

    // Success
    return result;
}

// Max 40 chars on Test Description
ENCODER_TEST(multi_video_encode_stream_test, "Test multiple video encode stream", BASIC_TEST);
