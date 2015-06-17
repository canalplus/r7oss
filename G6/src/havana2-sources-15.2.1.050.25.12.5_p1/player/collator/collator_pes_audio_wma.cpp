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

#include "frame_parser_audio_wma.h"
#include "wma_audio.h"
#include "collator_pes_audio_wma.h"

#define WMA_HEADER_SIZE 0

////////////////////////////////////////////////////////////////////////////
///
/// Initialize the class by resetting it.
///
Collator_PesAudioWma_c::Collator_PesAudioWma_c(void)
    : Collator_PesAudio_c(WMA_HEADER_SIZE)
    , WMADataBlockSize(0)
{
    Configuration.StreamIdentifierMask             = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode             = PES_START_CODE_AUDIO;
    Configuration.BlockTerminateMask               = 0xff;         // Picture
    Configuration.BlockTerminateCode               = 0x00;
    Configuration.IgnoreCodesRanges.NbEntries      = 1;
    Configuration.IgnoreCodesRanges.Table[0].Start = 0x01; // All slice codes
    Configuration.IgnoreCodesRanges.Table[0].End   = PES_START_CODE_PRIVATE_STREAM_1 - 1;
    Configuration.InsertFrameTerminateCode         = false;
    Configuration.TerminalCode                     = 0;
    Configuration.ExtendedHeaderLength             = 0;       //private data area after pes header
    Configuration.DeferredTerminateFlag            = false;
    //Configuration.DeferredTerminateCode[0]        =  Configuration.BlockTerminateCode;
    //Configuration.DeferredTerminateCode[1]        =  Configuration.BlockTerminateCode;

    PassPesPrivateDataToElementaryStreamHandler = false;
}

////////////////////////////////////////////////////////////////////////////
///
/// Search for the WMA audio synchonrization word and, if found, report its offset.
///
/// No sync word, just fine to carry on as is.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioWma_c::FindNextSyncWord(int *CodeOffset)
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
CollatorStatus_t Collator_PesAudioWma_c::DecideCollatorNextStateAndGetLength(unsigned int *FrameLength)
{
    // handle the easy case first to keep the rest of the logic clean
    if (CollatorState == GotSynchronized)
    {
        *FrameLength = RemainingElementaryLength;
        // as we need to collect (*FrameLength) bytes, move to ReadSubFrame state
        CollatorState = ReadSubFrame;
        return CollatorNoError;
    }

    // if we've just accumulated a stream properties object then extract the block size
    if (0 == memcmp(BufferBase, asf_guid_lookup[ASF_GUID_STREAM_PROPERTIES_OBJECT], sizeof(asf_guid_t)))
    {
        WmaAudioStreamParameters_t StreamParameters;
        FrameParserStatus_t Status;
        Status = FrameParser_AudioWma_c::ParseStreamHeader(BufferBase, &StreamParameters, false);

        if (Status == FrameParserNoError)
        {
            WMADataBlockSize = StreamParameters.BlockAlignment;
            SE_INFO(group_collator_audio, "Found Stream Properties Object - WMADataBlockSize %d\n", WMADataBlockSize);
        }
        else
        {
            SE_ERROR("Found GUID for Stream Properties Object but parser reported error\n");
        }
    }

    // check if the block we are *about* to accumulate is a stream properties object
    if ((RemainingElementaryLength > sizeof(asf_guid_t)) &&
        (0 == memcmp(RemainingElementaryData,
                     asf_guid_lookup[ASF_GUID_STREAM_PROPERTIES_OBJECT], sizeof(asf_guid_t))))
    {
        SE_INFO(group_collator_audio, "Anticipating a Stream Properties Object - Clearing WMADataBlockSize\n");
        WMADataBlockSize = 0;
    }

    // if we are handling normal data (WMADataBlockSize is non-zero)
    // we must check to see if we need to shrink the blocks to avoid
    // confusing the firmware. we only shrink the block if its size is
    // an integer multiple of the block alignment. this prevents us
    // from becoming unaligned on incoming writes and missing a stream
    // properties object (noting that the above code only scans the
    // first sixteen bytes).
    if ((0 != WMADataBlockSize) && (0 == (RemainingElementaryLength % WMADataBlockSize)))
    {
        *FrameLength = WMADataBlockSize;
    }
    else
    {
        *FrameLength = RemainingElementaryLength;
    }

    SE_DEBUG(group_collator_audio, "WMADataBlockSize %4d  RemainingElementaryLength %4d  FrameLength %4d\n",
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


