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

Source file name : collator_pes_audio_aac.cpp
Author :           Adam

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
05-Jul-07   Created (from collator_pes_audio_mpeg.cpp)      Adam

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesAudioAac_c
///
/// Implements AAC audio sync word scanning and frame length analysis.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_audio_aac.h"
#include "frame_parser_audio_aac.h"
#include "aac_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
#define AAC_HEADER_SIZE 14

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
Collator_PesAudioAac_c::Collator_PesAudioAac_c( void )
{
    if( InitializationStatus != CollatorNoError )
	return;

    Collator_PesAudioAac_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Search for the AAC audio synchonrization word and, if found, report its offset.
///
/// We auport 2 types of aac: 
/// ADTS (same sync as MPEG) and LOAS (2 sync types: 0x2b7 and 0x4de1)
/// see ISO/IEC 14496-3 for more details ($1.7.2) 
///
/// Weak start codes are, in fact, the primary reason we have
/// to verify the header of the subsequent frame before emitting the preceeding one.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioAac_c::FindNextSyncWord( int *CodeOffset )
{
    int i;
    unsigned char aacHeader[AAC_HEADER_SIZE];
    AacAudioParsedFrameHeader_t ParsedFrameHeader;
    int RemainingInPotential = PotentialFrameHeaderLength;
    unsigned char * PotentialFramePtr = PotentialFrameHeader;
    unsigned char * ElementaryPtr;

    // do the most naive possible search. there is no obvious need for performance here
    for( i=0; i<=(int)(RemainingElementaryLength + PotentialFrameHeaderLength - AAC_HEADER_SIZE); i++) 
    {
        if (RemainingInPotential > 0)
        {
            /* we need at least 16 bytes to verify a few items in the stream header...*/
            int size =  min(RemainingInPotential, AAC_HEADER_SIZE);
            memcpy(&aacHeader[0], PotentialFramePtr, size);
            memcpy(&aacHeader[size], &RemainingElementaryData[0], AAC_HEADER_SIZE - size);
            ElementaryPtr = aacHeader;
        }
        else
        {
            ElementaryPtr = &RemainingElementaryData[i - PotentialFrameHeaderLength];
        }
        
        FrameParserStatus_t FPStatus = FrameParser_AudioAac_c::ParseFrameHeader( ElementaryPtr,
                                                                                 &ParsedFrameHeader,
                                                                                 AAC_HEADER_SIZE,
                                                                                 AAC_GET_SYNCHRO );
        if( FPStatus == FrameParserNoError )
        {
            // it seems like we got a synchonization...
            *CodeOffset = (RemainingInPotential > 0)?(-RemainingInPotential):(i-PotentialFrameHeaderLength);;
            return CollatorNoError;
        }
        RemainingInPotential--;
        PotentialFramePtr++;
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
CollatorStatus_t Collator_PesAudioAac_c::DecideCollatorNextStateAndGetLength( unsigned int *FrameLength )
{
  FrameParserStatus_t FPStatus;
  CollatorStatus_t Status;
  AacAudioParsedFrameHeader_t ParsedFrameHeader;

  //

  FPStatus = FrameParser_AudioAac_c::ParseFrameHeader( StoredFrameHeader,
                                                       &ParsedFrameHeader, 
                                                       FrameHeaderLength,
                                                       AAC_GET_LENGTH );
  
  if( FPStatus == FrameParserNoError )
  {
      if (FormatType == AAC_AUDIO_UNDEFINED)
      {
          FormatType = ParsedFrameHeader.Type;
      }
      else if (ParsedFrameHeader.Type != FormatType)
      {
          // this is a change of format type in the same stream, so we surely have synchronized on a wrong sync...
          FormatType = AAC_AUDIO_UNDEFINED;
          return CollatorError;
      }

      *FrameLength     = ParsedFrameHeader.Length;
      
      CollatorState = (CollatorState == SeekingFrameEnd)?GotCompleteFrame:ReadSubFrame;
      
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
void  Collator_PesAudioAac_c::SetPesPrivateDataLength(unsigned char SpecificCode)
{
  /* do nothing, configuration already set to the right value... */
}


CollatorStatus_t Collator_PesAudioAac_c::Reset( void )
{
CollatorStatus_t Status;

//

    COLLATOR_DEBUG(">><<\n");

    Status = Collator_PesAudio_c::Reset();
    if( Status != CollatorNoError )
	return Status;

    // FrameHeaderLength belongs to Collator_PesAudio_c so we must set it after the class has been reset    
    FrameHeaderLength = AAC_HEADER_SIZE;

    Configuration.StreamIdentifierMask       = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode       = PES_START_CODE_AUDIO;
    Configuration.BlockTerminateMask         = 0xff;         // Picture
    Configuration.BlockTerminateCode         = 0x00;
    Configuration.IgnoreCodesRangeStart      = 0x01; // All slice codes
    Configuration.IgnoreCodesRangeEnd        = PES_START_CODE_AUDIO - 1;
    Configuration.InsertFrameTerminateCode   = false;
    Configuration.TerminalCode               = 0;
    Configuration.ExtendedHeaderLength       = 0;
    Configuration.DeferredTerminateFlag      = false;

    FormatType                               = AAC_AUDIO_UNDEFINED;

    return CollatorNoError;
}
