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

#include <stdarg.h>

#include "encoder.h"

#undef TRACE_TAG
#define TRACE_TAG "EncodeStream_c"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Implement a call in sequence
//

EncoderStatus_t   EncodeStream_c::CallInSequence(
    EncodeSequenceType_t      SequenceType,
    EncodeSequenceValue_t     SequenceValue,
    EncodeComponentFunction_t Fn,
    ...)
{
    va_list                   List;
    Buffer_t                  ControlStructureBuffer;
    EncodeControlStructure_t *ControlStructure;
// Ring_t                    DestinationRing; TODO add support for DestinationRing
    //
    // Garner a control structure, fill it in
    //
    BufferStatus_t Status = Encoder.Encode->GetControlStructurePool()->GetBuffer(&ControlStructureBuffer, IdentifierInSequenceCall);
    if (Status != BufferNoError)
    {
        SE_ERROR("Stream 0x%p: Failed to get a control structure buffer\n", this);
        return EncoderError;
    }

    ControlStructureBuffer->ObtainDataReference(NULL, NULL, (void **)(&ControlStructure));
    SE_ASSERT(ControlStructure != NULL);  // not supposed to be empty
    ControlStructure->Action            = EncodeActionInSequenceCall;
    ControlStructure->SequenceType      = SequenceType;
    ControlStructure->SequenceValue     = SequenceValue;
    ControlStructure->InSequence.Fn     = Fn;

    // DestinationRing                     = NULL;

    switch (Fn)
    {
    default:
        // Example use of va_start, va_end
        va_start(List, Fn);
        va_end(List);
        SE_ERROR("Stream 0x%p: Unsupported function call\n", this);
        ControlStructureBuffer->DecrementReferenceCount(IdentifierInSequenceCall);
        return EncoderNotSupported;
    }

    //
    // Send it to the appropriate process
    //
    // DestinationRing->Insert( (uintptr_t)ControlStructureBuffer );
    return EncoderNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Perform in sequence call
//

EncoderStatus_t   EncodeStream_c::PerformInSequenceCall(
    EncodeControlStructure_t *ControlStructure)
{
    EncoderStatus_t  Status;
    Status      = EncoderNoError;

    switch (ControlStructure->InSequence.Fn)
    {
    default:
        SE_ERROR("Stream 0x%p: Not supported\n", this);
        Status  = EncoderNotSupported;
        break;
    }

    return Status;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Store a control message in the appropriate table
//

EncoderStatus_t   EncodeStream_c::AccumulateControlMessage(
    Buffer_t                          Buffer,
    EncodeControlStructure_t         *Message,
    unsigned int                     *MessageCount,
    unsigned int                      MessageTableSize,
    EncodeBufferRecord_t             *MessageTable)
{
    unsigned int    i;

    for (i = 0; i < MessageTableSize; i++)
        if (MessageTable[i].Buffer == NULL)
        {
            MessageTable[i].Buffer              = Buffer;
            MessageTable[i].ControlStructure    = Message;
            (*MessageCount)++;
            return EncoderNoError;
        }

    SE_ERROR("Stream 0x%p: Implementation constant range error\n", this);
    Buffer->DecrementReferenceCount();
    return EncoderImplementationError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Process a single control message
//

EncoderStatus_t   EncodeStream_c::ProcessControlMessage(
    Buffer_t                          Buffer,
    EncodeControlStructure_t         *Message)
{
    EncoderStatus_t  Status;

    switch (Message->Action)
    {
    case EncodeActionInSequenceCall:
        Status  = PerformInSequenceCall(Message);
        if (Status != EncoderNoError)
        {
            SE_ERROR("Stream 0x%p: Failed InSequence call (%08x)\n", this, Status);
        }
        break;

    default:
        SE_ERROR("Stream 0x%p: Implementation error\n", this);
        Status  = EncoderImplementationError;
        break;
    }

    Buffer->DecrementReferenceCount();
    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Scan the table looking for messages to be actioned at, or before, this point
//

EncoderStatus_t   EncodeStream_c::ProcessAccumulatedControlMessages(
    unsigned int          *MessageCount,
    unsigned int           MessageTableSize,
    EncodeBufferRecord_t  *MessageTable,
    unsigned long long     SequenceNumber,
    unsigned long long     Time)
{
    unsigned int    i;
    bool            SequenceCheck;
    bool            ProcessNow;

    // If we have no messages to scan return in a timely fashion
    if (*MessageCount == 0)
    {
        return EncoderNoError;
    }

    // Perform the scan
    for (i = 0; i < MessageTableSize; i++)
    {
        if (MessageTable[i].Buffer != NULL)
        {
            SequenceCheck       = (MessageTable[i].ControlStructure->SequenceType == SequenceTypeBeforeSequenceNumber) ||
                                  (MessageTable[i].ControlStructure->SequenceType == SequenceTypeAfterSequenceNumber);
            ProcessNow          = SequenceCheck ? ((SequenceNumber != INVALID_SEQUENCE_VALUE) && (MessageTable[i].ControlStructure->SequenceValue <= SequenceNumber)) :
                                  ((Time           != INVALID_SEQUENCE_VALUE) && (MessageTable[i].ControlStructure->SequenceValue <= Time));

            if (ProcessNow)
            {
                ProcessControlMessage(MessageTable[i].Buffer, MessageTable[i].ControlStructure);
                MessageTable[i].Buffer                  = NULL;
                MessageTable[i].ControlStructure        = NULL;
                (*MessageCount)--;
            }
        }
    }

    return EncoderNoError;
}
