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

Source file name : collator_pes_audio_dtshd.cpp
Author :           Sylvain

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
06-Jun-07   Created                                         Sylvain

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesAudioDtshd_c
///
/// Implements DTSHD audio sync word scanning and frame length analysis.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_audio_dtshd.h"
#include "frame_parser_audio_dtshd.h"
#include "dtshd.h"

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
Collator_PesAudioDtshd_c::Collator_PesAudioDtshd_c( void )
{
  if( InitializationStatus != CollatorNoError )
    return;

  Collator_PesAudioDtshd_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Search for the DTSHD audio synchonrization word and, if found, report its offset.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioDtshd_c::FindNextSyncWord( int *CodeOffset )
{
  int i;
  unsigned char DtshdHeader[DTSHD_FRAME_HEADER_SIZE];

  int RemainingInPotential = PotentialFrameHeaderLength;
  unsigned char * PotentialFramePtr = PotentialFrameHeader;
  unsigned char * ElementaryPtr;
  int Offset;

  // do the most naive possible search. there is no obvious need for performance here
  for( i=0; i<=(int)(RemainingElementaryLength + PotentialFrameHeaderLength - DTSHD_FRAME_HEADER_SIZE); i++) 
    {
      unsigned int SyncWord;
      if (RemainingInPotential > 0)
	{
	  /* we need at least DTSHD_FRAME_HEADER_SIZE bytes to get the stream type...*/
	  int size =  min(RemainingInPotential, DTSHD_FRAME_HEADER_SIZE);
	  memcpy(&DtshdHeader[0], PotentialFramePtr, size);
	  memcpy(&DtshdHeader[size], &RemainingElementaryData[0], DTSHD_FRAME_HEADER_SIZE - size);
	  ElementaryPtr = DtshdHeader;
	}
      else
	{
	  ElementaryPtr = &RemainingElementaryData[i - PotentialFrameHeaderLength];
	}

      Bits.SetPointer(ElementaryPtr);
      SyncWord = Bits.Get(32);

      if ( ((SyncWord == DTSHD_START_CODE_CORE) && (Bits.Show(6) == DTSHD_START_CODE_CORE_EXT)) ||
	   (SyncWord == DTSHD_START_CODE_SUBSTREAM) ||
	   ((SyncWord == DTSHD_START_CODE_CORE_14_1) && (Bits.Show(12) == DTSHD_START_CODE_CORE_14_2_EXT)))
	{
	  Offset = (RemainingInPotential > 0)?(-RemainingInPotential):(i-PotentialFrameHeaderLength);

	  COLLATOR_DEBUG(">>Got Synchronization, i = %d, Offset = %d <<\n", i, Offset);
	  *CodeOffset = Offset;

	  VerifyDvdSyncWordPrediction( Offset );

	  if (SyncWord == DTSHD_START_CODE_SUBSTREAM)
	    {
	      GotCoreFrameSize = true;
	    }
	  else
	    {
	      GotCoreFrameSize = false;
	    }
	  // Getting disynchronized resets previous data...
	  CoreFrameSize = 0;
	  memset(  &SyncFrameHeader, 0, sizeof(DtshdAudioSyncFrameHeader_t) );

	  return CollatorNoError;
	}
      RemainingInPotential--;
      PotentialFramePtr++;
    }

  /* means we searched unsuccessfully for any sync inside the private data area */
  AdjustDvdSyncWordPredictionAfterConsumingData( -RemainingElementaryLength );

  return CollatorError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Search for the DTSHD audio synchonrization word and, if found, report its offset.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioDtshd_c::FindAnyNextSyncWord( int *CodeOffset, DtshdStreamType_t * Type)
{
  int i, OffsetIntoHeader = 0;
  unsigned char DtshdHeader[DTSHD_RAW_SYNCHRO_BYTES_NEEDED];

  int             RemainingInPotential = FrameHeaderLength;
  unsigned char * PotentialFramePtr = StoredFrameHeader;
  unsigned char * ElementaryPtr;

  // if we just got synchronized, skip the first synchro bytes
  if (CollatorState == GotSynchronized)
    {
      OffsetIntoHeader = DTSHD_RAW_SYNCHRO_BYTES_NEEDED;
      RemainingInPotential -= DTSHD_RAW_SYNCHRO_BYTES_NEEDED;
    }

  // do the most naive possible search. there is no obvious need for performance here
  for( i=OffsetIntoHeader; i<=(int)(RemainingElementaryLength + FrameHeaderLength - DTSHD_RAW_SYNCHRO_BYTES_NEEDED); i++) 
    {
      unsigned int SyncWord;
      if (RemainingInPotential > 0)
	{
	  /* we need at least DTSHD_RAW_SYNCHRO_BYTES_NEEDED bytes to get the stream type...*/
	  int size =  min(RemainingInPotential, DTSHD_RAW_SYNCHRO_BYTES_NEEDED);
	  memcpy(&DtshdHeader[0], &PotentialFramePtr[i], size);
	  memcpy(&DtshdHeader[size], &RemainingElementaryData[0], DTSHD_RAW_SYNCHRO_BYTES_NEEDED - size);
	  ElementaryPtr = DtshdHeader;
	}
      else
	{
	  ElementaryPtr = &RemainingElementaryData[i - FrameHeaderLength];
	}

      Bits.SetPointer(ElementaryPtr);
      SyncWord = Bits.Get(32);

      if ( (SyncWord == DTSHD_START_CODE_CORE) || 
	  (SyncWord == DTSHD_START_CODE_SUBSTREAM) || 
	  (SyncWord == DTSHD_START_CODE_CORE_14_1) && (Bits.Show(6) == DTSHD_START_CODE_CORE_14_2))
	{
	  *Type = (SyncWord == DTSHD_START_CODE_SUBSTREAM)?TypeDtshdExt:TypeDtshdCore;
	  *CodeOffset = (RemainingInPotential > 0)?(-RemainingInPotential):(i-FrameHeaderLength);

	  COLLATOR_DEBUG(">>Got Synchronization, i = %d <<\n", *CodeOffset);

	  return CollatorNoError;
	}
      RemainingInPotential--;
    }

  return CollatorError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Determine the new state of the collator according to the incoming sub frame
/// Also returns this sub frame length
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioDtshd_c::DecideCollatorNextStateAndGetLength( unsigned int *FrameLength )
{
  FrameParserStatus_t FPStatus;
  CollatorStatus_t Status;
  DtshdAudioParsedFrameHeader_t ParsedFrameHeader;

  //

  // first check if we know about the real frame size when it comes to dts core sync
  if (!GotCoreFrameSize)
  {
      int CodeOffset;
      DtshdStreamType_t Type;
      
      // To determine frame length, search for a synchro in the provided buffer 
      Status = FindAnyNextSyncWord( &CodeOffset, &Type );
      
      if (Status == CollatorNoError)
      {
          GotCoreFrameSize = true;

	  // CodeOffset may be negative, indicating the sync word has been found fully or
          // paritally within the frame header we already accumulated. Given this
	  // meaning it is impossible for the FrameHeaderLength + CodeOffset to be
	  // negative.
          COLLATOR_ASSERT(((int) FrameHeaderLength + CodeOffset) >= 0);
          
	  *FrameLength = FrameHeaderLength + CodeOffset;

	  if (0 == *FrameLength)
	  {
	      // The sync word is contained within the first byte of the StoredFrameHeader
	      // just as it would be it we had known the CoreFrameSize originally. At this
	      // stage therefore CoreFrameSize already has the right value (we are about to
	      // add zero to it) so we can simply change the value of GotCoreFrameSize
	      // (which we have already done) and try again.
	      return DecideCollatorNextStateAndGetLength(FrameLength);
	  }
      }
      else
      {
          // synchro not found, but frame is at least the size of the buffer we just searched
	  *FrameLength = RemainingElementaryLength + FrameHeaderLength - DTSHD_RAW_SYNCHRO_BYTES_NEEDED;
      }

      CollatorState = ReadSubFrame;
      CoreFrameSize += *FrameLength;
      Status = CollatorNoError;
  }
  else
  {
      DtshdParseModel_t ParseModel = {ParseForSynchro, 0, 0};

      FPStatus = FrameParser_AudioDtshd_c::ParseSingleFrameHeader( StoredFrameHeader,
                                                                   &ParsedFrameHeader,
                                                                   &Bits,
                                                                   FrameHeaderLength,
                                                                   RemainingElementaryData,
                                                                   RemainingElementaryLength,
                                                                   ParseModel,
                                                                   0 );
      
      if( FPStatus == FrameParserNoError )
      {
          if (CollatorState == GotSynchronized)
          {
              // remember the first synchronized substream properties
              memcpy(&SyncFrameHeader, &ParsedFrameHeader, sizeof(DtshdAudioSyncFrameHeader_t));
          }
          
          Status = CollatorNoError;
          
          if (ParsedFrameHeader.Type == TypeDtshdCore)
          {
              if (SyncFrameHeader.Type == TypeDtshdExt)
              {
                  // we  should not encounter such core streams, this means 
                  // we synchron    ized on a false substream...
                  // so we will accumulate t    his wrong extension frame, plus the regular frame
                  // and the frame parser will reanalyse     this situation to tell the codec to
                  // start decoding atfer he extension frame    
                  GotCoreFrameSize = false;
                  // get rid of frame header    
                  *FrameLength     = 0;
                  // accumulate this frame...       
                  CollatorState = GotSynchronized;
                  SyncFrameHeader.Type = TypeDtshdCore;
              }
              else
              {
                  // regular case, the next frame is a core substream that means 
                  // that this frame is over
                  *FrameLength = CoreFrameSize;
                  CollatorState = GotCompleteFrame;
              }
          }
          else if ( ParsedFrameHeader.Type == TypeDtshdExt)
          {
              if ((SyncFrameHeader.Type == TypeDtshdExt) &&
                  (SyncFrameHeader.SubStreamId == ParsedFrameHeader.SubStreamId) &&
                  (CollatorState != GotSynchronized) )
              {
                  // case of a pure dts-hd frame composed of extension substreams only
                  *FrameLength = ParsedFrameHeader.Length;
                  CollatorState = GotCompleteFrame;
              }
              else
              {
                  // ac cumulate this frame...
                  CollatorState = ReadSubFrame;
                  // we are confident wi    th these data...
                  *FrameLength  = ParsedFrameHeader.Length;
              }
          }
      }
      else
      {
          Status = CollatorError;
      }
  }
  
  
  if (CollatorState == GotCompleteFrame)
    {
      CodedFrameParameters->DataSpecificFlags = CoreFrameSize;
      ResetDvdSyncWordHeuristics();
    }

  return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Sets the extended pes header length according to the stream_id field of the pes header
///
/// \return void
///
void  Collator_PesAudioDtshd_c::SetPesPrivateDataLength(unsigned char SpecificCode)
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
/// at the predicted location then Collator_PesAudioDtshd_c::FindNextSyncWord()
/// will clear Collator_PesAudio_c::PassPesPrivateDataToElementaryStreamHandler
/// and we will progress as though we had DVD style PES encapsulation.
///
/// Should we ever loose sync then we will automatically switch back to
/// broadcast mode.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioDtshd_c::HandlePesPrivateData(unsigned char *PesPrivateData)
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

    COLLATOR_DEBUG("FirstAccessUnitPointer: %d\n", FirstAccessUnitPointer);

    if( ((SubStreamId & 0xf8) != 0x88) ||
	(NumberOfFrameHeaders > 127) ||
	(FirstAccessUnitPointer > 2034) )
    {
      MakeDvdSyncWordPrediction( INVALID_PREDICTION );
    }
    else
    {
      // FirstAccessUnitPointer is relative to the final byte of the private data area. Since
      // the private data area will itself be scanned for start codes this means that we must
      // add three to our predicted offset.
      //
      // We also have a special case for FirstAccessUnitPointer equal to zero which means no access units
      // start within the frame. Some streams have been observed where the FirstAccessUnitPointer is zero
      // but the DTS frame synchronization follows the PES header (i.e. FirstAccessUnitPointer should be 1).
      // Such streams are badly authored but working around the problem here is very unlikely to cause
      // harm to broadcast-style streams.
      MakeDvdSyncWordPrediction( ((FirstAccessUnitPointer == 0)?1:FirstAccessUnitPointer) + 3 );
    }
  }

  return CollatorNoError;
}

CollatorStatus_t Collator_PesAudioDtshd_c::Reset( void )
{
  CollatorStatus_t Status;

  //

  COLLATOR_DEBUG(">><<\n");

  Status = Collator_PesAudioDvd_c::Reset();
  if( Status != CollatorNoError )
    return Status;

  // FrameHeaderLength belongs to Collator_PesAudio_c so we must set it after the class has been reset    
  FrameHeaderLength = DTSHD_FRAME_HEADER_SIZE;

  Configuration.StreamIdentifierMask       = 0xff;
  Configuration.StreamIdentifierCode       = PES_START_CODE_PRIVATE_STREAM_1;
  Configuration.BlockTerminateMask         = 0xff;         // Picture
  Configuration.BlockTerminateCode         = 0x00;
  Configuration.IgnoreCodesRangeStart      = 0x01; // All slice codes
  Configuration.IgnoreCodesRangeEnd        = PES_START_CODE_PRIVATE_STREAM_1 - 1;
  Configuration.InsertFrameTerminateCode   = false;
  Configuration.TerminalCode               = 0;
  Configuration.ExtendedHeaderLength       = 0;
  Configuration.DeferredTerminateFlag      = false;

  // by default do only 5.1...
  EightChannelsRequired = false;

  // default frame size is unset...
  CoreFrameSize = 0;
  GotCoreFrameSize = false;
  memset(  &SyncFrameHeader, 0, sizeof(DtshdAudioSyncFrameHeader_t));

  return CollatorNoError;
}
