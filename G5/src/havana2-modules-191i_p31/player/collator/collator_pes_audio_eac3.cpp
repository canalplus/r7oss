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

Source file name : collator_pes_audio_eac3.cpp
Author :           Sylvain

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
22-May-07   Created (from collator_pes_audio_ac3.cpp)      Sylvain

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesAudioEAc3_c
///
/// Implements EAC3 audio sync word scanning and frame length analysis.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_audio_eac3.h"
#include "frame_parser_audio_eac3.h"
#include "eac3_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define EXTENDED_STREAM_PES_START_CODE          0xfd
#define EXTENDED_STREAM_PES_FULL_START_CODE     0x000001fd

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
Collator_PesAudioEAc3_c::Collator_PesAudioEAc3_c( void )
{
  if( InitializationStatus != CollatorNoError )
    return;

  Collator_PesAudioEAc3_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Search for the EAC3 audio synchonrization word and, if found, report its offset.
///
/// Additionally this method will compare the location of any sync word found with
/// the predicted location provided by Collator_PesAudioEAc3_c::HandlePesPrivateDataArea()
/// and choose the appropriate encapsulation mode.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioEAc3_c::FindNextSyncWord( int *CodeOffset )
{
  int i;
  unsigned char eac3Header[EAC3_BYTES_NEEDED];
  EAc3AudioParsedFrameHeader_t ParsedFrameHeader;
  int RemainingInPotential = PotentialFrameHeaderLength;
  unsigned char * PotentialFramePtr = PotentialFrameHeader;
  unsigned char * ElementaryPtr;
  int Offset;

  // do the most naive possible search. there is no obvious need for performance here
  for( i=0; i<=(int)(RemainingElementaryLength + PotentialFrameHeaderLength - EAC3_BYTES_NEEDED); i++) 
  {
      if (RemainingInPotential > 0)
	  {
		  /* we need at least 65 bytes to get the stream type...*/
		  int size =  min(RemainingInPotential, EAC3_BYTES_NEEDED);
		  memcpy(&eac3Header[0], PotentialFramePtr, size);
		  memcpy(&eac3Header[size], &RemainingElementaryData[0], EAC3_BYTES_NEEDED - size);
		  ElementaryPtr = eac3Header;
	  }
      else
	  {
		  ElementaryPtr = &RemainingElementaryData[i - PotentialFrameHeaderLength];
	  }

	  FrameParserStatus_t FPStatus = FrameParser_AudioEAc3_c::ParseSingleFrameHeader( ElementaryPtr,
																					  &ParsedFrameHeader,
																					  true);
	  
	  if( FPStatus == FrameParserNoError )
	  {
		  // the condition for synchronization is the following:
		  // * the frame is a regular AC3 frame, OR
		  // * the frame is an independant E-AC3 (DD+) subframe, belonging to requested programme, containing 6 blocks (1536 samples), OR
		  // * the frame is a independant E-AC3 (DD+) subframe, belonging to requested programme, containing less than six blocks, but with the convsync on 
		  // (i.e. this is the first block of the six...)
		  if ( (ParsedFrameHeader.Type == TypeAc3) ||
			   ((ParsedFrameHeader.Type == TypeEac3Ind) && (ParsedFrameHeader.SubStreamId == ProgrammeId) && (ParsedFrameHeader.NumberOfSamples == EAC3_NBSAMPLES_NEEDED)) ||
			   ((ParsedFrameHeader.Type == TypeEac3Ind) && (ParsedFrameHeader.SubStreamId == ProgrammeId) && (ParsedFrameHeader.FirstBlockForTranscoding)) )
		  {
			  Offset = (RemainingInPotential > 0)?(-RemainingInPotential):(i-PotentialFrameHeaderLength);
		    	  VerifyDvdSyncWordPrediction( Offset );

			  COLLATOR_DEBUG(">>Got Synchronization, i = %d <<\n", Offset);
			  *CodeOffset = Offset;
			  return CollatorNoError;
		  }
	  }
	  RemainingInPotential--;
	  PotentialFramePtr++;
  }
  
  AdjustDvdSyncWordPredictionAfterConsumingData( -RemainingElementaryLength );
  return CollatorError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Determine the new state of the collator according to the incoming sub frame
/// Also returns this sub frame length
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioEAc3_c::DecideCollatorNextStateAndGetLength( unsigned int *FrameLength )
{
  FrameParserStatus_t FPStatus;
  CollatorStatus_t Status = CollatorNoError; // assume success unless told otherwise
  EAc3AudioParsedFrameHeader_t ParsedFrameHeader;

  //

  FPStatus = FrameParser_AudioEAc3_c::ParseSingleFrameHeader( StoredFrameHeader,
															  &ParsedFrameHeader,
															  false);

  if( FPStatus == FrameParserNoError )
  {
      *FrameLength = ParsedFrameHeader.Length;
	  
      if (CollatorState == SeekingFrameEnd)
	  {
		  // we already have an independant substream accumulated, check what to do with
		  // this next one
		  if (((ParsedFrameHeader.Type == TypeEac3Ind) && (ParsedFrameHeader.SubStreamId == ProgrammeId) && (NbAccumulatedSamples == EAC3_NBSAMPLES_NEEDED))
			  || (ParsedFrameHeader.Type == TypeAc3))
		  {
			  // this is another independant subframe
			  CollatorState = GotCompleteFrame;
			  NbAccumulatedSamples = ParsedFrameHeader.NumberOfSamples;
			  ResetDvdSyncWordHeuristics();
			  COLLATOR_DEBUG("Got a complete frame!\n");
		  }
		  else if ( ParsedFrameHeader.SubStreamId != ProgrammeId )
		  {
			  // skip any subframe (independant or dependant) that does not belong to the requested programme
			  CollatorState = SkipSubFrame;
		  }
		  else if ( NbAccumulatedSamples <= EAC3_NBSAMPLES_NEEDED )
		  {
			  // according to collator channel configuration,
			  // read or skip the dependant subframe of the right programme...
			  CollatorState = EightChannelsRequired?ReadSubFrame:SkipSubFrame;
			  if (ParsedFrameHeader.Type == TypeEac3Ind)
			  {
				  // acumulate samples only if this is an eac3 independant bistream (dependant bitstream provide additional channels only)
				  NbAccumulatedSamples += ParsedFrameHeader.NumberOfSamples;
			  }
			  COLLATOR_DEBUG("Accumulate a subframe of %d/%d samples\n", ParsedFrameHeader.NumberOfSamples, NbAccumulatedSamples);
		  }
		  else 
		  {
			  COLLATOR_ERROR("Accumulated too many samples (%d of %d)\n",
					 NbAccumulatedSamples, EAC3_NBSAMPLES_NEEDED);
			  Status = CollatorError; 
		  }
	  }
      else
	  {
		  COLLATOR_DEBUG("Synchronized first block is a good candidate for transcoding\n");
		  CollatorState = ReadSubFrame;
		  NbAccumulatedSamples = ParsedFrameHeader.NumberOfSamples;
	  }
  }
  else
  {
      Status = CollatorError;
  }
  
  return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Sets the extended pes header length according to the stream_id field of the pes header
///
/// \return void
///
void  Collator_PesAudioEAc3_c::SetPesPrivateDataLength(unsigned char SpecificCode)
{
  /* by default the optional private data area length will be set to zero... */
  /* otherwise for DVD the private data area is 4 bytes long */
  Configuration.ExtendedHeaderLength = (SpecificCode == PES_START_CODE_PRIVATE_STREAM_1)?4:0;
}

////////////////////////////////////////////////////////////////////////////
///
/// Examine the PES private data header.
///
/// The data we have been passed may, or may not, be a PES private data area.
/// DVD stream will have the PES private data area but Bluray and broadcast
/// streams will not.
///
/// The private data only has about 10 non-variable bits so it is unsafe to
/// perform a simple signature check. Instead we use the values to predict the
/// location within the stream of the next sync word. If the sync word is found
/// at the predicted location then Collator_PesAudioEAc3_c::FindNextSyncWord()
/// will clear Collator_PesAudio_c::PassPesPrivateDataToElementaryStreamHandler
/// and we will progress as though we had DVD style PES encapsulation.
///
/// Should we ever loose sync then we will automatically switch back to
/// broadcast mode.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioEAc3_c::HandlePesPrivateData(unsigned char *PesPrivateData)
{
  BitStreamClass_c Bits;

  if( CollatorState == SeekingSyncWord)
  {
    // ensure the PES private data is passed to the sync detection code
    PassPesPrivateDataToElementaryStreamHandler = true;

    // parse the private data area (assuming that is what we have)
    Bits.SetPointer(PesPrivateData);
    unsigned int SubStreamId = Bits.Get(8);
    unsigned int NumberOfFrameHeaders = Bits.Get(8);
    unsigned int FirstAccessUnitPointer = Bits.Get(16);

    if( ((SubStreamId & 0xb8) != 0x80) ||
	(NumberOfFrameHeaders > 127) ||
	(FirstAccessUnitPointer > 2034) ||
	(FirstAccessUnitPointer == 0) )
    {
      MakeDvdSyncWordPrediction( INVALID_PREDICTION );
    }
    else
    {
      // FirstAccessUnitPointer is relative to the final byte of the private data area. Since
      // the private data area will itself be scanned for start codes this means that we must
      // add three to our predicted offset.
      MakeDvdSyncWordPrediction( FirstAccessUnitPointer + 3 );
    }
  }

  return CollatorNoError;
}


CollatorStatus_t Collator_PesAudioEAc3_c::Reset( void )
{
  CollatorStatus_t Status;

  //

  COLLATOR_DEBUG(">><<\n");

  Status = Collator_PesAudioDvd_c::Reset();
  if( Status != CollatorNoError )
    return Status;

  // FrameHeaderLength belongs to Collator_PesAudio_c so we must set it after the class has been reset    
  FrameHeaderLength = EAC3_BYTES_NEEDED;

  Configuration.StreamIdentifierMask       = 0xff;
  Configuration.StreamIdentifierCode       = 0xbd; // from Rome...
  Configuration.BlockTerminateMask         = 0xff;         // Picture
  Configuration.BlockTerminateCode         = 0x00;
  Configuration.IgnoreCodesRangeStart      = 0x01; // All slice codes
  Configuration.IgnoreCodesRangeEnd        = 0xbd - 1;
  Configuration.InsertFrameTerminateCode   = false;
  Configuration.TerminalCode               = 0;
  Configuration.ExtendedHeaderLength       = 0 /* 4 */;
  Configuration.DeferredTerminateFlag      = false;

  ProgrammeId = 0;
  NbAccumulatedSamples = 0;
  
  // by default do only 5.1...
  EightChannelsRequired = true;

  return CollatorNoError;
}

