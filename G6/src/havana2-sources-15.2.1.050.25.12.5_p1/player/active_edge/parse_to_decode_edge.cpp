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
#include "havana_stream.h"
#include "codec_mme_base.h"
#include "parse_to_decode_edge.h"
#include "decode_to_manifest_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "ParseToDecodeEdge_c"

void ParseToDecodeEdge_c::HandleMarkerFrame(
    Buffer_t Buffer,
    PlayerSequenceNumber_t *SequenceNumberStructure,
    ParsedFrameParameters_t *ParsedFrameParameters)
{
    SE_DEBUG(group_player, "Stream 0x%p Got Marker Frame #%llu\n", mStream, SequenceNumberStructure->Value);

    mSequenceNumber                     = SequenceNumberStructure->Value;
    mMaximumActualSequenceNumberSeen    = max(mSequenceNumber, mMaximumActualSequenceNumberSeen);
    mDiscardingUntilMarkerFrame         = false;

    ProcessAccumulatedBeforeControlMessages(mSequenceNumber, INVALID_TIME);

    SequenceNumberStructure->TimePassToCodec = OS_GetTimeInMicroSeconds();

    PlayerStatus_t Status  = mCodec->Input(Buffer);
    SE_ASSERT(Status == CodecNoError);

    Buffer->DecrementReferenceCount(IdentifierProcessParseToDecode);

    ProcessAccumulatedAfterControlMessages(mSequenceNumber, INVALID_TIME);
}

void ParseToDecodeEdge_c::UpdateCodecErrorStatistics(PlayerStatus_t Status)
{
    switch (Status)
    {
    case CodecError:
        mStream->Statistics().FrameCodecError++;
        SE_WARNING("Stream 0x%p Codec error FrameCodecError %d\n",
                   mStream, mStream->Statistics().FrameCodecError);
        break;

    case CodecUnknownFrame:
        mStream->Statistics().FrameCodecUnknownFrameError++;
        SE_WARNING("Stream 0x%p Codec error FrameCodecUnknownFrameError %d\n",
                   mStream, mStream->Statistics().FrameCodecUnknownFrameError);
        break;

    default:
        mStream->Statistics().FrameCodecError++;
        SE_WARNING("Stream 0x%p Codec unknown error %d\n",
                   mStream, Status);
        break;
    }
}

bool ParseToDecodeEdge_c::TestDiscardBuffer(Buffer_t Buffer, ParsedFrameParameters_t *ParsedFrameParameters)
{
    if (mStream->IsUnPlayable() || mDiscardingUntilMarkerFrame)
    {
        SE_DEBUG(group_player, "Stream 0x%p Frame Dropped stream UnPlayable %d DiscardingUntilMarkerFrame %d\n",
                 mStream, mStream->IsUnPlayable(), mDiscardingUntilMarkerFrame);
        return true;
    }

    PlayerStatus_t Status  = mStream->GetOutputTimer()->TestForFrameDrop(Buffer, OutputTimerBeforeDecodeWindow);

    if (Status != OutputTimerNoError)
    {
        SE_DEBUG(group_player, "Stream 0x%p Frame Dropped before decode window: Status 0x%x (%s) DecodeFrameIndex %d\n",
                 mStream, Status, OutputTimer_c::StringifyOutputTimerStatus(Status), ParsedFrameParameters->DecodeFrameIndex);
        return true;
    }

    if (ParsedFrameParameters->FirstParsedParametersForOutputFrame)
    {
        mStream->GetOutputTimer()->AwaitEntryIntoDecodeWindow(Buffer);
    }

    Status  = mStream->GetOutputTimer()->TestForFrameDrop(Buffer, OutputTimerBeforeDecode);

    if (Status != OutputTimerNoError)
    {
        SE_DEBUG(group_player, "Stream 0x%p Frame Dropped before decode: Status 0x%x (%s) DecodeFrameIndex %d\n",
                 mStream, Status, OutputTimer_c::StringifyOutputTimerStatus(Status), ParsedFrameParameters->DecodeFrameIndex);
        return true;
    }

    // since we have waited for entry in the decode window, we need to reassess discarding conditions
    if (mStream->IsUnPlayable() || mDiscardingUntilMarkerFrame || mStream->IsTerminating() || mStream->IsLowPowerState())
    {
        SE_DEBUG(group_player, "Stream 0x%p Frame Dropped stream UnPlayable=%d DiscardingUntilMarkerFrame %d Terminating %d IsLowPowerState %d\n",
                 mStream, mStream->IsUnPlayable(), mDiscardingUntilMarkerFrame, mStream->IsTerminating(), mStream->IsLowPowerState());
        return true;
    }

    return false;
}

void ParseToDecodeEdge_c::HandleCodedFrameBuffer(
    Buffer_t Buffer,
    PlayerSequenceNumber_t *SequenceNumberStructure,
    ParsedFrameParameters_t *ParsedFrameParameters)
{
    mSequenceNumber                     = SequenceNumberStructure->Value;
    mMaximumActualSequenceNumberSeen    = max(mSequenceNumber, mMaximumActualSequenceNumberSeen);

    ProcessAccumulatedBeforeControlMessages(mSequenceNumber, INVALID_TIME);

    bool DiscardBuffer = TestDiscardBuffer(Buffer, ParsedFrameParameters);

    if (!DiscardBuffer)
    {
        // handle the case where stream parameters changed while we were discarding
        if (mPromoteNextStreamParametersToNew && (ParsedFrameParameters->StreamParameterStructure != NULL))
        {
            ParsedFrameParameters->NewStreamParameters  = true;
            mPromoteNextStreamParametersToNew        = false;
        }

        SequenceNumberStructure->TimePassToCodec        = OS_GetTimeInMicroSeconds();

        //
        // Check any config update, unless a StreamSwitch is ongoing.
        //
        if (CheckDecoderConfig() &&
            ((mStream->SwitchingToCodec == NULL) || (mStream->SwitchingToCodec == mStream->GetCodec())))
        {
            int update = GetDecoderConfig();
            if (update != 0)
            {
                mCodec->UpdateConfig(update);
            }
        }

        PlayerStatus_t Status  = mCodec->Input(Buffer);

        if (Status != CodecNoError)
        {
            UpdateCodecErrorStatistics(Status);
            DiscardBuffer = true;
        }
    }

    if (DiscardBuffer)
    {
        // SE-PIPELINE TRACE
        SE_VERBOSE(group_player, "Stream 0x%p - %d - #%lld PTS=%lld Discard=%llu\n",
                   mStream,
                   mStream->GetStreamType(),
                   SequenceNumberStructure->CollatedFrameCounter,
                   SequenceNumberStructure->PTS,
                   SequenceNumberStructure->TimeEntryInProcess1
                  );

        if (ParsedFrameParameters->NewStreamParameters)
        {
            mPromoteNextStreamParametersToNew      = true;
        }

        if (ParsedFrameParameters->FirstParsedParametersForOutputFrame)
        {
            RecordNonDecodedFrame(Buffer, ParsedFrameParameters);
            mCodec->OutputPartialDecodeBuffers();
        }
    }
    else
    {
        // SE-PIPELINE TRACE
        SE_VERBOSE(group_player, "Stream 0x%p - %d - #%lld PTS=%lld PtD=%llu\n",
                   mStream,
                   mStream->GetStreamType(),
                   SequenceNumberStructure->CollatedFrameCounter,
                   SequenceNumberStructure->PTS,
                   SequenceNumberStructure->TimeEntryInProcess1
                  );
    }

    Buffer->DecrementReferenceCount(IdentifierProcessParseToDecode);

    ProcessAccumulatedAfterControlMessages(mSequenceNumber, INVALID_TIME);
}

void ParseToDecodeEdge_c::HandleCodedFrameBufferType(Buffer_t Buffer)
{
    PlayerSequenceNumber_t  *SequenceNumberStructure;
    Buffer->ObtainMetaDataReference(mStream->GetPlayer()->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
    SE_ASSERT(SequenceNumberStructure != NULL);

    ParsedFrameParameters_t *ParsedFrameParameters;
    Buffer->ObtainMetaDataReference(mStream->GetPlayer()->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    SequenceNumberStructure->TimeEntryInProcess1        = OS_GetTimeInMicroSeconds();
    SequenceNumberStructure->PTS                        = ParsedFrameParameters->NativePlaybackTime;

    if (SequenceNumberStructure->MarkerFrame)
    {
        HandleMarkerFrame(Buffer, SequenceNumberStructure, ParsedFrameParameters);
    }
    else
    {
        HandleCodedFrameBuffer(Buffer, SequenceNumberStructure, ParsedFrameParameters);

        // SE-PIPELINE TRACE
        SE_VERBOSE(group_player, "Stream 0x%p - %d - #%lld PtD=%llu\n",
                   mStream,
                   mStream->GetStreamType(),
                   SequenceNumberStructure->CollatedFrameCounter,
                   SequenceNumberStructure->TimeEntryInProcess1
                  );
    }
}

void   ParseToDecodeEdge_c::MainLoop()
{
    RingStatus_t                      RingStatus;
    Buffer_t                          Buffer;
    BufferType_t                      BufferType;

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
        // If low power state, thread must stay asleep until low power exit signal
        if (mStream->IsLowPowerState())
        {
            SE_INFO(group_player, "Stream 0x%p ParseToDecode thread entering low power..\n", mStream);
            // Signal end of low power processing
            // TBD: are we sure that all pending MME commands are completed here ???
            mStream->SetLowPowerEnterEvent();
            // Forever wait for wake-up event
            mStream->WaitForLowPowerExitEvent();
            SE_INFO(group_player, "Stream 0x%p ParseToDecode thread exiting low power..\n", mStream);
        }

        RingStatus      = mInputRing->Extract((uintptr_t *)(&Buffer), PLAYER_MAX_EVENT_WAIT);

        if ((RingStatus == RingNothingToGet) || (Buffer == NULL))
        {
            continue;
        }

        Buffer->GetType(&BufferType);
        Buffer->TransferOwnership(IdentifierProcessParseToDecode);

        //
        // If we were set to terminate while we were 'Extracting' we should
        // remove the buffer reference and exit.
        //

        if (mStream->IsTerminating() || mStream->IsLowPowerState())
        {
            Buffer->DecrementReferenceCount(IdentifierProcessParseToDecode);
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


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Function to record a frame as non-decoded
//

void   ParseToDecodeEdge_c::RecordNonDecodedFrame(Buffer_t                  Buffer,
                                                  ParsedFrameParameters_t  *ParsedFrameParameters)
{
    //
    // Fill me a record
    //
    OS_LockMutex(&mStream->NonDecodedBuffersLock);

    unsigned int i;
    for (i = 0; i < PLAYER_MAX_DISCARDED_FRAMES; i++)
        if (mStream->NonDecodedBuffers[i].Buffer == NULL)
        {
            //
            // Do we want the buffer, or can we take the index now
            //
            mStream->NonDecodedBuffers[i].ReleasedBuffer = ParsedFrameParameters->DisplayFrameIndex != INVALID_INDEX;

            if (mStream->NonDecodedBuffers[i].ReleasedBuffer)
            {
                mStream->NonDecodedBuffers[i].DisplayFrameIndex  = ParsedFrameParameters->DisplayFrameIndex;

                if (mDiscardingUntilMarkerFrame || ParsedFrameParameters->CollapseHolesInDisplayIndices)
                {
                    mStream->DisplayIndicesCollapse    = max(mStream->DisplayIndicesCollapse, ParsedFrameParameters->DisplayFrameIndex);
                }
            }
            else
            {
                Buffer->IncrementReferenceCount(IdentifierNonDecodedFrameList);
                mStream->NonDecodedBuffers[i].ParsedFrameParameters      = ParsedFrameParameters;        // Check for it later
                //
                // Shrink that buffer
                //
                Buffer->SetUsedDataSize(0);
                unsigned int BlockSize;
                Buffer->ObtainDataReference(&BlockSize, NULL, NULL);
                if (BlockSize != 0)
                {
                    BufferStatus_t Status = Buffer->ShrinkBuffer(0);
                    if (Status != BufferNoError)
                    {
                        SE_ERROR("Stream 0x%p Failed to shrink buffer\n", mStream);
                    }
                }
            }

            mStream->NonDecodedBuffers[i].Buffer                 = Buffer;
            mStream->InsertionsIntoNonDecodedBuffers++;

            //
            // Would it be wise to poke the post decode process to check the non decoded list
            //

            if ((mStream->InsertionsIntoNonDecodedBuffers - mStream->RemovalsFromNonDecodedBuffers) >
                (PLAYER_MAX_DISCARDED_FRAMES - 2))
            {
                OS_UnLockMutex(&mStream->NonDecodedBuffersLock);
                mStream->DecodeToManifestEdge->Insert((uintptr_t)NULL);
                OS_LockMutex(&mStream->NonDecodedBuffersLock);
            }

            OS_UnLockMutex(&mStream->NonDecodedBuffersLock);
            return;
        }

    OS_UnLockMutex(&mStream->NonDecodedBuffersLock);
}

PlayerStatus_t   ParseToDecodeEdge_c::CallInSequence(
    PlayerSequenceType_t      SequenceType,
    PlayerSequenceValue_t     SequenceValue,
    PlayerComponentFunction_t Fn,
    ...)
{
    va_list                   List;
    Buffer_t                  ControlStructureBuffer;
    PlayerControlStructure_t *ControlStructure;

    BufferStatus_t Status = mStream->GetPlayer()->GetControlStructurePool()->GetBuffer(&ControlStructureBuffer, IdentifierInSequenceCall);
    if (Status != BufferNoError)
    {
        SE_ERROR("Stream 0x%p Failed to get a control structure buffer\n", mStream);
        return PlayerError;
    }

    ControlStructureBuffer->ObtainDataReference(NULL, NULL, (void **)(&ControlStructure));
    SE_ASSERT(ControlStructure != NULL); // not expected to be empty
    ControlStructure->Action            = ActionInSequenceCall;
    ControlStructure->SequenceType      = SequenceType;
    ControlStructure->SequenceValue     = SequenceValue;
    ControlStructure->InSequence.Fn     = Fn;

    switch (Fn)
    {
    case CodecFnOutputPartialDecodeBuffers:
        break;

    case CodecFnDiscardQueuedDecodes:
        break;

    case CodecFnReleaseReferenceFrame:
        va_start(List, Fn);
        ControlStructure->InSequence.UnsignedInt        = va_arg(List, unsigned int);
        va_end(List);
        SE_VERBOSE(group_player, "Stream 0x%p Requesting a release %d\n", mStream, ControlStructure->InSequence.UnsignedInt);
        break;

    case PlayerFnSwitchCodec:
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

PlayerStatus_t   ParseToDecodeEdge_c::PerformInSequenceCall(PlayerControlStructure_t *ControlStructure)
{
    PlayerStatus_t  Status = PlayerNoError;

    switch (ControlStructure->InSequence.Fn)
    {
    case CodecFnOutputPartialDecodeBuffers:
        Status = mCodec->OutputPartialDecodeBuffers();
        if (Status != PlayerNoError) { SE_ERROR("Stream 0x%p Failed InSequence call (CodecFnOutputPartialDecodeBuffers)\n", mStream); }
        break;

    case CodecFnDiscardQueuedDecodes:
        Status = mCodec->DiscardQueuedDecodes();
        if (Status != PlayerNoError) { SE_ERROR("Stream 0x%p Failed InSequence call (CodecFnDiscardQueuedDecodes)\n", mStream); }
        break;

    case CodecFnReleaseReferenceFrame:
        SE_VERBOSE(group_player, "Stream 0x%p Performing a release %d\n", mStream, ControlStructure->InSequence.UnsignedInt);
        Status = mCodec->ReleaseReferenceFrame(ControlStructure->InSequence.UnsignedInt);
        if (Status != PlayerNoError) { SE_ERROR("Stream 0x%p Failed InSequence call (CodecFnReleaseReferenceFrame)\n", mStream); }
        break;

    case PlayerFnSwitchCodec:
        SwitchCodec();
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
//      Switch stream component function for the Codec
//

void ParseToDecodeEdge_c::SwitchCodec()
{
    SE_DEBUG(group_player, "Stream 0x%p\n", mStream);

    //
    // Wait for the last inserted decode to come out the other end
    //
    OS_Status_t WaitStatus = OS_WaitForEventAuto(&mStream->SwitchStreamLastOneOutOfTheCodec, PLAYER_MAX_DISCARD_DRAIN_TIME);
    if (WaitStatus == OS_TIMED_OUT)
    {
        SE_ERROR("Stream 0x%p Last decode did not complete in reasonable time\n", mStream);
    }

    //
    // Halt the current Codec;  will be destroyed during stream switch
    //
    mCodec->OutputPartialDecodeBuffers();
    mCodec->ReleaseReferenceFrame(CODEC_RELEASE_ALL);
    mCodec->Halt();
    // as mStream would be associated with new codec, so sending NULL value of mStream for current codec
    // it is done to send NULL values (updated value) to respective codecs bug 57576
    // so that already deleted/removed Stream pointer is not dereferenced
    mCodec->RegisterPlayer(NULL, NULL, NULL);

    mStream->CodecSwitchingOn();
    // Memory barrier avoid reordering with codec switching statements
    OS_Smp_Mb();//Write memory barrier: wmb_for_Stream_IsCodecSwitching coupled with: rmb_for_Stream_IsCodecSwitching

    //
    // Inform the decode buffer manager we are doing the switch
    //
    mStream->GetDecodeBufferManager()->EnterStreamSwitch();

    //
    // Switch over to the new Codec
    //

    SE_ASSERT((mStream->SwitchingToCodec != NULL) && (mStream->SwitchingToCodec != mCodec));
    mCodec = mStream->SwitchingToCodec;

    //
    // Initialize the Codec -- CodecSelectTransformer already set for the codec
    //
    mCodec->RegisterPlayer(mStream->GetPlayer(), mStream->GetPlayback(), mStream);

    PlayerStatus_t Status  = mCodec->Connect(mStream->DecodeToManifestEdge->GetInputPort());

    //
    // When a frame reaches this point, we can safely mark the state
    // of the codec as being initialized for this and subsequent frames.
    // This means that these frames will be released via the codec, where
    // the codec tables will be appropriately handled
    //
    //

    OS_Smp_Mb();//Write memory barrier: wmb_for_Stream_IsCodecSwitching coupled with: rmb_for_Stream_IsCodecSwitching
    mStream->CodecSwitchingOff();

    //
    // Ensure the Config parameters will be re-applied upon next ::input()
    //
    ReapplyDecoderConfig();

    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p Failed to register stream parameters with Codec\n", mStream);
    }

    OS_SemaphoreSignal(&mStream->mSemaphoreStreamSwitchCodec);

    SE_DEBUG(group_player, "Stream 0x%p completed\n", mStream);
}
