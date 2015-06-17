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

#define H264_BITRATE 5000000
#define H264_GOP_SIZE     10
#define H264_LEVEL        40
#define H264_PROFILE     100
#define H264_CPB_SIZE      5

#define H264_MIN_BITRATE        1024
#define H264_MAX_BITRATE    60000000
#define H264_MIN_GOP_SIZE          0
#define H264_MAX_GOP_SIZE         -1
#define H264_MIN_CPB_SIZE       1024
#define H264_MAX_CPB_SIZE  288000000

// GOOD CONTROL VALUE
const int32_t GOOD_H264_BITRATE_MODE[] =
{
    STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR, // MIN
    STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_OFF, // MAX
};

const int32_t GOOD_H264_BITRATE[] =
{
    H264_MIN_BITRATE, // MIN
    H264_MAX_BITRATE, // MAX
    H264_BITRATE,     // SUPPORTED
};

const int32_t GOOD_H264_GOP_SIZE[] =
{
    H264_MIN_GOP_SIZE, // MIN
    H264_MAX_GOP_SIZE, // MAX
    H264_GOP_SIZE,     // SUPPORTED
};

const int32_t GOOD_H264_LEVEL[] =
{
    STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_0, // MIN
    STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_2, // MAX
    STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_2, // SUPPORTED
};

const int32_t GOOD_H264_PROFILE[] =
{
    STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_BASELINE, // MIN
    STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH,     // MAX
    STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_MAIN,     // SUPPORTED
};

const int32_t GOOD_H264_CPB_SIZE[] =
{
    H264_MIN_CPB_SIZE, // MIN
    H264_MAX_CPB_SIZE, // MAX
    H264_BITRATE * 2,  // SUPPORTED
};

// GOOD COMPOUND CONTROL VALUE
const uint32_t GOOD_H264_SLICE_MODE[] =
{
    STM_SE_VIDEO_MULTI_SLICE_MODE_SINGLE, // SUPPORTED
};

// BAD CONTROL VALUE
const int32_t BAD_H264_BITRATE_MODE[] =
{
    STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_CBR - 1, // MIN-1
    STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_OFF + 1, // MAX+1
    STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_CBR,     // UNSUPPORTED
};

const int32_t BAD_H264_BITRATE[] =
{
    H264_MIN_BITRATE - 1, // MIN-1
    H264_MAX_BITRATE + 1, // MAX+1
    -1,                   // UNSUPPORTED
    0,                    // INVALID
};

const int32_t BAD_H264_LEVEL[] =
{
    STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_0_9 - 1, // MIN-1
    STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_2 + 1, // MAX+1
    STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_0_9,     // UNSUPPORTED
    43,                                             // INVALID
};

const int32_t BAD_H264_PROFILE[] =
{
    STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_BASELINE - 1, // MIN-1
    STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_444 + 1, // MAX+1
    STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_EXTENDED,     // UNSUPPORTED
    STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_10,      // UNSUPPORTED
    STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_422,     // UNSUPPORTED
    STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_444,     // UNSUPPORTED
    99,                                                    // INVALID
};

const int32_t BAD_H264_CPB_SIZE[] =
{
    H264_MIN_CPB_SIZE - 1, // MIN-1
    H264_MAX_CPB_SIZE + 1, // MAX+1
    -1,                    // UNSUPPORTED
    0,                     // INVALID
};

// BAD COMPOUND CONTROL VALUE
const uint32_t BAD_H264_SLICE_MODE[] =
{
    STM_SE_VIDEO_MULTI_SLICE_MODE_SINGLE - 1,    // MIN-1
    STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_BYTES + 1, // MAX+1
    STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_BYTES,     // UNSUPPORTED
    STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_MB,        // UNSUPPORTED
};

// These tests are expected to fail ... returning success is a failure!
static int controls_fail(EncoderContext *context)
{
    int i = 0;
    int result = 0;
    int failures = 0;
    int readControlValue = 0;
    stm_se_video_multi_slice_t sliceMode;
    pr_info("%s Testing Failure Cases - You could expect errors here\n", __func__);

    // STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE Control
    for (i = 0 ; i < sizeof(BAD_H264_BITRATE_MODE) / sizeof(int32_t) ; i++)
    {
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE, BAD_H264_BITRATE_MODE[i]);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE %u\n", __func__, BAD_H264_BITRATE_MODE[i]);
            failures++;
        }
    }

    // STM_SE_CTRL_ENCODE_STREAM_BITRATE Control
    for (i = 0 ; i < sizeof(BAD_H264_BITRATE) / sizeof(int32_t) ; i++)
    {
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_ENCODE_STREAM_BITRATE, BAD_H264_BITRATE[i]);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_ENCODE_STREAM_BITRATE\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL Control
    for (i = 0 ; i < sizeof(BAD_H264_LEVEL) / sizeof(int32_t) ; i++)
    {
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL, BAD_H264_LEVEL[i]);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE Control
    for (i = 0 ; i < sizeof(BAD_H264_PROFILE) / sizeof(int32_t) ; i++)
    {
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE, BAD_H264_PROFILE[i]);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE Control
    for (i = 0 ; i < sizeof(BAD_H264_CPB_SIZE) / sizeof(int32_t) ; i++)
    {
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE, BAD_H264_CPB_SIZE[i]);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE Control not authorized as it is a structure to be passed
    result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE, GOOD_H264_SLICE_MODE[0]);

    if (result >= 0)
    {
        pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE\n", __func__);
        failures++;
    }

    result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE, &readControlValue);

    if (result >= 0)
    {
        pr_err("Error: %s Expected failure on testing get invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE\n", __func__);
        failures++;
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE Compound Control
    for (i = 0 ; i < sizeof(BAD_H264_SLICE_MODE) / sizeof(uint32_t) ; i++)
    {
        sliceMode.control = BAD_H264_SLICE_MODE[i];
        sliceMode.slice_max_mb_size = 0;
        sliceMode.slice_max_byte_size = 0;
        result = stm_se_encode_stream_set_compound_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE, &sliceMode);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE\n", __func__);
            failures++;
        }
    }

    pr_info("%s Failure testing complete (%d)\n", __func__, failures);
    return failures;
}

// These tests are expected to pass ... returning success is a good thing
static int controls_pass(EncoderContext *context)
{
    int i = 0;
    int failures = 0;
    int result = 0;
    int readControlValue = 0;
    stm_se_video_multi_slice_t setSliceMode;
    stm_se_video_multi_slice_t getSliceMode;

    pr_info("%s Testing Success Cases - You could expect errors here\n", __func__);

    // STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE Control
    for (i = 0 ; i < sizeof(GOOD_H264_BITRATE_MODE) / sizeof(int32_t) ; i++)
    {
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE, GOOD_H264_BITRATE_MODE[i]);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE, &readControlValue);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE video encode GET control\n", __func__);
            failures++;
        }

        if (readControlValue != GOOD_H264_BITRATE_MODE[i])
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE video encode GET control\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_ENCODE_STREAM_BITRATE Control
    for (i = 0 ; i < sizeof(GOOD_H264_BITRATE) / sizeof(int32_t) ; i++)
    {
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_ENCODE_STREAM_BITRATE, GOOD_H264_BITRATE[i]);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_ENCODE_STREAM_BITRATE, &readControlValue);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_ENCODE_STREAM_BITRATE video encode GET control\n", __func__);
            failures++;
        }

        if (readControlValue != GOOD_H264_BITRATE[i])
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_ENCODE_STREAM_BITRATE video encode GET control\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE Control
    for (i = 0 ; i < sizeof(GOOD_H264_GOP_SIZE) / sizeof(int32_t) ; i++)
    {
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE, GOOD_H264_GOP_SIZE[i]);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE, &readControlValue);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE video encode GET control\n", __func__);
            failures++;
        }

        if (readControlValue != GOOD_H264_GOP_SIZE[i])
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE video encode GET control\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL Control
    for (i = 0 ; i < sizeof(GOOD_H264_LEVEL) / sizeof(int32_t) ; i++)
    {
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL, GOOD_H264_LEVEL[i]);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL, &readControlValue);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL video encode GET control\n", __func__);
            failures++;
        }

        if (readControlValue != GOOD_H264_LEVEL[i])
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL video encode GET control\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE Control
    for (i = 0 ; i < sizeof(GOOD_H264_PROFILE) / sizeof(int32_t) ; i++)
    {
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE, GOOD_H264_PROFILE[i]);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE, &readControlValue);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE video encode GET control\n", __func__);
            failures++;
        }

        if (readControlValue != GOOD_H264_PROFILE[i])
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE video encode GET control\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE Control
    for (i = 0 ; i < sizeof(GOOD_H264_CPB_SIZE) / sizeof(int32_t) ; i++)
    {
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE, GOOD_H264_CPB_SIZE[i]);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE, &readControlValue);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE video encode GET control\n", __func__);
            failures++;
        }

        if (readControlValue != GOOD_H264_CPB_SIZE[i])
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE video encode GET control\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE Control
    for (i = 0 ; i < sizeof(GOOD_H264_SLICE_MODE) / sizeof(uint32_t) ; i++)
    {
        setSliceMode.control = GOOD_H264_SLICE_MODE[i];
        setSliceMode.slice_max_mb_size = 0;
        setSliceMode.slice_max_byte_size = 0;
        result = stm_se_encode_stream_set_compound_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE, &setSliceMode);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_compound_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE, &getSliceMode);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE video encode GET control\n", __func__);
            failures++;
        }

        if ((getSliceMode.control != setSliceMode.control) || (getSliceMode.slice_max_mb_size != setSliceMode.slice_max_mb_size) || (getSliceMode.slice_max_byte_size != setSliceMode.slice_max_byte_size))
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE video encode GET control\n", __func__);
            failures++;
        }
    }

    pr_info("%s Success testing complete (%d)\n", __func__, failures);
    return failures;
}

static int h264_controls_test(EncoderTestArg *arg)
{
    EncoderContext context = {0};
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
    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264, &context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream\n", __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    // Perform Controls Testing
    failures += controls_fail(&context);
    failures += controls_pass(&context);

    // Thats it! time to clear up and carry on (or go home)
    EncoderTestCleanup(&context);
    return failures;
}

/*** Only 40 Chars will be displayed ***/
ENCODER_TEST(h264_controls_test, "Test the H264 Encode Stream Controls", BASIC_TEST);
