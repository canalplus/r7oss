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
#include "collate_to_parse_edge.h"
#include "parse_to_decode_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "CollateToParseEdge_c"

void CollateToParseEdge_c::HandleMarkerFrame(Buffer_t Buffer, PlayerSequenceNumber_t *SequenceNumberStructure)
{
    SE_DEBUG(group_player, "Stream 0x%p Got Marker Frame #%llu\n", mStream, SequenceNumberStructure->Value);

    mNextBufferSequenceNumber           = SequenceNumberStructure->Value + 1;
    mSequenceNumber                     = SequenceNumberStructure->Value;
    mMaximumActualSequenceNumberSeen    = max(mSequenceNumber, mMaximumActualSequenceNumberSeen);

    if (SequenceNumberStructure->MarkerType == DrainDiscardMarker)
    {
        mDiscardingUntilMarkerFrame = false;
    }

    ProcessAccumulatedBeforeControlMessages(mSequenceNumber, INVALID_TIME);

    mFrameParser->Input(Buffer);

    Buffer->DecrementReferenceCount(IdentifierProcessCollateToParse);

    ProcessAccumulatedAfterControlMessages(mSequenceNumber, INVALID_TIME);
}

void CollateToParseEdge_c::HandleCodedFrameBuffer(Buffer_t Buffer, PlayerSequenceNumber_t *SequenceNumberStructure)
{
    SequenceNumberStructure->Value      = mNextBufferSequenceNumber++;
    mSequenceNumber                     = SequenceNumberStructure->Value;
    mMaximumActualSequenceNumberSeen    = max(mSequenceNumber, mMaximumActualSequenceNumberSeen);

    ProcessAccumulatedBeforeControlMessages(mSequenceNumber, INVALID_TIME);

    unsigned int              BufferLength;
    Buffer->ObtainDataReference(NULL, &BufferLength, NULL);
    bool DiscardBuffer = (BufferLength != 0) && (mStream->IsUnPlayable() || mDiscardingUntilMarkerFrame);

    if (!DiscardBuffer)
    {
        // SE-PIPELINE TRACE
        SE_VERBOSE(group_player, "Stream 0x%p - %d - #%lld CtP=%llu\n",
                   mStream,
                   mStream->GetStreamType(),
                   SequenceNumberStructure->CollatedFrameCounter,
                   SequenceNumberStructure->TimeEntryInProcess0
                  );

        mFrameParser->Input(Buffer);
    }
    else
    {
        // SE-PIPELINE TRACE
        SE_VERBOSE(group_player, "Stream 0x%p - %d - #%lld Discard=%llu\n",
                   mStream,
                   mStream->GetStreamType(),
                   SequenceNumberStructure->CollatedFrameCounter,
                   SequenceNumberStructure->TimeEntryInProcess0
                  );

    }

    Buffer->DecrementReferenceCount(IdentifierProcessCollateToParse);

    ProcessAccumulatedAfterControlMessages(mSequenceNumber, INVALID_TIME);
}

void CollateToParseEdge_c::HandleCodedFrameBufferType(Buffer_t Buffer)
{
    PlayerSequenceNumber_t *SequenceNumberStructure;
    Buffer->ObtainMetaDataReference(mStream->GetPlayer()->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
    SE_ASSERT(SequenceNumberStructure != NULL);

    SequenceNumberStructure->TimeEntryInProcess0    = OS_GetTimeInMicroSeconds();

    if (SequenceNumberStructure->MarkerFrame)
    {
        HandleMarkerFrame(Buffer, SequenceNumberStructure);
    }
    else
    {
        HandleCodedFrameBuffer(Buffer, SequenceNumberStructure);

    }

}

void   CollateToParseEdge_c::MainLoop()
{
    RingStatus_t              RingStatus;
    Buffer_t                  Buffer;
    BufferType_t              BufferType;

    //
    // Signal we have started
    //
    OS_LockMutex(&mStream->StartStopLock);
    mStream->ProcessRunningCount++;
    OS_SetEvent(&mStream->StartStopEvent);
    OS_UnLockMutex(&mStream->StartStopLock);

    SE_DEBUG(group_player, "process starting Stream 0x%p\n", mStream);

    //
    // Main Loop
    //
    while (!mStream->IsTerminating())
    {
        RingStatus  = mInputRing->Extract((uintptr_t *)(&Buffer), PLAYER_MAX_EVENT_WAIT);

        if ((RingStatus == RingNothingToGet) || (Buffer == NULL))
        {
            continue;
        }

        Buffer->GetType(&BufferType);
        Buffer->TransferOwnership(IdentifierProcessCollateToParse);

        //
        // If we were set to terminate while we were 'Extracting' we should
        // remove the buffer reference and exit.
        //

        if (mStream->IsTerminating())
        {
            Buffer->DecrementReferenceCount(IdentifierProcessCollateToParse);
            continue;
        }

        if (BufferType == mStream->GetCodedFrameBufferType())
        {
            HandleCodedFrameBufferType(Buffer);
        }
        else if (BufferType == mStream->GetPlayer()->BufferPlayerControlStructureType)
        {
            HandlePlayerControlStructure(Buffer, mSequenceNumber, INVALID_TIME, mMaximumActualSequenceNumberSeen);
        }
        else
        {
            SE_ERROR("Unknown buffer type received - Implementation error\n");
            Buffer->DecrementReferenceCount();
        }
    }

    SE_DEBUG(group_player, "process terminating Stream 0x%p\n", mStream);

    // At this point, Stream->IsTerminating() must be true
    OS_Smp_Mb(); // Read memory barrier: rmb_for_Stream_Terminating coupled with: wmb_for_Stream_Terminating

    //
    // Signal we have terminated
    //
    OS_LockMutex(&mStream->StartStopLock);
    mStream->ProcessRunningCount--;
    OS_SetEvent(&mStream->StartStopEvent);
    OS_UnLockMutex(&mStream->StartStopLock);
}


PlayerStatus_t   CollateToParseEdge_c::CallInSequence(
    PlayerSequenceType_t      SequenceType,
    PlayerSequenceValue_t     SequenceValue,
    PlayerComponentFunction_t Fn,
    ...)
{
    Buffer_t                  ControlStructureBuffer;

    BufferStatus_t Status = mStream->GetPlayer()->GetControlStructurePool()->GetBuffer(&ControlStructureBuffer, IdentifierInSequenceCall);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to get a control structure buffer\n");
        return Status;
    }

    PlayerControlStructure_t *ControlStructure;
    ControlStructureBuffer->ObtainDataReference(NULL, NULL, (void **)(&ControlStructure));
    SE_ASSERT(ControlStructure != NULL);  // not supposed to be empty

    ControlStructure->Action            = ActionInSequenceCall;
    ControlStructure->SequenceType      = SequenceType;
    ControlStructure->SequenceValue     = SequenceValue;
    ControlStructure->InSequence.Fn     = Fn;

    switch (Fn)
    {
    case PlayerFnSwitchFrameParser:
        break;

    default:
        SE_ERROR("Unsupported function call\n");
        ControlStructureBuffer->DecrementReferenceCount(IdentifierInSequenceCall);
        return PlayerNotSupported;
    }

    RingStatus_t ringStatus = mInputRing->Insert((uintptr_t)ControlStructureBuffer);
    if (ringStatus != RingNoError) { return PlayerError; }

    return PlayerNoError;
}

PlayerStatus_t   CollateToParseEdge_c::PerformInSequenceCall(PlayerControlStructure_t *ControlStructure)
{
    PlayerStatus_t  Status = PlayerNoError;

    switch (ControlStructure->InSequence.Fn)
    {
    case PlayerFnSwitchFrameParser:
        SwitchFrameParser();
        break;

    default:
        SE_ERROR("Unsupported function call - Implementation error\n");
        Status  = PlayerNotSupported;
        break;
    }

    return Status;
}



// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Switch stream component function for the FrameParser
//

void    CollateToParseEdge_c::SwitchFrameParser()
{
    PlayerStatus_t  Status;
    unsigned int    LastDecodeFrameIndex;

    SE_DEBUG(group_player, "Stream 0x%p\n", mStream);

    //
    // Halt the current FrameParser; will be destroyed during stream switch
    //
    mFrameParser->Halt();

    //
    // Wait for the last decode to come out the other end, of the codec
    //
    OS_Status_t WaitStatus = OS_WaitForEventAuto(&mStream->SwitchStreamLastOneOutOfTheCodec, PLAYER_MAX_MARKER_TIME_THROUGH_CODEC);
    if (WaitStatus == OS_TIMED_OUT)
    {
        SE_ERROR("Stream 0x%p Last decode did not complete in reasonable time\n", mStream);
    }

    //
    mFrameParser->GetNextDecodeFrameIndex(&LastDecodeFrameIndex);

    mStream->GetPlayer()->SetLastNativeTime(mStream->GetPlayback(), INVALID_TIME);

    //
    // Switch over to the new FrameParser
    //
    SE_ASSERT((mStream->SwitchingToFrameParser != NULL) && (mStream->SwitchingToFrameParser != mFrameParser));
    mFrameParser = mStream->SwitchingToFrameParser;

    //
    // Initialize the FrameParser
    //
    mFrameParser->RegisterPlayer(mStream->GetPlayer(), mStream->GetPlayback(), mStream);
    Status  = mFrameParser->Connect(mStream->ParseToDecodeEdge->GetInputPort());

    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p Failed to register stream parameters with FrameParser\n", mStream);
    }

    mFrameParser->SetNextFrameIndices(LastDecodeFrameIndex);

    OS_SemaphoreSignal(&mStream->mSemaphoreStreamSwitchFrameParser);

    SE_DEBUG(group_player, "Stream 0x%p completed\n", mStream);
}
