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

#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "active_edge_base.h"

#undef TRACE_TAG
#define TRACE_TAG "ActiveEdge_Base_c"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Store a control message in the appropriate table
//

PlayerStatus_t   ActiveEdge_Base_c::AccumulateControlMessage(
    Buffer_t                          Buffer,
    PlayerControlStructure_t         *Message,
    unsigned int                     *MessageCount,
    unsigned int                      MessageTableSize,
    PlayerBufferRecord_t             *MessageTable)
{
    unsigned int    i;

    for (i = 0; i < MessageTableSize; i++)
    {
        if (MessageTable[i].Buffer == NULL)
        {
            MessageTable[i].Buffer              = Buffer;
            MessageTable[i].ControlStructure    = Message;
            (*MessageCount)++;
            return PlayerNoError;
        }
    }

    SE_ERROR("Message table full - Implementation constant range error\n");
    Buffer->DecrementReferenceCount();
    return PlayerImplementationError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Process a single control message
//

PlayerStatus_t   ActiveEdge_Base_c::ProcessControlMessage(
    Buffer_t                          Buffer,
    PlayerControlStructure_t         *Message)
{
    PlayerStatus_t  Status;

    switch (Message->Action)
    {
    case ActionInSequenceCall:
        Status  = PerformInSequenceCall(Message);
        if (Status != PlayerNoError)
        {
            SE_ERROR("Failed InSequence call (%08x)\n", Status);
        }

        break;

    default:
        SE_ERROR("Unhandled control structure - Implementation error\n");
        Status  = PlayerImplementationError;
        break;
    }

    Buffer->DecrementReferenceCount();
    return Status;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Scan the table looking for messages to be actioned at, or before, this point
//

PlayerStatus_t   ActiveEdge_Base_c::ProcessAccumulatedControlMessages(
    unsigned int                     *MessageCount,
    unsigned int                      MessageTableSize,
    PlayerBufferRecord_t             *MessageTable,
    unsigned long long                SequenceNumber,
    unsigned long long                Time)
{
    unsigned int    i;
    bool            SequenceCheck;
    bool            ProcessNow;

    //
    // If we have no messages to scan return in a timely fashion
    //

    if (*MessageCount == 0)
    {
        return PlayerNoError;
    }

    //
    // Perform the scan
    //

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

    return PlayerNoError;
}

void ActiveEdge_Base_c::HandlePlayerControlStructure(Buffer_t Buffer, unsigned long long SequenceNumber, unsigned long long Time, unsigned long long MaximumActualSequenceNumberSeen)
{
    PlayerControlStructure_t  *ControlStructure;

    Buffer->ObtainDataReference(NULL, NULL, (void **)(&ControlStructure));
    SE_ASSERT(ControlStructure != NULL);  // control buffer not supposed to be empty

    bool ProcessNow  = (ControlStructure->SequenceType == SequenceTypeImmediate);

    if (!ProcessNow)
    {
        bool SequenceCheck   = (ControlStructure->SequenceType == SequenceTypeBeforeSequenceNumber) ||
                               (ControlStructure->SequenceType == SequenceTypeAfterSequenceNumber);
        ProcessNow      = SequenceCheck ?
                          ((SequenceNumber != INVALID_SEQUENCE_VALUE) && (ControlStructure->SequenceValue <= MaximumActualSequenceNumberSeen)) :
                          (ValidTime(Time) && (ControlStructure->SequenceValue <= Time));
    }

    if (ProcessNow)
    {
        ProcessControlMessage(Buffer, ControlStructure);
    }
    else if ((ControlStructure->SequenceType == SequenceTypeBeforeSequenceNumber) ||
             (ControlStructure->SequenceType == SequenceTypeBeforePlaybackTime))
    {
        AccumulateControlMessage(Buffer, ControlStructure, &mAccumulatedBeforeControlMessagesCount, ACTIVE_EDGE_MAX_MESSAGES, mAccumulatedBeforeControlMessages);
    }
    else
    {
        AccumulateControlMessage(Buffer, ControlStructure, &mAccumulatedAfterControlMessagesCount, ACTIVE_EDGE_MAX_MESSAGES, mAccumulatedAfterControlMessages);
    }
}

void ActiveEdge_Base_c::ProcessAccumulatedBeforeControlMessages(unsigned long long SequenceNumber, unsigned long long Time)
{
    ProcessAccumulatedControlMessages(
        &mAccumulatedBeforeControlMessagesCount, ACTIVE_EDGE_MAX_MESSAGES, mAccumulatedBeforeControlMessages, SequenceNumber, Time);
}

void ActiveEdge_Base_c::ProcessAccumulatedAfterControlMessages(unsigned long long SequenceNumber, unsigned long long Time)
{
    ProcessAccumulatedControlMessages(
        &mAccumulatedAfterControlMessagesCount, ACTIVE_EDGE_MAX_MESSAGES, mAccumulatedAfterControlMessages, SequenceNumber, Time);
}
