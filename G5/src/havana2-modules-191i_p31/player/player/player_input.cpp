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

Source file name : player_input.cpp
Author :           Nick

Implementation of the data input related functions of the
generic class implementation of player 2


Date        Modification                                    Name
----        ------------                                    --------
06-Nov-06   Created                                         Nick

************************************************************************/

#include "player_generic.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Get a buffer to inject input data
//

PlayerStatus_t   Player_Generic_c::GetInjectBuffer( Buffer_t             *Buffer )
{
PlayerStatus_t    Status;

//

    Status      = InputBufferPool->GetBuffer( Buffer, IdentifierGetInjectBuffer );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::GetInjectBuffer - Failed to get an input buffer.\n" );
	return PlayerError;
    }

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Inject input data to the player
//

PlayerStatus_t   Player_Generic_c::InjectData(  PlayerPlayback_t          Playback,
						Buffer_t                  Buffer )
{
unsigned int              i;
unsigned int              Length;
void                     *Data;
PlayerInputMuxType_t      MuxType;
PlayerStatus_t            Status;
PlayerInputDescriptor_t  *Descriptor;

//

    Status      = Buffer->ObtainMetaDataReference( MetaDataInputDescriptorType, (void **)(&Descriptor) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::InjectData - Unable to obtain the meta data input descriptor.\n" );
	return Status;
    }

//

    if( Descriptor->MuxType == MuxTypeUnMuxed )
    {
	//
	// Un muxed data, call the appropriate collator
	//

	Status  = Buffer->ObtainDataReference( NULL, &Length, &Data );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::InjectData - unable to obtain data reference.\n" );
	    return Status;
	}

	Status  = Descriptor->UnMuxedStream->Collator->Input( Descriptor, Length, Data );
    }
    else
    {
	//
	// Data is muxed - seek a demultiplexor and pass on the call
	//

	for( i=0; i<DemultiplexorCount; i++ )
	{
	    Demultiplexors[i]->GetHandledMuxType( &MuxType );
	    if( MuxType == Descriptor->MuxType )
		break;
	}

	if( i < DemultiplexorCount )
	{
	    Status = Demultiplexors[i]->Demux( Playback, Descriptor->DemultiplexorContext, Buffer );
	}
	else
	{
	    report( severity_error, "Player_Generic_c::InjectData - No suitable demultiplexor registerred for this MuxType (%d).\n", Descriptor->MuxType );
	    Status = PlayerUnknowMuxType;
	}       
    }

    //
    // Release the buffer
    //

    Buffer->DecrementReferenceCount( IdentifierGetInjectBuffer );
    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Signal an input jump to the appropriate components
//

PlayerStatus_t   Player_Generic_c::InputJump(   PlayerPlayback_t          Playback,
						PlayerStream_t            Stream,
						bool                      SurplusDataInjected,
						bool                      ContinuousReverseJump )
{
PlayerStatus_t            Status;
PlayerStatus_t            CurrentStatus;
PlayerPlayback_t          SubPlayback;
PlayerStream_t            SubStream;

    //
    // Check parameters
    //

    if( (Playback               != PlayerAllPlaybacks) &&
	(Stream                 != PlayerAllStreams) &&
	(Stream->Playback       != Playback) )
    {
	report( severity_error, "Player_Generic_c::InputJump - Attempt to signal input jump on specific stream, and differing specific playback.\n" );
	return PlayerError;
    }

//

    if( Stream != PlayerAllStreams )
	Playback        = Stream->Playback;

    //
    // Perform two nested loops over affected playbacks and streams.
    // These should terminate appropriately for specific playbacks/streams
    // During the loops we accumulate a global status.
    //

    Status      = PlayerNoError;

    for( SubPlayback     = ((Playback == PlayerAllPlaybacks) ? ListOfPlaybacks : Playback);
	 ((Playback == PlayerAllPlaybacks) ? (SubPlayback != NULL) : (SubPlayback == Playback));
	 SubPlayback     = SubPlayback->Next )
    {
	for( SubStream   = ((Stream == PlayerAllStreams) ? SubPlayback->ListOfStreams : Stream);
	     ((Stream == PlayerAllStreams) ? (SubStream != NULL) : (SubStream == Stream));
	     SubStream   = SubStream->Next )
	{
	    CurrentStatus       = SubStream->Collator->InputJump( SurplusDataInjected, ContinuousReverseJump );
	    if( CurrentStatus != PlayerNoError )
		Status          = CurrentStatus;

	    if( SubStream->Demultiplexor != NULL )
	    {
		CurrentStatus   = SubStream->Demultiplexor->InputJump( SubStream->DemultiplexorContext );
		if( CurrentStatus != PlayerNoError )
		    Status      = CurrentStatus;
	    }
	}
    }

//

    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Signal an input glitch to the appropriate components
//

PlayerStatus_t   Player_Generic_c::InputGlitch( PlayerPlayback_t          Playback,
						PlayerStream_t            Stream )
{
PlayerStatus_t            Status;
PlayerStatus_t            CurrentStatus;
PlayerPlayback_t          SubPlayback;
PlayerStream_t            SubStream;

    //
    // Check parameters
    //

    if( (Playback               != PlayerAllPlaybacks) &&
	(Stream                 != PlayerAllStreams) &&
	(Stream->Playback       != Playback) )
    {
	report( severity_error, "Player_Generic_c::InputGlitch - Attempt to signal input jump on specific stream, and differing specific playback.\n" );
	return PlayerError;
    }

//

    if( Stream != PlayerAllStreams )
	Playback        = Stream->Playback;

    //
    // Perform two nested loops over affected playbacks and streams.
    // These should terminate appropriately for specific playbacks/streams
    // During the loops we accumulate a global status.
    //

    Status      = PlayerNoError;

    for( SubPlayback     = ((Playback == PlayerAllPlaybacks) ? ListOfPlaybacks : Playback);
	 ((Playback == PlayerAllPlaybacks) ? (SubPlayback != NULL) : (SubPlayback == Playback));
	 SubPlayback     = SubPlayback->Next )
    {
	for( SubStream   = ((Stream == PlayerAllStreams) ? SubPlayback->ListOfStreams : Stream);
	     ((Stream == PlayerAllStreams) ? (SubStream != NULL) : (SubStream == Stream));
	     SubStream   = SubStream->Next )
	{
	    CurrentStatus       = SubStream->Collator->InputGlitch();
	    if( CurrentStatus != PlayerNoError )
		Status          = CurrentStatus;
	}
    }

//

    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Register/deregister demultiplexors with particular streams
//


PlayerStatus_t   Player_Generic_c::AttachDemultiplexor( 
						PlayerStream_t            Stream,
						Demultiplexor_t           Demultiplexor,
						DemultiplexorContext_t    Context )
{
    Stream->Demultiplexor               = Demultiplexor;
    Stream->DemultiplexorContext        = Context;

    return PlayerNoError;
}

//

PlayerStatus_t   Player_Generic_c::DetachDemultiplexor( 
						PlayerStream_t            Stream )
{
    Stream->Demultiplexor               = NULL;

    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Given that one stream is suffering from a failure to deliver data in time
//	what is the status on other streams in the same multiplex.
//


PlayerStatus_t   Player_Generic_c::CheckForDemuxBufferMismatch(
						PlayerPlayback_t          Playback,
						PlayerStream_t            Stream )
{
PlayerStream_t	OtherStream;
unsigned int	CodedFrameBufferCount;
unsigned int	CodedFrameBuffersInUse;
unsigned int	MemoryInPool;
unsigned int	MemoryAllocated;

    for( OtherStream	= Playback->ListOfStreams;
	 OtherStream   != NULL;
	 OtherStream	= OtherStream->Next )
	if( Stream != OtherStream )
	{
	    OtherStream->CodedFrameBufferPool->GetPoolUsage(	&CodedFrameBufferCount,
								&CodedFrameBuffersInUse,
								&MemoryInPool, 
								&MemoryAllocated, NULL );

	    if( ((CodedFrameBuffersInUse * 10) >= (CodedFrameBufferCount * 9)) ||
		((MemoryAllocated * 10) >= (MemoryInPool * 9)) )
	    {
		report( severity_info, "\t\tStream(%s) appears to have filled all it's input buffers,\n\t\tprobable inappropriate buffer sizing for the multiplexed stream.\n",
			ToString(OtherStream->StreamType) );
		break;
	    }
	}

    return PlayerNoError;
}


