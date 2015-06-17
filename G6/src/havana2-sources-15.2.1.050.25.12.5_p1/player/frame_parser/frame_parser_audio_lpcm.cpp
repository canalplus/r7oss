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
/// \class FrameParser_AudioLpcm
/// \brief Frame parser for LPCM audio
///

#include "lpcm.h"
#include "collator_pes_audio_lpcm.h"
#include "frame_parser_audio_lpcm.h"
#include "spdifin_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_AudioLpcm_c"

static BufferDataDescriptor_t     LpcmAudioStreamParametersBuffer = BUFFER_LPCM_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     LpcmAudioFrameParametersBuffer = BUFFER_LPCM_AUDIO_FRAME_PARAMETERS_TYPE;

// this array is based on the LpcmSamplingFreq_t enum
static const int LpcmDVDSamplingFreq[LpcmSamplingFreqLast] =
{
    // DVD Video sfreq :
    48000, 96000 , 192000, 0 ,

    // Spdifin additional sfreq :
    32000, 16000 , 22050 , 24000,

    // DVD Audio additional sfreq :
    44100, 88200 , 176400, 0 ,
};

// this array is based on the LpcmSamplingFreq_t enum
const static unsigned char LpcmDVDAudioSampleCount[LpcmSamplingFreqNone] =
{
    40, 80, 160, 0, 0, 0, 0, 0, 40, 80, 160
};

// this array is based on the LpcmSamplingFreq_t enum
const static unsigned char LpcmDVDVideoSampleCount[] =
{
    80, 160
};

// this table redirects to the DVD code for sampling freqs
const static LpcmSamplingFreq_t LpcmBD2DVDSamplingFreq[16] =
{
    LpcmSamplingFreqNone,
    LpcmSamplingFreq48,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreq96,
    LpcmSamplingFreq192,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone
};
// this table redirects to the DVD code for sampling freqs
const static LpcmSamplingFreq_t LpcmSpdifin2DVDSamplingFreq[16] =
{
    LpcmSamplingFreqNone, // CEA861_0k
    LpcmSamplingFreq32,   // CEA861_32k
    LpcmSamplingFreq44p1, // CEA861_44k
    LpcmSamplingFreq48,   // CEA861_48k
    LpcmSamplingFreq88p2, // CEA861_88k
    LpcmSamplingFreq96,   // CEA861_96k
    LpcmSamplingFreq176p4,// CEA861_176k
    LpcmSamplingFreq192,  // CEA861_192k
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone,
    LpcmSamplingFreqNone
};

const static signed char BDChannelAssignment2ChannelCount[] =
{
    -1, 2, 2, 2, 4, 4, 4, 4, 6, 6, 8, 8
};

const static char DVDAudioChannelAssignment2ChannelCount2[] =
{
    0, 0, 1, 2, 1, 2, 3, 1, 2, 3, 2, 3, 4, 1, 2, 1, 2, 3, 1, 1, 2
};

const static char DVDAudioChannelAssignment2ChannelCount1[] =
{
    1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4
};

const static char BytesPerSample[] =
{
    2, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const static char AudioPesPrivateDataLength[] =
{
    LPCM_DVD_VIDEO_PRIVATE_HEADER_LENGTH,
    LPCM_DVD_AUDIO_MIN_PRIVATE_HEADER_LENGTH,
    LPCM_HD_DVD_PRIVATE_HEADER_LENGTH,
    LPCM_BD_PRIVATE_HEADER_LENGTH,
    SPDIFIN_PRIVATE_HEADER_LENGTH // SPDIFIN
};

const static char FirstAccessUnitOffset[] =
{
    LPCM_DVD_VIDEO_FIRST_ACCES_UNIT,
    LPCM_DVD_AUDIO_FIRST_ACCES_UNIT,
    LPCM_HD_DVD_FIRST_ACCES_UNIT,
    LPCM_BD_FIRST_ACCES_UNIT,
    SPDIFIN_FIRST_ACCES_UNIT // SPDIFIN
};

// /////////////////////////////////////////////////////////////////////////
///
/// This function parses an independant frame header and the
/// possible following dependant lpcm frame headers
///
/// \return Frame parser status code, FrameParserNoError indicates success.

FrameParserStatus_t FrameParser_AudioLpcm_c::ParseFrameHeader(unsigned char *FrameHeaderBytes,
                                                              LpcmAudioParsedFrameHeader_t *NextParsedFrameHeader,
                                                              int GivenFrameSize)
{
    BitStreamClass_c       Bits;
    LpcmSamplingFreq_t     AudioSamplingFrequency1;
    LpcmSamplingFreq_t     AudioSamplingFrequency2  = LpcmSamplingFreqNone;
    unsigned int           NbSamples                = 0;
    char                   NumberOfAudioChannels    = 1;
    LpcmWordSize_t         WordSize1;
    LpcmWordSize_t         WordSize2                = LpcmWordSizeNone;
    int                    DynamicRangeControl      = 0;
    int                    ChannelAssignment        = 0xff; // this is the default assignment for the firmware (not exported by the firmware headers)
    bool                   MuteFlag                 = false;
    bool                   EmphasisFlag             = false;
    int                    FirstAccessUnitPointer   = 1;
    int                    AudioFrameNumber         = 0;
    int                    NbAccessUnits            = 1;
    int                    BitShiftChannel2         = 0;
    int                    FrameSize                = 0;
    unsigned int           ExtraPrivateHeaderLength = 0;
    unsigned char          SubStreamId              = 0;
    unsigned int           AudioFrameSize           = 0;

    LpcmStreamType_t StreamType = NextParsedFrameHeader->Type;

    memset(NextParsedFrameHeader, 0, sizeof(LpcmAudioParsedFrameHeader_t));

    Bits.SetPointer(FrameHeaderBytes);

    switch (StreamType)
    {
    case   TypeLpcmDVDVideo:
    {
        ///< frame is a DVD video lpcm
        SubStreamId                  = Bits.Get(8);
        NbAccessUnits                = Bits.Get(8);
        FirstAccessUnitPointer       = Bits.Get(16);
        EmphasisFlag                 = Bits.Get(1);
        MuteFlag                     = Bits.Get(1);
        Bits.FlushUnseen(1);
        AudioFrameNumber             = Bits.Get(5);
        WordSize1                    = (LpcmWordSize_t) Bits.Get(2);
        AudioSamplingFrequency1      = (LpcmSamplingFreq_t) Bits.Get(2);
        Bits.FlushUnseen(1);
        NumberOfAudioChannels        = Bits.Get(3) + 1;
        DynamicRangeControl          = Bits.Get(8);

        // sanity checks...
        if ((SubStreamId & LPCM_DVD_VIDEO_SUBSTREAM_ID_MASK) != LPCM_DVD_VIDEO_SUBSTREAM_ID)
        {
            SE_ERROR("Invalid sub stream identifier (%x)\n", SubStreamId);
            return FrameParserError;
        }

        if ((AudioFrameNumber >= 20) && (AudioFrameNumber != 31))
        {
            SE_ERROR("Invalid audio frame number (%d)\n", AudioFrameNumber);
            return FrameParserError;
        }

        if (WordSize1 > LpcmWordSize24)
        {
            SE_ERROR("Invalid quantization word length (%d)\n", WordSize1);
            return FrameParserError;
        }

        if (NumberOfAudioChannels > 8)
        {
            SE_ERROR("Invalid number of audio channels (%d)\n", NumberOfAudioChannels);
            return FrameParserError;
        }

        if (AudioSamplingFrequency1 > LpcmSamplingFreq96)
        {
            SE_ERROR("Invalid Sampling Frequency (%d)\n", AudioSamplingFrequency1);
            return FrameParserError;
        }

        NbSamples          = LpcmDVDVideoSampleCount[AudioSamplingFrequency1];

        if (WordSize1 == LPCM_DVD_WS_20)
        {
            // 20 bits special case: 4 bits of the sample are loacted at the end of a
            // the first 16 -bit part of two samples ...
            // DVD Specifications for Read Only Disc / Part 3: Video Specifications
            // 5. Video Object / 5.4 Presentation Data / Figure 5.4.2.1-2
            AudioFrameSize = (NbSamples / 2) * NumberOfAudioChannels * 5;
        }
        else
        {
            AudioFrameSize = NbSamples * NumberOfAudioChannels * BytesPerSample[WordSize1];
        }

        break;
    }

    case TypeLpcmDVDAudio:
    {
        ///< frame is a DVD audio lpcm
        SubStreamId                  = Bits.Get(8);
        Bits.FlushUnseen(3); // reserved
        // char upc_ean_isrc_number     = Bits.Get(5);
        // char upc_ean_isrc_data       = Bits.Get(8);
        Bits.FlushUnseen(13); // replaces the commented code out above...
        ExtraPrivateHeaderLength     = Bits.Get(8) + LPCM_DVD_AUDIO_PRIVATE_HEADER_LENGTH;
        FirstAccessUnitPointer       = Bits.Get(16);
        EmphasisFlag                 = Bits.Get(1);
        Bits.FlushUnseen(1); // reserved
        //char StereoPlayBackMode      = Bits.Get(1);
        //char DownMixCodeValidity     = Bits.Get(1);
        //char DownMixCode             = Bits.Get(4);
        Bits.FlushUnseen(6); // replaces the commented code out above...
        WordSize1                    = (LpcmWordSize_t)Bits.Get(4);
        WordSize2                    = (LpcmWordSize_t)Bits.Get(4);
        AudioSamplingFrequency1      = (LpcmSamplingFreq_t)Bits.Get(4);
        AudioSamplingFrequency2      = (LpcmSamplingFreq_t)Bits.Get(4);
        Bits.FlushUnseen(4); // reserved
        Bits.FlushUnseen(4); // char MultiChannelType        = Bits.Get(4);
        BitShiftChannel2             = Bits.Get(3);
        ChannelAssignment            = Bits.Get(5);
        DynamicRangeControl          = Bits.Get(8);

        // sanity checks...
        if ((SubStreamId & LPCM_DVD_AUDIO_SUBSTREAM_ID_MASK) != LPCM_DVD_AUDIO_SUBSTREAM_ID)
        {
            SE_ERROR("Invalid sub stream identifier (%x)\n", SubStreamId);
            return FrameParserError;
        }

        if ((WordSize1 > LpcmWordSize24) || ((WordSize2 > LpcmWordSize24) && (WordSize2 != LPCM_DVD_AUDIO_NO_CH_GR2)))
        {
            SE_ERROR("Invalid quantization word length\n");
            return FrameParserError;
        }

        if (AudioSamplingFrequency1 >= LpcmSamplingFreqNone)
        {
            SE_ERROR("Invalid Sampling Frequency (%d)\n", AudioSamplingFrequency1);
            return FrameParserError;
        }

        // LpcmSamplingFreqNotSpecififed is used only for secondary streams of DVD-Audio
        // the 'not specified' lies outside the range of LpcmSamplingFreqLast - bring it inside
        // this is valid for use with LpcmDVDSamplingFreq, but not with LpcmDVDAudioSampleCount
        if (AudioSamplingFrequency2 == LpcmSamplingFreqNotSpecififed)
        {
            AudioSamplingFrequency2 = LpcmSamplingFreqNone;
        }

        if (AudioSamplingFrequency2 >= LpcmSamplingFreqLast)
        {
            SE_ERROR("Invalid Sampling Frequency (%d)\n", AudioSamplingFrequency2);
            return FrameParserError;
        }

        // DVDAudioChannelAssignment2ChannelCount1/2 have same size
        if (ChannelAssignment >= (int)(sizeof(DVDAudioChannelAssignment2ChannelCount1)
                                       / sizeof(DVDAudioChannelAssignment2ChannelCount1[0])))
        {
            SE_ERROR("Invalid channel assignment (%d)\n", ChannelAssignment);
            return FrameParserError;
        }

        //
        int NbSamples1 = LpcmDVDAudioSampleCount[AudioSamplingFrequency1];
        int NbSamples2 =
            (AudioSamplingFrequency2 < LpcmSamplingFreqNone) ?
            LpcmDVDAudioSampleCount[AudioSamplingFrequency2] : 0;
        char NumberOfAudioChannels1 = DVDAudioChannelAssignment2ChannelCount1[ChannelAssignment];
        char NumberOfAudioChannels2 = DVDAudioChannelAssignment2ChannelCount2[ChannelAssignment];
        NumberOfAudioChannels = NumberOfAudioChannels2 + NumberOfAudioChannels1;
        int AudioFrameSize1, AudioFrameSize2;
        NbSamples = NbSamples1;

        if (WordSize1 == LPCM_DVD_WS_20)
        {
            // 20 bits special case: 4 bits of the sample are located at the end of a
            // the first 16 -bit part of two samples ...
            // DVD Specifications for Read Only Disc / Part 3: Video Specifications
            // 5. Video Object / 5.4 Presentation Data / Figure 5.4.2.1-2
            AudioFrameSize1 = (NbSamples1 / 2) * NumberOfAudioChannels1 * 5;
        }
        else
        {
            AudioFrameSize1 = NbSamples1 * NumberOfAudioChannels1 * BytesPerSample[WordSize1];
        }

        if (WordSize2 == LPCM_DVD_WS_20)
        {
            // 20 bits special case: 4 bits of the sample are located at the end of a
            // the first 16 -bit part of two samples ...
            // DVD Specifications for Read Only Disc / Part 3: Video Specifications
            // 5. Video Object / 5.4 Presentation Data / Figure 5.4.2.1-2
            AudioFrameSize2 = (NbSamples2 / 2) * NumberOfAudioChannels2 * 5;
        }
        else
        {
            AudioFrameSize2 = NbSamples2 * NumberOfAudioChannels2 * BytesPerSample[WordSize2];
        }

        AudioFrameSize = AudioFrameSize1 + AudioFrameSize2;
        break;
    }

    case TypeLpcmDVDHD:
    {
        ///< frame is a DVD HD lpcm
        SubStreamId                  = Bits.Get(8);
        NbAccessUnits                = Bits.Get(8);
        FirstAccessUnitPointer       = Bits.Get(16);
        EmphasisFlag                 = Bits.Get(1);
        MuteFlag                     = Bits.Get(1);
        AudioFrameNumber             = Bits.Get(5);
        WordSize1                    = (LpcmWordSize_t)Bits.Get(2);
        AudioSamplingFrequency1      = (LpcmSamplingFreq_t)Bits.Get(3);
        NumberOfAudioChannels        = Bits.Get(4) + 1;
        DynamicRangeControl          = Bits.Get(8);
        Bits.FlushUnseen(3); // reserved
        //char DownMixCodeValidity     = Bits.Get(1);
        //char DownMixCode             = Bits.Get(4);
        Bits.FlushUnseen(5); // replaces the commented code out above...
        Bits.FlushUnseen(3); // reserved
        ChannelAssignment            = Bits.Get(5);

        // sanity checks...
        if ((SubStreamId & LPCM_DVD_VIDEO_SUBSTREAM_ID_MASK) != LPCM_DVD_VIDEO_SUBSTREAM_ID)
        {
            SE_ERROR("Invalid sub stream identifier (%x)\n", SubStreamId);
            return FrameParserError;
        }

        if ((AudioFrameNumber >= 20) && (AudioFrameNumber != 31))
        {
            SE_ERROR("Invalid audio frame number (%d)\n", AudioFrameNumber);
            return FrameParserError;
        }

        if (WordSize1 > LpcmWordSize24)
        {
            SE_ERROR("Invalid quantization word length (%d)\n", WordSize1);
            return FrameParserError;
        }

        if (NumberOfAudioChannels > 8)
        {
            SE_ERROR("Invalid number of audio channels (%d)\n", NumberOfAudioChannels);
            return FrameParserError;
        }

        if (AudioSamplingFrequency1 > LpcmSamplingFreq192)
        {
            SE_ERROR("Invalid Sampling Frequency (%d)\n", AudioSamplingFrequency1);
            return FrameParserError;
        }

        NbSamples = LpcmDVDAudioSampleCount[AudioSamplingFrequency1];

        if (WordSize1 == LPCM_DVD_WS_20)
        {
            // 20 bits special case: 4 bits of the sample are located at the end of a
            // the first 16 -bit part of two samples ...
            // DVD Specifications for Read Only Disc / Part 3: Video Specifications
            // 5. Video Object / 5.4 Presentation Data / Figure 5.4.2.1-2
            AudioFrameSize = (NbSamples / 2) * NumberOfAudioChannels * 5;
        }
        else
        {
            AudioFrameSize = NbSamples * NumberOfAudioChannels * BytesPerSample[WordSize1];
        }

        break;
    }

    case TypeLpcmDVDBD:
    {
        unsigned int  Sfreq;
        unsigned char BitsPerSample;
        unsigned int  BytesPerSample;
        ///< frame is a BD lpcm
        FrameSize          = Bits.Get(16);
        ChannelAssignment  = Bits.Get(4);
        Sfreq              = Bits.Get(4);
        BitsPerSample      = Bits.Get(2);

        if ((ChannelAssignment == 0) || (ChannelAssignment > 11))
        {
            SE_ERROR("Invalid channel assignment (%d)\n", ChannelAssignment);
            return FrameParserError;
        }
        {
            // Sfreq is a 4-bit value and cannot possibly overflow the tables - invalid comes back None
            AudioSamplingFrequency1     = LpcmBD2DVDSamplingFreq[Sfreq];

            if (AudioSamplingFrequency1 == LpcmSamplingFreqNone)
            {
                SE_ERROR("Invalid sampling frequency (Sfreq %d)\n", Sfreq);
                return FrameParserError;
            }

            if (BitsPerSample == 0)
            {
                SE_ERROR("Invalid bits per sample value\n");
                return FrameParserError;
            }

            WordSize1         = (LpcmWordSize_t)(BitsPerSample - 1);
            BytesPerSample    = ((BitsPerSample >= 2) ? 3 : 2);
        }

        NumberOfAudioChannels = BDChannelAssignment2ChannelCount[ChannelAssignment];
        NbSamples             = FrameSize / (BytesPerSample * NumberOfAudioChannels);
        AudioFrameSize        = FrameSize;
        break;
    }

    case TypeLpcmSPDIFIN:
    {
        unsigned int  Sfreq;
        unsigned char BitsPerSample;
        unsigned int  BytesPerSample;
        unsigned int  ChanAlloc;
        ///< frame is a SPDIFIN
        FrameSize                                                 = Bits.Get(16);
        ChanAlloc                                                 = Bits.Get(5);
        NextParsedFrameHeader->SpdifInProperties.Organisation     = HDMI_AMODE(ChanAlloc);
        Sfreq                                                     = Bits.Get(4);
        BitsPerSample                                             = Bits.Get(2);
        EmphasisFlag                                              = Bits.Get(1);
        NumberOfAudioChannels                                     = Bits.Get(4);
        NextParsedFrameHeader->SpdifInProperties.ChannelCount     = NumberOfAudioChannels;
        NextParsedFrameHeader->SpdifInProperties.DownMixInhibit   = Bits.Get(1);
        NextParsedFrameHeader->SpdifInProperties.LevelShiftValue  = Bits.Get(4);
        NextParsedFrameHeader->SpdifInProperties.LfePlaybackLevel = Bits.Get(2);
        NextParsedFrameHeader->SpdifInProperties.SpdifInLayout    = (stm_se_audio_stream_type_t) Bits.Get(4);

        WordSize1               = LpcmWordSize32;
        // Sfreq is a 4-bit value and cannot possibly overflow the tables - invalid comes back None
        AudioSamplingFrequency1 = LpcmSpdifin2DVDSamplingFreq[Sfreq];
        NextParsedFrameHeader->SpdifInProperties.SamplingFrequency = LpcmDVDSamplingFreq[AudioSamplingFrequency1];
        BytesPerSample          = (WordSize1 + 1); /* enum LpcmWordSize32 = 3 so BytesPerSample = 4*/
        if (AudioSamplingFrequency1 == LpcmSamplingFreqNone)
        {
            SE_ERROR("Invalid sampling frequency (Sfreq %d)\n", Sfreq);
            return FrameParserError;
        }
        if (BitsPerSample)
        {
            SE_ERROR("Invalid BisPerSample %d Must be 0 for SPDIFIN codec type\n", BitsPerSample);
            return FrameParserError;
        }
        if ((NextParsedFrameHeader->SpdifInProperties.ChannelCount == 0) || (NextParsedFrameHeader->SpdifInProperties.ChannelCount > 8))
        {
            SE_ERROR("Invalid ChannelCount %d Must be between 1-8 for SPDIFIN codec type\n", NextParsedFrameHeader->SpdifInProperties.ChannelCount);
            return FrameParserError;
        }
        NbSamples             = FrameSize / (BytesPerSample * NextParsedFrameHeader->SpdifInProperties.ChannelCount);
        AudioFrameSize        = FrameSize;
        break;
    }

    default:
        // should not occur
        SE_ERROR("Internal Error: Unknown LPCM Frame type\n");
        return FrameParserError;
    }

    // sanity check on the first access unit pointer: is it outside the packet?
    if ((FirstAccessUnitPointer + FirstAccessUnitOffset[StreamType]) > GivenFrameSize)
    {
        SE_ERROR("Invalid First Acces Unit Pointer value (%d)\n", FirstAccessUnitPointer);
        return FrameParserError;
    }
    else if ((FirstAccessUnitPointer + FirstAccessUnitOffset[StreamType]) < AudioPesPrivateDataLength[StreamType])
    {
        SE_ERROR("Invalid FirstAccessUnitPointer (%d) + FirstAccessUnitOffset (%d) must be >= AudioPesPrivateDataLength (%d)\n",
                 FirstAccessUnitPointer, FirstAccessUnitOffset[StreamType], AudioPesPrivateDataLength[StreamType]);
        return FrameParserError;
        //FirstAccessUnitPointer = AudioPesPrivateDataLength[StreamType] - FirstAccessUnitOffset[StreamType];
    }

    // compute the real number of access units, according to the frame size
    {
        unsigned int Payload          = (GivenFrameSize - (FirstAccessUnitPointer + FirstAccessUnitOffset[StreamType]));
        SE_DEBUG(group_frameparser_audio, "Payload %d , GivenFrameSize %d\n",
                 Payload, GivenFrameSize);
        // take ceiled number of frame header
        NbAccessUnits     = Payload / AudioFrameSize;
        NbAccessUnits     = ((NbAccessUnits * AudioFrameSize) < Payload) ? NbAccessUnits + 1 : NbAccessUnits;
        FrameSize =  NbAccessUnits * AudioFrameSize;
        NbSamples *= NbAccessUnits;
    }

    if (StreamType != TypeLpcmSPDIFIN)
    {
        SE_DEBUG(group_frameparser_audio, "SamplingFreq %d Hz, FrameSize %d, Type % d, WordSize %d , Aud Frame Id %d\n",
                 LpcmDVDSamplingFreq[AudioSamplingFrequency1], FrameSize, StreamType, WordSize1, AudioFrameNumber);
        SE_DEBUG(group_frameparser_audio, "FirstAccessUnitPointer %d, NbAccessUnits %d,  Nb Channels % d, Nb Samples %d\n",
                 FirstAccessUnitPointer, NbAccessUnits, NumberOfAudioChannels, NbSamples);

        if (StreamType == TypeLpcmDVDAudio)
        {
            SE_DEBUG(group_frameparser_audio, "GR2 properties: SamplingFreq %d Hz, WordSize2 %d\n",
                     LpcmDVDSamplingFreq[AudioSamplingFrequency2], WordSize2);
        }
    }
    else
    {
        SE_DEBUG(group_frameparser_audio, "SamplingFreq %d Hz, FrameSize %d, Type % d, WordSize %d , Aud Frame Id %d\n",
                 LpcmDVDSamplingFreq[AudioSamplingFrequency1], FrameSize, StreamType, WordSize1, AudioFrameNumber);
        SE_DEBUG(group_frameparser_audio, "FirstAccessUnitPointer %d, NbAccessUnits %d,  Nb Channels % d, Nb Samples %d\n",
                 FirstAccessUnitPointer, NbAccessUnits, NumberOfAudioChannels, NbSamples);
    }

    // we will send a whole audio frame
    NextParsedFrameHeader->Type                = StreamType;
    NextParsedFrameHeader->SamplingFrequency1  = AudioSamplingFrequency1;
    NextParsedFrameHeader->SamplingFrequency2  = AudioSamplingFrequency2;
    NextParsedFrameHeader->NumberOfSamples     = NbSamples;
    NextParsedFrameHeader->NumberOfChannels    = NumberOfAudioChannels;
    NextParsedFrameHeader->Length              = FrameSize;
    NextParsedFrameHeader->WordSize1           = WordSize1;
    NextParsedFrameHeader->WordSize2           = WordSize2;
    NextParsedFrameHeader->NbAccessUnits       = NbAccessUnits;
    NextParsedFrameHeader->FirstAccessUnitPointer = FirstAccessUnitPointer;
    NextParsedFrameHeader->DrcCode             = DynamicRangeControl;
    NextParsedFrameHeader->BitShiftChannel2    = BitShiftChannel2;
    NextParsedFrameHeader->EmphasisFlag        = EmphasisFlag;
    NextParsedFrameHeader->MuteFlag            = MuteFlag;
    NextParsedFrameHeader->PrivateHeaderLength = ((StreamType == TypeLpcmDVDAudio) ? ExtraPrivateHeaderLength : AudioPesPrivateDataLength[StreamType]);
    NextParsedFrameHeader->AudioFrameNumber    = AudioFrameNumber;
    NextParsedFrameHeader->SubStreamId         = SubStreamId;
    NextParsedFrameHeader->ChannelAssignment   = ChannelAssignment;
    NextParsedFrameHeader->AudioFrameSize      = AudioFrameSize;
    return FrameParserNoError;
}

///////////////////////////////////////////////////////////////////////////
///
///     Constructor
///
FrameParser_AudioLpcm_c::FrameParser_AudioLpcm_c(unsigned int DecodeLatencyInSamples)
    : FrameParser_Audio_c()
    , StreamParameters(NULL)
    , CurrentStreamParameters()
    , FrameParameters(NULL)
    , StreamType()
    , IndirectDecodeLatencyInSamples(DecodeLatencyInSamples)
{
    Configuration.FrameParserName               = "AudioLpcm";
    Configuration.StreamParametersCount         = 64;
    Configuration.StreamParametersDescriptor    = &LpcmAudioStreamParametersBuffer;
    Configuration.FrameParametersCount          = 64;
    Configuration.FrameParametersDescriptor     = &LpcmAudioFrameParametersBuffer;
}

////////////////////////////////////////////////////////////////////////////
///
///     Destructor
///
FrameParser_AudioLpcm_c::~FrameParser_AudioLpcm_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
///     Method to connect to neighbor
///
FrameParserStatus_t   FrameParser_AudioLpcm_c::Connect(Port_c *Port)
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
    CurrentStreamParameters.Type = TypeLpcmInvalid;
    //
    // Pass the call down the line
    //
    FrameParserStatus_t Status = FrameParser_Audio_c::Connect(Port);
    if (FrameParserNoError != Status)
    {
        return Status;
    }

    //
    // After calling the base class method we have a valid pointer to the collator.
    //
    StreamType = ((Collator_PesAudioLpcm_c *)Stream->GetCollator())->GetStreamType();

    //
    // Now we have stream type we can configure an appropriate jitter tollerance.
    // Normally the default is correct but there is considerable jitter in
    // timestamps when operating in AVR mode. At this stage in the pipeline we
    // handle this jitter by ignoring it (the threshold is only used to issue
    // warnings anyway).
    if (TypeLpcmSPDIFIN == StreamType)
    {
        PtsJitterTollerenceThreshold = 10000;
    }

    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Parse the frame header and store the results for when we emit the frame.
///
FrameParserStatus_t   FrameParser_AudioLpcm_c::ReadHeaders(void)
{
    SE_DEBUG(group_frameparser_audio, ">><<\n");
    //
    // Perform the common portion of the read headers function
    //
    FrameParser_Audio_c::ReadHeaders();
    //
    // the frame type is required to (re)parse the private data area
    LpcmAudioParsedFrameHeader_t ParsedFrameHeader;
    ParsedFrameHeader.Type = StreamType;
    FrameParserStatus_t Status = ParseFrameHeader(BufferData, &ParsedFrameHeader, BufferLength);
    if (Status != FrameParserNoError)
    {
        SE_ERROR("Failed to parse frame header, bad collator selected?\n");
        return Status;
    }

    if ((ParsedFrameHeader.Length + AudioPesPrivateDataLength[ParsedFrameHeader.Type]) != BufferLength)
    {
        SE_ERROR("Buffer length (%d) is inconsistent with frame header (%d), bad collator selected?\n",
                 BufferLength, ParsedFrameHeader.Length);
        return FrameParserError;
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
    ParsedFrameParameters->SizeofFrameParameterStructure = sizeof(LpcmAudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure       = FrameParameters;
    FrameParameters->DrcCode = ParsedFrameHeader.DrcCode;
    FrameParameters->NumberOfSamples = ParsedFrameHeader.NumberOfSamples;
    ParsedFrameParameters->DataOffset = AudioPesPrivateDataLength[ParsedFrameHeader.Type];

    // A SetGlobal Comand needs to be sent to update the frame parameters,
    // if some important part of the frame have been modified
    if ((CurrentStreamParameters.WordSize1 != ParsedFrameHeader.WordSize1) ||
        (CurrentStreamParameters.EmphasisFlag != ParsedFrameHeader.EmphasisFlag) ||
        (CurrentStreamParameters.MuteFlag != ParsedFrameHeader.MuteFlag) ||
        (CurrentStreamParameters.NumberOfChannels != ParsedFrameHeader.NumberOfChannels) ||
        (CurrentStreamParameters.SamplingFrequency1 != ParsedFrameHeader.SamplingFrequency1) ||
        (CurrentStreamParameters.Type == TypeLpcmInvalid) ||
        ((StreamType == TypeLpcmSPDIFIN) && (CurrentStreamParameters.NumberOfSamples != ParsedFrameHeader.NumberOfSamples)))
    {
        UpdateStreamParameters = true;
        Status = GetNewStreamParameters((void **) &StreamParameters);

        if (Status != FrameParserNoError)
        {
            SE_ERROR("Cannot get new stream parameters\n");
            return Status;
        }

        memcpy(StreamParameters, &ParsedFrameHeader, sizeof(LpcmAudioStreamParameters_t));
        memcpy(&CurrentStreamParameters, &ParsedFrameHeader, sizeof(LpcmAudioStreamParameters_t));
    }
    else
    {
        UpdateStreamParameters = false;
    }

    ParsedAudioParameters->Source.BitsPerSample = 0; // filled in by codec
    ParsedAudioParameters->Source.ChannelCount = 0;  // filled in by codec
    ParsedAudioParameters->Source.SampleRateHz = LpcmDVDSamplingFreq[ParsedFrameHeader.SamplingFrequency1];
    ParsedAudioParameters->SampleCount = ParsedFrameHeader.NumberOfSamples;
    ParsedAudioParameters->Organisation = 0; // filled in by codec

    ParsedAudioParameters->SpdifInProperties.Organisation     =   ParsedFrameHeader.SpdifInProperties.Organisation;
    ParsedAudioParameters->SpdifInProperties.ChannelCount     =   ParsedFrameHeader.SpdifInProperties.ChannelCount;
    ParsedAudioParameters->SpdifInProperties.DownMixInhibit   =   ParsedFrameHeader.SpdifInProperties.DownMixInhibit;
    ParsedAudioParameters->SpdifInProperties.LevelShiftValue  =   ParsedFrameHeader.SpdifInProperties.LevelShiftValue;
    ParsedAudioParameters->SpdifInProperties.LfePlaybackLevel =   ParsedFrameHeader.SpdifInProperties.LfePlaybackLevel;
    ParsedAudioParameters->SpdifInProperties.SpdifInLayout    =   ParsedFrameHeader.SpdifInProperties.SpdifInLayout;
    ParsedAudioParameters->SpdifInProperties.SamplingFrequency =   ParsedFrameHeader.SpdifInProperties.SamplingFrequency;

    Stream->Statistics().FrameParserAudioFrameSize = ParsedFrameHeader.Length;
    Stream->Statistics().FrameParserAudioSampleRate = ParsedAudioParameters->Source.SampleRateHz;
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     The reset reference frame list function
///
FrameParserStatus_t   FrameParser_AudioLpcm_c::ResetReferenceFrameList(void)
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
FrameParserStatus_t   FrameParser_AudioLpcm_c::PurgeQueuedPostDecodeParameterSettings(void)
{
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioLpcm_c::ProcessQueuedPostDecodeParameterSettings(void)
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
FrameParserStatus_t   FrameParser_AudioLpcm_c::GeneratePostDecodeParameterSettings(void)
{
    unsigned int SamplingFrequency = LpcmDVDSamplingFreq[CurrentStreamParameters.SamplingFrequency1];

    if (0 == SamplingFrequency)
    {
        SE_INFO(group_frameparser_audio, "SamplingFrequency 0 -- using default\n");
        SamplingFrequency = 48000;
    }

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
        TranslatePlaybackTimeNativeToNormalized(CodedFrameParameters->PlaybackTime, &ParsedFrameParameters->NormalizedPlaybackTime, CodedFrameParameters->SourceTimeFormat);
    }

    if (CodedFrameParameters->DecodeTimeValid)
    {
        ParsedFrameParameters->NativeDecodeTime         = CodedFrameParameters->DecodeTime;
        TranslatePlaybackTimeNativeToNormalized(CodedFrameParameters->DecodeTime, &ParsedFrameParameters->NormalizedDecodeTime, CodedFrameParameters->SourceTimeFormat);
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
    // Generate a decode time stamp to account for the SPDIF decoder latency
    //

    if (IndirectDecodeLatencyInSamples &&
        ValidTime(ParsedFrameParameters->NormalizedPlaybackTime) &&
        NotValidTime(ParsedFrameParameters->NormalizedDecodeTime))
    {
        unsigned long long DecodeLatencyInMicroseconds;
        DecodeLatencyInMicroseconds = IndirectDecodeLatencyInSamples + CurrentStreamParameters.NumberOfSamples;
        DecodeLatencyInMicroseconds *= 1000000;
        DecodeLatencyInMicroseconds /= SamplingFrequency;
//        if( ParsedFrameParameters->NormalizedPlaybackTime > DecodeLatencyInMicroseconds )
//        {
        ParsedFrameParameters->NormalizedDecodeTime = ParsedFrameParameters->NormalizedPlaybackTime -
                                                      DecodeLatencyInMicroseconds;
//        }
//        else
//        {
//            SE_ERROR("Cannot apply indirect decode latency because PTS is too small\n");
//        }
    }

    //
    // We can't fail after this point so this is a good time to provide a display frame index
    //
    ParsedFrameParameters->DisplayFrameIndex         = NextDisplayFrameIndex++;
    //
    // Use the super-class utilities to complete our housekeeping chores
    //
    HandleUpdateStreamParameters();
    GenerateNextFrameNormalizedPlaybackTime(CurrentStreamParameters.NumberOfSamples, SamplingFrequency);
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioLpcm_c::PrepareReferenceFrameList(void)
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioLpcm_c::UpdateReferenceFrameList(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioLpcm_c::ProcessReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioLpcm_c::ProcessReverseDecodeStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioLpcm_c::PurgeReverseDecodeUnsatisfiedReferenceStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioLpcm_c::PurgeReverseDecodeStack(void)
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for LPCM audio.
///
FrameParserStatus_t   FrameParser_AudioLpcm_c::TestForTrickModeFrameDrop(void)
{
    return FrameParserNoError;
}


