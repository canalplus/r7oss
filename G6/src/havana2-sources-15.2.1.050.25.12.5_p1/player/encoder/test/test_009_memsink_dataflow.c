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

// For Jiffies/
#include <linux/sched.h>

#include <stm_memsrc.h>
#include <stm_memsink.h>

#include "encoder_test.h"
#include "common.h"

// Some locals
#define VERBOSE_MODE 0
#define MS_SINGLE_SLEEP_ON_EAGAN     (10)
#define MAX_MS_WAIT_FOR_SINGLE_FRAME (5000)
#define TEST_BUFFER_SIZE (640*480*2)
#define COMPRESSED_BUFFER_SIZE TEST_BUFFER_SIZE

static int PullCompressedEncode(pp_descriptor *memsinkDescriptor)
{
    int retval = -EAGAIN;
    stm_se_capture_buffer_t *p_memsink_pull_buffer;
    int loop = 0;

    while (-EAGAIN == retval)
    {
        retval = stm_memsink_test_for_data(memsinkDescriptor->obj, &memsinkDescriptor->m_availableLen);

        if (retval && (-EAGAIN != retval))
        {
            pr_err("PullCompressedEncode : stm_memsink_test_for_data failed (%d)\n", retval);
            return -1;
        }

        mdelay(MS_SINGLE_SLEEP_ON_EAGAN);

        if (loop == MAX_MS_WAIT_FOR_SINGLE_FRAME / MS_SINGLE_SLEEP_ON_EAGAN)
        {
            pr_err("PullCompressedEncode : Still Nothing available\n");
            return -1;
        }

        loop++;
    }

    if (memsinkDescriptor->m_availableLen == 0)
    {
        pr_err("PullCompressedEncode : Nothing available\n");
        return 0;
    }

    //
    // setup memsink_pull_buffer
    //
    p_memsink_pull_buffer = (stm_se_capture_buffer_t *)memsinkDescriptor->m_Buffer;
    p_memsink_pull_buffer->physical_address = NULL;
    p_memsink_pull_buffer->virtual_address  = &memsinkDescriptor->m_Buffer      [sizeof(stm_se_capture_buffer_t)];
    p_memsink_pull_buffer->buffer_length    = COMPRESSED_BUFFER_SIZE           - sizeof(stm_se_capture_buffer_t);
    retval = stm_memsink_pull_data(memsinkDescriptor->obj,
                                   p_memsink_pull_buffer,
                                   p_memsink_pull_buffer->buffer_length,
                                   &memsinkDescriptor->m_availableLen);

    if (retval != 0)
    {
        pr_err("PullCompressedEncode : stm_memsink_pull_data fails (%d)\n", retval);
        return -1;
    }

    // Dump the first 64 bytes of any compressed buffers received
    if (0 != VERBOSE_MODE)
    {
        print_hex_dump(KERN_INFO, "Memsink Pull: ", DUMP_PREFIX_OFFSET,
                       16, 1,
                       p_memsink_pull_buffer->virtual_address, min((unsigned) 64, memsinkDescriptor->m_availableLen), true);
    }

    return 0;
}

static int SendBufferToStream(bpa_buffer *buffer, stm_se_encode_stream_h stream, stm_se_uncompressed_frame_metadata_t descriptor)
{
    int result = 0;
    descriptor.native_time_format = TIME_FORMAT_PTS;
    // Fake Data that will change
    descriptor.native_time   = jiffies / 10;
    descriptor.system_time   = jiffies;
    descriptor.discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;
    result = stm_se_encode_stream_inject_frame(stream,  buffer->virtual, buffer->physical, TEST_BUFFER_SIZE, descriptor);

    if (result < 0)
    {
        pr_err("Error: %s Failed to send buffer\n", __func__);
    }

    return result;
}

static int SendVideoBufferToStream(bpa_buffer *buffer, stm_se_encode_stream_h stream)
{
    stm_se_uncompressed_frame_metadata_t descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.media = STM_SE_ENCODE_STREAM_MEDIA_VIDEO;
    descriptor.video.frame_rate.framerate_num                        = 25;
    descriptor.video.frame_rate.framerate_den                        = 1;
    descriptor.video.video_parameters.width                          = 640;
    descriptor.video.video_parameters.height                         = 480;
    descriptor.video.video_parameters.colorspace                     = STM_SE_COLORSPACE_SMPTE170M;
    descriptor.video.video_parameters.aspect_ratio                   = STM_SE_ASPECT_RATIO_4_BY_3;
    descriptor.video.video_parameters.pixel_aspect_ratio_numerator   = 1;
    descriptor.video.video_parameters.pixel_aspect_ratio_denominator = 1;
    descriptor.video.video_parameters.scan_type                      = STM_SE_SCAN_TYPE_PROGRESSIVE;
    descriptor.video.pitch                                           = (640 * 2);
    descriptor.video.window_of_interest                              = (stm_se_picture_rectangle_t) { 0, 0, 640, 480 };
    descriptor.video.picture_type                                    = STM_SE_PICTURE_TYPE_UNKNOWN;
    descriptor.video.surface_format                                  = SURFACE_FORMAT_VIDEO_422_RASTER;
    descriptor.video.vertical_alignment                              = 1;
    return SendBufferToStream(buffer, stream, descriptor);
}

static int MemsinkTestDataFlow(bpa_buffer *buffer, EncoderContext *context, pp_descriptor *sink)
{
    int result = 0;
    int i;

    /* Now send some buffers to be codedified */
    for (i = 0; i < 5; i++)
    {
        if (0 != VERBOSE_MODE)
        {
            pr_info("Sending test buffer to Video Stream\n");
        }

        result = SendVideoBufferToStream(buffer, context->video_stream[0]);

        if (result < 0)
        {
            return result;
        }

        if (0 != VERBOSE_MODE)
        {
            pr_info("Trying to get encoded Video Stream\n");
        }

        result = PullCompressedEncode(sink);

        if (result < 0)
        {
            return result;
        }
    }

    return result;
}

static int MemsinkDataFlow(EncoderTestArg *arg)
{
    EncoderContext context = {0};
    int result = 0;
    bpa_buffer frame;
    bpa_buffer compressed_video_frame;

    memset(&gVideoTest, 0, sizeof(gVideoTest));

    // Create an Encode Object
    result = AllocateBuffer(TEST_BUFFER_SIZE, &frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the sequence (%d)\n", __func__, TEST_BUFFER_SIZE, result);
        return result;
    }

    result = AllocateBuffer(COMPRESSED_BUFFER_SIZE, &compressed_video_frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the stream (%d)\n", __func__, COMPRESSED_BUFFER_SIZE, result);
        FreeBuffer(&frame);
        return result;
    }

    memset(compressed_video_frame.virtual, 0, COMPRESSED_BUFFER_SIZE);
    gVideoTest[0].Sink.m_Buffer = (unsigned char *)compressed_video_frame.virtual;
    gVideoTest[0].Sink.m_BufferLen = compressed_video_frame.size;

    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new Encode\n", __func__);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        return result;
    }

    // Create Stream
    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL, &context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL EncodeStream\n", __func__);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    // Create a MemSink
    result = stm_memsink_new("EncoderVideoSink", STM_IOMODE_BLOCKING_IO, KERNEL, (stm_memsink_h *)&gVideoTest[0].Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new memsink\n", __func__);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    // Attach the memsink
    result = stm_se_encode_stream_attach(context.video_stream[0], gVideoTest[0].Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to memsink for video\n", __func__);

        if (stm_memsink_delete((stm_memsink_h)gVideoTest[0].Sink.obj) < 0)    // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object\n", __func__);
        }

        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    ///////////////////////////////////////////////////////////////////////
    //
    // Real World would perform data flow here...
    //
    result = MemsinkTestDataFlow(&frame, &context, &gVideoTest[0].Sink);

    if (result < 0)
    {
        pr_err("Error: %s Data Flow Testing Failed\n", __func__);
        // Tidy up following test failure
        stm_se_encode_stream_detach(context.video_stream[0], gVideoTest[0].Sink.obj);

        if (stm_memsink_delete((stm_memsink_h)gVideoTest[0].Sink.obj) < 0)    // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object\n", __func__);
        }

        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    ///////////////////////////////////////////////////////////////////////
    // Detach the memsink
    result = stm_se_encode_stream_detach(context.video_stream[0], gVideoTest[0].Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to detach encode stream from memsink\n", __func__);

        if (stm_memsink_delete((stm_memsink_h)gVideoTest[0].Sink.obj) < 0)    // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object\n", __func__);
        }

        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    // Remove the memsink
    result = stm_memsink_delete((stm_memsink_h)gVideoTest[0].Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to detach encode stream from memsink\n", __func__);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    // Remove Stream and Encode
    result = stm_se_encode_stream_delete(context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to delete STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL EncodeStream %d\n", __func__, result);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_delete(context.encode);

    if (result < 0)
    {
        FreeBuffer(&frame);
        FreeBuffer(&compressed_video_frame);
        pr_err("Error: %s stm_se_encode_delete returned %d\n", __func__, result);
        return result;
    }

    result = FreeBuffer(&compressed_video_frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to free stream buffer (%d)\n", __func__, result);
    }

    gVideoTest[0].Sink.m_Buffer = NULL;
    gVideoTest[0].Sink.m_BufferLen = 0;

    result = FreeBuffer(&frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to free sequence buffer (%d)\n", __func__, result);
    }

    // Success
    return result;
}

ENCODER_TEST(MemsinkDataFlow, "Test the DataFlow of a Memsink", BASIC_TEST);
