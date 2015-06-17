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
/// \class Collator_PesAudioAdpcm_c
///
/// Implements ADPCM audio data scanning and frame length analysis.
///

#include "frame_parser_audio_adpcm.h"
#include "collator_pes_audio_adpcm.h"

////////////////////////////////////////////////////////////////////////////
///
/// Initialize the class by resetting it.
///
Collator_PesAudioAdpcm_c::Collator_PesAudioAdpcm_c(AdpcmStreamType_t GivenStreamType)
    : Collator_PesAudio_c(0) // FrameHeaderLength
    , StreamType(GivenStreamType)
    , NextParsedFrameHeader()
    , AccumulatePrivateDataArea(true)
    , PesPrivateDataArea()
    , RemainingAudioDataLength(0)
{
    Configuration.StreamIdentifierMask             = 0xff;
    Configuration.StreamIdentifierCode             = 0xbd;
    Configuration.BlockTerminateMask               = 0xff;
    Configuration.BlockTerminateCode               = 0x00;
    Configuration.IgnoreCodesRanges.NbEntries      = 1;
    Configuration.IgnoreCodesRanges.Table[0].Start = 0x01; // All slice codes
    Configuration.IgnoreCodesRanges.Table[0].End   = 0xbd - 1;
    Configuration.InsertFrameTerminateCode         = false;
    Configuration.TerminalCode                     = 0;
    Configuration.ExtendedHeaderLength             = ADPCM_PRIVATE_DATE_LENGTH;
    Configuration.DeferredTerminateFlag            = false;

    ResetCollatorStateAfterForcedFrameFlush();
}

///
void Collator_PesAudioAdpcm_c::ResetCollatorStateAfterForcedFrameFlush()
{
    Collator_PesAudio_c::ResetCollatorStateAfterForcedFrameFlush();
    AccumulatePrivateDataArea  = true;
    RemainingAudioDataLength   = 0;
}

////////////////////////////////////////////////////////////////////////////
///
/// There is no sync word for ADPCM packet so we always set *CodeOffset to 0
/// and return a CollatorNoError status.
///
/// \return Collator status code, always CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioAdpcm_c::FindNextSyncWord(int *CodeOffset)
{
    *CodeOffset = 0;
    return CollatorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// This function determines the new state of the collator and also returns
/// the number of bytes to accumulate in function of the remaining bytes of
/// the elementary stream (RemainingElementaryLength).
///
/// It will set the collator state to GotCompleteFrame if we have so far
/// accumulated a PES private area and a right number of ADPCM blocks.
/// as determined previously in the adpcm frame parser.
///
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioAdpcm_c::DecideCollatorNextStateAndGetLength(unsigned int *FrameLength)
{
    // Debug message
    SE_DEBUG(group_collator_audio, ">><<\n");
    // Local variables.
    // - nbOfBytesToHaveBeforeFlushing: this is the number of bytes to accumulate before
    //   flushing the buffer for the frame parser. That is to say the private data (the
    //   size of which being Configuration.ExtendedHeaderLength) and the audio data (the size of
    //   which being "the audio frame size" * "number of access unit".
    // - Status: the return status
    unsigned int nbOfBytesToHaveBeforeFlushing =
        Configuration.ExtendedHeaderLength
        + NextParsedFrameHeader.AdpcmBlockSize * NextParsedFrameHeader.NbOfBlockPerBufferToDecode;
    CollatorStatus_t Status = CollatorNoError;

    // Accumulate the private data if required (ie if AccumulatePrivateDataArea is true).
    // It is required when we have to fill out a new buffer. That is to say:
    // - when we get here for the very first time
    // - when we have just flushed a complete buffer for the parser
    // Note that the private data has been saved previously in PesPrivateDataArea.
    //
    // After having accumulated the private data we set AccumulatePrivateDataArea to
    // false. It will be set to true below when a complete buffer has been accumulated.
    if (AccumulatePrivateDataArea)
    {
        Status = AccumulateData(Configuration.ExtendedHeaderLength, &PesPrivateDataArea[0]);

        if (Status == CollatorNoError)
        {
            AccumulatePrivateDataArea = false;
            SE_DEBUG(group_collator_audio, "Accumulate private data of length %d\n", Configuration.ExtendedHeaderLength);
        }
    }

    // Continue only if there is no error at this point.
    if (Status == CollatorNoError)
    {
        // If there are still audio bytes to accumulate to have a complete buffer we set
        // the frame length accordingly and we set the collator state to ReadSubFrame.
        if (RemainingAudioDataLength > 0)
        {
            CollatorState = ReadSubFrame;
            *FrameLength = min(RemainingAudioDataLength, RemainingElementaryLength);
            RemainingAudioDataLength -= *FrameLength;
            SE_DEBUG(group_collator_audio, "Reading %d bytes (rest of frame)\n", *FrameLength);
        }
        // If we have accumulated a complete buffer
        // - we set the collator state to GotCompleteFrame so the buffer will be flushed for the parser
        // - we set the FrameLenght to 0 since there is nothing more to accumulate for this buffer
        // - we set set the AccumulatePrivateDataArea to true since we'll need to begin the next buffer
        //   with the private data.
        else if (AccumulatedDataSize >= nbOfBytesToHaveBeforeFlushing)
        {
            SE_DEBUG(group_collator_audio, "****** GOT FRAME ********: RemainingElementaryLength: %d, RemainingAudioDataLength = %d, AccumulatedDataSize:%d\n", RemainingElementaryLength, RemainingAudioDataLength,
                     AccumulatedDataSize);
            CollatorState = GotCompleteFrame;
            *FrameLength = 0;
            AccumulatePrivateDataArea = true;
        }
        // If there is no audio byte remaining to accumulate but we do not have a complete buffer, it means that
        // we are at the beginning of a new buffer so:
        // - we set the number of audio bytes to accumulate (RemainingAudioDataLength).
        // - we set the FrameLength value accordingly.
        else
        {
            CollatorState = ReadSubFrame;
            SE_ASSERT(0 == RemainingAudioDataLength);

            if (RemainingElementaryLength < NextParsedFrameHeader.AdpcmBlockSize * NextParsedFrameHeader.NbOfBlockPerBufferToDecode)
            {
                RemainingAudioDataLength = NextParsedFrameHeader.AdpcmBlockSize * NextParsedFrameHeader.NbOfBlockPerBufferToDecode - RemainingElementaryLength;
                *FrameLength = RemainingElementaryLength;
            }
            else
            {
                *FrameLength = NextParsedFrameHeader.AdpcmBlockSize * NextParsedFrameHeader.NbOfBlockPerBufferToDecode;
            }
        }
    }

    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// This function fills up NextParsedFrameHeader according to the content of
/// the private data PesPrivateData. It also stores the private data PesPrivateData
/// as it into PesPrivateDataArea so as to use it afterward in the
/// DecideCollatorNextStateAndGetLength() method.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioAdpcm_c::HandlePesPrivateData(unsigned char *PesPrivateData)
{
    // Debug message
    SE_DEBUG(group_collator_audio, ">><<\n");
    // Local variables.
    // - FPStatus: the status of the ParseFrameHeader()
    // - Status: the status to return
    FrameParserStatus_t  FPStatus;
    FrameParserStatus_t  Status;
    // Set the stream type.
    NextParsedFrameHeader.Type = StreamType;
    // Fill out NextParsedFrameHeader according to the the private data.
    // If it is done with no error then we save the private data area to
    // PesPrivateDataArea so as to use it afterward in the
    // DecideCollatorNextStateAndGetLength() method.
    FPStatus = FrameParser_AudioAdpcm_c::ParseFrameHeader(PesPrivateData,
                                                          &NextParsedFrameHeader,
                                                          PesPayloadLength + Configuration.ExtendedHeaderLength);

    if (FPStatus != FrameParserNoError)
    {
        Status = CollatorError;
    }
    else
    {
        PassPesPrivateDataToElementaryStreamHandler = false;
        memcpy(&PesPrivateDataArea[0], PesPrivateData, Configuration.ExtendedHeaderLength);
        Status = CollatorNoError;
    }

    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for ADPCM audio.
///
/// \return void
///
void  Collator_PesAudioAdpcm_c::SetPesPrivateDataLength(unsigned char SpecificCode)
{
}

