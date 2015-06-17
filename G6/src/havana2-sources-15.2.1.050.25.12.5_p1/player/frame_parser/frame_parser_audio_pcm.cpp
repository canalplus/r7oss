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

#include "ring_generic.h"
#include "frame_parser_audio_pcm.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_AudioPcm_c"

#define DUMP_HEADERS

//{{{  Locally defined constants and macros

static BufferDataDescriptor_t     PcmAudioStreamParametersBuffer       = BUFFER_PCM_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     PcmAudioFrameParametersBuffer        = BUFFER_PCM_AUDIO_FRAME_PARAMETERS_TYPE;

#define CodeToInteger(a,b,c,d)          ((a << 0) | (b << 8) | (c << 16) | (d << 24))

static inline unsigned int BE2LE(unsigned int Value)
{
    return (((Value & 0xff) << 24) | ((Value & 0xff00) << 8) | ((Value >> 8) & 0xff00) | ((Value >> 24) & 0xff));
}
//}}}

FrameParser_AudioPcm_c::FrameParser_AudioPcm_c(void)
    : FrameParser_Audio_c()
    , ParsedFrameHeader()
    , StreamParameters(NULL)
    , CurrentStreamParameters()
    , FrameParameters(NULL)
    , StreamDataValid(false)
{
    Configuration.FrameParserName               = "AudioPcm";
    Configuration.StreamParametersCount         = 2;
    Configuration.StreamParametersDescriptor    = &PcmAudioStreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &PcmAudioFrameParametersBuffer;
}

FrameParser_AudioPcm_c::~FrameParser_AudioPcm_c(void)
{
    Halt();
}

//{{{  Connect
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Connect the output port
///
FrameParserStatus_t   FrameParser_AudioPcm_c::Connect(Port_c *Port)
{
    // Clear our parameter pointers
    StreamParameters                    = NULL;
    FrameParameters                     = NULL;
    StreamDataValid                     = false;
    // Set illegal state forcing a parameter update on the first frame
    memset(&CurrentStreamParameters, 0, sizeof(CurrentStreamParameters));
    // Pass the call down the line
    return FrameParser_Audio_c::Connect(Port);
}
//}}}

//{{{  ResetReferenceFrameList
// /////////////////////////////////////////////////////////////////////////
//
//      The reset reference frame list function
//
FrameParserStatus_t   FrameParser_AudioPcm_c::ResetReferenceFrameList(void)
{
    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE,
                                              CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL);
    return FrameParserNoError;
}
//}}}
//{{{  PrepareReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Pcm audio
///
FrameParserStatus_t   FrameParser_AudioPcm_c::PrepareReferenceFrameList(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  UpdateReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Pcm audio
///
FrameParserStatus_t   FrameParser_AudioPcm_c::UpdateReferenceFrameList(void)
{
    return FrameParserNoError;
}
//}}}

//{{{  ProcessReverseDecodeStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Pcm audio
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_AudioPcm_c::ProcessReverseDecodeStack(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  ProcessReverseDecodeUnsatisfiedReferenceStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Pcm audio
///
FrameParserStatus_t   FrameParser_AudioPcm_c::ProcessReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  PurgeReverseDecodeUnsatisfiedReferenceStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Pcm audio
///
FrameParserStatus_t   FrameParser_AudioPcm_c::PurgeReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  PurgeReverseDecodeStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Pcm audio
///
FrameParserStatus_t   FrameParser_AudioPcm_c::PurgeReverseDecodeStack(void)
{
    return FrameParserNoError;
}
//}}}

//{{{  PurgeQueuedPostDecodeParameterSettings
///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for Ogg Pcm Audio.
///
/// \copydoc FrameParser_Audio_c::PurgeQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioPcm_c::PurgeQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  ProcessQueuedPostDecodeParameterSettings
///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for Ogg Pcm Audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioPcm_c::ProcessQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  GeneratePostDecodeParameterSettings
////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
///
FrameParserStatus_t   FrameParser_AudioPcm_c::GeneratePostDecodeParameterSettings(void)
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

    //SE_INFO(group_frameparser_audio, "%llx, %llx\n", ParsedFrameParameters->NormalizedPlaybackTime, ParsedFrameParameters->NativePlaybackTime);
    // Synthesize the presentation time if required
    FrameParserStatus_t Status                          = HandleCurrentFrameNormalizedPlaybackTime();
    if (Status != FrameParserNoError)
    {
        return Status;
    }

    // We can't fail after this point so this is a good time to provide a display frame index
    ParsedFrameParameters->DisplayFrameIndex             = NextDisplayFrameIndex++;
    // Use the super-class utilities to complete our housekeeping chores
    HandleUpdateStreamParameters();
    GenerateNextFrameNormalizedPlaybackTime(ParsedAudioParameters->SampleCount,  CurrentStreamParameters.SampleRate);
    return FrameParserNoError;
}
//}}}

//{{{  TestForTrickModeFrameDrop
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Pcm audio
///
FrameParserStatus_t   FrameParser_AudioPcm_c::TestForTrickModeFrameDrop(void)
{
    return FrameParserNoError;
}
//}}}

//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_AudioPcm_c::ReadHeaders(void)
{
    FrameParserStatus_t         Status;
    Bits.SetPointer(BufferData);
    // Perform the common portion of the read headers function
    FrameParser_Audio_c::ReadHeaders();

    if (!StreamDataValid)
    {
        Status                  = GetNewStreamParameters((void **)&StreamParameters);

        if (Status != FrameParserNoError)
        {
            SE_ERROR("Cannot get new stream parameters\n");
            return Status;
        }

        Status                  = ReadStreamHeader();

        if (Status != FrameParserNoError)
        {
            SE_ERROR("Failed to parse stream parameters\n");
            return Status;
        }

        memcpy(&CurrentStreamParameters, StreamParameters, sizeof(CurrentStreamParameters));
        StreamDataValid         = true;
        return Status;
    }

    Status = GetNewFrameParameters((void **)&FrameParameters);

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
    ParsedFrameParameters->SizeofFrameParameterStructure                = sizeof(PcmAudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure                      = FrameParameters;
    //FrameParameters->FrameSize                                          = CurrentStreamParameters.BlockSize0;
    //FrameParameters->BitRate                                            = (CurrentStreamParameters.BlockSize0 * CurrentStreamParameters.SampleRate * 8);
    ParsedAudioParameters->Source.BitsPerSample                         = CurrentStreamParameters.BitsPerSample;
    ParsedAudioParameters->Source.ChannelCount                          = CurrentStreamParameters.ChannelCount;
    ParsedAudioParameters->Source.SampleRateHz                          = CurrentStreamParameters.SampleRate;
    ParsedAudioParameters->SampleCount                                  = BufferLength / ((CurrentStreamParameters.BitsPerSample / 8) * CurrentStreamParameters.ChannelCount);
    ParsedFrameParameters->DataOffset                                   = 0;
    FrameToDecode                                                       = true;
    return FrameParserNoError;
}
//}}}
//{{{  ReadStreamHeader
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a FormatInfo structure
///
/// /////////////////////////////////////////////////////////////////////////

FrameParserStatus_t   FrameParser_AudioPcm_c::ReadStreamHeader(void)
{
    char                        HeaderId[8];
    FrameParserStatus_t         Status;
    unsigned int                i;
    SE_DEBUG(group_frameparser_audio, "\n");
    SE_INFO(group_frameparser_audio, "BufferLength %d :\n", BufferLength);
    unsigned int                Checksum = 0;

    for (i = 0; i < BufferLength; i++)
    {
        if ((i & 0x0f) == 0)
        {
            SE_INFO(group_frameparser_audio,  "\n%06x", i);
        }

        SE_INFO(group_frameparser_audio,  " %02x", BufferData[i]);
        Checksum       += BufferData[i];
    }

    SE_INFO(group_frameparser_audio,  "\nChecksum %08x\n", Checksum);

    if (StreamParameters == NULL)
    {
        Status = GetNewStreamParameters((void **)&StreamParameters);
        if (Status != FrameParserNoError)
        {
            return Status;
        }
    }

    memset(HeaderId, 0, sizeof(HeaderId));

    for (i = 0; i < 4; i++)
    {
        HeaderId[i]                     = Bits.Get(8);
    }

    if (strcmp(HeaderId, "fmt ") != 0)
    {
        if (strcmp(HeaderId, " tmf") != 0)
        {
            SE_ERROR("Stream is not a valid Pcm stream (%s)\n", HeaderId);
            Stream->MarkUnPlayable();
            return FrameParserError;
        }

        StreamParameters->DataEndianness        = PCM_ENDIAN_BIG;
    }
    else
    {
        StreamParameters->DataEndianness        = PCM_ENDIAN_LITTLE;
    }

    Bits.Get(32);  // header size
    StreamParameters->CompressionCode           = Bits.Get(8) | (Bits.Get(8) << 8);

    switch (StreamParameters->CompressionCode)
    {
    case PCM_COMPRESSION_CODE_PCM:
        SE_INFO(group_frameparser_audio, "compression code %d (pcm)\n", StreamParameters->CompressionCode);
        break;

    case PCM_COMPRESSION_CODE_ALAW:
        SE_INFO(group_frameparser_audio, "compression code %d (a-law)\n", StreamParameters->CompressionCode);
        break;

    case PCM_COMPRESSION_CODE_MULAW:
        SE_INFO(group_frameparser_audio, "compression code %d (mu-law)\n", StreamParameters->CompressionCode);
        break;

    default:
        SE_ERROR("Invalid compression code %d - should be %d (pcm), %d (a-law) or %d (mu-law)\n", StreamParameters->CompressionCode,
                 PCM_COMPRESSION_CODE_PCM, PCM_COMPRESSION_CODE_ALAW, PCM_COMPRESSION_CODE_MULAW);
        Stream->MarkUnPlayable();
        return FrameParserError;
    }

    StreamParameters->ChannelCount              = Bits.Get(8) | (Bits.Get(8) << 8);
    StreamParameters->SampleRate                = BE2LE(Bits.Get(32));

    if ((StreamParameters->ChannelCount == 0) || (StreamParameters->SampleRate == 0))
    {
        SE_ERROR("Invalid Pcm channel count %d or Sample Rate %d\n", StreamParameters->ChannelCount, StreamParameters->SampleRate);
        Stream->MarkUnPlayable();
        return FrameParserError;
    }

    StreamParameters->BytesPerSecond            = BE2LE(Bits.Get(32));
    StreamParameters->BlockAlign                = Bits.Get(8) | (Bits.Get(8) << 8);
    StreamParameters->BitsPerSample             = Bits.Get(8) | (Bits.Get(8) << 8);
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_audio,  "StreamFormatInfo :-\n");
    SE_INFO(group_frameparser_audio,  "DataEndianness              %6u\n", StreamParameters->DataEndianness);
    SE_INFO(group_frameparser_audio,  "CompressionCode             %6u\n", StreamParameters->CompressionCode);
    SE_INFO(group_frameparser_audio,  "ChannelCount                %6u\n", StreamParameters->ChannelCount);
    SE_INFO(group_frameparser_audio,  "SampleRate                  %6u\n", StreamParameters->SampleRate);
    SE_INFO(group_frameparser_audio,  "BytesPerSecond              %6u\n", StreamParameters->BytesPerSecond);
    SE_INFO(group_frameparser_audio,  "BlockAlign                  %6u\n", StreamParameters->BlockAlign);
    SE_INFO(group_frameparser_audio,  "BitsPerSample               %6u\n", StreamParameters->BitsPerSample);
#endif
    return FrameParserNoError;
}
//}}}

