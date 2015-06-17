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

Source file name : frame_parser_base.cpp
Author :           Nick

Implementation of the base frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
28-Nov-06   Created                                         Nick
18-Apr-07   Doxygenated after discussion with Nick          Daniel

************************************************************************/

// /////////////////////////////////////////////////////////////////////////
///
/// \class FrameParser_Base_c
/// \brief Framework to unify the approach to frame parsing.
///
/// This base class provided a framework on which to construct a frame parser.
/// Deriving from this class is, strictly speaking, optional, but nevertheless
/// is highly recommended. This most important framework provided by this class
/// is a sophisticated helper function, FrameParser_Base_c::ProcessBuffer, that
/// splits the complex process of managing incoming data (especially during
/// trick modes) into smaller more easily implemented pieces.

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define DEFAULT_DECODE_BUFFER_COUNT     8

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//
FrameParser_Base_c::FrameParser_Base_c( void )
{
    InitializationStatus        = FrameParserError;

//

    OS_InitializeMutex( &Lock );

    CodedFrameBufferPool	= NULL;
    FrameBufferCount		= 0;
    DecodeBufferPool		= NULL;
    DecodeBufferCount		= 0;
    OutputRing			= NULL;

    BufferManager		= NULL;

    StreamParametersDescriptor	= NULL;
    StreamParametersType	= NOT_SPECIFIED;
    StreamParametersPool	= NULL;
    StreamParametersBuffer	= NULL;

    FrameParametersDescriptor	= NULL;
    FrameParametersType		= NOT_SPECIFIED;
    FrameParametersPool		= NULL;
    FrameParametersBuffer	= NULL;

    Buffer			= NULL;
    BufferLength		= 0;
    BufferData			= NULL;
    CodedFrameParameters	= NULL;
    ParsedFrameParameters	= NULL;
    StartCodeList		= NULL;

    FirstDecodeAfterInputJump	= true;
    SurplusDataInjected		= false;
    ContinuousReverseJump	= false;

    NextDecodeFrameIndex	= 0;
    NextDisplayFrameIndex	= 0;

    NativeTimeBaseLine		= INVALID_TIME;
    LastNativeTimeUsedInBaseline= INVALID_TIME;

    FrameToDecode		= false;

    PlaybackSpeed		= 1;
    PlaybackDirection		= PlayForward;

    //
    // Fill out default values for the configuration record
    //

    memset( &Configuration, 0x00, sizeof(FrameParserConfiguration_t) );

    Configuration.FrameParserName		= "Unspecified";

    Configuration.StreamParametersCount		= 32;
    Configuration.StreamParametersDescriptor	= NULL;

    Configuration.FrameParametersCount		= 32;
    Configuration.FrameParametersDescriptor	= NULL;

    Configuration.MaxReferenceFrameCount	= 32;		// Only need limiting for specific codecs (IE h264)

    Configuration.SupportSmoothReversePlay	= false;
    Configuration.InitializeStartCodeList	= false;

//

    InitializationStatus        = FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

FrameParser_Base_c::~FrameParser_Base_c(        void )
{
    OS_TerminateMutex( &Lock );
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

FrameParserStatus_t   FrameParser_Base_c::Halt(         void )
{
    PurgeQueuedPostDecodeParameterSettings();

    return BaseComponentClass_c::Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variable
//

FrameParserStatus_t   FrameParser_Base_c::Reset(        void )
{
    NextDecodeFrameIndex                = 0;
    NextDisplayFrameIndex               = 0;

    NativeTimeBaseLine                  = 0;
    LastNativeTimeUsedInBaseline        = INVALID_TIME;

    FirstDecodeAfterInputJump           = true;
    SurplusDataInjected                 = false;
    ContinuousReverseJump               = false;

    StartCodeList 		      	= NULL;

//

    if( StreamParametersPool != NULL )
    {
	if( StreamParametersBuffer != NULL )
	{
	    StreamParametersBuffer->DecrementReferenceCount( IdentifierFrameParser );
	    StreamParametersBuffer	= NULL;
	}

	BufferManager->DestroyPool( StreamParametersPool );
	StreamParametersPool                    = NULL;
    }

    if( FrameParametersPool != NULL )
    {
	if( FrameParametersBuffer != NULL )
	{
	    FrameParametersBuffer->DecrementReferenceCount( IdentifierFrameParser );
	    FrameParametersBuffer	= NULL;
	}

	BufferManager->DestroyPool( FrameParametersPool );
	FrameParametersPool                     = NULL;
    }

//

    return BaseComponentClass_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function function
//

FrameParserStatus_t   FrameParser_Base_c::RegisterOutputBufferRing( Ring_t        Ring )
{
PlayerStatus_t  Status;

//

    OutputRing  = Ring;

    //
    // Obtain the class list, and the coded frame buffer pool
    //

    Player->GetCodedFrameBufferPool( Stream, &CodedFrameBufferPool, NULL );
    Player->GetDecodeBufferPool( Stream, &DecodeBufferPool );

    //
    // Obtain Buffer counts
    //

    CodedFrameBufferPool->GetPoolUsage( &FrameBufferCount, NULL, NULL, NULL, NULL );

    if( DecodeBufferPool != NULL )
	Manifestor->GetDecodeBufferCount( &DecodeBufferCount );
    else
	DecodeBufferCount       = DEFAULT_DECODE_BUFFER_COUNT;

    //
    // Attach the generic parsed frame parameters to every element of the pool.
    //

    Status      = CodedFrameBufferPool->AttachMetaData( Player->MetaDataParsedFrameParametersType );
    if( Status != BufferNoError )
    {
	report( severity_error, "FrameParser_Base_c::RegisterOutputBufferRing - Failed to attach parsed frame parameters to all coded frame buffers.\n" );
	SetComponentState(ComponentInError);
	return Status;
    }

    //
    // Now create the frame and stream parameter buffers
    //

    Status	= RegisterStreamAndFrameDescriptors();
    if( Status != FrameParserNoError )
    {
	report( severity_error, "FrameParser_Base_c::RegisterOutputBufferRing - Failed to register the parameter buffer types.\n" );
	SetComponentState(ComponentInError);
	return Status;
    }

//

    Status      = BufferManager->CreatePool( &StreamParametersPool, StreamParametersType, Configuration.StreamParametersCount );
    if( Status != BufferNoError )
    {
	report( severity_error, "FrameParser_Base_c::RegisterOutputBufferRing - Failed to create a pool of stream parameter buffers.\n" );
	SetComponentState(ComponentInError);
	return Status;
    }

    Status      = BufferManager->CreatePool( &FrameParametersPool, FrameParametersType, Configuration.FrameParametersCount );
    if( Status != BufferNoError )
    {
	report( severity_error, "FrameParser_Base_c::RegisterOutputBufferRing - Failed to create a pool of frame parameter buffers.\n" );
	SetComponentState(ComponentInError);
	return Status;
    }

//

    StreamParametersBuffer      = NULL;
    FrameParametersBuffer       = NULL;

    //
    // Go live
    //

//    report (severity_error, "Setting component state to running\n");  
    SetComponentState( ComponentRunning );

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The input function perform base operations
//

FrameParserStatus_t   FrameParser_Base_c::Input(        Buffer_t                  CodedBuffer )
{
FrameParserStatus_t       Status;

    //
    // Initialize context pointers
    //

    Buffer                              = NULL;
    BufferLength			= 0;
    BufferData                          = NULL;
    CodedFrameParameters                = NULL;
    ParsedFrameParameters               = NULL;
    StartCodeList 			= NULL;

    //
    // Obtain pointers to data associated with the buffer.
    //

    Buffer      = CodedBuffer;

    Status      = Buffer->ObtainDataReference( NULL, &BufferLength, (void **)(&BufferData) );
    if( (Status != PlayerNoError) && (Status != BufferNoDataAttached) )
    {
	report( severity_error, "FrameParser_Base_c::Input - Unable to obtain data reference.\n" );
	return Status;
    }

//

    Status      = Buffer->ObtainMetaDataReference( Player->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "FrameParser_Base_c::Input - Unable to obtain the meta data \"CodedFrameParameters\".\n" );
	return Status;
    }

//

    Status      = Buffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "FrameParser_Base_c::Input - Unable to obtain the meta data \"ParsedFrameParameters\".\n" );
	return Status;
    }

//

    if( Configuration.InitializeStartCodeList )
    {
	Status	= Buffer->ObtainMetaDataReference( Player->MetaDataStartCodeListType, (void **)(&StartCodeList) );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "FrameParser_Base_c::Input - Unable to obtain the meta data \"StartCodeList\".\n" );
	    return Status;
	}
    }

//

    memset( ParsedFrameParameters, 0x00, sizeof(ParsedFrameParameters_t) );
    ParsedFrameParameters->DecodeFrameIndex             = INVALID_INDEX;
    ParsedFrameParameters->DisplayFrameIndex            = INVALID_INDEX;
    ParsedFrameParameters->NativePlaybackTime           = INVALID_TIME;
    ParsedFrameParameters->NormalizedPlaybackTime       = INVALID_TIME;
    ParsedFrameParameters->NativeDecodeTime             = INVALID_TIME;
    ParsedFrameParameters->NormalizedDecodeTime         = INVALID_TIME;
    ParsedFrameParameters->IndependentFrame		= true;			// Default a frame to being independent
										// to allow video decoders to mark single 
										// I fields as non-independent.

    //
    // Gather any useful information about the policies in force
    //

    Player->GetPlaybackSpeed( Playback, &PlaybackSpeed, &PlaybackDirection );

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Translate times from a 90Khz native time to a microsecond
//      normalized time. We will move the implementation of this 
//      if we see any codecs using non-90Khz times.
//
//      In my arithmetic I pop briefly into a 27Mhz timebase, there 
//      is no real reason for this, it's just that historically our 
//      chips extended the 90Khz to a 27Mhz wossname.
//

FrameParserStatus_t   FrameParser_Base_c::TranslatePlaybackTimeNativeToNormalized(
							unsigned long long        NativeTime,
							unsigned long long       *NormalizedTime )
{
unsigned long long      WrapMask;
unsigned long long      WrapOffset;
unsigned long long 	LastSetNativeTime;

    //
    // There is a fudge here, native time is actually only 33 bits long,
    // whereas normalized time is 64 bits long. We maintain a normalized 
    // baseline that handles the affects of wrapping, which we use to 
    // adjust the calculated value. 
    //
    //

    if( LastNativeTimeUsedInBaseline == INVALID_TIME )
    {
	LastSetNativeTime		= Player->GetLastNativeTime( Playback );

	NativeTimeBaseLine		= ValidTime(LastSetNativeTime) ?
						((LastSetNativeTime - NativeTime + 0x0000000100000000ULL) & 0xfffffffe00000000ULL) : 
						0;

	LastNativeTimeUsedInBaseline    = NativeTime;
    }

    //
    // Adjust the baseline if there is any wrap. 
    // First check for forward wrap, then reverse wrap.
    //

    WrapMask    = 0x0000000180000000ULL;
    WrapOffset  = 0x0000000200000000ULL;

    if( ((LastNativeTimeUsedInBaseline & WrapMask) == WrapMask) &&
	((NativeTime                   & WrapMask) == 0) )
	NativeTimeBaseLine      += WrapOffset;

    else if( ((LastNativeTimeUsedInBaseline & WrapMask) == 0) &&
	     ((NativeTime                   & WrapMask) == WrapMask) )
	NativeTimeBaseLine      -= WrapOffset;

    LastNativeTimeUsedInBaseline        = NativeTime;

    //
    // Calculate the Normalized time
    //

    *NormalizedTime     = (((NativeTimeBaseLine + NativeTime) * 300) + 13) / 27;

    Player->SetLastNativeTime( Playback, (NativeTimeBaseLine + NativeTime) );
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Translate times from a 1 microsecond normalized time to a 90Khz 
//      native time, assumes that the native time is only 33 bits.
//
//      In my arithmetic I pop briefly into a 27Mhz timebase, there 
//      is no real reason for this, it's just that historically our 
//      chips extended the 90Khz to a 27Mhz wossname.
//

FrameParserStatus_t   FrameParser_Base_c::TranslatePlaybackTimeNormalizedToNative(
							unsigned long long        NormalizedTime, 
							unsigned long long       *NativeTime )
{
    *NativeTime = (((NormalizedTime * 27) + 150) / 300) & 0x00000001ffffffffULL;

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Apply a corrective baseline wrap.
//
//      A nasty situation occurs when the first native times for 
//      multiple streams have different wrap states, IE first video 
//      pts at 0x1ffffffff , first audio at 0x000000001. In this case
//      the video will wrap almost immediately, and the two native 
//      times will differ wildly. Only the output timer (external to
//      both streams) will know this has occurred, it can correct the 
//      situation by applying a corrective native time baseline wrap to
//      the non-wrapping native time (audio in the example). This 
//      function allows it to do this.
//

FrameParserStatus_t   FrameParser_Base_c::ApplyCorrectiveNativeTimeWrap( void )
{
    NativeTimeBaseLine  += 0x0000000200000000ULL;
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
///
///      \brief Standard processing chain for single input buffer
///
///      Process buffer forms the heart of the framework to assist with implementing a
///      frame parser. This is the method that will call the protected virtual methods
///      of this class.
///                               
///      \return Frame parser status code, FrameParserNoError indicates success.        
///
FrameParserStatus_t   FrameParser_Base_c::ProcessBuffer( void )
{
FrameParserStatus_t     Status;

    //
    // Handle discontinuity in input stream
    //

    if( CodedFrameParameters->StreamDiscontinuity )
    {
	PurgeQueuedPostDecodeParameterSettings();
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
	    Buffer->IncrementReferenceCount( IdentifierProcessParseToDecode );
	    OutputRing->Insert( (unsigned int )Buffer );
	}

	return FrameParserNoError;
    }

    //
    // Parse the headers
    //

    FrameToDecode                       = false;

    Status      = ReadHeaders();
    if( Status != FrameParserNoError )
	return Status;

    //
    // Do we still have something to do
    //

    if( !FrameToDecode )
	return FrameParserNoError;

    //
    // Can we generate any queued index and pts values based on what we have seen
    //

    Status      = ProcessQueuedPostDecodeParameterSettings();
    if( Status != FrameParserNoError )
	return Status;

    //
    // Calculate the display index/PTS values
    //

    Status      = GeneratePostDecodeParameterSettings();
    if( Status != FrameParserNoError )
	return Status;

    //
    // Queue the frame for decode
    //

    Status      = QueueFrameForDecode();
    if( Status != FrameParserNoError )
	return Status;

    FirstDecodeAfterInputJump           = false;
    SurplusDataInjected                 = false;
    ContinuousReverseJump               = false;

//
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
///
///      \brief Queue a frame for decode.
///
///      Take the currently active buffer, FrameParser_Base_c::Buffer and queue it for
///      decode.
///        
///      This implementation provided by the FrameParser_Base_c supports simple forward
///      play by taking the active buffer and placing it directly on the output ring,
///      FrameParser_Base_c::OutputRing
///        
///      During reverse play this method is much more complex. Typically buffers must be
///      dispatched to the output ring in a different order during reverse playback. This means
///      that the implementation must squirrel the buffers away (and record their original order)
///      until FrameParser_Base_c::ProcessReverseDecodeStack (and
///      FrameParser_Base_c::ProcessReverseDecodeUnsatisfiedReferenceStack) are called.
///                               
///      \return Frame parser status code, FrameParserNoError indicates success.        
///

FrameParserStatus_t   FrameParser_Base_c::QueueFrameForDecode( void )
{
unsigned int	i;

    //
    // Adjust the independent frame flag from it's default value, 
    // or the value set in the specific frame parser.
    //

    if( ParsedFrameParameters->IndependentFrame )
    {
	for( i=0; i<ParsedFrameParameters->NumberOfReferenceFrameLists; i++ )
	    if( ParsedFrameParameters->ReferenceFrameList[i].EntryCount != 0 )
	    {
		ParsedFrameParameters->IndependentFrame	= false;
		break;
	    }
    }

    //
    // Base implementation derives the decode index, 
    // then queues the frame on the output ring.
    //

    ParsedFrameParameters->DecodeFrameIndex     = NextDecodeFrameIndex++;

#if 0
    report( severity_info, "Q %d (F = %d, K = %d, I = %d, R = %d)\n",
                ParsedFrameParameters->DecodeFrameIndex,
                ParsedFrameParameters->FirstParsedParametersForOutputFrame,
                ParsedFrameParameters->KeyFrame,
                ParsedFrameParameters->IndependentFrame,
                ParsedFrameParameters->ReferenceFrame );
#endif

//

    Buffer->IncrementReferenceCount( IdentifierProcessParseToDecode );
    OutputRing->Insert( (unsigned int )Buffer );

//OS_SleepMilliSeconds( 4000 );

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
///
///     \brief Internal function to register the stream/frame parameter 
///            meta data descriptors with the buffer manager.
///
///     Metadata generated by the frame parser and attached to buffers can, itself,
///     managed by the buffer manager to ensure that its lifetime is properly managed.
///     This method is used to register the metadata types used by a specific
///     frame parser.
///                               
///     \return Frame parser status code, FrameParserNoError indicates success.

FrameParserStatus_t   FrameParser_Base_c::RegisterStreamAndFrameDescriptors( void )
{
PlayerStatus_t  Status;

//

    Player->GetBufferManager( &BufferManager );

//

    Status      = BufferManager->FindBufferDataType( Configuration.StreamParametersDescriptor->TypeName, &StreamParametersType );
    if( Status != BufferNoError )
    {
	Status  = BufferManager->CreateBufferDataType( Configuration.StreamParametersDescriptor, &StreamParametersType );
	if( Status != BufferNoError )
	{
	    report( severity_error, "FrameParser_Base_c::RegisterStreamAndFrameDescriptors - Failed to create the stream parameters buffer type.\n" );
	    return FrameParserError;
	}
    }

    BufferManager->GetDescriptor( StreamParametersType, BufferDataTypeBase, &StreamParametersDescriptor );

//

    Status      = BufferManager->FindBufferDataType( Configuration.FrameParametersDescriptor->TypeName, &FrameParametersType );
    if( Status != BufferNoError )
    {
	Status  = BufferManager->CreateBufferDataType( Configuration.FrameParametersDescriptor, &FrameParametersType );
	if( Status != BufferNoError )
	{
	    report( severity_error, "FrameParser_Base_c::RegisterFrameAndFrameDescriptors - Failed to create the Frame parameters buffer type.\n" );
	    return FrameParserError;
	}
    }

    BufferManager->GetDescriptor( FrameParametersType, BufferDataTypeBase, &FrameParametersDescriptor );

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
///
///     \brief Get a new stream parameters buffer.
///       
///     Allocate, initialize with zeros and return a pointer to a block of data in which to store the stream
///     parameters. As a side effect the existing buffer, referenced via
///     FrameParser_Base_c::StreamParametersBuffer,
///     will have its reference count decreased, potentially causing it to be freed. It is the responsibility
///     of derived classes to make a claim on the existing buffer if they must refer to it later (or pass
///     it to another component that must refer to it later.
///                               
///     \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t   FrameParser_Base_c::GetNewStreamParameters( void  **Pointer )
{
FrameParserStatus_t     Status;

//

    if( StreamParametersBuffer != NULL )
    {
	StreamParametersBuffer->DecrementReferenceCount( IdentifierFrameParser );
	StreamParametersBuffer  = NULL;
    }

//

    Status      = StreamParametersPool->GetBuffer( &StreamParametersBuffer, IdentifierFrameParser );
    if( Status != BufferNoError )
    {
	report( severity_error, "FrameParser_Base_c::GetNewStreamParameters - Failed to allocate a buffer for the stream parameters.\n" );
	return FrameParserFailedToAllocateBuffer;
    }

//

    Status      = StreamParametersBuffer->ObtainDataReference( NULL, NULL, Pointer );
    if( Status == BufferNoError )
	memset( *Pointer, 0x00, StreamParametersDescriptor->FixedSize );

//

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
///
///     \brief Get a new frame parameters buffer.
///       
///     Allocate, initialise with zeros and return a pointer to a block of data in which to store the
///     frane parameters.
///     As a side effect the existing buffer, referenced via FrameParser_Base_c::FrameParametersBuffer,
///     will have its reference count decreased, potentially causing it to be freed. It is the responsibility
///     of derived classes to make a claim on the existing buffer if they must refer to it later (or pass
///     it to another component that must refer to it later.
///                             
///     \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t   FrameParser_Base_c::GetNewFrameParameters( void   **Pointer )
{
FrameParserStatus_t     Status;

//

    if( FrameParametersBuffer != NULL )
    {
	FrameParametersBuffer->DecrementReferenceCount( IdentifierFrameParser );
	FrameParametersBuffer   = NULL;
    }

//

    Status      = FrameParametersPool->GetBuffer( &FrameParametersBuffer, IdentifierFrameParser );
    if( Status != BufferNoError )
    {
	report( severity_error, "FrameParser_Base_c::GetNewFrameParameters - Failed to allocate a buffer to hold frame parameters.\n" );
	return FrameParserFailedToAllocateBuffer;
    }

//

    Status      = FrameParametersBuffer->ObtainDataReference( NULL, NULL, Pointer );
    if( Status == BufferNoError )
	memset( *Pointer, 0x00, FrameParametersDescriptor->FixedSize );

//

    return Status;
}

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Base_c::ReadHeaders(                                      void )
///     \brief Attempt to parse the headers of the currently active frame.
///
///     Read headers is responsible for scrutonizing the header either by using
///     the start code list or by directly intergating ::BufferData and ::BufferLength.
///
///     If a valid frame is found this method must also set ::FrameToDecode to true.
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///       

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Base_c::ProcessQueuedPostDecodeParameterSettings( void )
///     \brief Patch up any post decode parameters that could not be determined during initial parsing.
///
///     In some cases it may not be possible to fully analyse the state of a frame (its post decode
///     parameters) during analysis. Examples include I-frames within video streams where the display index
///     cannot be determined until the B-frame to P-frame transition within the stream.
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///       

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Base_c::PurgeQueuedPostDecodeParameterSettings(   void )
///     \brief Purge the queue of post decode parameters that could not be determined during initial parsing.
///
///     In some cases it may not be possible to fully analyse the state of a frame (its post decode
///     parameters) during analysis. Examples include I-frames within video streams where the display index
///     cannot be determined until the B-frame to P-frame transition within the stream.
///     \return Frame parser status code, FrameParserNoError indicates success.
///       

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Base_c::GeneratePostDecodeParameterSettings(              void )
///     \brief Populate the post decode parameter block.
///     
///     Populate FrameParser_Base_c::ParsedFrameParameters with the frame parameters (as they will be after
///     decode has taken place).
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///       

