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

#include "coder_null.h"

extern "C" int snprintf(char *buf, long size, const char *fmt, ...) __attribute__((format(printf, 3, 4))) ;

#undef TRACE_TAG
#define TRACE_TAG "Coder_Null_c"

// The CompletionCallback should be entirely encoder dependent and
// Should reference the encode context for that current frame
// to ensure that the Completion of the buffer is marked correctly.


Coder_Null_c::Coder_Null_c(void)
{
    SetGroupTrace(group_encoder_audio_coder | group_encoder_video_coder);

    Coder_Base_c::SetFrameMemory(CODED_FRAME_MAX_SIZE, ENCODER_STREAM_NULL_MAX_CODED_BUFFERS);
}

Coder_Null_c::~Coder_Null_c(void)
{
}

CoderStatus_t Coder_Null_c::CompletionCallback(Buffer_t Buffer, unsigned int DataSize)
{
    // Update the buffer with the results of the Encode Operation
    Buffer->SetUsedDataSize(DataSize);
    BufferStatus_t BufStatus = Buffer->ShrinkBuffer(DataSize);
    if (BufStatus != BufferNoError)
    {
        return CoderError;
    }

    // Send the completed buffer to the Output rings.
    CoderStatus_t Status = Output(Buffer);
    return Status;
}

CoderStatus_t Coder_Null_c::EncodeFrame(Buffer_t CodedFrameBuffer, Buffer_t InputBuffer)
{
    unsigned int   BlockSize;
    unsigned int   UsedData;
    char          *Data;
    static int     FrameNumber = 0;

    CodedFrameBuffer->ObtainDataReference(&BlockSize, &UsedData, (void **)&Data, CachedAddress);
    if (Data == NULL)
    {
        SE_ERROR("Failed to obtain data references from Output Buffer\n");
        return CoderError;
    }

    // Write test data to the 'Null Encoded' Buffer.
    // This data can be read from the output to verify
    // that buffers are moving through the system
    UsedData = snprintf(Data, BlockSize, "EncodeFrame number %d\n", ++FrameNumber);

    // Wait for Completion
    // This will be done by the Callback thread if we are using an MME/HW Callback interface.
    // In this NULL Encoder we simulate an immediate callback!
    CoderStatus_t Status = CompletionCallback(CodedFrameBuffer, UsedData);

    return Status;
}

CoderStatus_t Coder_Null_c::Input(Buffer_t    Buffer)
{
    CoderStatus_t Status;
    SE_DEBUG(GetGroupTrace(), "\n");

    // First perform base operations
    Status = Coder_Base_c::Input(Buffer);
    if (Status != CoderNoError)
    {
        return CODER_STATUS_RUNNING(Status);
    }

    // Perform the Encode
    return EncodeFrame(CodedFrameBuffer, Buffer);
}
