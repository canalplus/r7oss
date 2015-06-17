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

#include <linux/sched.h>

// Some locals
#define TEST_BUFFER_SIZE 2048

static int SendBufferToStream(bpa_buffer *buffer, stm_se_encode_stream_h stream, stm_se_uncompressed_frame_metadata_t descriptor)
{
    int result = 0;
    descriptor.system_time   = jiffies;
    descriptor.discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;
    result = stm_se_encode_stream_inject_frame(stream, buffer->virtual, buffer->physical, TEST_BUFFER_SIZE, descriptor);

    if (result < 0)
    {
        pr_err("Error: %s Failed to send buffer\n", __func__);
    }

    return result;
}

static int SendAudioBufferToStream(bpa_buffer *buffer, stm_se_encode_stream_h stream)
{
    stm_se_uncompressed_frame_metadata_t descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.media = STM_SE_ENCODE_STREAM_MEDIA_AUDIO;
    descriptor.native_time_format = TIME_FORMAT_US;
    descriptor.native_time  = (0xfeedfacedeadbeefULL); //Undefined to avoid messages
    descriptor.audio.core_format.sample_rate   = 48000;
    descriptor.audio.core_format.channel_placement.channel_count = 2;
    descriptor.audio.core_format.channel_placement.chan[0]       = (uint8_t)STM_SE_AUDIO_CHAN_L;
    descriptor.audio.core_format.channel_placement.chan[1]       = (uint8_t)STM_SE_AUDIO_CHAN_R;
    descriptor.audio.sample_format = STM_SE_AUDIO_PCM_FMT_S32LE;
    return SendBufferToStream(buffer, stream, descriptor);
}

static int SendVideoBufferToStream(bpa_buffer *buffer, stm_se_encode_stream_h stream)
{
    stm_se_uncompressed_frame_metadata_t descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.media = STM_SE_ENCODE_STREAM_MEDIA_VIDEO;
    descriptor.video.frame_rate.framerate_num                        = 25;
    descriptor.video.frame_rate.framerate_den                        = 1;
    descriptor.video.video_parameters.width                          = 32;
    descriptor.video.video_parameters.height                         = 32;
    descriptor.video.video_parameters.colorspace                     = STM_SE_COLORSPACE_SMPTE170M;
    descriptor.video.video_parameters.aspect_ratio                   = STM_SE_ASPECT_RATIO_UNSPECIFIED;
    descriptor.video.video_parameters.pixel_aspect_ratio_numerator   = 1;
    descriptor.video.video_parameters.pixel_aspect_ratio_denominator = 1;
    descriptor.video.video_parameters.scan_type                      = STM_SE_SCAN_TYPE_PROGRESSIVE;
    descriptor.video.pitch                                           = 64;
    descriptor.video.window_of_interest                              = (stm_se_picture_rectangle_t) { 0, 0, 32, 32 };
    descriptor.video.picture_type                                    = STM_SE_PICTURE_TYPE_UNKNOWN;
    descriptor.video.surface_format                                  = SURFACE_FORMAT_VIDEO_422_YUYV;
    descriptor.video.vertical_alignment                              = 0; // not used with 422 yuyv format
    descriptor.native_time_format = TIME_FORMAT_PTS;
    // Fake Data that will change
    descriptor.native_time   = jiffies / 10;
    return SendBufferToStream(buffer, stream, descriptor);
}


// Test correct audio/video frames injection
int TestFrameInjection(void)
{
    int result = 0;
    int i;
    EncoderContext context;
    bpa_buffer frame;
    context = (struct EncoderTestCtx)
    {
        0
    }; // Zero down the context struct
    result = AllocateBuffer(TEST_BUFFER_SIZE, &frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the sequence (%d)\n", __func__, TEST_BUFFER_SIZE, result);
        return result;
    }

    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new Encode\n", __func__);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    //request usage of YUYV422 input color format for next encode_stream(s), this format preventing full memory optimization
    result = stm_se_encode_set_control(context.encode, STM_SE_CTRL_VIDEO_ENCODE_INPUT_COLOR_FORMAT, SURFACE_FORMAT_VIDEO_422_YUYV);

    if (result < 0)
    {
        pr_err("Error: %s Failed to set SURFACE_FORMAT_VIDEO_422_YUYV as color format through encode control\n", __func__);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL, &context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL EncodeStream\n", __func__);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_new("EncodeStream1", context.encode, STM_SE_ENCODE_STREAM_ENCODING_AUDIO_NULL, &context.audio_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_AUDIO_NULL EncodeStream\n", __func__);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    /* Now send some valid buffers to be encoded */
    for (i = 0; i < 50; i++)
    {
        result = SendAudioBufferToStream(&frame, context.audio_stream[0]);

        if (result < 0)
        {
            FreeBuffer(&frame);
            EncoderTestCleanup(&context);
            return result;
        }

        result = SendVideoBufferToStream(&frame, context.video_stream[0]);

        if (result < 0)
        {
            FreeBuffer(&frame);
            EncoderTestCleanup(&context);
            return result;
        }
    }

    result = FreeBuffer(&frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to free BPA Buffer (%d)\n", __func__, result);
    }

    EncoderTestCleanup(&context);
    return 0;
}

// Test bad audio/video frames injection
int TestBadFrameInjection(void)
{
    int result = 0;
    EncoderContext context;
    bpa_buffer frame;
    context = (struct EncoderTestCtx)
    {
        0
    }; // Zero down the context struct
    result = AllocateBuffer(TEST_BUFFER_SIZE, &frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the sequence (%d)\n", __func__, TEST_BUFFER_SIZE, result);
        return result;
    }

    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new Encode\n", __func__);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    //request usage of a color format for next encode_stream(s), involving full memory optimization and preventing usage of 422YUYV
    result = stm_se_encode_set_control(context.encode, STM_SE_CTRL_VIDEO_ENCODE_INPUT_COLOR_FORMAT, SURFACE_FORMAT_VIDEO_420_RASTER2B);

    if (result < 0)
    {
        pr_err("Error: %s Failed to set SURFACE_FORMAT_VIDEO_420_RASTER2B as color format through encode control\n", __func__);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL, &context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL EncodeStream\n", __func__);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_new("EncodeStream1", context.encode, STM_SE_ENCODE_STREAM_ENCODING_AUDIO_NULL, &context.audio_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_AUDIO_NULL EncodeStream\n", __func__);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    /* Try to inject video frame with buffer not respecting the 16-bytes alignment constraint */
    frame.physical ++;
    pr_info("Test injection of a video frame not respecting the 16-bytes alignment constraint (addr 0x%lx)..\n", frame.physical);
    result = SendVideoBufferToStream(&frame, context.video_stream[0]);
    frame.physical --;

    if (result >= 0)
    {
        pr_info("%s: Injection of a not 16-bytes aligned video frame did not fail (%d) !\n", __func__, result);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return -1;
    }

    /* Try to inject video frame with buffer not respecting 'profile' color format -SURFACE_FORMAT_VIDEO_420_RASTER2B format here- */
    pr_info("Test injection of a video frame with YUYV422 color format not respecting color format -NV12- profile..\n");
    result = SendVideoBufferToStream(&frame, context.video_stream[0]);
    if (result >= 0)
    {
        pr_info("%s: Injection of a YUYV422 video frame did not fail (%d) whereas not respecting NV12 default color format !\n", __func__, result);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return -1;
    }

    result = FreeBuffer(&frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to free BPA Buffer (%d)\n", __func__, result);
    }

    EncoderTestCleanup(&context);
    return 0;
}

int encode_stream_frame_injection_test(EncoderTestArg *arg)
{
    int result;
    result = TestFrameInjection();

    if (result < 0)
    {
        return result;
    }

    result = TestBadFrameInjection();

    if (result < 0)
    {
        return result;
    }

    return 0;
}

ENCODER_TEST(encode_stream_frame_injection_test, "Test Frame Injection to NULL coders", BASIC_TEST);
