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

OS_TaskEntry(EncoderProcessCoderToOutput)
{
    EncodeStream_t      Stream = (EncodeStream_t)Parameter;
    Stream->ProcessCoderToOutput();
    OS_TerminateThread();
    return NULL;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Main process code
//

void   EncodeStream_c::ProcessCoderToOutput(void)
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

    //
    // Set parameters
    //
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
        RingStatus  = CoderOutputRing->Extract((uintptr_t *)(&Buffer), ENCODE_STREAM_MAX_EVENT_WAIT);

        if ((RingStatus == RingNothingToGet) || (Buffer == NULL))
        {
            continue;
        }

        Buffer->GetType(&BufferType);
        Buffer->GetIndex(&BufferIndex);
        Buffer->TransferOwnership(IdentifierEncoderCoderToOutput);

        //
        // If we were set to terminate while we were 'Extracting' we should
        // remove the buffer reference and exit.
        //
        if (Terminating)
        {
            Buffer->DecrementReferenceCount(IdentifierEncoderCoderToOutput);
            continue;
        }

        //
        // Deal with a coded frame buffer
        //
        if (BufferType == CodedFrameBufferType)
        {
            //
            // Apply a sequence number to the buffer
            //
            Buffer->ObtainMetaDataReference(MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
            SE_ASSERT(SequenceNumberStructure != NULL);

            SequenceNumberStructure->TimeEntryInProcess1    = OS_GetTimeInMicroSeconds();
            SequenceNumberStructure->DeltaEntryInProcess1   = SequenceNumberStructure->TimeEntryInProcess1 - LastEntryTime;
            LastEntryTime                   = SequenceNumberStructure->TimeEntryInProcess1;
            SequenceNumberStructure->TimePassToOutput       = INVALID_TIME;

            if ((MarkerInCodedFrameIndex != INVALID_INDEX) &&
                (MarkerInCodedFrameIndex == BufferIndex))
            {
                // This is a marker frame
                SequenceNumberStructure->MarkerFrame    = true;
                NextBufferSequenceNumber    = SequenceNumberStructure->Value + 1;
                DiscardingUntilMarkerFrameCtoO  = false;
                MarkerInCodedFrameIndex     = INVALID_INDEX;
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
                                              ENCODE_STREAM_MAX_CTOO_MESSAGES,
                                              AccumulatedBeforeCtoOControlMessages,
                                              SequenceNumber, INVALID_TIME);
            //
            // Pass the buffer to the encoder for processing
            // then release our hold on this buffer. When discarding we
            // do not pass on, unless we have a zero length buffer, used when
            // signalling events.
            //
            BufferStatus_t BufStatus = Buffer->ObtainDataReference(NULL, &BufferLength, NULL);
            if (BufStatus != BufferNoError)
            {
                if (BufStatus == BufferNoDataAttached)
                {
                    SE_VERBOSE(group_encoder_stream, "Stream 0x%p: Buffer 0x%p with zero buffer length\n", this, Buffer);
                }
                else
                {
                    SE_ERROR("Stream 0x%p: Unable to obtain the data reference\n", this);
                }
                Buffer->DecrementReferenceCount(IdentifierEncoderCoderToOutput);
                continue;
            }

            DiscardBuffer   = !SequenceNumberStructure->MarkerFrame &&
                              (BufferLength != 0) &&
                              (UnEncodable || DiscardingUntilMarkerFrameCtoO);

            if (!DiscardBuffer)
            {
                SE_VERBOSE(group_encoder_stream, "Stream 0x%p: Send Buffer 0x%p to Transporter\n", this, Buffer);
                /*TransporterStatus_t Status =*/ Encoder.Transporter->Input(Buffer);
            }
            else
            {
                SE_VERBOSE(group_encoder_stream, "Stream 0x%p: Discard Buffer 0x%p - MarkerFrame %d BufferLength %d UnEncodable %d Discarding %d\n",
                           this, Buffer, SequenceNumberStructure->MarkerFrame, BufferLength, UnEncodable, DiscardingUntilMarkerFrameCtoO);
            }

            Buffer->DecrementReferenceCount(IdentifierEncoderCoderToOutput);
            //
            // Process any outstanding control messages to be applied after this buffer
            //
            ProcessAccumulatedControlMessages(&AccumulatedAfterControlMessagesCount,
                                              ENCODE_STREAM_MAX_CTOO_MESSAGES,
                                              AccumulatedAfterCtoOControlMessages,
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
                    Table   = AccumulatedBeforeCtoOControlMessages;
                }
                else
                {
                    Count   = &AccumulatedAfterControlMessagesCount;
                    Table   = AccumulatedAfterCtoOControlMessages;
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

