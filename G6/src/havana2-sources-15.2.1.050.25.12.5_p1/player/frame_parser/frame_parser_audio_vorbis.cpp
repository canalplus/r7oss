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
#include "frame_parser_audio_vorbis.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_AudioVorbis_c"

static BufferDataDescriptor_t     VorbisAudioStreamParametersBuffer       = BUFFER_VORBIS_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     VorbisAudioFrameParametersBuffer        = BUFFER_VORBIS_AUDIO_FRAME_PARAMETERS_TYPE;

#define CodeToInteger(a,b,c,d)          ((a << 0) | (b << 8) | (c << 16) | (d << 24))

static inline unsigned int BE2LE(unsigned int Value)
{
    return (((Value & 0xff) << 24) | ((Value & 0xff00) << 8) | ((Value >> 8) & 0xff00) | ((Value >> 24) & 0xff));
}
//}}}

FrameParser_AudioVorbis_c::FrameParser_AudioVorbis_c(void)
    : FrameParser_Audio_c()
    , ParsedFrameHeader()
    , StreamParameters(NULL)
    , CurrentStreamParameters()
    , FrameParameters(NULL)
    , StreamHeadersRead(VorbisNoHeaders)
{
    Configuration.FrameParserName               = "AudioVorbis";
    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &VorbisAudioStreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &VorbisAudioFrameParametersBuffer;
}

FrameParser_AudioVorbis_c::~FrameParser_AudioVorbis_c(void)
{
    Halt();
}

//{{{  Connect
///
///
FrameParserStatus_t   FrameParser_AudioVorbis_c::Connect(Port_c *Port)
{
    // Clear our parameter pointers
    StreamParameters                    = NULL;
    FrameParameters                     = NULL;
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
FrameParserStatus_t   FrameParser_AudioVorbis_c::ResetReferenceFrameList(void)
{
    SE_DEBUG(group_frameparser_audio, ">><<");
    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE,
                                              CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL);
    return FrameParserNoError;
}
//}}}
//{{{  PrepareReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Vorbis audio
///
FrameParserStatus_t   FrameParser_AudioVorbis_c::PrepareReferenceFrameList(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  UpdateReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Vorbis audio
///
FrameParserStatus_t   FrameParser_AudioVorbis_c::UpdateReferenceFrameList(void)
{
    return FrameParserNoError;
}
//}}}

//{{{  ProcessReverseDecodeStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Vorbis audio
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_AudioVorbis_c::ProcessReverseDecodeStack(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  ProcessReverseDecodeUnsatisfiedReferenceStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Vorbis audio
///
FrameParserStatus_t   FrameParser_AudioVorbis_c::ProcessReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  PurgeReverseDecodeUnsatisfiedReferenceStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Vorbis audio
///
FrameParserStatus_t   FrameParser_AudioVorbis_c::PurgeReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  PurgeReverseDecodeStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Vorbis audio
///
FrameParserStatus_t   FrameParser_AudioVorbis_c::PurgeReverseDecodeStack(void)
{
    return FrameParserNoError;
}
//}}}

//{{{  PurgeQueuedPostDecodeParameterSettings
///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for Ogg Vorbis Audio.
///
/// \copydoc FrameParser_Audio_c::PurgeQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioVorbis_c::PurgeQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  ProcessQueuedPostDecodeParameterSettings
///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for Ogg Vorbis Audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioVorbis_c::ProcessQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  GeneratePostDecodeParameterSettings
////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
/// For Ogg Vorbis Audio these can be determined immediately (although it the first
/// frame for decode does not contain a PTS we must synthesize one).
///
FrameParserStatus_t   FrameParser_AudioVorbis_c::GeneratePostDecodeParameterSettings(void)
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

    // SE_INFO(group_frameparser_audio, "%llx, %llx\n", ParsedFrameParameters->NormalizedPlaybackTime, ParsedFrameParameters->NativePlaybackTime);
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
    //GenerateNextFrameNormalizedPlaybackTime (CurrentStreamParameters.SamplesPerFrame,  CurrentStreamParameters.SampleRate);

    return FrameParserNoError;
}
//}}}

//{{{  TestForTrickModeFrameDrop
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Vorbis audio
///
FrameParserStatus_t   FrameParser_AudioVorbis_c::TestForTrickModeFrameDrop(void)
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
FrameParserStatus_t   FrameParser_AudioVorbis_c::ReadHeaders(void)
{
    Bits.SetPointer(BufferData);

    // Perform the common portion of the read headers function
    FrameParser_Audio_c::ReadHeaders();
    FrameParserStatus_t Status = GetNewFrameParameters((void **)&FrameParameters);
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
    ParsedFrameParameters->SizeofFrameParameterStructure                = sizeof(VorbisAudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure                      = FrameParameters;

    // If all header information has not been read assume this is a header frame.
    if (StreamHeadersRead != VorbisAllHeaders)
    {
        return ReadStreamHeaders();
    }

    FrameParameters->FrameSize                                          = CurrentStreamParameters.BlockSize0;
    FrameParameters->BitRate                                            = (CurrentStreamParameters.BlockSize0 * CurrentStreamParameters.SampleRate * 8);
    FrameParameters->SamplesPresent                                     = true;
    ParsedAudioParameters->Source.BitsPerSample                         = CurrentStreamParameters.SampleSize;
    ParsedAudioParameters->Source.ChannelCount                          = CurrentStreamParameters.ChannelCount;
    ParsedAudioParameters->Source.SampleRateHz                          = CurrentStreamParameters.SampleRate;
    ParsedAudioParameters->SampleCount                                  = CurrentStreamParameters.SamplesPerFrame;
    ParsedFrameParameters->DataOffset                                   = 0;
    FrameToDecode                                                       = true;
    Stream->Statistics().FrameParserAudioSampleRate                       = CurrentStreamParameters.SampleRate;
    Stream->Statistics().FrameParserAudioFrameSize                        = CurrentStreamParameters.BlockSize0;

    return FrameParserNoError;
}
//}}}
//{{{  ReadStreamHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a FormatInfo structure
///
/// /////////////////////////////////////////////////////////////////////////

FrameParserStatus_t   FrameParser_AudioVorbis_c::ReadStreamHeaders(void)
{
    unsigned int                HeaderType;
    char                        HeaderName[8];
    FrameParserStatus_t         Status;
    unsigned int                i;
    SE_INFO(group_frameparser_audio, "BufferLength %d :\n", BufferLength);

    if (StreamParameters == NULL)
    {
        Status                                          = GetNewStreamParameters((void **)&StreamParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }
    }

    HeaderType                                          = Bits.Get(8);
    memset(HeaderName, 0, sizeof(HeaderName));

    for (i = 0; i < 6; i++)
    {
        HeaderName[i]                                   = Bits.Get(8);
    }

    if (strcmp(HeaderName, "vorbis") != 0)
    {
        SE_ERROR("Stream is not a valid Vorbis stream\n");
        Stream->MarkUnPlayable();
        return FrameParserError;
    }

    if (HeaderType == VORBIS_IDENTIFICATION_HEADER)
    {
        unsigned int    BlockSize;
        unsigned int    FramingFlag;
        StreamParameters->VorbisVersion                 = Bits.Get(32);

        if (StreamParameters->VorbisVersion != 0)
        {
            SE_ERROR("Invalid Vorbis version numbers should be 0 is %x\n", StreamParameters->VorbisVersion);
            Stream->MarkUnPlayable();
            return FrameParserError;
        }

        StreamParameters->ChannelCount                  = Bits.Get(8);
        StreamParameters->SampleRate                    = BE2LE(Bits.Get(32));

        if ((StreamParameters->ChannelCount == 0) || (StreamParameters->SampleRate == 0))
        {
            SE_ERROR("Invalid Vorbis channel count %d or Sample Rate %d\n",
                     StreamParameters->ChannelCount,
                     StreamParameters->SampleRate);
            Stream->MarkUnPlayable();
            return FrameParserError;
        }

        Bits.FlushUnseen(32);
        Bits.FlushUnseen(32);
        Bits.FlushUnseen(32);
        BlockSize                                       = Bits.Get(8);
        StreamParameters->BlockSize0                    = 1 << (BlockSize & 0x0f);
        StreamParameters->BlockSize1                    = 1 << ((BlockSize & 0xf0) >> 4);
        FramingFlag                                     = Bits.Get(8) & 0x01;

        if (FramingFlag != 1)
        {
            SE_ERROR("Invalid Vorbis framing flag %d\n", FramingFlag);
            Stream->MarkUnPlayable();
            return FrameParserError;
        }

        StreamHeadersRead                              |= VorbisIdentificationHeader;
    }
    else if (HeaderType == VORBIS_COMMENT_HEADER)
    {
        StreamHeadersRead                              |= VorbisCommentHeader;
    }
    else if (HeaderType == VORBIS_SETUP_HEADER)
    {
        StreamHeadersRead                              |= VorbisSetupHeader;
    }
    else
    {
        SE_ERROR("Unrecognised header (Not Identification, Comment or Setup) (%x)\n", HeaderType);
        Stream->MarkUnPlayable();
        return FrameParserError;
    }

    // Fill in frame details so codec knows this is a header only frame
    FrameParameters->FrameSize                                          = 0;
    FrameParameters->BitRate                                            = 0;
    FrameParameters->SamplesPresent                                     = false;
    FrameToDecode                                                       = true;
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_audio,  "StreamFormatInfo :-\n");
    SE_INFO(group_frameparser_audio,  "SampleRate                  %6u\n", StreamParameters->SampleRate);
    SE_INFO(group_frameparser_audio,  "SampleSize                  %6u\n", StreamParameters->SampleSize);
    SE_INFO(group_frameparser_audio,  "ChannelCount                %6u\n", StreamParameters->ChannelCount);
    SE_INFO(group_frameparser_audio,  "SamplesPerFrame             %6u\n", StreamParameters->SamplesPerFrame);
    SE_INFO(group_frameparser_audio,  "BlockSize0                  %6u\n", StreamParameters->BlockSize0);
    SE_INFO(group_frameparser_audio,  "BlockSize1                  %6u\n", StreamParameters->BlockSize1);
#endif
    return FrameParserNoError;
}
//}}}

