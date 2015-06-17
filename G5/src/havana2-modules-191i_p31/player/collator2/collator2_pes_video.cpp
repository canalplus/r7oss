/************************************************************************
Copyright (C) 2006, 2009 STMicroelectronics. All Rights Reserved.

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

Source file name : collator2_pes_video.cpp
Author :           Nick

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
19-Apr-07   Created from existing collator2_pes_video.cpp    Daniel
13-Oct-09   Rejigged from existing collator_pes_video.cpp   Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator2_pes_video.h"
#include "st_relay.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define ZERO_START_CODE_HEADER_SIZE     7               // Allow us to see 00 00 01 00 00 01 <other code>

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

////////////////////////////////////////////////////////////////////////////
//
//     Handle input, by scanning the start codes, and chunking the data
//
//  Note 1:
//
//     Since we are accepting pes encapsulated data, and 
//     junking the pes header, we need to accumulate the 
//    pes header, before parsing and junking it.
//
//  Note 2:
//
//    We also need to accumulate and parse padding headers 
//    to allow the skipping of pad data.
//
//  Note 3:
//
//    In general we deal with only 4 byte codes, so we do not
//    need to accumulate more than 4 bytes for a code.
//    A special case for this is code Zero. Pes packets may
//    partition the input at any point, so standard start 
//    codes can span a pes packet header, this is no problem 
//    so long as we check the accumulated bytes alongside new 
//    bytes after skipping a pes header. 
//
//  Note 4:
//
//    The zero code special case, when we see 00 00 01 00, we
//    need to accumulate a further 3 bytes this is because we
//    can have the special case of
//
//            00 00 01 00 00 01 <pes/packing start code>
//
//    where the first start code lead in has a terminal byte 
//    later in the stream which may lead to a completely different 
//    code. If we see 00 00 01 00 00 01 we always ignore the first 
//    code, as in a legal DVB stream this must be followed by a
//    pes/packing code of some sort, we accumulate the 3rd byte to
//    determine which
//

CollatorStatus_t   Collator2_PesVideo_c::ProcessInputForward(
	                                        unsigned int		  DataLength,
	                                        void			 *Data,
						unsigned int		 *DataLengthRemaining )
{
CollatorStatus_t        Status;
unsigned int		MinimumHeadroom;
unsigned int            Transfer;
unsigned int            Skip;
unsigned int            SpanningWord;
unsigned int            StartingWord;
unsigned int            SpanningCount;
unsigned int            CodeOffset;
unsigned char           Code;
bool			Loop;
bool			Exit;
bool			BlockTerminate;
FrameParserHeaderFlag_t	HeaderFlags;

//

    st_relayfs_write(ST_RELAY_TYPE_PES_VIDEO_BUFFER, ST_RELAY_SOURCE_VIDEO_COLLATOR, (unsigned char *)Data, DataLength, 0 );

    //
    // Initialize scan state
    //

    RemainingData       = (unsigned char *)Data;
    RemainingLength     = DataLength;
    Status		= CollatorNoError;

    while( RemainingLength != 0 )
    {
	//
	// Allow any higher priority player processes in
	// NOTE On entry we have the partition lock
	//

	OS_UnLockMutex( &PartitionLock );
	OS_LockMutex( &PartitionLock );

	//
	// Do we have room to accumulate, if not we try passing on the 
	// accumulated partitions and extending the buffer we have.
	//

	MinimumHeadroom	= GotPartialHeader ? max(MINIMUM_ACCUMULATION_HEADROOM, GotPartialDesiredSize) : MINIMUM_ACCUMULATION_HEADROOM;
	if( (CodedFrameBufferFreeSpace < MinimumHeadroom) || (PartitionPointUsedCount >= (MAXIMUM_PARTITION_POINTS-1)) )
	{
	    if( MinimumHeadroom > max(MINIMUM_ACCUMULATION_HEADROOM, (MaximumCodedFrameSize-16)) )
		report( severity_fatal, "Collator2_PesVideo_c::ProcessInputForward - Required headroom too large (0x%08x bytes) - Probable implementation error.\n", MinimumHeadroom );

	    Status	= PartitionOutput();
	    if( Status != CollatorNoError )
	    {
		report( severity_error, "Collator2_PesVideo_c::ProcessInputForward - Output of partitions failed.\n" );
		break;
	    }

	    if( (CodedFrameBufferFreeSpace < MinimumHeadroom) || (PartitionPointUsedCount >= (MAXIMUM_PARTITION_POINTS-1)) )
	    {
		if( !NonBlockingInput )
		    report( severity_fatal, "Collator2_PesVideo_c::ProcessInputForward - About to return CollatorWouldBlock when it is ok to block - Probable implementation error.\n" ); 

		Status	= CollatorWouldBlock;
		break;
	    }
	}

	//
	// Are we accumulating an extended header
	//

	if( GotPartialHeader )
	{
	    if( GotPartialCurrentSize < GotPartialDesiredSize )
	    {
	        Transfer	=  min( RemainingLength, (GotPartialDesiredSize - GotPartialCurrentSize) );

		Status	= AccumulateData( Transfer, RemainingData );
		if( Status != CollatorNoError )
		{
		    EmptyCurrentPartition(); 	           // Dump any collected data in the current partition
		    InitializePartition();
		    GotPartialHeader		= false;
		    DiscardingData		= false;
		    break;
		}

	    	GotPartialCurrentSize	+= Transfer;
	    	RemainingData		+= Transfer;
	    	RemainingLength		-= Transfer;
	    }

	    if( GotPartialCurrentSize >= GotPartialDesiredSize )
	    {
		Loop	= false;
		Exit	= false;

		StoredPartialHeader		= NextPartition->PartitionBase + NextPartition->PartitionSize - GotPartialCurrentSize;
		switch( GotPartialType )
		{
		    case HeaderZeroStartCode:
				if( (StoredPartialHeader[4] == 0x00) && (StoredPartialHeader[5] == 0x01) )
				{
				    GotPartialType		 = (StoredPartialHeader[6] == PES_PADDING_START_CODE) ? HeaderPaddingStartCode : HeaderPesStartCode;
				    GotPartialDesiredSize	 = (StoredPartialHeader[6] == PES_PADDING_START_CODE) ? PES_PADDING_INITIAL_HEADER_SIZE : PES_INITIAL_HEADER_SIZE;

				    StoredPartialHeader		+= 3;
				    GotPartialCurrentSize	 = 4;
				}
				else
				{
				    GotPartialDesiredSize	 = 4 + FrameParser->RequiredPresentationLength( 0x00 );
				    GotPartialType		 = HeaderGenericStartCode;
				}

				Loop			 	= true;
				break;

//

		    case HeaderPesStartCode:
				if( GotPartialCurrentSize >= PES_INITIAL_HEADER_SIZE )
				    GotPartialDesiredSize	= PES_HEADER_SIZE(StoredPartialHeader);

				if( GotPartialCurrentSize < GotPartialDesiredSize )
				{
				    Loop			= true;
				    break;
				}


				DiscardingData			= false;
				GotPartialHeader		= false;
				MoveCurrentPartitionBoundary( -GotPartialCurrentSize );			// Wind the partition back to release the header

				Status				= ReadPesHeader( StoredPartialHeader );
				if( Status != CollatorNoError )
				{
				    Exit			= true;
				    break;
				}

				if( DiscardingData )
				{
				    EmptyCurrentPartition(); 	           // Dump any collected data in the current partition
				    InitializePartition();
				    DiscardingData		= false;
				}

				//
				// Do we need to write the pts into the current partition
				//

				if( (NextPartition->PartitionSize == 0) && PlaybackTimeValid )
				{
				    NextPartition->CodedFrameParameters.PlaybackTimeValid   = PlaybackTimeValid;
				    NextPartition->CodedFrameParameters.PlaybackTime        = PlaybackTime;
				    PlaybackTimeValid                                       = false;
				    NextPartition->CodedFrameParameters.DecodeTimeValid     = DecodeTimeValid;
				    NextPartition->CodedFrameParameters.DecodeTime          = DecodeTime;
				    DecodeTimeValid                                         = false;
				}
				break;

//

		    case HeaderPaddingStartCode:
				Skipping			= PES_PADDING_SKIP(StoredPartialHeader);
				GotPartialHeader		= false;
				MoveCurrentPartitionBoundary( -GotPartialCurrentSize );			// Wind the partition back to release the header

				break;

//

		    case HeaderGenericStartCode:
				//
				// Is it going to terminate a frame
				//

				Code				= StoredPartialHeader[3];

				FrameParser->PresentCollatedHeader( Code, (StoredPartialHeader+4), &HeaderFlags );

				NextPartition->FrameFlags	|= HeaderFlags;
				BlockTerminate			= (HeaderFlags & FrameParserHeaderFlagPartitionPoint) != 0;

				GotPartialHeader		= false;

				if( BlockTerminate )
				{
				    MoveCurrentPartitionBoundary( -GotPartialCurrentSize );			// Wind the partition back to release the header
				    memcpy( CopyOfStoredPartialHeader, StoredPartialHeader, GotPartialCurrentSize );
				    AccumulateOnePartition();
				    AccumulateData( GotPartialCurrentSize, CopyOfStoredPartialHeader );

				    PartitionPointSafeToOutputCount	= PartitionPointUsedCount;
				    DiscardingData			= false;
				}

				//
				// Accumulate it in any event
				//

				Status      = AccumulateStartCode( PackStartCode(NextPartition->PartitionSize-GotPartialCurrentSize, Code) );
				if( Status != CollatorNoError )
				{
				    EmptyCurrentPartition(); 	           // Dump any collected data in the current partition
				    InitializePartition();
				    Exit	= true;
				    break;
				}

				//
				// If we had a block terminate, then we need to check that there is sufficient 
				// room to accumulate the new frame, or we may end up copying flipping great 
				// wodges of data later.
				//

				if( BlockTerminate && (CodedFrameBufferFreeSpace < (LargestFrameSeen + MINIMUM_ACCUMULATION_HEADROOM)) )
				{
				    Status	= PartitionOutput();
				    if( Status != CollatorNoError )
	    			    {
					report( severity_error, "Collator2_PesVideo_c::ProcessInputForward - Output of partitions failed.\n" );
					Exit	= true;
					break;
				    }
				}

				break;
		}

		if( Exit )
		    break;

		if( Loop )
		    continue;
	    }

            if( RemainingLength == 0 )
		break;
	}

	//
	// Are we skipping padding
	//

	if( Skipping != 0 )
	{
	    Skip                 = min( Skipping, RemainingLength );
	    RemainingData       += Skip;
	    RemainingLength     -= Skip;
	    Skipping            -= Skip;

	    if( RemainingLength == 0 )
		break;
	}

	//
	// Check for spanning header
	//

	SpanningWord             = 0xffffffff << (8 * min(NextPartition->PartitionSize,3));
	SpanningWord            |= NextPartition->PartitionBase[NextPartition->PartitionSize-3] << 16;
	SpanningWord            |= NextPartition->PartitionBase[NextPartition->PartitionSize-2] << 8;
	SpanningWord            |= NextPartition->PartitionBase[NextPartition->PartitionSize-1];

	StartingWord             = 0x00ffffff >> (8 * min((RemainingLength-1),3));
	StartingWord            |= RemainingData[0] << 24;
	StartingWord            |= RemainingData[1] << 16;
	StartingWord            |= RemainingData[2] <<  8;

	//
	// Check for a start code spanning, or in the first word
	// record the nature of the span in a counter indicating how many 
	// bytes of the code are in the remaining data. 
	// NOTE the 00 at the bottom indicates we have a byte for the code, 
	//      not what it is.
	//

	SpanningCount           = 0;

	if( (SpanningWord << 8) == 0x00000100 )
	{
	    SpanningCount       = 1;
	}
	else if( ((SpanningWord << 16) | ((StartingWord >> 16) & 0xff00)) == 0x00000100 )
	{
	    SpanningCount       = 2;
	}
	else if( ((SpanningWord << 24) | ((StartingWord >> 8)  & 0xffff00)) == 0x00000100 )
	{
	    SpanningCount       = 3;
	}
	else if( StartingWord == 0x00000100 )
	{
	    SpanningCount               = 4;
	    UseSpanningTime             = false;
	    SpanningPlaybackTimeValid   = false;
	    SpanningDecodeTimeValid     = false;
	}

	//
	// Check that if we have a spanning code, that the code is not to be ignored
	//

	if( (SpanningCount != 0) && inrange(RemainingData[SpanningCount-1], Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd) )
	    SpanningCount       = 0;

	//
	// Handle a spanning start code
	//

	if( SpanningCount != 0 )
	{
	    //
	    // Copy over the spanning bytes
	    //

	    AccumulateData( SpanningCount, RemainingData );
	    RemainingData       	+= SpanningCount;
	    RemainingLength     	-= SpanningCount;
	}

	//
	// Handle search for next start code
	//

	else
	{
	    //
	    // If we had no spanning code, but we had a spanning PTS, and we 
	    // had no normal PTS for this frame, then copy the spanning time 
	    // to the normal time.
	    //

	    if( !PlaybackTimeValid && SpanningPlaybackTimeValid )
	    {
		PlaybackTimeValid		= SpanningPlaybackTimeValid;
		PlaybackTime			= SpanningPlaybackTime;
		DecodeTimeValid			= SpanningDecodeTimeValid;
		DecodeTime			= SpanningDecodeTime;
		UseSpanningTime			= false;
		SpanningPlaybackTimeValid	= false;
		SpanningDecodeTimeValid		= false;
	    }

	    //
	    // Get a new start code
	    //

	    Status      = FindNextStartCode( &CodeOffset );
	    if( Status != CollatorNoError )
	    {
		//
		// No start code, copy remaining data into buffer, and exit
		//

		Status  = AccumulateData( RemainingLength, RemainingData );

		if( Status != CollatorWouldBlock )
		    RemainingLength	= 0;

		if( (Status != CollatorNoError) && (Status != CollatorWouldBlock) )
		    DiscardingData	= true;

		break;
	    }

	    //
	    // Got a start code accumulate upto it, and process
	    //

	    Status      = AccumulateData( CodeOffset+4, RemainingData );

	    if( Status != CollatorNoError )
	    {
		if( Status != CollatorWouldBlock )
		    DiscardingData	= true;

		break;
	    }

	    RemainingLength                     -= CodeOffset+4;
	    RemainingData                       += CodeOffset+4;
	}

	//
	// Now process the code, whether from spanning, or from search
	//

	GotPartialHeader		= true;
	GotPartialCurrentSize		= 4;

	StoredPartialHeader		= NextPartition->PartitionBase + NextPartition->PartitionSize - GotPartialCurrentSize;
	Code                 		= StoredPartialHeader[3];

	if( Code == 0x00 )
	{
	    GotPartialType		= HeaderZeroStartCode;
	    GotPartialDesiredSize	= ZERO_START_CODE_HEADER_SIZE;
	}
	else if( IS_PES_START_CODE_VIDEO(Code) )
	{
	    if( (Code & Configuration.StreamIdentifierMask) == Configuration.StreamIdentifierCode )
	    {
		GotPartialType		= HeaderPesStartCode;
		GotPartialDesiredSize	= PES_INITIAL_HEADER_SIZE;
	    }
	    else
	    {
		// Not interested
		GotPartialHeader	= false;
		DiscardingData		= true;
	    }
	}
	else if( Code == PES_PADDING_START_CODE )
	{
	    GotPartialType		= HeaderPaddingStartCode;
	    GotPartialDesiredSize	= PES_PADDING_INITIAL_HEADER_SIZE;
	}
	else if( DiscardingData )
	{
	    // If currently seeking a pes header then ignore the last case of a generic header
	    GotPartialHeader		= false;
	    EmptyCurrentPartition();
	}
	else 
	{
	    // A generic start code
	    GotPartialType		= HeaderGenericStartCode;

	    GotPartialDesiredSize	 = 4 + FrameParser->RequiredPresentationLength( Code );
	}
    }

//

    if( DataLengthRemaining != NULL )
	*DataLengthRemaining	= RemainingLength;

    return Status;
}


////////////////////////////////////////////////////////////////////////////
//
//     Handle input, by scanning the start codes, and chunking the data
//     NOTE Most of the notes above also apply here.
//

CollatorStatus_t   Collator2_PesVideo_c::ProcessInputBackward(
	                                        unsigned int		  DataLength,
	                                        void			 *Data,
						unsigned int		 *DataLengthRemaining )
{
CollatorStatus_t        Status;
unsigned int            SpanningWord;
unsigned int            StartingWord;
unsigned int            SpanningCount;
unsigned int            CodeOffset;
unsigned char           Code;
unsigned int 		AccumulationToCaptureCode;
bool			BlockTerminate;
FrameParserHeaderFlag_t	HeaderFlags;

//

    st_relayfs_write(ST_RELAY_TYPE_PES_VIDEO_BUFFER, ST_RELAY_SOURCE_VIDEO_COLLATOR, (unsigned char *)Data, DataLength, 0 );

    //
    // Initialize scan state
    //

    RemainingData       = (unsigned char *)Data;
    RemainingLength     = DataLength;
    Status		= CollatorNoError;

    while( RemainingLength != 0 )
    {
	//
	// Allow any higher priority player processes in
	// NOTE On entry we have the partition lock
	//

	OS_UnLockMutex( &PartitionLock );
	OS_LockMutex( &PartitionLock );

	//
	// Do we have room to accumulate, if not we try passing on the 
	// accumulated partitions and extending the buffer we have.
	//

	if( CodedFrameBufferFreeSpace < MINIMUM_ACCUMULATION_HEADROOM )
	{
	    Status	= PartitionOutput();
	    if( Status != CollatorNoError )
	    {
		report( severity_error, "Collator2_PesVideo_c::ProcessInputBackward - Output of partitions failed.\n" );
		break;
	    }

	    if( CodedFrameBufferFreeSpace < MINIMUM_ACCUMULATION_HEADROOM )
	    {
		if( !NonBlockingInput )
		    report( severity_fatal, "Collator2_PesVideo_c::ProcessInputBackward - About to return CollatorWouldBlock when it is ok to block - Probable implementation error.\n" ); 

		Status	= CollatorWouldBlock;
		break;
	    }
	}

	//
	// Check for spanning header
	//

	StartingWord             = 0xffffffff << (8 * min(RemainingLength,3));
	StartingWord            |= RemainingData[RemainingLength-3] << 16;
	StartingWord            |= RemainingData[RemainingLength-2] <<  8;
	StartingWord            |= RemainingData[RemainingLength-1];

	SpanningWord             = 0xffffffff >> (8 * min(NextPartition->PartitionSize,3));
	SpanningWord            |= NextPartition->PartitionBase[0] << 24;
	SpanningWord            |= NextPartition->PartitionBase[1] << 16;
	SpanningWord            |= NextPartition->PartitionBase[2] <<  8;

	//
	// Check for a start code spanning, or in the first word
	// record the nature of the span in a counter indicating how many 
	// bytes of the code are in the remaining data. 
	// NOTE the 00 at the bottom indicates we have a byte for the code, 
	//      not what it is.
	//

	SpanningCount           = 0;

	if( (StartingWord << 8) == 0x00000100 )
	{
	    SpanningCount       = 1;
	}
	else if( ((StartingWord << 16) | ((SpanningWord >> 16) & 0xff00)) == 0x00000100 )
	{
	    SpanningCount       = 2;
	}
	else if( ((StartingWord << 24) | ((SpanningWord >> 8)  & 0xffff00)) == 0x00000100 )
	{
	    SpanningCount       = 3;
	}

	SpanningCount	= min( SpanningCount, NextPartition->PartitionSize );

	//
	// Check that if we have a spanning code, that the code is not to be ignored
	//

	if( (SpanningCount != 0) && 
	     inrange(RemainingData[SpanningCount-1], Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd) )
	{
	    SpanningCount       = 0;
	}

	//
	// Handle a spanning start code
	//

	if( SpanningCount != 0 )
	{
	    AccumulationToCaptureCode	= (4 - SpanningCount);
	}

	//
	// Handle search for next start code
	//

	else
	{
	    //
	    // Get a new start code
	    //

	    Status      = FindPreviousStartCode( &CodeOffset );

	    if( Status != CollatorNoError )
	    {
		//
		// No start code, copy remaining data into buffer, and exit
		//

		Status  = AccumulateData( RemainingLength, RemainingData );

		if( Status != CollatorWouldBlock )
		    RemainingLength	= 0;

		if( (Status != CollatorNoError) && (Status != CollatorWouldBlock) )
		    DiscardingData	= true;

		break;
	    }

	    //
	    // Got a start code accumulate upto it, and process
	    //

	    AccumulationToCaptureCode	= (RemainingLength - CodeOffset);
	}

	//
	// Now process the code, whether from spanning, or from search
	//

	Status  = AccumulateData( AccumulationToCaptureCode, RemainingData + RemainingLength - AccumulationToCaptureCode );
	if( Status != CollatorNoError )
	{
	    if( Status == CollatorWouldBlock )
		break;

	    DiscardingData	= true;
	}

	RemainingLength		-= AccumulationToCaptureCode;
	Code			 = NextPartition->PartitionBase[3];

	if( IS_PES_START_CODE_VIDEO(Code) ||
	    (Code == PES_PADDING_START_CODE) )
	{
	    DiscardingData	= (Code == PES_PADDING_START_CODE) 							||	// It's a padding packet
				  (NextPartition->PartitionSize < PES_INITIAL_HEADER_SIZE) 				||	// There isn't a full pes header
				  (NextPartition->PartitionSize < PES_HEADER_SIZE(NextPartition->PartitionBase));

	    if( !DiscardingData )
	    {
		Status		= ReadPesHeader( NextPartition->PartitionBase );
		if( Status != CollatorNoError )
		    DiscardingData	= true;
	    }

	    if( !DiscardingData )
	    {
		//
		// Skip the packet header
		//

		MoveCurrentPartitionBoundary( -PES_HEADER_SIZE(NextPartition->PartitionBase) );

		//
		// Do we have a PTS to write
		//

		if( PlaybackTimeValid &&
		    ((PartitionPointUsedCount != PartitionPointMarkerCount) || (PartitionPointUsedCount == 0)) )
		{
		    NextPartition->CodedFrameParameters.PlaybackTimeValid				= false;		// Make sure the read header function did not pollute the new partition
		    NextPartition->CodedFrameParameters.DecodeTimeValid					= false;

		    PartitionPoints[PartitionPointMarkerCount].CodedFrameParameters.PlaybackTimeValid	= PlaybackTimeValid;
		    PartitionPoints[PartitionPointMarkerCount].CodedFrameParameters.PlaybackTime	= PlaybackTime;
		    PartitionPoints[PartitionPointMarkerCount].CodedFrameParameters.DecodeTimeValid	= DecodeTimeValid;
		    PartitionPoints[PartitionPointMarkerCount].CodedFrameParameters.DecodeTime		= DecodeTime;

		    PlaybackTimeValid						= false;
		    DecodeTimeValid						= false;
		}

		//
		// Record the state
		//

		PartitionPointMarkerCount		= PartitionPointUsedCount;
	    }
	}
	else 
	{
	    //
	    // A generic start code
	    //

	    Status	= AccumulateStartCode( PackStartCode(NextPartition->PartitionSize, Code) );
	    if( Status != CollatorNoError )
	    {
	    	DiscardingData	= true;

		EmptyCurrentPartition(); 	           // Dump any collected data in the current partition
		break;
	    }

	    //
	    // Is it going to terminate a frame
	    //

	    HeaderFlags			= 0;
	    if( NextPartition->PartitionSize > (4 + FrameParser->RequiredPresentationLength(Code)) )
		FrameParser->PresentCollatedHeader( Code, (NextPartition->PartitionBase+4), &HeaderFlags );

	    NextPartition->FrameFlags	|= HeaderFlags;
	    BlockTerminate		 = (HeaderFlags & FrameParserHeaderFlagPartitionPoint) != 0;
	    if( BlockTerminate )
	    {
		//
		// Adjust markers and accumulate the partition
		//

		PartitionPointSafeToOutputCount	= PartitionPointMarkerCount;
		PartitionPointMarkerCount	= PartitionPointUsedCount;

		AccumulateOnePartition();

	        //
	        // We need to check that there is sufficient room to accumulate 
	        // the new frame, or we may end up copying flipping great 
	        // wodges of data later.
	        //

	        if( ((HeaderFlags & FrameParserHeaderFlagConfirmReversiblePoint) != 0) ||
		    (CodedFrameBufferFreeSpace < (LargestFrameSeen + MINIMUM_ACCUMULATION_HEADROOM)) )
	        {
// Too fullness, how do we detect, and handle this ?
// also too many partitions in the table
		    Status	= PartitionOutput();
		    if( Status != CollatorNoError )
	    	    {
			report( severity_error, "Collator2_PesVideo_c::ProcessInputBackward - Output of partitions failed.\n" );
			break;
		    }
		}
	    }
	}
    }

//

    if( DataLengthRemaining != NULL )
	*DataLengthRemaining	= RemainingLength;

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The overlaod of the accumulate partition function, 
//	adds initialization of pts info to new partition
//

void   Collator2_PesVideo_c::AccumulateOnePartition( void )
{
    Collator2_Pes_c::AccumulateOnePartition();

    DiscardingData	= false;
    GotPartialHeader	= false;
    Skipping		= 0;

    //
    // at this point we sit (approximately) between frames and should update the PTS/DTS with the values
    // last extracted from the PES header. UseSpanningTime will (or at least should) be true when the
    // frame header spans two PES packets, at this point the frame started in the previous packet and
    // should therefore use the older PTS.
    //
    if( UseSpanningTime )
    {
	NextPartition->CodedFrameParameters.PlaybackTimeValid	= SpanningPlaybackTimeValid;
	NextPartition->CodedFrameParameters.PlaybackTime	= SpanningPlaybackTime;
	SpanningPlaybackTimeValid				= false;
	NextPartition->CodedFrameParameters.DecodeTimeValid	= SpanningDecodeTimeValid;
	NextPartition->CodedFrameParameters.DecodeTime		= SpanningDecodeTime;
	SpanningDecodeTimeValid					= false;
    }
    else
    {
	NextPartition->CodedFrameParameters.PlaybackTimeValid	= PlaybackTimeValid;
	NextPartition->CodedFrameParameters.PlaybackTime	= PlaybackTime;
	PlaybackTimeValid					= false;
	NextPartition->CodedFrameParameters.DecodeTimeValid	= DecodeTimeValid;
	NextPartition->CodedFrameParameters.DecodeTime		= DecodeTime;
	DecodeTimeValid						= false;
    }
}


