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
#include "frame_parser_audio_rma.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_AudioRma_c"
//#define DUMP_HEADERS

static BufferDataDescriptor_t     RmaAudioStreamParametersBuffer       = BUFFER_RMA_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     RmaAudioFrameParametersBuffer        = BUFFER_RMA_AUDIO_FRAME_PARAMETERS_TYPE;

#define CodeToInteger(a,b,c,d)          ((a << 0) | (b << 8) | (c << 16) | (d << 24))

static inline unsigned int BE2LE(unsigned int Value)
{
    return (((Value & 0xff) << 24) | ((Value & 0xff00) << 8) | ((Value >> 8) & 0xff00) | ((Value >> 24) & 0xff));
}
//}}}

FrameParser_AudioRma_c::FrameParser_AudioRma_c(void)
    : FrameParser_Audio_c()
    , ParsedFrameHeader()
    , StreamParameters(NULL)
    , CurrentStreamParameters()
    , FrameParameters(NULL)
    , StreamFormatInfoValid(false)
    , DataOffset(0)
{
    Configuration.FrameParserName               = "AudioRma";
    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &RmaAudioStreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &RmaAudioFrameParametersBuffer;
    StreamFormatInfoValid                       = false;
}

FrameParser_AudioRma_c::~FrameParser_AudioRma_c(void)
{
    Halt();
}

//{{{  Connect
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Connect the output port
///
FrameParserStatus_t   FrameParser_AudioRma_c::Connect(Port_c *Port)
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
FrameParserStatus_t   FrameParser_AudioRma_c::ResetReferenceFrameList(void)
{
    SE_DEBUG(group_frameparser_audio, ">><<");
    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL);
    return FrameParserNoError;
}
//}}}
//{{{  PrepareReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::PrepareReferenceFrameList(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  UpdateReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::UpdateReferenceFrameList(void)
{
    return FrameParserNoError;
}
//}}}

//{{{  ProcessReverseDecodeStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_AudioRma_c::ProcessReverseDecodeStack(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  ProcessReverseDecodeUnsatisfiedReferenceStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::ProcessReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  PurgeReverseDecodeUnsatisfiedReferenceStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::PurgeReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  PurgeReverseDecodeStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::PurgeReverseDecodeStack(void)
{
    return FrameParserNoError;
}
//}}}

//{{{  PurgeQueuedPostDecodeParameterSettings
///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for Real Audio.
///
/// \copydoc FrameParser_Audio_c::PurgeQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioRma_c::PurgeQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  ProcessQueuedPostDecodeParameterSettings
///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for Real Audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioRma_c::ProcessQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}
//}}}
//{{{  GeneratePostDecodeParameterSettings
////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
/// For Real Audio these can be determined immediately (although it the first
/// frame for decode does not contain a PTS we must synthesize one).
///
FrameParserStatus_t   FrameParser_AudioRma_c::GeneratePostDecodeParameterSettings(void)
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
    GenerateNextFrameNormalizedPlaybackTime(CurrentStreamParameters.SamplesPerFrame,  CurrentStreamParameters.SampleRate);
    return FrameParserNoError;
}
//}}}

//{{{  TestForTrickModeFrameDrop
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::TestForTrickModeFrameDrop(void)
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
FrameParserStatus_t   FrameParser_AudioRma_c::ReadHeaders(void)
{
    FrameParserStatus_t         Status;
    Bits.SetPointer(BufferData);
    DataOffset                  = 0;
    // Perform the common portion of the read headers function
    FrameParser_Audio_c::ReadHeaders();

    if (!StreamFormatInfoValid)
    {
        Status                  = GetNewStreamParameters((void **)&StreamParameters);

        if (Status != FrameParserNoError)
        {
            SE_ERROR("Cannot get new stream parameters\n");
            return Status;
        }

        Status                  = ReadStreamParameters();

        if (Status != FrameParserNoError)
        {
            SE_ERROR("Failed to parse stream parameters\n");
            return Status;
        }

        memcpy(&CurrentStreamParameters, StreamParameters, sizeof(CurrentStreamParameters));
        StreamFormatInfoValid   = true;
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
    ParsedFrameParameters->SizeofFrameParameterStructure                = sizeof(RmaAudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure                      = FrameParameters;
    FrameParameters->FrameSize                                          = CurrentStreamParameters.SubPacketSize;
    FrameParameters->BitRate                                            = (CurrentStreamParameters.SubPacketSize * CurrentStreamParameters.SampleRate * 8) /
                                                                          CurrentStreamParameters.SamplesPerFrame;
    ParsedAudioParameters->Source.BitsPerSample                         = CurrentStreamParameters.SampleSize;
    ParsedAudioParameters->Source.ChannelCount                          = CurrentStreamParameters.ChannelCount;
    ParsedAudioParameters->Source.SampleRateHz                          = CurrentStreamParameters.SampleRate;
    ParsedAudioParameters->SampleCount                                  = CurrentStreamParameters.SamplesPerFrame;
    ParsedFrameParameters->DataOffset                                   = DataOffset;
    FrameToDecode                                                       = true;
    return FrameParserNoError;
}
//}}}
//{{{  ReadStreamParameters
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a FormatInfo structure
///
/// /////////////////////////////////////////////////////////////////////////

FrameParserStatus_t   FrameParser_AudioRma_c::ReadStreamParameters(void)
{
    unsigned char              *StartPointer;
    unsigned char              *EndPointer;
    unsigned int                BitsInByte;
    SE_DEBUG(group_frameparser_audio, "\n");
    Bits.GetPosition(&StartPointer, &BitsInByte);
    StreamParameters->Length                    = Bits.Get(32);
    StreamParameters->HeaderSignature           = BE2LE(Bits.Get(32));
    StreamParameters->Version                   = Bits.Get(16);

    if (StreamParameters->Version == 3)
    {
        StreamParameters->HeaderSize            = Bits.Get(32);
    }
    else
    {
        Bits.Get(16);  // unused
        StreamParameters->RaSignature           = BE2LE(Bits.Get(32));
        StreamParameters->Size                  = Bits.Get(32);
        StreamParameters->Version2              = Bits.Get(16);
        StreamParameters->HeaderSize            = Bits.Get(32);
        StreamParameters->CodecFlavour          = Bits.Get(16);
        StreamParameters->CodedFrameSize        = Bits.Get(32);
        Bits.Get(32);
        Bits.Get(32);
        Bits.Get(32);
        StreamParameters->SubPacket             = Bits.Get(16);
        StreamParameters->FrameSize             = Bits.Get(16);
        StreamParameters->SubPacketSize         = Bits.Get(16);
        Bits.Get(16);

        if (StreamParameters->Version == 5)
        {
            Bits.Get(32);
            Bits.Get(16);
        }

        StreamParameters->SampleRate            = Bits.Get(16);
        Bits.Get(16);
        StreamParameters->SampleSize            = Bits.Get(16);
        StreamParameters->ChannelCount          = Bits.Get(16);

        if (StreamParameters->Version == 4)
        {
            Bits.Get(8);
            StreamParameters->InterleaverId     = BE2LE(Bits.Get(32));
            Bits.Get(8);
            StreamParameters->CodecId           = BE2LE(Bits.Get(32));
        }
        else
        {
            StreamParameters->InterleaverId     = BE2LE(Bits.Get(32));
            StreamParameters->CodecId           = BE2LE(Bits.Get(32));
        }

        Bits.Get(24);

        if (StreamParameters->Version == 5)
        {
            Bits.Get(8);
        }

        StreamParameters->CodecOpaqueDataLength  = Bits.Get(32);
        Bits.GetPosition(&EndPointer, &BitsInByte);
        DataOffset                               = (unsigned int)EndPointer - (unsigned int)StartPointer;

        switch (StreamParameters->CodecId)
        {
        case CodeToInteger('d', 'n', 'e', 't'):             /* AC3 */
            break;

        case CodeToInteger('s', 'i', 'p', 'r'):             /* Sipro */
            break;

        case CodeToInteger('2', '8', '_', '8'):             /* Real Audio 2.0 */
            break;

        case CodeToInteger('c', 'o', 'o', 'k'):             /* Rma */
        case CodeToInteger('a', 't', 'r', 'c'):             /* ATRAC */
        {
            StreamParameters->RmaVersion            = Bits.Get(32);
            StreamParameters->SamplesPerFrame       = Bits.Get(16) / StreamParameters->ChannelCount;
            break;
        }

        case CodeToInteger('r', 'a', 'a', 'c'):             /* LC-AAC */
        case CodeToInteger('r', 'a', 'c', 'p'):             /* HE-AAC */
            break;

        case CodeToInteger('r', 'a', 'l', 'f'):             /* Real Audio Lossless */
            break;
        }
    }

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_audio,  "StreamFormatInfo :-\n");
    SE_INFO(group_frameparser_audio,  "Length                      %6u\n", StreamParameters->Length);
    SE_INFO(group_frameparser_audio,  "HeaderSignature               %.4s\n", (unsigned char *)&StreamParameters->HeaderSignature);
    SE_INFO(group_frameparser_audio,  "Version                     %6u\n", StreamParameters->Version);
    SE_INFO(group_frameparser_audio,  "RaSignature                   %.4s\n", (unsigned char *)&StreamParameters->RaSignature);
    SE_INFO(group_frameparser_audio,  "Size                        %6u\n", StreamParameters->Size);
    SE_INFO(group_frameparser_audio,  "Version2                    %6u\n", StreamParameters->Version2);
    SE_INFO(group_frameparser_audio,  "HeaderSize                  %6u\n", StreamParameters->HeaderSize);
    SE_INFO(group_frameparser_audio,  "CodecFlavour                %6u\n", StreamParameters->CodecFlavour);
    SE_INFO(group_frameparser_audio,  "CodedFrameSize              %6u\n", StreamParameters->CodedFrameSize);
    SE_INFO(group_frameparser_audio,  "SubPacket                   %6u\n", StreamParameters->SubPacket);
    SE_INFO(group_frameparser_audio,  "FrameSize                   %6u\n", StreamParameters->FrameSize);
    SE_INFO(group_frameparser_audio,  "SubPacketSize               %6u\n", StreamParameters->SubPacketSize);
    SE_INFO(group_frameparser_audio,  "SampleRate                  %6u\n", StreamParameters->SampleRate);
    SE_INFO(group_frameparser_audio,  "SampleSize                  %6u\n", StreamParameters->SampleSize);
    SE_INFO(group_frameparser_audio,  "ChannelCount                %6u\n", StreamParameters->ChannelCount);
    SE_INFO(group_frameparser_audio,  "InterleaverId                 %.4s\n", (unsigned char *)&StreamParameters->InterleaverId);
    SE_INFO(group_frameparser_audio,  "CodecId                       %.4s\n", (unsigned char *)&StreamParameters->CodecId);
    SE_INFO(group_frameparser_audio,  "CodecOpaqueDataLength       %6u\n", StreamParameters->CodecOpaqueDataLength);
    SE_INFO(group_frameparser_audio,  "DataOffset                  %6u\n", DataOffset);
    SE_INFO(group_frameparser_audio,  "RmaVersion             0x%08x\n", StreamParameters->RmaVersion);
    SE_INFO(group_frameparser_audio,  "SamplesPerFrame             %6u\n", StreamParameters->SamplesPerFrame);
#endif
    return FrameParserNoError;
}
//}}}

