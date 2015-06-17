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

Source file name : player_settings.cpp
Author :           Nick

Implementation of the settings related functions of the
generic class implementation of player 2


Date        Modification                                    Name
----        ------------                                    --------
03-Nov-06   Created                                         Nick

************************************************************************/

#include <stdarg.h>
#include "player_generic.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

// Used for human readable policy reporting
static const char *LookupPlayerPolicy( PlayerPolicy_t Policy )
{
    switch( Policy )
    {
#define C(x) case x: return #x
    C(PolicyTrickModeAudio);
    C(PolicyPlay24FPSVideoAt25FPS);
    C(PolicyMasterClock);
    C(PolicyExternalTimeMapping);
    C(PolicyExternalTimeMappingVsyncLocked);
    C(PolicyAVDSynchronization);
    C(PolicySyncStartImmediate);
    C(PolicyManifestFirstFrameEarly);
    C(PolicyVideoBlankOnShutdown);
    C(PolicyStreamOnlyKeyFrames);
    C(PolicyStreamSingleGroupBetweenDiscontinuities);
    C(PolicyClampPlaybackIntervalOnPlaybackDirectionChange);
    C(PolicyPlayoutOnTerminate);
    C(PolicyPlayoutOnSwitch);
    C(PolicyPlayoutOnDrain);
    C(PolicyDisplayAspectRatio);
    C(PolicyDisplayFormat);
    C(PolicyTrickModeDomain);
    C(PolicyDiscardLateFrames);
    C(PolicyVideoStartImmediate);
    C(PolicyRebaseOnFailureToDeliverDataInTime);
    C(PolicyRebaseOnFailureToDecodeInTime);
    C(PolicyH264AllowNonIDRResynchronization);
    C(PolicyH264AllowBadPreProcessedFrames);
    C(PolicyH264TreatDuplicateDpbValuesAsNonReferenceFrameFirst);
    C(PolicyH264ForcePicOrderCntIgnoreDpbDisplayFrameOrdering);
    C(PolicyH264TreatTopBottomPictureStructAsInterlaced);
    C(PolicyMPEG2DoNotHonourProgressiveFrameFlag);
    C(PolicyClockPullingLimit2ToTheNPartsPerMillion);
    C(PolicyLimitInputInjectAhead);
    C(PolicyMPEG2ApplicationType);
    C(PolicyDecimateDecoderOutput);
    C(PolicySymmetricJumpDetection);
    C(PolicyPtsForwardJumpDetectionThreshold);
    C(PolicyPixelAspectRatioCorrection);
    C(PolicyAllowFrameDiscardAtNormalSpeed);
    C(PolicyOperateCollator2InReversibleMode);
    C(PolicyVideoOutputWindowResizeSteps);
    C(PolicyIgnoreStreamUnPlayableCalls);
    C(PolicyUsePTSDeducedDefaultFrameRates);

    // Private policies (see player_generic.h)
    C(PolicyPlayoutAlwaysPlayout);
    C(PolicyPlayoutAlwaysDiscard);
    C(PolicyStatisticsOnAudio);
    C(PolicyStatisticsOnVideo);
#undef C

    default:
        if (Policy < (PlayerPolicy_t) PolicyMaxExtraPolicy)
	{
		report( severity_info, "LookupPlayerPolicy - Lookup failed for policy %d\n", Policy );
		return "UNKNOWN";
	}

	return "UNSPECIFIED";
    }
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Static constant data
//

static BufferDataDescriptor_t	  MetaDataInputDescriptor 			= METADATA_PLAYER_INPUT_DESCRIPTOR_TYPE;
static BufferDataDescriptor_t	  MetaDataCodedFrameParameters 			= METADATA_CODED_FRAME_PARAMETERS_TYPE;
static BufferDataDescriptor_t	  MetaDataStartCodeList 			= METADATA_START_CODE_LIST_TYPE;
static BufferDataDescriptor_t	  MetaDataParsedFrameParameters			= METADATA_PARSED_FRAME_PARAMETERS_TYPE;
static BufferDataDescriptor_t	  MetaDataParsedFrameParametersReference	= METADATA_PARSED_FRAME_PARAMETERS_REFERENCE_TYPE;
static BufferDataDescriptor_t	  MetaDataParsedVideoParameters			= METADATA_PARSED_VIDEO_PARAMETERS_TYPE;
static BufferDataDescriptor_t	  MetaDataParsedAudioParameters			= METADATA_PARSED_AUDIO_PARAMETERS_TYPE;
static BufferDataDescriptor_t	  MetaDataVideoOutputTiming			= METADATA_VIDEO_OUTPUT_TIMING_TYPE;
static BufferDataDescriptor_t	  MetaDataAudioOutputTiming			= METADATA_AUDIO_OUTPUT_TIMING_TYPE;
static BufferDataDescriptor_t	  MetaDataVideoOutputSurfaceDescriptor		= METADATA_VIDEO_OUTPUT_SURFACE_DESCRIPTOR_TYPE;
static BufferDataDescriptor_t	  MetaDataAudioOutputSurfaceDescriptor		= METADATA_AUDIO_OUTPUT_SURFACE_DESCRIPTOR_TYPE;
static BufferDataDescriptor_t	  MetaDataBufferStructure			= METADATA_BUFFER_STRUCTURE_TYPE;

static BufferDataDescriptor_t	  BufferVideoPostProcessingControl		= BUFFER_VIDEO_POST_PROCESSING_CONTROL_TYPE;
static BufferDataDescriptor_t	  BufferAudioPostProcessingControl		= BUFFER_AUDIO_POST_PROCESSING_CONTROL_TYPE;

static BufferDataDescriptor_t	  BufferPlayerControlStructure			= BUFFER_PLAYER_CONTROL_STRUCTURE_TYPE;
static BufferDataDescriptor_t	  BufferInputBuffer				= BUFFER_INPUT_TYPE;
static BufferDataDescriptor_t	  MetaDataSequenceNumberDescriptor		= METADATA_SEQUENCE_NUMBER_TYPE;

static BufferDataDescriptor_t	  BufferCodedFrameBuffer			= BUFFER_CODED_FRAME_BUFFER_TYPE;

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Register the buffer manager to the player
//

PlayerStatus_t   Player_Generic_c::RegisterBufferManager(
						BufferManager_t		  BufferManager )
{
PlayerStatus_t		  Status;

//

    if( this->BufferManager != NULL )
    {
	report( severity_error, "Player_Generic_c::RegisterBufferManager - Attempt to change buffer manager, not permitted.\n" );
	return PlayerError;
    }

//

    this->BufferManager		= BufferManager;

    //
    // Register the predefined types, and the two buffer types used by the player
    //

    Status	= BufferManager->CreateBufferDataType( &MetaDataInputDescriptor, 		&MetaDataInputDescriptorType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataCodedFrameParameters, 		&MetaDataCodedFrameParametersType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataStartCodeList, 			&MetaDataStartCodeListType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataParsedFrameParameters,		&MetaDataParsedFrameParametersType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataParsedFrameParametersReference,	&MetaDataParsedFrameParametersReferenceType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataParsedVideoParameters,		&MetaDataParsedVideoParametersType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataParsedAudioParameters,		&MetaDataParsedAudioParametersType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataVideoOutputTiming,		&MetaDataVideoOutputTimingType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataAudioOutputTiming,		&MetaDataAudioOutputTimingType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataVideoOutputSurfaceDescriptor,	&MetaDataVideoOutputSurfaceDescriptorType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataAudioOutputSurfaceDescriptor,	&MetaDataAudioOutputSurfaceDescriptorType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataBufferStructure,		&MetaDataBufferStructureType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &MetaDataSequenceNumberDescriptor,	&MetaDataSequenceNumberType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &BufferVideoPostProcessingControl,	&BufferVideoPostProcessingControlType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &BufferAudioPostProcessingControl,	&BufferAudioPostProcessingControlType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &BufferPlayerControlStructure,		&BufferPlayerControlStructureType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &BufferInputBuffer,			&BufferInputBufferType );
    if( Status != BufferNoError )
	return Status;

    Status	= BufferManager->CreateBufferDataType( &BufferCodedFrameBuffer,			&BufferCodedFrameBufferType );
    if( Status != BufferNoError )
	return Status;

    //
    // Create pools of buffers for input and player control structures
    //

    Status	= BufferManager->CreatePool( &PlayerControlStructurePool, BufferPlayerControlStructureType, PLAYER_MAX_CONTROL_STRUCTURE_BUFFERS );
    if( Status != BufferNoError )
    {
	report( severity_error, "Player_Generic_c::RegisterBufferManager - Failed to create pool of player control structures.\n" );
	return Status;
    }

//

    Status	= BufferManager->CreatePool( &InputBufferPool, BufferInputBufferType, PLAYER_MAX_INPUT_BUFFERS );
    if( Status != BufferNoError )
    {
	report( severity_error, "Player_Generic_c::RegisterBufferManager - Failed to create pool of input buffers.\n" );
	return Status;
    }

    Status	= InputBufferPool->AttachMetaData( MetaDataInputDescriptorType );
    if( Status != BufferNoError )
    {
	report( severity_error, "Player_Generic_c::RegisterBufferManager - Failed to attach an input descriptor to all input buffers.\n" );
	return Status;
    }

//

    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Register a demultiplexor to the player (may be several registered at once)
//

PlayerStatus_t   Player_Generic_c::RegisterDemultiplexor(
						Demultiplexor_t		  Demultiplexor )
{
unsigned int		i;
PlayerInputMuxType_t	HandledType;
PlayerInputMuxType_t	PreviouslyHandledType;

//

    if( DemultiplexorCount >= PLAYER_MAX_DEMULTIPLEXORS )
    {
	report( severity_error, "Player_Generic_c::RegisterDemultiplexor - Too many demultiplexors.\n" );
	return PlayerError;
    }

//

    Demultiplexor->GetHandledMuxType( &HandledType );

    for( i=0; i<DemultiplexorCount; i++ )
    {
	Demultiplexor->GetHandledMuxType( &PreviouslyHandledType);
	if( HandledType == PreviouslyHandledType )
	{
	    report( severity_error, "Player_Generic_c::RegisterDemultiplexor - New demultiplexor duplicates previously registered one.\n" );
	    return PlayerError;
	}
    }

//

    Demultiplexors[DemultiplexorCount++]	= Demultiplexor;

    Demultiplexor->RegisterPlayer( this, PlayerAllPlaybacks, PlayerAllStreams );

//

    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Set a policy
//
PlayerStatus_t   Player_Generic_c::SetPolicy(	PlayerPlayback_t	  Playback,
						PlayerStream_t		  Stream,
						PlayerPolicy_t		  Policy,
						unsigned char		  PolicyValue )
{
PlayerPolicyState_t	*SpecificPolicyRecord;

report( severity_info, "SetPolicy - %08x %08x %-45s %d\n", Playback, Stream, LookupPlayerPolicy(Policy), PolicyValue );
//

    if( (Playback		!= PlayerAllPlaybacks) &&
	(Stream			!= PlayerAllStreams) &&
	(Stream->Playback	!= Playback) )
    {
	report( severity_error, "Player_Generic_c::SetPolicy - Attempt to set policy on specific stream, and differing specific playback.\n" );
	return PlayerError;
    }

//

    if( Stream != PlayerAllStreams )
	SpecificPolicyRecord	= &Stream->PolicyRecord;
    else if( Playback != PlayerAllPlaybacks )
	SpecificPolicyRecord	= &Playback->PolicyRecord;
    else
	SpecificPolicyRecord	= &PolicyRecord;

//

    SpecificPolicyRecord->Specified[Policy/32]	|= 1 << (Policy % 32);
    SpecificPolicyRecord->Value[Policy]		 = PolicyValue;

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Set module parameters
//

PlayerStatus_t   Player_Generic_c::SetModuleParameters(
						PlayerPlayback_t	  Playback,
						PlayerStream_t		  Stream,
						PlayerComponent_t	  Component,
						bool			  Immediately,
						unsigned int		  ParameterBlockSize,
						void			 *ParameterBlock )
{
unsigned int		  i;
PlayerStatus_t		  Status;
PlayerStatus_t		  CurrentStatus;
PlayerPlayback_t	  SubPlayback;
PlayerStream_t		  SubStream;
PlayerSequenceType_t	  SequenceType;
PlayerComponentFunction_t Fn;

//

    Status	= PlayerNoError;

    //
    // Check we are given consistent parameters
    //

    if( (Playback		!= PlayerAllPlaybacks) &&
	(Stream			!= PlayerAllStreams) &&
	(Stream->Playback	!= Playback) )
    {
	report( severity_error, "Player_Generic_c::SetModuleParameters - Attempt to set parameters on specific stream, and differing specific playback.\n" );
	return PlayerError;
    }

//

    if( !Immediately &&
	((Component == ComponentPlayer) ||
	 (Component == ComponentDemultiplexor) ||
	 (Component == ComponentOutputCoordinator) ||
	 (Component == ComponentCollator)) )
    {
	report( severity_error, "Player_Generic_c::SetModuleParameters - Setting for Player, demultiplexor, collator, and output coordinator can only occur immediately.\n" );
	return PlayerError;
    }

//

    if( !Immediately &&
	(ParameterBlockSize >= PLAYER_MAX_INLINE_PARAMETER_BLOCK_SIZE) )
    {
	report( severity_error, "Player_Generic_c::SetModuleParameters - Parameter block size larger than soft limit (%d > %d).\n",
				ParameterBlockSize, PLAYER_MAX_INLINE_PARAMETER_BLOCK_SIZE );
	return PlayerError;
    }

//

    if( Stream != PlayerAllStreams )
	Playback	= Stream->Playback;

    //
    // Split out player and demultiplexor as not being stream or
    // playback related, and only supported in immediate mode.
    //

    switch( Component )
    {
	case ComponentPlayer:
		Status	= SetModuleParameters( Playback, Stream, ParameterBlockSize, ParameterBlock ); 	// Player overload, next function in this file
		return Status;

	case ComponentDemultiplexor:
		for( i=0; i<DemultiplexorCount; i++ )
		{
		    CurrentStatus	= Demultiplexors[i]->SetModuleParameters(  ParameterBlockSize, ParameterBlock );
		    if( CurrentStatus != PlayerNoError )
			Status	= CurrentStatus;
		}
		return Status;

	default:
		break;
    }

    //
    // Select the function to be called and when
    //

    switch( Component )
    {
	case ComponentFrameParser:	Fn	= FrameParserFnSetModuleParameters;	break;
	case ComponentCodec:		Fn	= CodecFnSetModuleParameters;		break;
	case ComponentOutputTimer:	Fn	= OutputTimerFnSetModuleParameters;	break;
	case ComponentManifestor:	Fn	= ManifestorFnSetModuleParameters;	break;
        default:			Fn	= 0xffffffff;				break;
    }

    SequenceType	= Immediately ? SequenceTypeImmediate : SequenceTypeBeforeSequenceNumber;

    //
    // Perform two nested loops over affected playbacks and streams.
    // These should terminate appropriately for specific playbacks/streams
    //

    for( SubPlayback	 = ((Playback == PlayerAllPlaybacks) ? ListOfPlaybacks : Playback);
	 ((Playback == PlayerAllPlaybacks) ? (SubPlayback != NULL) : (SubPlayback == Playback));
	 SubPlayback	 = SubPlayback->Next )
    {
    	//
    	// Fudge the output timer to reference a particular stream and only call once per playback
    	//

	if( Component == ComponentOutputCoordinator )
	    Stream	= SubPlayback->ListOfStreams;

	for( SubStream	 = ((Stream == PlayerAllStreams) ? SubPlayback->ListOfStreams : Stream);
	     ((Stream == PlayerAllStreams) ? (SubStream != NULL) : (SubStream == Stream));
	     SubStream	 = SubStream->Next )
	{

	    if( Component == ComponentCollator )
		CurrentStatus	= SubStream->Collator->SetModuleParameters( ParameterBlockSize, ParameterBlock );
	    if( Component == ComponentOutputCoordinator )
		CurrentStatus	= SubPlayback->OutputCoordinator->SetModuleParameters( ParameterBlockSize, ParameterBlock );
	    else
		CurrentStatus	= CallInSequence( SubStream, SequenceType, SubStream->NextBufferSequenceNumber, Fn, ParameterBlockSize, ParameterBlock );

	    if( CurrentStatus != PlayerNoError )
		Status	= CurrentStatus;
	}
    }

//

    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Overload of SetModuleParameters, this version is responsible for setting player 
//	parameters, it is called by the master version of the function (above), which splits
//	out to individual component calls.
//

PlayerStatus_t   Player_Generic_c::SetModuleParameters(	
						PlayerPlayback_t	  Playback,
						PlayerStream_t	 	  Stream,
						unsigned int		  ParameterBlockSize,
						void			 *ParameterBlock )
{
PlayerParameterBlock_t	*PlayerParameterBlock = (PlayerParameterBlock_t *)ParameterBlock;

//

    if( ParameterBlockSize != sizeof(PlayerParameterBlock_t) )
    {
	report( severity_error, "Player_Generic_c::SetModuleParameters - Invalid parameter block.\n" );
	return PlayerError;
    }

//

    switch( PlayerParameterBlock->ParameterType )
    {
        case PlayerSetCodedFrameBufferParameters:

		//
		// Check that this is not stream specific, and update the relevant values.
		//

		if( Stream != PlayerAllStreams )
		{
		    report( severity_error, "Player_Generic_c::SetModuleParameters - Coded frame parameters cannot be set after a stream has been created.\n" );
		    return PlayerError;
		}

//

		switch( PlayerParameterBlock->CodedFrame.StreamType )
		{
		    case StreamTypeAudio:
			if( Playback == PlayerAllPlaybacks )
			{
			    AudioCodedFrameCount		= PlayerParameterBlock->CodedFrame.CodedFrameCount;
			    AudioCodedMemorySize		= PlayerParameterBlock->CodedFrame.CodedMemorySize;
			    AudioCodedFrameMaximumSize		= PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
			    memcpy( AudioCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
			}
			else
			{
			    Playback->AudioCodedFrameCount	= PlayerParameterBlock->CodedFrame.CodedFrameCount;
			    Playback->AudioCodedMemorySize	= PlayerParameterBlock->CodedFrame.CodedMemorySize;
			    Playback->AudioCodedFrameMaximumSize= PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
			    memcpy( Playback->AudioCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
			}
			break;
		    case StreamTypeVideo:
			if( Playback == PlayerAllPlaybacks )
			{
			    VideoCodedFrameCount		= PlayerParameterBlock->CodedFrame.CodedFrameCount;
			    VideoCodedMemorySize		= PlayerParameterBlock->CodedFrame.CodedMemorySize;
			    VideoCodedFrameMaximumSize		= PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
			    memcpy( VideoCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
			}
			else
			{

			    Playback->VideoCodedFrameCount	= PlayerParameterBlock->CodedFrame.CodedFrameCount;
			    Playback->VideoCodedMemorySize	= PlayerParameterBlock->CodedFrame.CodedMemorySize;
			    Playback->VideoCodedFrameMaximumSize= PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
			    memcpy( Playback->VideoCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
			}
			break;
		    case StreamTypeOther:
			if( Playback == PlayerAllPlaybacks )
			{
			    OtherCodedFrameCount		= PlayerParameterBlock->CodedFrame.CodedFrameCount;
			    OtherCodedMemorySize		= PlayerParameterBlock->CodedFrame.CodedMemorySize;
			    OtherCodedFrameMaximumSize		= PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
			    memcpy( OtherCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
			}
			else
			{
			    Playback->OtherCodedFrameCount	= PlayerParameterBlock->CodedFrame.CodedFrameCount;
			    Playback->OtherCodedMemorySize	= PlayerParameterBlock->CodedFrame.CodedMemorySize;
			    Playback->OtherCodedFrameMaximumSize= PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
			    memcpy( Playback->OtherCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
			}
			break;

		    default:
			report( severity_error, "Player_Generic_c::SetModuleParameters - Attempt to set Coded frame parameters for StreamTypeNone.\n" );
			return PlayerError;
			break;
		}
		break;

        default:
		report( severity_error, "Player_Generic_c::SetModuleParameters - Unrecognised parameter block (%d).\n", PlayerParameterBlock->ParameterType );
		return PlayerError;
    }

    return  PlayerNoError;

}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Set the presentation interval
//

PlayerStatus_t   Player_Generic_c::SetPresentationInterval(
						PlayerPlayback_t	  Playback,
						PlayerStream_t		  Stream,
						unsigned long long	  IntervalStartNativeTime,
						unsigned long long	  IntervalEndNativeTime )
{
PlayerStatus_t          Status                          = PlayerNoError;
unsigned long long      IntervalStartNormalizedTime     = INVALID_TIME;
unsigned long long      IntervalEndNormalizedTime       = INVALID_TIME;
PlayerStream_t		SubStream;

    //
    // First just check the consistency of the input
    //

    if( (Stream == PlayerAllStreams) ?
		(Playback == PlayerAllPlaybacks) :
		((Playback != PlayerAllPlaybacks) && (Playback != Stream->Playback)) )
    {
	report( severity_error, "Player_Generic_c::SetPresentationInterval - Invalid combination of Playback/Stream specifiers (%d %d %d).\n", 
			(Playback != PlayerAllPlaybacks), (Stream != PlayerAllStreams), ((Stream == PlayerAllStreams) ? 1 : (Playback == Stream->Playback)) );
	return PlayerError;
    }

    //
    // If there are no streams, translate the times as standard PTS times
    //

    if( Playback->ListOfStreams == NULL )
    {
	IntervalStartNormalizedTime	= (IntervalStartNativeTime == INVALID_TIME) ? INVALID_TIME : ((((IntervalStartNativeTime) * 300) + 13) / 27);
	IntervalEndNormalizedTime	= (IntervalEndNativeTime   == INVALID_TIME) ? INVALID_TIME : ((((IntervalEndNativeTime)   * 300) + 13) / 27);
    }

    //
    // perform the setting in a loop 
    //

    for( SubStream	 = ((Stream == PlayerAllStreams) ? Playback->ListOfStreams : Stream);
	 ((Stream 	== PlayerAllStreams) ? (SubStream != NULL) : (SubStream == Stream));
	 SubStream	 = SubStream->Next )
    {
	Status	= SubStream->FrameParser->TranslatePlaybackTimeNativeToNormalized( IntervalStartNativeTime, &IntervalStartNormalizedTime );
	if( Status == PlayerNoError )
	    Status	= SubStream->FrameParser->TranslatePlaybackTimeNativeToNormalized( IntervalEndNativeTime, &IntervalEndNormalizedTime );

	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::SetPresentationInterval - Failed to translate native time to normalized time.\n" );
	    return Status;
	}

	SubStream->RequestedPresentationIntervalStartNormalizedTime	= IntervalStartNormalizedTime;
	SubStream->RequestedPresentationIntervalEndNormalizedTime	= IntervalEndNormalizedTime;
    }

    //
    // Now erase any reversal limits, and if this was for all streams set the master requested times
    //

    if( Playback == PlayerAllPlaybacks )
	Playback	= Stream->Playback;

    Playback->PresentationIntervalReversalLimitStartNormalizedTime	= INVALID_TIME;
    Playback->PresentationIntervalReversalLimitEndNormalizedTime	= INVALID_TIME;

//

    if( Stream == PlayerAllStreams )
    {
	Playback->RequestedPresentationIntervalStartNormalizedTime	= IntervalStartNormalizedTime;
    	Playback->RequestedPresentationIntervalEndNormalizedTime	= IntervalEndNormalizedTime;
    }

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Get the buffer manager
//

PlayerStatus_t   Player_Generic_c::GetBufferManager(
						BufferManager_t		 *BufferManager )
{
    *BufferManager	= this->BufferManager;

    if( this->BufferManager == NULL )
    {
	report( severity_error, "Player_Generic_c::GetBufferManager - BufferManager has not been registerred.\n" );
	return PlayerError;
    }

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Find the class instances associated with a stream playback
//

PlayerStatus_t   Player_Generic_c::GetClassList(PlayerStream_t		  Stream,
						Collator_t		 *Collator,
						FrameParser_t		 *FrameParser,
						Codec_t			 *Codec,
						OutputTimer_t		 *OutputTimer,
						Manifestor_t		 *Manifestor )
{
    if( Collator != NULL )
	*Collator	= Stream->Collator;

    if( FrameParser != NULL )
	*FrameParser	= Stream->FrameParser;

    if( Codec != NULL )
	*Codec		= Stream->Codec;

    if( OutputTimer != NULL )
	*OutputTimer	= Stream->OutputTimer;

    if( Manifestor != NULL )
	*Manifestor	= Stream->Manifestor;

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Find the decode buffer pool associated with a stream playback
//

PlayerStatus_t   Player_Generic_c::GetDecodeBufferPool(
						PlayerStream_t	 	  Stream,
						BufferPool_t		 *Pool )
{
    *Pool	= Stream->DecodeBufferPool;
    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Find the post processing control buffer pool associated with a stream playback
//

PlayerStatus_t   Player_Generic_c::GetPostProcessControlBufferPool(
						PlayerStream_t	 	  Stream,
						BufferPool_t		 *Pool )
{
    *Pool	= Stream->PostProcessControlBufferPool;
    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Discover a playback speed
//

PlayerStatus_t   Player_Generic_c::GetPlaybackSpeed(
						PlayerPlayback_t	  Playback,
						Rational_t		 *Speed,
						PlayDirection_t		 *Direction )
{
    *Speed	= Playback->Speed;
    *Direction	= Playback->Direction;

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Retrieve the presentation interval
//

PlayerStatus_t   Player_Generic_c::GetPresentationInterval(
						PlayerStream_t		  Stream,
						unsigned long long	 *IntervalStartNormalizedTime,
						unsigned long long	 *IntervalEndNormalizedTime )
{
unsigned long long 	Limit;

    if( IntervalStartNormalizedTime != NULL )
    {
	Limit	= Stream->Playback->PresentationIntervalReversalLimitStartNormalizedTime;

	*IntervalStartNormalizedTime	= (Limit == INVALID_TIME) ?
						Stream->RequestedPresentationIntervalStartNormalizedTime :
						max(Limit, Stream->RequestedPresentationIntervalStartNormalizedTime);
    }

    if( IntervalEndNormalizedTime != NULL )
    {
	Limit	= Stream->Playback->PresentationIntervalReversalLimitEndNormalizedTime;

	*IntervalEndNormalizedTime	= (Limit == INVALID_TIME) ?
						Stream->RequestedPresentationIntervalEndNormalizedTime :
						min(Limit, Stream->RequestedPresentationIntervalEndNormalizedTime);
    }

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Discover the status of a policy
//

unsigned char	Player_Generic_c::PolicyValue(	PlayerPlayback_t	  Playback,
						PlayerStream_t		  Stream,
						PlayerPolicy_t		  Policy )
{
unsigned char		 Value;
unsigned int		 Offset;
unsigned int		 Mask;

//

    if( (Playback		!= PlayerAllPlaybacks) &&
	(Stream			!= PlayerAllStreams) &&
	(Stream->Playback	!= Playback) )
    {
	report( severity_error, "Player_Generic_c::PolicyValue - Attempt to get policy on specific stream, and differing specific playback.\n" );
	return PlayerError;
    }

//

    if( Stream != PlayerAllStreams )
	Playback	= Stream->Playback;

//

    Offset		= Policy/32;
    Mask		= (1 << (Policy % 32));

    Value		= 0;

    if( (PolicyRecord.Specified[Offset] & Mask) != 0 )
	Value	= PolicyRecord.Value[Policy];

    if( Playback != PlayerAllPlaybacks )
    {
	if( (Playback->PolicyRecord.Specified[Offset] & Mask) != 0 )
	    Value	= Playback->PolicyRecord.Value[Policy];

	if( Stream != PlayerAllStreams )
	{
	    if( (Stream->PolicyRecord.Specified[Offset] & Mask) != 0 )
		Value	= Stream->PolicyRecord.Value[Policy];
	}
    }

    return Value;
}


