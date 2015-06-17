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
/// \class FrameParser_AudioMlp_c
/// \brief Frame parser for MLP (Meridian Lossless Packing)
///

#include "mlp.h"
#include "frame_parser_audio_mlp.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_AudioMlp_c"

static BufferDataDescriptor_t     MlpAudioStreamParametersBuffer = BUFFER_MLP_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     MlpAudioFrameParametersBuffer = BUFFER_MLP_AUDIO_FRAME_PARAMETERS_TYPE;

//
// Bit rate lookup table for MLP audio frame parsing
//

// this array is based on the MlpSamplingFreq_t enum
const static unsigned char MlpSampleCount[MlpSamplingFreqNone] =
{
    40, 80, 160, 0, 0, 0, 0, 0, 40, 80, 160
};

// this array is based on the MlpSamplingFreq_t enum
static const int MlpSamplingFreq[MlpSamplingFreqNone] =
{
    48000, 96000, 192000, 0, 0, 0, 0, 0, 44100, 88200, 176400
};

////////////////////////////////////////////////////////////////////////////
///
/// Examine the supplied frame header and extract the information contained within.
///
/// This is a utility function shared by the frame parser and the equivalent
/// collator. Due to its shared nature this is a static method and does not access
/// any information not provided via the function arguments.
/// \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t FrameParser_AudioMlp_c::ParseSingleFrameHeader(unsigned char *FrameHeaderBytes,
                                                                   MlpAudioParsedFrameHeader_t *LocalParsedFrameHeader)
{
    BitStreamClass_c Bits;
    int          AccessUnitLength;
    unsigned int FormatSync;
    bool         IsMajorSync = false;
    bool         IsFBAType = false;

    Bits.SetPointer(FrameHeaderBytes);
    // check_nibble
    Bits.Flush(4);
    // access_unit_length
    AccessUnitLength = 2 * (Bits.Get(12));
    // input_timing
    Bits.FlushUnseen(16);
    // format_sync
    FormatSync = Bits.Get(32);

    if (FormatSync == MLP_FORMAT_SYNC_A)
    {
        IsMajorSync = true;
        IsFBAType = true;
    }
    else if (FormatSync == MLP_FORMAT_SYNC_B)
    {
        IsMajorSync = true;
    }

    if (IsMajorSync)
    {
        // the following variables are the exploded Format Info field of
        // a MLP major sync
        MlpSamplingFreq_t FreqId;

        // parse format_info field
        if (IsFBAType)
        {
            FreqId = (MlpSamplingFreq_t) Bits.Get(4);
            Bits.FlushUnseen(28);

            // sanity check on sampling frequency
            if (((FreqId > MlpSamplingFreq192) &&
                 (FreqId < MlpSamplingFreq44p1)) ||
                (FreqId >= MlpSamplingFreqNone))
            {
                SE_ERROR("Invalid Sampling Frequency: %d\n",
                         FreqId);
                return FrameParserError;
            }
        }
        else
        {
            // the format of format_info is the one described inthe DVD-Audio specs.
            MlpWordSize_t WordSizeId1 = (MlpWordSize_t) Bits.Get(4);
            MlpWordSize_t WordSizeId2 = (MlpWordSize_t) Bits.Get(4);
            FreqId = (MlpSamplingFreq_t) Bits.Get(4);
            MlpSamplingFreq_t FreqId2 = (MlpSamplingFreq_t) Bits.Get(4);

            // sanity checks on word sizes
            if ((WordSizeId1 >= MlpWordSizeNone) ||
                ((WordSizeId2 >= MlpWordSizeNone) && (WordSizeId2 != MLP_DVD_AUDIO_NO_CH_GR2)))
            {
                SE_ERROR("Invalid Word Size\n");
                return FrameParserError;
            }

            // sanity check on sampling frequencies
            if ((((FreqId > MlpSamplingFreq192) &&
                  (FreqId < MlpSamplingFreq44p1)) ||
                 (FreqId >= MlpSamplingFreqNone)) ||
                (((FreqId2 > MlpSamplingFreq96) &&
                  (FreqId2 < MlpSamplingFreq44p1)) ||
                 ((FreqId2 >= MlpSamplingFreq176p4) && (FreqId2 != MLP_DVD_AUDIO_NO_CH_GR2))))
            {
                SE_ERROR("Invalid Sampling Frequency\n");
                return FrameParserError;
            }

            Bits.FlushUnseen(16);
        }

        LocalParsedFrameHeader->NumberOfSamples = MlpSampleCount[FreqId];
        LocalParsedFrameHeader->SamplingFrequency = FreqId;
        {
            // sanity checks on signature
            unsigned int Signature = Bits.Get(16);

            if (Signature != MLP_SIGNATURE)
            {
                SE_ERROR("Wrong signature: %dx\n", Signature);
                return FrameParserError;
            }
        }
    }

    LocalParsedFrameHeader->IsMajorSync = IsMajorSync;
    LocalParsedFrameHeader->Length = AccessUnitLength;

    //

    if (IsMajorSync)
    {
        SE_DEBUG(group_frameparser_audio,  "IsMajorSync, IsFBAType: %d, Length: %d, NumberOfSamples: %d, Frequency %d\n",
                 IsFBAType, AccessUnitLength, LocalParsedFrameHeader->NumberOfSamples,
                 MlpSamplingFreq[LocalParsedFrameHeader->SamplingFrequency]);
    }
    else
    {
        SE_DEBUG(group_frameparser_audio,  "Length: %d\n",
                 AccessUnitLength);
    }

    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Examine the mlp globbed audio frames, and extract the correct metadata
/// to be attached to the frame buffer
///
/// \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t FrameParser_AudioMlp_c::ParseFrameHeader(unsigned char *FrameHeaderBytes,
                                                             MlpAudioParsedFrameHeader_t *LocalParsedFrameHeader,
                                                             int GivenFrameSize)
{
    int StreamIndex =  0, FrameSize =  0 ;
    MlpAudioParsedFrameHeader_t NextParsedFrameHeader;
    LocalParsedFrameHeader->AudioFrameNumber = 0;
    LocalParsedFrameHeader->NumberOfSamples = 0;

    do
    {
        unsigned char *NextFrameHeader = &FrameHeaderBytes[StreamIndex];
        FrameParserStatus_t Status;
        memset(&NextParsedFrameHeader, 0, sizeof(MlpAudioParsedFrameHeader_t));
        // parse a single frame
        Status = FrameParser_AudioMlp_c::ParseSingleFrameHeader(NextFrameHeader, &NextParsedFrameHeader);

        if (Status !=  FrameParserNoError)
        {
            return (Status);
        }

        LocalParsedFrameHeader->AudioFrameNumber += 1;

        if (NextParsedFrameHeader.IsMajorSync)
        {
            if (IsFirstMajorFrame)
            {
                // store the stream properties
                LocalParsedFrameHeader->SamplingFrequency = NextParsedFrameHeader.SamplingFrequency;
                LocalParsedFrameHeader->NumberOfSamples = NextParsedFrameHeader.NumberOfSamples;
                IsFirstMajorFrame = false;
                SE_INFO(group_frameparser_audio, "Mlp stream properties: Sampling Freq: %d, Nb of samples: %d\n",
                        MlpSamplingFreq[LocalParsedFrameHeader->SamplingFrequency],
                        LocalParsedFrameHeader->NumberOfSamples);
            }
            else
            {
                if (NextParsedFrameHeader.SamplingFrequency != LocalParsedFrameHeader->SamplingFrequency)
                {
                    SE_ERROR("Unauthorized sampling frequency update\n");
                }
            }
        }

        FrameSize += NextParsedFrameHeader.Length;
        StreamIndex += NextParsedFrameHeader.Length;
    }
    while (StreamIndex < GivenFrameSize);

    LocalParsedFrameHeader->Length = FrameSize;
    LocalParsedFrameHeader->NumberOfSamples = MlpSampleCount[LocalParsedFrameHeader->SamplingFrequency] *
                                              LocalParsedFrameHeader->AudioFrameNumber;
    SE_DEBUG(group_frameparser_audio, "SamplingFrequency: %d, FrameSize: %d, NumberOfSamples: %d, NbFrames: %d\n",
             MlpSamplingFreq[LocalParsedFrameHeader->SamplingFrequency],
             LocalParsedFrameHeader->Length,
             LocalParsedFrameHeader->NumberOfSamples,
             LocalParsedFrameHeader->AudioFrameNumber);
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     Constructor
///
FrameParser_AudioMlp_c::FrameParser_AudioMlp_c(void)
    : FrameParser_Audio_c()
    , ParsedFrameHeader()
    , StreamParameters(NULL)
    , CurrentStreamParameters()
    , FrameParameters(NULL)
    , IsFirstMajorFrame(true)
{
    Configuration.FrameParserName               = "AudioMlp";
    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &MlpAudioStreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &MlpAudioFrameParametersBuffer;
}

////////////////////////////////////////////////////////////////////////////
///
///     Destructor
///
FrameParser_AudioMlp_c::~FrameParser_AudioMlp_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
///     Method to connect to neighbor
///
FrameParserStatus_t   FrameParser_AudioMlp_c::Connect(Port_c *Port)
{
    //
    // Clear our parameter pointers
    //
    StreamParameters                    = NULL;
    FrameParameters                     = NULL;
    //
    // Set illegal state forcing a parameter update on the first frame
    //
    memset(&CurrentStreamParameters, 0, sizeof(CurrentStreamParameters));
    CurrentStreamParameters.AccumulatedFrameNumber = 0;
    memset(&ParsedFrameHeader, 0, sizeof(MlpAudioParsedFrameHeader_t));
    IsFirstMajorFrame = true;
    //
    // Pass the call down the line
    //
    return FrameParser_Audio_c::Connect(Port);
}


////////////////////////////////////////////////////////////////////////////
///
/// Parse the frame header and store the results for when we emit the frame.
///
FrameParserStatus_t   FrameParser_AudioMlp_c::ReadHeaders(void)
{
    //
    // Perform the common portion of the read headers function
    //
    FrameParser_Audio_c::ReadHeaders();

    FrameParserStatus_t Status = ParseFrameHeader(BufferData, &ParsedFrameHeader, BufferLength);
    if (Status != FrameParserNoError)
    {
        SE_ERROR("Failed to parse frame header, bad collator selected?\n");
        return Status;
    }

    if (ParsedFrameHeader.Length != BufferLength)
    {
        SE_ERROR("Buffer length is inconsistent with frame header, bad collator selected?\n");
        return FrameParserError;
    }

    FrameToDecode = true;

    if (CurrentStreamParameters.AccumulatedFrameNumber != ParsedFrameHeader.AudioFrameNumber)
    {
        Status = GetNewStreamParameters((void **) &StreamParameters);
        if (Status != FrameParserNoError)
        {
            SE_ERROR("Cannot get new stream parameters\n");
            return Status;
        }

        StreamParameters->AccumulatedFrameNumber = ParsedFrameHeader.AudioFrameNumber;
        CurrentStreamParameters.AccumulatedFrameNumber = ParsedFrameHeader.AudioFrameNumber;
        UpdateStreamParameters = true;
    }

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
    ParsedFrameParameters->NewFrameParameters                           = true;
    ParsedFrameParameters->SizeofFrameParameterStructure = sizeof(MlpAudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure       = FrameParameters;
    ParsedAudioParameters->Source.BitsPerSample = 0; // filled in by codec
    ParsedAudioParameters->Source.ChannelCount = 0;  // filled in by codec
    ParsedAudioParameters->Source.SampleRateHz = MlpSamplingFreq[ParsedFrameHeader.SamplingFrequency];
    ParsedAudioParameters->SampleCount = ParsedFrameHeader.NumberOfSamples;
    ParsedAudioParameters->Organisation = 0; // filled in by codec
    Stream->Statistics().FrameParserAudioSampleRate = ParsedAudioParameters->Source.SampleRateHz;
    Stream->Statistics().FrameParserAudioFrameSize = ParsedFrameHeader.Length;
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     The reset reference frame list function
///
FrameParserStatus_t   FrameParser_AudioMlp_c::ResetReferenceFrameList(void)
{
    SE_DEBUG(group_frameparser_audio, ">><<");
    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL);
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MLP audio.
///
/// \copydoc FrameParser_Audio_c::PurgeQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioMlp_c::PurgeQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MLP audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioMlp_c::ProcessQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
/// For MLP audio these can be determined immediately (although it the first
/// frame for decode does not contain a PTS we must synthesize one).
///
FrameParserStatus_t   FrameParser_AudioMlp_c::GeneratePostDecodeParameterSettings(void)
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
    ParsedFrameParameters->DisplayFrameIndex         = NextDisplayFrameIndex++;
    //
    // Use the super-class utilities to complete our housekeeping chores
    //
    HandleUpdateStreamParameters();
    GenerateNextFrameNormalizedPlaybackTime(ParsedFrameHeader.NumberOfSamples,
                                            MlpSamplingFreq[ParsedFrameHeader.SamplingFrequency]);
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MLP audio.
///
FrameParserStatus_t   FrameParser_AudioMlp_c::PrepareReferenceFrameList(void)
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MLP audio.
///
FrameParserStatus_t   FrameParser_AudioMlp_c::UpdateReferenceFrameList(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MLP audio.
///
FrameParserStatus_t   FrameParser_AudioMlp_c::ProcessReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MLP audio.
///
FrameParserStatus_t   FrameParser_AudioMlp_c::ProcessReverseDecodeStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MLP audio.
///
FrameParserStatus_t   FrameParser_AudioMlp_c::PurgeReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MLP audio.
///
FrameParserStatus_t   FrameParser_AudioMlp_c::PurgeReverseDecodeStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MLP audio.
///
FrameParserStatus_t   FrameParser_AudioMlp_c::TestForTrickModeFrameDrop(void)
{
    return FrameParserNoError;
}

