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

#include "preproc_audio_null.h"

#undef TRACE_TAG
#define TRACE_TAG "Preproc_Audio_Null_c"

Preproc_Audio_Null_c::Preproc_Audio_Null_c(void)
    : Preproc_Audio_c()
{
    SetFrameMemory(PREPROC_AUDIO_FRAME_MAX_SIZE, ENCODER_STREAM_NULL_MAX_PREPROC_BUFFERS);
}

Preproc_Audio_Null_c::~Preproc_Audio_Null_c(void)
{
}

/* @brief Single thread implementation: takes in the Input Buffer, copies it in MainOutputbuffer and sends to ring */
PreprocStatus_t Preproc_Audio_Null_c::Input(Buffer_t Buffer)
{
    PreprocStatus_t Status = PreprocNoError;
    SE_DEBUG(group_encoder_audio_preproc, "\n");

    // Call the parent implementation
    Status = Preproc_Audio_c::Input(Buffer);
    if (PreprocNoError != Status)
    {
        return PREPROC_STATUS_RUNNING(Status);
    }

    /* @note from here he have a  PreprocFrameBuffer reference, with attached InputBuffer */

    /* Actual Processing: Copy from CurrentInputBuffer.CachedAddress to MainOutputBuffer.CachedAddress */
    if (0 != CurrentInputBuffer.Size)
    {
        if (NULL == CurrentInputBuffer.CachedAddress)
        {
            SE_ERROR("Input data buffer reference is NULL\n");
            ReleaseMainPreprocFrameBuffer();
            return PreprocError;
        }

        if (NULL == MainOutputBuffer.CachedAddress)
        {
            SE_ERROR("preproc buffer reference is NULL\n");
            ReleaseMainPreprocFrameBuffer();
            return PreprocError;
        }

        memcpy(MainOutputBuffer.CachedAddress, CurrentInputBuffer.CachedAddress, CurrentInputBuffer.Size);
    }

    // Metadata has already be filled by Preproc_Base_c::Input(), only need to update the size
    MainOutputBuffer.Size = CurrentInputBuffer.Size;

    // Send buffer to Output using parent class
    Status = SendBufferToOutput(&MainOutputBuffer);
    if (PreprocNoError != Status)
    {
        SE_ERROR("SendBufferToOutput failed %08X\n", Status);
    }

    // Updates context at end of input
    InputPost(Status);
    return Status ;
}
