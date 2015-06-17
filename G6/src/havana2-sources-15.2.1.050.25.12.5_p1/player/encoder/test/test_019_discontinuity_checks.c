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

// For Jiffies/
#include <linux/sched.h>

#include <linux/delay.h>

#include <stm_memsrc.h>
#include <stm_memsink.h>

// Some locals
#define VIDEO_WIDTH  640
#define VIDEO_HEIGHT 480
#define VIDEO_TEST_BUFFER_SIZE (VIDEO_WIDTH*VIDEO_HEIGHT*2)
#define AUDIO_TEST_BUFFER_SIZE (1536*4*2)
#define TEST_BUFFER_SIZE VIDEO_TEST_BUFFER_SIZE
#define COMPRESSED_BUFFER_SIZE TEST_BUFFER_SIZE

#define DISCONTINUITY_LOOP            17
#define DISCONTINUITY_INPUT           0
#define DISCONTINUITY_COMMAND         1
#define DISCONTINUITY_OUTPUT          2
#define DISCONTINUITY_AUDIO           0
#define DISCONTINUITY_VIDEO           1

// Discontinuity Tests

static stm_se_discontinuity_t DiscontinuityLUT[DISCONTINUITY_LOOP][2] =
{
// input metadata, inject discontinuity command, expected output metatdata (buf1, buf2 )
// 0  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_DISCONTINUOUS
// 1  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CONTINUOUS               => expect STM_SE_DISCONTINUITY_DISCONTINUOUS from test 0
    {STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_DISCONTINUOUS     },
    {STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS        },
// 2  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
// 3  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CONTINUOUS               => expect STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST from test 2
    {STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST},
    {STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS        },
// 4  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS
    {STM_SE_DISCONTINUITY_DISCONTINUOUS,      STM_SE_DISCONTINUITY_CONTINUOUS        },
// 5  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
    {STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST, STM_SE_DISCONTINUITY_CONTINUOUS        },
// 6  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_EOS
    {STM_SE_DISCONTINUITY_EOS,                STM_SE_DISCONTINUITY_CONTINUOUS        },
// 7  - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS
// 8  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS                     => expect STM_SE_DISCONTINUITY_DISCONTINUOUS from test 7
    {STM_SE_DISCONTINUITY_DISCONTINUOUS,      STM_SE_DISCONTINUITY_CONTINUOUS        },
    {STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS        },
// 9  - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
// 10 - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS               => expect (STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST|STM_SE_DISCONTINUITY_DISCONTINUOUS) from test 9
    {STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST, STM_SE_DISCONTINUITY_CONTINUOUS        },
    {STM_SE_DISCONTINUITY_DISCONTINUOUS,      STM_SE_DISCONTINUITY_CONTINUOUS        },
// 11 - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_EOS
    {STM_SE_DISCONTINUITY_EOS,                STM_SE_DISCONTINUITY_CONTINUOUS        },
// 12 - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_FADEOUT
    {STM_SE_DISCONTINUITY_FADEOUT,            STM_SE_DISCONTINUITY_CONTINUOUS        },
// 13 - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_MUTE
    {STM_SE_DISCONTINUITY_MUTE,               STM_SE_DISCONTINUITY_CONTINUOUS        },
// 14 - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_MUTE
    {STM_SE_DISCONTINUITY_MUTE,               STM_SE_DISCONTINUITY_CONTINUOUS        },
// 15 - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_FADEIN
    {STM_SE_DISCONTINUITY_FADEIN,             STM_SE_DISCONTINUITY_CONTINUOUS        },
// 16 - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS
    {STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS        },
};

static stm_se_discontinuity_t AResultsLUT[DISCONTINUITY_LOOP][4] =
{
// 0  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_DISCONTINUOUS
// 1  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CONTINUOUS               => expect STM_SE_DISCONTINUITY_DISCONTINUOUS from test 0
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
    { STM_SE_DISCONTINUITY_DISCONTINUOUS,      STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 2  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
// 3  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CONTINUOUS               => expect STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST from test 2
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
    { STM_SE_DISCONTINUITY_CONTINUOUS        , STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 4  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS
    { STM_SE_DISCONTINUITY_CONTINUOUS       , STM_SE_DISCONTINUITY_DISCONTINUOUS                                   , 0, 0},
// 5  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 6  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_EOS
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS,   STM_SE_DISCONTINUITY_EOS            , 0},
// 7  - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS
// 8  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS                     => expect STM_SE_DISCONTINUITY_DISCONTINUOUS from test 7
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_DISCONTINUOUS                                        , 0, 0},
// 9  - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
// 10 - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS               => expect (STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST|STM_SE_DISCONTINUITY_DISCONTINUOUS) from test 9
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
    { STM_SE_DISCONTINUITY_DISCONTINUOUS,      STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 11 - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_EOS
    { STM_SE_DISCONTINUITY_CONTINUOUS, STM_SE_DISCONTINUITY_EOS                                                     , 0, 0},
// 12  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_FADEOUT
    { STM_SE_DISCONTINUITY_FADEOUT,         STM_SE_DISCONTINUITY_CONTINUOUS                                         , 0, 0},
// 13  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_MUTE
    { STM_SE_DISCONTINUITY_MUTE,         STM_SE_DISCONTINUITY_CONTINUOUS                                            , 0, 0},
// 14  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_MUTE
    { STM_SE_DISCONTINUITY_MUTE,         STM_SE_DISCONTINUITY_CONTINUOUS                                            , 0, 0},
// 15  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_FADEIN
    { STM_SE_DISCONTINUITY_FADEIN,         STM_SE_DISCONTINUITY_CONTINUOUS                                          , 0, 0},
// 16  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                      , 0, 0}
};

static stm_se_discontinuity_t VResultsLUT[DISCONTINUITY_LOOP][4] =
{
// 0  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_DISCONTINUOUS
// 1  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CONTINUOUS               => expect STM_SE_DISCONTINUITY_DISCONTINUOUS from test 0
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
    { STM_SE_DISCONTINUITY_DISCONTINUOUS,      STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 2  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
// 3  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CONTINUOUS               => expect STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST from test 2
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
    { STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST, STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 4  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS
    { STM_SE_DISCONTINUITY_DISCONTINUOUS,      STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 5  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
    { STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST, STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 6  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_EOS
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_EOS                                            , 0, 0},
// 7  - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS
// 8  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS                     => expect STM_SE_DISCONTINUITY_DISCONTINUOUS from test 7
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
    { STM_SE_DISCONTINUITY_DISCONTINUOUS,      STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 9  - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
// 10 - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS               => expect (STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST|STM_SE_DISCONTINUITY_DISCONTINUOUS) from test 9
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
    { (STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST | STM_SE_DISCONTINUITY_DISCONTINUOUS), STM_SE_DISCONTINUITY_CONTINUOUS, 0, 0},
// 11 - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_EOS
    { STM_SE_DISCONTINUITY_EOS,                STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 12  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 13  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 14  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 15  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0},
// 16  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS
    { STM_SE_DISCONTINUITY_CONTINUOUS,         STM_SE_DISCONTINUITY_CONTINUOUS                                     , 0, 0}
};



static unsigned int BufferSizeLUT[DISCONTINUITY_LOOP][2] =
{
// Audio buffer size, Video buffer size
// 0  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_DISCONTINUOUS
// 1  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CONTINUOUS               => expect STM_SE_DISCONTINUITY_DISCONTINUOUS from test 0
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
// 2  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
// 3  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CONTINUOUS               => expect STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST from test 2
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
// 4  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
// 5  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
// 6  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_EOS
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
// 7  - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS
// 8  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS                     => expect STM_SE_DISCONTINUITY_DISCONTINUOUS from test 7
    {0, 0},
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
// 9  - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
// 10 - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS               => expect (STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST|STM_SE_DISCONTINUITY_DISCONTINUOUS) from test 9
    {AUDIO_TEST_BUFFER_SIZE, 0},
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
// 11 - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_EOS
    {0, 0},
// 12  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_FADEOUT
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
// 13  - stm_se_encode_stream_inject_discontinuity  STM_SE_DISCONTINUITY_MUTE
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
// 14  - stm_se_encode_stream_inject_discontinuity  STM_SE_DISCONTINUITY_MUTE
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
// 15  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_FADEIN
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
// 16  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CONTINUOUS
    {AUDIO_TEST_BUFFER_SIZE, VIDEO_TEST_BUFFER_SIZE},
};


// Number of expected buffers [0-2]
static unsigned int NbOutputBuffers[DISCONTINUITY_LOOP][2] =
{
// 0  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_DISCONTINUOUS
// 1  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CONTINUOUS               => expect STM_SE_DISCONTINUITY_DISCONTINUOUS from test 0
    {1, 1},
    {1, 1},
// 2  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
// 3  - stm_se_encode_stream_inject_discontinuity STM_SE_DISCONTINUITY_CONTINUOUS               => expect STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST from test 2
    {1, 1},
    {1, 1},
// 4  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS
    {2, 1},
// 5  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
    {1, 1},
// 6  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_EOS
    {3, 2},
// 7  - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS
// 8  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS                     => expect STM_SE_DISCONTINUITY_DISCONTINUOUS from test 7
    {0, 0}, // Fake buffer in => no buffer out
    {2, 1},
// 9  - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST
// 10 - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_DISCONTINUOUS               => expect (STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST|STM_SE_DISCONTINUITY_DISCONTINUOUS) from test 9
    {1, 0}, // Fake buffer in => no buffer out
    {1, 1},
// 11 - input null buffer with discontinuity metadata STM_SE_DISCONTINUITY_EOS
    {2, 1}, // Fake buffer in => Fake buffer out (true buffer after audio drain added: there was data injected after the DISCONTINUITY  in previous test)
// 12  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_FADEOUT
    {1, 1},
// 13  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_MUTE
    {1, 1},
// 14  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_MUTE
    {1, 1},
// 15  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_FADEIN
    {1, 1},
// 16  - input buffer with discontinuity metadata STM_SE_DISCONTINUITY_CONTINUOUS
    {1, 1}
};

static int PullCompressedEncode(pp_descriptor *memsinkDescriptor, stm_se_discontinuity_t discontinuity, int idx, bool video)
{
    int retval;
    stm_se_capture_buffer_t *p_memsink_pull_buffer;
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

        mdelay(10); //in ms

        if (loop == 100)
        {
            pr_err("PullCompressedEncode : Nothing available\n");
            return 0;
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

    // Check discontinuity
    if (p_memsink_pull_buffer->u.compressed.discontinuity != discontinuity)
    {
        pr_err("PullCompressedEncode : %s Frame %d : KO: Expected discontinuity 0x%x but got 0x%x instead! Payload %d\n\n",
               (video ? "Video" : "Audio"), idx, discontinuity, p_memsink_pull_buffer->u.compressed.discontinuity, p_memsink_pull_buffer->payload_length);
        return -1;
    }
    else
    {
        pr_info("PullCompressedEncode : %s Frame %d : OK: got expected discontinuity 0x%x Payload %d\n\n",
                (video ? "Video" : "Audio"), idx, discontinuity, p_memsink_pull_buffer->payload_length);
    }

    return 1;
}

static int SendAudioBufferToStream(bpa_buffer *buffer, stm_se_encode_stream_h stream)
{
    int result = 0;
    static int nbframe = 0;
    stm_se_uncompressed_frame_metadata_t descriptor;
    stm_se_discontinuity_t               discontinuity;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.media = STM_SE_ENCODE_STREAM_MEDIA_AUDIO;
    descriptor.audio.core_format.sample_rate   = 48000;
    descriptor.audio.core_format.channel_placement.channel_count = 2;
    descriptor.audio.core_format.channel_placement.chan[0]       = (uint8_t)STM_SE_AUDIO_CHAN_L;
    descriptor.audio.core_format.channel_placement.chan[1]       = (uint8_t)STM_SE_AUDIO_CHAN_R;
    descriptor.audio.sample_format = STM_SE_AUDIO_PCM_FMT_S32LE;
    descriptor.native_time   = (0xfeedfacedeadbeefULL); //Undefined to avoid messages
    descriptor.system_time   = jiffies;
    discontinuity            = (stm_se_discontinuity_t)(DiscontinuityLUT[nbframe][DISCONTINUITY_INPUT] & (~STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST));
    descriptor.discontinuity = discontinuity;
    result = stm_se_encode_stream_inject_frame(stream,  buffer->virtual, buffer->physical, BufferSizeLUT[nbframe][DISCONTINUITY_AUDIO], descriptor);

    if (result < 0)
    {
        pr_err("Error: %s Failed to send buffer\n", __func__);
    }

    nbframe ++;
    return result;
}

static int FillBuffer(void *buffer)
{
    memset(buffer, 0x80, TEST_BUFFER_SIZE);
    return 0;
}
static int SendVideoBufferToStream(bpa_buffer *buffer, stm_se_encode_stream_h stream)
{
    int result = 0;
    static int nbframe = 0;
    stm_se_uncompressed_frame_metadata_t descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.media = STM_SE_ENCODE_STREAM_MEDIA_VIDEO;
    descriptor.video.frame_rate.framerate_num                        = 25;
    descriptor.video.frame_rate.framerate_den                        = 1;
    descriptor.video.video_parameters.width                          = VIDEO_WIDTH;
    descriptor.video.video_parameters.height                         = VIDEO_HEIGHT;
    descriptor.video.video_parameters.colorspace                     = STM_SE_COLORSPACE_SMPTE170M;
    descriptor.video.video_parameters.aspect_ratio                   = STM_SE_ASPECT_RATIO_4_BY_3;
    descriptor.video.video_parameters.pixel_aspect_ratio_numerator   = 1;
    descriptor.video.video_parameters.pixel_aspect_ratio_denominator = 1;
    descriptor.video.video_parameters.scan_type                      = STM_SE_SCAN_TYPE_PROGRESSIVE;
    descriptor.video.pitch                                           = (VIDEO_WIDTH * 2);
    descriptor.video.window_of_interest                              = (stm_se_picture_rectangle_t) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT };
    descriptor.video.picture_type                                    = STM_SE_PICTURE_TYPE_UNKNOWN;
    descriptor.video.surface_format                                  = SURFACE_FORMAT_VIDEO_422_RASTER;
    descriptor.video.vertical_alignment                              = 1;
    descriptor.native_time_format = TIME_FORMAT_PTS;
    descriptor.native_time   = jiffies / 10;
    descriptor.system_time   = jiffies;
    descriptor.discontinuity = DiscontinuityLUT[nbframe][DISCONTINUITY_INPUT] & (~STM_SE_DISCONTINUITY_FADEOUT) & (~STM_SE_DISCONTINUITY_MUTE) & (~STM_SE_DISCONTINUITY_FADEIN);
    result = stm_se_encode_stream_inject_frame(stream,  buffer->virtual, buffer->physical, BufferSizeLUT[nbframe][DISCONTINUITY_VIDEO], descriptor);

    if (result < 0)
    {
        pr_err("Error: %s Failed to send buffer\n", __func__);
    }

    nbframe ++;
    return result;
}

static int DiscontinuityChecksDataFlow(bpa_buffer *buffer, EncoderContext *context, pp_descriptor *videosink, pp_descriptor *audiosink)
{
    int result = 0;
    int i, j;
    stm_se_discontinuity_t discontinuity;

    /* Now send some buffers to be coded */
    for (i = 0; i < DISCONTINUITY_LOOP; i++)
    {
        pr_info("Sending test buffer to Video Stream %d\n", i);
        result = SendVideoBufferToStream(buffer, context->video_stream[0]);

        if (result < 0)
        {
            return result;
        }

        discontinuity = DiscontinuityLUT[i][DISCONTINUITY_COMMAND] & (~STM_SE_DISCONTINUITY_FADEOUT) & (~STM_SE_DISCONTINUITY_MUTE) & (~STM_SE_DISCONTINUITY_FADEIN);

        if (discontinuity != STM_SE_DISCONTINUITY_CONTINUOUS)
        {
            result = stm_se_encode_stream_inject_discontinuity(context->video_stream[0], discontinuity);

            if (result < 0)
            {
                pr_err("Error: %s Failed to inject discontinuity\n", __func__);
            }
        }

        pr_info("Trying to get encoded Video Stream %d\n", i);

        for (j = 0; j < NbOutputBuffers[i][DISCONTINUITY_VIDEO]; j++)
        {
            discontinuity = (stm_se_discontinuity_t)VResultsLUT[i][j];
            result = PullCompressedEncode(videosink, discontinuity, i, true);

            if (result < 0)
            {
                return result;
            }
        }

        pr_info("Sending test buffer to Audio Stream %d\n", i);
        result = SendAudioBufferToStream(buffer, context->audio_stream[0]);

        if (result < 0)
        {
            return result;
        }

        discontinuity = (stm_se_discontinuity_t)(DiscontinuityLUT[i][DISCONTINUITY_COMMAND] & (~STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST));

        if (discontinuity != STM_SE_DISCONTINUITY_CONTINUOUS)
        {
            result = stm_se_encode_stream_inject_discontinuity(context->audio_stream[0], discontinuity);

            if (result < 0)
            {
                pr_err("Error: %s Failed to inject discontinuity\n", __func__);
            }
        }

        pr_info("Trying to get encoded Audio Stream %d\n", i);

        for (j = 0; j < NbOutputBuffers[i][DISCONTINUITY_AUDIO]; j++)
        {
            discontinuity = (stm_se_discontinuity_t)(AResultsLUT[i][j]);
            result = PullCompressedEncode(audiosink, discontinuity, i, false);

            if (result < 0)
            {
                return result;
            }
        }
    }

    return result;
}

static int DiscontinuityChecks(EncoderTestArg *arg)
{
    EncoderContext context = {0};
    int result = 0;
    bpa_buffer frame;
    bpa_buffer compressed_video_frame;
    bpa_buffer compressed_audio_frame;

    memset(&gVideoTest, 0, sizeof(gVideoTest));
    memset(&gAudioTest, 0, sizeof(gAudioTest));

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
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the video stream (%d)\n", __func__, COMPRESSED_BUFFER_SIZE, result);
        FreeBuffer(&frame);
        return result;
    }

    memset(compressed_video_frame.virtual, 0, COMPRESSED_BUFFER_SIZE);
    gVideoTest[0].Sink.m_Buffer = (unsigned char *)compressed_video_frame.virtual;
    gVideoTest[0].Sink.m_BufferLen = compressed_video_frame.size;

    result = AllocateBuffer(COMPRESSED_BUFFER_SIZE, &compressed_audio_frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the audio stream (%d)\n", __func__, COMPRESSED_BUFFER_SIZE, result);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        return result;
    }

    memset(compressed_audio_frame.virtual, 0, COMPRESSED_BUFFER_SIZE);
    gAudioTest.Sink.m_Buffer = (unsigned char *)compressed_audio_frame.virtual;
    gAudioTest.Sink.m_BufferLen = compressed_audio_frame.size;

    // Initialize buffer
    FillBuffer(frame.virtual);
    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new Encode\n", __func__);
        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        return result;
    }

    // Create Stream
    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264, &context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream", __func__);
        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_new("EncodeStream1", context.encode, STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AC3, &context.audio_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AC3 EncodeStream", __func__);
        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    // Create a MemSink
    result = stm_memsink_new("EncoderVideoSink", STM_IOMODE_BLOCKING_IO, KERNEL, (stm_memsink_h *)&gVideoTest[0].Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new memsink", __func__);
        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_memsink_new("EncoderAudioSink", STM_IOMODE_BLOCKING_IO, KERNEL, (stm_memsink_h *)&gAudioTest.Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new memsink", __func__);
        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    // Attach the memsink
    result = stm_se_encode_stream_attach(context.video_stream[0], gVideoTest[0].Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to memsink for video", __func__);

        if (stm_memsink_delete((stm_memsink_h)gVideoTest[0].Sink.obj) < 0)    // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object", __func__);
        }

        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_attach(context.audio_stream[0], gAudioTest.Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to memsink for audio", __func__);

        if (stm_memsink_delete((stm_memsink_h)gAudioTest.Sink.obj) < 0)    // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object", __func__);
        }

        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    ///////////////////////////////////////////////////////////////////////
    //
    // Real World would perform data flow here...
    //
    result = DiscontinuityChecksDataFlow(&frame, &context, &gVideoTest[0].Sink, &gAudioTest.Sink);

    if (result < 0)
    {
        pr_err("Error: %s Data Flow Testing Failed\n", __func__);
        // Tidy up following test failure
        stm_se_encode_stream_detach(context.video_stream[0], gVideoTest[0].Sink.obj);

        if (stm_memsink_delete((stm_memsink_h)gVideoTest[0].Sink.obj) < 0)    // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object", __func__);
        }

        stm_se_encode_stream_detach(context.audio_stream[0], gAudioTest.Sink.obj);

        if (stm_memsink_delete((stm_memsink_h)gAudioTest.Sink.obj) < 0)    // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object", __func__);
        }

        FreeBuffer(&compressed_audio_frame);
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
            pr_err("Error: %s Failed to delete memsink object", __func__);
        }

        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_detach(context.audio_stream[0], gAudioTest.Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to detach encode stream from memsink\n", __func__);

        if (stm_memsink_delete((stm_memsink_h)gAudioTest.Sink.obj) < 0)    // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object", __func__);
        }

        FreeBuffer(&compressed_audio_frame);
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
        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_memsink_delete((stm_memsink_h)gAudioTest.Sink.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to detach encode stream from memsink\n", __func__);
        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    // Remove Stream and Encode
    result = stm_se_encode_stream_delete(context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to delete STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream %d\n", __func__, result);
        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_stream_delete(context.audio_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to delete STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AC3 EncodeStream %d\n", __func__, result);
        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_delete(context.encode);

    if (result < 0)
    {
        FreeBuffer(&compressed_audio_frame);
        FreeBuffer(&compressed_video_frame);
        FreeBuffer(&frame);
        pr_err("Error: %s stm_se_encode_delete returned %d\n", __func__, result);
        return result;
    }

    result = FreeBuffer(&compressed_video_frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to free video stream buffer (%d)\n", __func__, result);
    }

    gVideoTest[0].Sink.m_Buffer = NULL;
    gVideoTest[0].Sink.m_BufferLen = 0;

    result = FreeBuffer(&compressed_audio_frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to free audio stream buffer (%d)\n", __func__, result);
    }

    gAudioTest.Sink.m_Buffer = NULL;
    gAudioTest.Sink.m_BufferLen = 0;

    result = FreeBuffer(&frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to free sequence buffer (%d)\n", __func__, result);
    }

    // Success
    return result;
}

ENCODER_TEST(DiscontinuityChecks, "Check the discontinuities", BASIC_TEST);
