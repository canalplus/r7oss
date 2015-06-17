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

        mBufferReference = NULL;
The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef H_ENCODE_COORDINATOR_BUFFER
#define H_ENCODE_COORDINATOR_BUFFER

#include "timestamps.h"
#ifndef UNITTESTS
#include "audio_conversions.h"
#endif

// rather than using buffer_c class directly , trancode
// coordinator relies internally on the TranscodeBuffer_c
// class , purpose is to extend the buffer class to add
// new information .
// This implementation is somewhere similar to wrapper / decorator
// C++ pattern
//
#define ENUM_BUFFER_STR(flag,i)   \
    (( (i==0) && (flag & (1<<i)))?"BufferOwnByEncoder ": \
     (( (i==1) && (flag & (1<<i)))?"BufferClone ":        \
      (( (i==2) && (flag & (1<<i)))?"BufferLastClone ":    \
       (( (i==3) && (flag & (1<<i)))?"BufferDecode ":       \
        (( (i==4) && (flag & (1<<i)))?"BufferReference ":\
         "" )))))

#define MAX_BUFFER_STATE    5
typedef enum BufferState_s
{
    // buffer is unused
    BufferUnused = 0,

    // buffer has been sent to encoder
    BufferOwnByEncoder = (1 << 0),

    // Buffer is a CLone buffer
    BufferClone = (1 << 1),

    // Buffer is the last clone of a gap
    BufferLastClone = (1 << 2),

    // Buffer coming from decoder
    BufferDecode = (1 << 3),

    // Buffer is used a source for a clone buffer
    BufferReference = (1 << 4),

} BufferState_t;



#define MEDIA_STR(x)   (((x) == STM_SE_ENCODE_STREAM_MEDIA_VIDEO) ? "VIDEO" : ((x) == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) ? "AUDIO" : "ANY")

class TranscodeBuffer_c
{
public:
    TranscodeBuffer_c(void)
        : mBufferState(BufferUnused)
        , mBuffer(NULL)
        , mBufferReference(NULL)
        , mMedia(STM_SE_ENCODE_STREAM_MEDIA_ANY)
        , mEncoderBufferTypes(NULL)
    {}

    void FinalizeInit(const EncoderBufferTypes  *EncoderBufferTypes);

    Buffer_t GetBuffer(void);
    void     SetBuffer(Buffer_t b);
    stm_se_encode_stream_media_t GetMedia(void);


    // Buffer State Management
    char *BufferStateString(unsigned int v, char *s, size_t n);
    char *BufferStateString(char *s, size_t n);
    void BufferStateSet(BufferState_t flag);
    void BufferStateClear(BufferState_t flag);
    bool IsBufferStateUnused(void);
    bool IsBufferState(BufferState_t flag);

    // Clone Buffer
    void BufferStateSetCloneReference(TranscodeBuffer_c *reference);
    TranscodeBuffer_c  *BufferStateGetCloneReference(void);
    class TranscodeBuffer_c *CloneBuffer(TranscodeBuffer_c TranscodeBuffer[], Encoder_c *Encoder, uint64_t encodeTime);

    // Set disconinuity
    bool SetEncodeDiscontinuity(stm_se_discontinuity_t discontinuity);

    // Indicates if the given buffer contains EOS_dicontinuity marker
    bool IsEOS(void);

    // Gets the frame duration of the current frame
    bool GetBufferFrameDuration(uint64_t *FrameDuration);

    // Sets the encode PTS of the current frame
    bool SetEncodeTime(uint64_t encodeTime);
    // Sets the native PTS of the current frame
    bool SetNativeTime(uint64_t nativeTime);

    // Get Current PTS
    uint64_t Pts(void);


    // Decorated Interface
    void GetIndex(unsigned int *index);

private:
    uint32_t                        mBufferState;
    Buffer_c                       *mBuffer;
    TranscodeBuffer_c              *mBufferReference;
    stm_se_encode_stream_media_t    mMedia;
    const EncoderBufferTypes       *mEncoderBufferTypes;
};

typedef  class TranscodeBuffer_c     *TranscodeBuffer_t;


#endif  // H_ENCODE_COORDINATOR_BUFFER


