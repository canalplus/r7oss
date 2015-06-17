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

#include <linux/delay.h>

// For Jiffies/
#include <linux/sched.h>

// For checking returned buffers
#include <linux/crc32.h>

#include <linux/delay.h>

#include <stm_memsrc.h>
#include <stm_memsink.h>

#include "encoder_test.h"
#include "common.h"

#include "video_buffer_traffic320yuv.h"
// video_buffer_traffic320yuv defines the following:
#define VIDEO_WIDTH       TRAFFIC_VIDEO_WIDTH
#define VIDEO_HEIGHT      TRAFFIC_VIDEO_HEIGHT
#define VIDEO_BUFFER_SIZE TRAFFIC_VIDEO_BUFFER_SIZE
#define VIDEO_SURFACE_FORMAT TRAFFIC_VIDEO_SURFACE_FORMAT

// Raster formats = bytes per pixel - all planar formats = 1
// Planar formats = total plane size as 2xfactor of main plane - all raster format = 2
// Width Alignment (bytes)
// Height Alignment (bytes) - obtained from SE DecodedBufferManager

static const int LUT[14][4] =
{
    {1, 2, 1,  1}, //    SURFACE_FORMAT_UNKNOWN,
    {1, 2, 1,  1}, //    SURFACE_FORMAT_MARKER_FRAME,
    {1, 2, 1,  1}, //    SURFACE_FORMAT_AUDIO,
    {1, 3, 16, 32},//    SURFACE_FORMAT_VIDEO_420_MACROBLOCK,
    {1, 3, 16, 32},//    SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK,
    {2, 2, 4,  1}, //    SURFACE_FORMAT_VIDEO_422_RASTER,
    {1, 3, 2,  16}, //    SURFACE_FORMAT_VIDEO_420_PLANAR,
    {1, 3, 2,  16}, //    SURFACE_FORMAT_VIDEO_420_PLANAR_ALIGNED,
    {1, 4, 2,  16}, //    SURFACE_FORMAT_VIDEO_422_PLANAR,
    {4, 2, 4,  32}, //    SURFACE_FORMAT_VIDEO_8888_ARGB,
    {3, 2, 4,  32}, //    SURFACE_FORMAT_VIDEO_888_RGB,
    {2, 2, 2,  32}, //    SURFACE_FORMAT_VIDEO_565_RGB,
    {2, 2, 4,  32}, //    SURFACE_FORMAT_VIDEO_422_YUYV,
    {1, 3, 4,  32}, //    SURFACE_FORMAT_VIDEO_420_RASTER2B - Alignment may not be true in practice if input buffer not from decoder
};

// Some locals

#define SD_WIDTH         720
#define VIDEO_FRAME_RATE_NUM (25)
#define VIDEO_FRAME_RATE_DEN (1)
#define TIME_PER_FRAME ((1000*VIDEO_FRAME_RATE_DEN)/VIDEO_FRAME_RATE_NUM)

// Test conditions: we inject 25 frames to video encoder with much constraints on bitrate / CPB
// Video frames are expected to be encoded and others to be skipped
// Behaviour is not the same on all platforms
#define VIDEO_FRAME_TO_ENCODE 25
#define EXPECTED_VIDEO_ENCODED_FRAME 15
#define EXPECTED_VIDEO_SKIPPED_FRAME 10

// Determine the output frame size....
#define VIDEO_PITCH     (((VIDEO_WIDTH*LUT[VIDEO_SURFACE_FORMAT][0] + LUT[VIDEO_SURFACE_FORMAT][2]-1)/LUT[VIDEO_SURFACE_FORMAT][2])*LUT[VIDEO_SURFACE_FORMAT][2])
#define VIDEO_BUFFER_HEIGHT     (((VIDEO_HEIGHT + LUT[VIDEO_SURFACE_FORMAT][3]-1)/LUT[VIDEO_SURFACE_FORMAT][3])*LUT[VIDEO_SURFACE_FORMAT][3])
#define VIDEO_FRAME_SIZE    ((VIDEO_PITCH*VIDEO_BUFFER_HEIGHT*LUT[VIDEO_SURFACE_FORMAT][1])/2)
#define TEST_BUFFER_SIZE    min(VIDEO_FRAME_SIZE, VIDEO_BUFFER_SIZE)
#define VERTICAL_ALIGNMENT  (LUT[VIDEO_SURFACE_FORMAT][3])

#define COMPRESSED_BUFFER_SIZE  1920*1080
#define VIDEO_BITRATE_MODE STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR
//#define VIDEO_BITRATE_MODE -1 //Fix qp
#define VIDEO_BITRATE 100000
#define VIDEO_CPB_BUFFER_SIZE (VIDEO_BITRATE/4)

//#define DUMP_INPUT_BUFFER
//#define DUMP_OUTPUT_BUFFER

/* Return: -1: fail; 0: pass 1: EOS */
static int PullCompressedEncode(pp_descriptor *memsinkDescriptor)
{
    int retval = 0;
    stm_se_capture_buffer_t *p_memsink_pull_buffer;
    u32 crc = 0;
    int loop = 0;

    //Wait for encoded frame to be available for memsink
    do
    {
        loop ++;
        memsinkDescriptor->m_availableLen = 0;
        retval = stm_memsink_test_for_data(memsinkDescriptor->obj, &memsinkDescriptor->m_availableLen);

        if (retval && (-EAGAIN != retval))
        {
            pr_err("Error: %s stm_memsink_test_for_data failed (%d)\n", __func__, retval);
            return -1;
        }

        mdelay(1); //in ms

        if (loop == 100)
        {
            pr_err("PullCompressedEncode : Nothing available\n");
            break;
        }
    }
    while (-EAGAIN == retval);

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

    // Check for EOS
    if (p_memsink_pull_buffer->u.compressed.discontinuity & STM_SE_DISCONTINUITY_EOS)
    {
        pr_info("%s EOS Detected! discontinuity 0x%x\n", __func__, p_memsink_pull_buffer->u.compressed.discontinuity);
        return 1;
    }

#ifdef DUMP_OUTPUT_BUFFER
    print_hex_dump(KERN_INFO, "OUTPUT BUFFER DUMP IN HEXADECIMAL: ", DUMP_PREFIX_OFFSET, 16, 1,
                   p_memsink_pull_buffer->virtual_address, min((unsigned) 64, memsinkDescriptor->m_availableLen), true);
#endif
    crc = crc32_be(crc, p_memsink_pull_buffer->virtual_address, memsinkDescriptor->m_availableLen);
    pr_info("CRC Value of this frame pull is 0x%08x\n", crc);
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
    result = stm_se_encode_stream_inject_frame(stream, buffer->virtual, buffer->physical, buffer->size, descriptor);

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
    descriptor.video.frame_rate.framerate_num                        = VIDEO_FRAME_RATE_NUM;
    descriptor.video.frame_rate.framerate_den                        = VIDEO_FRAME_RATE_DEN;
    descriptor.video.video_parameters.width                          = VIDEO_WIDTH;
    descriptor.video.video_parameters.height                         = VIDEO_HEIGHT;
    descriptor.video.video_parameters.colorspace                     = (VIDEO_WIDTH > SD_WIDTH ? STM_SE_COLORSPACE_SMPTE170M : STM_SE_COLORSPACE_SMPTE240M);
    descriptor.video.video_parameters.aspect_ratio                   = STM_SE_ASPECT_RATIO_4_BY_3;
    descriptor.video.video_parameters.pixel_aspect_ratio_numerator   = 1;
    descriptor.video.video_parameters.pixel_aspect_ratio_denominator = 1;
    descriptor.video.video_parameters.scan_type                      = STM_SE_SCAN_TYPE_PROGRESSIVE;
    descriptor.video.pitch                                           = VIDEO_PITCH;
    descriptor.video.window_of_interest                              = (stm_se_picture_rectangle_t) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT };
    descriptor.video.picture_type                                    = STM_SE_PICTURE_TYPE_UNKNOWN;
    descriptor.video.surface_format                                  = VIDEO_SURFACE_FORMAT;
    descriptor.video.vertical_alignment                              = VERTICAL_ALIGNMENT;
    return SendBufferToStream(buffer, stream, descriptor);
}

static int StaticImageEncode(EncoderContext *context, pp_descriptor *sink)
{
    int result = 0;
    int i;
    stm_se_picture_resolution_t resolution;
    bpa_buffer raw_video_frame;
    bpa_buffer compressed_video_frame;
    // Allocate Memory Buffer
    result = AllocateBuffer(TEST_BUFFER_SIZE, &raw_video_frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the sequence (%d)\n", __func__, TEST_BUFFER_SIZE, result);
        return result;
    }

    result = AllocateBuffer(COMPRESSED_BUFFER_SIZE, &compressed_video_frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the stream (%d)\n", __func__, COMPRESSED_BUFFER_SIZE, result);
        goto allocation_fail;
    }

    memset(compressed_video_frame.virtual, 0, COMPRESSED_BUFFER_SIZE);
    sink->m_Buffer = (unsigned char *)compressed_video_frame.virtual;
    sink->m_BufferLen = compressed_video_frame.size;

    // Copy Image to the physically contiguous buffer
    memcpy(raw_video_frame.virtual, traffic_video_buffer, raw_video_frame.size);
#ifdef DUMP_INPUT_BUFFER
    print_hex_dump(KERN_INFO, "INPUT BUFFER DUMP IN HEXADECIMAL: ", DUMP_PREFIX_OFFSET, 16, 1,
                   raw_video_frame.virtual, min((unsigned) 64, raw_video_frame.size), true);
#endif
    resolution.width  = VIDEO_WIDTH;
    resolution.height = VIDEO_HEIGHT;
    result = stm_se_encode_stream_set_compound_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, (void *)&resolution);

    if (result < 0)
    {
        pr_err("Error: %s Expected pass on testing set STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION control\n", __func__);
        goto free_buffer;
    }

    // STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE Control
    result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE, VIDEO_BITRATE_MODE);

    if (result < 0)
    {
        pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
        goto free_buffer;
    }

    // STM_SE_CTRL_ENCODE_STREAM_BITRATE Control
    result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_ENCODE_STREAM_BITRATE, VIDEO_BITRATE);

    if (result < 0)
    {
        pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
        goto free_buffer;
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE Control
    result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE, VIDEO_CPB_BUFFER_SIZE);

    if (result < 0)
    {
        pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
        goto free_buffer;
    }

    // Send for encode
    /* Encode 1 second of (static) video */
    for (i = 0; i < (VIDEO_FRAME_TO_ENCODE - 1); i++)
    {
        result = SendVideoBufferToStream(&raw_video_frame, context->video_stream[0]);

        if (result < 0)
        {
            goto free_buffer;
        }

        result = PullCompressedEncode(sink);

        if (result < 0)
        {
            goto free_buffer;
        }
    }

    /* Insert discontinuity EOS */
    /* Proposed to test the order for robustness: send video buffer, inject EOS, pull encodex2 */
    result = SendVideoBufferToStream(&raw_video_frame, context->video_stream[0]);

    if (result < 0)
    {
        goto free_buffer;
    }

    result = stm_se_encode_stream_inject_discontinuity(context->video_stream[0], STM_SE_DISCONTINUITY_EOS);

    if (result < 0)
    {
        pr_err("Error: %s Failed to inject discontinuity EOS\n", __func__);
        goto free_buffer;
    }

    result = PullCompressedEncode(sink);

    if (result < 0)
    {
        goto free_buffer;
    }

    result = PullCompressedEncode(sink);

    if (result < 0)
    {
        goto free_buffer;
    }

    if (result == 0)
    {
        pr_err("Error: %s Failed to detect discontinuity EOS\n", __func__);
        result = -1;
        goto free_buffer;
    }

    // Free BPA Memory
    result = FreeBuffer(&compressed_video_frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to free stream buffer (%d)\n", __func__, result);
        goto free_fail;
    }

    sink->m_Buffer = NULL;
    sink->m_BufferLen = 0;

    result = FreeBuffer(&raw_video_frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to free sequence buffer (%d)\n", __func__, result);
    }

    return result;

free_buffer:
    FreeBuffer(&compressed_video_frame);
allocation_fail:
free_fail:
    FreeBuffer(&raw_video_frame);

    return result;
}

static int StaticEncodeTest(EncoderTestArg *arg)
{
    EncoderContext context = {0};
    int result = 0;

    memset(&gVideoTest, 0, sizeof(gVideoTest));

    // Create an Encode Object
    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new Encode\n", __func__);
        return result;
    }

    // Create Stream
    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264, &context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream\n", __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    // Create a MemSink
    result = stm_memsink_new("EncoderVideoSink", STM_IOMODE_BLOCKING_IO, KERNEL, (stm_memsink_h *)&gVideoTest[0].Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new memsink\n", __func__);
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

        EncoderTestCleanup(&context);
        return result;
    }

    result = VideoEncodeEventSubscribe(&context);

    if (result < 0)
    {
        pr_err("Error: %s - VideoEncodeEventSubscribe failed\n", __func__);

        EncoderTestCleanup(&context);
        return result;
    }

    ///////////////////////////////////////////////////////////////////////
    //
    // Real World would perform data flow here...
    //
    result = StaticImageEncode(&context, &gVideoTest[0].Sink);

    if (result < 0)
    {
        pr_err("Error: %s Static Image Encode Failed\n", __func__);
        // Tidy up following test failure
        stm_se_encode_stream_detach(context.video_stream[0], gVideoTest[0].Sink.obj);

        if (stm_memsink_delete((stm_memsink_h)gVideoTest[0].Sink.obj) < 0)    // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object\n", __func__);
        }

        EncoderTestCleanup(&context);
        return result;
    }

    result = VideoEncodeEventUnsubscribe(&context);

    if (result < 0)
    {
        pr_err("Error: %s - VideoEncodeEventUnsubscribe failed\n", __func__);

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

        EncoderTestCleanup(&context);
        return result;
    }

    // Remove the memsink
    result = stm_memsink_delete((stm_memsink_h)gVideoTest[0].Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to detach encode stream from memsink\n", __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    // Remove Stream and Encode
    result = stm_se_encode_stream_delete(context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to delete STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream %d\n", __func__, result);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_delete(context.encode);

    if (result < 0)
    {
        pr_err("Error: %s stm_se_encode_delete returned %d\n", __func__, result);
        return result;
    }

    // Check events received
    if ((gVideoTest[0].EncodedFramesReceived != EXPECTED_VIDEO_ENCODED_FRAME) ||
        (gVideoTest[0].SkippedFramesReceived != EXPECTED_VIDEO_SKIPPED_FRAME))
    {
        pr_info("Event test criteria fail\n");
        pr_info("Expected %u video encoded events, got %u \n", EXPECTED_VIDEO_ENCODED_FRAME, gVideoTest[0].EncodedFramesReceived);
        pr_info("Expected %u video skipped events, got %u \n", EXPECTED_VIDEO_SKIPPED_FRAME, gVideoTest[0].SkippedFramesReceived);
        result = -1;
    }
    else
    {
        pr_info("Got expected %u video encode events and %u video encode skipped events\n", \
                gVideoTest[0].EncodedFramesReceived, gVideoTest[0].SkippedFramesReceived);
    }

    // Success
    return result;
}
/*** Only 40 Chars will be displayed ***/
ENCODER_TEST(StaticEncodeTest, "Test video encoding checking events", BASIC_TEST);
