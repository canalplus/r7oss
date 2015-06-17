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

#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/semaphore.h>

#include <stm_memsrc.h>
#include <stm_memsink.h>
#include <stm_event.h>

#include "encoder_test.h"
#include "common.h"

#include "audio_buffer_lolita_18432smp_48_2ch_s32se.h"

/* Whether CRC is checked */
#define CHECK_SIZE_AND_CRC         1
#define LESS_PRINTS                1
#define TEST_WITH_INJECTION_IN_US  0  //do PTS injection in microsec instead of pts, both should still pass checks
#define TEST_WITH_NEXT_PTS_NULL    0  //tests extraplation by injecting one non 0 PTS and then all '0' => PTS checks should still pass

#define TEST_WITH_PTS_WRAP         0  //starts with PTS = 1, for wrap around case (no check)

/* Input format */
#define AUDIO_BUFFER_SIZE              LOLITA_BUFFER_SIZE
#define AUDIO_IN_SAMPLING_FREQUENCY    LOLITA_SAMPLING_FREQUENCY
#define AUDIO_IN_PCM_FORMAT            LOLITA_PCM_FORMAT
#define AUDIO_IN_BYTES_PER_SAMPLE      LOLITA_BYTES_PER_SAMPLE
#define AUDIO_IN_EMPHASIS              LOLITA_EMPHASIS
#define AUDIO_IN_PROG_LEVEL            LOLITA_PROG_LEVEL
#define AUDIO_IN_NB_CHAN               LOLITA_NB_CHAN
#define AUDIO_IN_CHAN_0                LOLITA_CHAN_0
#define AUDIO_IN_CHAN_1                LOLITA_CHAN_1
#define AUDIO_IN_CHAN_2                LOLITA_CHAN_2
#define AUDIO_IN_CHAN_3                LOLITA_CHAN_3
#define AUDIO_IN_CHAN_4                LOLITA_CHAN_4
#define AUDIO_IN_CHAN_5                LOLITA_CHAN_5
#define AUDIO_IN_CHAN_6                LOLITA_CHAN_6
#define AUDIO_IN_CHAN_7                LOLITA_CHAN_7


//Fixed Codec Params
#define TEST_ENCODING_CODEC                        STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AAC
#define TEST_SAMPLES_PER_ES_FRAME                  1024
#define TEST_CODEC_INTERNAL_DELAY_FRAME            2
#define TEST_CODEC_MAX_BITS_PER_CH_PER_CODEC_FRAME 6144
#define TEST_CODEC_BYTES_HEADER_PER_FRAME          7

//Encoding Controls
#define AUDIO_OUT_BITRATE               192000
#define AUDIO_OUT_SAMPlING_FREQUENCY    AUDIO_IN_SAMPLING_FREQUENCY

/* Macro for flow control */
//Inject (FRAME_PER_INJECTION_TIMES_FACTOR / FRAME_DIVISION_FACTOR) frames every time
#define FRAME_DIVISION_FACTOR             16
#define FRAME_PER_INJECTION_TIMES_FACTOR  16
#define TEST_SAMPLES_PER_INJECTION            \
    (((FRAME_PER_INJECTION_TIMES_FACTOR)*(TEST_SAMPLES_PER_ES_FRAME))/FRAME_DIVISION_FACTOR)


//Local Macros
#define CHANNELS_PER_PAIR 2
#define BITS_PER_BYTE     8
#define ROUNDED_DIV(x,y) (((x)+((y)/2))/(y))
#define ROUNDUP_DIV(x,y) (((x)+((y)-1))/(y))

#define TEST_BYTES_PER_INJECTION              \
    ((TEST_SAMPLES_PER_INJECTION) * (AUDIO_IN_BYTES_PER_SAMPLE) * (AUDIO_IN_NB_CHAN))
#define TEST_TOTAL_NUMBER_SAMPLES              \
    ((AUDIO_BUFFER_SIZE)/((AUDIO_IN_NB_CHAN)*(AUDIO_IN_BYTES_PER_SAMPLE)))
#define TEST_TOTAL_NUMBER_FRAMES_INJECT              \
    (((TEST_TOTAL_NUMBER_SAMPLES)/(TEST_SAMPLES_PER_ES_FRAME)))
#define TEST_TOTAL_NUMBER_FRAMES_INJECT_WITH_EOF     \
    ((TEST_CODEC_INTERNAL_DELAY_FRAME) + (TEST_TOTAL_NUMBER_FRAMES_INJECT))
#define TEST_NUMBER_OF_INJECTIONS             \
    ROUNDUP_DIV((TEST_TOTAL_NUMBER_FRAMES_INJECT), (TEST_SAMPLES_PER_INJECTION))
/* Warning: AACE ES Frame size is max, not linked to bitrate */
#define TEST_CODED_ES_BUFFER_SIZE             \
    ((TEST_CODEC_BYTES_HEADER_PER_FRAME) + ROUNDUP_DIV(((TEST_CODEC_MAX_BITS_PER_CH_PER_CODEC_FRAME) * (AUDIO_IN_NB_CHAN)), 8))
#define TEST_NB_CODEC_BUFFER_PER_INJECTION    \
    ((TEST_CODEC_INTERNAL_DELAY_FRAME) + ROUNDUP_DIV((TEST_SAMPLES_PER_INJECTION), (TEST_SAMPLES_PER_ES_FRAME)))
#define TEST_OUT_MEMORY_PER_INJECTION         \
    (((TEST_NB_CODEC_BUFFER_PER_INJECTION) * (TEST_CODED_ES_BUFFER_SIZE)) +  sizeof (stm_se_capture_buffer_t))
#define TEST_MS_PER_INJECTION                 \
    (ROUNDED_DIV((TEST_SAMPLES_PER_INJECTION*10),ROUNDED_DIV((AUDIO_IN_SAMPLING_FREQUENCY),100)))
#define TEST_MS_PER_FRAME                     \
    (ROUNDED_DIV((TEST_SAMPLES_PER_ES_FRAME*10),ROUNDED_DIV((AUDIO_IN_SAMPLING_FREQUENCY),100)))

static const audio_expected_test_result_t Expected_Results[] =
{
    //Test 2.0 192kbps
    {
        20,
        {
            { 519 , 0xF0B61105, 0 },
            { 725 , 0x0266EE17, 0 },
            { 533 , 0xD1ED8055, 0 },
            { 536 , 0xF33B5E4C, 0 },
            { 492 , 0x45126FB6, 0 },
            { 483 , 0x84081C80, 0 },
            { 488 , 0xBD679478, 0 },
            { 509 , 0xF89E7C21, 0 },
            { 505 , 0x84AD3568, 0 },
            { 562 , 0x134BB89A, 0 },
            { 509 , 0x322144ae, 0 },
            { 379 , 0xb626101d, 0 },
            { 507 , 0x31ae0487, 0 },
            { 707 , 0x21560b7d, 0 },
            { 494 , 0xf10e2dd0, 0 },
            { 547 , 0x5489dffa, 0 },
            { 533 , 0x38ea5884, 0 },
            { 469 , 0xddefa391, 0 },
            { 549 , 0x4b40d6f5, 0 },
            { 379 , 0xce0d8667, 0 }
        }
    },
};

/// Global audio test context (communication between injection and callback)
//extern EncoderStkpiAudioTest_t gAudioTest;

static unsigned char gmBuffer[TEST_OUT_MEMORY_PER_INJECTION];

static int Fade_Out_In(EncoderTestArg *arg)
{
    EncoderContext context = { 0 };
    int result = 0;
    stm_se_audio_bitrate_control_t BitRateControl;
    memset(&BitRateControl, 0, sizeof(stm_se_audio_bitrate_control_t));
    memset(&gAudioTest, 0, sizeof(gAudioTest));
    gAudioTest.TestWithExtrapolation     = (0 != TEST_WITH_NEXT_PTS_NULL);
    gAudioTest.TestWithMicroSecInjection = (0 != TEST_WITH_INJECTION_IN_US);
    gAudioTest.TestWithPtsWrap           = (0 != TEST_WITH_PTS_WRAP);
    gAudioTest.MorePrints                = 0;//(0 == LESS_PRINTS);
    gAudioTest.InputVector               = lolita_audio_buffer;
    gAudioTest.InputVectorSize           = AUDIO_BUFFER_SIZE;
    gAudioTest.BytesPerSample            = AUDIO_IN_BYTES_PER_SAMPLE;
    gAudioTest.UMetadata.audio.emphasis                       = AUDIO_IN_EMPHASIS;
    gAudioTest.UMetadata.audio.program_level                  = AUDIO_IN_PROG_LEVEL;
    gAudioTest.UMetadata.audio.sample_format                  = AUDIO_IN_PCM_FORMAT;
    gAudioTest.UMetadata.audio.core_format.channel_placement.channel_count = AUDIO_IN_NB_CHAN;
    gAudioTest.UMetadata.audio.core_format.channel_placement.chan[0]       = (uint8_t)AUDIO_IN_CHAN_0;
    gAudioTest.UMetadata.audio.core_format.channel_placement.chan[1]       = (uint8_t)AUDIO_IN_CHAN_1;
    gAudioTest.UMetadata.audio.core_format.channel_placement.chan[2]       = (uint8_t)AUDIO_IN_CHAN_2;
    gAudioTest.UMetadata.audio.core_format.channel_placement.chan[3]       = (uint8_t)AUDIO_IN_CHAN_3;
    gAudioTest.UMetadata.audio.core_format.channel_placement.chan[4]       = (uint8_t)AUDIO_IN_CHAN_4;
    gAudioTest.UMetadata.audio.core_format.channel_placement.chan[5]       = (uint8_t)AUDIO_IN_CHAN_5;
    gAudioTest.UMetadata.audio.core_format.channel_placement.chan[6]       = (uint8_t)AUDIO_IN_CHAN_6;
    gAudioTest.UMetadata.audio.core_format.channel_placement.chan[7]       = (uint8_t)AUDIO_IN_CHAN_7;
    gAudioTest.UMetadata.audio.core_format.sample_rate  = AUDIO_IN_SAMPLING_FREQUENCY;
    gAudioTest.BytesPerInjection         = TEST_BYTES_PER_INJECTION;
    gAudioTest.SamplesPerInjection       = TEST_SAMPLES_PER_INJECTION;
    gAudioTest.MsPerInjection            = TEST_MS_PER_FRAME;
    gAudioTest.Sink.m_BufferLen = TEST_OUT_MEMORY_PER_INJECTION;
    gAudioTest.Sink.m_Buffer    = &gmBuffer[0];
    gAudioTest.InjectEosAfterLastFrame = false;
    gAudioTest.CollectFramesOnCallback = false;
    gAudioTest.FadeOutFrameNo          = 10;
    gAudioTest.MuteFrameNo             = 11;
    gAudioTest.FadeInFrameNo           = 12;
    // Create an Encode Object
    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new Encode\n", __func__);
        return result;
    }

    // Create Stream
    result = stm_se_encode_stream_new("EncodeStream0", context.encode, TEST_ENCODING_CODEC, &context.audio_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AAC EncodeStream\n", __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    // Create a MemSink
    result = stm_memsink_new("EncoderAudioSink", STM_IOMODE_BLOCKING_IO, KERNEL, (stm_memsink_h *)&gAudioTest.Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new memsink\n",  __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    // Attach the memsink
    result = stm_se_encode_stream_attach(context.audio_stream[0], gAudioTest.Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to memsink for audio\n", __func__);

        if (stm_memsink_delete((stm_memsink_h)gAudioTest.Sink.obj) < 0) // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object\n", __func__);
        }

        EncoderTestCleanup(&context);
        return result;
    }

    ///////////////////////////////////////////////////////////////////////
    BitRateControl.bitrate  = 192000;
    pr_info("\n%s[00]: Set the bitrate to %i\n", __func__, BitRateControl.bitrate);
    /* Change the birate */
    result = stm_se_encode_stream_set_compound_control(context.audio_stream[0], STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL, (void *)&BitRateControl);

    if (0 > result)
    {
        pr_err("Error: %s Failed to set bitrate control\n", __func__);
    }
    else
    {
        gAudioTest.ExpectedTestResult = (0 == CHECK_SIZE_AND_CRC) ? NULL : (audio_expected_test_result_t *)&Expected_Results[0];
        result = AudioFramesEncode(&context);
    }

    if (result < 0)
    {
        pr_err("Error: %s  Audio Encode FadeOut Failed\n", __func__);
        // Tidy up following test failure
        stm_se_encode_stream_detach(context.audio_stream[0], gAudioTest.Sink.obj);

        if (stm_memsink_delete((stm_memsink_h)gAudioTest.Sink.obj) < 0)
        {
            pr_err("Error: %s Failed to delete memsink object\n", __func__);
        }

        EncoderTestCleanup(&context);
        return result;
    }



    ///////////////////////////////////////////////////////////////////////
    // Detach the memsink
    result = stm_se_encode_stream_detach(context.audio_stream[0], gAudioTest.Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to detach encode stream from memsink\n", __func__);

        if (stm_memsink_delete((stm_memsink_h)gAudioTest.Sink.obj) < 0) // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object\n", __func__);
        }

        EncoderTestCleanup(&context);
        return result;
    }

    // Remove the memsink
    result = stm_memsink_delete((stm_memsink_h)gAudioTest.Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to detach encode stream from memsink\n", __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    // Remove Stream and Encode
    result = stm_se_encode_stream_delete(context.audio_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to delete EncodeStream %d\n", __func__, result);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_delete(context.encode);

    if (result < 0)
    {
        pr_err("Error: %s stm_se_encode_delete returned %d\n", __func__, result);
        return result;
    }

    // Success
    return result;
}

ENCODER_TEST(Fade_Out_In, "Check the FadeOut and FadeIn", BASIC_TEST);
