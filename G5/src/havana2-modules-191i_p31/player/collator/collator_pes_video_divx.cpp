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

Source file name : collator_pes_video_divx_.cpp
Author :           Chris

Implementation of the pes collator class for player 2.

Date        Modification                                    Name
----        ------------                                    --------
11-Jul-07   Created                                         Chris

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesVideoDivx_c
///
/// Implements initialisation of collator video class for Divx
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers
//
#include "collator_pes_video_divx.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
//
#define ZERO_START_CODE_HEADER_SIZE     7               // Allow us to see 00 00 01 00 00 01 <other code>

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

////////////////////////////////////////////////////////////////////////////
///
/// Initialize the class by resetting it.
///
/// During a constructor calls to virtual methods resolve to the current class (because
/// the vtable is still for the class being constructed). This means we need to call
/// ::Reset again because the calls made by the sub-constructors will not have called
/// our reset method.
///
Collator_PesVideoDivx_c::Collator_PesVideoDivx_c( void )
{
    if( InitializationStatus != CollatorNoError )
	return;

    Collator_PesVideoDivx_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Resets and configures according to the requirements of this stream content
///
/// \return void
///
CollatorStatus_t Collator_PesVideoDivx_c::Reset( void )
{
CollatorStatus_t Status;

//

    COLLATOR_DEBUG(">><<\n");

    Status = Collator_PesVideo_c::Reset();
    if( Status != CollatorNoError )
	return Status;

    Configuration.GenerateStartCodeList      = true;
    Configuration.MaxStartCodes              = 16;

    Configuration.StreamIdentifierMask       = 0xff;                            // Video
    Configuration.StreamIdentifierCode       = PES_START_CODE_VIDEO;
    Configuration.BlockTerminateMask         = 0xff;                            // Picture
    Configuration.BlockTerminateCode         = 0xb6;
    Configuration.IgnoreCodesRangeStart      = 0x01;
    Configuration.IgnoreCodesRangeEnd        = 0x1F;
    Configuration.InsertFrameTerminateCode   = false;
    Configuration.TerminalCode               = 0x00;
    Configuration.ExtendedHeaderLength       = 0;
    IgnoreCodes                              = false;
    Version                                  = 5;
	
    Configuration.DeferredTerminateFlag           = true;
    Configuration.StreamTerminateFlushesFrame     = false;

    return CollatorNoError;
}

CollatorStatus_t   Collator_PesVideoDivx_c::Input(
						  PlayerInputDescriptor_t        *Input,
						  unsigned int            DataLength,
						  void                   *Data,
						bool			  NonBlocking,
						unsigned int		 *DataLengthRemaining )
{
unsigned int            i;
CollatorStatus_t        Status;
unsigned int            HeaderSize;
unsigned int            Transfer;
unsigned int            Skip;
unsigned int            SpanningWord;
unsigned int            StartingWord;
unsigned int            SpanningCount;
unsigned int            CodeOffset;
unsigned char           Code;
//

    AssertComponentState( "Collator_PesVideoDivx_c::Input", ComponentRunning );

    COLLATOR_ASSERT( !NonBlocking );
    InputEntry( Input, DataLength, Data, NonBlocking );

    ActOnInputDescriptor( Input );

    //
    // Initialize scan state
    //

    RemainingData       = (unsigned char *)Data;
    RemainingLength     = DataLength;

    while( RemainingLength != 0 )
    {
	//
	// Are we building a pes header
	//

	if( GotPartialPesHeader )
	{
	    HeaderSize          = PES_INITIAL_HEADER_SIZE;
	    if( RemainingLength >= (PES_INITIAL_HEADER_SIZE - GotPartialPesHeaderBytes) )
		HeaderSize      = GotPartialPesHeaderBytes >= PES_INITIAL_HEADER_SIZE ?
		PES_HEADER_SIZE(StoredPesHeader) :
	    PES_HEADER_SIZE(RemainingData-GotPartialPesHeaderBytes);

	    Transfer    = min( RemainingLength, (HeaderSize - GotPartialPesHeaderBytes) );
	    memcpy( StoredPesHeader+GotPartialPesHeaderBytes, RemainingData, Transfer );

	    GotPartialPesHeaderBytes    += Transfer;
	    RemainingData               += Transfer;
	    RemainingLength             -= Transfer;

	    if( GotPartialPesHeaderBytes == PES_HEADER_SIZE(StoredPesHeader) )
	    {
		//
		// Since we are going to process the partial header, we will not have it in future
		//

		GotPartialPesHeader     = false;

		//

		Status  = ReadPesHeader();
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
	    }

	    if( RemainingLength == 0 )
	    {
		InputExit();
		return CollatorNoError;
	    }
	}

	//
	// Are we building a padding header
	//

	if( GotPartialPaddingHeader )
	{
	    report(severity_error,"Partial Packing\n");
	    HeaderSize          = PES_PADDING_INITIAL_HEADER_SIZE;

	    Transfer    = min( RemainingLength, (HeaderSize - GotPartialPaddingHeaderBytes) );
	    memcpy( StoredPaddingHeader+GotPartialPaddingHeaderBytes, RemainingData, Transfer );

	    GotPartialPaddingHeaderBytes        += Transfer;
	    RemainingData                       += Transfer;
	    RemainingLength                     -= Transfer;

	    if( GotPartialPaddingHeaderBytes == PES_PADDING_INITIAL_HEADER_SIZE )
	    {
		Skipping                         = PES_PADDING_SKIP(StoredPaddingHeader);
		report(severity_error,"Skipping\n");
		GotPartialPaddingHeader          = false;
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

	SpanningWord             = 0xffffffff << (8 * min(AccumulatedDataSize,3));
	SpanningWord            |= BufferBase[AccumulatedDataSize-3] << 16;
	SpanningWord            |= BufferBase[AccumulatedDataSize-2] << 8;
	SpanningWord            |= BufferBase[AccumulatedDataSize-1];

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
	    SpanningCount       = 4;
	    UseSpanningTime     = false;
	}

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
	    //
	    // Copy over the spanning bytes
	    //

	    for( i=0; i<SpanningCount; i++ )
		BufferBase[AccumulatedDataSize + i]     = RemainingData[i];

	    AccumulatedDataSize += SpanningCount;
	    RemainingData       += SpanningCount;
	    RemainingLength     -= SpanningCount;

	    Code                 = BufferBase[AccumulatedDataSize-1];

//            report(severity_info,"Start Code %x\n",Code);

	    if( Code == 0x31 )
	    {
		Version = *RemainingData;
		//report(severity_error,"Version Number %d\n",Version);
		IgnoreCodes = false;
	    }

	    if( Code == 0x00 )
	    {
		GotPartialZeroHeader             = true;
		AccumulatedDataSize             -= 4;           // Wind to before it
		GotPartialZeroHeaderBytes        = 4;
		StoredZeroHeader                 = BufferBase + AccumulatedDataSize;
		continue;
	    }

	    //
	    // Is it a pes header, and is it a pes stream we are interested in
	    //

	    else if( IS_PES_START_CODE_VIDEO(Code) )
	    {
		AccumulatedDataSize             -= 4;           // Wind to before it
		if( (Code & Configuration.StreamIdentifierMask) == Configuration.StreamIdentifierCode )
		{
		    GotPartialPesHeader         = true;
		    GotPartialPesHeaderBytes     = 4;
		    StoredPesHeader              = BufferBase + AccumulatedDataSize;
		    }
		else
		{
			SeekingPesHeader                 = true;
		}
		continue;
	    }

	    //
	    // Or is it a padding block
	    //

	    else if( Code == PES_PADDING_START_CODE )
	    {
		GotPartialPaddingHeader          = true;
		AccumulatedDataSize             -= 4;           // Wind to before it
		GotPartialPaddingHeaderBytes     = 4;
		StoredPaddingHeader              = BufferBase + AccumulatedDataSize;
		continue;
	    }

	    //
	    // Or if we are seeking a pes header, dump what we have and try again
	    //

	    else if( SeekingPesHeader )
	    {
		AccumulatedDataSize              = 0;           // Wind to before it
		continue;
	    }

	    //
	    // Or is it a block terminate code
	    //

	    else if( TerminationFlagIsSet)
	    {
		AccumulatedDataSize             -=4;
		Status                           = InternalFrameFlush();
		if( Status != CollatorNoError )
		{
		    InputExit();
		    return Status;
		}

		BufferBase[0]                    = 0x00;
		BufferBase[1]                    = 0x00;
		BufferBase[2]                    = 0x01;
		BufferBase[3]                    = Code;
		AccumulatedDataSize              = 4;
		SeekingPesHeader                 = false;
		TerminationFlagIsSet = false;

		if ( Configuration.DeferredTerminateFlag &&
		     ((Code & Configuration.BlockTerminateMask) == Configuration.BlockTerminateCode))
		{
			TerminationFlagIsSet = true;
		}                

	    }
	    else if ( Configuration.DeferredTerminateFlag &&
			  ((Code & Configuration.BlockTerminateMask) == Configuration.BlockTerminateCode)) 
	    {
		IgnoreCodes = true;
		TerminationFlagIsSet = true;
	    }


	    //
	    // Otherwise (and if its a block terminate) accumulate the start code
	    //

	    Status      = AccumulateStartCode( PackStartCode(AccumulatedDataSize-4,Code) );
	    if( Status != CollatorNoError )
	    {
		DiscardAccumulatedData();
		InputExit();
		return Status;
	    }
	}
        
	//
	// If we had no spanning code, but we had a spanning PTS, and we 
	// had no normal PTS for this frame, the copy the spanning time 
	// to the normal time.
	//

	else if( !PlaybackTimeValid )
	{
	    PlaybackTimeValid		= SpanningPlaybackTimeValid;
	    PlaybackTime		= SpanningPlaybackTime;
	    DecodeTimeValid		= SpanningDecodeTimeValid;
	    DecodeTime			= SpanningDecodeTime;
	    UseSpanningTime		= false;
	    SpanningPlaybackTimeValid	= false;
	    SpanningDecodeTimeValid	= false;
	}

	//
	// Now enter the loop processing start codes
	//


	while( true )
	{

	    Status      = FindNextStartCode( &CodeOffset );

	    if( Status != CollatorNoError)
	    {
		//
		// Terminal code after start code processing copy remaining data into buffer
		//

		Status  = AccumulateData( RemainingLength, RemainingData );
		if( Status != CollatorNoError )
		    DiscardAccumulatedData();

		RemainingLength         = 0;

		InputExit();
		return Status;
	    }

	    //
	    // Got one accumulate upto and including it
	    //

	    Status      = AccumulateData( CodeOffset+4, RemainingData );
	    if( Status != CollatorNoError )
	    {
		DiscardAccumulatedData();
		InputExit();
		return Status;
	    }

	    Code                                 = RemainingData[CodeOffset+3];
	    RemainingLength                     -= CodeOffset+4;
	    RemainingData                       += CodeOffset+4;

//            report(severity_info,"Start Code %x (ignore %d)\n",Code,IgnoreCodes);


	    // second case is for when we have 2 B6's in a group which can cause issues. Test with BatmanBegins
	    if ((IgnoreCodes == false) || ((Code & Configuration.BlockTerminateMask) == Configuration.BlockTerminateCode) )
	    {    

		//
		// Is it a pes header, and is it a pes stream we are interested in
		//

		if( Code == 0x00 )
		{
		    GotPartialZeroHeader                 = true;
		    AccumulatedDataSize         -= 4;           // Wind to before it
		    GotPartialZeroHeaderBytes    = 4;
		    StoredZeroHeader             = BufferBase + AccumulatedDataSize;
		    continue;
		}

		else if( IS_PES_START_CODE_VIDEO(Code) )
		{
		    AccumulatedDataSize         -= 4;           // Wind to before it
		    if( (Code & Configuration.StreamIdentifierMask) == Configuration.StreamIdentifierCode )
		    {
			GotPartialPesHeader             = true;
			GotPartialPesHeaderBytes         = 4;
			StoredPesHeader          = BufferBase + AccumulatedDataSize;
		    }
		    else
		    {
			SeekingPesHeader                 = true;
		    }
		    break;
		}

		//
		// Or is it a padding block
		//

		else if( Code == PES_PADDING_START_CODE )
		{
		    GotPartialPaddingHeader              = true;
		    AccumulatedDataSize         -= 4;           // Wind to before it
		    GotPartialPaddingHeaderBytes         = 4;
		    StoredPaddingHeader          = BufferBase + AccumulatedDataSize;
		    break;
		}

		//
		// Or if we are seeking a pes header, dump what we have and try again
		//

		else if( SeekingPesHeader )
		{
		    AccumulatedDataSize          = 0;           // Wind to before it
		    break;
		}

		//
		// Or is it a block terminate code
		//

		else if( TerminationFlagIsSet)
		{
		    AccumulatedDataSize         -=4;
		    Status                               = InternalFrameFlush();
		    if( Status != CollatorNoError )
		    {
			InputExit();
			return Status;
		    }

		    BufferBase[0]                        = 0x00;
		    BufferBase[1]                        = 0x00;
		    BufferBase[2]                        = 0x01;
		    BufferBase[3]                        = Code;
		    AccumulatedDataSize          = 4;
		    SeekingPesHeader             = false;


		    if ( Configuration.DeferredTerminateFlag &&
			 ((Code & Configuration.BlockTerminateMask) == Configuration.BlockTerminateCode))
		    {    
			TerminationFlagIsSet = true;
		    }

		    else
			TerminationFlagIsSet = false;                    


		}

		else if ( (Code & Configuration.BlockTerminateMask) == Configuration.BlockTerminateCode)
		{
		    TerminationFlagIsSet = true;
		    IgnoreCodes = true;
		}

		//
		// Otherwise (and if its a block terminate) accumulate the start code
		//

		Status  = AccumulateStartCode( PackStartCode(AccumulatedDataSize-4,Code) );
		if( Status != CollatorNoError )
		{
		    DiscardAccumulatedData();
		    InputExit();
		    return Status;
		}
		else if (Version != 3) // work around for issues with fake start codes in 311 streams
		{	    
			IgnoreCodes = false;
		}
	    }
	    else
	    {    
//                report (severity_error,"Ignoring Code %x\n",Code);
		continue;
	    }

	}

    }

    InputExit();
    return CollatorNoError;
}
