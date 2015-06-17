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

#ifndef H_FRAME_PARSER_AUDIO_DTSHD
#define H_FRAME_PARSER_AUDIO_DTSHD

#include "dtshd.h"
#include "frame_parser_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_AudioDtshd_c"

class FrameParser_AudioDtshd_c : public FrameParser_Audio_c
{
public:
    FrameParser_AudioDtshd_c(void);
    ~FrameParser_AudioDtshd_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);

    //
    // Stream specific functions
    //

    FrameParserStatus_t   ReadHeaders(void);
    FrameParserStatus_t   ResetReferenceFrameList(void);
    FrameParserStatus_t   PurgeQueuedPostDecodeParameterSettings(void);
    FrameParserStatus_t   PrepareReferenceFrameList(void);
    FrameParserStatus_t   ProcessQueuedPostDecodeParameterSettings(void);
    FrameParserStatus_t   GeneratePostDecodeParameterSettings(void);
    FrameParserStatus_t   UpdateReferenceFrameList(void);

    FrameParserStatus_t   ProcessReverseDecodeUnsatisfiedReferenceStack(void);
    FrameParserStatus_t   ProcessReverseDecodeStack(void);
    FrameParserStatus_t   PurgeReverseDecodeUnsatisfiedReferenceStack(void);
    FrameParserStatus_t   PurgeReverseDecodeStack(void);
    FrameParserStatus_t   TestForTrickModeFrameDrop(void);

    FrameParserStatus_t ParseFrameHeader(unsigned char *FrameHeader,
                                         DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                         int RemainingElementaryLength);

    static FrameParserStatus_t ParseSingleFrameHeader(unsigned char *FrameHeaderBytes,
                                                      DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                                      BitStreamClass_c *Bits,
                                                      unsigned int FrameHeaderLength,
                                                      unsigned char *OtherFrameHeaderBytes,
                                                      unsigned int RemainingFrameHeaderBytes,
                                                      DtshdParseModel_t Model,
                                                      unsigned char SelectedAudioPresentation);

    static FrameParserStatus_t ParseCoreHeader(BitStreamClass_c *Bits,
                                               DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                               unsigned int SyncWord);

    static void ParseExtensionSubstreamAssetHeader(BitStreamClass_c *Bits,
                                                   unsigned int *SamplingFrequency,
                                                   DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                                   unsigned int nuBits4ExSSFsize,
                                                   unsigned char SelectedAudioPresentation);

    static void ParseNumAssets(BitStreamClass_c *Bits,
                               DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                               bool               bStaticFieldsPresent,
                               unsigned int      *nuNumMixOutConfigs,
                               unsigned int      *nuNumMixOutCh,
                               unsigned int      *NumAssets,
                               unsigned int      *NumAudioPresent,
                               bool              *bMixMetadataEnable);

    static void ParsingDynamicMetadata(BitStreamClass_c *Bits,
                                       bool               bMixMetadataEnabl,
                                       bool               bMixMetadataPresent,
                                       bool               bEmbededStereoFlag,
                                       bool               bEmbededSixChFlag,
                                       unsigned int       nuNumMixOutConfigs,
                                       unsigned int       nuNumMixOutCh[4],
                                       unsigned int       nuTotalNumChs);

    static void ExtractMixData(BitStreamClass_c *Bits,
                               unsigned int      *nuNumMixOutCh,
                               unsigned int      *nDecCh,
                               unsigned int       ns,
                               unsigned int       nE);

    static void ParsingNavigationData(BitStreamClass_c *Bits,
                                      unsigned int       nAst,
                                      unsigned int       bOne2OneMapChannels2Speakers,
                                      bool               bMixMetadataEnabl,
                                      bool               bMixMetadataPresent,
                                      unsigned int       nuNumMixOutConfigs,
                                      unsigned int       nuNumMixOutCh[4],
                                      DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                      unsigned int                    nuBits4ExSSFsize);

    static void IsCompatibleCorePresent(BitStreamClass_c               *Bits,
                                        DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                        unsigned char                   SelectedAudioPresentation,
                                        unsigned int                    nuNumAudioPresent);

    static unsigned short CrcUpdate4BitsFast(unsigned char val, unsigned short crc);

    static unsigned int NumSpkrTableLookUp(unsigned int ChannelMask);

    static unsigned int CountBitsSet_to_1(unsigned int ChannelMask);

    static void GetSubstreamOnlyNumberOfSamples(BitStreamClass_c *Bits,
                                                DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                                unsigned char *FrameHeaderBytes);

private:
    DtshdAudioParsedFrameHeader_t ParsedFrameHeader;

    DtshdAudioStreamParameters_t  *StreamParameters;
    DtshdAudioStreamParameters_t   CurrentStreamParameters;
    DtshdAudioFrameParameters_t   *FrameParameters;

    bool FirstTime;
    bool DecodeLowestExtensionId;      ///< in case of multiple extensions, decode the lower extension id (by default)
    unsigned char DecodeExtensionId;   ///< if DecodeLowestExtensionId is false, decode the extension id indicated by this field
    unsigned char SelectedAudioPresentation;   ///< if DecodeLowestExtensionId is false, decode the extension id indicated by this field

    DISALLOW_COPY_AND_ASSIGN(FrameParser_AudioDtshd_c);
};

#endif /* H_FRAME_PARSER_AUDIO_DTSHD */
