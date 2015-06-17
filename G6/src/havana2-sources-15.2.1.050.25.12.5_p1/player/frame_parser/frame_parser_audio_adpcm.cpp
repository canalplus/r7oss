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

////////////////////////////////////////////////////////////////////////
///
/// \class FrameParser_AudioAdpcm
/// \brief Frame parser for ADPCM audio
///

#include "collator_pes_audio_adpcm.h"
#include "codec_mme_audio.h"
#include "frame_parser_audio_adpcm.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_AudioAdpcm_c"

static BufferDataDescriptor_t     AdpcmAudioStreamParametersBuffer = BUFFER_ADPCM_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     AdpcmAudioFrameParametersBuffer = BUFFER_ADPCM_AUDIO_FRAME_PARAMETERS_TYPE;


// /////////////////////////////////////////////////////////////////////////
///
/// This function parses FrameHeaderBytes, which is the private data area,
/// and fills the ParsedFrameHeader structure.
/// Arguments:
///  - FrameHeaderBytes shall be a pointer to the PES private data. This is the
///    input of the function
///  - ParsedFrameHeader: points to a structure that will contain the
///    necessay information to decode. This is the output of the function
///    (only the Type member is used as an input).
///  - GivenFrameSize: is not used here.
///
///
///
/// \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t FrameParser_AudioAdpcm_c::ParseFrameHeader(unsigned char *FrameHeaderBytes,
                                                               AdpcmAudioParsedFrameHeader_t *ParsedFrameHeader,
                                                               int GivenFrameSize)
{
    // Local variables.
    // - NbOfSamplesPerBlock: the number of samples per ADPCM block.
    // - NumberOfAudioChannels: the number of audio channels.
    // - NbOfBlockPerBufferToDecode: in some cases the adpcm block size and the audio frequency are such
    //   that it is necessary to send to the decoder a buffer larger than the adpcm block. NbOfBlockPerBufferToDecode
    //   is the number of blocks that should be in the buffer to decode.
    // - AdpcmBlockSize: the ADPCM block.
    // - StreamType: the type of the stream, (IMA, MS, invalid, etc.). Note that, though ParsedFrameHeader
    //   is an output of the function, the Type member has been set in the ReadHeader() method.
    // - privateData: a pointer to the PES private data.
    // - status: the status to return.
    //
    unsigned int           NbOfSamplesPerBlock;
    char                   NumberOfAudioChannels;
    int                    NbOfBlockPerBufferToDecode;
    unsigned int           AdpcmBlockSize;
    AdpcmStreamType_t      StreamType                   = ParsedFrameHeader->Type;
    AdpcmPrivateData_t     *privateData                 = (AdpcmPrivateData_t *)FrameHeaderBytes;
    FrameParserStatus_t    status                       = FrameParserNoError;

    // Clear the ParsedFrameHeader.
    memset(ParsedFrameHeader, 0, sizeof(AdpcmAudioParsedFrameHeader_t));

    // Just for debug diplay the PES private data.
    for (unsigned int k = 0; k < ADPCM_PRIVATE_DATE_LENGTH; k++)
    {
        SE_DEBUG(group_frameparser_audio, "FrameHeaderBytes[%d] = %x\n", k, FrameHeaderBytes[k]);
    }

    NumberOfAudioChannels   = privateData->NbOfChannels;
    NbOfSamplesPerBlock     = privateData->NbOfSamplesPerBlock;

    if (StreamType == TypeImaAdpcm)
    {
        AdpcmBlockSize = 4 * NumberOfAudioChannels * (1 + (NbOfSamplesPerBlock - 1) / 8);
        NbOfBlockPerBufferToDecode = 1 + ((20 * privateData->Frequency) / 1000) / NbOfSamplesPerBlock;
    }
    else
    {
        AdpcmBlockSize = NumberOfAudioChannels * (6 + NbOfSamplesPerBlock / 2);
        NbOfBlockPerBufferToDecode = 1 + ((20 * privateData->Frequency) / 1000) / NbOfSamplesPerBlock;
    }

    ParsedFrameHeader->Type                        = StreamType;
    ParsedFrameHeader->SamplingFrequency1          = privateData->Frequency;
    ParsedFrameHeader->NbOfSamplesOverAllBlocks    = NbOfSamplesPerBlock * NbOfBlockPerBufferToDecode;
    ParsedFrameHeader->NumberOfChannels            = NumberOfAudioChannels;
    ParsedFrameHeader->SizeOfBufferToDecode        = AdpcmBlockSize * NbOfBlockPerBufferToDecode;;
    ParsedFrameHeader->NbOfBlockPerBufferToDecode  = NbOfBlockPerBufferToDecode;
    ParsedFrameHeader->NbOfCoefficients            = privateData->NbOfCoefficients;
    ParsedFrameHeader->PrivateHeaderLength         = ADPCM_PRIVATE_DATE_LENGTH;
    ParsedFrameHeader->SubStreamId                 = 0xA0;
    ParsedFrameHeader->AdpcmBlockSize              = AdpcmBlockSize;
    return status;
}



///////////////////////////////////////////////////////////////////////////
///
///     Constructor
///
FrameParser_AudioAdpcm_c::FrameParser_AudioAdpcm_c(unsigned int DecodeLatencyInSamples)
    : FrameParser_Audio_c()
    , StreamParameters(NULL)
    , CurrentStreamParameters()
    , FrameParameters(NULL)
    , StreamType()
    , IndirectDecodeLatencyInSamples(DecodeLatencyInSamples)
    , LastPacket(0)
{
    Configuration.FrameParserName             = "AudioAdpcm";
    Configuration.StreamParametersCount       = 64;
    Configuration.StreamParametersDescriptor  = &AdpcmAudioStreamParametersBuffer;
    Configuration.FrameParametersCount        = 64;
    Configuration.FrameParametersDescriptor   = &AdpcmAudioFrameParametersBuffer;
}

////////////////////////////////////////////////////////////////////////////
///
///     Destructor
///
FrameParser_AudioAdpcm_c::~FrameParser_AudioAdpcm_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
///     Method to connect to neighbor
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::Connect(Port_c *Port)
{
    // Clear our parameter pointers
    StreamParameters                    = NULL;
    FrameParameters                     = NULL;

    // Set illegal state forcing a stream parameter update on the first frame
    memset(&CurrentStreamParameters, 0, sizeof(CurrentStreamParameters));
    CurrentStreamParameters.Type = TypeAdpcmInvalid;

    // Pass the call down the line
    FrameParserStatus_t Status = FrameParser_Audio_c::Connect(Port);
    if (Status == FrameParserNoError)
    {
        // After calling the base class method we have a valid pointer to the collator.
        StreamType = ((Collator_PesAudioAdpcm_c *)Stream->GetCollator())->GetStreamType();
    }

    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Parse the frame header and store the results for when we emit the frame.
/// We store in particular the stream parameter and the audio parameters
/// in the buffer metadata.
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::ReadHeaders(void)
{
    SE_DEBUG(group_frameparser_audio, ">><<\n");
    //
    // Perform the common portion of the read headers function
    //
    FrameParser_Audio_c::ReadHeaders();

    // the frame type is required to parse the private data area
    AdpcmAudioParsedFrameHeader_t ParsedFrameHeader;
    ParsedFrameHeader.Type = StreamType;
    FrameParserStatus_t Status = ParseFrameHeader(BufferData, &ParsedFrameHeader, BufferLength);
    if (Status != FrameParserNoError)
    {
        SE_ERROR("Failed to parse frame header, bad collator selected?\n");
        return Status;
    }

    FrameToDecode = true;
    Status = GetNewFrameParameters((void **) &FrameParameters);
    if (Status != FrameParserNoError)
    {
        SE_ERROR("Cannot get new frame parameters\n");
        return Status;
    }

    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = true;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;
    ParsedFrameParameters->KeyFrame                                     = true;
    ParsedFrameParameters->ReferenceFrame                               = false;
    ParsedFrameParameters->NewFrameParameters        = true;
    ParsedFrameParameters->SizeofFrameParameterStructure = sizeof(AdpcmAudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure       = FrameParameters;
    ParsedFrameParameters->DataOffset = ADPCM_PRIVATE_DATE_LENGTH;
    FrameParameters->DrcCode = ParsedFrameHeader.DrcCode;
    FrameParameters->NbOfSamplesOverAllBlocks = ParsedFrameHeader.NbOfSamplesOverAllBlocks;
    FrameParameters->NbOfBlockPerBufferToDecode = ParsedFrameHeader.NbOfBlockPerBufferToDecode;

    // If the type of the current stream is not valid, which occurs for the first frame,
    // then we update the stream parameters.
    if (CurrentStreamParameters.Type == TypeAdpcmInvalid)
    {
        UpdateStreamParameters = true;
        Status = GetNewStreamParameters((void **) &StreamParameters);
        if (Status != FrameParserNoError)
        {
            SE_ERROR("Cannot get new stream parameters\n");
            return Status;
        }

        memcpy(StreamParameters, &ParsedFrameHeader, sizeof(AdpcmAudioStreamParameters_t));
        memcpy(&CurrentStreamParameters, &ParsedFrameHeader, sizeof(AdpcmAudioStreamParameters_t));
    }
    else
    {
        UpdateStreamParameters = false;
    }

    ParsedAudioParameters->Source.BitsPerSample = 0;
    ParsedAudioParameters->Source.ChannelCount = 0;
    /*Status = GetFreqFromAccFreqCode(ParsedAudioParameters->Source.SampleRateHz,(enum eAccFsCode)( ParsedFrameHeader.SamplingFrequency1));
    if(Status!=FrameParserNoError)
    {
        SE_DEBUG(group_frameparser_audio, "Error in getting frequency in Hz from ACC frequency code");
        return FrameParserError;
    }*/
    ParsedAudioParameters->Source.SampleRateHz = ParsedFrameHeader.SamplingFrequency1;
    ParsedAudioParameters->SampleCount = ParsedFrameHeader.NbOfSamplesOverAllBlocks;
    ParsedAudioParameters->Organisation = 0; // filled in by codec
    Stream->Statistics().FrameParserAudioFrameSize = ParsedFrameHeader.SizeOfBufferToDecode;
    Stream->Statistics().FrameParserAudioSampleRate = ParsedAudioParameters->Source.SampleRateHz;
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     The reset reference frame list function
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::ResetReferenceFrameList(void)
{
    SE_DEBUG(group_frameparser_audio, ">><<");
    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL);
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
/// \copydoc FrameParser_Audio_c::PurgeQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::PurgeQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::ProcessQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
/// For LPCM audio these can be determined immediately (although it the first
/// frame for decode does not contain a PTS we must synthesize one).
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::GeneratePostDecodeParameterSettings(void)
{
    //
    // Default setting
    //
    ParsedFrameParameters->DisplayFrameIndex            = INVALID_INDEX;
    ParsedFrameParameters->NativePlaybackTime           = INVALID_TIME;
    ParsedFrameParameters->NormalizedPlaybackTime       = INVALID_TIME;
    ParsedFrameParameters->NativeDecodeTime             = INVALID_TIME;
    ParsedFrameParameters->NormalizedDecodeTime         = INVALID_TIME;

    //
    // Record in the structure the decode and presentation times if specified
    //

    if (CodedFrameParameters->PlaybackTimeValid)
    {
        ParsedFrameParameters->NativePlaybackTime       = CodedFrameParameters->PlaybackTime;
        TranslatePlaybackTimeNativeToNormalized(CodedFrameParameters->PlaybackTime, &ParsedFrameParameters->NormalizedPlaybackTime);
    }

    if (CodedFrameParameters->DecodeTimeValid)
    {
        ParsedFrameParameters->NativeDecodeTime         = CodedFrameParameters->DecodeTime;
        TranslatePlaybackTimeNativeToNormalized(CodedFrameParameters->DecodeTime, &ParsedFrameParameters->NormalizedDecodeTime);
    }

    //
    // Synthesize the presentation time if required
    //
    FrameParserStatus_t Status = HandleCurrentFrameNormalizedPlaybackTime();
    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // We can't fail after this point so this is a good time to provide a display frame index
    //
    ParsedFrameParameters->DisplayFrameIndex = NextDisplayFrameIndex++;
    //
    // Use the super-class utilities to complete our housekeeping chores
    //
    HandleUpdateStreamParameters();
    GenerateNextFrameNormalizedPlaybackTime(CurrentStreamParameters.NbOfSamplesOverAllBlocks,
                                            CurrentStreamParameters.SamplingFrequency1);
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::PrepareReferenceFrameList(void)
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::UpdateReferenceFrameList(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::ProcessReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::ProcessReverseDecodeStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::PurgeReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::PurgeReverseDecodeStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioAdpcm_c::TestForTrickModeFrameDrop(void)
{
    return FrameParserNoError;
}

