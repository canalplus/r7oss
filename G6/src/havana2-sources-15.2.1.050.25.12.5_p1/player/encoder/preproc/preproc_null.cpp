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
#include "preproc_null.h"

#undef TRACE_TAG
#define TRACE_TAG "Preproc_Null_c"

Preproc_Null_c::Preproc_Null_c()
{
    SetGroupTrace(group_encoder_audio_preproc | group_encoder_video_preproc);
    SetFrameMemory(PREPROC_AUDIO_FRAME_MAX_SIZE, ENCODER_STREAM_NULL_MAX_PREPROC_BUFFERS);
}

Preproc_Null_c::~Preproc_Null_c()
{
    Halt();
}

PreprocStatus_t Preproc_Null_c::Input(Buffer_t    Buffer)
{
    unsigned int DataSize;
    void *InputBufferAddr;
    void *PreprocBufferAddr;
    Buffer_t ContentBuffer;

    SE_DEBUG(GetGroupTrace(), "\n");

    // Call the Base Class implementation to update statistics
    PreprocStatus_t Status = Preproc_Base_c::Input(Buffer);
    if (Status != PreprocNoError)
    {
        return PREPROC_STATUS_RUNNING(Status);
    }

    // Copy data buffer using memcpy
    Buffer->ObtainDataReference(NULL, &DataSize, (void **)(&InputBufferAddr), CachedAddress);
    if (InputBufferAddr == NULL)
    {
        SE_ERROR("Unable to get input data buffer reference\n");
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PreprocError;
    }

    PreprocFrameBuffer->ObtainDataReference(NULL, NULL, (void **)(&PreprocBufferAddr), CachedAddress);
    if (PreprocBufferAddr == NULL)
    {
        SE_ERROR("Unable to get preproc data buffer reference\n");
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PreprocError;
    }

    memcpy(PreprocBufferAddr, InputBufferAddr, DataSize);

    // Completion
    PreprocFrameBuffer->SetUsedDataSize(DataSize);

    PreprocFrameBuffer->ObtainAttachedBufferReference(PreprocFrameAllocType, &ContentBuffer);
    SE_ASSERT(ContentBuffer != NULL);

    ContentBuffer->SetUsedDataSize(DataSize);
    BufferStatus_t BufStatus = ContentBuffer->ShrinkBuffer(max(DataSize, 1));
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to shrink the operating buffer to size (%08x)\n", BufStatus);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PreprocError;
    }

    // Simply pass it on to the output ring
    return Output(PreprocFrameBuffer);
}

PreprocStatus_t Preproc_Null_c::Halt()
{
    return Preproc_Base_c::Halt();
}
