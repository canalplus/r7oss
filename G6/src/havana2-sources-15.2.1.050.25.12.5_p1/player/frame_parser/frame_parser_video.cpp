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

#include "stack_generic.h"
#include "ring_generic.h"
#include "st_relayfs_se.h"
#include "frame_parser_video.h"
#include "parse_to_decode_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_Video_c"

#define ANTI_EMULATION_BUFFER_SIZE      1024

#define DEFAULT_DECODE_TIME_OFFSET      3600            // thats 40ms in 90Khz ticks
#define MAXIMUM_DECODE_TIME_OFFSET      (4 * 90000)     // thats 4s in 90Khz ticks

#define MAX_ALLOWED_SMOOTH_REVERSE_PLAY_FAILURES   2    // After this we assume reverse play just isn't practical

#define ValidFrameRate(F)   inrange( F, 7, 120 )

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//
FrameParser_Video_c::FrameParser_Video_c(void)
    : FrameParser_Base_c()
    , ParsedVideoParameters(NULL)
    , NextDisplayFieldIndex(0)
    , NextDecodeFieldIndex(0)
    , CollapseHolesInDisplayIndices(true)
    , NewStreamParametersSeenButNotQueued(false)
    , ReverseDecodeUnsatisfiedReferenceStack(NULL)
    , ReverseDecodeSingleFrameStack(NULL)
    , ReverseDecodeStack(NULL)
    , CodedFramePlaybackTimeValid(false)
    , CodedFramePlaybackTime(0)
    , CodedFrameDecodeTimeValid(false)
    , CodedFrameDecodeTime(0)
    , LastRecordedPlaybackTimeDisplayFieldIndex(0)
    , LastRecordedNormalizedPlaybackTime(INVALID_TIME)
    , LastRecordedDecodeTimeFieldIndex(0)
    , LastRecordedNormalizedDecodeTime(INVALID_TIME)
    , ReferenceFrameList()
    , ReverseQueuedPostDecodeSettingsRing(NULL)
    , FirstDecodeOfFrame(true)
    , AccumulatedPictureStructure(StructureEmpty)
    , OldAccumulatedPictureStructure(StructureEmpty)
    , DeferredCodedFrameBuffer(NULL)
    , DeferredParsedFrameParameters(NULL)
    , DeferredParsedVideoParameters(NULL)
    , DeferredCodedFrameBufferSecondField(NULL)
    , DeferredParsedFrameParametersSecondField(NULL)
    , DeferredParsedVideoParametersSecondField(NULL)
    , LastFieldRate(1)
    , NumberOfUtilizedFrameParameters(0)
    , NumberOfUtilizedStreamParameters(0)
    , NumberOfUtilizedDecodeBuffers(0)
    , RevPlayDiscardingState(false)
    , RevPlayAccumulatedFrameCount(0)
    , RevPlayDiscardedFrameCount(0)
    , RevPlaySmoothReverseFailureCount(0)
    , AntiEmulationContent(0)
    , AntiEmulationBuffer(NULL)
    , mAntiEmulationBufferMaxSize(ANTI_EMULATION_BUFFER_SIZE)
    , AntiEmulationSource(NULL)
    , LastSeenContentFrameRate(0)
    , LastSeenContentWidth(0)
    , LastSeenContentHeight(0)
    , StandardPTSDeducedFrameRate(false)
    , LastStandardPTSDeducedFrameRate(INVALID_FRAMERATE)
    , DeduceFrameRateElapsedFields(0)
    , DeduceFrameRateElapsedTime(0)
    , DefaultFrameRate(24)
    , ContainerFrameRate(INVALID_FRAMERATE)
    , PTSDeducedFrameRate(INVALID_FRAMERATE)
    , StreamEncodedFrameRate(INVALID_FRAMERATE)
    , LastResolvedFrameRate(INVALID_FRAMERATE)
    , LastReverseDecodeWasIndependentFirstField(false)
    , GotSequenceEndCode(false)
    , mMinFrameWidth(DELTA_MIN_FRAME_WIDTH)
    , mMaxFrameWidth(DELTA_MAX_FRAME_WIDTH)
    , mMinFrameHeight(DELTA_MIN_FRAME_HEIGHT)
    , mMaxFrameHeight(DELTA_MAX_FRAME_HEIGHT)
    , RelayfsIndex(0)
{
    if (InitializationStatus != FrameParserNoError)
    {
        return;
    }

    SetGroupTrace(group_frameparser_video);

    Configuration.SupportSmoothReversePlay  = true;     // By default we support smooth reverse
    Configuration.InitializeStartCodeList   = true;     // By default we get the start code lists

    RelayfsIndex = st_relayfs_getindex_fortype_se(ST_RELAY_TYPE_CODED_VIDEO_BUFFER);

    // TODO(pht) move to a FinalizeInit method
    AntiEmulationBuffer = new unsigned char[mAntiEmulationBufferMaxSize];
    if (AntiEmulationBuffer == NULL)
    {
        SE_ERROR("Unable to allocate AntiEmulationBuffer\n");
        InitializationStatus = FrameParserError;
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

FrameParser_Video_c::~FrameParser_Video_c(void)
{
    // TODO(pht) call halt ? shall not rely on prior call to halt to release resources..

    delete ReverseQueuedPostDecodeSettingsRing;
    delete ReverseDecodeUnsatisfiedReferenceStack;
    delete ReverseDecodeSingleFrameStack;
    delete ReverseDecodeStack;

    st_relayfs_freeindex_fortype_se(ST_RELAY_TYPE_CODED_VIDEO_BUFFER, RelayfsIndex);

    delete [] AntiEmulationBuffer;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

FrameParserStatus_t   FrameParser_Video_c::Halt(void)
{
    if (!TestComponentState(ComponentRunning))
    {
        return FrameParserNoError;
    }

    if (PlaybackDirection == PlayForward)
    {
        ForPlayPurgeQueuedPostDecodeParameterSettings();
    }
    else if (ReverseDecodeStack != NULL)
    {
        RevPlayPurgeDecodeStacks();
    }

    return FrameParser_Base_c::Halt();
}

void   FrameParser_Video_c::ResetDeferredParameters()
{
    DeferredCodedFrameBuffer                    = NULL;
    DeferredParsedFrameParameters               = NULL;
    DeferredParsedVideoParameters               = NULL;
    DeferredCodedFrameBufferSecondField         = NULL;
    DeferredParsedFrameParametersSecondField    = NULL;
    DeferredParsedVideoParametersSecondField    = NULL;
}
// /////////////////////////////////////////////////////////////////////////
//
//      Method to connect to neighbor
//

FrameParserStatus_t   FrameParser_Video_c::Connect(Port_c *Port)
{
    FrameParserStatus_t     Status;
    //
    // First allow the base class to perform it's operations,
    // as we operate on the buffer pool obtained by it.
    //
    Status      = FrameParser_Base_c::Connect(Port);

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // Obtain stacks used in reverse play
    //
    ReverseDecodeUnsatisfiedReferenceStack      = new class StackGeneric_c(FrameBufferCount);

    if ((ReverseDecodeUnsatisfiedReferenceStack == NULL) ||
        (ReverseDecodeUnsatisfiedReferenceStack->InitializationStatus != StackNoError))
    {
        SE_ERROR("Failed to create stack to hold buffers with reference frames unsatisfied during reverse play (Open groups)\n");
        SetComponentState(ComponentInError);
        return FrameParserFailedToCreateReversePlayStacks;
    }

    ReverseDecodeSingleFrameStack = new class StackGeneric_c(FrameBufferCount);
    if ((ReverseDecodeSingleFrameStack == NULL) ||
        (ReverseDecodeSingleFrameStack->InitializationStatus != StackNoError))
    {
        SE_ERROR("Failed to create stack to hold buffers associated with a single decode frame\n");
        SetComponentState(ComponentInError);
        return FrameParserFailedToCreateReversePlayStacks;
    }

    ReverseDecodeStack  = new class StackGeneric_c(FrameBufferCount);

    if ((ReverseDecodeStack == NULL) ||
        (ReverseDecodeStack->InitializationStatus != StackNoError))
    {
        SE_ERROR("Failed to create stack to hold non-reference frame decodes during reverse play\n");
        SetComponentState(ComponentInError);
        return FrameParserFailedToCreateReversePlayStacks;
    }

    //
    // Attach the video specific parsed frame parameters to every element of the pool
    //
    Status = CodedFrameBufferPool->AttachMetaData(Player->MetaDataParsedVideoParametersType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to attach parsed video parameters to all coded frame buffers\n");
        SetComponentState(ComponentInError);
        return Status;
    }

    //
    // Create the ring used to hold buffer pointers when
    // we defer generation of PTS/display indices during
    // reverse play. (NOTE 2 entries per frame)
    //
    ReverseQueuedPostDecodeSettingsRing = new class RingGeneric_c(2 * FrameBufferCount);
    if ((ReverseQueuedPostDecodeSettingsRing == NULL) ||
        (ReverseQueuedPostDecodeSettingsRing->InitializationStatus != RingNoError))
    {
        SE_ERROR("Failed to create ring to hold buffers with display indices/Playback times not set during reverse play\n");
        SetComponentState(ComponentInError);
        return FrameParserFailedToCreateReversePlayStacks;
    }

//
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The input function perform video specific operations
//

FrameParserStatus_t   FrameParser_Video_c::Input(Buffer_t CodedBuffer)
{
    FrameParserStatus_t     Status;
    //
    // Are we allowed in here
    //
    AssertComponentState(ComponentRunning);
    //
    // Initialize context pointers
    //
    ParsedVideoParameters       = NULL;
    //
    // First perform base operations
    //
    Status      = FrameParser_Base_c::Input(CodedBuffer);

    if (Status != FrameParserNoError)
    {
        IncrementErrorStatistics(Status);
        return Status;
    }

    st_relayfs_write_se(ST_RELAY_TYPE_CODED_VIDEO_BUFFER + RelayfsIndex, ST_RELAY_SOURCE_SE,
                        (unsigned char *)BufferData, BufferLength, 0);
    //
    // Obtain video specific pointers to data associated with the buffer.
    //
    Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters));
    SE_ASSERT(ParsedVideoParameters != NULL);

    memset(ParsedVideoParameters, 0, sizeof(ParsedVideoParameters_t)); // TODO(pht) change
    ParsedVideoParameters->Content.PixelAspectRatio = 0; // rational
    ParsedVideoParameters->Content.FrameRate        = 0; // rational
    //
    // Now execute the processing chain for a buffer
    //
    Status = ProcessBuffer();

    if (Status != PlayerNoError)
    {
        IncrementErrorStatistics(Status);
    }

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The standard processing chain for a single input buffer
//      this is the video specific implementation, it probably
//      differs quite significantly from the audio implementation.
//

FrameParserStatus_t   FrameParser_Video_c::ProcessBuffer(void)
{
    FrameParserStatus_t     Status;

    //
    // Handle discontinuity in input stream
    //

    if (CodedFrameParameters->StreamDiscontinuity)
    {
#if 0
        SE_INFO(group_frameparser_video,  "Disco - %d %d %d - %d (%d %d %d)\n",
                CodedFrameParameters->StreamDiscontinuity, CodedFrameParameters->ContinuousReverseJump, CodedFrameParameters->FlushBeforeDiscontinuity,
                (PlaybackDirection == PlayForward),
                ReverseDecodeStack->NonEmpty(), ReverseDecodeUnsatisfiedReferenceStack->NonEmpty(), ReverseDecodeSingleFrameStack->NonEmpty());
#endif
        SE_VERBOSE2(group_frameparser_video, group_player, "Stream 0x%p - Inserting discontinuity at parser level\n", Stream);
        if ((PlaybackDirection == PlayBackward) && CodedFrameParameters->ContinuousReverseJump)
        {
            RevPlayProcessDecodeStacks();
        }
        else
        {
            if (PlaybackDirection == PlayForward)
            {
                ForPlayPurgeQueuedPostDecodeParameterSettings();
            }

            if ((PlaybackDirection == PlayBackward) || ReverseDecodeStack->NonEmpty() || ReverseDecodeUnsatisfiedReferenceStack->NonEmpty())
            {
                RevPlayPurgeDecodeStacks();
            }

            CollapseHolesInDisplayIndices               = true;
            LastRecordedPlaybackTimeDisplayFieldIndex   = 0;                    // Invalidate the time bases used in ongoing calculations
            LastRecordedNormalizedPlaybackTime          = INVALID_TIME;
            LastRecordedDecodeTimeFieldIndex            = 0;
            LastRecordedNormalizedDecodeTime            = INVALID_TIME;
            ResetReferenceFrameList();
        }

        AccumulatedPictureStructure = StructureEmpty;
        FirstDecodeAfterInputJump       = true;
        SurplusDataInjected             = CodedFrameParameters->FlushBeforeDiscontinuity;
        ContinuousReverseJump           = CodedFrameParameters->ContinuousReverseJump;
        Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnOutputPartialDecodeBuffers);
    }

    //
    // Do we have something to do with this buffer
    //

    if (BufferLength == 0)
    {
        //
        // Check for a marker frame, identified by no flags and no data
        //
        if (!CodedFrameParameters->StreamDiscontinuity)
        {
            Buffer->IncrementReferenceCount(IdentifierFrameParserMarkerFrame);
            mOutputPort->Insert((uintptr_t)Buffer);
        }

        return FrameParserNoError;
    }

    //
    // Copy the playback and decode times from coded frame parameters
    //

    if (CodedFrameParameters->PlaybackTimeValid)
    {
        CodedFramePlaybackTimeValid     = true;
        CodedFramePlaybackTime          = CodedFrameParameters->PlaybackTime;
    }

    if (CodedFrameParameters->DecodeTimeValid)
    {
        CodedFrameDecodeTimeValid       = true;
        CodedFrameDecodeTime            = CodedFrameParameters->DecodeTime;
    }

    //
    // Parse the headers
    //
    FrameToDecode                       = false;
    ParsedVideoParameters->FirstSlice   = true;         // Default for non-slice decoders
    Status      = ReadHeaders();

    if (Status != FrameParserNoError)
    {
        // Reset User Data Index
        UserDataIndex = 0;

        if (Status == FrameParserHeaderSyntaxError)
        {
            SE_ERROR("Syntax errors detected\n");
        }

        return Status;
    }

    //
    // Do we still have something to do
    //

    if (!FrameToDecode)
    {
        return FrameParserNoError;
    }

    //
    // If the decode width/height have not be overridden, use the default stream, width/height
    //

    if (ParsedVideoParameters->Content.DecodeWidth == 0)
    {
        ParsedVideoParameters->Content.DecodeWidth    = ParsedVideoParameters->Content.Width;
    }

    if (ParsedVideoParameters->Content.DecodeHeight == 0)
    {
        ParsedVideoParameters->Content.DecodeHeight   = ParsedVideoParameters->Content.Height;
    }

    //
    // Perform the separate chain depending on the direction of play
    //

    if (PlaybackDirection == PlayForward)
    {
        Status  = ForPlayProcessFrame();
    }
    else
    {
        Status  = RevPlayProcessFrame();
    }

    if (Status == FrameParserNoError)
    {
        // Restore the UserDataNumber for current frame after ReadRemainingStartCode (which uses ReadHeaders)
        int temp1 = ParsedFrameParameters->UserDataNumber;
        Status    = ReadRemainingStartCode();
        ParsedFrameParameters->UserDataNumber = temp1;
    }
    else
    {
        //Reset User Data Index
        UserDataIndex = 0;
    }

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Deal with decode of a single frame in forward play
//

FrameParserStatus_t   FrameParser_Video_c::ForPlayProcessFrame(void)
{
    FrameParserStatus_t     Status;

    //
    // Have we missed any new stream parameters
    //

    if (NewStreamParametersSeenButNotQueued)
    {
        ParsedFrameParameters->NewStreamParameters    = true;
    }
    else if (ParsedFrameParameters->NewStreamParameters)
    {
        NewStreamParametersSeenButNotQueued       = true;
    }

    //
    // Prepare the reference frame list for this frame
    //
    Status      = PrepareReferenceFrameList();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // Can we generate any queued index and pts values based on what we have seen
    //
    Status      = ForPlayProcessQueuedPostDecodeParameterSettings();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // Calculate the display index/PTS values
    //
    Status      = ForPlayGeneratePostDecodeParameterSettings();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // Queue the frame for decode
    //
    Status      = ForPlayQueueFrameForDecode();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    FirstDecodeAfterInputJump           = false;
    SurplusDataInjected                 = false;
    ContinuousReverseJump               = false;
    //
    // Update reference frame management
    //
    Status      = ForPlayUpdateReferenceFrameList();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // If this is the last thing before a stream termination code,
    // then clean out the frame labellings.
    //

    if (CodedFrameParameters->FollowedByStreamTerminate || GotSequenceEndCode)
    {
        GotSequenceEndCode = false;
        Status  = ForPlayPurgeQueuedPostDecodeParameterSettings();
    }

    // Mpeg2 specific check to decide if a picture can be send to manifest.
    // The check is based on temporal reference information. This is to solve bug 32422.
    ForPlayCheckForReferenceReadyForManifestation();
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Deal with a single frame in reverse play
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayProcessFrame(void)
{
    FrameParserStatus_t     Status;
    bool            NewFrame;
    bool            SecondFieldOfIndependentFrame;
    //
    // If we cannot handle smooth reverse, then discard all none independent
    // frames but to ensure decent timing we increment field counts.
    // NOTE we treat a non-I second field of an I first field as I for this test
    //
    SecondFieldOfIndependentFrame       = !FirstDecodeAfterInputJump &&
                                          LastReverseDecodeWasIndependentFirstField &&
                                          !ParsedFrameParameters->FirstParsedParametersForOutputFrame;
    LastReverseDecodeWasIndependentFirstField   = ParsedFrameParameters->IndependentFrame &&
                                                  ParsedFrameParameters->FirstParsedParametersForOutputFrame &&
                                                  (ParsedVideoParameters->PictureStructure != StructureFrame);
    Stream->GetOutputTimer()->SetSmoothReverseSupport(Configuration.SupportSmoothReversePlay);

    if (!Configuration.SupportSmoothReversePlay)
    {
        // We are in backward, not smooth, and just dropping the not I frames
        // We call MonitorGroupStructure() here to know the proportion of B and P frames in a GOP,
        // after this point if would be too late as Bs and Ps are dropped.
        Stream->GetOutputTimer()->MonitorGroupStructure(ParsedFrameParameters);
    }

    if (!Configuration.SupportSmoothReversePlay &&
        (!ParsedFrameParameters->IndependentFrame && !SecondFieldOfIndependentFrame))
    {
        NextDisplayFieldIndex -= (ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1]);
        return FrameParserNoError;
    }

    //
    // Check for a transition to a discarding state
    //

    if (!RevPlayDiscardingState)
    {
        Status  = RevPlayCheckResourceUtilization();

        if (Status != FrameParserNoError)
        {
            SE_ERROR("Failed to check resource utilization - Implementation error\n");
            Stream->MarkUnPlayable();
            return Status;
        }
    }

    //
    // If we are in discarding state then just count the
    // frame/field discarded, and release it
    // (we just don't claim to effect a release).
    //
    NewFrame    = ParsedFrameParameters->NewFrameParameters &&
                  ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if (RevPlayDiscardingState)
    {
        if (NewFrame)
        {
            RevPlayDiscardedFrameCount++;
        }

        return FrameParserNoError;
    }

    //
    // We are not discarding so we are going to process this frame.
    // Initialize pts/display indices
    //

    if (NewFrame)
    {
        RevPlayAccumulatedFrameCount++;
    }

    InitializePostDecodeParameterSettings();
    //
    // Queue the frame for decode
    //
    Status      = RevPlayQueueFrameForDecode();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    FirstDecodeAfterInputJump           = false;
    SurplusDataInjected                 = false;
    ContinuousReverseJump               = false;
    //
    // Update reference frame management
    //
    Status      = RevPlayAppendToReferenceFrameList();

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Addition to the base queue a buffer for decode increments
//      the field index.
//

FrameParserStatus_t   FrameParser_Video_c::ForPlayQueueFrameForDecode(void)
{
    LastSeenContentFrameRate        = ParsedVideoParameters->Content.FrameRate;
    LastSeenContentWidth        = ParsedVideoParameters->Content.Width;
    LastSeenContentHeight       = ParsedVideoParameters->Content.Height;

    //
    // Force the rewind of decode frame index, if this is not a first slice,
    // and only increment the decode field index if it is a first slice.
    //

    if (!ParsedVideoParameters->FirstSlice)
    {
        NextDecodeFrameIndex--;
    }
    else
    {
        if (PlaybackDirection == PlayForward)
        {
            NextDecodeFieldIndex  += (ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1]);
        }
        else
        {
            NextDecodeFieldIndex  -= (ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1]);
        }
    }

    //
    // Output queuing debug information
    //
#if 0

    if (ParsedVideoParameters->FirstSlice)
    {
        unsigned int     i;
        unsigned int     Length;
        unsigned char   *Data;
        unsigned int     Checksum;
        Buffer->ObtainDataReference(NULL, &Length, (void **)&Data);
        Length      -= ParsedFrameParameters->DataOffset;
        Data        += ParsedFrameParameters->DataOffset;
        Checksum    = 0;

        for (i = 0; i < Length; i++)
        {
            Checksum        += Data[i];
        }

        SE_INFO(group_frameparser_video,  "Q %d ( K = %d, R = %d, RL= {%d %d %d}, ST = %d, PS = %d, FPFP= %d, FS = %d) %09llx (%08x %d)\n",
                NextDecodeFrameIndex,
                ParsedFrameParameters->KeyFrame,
                ParsedFrameParameters->ReferenceFrame,
                ParsedFrameParameters->ReferenceFrameList[0].EntryCount, ParsedFrameParameters->ReferenceFrameList[1].EntryCount, ParsedFrameParameters->ReferenceFrameList[2].EntryCount,
                ParsedVideoParameters->SliceType,
                ParsedVideoParameters->PictureStructure,
                ParsedFrameParameters->FirstParsedParametersForOutputFrame,
                ParsedVideoParameters->FirstSlice,
                ParsedFrameParameters->NativePlaybackTime,
                Checksum, Length);
    }

#endif
    //
    // Allow the base class to actually queue the frame
    //
    NewStreamParametersSeenButNotQueued   = false;
    return FrameParser_Base_c::QueueFrameForDecode();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Specific reverse play implementation of queue for decode
//      the field index.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayQueueFrameForDecode(void)
{
    FrameParserStatus_t     Status;
    //
    // Prepare the reference frame list for this frame
    //
    Status  = PrepareReferenceFrameList();

    if (Status != FrameParserNoError)
    {
        ParsedFrameParameters->NumberOfReferenceFrameLists    = INVALID_INDEX;
    }

    //
    // Deal with reference frame
    //

    if (ParsedFrameParameters->ReferenceFrame)
    {
        //
        // Check that we can decode this
        //
        if (ParsedFrameParameters->NumberOfReferenceFrameLists == INVALID_INDEX)
        {
            // This happens for some perfectly valid H264 streams, we simply cannot reverse play these without slipping into IDR only synchronization mode
            SE_ERROR("Insufficient reference frames for a reference frame in reverse play mode\n");
            RevPlayDiscardingState  = true;
            RevPlaySmoothReverseFailureCount++;
            return FrameParserInsufficientReferenceFrames;
        }

        //
        // Go ahead and decode it (just as for forward play)
        //
        Status  = ForPlayQueueFrameForDecode();

        if (Status != FrameParserNoError)
        {
            SE_ERROR("Failed to queue reference frames for decode\n");
            return Status;
        }
    }

    //
    // Now whether it was a reference frame or not, we want to stack it
    //
    Buffer->IncrementReferenceCount(IdentifierReverseDecodeStack);
    ReverseDecodeStack->Push((unsigned int)Buffer);
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      This function is responsible for walking the stacks to handle
//      reverse decode.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayProcessDecodeStacks(void)
{
    FrameParserStatus_t               Status;
    Buffer_t                          PreservedBuffer;
    ParsedFrameParameters_t          *PreservedParsedFrameParameters;
    ParsedVideoParameters_t          *PreservedParsedVideoParameters;
    //
    // Preserve the pointers relating to the current buffer
    // while we trawl the reverse decode stacks.
    //
    PreservedBuffer                     = Buffer;
    PreservedParsedFrameParameters      = ParsedFrameParameters;
    PreservedParsedVideoParameters      = ParsedVideoParameters;

    //
    // Move last times unsatisfied reference reverse decode stack
    // (these were the frames in the open group that were missing
    // reference frames in the previous stach unwind). Noting that
    // any reference frames will be recorded. Also we allow next
    // sequence processing of the frames, generally null, but h264
    // does some work.
    //

    while (ReverseDecodeUnsatisfiedReferenceStack->NonEmpty())
    {
        ReverseDecodeUnsatisfiedReferenceStack->Pop((unsigned int *)&Buffer);
        Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
        SE_ASSERT(ParsedFrameParameters != NULL);
        Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters));
        SE_ASSERT(ParsedVideoParameters != NULL);
        RevPlayNextSequenceFrameProcess();

        if (ParsedFrameParameters->NumberOfReferenceFrameLists == INVALID_INDEX)
        {
            Status  = PrepareReferenceFrameList();

            if (Status != FrameParserNoError)
            {
                SE_ERROR("Insufficient reference frames for a deferred decode\n");
                ParsedFrameParameters->NumberOfReferenceFrameLists = INVALID_INDEX;
                // Lose this frame
                Buffer->DecrementReferenceCount(IdentifierReverseDecodeStack);
                continue;
            }
        }

        RevPlayAppendToReferenceFrameList();
        ReverseDecodeStack->Push((unsigned int)Buffer);
    }

    //
    // Now process the stack in reverse order
    //

    while (ReverseDecodeStack->NonEmpty())
    {
        //
        // First extract a single frame onto the single frame stack
        // We do this because individual frames are always decoded in
        // forward order.
        //
        do
        {
            ReverseDecodeStack->Pop((unsigned int *)&Buffer);
            Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
            SE_ASSERT(ParsedFrameParameters != NULL);
            ReverseDecodeSingleFrameStack->Push((unsigned int)Buffer);
        }
        while (ReverseDecodeStack->NonEmpty() &&
               !ParsedFrameParameters->FirstParsedParametersForOutputFrame &&
               (ParsedFrameParameters->NumberOfReferenceFrameLists != INVALID_INDEX));

        //
        // If we exited because of an invalid index, the we have an open group
        // so we exit processing, and transfer all remaining data onto the
        // unsatisfied reference stack for deferred decode.
        //

        if (ParsedFrameParameters->NumberOfReferenceFrameLists == INVALID_INDEX)
        {
            break;
        }

        //
        // Now we loop processing the single frame stack in forward order
        //

        while (ReverseDecodeSingleFrameStack->NonEmpty())
        {
            //
            // Retrieve the buffer and the pointers (they were
            // there before so we assume they are there now).
            //
            ReverseDecodeSingleFrameStack->Pop((unsigned int *)&Buffer);
            Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
            SE_ASSERT(ParsedFrameParameters != NULL);
            Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters));
            SE_ASSERT(ParsedVideoParameters != NULL);
            //
            // Calculate the display index/PTS values
            //
            Status  = RevPlayGeneratePostDecodeParameterSettings();

            if (Status != FrameParserNoError)
            {
                // We are in serious trouble, deadlock is on its way.
                SE_ERROR("Failed to generate post decode parameters - Fatal implementation error\n");
                return PlayerImplementationError;
            }

            //
            // Split into Reference or non-reference frame handling
            //

            if (ParsedFrameParameters->ReferenceFrame)
            {
                RevPlayRemoveReferenceFrameFromList();
            }
            else
            {
                //
                // Go ahead and decode it (just as for forward play), this cannot fail
                // but even if it does we must ignore it and carry on regardless
                //
                ForPlayQueueFrameForDecode();
            }

            //
            // We are at liberty to forget about this buffer now
            //
            Buffer->DecrementReferenceCount(IdentifierReverseDecodeStack);
        }
    }

    //
    // If we stopped processing the reverse decode stack due
    // to unsatisfied reference frames (an open group), then
    // transfer the remains of the stack to the unsatisfied
    // reference stack.
    //
    RevPlayClearResourceUtilization();

    while (ReverseDecodeSingleFrameStack->NonEmpty())
    {
        // Note single frame goes back on reverse decode to re-establish the reverse order of its component fields/slices.
        ReverseDecodeSingleFrameStack->Pop((unsigned int *)&Buffer);
        ReverseDecodeStack->Push((unsigned int)Buffer);
    }

    while (ReverseDecodeStack->NonEmpty())
    {
        ReverseDecodeStack->Pop((unsigned int *)&Buffer);
        ReverseDecodeUnsatisfiedReferenceStack->Push((unsigned int)Buffer);
        //
        // Increment the resource utilization counts
        //
        Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
        SE_ASSERT(ParsedFrameParameters != NULL);
        Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters));
        SE_ASSERT(ParsedVideoParameters != NULL);

        RevPlayCheckResourceUtilization();
    }

    //
    // Junk the list of reference frames (do not release the frames,
    // just junk the reference frame list).
    //
    RevPlayJunkReferenceFrameList();
    //
    // Restore the preserved pointers before completing
    //
    Buffer                      = PreservedBuffer;
    ParsedFrameParameters       = PreservedParsedFrameParameters;
    ParsedVideoParameters       = PreservedParsedVideoParameters;
    ReverseQueuedPostDecodeSettingsRing->Flush();
    ReverseDecodeSingleFrameStack->Flush();
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      This function is responsible for walking the stacks to discard
//      everything on them when we abandon reverse decode.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayPurgeDecodeStacks(void)
{
    FrameParserStatus_t               Status;
    Buffer_t                          PreservedBuffer;
    ParsedFrameParameters_t          *PreservedParsedFrameParameters;
    ParsedVideoParameters_t          *PreservedParsedVideoParameters;
    //
    // Preserve the pointers relating to the current buffer
    // while we trawl the reverse decode stacks.
    //
    PreservedBuffer                     = Buffer;
    PreservedParsedFrameParameters      = ParsedFrameParameters;
    PreservedParsedVideoParameters      = ParsedVideoParameters;

    //
    // Move last times unsatisfied reference reverse decode stack
    // (these were the frames in the open group that were missing
    // reference frames in the previous stach unwind).
    //

    while (ReverseDecodeUnsatisfiedReferenceStack->NonEmpty())
    {
        ReverseDecodeUnsatisfiedReferenceStack->Pop((unsigned int *)&Buffer);
        ReverseDecodeStack->Push((unsigned int)Buffer);
    }

    //
    // Now process the stack in reverse order
    //

    while (ReverseDecodeStack->NonEmpty())
    {
        //
        // Retrieve the buffer and the pointers (they were
        // there before so we assume they are there now).
        //
        ReverseDecodeStack->Pop((unsigned int *)&Buffer);
        Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
        SE_ASSERT(ParsedFrameParameters != NULL);
        Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters));
        SE_ASSERT(ParsedVideoParameters != NULL);

        //
        // Split into Reference or non-reference frame handling
        //

        if (ParsedFrameParameters->ReferenceFrame)
        {
            //
            // Calculate the display index/PTS values this guarantees that
            // refrence frames that are held in frame re-ordering will be released.
            //
            Status      = RevPlayGeneratePostDecodeParameterSettings();

            if (Status != FrameParserNoError)
            {
                // We are in serious trouble, deadlock is on its way.
                SE_ERROR("Failed to generate post decode parameters - Fatal implementation error\n");
            }
        }
        else
        {
            //
            // For non-reference frames we do nothing, these have not been
            // passed on, so we are the only component that knows about them.
            //
        }

        //
        // We are at liberty to forget about this buffer now
        //
        Buffer->DecrementReferenceCount(IdentifierReverseDecodeStack);
    }

    //
    // We have now processed all stacked entities, however it is possible that
    // some codec may still hold frames for post decode parameter settings
    // (H264 specifically), so we issue a purge on this operation.
    //
    Status  = RevPlayPurgeQueuedPostDecodeParameterSettings();

    if (Status != FrameParserNoError)
    {
        // We are in serious trouble, deadlock may be on its way.
        SE_ERROR("Failed to purge post decode parameters - Probable fatal implementation error\n");
    }

    //
    // Restore the preserved pointers before completing
    //
    Buffer                      = PreservedBuffer;
    ParsedFrameParameters       = PreservedParsedFrameParameters;
    ParsedVideoParameters       = PreservedParsedVideoParameters;
    //
    // Clear the resource utilization counts
    //
    RevPlayClearResourceUtilization();
//
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      This function is responsible for walking only the unsatisfied
//  reference stack, when we have a failure to do smooth reverse decode,
//  to discard everything on it.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayPurgeUnsatisfiedReferenceStack(void)
{
    FrameParserStatus_t               Status;
    Buffer_t                          PreservedBuffer;
    ParsedFrameParameters_t          *PreservedParsedFrameParameters;
    ParsedVideoParameters_t          *PreservedParsedVideoParameters;
    bool                  NewFrame;
    //
    // Preserve the pointers relating to the
    // current buffer while we trawl the stack.
    //
    PreservedBuffer                     = Buffer;
    PreservedParsedFrameParameters      = ParsedFrameParameters;
    PreservedParsedVideoParameters      = ParsedVideoParameters;

    //
    // Now process the stack in reverse order
    //

    while (ReverseDecodeUnsatisfiedReferenceStack->NonEmpty())
    {
        //
        // Retrieve the buffer and the pointers (they were
        // there before so we assume they are there now).
        //
        ReverseDecodeUnsatisfiedReferenceStack->Pop((unsigned int *)&Buffer);
        Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
        SE_ASSERT(ParsedFrameParameters != NULL);
        Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters));
        SE_ASSERT(ParsedVideoParameters != NULL);

        //
        // If this is a reference framne, it has been passed down the decode chain
        // it needs index/PTS values calculating, and freeing up in the codec
        //

        if (ParsedFrameParameters->ReferenceFrame)
        {
            Status      = RevPlayGeneratePostDecodeParameterSettings();

            if (Status != FrameParserNoError)
            {
                // We are in serious trouble, deadlock is on its way.
                SE_ERROR("Failed to generate post decode parameters - Fatal implementation error\n");
                return PlayerImplementationError;
            }

            // Note we do not remove the reference frame from the reference frame list
            // because in the unsatisfied reference stack it has not been placed int the list yet.
            // However we must release it as a reference frame from the codec
            Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE,
                                                      CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex);
        }

        //
        // Adjust the counts of discarded frames
        //
        NewFrame    = ParsedFrameParameters->NewFrameParameters &&
                      ParsedFrameParameters->FirstParsedParametersForOutputFrame;

        if (NewFrame)
        {
            RevPlayDiscardedFrameCount++;
        }

        //
        // We are at liberty to forget about this buffer now
        //
        Buffer->DecrementReferenceCount(IdentifierReverseDecodeStack);
    }

    //
    // Restore the preserved pointers before completing
    //
    Buffer                      = PreservedBuffer;
    ParsedFrameParameters       = PreservedParsedFrameParameters;
    ParsedVideoParameters       = PreservedParsedVideoParameters;
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Reverse play function to count resource utilization and check if
//  it allows continued smooth reverse play. NOTE this function is only
//  called if we are not already in a discarding state.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayClearResourceUtilization(void)
{
    NumberOfUtilizedFrameParameters     = 0;
    NumberOfUtilizedStreamParameters    = 0;
    NumberOfUtilizedDecodeBuffers       = 0;
    RevPlayDiscardingState              = false;
    RevPlayAccumulatedFrameCount        = 0;
    RevPlayDiscardedFrameCount          = 0;

    if (RevPlaySmoothReverseFailureCount > MAX_ALLOWED_SMOOTH_REVERSE_PLAY_FAILURES)
    {
        Configuration.SupportSmoothReversePlay  = false;
        Stream->GetOutputTimer()->SetSmoothReverseSupport(Configuration.SupportSmoothReversePlay);
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Reverse play function to count resource utilization and check if
//  it allows continued smooth reverse play. NOTE this function is only
//  called if we are not already in a discarding state.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayCheckResourceUtilization(void)
{
    bool                NewFrame;
    unsigned int        WorkingCount;
    unsigned int        DecodeBufferCount;

    //
    // Update our counts of utilized resources
    //

    if (ParsedFrameParameters->NewFrameParameters && ParsedVideoParameters->FirstSlice)
    {
        NumberOfUtilizedFrameParameters++;
    }

    if (ParsedFrameParameters->NewStreamParameters)
    {
        NumberOfUtilizedStreamParameters++;
    }

    NewFrame    = ParsedVideoParameters->FirstSlice &&
                  ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if (NewFrame && ParsedFrameParameters->ReferenceFrame)
    {
        NumberOfUtilizedDecodeBuffers++;
    }

    //
    // Here we check our counts of utilized resources to
    // ensure that we can function with this frame.
    // Note we use GE (>=) checks on resource needed before
    // this point, and GT (>) checks on resources needed after
    // this point.
    //
    Stream->GetDecodeBufferManager()->GetEstimatedBufferCount(&DecodeBufferCount);
    WorkingCount    = 2 * (PLAYER_MINIMUM_NUMBER_OF_WORKING_DECODE_BUFFERS + 1);    /* 2x for field decodes */
#define RevResourceCheckGE( U, A, T )   if( !RevPlayDiscardingState && ((U) >= (A)) ) {SE_INFO(group_frameparser_video,  "Unable to smooth reverse (%s)\n", T ); RevPlayDiscardingState = true; }
#define RevResourceCheckGT( U, A, T )   if( !RevPlayDiscardingState && ((U) > (A)) ) {SE_INFO(group_frameparser_video,  "Unable to smooth reverse (%s)\n", T ); RevPlayDiscardingState = true; }
    RevResourceCheckGT(NumberOfUtilizedFrameParameters, (Configuration.FrameParametersCount - WorkingCount), "FrameParameters");
    RevResourceCheckGE(NumberOfUtilizedStreamParameters, (Configuration.StreamParametersCount - WorkingCount), "StreamParameters");
    RevResourceCheckGT(NumberOfUtilizedDecodeBuffers, (DecodeBufferCount - 3), "DecodeBuffers");
    RevResourceCheckGT(NumberOfUtilizedDecodeBuffers, (Configuration.MaxReferenceFrameCount - 3), "ReferenceFrames");    // We only accumulate reference frames
#undef RevResourceCheck

    //
    // If we are transitioning to a discard state, then we need to purge
    // the ReverseDecodeUnsatisfiedReferenceStack, since we will never get
    // around to being able to satisfy their references.
    //
    // Also we offer to raise the failed to reverse decode smoothly event
    //

    if (RevPlayDiscardingState)
    {
        RevPlaySmoothReverseFailureCount++;
        RevPlayDiscardedFrameCount  = 0;
        RevPlayPurgeUnsatisfiedReferenceStack();
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to initialize the post decode parameter
//      settings for reverse play, these consist of the display frame index,
//      and presentation time, both of which may be deferred if not explicitly
//      specified during reverse play.
//

FrameParserStatus_t   FrameParser_Video_c::InitializePostDecodeParameterSettings(void)
{
    bool            ReasonableDecodeTime;
    //
    // Default setting
    //
    ParsedFrameParameters->DisplayFrameIndex            = INVALID_INDEX;
    ParsedFrameParameters->NativePlaybackTime           = INVALID_TIME;
    ParsedFrameParameters->NormalizedPlaybackTime       = INVALID_TIME;
    ParsedFrameParameters->NativeDecodeTime             = INVALID_TIME;
    ParsedFrameParameters->NormalizedDecodeTime         = INVALID_TIME;

    //
    // Record in the structure the decode and presentation times if specified
    // specifying the default decode time as the presentation time
    //

    if (CodedFramePlaybackTimeValid)
    {
        CodedFramePlaybackTimeValid                     = false;
        ParsedFrameParameters->NativePlaybackTime       = CodedFramePlaybackTime;
        TranslatePlaybackTimeNativeToNormalized(CodedFramePlaybackTime, &ParsedFrameParameters->NormalizedPlaybackTime, CodedFrameParameters->SourceTimeFormat);
    }

    if (CodedFrameDecodeTimeValid)
    {
        CodedFrameDecodeTimeValid                       = false;
        ReasonableDecodeTime                            = (NotValidTime(ParsedFrameParameters->NativePlaybackTime)) ||
                                                          (((CodedFramePlaybackTime - CodedFrameDecodeTime) & 0x1ffffffffull) < MAXIMUM_DECODE_TIME_OFFSET);

        if (CodedFrameDecodeTime > CodedFramePlaybackTime)
        {
            SE_INFO(group_frameparser_video,  "DTS(%016llx) > PTS(%016llx)!!!\n", CodedFrameDecodeTime, CodedFramePlaybackTime);
            CodedFrameDecodeTime = CodedFramePlaybackTime;
            ReasonableDecodeTime = true;
        }

        if (ReasonableDecodeTime)
        {
            ParsedFrameParameters->NativeDecodeTime     = CodedFrameDecodeTime;
            TranslatePlaybackTimeNativeToNormalized(CodedFrameDecodeTime, &ParsedFrameParameters->NormalizedDecodeTime, CodedFrameParameters->SourceTimeFormat);
        }
        else
        {
            SE_ERROR("(PTS - DTS) ridiculously large (%lld 90kHz ticks)\n  (%016llx %016llx %016llx)\n",
                     (CodedFramePlaybackTime - CodedFrameDecodeTime),
                     ParsedFrameParameters->NativePlaybackTime, CodedFramePlaybackTime, CodedFrameDecodeTime);
        }
    }

//
    return FrameParserNoError;
}



// /////////////////////////////////////////////////////////////////////////
//
//      The calculation code in different places to calculate
//      the display frame index and time stamps, the same for
//      B frames and deferred reference frames.
//

void   FrameParser_Video_c::CalculateFrameIndexAndPts(
    ParsedFrameParameters_t         *ParsedFrame,
    ParsedVideoParameters_t         *ParsedVideo)
{
    int                      ElapsedFields;
    long long        ElapsedTime;
    long long        MicroSecondsPerFrame;
    int                      FieldIndex;
    Rational_t               FieldRate;
    Rational_t               Temp;
    bool                     DerivePresentationTime;

    //
    // Calculate the field indices, if this is not the first decode of the
    // frame abort before proceeding on with display frame and pts/dts generation.
    //

    if (PlaybackDirection == PlayForward)
    {
        FieldIndex                               = NextDisplayFieldIndex;
        NextDisplayFieldIndex                   += (ParsedVideo->DisplayCount[0] + ParsedVideo->DisplayCount[1]);
    }
    else
    {
        NextDisplayFieldIndex                   -= (ParsedVideo->DisplayCount[0] + ParsedVideo->DisplayCount[1]);
        FieldIndex                               = NextDisplayFieldIndex;
    }

    if (!ParsedFrame->FirstParsedParametersForOutputFrame)
    {
        return;
    }

    //
    // Special case (invented for h264) if this is a field,
    // but it is not the first field to be displayed, then we
    // knock one off the field index. This was detected in
    // h264 because of the Picture Adapative Frame Field tests
    // which can often switch decode order ie Frame, top, bottom, bottom, top ...
    //

    if ((ParsedVideo->PictureStructure != StructureFrame) &&
        ((ParsedVideo->PictureStructure == StructureTopField) != ParsedVideo->TopFieldFirst))
    {
        FieldIndex--;
    }

    //
    // Obtain the presentation time
    //
    DerivePresentationTime      = NotValidTime(ParsedFrame->NormalizedPlaybackTime);

    if (DerivePresentationTime)
    {
        if (ValidTime(LastRecordedNormalizedPlaybackTime))
        {
            ElapsedFields                           = FieldIndex - LastRecordedPlaybackTimeDisplayFieldIndex;
            ElapsedTime                             = LongLongIntegerPart((1000000 / LastFieldRate) * ElapsedFields);
            ParsedFrame->NormalizedPlaybackTime     = LastRecordedNormalizedPlaybackTime + ElapsedTime;
            TranslatePlaybackTimeNormalizedToNative(ParsedFrame->NormalizedPlaybackTime, &ParsedFrame->NativePlaybackTime);

            // Process interpolated PTS, if User data are available
            if (ParsedFrameParameters->UserDataNumber > 0)
            {
                for (unsigned int i = 0; i < ParsedFrameParameters->UserDataNumber ; i++)
                {
                    AccumulatedUserData[i].UserDataGenericParameters.Pts = (uint32_t) ParsedFrame->NativePlaybackTime;
                    // Just one bit to be stored in pts_msb
                    AccumulatedUserData[i].UserDataGenericParameters.Pts_Msb = ParsedFrame->NativePlaybackTime >> 32;
                    AccumulatedUserData[i].UserDataGenericParameters.IsPtsInterpolated = 1;  // is_pts_interpolated flag is set to 1
                }
            }
        }
        else
        {
            ParsedFrame->NativePlaybackTime         = UNSPECIFIED_TIME;
            ParsedFrame->NormalizedPlaybackTime     = UNSPECIFIED_TIME;
        }
    }

    //
    // Can we deduce a framerate from incoming PTS values
    //

    if (!DerivePresentationTime && (ValidTime(LastRecordedNormalizedPlaybackTime)))
    {
        DeduceFrameRateElapsedFields    += (FieldIndex - LastRecordedPlaybackTimeDisplayFieldIndex) * (ParsedVideo->Content.Progressive ? 2 : 1);
        DeduceFrameRateElapsedTime  += ParsedFrame->NormalizedPlaybackTime - LastRecordedNormalizedPlaybackTime;

        if ((DeduceFrameRateElapsedTime >= (200 * 1000)) && (DeduceFrameRateElapsedFields >= 12))
        {
            MicroSecondsPerFrame    = (2 * DeduceFrameRateElapsedTime) / DeduceFrameRateElapsedFields;
            DeduceFrameRateFromPresentationTime(MicroSecondsPerFrame);
            DeduceFrameRateElapsedFields    = 0;
            DeduceFrameRateElapsedTime      = 0;
        }
    }

    //
    // We rebase recorded times for out pts derivation, if we
    // have a specified time or if the field/frame rate has changed (when we have recorded times).
    //
    FieldRate                               = ParsedVideo->Content.FrameRate;

    if (!ParsedVideo->Content.Progressive)
    {
        FieldRate                           = FieldRate * 2;
    }

    if (!DerivePresentationTime ||
        (ValidTime(LastRecordedNormalizedPlaybackTime) && (FieldRate != LastFieldRate)))
    {
        LastRecordedPlaybackTimeDisplayFieldIndex   = FieldIndex;
        LastRecordedNormalizedPlaybackTime          = ParsedFrame->NormalizedPlaybackTime;
    }

    LastFieldRate       = FieldRate;
    //
    // Calculate the indices
    //
    // Setting DisplayFrameIndex releases frame from reordering loop in decode2manifest so it must be done after timing.
    // Otherwise untimmed frame may be released from reeoreding loop.
    ParsedFrame->DisplayFrameIndex               = NextDisplayFrameIndex++;
    ParsedFrame->CollapseHolesInDisplayIndices   = CollapseHolesInDisplayIndices;
    CollapseHolesInDisplayIndices        = false;

    StoreTemporalReferenceForLastRecordedFrame(ParsedFrame);
}

// /////////////////////////////////////////////////////////////////////////
//
//      The calculation code to calculate the decode time stamp.
//
//      Note the deduced decode times are jittered by up to 1/4
//      of a frame period. We use field counts in the calculation.
//      With 3:2 pulldown this jitters the value (not using field
//      counts in this case would contract all decode times to take
//      place during 4/5ths of the time). Without 3:2 pulldown the
//      scheme is accurate to within the calculation error.
//      We are not concerned about the jitter, because the output
//      timer allows decodes to commence early by several frame times,
//      and if the caller requires more accuracy, he/she should specify
//      decode times rather than allowing us to deduce them.
//

void   FrameParser_Video_c::CalculateDts(
    ParsedFrameParameters_t         *ParsedFrame,
    ParsedVideoParameters_t         *ParsedVideo)
{
    unsigned int             ElapsedFields;
    unsigned long long   ElapsedTime;
    Rational_t               FieldRate;
    Rational_t               Temp;

    //
    // Do nothing if this is not the first decode of a frame
    //

    if (!ParsedFrame->FirstParsedParametersForOutputFrame)
    {
        return;
    }

    //
    // Obtain the Decode time
    //

    if (NotValidTime(ParsedFrame->NormalizedDecodeTime))
    {
        if (ValidTime(LastRecordedNormalizedDecodeTime))
        {
            ElapsedFields                       = NextDecodeFieldIndex - LastRecordedDecodeTimeFieldIndex;
            FieldRate                           = ParsedVideo->Content.FrameRate;

            if (!ParsedVideo->Content.Progressive)
            {
                FieldRate                       = FieldRate * 2;
            }

            Temp                                = (1000000 / FieldRate) * ElapsedFields;
            ElapsedTime                         = Temp.LongLongIntegerPart();
            ParsedFrame->NormalizedDecodeTime   = LastRecordedNormalizedDecodeTime + ElapsedTime;
            TranslatePlaybackTimeNormalizedToNative(ParsedFrame->NormalizedDecodeTime, &ParsedFrame->NativeDecodeTime);
        }
        else
        {
            ParsedFrame->NativeDecodeTime       = UNSPECIFIED_TIME;
            ParsedFrame->NormalizedDecodeTime   = UNSPECIFIED_TIME;
        }
    }
    else
    {
        LastRecordedDecodeTimeFieldIndex        = NextDecodeFieldIndex;
        LastRecordedNormalizedDecodeTime        = ParsedFrame->NormalizedDecodeTime;
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      The reset reference frame list function
//

FrameParserStatus_t   FrameParser_Video_c::ResetReferenceFrameList(void)
{
    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL);
    ReferenceFrameList.EntryCount       = 0;
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to purge deferred post decode parameter
//      settings, these consist of the display frame index, and presentation
//      time. Here we treat them as if we had received a reference frame.
//

FrameParserStatus_t   FrameParser_Video_c::ForPlayPurgeQueuedPostDecodeParameterSettings(void)
{
    //
    // Do we have something to process.
    //
    if (DeferredCodedFrameBuffer != NULL)
    {
        CalculateFrameIndexAndPts(DeferredParsedFrameParameters, DeferredParsedVideoParameters);
        DeferredCodedFrameBuffer->DecrementReferenceCount();

        if (DeferredCodedFrameBufferSecondField != NULL)
        {
            CalculateFrameIndexAndPts(DeferredParsedFrameParametersSecondField, DeferredParsedVideoParametersSecondField);
            DeferredCodedFrameBufferSecondField->DecrementReferenceCount();
        }

        ResetDeferredParameters();
    }

//
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to process deferred post decode parameter
//      settings, these consist of the display frame index, and presentation
//      time, they can be processed if we have a new reference frame (in mpeg2)
//

FrameParserStatus_t   FrameParser_Video_c::ForPlayProcessQueuedPostDecodeParameterSettings(void)
{
    //
    // Do we have something to process, and can we process it
    //
    if (FirstDecodeOfFrame && (DeferredCodedFrameBuffer != NULL) && ParsedFrameParameters->ReferenceFrame)
    {
        CalculateFrameIndexAndPts(DeferredParsedFrameParameters, DeferredParsedVideoParameters);
        DeferredCodedFrameBuffer->DecrementReferenceCount();

        if (DeferredCodedFrameBufferSecondField != NULL)
        {
            CalculateFrameIndexAndPts(DeferredParsedFrameParametersSecondField, DeferredParsedVideoParametersSecondField);
            DeferredCodedFrameBufferSecondField->DecrementReferenceCount();
        }

        ResetDeferredParameters();
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to generate the post decode parameter
//      settings, these consist of the display frame index, and presentation
//      time, both of which may be deferred if the information is unavailable.
//
//      For mpeg2, even though we could use temporal reference, I am going
//      to use a no holes system.
//

FrameParserStatus_t   FrameParser_Video_c::ForPlayGeneratePostDecodeParameterSettings(void)
{
    //
    // Default setting
    //
    InitializePostDecodeParameterSettings();

    //
    // If this is the first decode of a field then we need display frame indices and presentation timnes
    // If this is not a reference frame do the calculation now, otherwise defer it.
    //

    // If in I only decode mode, we need to have display frame index computed. It was reset in InitializePostDecodeParameterSettings().
    int TrickModePolicy = Player->PolicyValue(Playback, Stream, PolicyTrickModeDomain);
    if ((ParsedFrameParameters->IndependentFrame)
        && (
            TrickModePolicy == PolicyValueTrickModeDecodeKeyFrames
            || Player->PolicyValue(Playback, Stream, PolicyStreamOnlyKeyFrames) == PolicyValueApply
            || Player->PolicyValue(Playback, Stream, PolicyStreamDiscardFrames) == PolicyValueKeyFramesOnly
        )
       )
    {
        CalculateFrameIndexAndPts(ParsedFrameParameters, ParsedVideoParameters);
        ResetDeferredParameters();
        return FrameParserNoError;
    }

    if (!ParsedFrameParameters->ReferenceFrame)
    {
        CalculateFrameIndexAndPts(ParsedFrameParameters, ParsedVideoParameters);
    }
    else
    {
        //
        // Here we defer, in order to cope with trick modes where a frame may
        // be discarded before decode, we increment the reference count on the
        // coded frame buffer, to guarantee that when we come to update the PTS
        // etc on it, that it will be available to us.
        //
        Buffer->IncrementReferenceCount();

        if ((DeferredCodedFrameBuffer != NULL) &&
            ((DeferredParsedVideoParameters->PictureStructure ^ ParsedVideoParameters->PictureStructure) != StructureFrame))
        {
            SE_ERROR("Deferred Field/Frame inconsistency - broken stream/implementation error\n");
            ForPlayPurgeQueuedPostDecodeParameterSettings();
        }

        if (DeferredCodedFrameBufferSecondField != NULL)
        {
            SE_ERROR("Repeated deferral of second field - broken stream/implementation error\n");
            ForPlayPurgeQueuedPostDecodeParameterSettings();
        }

        if (DeferredCodedFrameBuffer != NULL)
        {
            DeferredCodedFrameBufferSecondField     = Buffer;
            DeferredParsedFrameParametersSecondField    = ParsedFrameParameters;
            DeferredParsedVideoParametersSecondField    = ParsedVideoParameters;
        }
        else
        {
            DeferredCodedFrameBuffer                    = Buffer;
            DeferredParsedFrameParameters               = ParsedFrameParameters;
            DeferredParsedVideoParameters               = ParsedVideoParameters;
        }

        CalculateDts(ParsedFrameParameters, ParsedVideoParameters);
    }

//
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to generate the post decode parameter
//      settings for reverse play, these consist of the display frame index,
//      and presentation time, both of which may be deferred if the information
//      is unavailable.
//
//      For mpeg2 reverse play, this function will use a simple numbering system,
//      Imaging a sequence  I B B P B B this should be numbered (in reverse) as
//                          3 5 4 0 2 1
//      These will be presented to this function in reverse order ( B B P B B I )
//      so for non ref frames we ring them, and for ref frames we use the next number
//      and then process what is on the ring.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayGeneratePostDecodeParameterSettings(void)
{
    //
    // If this is not a reference frame then place it on the ring for calculation later
    //
    if (!ParsedFrameParameters->ReferenceFrame)
    {
        ReverseQueuedPostDecodeSettingsRing->Insert((uintptr_t)ParsedFrameParameters);
        ReverseQueuedPostDecodeSettingsRing->Insert((uintptr_t)ParsedVideoParameters);
    }
    else
        //
        // If this is a reference frame then process it, if is the first
        // field (or a whole frame) then process the frames on the ring.
        //
    {
        CalculateFrameIndexAndPts(ParsedFrameParameters, ParsedVideoParameters);

        if (ParsedFrameParameters->FirstParsedParametersForOutputFrame)
        {
            while (ReverseQueuedPostDecodeSettingsRing->NonEmpty())
            {
                ReverseQueuedPostDecodeSettingsRing->Extract((uintptr_t *)&DeferredParsedFrameParameters);
                ReverseQueuedPostDecodeSettingsRing->Extract((uintptr_t *)&DeferredParsedVideoParameters);
                CalculateFrameIndexAndPts(DeferredParsedFrameParameters, DeferredParsedVideoParameters);
            }
        }
    }

//
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to manage a reference frame list in forward play
//      we only record a reference frame as such on the last field, in order to
//      ensure the correct management of reference frames in the codec, we immediately
//      inform the codec of a release on the first field of a field picture.
//
/*
FrameParserStatus_t   FrameParser_Video_c::ForPlayUpdateReferenceFrameList( void )
{
unsigned int    i;
bool            LastField;

//

    if( ParsedFrameParameters->ReferenceFrame )
    {
    LastField       = (ParsedVideoParameters->PictureStructure == StructureFrame) ||
              !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if( LastField )
    {
        if( ReferenceFrameList.EntryCount >= MAX_REFERENCE_FRAMES_FORWARD_PLAY )
        {
        Stream->ParseToDecodeEdge->CallInSequence( SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0] );

        ReferenceFrameList.EntryCount--;
        for( i=0; i<ReferenceFrameList.EntryCount; i++ )
            ReferenceFrameList.EntryIndicies[i] = ReferenceFrameList.EntryIndicies[i+1];
        }

        ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount++] = ParsedFrameParameters->DecodeFrameIndex;
    }
    else
    {
        Stream->ParseToDecodeEdge->CallInSequence( SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex );
    }
    }

    return FrameParserNoError;
}
*/

// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to add a frame to the reference
//      frame list in reverse play.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayAppendToReferenceFrameList(void)
{
    bool LastField   = (ParsedVideoParameters->PictureStructure == StructureFrame) ||
                       !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if (ParsedFrameParameters->ReferenceFrame && LastField)
    {
        if (ReferenceFrameList.EntryCount >= MAX_ENTRIES_IN_REFERENCE_FRAME_LIST)
        {
            SE_ERROR("List full - Implementation error\n");
            return PlayerImplementationError;
        }

        ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount++] = ParsedFrameParameters->DecodeFrameIndex;
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to remove a frame from the reference
//      frame list in reverse play.
//
//      Note, we only inserted the reference frame in the list on the last
//      field but we need to inform the codec we are finished with it on both
//      fields (for field pictures).
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayRemoveReferenceFrameFromList(void)
{
    bool LastField   = (ParsedVideoParameters->PictureStructure == StructureFrame) ||
                       !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if (ReferenceFrameList.EntryCount != 0)
    {
        Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex);

        if (LastField)
        {
            ReferenceFrameList.EntryCount--;
        }
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to junk the reference frame list
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayJunkReferenceFrameList(void)
{
    ReferenceFrameList.EntryCount       = 0;
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Private function to convert a us per frame
//
//  We define matching as within 8us, since we can have errors of
//  up to 5.5555555 us when trying to express a frame time as a
//  PTS (90kHz clock)
//

#define ValueMatchesFrameTime(V,T)   inrange( V, T - 8, T + 8 )

void   FrameParser_Video_c::DeduceFrameRateFromPresentationTime(long long int     MicroSecondsPerFrame)
{
    long long int    CurrentMicroSecondsPerFrame;

    //
    // Are we OK with what we have got.
    // If not, then we mark the rate as not standard, to force a recalculation,
    // but not immediately, since this may be just a glitch.
    //

    if (ValidFrameRate(PTSDeducedFrameRate) && StandardPTSDeducedFrameRate)
    {
        CurrentMicroSecondsPerFrame = RoundedLongLongIntegerPart(1000000 / PTSDeducedFrameRate);

        if (!ValueMatchesFrameTime(MicroSecondsPerFrame, CurrentMicroSecondsPerFrame))
        {
            StandardPTSDeducedFrameRate   = false;
        }

        return;
    }

    //
    // Check against the standard values
    //
    StandardPTSDeducedFrameRate = true;

    if (ValueMatchesFrameTime(MicroSecondsPerFrame, 16667))              // 60fps = 16666.67 us
    {
        PTSDeducedFrameRate = Rational_t(60, 1);
    }
    else if (ValueMatchesFrameTime(MicroSecondsPerFrame, 16683))             // 59fps = 16683.33 us
    {
        PTSDeducedFrameRate = Rational_t(60000, 1001);
    }
    else if (ValueMatchesFrameTime(MicroSecondsPerFrame, 20000))             // 50fps = 20000.00 us
    {
        PTSDeducedFrameRate = Rational_t(50, 1);
    }
    else if (ValueMatchesFrameTime(MicroSecondsPerFrame, 33333))             // 30fps = 33333.33 us
    {
        PTSDeducedFrameRate = Rational_t(30, 1);
    }
    else if (ValueMatchesFrameTime(MicroSecondsPerFrame, 33367))             // 29fps = 33366.67 us
    {
        PTSDeducedFrameRate = Rational_t(30000, 1001);
    }
    else if (ValueMatchesFrameTime(MicroSecondsPerFrame, 40000))             // 25fps = 40000.00 us
    {
        PTSDeducedFrameRate = Rational_t(25, 1);
    }
    else if (ValueMatchesFrameTime(MicroSecondsPerFrame, 41667))             // 24fps = 41666.67 us
    {
        PTSDeducedFrameRate = Rational_t(24, 1);
    }
    else if (ValueMatchesFrameTime(MicroSecondsPerFrame, 41708))             // 23fps = 41708.33 us
    {
        PTSDeducedFrameRate = Rational_t(24000, 1001);
    }
    else if (ValueMatchesFrameTime(MicroSecondsPerFrame, 66733))            // 14.9fps = 66733.33 us
    {
        PTSDeducedFrameRate     = Rational_t(30000 / 2, 1001);
    }
    else if (ValueMatchesFrameTime(MicroSecondsPerFrame, 133467))           // 7fps = 133466.67 us
    {
        PTSDeducedFrameRate     = Rational_t(30000 / 4, 1001);
    }
    else if (PTSDeducedFrameRate != Rational_t(1000000, MicroSecondsPerFrame))    // If it has changed since the last time
    {
        StandardPTSDeducedFrameRate = false;
        PTSDeducedFrameRate     = Rational_t(1000000, MicroSecondsPerFrame);
    }

    // If it was non standard, but has not changed since
    // last time we let it become the new standard.

    if (StandardPTSDeducedFrameRate)
    {
        if (PTSDeducedFrameRate != LastStandardPTSDeducedFrameRate)
        {
            SE_INFO(group_frameparser_video,  "DeduceFrameRateFromPresentationTime - Framerate = %d.%06d\n", PTSDeducedFrameRate.IntegerPart(), PTSDeducedFrameRate.RemainderDecimal());
        }

        LastStandardPTSDeducedFrameRate = PTSDeducedFrameRate;
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Functions to load the anti emulation buffer
//

void   FrameParser_Video_c::LoadAntiEmulationBuffer(unsigned char  *Pointer)
{
    AntiEmulationSource         = Pointer;
    AntiEmulationContent        = 0;
    Bits.SetPointer(AntiEmulationBuffer);
#if 0
    unsigned int        i, j;
    SE_INFO(group_frameparser_video,  "LoadAntiEmulationBuffer\n");

    for (i = 0; i < 256; i += 16)
    {
        SE_INFO(group_frameparser_video,  "        ");

        for (j = 0; j < 16; j++)
        {
            SE_INFO(group_frameparser_video,  "%02x ", Pointer[i + j]);
        }

        SE_INFO(group_frameparser_video,  "\n");
    }

    memset(AntiEmulationBuffer, 0xaa, mAntiEmulationBufferMaxSize);
#endif
}


// --------


void FrameParser_Video_c::CheckAntiEmulationBuffer(unsigned int    Size)
{
    unsigned int             i, j;
    FrameParserStatus_t      Status;
    unsigned int             UntouchedBytesInBuffer;
    unsigned int             BitsInByte;
    unsigned char           *NewBase;
    unsigned char           *Pointer;

    //
    // Check that the request is supportable
    //

    if (Size > (mAntiEmulationBufferMaxSize - 16))
    {
        SE_INFO(group_frameparser_video,
                "Buffer overflow, requesting %u (avail: %d) - Implementation error\n",
                Size, (mAntiEmulationBufferMaxSize - 16));
        return;
    }

    //
    // Do we already have the required data, or do we need some more
    // if we need some more, preserve the unused ones that we have.
    //
    BitsInByte                  = 8;
    NewBase                     = AntiEmulationBuffer;
    UntouchedBytesInBuffer      = 0;

    if (AntiEmulationContent != 0)
    {
        //
        // First let us check the status of the buffer
        //
        Status  = TestAntiEmulationBuffer();

        if (Status != FrameParserNoError)
        {
            SE_ERROR("Already gone bad (%d)\n", Size);
            return;
        }

        //
        // Can we satisfy the request without trying
        //
        Bits.GetPosition(&Pointer, &BitsInByte);
        UntouchedBytesInBuffer  = AntiEmulationContent - (Pointer - AntiEmulationBuffer) - (BitsInByte != 8);

        if (UntouchedBytesInBuffer >= Size)
        {
            return;
        }

        //
        // Know how many bytes to save, do we need to shuffle down, or can we append
        //

        if (((Pointer - AntiEmulationBuffer) + Size + 16) > mAntiEmulationBufferMaxSize)
        {
            AntiEmulationContent        = UntouchedBytesInBuffer + (BitsInByte != 8);

            for (i = 0; i < AntiEmulationContent; i++)
            {
                AntiEmulationBuffer[i]  = Pointer[i];
            }

            NewBase     = AntiEmulationBuffer;
        }
        else
        {
            NewBase     = Pointer;
        }
    }

//

    for (i = UntouchedBytesInBuffer, j = 0;
         i < (Size + 3);
         i++, j++)
        if ((AntiEmulationSource[j] == 0) &&
            (AntiEmulationSource[j + 1] == 0) &&
            (AntiEmulationSource[j + 2] == 3))
        {
            memcpy(AntiEmulationBuffer + AntiEmulationContent, AntiEmulationSource, j + 2);
            AntiEmulationContent        += j + 2;
            AntiEmulationSource         += j + 3;
            j                            = -1;
        }

    if (j != 0)
    {
        memcpy(AntiEmulationBuffer + AntiEmulationContent, AntiEmulationSource, j);
        AntiEmulationContent    += j;
        AntiEmulationSource     += j;
    }

//
    Bits.SetPointer(NewBase);
    Bits.FlushUnseen(8 - BitsInByte);
#if 0
    SE_INFO(group_frameparser_video,  "CheckAntiEmulationBuffer(%d) - Content is %d, Untouched was %d\n", Size, AntiEmulationContent, UntouchedBytesInBuffer);

    for (i = 0; i < AntiEmulationContent; i += 16)
    {
        SE_INFO(group_frameparser_video,  "        ");

        for (j = 0; j < 16; j++)
        {
            SE_INFO(group_frameparser_video,  "%02x ", AntiEmulationBuffer[i + j]);
        }

        SE_INFO(group_frameparser_video,  "\n");
    }

#endif
}


// --------


FrameParserStatus_t   FrameParser_Video_c::TestAntiEmulationBuffer(void)
{
    unsigned int     BitsInByte;
    unsigned char   *Pointer;
    unsigned int     BytesUsed;
//
    Bits.GetPosition(&Pointer, &BitsInByte);
    BytesUsed   = (Pointer - AntiEmulationBuffer) + (BitsInByte != 8);

    if ((BytesUsed != 0) && (BytesUsed > AntiEmulationContent))
    {
        SE_ERROR("Anti emulation buffering failure (%d [%d,%d]) - Implementation error\n",
                 AntiEmulationContent - BytesUsed, BytesUsed, AntiEmulationContent);
        return FrameParserError;
    }

#if 0
    SE_INFO(group_frameparser_video,  "%d %d\n",  BytesUsed, BitsInByte);
#endif
//
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Functions to load the anti emulation buffer
//

static Rational_t   SelectFrameRate(Rational_t  F0,
                                    Rational_t  F1,
                                    Rational_t  F2,
                                    Rational_t  F3)
{
    return  ValidFrameRate(F0) ? F0 : (ValidFrameRate(F1) ? F1 : (ValidFrameRate(F2) ? F2 : F3));
}


Rational_t   FrameParser_Video_c::ResolveFrameRate(void)
{
    Rational_t  FrameRate;
    int Precedence = Player->PolicyValue(Playback, Stream, PolicyFrameRateCalculationPrecedence);
    unsigned int ContainerFrameRateValue = (unsigned int)Player->PolicyValue(Playback, Stream, PolicyContainerFrameRate);

    if (ContainerFrameRateValue != INVALID_FRAMERATE)
    {
        ContainerFrameRate = Rational_t(ContainerFrameRateValue, 1);
    }

    switch (Precedence)
    {
    default:
    case PolicyValuePrecedenceStreamPtsContainerDefault:
        FrameRate   = SelectFrameRate(StreamEncodedFrameRate, PTSDeducedFrameRate, ContainerFrameRate, DefaultFrameRate);
        break;

    case PolicyValuePrecedenceStreamContainerPtsDefault:
        FrameRate   = SelectFrameRate(StreamEncodedFrameRate, ContainerFrameRate, PTSDeducedFrameRate, DefaultFrameRate);
        break;

    case PolicyValuePrecedencePtsStreamContainerDefault:
        FrameRate   = SelectFrameRate(PTSDeducedFrameRate, StreamEncodedFrameRate, ContainerFrameRate, DefaultFrameRate);
        break;

    case PolicyValuePrecedencePtsContainerStreamDefault:
        FrameRate   = SelectFrameRate(PTSDeducedFrameRate, ContainerFrameRate, StreamEncodedFrameRate, DefaultFrameRate);
        break;

    case PolicyValuePrecedenceContainerPtsStreamDefault:
        FrameRate   = SelectFrameRate(ContainerFrameRate, PTSDeducedFrameRate, StreamEncodedFrameRate, DefaultFrameRate);
        break;

    case PolicyValuePrecedenceContainerStreamPtsDefault:
        FrameRate   = SelectFrameRate(ContainerFrameRate, StreamEncodedFrameRate, PTSDeducedFrameRate, DefaultFrameRate);
        break;
    }

//

    if (LastResolvedFrameRate != FrameRate)
    {
        SE_INFO(group_frameparser_video,  "ResolveFrameRate - Framerate = %d.%03d\n", FrameRate.IntegerPart(), FrameRate.RemainderDecimal(3));
    }

//
    LastResolvedFrameRate   = FrameRate;
    Stream->Statistics().VideoFrameRateIntegerPart = FrameRate.IntegerPart();
    Stream->Statistics().VideoFrameRateRemainderDecimal = FrameRate.RemainderDecimal();
    return FrameRate;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to reset reverse failure counter: if backward turns to I only
//  for some reason (errors on some portion of stream...), then on next
//  backward speed request, we will try again smooth reverse.
//

FrameParserStatus_t   FrameParser_Video_c::ResetReverseFailureCounter(void)
{
    RevPlaySmoothReverseFailureCount        = 0;
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to check for resolution constraints
//

FrameParserStatus_t FrameParser_Video_c::CheckForResolutionConstraints(unsigned int Width, unsigned int Height)
{
    FrameParserStatus_t Status;
    Status = CheckForResolutionBasedOnMemProfile(Width, Height);
    if (Status != FrameParserNoError)
    {
        return Status;
    }

    Status = CheckForResolutionBasedOnCodec(Width, Height);
    if (Status != FrameParserNoError)
    {
        return Status;
    }
    return Status;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to check for resolution constraints
//      depending on memory profile defined in player
//

FrameParserStatus_t FrameParser_Video_c::CheckForResolutionBasedOnMemProfile(unsigned int Width, unsigned int Height)
{
    unsigned int MaxSupportedWidth, MaxSupportedHeight, MaxSupportedMB;
    int MemProfilePolicy;

    //
    // Check the parsed resolution against some limits
    //    - Width and height must not exceed MAX_SUPPORTED_WIDTH and MAX_SUPPORTED_HEIGHT
    //        depending on memory profile (avoid further errors in collator and fp)
    //    - Total number of macroblocks must not exceed MAX_SUPPORTED_TOTAL_MBS
    //    - Width and height are not greater than 10 times the value of each other
    //
    MemProfilePolicy = Player->PolicyValue(Playback, PlayerAllStreams, PolicyVideoPlayStreamMemoryProfile);
    if (MemProfilePolicy == PolicyValueVideoPlayStreamMemoryProfile4K2K)
    {
        MaxSupportedWidth  = PLAYER_4K2K_FRAME_WIDTH;
        MaxSupportedHeight = PLAYER_4K2K_FRAME_HEIGHT;
    }
    else if (MemProfilePolicy == PolicyValueVideoPlayStreamMemoryProfileUHD)
    {
        MaxSupportedWidth  = PLAYER_UHD_FRAME_WIDTH;
        MaxSupportedHeight = PLAYER_UHD_FRAME_HEIGHT;
    }
    else if (MemProfilePolicy == PolicyValueVideoPlayStreamMemoryProfileSD)
    {
        MaxSupportedWidth  = PLAYER_SD_FRAME_WIDTH;
        MaxSupportedHeight = PLAYER_SD_FRAME_HEIGHT;
    }
    else if (MemProfilePolicy == PolicyValueVideoPlayStreamMemoryProfileHD720p)
    {
        MaxSupportedWidth  = PLAYER_HD720P_FRAME_WIDTH;
        MaxSupportedHeight = PLAYER_HD720P_FRAME_HEIGHT;
    }
    else
    {
        MaxSupportedWidth  = PLAYER_HD_FRAME_WIDTH;
        MaxSupportedHeight = PLAYER_HD_FRAME_HEIGHT;
    }

    MaxSupportedMB = (MaxSupportedWidth / 16) * (MaxSupportedHeight / 16);

    // Check that stream does not exceed max 4K width and height + theoretical nb of MB
    if (((Width > PLAYER_4K2K_FRAME_WIDTH) || (Height > PLAYER_4K2K_FRAME_HEIGHT)) ||
        ((Width / 16) * (Height / 16) > MaxSupportedMB) ||
        ((Width > Height * 10) || (Height > Width * 10)))
    {
        SE_ERROR("Resolution constraints check fails: w: %d (max: %d) h: %d (max: %d) mb: %d (max: %d=%dx%d)\n",
                 Width, PLAYER_4K2K_FRAME_WIDTH, Height, PLAYER_4K2K_FRAME_HEIGHT,
                 (Width / 16) * (Height / 16), MaxSupportedMB,
                 MaxSupportedWidth / 16, MaxSupportedHeight / 16);
        return FrameParserError;
    }
    else
    {
        return FrameParserNoError;
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to check for basic H/W resolution constraints
//

FrameParserStatus_t FrameParser_Video_c::CheckForResolutionBasedOnCodec(unsigned int Width, unsigned int Height)
{
    //
    // Check that the width/height are not less than 64 pixels due to
    // hardware constraint
    //
    if ((Width < mMinFrameWidth) || (Height < mMinFrameHeight)
        || (Width > mMaxFrameWidth) || (Height > mMaxFrameHeight))
    {
        SE_ERROR("Resolution constraints check failed: w: %d (min: %d, max: %d) h: %d (min: %d, max: %d)\n",
                 Width, mMinFrameWidth, mMaxFrameWidth, Height, mMinFrameHeight, mMaxFrameHeight);
        return FrameParserError;
    }
    else
    {
        return FrameParserNoError;
    }
}
