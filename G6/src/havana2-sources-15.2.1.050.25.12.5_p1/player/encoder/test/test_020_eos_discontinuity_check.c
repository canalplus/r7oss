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
#include <linux/delay.h>
#include <linux/slab.h>

#include <stm_memsrc.h>
#include <stm_memsink.h>

#include "encoder_test.h"
#include "common.h"
#include "compressed_video_buffer.h"
#include "compressed_audio_buffer.h"

#define TRANSCODED_BUFFER_SIZE (2 * COMPRESSED_VIDEO_BUFFER_SIZE)

typedef struct playback_context_struct_s
{
    stm_se_playback_h       playback_handle;
    stm_se_play_stream_h    audio_stream_handle;
    stm_se_play_stream_h    video_stream_handle;
    bool                    is_audio_encode_stream_attached;
    bool                    is_video_encode_stream_attached;
} playback_context_t;

typedef struct encode_context_struct_s
{
    stm_se_encode_h         encode_handle;
    stm_se_encode_stream_h  audio_stream_handle;
    stm_se_encode_stream_h  video_stream_handle;
    bool                    is_audio_sink_attached;
    bool                    is_video_sink_attached;
} encode_context_t;

typedef struct memsink_context_struct_s
{
    stm_memsink_h           audio_sink_handle;
    stm_memsink_h           video_sink_handle;
} memsink_context_t;

typedef struct test_context_struct_s
{
    playback_context_t      playback_context;
    encode_context_t        encode_context;
    memsink_context_t       memsink_context;
} test_context_t;


#define INJECT_COUNT 1

static int PullCompressedEncode(pp_descriptor *memsinkDescriptor, stm_se_discontinuity_t discontinuity, bool is_video)
{
    int retval;
    stm_se_capture_buffer_t *p_memsink_pull_buffer;
    int loop = 0;

    //Wait for encoded frame to be available for memsink
    do
    {
        memsinkDescriptor->m_availableLen = 0;
        retval = stm_memsink_test_for_data(memsinkDescriptor->obj, &memsinkDescriptor->m_availableLen);
        if (retval && (-EAGAIN != retval))
        {
            pr_err("Error: %s stm_memsink_test_for_data failed (%d)\n", __func__, retval);
            return -1;
        }

        mdelay(10); //in ms

        if (loop == 100)
        {
            pr_err("PullCompressedEncode : Nothing available\n");
            return -1;
        }
        loop ++;
    }
    while (-EAGAIN == retval);

    // setup memsink_pull_buffer
    p_memsink_pull_buffer = (stm_se_capture_buffer_t *)memsinkDescriptor->m_Buffer;
    p_memsink_pull_buffer->physical_address = NULL;
    p_memsink_pull_buffer->virtual_address  = &memsinkDescriptor->m_Buffer      [sizeof(stm_se_capture_buffer_t)];
    p_memsink_pull_buffer->buffer_length    = TRANSCODED_BUFFER_SIZE - sizeof(stm_se_capture_buffer_t);
    retval = stm_memsink_pull_data(memsinkDescriptor->obj,
                                   p_memsink_pull_buffer,
                                   p_memsink_pull_buffer->buffer_length,
                                   &memsinkDescriptor->m_availableLen);

    if (retval != 0)
    {
        pr_err("PullCompressedEncode : stm_memsink_pull_data fails (%d)\n", retval);
        return -1;
    }

    // Check discontinuity
    if (p_memsink_pull_buffer->u.compressed.discontinuity != discontinuity)
    {
        pr_err("PullCompressedEncode : %s Frame : Expect discontinuity 0x%x but get 0x%x instead!\n",
               (is_video ? "Video" : "Audio"), discontinuity, p_memsink_pull_buffer->u.compressed.discontinuity);
        return -1;
    }
    else
    {
        pr_info("PullCompressedEncode : %s Frame : Expected discontinuity 0x%x Payload %d\n",
                (is_video ? "Video" : "Audio"), discontinuity, p_memsink_pull_buffer->payload_length);
    }

    return 1;
}

static int EOSDiscontinuityCheckDataFlow(bpa_buffer *buffer, pp_descriptor *sink, bool is_video)
{
    int Result = 0;
    int i;
    stm_se_discontinuity_t discontinuity;

    for (i = 0; i < INJECT_COUNT ; i++)
    {
        if (is_video)
        {
            pr_info("--------- Video encode EOS ------------\n");
            discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;
            pr_info("Trying to pull payload 1\n");
            Result = PullCompressedEncode(sink, discontinuity, is_video);
            if (Result < 0)
            {
                return Result;
            }

            pr_info("Trying to pull EOS discontinuity\n");
            discontinuity = STM_SE_DISCONTINUITY_EOS;
            Result = PullCompressedEncode(sink, discontinuity, is_video);
            if (Result < 0)
            {
                return Result;
            }
        }
        else
        {
            pr_info("--------- Audio encode EOS ------------\n");
            discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;
            pr_info("Trying to pull payload 1\n");
            Result = PullCompressedEncode(sink, discontinuity, is_video);
            if (Result < 0)
            {
                return Result;
            }

            pr_info("Trying to pull payload 2\n");
            Result = PullCompressedEncode(sink, discontinuity, is_video);
            if (Result < 0)
            {
                return Result;
            }

            pr_info("Trying to pull payload 3\n");
            Result = PullCompressedEncode(sink, discontinuity, is_video);
            if (Result < 0)
            {
                return Result;
            }

            pr_info("Trying to pull EOS discontinuity\n");
            discontinuity = STM_SE_DISCONTINUITY_EOS;
            Result = PullCompressedEncode(sink, discontinuity, is_video);
            if (Result < 0)
            {
                return Result;
            }
        }
    }

    return Result;
}

static void PlaybackCleanup(playback_context_t *playback_context, encode_context_t *encode_context)
{
    if (playback_context->video_stream_handle)
    {
        if (playback_context->is_video_encode_stream_attached)
        {
            if (stm_se_play_stream_detach(playback_context->video_stream_handle, encode_context->video_stream_handle))
            {
                pr_err("Failed to detach video encode stream from play stream\n");
            }
        }
        if (stm_se_play_stream_delete(playback_context->video_stream_handle) < 0)
        {
            pr_err("Failed to delete video play stream\n");
        }
    }
    if (playback_context->audio_stream_handle)
    {
        if (playback_context->is_audio_encode_stream_attached)
        {
            if (stm_se_play_stream_detach(playback_context->audio_stream_handle, encode_context->audio_stream_handle))
            {
                pr_err("Failed to detach audio encode stream from play stream\n");
            }
        }
        if (stm_se_play_stream_delete(playback_context->audio_stream_handle) < 0)
        {
            pr_err("Failed to delete audio play stream\n");
        }
    }
    if (playback_context->playback_handle)
    {
        if (stm_se_playback_delete(playback_context->playback_handle) < 0)
        {
            pr_err("Failed to delete playback\n");
        }
    }
}

static void EncodeCleanup(encode_context_t *encode_context, memsink_context_t *memsink_context)
{
    if (encode_context->video_stream_handle)
    {
        if (encode_context->is_video_sink_attached)
        {
            if (stm_se_encode_stream_detach(encode_context->video_stream_handle, memsink_context->video_sink_handle) < 0)
            {
                pr_err("Failed to detach video sink from encode stream\n");
            }
        }
        if (stm_se_encode_stream_delete(encode_context->video_stream_handle) < 0)
        {
            pr_err("Failed to delete video encode stream\n");
        }
    }
    if (encode_context->audio_stream_handle)
    {
        if (encode_context->is_audio_sink_attached)
        {
            if (stm_se_encode_stream_detach(encode_context->audio_stream_handle, memsink_context->audio_sink_handle) < 0)
            {
                pr_err("Failed to detach audio sink from encode stream\n");
            }
        }
        if (stm_se_encode_stream_delete(encode_context->audio_stream_handle) < 0)
        {
            pr_err("Failed to delete audio encode stream\n");
        }
    }
    if (encode_context->encode_handle)
    {
        if (stm_se_encode_delete(encode_context->encode_handle) < 0)
        {
            pr_err("Failed to delete encode\n");
        }
    }
}

static void MemsinkCleanup(memsink_context_t *memsink_context)
{
    if (memsink_context->audio_sink_handle)
    {
        if (stm_memsink_delete(memsink_context->audio_sink_handle) < 0)
        {
            pr_err("Error: %s Failed to delete audio memsink\n", __func__);
        }
    }
    if (memsink_context->video_sink_handle)
    {
        if (stm_memsink_delete(memsink_context->video_sink_handle) < 0)
        {
            pr_err("Error: %s Failed to delete video memsink\n", __func__);
        }
    }
}

void TestCleanUp(test_context_t *test_context)
{
    PlaybackCleanup(&test_context->playback_context, &test_context->encode_context);
    EncodeCleanup(&test_context->encode_context, &test_context->memsink_context);
    MemsinkCleanup(&test_context->memsink_context);
}


/////////////////////// VIDEO ///////////////////////////////////

static int EOSDiscontinuityCheckVideo(test_context_t *test_context)
{
    int Result = 0;
    playback_context_t *playback_context = &test_context->playback_context;
    encode_context_t   *encode_context = &test_context->encode_context;
    memsink_context_t  *memsink_context = &test_context->memsink_context;
    int i;
    bpa_buffer compressed_video_frame;
    bpa_buffer transcoded_video_frame;
    stm_se_framerate_t framerate = { 30 , 1 };

    // Create video play stream
    Result = stm_se_play_stream_new("Video MPEG2",
                                    playback_context->playback_handle,
                                    STM_SE_STREAM_ENCODING_VIDEO_MPEG2,
                                    &(playback_context->video_stream_handle));
    if (Result < 0)
    {
        pr_err("Error: %s Failed to create MPEG2 stream\n", __func__);
        return Result;
    }

    // Allocate Video Buffer for decode
    Result = AllocateBuffer(COMPRESSED_VIDEO_BUFFER_SIZE, &compressed_video_frame);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the video stream (%d)\n", __func__, COMPRESSED_VIDEO_BUFFER_SIZE, Result);
        return Result;
    }

    // Fill video buffer
    memcpy(compressed_video_frame.virtual, compressed_video_buffer, compressed_video_frame.size);

    Result = AllocateBuffer(TRANSCODED_BUFFER_SIZE, &transcoded_video_frame);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the transcoded video stream (%d)\n", __func__, TRANSCODED_BUFFER_SIZE, Result);
        FreeBuffer(&compressed_video_frame);
        return Result;
    }

    memset(transcoded_video_frame.virtual, 0, TRANSCODED_BUFFER_SIZE);
    gVideoTest[0].Sink.m_Buffer = (unsigned char *)transcoded_video_frame.virtual;
    gVideoTest[0].Sink.m_BufferLen = transcoded_video_frame.size;

    // Create encode stream
    Result = stm_se_encode_stream_new("Video Encode",
                                      encode_context->encode_handle,
                                      STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264,
                                      &(encode_context->video_stream_handle));
    if (Result < 0)
    {
        pr_err("Error: %s Failed to create encode stream\n", __func__);
        FreeBuffer(&transcoded_video_frame);
        FreeBuffer(&compressed_video_frame);
        return Result;
    }

    // Change the framerate
    pr_info("%s Set the framerate to %d/%d\n", __func__, framerate.framerate_num, framerate.framerate_den);
    Result = stm_se_encode_stream_set_compound_control(encode_context->video_stream_handle, STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE, &framerate);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to set framerate control\n", __func__);
        FreeBuffer(&transcoded_video_frame);
        FreeBuffer(&compressed_video_frame);
        return Result;
    }

    // Create a MemSink
    Result = stm_memsink_new("Video MemSink", STM_IOMODE_BLOCKING_IO, KERNEL, (stm_memsink_h *)&gVideoTest[0].Sink.obj);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to create a new memsink\n", __func__);
        FreeBuffer(&transcoded_video_frame);
        FreeBuffer(&compressed_video_frame);
        return Result;
    }
    memsink_context->video_sink_handle = gVideoTest[0].Sink.obj;

    // Attach the memsink
    Result = stm_se_encode_stream_attach(encode_context->video_stream_handle, memsink_context->video_sink_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to memsink\n", __func__);
        FreeBuffer(&transcoded_video_frame);
        FreeBuffer(&compressed_video_frame);
        return Result;
    }
    encode_context->is_video_sink_attached = true;

    // Attach the encode stream to play stream
    Result = stm_se_play_stream_attach(playback_context->video_stream_handle,
                                       encode_context->video_stream_handle,
                                       STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);
    if (Result)
    {
        pr_err("Error: %s Failed to attach encode stream to play stream\n", __func__);
        FreeBuffer(&transcoded_video_frame);
        FreeBuffer(&compressed_video_frame);
        return Result;
    }
    playback_context->is_video_encode_stream_attached = true;

    ///////////////////////////////////////////////////////////////////////
    //
    // Real World would perform data flow here...
    //
    // tunneled mode: Not using stm_se_encode_stream_inject_discontinuity()/stm_se_encode_stream_inject_frame()
    ///////////////////////////////////////////////////////////////////////

    // Call first stm_se_playback_set_speed() to force a drain of the encode stream
    // And check it does not affect further processing (test for bug 43228)
    stm_se_playback_set_speed(playback_context->playback_handle, 1001);

    // Transfert scenario: I frame - EOS
    for (i = 0; i < INJECT_COUNT; i++)
    {
        stm_se_play_stream_inject_data(playback_context->video_stream_handle, (void *)compressed_video_frame.virtual  , COMPRESSED_VIDEO_BUFFER_SIZE);
        stm_se_play_stream_inject_discontinuity(playback_context->video_stream_handle,
                                                false /* smooth_reverse */,
                                                false /* surplus_data */,
                                                true  /* end_of_stream */);
    }

    // delay to be sure that latest EOS is raised
    mdelay(100);

    pr_info("Check VIDEO encoded stream:\n");
    Result = EOSDiscontinuityCheckDataFlow(&compressed_video_frame, &gVideoTest[0].Sink, true);
    if (Result < 0)
    {
        pr_err("Error: %s data flow testing failed !\n", __func__);
        FreeBuffer(&transcoded_video_frame);
        FreeBuffer(&compressed_video_frame);
        return Result;
    }

    // Detach the encode stream from play stream
    Result = stm_se_play_stream_detach(playback_context->video_stream_handle,
                                       encode_context->video_stream_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to detach encode stream from play stream\n", __func__);
        FreeBuffer(&transcoded_video_frame);
        FreeBuffer(&compressed_video_frame);
        return Result;
    }
    playback_context->is_video_encode_stream_attached = false;

    // Detach the memsink
    Result = stm_se_encode_stream_detach(encode_context->video_stream_handle, memsink_context->video_sink_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to detach memsink from encode stream\n", __func__);
        FreeBuffer(&transcoded_video_frame);
        FreeBuffer(&compressed_video_frame);
        return Result;
    }
    encode_context->is_video_sink_attached = false;

    // Remove the memsink
    Result = stm_memsink_delete(memsink_context->video_sink_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to delete memsink\n", __func__);
        FreeBuffer(&transcoded_video_frame);
        FreeBuffer(&compressed_video_frame);
        return Result;
    }
    memsink_context->video_sink_handle = 0;

    // Free transcoded buffer
    Result = FreeBuffer(&transcoded_video_frame);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to free transcoded video buffer (%d)\n", __func__, Result);
        FreeBuffer(&compressed_video_frame);
        return Result;
    }

    gVideoTest[0].Sink.m_Buffer = NULL;
    gVideoTest[0].Sink.m_BufferLen = 0;

    // Free compressed buffer
    Result = FreeBuffer(&compressed_video_frame);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to free compressed video buffer (%d)\n", __func__, Result);
        return Result;
    }

    // Remove play stream and encode stream
    Result = stm_se_encode_stream_delete(encode_context->video_stream_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to delete encode stream\n", __func__);
        return Result;
    }
    encode_context->video_stream_handle = 0;

    Result = stm_se_play_stream_delete(playback_context->video_stream_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to delete play stream\n", __func__);
        return Result;
    }
    playback_context->video_stream_handle = 0;

    // Success
    return 0;
}

//////////////////////////// AUDIO //////////////////////////////////

static int EOSDiscontinuityCheckAudio(test_context_t *test_context)
{
    int Result = 0;
    playback_context_t *playback_context = &test_context->playback_context;
    encode_context_t   *encode_context = &test_context->encode_context;
    memsink_context_t  *memsink_context = &test_context->memsink_context;
    int i;
    bpa_buffer compressed_audio_frame;
    bpa_buffer transcoded_audio_frame;
    int transfer_size = 0;
    int offset = 0;
    stm_se_audio_bitrate_control_t bitrate_control;

    // Create the audio play stream
    Result = stm_se_play_stream_new("Audio AC3",
                                    playback_context->playback_handle,
                                    STM_SE_STREAM_ENCODING_AUDIO_AC3,
                                    &(playback_context->audio_stream_handle));
    if (Result < 0)
    {
        pr_err("Error: %s Failed to create AC3 stream\n", __func__);
        return Result;
    }

    // Allocate audio Buffer for decode
    Result = AllocateBuffer(COMPRESSED_AUDIO_BUFFER_SIZE, &compressed_audio_frame);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the audio stream (%d)\n", __func__, COMPRESSED_AUDIO_BUFFER_SIZE, Result);
        return Result;
    }

    // Fill audio buffer
    memcpy(compressed_audio_frame.virtual, compressed_audio_buffer, compressed_audio_frame.size);

    Result = AllocateBuffer(TRANSCODED_BUFFER_SIZE, &transcoded_audio_frame);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the transcoded audio stream (%d)\n", __func__, TRANSCODED_BUFFER_SIZE, Result);
        FreeBuffer(&compressed_audio_frame);
        return Result;
    }

    memset(transcoded_audio_frame.virtual, 0, TRANSCODED_BUFFER_SIZE);
    gAudioTest.Sink.m_Buffer = (unsigned char *)transcoded_audio_frame.virtual;
    gAudioTest.Sink.m_BufferLen = transcoded_audio_frame.size;

    // Create Stream
    Result = stm_se_encode_stream_new("Audio Encode", encode_context->encode_handle,
                                      STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AC3,
                                      &encode_context->audio_stream_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to create encode stream\n", __func__);
        FreeBuffer(&transcoded_audio_frame);
        FreeBuffer(&compressed_audio_frame);
        return Result;
    }

    // Change the birate
    memset(&bitrate_control, 0, sizeof(stm_se_audio_bitrate_control_t));
    bitrate_control.bitrate  = 384000;
    pr_info("%s Set the bitrate to %d\n", __func__, bitrate_control.bitrate);
    Result = stm_se_encode_stream_set_compound_control(encode_context->audio_stream_handle, STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL, (void *)&bitrate_control);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to set bitrate control\n", __func__);
        FreeBuffer(&transcoded_audio_frame);
        FreeBuffer(&compressed_audio_frame);
        return Result;
    }
    else
    {
        // Change to 5.1
        stm_se_ctrl_t control = STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT;
        stm_se_audio_core_format_t value;
        memset(&value, 0, sizeof(stm_se_audio_core_format_t)); //sets to bypass values.
        value.channel_placement.channel_count = 6; //6 channels.
        value.channel_placement.chan[0] = (uint8_t)STM_SE_AUDIO_CHAN_L;
        value.channel_placement.chan[1] = (uint8_t)STM_SE_AUDIO_CHAN_R;
        value.channel_placement.chan[2] = (uint8_t)STM_SE_AUDIO_CHAN_LFE;
        value.channel_placement.chan[3] = (uint8_t)STM_SE_AUDIO_CHAN_C;
        value.channel_placement.chan[4] = (uint8_t)STM_SE_AUDIO_CHAN_LS;
        value.channel_placement.chan[5] = (uint8_t)STM_SE_AUDIO_CHAN_RS;
        Result = stm_se_encode_stream_set_compound_control(encode_context->audio_stream_handle, control, (void *)&value);
        if (Result < 0)
        {
            pr_err("Error: %s Failed to set channel routing control\n", __func__);
            FreeBuffer(&transcoded_audio_frame);
            FreeBuffer(&compressed_audio_frame);
            return Result;
        }
    }

    // Create a MemSink
    Result = stm_memsink_new("Audio MemSink", STM_IOMODE_BLOCKING_IO, KERNEL, (stm_memsink_h *)&gAudioTest.Sink.obj);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to create a new memsink\n", __func__);
        FreeBuffer(&transcoded_audio_frame);
        FreeBuffer(&compressed_audio_frame);
        return Result;
    }
    memsink_context->audio_sink_handle = gAudioTest.Sink.obj;

    // Attach the memsink
    Result = stm_se_encode_stream_attach(encode_context->audio_stream_handle, memsink_context->audio_sink_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to memsink\n", __func__);
        FreeBuffer(&transcoded_audio_frame);
        FreeBuffer(&compressed_audio_frame);
        return Result;
    }
    encode_context->is_audio_sink_attached = true;

    // Attach the encode stream to play stream
    Result = stm_se_play_stream_attach(playback_context->audio_stream_handle,
                                       encode_context->audio_stream_handle,
                                       STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to play stream\n", __func__);
        FreeBuffer(&transcoded_audio_frame);
        FreeBuffer(&compressed_audio_frame);
        return Result;
    }
    playback_context->is_audio_encode_stream_attached = true;

    ///////////////////////////////////////////////////////////////////////
    //
    // Real World would perform data flow here...
    //
    // tunneled mode: Not using stm_se_encode_stream_inject_discontinuity()/stm_se_encode_stream_inject_frame()
    ///////////////////////////////////////////////////////////////////////

    // Call first stm_se_playback_set_speed() to force a drain of the encode stream
    // And check it does not affect further processing (test for bug 43228)
    stm_se_playback_set_speed(playback_context->playback_handle, 1002);

    // Transfert scenario (this pes stream extracted using strelay): audio samples - audio samples - audio samples - EOS
    for (i = 0; i < INJECT_COUNT; i++)
    {
        transfer_size = 2042;
        offset = 64;
        stm_se_play_stream_inject_data(playback_context->audio_stream_handle, (void *)compressed_audio_frame.virtual + offset  , transfer_size);
        pr_info("transfered %d bytes\n", transfer_size);
        transfer_size = 2037;
        offset = offset + 2042 + 64;
        stm_se_play_stream_inject_data(playback_context->audio_stream_handle, (void *)compressed_audio_frame.virtual + offset  , transfer_size);
        pr_info("transfered %d bytes\n", transfer_size);
        transfer_size = 2037;
        offset = offset + 2037 + 64;
        stm_se_play_stream_inject_data(playback_context->audio_stream_handle, (void *)compressed_audio_frame.virtual + offset  , transfer_size);
        pr_info("transfered %d bytes\n", transfer_size);
        // Latest data (34 bytes) of te stream are not needed
        transfer_size = 34;
        offset = offset + 2037 + 64;
        stm_se_play_stream_inject_data(playback_context->audio_stream_handle, (void *)compressed_audio_frame.virtual + offset  , transfer_size);
        pr_info("transfered %d bytes\n", transfer_size);
        stm_se_play_stream_inject_discontinuity(playback_context->audio_stream_handle,
                                                false /* smooth_reverse */,
                                                false /* surplus_data */,
                                                true  /* end_of_stream */);
    }

    // delay to be sure that latest EOS is raised
    mdelay(100);

    pr_info("Check AUDIO encoded stream:\n");
    Result = EOSDiscontinuityCheckDataFlow(&compressed_audio_frame, &gAudioTest.Sink, false);
    if (Result < 0)
    {
        pr_err("Error: %s data flow testing failed !\n", __func__);
        FreeBuffer(&transcoded_audio_frame);
        FreeBuffer(&compressed_audio_frame);
        return Result;
    }

    // Detach encode stream from play stream
    Result = stm_se_play_stream_detach(playback_context->audio_stream_handle,
                                       encode_context->audio_stream_handle);
    if (Result)
    {
        pr_err("Error: %s Failed to detach play stream from encode stream\n", __func__);
        FreeBuffer(&transcoded_audio_frame);
        FreeBuffer(&compressed_audio_frame);
        return Result;
    }
    playback_context->is_audio_encode_stream_attached = false;

    // Detach the memsink
    Result = stm_se_encode_stream_detach(encode_context->audio_stream_handle, memsink_context->audio_sink_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to detach encode stream from memsink\n", __func__);
        FreeBuffer(&transcoded_audio_frame);
        FreeBuffer(&compressed_audio_frame);
        return Result;
    }
    encode_context->is_audio_sink_attached = false;

    // Remove the memsink
    Result = stm_memsink_delete(memsink_context->audio_sink_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to delete memsink\n", __func__);
        FreeBuffer(&transcoded_audio_frame);
        FreeBuffer(&compressed_audio_frame);
        return Result;
    }
    memsink_context->audio_sink_handle = 0;

    // Free transcoded buffer
    Result = FreeBuffer(&transcoded_audio_frame);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to free transcoded audio buffer (%d)\n", __func__, Result);
        FreeBuffer(&compressed_audio_frame);
        return Result;
    }

    gAudioTest.Sink.m_Buffer = NULL;
    gAudioTest.Sink.m_BufferLen = 0;

    // Free compressed buffer
    Result = FreeBuffer(&compressed_audio_frame);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to free compressed audio buffer (%d)\n", __func__, Result);
        return Result;
    }

    // Remove play stream and encode stream
    Result = stm_se_encode_stream_delete(encode_context->audio_stream_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to delete encode stream\n", __func__);
        return Result;
    }
    encode_context->audio_stream_handle = 0;

    Result = stm_se_play_stream_delete(playback_context->audio_stream_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to delete play stream\n", __func__);
        return Result;
    }
    playback_context->audio_stream_handle = 0;

    // Success
    return 0;
}


static int ApplyNRTControl(encode_context_t *encode_context, int32_t control_value)
{
    int Result = 0;
    int32_t EncodeNRTCtrl;

    // Apply encode NRT control
    pr_info("Set encode NRT control to %d\n", control_value);
    Result = stm_se_encode_set_control(encode_context->encode_handle, STM_SE_CTRL_ENCODE_NRT_MODE, control_value);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to set encode NRT control\n", __func__);
        return Result;
    }

    // Check that control has correctly been applied
    pr_info("Check encode NRT control value has correctly been set\n");
    Result = stm_se_encode_get_control(encode_context->encode_handle, STM_SE_CTRL_ENCODE_NRT_MODE, &EncodeNRTCtrl);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to get encode NRT control\n", __func__);
        return Result;
    }
    if (EncodeNRTCtrl != control_value)
    {
        pr_err("Error: %s Failed to retrieve encode NRT control value %d (expected %d)\n", __func__, EncodeNRTCtrl, control_value);
        return -1;
    }

    // Success
    return 0;
}

static int EOSDiscontinuityCheck(bool ForceNRT)
{
    int Result = 0;
    test_context_t      test_context;
    playback_context_t *playback_context = &test_context.playback_context;
    encode_context_t   *encode_context = &test_context.encode_context;

    memset(&test_context, 0, sizeof(test_context_t));

    // Create playback and encode
    Result = stm_se_playback_new("playback0", &(playback_context->playback_handle));
    if (Result < 0)
    {
        pr_err("Error: %s Failed to create playback\n", __func__);
        TestCleanUp(&test_context);
        return Result;
    }

    Result = stm_se_encode_new("Encode0", &encode_context->encode_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to create encode\n", __func__);
        TestCleanUp(&test_context);
        return Result;
    }

    // Set normal play speed 1x and disable synchro
    stm_se_playback_set_speed(playback_context->playback_handle, 1000);
    stm_se_set_control(STM_SE_CTRL_ENABLE_SYNC, STM_SE_CTRL_VALUE_DISAPPLY);

    if (ForceNRT)
    {
        // Force NRT mode
        Result = ApplyNRTControl(encode_context, STM_SE_CTRL_VALUE_APPLY);
        if (Result < 0)
        {
            pr_err("Error: %s Failed to set encode NRT control\n", __func__);
            TestCleanUp(&test_context);
            return Result;
        }
    }

    // Check EOS discontinuity for video
    Result = EOSDiscontinuityCheckVideo(&test_context);
    if (Result < 0)
    {
        pr_err("Error: %s EOS discontinuity test for video FAILED\n", __func__);
        TestCleanUp(&test_context);
        return Result;
    }

    // Check EOS discontinuity for audio
    Result = EOSDiscontinuityCheckAudio(&test_context);
    if (Result < 0)
    {
        pr_err("Error: %s EOS discontinuity test for audio FAILED\n", __func__);
        TestCleanUp(&test_context);
        return Result;
    }

    // Delete playback and encode
    Result = stm_se_encode_delete(encode_context->encode_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to delete encode\n", __func__);
        TestCleanUp(&test_context);
        return Result;
    }
    encode_context->encode_handle = 0;

    Result = stm_se_playback_delete(playback_context->playback_handle);
    if (Result < 0)
    {
        pr_err("Error: %s Failed to delete playback\n", __func__);
        TestCleanUp(&test_context);
        return Result;
    }
    playback_context->playback_handle = 0;

    // Success
    return 0;
}

static int TestEOSDiscontinuityCheck(EncoderTestArg *arg)
{
    int Result;

    memset(&gVideoTest, 0, sizeof(gVideoTest));
    memset(&gAudioTest, 0, sizeof(gAudioTest));

    // Run test in default transcode mode
    pr_info("Test tunneled transcode in default mode:\n");
    Result = EOSDiscontinuityCheck(false);
    if (Result < 0)
    {
        pr_err("Error: %s EOSDiscontinuityCheck test in default transcode mode failed\n", __func__);
        return Result;
    }

    // Run test in NRT transcode mode
    pr_info("Test tunneled transcode in NRT mode:\n");
    pr_info("=> STM_SE_CTRL_ENCODE_NRT_MODE control set to STM_SE_CTRL_VALUE_APPLY\n");
    Result = EOSDiscontinuityCheck(true);
    if (Result < 0)
    {
        pr_err("Error: %s EOSDiscontinuityCheck test in NRT transcode mode failed\n", __func__);
        return Result;
    }

    // Success
    return 0;
}

ENCODER_TEST(TestEOSDiscontinuityCheck, "Check EOS discontinuity in transcode", BASIC_TEST);
