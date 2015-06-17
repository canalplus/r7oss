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

Source file name : collator_pes_audio_avs.cpp
Author :           Daniel

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
19-Apr-07   Created                                         Daniel

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesAudioAvs_c
///
/// Implements AVS audio sync word scanning and frame length analysis.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_audio_avs.h"
#include "frame_parser_audio_avs.h"
#include "avs_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
#define AVS_HEADER_SIZE 5

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
Collator_PesAudioAvs_c::Collator_PesAudioAvs_c( void )
{
    if( InitializationStatus != CollatorNoError )
        return;

    Collator_PesAudioAvs_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Search for the AVS audio synchonrization word and, if found, report its offset.
///
/// The AVS audio start code is supprisingly weak. We must search for 14 consecutive
/// set bits (starting on a byte boundary).
///
/// Weak start codes are, in fact, the primary reason we have
/// to verify the header of the subsequent frame before emitting the preceeding one.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioAvs_c::FindNextSyncWord( int *CodeOffset )
{
unsigned int i;

    // check the last byte of any previous blocks
    if( PotentialFrameHeaderLength )
    {
        if( PotentialFrameHeader[PotentialFrameHeaderLength - 1] == 0xff &&
            (RemainingElementaryData[0] & 0xe0) == 0xe0 )
        {
            *CodeOffset = -1;
            return CollatorNoError;

        }
    }

    // do the most naive possible search. there is no obvious need for performance here
    for( i=0; i<RemainingElementaryLength-1; i++) {
        if( RemainingElementaryData[i] == 0xff &&
            (RemainingElementaryData[i+1] & 0xe0) == 0xe0 )
        {
            *CodeOffset = i;
            return CollatorNoError;
        }
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
CollatorStatus_t Collator_PesAudioAvs_c::DecideCollatorNextStateAndGetLength( unsigned int *FrameLength )
{
  FrameParserStatus_t FPStatus;
  CollatorStatus_t Status;
  unsigned int ExtensionLength;
  AvsAudioParsedFrameHeader_t ParsedFrameHeader;

  //
  // Check to see if the frame has a valid extension header
  //

  if( CollatorState == SeekingFrameEnd )
  {
      FPStatus = FrameParser_AudioAvs_c::ParseExtensionHeader( StoredFrameHeader, &ExtensionLength );

      if (FPStatus == FrameParserNoError )
      {
          *FrameLength = ExtensionLength;

          CollatorState = ReadSubFrame;

          return CollatorNoError;
      }
  }

  //
  // Having handled extension headers we can handle headers.
  //

  FPStatus = FrameParser_AudioAvs_c::ParseFrameHeader( StoredFrameHeader,
                                                        &ParsedFrameHeader );

  if( FPStatus == FrameParserNoError )
    {
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
void  Collator_PesAudioAvs_c::SetPesPrivateDataLength(unsigned char SpecificCode)
{
  /* do nothing, configuration already set to the right value... */
}


CollatorStatus_t Collator_PesAudioAvs_c::Reset( void )
{
CollatorStatus_t Status;

//

    COLLATOR_DEBUG(">><<\n");

    Status = Collator_PesAudio_c::Reset();
    if( Status != CollatorNoError )
        return Status;

    // FrameHeaderLength belongs to Collator_PesAudio_c so we must set it after the class has been reset    
    FrameHeaderLength = AVS_HEADER_SIZE;

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

    return CollatorNoError;
}
