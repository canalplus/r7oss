/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesAudioDra_c
///
/// Implements DRA audio sync word scanning and frame length analysis.
///

#include "frame_parser_audio_dra.h"
#include "dra_audio.h"
#include "collator_pes_audio_dra.h"

////////////////////////////////////////////////////////////////////////////
///
/// Initialize the class by resetting it.
///
///
Collator_PesAudioDra_c::Collator_PesAudioDra_c(void)
    : Collator_PesAudio_c(DRA_AUDIO_FRAME_HEADER_SIZE)
{
    Configuration.StreamIdentifierMask             = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode             = PES_START_CODE_PRIVATE_STREAM_1;
    Configuration.BlockTerminateMask               = 0xff;         // Picture
    Configuration.BlockTerminateCode               = 0x00;
    Configuration.IgnoreCodesRanges.NbEntries      = 1;
    Configuration.IgnoreCodesRanges.Table[0].Start = 0x01; // All slice codes
    Configuration.IgnoreCodesRanges.Table[0].End   = PES_START_CODE_PRIVATE_STREAM_1 - 1;
    Configuration.InsertFrameTerminateCode         = false;
    Configuration.TerminalCode                     = 0;
    Configuration.ExtendedHeaderLength             = 0;
    Configuration.DeferredTerminateFlag            = false;
}

////////////////////////////////////////////////////////////////////////////
///
/// Search for the DRA audio synchonrization word and, if found, report its offset.
///
/// The DRA audio start code is 0x7fff. We must search for 15 consecutive
/// set bits (starting on a byte boundary).
///
/// Weak start codes are, in fact, the primary reason we have
/// to verify the header of the subsequent frame before emitting the preceding one.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioDra_c::FindNextSyncWord(int *CodeOffset)
{
    unsigned int i;

    // check the last byte of any previous blocks
    if (PotentialFrameHeaderLength)
    {
        if (((PotentialFrameHeader[PotentialFrameHeaderLength - 1] & DRA_START_SYNC_CODE_BYTE0_MASK) == DRA_START_SYNC_CODE_BYTE0) &&
            RemainingElementaryData[0]  == DRA_START_SYNC_CODE_BYTE1)
        {
            *CodeOffset = -1;
            return CollatorNoError;
        }
    }

    // do the most naive possible search. there is no obvious need for performance here
    for (i = 0; i < RemainingElementaryLength - 1; i++)
    {
        if ((RemainingElementaryData[i] & DRA_START_SYNC_CODE_BYTE0_MASK) == DRA_START_SYNC_CODE_BYTE0 &&
            (RemainingElementaryData[i + 1] == DRA_START_SYNC_CODE_BYTE1))
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
CollatorStatus_t Collator_PesAudioDra_c::DecideCollatorNextStateAndGetLength(unsigned int *FrameLength)
{
    FrameParserStatus_t FPStatus;
    CollatorStatus_t Status;
    DraAudioParsedFrameHeader_t ParsedFrameHeader;
    FPStatus = FrameParser_AudioDra_c::ParseFrameHeader(StoredFrameHeader,
                                                        &ParsedFrameHeader);

    if (FPStatus == FrameParserNoError)
    {
        *FrameLength  =  ParsedFrameHeader.Audio_FrameSize;
        CollatorState = (CollatorState == SeekingFrameEnd) ? GotCompleteFrame : ReadSubFrame;
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
void  Collator_PesAudioDra_c::SetPesPrivateDataLength(unsigned char SpecificCode)
{
    /* do nothing, configuration already set to the right value... */
}

