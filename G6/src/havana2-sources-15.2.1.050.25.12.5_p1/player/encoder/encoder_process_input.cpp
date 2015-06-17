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

#include "encoder.h"

#undef TRACE_TAG
#define TRACE_TAG "EncodeStream_c"


// C stub
OS_TaskEntry(EncoderProcessInputToPreprocessor)
{
    EncodeStream_t      Stream = (EncodeStream_t)Parameter;
    Stream->ProcessInputToPreprocessor();
    OS_TerminateThread();
    return NULL;
}

// Main process code
void   EncodeStream_c::ProcessInputToPreprocessor(void)
{
    EncoderStatus_t                           Status;
    RingStatus_t                              RingStatus;
    Buffer_t                                  Buffer;
    BufferType_t                              BufferType;
    stm_se_uncompressed_frame_metadata_t     *UncompressedMetaData;

    // Signal we have started
    OS_LockMutex(&StartStopLock);
    ProcessRunningCount++;
    OS_SetEvent(&StartStopEvent);
    OS_UnLockMutex(&StartStopLock);

    SE_DEBUG(group_encoder_stream, "Stream 0x%p: process starting\n", this);

    // Main Loop
    while (!Terminating)
    {
        RingStatus  = mInputRing->Extract((uintptr_t *)(&Buffer), ENCODE_STREAM_MAX_EVENT_WAIT);
        if ((RingStatus == RingNothingToGet) || (Buffer == NULL))
        {
            continue;
        }

        Buffer->GetType(&BufferType);
        Buffer->TransferOwnership(IdentifierEncoderInputToPreProcess);

        // If we were set to terminate while we were 'Extracting' we should
        // remove the buffer reference and exit.
        if (Terminating)
        {
            ReleaseInputBuffer(Buffer);
            continue;
        }

        // Deal with a Input buffer
        if (BufferType == BufferInputBufferType)
        {
            Buffer->ObtainMetaDataReference(InputMetaDataBufferType, (void **)(&UncompressedMetaData));
            SE_ASSERT(UncompressedMetaData != NULL);

            // First check that the buffer should be pushed to encode
            // May be an EOS standalone frame if buffer size is 0
            uint32_t    BufferSize = 0;
            Buffer->ObtainDataReference(NULL, &BufferSize, NULL);
            if (BufferSize != 0)
            {
                // Send buffer to encoder
                SE_VERBOSE(group_encoder_stream, "Stream 0x%p: Send Buffer 0x%p to Preproc\n", this, Buffer);
                Status = Input(Buffer);
                if (Status != EncoderNoError)
                {
                    SE_ERROR("Stream 0x%p: Unable to encode Buffer 0x%p\n", this, Buffer);
                }
            }
            else if ((UncompressedMetaData->discontinuity & STM_SE_DISCONTINUITY_EOS) == STM_SE_DISCONTINUITY_EOS)
            {
                // Inject discontinuity only in case of standalone EOS frame
                // When the EOS is carried by the last frame to be encoded, it is not needed because already handled by the preproc
                SE_VERBOSE(group_encoder_stream, "Stream 0x%p: Send EOS to Preproc\n", this);
                InjectDiscontinuity(STM_SE_DISCONTINUITY_EOS);
            }

            // Release the encode input buffer
            ReleaseInputBuffer(Buffer);
        }
        else
        {
            SE_ERROR("Stream 0x%p: Buffer 0x%p of invalid type %d\n", this, Buffer, BufferType);
            Buffer->DecrementReferenceCount();
        }
    }
    SE_DEBUG(group_encoder_stream, "Stream 0x%p: process terminating\n", this);

    // At this point, Terminating must be true
    OS_Smp_Mb(); // Read memory barrier: rmb_for_EncodeStream_Terminating coupled with: wmb_for_EncodeStream_Terminating

    // Signal we have terminated
    OS_LockMutex(&StartStopLock);
    ProcessRunningCount--;
    OS_SetEvent(&StartStopEvent);
    OS_UnLockMutex(&StartStopLock);
}

