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

#include "encoder.h"

#undef TRACE_TAG
#define TRACE_TAG "EncodeStream_c"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      C stub
//

OS_TaskEntry(EncoderProcessPreprocessorToCoder)
{
    EncodeStream_t      Stream = (EncodeStream_t)Parameter;
    Stream->ProcessPreprocessorToCoder();
    OS_TerminateThread();
    return NULL;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Main process code
//

void   EncodeStream_c::ProcessPreprocessorToCoder(void)
{
    RingStatus_t              RingStatus;
    Buffer_t                  Buffer;
    BufferType_t              BufferType;
    unsigned int              BufferIndex;
    EncodeControlStructure_t *ControlStructure;
    EncoderSequenceNumber_t  *SequenceNumberStructure;
    unsigned long long        LastEntryTime;
    unsigned long long        SequenceNumber;
    unsigned long long        MaximumActualSequenceNumberSeen;
    unsigned int              AccumulatedBeforeControlMessagesCount;
    unsigned int              AccumulatedAfterControlMessagesCount;
    bool                      ProcessNow;
    unsigned int             *Count;
    EncodeBufferRecord_t     *Table;
    unsigned int              BufferLength;
    bool                      DiscardBuffer;
    EncoderStatus_t           Status;

    //
    // Set parameters
    //
    Status = EncoderNoError;
    LastEntryTime               = OS_GetTimeInMicroSeconds();
    SequenceNumber              = INVALID_SEQUENCE_VALUE;
    MaximumActualSequenceNumberSeen         = 0;
    AccumulatedBeforeControlMessagesCount   = 0;
    AccumulatedAfterControlMessagesCount    = 0;

    //
    // Signal we have started
    //
    OS_LockMutex(&StartStopLock);
    ProcessRunningCount++;
    OS_SetEvent(&StartStopEvent);
    OS_UnLockMutex(&StartStopLock);

    SE_DEBUG(group_encoder_stream, "Stream 0x%p: process starting\n", this);

    //
    // Main Loop
    //
    while (!Terminating)
    {
        // If low power state, thread must stay asleep until low power exit signal
        if (IsLowPowerState)
        {
            SE_INFO(group_encoder_stream, "Stream 0x%p: entering low power..\n", this);
            // Signal end of low power processing
            // TBD: are we sure that all pending MME commands are completed here ???
            OS_SetEventInterruptible(&LowPowerEnterEvent);
            // Forever wait for wake-up event
            OS_Status_t WaitStatus = OS_WaitForEventInterruptible(&LowPowerExitEvent, OS_INFINITE);
            if (WaitStatus == OS_INTERRUPTED)
            {
                SE_INFO(group_encoder_stream, "wait for LP exit interrupted; LowPowerExitEvent:%d\n", LowPowerExitEvent.Valid);
            }
            SE_INFO(group_encoder_stream, "Stream 0x%p: exiting low power..\n", this);
        }

        RingStatus  = PreprocOutputRing->Extract((uintptr_t *)(&Buffer), ENCODE_STREAM_MAX_EVENT_WAIT);

        if ((RingStatus == RingNothingToGet) || (Buffer == NULL) || Terminating)
        {
            continue;
        }

        Buffer->GetType(&BufferType);
        Buffer->GetIndex(&BufferIndex);
        Buffer->TransferOwnership(IdentifierEncoderPreProcessToCoder);

        //
        // Deal with a decoded frame buffer
        //
        if (BufferType == PreprocFrameBufferType)
        {
            //
            // Apply a sequence number to the buffer
            //
            Buffer->ObtainMetaDataReference(MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
            SE_ASSERT(SequenceNumberStructure != NULL);

            SequenceNumberStructure->TimeEntryInProcess0    = OS_GetTimeInMicroSeconds();
            SequenceNumberStructure->DeltaEntryInProcess0   = SequenceNumberStructure->TimeEntryInProcess0 - LastEntryTime;
            LastEntryTime                   = SequenceNumberStructure->TimeEntryInProcess0;
            SequenceNumberStructure->TimeEntryInProcess1    = INVALID_TIME;
            SequenceNumberStructure->TimePassToCoder        = INVALID_TIME;
            SequenceNumberStructure->TimePassToOutput       = INVALID_TIME;

            if ((MarkerInPreprocFrameIndex != INVALID_INDEX) &&
                (MarkerInPreprocFrameIndex == BufferIndex))
            {
                // This is a marker frame
                SequenceNumberStructure->MarkerFrame    = true;
                NextBufferSequenceNumber    = SequenceNumberStructure->Value + 1;
                DiscardingUntilMarkerFramePtoC  = false;
                MarkerInPreprocFrameIndex   = INVALID_INDEX;
            }
            else
            {
                SequenceNumberStructure->MarkerFrame    = false;
                SequenceNumberStructure->Value      = NextBufferSequenceNumber++;
            }

            SequenceNumber              = SequenceNumberStructure->Value;
            MaximumActualSequenceNumberSeen     = max(SequenceNumber, MaximumActualSequenceNumberSeen);
            //
            // Process any outstanding control messages to be applied before this buffer
            //
            ProcessAccumulatedControlMessages(&AccumulatedBeforeControlMessagesCount,
                                              ENCODE_STREAM_MAX_PTOC_MESSAGES,
                                              AccumulatedBeforePtoCControlMessages,
                                              SequenceNumber, INVALID_TIME);
            //
            // Pass the buffer to the encoder for processing
            // then release our hold on this buffer. When discarding we
            // do not pass on, unless we have a zero length buffer, used when
            // signalling events.
            //
            Buffer->ObtainDataReference(NULL, &BufferLength, NULL);
            DiscardBuffer   = !SequenceNumberStructure->MarkerFrame &&
                              (BufferLength != 0) &&
                              (UnEncodable || DiscardingUntilMarkerFramePtoC || (Status != EncoderNoError));

            if (!DiscardBuffer)
            {
                SE_VERBOSE(group_encoder_stream, "Stream 0x%p: Send Buffer 0x%p to Coder\n", this, Buffer);
                Status = Encoder.Coder->Input(Buffer);
                if (Status != EncoderNoError)
                {
                    SignalEvent(STM_SE_ENCODE_STREAM_EVENT_FATAL_ERROR);
                    SE_ERROR("Input coder failed, all other input buffers will be dropped => encode stream must be destroyed, Status %u\n", Status);
                }
            }
            else
            {
                SE_VERBOSE(group_encoder_stream, "Stream 0x%p: Discard Buffer 0x%p - MarkerFrame %d BufferLength %d UnEncodable %d Discarding %d\n",
                           this, Buffer, SequenceNumberStructure->MarkerFrame, BufferLength, UnEncodable, DiscardingUntilMarkerFramePtoC);
            }

            Buffer->DecrementReferenceCount(IdentifierEncoderPreProcessToCoder);
            //
            // Process any outstanding control messages to be applied after this buffer
            //
            ProcessAccumulatedControlMessages(&AccumulatedAfterControlMessagesCount,
                                              ENCODE_STREAM_MAX_PTOC_MESSAGES,
                                              AccumulatedAfterPtoCControlMessages,
                                              SequenceNumber, INVALID_TIME);
        }
        //
        // Deal with an Encoder control structure
        //
        else if (BufferType == BufferEncoderControlStructureType)
        {
            Buffer->ObtainDataReference(NULL, NULL, (void **)(&ControlStructure));
            SE_ASSERT(ControlStructure != NULL);  // not supposed to be empty
            ProcessNow  = (ControlStructure->SequenceType == SequenceTypeImmediate) ||
                          ((SequenceNumber != INVALID_SEQUENCE_VALUE) && (ControlStructure->SequenceValue <= MaximumActualSequenceNumberSeen));

            if (ProcessNow)
            {
                ProcessControlMessage(Buffer, ControlStructure);
            }
            else
            {
                if ((ControlStructure->SequenceType == SequenceTypeBeforeSequenceNumber) ||
                    (ControlStructure->SequenceType == SequenceTypeBeforePlaybackTime))
                {
                    Count   = &AccumulatedBeforeControlMessagesCount;
                    Table   = AccumulatedBeforePtoCControlMessages;
                }
                else
                {
                    Count   = &AccumulatedAfterControlMessagesCount;
                    Table   = AccumulatedAfterPtoCControlMessages;
                }

                AccumulateControlMessage(Buffer, ControlStructure, Count, ENCODE_STREAM_MAX_PTOC_MESSAGES, Table);
            }
        }
        else
        {
            SE_ERROR("Stream 0x%p: Unknown buffer type %d received\n", this, BufferType);
            Buffer->DecrementReferenceCount();
        }
    }
    SE_INFO(group_encoder_stream, "Stream 0x%p: terminating holding control structures %d\n", this, AccumulatedBeforeControlMessagesCount + AccumulatedAfterControlMessagesCount);
    SE_DEBUG(group_encoder_stream, "Stream 0x%p: process terminating\n", this);

    // At this point, Terminating must be true
    OS_Smp_Mb(); // Read memory barrier: rmb_for_EncodeStream_Terminating coupled with: wmb_for_EncodeStream_Terminating

    //
    // Signal we have terminated
    //
    OS_LockMutex(&StartStopLock);
    ProcessRunningCount--;
    OS_SetEvent(&StartStopEvent);
    OS_UnLockMutex(&StartStopLock);
}

