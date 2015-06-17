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

Source file name : collator_pes.cpp
Author :           Audio

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
19-Apr-07   Created from existing collator_pes_video.cpp    Daniel

************************************************************************/

// /////////////////////////////////////////////////////////////////////////
/// \class Collator_PesAudio_c
///
/// Specialized PES collator implementing audio specific features.
///
/// None of the common audio standards use MPEG start codes to demark
/// frame boundaries. Instead, once the stream has been de-packetized
/// we must perform a limited form of frame analysis to discover the
/// frame boundaries.
///
/// This class provides a framework on which to base that frame
/// analysis. Basically this class provides most of the machinary to
/// manage buffers but leaves the sub-class to implement
/// Collator_PesAudio_c::ScanForSyncWord
/// and Collator_PesAudio_c::GetFrameLength.
///
/// \todo Currently this class does not honour PES padding (causing
///       accumulated data to be spuriously discarded when playing back
///       padded streams).
///

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "collator_pes_audio.h"
#include "st_relay.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

////////////////////////////////////////////////////////////////////////////
/// \fn CollatorStatus_t Collator_PesAudio_c::FindNextSyncWord( int *CodeOffset )
///
/// Scan the input until an appropriate synchronization sequence is found.
///
/// If the encoding format does not have any useable synchronization sequence
/// this method should set CodeOffset to zero and return CollatorNoError.
///
/// In addition to scanning the input found in Collator_PesAudio_c::RemainingElementaryData
/// this method should also examine Collator_PesAudio_c::PotentialFrameHeader to see
/// if there is a synchronization sequence that spans blocks. If such a sequence
/// if found this is indicated by providing a negative offset.
///
/// \return Collator status code, CollatorNoError indicates success.
///

////////////////////////////////////////////////////////////////////////////
/// \fn CollatorStatus_t Collator_PesAudio_c::DecideCollatorNextStateAndGetLength( unsigned int *FrameLength )
///
/// Examine the frame header pointed to by Collator_PesAudioEAc3_c::StoredFrameHeader
/// and determine how many bytes should be accumulated. Additionally this method
/// sets the collator state.
///
/// For trivial bitstreams where the header identifies the exact length of the frame
/// (e.g. MPEG audio) the state is typically set to GotCompleteFrame to indicate that
/// the previously accumulated frame should be emitted.
///
/// More complex bitstreams may require secondary analysis (by a later call to
/// this method) and these typically use either the ReadSubFrame state if they want to
/// accumulate data prior to subsequent analysis or the SkipSubFrame state to discard
/// data prior to subsequent analysis.
///
/// Note that even for trivial bitstreams the collator must use the ReadSubFrame state
/// for the first frame after a glitch since there is nothing to emit.
///
/// If the encoding format does not have any useable synchronization sequence
/// and therefore no easy means to calculate the frame length then an arbitary
/// chunk size should be selected. The choice of chunk size will depend on the
/// level of compresion achieved by the encoder but in general 1024 bytes would
/// seem appropriate for all but PCM data.
///
/// \return Collator status code, CollatorNoError indicates success.
///

////////////////////////////////////////////////////////////////////////////
///
/// Scan the input until an appropriate synchronization sequence is found.
///
/// Scans any ::RemainingElementaryData searching for a
/// synchronization sequence using ::FindNextSyncWord,
/// a pure virtual method provided by sub-classes.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::SearchForSyncWord( void )
{
CollatorStatus_t Status;
int CodeOffset;

//

    COLLATOR_DEBUG( ">><<\n");

    Status = FindNextSyncWord( &CodeOffset );
    if( Status == CollatorNoError )
    {
	COLLATOR_DEBUG( "Tentatively locked to synchronization sequence (at %d)\n", CodeOffset );

    	// switch state
	CollatorState = GotSynchronized;
       	GotPartialFrameHeaderBytes = 0;
	
	if ( CodeOffset >= 0 )
	{
	    // discard any data leading up to the start code
	    RemainingElementaryData += CodeOffset;
	    RemainingElementaryLength -= CodeOffset;
	    
	}
	else
	{
	    COLLATOR_DEBUG("Synchronization sequence spans multiple PES packets\n");

	    // accumulate any data from the old block
	    Status = AccumulateData( -1 * CodeOffset,
	                             PotentialFrameHeader + PotentialFrameHeaderLength + CodeOffset );
            if( Status != CollatorNoError )
                COLLATOR_DEBUG( "Cannot accumulate data #1 (%d)\n", Status );
	    GotPartialFrameHeaderBytes += (-1 * CodeOffset);                          
	}

	PotentialFrameHeaderLength = 0;
    }
    else
    {
	// copy the last few bytes of the frame into PotentialFrameHeader (so that FindNextSyncWord
	// can return a negative CodeOffset if the synchronization sequence spans blocks)
	if( RemainingElementaryLength < FrameHeaderLength )
	{
	    if ( PotentialFrameHeaderLength + RemainingElementaryLength > FrameHeaderLength )
	    {
		// shuffle the existing potential frame header downwards
		unsigned int BytesToKeep = FrameHeaderLength - RemainingElementaryLength;
		unsigned int ShuffleBy = PotentialFrameHeaderLength - BytesToKeep;

		memmove( PotentialFrameHeader, PotentialFrameHeader + ShuffleBy, BytesToKeep );
		PotentialFrameHeaderLength = BytesToKeep;
	    }

	    memcpy( PotentialFrameHeader + PotentialFrameHeaderLength,
	            RemainingElementaryData,
		    RemainingElementaryLength );
	    PotentialFrameHeaderLength += RemainingElementaryLength;
	}
	else
	{
	    memcpy( PotentialFrameHeader,
	            RemainingElementaryData + RemainingElementaryLength - FrameHeaderLength,
		    FrameHeaderLength );
	    PotentialFrameHeaderLength = FrameHeaderLength;
	}


    	COLLATOR_DEBUG("Discarded %d bytes while searching for synchronization sequence\n",
    	               RemainingElementaryLength);
    	RemainingElementaryLength = 0;

	// we have contained the 'error' and don't want to propagate it
	Status = CollatorNoError;
    }
    
//
       
    return Status;	
}

////////////////////////////////////////////////////////////////////////////
///
/// Accumulate incoming data until we have the parseable frame header.
///
/// Accumulate data until we have ::FrameHeaderLength
/// bytes stashed away. At this point there is sufficient data accumulated
/// to determine how many bytes will pass us by before the next frame header
/// is expected.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::ReadPartialFrameHeader( void )
{
  CollatorStatus_t Status;
  unsigned int BytesNeeded, BytesToRead, FrameLength;
  CollatorState_t OldCollatorState;
  
  //
  
  COLLATOR_DEBUG(">><<\n");
  
  //
  
  BytesNeeded = FrameHeaderLength - GotPartialFrameHeaderBytes;
  BytesToRead = min( RemainingElementaryLength, BytesNeeded );

  Status = AccumulateData( BytesToRead, RemainingElementaryData );
  if( Status == CollatorNoError )
    {
      GotPartialFrameHeaderBytes += BytesToRead;
      RemainingElementaryData += BytesToRead;
      RemainingElementaryLength -= BytesToRead;
      
      COLLATOR_DEBUG( "BytesNeeded %d; BytesToRead %d\n", BytesNeeded, BytesToRead );
      if( BytesNeeded == BytesToRead )
    	{
	  //
	  // Woo hoo! We've got the whole header, examine it and change state
	  //
	  
	  StoredFrameHeader = BufferBase + (AccumulatedDataSize - FrameHeaderLength);
    	  
	  COLLATOR_DEBUG("Got entire frame header packet\n");
	  //report_dump_hex( severity_note, StoredFrameHeader, FrameHeaderLength, 32, 0);
	  
	  OldCollatorState = CollatorState;
	  Status = DecideCollatorNextStateAndGetLength(&FrameLength);
	  if( Status != CollatorNoError )
	    {
	      COLLATOR_DEBUG( "Badly formed frame header; seeking new frame header\n" );
	      return HandleMissingNextFrameHeader();
	    }
	  if (FrameLength == 0)
	    {
	      // The LPCM collator needs to do this in order to get a frame evicted before
              // accumulating data from the PES private data area into the frame. The only
	      // way it can do this is by reporting a zero length frame and updating some
	      // internal state variables. On the next call it will report a non-zero value
	      // (i.e. we won't loop forever accumulating no data).
	      COLLATOR_DEBUG("Sub-class reported unlikely (but potentially legitimate) frame length (%d)\n", FrameLength);
	    }
	  if (FrameLength > MaximumCodedFrameSize)
	    {
	      COLLATOR_ERROR("Sub-class reported absurd frame length (%d)\n", FrameLength);
	      return HandleMissingNextFrameHeader();
	    }
	  
	  // this is the number of bytes we must absorb before switching state to SeekingFrameEnd.
	  // if the value is negative then we've already started absorbing the subsequent frame
	  // header.
	  FramePayloadRemaining = FrameLength - FrameHeaderLength;

	  if (CollatorState == GotCompleteFrame)
	    {
	      AccumulatedFrameReady = true;
	      
	      //
	      // update the coded frame parameters using the parameters calculated the
	      // last time we saw a frame header.
	      //
	      CodedFrameParameters->PlaybackTimeValid = NextPlaybackTimeValid;
	      CodedFrameParameters->PlaybackTime = NextPlaybackTime;
	      CodedFrameParameters->DecodeTimeValid = NextDecodeTimeValid;
	      CodedFrameParameters->DecodeTime = NextDecodeTime;
	    }
	  else if (CollatorState == SkipSubFrame)
	    {
	      /* discard the accumulated frame header */
	      AccumulatedDataSize -= FrameHeaderLength;
	    }
	    
	  if (CollatorState == GotCompleteFrame || OldCollatorState == GotSynchronized)
	    {
	      //
	      // at this point we have discovered a frame header and need to attach a time to it.
	      // we can choose between the normal stamp (the stamp of the current PES packet) or the
	      // spanning stamp (the stamp of the previous PES packet). Basically if we have accumulated
	      // a greater number of bytes than our current offset into the PES packet then we want to
	      // use the spanning time.
	      //
	  	
	      bool WantSpanningTime = GetOffsetIntoPacket() < (int) GotPartialFrameHeaderBytes;

	      if( WantSpanningTime && !UseSpanningTime)
	        {
	      	  COLLATOR_ERROR("Wanted to take the spanning time but this was not available.");
	      	  WantSpanningTime = false;
	        }
	      
              if( WantSpanningTime )
                {
		  NextPlaybackTimeValid = SpanningPlaybackTimeValid;
		  NextPlaybackTime	= SpanningPlaybackTime;
		  SpanningPlaybackTimeValid = false;
		  NextDecodeTimeValid	= SpanningDecodeTimeValid;
		  NextDecodeTime	= SpanningDecodeTime;
		  SpanningDecodeTimeValid = false;
		  UseSpanningTime       = false;
    	        }
    	      else
                {
	          NextPlaybackTimeValid = PlaybackTimeValid;
	          NextPlaybackTime	= PlaybackTime;
	          PlaybackTimeValid	= false;
	          NextDecodeTimeValid	= DecodeTimeValid;
	          NextDecodeTime	= DecodeTime;
	          DecodeTimeValid	= false;
                }
	  }
	  
	  // switch states and absorb the packet
	  COLLATOR_DEBUG( "Discovered frame header (frame length %d bytes)\n", FrameLength );
    	}
    }
  else
    {
        COLLATOR_DEBUG( "Cannot accumulate data #3 (%d)\n", Status );
    }  
  return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Handle losing lock on the frame headers.
///
/// This function is called to handle the data that was spuriously accumulated
/// when the frame header was badly parsed.
///
/// In principle this function is quite simple. We allocate a new accumulation buffer and
/// use the currently accumulated data is the data source to run the elementary stream
/// state machine. There is however a little extra logic to get rid of recursion.
/// Specificially we change the error handling behaviour if this method is re-entered
/// so that there error is reported back to the already executing copy of the method.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::HandleMissingNextFrameHeader( void )
{
    CollatorStatus_t Status;
    
    //
    // Mark the collator as having lost frame lock.
    // Yes! We really do want to do this before the re-entry checks.
    //
    
    CollatorState = SeekingSyncWord; // we really do want to do this before the re-entry checks
    AccumulatedFrameReady = false;
    
    //
    // Check for re-entry
    //
    
    if( AlreadyHandlingMissingNextFrameHeader )
    {
	COLLATOR_DEBUG( "Re-entered the error recovery handler, initiating stack unwind\n" );
	return CollatorUnwindStack;
    }
    
    //
    // Check whether the sub-class wants trivial or aggressive error recovery
    //

    if( !ReprocessAccumulatedDataDuringErrorRecovery )
    {
	DiscardAccumulatedData();
	return CollatorNoError;
    }

    //
    // Remember the original elementary stream pointers for when we return to 'normal' processing.
    //
    
    unsigned char *OldRemainingElementaryOrigin = RemainingElementaryOrigin;
    unsigned char *OldRemainingElementaryData   = RemainingElementaryData;
    unsigned int OldRemainingElementaryLength   = RemainingElementaryLength;

    //
    // Take ownership of the already accumulated data
    //
    
    Buffer_t ReprocessingDataBuffer = CodedFrameBuffer;
    unsigned char *ReprocessingData = BufferBase;
    unsigned int ReprocessingDataLength = AccumulatedDataSize;
    
    ReprocessingDataBuffer->SetUsedDataSize( ReprocessingDataLength );

    Status      = ReprocessingDataBuffer->ShrinkBuffer( max(ReprocessingDataLength, 1) );
    if( Status != BufferNoError )
    {
	COLLATOR_ERROR("Failed to shrink the reprocessing buffer to size (%08x).\n", Status );
	// not fatal - we're merely wasting memory
    }
    
    // At the time of writing GetNewBuffer() doesn't check for leaks. This is good because otherwise
    // we wouldn't have transfer the ownership of the ReprocessingDataBuffer by making this call.
    Status = GetNewBuffer();
    if( Status != CollatorNoError )
    {
	COLLATOR_ERROR( "Cannot get new buffer during error recovery\n" );
	return CollatorError;
    }

    //
    // Remember that we are re-processing the previously accumulated elementary stream
    //
    
    AlreadyHandlingMissingNextFrameHeader = true;
    
    //
    // WARNING: From this point on we own the ReprocessingDataBuffer, have set the recursion avoidance
    //          marker and may have damaged the RemainingElementaryData pointer. There should be no
    //          short-circuit exit paths used after this point otherwise we risk avoiding the clean up
    //          at the bottom of the method.
    //
    
    while ( ReprocessingDataLength > 1 )
    {
	//
	// Remove the first byte from the recovery buffer (to avoid detecting again the same start code).
	//

	ReprocessingData += 1;
	ReprocessingDataLength -= 1;

	//
	// Search for a start code in the reprocessing data. This allows us to throw away data that we
	// know will never need reprocessing which makes the recursion avoidance code more efficient.
	//
	
	RemainingElementaryOrigin = ReprocessingData;
	RemainingElementaryData   = ReprocessingData;
	RemainingElementaryLength = ReprocessingDataLength;
	
	int CodeOffset;
	PotentialFrameHeaderLength = 0; // ensure no (now voided) historic data is considered by sub-class
	Status = FindNextSyncWord( &CodeOffset );
	if( Status == CodecNoError )
	{
	    COLLATOR_ASSERT( CodeOffset >= 0 );
	    COLLATOR_DEBUG( "Found start code during error recovery (byte %d of %d)\n",
		            CodeOffset, ReprocessingDataLength );
	    
	    // We found a start code, snip off all preceeding data
	    ReprocessingData += CodeOffset;
	    ReprocessingDataLength -= CodeOffset;
	}
	else
	{
	    // We didn't find a start code, snip off everything except the last few bytes. This
	    // final fragment may contain a partial start code so we want to pass if through the
            // elementary stream handler again.
	    unsigned FinalBytes = min( ReprocessingDataLength, FrameHeaderLength-1 );
	    COLLATOR_DEBUG( "Found no start code during error recovery (processing final %d bytes of %d)\n",
		            ReprocessingDataLength, FinalBytes );	    

	    ReprocessingData += ReprocessingDataLength;
	    ReprocessingDataLength = FinalBytes;
	    ReprocessingData -= ReprocessingDataLength;
	}
	
	//
	// Process the elementary stream
	//
	
	Status = HandleElementaryStream( ReprocessingDataLength, ReprocessingData );
	if( CollatorNoError == Status )
	{
	    COLLATOR_DEBUG( "Error recovery completed, returning to normal processing\n" );
	    // All data consumed and stored in the subsequent accumulation buffer
	    break; // Success will propagate when we return Status
	}
	else if( CollatorUnwindStack == Status )
	{
	    COLLATOR_DEBUG( "Stack unwound successfully, re-trying error recovery\n" );
	    // We found a frame header but lost lock again... let's have another go
	    AccumulatedDataSize = 0; // make sure no accumulated data is carried round the loop
	    continue;
	}
	else
	{
	    COLLATOR_ERROR( "Error handling elementary stream during error recovery\n" );
	    break; // Failure will propagate when we return Status
	}
    }
    
    //
    // Free the buffer we just consumed and restore the original elementary stream pointers
    //

    RemainingElementaryOrigin = OldRemainingElementaryOrigin;
    RemainingElementaryData   = OldRemainingElementaryData;
    RemainingElementaryLength = OldRemainingElementaryLength;
    (void) ReprocessingDataBuffer->DecrementReferenceCount( IdentifierCollator );
    AlreadyHandlingMissingNextFrameHeader = false;
  
    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Accumulate data until we reach the next frame header.
///
/// The function has little or no intelligence. Basically it will squirrel
/// away data until Collator_PesAudio_c::GotPartialFrameHeaderBytes reaches
/// Collator_PesAudio_c::FrameHeaderLength and then set
/// Collator_PesAudio_c::GotPartialFrameHeader.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::ReadFrame( void )
{
  CollatorStatus_t Status;
  unsigned int BytesToRead;
  
  //
  
  COLLATOR_DEBUG(">><<\n");

  
  //
  
  // if the FramePayloadRemaining is -ve then we have no work to do except
  // record the fact that we've already accumulated part of the next frame header
  if (FramePayloadRemaining < 0)
    {
      GotPartialFrameHeaderBytes = -FramePayloadRemaining;
      CollatorState = SeekingFrameEnd;
      return CollatorNoError;
    }
  
  //
  
  BytesToRead = min( FramePayloadRemaining, RemainingElementaryLength );
  
  if (CollatorState == ReadSubFrame)
    {
      Status = AccumulateData( BytesToRead, RemainingElementaryData );
      if( Status != CollatorNoError )
          COLLATOR_DEBUG( "Cannot accumulate data #4 (%d)\n", Status );
    }
  else
    {
      Status = CollatorNoError;
    }

  if( Status == CollatorNoError )
    {
      if (BytesToRead == FramePayloadRemaining) 
	{
	  GotPartialFrameHeaderBytes = 0;
	  CollatorState = SeekingFrameEnd;
	}
      
      RemainingElementaryData += BytesToRead;
      RemainingElementaryLength -= BytesToRead;
      FramePayloadRemaining -= BytesToRead;
    }
  
  //
  
  return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Handle a block of data that is not frame alligned.
///
/// There may be the end of a frame, whole frames, the start of a frame or even just
/// the middle of the frame.
///
/// If we have incomplete blocks we build up a complete one in the saved data,
/// in order to process we need to aquire a frame plus the next header (for sync check)
/// we can end up with the save buffer having a frame + part of a header, and a secondary
/// save with just part of a header.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::HandleElementaryStream( unsigned int Length, unsigned char *Data )
{
  CollatorStatus_t Status;
  
  //
  
  if( DiscardPesPacket )
  {
      COLLATOR_DEBUG( "Discarding %d bytes of elementary stream\n", Length );
      return CodecNoError;
  }
  
  //
  // Copy our arguments to class members so the utility functions can
  // act upon it.
  //
  
  RemainingElementaryOrigin	= Data;
  RemainingElementaryData	= Data;
  RemainingElementaryLength	= Length;
  
  //
  // Continually handle units of input until all input is exhausted (or an error occurs).
  //
  // Be aware that our helper functions may, during their execution, cause state changes
  // that result in a different branch being taken next time round the loop.
  //
  
  Status = CollatorNoError;
  while( Status == CollatorNoError &&  RemainingElementaryLength != 0 )
    {
      COLLATOR_DEBUG("ES loop has %d bytes remaining\n", RemainingElementaryLength);
      //report_dump_hex( severity_note, RemainingElementaryData, min(RemainingElementaryLength, 188), 32, 0);
      
      switch (CollatorState)
	{
	case SeekingSyncWord:
	  //
	  // Try to lock to incoming frame headers
	  //
	  
	  Status = SearchForSyncWord();
	  break;
	  
	case GotSynchronized:
	case SeekingFrameEnd:
	  //
	  // Read in the remains of the frame header
	  //
	  
	  Status = ReadPartialFrameHeader();

	  break;

	case ReadSubFrame:
	case SkipSubFrame:
	  //
	  // Squirrel away the frame
	  //
	  
	  Status = ReadFrame();
	  break;

	case GotCompleteFrame:
	  //
	  // Pass the accumulated subframes to the frame parser
	  //

	  InternalFrameFlush();
	  CollatorState = ReadSubFrame;
	  break;

	default:
	  // should not occur...
	  COLLATOR_DEBUG("General failure; wrong collator state");
	  Status = CollatorError;
	  break;
	}
    }
    
    if( Status != CollatorNoError )
    {
    	// if anything when wrong then we need to resynchronize
    	COLLATOR_DEBUG("General failure; seeking new synchronization sequence\n");
    	DiscardAccumulatedData();
	CollatorState = SeekingSyncWord;
    }
    
    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Scan the input until an appropriate PES start code is found.
///
/// Scans any Collator_Pes_c::RemainingData searching for a PES start code.
/// The configuration for this comes from Collator_Base_c::Configuration and
/// is this means that only the interesting (e.g. PES audio packets) start
/// codes will be detected.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::SearchForPesHeader( void )
{
CollatorStatus_t Status;
unsigned int CodeOffset;

//

    //
    // If there are any trailing start codes handle those.
    //

    while( TrailingStartCodeBytes && RemainingLength )
    {
	if( TrailingStartCodeBytes == 3 )
	{
	    // We've got the 0, 0, 1 so if the code is *not* in the ignore range then we've got one
	    unsigned char SpecificCode = RemainingData[0];
	    if( !inrange( SpecificCode, Configuration.IgnoreCodesRangeStart,
	                                Configuration.IgnoreCodesRangeEnd ) )
	    {
		COLLATOR_DEBUG("Found a trailing startcode 00, 00, 01, %x\n", SpecificCode);

		// Consume the specific start code
		RemainingData++;
		RemainingLength--;
		
		// Switch state (and reflect the data we are about to accumulate)
		SeekingPesHeader = false;
		GotPartialPesHeader = true;
		//assert( AccumulatedDataSize == 0 );
       	        GotPartialPesHeaderBytes = 4;

		// There are now no trailing start code bytes
		TrailingStartCodeBytes = 0;

		// Finally, accumulate the data (by reconstructing it)
		unsigned char StartCode[4] = { 0, 0, 1, SpecificCode };
                Status = AccumulateData( 4, StartCode );
                if( Status != CollatorNoError )
                    COLLATOR_DEBUG( "Cannot accumulate data #5 (%d)\n", Status );
                return Status;
	    }

	    // Nope, that's not a suitable start code.
	    COLLATOR_DEBUG("Trailing start code 00, 00, 01, %x was in the ignore range\n", SpecificCode);
	    TrailingStartCodeBytes = 0;
	    break;
	}
	else if( TrailingStartCodeBytes == 2 )
	{
	    // Got two zeros, a one gets us ready to read the code.
	    if( RemainingData[0] == 1 )
	    {
		COLLATOR_DEBUG("Trailing start code looks good (found 00, 00; got 01)\n");
		TrailingStartCodeBytes++;
		RemainingData++;
		RemainingLength--;
		continue;
	    }

	    // Got two zeros, another zero still leaves us with two zeros.
	    if( RemainingData[0] == 0 )
	    {
		COLLATOR_DEBUG("Trailing start code looks OK (found 00, 00; got 00)\n");
		RemainingData++;
		RemainingLength--;
		continue;
	    }

	    // Nope, that's not a suitable start code.
	    COLLATOR_DEBUG("Trailing 00, 00 was not part of a start code\n");
	    TrailingStartCodeBytes = 0;
	    break;

	}
	else if( TrailingStartCodeBytes == 1 )
	{
	    // Got one zero, another zero gives us two (duh).
	    if( RemainingData[0] == 0 )
	    {
		COLLATOR_DEBUG("Trailing start code looks good (found 00; got 00)\n");
		RemainingData++;
		RemainingLength--;
		continue;
	    }

	    // Nope, that's not a suitable start code.
	    COLLATOR_DEBUG("Trailing 00 was not part of a start code\n");
	    TrailingStartCodeBytes = 0;
	    break;
	}
	else
	{
	    COLLATOR_ERROR( "TrailingStartCodeBytes has illegal value: %d\n", TrailingStartCodeBytes );
	    TrailingStartCodeBytes = 0;
	    return CollatorError;
	}
    }

    if( RemainingLength == 0 )
    {
	return CollatorNoError;
    }

    //assert(TrailingStartCodeBytes == 0);


//

    Status = FindNextStartCode( &CodeOffset );
    if( Status == CollatorNoError )
    {
    	COLLATOR_DEBUG("Locked to PES packet boundaries\n");
    	
    	// discard any data leading up to the start code
    	RemainingData += CodeOffset;
    	RemainingLength -= CodeOffset;
    	
    	// switch state
       	SeekingPesHeader = false;
       	GotPartialPesHeader = true;
       	GotPartialPesHeaderBytes = 0;
    }
    else
    {
	// examine the end of the buffer to determine if there is a (potential) trailing start code
	//assert( RemainingLength >= 1 );
	if ( RemainingData[RemainingLength - 1] < 1 )
	{
	    unsigned char LastBytes[3];

	    LastBytes[0] = (RemainingLength >= 3 ? RemainingData[RemainingLength - 3] : 0xff);
	    LastBytes[1] = (RemainingLength >= 2 ? RemainingData[RemainingLength - 2] : 0xff);
	    LastBytes[2] = RemainingData[RemainingLength - 1];

	    if ( LastBytes[0] == 0 && LastBytes[1] == 0 && LastBytes[2] == 1 )
	    {
		TrailingStartCodeBytes = 3;
	    }
	    else if ( LastBytes[1] == 0 && LastBytes[2] == 0)
	    {
		TrailingStartCodeBytes = 2;
	    }
	    else if ( LastBytes[2] == 0 )
	    {
		TrailingStartCodeBytes = 1;
	    }
	    
	}

    	COLLATOR_DEBUG("Discarded %d bytes while searching for PES header (%d might be start code)\n",
	               RemainingLength, TrailingStartCodeBytes);
    	RemainingLength = 0;
    }
    
//
       
    return CollatorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Accumulate incoming data until we have the full PES header.
///
/// Strictly speaking this method handles two sub-states. In the first state
/// we do not have sufficient data accumulated to determine how long the PES
/// header is. In the second we still don't have a complete PES packet but
/// at least we know how much more data we need.
///
/// This code assumes that PES packet uses >=9 bytes PES headers rather than
/// the 6 byte headers found in the program stream map, padding stream,
/// private stream 2, etc.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::ReadPartialPesHeader( void )
{
CollatorStatus_t Status;
unsigned int PesHeaderBytes, BytesNeeded, BytesToRead;
unsigned char PesPrivateData[MAX_PES_PRIVATE_DATA_LENGTH];

//

    if( GotPartialPesHeaderBytes < PES_INITIAL_HEADER_SIZE )
    {
    	COLLATOR_DEBUG("Waiting for first part of PES header\n");

	StoredPesHeader = BufferBase + AccumulatedDataSize - GotPartialPesHeaderBytes;

    	BytesToRead = min( RemainingLength, PES_INITIAL_HEADER_SIZE - GotPartialPesHeaderBytes );
    	
    	Status = AccumulateData( BytesToRead, RemainingData );
    	if( Status == CollatorNoError )
    	{
    	    GotPartialPesHeaderBytes += BytesToRead;
    	    RemainingData += BytesToRead;
    	    RemainingLength -= BytesToRead;
    	}
        else
        {
            COLLATOR_DEBUG( "Cannot accumulate data #6 (%d)\n", Status );
        }
    	
    	return Status;
    }
    
    //
    // We now have accumulated sufficient data to know how long the PES header actually is!
    //

    // pass the stream_id field to the collator sub-class (might update Configuration.ExtendedHeaderLength)
    SetPesPrivateDataLength(StoredPesHeader[3]);
    PesHeaderBytes = PES_INITIAL_HEADER_SIZE + StoredPesHeader[8] + Configuration.ExtendedHeaderLength;
    BytesNeeded = PesHeaderBytes - GotPartialPesHeaderBytes;
    BytesToRead = min( RemainingLength, BytesNeeded );
    
    Status = AccumulateData( BytesToRead, RemainingData );
    if( Status == CollatorNoError )
    {
    	GotPartialPesHeaderBytes += BytesToRead;
    	RemainingData += BytesToRead;
    	RemainingLength -= BytesToRead;

	COLLATOR_DEBUG( "BytesNeeded %d; BytesToRead %d\n", BytesNeeded, BytesToRead );
    	if( BytesNeeded == BytesToRead )
    	{
    	    //
    	    // Woo hoo! We've got the whole header, examine it and change state
    	    //
    	    
    	    COLLATOR_DEBUG("Got entire PES header\n");
	    //report_dump_hex( severity_note, StoredPesHeader, PesHeaderBytes, 32, 0);


    	    Status = CollatorNoError; // strictly speaking this is a no-op but the code might change
   	    if( StoredPesHeader[0] != 0x00 || StoredPesHeader[1] != 0x00 || StoredPesHeader[2] != 0x01 ||
    	        CollatorNoError != ( Status = ReadPesHeader() ) )
    	    {
	      COLLATOR_DEBUG( "%s; seeking new PES header",
			      ( Status == CollatorNoError ? "Start code not where expected" :
				"Badly formed PES header" ) );
	      
	      SeekingPesHeader = true;
	      DiscardAccumulatedData();
	      
	      // we have contained the error by changing states...
	      return CollatorNoError;
    	    }
   	    
   	    //
	    // Placeholder: Generic stream id based PES filtering (configured by sub-class) could be inserted
            //              here (set DiscardPesPacket to true to discard).
	    //
	    
	    if (Configuration.ExtendedHeaderLength)
	    {
	        // store a pointer to the PES private header. it is located just above the end of the
	        // accumulated data and is will be safely accumulated providing the private header is
	        // smaller than the rest of the PES packet. if a very large PES private header is
	        // encountered we will need to introduce a temporary buffer to store the header in.
	        if (Configuration.ExtendedHeaderLength <= MAX_PES_PRIVATE_DATA_LENGTH)
		{
		  memcpy(PesPrivateData, BufferBase + AccumulatedDataSize - Configuration.ExtendedHeaderLength, Configuration.ExtendedHeaderLength);
		}
		else
		{
		  COLLATOR_ERROR("Implementation error: Pes Private data area too big for temporay buffer\n");
		}

	        Status = HandlePesPrivateData( PesPrivateData );
	        if( Status != CollatorNoError )
	        {
	            COLLATOR_ERROR("Unhandled error when parsing PES private data\n");
		    return (Status);
	        }
	    }

	    // discard the actual PES packet from the accumulate buffer
	    AccumulatedDataSize -= PesHeaderBytes;
	    
	    // record the number of bytes we need to ignore before we reach the next start code
	    PesPayloadRemaining = PesPayloadLength;
	    
	    // switch states and absorb the packet
	    COLLATOR_DEBUG("Discovered PES packet (header %d bytes, payload %d bytes)\n",
	                   PesPacketLength - PesPayloadLength + 6, PesPayloadLength);
    	    GotPartialPesHeader = false;
    	    
    	    if( PassPesPrivateDataToElementaryStreamHandler && Configuration.ExtendedHeaderLength)
    	    {
    	    	// update PesPacketLength (to ensure that GetOffsetIntoPacket gives the correct value)
    	    	PesPayloadLength += Configuration.ExtendedHeaderLength;

    	    	Status = HandleElementaryStream( Configuration.ExtendedHeaderLength, PesPrivateData );
    	    	if( Status != CollatorNoError )
    	    	{
    	    	    COLLATOR_ERROR("Failed not accumulate the PES private data area\n");
    	    	}
    	    }
    	}
    }
    else
    {
        COLLATOR_DEBUG( "Cannot accumulate data #7 (%d)\n", Status );
    }
    
    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Pass data to the elementary stream handler until the PES payload is consumed.
///
/// When all data has been consumed then switch back to the GotPartialPesHeader
/// state since (if the stream is correctly formed) the next three bytes will
/// contain the PES start code.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::ReadPesPacket( void )
{
CollatorStatus_t Status;
unsigned int BytesToRead;

//
    
    BytesToRead = min( PesPayloadRemaining, RemainingLength );
    
    Status = HandleElementaryStream( BytesToRead, RemainingData );
    if( Status == CollatorNoError )
    {
	if (BytesToRead == PesPayloadRemaining)
	{
	    GotPartialPesHeader = true;
	    GotPartialPesHeaderBytes = 0;
	}
	
	RemainingData += BytesToRead;
	RemainingLength -= BytesToRead;
	PesPayloadRemaining -= BytesToRead;
    }
   
//
    
    return CollatorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Reset the elementary stream state machine.
///
/// If we are forced to emit a frame the elementary stream state machine (and any
/// control variables) must be reset.
///
/// This method exists primarily for sub-classes to hook. For most sub-classes
/// the trivial reset logic below is sufficient but those that maintain state
/// (especially those handling PES private data) will need to work harder.
///
void Collator_PesAudio_c::ResetCollatorStateAfterForcedFrameFlush()
{
    // we have been forced to emit the frame against our will (e.g. someone other that the collator has
    // caused the data to be emitted). we therefore have to start looking for a new sync word. don't worry
    // if the external geezer got it right this will be right in front of our nose.

    CollatorState = SeekingSyncWord;
}

////////////////////////////////////////////////////////////////////////////
///
/// De-packetize incoming data and pass it to the elementary stream handler.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t   Collator_PesAudio_c::Input(PlayerInputDescriptor_t	 *Input,
					      unsigned int		  DataLength,
					      void			 *Data,
					      bool			  NonBlocking,
					      unsigned int		 *DataLengthRemaining )
{
CollatorStatus_t	Status;

//
    st_relayfs_write(ST_RELAY_TYPE_PES_AUDIO_BUFFER, ST_RELAY_SOURCE_AUDIO_COLLATOR, (unsigned char *)Data, DataLength, 0 );

    COLLATOR_ASSERT( !NonBlocking );
    AssertComponentState( "Collator_PesAudio_c::Input", ComponentRunning );
    InputEntry( Input, DataLength, Data, NonBlocking );

    ActOnInputDescriptor( Input );
    
    //
    // Copy our arguments to class members so the utility functions can
    // act upon it.
    //
    
    RemainingData	= (unsigned char *)Data;
    RemainingLength	= DataLength;

    //
    // Continually handle units of input until all input is exhausted (or an error occurs).
    //
    // Be aware that our helper functions may, during their execution, cause state changes
    // that result in a different branch being taken next time round the loop.
    //

    Status = CollatorNoError;
    while( Status == CollatorNoError &&  RemainingLength != 0 )
    {
    	//COLLATOR_DEBUG("De-PESing loop has %d bytes remaining\n", RemainingLength);
    	//report_dump_hex( severity_note, RemainingData, min(RemainingLength, 188), 32, 0);
    	
    	if( SeekingPesHeader ) 
    	{	
    	    //
    	    // Try to lock to incoming PES headers
    	    //
    	    
    	    Status = SearchForPesHeader();
    	}
    	else if ( GotPartialPesHeader )
    	{
    	    //
    	    // Read in the remains of the PES header
    	    //
    	    
    	    Status = ReadPartialPesHeader();
    	}
    	else
    	{
    	    //
    	    // Send the PES packet for frame level analysis
    	    //
    	    
    	    Status = ReadPesPacket();
    	}
    }
    
    if( Status != CollatorNoError )
    {
    	// if anything when wrong then we need to resynchronize
    	COLLATOR_DEBUG("General failure; seeking new PES header\n");
    	DiscardAccumulatedData();
    	SeekingPesHeader = true;
    }

    InputExit();
    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Bring all class state variables to their initial values.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t   Collator_PesAudio_c::Reset(	void )
{
    PesPayloadRemaining = 0;
    TrailingStartCodeBytes = 0;
    
    CollatorState = SeekingSyncWord;
    GotPartialFrameHeaderBytes = 0;
    StoredFrameHeader = NULL;
    
    FramePayloadRemaining = 0;
    
    AccumulatedFrameReady = false;
    PassPesPrivateDataToElementaryStreamHandler = true;
    DiscardPesPacket = false;
    ReprocessAccumulatedDataDuringErrorRecovery = true;
    AlreadyHandlingMissingNextFrameHeader = false;

    PotentialFrameHeaderLength = 0;

    RemainingElementaryLength = 0;
    RemainingElementaryOrigin = NULL;
    RemainingElementaryData   = NULL;
    
    FrameHeaderLength = 0;

    return Collator_Pes_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Pass accumulated data to the output ring and maintain class state variables.
///
/// \todo If we were more clever about buffer management we wouldn't have to
///       copy the frame header onto the stack.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t   Collator_PesAudio_c::InternalFrameFlush(		void )
{
CollatorStatus_t	Status;
unsigned char CopiedFrameHeader[FrameHeaderLength];
//

    AssertComponentState( "Collator_PesAudio_c::InternalFrameFlush", ComponentRunning );

//
    COLLATOR_DEBUG(">><<\n");

    // temporarily copy the following frame header (if there is one) to the stack
    if( AccumulatedFrameReady )
    {
        memcpy(CopiedFrameHeader, StoredFrameHeader, FrameHeaderLength);
        AccumulatedDataSize -= FrameHeaderLength;
        //Assert( BufferBase + AccumulatedDataLength == StoredFrameHeader );
    }
    
    // now pass the complete frame onward
    Status					= Collator_Pes_c::InternalFrameFlush();
    if( Status != CodecNoError )
	return Status;
	
    if( AccumulatedFrameReady )
    {
   	// put the stored frame header into the new buffer
    	Status = AccumulateData( FrameHeaderLength, CopiedFrameHeader );
        if( Status != CollatorNoError )
            COLLATOR_DEBUG( "Cannot accumulate data #8 (%d)\n", Status );
    	AccumulatedFrameReady = false;
    }
    else
    {
	ResetCollatorStateAfterForcedFrameFlush();
    }

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Call the super-class DiscardAccumulatedData and make sure that AccumlatedFrameReady is false.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t   Collator_PesAudio_c::DiscardAccumulatedData(		void )
{
CollatorStatus_t Status;

    Status = Collator_Pes_c::DiscardAccumulatedData();
    AccumulatedFrameReady = false;
    
    return Status;;
}

////////////////////////////////////////////////////////////////////////////
///
/// Examine the PES private data header.
///
/// This is a stub implementation for collators that don't have a private
/// data area.
///
/// Collators that do must re-implement this method in order to extract any
/// useful data from it.
///
/// For collators that may, or may not, have a PES private data area this
/// method must also be responsible for determining which type of stream
/// is being played. See Collator_PesAudioEAc3_c::HandlePesPrivateData()
/// for more an example of such a collator.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::HandlePesPrivateData(unsigned char *PesPrivateData)
{
    return CollatorNoError;
}
