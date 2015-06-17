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
/// \class FrameParser_AudioAac_c
/// \brief Frame parser for AAC audio.
///

#include "aac_audio.h"
#include "frame_parser_audio_aac.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_AudioAac_c"

const unsigned int UnplayabilityThreshold = 4;

static BufferDataDescriptor_t     AacAudioStreamParametersBuffer = BUFFER_AAC_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     AacAudioFrameParametersBuffer = BUFFER_AAC_AUDIO_FRAME_PARAMETERS_TYPE;

//
// Sample rate lookup table for AAC audio frame parsing
//

#define AAC_MAX_SAMPLE_RATE_IDX     12
#define AAC_MAX_ISO_SAMPLE_RATE_IDX 11

static int aac_sample_rates[AAC_MAX_SAMPLE_RATE_IDX + 1] =
{
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

#define AAC_NB_CHANNELS_IDX 7
static const unsigned int aac_nb_channels[AAC_NB_CHANNELS_IDX] =
{
    1, // mono
    2, // stereo
    3, // C+L+R
    4, // C+L+R+rear surround
    5, // 5.0
    6, // 5.1
    8, // 7.1
};


const char *FrameTypeName[] =
{
    "ADTS",
    "ADIF",
    "MP4",
    "LOAS",
    "RAW"
};

#define FRAME_COND_ERROR(fmt, args...) do { if ( Action != AAC_GET_SYNCHRO ) { SE_ERROR(fmt, ##args); } } while (0);

////////////////////////////////////////////////////////////////////////////
///
/// Examine the supplied frame header and extract the information contained within.
///
/// This is a utility function shared by the frame parser and the equivalent
/// collator. Due to its shared nature this is a static method and does not access
/// any information not provided via the function arguments.
///
/// ** AAC format **
///
/// AAAAAAAA AAAABCCD EEFFFFGH HHIJKLMM
/// MMMMMMMM MMMNNNNN NNNNNNOO ........
///
/// Sign            Length          Position         Description
///
/// A                12             (31-20)          Sync code
/// B                 1              (19)            ID
/// C                 2             (18-17)          layer
/// D                 1              (16)            protect absent
/// E                 2             (15-14)          profile
/// F                 4             (13-10)          sample freq index
/// G                 1              (9)             private
/// H                 3             (8-6)            channel config
/// I                 1              (5)             original/copy
/// J                 1              (4)             home
/// K                 1              (3)             copyright id
/// L                 1              (2)             copyright start
/// M                 13         (1-0,31-21)         frame length
/// N                 11           (20-10)           adts buffer fullness
/// O                 2             (9-8)            num of raw data blocks in frame
///
/////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - parse a frame header (NOTE do we already know it has a valid sync word?).
//
///
/// \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t FrameParser_AudioAac_c::ParseFrameHeader(unsigned char *FrameHeaderBytes,
                                                             AacAudioParsedFrameHeader_t *ParsedFrameHeader,
                                                             int AvailableBytes,
                                                             AacFrameParsingPurpose_t Action,
                                                             bool EnableHeaderUnplayableErrors)
{
    unsigned int    SamplingFrequency = 0;
    unsigned int    SampleCount = 0;
    bool            ExpliciteSbrExtension = false;
    unsigned int    FrameSize = 0;
    BitStreamClass_c    Bits;
    eAacFormatType Type;
    Bits.SetPointer(FrameHeaderBytes);
    unsigned int Sync = Bits.Get(11);

    if (Sync == AAC_AUDIO_LOAS_ASS_SYNC_WORD)
    {
        Type = AAC_LOAS_FORMAT;
        FrameSize = Bits.Get(13) + AAC_LOAS_ASS_SYNC_LENGTH_HEADER_SIZE;

        if (FrameSize > AAC_LOAS_ASS_MAX_FRAME_SIZE)
        {
            FRAME_COND_ERROR("Invalid frame size (%d bytes)\n", FrameSize);
            return FrameParserError;
        }
        else if (FrameSize > (AAC_LOAS_ASS_SYNC_LENGTH_HEADER_SIZE + AAC_ADTS_MAX_FRAME_SIZE(AAC_MAX_NB_CHANNELS, 1)))
        {
            SE_DEBUG(group_frameparser_audio, "Warning : LOAS FrameSize %d  exceeds broadcast expectations (%d bytes expected for 1 raw-block)\n",
                     FrameSize,
                     AAC_ADTS_MAX_FRAME_SIZE(AAC_MAX_NB_CHANNELS, 1));
        }


        if (FrameParserNoError != FrameParser_AudioAac_c::ParseAudioMuxElementConfig(&Bits,
                                                                                     &SamplingFrequency,
                                                                                     &SampleCount,
                                                                                     &ExpliciteSbrExtension,
                                                                                     AvailableBytes - AAC_LOAS_ASS_SYNC_LENGTH_HEADER_SIZE,
                                                                                     Action))
        {
            return FrameParserError;
        }
    }
    else
    {
        // get more bits
        Sync |= Bits.Get(1) << 11;

        if (Sync == AAC_AUDIO_ADTS_SYNC_WORD)
        {
            Type = AAC_ADTS_FORMAT;
            Bits.FlushUnseen(1); //bool ID = Bits.Get(1);
            unsigned char Layer = Bits.Get(2);

            if (Layer != 0)
            {
                FRAME_COND_ERROR("Invalid AAC layer %d\n", Layer);
                return FrameParserError;
            }

            Bits.FlushUnseen(1); //protection_absent;
            unsigned int profile_ObjectType = Bits.Get(2);

            if ((profile_ObjectType + 1) != AAC_AUDIO_PROFILE_LC)
            {
                if (EnableHeaderUnplayableErrors)
                {
                    FRAME_COND_ERROR("Unsupported AAC profile in ADTS: %d\n", profile_ObjectType);
                    return FrameParserHeaderUnplayable;
                }
            }

            unsigned char sampling_frequency_index = Bits.Get(4);

            if (sampling_frequency_index > AAC_MAX_ISO_SAMPLE_RATE_IDX)
            {
                if (sampling_frequency_index <= AAC_MAX_SAMPLE_RATE_IDX)
                {
                    FRAME_COND_ERROR("sampling frequency index %d (%d Hz) is not supported by SE audio-sub-system \n", sampling_frequency_index, aac_sample_rates[sampling_frequency_index]);
                }
                else
                {
                    FRAME_COND_ERROR("Invalid sampling frequency index %d\n", sampling_frequency_index);
                }
                return FrameParserError;
            }

            // multiply the sampling freq by two in case a sbr object is present
            SamplingFrequency = aac_sample_rates[sampling_frequency_index];
            Bits.FlushUnseen(1); //private_bit
            unsigned int  channel_configuration = Bits.Get(3);
            unsigned int  nchan = AAC_MAX_NB_CHANNELS;

            if ((channel_configuration != 0) && (channel_configuration <= AAC_NB_CHANNELS_IDX))
            {
                nchan = aac_nb_channels[channel_configuration - 1];
            }

            Bits.FlushUnseen(1 + 1 + 1 + 1); //original/copy, home, copyright_identification_bit, copyright_identification_start
            FrameSize = Bits.Get(13); // aac_frame_length

            Bits.FlushUnseen(11); //adts_buffer_fullness
            unsigned int no_raw_data_blocks_in_frame = Bits.Get(2) + 1;
            // Note : to-date (2012-08-18)  we support only 1024 samples per block (960 samples per AAC-Block/Frames are not supported)
            SampleCount  = no_raw_data_blocks_in_frame * AAC_DEFAULT_NBSAMPLES_PER_FRAME;

            if (no_raw_data_blocks_in_frame > AAC_ADTS_NB_MAX_RAW_DATA_BLOCK)
            {
                FRAME_COND_ERROR("Frame Exceeding the number of raw data block supported by SE %d \n", no_raw_data_blocks_in_frame);
                return FrameParserError;
            }

            if ((FrameSize < AAC_ADTS_MIN_FRAME_SIZE)
                || (FrameSize > AAC_ADTS_MAX_FRAME_SIZE(nchan, no_raw_data_blocks_in_frame)))
            {
                FRAME_COND_ERROR("Invalid frame size (%d bytes)\n", FrameSize);
                return FrameParserError;
            }
            else if (FrameSize > AAC_ADTS_MAX_FRAME_SIZE(AAC_MAX_NB_CHANNELS, 1))
            {
                SE_DEBUG(group_frameparser_audio, "Warning : ADTS FrameSize %d for %d channels and %d raw-data-block exceeds broadcast expectations (%d bytes expected for 1 raw-block)\n",
                         FrameSize, nchan, no_raw_data_blocks_in_frame,
                         AAC_ADTS_MAX_FRAME_SIZE(AAC_MAX_NB_CHANNELS, 1));
            }

            if (SampleCount > AAC_MAX_NBSAMPLES_PER_FRAME)
            {
                FRAME_COND_ERROR("Invalid SampleCount (%d samples)\n", SampleCount);
                return FrameParserError;
            }
        }
        else
        {
            Sync |= Bits.Get(4) << 12;

            if (Sync == AAC_AUDIO_LOAS_EPASS_SYNC_WORD)
            {
                Type = AAC_LOAS_FORMAT;
                Bits.FlushUnseen(4); //futureUse
                FrameSize = Bits.Get(13) + AAC_LOAS_EP_ASS_HEADER_SIZE;
                // continue the parsing to get more info about the frame
                Bits.FlushUnseen(5 + 18); //frameCounter, headerParity
                AvailableBytes -= AAC_LOAS_EP_ASS_HEADER_SIZE;
                // now parse the EPMuxElement...
                bool epUsePreviousMuxConfig = Bits.Get(1);
                Bits.FlushUnseen(2); //epUsePreviousMuxConfigParity

                if (!epUsePreviousMuxConfig)
                {
                    unsigned int epSpecificConfigLength = Bits.Get(10);
                    unsigned int epSpecificConfigLengthParity = Bits.Get(11);
                    AvailableBytes -= 3;

                    if (AvailableBytes > 0)
                    {
                        Bits.FlushUnseen(epSpecificConfigLength * 8); //ErrorProtectionSpecificConfig
                        Bits.FlushUnseen(epSpecificConfigLengthParity * 8); //ErrorProtectionSpecificConfigParity
                        AvailableBytes -= epSpecificConfigLength + epSpecificConfigLengthParity;
                    }
                }
                else
                {
                    Bits.FlushUnseen(5); //ByteAlign()
                    AvailableBytes -= 1;
                }

                if (FrameParserNoError != FrameParser_AudioAac_c::ParseAudioMuxElementConfig(&Bits,
                                                                                             &SamplingFrequency,
                                                                                             &SampleCount,
                                                                                             &ExpliciteSbrExtension,
                                                                                             AvailableBytes,
                                                                                             Action))
                {
                    return FrameParserError;
                }
            }
            else
            {
                Sync |= Bits.Get(16) << 16;

                if (Sync == AAC_AUDIO_ADIF_SYNC_WORD)
                {
                    Type = AAC_ADIF_FORMAT;
                    FRAME_COND_ERROR("The AAC ADIF format is not supported yet!\n");
                    return FrameParserHeaderUnplayable;
                }
                else
                {
                    FRAME_COND_ERROR("Unknown Synchronization (0x%x)\n", Sync);
                    return FrameParserError;
                }
            }
        }
    }

    ParsedFrameHeader->SamplingFrequency     = SamplingFrequency;
    ParsedFrameHeader->ExpliciteSbrExtension = ExpliciteSbrExtension;
    ParsedFrameHeader->NumberOfSamples       = SampleCount;
    ParsedFrameHeader->Length                = FrameSize;
    ParsedFrameHeader->Type                  = Type;
    return FrameParserNoError;
}

FrameParserStatus_t FrameParser_AudioAac_c::ParseAudioMuxElementConfig(BitStreamClass_c *Bits,
                                                                       unsigned int      *SamplingFrequency,
                                                                       unsigned int      *SampleCount,
                                                                       bool              *ExpliciteSbrExtension,
                                                                       int                AvailableBytes,
                                                                       AacFrameParsingPurpose_t Action)
{
    // do as if a sbr extension is always present (searching for the sbr flag requires parsing efforts...)
    bool useSameStreamMux;

    if (AvailableBytes > 0)
    {
        useSameStreamMux = Bits->Get(1);
    }
    else
    {
        useSameStreamMux = true;
    }

    if (!useSameStreamMux)
    {
        bool audioMuxVersion = Bits->Get(1);

        if (!audioMuxVersion)
        {
            // only get program 0 and layer 0 information ...
            Bits->FlushUnseen(1 + 6 + 4 + 3); // allStreamSameTimeFraming, numSubFrames, numProgram, numLayer
            // now parse AudioSpecificConfig
            unsigned int audioObjectType = Bits->Get(5);

            if ((audioObjectType != AAC_AUDIO_PROFILE_LC) && (audioObjectType != AAC_AUDIO_PROFILE_SBR))
            {
                // supported audio profiles (audio firmware):
                // are LC (low complexity) and HE (LC+SBR)
                FRAME_COND_ERROR("Unsupported AAC Audio Object type: %d\n", audioObjectType);
                return FrameParserError;
            }

            unsigned int samplingFrequencyIndex = Bits->Get(4);

            if (samplingFrequencyIndex == 0xf)
            {
                *SamplingFrequency = Bits->Get(24);
            }
            else if (samplingFrequencyIndex <= AAC_MAX_ISO_SAMPLE_RATE_IDX)
            {
                *SamplingFrequency = aac_sample_rates[samplingFrequencyIndex];
            }
            else
            {
                if (samplingFrequencyIndex <= AAC_MAX_SAMPLE_RATE_IDX)
                {
                    FRAME_COND_ERROR("sampling frequency index %d (%d Hz) is not supported by SE audio-sub-system \n", samplingFrequencyIndex, aac_sample_rates[samplingFrequencyIndex]);
                }
                else
                {
                    FRAME_COND_ERROR("Invalid sampling frequency index %d \n", samplingFrequencyIndex);
                }
                return FrameParserError;
            }

            unsigned int channelConfiguration = Bits->Get(4);

            if (channelConfiguration > AAC_NB_CHANNELS_IDX)
            {
                FRAME_COND_ERROR("Invalid channel configuration %d\n", channelConfiguration);
                return FrameParserError;
            }

            if (audioObjectType == AAC_AUDIO_PROFILE_SBR)
            {
                *ExpliciteSbrExtension = true;
                // simply perform checks on the following values...
                samplingFrequencyIndex = Bits->Get(4);

                if (samplingFrequencyIndex == 0xf)
                {
                    *SamplingFrequency = Bits->Get(24);
                }
                else if (samplingFrequencyIndex <= AAC_MAX_ISO_SAMPLE_RATE_IDX)
                {
                    *SamplingFrequency = aac_sample_rates[samplingFrequencyIndex];
                }
                else
                {
                    if (samplingFrequencyIndex <= AAC_MAX_SAMPLE_RATE_IDX)
                    {
                        FRAME_COND_ERROR("sampling frequency index %d (%d Hz) is not supported by SE audio-sub-system \n", samplingFrequencyIndex, aac_sample_rates[samplingFrequencyIndex]);
                    }
                    else
                    {
                        FRAME_COND_ERROR("Invalid sampling frequency index %d\n", samplingFrequencyIndex);
                    }
                    return FrameParserError;
                }

                audioObjectType = Bits->Get(5);

                if (audioObjectType != AAC_AUDIO_PROFILE_LC)
                {
                    // supported audio profiles (audio firmware):
                    // is LC (low complexity)
                    FRAME_COND_ERROR("Unsupported AAC Audio Object type: %d\n", audioObjectType);
                    return FrameParserError;
                }
            }
        }
        else
        {
            FRAME_COND_ERROR("AAC LOAS parser: invalid audioMuxVersion reserved value\n");
            return FrameParserError;
        }

        // distinguish between implicit and explicit signalling of SBR
        // See NOTE 3 of Table 1.21, page 47 of w5711_(14496-3_3rd_sp1).doc - document name
        // ISO/IEC 14496-3:2001(E) provided by Gael
        *SampleCount        = (*ExpliciteSbrExtension) ? 2048 : 1024;
    } // !useSameStreamMux
    else
    {
        // we're in the situation where we found the sync word but don't have
        if (Action == AAC_GET_SYNCHRO)
        {
            // means we cannot sync without knowing all the audio properties. (useSameconfig is true)
            return FrameParserError;
        }

        // use same parameters as last frame...
        *SampleCount = 0;
        *SamplingFrequency = 0;
    }

    return FrameParserNoError;
}



////////////////////////////////////////////////////////////////////////////
///
///     Constructor
///
FrameParser_AudioAac_c::FrameParser_AudioAac_c(void)
    : FrameParser_Audio_c()
    , ParsedFrameHeader()
    , StreamParameters(NULL)
    , CurrentStreamParameters()
    , FrameParameters(NULL)
    , NumHeaderUnplayableErrors(0)
    , isFirstFrame(true)
    , StreamBased((BaseComponentClass_c::EnableCoprocessorParsing == 0) ? false : true)
{
    Configuration.FrameParserName               = "AudioAac";
    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &AacAudioStreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &AacAudioFrameParametersBuffer;

    ParsedFrameHeader.Type                  = AAC_AUTO_TYPE;
    ParsedFrameHeader.SamplingFrequency     = 0;
    ParsedFrameHeader.ExpliciteSbrExtension = false;
    ParsedFrameHeader.NumberOfSamples       = AAC_DEFAULT_NBSAMPLES_PER_FRAME;

    CurrentStreamParameters.Layer = 0; // illegal layer... force frames a parameters update
}

////////////////////////////////////////////////////////////////////////////
///
///     Destructor
///
FrameParser_AudioAac_c::~FrameParser_AudioAac_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
///     Method to connect to neighbor
///
FrameParserStatus_t   FrameParser_AudioAac_c::Connect(Port_c *Port)
{
    //
    // Clear our parameter pointers
    //
    StreamParameters                    = NULL;
    FrameParameters                     = NULL;
    //
    // Pass the call down the line
    //
    return FrameParser_Audio_c::Connect(Port);
}


////////////////////////////////////////////////////////////////////////////
///
///     static helper function to get Original Encoding enum values
//      from sample count
///
AudioOriginalEncoding_t FrameParser_AudioAac_c::GetHeAacOriginalEncodingFromSampleCount(int SampleCount)
{
    switch (SampleCount)
    {
    case 960:
        return AudioOriginalEncodingHEAAC_960;
    case 1024:
        return AudioOriginalEncodingHEAAC_1024;
    case 1920:
        return AudioOriginalEncodingHEAAC_1920;
    case 2048:
        return AudioOriginalEncodingHEAAC_2048;
    default:
        return AudioOriginalEncodingHEAAC_1024;
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Parse the frame header and store the results for when we emit the frame.
///
FrameParserStatus_t   FrameParser_AudioAac_c::ReadHeaders(void)
{
    FrameParserStatus_t Status;
    //
    // Perform the common portion of the read headers function
    //
    FrameParser_Audio_c::ReadHeaders();

    //
    if (StreamBased == false)
    {
        // save the previous frame parameters
        AacAudioParsedFrameHeader_t LastParsedFrameHeader;
        memcpy(&LastParsedFrameHeader, &ParsedFrameHeader, sizeof(AacAudioParsedFrameHeader_t));
        Status = ParseFrameHeader(BufferData, &ParsedFrameHeader, BufferLength, AAC_GET_FRAME_PROPERTIES, true);

        if (Status != FrameParserNoError)
        {
            if (Status == FrameParserHeaderUnplayable)
            {
                NumHeaderUnplayableErrors++;

                if (NumHeaderUnplayableErrors >= UnplayabilityThreshold)
                {
                    // this is clearly not a passing bit error
                    SE_ERROR("Too many unplayability reports, marking stream unplayable\n");
                    Stream->MarkUnPlayable();
                }
            }
            else
            {
                SE_ERROR("Failed to parse frame header, bad collator selected?\n");
            }

            return Status;
        }

        if (ParsedFrameHeader.Length != BufferLength)
        {
            SE_ERROR("Buffer length is inconsistent with frame header, bad collator selected?\n");
            return FrameParserError;
        }

        if (isFirstFrame)
        {
            isFirstFrame = false;
            SE_INFO(group_frameparser_audio, "AAC Frame type: %s, FrameSize %d, Number of samples: %d, SamplingFrequency %d . %s\n",
                    FrameTypeName[ParsedFrameHeader.Type],
                    ParsedFrameHeader.Length,
                    ParsedFrameHeader.NumberOfSamples,
                    ParsedFrameHeader.SamplingFrequency,
                    ParsedFrameHeader.ExpliciteSbrExtension ? "Explicite SBR Extension" : "");
        }

        if ((ParsedFrameHeader.SamplingFrequency == 0) || (ParsedFrameHeader.NumberOfSamples == 0))
        {
            // the current frame has no such properties, we must refer to the previous frame...
            if ((LastParsedFrameHeader.SamplingFrequency == 0) || (LastParsedFrameHeader.NumberOfSamples == 0))
            {
                // the previous frame has no properties either, we cannot decode this frame...
                FrameToDecode = false;
                SE_ERROR("This frame should not be sent for decode (it relies on previous frame for audio parameters)\n");
                return FrameParserError;
            }
            else
            {
                FrameToDecode = true;
                // make the previous frame properties the current ones..
                memcpy(&ParsedFrameHeader, &LastParsedFrameHeader, sizeof(AacAudioParsedFrameHeader_t));
            }
        }
        else
        {
            FrameToDecode = true;
        }

        NumHeaderUnplayableErrors = 0;
    } // Streambased == false

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
    ParsedFrameParameters->SizeofFrameParameterStructure = sizeof(AacAudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure       = FrameParameters;

    if (StreamBased == false)
    {
        unsigned int sbr_oversample = (ParsedFrameHeader.ExpliciteSbrExtension) ? 1 /* already oversampled */ : 2 /* assume implicite SBR */ ;
        FrameParameters->FrameSize  = ParsedFrameHeader.Length;
        FrameParameters->Type       = ParsedFrameHeader.Type;
        ParsedAudioParameters->BackwardCompatibleProperties.SampleRateHz = ParsedFrameHeader.SamplingFrequency;
        ParsedAudioParameters->BackwardCompatibleProperties.SampleCount  = ParsedFrameHeader.NumberOfSamples;
        ParsedAudioParameters->Source.SampleRateHz = ParsedFrameHeader.SamplingFrequency * sbr_oversample;
        ParsedAudioParameters->SampleCount         = ParsedFrameHeader.NumberOfSamples   * sbr_oversample;
    }
    else
    {
        ParsedAudioParameters->Source.SampleRateHz = 0;  // filled in by codec not know to the parser in stream base decoding
        ParsedAudioParameters->SampleCount = 1024 * 2; // No info to the parser is SBR is present or not : Set to 2048 considering SBR.
        Stream->Statistics().FrameParserAudioSampleRate = 0; // not know to the parser in stream base decoding
        Stream->Statistics().FrameParserAudioFrameSize = 0; // not know to the parser in stream base decoding
        FrameToDecode = true;
    }

    if (ParsedFrameHeader.ExpliciteSbrExtension == true)
    {
        ParsedAudioParameters->OriginalEncoding = GetHeAacOriginalEncodingFromSampleCount(ParsedAudioParameters->SampleCount);
    }
    else
    {
        ParsedAudioParameters->OriginalEncoding = AudioOriginalEncodingAAC;
    }
    ParsedAudioParameters->Source.BitsPerSample = 0; // filled in by codec
    ParsedAudioParameters->Source.ChannelCount = 0;  // filled in by codec
    ParsedAudioParameters->Organisation = 0; // filled in by codec
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     The reset reference frame list function
///
FrameParserStatus_t   FrameParser_AudioAac_c::ResetReferenceFrameList(void)
{
    SE_DEBUG(group_frameparser_audio, ">><<");
    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL);
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for AAC audio.
///
/// \copydoc FrameParser_Audio_c::PurgeQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioAac_c::PurgeQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for AAC audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioAac_c::ProcessQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
/// For MPEG audio these can be determined immediately (although it the first
/// frame for decode does not contain a PTS we must synthesize one).
///
FrameParserStatus_t   FrameParser_AudioAac_c::GeneratePostDecodeParameterSettings(void)
{
    FrameParserStatus_t Status;
    //
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
    Status = HandleCurrentFrameNormalizedPlaybackTime();

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
    if (StreamBased == false)
    {
        // no call to HandleUpdateStreamParameters() because UpdateStreamParameters is always false
        SE_ASSERT(false == UpdateStreamParameters && NULL == StreamParametersBuffer);
        GenerateNextFrameNormalizedPlaybackTime(ParsedFrameHeader.NumberOfSamples,
                                                ParsedFrameHeader.SamplingFrequency);
    }

    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for AAC audio.
///
/// \copydoc FrameParser_Audio_c::PrepareReferenceFrameList()
///
FrameParserStatus_t   FrameParser_AudioAac_c::PrepareReferenceFrameList(void)
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for AAC audio.
///
/// \copydoc FrameParser_Audio_c::PrepareReferenceFrameList()
///
FrameParserStatus_t   FrameParser_AudioAac_c::UpdateReferenceFrameList(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for AAC audio.
///
/// \copydoc FrameParser_Audio_c::ProcessReverseDecodeUnsatisfiedReferenceStack()
///
FrameParserStatus_t   FrameParser_AudioAac_c::ProcessReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for AAC audio.
///
/// \copydoc FrameParser_Audio_c::ProcessReverseDecodeStack()
///
FrameParserStatus_t   FrameParser_AudioAac_c::ProcessReverseDecodeStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for AAC audio.
///
/// \copydoc FrameParser_Audio_c::PurgeReverseDecodeUnsatisfiedReferenceStack()
///
FrameParserStatus_t   FrameParser_AudioAac_c::PurgeReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for AAC audio.
///
/// \copydoc FrameParser_Audio_c::PurgeReverseDecodeStack()
///
FrameParserStatus_t   FrameParser_AudioAac_c::PurgeReverseDecodeStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for AAC audio.
///
/// \copydoc FrameParser_Audio_c::TestForTrickModeFrameDrop()
///
FrameParserStatus_t   FrameParser_AudioAac_c::TestForTrickModeFrameDrop(void)
{
    return FrameParserNoError;
}


