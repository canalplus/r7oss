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

Source file name : player_construct.cpp
Author :           Nick

Implementation of the constructor and destructor functions of the
generic class implementation of player 2


Date        Modification                                    Name
----        ------------                                    --------
03-Nov-06   Created                                         Nick

************************************************************************/

#include "player_generic.h"
#include "st_relay.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Contructor function - Initialize our data
//

Player_Generic_c::Player_Generic_c( void )
{
unsigned int    i;

//

    InitializationStatus        = BufferError;

//

    OS_InitializeMutex( &Lock );

    ShutdownPlayer              = false;

    BufferManager               = NULL;
    PlayerControlStructurePool  = NULL;
    InputBufferPool             = NULL;
    DemultiplexorCount          = 0;
    ListOfPlaybacks             = NULL;

    AudioCodedFrameCount	= PLAYER_AUDIO_DEFAULT_CODED_FRAME_COUNT;
    AudioCodedMemorySize	= PLAYER_AUDIO_DEFAULT_CODED_MEMORY_SIZE;
    AudioCodedFrameMaximumSize	= PLAYER_AUDIO_DEFAULT_CODED_FRAME_MAXIMUM_SIZE;
    VideoCodedFrameCount	= PLAYER_VIDEO_DEFAULT_CODED_FRAME_COUNT;
    VideoCodedMemorySize	= PLAYER_VIDEO_DEFAULT_CODED_MEMORY_SIZE;
    VideoCodedFrameMaximumSize	= PLAYER_VIDEO_DEFAULT_CODED_FRAME_MAXIMUM_SIZE;
    OtherCodedFrameCount	= PLAYER_OTHER_DEFAULT_CODED_FRAME_COUNT;
    OtherCodedMemorySize	= PLAYER_OTHER_DEFAULT_CODED_MEMORY_SIZE;
    OtherCodedFrameMaximumSize	= PLAYER_OTHER_DEFAULT_CODED_FRAME_MAXIMUM_SIZE;

    strcpy( AudioCodedMemoryPartitionName, PLAYER_AUDIO_DEFAULT_CODED_FRAME_PARTITION_NAME );
    strcpy( VideoCodedMemoryPartitionName, PLAYER_VIDEO_DEFAULT_CODED_FRAME_PARTITION_NAME );
    strcpy( OtherCodedMemoryPartitionName, PLAYER_OTHER_DEFAULT_CODED_FRAME_PARTITION_NAME );

    memset( &PolicyRecord, 0x00, sizeof(PlayerPolicyState_t) );

    EventListHead               = INVALID_INDEX;
    EventListTail               = INVALID_INDEX;
    for( i=0; i<PLAYER_MAX_OUTSTANDING_EVENTS; i++ )
	EventList[i].Record.Code = EventIllegalIdentifier;

    for( i=0; i<PLAYER_MAX_EVENT_SIGNALS; i++ )
	ExternalEventSignals[i].Signal  = NULL;

    OS_InitializeEvent( &InternalEventSignal );

//

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, (PlayerPolicy_t)PolicyPlayoutAlwaysPlayout,		PolicyValuePlayout );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, (PlayerPolicy_t)PolicyPlayoutAlwaysDiscard,		PolicyValueDiscard );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyMasterClock,						PolicyValueVideoClockMaster );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyManifestFirstFrameEarly,				PolicyValueApply );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyExternalTimeMapping,					PolicyValueDisapply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyExternalTimeMappingVsyncLocked,			PolicyValueDisapply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyAVDSynchronization,   				PolicyValueApply );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyClampPlaybackIntervalOnPlaybackDirectionChange,	PolicyValueApply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyTrickModeDomain,	 				PolicyValueTrickModeAuto );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, (PlayerPolicy_t)PolicyStatisticsOnAudio,			PolicyValueDisapply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, (PlayerPolicy_t)PolicyStatisticsOnVideo,			PolicyValueDisapply );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyDiscardLateFrames,	 				PolicyValueDiscardLateFramesAfterSynchronize );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyVideoStartImmediate,					PolicyValueApply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicySyncStartImmediate,   				PolicyValueDisapply );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyRebaseOnFailureToDeliverDataInTime,			PolicyValueApply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyRebaseOnFailureToDecodeInTime,			PolicyValueApply );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyH264AllowNonIDRResynchronization,			PolicyValueApply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyH264AllowBadPreProcessedFrames,			PolicyValueApply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyH264TreatDuplicateDpbValuesAsNonReferenceFrameFirst,	PolicyValueApply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyH264ForcePicOrderCntIgnoreDpbDisplayFrameOrdering,	PolicyValueDisapply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyH264TreatTopBottomPictureStructAsInterlaced,		PolicyValueApply );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyMPEG2ApplicationType,				PolicyValueMPEG2ApplicationDvb );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyMPEG2DoNotHonourProgressiveFrameFlag,		PolicyValueDisapply );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyClockPullingLimit2ToTheNPartsPerMillion,		8 );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyLimitInputInjectAhead,				PolicyValueDisapply );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyDecimateDecoderOutput,				PolicyValueDecimateDecoderOutputDisabled);

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicySymmetricJumpDetection,				PolicyValueDisapply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyPtsForwardJumpDetectionThreshold,			4 );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyPixelAspectRatioCorrection, 				PolicyValuePixelAspectRatioCorrectionDisabled);

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyAllowFrameDiscardAtNormalSpeed,			PolicyValueDisapply );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyOperateCollator2InReversibleMode,			PolicyValueDisapply );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyVideoOutputWindowResizeSteps,			1 );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyIgnoreStreamUnPlayableCalls,				PolicyValueDisapply );

    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyUsePTSDeducedDefaultFrameRates,			PolicyValueApply );

    //
    // Here sits Nicks debug setting for player policies, do not add normal initialization after this point
    //

#if 0
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyAVDSynchronization,   				PolicyValueDisapply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyDiscardLateFrames,	 				PolicyValueDiscardLateFramesNever );
//    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyMasterClock,                                         PolicyValueSystemClockMaster );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyAllowFrameDiscardAtNormalSpeed,			PolicyValueApply );
    SetPolicy( PlayerAllPlaybacks, PlayerAllStreams, PolicyRebaseOnFailureToDecodeInTime,			PolicyValueDisapply );
#endif

//

    InitializationStatus        = BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Destructor function - close down
//

Player_Generic_c::~Player_Generic_c( void )
{
    ShutdownPlayer              = true;

    OS_TerminateEvent( &InternalEventSignal );
    OS_TerminateMutex( &Lock );
}


