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
/// \class Collator_PesAudioEAc3_c
///
/// Implements EAC3 audio sync word scanning and frame length analysis.
///

#include "frame_parser_audio_eac3.h"
#include "eac3_audio.h"
#include "collator_pes_audio_eac3.h"

#define EXTENDED_STREAM_PES_START_CODE          0xfd
#define EXTENDED_STREAM_PES_FULL_START_CODE     0x000001fd
#define MAX_DDPLUS_FRAME_SIZE                   (24 *1024)

////////////////////////////////////////////////////////////////////////////
///
///
Collator_PesAudioEAc3_c::Collator_PesAudioEAc3_c(void)
    : Collator_PesAudioDvd_c(EAC3_BYTES_NEEDED)
    , EightChannelsRequired(true)
    , ProgrammeId(0)
    , NbAccumulatedSamples(0)
    , SelectedSubStreamNbAccumulatedSamples(0)
    , FirstBlockConvSync(0)
    , InvalidBSID(false)
    , DeltaLength(0)
{
    Configuration.StreamIdentifierMask             = 0xff;
    Configuration.StreamIdentifierCode             = 0xbd; // from Rome...
    Configuration.BlockTerminateMask               = 0xff; // Picture
    Configuration.BlockTerminateCode               = 0x00;
    Configuration.IgnoreCodesRanges.NbEntries      = 1;
    Configuration.IgnoreCodesRanges.Table[0].Start = 0x01; // All slice codes
    Configuration.IgnoreCodesRanges.Table[0].End   = 0xbd - 1;
    Configuration.InsertFrameTerminateCode         = false;
    Configuration.TerminalCode                     = 0;
    Configuration.ExtendedHeaderLength             = 0 /* 4 */;
    Configuration.DeferredTerminateFlag            = false;
}

////////////////////////////////////////////////////////////////////////////
///
/// Search for the EAC3 audio synchonrization word and, if found, report its offset.
///
/// Additionally this method will compare the location of any sync word found with
/// the predicted location provided by Collator_PesAudioEAc3_c::HandlePesPrivateDataArea()
/// and choose the appropriate encapsulation mode.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioEAc3_c::FindNextSyncWord(int *CodeOffset)
{
    int i;
    unsigned char eac3Header[EAC3_BYTES_NEEDED];
    EAc3AudioParsedFrameHeader_t ParsedFrameHeader;
    int RemainingInPotential = PotentialFrameHeaderLength;
    unsigned char *PotentialFramePtr = PotentialFrameHeader;
    unsigned char *ElementaryPtr;
    int Offset;

    // do the most naive possible search. there is no obvious need for performance here
    for (i = 0; i <= (int)(RemainingElementaryLength + PotentialFrameHeaderLength - EAC3_BYTES_NEEDED); i++)
    {
        if (RemainingInPotential > 0)
        {
            /* we need at least 65 bytes to get the stream type...*/
            int size =  min(RemainingInPotential, EAC3_BYTES_NEEDED);
            memcpy(&eac3Header[0], PotentialFramePtr, size);
            memcpy(&eac3Header[size], &RemainingElementaryData[0], EAC3_BYTES_NEEDED - size);
            ElementaryPtr = eac3Header;
        }
        else
        {
            ElementaryPtr = &RemainingElementaryData[i - PotentialFrameHeaderLength];
        }

        FrameParserStatus_t FPStatus = FrameParser_AudioEAc3_c::ParseSingleFrameHeader(ElementaryPtr,
                                                                                       &ParsedFrameHeader,
                                                                                       true);

        if (FPStatus == FrameParserNoError)
        {
            // the condition for synchronization is the following:
            // * the frame is a regular AC3 frame, OR
            // * the frame is an independant E-AC3 (DD+) subframe, belonging to requested programme, containing 6 blocks (1536 samples), OR
            // * the frame is a independant E-AC3 (DD+) subframe, belonging to requested programme, containing less than six blocks, but with the convsync on
            // Also allow stream with invalid BSID : Certification requirement : FW will generate mute for these frames
            // (i.e. this is the first block of the six...)
            if ((ParsedFrameHeader.Type == TypeInValidBSID) || (ParsedFrameHeader.Type == TypeAc3) ||
                ((ParsedFrameHeader.Type == TypeEac3Ind) && (ParsedFrameHeader.SubStreamId == ProgrammeId) && (ParsedFrameHeader.NumberOfSamples == EAC3_NBSAMPLES_NEEDED)) ||
                ((ParsedFrameHeader.Type == TypeEac3Ind) && (ParsedFrameHeader.SubStreamId == ProgrammeId) && (ParsedFrameHeader.FirstBlockForTranscoding)))
            {
                Offset = (RemainingInPotential > 0) ? (-RemainingInPotential) : (i - PotentialFrameHeaderLength);
                VerifyDvdSyncWordPrediction(Offset);
                SE_DEBUG(group_collator_audio, ">>Got Synchronization, i = %d <<\n", Offset);
                *CodeOffset = Offset;
                return CollatorNoError;
            }
        }

        RemainingInPotential--;
        PotentialFramePtr++;
    }

    AdjustDvdSyncWordPredictionAfterConsumingData(-RemainingElementaryLength);
    return CollatorError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Read the subframe if required the subframe
///
/// Additionally drop the subframe if the subframe exceeds the max DD+ frame size
/// \return void.
///
void Collator_PesAudioEAc3_c::ReadSubframe(EAc3AudioParsedFrameHeader_t *ParsedFrameHeader)
{
    // according to collator channel configuration,
    // read or skip the dependant subframe of the right programme...
    CollatorState = EightChannelsRequired ? ReadSubFrame : SkipSubFrame;

    // we only increment NbAccumulatedSamples if we are of type TypeEac3Ind and SubStreamId == 0
    if (ParsedFrameHeader->Type == TypeEac3Ind && ParsedFrameHeader->SubStreamId == 0)
    {
        // acumulate samples only if this is an eac3 independant bistream (dependant bitstream provide additional channels only)
        NbAccumulatedSamples += ParsedFrameHeader->NumberOfSamples;
    }
    else if ((ParsedFrameHeader->Type == TypeEac3Ind) &&
             (ParsedFrameHeader->SubStreamId == (unsigned int)Player->PolicyValue(Playback, Stream, PolicyAudioSubstreamId)))
    {
        // acumulate samples for the selected independant bistream (dependant bitstream provide additional channels only)
        SelectedSubStreamNbAccumulatedSamples += ParsedFrameHeader->NumberOfSamples;
    }
    else if (ParsedFrameHeader->Type == TypeEac3Reserved)
    {
        CollatorState = SkipSubFrame;
    }

    // The AccumulatedDataSize already contains the header.Since the
    // parsing of the header starts only after the accumulation of 65
    // bytes, so the frame length should be reduced by 65 bytes. The
    // problem will arise only if the AccumulatedDataSize + 65 more bytes
    // than the maximum allowable superframe size (24Kb)

    if (CollatorState == ReadSubFrame && ((AccumulatedDataSize + ParsedFrameHeader->Length) < (MAX_DDPLUS_FRAME_SIZE + EAC3_BYTES_NEEDED)))
    {
        SE_DEBUG(group_collator_audio, "Accumulate a subframe of %d/%d samples\n", ParsedFrameHeader->NumberOfSamples, NbAccumulatedSamples);
        //SE_INFO(group_collator_audio, "ReadSubFrame : %d, %d, Blocks:%d!\n",ParsedFrameHeader->Type, ParsedFrameHeader->SubStreamId, NbAccumulatedSamples/256);
    }
    else
    {
        CollatorState = SkipSubFrame;
        SE_DEBUG(group_collator_audio, "SkipSubFrame for size : %d, %d!\n", ParsedFrameHeader->Type, ParsedFrameHeader->SubStreamId);
    }
}
////////////////////////////////////////////////////////////////////////////
///
/// Determine the new state of the collator according to the incoming sub frame for MSxx use case
/// Also returns this sub frame length in samples
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioEAc3_c::HandleProfileMSxx(EAc3AudioParsedFrameHeader_t *ParsedFrameHeader)
{
    CollatorStatus_t Status = CollatorNoError; // assume success unless told otherwise

    if ((ParsedFrameHeader->FirstBlockForTranscoding && NbAccumulatedSamples && (ParsedFrameHeader->SubStreamId == 0)) ||
        (ParsedFrameHeader->Type == TypeAc3) || (ParsedFrameHeader->Type == TypeInValidBSID))
    {
        // Deliver a frame if we have convsync or a block which if the firrst block for transcoding
        //SE_INFO(group_collator_audio, "complete[%d]:Type:%d,subId:%d,blocks:%d!\n",Framecount,ParsedFrameHeader->Type, ParsedFrameHeader->SubStreamId,(NbAccumulatedSamples/256));
        UpdateAccumulatedSamplesAndChangeState(GotCompleteFrame, ParsedFrameHeader);
    }
    else if ((ParsedFrameHeader->Type == TypeEac3Ind) && (ParsedFrameHeader->SubStreamId == 0) &&
             ((NbAccumulatedSamples == EAC3_NBSAMPLES_NEEDED) || ((SelectedSubStreamNbAccumulatedSamples == EAC3_NBSAMPLES_NEEDED))))
    {
        // this is another independant subframe.Deliver the frame if No. of samples accumulated for I0 or for SelectedSubStream equal to 1536 (AC3 frame size).see bug30466
        //SE_INFO(group_collator_audio, "complete[%d]:Type:%d,subId:%d,blocks:%d!\n",Framecount,ParsedFrameHeader->Type, ParsedFrameHeader->SubStreamId,(NbAccumulatedSamples/256));
        UpdateAccumulatedSamplesAndChangeState(GotCompleteFrame, ParsedFrameHeader);
    }
    else if (NbAccumulatedSamples <= EAC3_NBSAMPLES_NEEDED)
    {
        ReadSubframe(ParsedFrameHeader);
    }
    else
    {
        SE_ERROR("Accumulated too many samples (%d of %d)\n",
                 NbAccumulatedSamples, EAC3_NBSAMPLES_NEEDED);
        Status = CollatorError;
    }

    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Determine the new state of the collator according to the incoming sub frame for non Msxx use case
/// Also returns this sub frame length in samples
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioEAc3_c::HandleProfileNonMSxx(EAc3AudioParsedFrameHeader_t *ParsedFrameHeader)
{
    CollatorStatus_t Status = CollatorNoError; // assume success unless told otherwise

    if (((!FirstBlockConvSync) && (ParsedFrameHeader->NumberOfSamples != EAC3_NBSAMPLES_NEEDED))
        || ((FirstBlockConvSync && ParsedFrameHeader->FirstBlockForTranscoding) && (NbAccumulatedSamples != EAC3_NBSAMPLES_NEEDED)))
    {
        /// TODO : Condition the reporting of Error to MS1x playback profile : in that case such frames shall be dropped.
        //SE_INFO(group_collator_audio, "complete[%d]:Type:%d,subId:%d,blocks:%d!\n",Framecount,ParsedFrameHeader->Type, ParsedFrameHeader->SubStreamId,(NbAccumulatedSamples/256));
        // return CollatorError;
        UpdateAccumulatedSamplesAndChangeState(GotCompleteFrame, ParsedFrameHeader);
    }
    else if (((ParsedFrameHeader->Type == TypeEac3Ind) && (ParsedFrameHeader->SubStreamId == ProgrammeId) && (NbAccumulatedSamples == EAC3_NBSAMPLES_NEEDED))
             || (ParsedFrameHeader->Type == TypeAc3) || (ParsedFrameHeader->Type == TypeInValidBSID))
    {
        // this is another independant subframe
        //SE_INFO(group_collator_audio, "complete[%d]:Type:%d,subId:%d,blocks:%d!\n",Framecount,ParsedFrameHeader->Type, ParsedFrameHeader->SubStreamId,(NbAccumulatedSamples/256));
        UpdateAccumulatedSamplesAndChangeState(GotCompleteFrame, ParsedFrameHeader);
    }
    else if (ParsedFrameHeader->SubStreamId != ProgrammeId)
    {
        // skip any subframe (independant or dependant) that does not belong to the requested programme
        CollatorState = SkipSubFrame;
    }
    else if (NbAccumulatedSamples <= EAC3_NBSAMPLES_NEEDED)
    {
        ReadSubframe(ParsedFrameHeader);
    }
    else
    {
        SE_ERROR("Accumulated too many samples (%d of %d)\n",
                 NbAccumulatedSamples, EAC3_NBSAMPLES_NEEDED);
        Status = CollatorError;
    }

    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Determine the new state of the collator according to the incoming sub frame
/// Also returns this sub frame length
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudioEAc3_c::DecideCollatorNextStateAndGetLength(unsigned int *FrameLength)
{
    FrameParserStatus_t FPStatus;
    CollatorStatus_t Status = CollatorNoError; // assume success unless told otherwise
    EAc3AudioParsedFrameHeader_t ParsedFrameHeader;
    FPStatus = FrameParser_AudioEAc3_c::ParseSingleFrameHeader(StoredFrameHeader,
                                                               &ParsedFrameHeader,
                                                               true); // Always check the convsync

    if (FPStatus == FrameParserNoError)
    {
        *FrameLength = ParsedFrameHeader.Length;
        //store the alternate Size
        DeltaLength = ParsedFrameHeader.DeltaLength;
        InvalidBSID = ((ParsedFrameHeader.Type == TypeInValidBSID)) ? true : false;

        if (CollatorState == SeekingFrameEnd)
        {
            // we already have an independant substream accumulated, check what to do with
            // this next one
            /* If we have not got any ConvSync for the first block(non 1536) issue the error OR if we have found the ConvSync between the blocks(before 1536 samples)  then also issue the error*/
            /* If our strategy is to decode and play non convertable frames. Then instead of issuing an error send "non accumulated" frames to the decoder */
            Status = HandleProfileMSxx(&ParsedFrameHeader);
        }
        else
        {
            SE_DEBUG(group_collator_audio, "Synchronized first block is a good candidate for transcoding\n");
            UpdateAccumulatedSamplesAndChangeState(ReadSubFrame, &ParsedFrameHeader);
        }
    }
    else
    {
        Status = CollatorError;

        if (InvalidBSID)
        {
            *FrameLength = DeltaLength;
            // If we have an alternate Framesize, check for sync there else we go to ValidateFrame
            CollatorState = (DeltaLength != 0) ? ReadSubFrame : ValidateFrame; // we have to read this in too
            InvalidBSID = false;
            // Fake no error as we need to check the next size
            //SE_INFO(group_collator_audio, "Jump %d\n",DeltaLength);
            DeltaLength = 0;
            Status = CollatorNoError;
        }
    }

    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Sets the extended pes header length according to the stream_id field of the pes header
///
/// \return void
///
void  Collator_PesAudioEAc3_c::SetPesPrivateDataLength(unsigned char SpecificCode)
{
    /* by default the optional private data area length will be set to zero... */
    /* otherwise for DVD the private data area is 4 bytes long */
    Configuration.ExtendedHeaderLength = (SpecificCode == PES_START_CODE_PRIVATE_STREAM_1) ? 4 : 0;
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
CollatorStatus_t Collator_PesAudioEAc3_c::HandlePesPrivateData(unsigned char *PesPrivateData)
{
    BitStreamClass_c Bits;

    if (CollatorState == SeekingSyncWord)
    {
        // ensure the PES private data is passed to the sync detection code
        PassPesPrivateDataToElementaryStreamHandler = true;
        // parse the private data area (assuming that is what we have)
        Bits.SetPointer(PesPrivateData);
        unsigned int SubStreamId = Bits.Get(8);
        unsigned int NumberOfFrameHeaders = Bits.Get(8);
        unsigned int FirstAccessUnitPointer = Bits.Get(16);

        if (((SubStreamId & 0xb8) != 0x80) ||
            (NumberOfFrameHeaders > 127) ||
            (FirstAccessUnitPointer > 2034) ||
            (FirstAccessUnitPointer == 0))
        {
            MakeDvdSyncWordPrediction(INVALID_PREDICTION);
        }
        else
        {
            // FirstAccessUnitPointer is relative to the final byte of the private data area. Since
            // the private data area will itself be scanned for start codes this means that we must
            // add three to our predicted offset.
            MakeDvdSyncWordPrediction(FirstAccessUnitPointer + 3);
        }
    }

    return CollatorNoError;
}

