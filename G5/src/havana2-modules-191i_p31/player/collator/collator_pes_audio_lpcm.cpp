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

Source file name : collator_pes_audio_lpcm.cpp
Author :           Sylvain

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
06-Jun-07   Created                                         Sylvain

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesAudioPcm_c
///
/// Implements LPCM audio sync word scanning and frame length analysis.
///

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "collator_pes_audio_lpcm.h"
#include "frame_parser_audio_lpcm.h"
#include "lpcm.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//
//
// Lookup tables for LPCM header parsing
//

const char AudioPesPrivateDataLength[]=
{
  LPCM_DVD_VIDEO_PRIVATE_HEADER_LENGTH,
  LPCM_DVD_AUDIO_MIN_PRIVATE_HEADER_LENGTH,
  LPCM_HD_DVD_PRIVATE_HEADER_LENGTH,
  LPCM_BD_PRIVATE_HEADER_LENGTH,
  LPCM_BD_PRIVATE_HEADER_LENGTH // SPDIFIN
};

const static char FirstAccessUnitOffset[] =
{
  LPCM_DVD_VIDEO_FIRST_ACCES_UNIT,
  LPCM_DVD_AUDIO_FIRST_ACCES_UNIT,
  LPCM_HD_DVD_FIRST_ACCES_UNIT,
  LPCM_BD_FIRST_ACCES_UNIT,
  LPCM_BD_FIRST_ACCES_UNIT // SPDIFIN
};

#define NB_SAMPLES_MAX (4096/2)
#define NB_SAMPLES_48_KHZ 960
#define NB_SAMPLES_96_KHZ 1920
#define NB_SAMPLES_192_KHZ 1920

#define NB_SAMPLES_SPDIFIN 1024

#warning "FIXME:update NB_SAMPLES_192_KHZ to 3840 when Dan improves the AVSync"

const static char NbAudioFramesToGlob[TypeLpcmSPDIFIN + 1][LpcmSamplingFreqLast] =
{ 
  // DVD Video
  { 
    NB_SAMPLES_48_KHZ/80,   ///< 48 kHz 
    NB_SAMPLES_96_KHZ/160,  ///< 96 kHz 
    0, 
    0,
    0, 
    0, 
    0, 
    0, 
    0, 
    0, 
    0,    
  },
  // DVD Audio
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
  },
  // HD-DVD
  { 
    NB_SAMPLES_48_KHZ/40,    ///< 48 kHz 
    NB_SAMPLES_96_KHZ/80,    ///< 96 kHz 
    NB_SAMPLES_192_KHZ/160,   ///< 192 kHz 
    0, 
    0, 
    0, 
    0, 
    0, 
    0, 
    0, 
    0, 
  },
  // HDMV (BD)
  { 
    NB_SAMPLES_48_KHZ/240,    ///< 48 kHz 
    NB_SAMPLES_96_KHZ/480,    ///< 96 kHz 
    NB_SAMPLES_192_KHZ/960,   ///< 192 kHz 
    0, 
    0, 
    0, 
    0, 
    0, 
    0, 
    0, 
    0 
  },

  // SPDIFIN
  {
      /// Each Spdifin Frame is made of NB_SAMPLES_SPDIFIN : don't glob several frames in SPDIFIN.
      1, // 48k
      1, // 96k
      1, // 192k
      0, // Not Defined... 
      
      1, // 32k
      1, // 16k
      1, // 22k
      1, // 24k
      
      1, // 44k
      1, // 88k
      1, // 176k
      
  }
};


// this table gives the presentation duration of one audio frame for each lpcm 
// type vs frequency
#define PTS_TIME_UNIT 11

const static unsigned int LpcmPresentationTime[TypeLpcmSPDIFIN + 1][LpcmSamplingFreqNone] = 
{
  {  1667/PTS_TIME_UNIT, 1667/PTS_TIME_UNIT, 0,                  0, 0, 0, 0, 0, 0,                 0,                 0   }, // DVD Video
  {  833/PTS_TIME_UNIT,  833/PTS_TIME_UNIT,  833/PTS_TIME_UNIT,  0, 0, 0, 0, 0, 907/PTS_TIME_UNIT, 907/PTS_TIME_UNIT, 907/PTS_TIME_UNIT }, // DVD Audio
  {  1667/PTS_TIME_UNIT, 1667/PTS_TIME_UNIT, 1667/PTS_TIME_UNIT, 0, 0, 0, 0, 0, 0,                 0,                 0   }, // HD-DVD
  {  0,                  0,                  0,                  0, 0, 0, 0, 0, 0,                 0,                 0   }, // BD
  {  0,                  0,                  0,                  0, 0, 0, 0, 0, 0,                 0,                 0   }, // SpdifIn
};

////////////////////////////////////////////////////////////////////////////
///
/// Initialize the class by resetting it.
///
/// During a constructor calls to virtual methods resolve to the current class (because
/// the vtable is still for the class being constructed). This means we need to call
/// ::Reset again because the calls made by the sub-constructors will not have called
/// our reset method.
///
Collator_PesAudioLpcm_c::Collator_PesAudioLpcm_c( LpcmStreamType_t GivenStreamType )
{
  if( InitializationStatus != CollatorNoError )
    return;

  StreamType = GivenStreamType;
  
  Collator_PesAudioLpcm_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Search for the LPCM audio synchonrization word and, if found, report its offset.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioLpcm_c::FindNextSyncWord( int *CodeOffset )
{
  
  if ( IsPesPrivateDataAreaValid )
    {
      // There is no sync on lpcm, so we are always sync'ed!....
      *CodeOffset = 0;
      return CollatorNoError;
    }
  else
    {
      /// ... if the pda was looking good.
      return CollatorError;
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Determine the new state of the collator according to the incoming sub frame
/// Also returns this sub frame length
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioLpcm_c::DecideCollatorNextStateAndGetLength( unsigned int *FrameLength )
{
  CollatorStatus_t Status = CollatorNoError;

  //
  COLLATOR_DEBUG(">><<\n");
  
  if ( IsFirstPacket )
    {
      //case of very first packet, skip the packet portion before the FirstAccessUnitOffsetPointer
      CollatorState = SkipSubFrame;

      IsFirstPacket = false;

      // for the very first packet of dvd-audio, the stuffing bytes are not part of the PDA
      // since Configuration.ExtendedHeaderLength is not the same as PrivateHeaderLength,
      // so skip these bytes in this case
      
      *FrameLength = PesPrivateToSkip +
	             NextParsedFrameHeader.FirstAccessUnitPointer + 
	             FirstAccessUnitOffset[StreamType] -
	             NextParsedFrameHeader.PrivateHeaderLength;
      PesPrivateToSkip = 0;

      COLLATOR_TRACE("First packet: Skipping %d bytes\n", *FrameLength);

      return (Status);
    }

  // accumulate the private data area for the frame parser, 
  // if some major parameter inside have changed...

  if ( AccumulatePrivateDataArea )
    {
      // save the location of the private data area, to update it later...
      Status = AccumulateData( Configuration.ExtendedHeaderLength, &NewPesPrivateDataArea[0] );
      if ( Status != CollatorNoError )
	{
	  return (Status);
	}
      AccumulatePrivateDataArea = false;
      COLLATOR_DEBUG("Accumulate PDA of length %d\n", Configuration.ExtendedHeaderLength);
    }

  if ( !IsPesPrivateDataAreaValid )
    {
      //case of wrong packet passed to collator, skip the whole packet
      CollatorState = SkipSubFrame;
      *FrameLength = RemainingElementaryLength;
    }
  else if ( PesPrivateToSkip > 0 )
    {
      CollatorState = SkipSubFrame;
      *FrameLength = min(PesPrivateToSkip, RemainingElementaryLength);
      PesPrivateToSkip -= *FrameLength;
      COLLATOR_DEBUG("Skipping %d bytes of pda\n", *FrameLength);
    }
  else if ( RemainingDataLength > 0 )
    {
      CollatorState = ReadSubFrame;
      *FrameLength = min(RemainingDataLength, RemainingElementaryLength);
      RemainingDataLength -= *FrameLength;
      COLLATOR_DEBUG("Reading %d bytes (rest of frame)\n", *FrameLength);
    }
  else if ( (AccumulatedFrameNumber >= NbAudioFramesToGlob[StreamType][NextParsedFrameHeader.SamplingFrequency1]) ||
       IsPesPrivateDataAreaNew )
    {
      // flush the frame if we have already more than x accumulated frames
      // or if some pda key parameters are new
      CollatorState = GotCompleteFrame;
      COLLATOR_DEBUG("Flush after %d audio frames\n", AccumulatedFrameNumber);

      AccumulatedFrameNumber = 0;
      // prevent accumulating anything for this frame, the next thing we need to do
      // is accumlate the private data area for the frame parser but we can't do that
      // until we've cleared this frame from the accumulation buffer.
      *FrameLength = 0;
      // at next call of this function accumulate the pda
      AccumulatePrivateDataArea = true;
      // reset
      IsPesPrivateDataAreaNew = false;

      PlaybackTime += (GlobbedFramesOfNewPacket * LpcmPresentationTime[StreamType][NextParsedFrameHeader.SamplingFrequency1]);

      COLLATOR_DEBUG("PlaybackTime: %x\n", PlaybackTime);
    }
  else
    {
      // normal case: accumulate the audio frames
      CollatorState = ReadSubFrame;
      AccumulatedFrameNumber += 1;
      GlobbedFramesOfNewPacket += 1;
      
      COLLATOR_ASSERT(0 == RemainingDataLength);
      COLLATOR_ASSERT(0 == PesPrivateToSkip);

      // DVD-audio allows stuffing bytes to form part of the private data area (and these can be different
      // for each packet). We can't easily handle this in the PES layer since the length can't be predicted
      // and therefore we cannot set Configuration.ExtendedHeaderLength until it is too late. Instead
      // Collator_PesAudioLpcm_c::HandlePesPrivateData() records how many bytes we must skip
      // (PesPrivateToSkip) and leaves it to the state change logic to skip that data. This means that we
      // must ensure that this method will be called at the right point to skip data. We do this be making
      // sure we don't ever return a *FrameLength larger than the RemainingElementaryData .
      if ((RemainingElementaryLength < NextParsedFrameHeader.AudioFrameSize))
	{
	  RemainingDataLength = NextParsedFrameHeader.AudioFrameSize - RemainingElementaryLength;
	  *FrameLength        = RemainingElementaryLength;
	}
      else
	{
	  // read the whole frame!
	  *FrameLength        = NextParsedFrameHeader.AudioFrameSize;
	}

      COLLATOR_DEBUG("Read frame of size %d (total frames in this chunk: %d)\n", 
		     *FrameLength, 
		     AccumulatedFrameNumber);
    }

 return Status;
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
/// at the predicted location then Collator_PesAudioLpcm_c::FindNextSyncWord()
/// will clear Collator_PesAudio_c::PassPesPrivateDataToElementaryStreamHandler
/// and we will progress as though we had DVD style PES encapsulation.
///
/// Should we ever loose sync then we will automatically switch back to
/// broadcast mode.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioLpcm_c::HandlePesPrivateData(unsigned char *PesPrivateData)
{

  FrameParserStatus_t  FPStatus;

  COLLATOR_DEBUG(">><<\n");

  if (StreamId != Configuration.StreamIdentifierCode)
    {
      IsPesPrivateDataAreaValid = false;
      return (CollatorError);
    }

  NextParsedFrameHeader.Type = StreamType;

  FPStatus = FrameParser_AudioLpcm_c::ParseFrameHeader( PesPrivateData,
							&NextParsedFrameHeader,
							PesPayloadLength + Configuration.ExtendedHeaderLength );

  if (FPStatus != FrameParserNoError)
    {
      IsPesPrivateDataAreaValid = false;
      return (CollatorError);
    }
  else
    {
      IsPesPrivateDataAreaValid = true;
      // no need to pass this private data area any more to elementary stream handler,
      PassPesPrivateDataToElementaryStreamHandler = false;
      GlobbedFramesOfNewPacket = 0;

      // inform the state change logic how many stuffing bytes it needs to skip
      PesPrivateToSkip = NextParsedFrameHeader.PrivateHeaderLength - Configuration.ExtendedHeaderLength;

      if (IsFirstPacket)
	{
	  // special case of very first packet
	  AccumulatePrivateDataArea = true;
	  IsPesPrivateDataAreaNew = false;
	  memcpy( &NewPesPrivateDataArea[0], PesPrivateData, Configuration.ExtendedHeaderLength );
	}
      else 
	{
	  if ( (ParsedFrameHeader.SubStreamId != NextParsedFrameHeader.SubStreamId) ||
	       (ParsedFrameHeader.WordSize1 != NextParsedFrameHeader.WordSize1) || 
	       (ParsedFrameHeader.EmphasisFlag != NextParsedFrameHeader.EmphasisFlag) || 
	       (ParsedFrameHeader.MuteFlag != NextParsedFrameHeader.MuteFlag) || 
	       (ParsedFrameHeader.NumberOfChannels != NextParsedFrameHeader.NumberOfChannels) || 
	       (ParsedFrameHeader.SamplingFrequency1 != NextParsedFrameHeader.SamplingFrequency1))
	    {
	      // check for any parameter change in the private data area compared to last pda
	      // save this new pda to pass it later to the frame parser
	      IsPesPrivateDataAreaNew = true;
	      memcpy( &NewPesPrivateDataArea[0], PesPrivateData, Configuration.ExtendedHeaderLength );
	    }
	  else
	    {
	      IsPesPrivateDataAreaNew = false;
	    }
	}

      // do a few consistency check on access unit pointer...
      if ((GuessedNextFirstAccessUnit != 0) && 
	  (GuessedNextFirstAccessUnit != NextParsedFrameHeader.FirstAccessUnitPointer))
	{
	  COLLATOR_DEBUG("Uncorrect FirstAccessUnitPointer (%d. vs expected %d)\n", NextParsedFrameHeader.FirstAccessUnitPointer, GuessedNextFirstAccessUnit);
	  GuessedNextFirstAccessUnit = 0;
	}
      
      // and guess next access unit pointer
      if ((StreamType != TypeLpcmDVDBD) && (StreamType != TypeLpcmSPDIFIN))
	{
	  GuessedNextFirstAccessUnit = NextParsedFrameHeader.Length + NextParsedFrameHeader.FirstAccessUnitPointer - PesPayloadLength;
	}

      // save back parsed packet info
      memcpy(&ParsedFrameHeader, &NextParsedFrameHeader, sizeof(LpcmAudioParsedFrameHeader_t));

      return (CollatorNoError);
    }

  //
}

////////////////////////////////////////////////////////////////////////////
///
/// Reset the LPCM addition elementary stream state.
///
void Collator_PesAudioLpcm_c::ResetCollatorStateAfterForcedFrameFlush()
{
    Collator_PesAudio_c::ResetCollatorStateAfterForcedFrameFlush();
    
    GuessedNextFirstAccessUnit               = 0;
    AccumulatedFrameNumber                   = 0;
    IsPesPrivateDataAreaNew                  = false;
    AccumulatePrivateDataArea                = false;
    IsPesPrivateDataAreaValid                = false;
    IsFirstPacket                            = true;
    RemainingDataLength                      = 0;
    PesPrivateToSkip                         = 0;
    GlobbedFramesOfNewPacket                 = 0;

    memset ( &ParsedFrameHeader, 0 , sizeof(LpcmAudioParsedFrameHeader_t) );
    memset ( &NextParsedFrameHeader, 0 , sizeof(LpcmAudioParsedFrameHeader_t) );
}

////////////////////////////////////////////////////////////////////////////
///
/// Sets the extended pes header length according to the stream_id field of the pes header
///
/// \return void
///
void  Collator_PesAudioLpcm_c::SetPesPrivateDataLength(unsigned char SpecificCode)
{
  /* length of PDA has already been given, but store the Stream Id to make further checks... */
  StreamId = SpecificCode;
  //  Configuration.ExtendedHeaderLength = AudioPesPrivateDataLength[StreamType];
}

CollatorStatus_t Collator_PesAudioLpcm_c::Reset( void )
{
  CollatorStatus_t Status;
	
  //
	
  COLLATOR_DEBUG(">><<\n");

  Status = Collator_PesAudio_c::Reset();
  if( Status != CollatorNoError )
    return Status;
    
  // FrameHeaderLength belongs to Collator_PesAudio_c so we must set it after the class has been reset    
  FrameHeaderLength = LPCM_FRAME_HEADER_SIZE;

  Configuration.StreamIdentifierMask       = 0xff;
  Configuration.StreamIdentifierCode       = 0xbd; // lpcm packets always have a stream_id equal to 0xbd
  Configuration.BlockTerminateMask         = 0xff;
  Configuration.BlockTerminateCode         = 0x00;
  Configuration.IgnoreCodesRangeStart      = 0x01; // All slice codes
  Configuration.IgnoreCodesRangeEnd        = 0xbd - 1;
  Configuration.InsertFrameTerminateCode   = false;
  Configuration.TerminalCode               = 0;
  Configuration.ExtendedHeaderLength       = AudioPesPrivateDataLength[StreamType];
  Configuration.DeferredTerminateFlag      = false;

  ResetCollatorStateAfterForcedFrameFlush();

  return CollatorNoError;
}
