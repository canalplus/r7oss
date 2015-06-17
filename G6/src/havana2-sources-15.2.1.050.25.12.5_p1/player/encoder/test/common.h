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

#ifndef COMMON_H_
#define COMMON_H_

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

char *PassFail(int v);

void EncoderTestCleanup(EncoderContext *context);
void MultiEncoderTestCleanup(EncoderContext *context);

/* Audio Common */
typedef struct expected_result_s
{
    unsigned int size; //Expected frame size in bytes
    unsigned int crc;  //Expected CRC
    uint64_t     pts;
} audio_expected_result_t;
#define AUDIO_MAX_NUMBER_FRAMES 32
typedef struct expected_test_result_s
{
    int expected_nr_frames;  // Expected number of frames
    audio_expected_result_t expected_results[AUDIO_MAX_NUMBER_FRAMES];
} audio_expected_test_result_t;

/// Audio test context
typedef struct EncoderStkpiAudioTest_s
{
//public:
    //Input Description
    uint8_t                 *InputVector;             //!< Pointer to the array of input PCM
    uint32_t                InputVectorSize;      //!< Size of the array
    stm_se_uncompressed_frame_metadata_t UMetadata;   //!< Describes the input PCM
    uint32_t                BytesPerSample;       //!< Bytes Per PCM Sample

    //Injection Description
    uint32_t                SamplesPerInjection;      //!< Number of samples per injection
    uint32_t                BytesPerInjection;        //!< equivalent number of bytes per injection
    uint32_t                MsPerInjection;           //!< equivalent number of ms per injection

    //Options
    bool                    TestWithPtsWrap;            //!< Start with small PTS that can wrap due to delay
    bool                    TestWithExtrapolation;      //!< Test Extrapolation with 0 PTS injection
    bool                    TestWithMicroSecInjection;  //!< Test with timestamps in microseconds injection
    bool                    MorePrints;             //!< More prints
    bool                    InjectEosAfterLastFrame;    //!< EOS is sent after last injection, rather than with it
    bool                    CollectFramesOnCallback;    //!< Collects coded frames based on event, rather than simple polling

    //Test
    audio_expected_test_result_t *ExpectedTestResult;   //!< array to check the coded frames against.

    //Memory sink description
    pp_descriptor           Sink; //TODO Remove this as it is already present in EncoderContext

//private:
    unsigned int            FramesReceived;             //!< Number of codedframes collected
    bool                    EosReceived;                //!< true once EOS received from encoder
    unsigned int            NrEventsReceived;
    unsigned int            FadeOutFrameNo;
    unsigned int            MuteFrameNo;
    unsigned int            FadeInFrameNo;
} EncoderStkpiAudioTest_t;

/// Video test context
typedef struct EncoderStkpiVideoTest_s
{
    unsigned int            EncodedFramesReceived;      //!< Number of coded frames collected
    unsigned int            SkippedFramesReceived;      //!< Number of skipped frames collected
    unsigned int            FatalErrorReceived;         //!< Number of fatal errors collected
    unsigned int            HardwareFailureReceived;    //!< Number of hardware failures collected
    bool                    EosReceived;                //!< true once EOS received from encoder
    unsigned int            NrEventsReceived;
    pp_descriptor           Sink; //TODO Remove this as it is already present in EncoderContext
} EncoderStkpiVideoTest_t;

/// Global audio test context (communication between injection and callback)
extern EncoderStkpiAudioTest_t gAudioTest;

/// Global video test context (communication between injection and callback)
extern EncoderStkpiVideoTest_t gVideoTest[TEST_MAX_ENCODE_STREAM];

/// Test encode context
extern EncoderContext gContext[TEST_MAX_ENCODE];

/// Fake buffer table
extern bpa_buffer fake_buffer[MAX_FAKE_BUFFER];

/***/

int AllocateBuffer(int size, bpa_buffer *buffer);
int FreeBuffer(bpa_buffer *buffer);
uint64_t ConvertTimeStamp(uint64_t FromValue, stm_se_time_format_t FromFormat, stm_se_time_format_t ToFormat);
int AudioFramesEncode(EncoderContext *context);
int VideoEncodeEventSubscribe(EncoderContext *context);
int VideoEncodeEventUnsubscribe(EncoderContext *context);
unsigned int GetTimeInSeconds(void);
int get_next_frame_id(int frame_id, int max_frame_nb);

#endif /* COMMON_H_ */
