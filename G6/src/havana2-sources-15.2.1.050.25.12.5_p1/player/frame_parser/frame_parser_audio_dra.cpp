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

#include "dra_audio.h"
#include "frame_parser_audio_dra.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_AudioDra_c"
//

static BufferDataDescriptor_t     DraAudioStreamParametersBuffer = BUFFER_DRA_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     DraAudioFrameParametersBuffer = BUFFER_DRA_AUDIO_FRAME_PARAMETERS_TYPE;

#define GET_BITS(value, bitsreq, bitunused) ((value >> (bitunused - bitsreq)) & ((1 << bitsreq) - 1))
#define UPDATE_FRAMEHEADERBYTE(BitConsumed, BitUnUsed, FrameHeader, FrameHeaderBytes, HeaderByteUsed) \
        if(BitConsumed >> 3) \
        { \
             int i; \
             BitUnUsed -= BitConsumed; \
             for(i = 0; i < (BitConsumed >> 3);i++) \
             { \
                 FrameHeader <<= 8; \
                 FrameHeader |= FrameHeaderBytes[HeaderByteUsed++]; \
                 BitUnUsed +=8; \
             } \
        } \
        else \
             BitUnUsed -= BitConsumed; \
 
//
// Sample rate lookup table for DRA audio frame parsing
//

static unsigned int DraFrequency[] =
{
    8000, 11025, 12000,  16000,
    22050, 24000, 32000,  44100,
    48000, 88200, 96000, 174600,
    192000,     0,     0,      0
};


////////////////////////////////////////////////////////////////////////////
///
/// Examine the supplied frame header and extract the information contained within.
///
/// This is a utility function shared by the frame parser and the equivalent
/// collator. Due to its shared nature this is a static method and does not access
/// any information not provided via the function arguments.
///
/// <b>DRA format</b>
///           A standard decoder needs to support general frame header only
///
/// <pre>
/// AAAAAAAAAAAAAAAA B CCCCCCCCCC DD EEEE FFF G H I J KKKKK
///
/// Sign            Length          Position         Description
///
/// A                16             (31-16)          Frame sync 0X7FFF)
/// B                 1             (15)             HEADER TYPE
///                                                     0 - GENERAL FRAME HEADER
///                                                     1 - EXTENDED FRAME HEADER
/// C               10/13           (14,5) /(14, 2)  AUDIO DATA FRAME LENGTH
///                                                     FOR GENERAL FRAME HEADER: 10 BITS
///                                                     FOR EXTENDED FRAME HEADER: 13BITS
/// D                 2             (4,3)            NO. OF BLOCKS PER FRAME
///                                                     2 ^ No_of_block_per_frame
/// E                 4             (2, 0 - 31)      SAMPLE RATE INDEX
///                                                     bits     SAMPLING FREQ
///                                                     0000     8000
///                                                     0001     11025
///                                                     0010     12000
///                                                     0011     16000
///                                                     0100     22050
///                                                     0101     24000
///                                                     0000     8000
///                                                     0001     11025
///                                                     0010     12000
///                                                     0011     16000
///                                                     0100     22050
///                                                     0101     24000
///                                                     0110     32000
///                                                     0111     44100
///                                                     1000     48000
///                                                     1001     88200
///                                                     1010     96000
///                                                     1011     174600
///                                                     1100     192000
///                                                     1101     RESERVED
///                                                     1110     RESERVED
///                                                     1111     RESERVED
///
/// F                3              (30,28)          NORMAL NO. OF CHANNELS
///                                                     CHANNEL FROM 1 - 8
/// G                1              (27)             NO. OF LFE CHANNEL
/// H                1              (26)             AUX DATA INFO
///                                                     0 - NOT PRESENT
///                                                     1 - PRESENT
/// I                1              (25)             SUM/DIFF CODING IS USED IN FRAME
///                                                     0 - NOT PRESENT
///                                                     1 - PRESENT
/// J                1              (24)             JOINT INTENSITY CODING
///                                                     0 - NOT PRESENT
///                                                     1 - PRESENT
/// </pre>
///
/// \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t FrameParser_AudioDra_c::ParseFrameHeader(unsigned char *FrameHeaderBytes, DraAudioParsedFrameHeader_t *ParsedFrameHeader)
{
    unsigned int    FrameHeader;
    unsigned int    HeaderByteUsed = 0;
    unsigned char   BitUnused = 32;
    unsigned char   HeaderType = 0;         // General Frame Header or Extension Frame Header
    unsigned short  Audio_FrameSize;        // 10 bits for gen frame header and 13 bit for Ext frame haeder
    unsigned char   MDCT_BLKPerFrame;
    unsigned int    PCMSamplePerFrame;
    unsigned char   SampleRateIndex;
    unsigned short  Num_NormalChannel;
    unsigned char   Num_LfeChannel;
    unsigned char   AuxDataInfoPresent;
    unsigned char   SumDiffUsedinFrame = 0;
    unsigned char   JointIntensityCodPresent = 0;
    unsigned int    SamplingFrequency;

    FrameHeader = FrameHeaderBytes[HeaderByteUsed] << 24 | FrameHeaderBytes[HeaderByteUsed + 1] << 16 |
                  FrameHeaderBytes[HeaderByteUsed + 2] <<  8 | FrameHeaderBytes[HeaderByteUsed + 3];
    HeaderByteUsed += 4;

    if (GET_BITS(FrameHeader, DRA_START_SYNC, BitUnused) != DRA_START_SYNC_WORD)
    {
        SE_ERROR("Invalid start code %x\n", GET_BITS(FrameHeader, DRA_START_SYNC, BitUnused));
        return FrameParserError;
    }

    UPDATE_FRAMEHEADERBYTE(DRA_START_SYNC, BitUnused, FrameHeader,
                           FrameHeaderBytes, HeaderByteUsed);

    switch (GET_BITS(FrameHeader, DRA_HEADER_FRAME_TYPE_MASK, BitUnused))
    {
    case DRA_ES_FRAME_HEADER_GEN:
        HeaderType = 0;
        break;

    case DRA_ES_FRAME_HEADER_EXT:
        HeaderType = 1;
        break;
    }

    UPDATE_FRAMEHEADERBYTE(DRA_HEADER_FRAME_TYPE_MASK, BitUnused, FrameHeader,
                           FrameHeaderBytes, HeaderByteUsed);

    if (HeaderType)
    {
        Audio_FrameSize = GET_BITS(FrameHeader, DRA_EXT_HDR_AUDIO_FRAME_LEN, BitUnused);
        UPDATE_FRAMEHEADERBYTE(DRA_EXT_HDR_AUDIO_FRAME_LEN, BitUnused, FrameHeader,
                               FrameHeaderBytes, HeaderByteUsed);
    }
    else
    {
        Audio_FrameSize = GET_BITS(FrameHeader, DRA_GEN_HDR_AUDIO_FRAME_LEN, BitUnused);
        UPDATE_FRAMEHEADERBYTE(DRA_GEN_HDR_AUDIO_FRAME_LEN, BitUnused, FrameHeader,
                               FrameHeaderBytes, HeaderByteUsed);
    }

    MDCT_BLKPerFrame = GET_BITS(FrameHeader, DRA_NO_OF_BLOCKS_PER_FRAME, BitUnused);
    UPDATE_FRAMEHEADERBYTE(DRA_NO_OF_BLOCKS_PER_FRAME, BitUnused, FrameHeader,
                           FrameHeaderBytes, HeaderByteUsed);
    // For one short window MDCT block contains 128 PCM audio samples
    //, the number of audio PCM samples in the frame is 128*nNumBlocksPerFrm.
    PCMSamplePerFrame = 1 << (7 + MDCT_BLKPerFrame);
    MDCT_BLKPerFrame  = 1 << MDCT_BLKPerFrame;
    SampleRateIndex = GET_BITS(FrameHeader, DRA_SAMPLE_RATE_INDEX, BitUnused);
    UPDATE_FRAMEHEADERBYTE(DRA_SAMPLE_RATE_INDEX, BitUnused, FrameHeader,
                           FrameHeaderBytes, HeaderByteUsed);

    if (HeaderType)
    {
        Num_NormalChannel = GET_BITS(FrameHeader, DRA_NUM_OF_CHANNEL_FOR_EXT_HDR, BitUnused);
        UPDATE_FRAMEHEADERBYTE(DRA_NUM_OF_CHANNEL_FOR_EXT_HDR, BitUnused, FrameHeader,
                               FrameHeaderBytes, HeaderByteUsed);
        Num_LfeChannel = GET_BITS(FrameHeader, DRA_NUM_OF_LFE_FOR_EXT_HDR, BitUnused);
        UPDATE_FRAMEHEADERBYTE(DRA_NUM_OF_LFE_FOR_EXT_HDR, BitUnused, FrameHeader,
                               FrameHeaderBytes, HeaderByteUsed);
    }
    else
    {
        Num_NormalChannel = GET_BITS(FrameHeader, DRA_NUM_OF_CHANNEL_FOR_GEN_HDR, BitUnused);
        UPDATE_FRAMEHEADERBYTE(DRA_NUM_OF_CHANNEL_FOR_GEN_HDR, BitUnused, FrameHeader,
                               FrameHeaderBytes, HeaderByteUsed);
        Num_LfeChannel = GET_BITS(FrameHeader, DRA_NUM_OF_LFE_FOR_GEN_HDR, BitUnused);
        UPDATE_FRAMEHEADERBYTE(DRA_NUM_OF_LFE_FOR_GEN_HDR, BitUnused, FrameHeader,
                               FrameHeaderBytes, HeaderByteUsed);
    }

    AuxDataInfoPresent = GET_BITS(FrameHeader, DRA_AUX_DATA_INFO, BitUnused);
    UPDATE_FRAMEHEADERBYTE(DRA_AUX_DATA_INFO, BitUnused, FrameHeader,
                           FrameHeaderBytes, HeaderByteUsed);

    if (HeaderType == DRA_ES_FRAME_HEADER_GEN)
    {
        SumDiffUsedinFrame = GET_BITS(FrameHeader, DRA_SUM_DIFF_CODING_FOR_GEN_HDR, BitUnused);
        UPDATE_FRAMEHEADERBYTE(DRA_SUM_DIFF_CODING_FOR_GEN_HDR, BitUnused, FrameHeader,
                               FrameHeaderBytes, HeaderByteUsed);
        JointIntensityCodPresent = GET_BITS(FrameHeader, DRA_JOINT_INTS_CODING_FOR_GEN_HDR, BitUnused);
        UPDATE_FRAMEHEADERBYTE(DRA_JOINT_INTS_CODING_FOR_GEN_HDR, BitUnused, FrameHeader,
                               FrameHeaderBytes, HeaderByteUsed);
    }

    SamplingFrequency = DraFrequency[SampleRateIndex];

    if (SamplingFrequency == 0)
    {
        SE_ERROR("Invalid frequency, %x, %x\n",
                 FrameHeader, SamplingFrequency);
        return FrameParserError;
    }

    SE_DEBUG(group_frameparser_audio,  "Header %8x, Frequency %5d, FrameSize %d\n",
             FrameHeader, SamplingFrequency, PCMSamplePerFrame);
    ParsedFrameHeader->Header                   = DRA_START_SYNC_WORD;
    ParsedFrameHeader->HeaderType               = HeaderType;
    ParsedFrameHeader->Audio_FrameSize          = Audio_FrameSize << 2;
    ParsedFrameHeader->MDCT_BLKPerFrame         = MDCT_BLKPerFrame;
    ParsedFrameHeader->PCMSamplePerFrame        = PCMSamplePerFrame;
    ParsedFrameHeader->SampleRateIndex          = SampleRateIndex;
    ParsedFrameHeader->Num_NormalChannel        = Num_NormalChannel;
    ParsedFrameHeader->Num_LfeChannel           = Num_LfeChannel;
    ParsedFrameHeader->AuxDataInfoPresent       = AuxDataInfoPresent;
    ParsedFrameHeader->SumDiffUsedinFrame       = SumDiffUsedinFrame;
    ParsedFrameHeader->JointIntensityCodPresent = JointIntensityCodPresent;
    ParsedFrameHeader->SamplingFrequency        = SamplingFrequency;
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     Constructor
///
FrameParser_AudioDra_c::FrameParser_AudioDra_c(void)
    : FrameParser_Audio_c()
    , ParsedFrameHeader()
    , StreamParameters(NULL)
    , CurrentStreamParameters()
    , FrameParameters(NULL)
{
    Configuration.FrameParserName               = "AudioDra";
    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &DraAudioStreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &DraAudioFrameParametersBuffer;
}

////////////////////////////////////////////////////////////////////////////
///
///     Destructor
///
FrameParser_AudioDra_c::~FrameParser_AudioDra_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
///     Method to connect to neighbor
///
FrameParserStatus_t   FrameParser_AudioDra_c::Connect(Port_c *Port)
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
    //
    // Pass the call down the line
    //
    return FrameParser_Audio_c::Connect(Port);
}


////////////////////////////////////////////////////////////////////////////
///
/// Parse the frame header and store the results for when we emit the frame.
///
FrameParserStatus_t   FrameParser_AudioDra_c::ReadHeaders(void)
{
    //
    // Perform the common portion of the read headers function
    //
    FrameParser_Audio_c::ReadHeaders();

    FrameParserStatus_t Status = ParseFrameHeader(BufferData, &ParsedFrameHeader);
    if (Status != FrameParserNoError)
    {
        SE_ERROR("Failed to parse frame header, bad collator selected?\n");
        return Status;
    }

    FrameToDecode = true;

    if (CurrentStreamParameters.Frequency != ParsedFrameHeader.SamplingFrequency)
    {
        Status = GetNewStreamParameters((void **) &StreamParameters);
        if (Status != FrameParserNoError)
        {
            SE_ERROR("Cannot get new stream parameters\n");
            return Status;
        }

        StreamParameters->Frequency = CurrentStreamParameters.Frequency = ParsedFrameHeader.SamplingFrequency;
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
    ParsedFrameParameters->NewFrameParameters            = true;
    ParsedFrameParameters->SizeofFrameParameterStructure = sizeof(DraAudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure       = FrameParameters;
    FrameParameters->FrameSize = ParsedFrameHeader.Audio_FrameSize;
    ParsedAudioParameters->Source.BitsPerSample = 0; // filled in by codec
    ParsedAudioParameters->Source.ChannelCount  = 0;  // filled in by codec
    ParsedAudioParameters->Source.SampleRateHz  = ParsedFrameHeader.SamplingFrequency;
    ParsedAudioParameters->SampleCount          = ParsedFrameHeader.PCMSamplePerFrame;
    ParsedAudioParameters->Organisation         = 0; // filled in by codec
    Stream->Statistics().FrameParserAudioSampleRate = ParsedFrameHeader.SamplingFrequency;
    Stream->Statistics().FrameParserAudioFrameSize  = ParsedFrameHeader.Audio_FrameSize;
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     The reset reference frame list function
///
FrameParserStatus_t   FrameParser_AudioDra_c::ResetReferenceFrameList(void)
{
    SE_DEBUG(group_frameparser_audio, ">><<");
    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL);
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DRA audio.
///
///
FrameParserStatus_t   FrameParser_AudioDra_c::PurgeQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DRA audio.
///
///
FrameParserStatus_t   FrameParser_AudioDra_c::ProcessQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
/// For DRA audio these can be determined immediately (although it the first
/// frame for decode does not contain a PTS we must synthesize one).
///
FrameParserStatus_t   FrameParser_AudioDra_c::GeneratePostDecodeParameterSettings(void)
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
    GenerateNextFrameNormalizedPlaybackTime(ParsedFrameHeader.PCMSamplePerFrame,
                                            ParsedFrameHeader.SamplingFrequency);
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DRA audio.
///
FrameParserStatus_t   FrameParser_AudioDra_c::PrepareReferenceFrameList(void)
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DRA audio.
///
FrameParserStatus_t   FrameParser_AudioDra_c::UpdateReferenceFrameList(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DRA audio.
///
FrameParserStatus_t   FrameParser_AudioDra_c::ProcessReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DRA audio.
///
FrameParserStatus_t   FrameParser_AudioDra_c::ProcessReverseDecodeStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DRA audio.
///
FrameParserStatus_t   FrameParser_AudioDra_c::PurgeReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DRA audio.
///
FrameParserStatus_t   FrameParser_AudioDra_c::PurgeReverseDecodeStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DRA audio.
///
FrameParserStatus_t   FrameParser_AudioDra_c::TestForTrickModeFrameDrop(void)
{
    return FrameParserNoError;
}








