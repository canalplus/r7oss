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

Source file name : collator_pes_video.cpp
Author :           Nick

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
19-Apr-07   Created from existing collator_pes_video.cpp    Daniel
13-Oct-09   Rejigged from existing collator_pes_video.cpp   Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_video.h"
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

/**
 * Check if after concatenation buf1 and buf2 there will be Start Code crossing
 * boundary of buffers.
 *
 * \return number of buffers from buf2 that attached to buf1 form Start Code.
 * In case that there is not enough data in buf2 then assume they will form startcode
 * startcode is a sequence of bytes: 0 0 1 x
 *
 * Example:
 *   buf1= "... 0 0 1"                => 1
 *   buf1= "..... 0 0" buf2= "1..."   => 2
 *   buf1= "..... 0 0" buf2= ""       => 2 (there is no enough data in buf2 so assume it will make start code)
 *
 */
static int ScanForSpanningStartcode(unsigned char *buf1, int len1, unsigned char *buf2, int len2)
{
    /*  ______  ______
     *  buf1 | | buf2
     *  a b c    k l
     *
     *  0 0 1            => 1
     *    0 0    1       => 2
     *      0    0 1     => 3
     * */

    unsigned a,b,c,k,l;
    a = b = c = k = l = 0xff;
    switch(len1)
    {
	default:
	case 3: a = buf1[len1-3];
	case 2: b = buf1[len1-2];
	case 1: c = buf1[len1-1];
		break;
	case 0:
		return 0;
    }
    switch(len2)
    {
	default:
	case 2: l=buf2[1];
	case 1: k=buf2[0];
		break;
	case 0:
		break;
    }
    if( a==0 && b==0 & c==1)
	return 1;
    if( b==0 && c==0){
	if( (len2 >=1 && k == 1)  || len2 == 0 )
	    return 2;
    }
    if( c==0 ){
	switch(len2){
	    default:
	    case 2:
		if(k==0 && l==1)
		    return 3;
		else
		    return 0;
		break;
	    case 1:
		if(k == 0)
		    return 3; // possible there is "1" in next buffer;
		else
		    return 0; // no hope for startcode
		break;
	    case 0:
		return 3;
	}
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////
///
///     Handle input, by scanning the start codes, and chunking the data
///
///  \par Note 1:
///
///     Since we are accepting pes encapsulated data, and 
///     junking the pes header, we need to accumulate the 
///     pes header, before parsing and junking it.
///
///  \par Note 2:
///
///     We also need to accumulate and parse padding headers 
///     to allow the skipping of pad data.
///
///  \par Note 3:
///
///     In general we deal with only 4 byte codes, so we do not
///     need to accumulate more than 4 bytes for a code.
///     A special case for this is code Zero. Pes packets may
///     partition the input at any point, so standard start 
///     codes can span a pes packet header, this is no problem 
///     so long as we check the accumulated bytes alongside new 
///     bytes after skipping a pes header. 
///
///  \par Note 4:
///
///     The zero code special case, when we see 00 00 01 00, we
///     need to accumulate a further 3 bytes this is because we
///     can have the special case of 
///
///             00 00 01 00 00 01 <pes/packing start code>
///
///     where the first start code lead in has a terminal byte 
///     later in the stream which may lead to a completely different 
///     code. If we see 00 00 01 00 00 01 we always ignore the first 
///     code, as in a legal DVB stream this must be followed by a
///     pes/packing code of some sort, we accumulate the 3rd byte to
///     determine which
///
///     \todo This function weighs in at over 450 lines...
///
CollatorStatus_t   Collator_PesVideo_c::Input(
					PlayerInputDescriptor_t	 *Input,
					unsigned int		  DataLength,
					void			 *Data,
					bool			  NonBlocking,
					unsigned int		 *DataLengthRemaining )
{
unsigned int            i;
CollatorStatus_t        Status;
unsigned int            Transfer;
unsigned int            Skip;
unsigned int            SpanningCount;
unsigned int            CodeOffset;
unsigned char           Code;
bool			Loop;
bool			BlockTerminate;
FrameParserHeaderFlag_t	HeaderFlags;

//

    st_relayfs_write(ST_RELAY_TYPE_PES_VIDEO_BUFFER, ST_RELAY_SOURCE_VIDEO_COLLATOR, (unsigned char *)Data, DataLength, 0 );

    COLLATOR_ASSERT( !NonBlocking );
    AssertComponentState( "Collator_PesVideo_c::Input", ComponentRunning );
    InputEntry( Input, DataLength, Data, NonBlocking );

    ActOnInputDescriptor( Input );

    //
    // Initialize scan state
    //

    RemainingData       = (unsigned char *)Data;
    RemainingLength     = DataLength;

    while( (RemainingLength != 0) ||
	   (GotPartialHeader && (GotPartialCurrentSize >= GotPartialDesiredSize)) )
    {
	//
	// Are we accumulating an extended header
	//

	if( GotPartialHeader )
	{
	    if( GotPartialCurrentSize < GotPartialDesiredSize )
	    {
	        Transfer	=  min( RemainingLength, (GotPartialDesiredSize - GotPartialCurrentSize) );
	    	memcpy( StoredPartialHeader+GotPartialCurrentSize, RemainingData, Transfer );

	    	GotPartialCurrentSize	+= Transfer;
	    	RemainingData		+= Transfer;
	    	RemainingLength		-= Transfer;
	    }

	    if( GotPartialCurrentSize >= GotPartialDesiredSize )
	    {
		Loop	= false;

		switch( GotPartialType )
		{
		    case HeaderZeroStartCode:
				if( (StoredPartialHeader[4] == 0x00) && (StoredPartialHeader[5] == 0x01) )
				{
				    GotPartialType		 = (StoredPartialHeader[6] == PES_PADDING_START_CODE) ? HeaderPaddingStartCode : HeaderPesStartCode;
				    GotPartialDesiredSize	 = (StoredPartialHeader[6] == PES_PADDING_START_CODE) ? PES_PADDING_INITIAL_HEADER_SIZE : PES_INITIAL_HEADER_SIZE;
				    AccumulatedDataSize 	+= 3;
				    StoredPartialHeader		+= 3;
				    GotPartialCurrentSize	 = 4;
				}
				else
				{
				    GotPartialDesiredSize	 = 4;
				    if( Configuration.DetermineFrameBoundariesByPresentationToFrameParser )
					GotPartialDesiredSize	+= FrameParser->RequiredPresentationLength( 0x00 );

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

				GotPartialHeader		= false;
				StoredPesHeader			= StoredPartialHeader;
				Status				= ReadPesHeader();
				if( Status != CollatorNoError )
				{
				    InputExit();
				    return Status;
				}

				if( SeekingPesHeader )
				{
				    AccumulatedDataSize         = 0;            // Dump any collected data
				    SeekingPesHeader            = false;
				}

				break;

//

		    case HeaderPaddingStartCode:
				Skipping			= PES_PADDING_SKIP(StoredPartialHeader);
				GotPartialHeader		= false;
				break;

//

		    case HeaderGenericStartCode:
				//
				// Is it going to terminate a frame
				//

				Code				= StoredPartialHeader[3];

				if( Configuration.DetermineFrameBoundariesByPresentationToFrameParser )
				{
				    FrameParser->PresentCollatedHeader( Code, (StoredPartialHeader+4), &HeaderFlags );
				    BlockTerminate		= (HeaderFlags & FrameParserHeaderFlagPartitionPoint) != 0;
				}
				else
				{
				    BlockTerminate		= (((Code & Configuration.BlockTerminateMask) == Configuration.BlockTerminateCode) && !Configuration.DeferredTerminateFlag) ||
								  (Configuration.StreamTerminateFlushesFrame && (Code == Configuration.StreamTerminationCode)) ||
								  (Configuration.DeferredTerminateFlag && TerminationFlagIsSet);
				    TerminationFlagIsSet	= false;
				}

				GotPartialHeader		= false;

				if( BlockTerminate )
				{
				    memcpy( StoredPartialHeaderCopy, StoredPartialHeader, GotPartialCurrentSize );

				    Status			= InternalFrameFlush();
				    if( Status != CollatorNoError )
				    {
					InputExit();
					return Status;
				    }

				    memcpy( BufferBase, StoredPartialHeaderCopy, GotPartialCurrentSize );
				    AccumulatedDataSize		= 0;
				    SeekingPesHeader		= false;
				}

				//
				// Accumulate it in any event
				//

				Status      = AccumulateStartCode( PackStartCode(AccumulatedDataSize, Code) );
				if( Status != CollatorNoError )
				{
				    DiscardAccumulatedData();
				    InputExit();
				    return Status;
				}

				AccumulatedDataSize         += GotPartialCurrentSize;

				//
				// Check whether or not this start code will be a block terminate in the future
				//

				if ( Configuration.DeferredTerminateFlag && ((Code & Configuration.BlockTerminateMask) == Configuration.BlockTerminateCode)) 
				    TerminationFlagIsSet = true;

				break;
		}

		if( Loop )
		    continue;
	    }

            if( RemainingLength == 0 )
	    {
		InputExit();
                return CollatorNoError;
	    }
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
	    {
		InputExit();
		return CollatorNoError;
	    }
	}


	//
	// Check for spanning header
	//
	// Check for a start code spanning, or in the first word
	// record the nature of the span in a counter indicating how many 
	// bytes of the code are in the remaining data. 

	SpanningCount = ScanForSpanningStartcode(
		BufferBase, AccumulatedDataSize,
		RemainingData, RemainingLength );

	if(SpanningCount == 0 && RemainingLength >=3 && memcmp(RemainingData, "\x00\x00\x01", 3) == 0 )
	{
	    SpanningCount               = 4;
	    UseSpanningTime             = false;
	    SpanningPlaybackTimeValid   = false;
	    SpanningDecodeTimeValid     = false;
	}

	//
	// Check that if we have a spanning code, that the code is not to be ignored
	//

	if( (SpanningCount != 0) &&  (SpanningCount <= RemainingLength) &&
	    inrange(RemainingData[SpanningCount-1], Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd) )
	{
	    SpanningCount       = 0;
	}

	//
	// Handle a spanning start code
	//

	if( SpanningCount != 0 )
	{
	    //
	    // Copy over the spanning bytes
	    //
	    for( i=0; i< min(SpanningCount, RemainingLength); i++ )
		BufferBase[AccumulatedDataSize + i]     = RemainingData[i];

	    if( SpanningCount > RemainingLength ){
		RemainingData       	+= i;
		RemainingLength     	-= i;
		AccumulatedDataSize 	+= i;
		break;
	    }

	    RemainingData       	+= i;
	    RemainingLength     	-= i;
	    AccumulatedDataSize 	+= SpanningCount -4;
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
		if( Status != CollatorNoError )
		    DiscardAccumulatedData();

		RemainingLength         = 0;
		InputExit();
		return Status;
	    }

	    //
	    // Got a start code accumulate upto it, and process
	    //

	    Status      = AccumulateData( CodeOffset+4, RemainingData );
	    if( Status != CollatorNoError )
	    {
		DiscardAccumulatedData();
		InputExit();
		return Status;
	    }

	    AccumulatedDataSize			-= 4;
	    RemainingLength                     -= CodeOffset+4;
	    RemainingData                       += CodeOffset+4;
	}

	//
	// Now process the code, whether from spanning, or from search
	//

	GotPartialHeader		= true;
	GotPartialCurrentSize		= 4;
	StoredPartialHeader		= BufferBase + AccumulatedDataSize;
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
		SeekingPesHeader	= true;
	    }
	}
	else if( Code == PES_PADDING_START_CODE )
	{
	    GotPartialType		= HeaderPaddingStartCode;
	    GotPartialDesiredSize	= PES_PADDING_INITIAL_HEADER_SIZE;
	}
	else if( SeekingPesHeader )
	{
	    // If currently seeking a pes header then ignore the last case of a generic header
	    GotPartialHeader		= false;
	    AccumulatedDataSize		= 0;
	}
	else 
	{
	    // A generic start code
	    GotPartialType		= HeaderGenericStartCode;

	    GotPartialDesiredSize	 = 4;
	    if( Configuration.DetermineFrameBoundariesByPresentationToFrameParser )
		GotPartialDesiredSize	+= FrameParser->RequiredPresentationLength( Code );
	}
    }

    InputExit();
    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Frame Flush functions
//

CollatorStatus_t   Collator_PesVideo_c::InternalFrameFlush( bool        FlushedByStreamTerminate )
{
    CodedFrameParameters->FollowedByStreamTerminate     = FlushedByStreamTerminate;
    return InternalFrameFlush();
}


// -----------------------

CollatorStatus_t   Collator_PesVideo_c::InternalFrameFlush(             void )
{
CollatorStatus_t        Status;

//

    AssertComponentState( "Collator_PesVideo_c::InternalFrameFlush", ComponentRunning );

//

    Status                                      = Collator_Pes_c::InternalFrameFlush();
    if( Status != CodecNoError )
	return Status;

    SeekingPesHeader                            = true;
    GotPartialHeader				= false;		// New style all but divx
    GotPartialZeroHeader                        = false;		// Old style for divx support only
    GotPartialPesHeader                         = false;
    GotPartialPaddingHeader                     = false;
    Skipping                                    = 0;

    TerminationFlagIsSet = false;

    //
    // at this point we sit (approximately) between frames and should update the PTS/DTS with the values
    // last extracted from the PES header. UseSpanningTime will (or at least should) be true when the
    // frame header spans two PES packets, at this point the frame started in the previous packet and
    // should therefore use the older PTS.
    //
    if( UseSpanningTime )
    {
	CodedFrameParameters->PlaybackTimeValid = SpanningPlaybackTimeValid;
	CodedFrameParameters->PlaybackTime      = SpanningPlaybackTime;
	SpanningPlaybackTimeValid               = false;
	CodedFrameParameters->DecodeTimeValid   = SpanningDecodeTimeValid;
	CodedFrameParameters->DecodeTime        = SpanningDecodeTime;
	SpanningDecodeTimeValid                 = false;
    }
    else
    {
	CodedFrameParameters->PlaybackTimeValid = PlaybackTimeValid;
	CodedFrameParameters->PlaybackTime      = PlaybackTime;
	PlaybackTimeValid                       = false;
	CodedFrameParameters->DecodeTimeValid   = DecodeTimeValid;
	CodedFrameParameters->DecodeTime        = DecodeTime;
	DecodeTimeValid                         = false;
    }

    return CodecNoError;
}


