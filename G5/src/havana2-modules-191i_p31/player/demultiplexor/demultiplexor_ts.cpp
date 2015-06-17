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

Source file name : demultiplexor_base.cpp
Author :           Nick

Implementation of the base demultiplexor class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
13-Nov-06   Created                                         Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "demultiplexor_ts.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define MINIMUM_TIME_BETWEEN_GLITCH_PRINTS	2000	// Milli seconds

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//

Demultiplexor_Ts_c::Demultiplexor_Ts_c( void )
{
    //
    // If the base constructor failed just bomb out
    //

    if( InitializationStatus != DemultiplexorNoError )
	return;

    //
    // Now perform our initialization
    //

    InitializationStatus        = DemultiplexorError;

//

    Demultiplexor_Base_c::SetContextSize( sizeof(struct DemultiplexorContext_s) );

//

    InitializationStatus        = DemultiplexorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The add a stream to a context function
//

DemultiplexorStatus_t   Demultiplexor_Ts_c::GetHandledMuxType(
						PlayerInputMuxType_t     *HandledType )
{
    *HandledType        = MuxTypeTransportStream;
    return DemultiplexorError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The add a stream to a context function
//

DemultiplexorStatus_t   Demultiplexor_Ts_c::AddStream(
						DemultiplexorContext_t    Context,
						PlayerStream_t            Stream,
						unsigned int              StreamIdentifier )
{
DemultiplexorStatus_t     Status;
unsigned int PidTableIndex;

//

    Status      = Demultiplexor_Base_c::AddStream( Context, Stream, StreamIdentifier );
    if( Status != DemultiplexorNoError )
	return Status;

//

    PidTableIndex = (StreamIdentifier & (DVB_MAX_PIDS - 1));

    Context->PidTable[PidTableIndex]                                            = Context->Base.LastStreamSet + 1;
    Context->Streams[Context->Base.LastStreamSet].ValidExpectedContinuityCount  = false;
#ifdef DO_NICKS_SUGGESTED_IMPROVEMENTS
    Context->Streams[Context->Base.LastStreamSet].AccumulationBufferPointer     = 0;
#endif
    Context->Streams[Context->Base.LastStreamSet].SelectOnPriority              = ((StreamIdentifier & DEMULTIPLEXOR_SELECT_ON_PRIORITY) != 0);
    Context->Streams[Context->Base.LastStreamSet].DesiredPriority               = ((StreamIdentifier & DEMULTIPLEXOR_PRIORITY_HIGH) != 0);
    Context->Streams[Context->Base.LastStreamSet].TimeOfLastDiscontinuityPrint	= INVALID_TIME;
    Context->AddedNewStream                                                     = true;

//

    return DemultiplexorError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The remove a stream from a context function
//

DemultiplexorStatus_t   Demultiplexor_Ts_c::RemoveStream(
						DemultiplexorContext_t    Context,
						unsigned int              StreamIdentifier )
{
unsigned int    PidTableIndex;

    PidTableIndex       = (StreamIdentifier & (DVB_MAX_PIDS - 1));

    Context->PidTable[PidTableIndex]    = 0;

    return Demultiplexor_Base_c::RemoveStream( Context, StreamIdentifier );
}


// /////////////////////////////////////////////////////////////////////////
//
//      The input jump function resets continuity count preconceptions
//

DemultiplexorStatus_t   Demultiplexor_Ts_c::InputJump(
						DemultiplexorContext_t    Context )
{
unsigned int      i;

    //
    // Reset our expectations of a continuity count
    //

    for( i=0; i<DEMULTIPLEXOR_MAX_STREAMS; i++ )
	Context->Streams[i].ValidExpectedContinuityCount        = false;

//

    return Demultiplexor_Base_c::InputJump( Context );
}


// /////////////////////////////////////////////////////////////////////////
//
//      The demux function
//

DemultiplexorStatus_t   Demultiplexor_Ts_c::Demux(
						PlayerPlayback_t          Playback,
						DemultiplexorContext_t    Context,
						Buffer_t                  Buffer )
{
DemultiplexorStatus_t             Status;
unsigned int                      PacketStart;
unsigned int                      Entry;
unsigned int                      Header;
unsigned int                      Pid;
unsigned int                      DataOffset;
bool                              DeferredValidExpectedContinuityCount;
DemultiplexorStreamContext_t     *Stream;
DemultiplexorBaseStreamContext_t *BaseStream;

//

    Status      = Demultiplexor_Base_c::Demux( Playback, Context, Buffer );
    if( Status != DemultiplexorNoError )
	return Status;

//

    //
    // If the packet is a multiple of s BluRay packet assume they're BluRay packets
    //
    if( Context->AddedNewStream ||
       (Context->Base.BufferLength % (DVB_PACKET_SIZE + Context->BluRayExtraData)) != 0 )
    {
	Context->BluRayExtraData                        = 0;    // Default to DVB
	if( (Context->Base.BufferLength % (DVB_PACKET_SIZE + 4)) == 0)
	{
	    if( (Context->Base.BufferLength % DVB_PACKET_SIZE) == 0)
	    {
		unsigned int    PacketCount             = Context->Base.BufferLength / (DVB_PACKET_SIZE + 4);
		unsigned int    i;

		// Check 8 packets to see if sync byte is 192 or 188 aligned.  Only check 192 if not 188
		// If neither assume 188.
		if( PacketCount > 8 )
		    PacketCount                         = 8;
		for( i=0; i<PacketCount; i++ )
		{
		    if( Context->Base.BufferData[DVB_PACKET_SIZE*i] != DVB_SYNC_BYTE )
			break;
		}
		if( i != PacketCount )
		{
		    for( i=0; i<PacketCount; i++ )
		    {
			if( Context->Base.BufferData[4+((DVB_PACKET_SIZE+4)*i)] != DVB_SYNC_BYTE )
			    break;
		    }
		    if( i == PacketCount )
			Context->BluRayExtraData        = 4;
		}
	    }
	    else
		Context->BluRayExtraData                = 4;    // 192 but not 188 byte aligned - assume blu ray
	}

	Context->AddedNewStream = false;
    }

    if( (Context->Base.BufferLength % (DVB_PACKET_SIZE + Context->BluRayExtraData)) != 0)
    {
	report( severity_error, "Demultiplexor_Ts_c::Demux - Buffer length not whole number of packets %08x\n", Context->Base.BufferLength );
	return DemultiplexorError;
    }

    //
    // Packet handling loop we lock and unlock the mutex to allow 
    // stream removal/addition to be synchronized with packet handling.
    //

    OS_LockMutex( &Context->Base.Lock );
    for( PacketStart=0; PacketStart!=Context->Base.BufferLength; PacketStart+=(DVB_PACKET_SIZE + Context->BluRayExtraData) )
    {
      unsigned int NewPacketStart = PacketStart + Context->BluRayExtraData;

      // PB: No wonder UnlockMutex and LockMutex hit the profiler for transport stream, 
      //     lets deschedule and check whats on our queue every packet, that won't cost very much will it...  actually it probably won't
	//
	// Note we allow higher priority processes in here 
	// (for remove/add stream activities), due to
	// scheduling algorithms this does not nescessarily
	// allow access to same priority processes.
	//

	// Move these somewhere more sensible
#ifndef DO_NICKS_SUGGESTED_IMPROVEMENTS
	OS_UnLockMutex( &Context->Base.Lock );
	OS_LockMutex( &Context->Base.Lock );
#endif

//

	Header  = Context->Base.BufferData[NewPacketStart]        |
		  (Context->Base.BufferData[NewPacketStart+1] <<  8) |
		  (Context->Base.BufferData[NewPacketStart+2] << 16) |
		  (Context->Base.BufferData[NewPacketStart+3] << 24);

	//
	// Extract the pid, is it interesting
	//

	Pid             = DVB_PID(Header);
	if( Context->PidTable[Pid] == 0 )
	    continue;

	Entry           = Context->PidTable[Pid] - 1;
	BaseStream      = &Context->Base.Streams[Entry];
	Stream          = &Context->Streams[Entry];

	//
	// We are interested, Check validity of packet
	//

	if( !DVB_VALID_PACKET(Header) )
	{
	    report( severity_error, "Demultiplexor_Ts_c::Demux - Invalid packet (%02x %02x %02x %02x)\n",
			Context->Base.BufferData[NewPacketStart], Context->Base.BufferData[NewPacketStart+1], Context->Base.BufferData[NewPacketStart+2], Context->Base.BufferData[NewPacketStart+3] );
	    continue;
	}

	//
	// Handle adaptation field
	//

	DataOffset                              = DVB_HEADER_SIZE;
	DeferredValidExpectedContinuityCount    = true;
	if( DVB_ADAPTATION_FIELD(Header) )
	{
	    DataOffset += 1 + Context->Base.BufferData[NewPacketStart+DVB_HEADER_SIZE];

	    if( (Context->Base.BufferData[NewPacketStart+DVB_HEADER_SIZE] != 0) &&
		((Context->Base.BufferData[NewPacketStart+DVB_HEADER_SIZE+1] & DVB_DISCONTINUITY_INDICATOR_MASK) != 0) )
		DeferredValidExpectedContinuityCount        = false;
	}

	//
	// Perform continuity check
	//

	if( Stream->ValidExpectedContinuityCount &&
	    (DVB_CONTINUITY_COUNT(Header) != Stream->ExpectedContinuityCount) )
	{
	unsigned long long 	Now = OS_GetTimeInMilliSeconds();

	    // 
	    // Check for repeat packet - if so skip whole packet
	    //

	    if( ((DVB_CONTINUITY_COUNT(Header) + 1) & 0x0f) == Stream->ExpectedContinuityCount )
		continue;

	    if( !ValidTime(Stream->TimeOfLastDiscontinuityPrint) || ((Now - Stream->TimeOfLastDiscontinuityPrint) > MINIMUM_TIME_BETWEEN_GLITCH_PRINTS) )
	    {
		report( severity_error, "Demultiplexor_Ts_c::Demux - Noted a continuity count error, forcing a glitch (sparse print).\n" );
		Stream->TimeOfLastDiscontinuityPrint	= Now;
	    }
	    Player->InputGlitch( PlayerAllPlaybacks, BaseStream->Stream );
	}

	Stream->ExpectedContinuityCount       = (DVB_CONTINUITY_COUNT(Header) + 1) & 0x0f;
	Stream->ValidExpectedContinuityCount  = DeferredValidExpectedContinuityCount;

	//
	// Optionally discard packets of inappropriate priority (note that priority filtering
	// must take place after the continuity check).
	//

	if( Stream->SelectOnPriority && Stream->DesiredPriority != (bool) DVB_PRIORITY(Header) )
		continue;

	//
	// Pass on to the appropriate collator
	//

	if( DVB_PAYLOAD_PRESENT(Header) && (DataOffset < DVB_PACKET_SIZE) )
	{
	    //
	    // Ignore return status (others may be interested in this stream)
	    //
#ifdef DO_NICKS_SUGGESTED_IMPROVEMENTS
	  if ( (Stream->AccumulationBufferPointer + (DVB_PACKET_SIZE-DataOffset)) > ACCUMULATION_BUFFER_SIZE ) {
	    BaseStream->Collator->Input( Context->Base.Descriptor, 
					 Stream->AccumulationBufferPointer,
					 Stream->AccumulationBuffer);

	    Stream->AccumulationBufferPointer = 0;

	    // This is really really bad is it ever safe to do this????
	    OS_UnLockMutex( &Context->Base.Lock );
	    OS_LockMutex( &Context->Base.Lock );
	  }

	  memcpy(&Stream->AccumulationBuffer[Stream->AccumulationBufferPointer],&Context->Base.BufferData[NewPacketStart+DataOffset],DVB_PACKET_SIZE-DataOffset);
	  Stream->AccumulationBufferPointer += DVB_PACKET_SIZE-DataOffset;
#else
	    BaseStream->Collator->Input( Context->Base.Descriptor, 
					  DVB_PACKET_SIZE-DataOffset, 
					  &Context->Base.BufferData[NewPacketStart+DataOffset] );           
#endif

	}
    }

#ifdef DO_NICKS_SUGGESTED_IMPROVEMENTS
    for (Entry = 0; Entry < DEMULTIPLEXOR_MAX_STREAMS; Entry++)
    {
	BaseStream      = &Context->Base.Streams[Entry];
	Stream          = &Context->Streams[Entry];

	if (BaseStream->Stream && Stream->AccumulationBufferPointer)
	{
	    BaseStream->Collator->Input( Context->Base.Descriptor, 
					 Stream->AccumulationBufferPointer,
					 Stream->AccumulationBuffer);

	    Stream->AccumulationBufferPointer = 0;
	}
    }
#endif

    OS_UnLockMutex( &Context->Base.Lock );

//

    return DemultiplexorNoError;
}


