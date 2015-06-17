/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : frame_parser_video.cpp
Author :           Nick

Implementation of the base video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
29-Nov-06   Created                                         Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_video.h"
#include "stack_generic.h"
#include "ring_generic.h"
#include "st_relay.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define         DEFAULT_DECODE_TIME_OFFSET      3600            // thats 40ms in 90Khz ticks
#define         MAXIMUM_DECODE_TIME_OFFSET      (4 * 90000)     // thats 4s in 90Khz ticks

#define		MAX_ALLOWED_SMOOTH_REVERSE_PLAY_FAILURES   2	// After this we assume reverse play just isn't practical

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//
FrameParser_Video_c::FrameParser_Video_c( void )
{
    if( InitializationStatus != FrameParserNoError )
	return;

//

    ReverseQueuedPostDecodeSettingsRing         = NULL;
    ReverseDecodeUnsatisfiedReferenceStack      = NULL;
    ReverseDecodeSingleFrameStack		= NULL;
    ReverseDecodeStack                          = NULL;
    DeferredCodedFrameBuffer                    = NULL;
    DeferredParsedFrameParameters               = NULL;
    DeferredParsedVideoParameters               = NULL;
    DeferredCodedFrameBufferSecondField         = NULL;
    DeferredParsedFrameParametersSecondField    = NULL;
    DeferredParsedVideoParametersSecondField    = NULL;

//

    Configuration.SupportSmoothReversePlay      = true;		// By default we support smooth reverse
    Configuration.InitializeStartCodeList	= true;		// By default we get the start code lists

    RevPlaySmoothReverseFailureCount		= 0;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

FrameParser_Video_c::~FrameParser_Video_c(      void )
{
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

FrameParserStatus_t   FrameParser_Video_c::Halt(        void )
{
    if( !TestComponentState( ComponentRunning ) )
	return FrameParserNoError;

    if( PlaybackDirection == PlayForward )
	ForPlayPurgeQueuedPostDecodeParameterSettings();
    else if( ReverseDecodeStack != NULL )
	RevPlayPurgeDecodeStacks();

    return FrameParser_Base_c::Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variable
//

FrameParserStatus_t   FrameParser_Video_c::Reset(       void )
{
    ParsedVideoParameters       		= NULL;

    CollapseHolesInDisplayIndices		= true;
    NextDecodeFieldIndex        		= 0;
    NextDisplayFieldIndex       		= 0;
    RevPlaySmoothReverseFailureCount		= 0;

//

    NewStreamParametersSeenButNotQueued		= false;

    CodedFramePlaybackTimeValid                 = false;
    CodedFrameDecodeTimeValid                   = false;

    AccumulatedPictureStructure                 = StructureEmpty;

    LastRecordedPlaybackTimeDisplayFieldIndex   = 0;
    LastRecordedNormalizedPlaybackTime          = INVALID_TIME;

    LastRecordedDecodeTimeFieldIndex            = 0;
    LastRecordedNormalizedDecodeTime            = INVALID_TIME;

    LastSeenContentFrameRate			= 0;
    LastSeenContentWidth			= 0;
    LastSeenContentHeight			= 0;

    RevPlayClearResourceUtilization();

    // Added by PB
    DeferredCodedFrameBuffer                    = NULL;
    DeferredParsedFrameParameters               = NULL;
    DeferredParsedVideoParameters               = NULL;
    DeferredCodedFrameBufferSecondField         = NULL;
    DeferredParsedFrameParametersSecondField    = NULL;
    DeferredParsedVideoParametersSecondField    = NULL;

    LastFieldRate                               = 1;

    ValidPTSDeducedFrameRate			= false;
    StandardPTSDeducedFrameRate			= false;
    LastStandardPTSDeducedFrameRate		= 0;

//

    if( ReverseQueuedPostDecodeSettingsRing != NULL)
    {
	delete ReverseQueuedPostDecodeSettingsRing;
	ReverseQueuedPostDecodeSettingsRing = NULL;         
    }

    if( ReverseDecodeUnsatisfiedReferenceStack != NULL )
    {
	delete ReverseDecodeUnsatisfiedReferenceStack;
	ReverseDecodeUnsatisfiedReferenceStack  = NULL;
    }

    if( ReverseDecodeSingleFrameStack != NULL )
    {
	delete ReverseDecodeSingleFrameStack;
	ReverseDecodeSingleFrameStack		= NULL;
    }

    if( ReverseDecodeStack != NULL )
    {
	delete ReverseDecodeStack;
	ReverseDecodeStack                      = NULL;
    }

//

    return FrameParser_Base_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function
//

FrameParserStatus_t   FrameParser_Video_c::RegisterOutputBufferRing(
					Ring_t          Ring )
{
FrameParserStatus_t     Status;

    //
    // First allow the base class to perform it's operations, 
    // as we operate on the buffer pool obtained by it.
    //

    Status      = FrameParser_Base_c::RegisterOutputBufferRing( Ring );
    if( Status != FrameParserNoError )
	return Status;

    //
    // Obtain stacks used in reverse play
    //

    ReverseDecodeUnsatisfiedReferenceStack      = new class StackGeneric_c(FrameBufferCount);
    if( (ReverseDecodeUnsatisfiedReferenceStack == NULL) ||
	(ReverseDecodeUnsatisfiedReferenceStack->InitializationStatus != StackNoError) )
    {
	report( severity_error, "FrameParser_Video_c::RegisterOutputBufferRing - Failed to create stack to hold buffers with reference frames unsatisfied during reverse play (Open groups).\n" );
	SetComponentState(ComponentInError);
	return FrameParserFailedToCreateReversePlayStacks;
    }

    ReverseDecodeSingleFrameStack	      = new class StackGeneric_c(FrameBufferCount);
    if( (ReverseDecodeSingleFrameStack == NULL) ||
	(ReverseDecodeSingleFrameStack->InitializationStatus != StackNoError) )
    {
	report( severity_error, "FrameParser_Video_c::RegisterOutputBufferRing - Failed to create stack to hold buffers associated with a single decode frame.\n" );
	SetComponentState(ComponentInError);
	return FrameParserFailedToCreateReversePlayStacks;
    }

    ReverseDecodeStack  = new class StackGeneric_c(FrameBufferCount);
    if( (ReverseDecodeStack == NULL) ||
	(ReverseDecodeStack->InitializationStatus != StackNoError) )
    {
	report( severity_error, "FrameParser_Video_c::RegisterOutputBufferRing - Failed to create stack to hold non-reference frame decodes during reverse play.\n" );
	SetComponentState(ComponentInError);
	return FrameParserFailedToCreateReversePlayStacks;
    }

    //
    // Attach the video specific parsed frame parameters to every element of the pool
    //

    Status      = CodedFrameBufferPool->AttachMetaData( Player->MetaDataParsedVideoParametersType );
    if( Status != BufferNoError )
    {
	report( severity_error, "FrameParser_Video_c::RegisterCodedFrameBufferPool - Failed to attach parsed video parameters to all coded frame buffers.\n" );
	SetComponentState(ComponentInError);
	return Status;
    }

    //
    // Create the ring used to hold buffer pointers when 
    // we defer generation of PTS/display indices during
    // reverse play. (NOTE 2 entries per frame)
    //

    ReverseQueuedPostDecodeSettingsRing = new class RingGeneric_c(2 * FrameBufferCount);
    if( (ReverseQueuedPostDecodeSettingsRing == NULL) ||
	(ReverseQueuedPostDecodeSettingsRing->InitializationStatus != RingNoError) )
    {
	report( severity_error, "FrameParser_Video_c::RegisterOutputBufferRing - Failed to create ring to hold buffers with display indices/Playback times not set during reverse play.\n" );
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

FrameParserStatus_t   FrameParser_Video_c::Input(       Buffer_t                  CodedBuffer )
{
FrameParserStatus_t     Status;

    //
    // Are we allowed in here
    //

    AssertComponentState( "FrameParser_Video_c::Input", ComponentRunning );

    //
    // Initialize context pointers
    //

    ParsedVideoParameters       = NULL;

    //
    // First perform base operations
    //

    Status      = FrameParser_Base_c::Input( CodedBuffer );
    if( Status != FrameParserNoError )
	return Status;

    st_relayfs_write(ST_RELAY_TYPE_CODED_VIDEO_BUFFER, ST_RELAY_SOURCE_VIDEO_FRAME_PARSER, (unsigned char *)BufferData, BufferLength, 0 );

    //
    // Obtain video specific pointers to data associated with the buffer.
    //

    Status      = Buffer->ObtainMetaDataReference( Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "FrameParser_Video_c::Input - Unable to obtain the meta data \"ParsedVideoParameters\".\n" );
	return Status;
    }

    memset( ParsedVideoParameters, 0x00, sizeof(ParsedVideoParameters_t) );

    //
    // Now execute the processing chain for a buffer
    //

    return ProcessBuffer();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The standard processing chain for a single input buffer
//      this is the video specific implementation, it probably 
//      differs quite significantly from the audio implementation.
//

FrameParserStatus_t   FrameParser_Video_c::ProcessBuffer( void )
{
FrameParserStatus_t     Status;

    //
    // Handle discontinuity in input stream
    //

    if( CodedFrameParameters->StreamDiscontinuity )
    {
#if 0
	report( severity_info, "Disco - %d %d %d - %d (%d %d %d)\n", 
		CodedFrameParameters->StreamDiscontinuity, CodedFrameParameters->ContinuousReverseJump, CodedFrameParameters->FlushBeforeDiscontinuity,
		(PlaybackDirection == PlayForward),
		ReverseDecodeStack->NonEmpty(), ReverseDecodeUnsatisfiedReferenceStack->NonEmpty(), ReverseDecodeSingleFrameStack->NonEmpty() );
#endif

	if( (PlaybackDirection == PlayBackward) && CodedFrameParameters->ContinuousReverseJump )
	{
	    RevPlayProcessDecodeStacks();
	}
	else
	{
	    if( PlaybackDirection == PlayForward )
		ForPlayPurgeQueuedPostDecodeParameterSettings();

	    if( (PlaybackDirection == PlayBackward) || ReverseDecodeStack->NonEmpty() || ReverseDecodeUnsatisfiedReferenceStack->NonEmpty() )
		RevPlayPurgeDecodeStacks();

	    CollapseHolesInDisplayIndices		= true;

	    LastRecordedPlaybackTimeDisplayFieldIndex   = 0;                    // Invalidate the time bases used in ongoing calculations
	    LastRecordedNormalizedPlaybackTime          = INVALID_TIME;

	    LastRecordedDecodeTimeFieldIndex            = 0;
	    LastRecordedNormalizedDecodeTime            = INVALID_TIME;

	    ResetReferenceFrameList();
	}

	AccumulatedPictureStructure	= StructureEmpty;
	FirstDecodeAfterInputJump       = true;
	SurplusDataInjected             = CodedFrameParameters->FlushBeforeDiscontinuity;
	ContinuousReverseJump           = CodedFrameParameters->ContinuousReverseJump;
	Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnOutputPartialDecodeBuffers );
    }

    //
    // Do we have something to do with this buffer
    //

    if( BufferLength == 0 )
    {
	//
	// Check for a marker frame, identified by no flags and no data
	//

	if( !CodedFrameParameters->StreamDiscontinuity )
	{
	    Buffer->IncrementReferenceCount( IdentifierFrameParserMarkerFrame );
	    OutputRing->Insert( (unsigned int )Buffer );
	}

	return FrameParserNoError;
    }

    //
    // Copy the playback and decode times from coded frame parameters
    //

    if( CodedFrameParameters->PlaybackTimeValid )
    {
	CodedFramePlaybackTimeValid     = true;
	CodedFramePlaybackTime          = CodedFrameParameters->PlaybackTime;
    }

    if( CodedFrameParameters->DecodeTimeValid )
    {
	CodedFrameDecodeTimeValid       = true;
	CodedFrameDecodeTime            = CodedFrameParameters->DecodeTime;
    }

    //
    // Parse the headers
    //

    FrameToDecode                       = false; 
    ParsedVideoParameters->FirstSlice	= true;			// Default for non-slice decoders


    Status      = ReadHeaders();
    if( Status != FrameParserNoError )
	return Status;

    //
    // Do we still have something to do
    //

    if( !FrameToDecode )
	return FrameParserNoError;

    //
    // Perform the separate chain depending on the direction of play
    //

    if( PlaybackDirection == PlayForward )
	Status  = ForPlayProcessFrame();
    else
	Status  = RevPlayProcessFrame();

//

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Deal with decode of a single frame in forward play
//

FrameParserStatus_t   FrameParser_Video_c::ForPlayProcessFrame(      void )
{
FrameParserStatus_t     Status;

    //
    // Have we missed any new stream parameters
    //

    if( NewStreamParametersSeenButNotQueued )
	ParsedFrameParameters->NewStreamParameters	= true;
    else if( ParsedFrameParameters->NewStreamParameters )
	NewStreamParametersSeenButNotQueued		= true;

    //
    // Prepare the reference frame list for this frame
    //

    Status      = PrepareReferenceFrameList();
    if( Status != FrameParserNoError )
	return Status;

    //
    // Can we generate any queued index and pts values based on what we have seen
    //

    Status      = ForPlayProcessQueuedPostDecodeParameterSettings();
    if( Status != FrameParserNoError )
	return Status;

    //
    // Calculate the display index/PTS values
    //

    Status      = ForPlayGeneratePostDecodeParameterSettings();
    if( Status != FrameParserNoError )
	return Status;

    //
    // Queue the frame for decode
    //

    Status      = ForPlayQueueFrameForDecode();
    if( Status != FrameParserNoError )
	return Status;

    FirstDecodeAfterInputJump           = false;
    SurplusDataInjected                 = false;
    ContinuousReverseJump               = false;

    //
    // Update reference frame management
    //

    Status      = ForPlayUpdateReferenceFrameList();
    if( Status != FrameParserNoError )
	return Status;

    //
    // If this is the last thing before a stream termination code, 
    // then clean out the frame labellings.
    //

    if( CodedFrameParameters->FollowedByStreamTerminate )
	Status	= ForPlayPurgeQueuedPostDecodeParameterSettings();

//

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Deal with a single frame in reverse play
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayProcessFrame(      void )
{
FrameParserStatus_t     Status;
bool			NewFrame;

    //
    // If we cannot handle smooth reverse, then discard all none independent 
    // frames but to ensure decent timing we increment field counts.
    //

    if( !Configuration.SupportSmoothReversePlay &&
	!ParsedFrameParameters->IndependentFrame )
    {
	NextDisplayFieldIndex		-= (ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1]);
	return FrameParserNoError;
    }

    //
    // Check for a transition to a discarding state
    //

    if( !RevPlayDiscardingState )
    {
	Status	= RevPlayCheckResourceUtilization();
	if( Status != FrameParserNoError )
	{
	    report( severity_error, "FrameParser_Video_c::RevPlayProcessFrame - Failed to check resource utilization - Implementation error.\n" );
	    Player->MarkStreamUnPlayable( Stream );
	    return Status;
	}
    }

    //
    // If we are in discarding state then just count the 
    // frame/field discarded, and release it 
    // (we just don't claim to effect a release).
    //

    NewFrame	= ParsedFrameParameters->NewFrameParameters && 
		  ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if( RevPlayDiscardingState )
    {
	if( NewFrame )
	    RevPlayDiscardedFrameCount++;

	return FrameParserNoError;
    }

    //
    // We are not discarding so we are going to process this frame.
    // Initialize pts/display indices
    //

    if( NewFrame )
	RevPlayAccumulatedFrameCount++;

    InitializePostDecodeParameterSettings();

    //
    // Queue the frame for decode
    //

    Status      = RevPlayQueueFrameForDecode();
    if( Status != FrameParserNoError )
	return Status;

    FirstDecodeAfterInputJump           = false;
    SurplusDataInjected                 = false;
    ContinuousReverseJump               = false;

    //
    // Update reference frame management
    //

    Status      = RevPlayAppendToReferenceFrameList();

//

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Addition to the base queue a buffer for decode increments 
//      the field index.
//

FrameParserStatus_t   FrameParser_Video_c::ForPlayQueueFrameForDecode( void )
{
PlayerStatus_t		Status;
PlayerEventRecord_t	Event;

    //
    // Is there anything we should signal
    //

    if( (LastSeenContentFrameRate != ParsedVideoParameters->Content.FrameRate) &&
	((EventMask & EventFrameRateChangeParse) != 0) )
    {
	Event.Code			= EventFrameRateChangeParse;
	Event.Playback			= Playback;
	Event.Stream			= Stream;
	Event.PlaybackTime		= ParsedFrameParameters->NativePlaybackTime;
	Event.Rational			= ParsedVideoParameters->Content.FrameRate;
	Status				= Player->SignalEvent( &Event );
	if( Status != PlayerNoError )
	    report( severity_error, "FrameParser_Video_c::ForPlayQueueFrameForDecode - Failed to signal event.\n" );
    }

//

    if( ((LastSeenContentWidth  != ParsedVideoParameters->Content.Width) ||
	 (LastSeenContentHeight != ParsedVideoParameters->Content.Height) ) &&
	((EventMask & EventSizeChangeParse) != 0) )
    {
	Event.Code			= EventSizeChangeParse;
	Event.Playback			= Playback;
	Event.Stream			= Stream;
	Event.PlaybackTime		= ParsedFrameParameters->NativePlaybackTime;
	Event.Value[0].UnsignedInt	= ParsedVideoParameters->Content.Width;
	Event.Value[1].UnsignedInt	= ParsedVideoParameters->Content.Height;
	Status				= Player->SignalEvent( &Event );
	if( Status != PlayerNoError )
	    report( severity_error, "FrameParser_Video_c::ForPlayQueueFrameForDecode - Failed to signal event.\n" );
    }

//

    LastSeenContentFrameRate		= ParsedVideoParameters->Content.FrameRate;
    LastSeenContentWidth		= ParsedVideoParameters->Content.Width;
    LastSeenContentHeight		= ParsedVideoParameters->Content.Height;

    //
    // Force the rewind of decode frame index, if this is not a first slice,
    // and only increment the decode field index if it is a first slice.
    //

    if( !ParsedVideoParameters->FirstSlice )
	NextDecodeFrameIndex--;
    else
    {
	if( PlaybackDirection == PlayForward )
	    NextDecodeFieldIndex	+= (ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1]);
	else
	    NextDecodeFieldIndex	-= (ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1]);
    }

    //
    // Output queueing debug information
    //

#if 0
//if( ParsedVideoParameters->FirstSlice  && ParsedFrameParameters->IndependentFrame )
if( ParsedVideoParameters->FirstSlice )
{
unsigned int     i;
unsigned int     Length;
unsigned char   *Data;
unsigned int     Checksum;

    Buffer->ObtainDataReference( NULL, &Length, (void **)&Data );

    Length      -= ParsedFrameParameters->DataOffset;
    Data        += ParsedFrameParameters->DataOffset;

    Checksum    = 0;
    for( i=0; i<Length; i++ )
	Checksum        += Data[i];

    report( severity_info, "Q %d ( K = %d, R = %d, RL= {%d %d %d}, ST = %d, PS = %d, FPFP= %d, FS = %d) (%08x %d)\n",
		NextDecodeFrameIndex, 
		ParsedFrameParameters->KeyFrame,
		ParsedFrameParameters->ReferenceFrame, 
		ParsedFrameParameters->ReferenceFrameList[0].EntryCount, ParsedFrameParameters->ReferenceFrameList[1].EntryCount, ParsedFrameParameters->ReferenceFrameList[2].EntryCount,
		ParsedVideoParameters->SliceType,
		ParsedVideoParameters->PictureStructure,
		ParsedFrameParameters->FirstParsedParametersForOutputFrame,
		ParsedVideoParameters->FirstSlice,
		Checksum, Length );

#if 0
    report( severity_info, "\t\t%4d %4d %4d %4d - %4d %4d %4d %4d - %4d %4d %4d %4d\n", 
	ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[1], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[2], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[3],
	ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[0], ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[1], ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[2], ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[3],
	ParsedFrameParameters->ReferenceFrameList[2].EntryIndicies[0], ParsedFrameParameters->ReferenceFrameList[2].EntryIndicies[1], ParsedFrameParameters->ReferenceFrameList[2].EntryIndicies[2], ParsedFrameParameters->ReferenceFrameList[2].EntryIndicies[3] );
#endif
}
#endif

    //
    // Allow the base class to actually queue the frame
    //

    NewStreamParametersSeenButNotQueued		= false;

    return FrameParser_Base_c::QueueFrameForDecode();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Specific reverse play implementation of queue for decode
//      the field index.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayQueueFrameForDecode( void )
{
FrameParserStatus_t     Status;

    //
    // Prepare the reference frame list for this frame
    //

    Status  = PrepareReferenceFrameList();
    if( Status != FrameParserNoError )
	ParsedFrameParameters->NumberOfReferenceFrameLists	= INVALID_INDEX;

    //
    // Deal with reference frame
    //

    if( ParsedFrameParameters->ReferenceFrame )
    {
	//
	// Check that we can decode this
	//

	if( ParsedFrameParameters->NumberOfReferenceFrameLists == INVALID_INDEX )
	{
	    // This happens for some perfectly valid H264 streams, we simply cannot reverse play these without slipping into IDR only synchronization mode
	    report( severity_error, "FrameParser_Video_c::RevPlayQueueFrameForDecode - Insufficient reference frames for a reference frame in reverse play mode.\n" );
	    RevPlayDiscardingState	= true;
	    RevPlaySmoothReverseFailureCount++;

	    return FrameParserInsufficientReferenceFrames;
        }

	//
	// Go ahead and decode it (just as for forward play)
	//

	Status  = ForPlayQueueFrameForDecode();
	if( Status != FrameParserNoError )
	{
	    report( severity_error, "FrameParser_Video_c::RevPlayQueueFrameForDecode - Failed to queue reference frames for decode.\n" );
	    return Status;
	}
    }

    //
    // Now whether it was a reference frame or not, we want to stack it
    //

    Buffer->IncrementReferenceCount( IdentifierReverseDecodeStack );
    ReverseDecodeStack->Push( (unsigned int)Buffer );

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      This function is responsible for walking the stacks to handle 
//      reverse decode.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayProcessDecodeStacks(          void )
{
FrameParserStatus_t               Status;
Buffer_t                          PreservedBuffer;
ParsedFrameParameters_t          *PreservedParsedFrameParameters;
ParsedVideoParameters_t          *PreservedParsedVideoParameters;

#if 0
report( severity_info, "Utilized %2d %2d %2d\n", NumberOfUtilizedFrameParameters, NumberOfUtilizedStreamParameters, NumberOfUtilizedDecodeBuffers );
{
unsigned int                      C0,C1, C2;

FrameParametersPool->GetPoolUsage( &C0, &C1, NULL, NULL, NULL );
report( severity_info, "         FP %d/%d => %d/%d\n", NumberOfUtilizedFrameParameters, Configuration.FrameParametersCount, C1, C0 );  

Manifestor->GetDecodeBufferCount( &C2 );
DecodeBufferPool->GetPoolUsage( &C0, &C1, NULL, NULL, NULL );
report( severity_info, "         DB %d/%d => %d/%d\n", NumberOfUtilizedDecodeBuffers, C2, C1, C0 );
}
#endif

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

    while( ReverseDecodeUnsatisfiedReferenceStack->NonEmpty() )
    {
	ReverseDecodeUnsatisfiedReferenceStack->Pop( (unsigned int *)&Buffer );

	Buffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters) );
	Buffer->ObtainMetaDataReference( Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters) );

	RevPlayNextSequenceFrameProcess();

	if( ParsedFrameParameters->NumberOfReferenceFrameLists == INVALID_INDEX )
	{
	    Status  = PrepareReferenceFrameList();
	    if( Status != FrameParserNoError )
	    {
		report( severity_error, "FrameParser_Video_c::RevPlayProcessDecodeStacks - Insufficient reference frames for a deferred decode.\n" );
		ParsedFrameParameters->NumberOfReferenceFrameLists = INVALID_INDEX;

		// Lose this frame
		Buffer->DecrementReferenceCount( IdentifierReverseDecodeStack );
		continue;
	    }
	}

	RevPlayAppendToReferenceFrameList();

	ReverseDecodeStack->Push( (unsigned int)Buffer );
    }

    //
    // Now process the stack in reverse order
    //

    while( ReverseDecodeStack->NonEmpty() )
    {
	//
	// First extract a single frame onto the single frame stack 
	// We do this because individual frames are always decoded in 
	// forward order.
	//

	do
	{
	    ReverseDecodeStack->Pop( (unsigned int *)&Buffer );
	    Buffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters) );
	    ReverseDecodeSingleFrameStack->Push( (unsigned int)Buffer );
	} while( ReverseDecodeStack->NonEmpty() &&
		 !ParsedFrameParameters->FirstParsedParametersForOutputFrame &&
		 (ParsedFrameParameters->NumberOfReferenceFrameLists != INVALID_INDEX) );

	//
	// If we exited because of an invalid index, the we have an open group
	// so we exit processing, and transfer all remaining data onto the 
	// unsatisfied reference stack for deferred decode.
	//

	if( ParsedFrameParameters->NumberOfReferenceFrameLists == INVALID_INDEX )
	    break;

	//
	// Now we loop processing the single frame stack in forward order
	//

	while( ReverseDecodeSingleFrameStack->NonEmpty() )
	{
	    //
	    // Retrieve the buffer and the pointers (they were 
	    // there before so we assume they are there now).
	    //

	    ReverseDecodeSingleFrameStack->Pop( (unsigned int *)&Buffer );

	    Buffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters) );
	    Buffer->ObtainMetaDataReference( Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters) );

	    //
	    // Calculate the display index/PTS values
	    //

	    Status  = RevPlayGeneratePostDecodeParameterSettings();
	    if( Status != FrameParserNoError )
	    {
	        // We are in serious trouble, deadlock is on its way.
	        report( severity_error, "FrameParser_Video_c::RevPlayProcessDecodeStacks - Failed to generate post decode parameters - Fatal implementation error.\n" );
	        return PlayerImplementationError;
	    }

	    //
	    // Split into Reference or non-reference frame handling
	    //

	    if( ParsedFrameParameters->ReferenceFrame )
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

	    Buffer->DecrementReferenceCount( IdentifierReverseDecodeStack );
	}
    }

    //
    // If we stopped processing the reverse decode stack due
    // to unsatisfied reference frames (an open group), then
    // transfer the remains of the stack to the unsatisfied 
    // reference stack.
    //

    RevPlayClearResourceUtilization();

    while( ReverseDecodeSingleFrameStack->NonEmpty() )
    {
	// Note single frame goes back on reverse decode to re-establish the reverse order of its component fields/slices.
	ReverseDecodeSingleFrameStack->Pop( (unsigned int *)&Buffer );
	ReverseDecodeStack->Push( (unsigned int)Buffer );
    }

    while( ReverseDecodeStack->NonEmpty() )
    {
	ReverseDecodeStack->Pop( (unsigned int *)&Buffer );
	ReverseDecodeUnsatisfiedReferenceStack->Push( (unsigned int)Buffer );

	//
	// Increment the resource utilization counts 
	//

	Buffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters) );
	Buffer->ObtainMetaDataReference( Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters) );

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

FrameParserStatus_t   FrameParser_Video_c::RevPlayPurgeDecodeStacks(            void )
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

    while( ReverseDecodeUnsatisfiedReferenceStack->NonEmpty() )
    {
	ReverseDecodeUnsatisfiedReferenceStack->Pop( (unsigned int *)&Buffer );
	ReverseDecodeStack->Push( (unsigned int)Buffer );
    }

    //
    // Now process the stack in reverse order
    //

    while( ReverseDecodeStack->NonEmpty() )
    {
	//
	// Retrieve the buffer and the pointers (they were 
	// there before so we assume they are there now).
	//

	ReverseDecodeStack->Pop( (unsigned int *)&Buffer );
	Buffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters) );
	Buffer->ObtainMetaDataReference( Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters) );

	//
	// Split into Reference or non-reference frame handling
	//

	if( ParsedFrameParameters->ReferenceFrame )
	{
	    //
	    // Calculate the display index/PTS values this guarantees that 
	    // refrence frames that are held in frame re-ordering will be released.
	    //

	    Status      = RevPlayGeneratePostDecodeParameterSettings();
	    if( Status != FrameParserNoError )
	    {
		// We are in serious trouble, deadlock is on its way.
		report( severity_error, "FrameParser_Video_c::RevPlayPurgeDecodeStacks - Failed to generate post decode parameters - Fatal implementation error.\n" );
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

	Buffer->DecrementReferenceCount( IdentifierReverseDecodeStack );
    }

    //
    // We have now processed all stacked entities, however it is possible that 
    // some codec may still hold frames for post decode parameter settings 
    // (H264 specifically), so we issue a purge on this operation.
    //

    Status	= RevPlayPurgeQueuedPostDecodeParameterSettings();
    if( Status != FrameParserNoError )
    {
	// We are in serious trouble, deadlock may be on its way.
	report( severity_error, "FrameParser_Video_c::RevPlayPurgeDecodeStacks - Failed to purge post decode parameters - Probable fatal implementation error.\n" );
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
//	reference stack, when we have a failure to do smooth reverse decode,
//	to discard everything on it.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayPurgeUnsatisfiedReferenceStack(            void )
{
FrameParserStatus_t               Status;
Buffer_t                          PreservedBuffer;
ParsedFrameParameters_t          *PreservedParsedFrameParameters;
ParsedVideoParameters_t          *PreservedParsedVideoParameters;
bool				  NewFrame;

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

    while( ReverseDecodeUnsatisfiedReferenceStack->NonEmpty() )
    {
	//
	// Retrieve the buffer and the pointers (they were 
	// there before so we assume they are there now).
	//

	ReverseDecodeUnsatisfiedReferenceStack->Pop( (unsigned int *)&Buffer );
	Buffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters) );
	Buffer->ObtainMetaDataReference( Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters) );

	//
	// If this is a reference framne, it has been passed down the decode chain
	// it needs index/PTS values calculating, and freeing up in the codec
	//

	if( ParsedFrameParameters->ReferenceFrame )
	{
	    Status      = RevPlayGeneratePostDecodeParameterSettings();
	    if( Status != FrameParserNoError )
	    {
		// We are in serious trouble, deadlock is on its way.
		report( severity_error, "FrameParser_Video_c::RevPlayPurgeUnsatisfiedReferenceStack - Failed to generate post decode parameters - Fatal implementation error.\n" );
		return PlayerImplementationError;
	    }

	    // Note we do not remove the reference frame from the reference frame list
	    // because in the unsatisfied reference stack it has not been placed int the list yet.
	    // However we must release it as a reference frame from the codec

	    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex );
	}

	//
	// Adjust the counts of discarded frames
	//

	NewFrame	= ParsedFrameParameters->NewFrameParameters && 
			  ParsedFrameParameters->FirstParsedParametersForOutputFrame;

	if( NewFrame )
	    RevPlayDiscardedFrameCount++;

	//
	// We are at liberty to forget about this buffer now
	//

	Buffer->DecrementReferenceCount( IdentifierReverseDecodeStack );
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
//	it allows continued smooth reverse play. NOTE this function is only
//	called if we are not already in a discarding state.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayClearResourceUtilization( void )
{
    NumberOfUtilizedFrameParameters		= 0;
    NumberOfUtilizedStreamParameters		= 0;
    NumberOfUtilizedDecodeBuffers		= 0;
    RevPlayDiscardingState			= false;
    RevPlayAccumulatedFrameCount		= 0;
    RevPlayDiscardedFrameCount			= 0;

    if( RevPlaySmoothReverseFailureCount > MAX_ALLOWED_SMOOTH_REVERSE_PLAY_FAILURES )
	Configuration.SupportSmoothReversePlay	= false;

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Reverse play function to count resource utilization and check if
//	it allows continued smooth reverse play. NOTE this function is only
//	called if we are not already in a discarding state.
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayCheckResourceUtilization( void )
{
PlayerStatus_t		Status;
bool			NewFrame;
unsigned int		WorkingCount;
unsigned int		DecodeBufferCount;
PlayerEventRecord_t	Event;

    //
    // Update our counts of utilized resources
    //

    if( ParsedFrameParameters->NewFrameParameters && ParsedVideoParameters->FirstSlice )
	NumberOfUtilizedFrameParameters++;

    if( ParsedFrameParameters->NewStreamParameters )
	NumberOfUtilizedStreamParameters++;

    NewFrame	= ParsedVideoParameters->FirstSlice &&
		  ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if( NewFrame && ParsedFrameParameters->ReferenceFrame )
	NumberOfUtilizedDecodeBuffers++;

    //
    // Here we check our counts of utilized resources to 
    // ensure that we can function with this frame.
    // Note we use GE (>=) checks on resource needed before 
    // this point, and GT (>) checks on resources needed after 
    // this point.
    //

    Manifestor->GetDecodeBufferCount( &DecodeBufferCount );
    WorkingCount	= 2 * (PLAYER_MINIMUM_NUMBER_OF_WORKING_DECODE_BUFFERS + 1);	/* 2x for field decodes */

#define RevResourceCheckGE( U, A, T )	if( !RevPlayDiscardingState && ((U) >= (A)) ) {report( severity_info, "FrameParser_Video_c::RevPlayProcessFrame - Unable to smooth reverse (%s)\n", T ); RevPlayDiscardingState = true; }
#define RevResourceCheckGT( U, A, T )	if( !RevPlayDiscardingState && ((U) > (A)) ) {report( severity_info, "FrameParser_Video_c::RevPlayProcessFrame - Unable to smooth reverse (%s)\n", T ); RevPlayDiscardingState = true; }

    RevResourceCheckGT( NumberOfUtilizedFrameParameters, (Configuration.FrameParametersCount - WorkingCount), "FrameParameters" );
    RevResourceCheckGE( NumberOfUtilizedStreamParameters, (Configuration.StreamParametersCount - WorkingCount), "StreamParameters" );
    RevResourceCheckGT( NumberOfUtilizedDecodeBuffers, (DecodeBufferCount-3), "DecodeBuffers" );
    RevResourceCheckGT( NumberOfUtilizedDecodeBuffers, (Configuration.MaxReferenceFrameCount-3), "ReferenceFrames" );	// We only accumulate reference frames 

#undef RevResourceCheck

    //
    // If we are transitioning to a discard state, then we need to purge
    // the ReverseDecodeUnsatisfiedReferenceStack, since we will never get 
    // around to being able to satisfy their references.
    //
    // Also we offer to raise the failed to reverse decode smoothly event
    //

    if( RevPlayDiscardingState )
    {
	RevPlaySmoothReverseFailureCount++;
	RevPlayDiscardedFrameCount	= 0;
	RevPlayPurgeUnsatisfiedReferenceStack();

	if( (EventFailureToPlaySmoothReverse & EventMask) != 0 )
	{
	    Event.Code			= EventFailureToPlaySmoothReverse;
	    Event.Playback		= Playback;
	    Event.Stream		= Stream;
	    Event.PlaybackTime		= TIME_NOT_APPLICABLE;
	    Event.UserData		= EventUserData;

	    Status			= Player->SignalEvent( &Event );
	    if( Status != PlayerNoError )
		report( severity_error, "FrameParser_Video_c::RevPlayCheckResourceUtilization - Failed to signal event.\n" );
	}
    }

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to initialize the post decode parameter 
//      settings for reverse play, these consist of the display frame index, 
//      and presentation time, both of which may be deferred if not explicitly 
//      specified during reverse play.
//

FrameParserStatus_t   FrameParser_Video_c::InitializePostDecodeParameterSettings( void )
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

    if( CodedFramePlaybackTimeValid )
    {
	CodedFramePlaybackTimeValid                     = false;
	ParsedFrameParameters->NativePlaybackTime       = CodedFramePlaybackTime;
	TranslatePlaybackTimeNativeToNormalized( CodedFramePlaybackTime, &ParsedFrameParameters->NormalizedPlaybackTime );
    }

    if( CodedFrameDecodeTimeValid )
    {
	CodedFrameDecodeTimeValid                       = false;
	ReasonableDecodeTime                            = (ParsedFrameParameters->NativePlaybackTime == INVALID_TIME) ||
							  (((CodedFramePlaybackTime - CodedFrameDecodeTime) & 0x1ffffffffull) < MAXIMUM_DECODE_TIME_OFFSET);

	if( ReasonableDecodeTime )
	{
	    ParsedFrameParameters->NativeDecodeTime     = CodedFrameDecodeTime;
	    TranslatePlaybackTimeNativeToNormalized( CodedFrameDecodeTime, &ParsedFrameParameters->NormalizedDecodeTime );
	}
	else
	{
	    report( severity_error, "FrameParser_Video_c::InitializePostDecodeParameterSettings - (PTS - DTS) ridiculuously large (%d 90kHz ticks)\n", 
				(CodedFramePlaybackTime - CodedFrameDecodeTime) ); 
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
					ParsedVideoParameters_t         *ParsedVideo )
{
int                      ElapsedFields;
long long		 ElapsedTime;
long long 		 MicroSecondsPerFrame;
int                      FieldIndex;
Rational_t               FieldRate;
Rational_t               Temp;
bool                     DerivePresentationTime;

    //
    // Calculate the field indices, if this is not the first decode of the 
    // frame abort before proceeding on with display frame and pts/dts generation.
    //

    if( PlaybackDirection == PlayForward )
    {
	FieldIndex                               = NextDisplayFieldIndex;
	NextDisplayFieldIndex                   += (ParsedVideo->DisplayCount[0] + ParsedVideo->DisplayCount[1]);
    }
    else
    {
	NextDisplayFieldIndex                   -= (ParsedVideo->DisplayCount[0] + ParsedVideo->DisplayCount[1]);
	FieldIndex                               = NextDisplayFieldIndex;
    }

    if( !ParsedFrame->FirstParsedParametersForOutputFrame )
	return;

    //
    // Special case (invented for h264) if this is a field,
    // but it is not the first field to be displayed, then we 
    // knock one off the field index. This was detected in
    // h264 because of the Picture Adapative Frame Field tests
    // which can often switch decode order ie Frame, top, bottom, bottom, top ...
    //

    if( (ParsedVideo->PictureStructure != StructureFrame) &&
	((ParsedVideo->PictureStructure == StructureTopField) != ParsedVideo->TopFieldFirst) )
	FieldIndex--;

    //
    // Calculate the indices
    //

    ParsedFrame->DisplayFrameIndex               = NextDisplayFrameIndex++;
    ParsedFrame->CollapseHolesInDisplayIndices	 = CollapseHolesInDisplayIndices;
    CollapseHolesInDisplayIndices		 = false;

    //
    // Obtain the presentation time
    //

    DerivePresentationTime      = ParsedFrame->NormalizedPlaybackTime == INVALID_TIME;

    if( DerivePresentationTime )
    {
	if( LastRecordedNormalizedPlaybackTime != INVALID_TIME )
	{
	    ElapsedFields                           = FieldIndex - LastRecordedPlaybackTimeDisplayFieldIndex;
	    ElapsedTime                             = LongLongIntegerPart((1000000 / LastFieldRate) * ElapsedFields);

	    ParsedFrame->NormalizedPlaybackTime     = LastRecordedNormalizedPlaybackTime + ElapsedTime;
	    TranslatePlaybackTimeNormalizedToNative( ParsedFrame->NormalizedPlaybackTime, &ParsedFrame->NativePlaybackTime );
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

    if( !DerivePresentationTime && (LastRecordedNormalizedPlaybackTime != INVALID_TIME) )
    {
	ElapsedFields		= (FieldIndex - LastRecordedPlaybackTimeDisplayFieldIndex) * (ParsedVideo->Content.Progressive ? 2 : 1);
	ElapsedTime		= ParsedFrame->NormalizedPlaybackTime - LastRecordedNormalizedPlaybackTime;
	MicroSecondsPerFrame	= (2 * ElapsedTime) / ElapsedFields;

	DeduceFrameRateFromPresentationTime( MicroSecondsPerFrame );
    }

    //
    // We rebase recorded times for out pts derivation, if we 
    // have a specified time or if the field/frame rate has changed (when we have recorded times). 
    //

    FieldRate                               = ParsedVideo->Content.FrameRate;
    if( !ParsedVideo->Content.Progressive )
	FieldRate                           = FieldRate * 2;

    if( !DerivePresentationTime ||
	((LastRecordedNormalizedPlaybackTime != INVALID_TIME) && (FieldRate != LastFieldRate)) )
    {
	LastRecordedPlaybackTimeDisplayFieldIndex   = FieldIndex;
	LastRecordedNormalizedPlaybackTime          = ParsedFrame->NormalizedPlaybackTime;
    }

    LastFieldRate       = FieldRate;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The calculation code to calculate the decode time stamp.
//
//      Note the deduced decode times are jittered by upto 1/4 
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
					ParsedVideoParameters_t         *ParsedVideo )
{
unsigned int             ElapsedFields;
unsigned long long 	 ElapsedTime;
Rational_t               FieldRate;
Rational_t               Temp;

    //
    // Do nothing if this is not the first decode of a frame
    //

    if( !ParsedFrame->FirstParsedParametersForOutputFrame )
	return;

    //
    // Obtain the Decode time
    //

    if( ParsedFrame->NormalizedDecodeTime == INVALID_TIME )
    {
	if( LastRecordedNormalizedDecodeTime != INVALID_TIME )
	{
	    ElapsedFields                       = NextDecodeFieldIndex - LastRecordedDecodeTimeFieldIndex;
	    FieldRate                           = ParsedVideo->Content.FrameRate;
	    if( !ParsedVideo->Content.Progressive )
		FieldRate                       = FieldRate * 2;

	    Temp                                = (1000000 / FieldRate) * ElapsedFields;
	    ElapsedTime                         = Temp.LongLongIntegerPart();

	    ParsedFrame->NormalizedDecodeTime   = LastRecordedNormalizedDecodeTime + ElapsedTime;
	    TranslatePlaybackTimeNormalizedToNative( ParsedFrame->NormalizedDecodeTime, &ParsedFrame->NativeDecodeTime );
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

FrameParserStatus_t   FrameParser_Video_c::ResetReferenceFrameList( void )
{
    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL );

    ReferenceFrameList.EntryCount       = 0;

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to purge deferred post decode parameter
//      settings, these consist of the display frame index, and presentation 
//      time. Here we treat them as if we had received a reference frame.
//

FrameParserStatus_t   FrameParser_Video_c::ForPlayPurgeQueuedPostDecodeParameterSettings( void )
{
    //
    // Do we have something to process.
    //

    if( DeferredCodedFrameBuffer != NULL )
    {
	CalculateFrameIndexAndPts( DeferredParsedFrameParameters, DeferredParsedVideoParameters );
	DeferredCodedFrameBuffer->DecrementReferenceCount();

	if( DeferredCodedFrameBufferSecondField != NULL )
	{
	    CalculateFrameIndexAndPts( DeferredParsedFrameParametersSecondField, DeferredParsedVideoParametersSecondField );
	    DeferredCodedFrameBufferSecondField->DecrementReferenceCount();
	}

	DeferredCodedFrameBuffer                        = NULL;
	DeferredParsedFrameParameters                   = NULL;
	DeferredParsedVideoParameters                   = NULL;
	DeferredCodedFrameBufferSecondField             = NULL;
	DeferredParsedFrameParametersSecondField        = NULL;
	DeferredParsedVideoParametersSecondField        = NULL;
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

FrameParserStatus_t   FrameParser_Video_c::ForPlayProcessQueuedPostDecodeParameterSettings( void )
{
    //
    // Do we have something to process, and can we process it
    //

    if( FirstDecodeOfFrame && (DeferredCodedFrameBuffer != NULL) && ParsedFrameParameters->ReferenceFrame )
    {
	CalculateFrameIndexAndPts( DeferredParsedFrameParameters, DeferredParsedVideoParameters );
	DeferredCodedFrameBuffer->DecrementReferenceCount();

	if( DeferredCodedFrameBufferSecondField != NULL )
	{
	    CalculateFrameIndexAndPts( DeferredParsedFrameParametersSecondField, DeferredParsedVideoParametersSecondField );
	    DeferredCodedFrameBufferSecondField->DecrementReferenceCount();
	}

	DeferredCodedFrameBuffer                        = NULL;
	DeferredParsedFrameParameters                   = NULL;
	DeferredParsedVideoParameters                   = NULL;
	DeferredCodedFrameBufferSecondField             = NULL;
	DeferredParsedFrameParametersSecondField        = NULL;
	DeferredParsedVideoParametersSecondField        = NULL;
    }

//

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

FrameParserStatus_t   FrameParser_Video_c::ForPlayGeneratePostDecodeParameterSettings( void )
{
    //
    // Default setting
    //

    InitializePostDecodeParameterSettings();

    //
    // If this is the first decode of a field then we need display frame indices and presentation timnes
    // If this is not a reference frame do the calculation now, otherwise defer it.
    //

    if( !ParsedFrameParameters->ReferenceFrame )
    {
	CalculateFrameIndexAndPts( ParsedFrameParameters, ParsedVideoParameters );
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

	if( (DeferredCodedFrameBuffer != NULL) &&
	    ((DeferredParsedVideoParameters->PictureStructure ^ ParsedVideoParameters->PictureStructure) != StructureFrame) )
	{
	    report( severity_error, "FrameParser_Video_c::ForPlayGeneratePostDecodeParameterSettings - Deferred Field/Frame inconsistency - broken stream/implementation error.\n" );
	    ForPlayPurgeQueuedPostDecodeParameterSettings();
	}

	if( DeferredCodedFrameBufferSecondField != NULL )
	{
	    report( severity_error, "FrameParser_Video_c::ForPlayGeneratePostDecodeParameterSettings - Repeated deferral of second field - broken stream/implementation error.\n" );
	    ForPlayPurgeQueuedPostDecodeParameterSettings();
	}

//

	if( DeferredCodedFrameBuffer != NULL )
	{
	    DeferredCodedFrameBufferSecondField		= Buffer;
	    DeferredParsedFrameParametersSecondField    = ParsedFrameParameters;
	    DeferredParsedVideoParametersSecondField    = ParsedVideoParameters;
	}
	else
	{
	    DeferredCodedFrameBuffer                    = Buffer;
	    DeferredParsedFrameParameters               = ParsedFrameParameters;
	    DeferredParsedVideoParameters               = ParsedVideoParameters;
	}

	CalculateDts( ParsedFrameParameters, ParsedVideoParameters );
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

FrameParserStatus_t   FrameParser_Video_c::RevPlayGeneratePostDecodeParameterSettings( void )
{
    //
    // If this is not a reference frame then place it on the ring for calculation later
    //

    if( !ParsedFrameParameters->ReferenceFrame )
    {
	ReverseQueuedPostDecodeSettingsRing->Insert( (unsigned int)ParsedFrameParameters );
	ReverseQueuedPostDecodeSettingsRing->Insert( (unsigned int)ParsedVideoParameters );
    }
    else

    //
    // If this is a reference frame then process it, if is the first 
    // field (or a whole frame) then process the frames on the ring.
    //

    {
	CalculateFrameIndexAndPts( ParsedFrameParameters, ParsedVideoParameters );

	if( ParsedFrameParameters->FirstParsedParametersForOutputFrame )
	{
	    while( ReverseQueuedPostDecodeSettingsRing->NonEmpty() )
	    {
		ReverseQueuedPostDecodeSettingsRing->Extract( (unsigned int *)&DeferredParsedFrameParameters );
		ReverseQueuedPostDecodeSettingsRing->Extract( (unsigned int *)&DeferredParsedVideoParameters );
		CalculateFrameIndexAndPts( DeferredParsedFrameParameters, DeferredParsedVideoParameters );
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
		Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0] );

		ReferenceFrameList.EntryCount--;
		for( i=0; i<ReferenceFrameList.EntryCount; i++ )
		    ReferenceFrameList.EntryIndicies[i] = ReferenceFrameList.EntryIndicies[i+1];
	    }

	    ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount++] = ParsedFrameParameters->DecodeFrameIndex;
	}
	else
	{
	    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex );
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

FrameParserStatus_t   FrameParser_Video_c::RevPlayAppendToReferenceFrameList( void )
{
bool            LastField;

//

    LastField   = (ParsedVideoParameters->PictureStructure == StructureFrame) || 
		  !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if( ParsedFrameParameters->ReferenceFrame && LastField )
    {
	if( ReferenceFrameList.EntryCount >= MAX_ENTRIES_IN_REFERENCE_FRAME_LIST )
	{
	    report( severity_error, "FrameParser_Video_c::RevPlayAppendToReferenceFrameList - List full - Implementation error.\n" );
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

FrameParserStatus_t   FrameParser_Video_c::RevPlayRemoveReferenceFrameFromList( void )
{
bool            LastField;

//

    LastField   = (ParsedVideoParameters->PictureStructure == StructureFrame) || 
		  !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if( ReferenceFrameList.EntryCount != 0 )
    {
	Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex );

	if( LastField )
	    ReferenceFrameList.EntryCount--;
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to junk the reference frame list
//

FrameParserStatus_t   FrameParser_Video_c::RevPlayJunkReferenceFrameList( void )
{
    ReferenceFrameList.EntryCount       = 0;
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Private function to convert a us per frame 
//
//	We define matching as within 8us, since we can have errors of 
//	upto 5.5555555 us when trying to express a frame time as a 
//	PTS (90kHz clock) 
//

#define ValueMatchesFrameTime(V,T)   inrange( V, T - 8, T + 8 )

void   FrameParser_Video_c::DeduceFrameRateFromPresentationTime( 	long long int	  MicroSecondsPerFrame )
{
long long int	 CurrentMicroSecondsPerFrame;

    //
    // Are we ok with what we have got. 
    // If not, then we mark the rate as not standard, to force a recalculation, 
    // but not immediately, since this may be just a glitch.
    //

    if( ValidPTSDeducedFrameRate && StandardPTSDeducedFrameRate )
    {
	CurrentMicroSecondsPerFrame	= RoundedLongLongIntegerPart(1000000 / PTSDeducedFrameRate);

	if( !ValueMatchesFrameTime(MicroSecondsPerFrame, CurrentMicroSecondsPerFrame) )
	    StandardPTSDeducedFrameRate	= false;

	return;
    }

    //
    // Check against the standard values
    //

    ValidPTSDeducedFrameRate	= true;
    StandardPTSDeducedFrameRate	= true;

//

    if( ValueMatchesFrameTime(MicroSecondsPerFrame, 16667) )				// 60fps = 16666.67 us
    {
	PTSDeducedFrameRate	= Rational_t( 60, 1 );
    }
    else if( ValueMatchesFrameTime(MicroSecondsPerFrame, 16683) )			// 59fps = 16683.33 us
    {
	PTSDeducedFrameRate	= Rational_t( 60000, 1001 );
    }
    else if( ValueMatchesFrameTime(MicroSecondsPerFrame, 20000) )			// 50fps = 20000.00 us
    {
	PTSDeducedFrameRate	= Rational_t( 50, 1 );
    }
    else if( ValueMatchesFrameTime(MicroSecondsPerFrame, 33333) )			// 30fps = 33333.33 us
    {
	PTSDeducedFrameRate	= Rational_t( 30, 1 );
    }
    else if( ValueMatchesFrameTime(MicroSecondsPerFrame, 33367) )			// 29fps = 33366.67 us
    {
	PTSDeducedFrameRate	= Rational_t( 30000, 1001 );
    }
    else if( ValueMatchesFrameTime(MicroSecondsPerFrame, 40000) )			// 25fps = 40000.00 us
    {
	PTSDeducedFrameRate	= Rational_t( 25, 1 );
    }
    else if( ValueMatchesFrameTime(MicroSecondsPerFrame, 41667) )			// 24fps = 41666.67 us
    {
	PTSDeducedFrameRate	= Rational_t( 24, 1 );
    }
    else if( ValueMatchesFrameTime(MicroSecondsPerFrame, 41708) )			// 23fps = 41708.33 us
    {
	PTSDeducedFrameRate	= Rational_t( 24000, 1001 );
    }
    else if( PTSDeducedFrameRate != Rational_t( 1000000, MicroSecondsPerFrame ) )	// If it has changed since the last time
    {
	StandardPTSDeducedFrameRate	= false;
	PTSDeducedFrameRate		= Rational_t( 1000000, MicroSecondsPerFrame );
    }
											// If it was non standard, but has not changed since
											// last time we let it become the new standard.

//

    if( StandardPTSDeducedFrameRate )
    {
	if( PTSDeducedFrameRate != LastStandardPTSDeducedFrameRate )
	    report( severity_info, "DeduceFrameRateFromPresentationTime - Framerate = %d.%06d\n", PTSDeducedFrameRate.IntegerPart(), PTSDeducedFrameRate.RemainderDecimal() );

	LastStandardPTSDeducedFrameRate	= PTSDeducedFrameRate;
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Functions to load the anti emulation buffer
//

void   FrameParser_Video_c::LoadAntiEmulationBuffer( unsigned char  *Pointer )
{
    AntiEmulationSource         = Pointer;
    AntiEmulationContent        = 0;
    Bits.SetPointer( AntiEmulationBuffer );

#if 0
//#ifdef DUMP_HEADERS
    unsigned int        i,j; 

    report( severity_info, "LoadAntiEmulationBuffer\n" );
    for( i=0; i<256; i+=16 )
    {
	report( severity_info, "        " );
	for( j=0; j<16; j++ )
	    report( severity_info, "%02x ", Pointer[i+j] ); 
	report( severity_info, "\n" );
    }

    memset( AntiEmulationBuffer, 0xaa, ANTI_EMULATION_BUFFER_SIZE );
#endif
}


// --------


void FrameParser_Video_c::CheckAntiEmulationBuffer( unsigned int    Size )
{
unsigned int             i,j;
FrameParserStatus_t      Status;
unsigned int             UntouchedBytesInBuffer;
unsigned int             BitsInByte;
unsigned char           *NewBase;
unsigned char           *Pointer;

    //
    // Check that the request is supportable
    //

    if( Size > (ANTI_EMULATION_BUFFER_SIZE - 16) )
    {
	report( severity_info, "FrameParser_Video_c::CheckAntiEmulationBuffer - Buffer overflow, requesting %d - Implementation error.\n" );
	return;
    }

    //
    // Do we already have the required data, or do we need some more
    // if we need some more, preserve the unused ones that we have.
    //

    BitsInByte                  = 8;
    NewBase                     = AntiEmulationBuffer;
    UntouchedBytesInBuffer      = 0;

    if( AntiEmulationContent != 0 )
    {
	//
	// First let us check the status of the buffer
	//

	Status  = TestAntiEmulationBuffer();
	if( Status != FrameParserNoError )
	{
	    report( severity_error, "FrameParser_Video_c::CheckAntiEmulationBuffer - Already gone bad (%d).\n", Size );
	    return;
	}

	//
	// Can we satisfy the request without trying
	//

	Bits.GetPosition( &Pointer, &BitsInByte );
	UntouchedBytesInBuffer  = AntiEmulationContent - (Pointer - AntiEmulationBuffer) - (BitsInByte != 8);

	if( UntouchedBytesInBuffer >= Size )
	    return;

	//
	// Know how many bytes to save, do we need to shuffle down, or can we append
	//

	if( ((Pointer - AntiEmulationBuffer) + Size + 16) > ANTI_EMULATION_BUFFER_SIZE )
	{
	    AntiEmulationContent        = UntouchedBytesInBuffer + (BitsInByte != 8);

	    for( i=0; i<AntiEmulationContent; i++ )
		AntiEmulationBuffer[i]  = Pointer[i];

	    NewBase     = AntiEmulationBuffer;
	}
	else
	{
	    NewBase     = Pointer;
	}
    }

//

    for( i = UntouchedBytesInBuffer, j=0; 
	 i<(Size+3); 
	 i++, j++ )
	if( (AntiEmulationSource[j] == 0) &&
	    (AntiEmulationSource[j+1] == 0) &&
	    (AntiEmulationSource[j+2] == 3) )
	{
	    memcpy( AntiEmulationBuffer+AntiEmulationContent, AntiEmulationSource, j + 2 );
	    AntiEmulationContent        += j + 2;
	    AntiEmulationSource         += j + 3;
	    j                            = -1;
	}

    if( j != 0 )
    {
	memcpy( AntiEmulationBuffer+AntiEmulationContent, AntiEmulationSource, j );
	AntiEmulationContent    += j;
	AntiEmulationSource     += j;
    }

//

    Bits.SetPointer( NewBase );
    Bits.FlushUnseen( 8 - BitsInByte );

#if 0
//#ifdef DUMP_HEADERS
    report( severity_info, "CheckAntiEmulationBuffer(%d) - Content is %d, Untouched was %d\n", Size, AntiEmulationContent, UntouchedBytesInBuffer );
    for( i=0; i<AntiEmulationContent; i+=16 )
    {
	report( severity_info, "        " );
	for( j=0; j<16; j++ )
	    report( severity_info, "%02x ", AntiEmulationBuffer[i+j] ); 
	report( severity_info, "\n" );
    }
#endif
}


// --------


FrameParserStatus_t   FrameParser_Video_c::TestAntiEmulationBuffer( void )
{
unsigned int     BitsInByte;
unsigned char   *Pointer;
unsigned int     BytesUsed;

//

    Bits.GetPosition( &Pointer, &BitsInByte );

    BytesUsed   = (Pointer - AntiEmulationBuffer) + (BitsInByte != 8);
    if( (BytesUsed != 0) && (BytesUsed > AntiEmulationContent) )
    {
	report( severity_error, "FrameParser_Video_c::TestAntiEmulationBuffer - Anti emulation buffering failure (%d [%d,%d]) - Implementation error\n", 
		AntiEmulationContent - BytesUsed, BytesUsed, AntiEmulationContent );
	return FrameParserError;
    }

#if 0
    report( severity_info, "TEST %d %d\n",  BytesUsed, BitsInByte );
#endif

//
    return FrameParserNoError;
}

