/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

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

Source file name : codec_mme_base.cpp
Author :           Nick

Implementation of the base codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
23-Jan-06   Created                                         Nick
11-Sep-07   Added Event/Count for stream output codecs      Adam

************************************************************************/

//#define DUMP_COMMANDS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "osdev_device.h"
#include "codec_mme_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      C wrapper for the MME callback
//

typedef void (*MME_GenericCallback_t) (MME_Event_t Event, MME_Command_t * CallbackData, void *UserData);

static void MMECallbackStub(    MME_Event_t      Event,
				MME_Command_t   *CallbackData,
				void            *UserData )
{
Codec_MmeBase_c         *Self = (Codec_MmeBase_c *)UserData;

    Self->CallbackFromMME( Event, CallbackData );
//    report (severity_error, "MME Callback !! \n");
    return;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//
Codec_MmeBase_c::Codec_MmeBase_c( void )
{
    InitializationStatus        = CodecError;

//

    OS_InitializeMutex( &Lock );

    DataTypesInitialized                = false;
    MMEInitialized                      = false;
    MMECommandPreparedCount             = 0;
    MMECommandAbortedCount              = 0;
    MMECommandCompletedCount            = 0;

    CodedFrameBufferPool                = NULL;

    StreamParameterContextPool          = NULL;
    DecodeContextPool                   = NULL;

    DecodeBufferPool                    = NULL;
    PostProcessControlBufferPool        = NULL;

    IndexBufferMapSize                  = 0;
    IndexBufferMap                      = NULL;

    MarkerBuffer                        = NULL;

    DiscardDecodesUntil                 = 0;

    SelectedTransformer                 = 0;
    ForceStreamParameterReload          = false;

    //
    // Fill out default values for the configuration record
    //

    memset( &Configuration, 0x00, sizeof(CodecConfiguration_t) );

    Configuration.CodecName                             = "Unspecified";

    strcpy( Configuration.TranscodedMemoryPartitionName, "BPA2_Region0" );
    strcpy( Configuration.AncillaryMemoryPartitionName, "BPA2_Region1" );

    Configuration.DecodeOutputFormat                    = FormatUnknown;

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = NULL;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = NULL;

    Configuration.AudioVideoDataParsedParametersType    = NOT_SPECIFIED;
    Configuration.AudioVideoDataParsedParametersPointer = NULL;
    Configuration.SizeOfAudioVideoDataParsedParameters  = NOT_SPECIFIED;

    Configuration.AvailableTransformers                 = 0;
    Configuration.SizeOfTransformCapabilityStructure    = 0;
    Configuration.TransformCapabilityStructurePointer   = NULL;

    Configuration.AddressingMode                        = PhysicalAddress;

    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;
    Configuration.IgnoreFindCodedDataBuffer             = false;

    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration	= 1024;
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration	= 1024;
    Configuration.TrickModeParameters.SubstandardDecodeSupported     			= false;
    Configuration.TrickModeParameters.SubstandardDecodeRateIncrease     		= 1;
    Configuration.TrickModeParameters.DefaultGroupSize                  		= 1;
    Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount   		= 0;

    Configuration.SliceDecodePermitted                  = false;

//

    InitializationStatus        = CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

Codec_MmeBase_c::~Codec_MmeBase_c(      void )
{
    OS_TerminateMutex( &Lock );
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//
//      NOTE for some calls we ignore the return statuses, this is because
//      we will procede with the halt even if we fail (what else can we do)
//

CodecStatus_t   Codec_MmeBase_c::Halt(  void )
{
Buffer_t LocalMarkerBuffer;

    if( TestComponentState(ComponentRunning) )
    {
	//
	// Move the base state to halted early, to ensure 
	// no activity can be queued once we start halting
	//

	BaseComponentClass_c::Halt();

	//
	// Terminate any partially accumulated buffers
	//

	OutputPartialDecodeBuffers();

	//
	// Terminate the MME transform, this will involve waiting
	// for all the currently queued transactions to complete. 
	//

	TerminateMMETransformer();

	//
	// Pass on any marker buffer
	//

	LocalMarkerBuffer = TakeMarkerBuffer();
	if( LocalMarkerBuffer != NULL )
	    MarkerBuffer        = NULL;

	//
	// Lose the output ring
	//

	DecodeBufferPool                = NULL;
	CodedFrameBufferPool            = NULL;
	PostProcessControlBufferPool    = NULL;
	OutputRing                      = NULL;
    }

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variables
//

CodecStatus_t   Codec_MmeBase_c::Reset( void )
{
    //
    // Delete the decode and stream parameter contexts
    //

    if( DecodeContextPool != NULL )
    {
	BufferManager->DestroyPool( DecodeContextPool );
	DecodeContextPool               = NULL;
    }

    if( StreamParameterContextPool != NULL )
    {
	BufferManager->DestroyPool( StreamParameterContextPool );
	StreamParameterContextPool      = NULL;
    }

    DecodeContextBuffer                 = NULL;
    StreamParameterContextBuffer        = NULL;

    //
    // Free the indexing map
    //

    if( IndexBufferMap != NULL )
    {
	delete IndexBufferMap;
	IndexBufferMapSize              = 0;
	IndexBufferMap                  = NULL;
    }

    //
    // Initialize other variables
    //

    DecodeBufferCount           = 0;

    CodedFrameBuffer            = NULL;
    CodedDataLength             = 0;
    CodedData                   = NULL;
    ParsedFrameParameters       = NULL;

    memset( BufferState, 0x00, CODEC_MAX_DECODE_BUFFERS * sizeof(CodecBufferState_t) );

    CurrentDecodeBufferIndex    = INVALID_INDEX;
    CurrentDecodeBuffer         = NULL;
    CurrentDecodeIndex          = INVALID_INDEX;

//      report (severity_error,"Current Decode Index reset to %d\n",CurrentDecodeIndex);

    StreamParameterContext      = NULL;
    DecodeContext               = NULL;

    SelectedTransformer         = 0;
    ForceStreamParameterReload  = false;

//

    DecodeTimeShortIntegrationPeriod	= 0;
    DecodeTimeLongIntegrationPeriod	= 0;
    NextDecodeTime			= 0;
    LastDecodeCompletionTime		= INVALID_TIME;
    memset( DecodeTimes, 0x00, 16 * CODEC_MAX_DECODE_BUFFERS * sizeof(unsigned long long) );
    ShortTotalDecodeTime		= 0;
    LongTotalDecodeTime			= 0;

//

    return BaseComponentClass_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      SetModuleParameters function
//      Action  : Allows external user to set up important environmental parameters
//      Input   :
//      Output  :
//      Result  :
//

CodecStatus_t   Codec_MmeBase_c::SetModuleParameters(
						unsigned int   ParameterBlockSize,
						void          *ParameterBlock )
{
    struct CodecParameterBlock_s*       CodecParameterBlock = (struct CodecParameterBlock_s*)ParameterBlock;


    if (ParameterBlockSize != sizeof(struct CodecParameterBlock_s))
    {
	report( severity_error, "Codec_MmeBase_c::SetModuleParameters: Invalid parameter block.\n");
	return CodecError;
    }

    switch (CodecParameterBlock->ParameterType)
    {
	case CodecSelectTransformer:
	    if (CodecParameterBlock->Transformer >= Configuration.AvailableTransformers)        // TODO ask transformers
	    {
		report( severity_error, "Codec_MmeBase_c::SetModuleParameters - Invalid transformer id (%d >= %d).\n", CodecParameterBlock->Transformer, Configuration.AvailableTransformers);
		return CodecError;
	    }
	    SelectedTransformer         = CodecParameterBlock->Transformer;
	    report( severity_info, "Codec_MmeBase_c::SetModuleParameters - Setting selected transformer to %d\n", SelectedTransformer );
	    break;

	case CodecSpecifyTranscodedMemoryPartition:
	    strcpy( Configuration.TranscodedMemoryPartitionName, CodecParameterBlock->PartitionName );
	    report( severity_info, "Codec_MmeBase_c::SetModuleParameters - Specified ancillary memory partition '%s'.\n", CodecParameterBlock->PartitionName );
	    break;

	case CodecSpecifyAncillaryMemoryPartition:
	    strcpy( Configuration.AncillaryMemoryPartitionName, CodecParameterBlock->PartitionName );
	    report( severity_info, "Codec_MmeBase_c::SetModuleParameters - Specified ancillary memory partition '%s'.\n", CodecParameterBlock->PartitionName );
	    break;

	default:
	    report( severity_error, "Codec_MmeBase_c::SetModuleParameters: Unrecognised parameter block (%d).\n", CodecParameterBlock->ParameterType);
	    return CodecError;
    }

    return  CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function function
//

CodecStatus_t   Codec_MmeBase_c::GetTrickModeParameters( CodecTrickModeParameters_t     *TrickModeParameters )
{
    memcpy( TrickModeParameters, &Configuration.TrickModeParameters, sizeof(CodecTrickModeParameters_t) );
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function function
//

CodecStatus_t   Codec_MmeBase_c::RegisterOutputBufferRing( Ring_t         Ring )
{
PlayerStatus_t          Status;

//

    OutputRing  = Ring;

    //
    // Obtain the class list
    //

    Player->GetBufferManager( &BufferManager );
    if( Manifestor == NULL )
    {
	report( severity_error, "Codec_MmeBase_c::GetCodedFrameBufferPool(%s) - This implementation does not support no-output decoding.\n", Configuration.CodecName );
	return PlayerNotSupported;
    }

    //
    // Initialize the data types we use.
    //

    Status = InitializeDataTypes();
    if( Status != CodecNoError )
	return Status;

    //
    // Force the allocation of the coded frame buffers
    //

    Status = Player->GetCodedFrameBufferPool( Stream, &CodedFrameBufferPool );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Codec_MmeBase_c::RegisterOutputBufferRing(%s) - Failed to get coded frame buffer pool.\n", Configuration.CodecName );
	SetComponentState(ComponentInError);
	return PlayerError;
    }

    CodedFrameBufferPool->GetType( &CodedFrameBufferType );

    //
    // Obtain the decode buffer pool
    //

    Player->GetDecodeBufferPool( Stream, &DecodeBufferPool );
    if( DecodeBufferPool == NULL )
    {
	report( severity_error, "Codec_MmeBase_c::RegisterOutputBufferRing(%s) - This implementation does not support no-output decoding.\n", Configuration.CodecName );
	SetComponentState(ComponentInError);
	return PlayerNotSupported;
    }

    DecodeBufferPool->GetPoolUsage( &DecodeBufferCount, NULL, NULL, NULL, NULL );

    //
    // Obtain the post processing control buffer pool
    //

    Player->GetPostProcessControlBufferPool( Stream, &PostProcessControlBufferPool );
    if( PostProcessControlBufferPool == NULL )
    {
	report( severity_error, "Codec_MmeBase_c::RegisterOutputBufferRing(%s) - This implementation does not support no-output decoding.\n", Configuration.CodecName );
	SetComponentState(ComponentInError);
	return PlayerNotSupported;
    }

    //
    // Create the mapping between decode indices and decode buffers
    //

    IndexBufferMapSize  = DecodeBufferCount * Configuration.MaxDecodeIndicesPerBuffer;
    IndexBufferMap      = new CodecIndexBufferMap_t[IndexBufferMapSize];
    if( IndexBufferMap == NULL )
    {
	report( severity_error, "Codec_MmeBase_c::RegisterOutputBufferRing(%s) - Failed to allocate DecodeIndex <=> Buffer map.\n", Configuration.CodecName );
	SetComponentState(ComponentInError);
	return PlayerInsufficientMemory;
    }

    memset( IndexBufferMap, 0xff, IndexBufferMapSize * sizeof(CodecIndexBufferMap_t) );

    //
    // Attach the stream specific (audio|video|data) 
    // parsed frame parameters to the decode buffer pool.
    //

    Status      = DecodeBufferPool->AttachMetaData( Configuration.AudioVideoDataParsedParametersType );
    if( Status != BufferNoError )
    {
	report( severity_error, "Codec_MmeBase_c::RegisterOutputBufferRing(%s) - Failed to attach stream specific parsed parameters to all decode buffers.\n", Configuration.CodecName );
	SetComponentState(ComponentInError);
	return Status;
    }

    //
    // Create the stream parameters context buffers
    //

    Status      = BufferManager->CreatePool( &StreamParameterContextPool, StreamParameterContextType, Configuration.StreamParameterContextCount );
    if( Status != BufferNoError )
    {
	report( severity_error, "Codec_MmeBase_c::RegisterOutputBufferRing(%s) - Failed to create a pool of stream parameter context buffers.\n", Configuration.CodecName );
	SetComponentState(ComponentInError);
	return Status;
    }

    StreamParameterContextBuffer        = NULL;

    //
    // Now create the decode context buffers
    //

    Status      = BufferManager->CreatePool( &DecodeContextPool, DecodeContextType, Configuration.DecodeContextCount );
    if( Status != BufferNoError )
    {
	report( severity_error, "Codec_MmeBase_c::RegisterOutputBufferRing(%s) - Failed to create a pool of decode context buffers.\n", Configuration.CodecName );
	SetComponentState(ComponentInError);
	return Status;
    }

    DecodeContextBuffer         = NULL;

    //
    // Start the mme transformer
    //

    Status      = InitializeMMETransformer();
    if( Status != CodecNoError )
    {
	report( severity_error, "Codec_MmeBase_c::RegisterOutputBufferRing(%s) - Failed to start the mme transformer.\n", Configuration.CodecName );
	SetComponentState(ComponentInError);
	return Status;
    }

    // NOTE: the implementation taken is the one in codec mme audio spdif class!
    Status      = CreateAttributeEvents ();
    if( Status != CodecNoError )
    {
	report( severity_error, "Codec_MmeBase_c::RegisterOutputBufferRing(%s) - Failed to create attribute events.\n", Configuration.CodecName );
	SetComponentState(ComponentInError);
	return Status;
    }

    //
    // Go live
    //

    SetComponentState( ComponentRunning );

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Release any partially decoded buffers
//

CodecStatus_t   Codec_MmeBase_c::OutputPartialDecodeBuffers( void )
{
    if( CurrentDecodeBufferIndex != INVALID_INDEX )
    {
	// Operation cannot fail
	SetOutputOnDecodesComplete( CurrentDecodeBufferIndex, true );
	CurrentDecodeBufferIndex        = INVALID_INDEX;
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Set the discard pointer to discard any queued decodes
//

CodecStatus_t   Codec_MmeBase_c::DiscardQueuedDecodes( void )
{
    DiscardDecodesUntil = MMECommandPreparedCount;

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Release a reference frame
//

CodecStatus_t   Codec_MmeBase_c::ReleaseReferenceFrame( unsigned int    ReferenceFrameDecodeIndex )
{
unsigned int    i;
unsigned int    Index;
CodecStatus_t   Status;

    //
    // Deal with the generic case (release all)
    //

    if( ReferenceFrameDecodeIndex == CODEC_RELEASE_ALL )
    {
	for( i=0; i<DecodeBufferCount; i++ )
	{
	    if( BufferState[i].ReferenceFrameCount != 0 )
	    {
	    	if( BufferState[i].MacroblockStructurePresent )
		    BufferState[i].BufferMacroblockStructure->DecrementReferenceCount();

		while( BufferState[i].ReferenceFrameCount != 0 )
	    	{
		    BufferState[i].ReferenceFrameCount--;
		    DecrementReferenceCount( i );
	    	}
	    }
	}
    }

    //
    // Deal with the specific case
    //

    else
    {
	Status  = TranslateDecodeIndex( ReferenceFrameDecodeIndex, &Index );
	if( (Status != CodecNoError) || (BufferState[Index].ReferenceFrameCount == 0) )
	{
#if 0
	    //
	    // It is legal to receive a release command for a frame we have never seen
	    // this is because a frame may be discarded or dropped, between the frame 
	    // parser and us.
	    //
	    report( severity_error, "Codec_MmeBase_c::ReleaseReferenceFrame(%s) - Not a reference frame.\n", Configuration.CodecName );
#endif
	    return CodecUnknownFrame;
	}

	BufferState[Index].ReferenceFrameCount--;

	if( (BufferState[Index].ReferenceFrameCount == 0) && BufferState[Index].MacroblockStructurePresent )
		BufferState[Index].BufferMacroblockStructure->DecrementReferenceCount();

	DecrementReferenceCount( Index );
    }

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Check that we have all of the reference frames mentioned 
//      in a reference frame list.
//

CodecStatus_t   Codec_MmeBase_c::CheckReferenceFrameList(
						unsigned int              NumberOfReferenceFrameLists,
						ReferenceFrameList_t      ReferenceFrameList[] )
{
unsigned int      i,j;
unsigned int      BufferIndex;
CodecStatus_t     Status;

//

    for( i=0; i<NumberOfReferenceFrameLists; i++ )
    {
	for( j=0; j<ReferenceFrameList[i].EntryCount; j++ )
	{
	    Status      = TranslateDecodeIndex( ReferenceFrameList[i].EntryIndicies[j], &BufferIndex );
	    if( Status != CodecNoError )
		return CodecUnknownFrame;
	}
    }

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Release a decode buffer
//

CodecStatus_t   Codec_MmeBase_c::ReleaseDecodeBuffer(   Buffer_t          Buffer )
{
unsigned int    Index;
CodecStatus_t	Status;

//

    Buffer->GetIndex( &Index );
    Status	= DecrementReferenceCount( Index );

    //
    // If the mapping was no longer extant, then we just release the buffer
    //

    if( Status != CodecNoError )
	Buffer->DecrementReferenceCount();

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The get coded frame buffer pool fn
//

CodecStatus_t   Codec_MmeBase_c::Input(         Buffer_t          CodedBuffer )
{
CodecStatus_t             Status;
unsigned int              StreamParameterContextSize;
unsigned int              DecodeContextSize;
Buffer_t                  OldMarkerBuffer;
Buffer_t                  LocalMarkerBuffer;
BufferStructure_t         BufferStructure;

    //
    // Initialize context pointers
    //

    CodedFrameBuffer                                            = NULL;
    CodedData                                                   = NULL;
    CodedDataLength                                             = 0;
    ParsedFrameParameters                                       = NULL;
    *Configuration.AudioVideoDataParsedParametersPointer        = NULL;

    //
    // Obtain pointers to data associated with the buffer.
    //

    CodedFrameBuffer    = CodedBuffer;

    if( !Configuration.IgnoreFindCodedDataBuffer )
    {
	CodedDataLength		= 0;
	CodedData		= NULL;

	Status      = CodedFrameBuffer->ObtainDataReference( NULL, &CodedDataLength, (void **)(&CodedData), Configuration.AddressingMode );
	if( (Status != PlayerNoError) && (Status != BufferNoDataAttached) )
	{
	    report( severity_error, "Codec_MmeBase_c::Input(%s) - Unable to obtain data reference.\n", Configuration.CodecName );
	    return Status;
	}
    }

//

    Status      = CodedFrameBuffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Codec_MmeBase_c::Input(%s) - Unable to obtain the meta data \"ParsedFrameParameters\".\n", Configuration.CodecName );
	return Status;
    }

//

    Status      = CodedFrameBuffer->ObtainMetaDataReference( Configuration.AudioVideoDataParsedParametersType, Configuration.AudioVideoDataParsedParametersPointer );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Codec_MmeBase_c::Input(%s) - Unable to obtain the meta data \"Parsed[Audio|Video|Data]Parameters\".\n", Configuration.CodecName );
	return Status;
    }

    //
    // Handle the special case of a marker frame
    //

    if( (CodedDataLength == 0) && !ParsedFrameParameters->NewStreamParameters && !ParsedFrameParameters->NewFrameParameters )
    {
	//
	// Test we don't already have one
	//

	OldMarkerBuffer = TakeMarkerBuffer();
	if( OldMarkerBuffer != NULL )
	{
	    report( severity_error, "Codec_MmeBase_c::Input(%s) - Marker frame recognized when holding a marker frame already.\n", Configuration.CodecName );

	    OldMarkerBuffer->DecrementReferenceCount( IdentifierCodec );
	}

	//
	// Get a marker buffer
	//

	memset( &BufferStructure, 0x00, sizeof(BufferStructure_t) );
	BufferStructure.Format  = FormatMarkerFrame;

	Status      = Manifestor->GetDecodeBuffer( &BufferStructure, &LocalMarkerBuffer );
	if( Status != ManifestorNoError )
	{
	    report( severity_error, "Codec_MmeBase_c::Input(%s) - Failed to get marker decode buffer from manifestor.\n", Configuration.CodecName );
	    return Status;
	}

	LocalMarkerBuffer->TransferOwnership( IdentifierCodec );

	Status      = LocalMarkerBuffer->AttachMetaData( Player->MetaDataParsedFrameParametersReferenceType, UNSPECIFIED_SIZE, (void *)ParsedFrameParameters );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Codec_MmeBase_c::Input(%s) - Unable to attach a reference to \"ParsedFrameParameters\" to the marker buffer.\n", Configuration.CodecName );
	    return Status;
	}

	LocalMarkerBuffer->AttachBuffer( CodedFrameBuffer );

	//
	// Queue/pass on the buffer
	//      

	PassOnMarkerBufferAt    = MMECommandPreparedCount;
	MarkerBuffer            = LocalMarkerBuffer;
	TestMarkerFramePassOn();

	return CodecNoError;
    }

    //
    // Adjust the coded data to take account of the offset
    //

    if( !Configuration.IgnoreFindCodedDataBuffer )
    {
	CodedData               += ParsedFrameParameters->DataOffset;
	CodedDataLength         -= ParsedFrameParameters->DataOffset;
    }

    //
    // Check that the decode index is monotonicaly increasing
    //

    if( (CurrentDecodeIndex != INVALID_INDEX) &&
	((ParsedFrameParameters->DecodeFrameIndex < CurrentDecodeIndex) ||
	(!Configuration.SliceDecodePermitted && (ParsedFrameParameters->DecodeFrameIndex == CurrentDecodeIndex))) )
    {
	report( severity_error, "Codec_MmeBase_c::Input(%s) - Decode indices must be monotonicaly increasing (%d vs %d).\n", Configuration.CodecName, ParsedFrameParameters->DecodeFrameIndex, CurrentDecodeIndex );
	return PlayerImplementationError;
    }

    //
    // If new stream parameters, the obtain a stream parameters context
    //

    if( ParsedFrameParameters->NewStreamParameters || ForceStreamParameterReload )
    {
	ParsedFrameParameters->NewStreamParameters = true;
	ForceStreamParameterReload      = false;

	if( StreamParameterContextBuffer != NULL )
	{
	    report( severity_error, "Codec_MmeBase_c::Input(%s) - We already have a stream parameter context.\n", Configuration.CodecName );
	}
	else
	{
	    Status      = StreamParameterContextPool->GetBuffer( &StreamParameterContextBuffer );
	    if( Status != BufferNoError )
	    {
		report( severity_error, "Codec_MmeBase_c::Input(%s) - Failed to get stream parameter context.\n", Configuration.CodecName );
		return Status;
	    }

	    StreamParameterContextBuffer->ObtainDataReference( &StreamParameterContextSize, NULL, (void **)&StreamParameterContext );
	    memset( StreamParameterContext, 0x00, StreamParameterContextSize );

	    StreamParameterContext->StreamParameterContextBuffer        = StreamParameterContextBuffer;
	}
    }

    //
    // If a new frame is present obtain a decode context
    //

    if( ParsedFrameParameters->NewFrameParameters )
    {
	if( DecodeContextBuffer != NULL )
	{
	    report( severity_error, "Codec_MmeBase_c::Input(%s) - We already have a decode context.\n", Configuration.CodecName );
	}
	else
	{
	    Status      = DecodeContextPool->GetBuffer( &DecodeContextBuffer );
	    if( Status != BufferNoError )
	    {
		report( severity_error, "Codec_MmeBase_c::Input(%s) - Failed to get decode context.\n", Configuration.CodecName );
		return Status;
	    }

	    DecodeContextBuffer->ObtainDataReference( &DecodeContextSize, NULL, (void **)&DecodeContext );
	    memset( DecodeContext, 0x00, DecodeContextSize );
	    DecodeContext->BufferIndex = INVALID_INDEX;

	    DecodeContext->DecodeContextBuffer  = DecodeContextBuffer;
	}
    }

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - The mapping/translation functions for decode indices
//

CodecStatus_t   Codec_MmeBase_c::MapBufferToDecodeIndex(
						unsigned int      DecodeIndex,
						unsigned int      BufferIndex )
{
unsigned int    i;

   for( i=0; i<IndexBufferMapSize; i++ )
	if( IndexBufferMap[i].DecodeIndex == INVALID_INDEX )
	{
	    IndexBufferMap[i].DecodeIndex       = DecodeIndex;
	    IndexBufferMap[i].BufferIndex       = BufferIndex;
	    return CodecNoError;
	}

//

    report( severity_error, "Codec_MmeBase_c::MapBufferToDecodeIndex(%s) - Map table full, implementation error.\n", Configuration.CodecName );
    return PlayerImplementationError;
}

// ---------------

CodecStatus_t   Codec_MmeBase_c::UnMapBufferIndex(      unsigned int      BufferIndex )
{
unsigned int    i;

    //
    // Do not break out on finding the buffer, because it is perfectly legal 
    // to have more than one decode index associated with a buffer.
    //

    for( i=0; i<IndexBufferMapSize; i++ )
	if( IndexBufferMap[i].BufferIndex == BufferIndex )
	{
	    IndexBufferMap[i].DecodeIndex       = INVALID_INDEX;
	    IndexBufferMap[i].BufferIndex       = INVALID_INDEX;
	}

    return CodecNoError;
}

// ---------------


CodecStatus_t   Codec_MmeBase_c::TranslateDecodeIndex(
						unsigned int      DecodeIndex,
						unsigned int     *BufferIndex )
{
unsigned int    i;      

    for( i=0; i<IndexBufferMapSize; i++ )
	if( IndexBufferMap[i].DecodeIndex == DecodeIndex )
	{
	    *BufferIndex        = IndexBufferMap[i].BufferIndex;
//          report (severity_error, "Converted %d into %d\n",DecodeIndex, IndexBufferMap[i].BufferIndex);
	    return CodecNoError;
	}

//

    *BufferIndex        = INVALID_INDEX;

//    report(severity_error,"Opps! Unknown decode index %d\n", DecodeIndex);    

    return CodecUnknownFrame;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function control moving of buffers to output ring, sets the flag
//      indicating there will be no further decodes into this buffer,
//      and optionally checks to see if it should be immediately moved
//      to the output ring.
//

CodecStatus_t   Codec_MmeBase_c::SetOutputOnDecodesComplete(
						unsigned int      BufferIndex,
						bool              TestForImmediateOutput )
{
    if( !TestForImmediateOutput )
    {
	BufferState[BufferIndex].OutputOnDecodesComplete        = true;
    }
    else
    {
	OS_LockMutex( &Lock );

	BufferState[BufferIndex].OutputOnDecodesComplete        = true;
	if( BufferState[BufferIndex].DecodesInProgress == 0 )
	    OutputRing->Insert( (unsigned int )BufferState[BufferIndex].Buffer );

	OS_UnLockMutex( &Lock );
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to obtain a new decode buffer.
//

CodecStatus_t   Codec_MmeBase_c::GetDecodeBuffer( void )
{
PlayerStatus_t           Status;
BufferStructure_t        BufferStructure;

    //
    // Get a buffer
    //

    Status      = FillOutDecodeBufferRequest( &BufferStructure );
    if( Status != CodecNoError )
    {
	report( severity_error, "Codec_MmeBase_c::GetDecodeBuffer(%s) - Failed to fill out a buffer request structure.\n", Configuration.CodecName );
	return Status;
    }

    Status      = Manifestor->GetDecodeBuffer( &BufferStructure, &CurrentDecodeBuffer );
    if( Status != BufferNoError )
    {
	report( severity_error, "Codec_MmeBase_c::GetDecodeBuffer(%s) - Failed to obtain a decode buffer from the manifestor.\n", Configuration.CodecName );
	return Status;
    }

    CurrentDecodeBuffer->TransferOwnership( IdentifierCodec );

    //
    // Map it and initialize the mapped entry.
    //

    CurrentDecodeBuffer->GetIndex( &CurrentDecodeBufferIndex );

    if( CurrentDecodeBufferIndex >= CODEC_MAX_DECODE_BUFFERS )
	report( severity_fatal, "Codec_MmeBase_c::GetDecodeBuffer(%s) - Decode buffer index >= CODEC_MAX_DECODE_BUFFERS - Implementation error.\n" );

    Status      = MapBufferToDecodeIndex( ParsedFrameParameters->DecodeFrameIndex, CurrentDecodeBufferIndex );
    if( Status != CodecNoError )
	return Status;

    memset( &BufferState[CurrentDecodeBufferIndex], 0x00, sizeof(CodecBufferState_t) );

    BufferState[CurrentDecodeBufferIndex].Buffer                        = CurrentDecodeBuffer;
    BufferState[CurrentDecodeBufferIndex].OutputOnDecodesComplete       = false;
    BufferState[CurrentDecodeBufferIndex].DecodesInProgress             = 0;

    //
    // Obtain the interesting references to the buffer
    //

    CurrentDecodeBuffer->ObtainDataReference( &BufferState[CurrentDecodeBufferIndex].BufferLength,
					      NULL,
					      (void **)(&BufferState[CurrentDecodeBufferIndex].BufferPointer),
					      Configuration.AddressingMode );

    Status      = CurrentDecodeBuffer->AttachMetaData( Player->MetaDataParsedFrameParametersReferenceType, UNSPECIFIED_SIZE, (void *)ParsedFrameParameters );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Codec_MmeBase_c::GetDecodeBuffer(%s) - Unable to attach a reference to \"ParsedFrameParameters\" to the decode buffer.\n", Configuration.CodecName );
	return Status;
    }

    Status      = CurrentDecodeBuffer->ObtainMetaDataReference( Configuration.AudioVideoDataParsedParametersType, (void **)(&BufferState[CurrentDecodeBufferIndex].AudioVideoDataParsedParameters) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Codec_MmeBase_c::GetDecodeBuffer(%s) - Unable to obtain the meta data \"Parsed[Audio|Video|Data]Parameters\".\n", Configuration.CodecName );
	return Status;
    }

    Status      = CurrentDecodeBuffer->ObtainMetaDataReference( Player->MetaDataBufferStructureType, (void **)(&BufferState[CurrentDecodeBufferIndex].BufferStructure) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Codec_MmeBase_c::GetDecodeBuffer(%s) - Unable to obtain the meta data \"BufferStructure\".\n", Configuration.CodecName );
	return Status;
    }

    //
    // We attached a reference to the parsed frame parameters for the first coded frame
    // We actually copy over the parsed stream specific parameters, because we modify these
    // as several decodes affect one buffer.
    // After doing the memcpy we attach the coded frame to to the decode
    // buffer, this extends the lifetime of any data pointed to in the 
    // parsed parameters to the lifetime of this decode buffer.
    //
    // We do the attachment because we have copied pointers to the original 
    // (mpeg2/H264/mp3...) headers, and we wish these pointers to be valid so 
    // long as the decode buffer is around.
    //

    memcpy( BufferState[CurrentDecodeBufferIndex].AudioVideoDataParsedParameters, *Configuration.AudioVideoDataParsedParametersPointer, Configuration.SizeOfAudioVideoDataParsedParameters );

    CurrentDecodeBuffer->AttachBuffer( CodedFrameBuffer );

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to send a set stream parameters
//

CodecStatus_t   Codec_MmeBase_c::ReleaseDecodeContext(          CodecBaseDecodeContext_t *Context )
{
unsigned int    i,j;
CodecStatus_t   Status;
Buffer_t        AttachedCodedDataBuffer;

    //
    // If the decodes in progress flag has already been 
    // incremented we decrement it and test for buffer release
    //

    if( Context->DecodeInProgress )
    {
	OS_LockMutex( &Lock );

	Context->DecodeInProgress     = false;

	BufferState[Context->BufferIndex].DecodesInProgress--;
	if( BufferState[Context->BufferIndex].OutputOnDecodesComplete )
	{
	    if( BufferState[Context->BufferIndex].DecodesInProgress == 0 )
	    {
		//
		// Buffer is complete, check for discard or place on output ring
		//

		if( (MMECommandAbortedCount + MMECommandCompletedCount) < DiscardDecodesUntil )
		{
		    OS_UnLockMutex( &Lock );
		    ReleaseDecodeBuffer( BufferState[Context->BufferIndex].Buffer );
		    OS_LockMutex( &Lock );
		}
		else
		{
#if 0
if( Configuration.DecodeOutputFormat == FormatVideo420_MacroBlock )
{
unsigned int       i,j;
unsigned int       Length;
unsigned char     *Pointer;
unsigned int       w,h,hw;

    BufferState[Context->BufferIndex].Buffer->ObtainDataReference( &Length, NULL, (void **)(&Pointer), UnCachedAddress );

    w   = BufferState[Context->BufferIndex].BufferStructure->Dimension[0];
    h   = BufferState[Context->BufferIndex].BufferStructure->Dimension[1];
    hw  = w/2;

    for( i=0; i<w; i++ )
    {
	for( j=0; j<32; j++ )
	    Pointer[i + (j * w)]        = (i==0) ? (j + 0x30) : ((i & 0x7f) + 0x40);
    }

    for( i=0; i<hw; i++ )
    {
	for( j=0; j<16; j++ )
	{
	    Pointer[i + (j * hw) + BufferState[Context->BufferIndex].BufferStructure->ComponentOffset[1]]                       = 0x60 + (i & 0x3f);
	    Pointer[i + (j * hw) + BufferState[Context->BufferIndex].BufferStructure->ComponentOffset[1] + ((w * h) / 4)]       = 0xa0 + (i & 0x3f);
	}
    }

    BufferState[Context->BufferIndex].BufferStructure->Format   = FormatVideo420_Planar;
}
#endif
		    OutputRing->Insert( (unsigned int )BufferState[Context->BufferIndex].Buffer );
		}
	    }

	    //
	    // Special case, if we are releasing a decode context due to a failure,
	    // then it is possible this can result in invalidation of the current decode index
	    //

	    if( Context->BufferIndex == CurrentDecodeBufferIndex )
		CurrentDecodeBufferIndex        = INVALID_INDEX;
	}
	OS_UnLockMutex( &Lock );
    }

    //
    // If requested we shrink any attached coded buffer to zero size.
    //

    if( Configuration.ShrinkCodedDataBuffersAfterDecode )
    {
	do
	{
	    Status      = Context->DecodeContextBuffer->ObtainAttachedBufferReference( CodedFrameBufferType, &AttachedCodedDataBuffer );
	    if( Status == BufferNoError )
	    {
		AttachedCodedDataBuffer->SetUsedDataSize( 0 );

		Status  = AttachedCodedDataBuffer->ShrinkBuffer( 0 );
		if( Status != BufferNoError )
		    report( severity_info, "Codec_MmeBase_c::ReleaseDecodeContext - Failed to shrink buffer.\n" );

		Status = Context->DecodeContextBuffer->DetachBuffer( AttachedCodedDataBuffer );
		if( Status != BufferNoError )
		{
		    report( severity_info, "Codec_MmeBase_c::ReleaseDecodeContext - Failed to detach buffer.\n" );
		    break;
		}
	    }
	} while( Status == BufferNoError );
    }


    //
    // Release reference frames
    //

    for( i=0; i<Context->NumberOfReferenceFrameLists; i++ )
	for( j=0; j<Context->ReferenceFrameList[i].EntryCount; j++ )
	{
	unsigned int	Index	= Context->ReferenceFrameList[i].EntryIndicies[j];

	    if( Index != INVALID_INDEX )
	    {
		if( BufferState[Index].MacroblockStructurePresent )
		    BufferState[Index].BufferMacroblockStructure->DecrementReferenceCount();

		DecrementReferenceCount( Index );
	    }
	}

    //
    // Are we about to release the current context 
    //

    if( Context == DecodeContext )
    {
	DecodeContextBuffer     = NULL;
	DecodeContext           = NULL;
    }

    //
    // Release the decode context
    //

    Context->DecodeContextBuffer->DecrementReferenceCount();

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to translate reference frame indices,
//      and increment usage counts appropriately.
//

CodecStatus_t   Codec_MmeBase_c::TranslateReferenceFrameLists(  bool              IncrementUseCountForReferenceFrame )
{
unsigned int      i,j;
unsigned int      BufferIndex;
CodecStatus_t     Status;

//

    if( IncrementUseCountForReferenceFrame && ParsedFrameParameters->ReferenceFrame )
    {
	CurrentDecodeBuffer->IncrementReferenceCount();
	BufferState[CurrentDecodeBufferIndex].ReferenceFrameCount++;
    }

//

    DecodeContext->NumberOfReferenceFrameLists  = ParsedFrameParameters->NumberOfReferenceFrameLists;
    memcpy( DecodeContext->ReferenceFrameList, ParsedFrameParameters->ReferenceFrameList, DecodeContext->NumberOfReferenceFrameLists * sizeof(ReferenceFrameList_t) );

    for( i=0; i<ParsedFrameParameters->NumberOfReferenceFrameLists; i++ )
    {
	for( j=0; j<ParsedFrameParameters->ReferenceFrameList[i].EntryCount; j++ )
	{
	    Status      = TranslateDecodeIndex( ParsedFrameParameters->ReferenceFrameList[i].EntryIndicies[j],
						&BufferIndex );
	    if( Status != CodecNoError )
	    {
		//
		// If we cannot translate the reference, we use the current decoding frame, 
		// this solves a whole heap of problems, because it provides us with a real 
		// frame to mess around with.
		//

		report( severity_error, "Codec_MmeBase_c::TranslateReferenceFrameLists(%s) - Unable to translate reference.\n", Configuration.CodecName );
		report( severity_info,  "        Missing index is %d (%d %d)\n", ParsedFrameParameters->ReferenceFrameList[i].EntryIndicies[j], ParsedFrameParameters->NumberOfReferenceFrameLists, ParsedFrameParameters->ReferenceFrameList[i].EntryCount );
		report( severity_info,  "        Missing %d %d\n", ParsedFrameParameters->DecodeFrameIndex, ParsedFrameParameters->ReferenceFrameList[i].EntryIndicies[j] );
		report( severity_info,  "        Missing %d - %2d %2d %2d %2d %2d %2d %2d %2d\n", ParsedFrameParameters->ReferenceFrameList[0].EntryCount,
			ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0],ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[1],
			ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[2],ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[3],
			ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[4],ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[5],
			ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[6],ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[7] );
		report( severity_info,  "        Missing %d - %2d %2d %2d %2d %2d %2d %2d %2d\n", ParsedFrameParameters->ReferenceFrameList[1].EntryCount, 
			ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[0],ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[1],
			ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[2],ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[3],
			ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[4],ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[5],
			ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[6],ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[7] );
		BufferIndex     = CurrentDecodeBufferIndex;
	    }

	    DecodeContext->ReferenceFrameList[i].EntryIndicies[j]   = BufferIndex;
	    BufferState[BufferIndex].Buffer->IncrementReferenceCount();

	    if( BufferState[BufferIndex].MacroblockStructurePresent )
		BufferState[BufferIndex].BufferMacroblockStructure->IncrementReferenceCount();
	}
    }

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to initialize all the types with the buffer manager
//

CodecStatus_t   Codec_MmeBase_c::InitializeDataType(    BufferDataDescriptor_t   *InitialDescriptor,
							BufferType_t             *Type,
							BufferDataDescriptor_t  **ManagedDescriptor )
{
PlayerStatus_t  Status;

    Status      = BufferManager->FindBufferDataType( InitialDescriptor->TypeName, Type );
    if( Status != BufferNoError )
    {
	Status  = BufferManager->CreateBufferDataType( InitialDescriptor, Type );
	if( Status != BufferNoError )
	{
	    report( severity_error, "Codec_MmeBase_c::InitializeDataType(%s) - Failed to create the '%s' buffer type.\n", Configuration.CodecName, InitialDescriptor->TypeName );
	    return Status;
	}
    }

    BufferManager->GetDescriptor( *Type, (BufferPredefinedType_t)InitialDescriptor->Type, ManagedDescriptor );
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to initialize all the types with the buffer manager
//

CodecStatus_t   Codec_MmeBase_c::InitializeDataTypes(   void )
{
CodecStatus_t   Status;

//

    if( DataTypesInitialized )
	return CodecNoError;

    //
    // Stream parameters context type
    //
    Status      = InitializeDataType( Configuration.StreamParameterContextDescriptor, &StreamParameterContextType, &StreamParameterContextDescriptor );
    if( Status != PlayerNoError )
	return Status;

    //
    // Decode context type
    //
    Status      = InitializeDataType( Configuration.DecodeContextDescriptor, &DecodeContextType, &DecodeContextDescriptor );
    if( Status != PlayerNoError )
	return Status;

//

    DataTypesInitialized        = true;
    return CodecNoError;
}

///////////////////////////////////////////////////////////////////////////
///
/// Verify that the transformer is capable of correct operation.
///
/// This method is always called before a transformer is initialized. It may
/// also, optionally, be called from the sub-class constructor. Such classes
/// typically fail to initialize properly if their needs are not met. By failing
/// early an alternative codec (presumably with a reduced feature set) may be
/// substituted.
///
CodecStatus_t Codec_MmeBase_c::VerifyMMECapabilities( unsigned int ActualTransformer )
{
CodecStatus_t                    Status;
MME_ERROR                        MMEStatus;
MME_TransformerCapability_t      Capability;

    if( Configuration.SizeOfTransformCapabilityStructure != 0 )
    {
	memset( &Capability, 0, sizeof(MME_TransformerCapability_t) );
	memset( Configuration.TransformCapabilityStructurePointer, 0x00, Configuration.SizeOfTransformCapabilityStructure );

	Capability.StructSize           = sizeof(MME_TransformerCapability_t);
	Capability.TransformerInfoSize  = Configuration.SizeOfTransformCapabilityStructure;
	Capability.TransformerInfo_p    = Configuration.TransformCapabilityStructurePointer;

	MMEStatus                       = MME_GetTransformerCapability( Configuration.TransformName[ActualTransformer], &Capability );
	if( MMEStatus != MME_SUCCESS )
	{
	    report( severity_error, "Codec_MmeBase_c::InitializeMMETransformer(%s:%s) - Unable to read capabilities (%08x).\n", Configuration.CodecName, Configuration.TransformName[ActualTransformer], MMEStatus );
	    return CodecError;
	}

	CODEC_TRACE("Found %s transformer (version %x)\n", Configuration.TransformName[ActualTransformer], Capability.Version );

	Status                          = HandleCapabilities();
	if( Status != CodecNoError )
	    return Status;
    }

    return CodecNoError;
}

///////////////////////////////////////////////////////////////////////////
///
/// Verify that at least one transformer is capable of correct operation.
///
/// This method is a more relaxed version of Codec_MmeBase_c::VerifyMMECapabilities().
/// Basically we work through all the possible transformer names and report success
/// if at least one can support our operation.
///
CodecStatus_t   Codec_MmeBase_c::GloballyVerifyMMECapabilities( void )
{
CodecStatus_t Status = CodecNoError;

    for (unsigned int i=0; i<CODEC_MAX_TRANSFORMERS; i++)
    {
	if( i > 0 && 0 == strcmp( Configuration.TransformName[i],
				  Configuration.TransformName[i-1] ) )
	    break;

	Status = VerifyMMECapabilities(i);
	if( Status == CodecNoError )
	    return CodecNoError;
    }

    return Status;
}

///////////////////////////////////////////////////////////////////////////
///
///     Function to initialize an mme transformer
///
CodecStatus_t   Codec_MmeBase_c::InitializeMMETransformer(      void )
{
CodecStatus_t                    Status;
MME_ERROR                        MMEStatus;

//

    MMECommandPreparedCount     = 0;
    MMECommandAbortedCount      = 0;
    MMECommandCompletedCount    = 0;

    //
    // Is there any capability information the caller is interested in
    //

    Status = VerifyMMECapabilities();
    if( Status != CodecNoError )
	return Status; // sub-routine puts errors to console

    //
    // Handle the transformer initialization
    //

    memset( &MMEInitializationParameters, 0x00, sizeof(MME_TransformerInitParams_t) );

    MMEInitializationParameters.Priority                        = MME_PRIORITY_NORMAL;
    MMEInitializationParameters.StructSize                      = sizeof(MME_TransformerInitParams_t);
    MMEInitializationParameters.Callback                        = &MMECallbackStub;
    MMEInitializationParameters.CallbackUserData                = this;

    Status      = FillOutTransformerInitializationParameters();
    if( Status != CodecNoError )
    {
	report( severity_error, "Codec_MmeBase_c::InitializeMMETransformer(%s) - Failed to fill out transformer initialization parameters (%08x).\n", Configuration.CodecName, Status );
	return Status;
    }

//

    MMEStatus   = MME_InitTransformer( Configuration.TransformName[SelectedTransformer], &MMEInitializationParameters, &MMEHandle );
    if( MMEStatus != MME_SUCCESS )
    {
	report( severity_error, "Codec_MmmBase_c::InitializeMMETransformer(%s,%s) - Failed to initialize mme transformer (%08x).\n",
	       Configuration.CodecName, Configuration.TransformName[SelectedTransformer], MMEStatus );
	return CodecError;
    }

    //report(severity_info, "Initialised MME with handle %x\n",MMEHandle);

//

    MMEInitialized      = true;
    MMECallbackPriorityBoosted = false;

//

    return CodecNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Terminate the mme transformer system
//

CodecStatus_t   Codec_MmeBase_c::TerminateMMETransformer( void )
{
unsigned int            i;
MME_ERROR               Status;
unsigned int            MaxTimeToWait;

//

    if( MMEInitialized )
    {
	MMEInitialized  = false;

	//
	// Wait a reasonable time for all mme transactions to terminate
	//

	MaxTimeToWait   = (Configuration.StreamParameterContextCount + Configuration.DecodeContextCount) *
				CODEC_MAX_WAIT_FOR_MME_COMMAND_COMPLETION;

	for( i=0; ((i<MaxTimeToWait/10) && (MMECommandPreparedCount > (MMECommandCompletedCount + MMECommandAbortedCount))); i++ )
	{
	    OS_SleepMilliSeconds( 10 );
	}
	if( MMECommandPreparedCount > (MMECommandCompletedCount + MMECommandAbortedCount) )
	    report( severity_error, "Codec_MmeBase_c::TerminateMMETransformer(%s) - Transformer failed to complete %d commands in %dms.\n", Configuration.CodecName, (MMECommandPreparedCount - MMECommandCompletedCount - MMECommandAbortedCount), MaxTimeToWait );

	//
	// Terminate the transformer
	//
//      unsigned int BeforeTime = OS_GetTimeInMilliSeconds();

	Status      = MME_TermTransformer( MMEHandle );

//      report(severity_error, "/\\/\\/\\/\\ MME_TermTransformer took %d ms /\\/\\/\\/\\\n",OS_GetTimeInMilliSeconds() - BeforeTime);

	if (Status != MME_SUCCESS)
	{
	    report( severity_error, "Codec_MmeBase_c::TerminateMMETransformer(%s) - Failed to terminate mme transformer (%08x).\n", Configuration.CodecName, Status );
	    return CodecError;
	}
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to send a set stream parameters
//

CodecStatus_t   Codec_MmeBase_c::SendMMEStreamParameters(       void )
{
MME_ERROR       Status;

    //
    // Perform the generic portion of the setup
    //

    StreamParameterContext->MMECommand.StructSize                       = sizeof(MME_Command_t);
    StreamParameterContext->MMECommand.CmdCode                          = MME_SET_GLOBAL_TRANSFORM_PARAMS;
    StreamParameterContext->MMECommand.CmdEnd                           = MME_COMMAND_END_RETURN_NOTIFY;
    StreamParameterContext->MMECommand.DueTime                          = 0;                    // No Idea what to put here, I just want it done in sequence.
    StreamParameterContext->MMECommand.NumberInputBuffers               = 0;
    StreamParameterContext->MMECommand.NumberOutputBuffers              = 0;
    StreamParameterContext->MMECommand.DataBuffers_p                    = NULL;

    //
    // Check that we have not commenced shutdown.
    //

    MMECommandPreparedCount++;

    if( TestComponentState(ComponentHalted) )
    {
	MMECommandAbortedCount++;
	return CodecNoError;
    }

    //
    // Do we wish to dump the mme command
    //

#ifdef DUMP_COMMANDS
    DumpMMECommand( &StreamParameterContext->MMECommand );
#endif

    //
    // Perform the mme transaction - Note at this point we invalidate
    // our pointer to the context buffer, after the send we invalidate
    // out pointer to the context data.
    //

#ifdef __KERNEL__
//	DecodeContextBuffer->FlushCache();	
//	OSDEV_FlushCacheRange((void*)&(ParsedFrameParameters->StreamParameterStructure),ParsedFrameParameters->SizeofStreamParameterStructure);
#endif

    Status      = MME_SendCommand( MMEHandle, &StreamParameterContext->MMECommand );
    if( Status != MME_SUCCESS )
    {
	report( severity_error, "Codec_MmeBase_c::SendMMEStreamParameters(%s) - Unable to send stream parameters (%08x).\n", Configuration.CodecName, Status );
	return CodecError;
    }

    StreamParameterContext              = NULL;
    StreamParameterContextBuffer        = NULL;

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to send a set stream parameters
//

CodecStatus_t   Codec_MmeBase_c::SendMMEDecodeCommand(  void )
{
MME_ERROR       Status;

    //
    // Perform the generic portion of the setup
    //

    DecodeContext->MMECommand.StructSize                        = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = 0;                    // No Idea what to put here, I just want it done in sequence.


    //
    // Check that we have not commenced shutdown.
    //

    MMECommandPreparedCount++;

    if( TestComponentState(ComponentHalted) )
    {
	MMECommandAbortedCount++;
	return CodecNoError;
    }

    //
    // Do we wish to dump the mme command
    //

#ifdef DUMP_COMMANDS
    DumpMMECommand( &DecodeContext->MMECommand );
#endif

    //
    // Perform the mme transaction - Note at this point we invalidate
    // our pointer to the context buffer, after the send we invalidate
    // out pointer to the context data.
    //

#ifdef __KERNEL__
	DecodeContextBuffer->FlushCache();
	CodedFrameBuffer->FlushCache();
#endif

    DecodeContextBuffer                 = NULL;
    DecodeContext->DecodeCommenceTime   = OS_GetTimeInMicroSeconds();

    //report (severity_error, "Sending actual MME Decode frame command %d\n",DecodeContext->MMECommand.CmdCode);

    Status      = MME_SendCommand( MMEHandle, &DecodeContext->MMECommand );
    if( Status != MME_SUCCESS )
    {
	report( severity_error, "Codec_MmeBase_c::SendMMEDecodeCommand(%s) - Unable to send decode command (%08x).\n", Configuration.CodecName, Status );
	return CodecError;
    }

    DecodeContext       = NULL;

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Callback function from MME
//
//      NOTE we forced the command to be the first element in each of the 
//      command contexts, this allows us to find the context by simply
//      casting the command address.
//

void   Codec_MmeBase_c::CallbackFromMME( MME_Event_t Event, MME_Command_t *CallbackData )
{
CodecBaseStreamParameterContext_t       *StreamParameterContext;
CodecBaseDecodeContext_t                *DecodeContext;
CodecStatus_t 				 Status;

//

    if( CallbackData == NULL )
    {
	report( severity_error, "Codec_MmeBase_c::CallbackFromMME(%s) - ####################### No CallbackData #######################\n", Configuration.CodecName );
	return;
    }

    //
    // Switch to perform appropriate actions per command
    //

    switch( CallbackData->CmdCode )
    {
	case MME_SET_GLOBAL_TRANSFORM_PARAMS:
	    //
	    //
	    //

	    StreamParameterContext  = (CodecBaseStreamParameterContext_t *)CallbackData;

	    StreamParameterContext->StreamParameterContextBuffer->DecrementReferenceCount();

	    MMECommandCompletedCount++;

	    //
	    // boost the callback priority to be the same as the DecodeToManifest process
	    //
	    if (!MMECallbackPriorityBoosted)
	    {
		OS_SetPriority( OS_MID_PRIORITY + 10 );
		MMECallbackPriorityBoosted = true;
	    }
//                      report(severity_error,"Set MME Global Params Callback\n");
	    break;

	case MME_TRANSFORM:
	    //
	    // in case of StreamBase decoders the transform may be return with NOT_ENOUGH_MEMORY event so... incomplete
	    //

	    if (Event == MME_COMMAND_COMPLETED_EVT)
	    {
		DecodeContext           = (CodecBaseDecodeContext_t *)CallbackData;

		Status = ValidateDecodeContext( DecodeContext );
		if( Status != CodecNoError )
		    report( severity_error, "Codec_MmeBase_c::CallbackFromMME(%s) - Failed to validate the decode context\n", Configuration.CodecName );

		if (CallbackData->CmdStatus.Error != MME_SUCCESS)
		    report( severity_error, "MME Transform Callback %x\n",CallbackData->CmdStatus.Error);
		
		Status = CheckCodecReturnParameters( DecodeContext );
		if( Status != CodecNoError )
		{
		    report( severity_error, "Codec_MmeBase_c::CallbackFromMME(%s) - Failed to check codec return parameters\n", Configuration.CodecName );
		}


		//
		// Calculate the decode time
		//

		Status	= CalculateMaximumFrameRate( DecodeContext );
		if( Status != CodecNoError )
		    report( severity_error, "Codec_MmeBase_c::CallbackFromMME(%s) - Failed to adjust maximum frame rate.\n", Configuration.CodecName );

#if 0
		report( severity_info, "Decode took %6lldus\n", OS_GetTimeInMicroSeconds() - DecodeContext->DecodeCommenceTime );
{
static unsigned long long LastCommence  = 0;
static unsigned long long LastComplete  = 0;
unsigned long long Now          = OS_GetTimeInMicroSeconds();

    if( (Now - DecodeContext->DecodeCommenceTime) > 30000 )
    report( severity_info, "Decode times - CommenceInterval %6lld, CompleteInterval %6lld, DurationInterval %6lld.\n", 
			DecodeContext->DecodeCommenceTime - LastCommence,
			Now - LastComplete,
			Now - DecodeContext->DecodeCommenceTime );

    LastCommence        = DecodeContext->DecodeCommenceTime;
    LastComplete        = Now;
}
#endif

		ReleaseDecodeContext( DecodeContext );

//              report( severity_note, "Codec_MmeBase_c::CallbackFromMME(%s) - Not currently doing error checking/handling.\n", Configuration.CodecName );

		MMECommandCompletedCount++;
	    }
	    else
	    {
		//report( severity_info, "Codec_MmeBase_c::CallbackFromMME(%s) - Transform Command returns INCOMPLETE \n", Configuration.CodecName );
		CODEC_DEBUG("Codec_MmeBase_c::CallbackFromMME(%s) - Transform Command returns INCOMPLETE \n", Configuration.CodecName );
	    }

	    break;

	case MME_SEND_BUFFERS:
	    MMECommandCompletedCount++;
	    break;
	default:
	    break;
    }

    //
    // Test for a marker buffer to be passed on
    //

    if( MarkerBuffer != NULL )
	TestMarkerFramePassOn();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to decrement the reference count on a buffer, also 
//      wraps the cleansing of the associated data in the codec.
//

CodecStatus_t   Codec_MmeBase_c::DecrementReferenceCount(       unsigned int              BufferIndex )
{
Buffer_t        Buffer;
unsigned int    Count;

//

    Buffer      = BufferState[BufferIndex].Buffer;

    if (Buffer == NULL)
    {
	report(severity_error, "Codec_MmeBase_c::DecrementReferenceCount(%s) - NULL buffer index %d\n", Configuration.CodecName, BufferIndex );
	return CodecError;
    }

    OS_LockMutex( &Lock );
    Buffer->GetOwnerCount( &Count );

    if( Count == 1 )
    {
	if( (BufferState[BufferIndex].ReferenceFrameCount != 0) ||
	    (BufferState[BufferIndex].DecodesInProgress != 0) )
	{
	    report( severity_error, "Codec_MmeBase_c::DecrementReferenceCount(%s) - BufferState inconsistency (%d %d), implementation error.\n", Configuration.CodecName,
			BufferState[BufferIndex].ReferenceFrameCount, BufferState[BufferIndex].DecodesInProgress );
	}

	UnMapBufferIndex( BufferIndex );
	memset( &BufferState[BufferIndex], 0x00, sizeof(CodecBufferState_t) );
    }

    Buffer->DecrementReferenceCount();
    OS_UnLockMutex( &Lock );

//

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to adjust the maximum frame rate based on actual decode times
//

CodecStatus_t   Codec_MmeBase_c::CalculateMaximumFrameRate( CodecBaseDecodeContext_t   *DecodeContext  )
{
unsigned long long			 Now;
unsigned long long			 DecodeTime;

//

    Now				= OS_GetTimeInMicroSeconds();
    DecodeTime			= min( Now - DecodeContext->DecodeCommenceTime, Now - LastDecodeCompletionTime );

    if( BufferState[DecodeContext->BufferIndex].FieldDecode )
	DecodeTime	*= 2;

//

    if( DecodeTimeShortIntegrationPeriod == 0 )
    {
	Manifestor->GetDecodeBufferCount( &DecodeTimeLongIntegrationPeriod );
	DecodeTimeShortIntegrationPeriod	 = 4;

	if( BufferState[DecodeContext->BufferIndex].FieldDecode )
	{
	    DecodeTimeLongIntegrationPeriod	*= 2;
	    DecodeTimeShortIntegrationPeriod	*= 2;
	}
    }

//

    ShortTotalDecodeTime						= ShortTotalDecodeTime - DecodeTimes[(NextDecodeTime + DecodeTimeLongIntegrationPeriod - DecodeTimeShortIntegrationPeriod) % DecodeTimeLongIntegrationPeriod] + DecodeTime;
    LongTotalDecodeTime							= LongTotalDecodeTime - DecodeTimes[NextDecodeTime % DecodeTimeLongIntegrationPeriod] + DecodeTime;
    DecodeTimes[NextDecodeTime % DecodeTimeLongIntegrationPeriod]	= DecodeTime;
    NextDecodeTime++;

    LastDecodeCompletionTime						= Now;

//

#if 0
    if( (NextDecodeTime % DecodeTimeIntegrationPeriod) == 0 )
	report( severity_error, "Decode Times(%s) - Average = %lld(over %d), Max Framerate = %d\n", 
		Configuration.CodecName, 
		TotalDecodeTime / DecodeTimeIntegrationPeriod, 
		DecodeTimeIntegrationPeriod,
		(unsigned int)((1000000ull * DecodeTimeIntegrationPeriod) / TotalDecodeTime) );
#endif

    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration	= Rational_t( (1000000ull * DecodeTimeShortIntegrationPeriod), ShortTotalDecodeTime );
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration	= Rational_t( (1000000ull * DecodeTimeLongIntegrationPeriod), LongTotalDecodeTime );

//

    return CodecNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Take ownership of Codec_MmeBase_c::MarkerBuffer in a thread-safe way.
///
/// Atomically read the marker buffer and set it to NULL. Nobody except
/// this method (and Codec_MmeBase_c::Reset() is permitted to set MarkerBuffer
/// to NULL.
///
Buffer_t Codec_MmeBase_c::TakeMarkerBuffer( void )
{
	Buffer_t LocalMarkerBuffer;

	OS_LockMutex( &Lock );
	LocalMarkerBuffer = MarkerBuffer;
	MarkerBuffer = NULL;
	OS_UnLockMutex( &Lock );

	return LocalMarkerBuffer;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Test whether or not we have to pass on a marker frame.
//

CodecStatus_t   Codec_MmeBase_c::TestMarkerFramePassOn( void )
{
Buffer_t                  LocalMarkerBuffer;

    if( (MarkerBuffer != NULL) &&
	((MMECommandAbortedCount + MMECommandCompletedCount) >= PassOnMarkerBufferAt) )
    {
	LocalMarkerBuffer       = TakeMarkerBuffer();
	if( LocalMarkerBuffer )
	    OutputRing->Insert( (unsigned int)LocalMarkerBuffer );
    }


    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out an mme command
//

void   Codec_MmeBase_c::DumpMMECommand( MME_Command_t *CmdInfo_p )
{
    report( severity_info,  "AZA - COMMAND (%d)\n", ParsedFrameParameters->DecodeFrameIndex );
    report( severity_info,  "AZA -     StructSize                             = %d\n", CmdInfo_p->StructSize );
    report( severity_info,  "AZA -     CmdCode                                = %d\n", CmdInfo_p->CmdCode );
    report( severity_info,  "AZA -     CmdEnd                                 = %d\n", CmdInfo_p->CmdEnd );
    report( severity_info,  "AZA -     DueTime                                = %d\n", CmdInfo_p->DueTime );
    report( severity_info,  "AZA -     NumberInputBuffers                     = %d\n", CmdInfo_p->NumberInputBuffers );
    report( severity_info,  "AZA -     NumberOutputBuffers                    = %d\n", CmdInfo_p->NumberOutputBuffers );
    report( severity_info,  "AZA -     DataBuffers_p                          = %08x\n", CmdInfo_p->DataBuffers_p );
    report( severity_info,  "AZA -     CmdStatus\n" );
    report( severity_info,  "AZA -         CmdId                              = %d\n", CmdInfo_p->CmdStatus.CmdId );
    report( severity_info,  "AZA -         State                              = %d\n", CmdInfo_p->CmdStatus.State );
    report( severity_info,  "AZA -         ProcessedTime                      = %d\n", CmdInfo_p->CmdStatus.ProcessedTime );
    report( severity_info,  "AZA -         Error                              = %d\n", CmdInfo_p->CmdStatus.Error );
    report( severity_info,  "AZA -         AdditionalInfoSize                 = %d\n", CmdInfo_p->CmdStatus.AdditionalInfoSize );
    report( severity_info,  "AZA -         AdditionalInfo_p                   = %08x\n", CmdInfo_p->CmdStatus.AdditionalInfo_p );
    report( severity_info,  "AZA -     ParamSize                              = %d\n", CmdInfo_p->ParamSize );
    report( severity_info,  "AZA -     Param_p                                = %08x\n", CmdInfo_p->Param_p );

//

    switch( CmdInfo_p->CmdCode )
    {
	case MME_SET_GLOBAL_TRANSFORM_PARAMS:
		DumpSetStreamParameters( CmdInfo_p->Param_p );
		break;

	case MME_TRANSFORM:
		DumpDecodeParameters( CmdInfo_p->Param_p );
		break;

	default:break;
    }
}

////////////////////////////////////////////////////////////////////////////
/// \fn CodecStatus_t Codec_MmeBase_c::FillOutTransformerInitializationParameters( void )
///
/// Populate the transformers initialization parameters.
///
/// This method is required the fill out the TransformerInitParamsSize and TransformerInitParams_p
/// members of Codec_MmeBase_c::MMEInitializationParameters and initialize and data pointed to
/// as a result.
///
/// This method may, optionally, change the default values held in other members of
/// Codec_MmeBase_c::MMEInitializationParameters .
///


////////////////////////////////////////////////////////////////////////////
/// \fn CodecStatus_t   FillOutSendBufferCommand(   void );
/// This function may only implemented for stream base inheritors.
/// Populates the SEND_BUFFER Command structure

CodecStatus_t   Codec_MmeBase_c::FillOutSendBufferCommand(void)
{
    // TODO :: default implementation
    return CodecNoError;
}

