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
/// \class Collator_PesAudioAac_c
///
/// Implements AAC audio sync word scanning and frame length analysis.
///

#include "frame_parser_audio_aac.h"
#include "aac_audio.h"
#include "collator_pes_audio_aac.h"

#define AAC_HEADER_SIZE 14

////////////////////////////////////////////////////////////////////////////
///
/// Initialize the class by resetting it.
///
Collator_PesAudioAac_c::Collator_PesAudioAac_c(void)
// In stream based, decoding parsing is not done at SE level so set FrameHeaderLength = 0
    : Collator_PesAudio_c((BaseComponentClass_c::EnableCoprocessorParsing == 0) ? AAC_HEADER_SIZE : 0)
    , StreamBased((BaseComponentClass_c::EnableCoprocessorParsing == 0) ? false : true)
    , FormatType(AAC_RESERVED_FORMAT)
{
    Configuration.StreamIdentifierMask             = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode             = PES_START_CODE_AUDIO;
    Configuration.BlockTerminateMask               = 0xff;         // Picture
    Configuration.BlockTerminateCode               = 0x00;
    Configuration.IgnoreCodesRanges.NbEntries      = 1;
    Configuration.IgnoreCodesRanges.Table[0].Start = 0x01; // All slice codes
    Configuration.IgnoreCodesRanges.Table[0].End   = PES_START_CODE_AUDIO - 1;
    Configuration.InsertFrameTerminateCode         = false;
    Configuration.TerminalCode                     = 0;
    Configuration.ExtendedHeaderLength             = 0;
    Configuration.DeferredTerminateFlag            = false;
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
/// to verify the header of the subsequent frame before emitting the preceding one.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioAac_c::FindNextSyncWord(int *CodeOffset)
{
    if (StreamBased)
    {
        *CodeOffset = 0; // No parsing required in stream base decoding
        return CollatorNoError;
    }

    int i;
    unsigned char aacHeader[AAC_HEADER_SIZE];
    AacAudioParsedFrameHeader_t ParsedFrameHeader;
    int RemainingInPotential = PotentialFrameHeaderLength;
    unsigned char *PotentialFramePtr = PotentialFrameHeader;
    unsigned char *ElementaryPtr;

    // do the most naive possible search. there is no obvious need for performance here
    for (i = 0; i <= (int)(RemainingElementaryLength + PotentialFrameHeaderLength - AAC_HEADER_SIZE); i++)
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

        FrameParserStatus_t FPStatus = FrameParser_AudioAac_c::ParseFrameHeader(ElementaryPtr,
                                                                                &ParsedFrameHeader,
                                                                                AAC_HEADER_SIZE,
                                                                                AAC_GET_SYNCHRO);

        if (FPStatus == FrameParserNoError)
        {
            // it seems like we got a synchonization...
            *CodeOffset = (RemainingInPotential > 0) ? (-RemainingInPotential) : (i - PotentialFrameHeaderLength);;
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
CollatorStatus_t Collator_PesAudioAac_c::DecideCollatorNextStateAndGetLength(unsigned int *FrameLength)
{
    if (StreamBased)
    {
        // In stream base no parsing needed make FrameLength as PesPayloadLength
        *FrameLength = PesPayloadLength;
        CollatorState = GotCompleteFrame;
        return CollatorNoError;
    }

    FrameParserStatus_t FPStatus;
    CollatorStatus_t Status;
    AacAudioParsedFrameHeader_t ParsedFrameHeader;
    //
    FPStatus = FrameParser_AudioAac_c::ParseFrameHeader(StoredFrameHeader,
                                                        &ParsedFrameHeader,
                                                        FrameHeaderLength,
                                                        AAC_GET_LENGTH);

    if (FPStatus == FrameParserNoError)
    {
        if (FormatType == AAC_RESERVED_FORMAT)
        {
            FormatType = ParsedFrameHeader.Type;
        }
        else if (ParsedFrameHeader.Type != FormatType)
        {
            // this is a change of format type in the same stream, so we surely have synchronized on a wrong sync...
            FormatType = AAC_RESERVED_FORMAT;
            return CollatorError;
        }

        *FrameLength     = ParsedFrameHeader.Length;
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
void  Collator_PesAudioAac_c::SetPesPrivateDataLength(unsigned char SpecificCode)
{
    /* do nothing, configuration already set to the right value... */
}

