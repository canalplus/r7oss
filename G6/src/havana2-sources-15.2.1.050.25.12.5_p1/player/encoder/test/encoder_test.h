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
#ifndef ENCODER_TEST_H_
#define ENCODER_TEST_H_

#include <stm_event.h>

#include "stm_se.h"

// Mme transformer is limited to 32 instances by multicom
// ENCODE_MAX_NUMBER is the maximum number of possible encoder
// ENCODE_STREAM_MAX_NUMBER is the maximum number of possible A/V encode stream inside an encoder
#define TEST_MAX_ENCODE_STREAM ENCODE_STREAM_MAX_NUMBER
#define TEST_MAX_ENCODE ENCODE_MAX_NUMBER
#define MAX_FAKE_BUFFER 500

// Type of test
typedef enum
{
    BASIC_TEST = (1 << 0),
    LONG_TEST = (1 << 1),
} test_type_t;

static inline const char *Stringify(test_type_t aTestType)
{
    switch (aTestType)
    {
        ENTRY(BASIC_TEST);
        ENTRY(LONG_TEST);
    default: return "<unknown test type>";
    }
}

// Some locals
typedef struct pp_descriptor_s
{
    stm_object_h    obj;                        //!< Memory sink object
    unsigned char  *m_Buffer;                   //!< Pointer to the buffer to store the grabbed codedbuffers
    unsigned int    m_BufferLen;                //!< Size of the buffer to store the grabbed codedbuffers
    unsigned int    m_availableLen;
    struct my_io_descriptor *out;
    struct my_io_descriptor *in;
} pp_descriptor;

// A buffer from BPA with two pointers
typedef struct bpa_buffer_s
{
    void          *virtual;
    unsigned long  physical;
    unsigned int   size;
    char          *partition;
} bpa_buffer;

typedef struct EncoderTestCtx
{
    stm_se_encode_h           encode;
    stm_se_encode_stream_h    audio_stream[TEST_MAX_ENCODE_STREAM];
    stm_se_encode_stream_h    video_stream[TEST_MAX_ENCODE_STREAM];
    stm_event_subscription_h  event_subscription[TEST_MAX_ENCODE_STREAM];
    struct semaphore         *semaphore;
    struct mutex             *mutex;
    pp_descriptor             sink[TEST_MAX_ENCODE_STREAM];
    bpa_buffer                raw_video_frame[TEST_MAX_ENCODE_STREAM];
    int encode_id;
    int stream_id;
} EncoderContext;

typedef struct EncoderTestArg_s
{
    int free_memory_target;
    int duration;
} EncoderTestArg;

typedef struct EncoderTest_s
{
    int (*function)(EncoderTestArg *);
    char  *description;
    test_type_t type;
    int    status;
} EncoderTest;

#define ENCODER_TEST(function, description, type)   \
        EncoderTest test_##function                 \
        __used __section(.encoder.tests.stkpi)      \
        __attribute__((aligned((sizeof(long)))))    \
        = { function, description, type, 0 }

#endif /* ENCODER_TEST_H_ */
