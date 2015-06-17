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

Source file name : collator_pes_audio_wma.cpp
Author :           Adam

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
11-Sep-07   Created                                         Adam

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesAudioWma_c
///
/// Implements WMA audio sync word scanning and frame length analysis.
/// Actually it doesn't because you can't do that with WMA!
///
/// The userspace will either send, in a single write, whole PES packets
/// containing either a single 'Stream Properties Object' or a single ASF
/// packet. Ordinarily an ASF packet contains exactly one WMA Data Block
/// but this is not always the case so we are forced to parse the
/// stream properties object to determine the block size the firmware
/// expects data to be delivered in.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#define ENABLE_COLLATOR_DEBUG 0

#include "collator_pes_audio_wma.h"
#include "frame_parser_audio_wma.h"
#include "wma_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
#define WMA_HEADER_SIZE 0

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
Collator_PesAudioWma_c::Collator_PesAudioWma_c( void )
{
    if( InitializationStatus != CollatorNoError )
	return;

    Collator_PesAudioWma_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Search for the WMA audio synchonrization word and, if found, report its offset.
///
/// No sync word, just fine to carry on as is.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioWma_c::FindNextSyncWord( int *CodeOffset )
{
    *CodeOffset = 0;
    return CollatorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// As far as I understand it thus far, we just want to hand the entire
/// pes data on to be processed by the frame_parser
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioWma_c::DecideCollatorNextStateAndGetLength( unsigned int *FrameLength )
{
    // handle the easy case first to keep the rest of the logic clean
    if (CollatorState == GotSynchronized) {
	*FrameLength = RemainingElementaryLength;
	CollatorState = SeekingFrameEnd;
	return CollatorNoError;
    }

    // if we've just accumulated a stream properties object then extract the block size
    if( 0 == memcmp(BufferBase, asf_guid_lookup[ASF_GUID_STREAM_PROPERTIES_OBJECT], sizeof(asf_guid_t) ) ) {
        WmaAudioStreamParameters_t StreamParameters;
	FrameParserStatus_t Status;

	Status = FrameParser_AudioWma_c::ParseStreamHeader(BufferBase, &StreamParameters, false);
	if (Status == FrameParserNoError) {
	    WMADataBlockSize = StreamParameters.BlockAlignment;
	    COLLATOR_TRACE( "Found Stream Properties Object - WMADataBlockSize %d\n", WMADataBlockSize);
        } else {
	    COLLATOR_ERROR( "Found GUID for Stream Properties Object but parser reported error\n");
	}
    }

    // check if the block we are *about* to accumulate is a stream properties object
    if( ( RemainingElementaryLength > sizeof(asf_guid_t) ) &&
	( 0 == memcmp(RemainingElementaryData,
		      asf_guid_lookup[ASF_GUID_STREAM_PROPERTIES_OBJECT], sizeof(asf_guid_t) ) ) ) {
        COLLATOR_TRACE( "Anticipating a Stream Properties Object - Clearing WMADataBlockSize\n" );
	WMADataBlockSize = 0;
    }
    
    // if we are handling normal data (WMADataBlockSize is non-zero)
    // we must check to see if we need to shrink the blocks to avoid
    // confusing the firmware. we only shrink the block if its size is
    // an integer multiple of the block alignment. this prevents us
    // from becoming unaligned on incoming writes and missing a stream
    // properties object (noting that the above code only scans the
    // first sixteen bytes).
    if ( ( 0 != WMADataBlockSize ) && ( 0 == ( RemainingElementaryLength % WMADataBlockSize ) ) )
	*FrameLength = WMADataBlockSize;
    else
	*FrameLength = RemainingElementaryLength;
    COLLATOR_DEBUG("WMADataBlockSize %4d  RemainingElementaryLength %4d  FrameLength %4d\n",
		   WMADataBlockSize, RemainingElementaryLength, *FrameLength);

    CollatorState = GotCompleteFrame;
    return CollatorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Sets the extended pes header length according to the stream_id field of the pes header
///
/// \return void
///
void  Collator_PesAudioWma_c::SetPesPrivateDataLength(unsigned char SpecificCode)
{
  /* do nothing, configuration already set to the right value... */
}

////////////////////////////////////////////////////////////////////////////
///
/// Examine the PES private data header.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioWma_c::HandlePesPrivateData(unsigned char *PesPrivateData)
{
  /* do nothing, configuration already set to the right value... */
    return CollatorNoError;
}

CollatorStatus_t Collator_PesAudioWma_c::Reset( void )
{
    CollatorStatus_t Status;

    COLLATOR_DEBUG(">><<\n");

    Status = Collator_PesAudio_c::Reset();
    if( Status != CollatorNoError )
	return Status;

    // FrameHeaderLength belongs to Collator_PesAudio_c so we must set it after the class has been reset    
    FrameHeaderLength = WMA_HEADER_SIZE;

    Configuration.StreamIdentifierMask       = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode       = PES_START_CODE_AUDIO;
    Configuration.BlockTerminateMask         = 0xff;         // Picture
    Configuration.BlockTerminateCode         = 0x00;
    Configuration.IgnoreCodesRangeStart      = 0x01; // All slice codes
    Configuration.IgnoreCodesRangeEnd        = PES_START_CODE_PRIVATE_STREAM_1 - 1;
    Configuration.InsertFrameTerminateCode   = false;
    Configuration.TerminalCode               = 0;
    Configuration.ExtendedHeaderLength       = 0;       //private data area after pes header
    Configuration.DeferredTerminateFlag      = false;
    //Configuration.DeferredTerminateCode[0]   =  Configuration.BlockTerminateCode;
    //Configuration.DeferredTerminateCode[1]   =  Configuration.BlockTerminateCode;

    PassPesPrivateDataToElementaryStreamHandler = false;

    WMADataBlockSize = 0;

    return CollatorNoError;
}
