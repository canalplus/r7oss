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
#include "encode_stream.h"
#include "encode_coordinator_buffer.h"

#undef TRACE_TAG
#define TRACE_TAG "TranscodeBuffer_c"


void TranscodeBuffer_c::FinalizeInit(const EncoderBufferTypes  *EncoderBufferTypes)
{
    mEncoderBufferTypes = EncoderBufferTypes;
}

Buffer_t TranscodeBuffer_c::GetBuffer(void)
{
    return mBuffer;
}

void  TranscodeBuffer_c::SetBuffer(Buffer_t buffer)
{
    stm_se_uncompressed_frame_metadata_t *Metadata;

    buffer->ObtainMetaDataReference(mEncoderBufferTypes->InputMetaDataBufferType, (void **)&Metadata);
    SE_ASSERT(Metadata != NULL);
    mMedia = Metadata->media;
    SE_EXTRAVERB(group_encoder_stream, "Media is %s\n", MEDIA_STR(mMedia));
    mBuffer = buffer;
}

stm_se_encode_stream_media_t TranscodeBuffer_c::GetMedia(void)
{
    return mMedia;
}

char *TranscodeBuffer_c::BufferStateString(unsigned int v, char *s, size_t n)
{
    s[0] = '\0';

    for (unsigned int i = 0; i < MAX_BUFFER_STATE; i++)
    {
        const char *p = ENUM_BUFFER_STR(v, i);
        strncat(s, p, n - strlen(s) - 1);
    }
    s[n - 1] = '\0';

    return s;
}

char *TranscodeBuffer_c::BufferStateString(char *s, size_t n)
{
    if (mBuffer == NULL) { return (char *)"NULL BUFFER"; }
    return BufferStateString(mBufferState, s, n);
}

void TranscodeBuffer_c::BufferStateSet(BufferState_t flag)
{
    mBufferState = mBufferState | flag;

    if (flag == BufferUnused)    { mBufferState = BufferUnused; }
    if (flag == BufferLastClone) { mBufferState = mBufferState | BufferClone; }
}

void TranscodeBuffer_c::BufferStateClear(BufferState_t flag)
{
    mBufferState = mBufferState & (~flag);
}

bool TranscodeBuffer_c::IsBufferStateUnused(void)
{
    return (mBufferState == 0);
}

bool TranscodeBuffer_c::IsBufferState(BufferState_t flag)
{
    return ((mBufferState & flag) != 0);
}

void TranscodeBuffer_c::BufferStateSetCloneReference(TranscodeBuffer_c *reference)
{
    mBufferReference = reference;
}

TranscodeBuffer_c  *TranscodeBuffer_c::BufferStateGetCloneReference(void)
{
    return mBufferReference;
}

// Create a new Encode input buffer with clone metadata and pointing on the same uncompressed frame
class TranscodeBuffer_c *TranscodeBuffer_c::CloneBuffer(TranscodeBuffer_c TranscodeBuffer[], Encoder_c *Encoder, uint64_t encodeTime)
{
    EncoderStatus_t EncoderStatus;
    Buffer_t        ClonedBuffer;

    stm_se_uncompressed_frame_metadata_t *OriginalMetaData;
    stm_se_uncompressed_frame_metadata_t *ClonedMetaData;
    void           *InputBufferAddr[3];
    unsigned int    DataSize;

    SE_EXTRAVERB(group_encoder_stream, "%s:> get clone for buffer %p\n", MEDIA_STR(mMedia), mBuffer);

    // Get Encoder buffer with no blocking mode
    EncoderStatus = Encoder->GetInputBuffer(&ClonedBuffer, true);
    if ((EncoderStatus != EncoderNoError) || (ClonedBuffer == NULL))
    {
        SE_WARNING("Failed to get new encoder input buffer\n");
        return NULL;
    }

    //
    // Clone the metadata
    //

    // 1 - get original metadata
    mBuffer->ObtainMetaDataReference(mEncoderBufferTypes->InputMetaDataBufferType, (void **)(&OriginalMetaData));
    SE_ASSERT(OriginalMetaData != NULL);

    // 2 - get clone metadata
    ClonedBuffer->ObtainMetaDataReference(mEncoderBufferTypes->InputMetaDataBufferType, (void **)(&ClonedMetaData));
    SE_ASSERT(ClonedMetaData != NULL);
    SE_EXTRAVERB(group_encoder_stream, "%s: get clone buffer %p\n", MEDIA_STR(mMedia), ClonedBuffer);

    // 3 - Copy original metadata to clone
    memcpy(ClonedMetaData, OriginalMetaData, sizeof(stm_se_uncompressed_frame_metadata_t));

    //
    // Attach original frame buffer to clone buffer
    //

    // 1 - obtain original frame buffer
    // Initialize buffer address
    InputBufferAddr[0] = NULL;
    InputBufferAddr[1] = NULL;
    InputBufferAddr[2] = NULL;
    // Get physical address of buffer
    mBuffer->ObtainDataReference(NULL, &DataSize, (void **)(&InputBufferAddr[PhysicalAddress]), PhysicalAddress);
    SE_ASSERT(InputBufferAddr[PhysicalAddress] != NULL);
    // Get virtual address of buffer
    mBuffer->ObtainDataReference(NULL, &DataSize, (void **)(&InputBufferAddr[CachedAddress]), CachedAddress);
    SE_ASSERT(InputBufferAddr[CachedAddress] != NULL);

    // 2 - attach it to clone buffer
    ClonedBuffer->RegisterDataReference(DataSize, InputBufferAddr);
    ClonedBuffer->SetUsedDataSize(DataSize);

    unsigned int    BufferIndex;
    ClonedBuffer->GetIndex(&BufferIndex);
    SE_EXTRAVERB(group_encoder_stream, "%s:< Clone BufferIndex %d\n", MEDIA_STR(mMedia), BufferIndex);

    // creating the trancode Buffer
    TranscodeBuffer[BufferIndex].SetBuffer(ClonedBuffer);

    // 3 - set Timing information
    TranscodeBuffer[BufferIndex].SetEncodeTime(encodeTime);

    return  &TranscodeBuffer[BufferIndex];
}

// Indicates if the given buffer contains EOS_dicontinuity marker
bool TranscodeBuffer_c::IsEOS(void)
{
    stm_se_uncompressed_frame_metadata_t    *Metadata;

    // Get metadata buffer (attached to input buffer) to be filled
    mBuffer->ObtainMetaDataReference(mEncoderBufferTypes->InputMetaDataBufferType, (void **)(&Metadata));
    SE_ASSERT(Metadata != NULL);

    return ((Metadata->discontinuity & STM_SE_DISCONTINUITY_EOS) == STM_SE_DISCONTINUITY_EOS);
}

bool TranscodeBuffer_c::SetEncodeDiscontinuity(stm_se_discontinuity_t discontinuity)
{
    stm_se_uncompressed_frame_metadata_t *MetaData;

    SE_EXTRAVERB(group_encoder_stream, "%s: buffer %p discontinuity %d\n", MEDIA_STR(mMedia), mBuffer, discontinuity);

    // Get uncompressed metadata buffer
    mBuffer->ObtainMetaDataReference(mEncoderBufferTypes->InputMetaDataBufferType, (void **)(&MetaData));
    SE_ASSERT(MetaData != NULL);

    // Set discontinuity in uncompressed metadata
    MetaData->discontinuity = (stm_se_discontinuity_t)((int)MetaData->discontinuity | (int)discontinuity);

    return true;
}

// Gets the frame duration of the current frame
bool TranscodeBuffer_c::GetBufferFrameDuration(uint64_t *FrameDuration)
{
    BufferStatus_t                          Status;
    stm_se_uncompressed_frame_metadata_t    *Metadata;

    SE_EXTRAVERB(group_encoder_stream, "%s\n", MEDIA_STR(mMedia));

    // Init returned value even un case of error
    *FrameDuration = 0;

    // Get metadata buffer (attached to input buffer) to be filled
    mBuffer->ObtainMetaDataReference(mEncoderBufferTypes->InputMetaDataBufferType, (void **)(&Metadata));
    SE_ASSERT(Metadata != NULL);

#ifndef UNITTESTS
    // Compute the frame duration
    if (Metadata->media == STM_SE_ENCODE_STREAM_MEDIA_AUDIO)
    {
        uint32_t BufferLength;
        Status = mBuffer->ObtainDataReference(NULL, &BufferLength, NULL);
        // case of EOS , no payload
        if (Status != BufferNoError)
        {
            *FrameDuration = 0;
            return false;
        }

        uint64_t SampleLength = (uint64_t) StmSeAudioGetNrBytesFromLpcmFormat(Metadata->audio.sample_format) * Metadata->audio.core_format.channel_placement.channel_count;
        SE_ASSERT(SampleLength != 0);

        uint64_t NbSample = BufferLength / SampleLength;
        SE_ASSERT(Metadata->audio.core_format.sample_rate != 0);

        *FrameDuration = (NbSample * 1000000) / Metadata->audio.core_format.sample_rate;
    }
    else if (Metadata->media == STM_SE_ENCODE_STREAM_MEDIA_VIDEO)
    {
        SE_ASSERT(Metadata->video.frame_rate.framerate_num != 0);
        *FrameDuration = (1000000 * (uint64_t)Metadata->video.frame_rate.framerate_den) / Metadata->video.frame_rate.framerate_num;
    }
    else
    {
        SE_ERROR("Unsupported buffer media %d for buffer %p\n", Metadata->media, mBuffer);
        return false;
    }
#else
    // FIXME: use fake frame duration for SE unittests  684
    uint32_t BufferLength;
    Status = mBuffer->ObtainDataReference(NULL, &BufferLength, NULL);
    SE_ASSERT(Status == BufferNoError);
    *FrameDuration = BufferLength;
#endif

    SE_EXTRAVERB(group_encoder_stream, "%s: frame duration %lld usec\n", MEDIA_STR(mMedia), *FrameDuration);
    return true;
}


uint64_t TranscodeBuffer_c::Pts(void)
{
    stm_se_uncompressed_frame_metadata_t    *Metadata;

    // Get uncompressed metadata attached to buffer
    mBuffer->ObtainMetaDataReference(mEncoderBufferTypes->InputMetaDataBufferType, (void **)(&Metadata));
    SE_ASSERT(Metadata != NULL);

    TimeStamp_c TimeStamp = TimeStamp_c(Metadata->native_time, Metadata->native_time_format);
    return TimeStamp.uSecValue();
}


// Sets the encode PTS of the current frame
bool TranscodeBuffer_c::SetEncodeTime(uint64_t encodeTime)
{
    stm_se_uncompressed_frame_metadata_t    *UncompressedMetaData;
    __stm_se_encode_coordinator_metadata_t *EncodeCoordinatorMetaData;

    SE_EXTRAVERB(group_encoder_stream, "%s: buffer %p encodeTime %lld\n", MEDIA_STR(mMedia), mBuffer, encodeTime);

    // Get uncompressed metadata buffer
    mBuffer->ObtainMetaDataReference(mEncoderBufferTypes->InputMetaDataBufferType, (void **)(&UncompressedMetaData));
    SE_ASSERT(UncompressedMetaData != NULL);

    // Get encode coordinator metadata from input buffer
    mBuffer->ObtainMetaDataReference(mEncoderBufferTypes->EncodeCoordinatorMetaDataBufferType, (void **)(&EncodeCoordinatorMetaData));
    SE_ASSERT(EncodeCoordinatorMetaData != NULL);

    // Set encoded_time in uncompressed metadata, in native_time_format unit
    TimeStamp_c TimeStamp = TimeStamp_c(encodeTime, TIME_FORMAT_US);
    EncodeCoordinatorMetaData->encoded_time_format = UncompressedMetaData->native_time_format;
    EncodeCoordinatorMetaData->encoded_time = TimeStamp.Value(EncodeCoordinatorMetaData->encoded_time_format);

    return TimeStamp.IsValid();
}

// Sets the native PTS of the current frame
bool TranscodeBuffer_c::SetNativeTime(uint64_t nativeTime)
{
    stm_se_uncompressed_frame_metadata_t    *UncompressedMetaData;

    SE_EXTRAVERB(group_encoder_stream, "%s: buffer %p nativeTime %lld\n", MEDIA_STR(mMedia), mBuffer, nativeTime);

    // Get uncompressed metadata buffer
    mBuffer->ObtainMetaDataReference(mEncoderBufferTypes->InputMetaDataBufferType, (void **)(&UncompressedMetaData));
    SE_ASSERT(UncompressedMetaData != NULL);

    // Set native_time in uncompressed metadata, in native_time_format unit
    TimeStamp_c TimeStamp = TimeStamp_c(nativeTime, TIME_FORMAT_US);
    UncompressedMetaData->native_time = TimeStamp.Value(UncompressedMetaData->native_time_format);

    return TimeStamp.IsValid();
}


void TranscodeBuffer_c::GetIndex(unsigned int *index)
{
    if (mBuffer == NULL)
    {
        SE_ERROR("TranscodeBuffer not assigned\n");
        return;
    }

    mBuffer->GetIndex(index);
}
