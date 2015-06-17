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

#define VIDEO_PREPROC_MIN_WIDTH      120 // fvdp-enc limit
#define VIDEO_PREPROC_MIN_HEIGHT      36 // fvdp-enc limit
#define VIDEO_PREPROC_MAX_WIDTH     1920
#define VIDEO_PREPROC_MAX_HEIGHT    1088
#define VIDEO_PREPROC_MIN_FRAMERATE    1
#define VIDEO_PREPROC_MAX_FRAMERATE   60

// CONTROL VALUE
const int32_t GOOD_VIDEO_PREPROC_DEINTERLACING[] =
{
    STM_SE_CTRL_VALUE_DISAPPLY, // MIN
    STM_SE_CTRL_VALUE_APPLY,    // MAX
};

const int32_t BAD_VIDEO_PREPROC_DEINTERLACING[] =
{
    STM_SE_CTRL_VALUE_DISAPPLY - 1, // MIN
    STM_SE_CTRL_VALUE_APPLY + 1,    // MAX
};

const int32_t GOOD_VIDEO_PREPROC_NOISE_FILTERING[] =
{
    STM_SE_CTRL_VALUE_DISAPPLY, // SUPPORTED
};

const int32_t BAD_VIDEO_PREPROC_NOISE_FILTERING[] =
{
    STM_SE_CTRL_VALUE_DISAPPLY - 1, // MIN
    STM_SE_CTRL_VALUE_APPLY + 1,    // MAX
    STM_SE_CTRL_VALUE_APPLY,        // UNSUPPORTED
};

const int32_t GOOD_VIDEO_PREPROC_DISPLAY_ASPECT_RATIO[] =
{
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE,  // MIN
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_16_BY_9, // MAX
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_4_BY_3,  // SUPPORTED
};

const int32_t BAD_VIDEO_PREPROC_DISPLAY_ASPECT_RATIO[] =
{
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE - 1,  // MIN
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_16_BY_9 + 1, // MAX
};

const int32_t GOOD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING[] =
{
    STM_SE_CTRL_VALUE_DISAPPLY, // MIN
    STM_SE_CTRL_VALUE_APPLY,    // MAX
};

const int32_t BAD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING[] =
{
    STM_SE_CTRL_VALUE_DISAPPLY - 1, // MIN
    STM_SE_CTRL_VALUE_APPLY + 1,    // MAX
};

// COMPOUND CONTROL VALUE
const int32_t GOOD_VIDEO_PREPROC_RESOLUTION[][2] =
{
    {VIDEO_PREPROC_MIN_WIDTH, VIDEO_PREPROC_MIN_HEIGHT}, // MIN
    {VIDEO_PREPROC_MAX_WIDTH, VIDEO_PREPROC_MAX_HEIGHT}, // MAX
    {1920, 1080},                                        // SUPPORTED
    {1920, 1088},                                        // SUPPORTED
    {1280, 720},                                         // SUPPORTED
};

const int32_t BAD_VIDEO_PREPROC_RESOLUTION[][2] =
{
    {VIDEO_PREPROC_MIN_WIDTH - 1, VIDEO_PREPROC_MIN_HEIGHT}, // MIN WIDTH
    {VIDEO_PREPROC_MAX_WIDTH + 1, VIDEO_PREPROC_MAX_HEIGHT}, // MAX WIDTH
    {VIDEO_PREPROC_MIN_WIDTH, VIDEO_PREPROC_MIN_HEIGHT - 1}, // MIN HEIGHT
    {VIDEO_PREPROC_MAX_WIDTH, VIDEO_PREPROC_MAX_HEIGHT + 1}, // MAX HEIGHT
    {4096, 2048},                                            // UNSUPPORTED
    {0, 0},                                                  // INVALID
};

const int32_t GOOD_VIDEO_PREPROC_FRAMERATE[][2] =
{
    {VIDEO_PREPROC_MIN_FRAMERATE, 1}, // MIN
    {VIDEO_PREPROC_MAX_FRAMERATE, 1}, // MAX
    {30, 1},                          // SUPPORTED
    {24, 1},                          // SUPPORTED
    {25, 1},                          // SUPPORTED
    {50, 1},                          // SUPPORTED
    {60000, 1001},                    // SUPPORTED
    {30000, 1001},                    // SUPPORTED
    {24000, 1001},                    // SUPPORTED
};

const int32_t BAD_VIDEO_PREPROC_FRAMERATE[][2] =
{
    {VIDEO_PREPROC_MIN_FRAMERATE - 1, 1}, // MIN
    {VIDEO_PREPROC_MAX_FRAMERATE + 1, 1}, // MAX
    {1, 0},                               // INVALID
    {0, 1},                               // INVALID
    {0, 0},                               // INVALID
};

// These tests are expected to fail ... returning success is a failure!
static int controls_fail(EncoderContext *context)
{
    int i = 0;
    int result = 0;
    int failures = 0;
    int readControlValue = 0;
    stm_se_picture_resolution_t Resolution;
    stm_se_framerate_t          Framerate;

    pr_info("%s: Testing Failure Cases - You could expect errors here\n", __func__);

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION Control not authorized as it is a structure to be passed
    result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, BAD_VIDEO_PREPROC_RESOLUTION[0][0]);

    if (result >= 0)
    {
        pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION\n", __func__);
        failures++;
    }

    result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, &readControlValue);

    if (result >= 0)
    {
        pr_err("Error: %s Expected failure on testing get invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION\n", __func__);
        failures++;
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION Compound Control
    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_RESOLUTION) / sizeof(BAD_VIDEO_PREPROC_RESOLUTION[0])) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_RESOLUTION[%d]\n", i);
        Resolution.width  = BAD_VIDEO_PREPROC_RESOLUTION[i][0];
        Resolution.height = BAD_VIDEO_PREPROC_RESOLUTION[i][1];
        result = stm_se_encode_stream_set_compound_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, &Resolution);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING Control
    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_DEINTERLACING) / sizeof(uint32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_DEINTERLACING[%d]\n", i);
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING, BAD_VIDEO_PREPROC_DEINTERLACING[i]);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING %u\n", __func__, BAD_VIDEO_PREPROC_DEINTERLACING[i]);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING Control
    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_NOISE_FILTERING) / sizeof(uint32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_NOISE_FILTERING[%d]\n", i);
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING, BAD_VIDEO_PREPROC_NOISE_FILTERING[i]);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING %u\n", __func__, BAD_VIDEO_PREPROC_NOISE_FILTERING[i]);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE Control not authorized as it is a structure to be passed
    result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE, BAD_VIDEO_PREPROC_FRAMERATE[0][0]);

    if (result >= 0)
    {
        pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE\n", __func__);
        failures++;
    }

    result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE, &readControlValue);

    if (result >= 0)
    {
        pr_err("Error: %s Expected failure on testing get invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE\n", __func__);
        failures++;
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE Compound Control
    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_FRAMERATE) / sizeof(BAD_VIDEO_PREPROC_FRAMERATE[0])) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_FRAMERATE[%d]\n", i);
        Framerate.framerate_num = BAD_VIDEO_PREPROC_FRAMERATE[i][0];
        Framerate.framerate_den = BAD_VIDEO_PREPROC_FRAMERATE[i][1];
        result = stm_se_encode_stream_set_compound_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE, &Framerate);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO Control
    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_DISPLAY_ASPECT_RATIO) / sizeof(uint32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_DISPLAY_ASPECT_RATIO[%d]\n", i);
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO, BAD_VIDEO_PREPROC_DISPLAY_ASPECT_RATIO[i]);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO %u\n", __func__, BAD_VIDEO_PREPROC_DISPLAY_ASPECT_RATIO[i]);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING Control
    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING) / sizeof(uint32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING[%d]\n", i);
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING, BAD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING[i]);

        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid control STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING %u\n", __func__, BAD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING[i]);
            failures++;
        }
    }

    pr_err("%s: Failure testing complete (%d)\n", __func__, failures);
    return failures;
}

// These tests are expected to pass ... returning success is a good thing
static int controls_pass(EncoderContext *context)
{
    int i = 0;
    int failures = 0;
    int result = 0;
    int readControlValue = 0;
    stm_se_picture_resolution_t setResolution;
    stm_se_picture_resolution_t getResolution;
    stm_se_framerate_t          setFramerate;
    stm_se_framerate_t          getFramerate;

    pr_info("%s: Testing Pass Cases - You should not expect errors here\n", __func__);

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION Compound Control
    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_RESOLUTION) / sizeof(GOOD_VIDEO_PREPROC_RESOLUTION[0])) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_RESOLUTION[%d]\n", i);
        setResolution.width  = GOOD_VIDEO_PREPROC_RESOLUTION[i][0];
        setResolution.height = GOOD_VIDEO_PREPROC_RESOLUTION[i][1];
        result = stm_se_encode_stream_set_compound_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, &setResolution);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_compound_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, &getResolution);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION video encode GET control\n", __func__);
            failures++;
        }

        if ((getResolution.width != setResolution.width) || (getResolution.height != setResolution.height))
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION video encode GET control\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING Control
    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_DEINTERLACING) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_DEINTERLACING[%d]\n", i);
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING, GOOD_VIDEO_PREPROC_DEINTERLACING[i]);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING, &readControlValue);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING video encode GET control\n", __func__);
            failures++;
        }

        if (readControlValue != GOOD_VIDEO_PREPROC_DEINTERLACING[i])
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING video encode GET control\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING Control
    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_NOISE_FILTERING) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_NOISE_FILTERING[%d]\n", i);
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING, GOOD_VIDEO_PREPROC_NOISE_FILTERING[i]);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING, &readControlValue);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING video encode GET control\n", __func__);
            failures++;
        }

        if (readControlValue != GOOD_VIDEO_PREPROC_NOISE_FILTERING[i])
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING video encode GET control\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE Compound Control
    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_FRAMERATE) / sizeof(GOOD_VIDEO_PREPROC_FRAMERATE[0])) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_FRAMERATE[%d]\n", i);
        setFramerate.framerate_num = GOOD_VIDEO_PREPROC_FRAMERATE[i][0];
        setFramerate.framerate_den = GOOD_VIDEO_PREPROC_FRAMERATE[i][1];
        result = stm_se_encode_stream_set_compound_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE, &setFramerate);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_compound_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE, &getFramerate);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE video encode GET control\n", __func__);
            failures++;
        }

        if ((getFramerate.framerate_num != setFramerate.framerate_num) || (getFramerate.framerate_den != setFramerate.framerate_den))
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE video encode GET control\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO Control
    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_DISPLAY_ASPECT_RATIO) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_DISPLAY_ASPECT_RATIO[%d]\n", i);
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO, GOOD_VIDEO_PREPROC_DISPLAY_ASPECT_RATIO[i]);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO, &readControlValue);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO video encode GET control\n", __func__);
            failures++;
        }

        if (readControlValue != GOOD_VIDEO_PREPROC_DISPLAY_ASPECT_RATIO[i])
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO video encode GET control\n", __func__);
            failures++;
        }
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING Control
    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING[%d]\n", i);
        result = stm_se_encode_stream_set_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING, GOOD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING[i]);

        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            failures++;
        }

        result = stm_se_encode_stream_get_control(context->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING, &readControlValue);

        if (result < 0)
        {
            pr_err("Error: %s Failed to perform STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING video encode GET control\n", __func__);
            failures++;
        }

        if (readControlValue != GOOD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING[i])
        {
            pr_err("Error: %s Failed to compare STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING video encode GET control\n", __func__);
            failures++;
        }
    }

    pr_info("%s: Pass testing complete (%d)\n", __func__, failures);
    return failures;
}

static int video_preproc_ctrl_test(EncoderTestArg *arg)
{
    EncoderContext context = {0};
    int result = 0;
    int failures = 0;
    // Test creating and destroying of a single EncodeStream
    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("%s: Failed to create a new Encode\n", __func__);
        return result;
    }

    // Create Stream
    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264, &context.video_stream[0]);

    if (result < 0)
    {
        pr_err("%s: Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream\n", __func__);
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
ENCODER_TEST(video_preproc_ctrl_test, "Test Video Preproc Controls", BASIC_TEST);
