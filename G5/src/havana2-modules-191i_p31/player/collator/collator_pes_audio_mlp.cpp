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

Source file name : collator_pes_audio_mlp.cpp
Author :           Sylvain

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
08-Oct-07   Creation                                        Sylvain

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesAudioMlp_c
///
/// Implements MLP audio sync word scanning and frame length analysis.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_audio_mlp.h"
#include "frame_parser_audio_mlp.h"
#include "mlp.h"


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
#define MLP_HEADER_SIZE 14


#define NB_SAMPLES_MAX (4096/2)
#define NB_SAMPLES_48_KHZ 960
#define NB_SAMPLES_96_KHZ 1920
#define NB_SAMPLES_192_KHZ 1920
#warning "FIXME:update NB_SAMPLES_192_KHZ to 3840 when Dan improves the AVSync"

const char NbAudioFramesToGlob[MlpSamplingFreqNone] =
{ 
  NB_SAMPLES_48_KHZ/40,    ///< 48 kHz 
  NB_SAMPLES_96_KHZ/80,    ///< 96 kHz 
  NB_SAMPLES_192_KHZ/160,   ///< 192 kHz 
  0, 
  0, 
  0, 
  0, 
  0, 
  NB_SAMPLES_48_KHZ/40,  ///< 44.1 kHz 
  NB_SAMPLES_96_KHZ/80,  ///< 88.2 kHz 
  NB_SAMPLES_192_KHZ/160  ///< 176.4 kHz 
};


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
Collator_PesAudioMlp_c::Collator_PesAudioMlp_c( void )
{
    if( InitializationStatus != CollatorNoError )
	return;

    Collator_PesAudioMlp_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Search for the MLP audio synchonrization word and, if found, report its offset.
///
/// Weak start codes are, in fact, the primary reason we have
/// to verify the header of the subsequent frame before emitting the preceeding one.
///
/// MLP frames synchronization can be of two types: minor and major sync
/// This function will synchronize on major mlp sync
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioMlp_c::FindNextSyncWord( int *CodeOffset )
{
  int i;
  unsigned char MlpHeader[MLP_HEADER_SIZE];

  int RemainingInPotential = PotentialFrameHeaderLength;
  unsigned char * PotentialFramePtr = PotentialFrameHeader;

  // do the most naive possible search. there is no obvious need for performance here
  for( i=0; i<=(int)(RemainingElementaryLength + PotentialFrameHeaderLength - MLP_HEADER_SIZE); i++) 
    {
      unsigned int SyncWord, Signature;
      unsigned char * ElementaryPtr;

      if (RemainingInPotential > 0)
	{
	  /* we need at least MLP_HEADER_SIZE bytes to get the stream type...*/
	  int size =  min(RemainingInPotential, MLP_HEADER_SIZE);
	  memcpy(&MlpHeader[0], PotentialFramePtr, size);
	  memcpy(&MlpHeader[size], &RemainingElementaryData[0], MLP_HEADER_SIZE - size);
	  ElementaryPtr = MlpHeader;
	}
      else
	{
	  ElementaryPtr = &RemainingElementaryData[i - PotentialFrameHeaderLength];
	}

      Bits.SetPointer(ElementaryPtr + 4);

      // get the major sync
      SyncWord = Bits.Get(32);
      Bits.FlushUnseen(32);
      Signature = Bits.Get(16);

      if ( ((SyncWord == MLP_FORMAT_SYNC_A) || (SyncWord == MLP_FORMAT_SYNC_B))
	   && (Signature == MLP_SIGNATURE) )
	{
	  int Offset = (RemainingInPotential > 0)?(-RemainingInPotential):(i-PotentialFrameHeaderLength);

	  COLLATOR_DEBUG(">>Got Synchronization, i = %d, Offset= %d <<\n", i, Offset);
	  *CodeOffset = Offset;

	  VerifyDvdSyncWordPrediction( Offset + StuffingBytesLength );

	  return CollatorNoError;
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
/// We need to glob a few audio frames since one audio MLP frame
/// is only 1/1200 or 1/1102.5 second
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioMlp_c::DecideCollatorNextStateAndGetLength( unsigned int *FrameLength )
{
  FrameParserStatus_t FPStatus;
  CollatorStatus_t Status;
  MlpAudioParsedFrameHeader_t ParsedFrameHeader;

  //

  if (StuffingBytesLength > 0)
  {
    COLLATOR_DEBUG("Skipping %d bytest of pda stuffing bytes\n", StuffingBytesLength);
    // get rid of the private data area stuffing bytes 
    *FrameLength = StuffingBytesLength;
    CollatorState = SkipSubFrame;
    return CollatorNoError;
  } 

  FPStatus = FrameParser_AudioMlp_c::ParseSingleFrameHeader( StoredFrameHeader,
							     &ParsedFrameHeader );

  if( FPStatus == FrameParserNoError )
  {
    // normally we have sync on a major sync frame, so we should for the 
    // very first frame go into this statement
    if (ParsedFrameHeader.IsMajorSync && (NbFramesToGlob == 0))
    {
      NbFramesToGlob = NbAudioFramesToGlob[ParsedFrameHeader.SamplingFrequency];
      COLLATOR_DEBUG("Setting number of frames to glob to %d\n", NbFramesToGlob);
    }
      
    if (CollatorState == SeekingFrameEnd)
    {
      AccumulatedFrameNumber += 1;
    }

    *FrameLength = ParsedFrameHeader.Length;

    if (AccumulatedFrameNumber >= NbFramesToGlob)
    {
      CollatorState = GotCompleteFrame;
      AccumulatedFrameNumber = 0;
      ResetDvdSyncWordHeuristics();
    }
    else if (ParsedFrameHeader.Length > FrameHeaderLength)
    {
      CollatorState = ReadSubFrame;
    }

    COLLATOR_DEBUG("Length: %d\n", ParsedFrameHeader.Length);

    Status = CollatorNoError;
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
void  Collator_PesAudioMlp_c::SetPesPrivateDataLength(unsigned char SpecificCode)
{
  /* by default the optional private data area length is set to zero */
  /*   (broadcast or Blu-ray mode */
  /* when the Pes stream_id is PES_START_CODE_PRIVATE_STREAM_1 we consider */
  /* this is a DVD-Audio, so we set the private data area to 10 bytes long */
  /* If we have mistaken, the HandlePesPrivateData will mark these bytes as part of the stream */

  Configuration.ExtendedHeaderLength = (IS_PES_START_CODE_PRIVATE_STREAM_1(SpecificCode))?MLP_PRIVATE_DATA_AREA_SIZE:0;

  if (IS_PES_START_CODE_EXTENDED_STREAM_ID(SpecificCode))
  {
    COLLATOR_DEBUG("Pes SubStream ID: 0x%x\n", StoredPesHeader[16]);
  }
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
CollatorStatus_t Collator_PesAudioMlp_c::HandlePesPrivateData(unsigned char *PesPrivateData)
{
  BitStreamClass_c Bits;

  // ensure the PES private data is passed to the sync detection code
  if( CollatorState == SeekingSyncWord)
  {
    PassPesPrivateDataToElementaryStreamHandler = true;
  }
  
  // parse the private data area (assuming that is what we have)
  Bits.SetPointer(PesPrivateData);
  unsigned int SubStreamId = Bits.Get(8);
  unsigned int Reserved = Bits.Get(3);
  Bits.FlushUnseen(5 + 8);
  unsigned int PrivateHeaderLength = Bits.Get(8);
  unsigned int FirstAccessUnitPointer = Bits.Get(16);
  
  COLLATOR_DEBUG("FirstAccessUnitPointer: %d \n", FirstAccessUnitPointer);
  
  if( ((SubStreamId & 0xff) != 0xA1) ||
      (Reserved != 0) ||
      (FirstAccessUnitPointer > 2034) ||
      (FirstAccessUnitPointer < 5) )
  {
    // there is no private data area of type Packed PCM (MLP in DVD-Audio)
    // check if we have a HD-DVD private data header type
    Bits.SetPointer(PesPrivateData + 1);
    FirstAccessUnitPointer = Bits.Get(16);
    if( ((SubStreamId & 0xf8) != 0xB0) ||
	(FirstAccessUnitPointer > 2025) ||
	(FirstAccessUnitPointer < 2) )
    {
      MakeDvdSyncWordPrediction( INVALID_PREDICTION );
    }
  }
  else
  {
    // FirstAccessUnitPointer is relative to the final byte of the private data area. Since
    // the private data area will itself be scanned for start codes this means that we must
    // add the private header length up to the first_acces_unit_pointer field 
    // to our predicted offset
    // see DVD Specifications for Read-Only Disc / PArt 4.Audio Specifications Table 7.2.3.1.2-2
    MakeDvdSyncWordPrediction( FirstAccessUnitPointer - 1 + 6 );
    StuffingBytesLength = PrivateHeaderLength - 6;
  }
  
  return CollatorNoError;
}


CollatorStatus_t Collator_PesAudioMlp_c::Reset( void )
{
CollatorStatus_t Status;

//

    COLLATOR_DEBUG(">><<\n");

    Status = Collator_PesAudioDvd_c::Reset();
    if( Status != CollatorNoError )
	return Status;

    // FrameHeaderLength belongs to Collator_PesAudio_c so we must set it after the class has been reset    
    FrameHeaderLength = MLP_HEADER_SIZE;

    Configuration.StreamIdentifierMask       = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode       = PES_START_CODE_PRIVATE_STREAM_1;

    Configuration.SubStreamIdentifierMask    = 0xff;
    Configuration.SubStreamIdentifierCode    = MLP_STREAM_ID_EXTENSION_MLP;

    Configuration.BlockTerminateMask         = 0xff;         // Picture
    Configuration.BlockTerminateCode         = 0x00;
    Configuration.IgnoreCodesRangeStart      = 0x01; // All slice codes
    Configuration.IgnoreCodesRangeEnd        = PES_START_CODE_PRIVATE_STREAM_1 - 1;
    Configuration.InsertFrameTerminateCode   = false;
    Configuration.TerminalCode               = 0;
    Configuration.ExtendedHeaderLength       = 0;
    Configuration.DeferredTerminateFlag      = false;

    AccumulatedFrameNumber = 0;
    NbFramesToGlob = 0;

    StuffingBytesLength = 0;

    return CollatorNoError;
}
