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

// These tests are expected to fail ... returning success is a failure!
static int controls_fail(EncoderContext *context)
{
    int result = 0;
    int failures = 0;
    int control_value;
    pr_err("Error: %s Testing Failure Cases - You could expect errors here\n", __func__);
    // These test simply verify that we aren't letting through unhandled controls incorrectly:
    result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_PLAYBACK_LATENCY, &control_value);

    if (result >= 0)
    {
        pr_err("Error: %s Expected failure on testing get invalid control\n", __func__);
        failures++;
    }

    result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_PLAYBACK_LATENCY, 500);

    if (result >= 0)
    {
        pr_err("Error: %s Expected failure on testing set invalid control\n", __func__);
        failures++;
    }

    // And of course - we need to protect against NULL pointers in the API
    result = stm_se_encode_stream_get_control(NULL, STM_SE_CTRL_UNKNOWN_OPTION, &control_value);

    if (result >= 0)
    {
        pr_err("Error: %s Expected failure on testing get NULL control\n", __func__);
        failures++;
    }

    result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_UNKNOWN_OPTION, NULL);

    if (result >= 0)
    {
        pr_err("Error: %s Expected failure on testing get NULL control\n", __func__);
        failures++;
    }

    result = stm_se_encode_stream_set_control(NULL, STM_SE_CTRL_UNKNOWN_OPTION, 0);

    if (result >= 0)
    {
        pr_err("Error: %s Expected failure on testing set NULL control\n", __func__);
        failures++;
    }

    pr_info("%s: Failure testing complete\n", __func__);
    return failures;
}

//Util function to test a simple control set/get
static void test_simple_control_pass(int *failures, stm_se_encode_stream_h stream, stm_se_ctrl_t control, char *control_name, uint32_t value)
{
    int result   = 0;
    uint32_t saved_value = value;
    bool test_set_equal_get = true;
    result = stm_se_encode_stream_set_control(stream, control, value);

    if (result < 0)
    {
        pr_err("Error: %s Expected success on set %s\n", __func__, control_name);
        (*failures)++;
        test_set_equal_get = false;
    }

    result = stm_se_encode_stream_get_control(stream, control, &value);

    if (result < 0)
    {
        pr_err("Error: %s Expected success on get %s\n", __func__, control_name);
        (*failures)++;
        test_set_equal_get = false;
    }

    if ((true == test_set_equal_get) && (saved_value != value))
    {
        pr_err("Error: %s Got %u instead of %u for %s\n", __func__, value, saved_value, control_name);
        (*failures)++;
    }
}

//Util function to test a compound control set/get
static void test_compound_control_pass(int *failures, stm_se_encode_stream_h stream, stm_se_ctrl_t control, char *control_name, uint32_t control_size, uint32_t *value, uint32_t *saved_value)
{
    int result   = 0;
    bool test_set_equal_get = true;
    result = stm_se_encode_stream_set_compound_control(stream, control, (void *)value);

    if (result < 0)
    {
        pr_err("Error: %s Expected success on set %s\n", __func__, control_name);
        (*failures)++;
        test_set_equal_get = false;
    }

    result = stm_se_encode_stream_get_compound_control(stream, control, (void *)value);

    if (result < 0)
    {
        pr_err("Error: %s Expected success on get %s\n", __func__, control_name);
        (*failures)++;
        test_set_equal_get = false;
    }

    if ((true == test_set_equal_get) && (0 != memcmp(value, saved_value, control_size)))
    {
        pr_err("Error: %s Returned compound value is not equal to set value for %s\n", __func__, control_name);
        (*failures)++;
    }
}

// These tests are expected to pass ... returning success is a good thing
static int controls_pass(EncoderContext *context)
{
    int failures = 0;
    //STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE
    {
        stm_se_encode_stream_h stream = context->audio_stream[0];
        stm_se_ctrl_t control = STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE;
        char control_name[] = "STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE";
        uint32_t value = STM_SE_CTRL_VALUE_APPLY;
        test_simple_control_pass(&failures, stream, control, control_name, value);
    }
    //STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS
    {
        stm_se_encode_stream_h stream = context->audio_stream[0];
        stm_se_ctrl_t control = STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS;
        char control_name[] = "STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS";
        uint32_t value = STM_SE_AUDIO_NO_FUTHER_COPY_AUTHORISED;
        test_simple_control_pass(&failures, stream, control, control_name, value);
    }
    //STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT
    {
        stm_se_encode_stream_h stream = context->audio_stream[0];
        stm_se_ctrl_t control = STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT;
        char control_name[] = "STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT";
        stm_se_audio_core_format_t value, saved_value;
        uint32_t control_size = sizeof(stm_se_audio_core_format_t);
        memset(&value, 0, control_size);
        value.sample_rate = 44100;
        value.channel_placement.channel_count = 2;
        value.channel_placement.chan[0] = (uint8_t)STM_SE_AUDIO_CHAN_L;
        value.channel_placement.chan[1] = (uint8_t)STM_SE_AUDIO_CHAN_R;
        memcpy(&saved_value, &value, control_size);
        test_compound_control_pass(&failures, stream, control, control_name, control_size, (uint32_t *)&value, (uint32_t *)&saved_value);
    }
    //STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL
    {
        stm_se_encode_stream_h stream = context->audio_stream[0];
        stm_se_ctrl_t control = STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL;
        char control_name[] = "STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL";
        stm_se_audio_bitrate_control_t value, saved_value;
        uint32_t control_size = sizeof(stm_se_audio_bitrate_control_t);
        memset(&value, 0, control_size);
        value.bitrate = 192000;
        value.is_vbr = false;
        memcpy(&saved_value, &value, control_size);
        test_compound_control_pass(&failures, stream, control, control_name, control_size, (uint32_t *)&value, (uint32_t *)&saved_value);
    }
    //STM_SE_CTRL_STREAM_DRIVEN_DUALMONO
    {
        stm_se_encode_stream_h stream = context->audio_stream[0];
        stm_se_ctrl_t control = STM_SE_CTRL_STREAM_DRIVEN_DUALMONO;
        char control_name[] = "STM_SE_CTRL_STREAM_DRIVEN_DUALMONO";
        uint32_t value = STM_SE_CTRL_VALUE_USER_ENFORCED_DUALMONO;
        test_simple_control_pass(&failures, stream, control, control_name, value);
    }
    //STM_SE_CTRL_DUALMONO
    {
        stm_se_encode_stream_h stream = context->audio_stream[0];
        stm_se_ctrl_t control = STM_SE_CTRL_DUALMONO;
        char control_name[] = "STM_SE_CTRL_DUALMONO";
        uint32_t value = STM_SE_MONO_OUT;
        test_simple_control_pass(&failures, stream, control, control_name, value);
    }
    return failures;
}

static int controls_test(EncoderTestArg *arg)
{
    EncoderContext context = { 0 };
    int result = 0;
    int failures = 0;
    // Test creating and destroying of a single EncodeStream
    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new Encode\n", __func__);
        return result;
    }

    // Create Stream
    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL, &context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL EncodeStream\n", __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    // Create Audio Stream (we do not care which as long as not AUDIO_NULL)
    /* Try AAC */
    result = stm_se_encode_stream_new("EncodeStream1", context.encode, STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AAC, &context.audio_stream[0]);

    if (result < 0)
    {
        /* Try AC-3 */
        result = stm_se_encode_stream_new("EncodeStream1", context.encode, STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AC3, &context.audio_stream[0]);

        if (result < 0)
        {
            /* Try MP3 */
            result = stm_se_encode_stream_new("EncodeStream1", context.encode, STM_SE_ENCODE_STREAM_ENCODING_AUDIO_MP3, &context.audio_stream[0]);

            if (result < 0)
            {
                pr_err("Error: %s Failed to create a new audio stream, with any codec\n", __func__);
                EncoderTestCleanup(&context);
                return result;
            }
        }
    }

    // Perform Controls Testing
    failures += controls_fail(&context);
    failures += controls_pass(&context);
    // Thats it! time to clear up and carry on (or go home)
    EncoderTestCleanup(&context);
    return failures;
}

/*** Only 40 Chars will be displayed ***/
ENCODER_TEST(controls_test, "Test the Encode Stream Controls", BASIC_TEST);
