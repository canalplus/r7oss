/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : frame_parser_audio_dtshd.cpp
Author :           Sylvain

Implementation of the DTSHD audio frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
06-Jun-07   Ported to PLayer2                               Sylvain

************************************************************************/

////////////////////////////////////////////////////////////////////////
///
/// \class FrameParser_AudioDtshd
/// \brief Frame parser for DTSHD audio
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "dtshd.h"
#include "frame_parser_audio_dtshd.h"
#include "collator_pes_audio_dtshd.h"
#include <stddef.h>

//
#ifdef ENABLE_FRAME_DEBUG
#undef ENABLE_FRAME_DEBUG
#define ENABLE_FRAME_DEBUG 0
#endif

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#undef FRAME_TAG
#define FRAME_TAG "DTSHD audio frame parser"

static BufferDataDescriptor_t     DtshdAudioStreamParametersBuffer = BUFFER_DTSHD_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     DtshdAudioFrameParametersBuffer = BUFFER_DTSHD_AUDIO_FRAME_PARAMETERS_TYPE;

//
// Lookup tables for DTSHD header parsing
//

static int DTSSampleFreqs[16] = 
{
    0,
    8000, 16000, 32000,     0,     0,
    11025, 22050, 44100,    0,     0,
    12000, 24000, 48000,    0,     0
};

static int DTSHDSampleFreqs[16] = 
{
    8000,   16000,  32000,  64000, 128000,
    22050,  44100,  88200, 176400, 352800,
    12000,  24000,  48000,  96000, 192000, 384000
};

unsigned short CrcLookup[16] = 
{
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF
};

unsigned char NumberOfChannelsLookupTable[16]=
{
    1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 2, 1, 2, 1, 2
};

const char* DTSExtSubStreamTypeName[]=
{
    "DTS_EXSUBSTREAM_CORE",
    "DTS_EXSUBSTREAM_XBR",
    "DTS_EXSUBSTREAM_XXCH",
    "DTS_EXSUBSTREAM_X96",
    "DTS_EXSUBSTREAM_LBR",
    "DTS_EXSUBSTREAM_XLL"
};

const char* DTSCoreSubStreamExtTypeName[7]=
{
    "XCh",
    "",
    "X96k",
    "",
    "",
    "",
    "XXCh"
};

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined macros
//


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
///
/// This function parses the core and substream extension of a dts-hd stream
/// \todo: parse all the core substream and extension assets to know exactly
/// how many components (and their types) are in this stream
///
/// \return Frame parser status code, FrameParserNoError indicates success.

FrameParserStatus_t FrameParser_AudioDtshd_c::ParseFrameHeader( unsigned char *FrameHeaderBytes, 
                                                                DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                                                int GivenFrameSize )
{
    int StreamIndex =  0, FrameSize = 0;
    DtshdAudioParsedFrameHeader_t NextParsedFrameHeader;

    memset(ParsedFrameHeader, 0, sizeof(DtshdAudioParsedFrameHeader_t));
  
    do
    {
        unsigned char* NextFrameHeader = &FrameHeaderBytes[StreamIndex];
        FrameParserStatus_t Status;
        DtshdParseModel_t ParsingModel;
        
        memset(&NextParsedFrameHeader, 0, sizeof(DtshdAudioParsedFrameHeader_t));
        
        if ((ParsedFrameHeader->NumberOfExtSubStreams == 0) && (DecodeLowestExtensionId))
        {
            ParsingModel.ParseType = ParseExtended;
        }
        else
        {
            ParsingModel.ParseType = ParseSmart;
            ParsingModel.ParseExtendedIndex = DecodeExtensionId;
        }

        // parse a single frame
        Status = FrameParser_AudioDtshd_c::ParseSingleFrameHeader( NextFrameHeader, 
                                                                   &NextParsedFrameHeader, 
                                                                   &Bits, 
                                                                   10, 
                                                                   NextFrameHeader + 10,
                                                                   GivenFrameSize - 10 - StreamIndex,
                                                                   ParsingModel,
                                                                   SelectedAudioPresentation );
      
        if (Status !=  FrameParserNoError) 
        {
            return (Status);
        }

        if (NextParsedFrameHeader.Type == TypeDtshdCore)
        {
            if (ParsedFrameHeader->NumberOfExtSubStreams > 0)
            {
                // case of an extension substream met before a core substream, skip this frame...
                ParsedFrameParameters->DataOffset = StreamIndex;
            }

            ParsedFrameHeader->CoreSamplingFrequency = NextParsedFrameHeader.CoreSamplingFrequency;
            ParsedFrameHeader->CoreNumberOfSamples = NextParsedFrameHeader.CoreNumberOfSamples;

            if (NextParsedFrameHeader.HasCoreExtensions)
            {
                ParsedFrameHeader->CoreExtensionType = NextParsedFrameHeader.CoreExtensionType;
                ParsedFrameHeader->HasCoreExtensions = true;
            }

            // we cannot rely on the header frame size, so get it from the collator
            int CoreSize = CodedFrameParameters->DataSpecificFlags;

            NextParsedFrameHeader.Length = CoreSize;
            ParsedFrameHeader->CoreSize = CoreSize;

            ParsedFrameHeader->NumberOfCoreSubStreams++;
        }
        else if (NextParsedFrameHeader.Type == TypeDtshdExt)
        {
            // check if the policy is to decode the lowset id extension
            if ((ParsedFrameHeader->NumberOfExtSubStreams == 0) && (DecodeLowestExtensionId))
            {
                DecodeExtensionId = NextParsedFrameHeader.SubStreamId;
            }

            // accumulate the properties of the selected extension only
            if (NextParsedFrameHeader.SubStreamId == DecodeExtensionId)
            {
                // copy the properties of the extension to the main properties structure
                memcpy(&ParsedFrameHeader->NumberOfAssets, 
                       &NextParsedFrameHeader.NumberOfAssets, 
                       sizeof(DtshdAudioParsedFrameHeader_t) - offsetof(DtshdAudioParsedFrameHeader_t, NumberOfAssets));
                
                // modify the core extension properties only if the core is present
                if ((ParsedFrameHeader->NumberOfCoreSubStreams > 0) && (NextParsedFrameHeader.HasCoreExtensions))
                {
                    ParsedFrameHeader->CoreExtensionType = NextParsedFrameHeader.CoreExtensionType;
                    ParsedFrameHeader->HasCoreExtensions = true;
                }
            }
            ParsedFrameHeader->NumberOfExtSubStreams++;
        }
        else
        {
            // what else could it be? raise an error!
            FRAME_ERROR("Unknown substream type: %d...\n", NextParsedFrameHeader.Type);
            return FrameParserError;
        }
      
        StreamIndex += NextParsedFrameHeader.Length;
        FrameSize += NextParsedFrameHeader.Length;
      
    } while (StreamIndex < GivenFrameSize);
  
    if ((FrameSize != GivenFrameSize) && (ParsedFrameParameters->DataOffset == 0))
    {
        FRAME_ERROR("Given frame size mismatch: %d (expected:%d)\n", FrameSize, GivenFrameSize);
        return FrameParserError;
    }
  
    ParsedFrameHeader->Length  =  FrameSize;

    { 
        int nbCore= ParsedFrameHeader->NumberOfCoreSubStreams;
        int nbSub = ParsedFrameHeader->NumberOfExtSubStreams;

        if (nbSub)
        {
            if (nbCore)
            {
                // as the extension might modify both the sample rate and the number of samples, apply 
                // the ratio between the core and the extension substream
                int ratio, NumberOfSamples = ParsedFrameHeader->CoreNumberOfSamples;
                int coresp, extsp;
                coresp = ParsedFrameHeader->CoreSamplingFrequency;
                extsp = ParsedFrameHeader->ExtensionSamplingFrequency;
                
                if (extsp > coresp)
                {
                    ratio = extsp / coresp;
                    NumberOfSamples *= ratio;
                }
                else
                {
                    ratio = coresp / extsp;
                    NumberOfSamples /= ratio;
                }
                ParsedFrameHeader->NumberOfSamples = NumberOfSamples;
            }
            else
            {
                // no core is present, guess the number of samples by parsing the lbr or the xll asset...
                FrameParser_AudioDtshd_c::GetSubstreamOnlyNumberOfSamples(&Bits, ParsedFrameHeader, FrameHeaderBytes);
            }
            ParsedFrameHeader->SamplingFrequency = ParsedFrameHeader->ExtensionSamplingFrequency;
        }
        else
        {
            if (ParsedFrameHeader->HasCoreExtensions && (ParsedFrameHeader->CoreExtensionType == DTS_CORE_EXTENSION_X96))
            {
                ParsedFrameHeader->CoreSamplingFrequency *= 2;
                ParsedFrameHeader->CoreNumberOfSamples *= 2;
            }
            ParsedFrameHeader->SamplingFrequency = ParsedFrameHeader->CoreSamplingFrequency;
            ParsedFrameHeader->NumberOfSamples   = ParsedFrameHeader->CoreNumberOfSamples;
        }
  
        if (FirstTime)
        {
            FRAME_TRACE("DTS stream properties: SamplingFreq: %d Hz,  FrameSize %d, Nb Samples: %d\n",
                        ParsedFrameHeader->SamplingFrequency, 
                        ParsedFrameHeader->Length, 
                        ParsedFrameHeader->NumberOfSamples);
	
            FRAME_TRACE("Core substreams: %d, Extension substreams: %d\n",nbCore, nbSub);

            if (nbCore && nbSub)
            {
                FRAME_TRACE("DTS core substream properties: SamplingFreq: %d Hz,  FrameSize %d, Nb Samples: %d\n",
                            ParsedFrameHeader->CoreSamplingFrequency, 
                            ParsedFrameHeader->CoreSize, 
                            ParsedFrameHeader->CoreNumberOfSamples);
            }

            if (nbSub)
            {
                FRAME_TRACE("Number of assets in the extension %d: %d\n",
                            DecodeExtensionId, ParsedFrameHeader->NumberOfAssets);
                
                for (int astIdx = 0; astIdx < ParsedFrameHeader->NumberOfAssets; astIdx++)
                {
                    FRAME_TRACE("Type of Coding Component present in asset %d:\n", astIdx);
                    unsigned int Code = ParsedFrameHeader->CodingComponent[astIdx];
                    
                    for (int id = CodeDtshdExSSCore, idx = 0; id <= CodeDtshdExtSSXLL; id <<= 1, idx ++)
                    {
                        if (Code&id)
                        {
                            FRAME_TRACE("\t%s\n", DTSExtSubStreamTypeName[idx]);
                        }
                    }
                } // end of asset loop
            }

            if (ParsedFrameHeader->HasCoreExtensions)
            {
                FRAME_TRACE("Type of extension present in core: %s\n", DTSCoreSubStreamExtTypeName[ParsedFrameHeader->CoreExtensionType]);
            }

            FirstTime = false;
        }
    }

    return FrameParserNoError;
}

// Process 4 bits of the message to update the CRC Value.
// Note that the data must be in the low nibble of val.
unsigned short FrameParser_AudioDtshd_c::CrcUpdate4BitsFast( unsigned char val, unsigned short crc )
{
    // Step one, extract the most significant 4 bits of the CRC
    unsigned char t = crc >> 12;
    // XOR in the message Data into the extracted bits
    t = t ^ val;
    // Shift the CRC Register left 4 bits
    crc = crc << 4;
    // Do the table lookups and XOR the result into the CRC Tables
    return(crc ^ CrcLookup[t]);
}


////////////////////////////////////////////////////////////////////////////
///
/// Examine the supplied frame header and extract the information contained
/// within.
///
/// This is a utility function shared by the frame parser and the equivalent
/// collator. Due to its shared nature this is a static method and does not
/// access any information not provided via the function arguments.
///
///
/// <b>DTS frame structure</b>
///
/// <pre>
/// SSSSSSSS SSSSSSSS SSSSSSSS SSSSSSSS
/// TDDDDDcP PPPPPPZZ ZZZZZZZZ ZZZZCCCC
/// CCFFFFRR RRRmrtdh AAAewLLf sNNNNpUU
/// UUUUUUUU UUUUUUiE EEEHHMMM
///
/// The 16-bit Header CRC check bytes (U) are dependant on CRC present flags (c) being true.
///
///  S  Sync word (32 bits)
///  T  Frame type (1 bit)
///  D  Deficit sample count (5 bits)
///  c  CRC present flag (1 bit)
///  P  Number of PCM sample blocks (7 bits)
///  Z  Primary frame byte size (14 bits)
///  C  Audio channel arrangement (6 bits)
///  F  Core audio sampling frequency (4 bits)
///  R  Transmission bit rate (5 bits)
///  m  Embedded down mix enabled (1 bit)
///  r  Embedded dynamic range flag (1 bit)
///  t  Embedded time stamp flag (1 bit)
///  d  Auxillary data flag (1 bit)
///  h  HDCD (1 bit)
///  A  Extension audio descriptor flag (3 bits)
///  e  Extended coding flag (1 bit)
///  w  Audio sync word insertion flag (1 bit)
///  L  Low frequency effects flag (2 bits)
///  f  Front sum/difference flag (1 bit)
///  s  Surrounds sum/difference flag (1 bit)
///  N  Dialog normalization parameters/Unspecified (4 bits)
///  p  Predictor history flag switch (1 bit)
///  U  Header CRC check bytes (16 bits)  OPTIONAL
///  i  Multirate interpolator switch (1 bit)
///  E  Encoder software revision (4 bits)
///  H  Copy history (2 bits)
///  M  Source PCM resolution (3 bits)
///
/// </pre>
///
/// \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t FrameParser_AudioDtshd_c::ParseSingleFrameHeader( unsigned char                 *FrameHeaderBytes, 
                                                                      DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,  
                                                                      BitStreamClass_c		        *Bits,
                                                                      unsigned int                   FrameHeaderLength,
                                                                      unsigned char                 *OtherFrameHeaderBytes,
                                                                      unsigned int                   RemainingFrameHeaderBytes,
                                                                      DtshdParseModel_t              ParseModel,
                                                                      unsigned char                  SelectedAudioPresentation )
{
  
    Bits->SetPointer( FrameHeaderBytes );
    unsigned int SyncWord            = Bits->Get(32);
    unsigned int NumberOfSamples = 0, FrameSize = 0;

    if ( (SyncWord == DTSHD_START_CODE_CORE) ||
         ((SyncWord == DTSHD_START_CODE_CORE_14_1) && (Bits->Get(6) == DTSHD_START_CODE_CORE_14_2)) )
    {
        FrameParserStatus_t status;
        
        status = FrameParser_AudioDtshd_c::ParseCoreHeader(Bits, 
                                                           ParsedFrameHeader,
                                                           SyncWord);
        
        if (status != FrameParserNoError)
            return status;

        ParsedFrameHeader->Type = TypeDtshdCore;
        FrameSize = ParsedFrameHeader->CoreSize;
        NumberOfSamples = ParsedFrameHeader->CoreNumberOfSamples;
    } 
    else if (SyncWord == DTSHD_START_CODE_SUBSTREAM) 
    {
        // This is a substream extension (pure DTS-HD)
        // so the method to get the length of this frame is
        // to go calculate the checksum on the header bytes to be sure 
        // we really met a substream extension synchronization


        ParsedFrameHeader->Type        = TypeDtshdExt;
        Bits->FlushUnseen(8);                                 //UserDefinedBits
      
        unsigned int nuExtSSIndex      = Bits->Get(2);
        ParsedFrameHeader->SubStreamId = nuExtSSIndex;
        bool bHeaderSizeType           = Bits->Get(1);
        unsigned char nuBits4Header    = 8 + 4*bHeaderSizeType;
        unsigned char nuBits4ExSSFsize = 16 + 4*bHeaderSizeType;

        unsigned int nuExtSSHeaderSize = Bits->Get(nuBits4Header) + 1;
        unsigned int nuExtSSFSize      = Bits->Get(nuBits4ExSSFsize) + 1;
        unsigned int SamplingFrequency = 0;

        // calculate the checksum to be sure about the synchronization (if we have enough header bytes...)

        if (ParseModel.ParseType == ParseForSynchro)
        {
            if ((FrameHeaderLength + RemainingFrameHeaderBytes) < nuExtSSHeaderSize)
            {
                FRAME_DEBUG("Not enough header bytes to compute checksum %d vs %d required...\n", FrameHeaderLength + RemainingFrameHeaderBytes, nuExtSSHeaderSize);
            }
            else
            {
                unsigned int i;
                // Initialize the CRC to 0xFFFF as per specification
                unsigned short crc = 0xffff;
                // crc is calculated on substream header from position 5 (nuExtSSIndex) 
                // to position ByteAlign inclusive (position of nCRC16ExtSSHeader)
                unsigned char * HdrPtr = FrameHeaderBytes + 5;
	      
                for( i = 5; i < FrameHeaderLength; i++)
                {
                    crc = CrcUpdate4BitsFast( *HdrPtr >> 4, crc ); // High nibble first
                    crc = CrcUpdate4BitsFast( *HdrPtr & 0x0F, crc ); // Low nibble
		  
                    HdrPtr++;
                }
	      
                HdrPtr = OtherFrameHeaderBytes;
	      
                for( ; (i < nuExtSSHeaderSize - 2); i++)
                {
                    crc = CrcUpdate4BitsFast( *HdrPtr >> 4, crc ); // High nibble first
                    crc = CrcUpdate4BitsFast( *HdrPtr & 0x0F, crc ); // Low nibble
		  
                    HdrPtr++;
                }
	      
                unsigned short crc_ref = (*HdrPtr << 8) + *(HdrPtr+1);
                if (crc_ref != crc)
                {
                    FRAME_ERROR("bad checksum in extension substream: 0x%x vs 0x%x \n", crc_ref, crc);
                    return FrameParserError;
                }
            }
        }
        else if ( (ParseModel.ParseType == ParseExtended) || 
                  ((ParseModel.ParseType == ParseSmart) && (ParseModel.ParseExtendedIndex == nuExtSSIndex)) )
        {
            ParsedFrameHeader->ExtensionHeaderSize = nuExtSSHeaderSize;
            FrameParser_AudioDtshd_c::ParseExtensionSubstreamAssetHeader(Bits, &SamplingFrequency, ParsedFrameHeader, nuBits4ExSSFsize, SelectedAudioPresentation);
            ParsedFrameHeader->ExtensionSamplingFrequency = SamplingFrequency;
        }
	  
        FrameSize = nuExtSSFSize;
    } 
    else 
    {
        FRAME_ERROR("Unrecognized frame synchronization: 0x%x \n", SyncWord);
        return FrameParserError;
    }
  
    //
    ParsedFrameHeader->NumberOfSamples = NumberOfSamples;
    ParsedFrameHeader->Length = FrameSize; 
  
    //
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
///
/// This function parses the regular dts core header and retrieves the sampling 
/// frequency, the frame size, and the number of samples of this stream
///
/// \return a frame status

FrameParserStatus_t FrameParser_AudioDtshd_c::ParseCoreHeader(BitStreamClass_c * Bits, 
                                                              DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                                              unsigned int SyncWord)
{
    unsigned int NumberOfPcmSampleBlocks, PrimaryFrameByteSize, CoreAudioSamplingFrequency;
    int SkipBitsCount = (SyncWord == DTSHD_START_CODE_CORE_14_1)?2:0;
    bool IsNormalFrame = Bits->Get(1); // FTYPE
    Bits->FlushUnseen(5+1);  // SHORT, CPF
    NumberOfPcmSampleBlocks      = Bits->Get(4);
    Bits->FlushUnseen(SkipBitsCount);
    NumberOfPcmSampleBlocks      = (NumberOfPcmSampleBlocks << 3) + Bits->Get(3);
    PrimaryFrameByteSize         = Bits->Get(11);
    Bits->FlushUnseen(SkipBitsCount);
    PrimaryFrameByteSize         = (PrimaryFrameByteSize << 3) + Bits->Get(3);
    Bits->FlushUnseen(6);       //  AMODE
    CoreAudioSamplingFrequency   = Bits->Get(4);
    Bits->FlushUnseen(1+SkipBitsCount+4+1+1+1+1+1); // RATE, FIXEDBIT, DYNF, TIMEF, AUXF, HDCD
    ParsedFrameHeader->CoreExtensionType           = (DtshdCoreExtensionType_t) Bits->Get(3);
    ParsedFrameHeader->HasCoreExtensions           = Bits->Get(1);
    
    // sanity checks
    if ( ((NumberOfPcmSampleBlocks < 5) && (NumberOfPcmSampleBlocks > 127)) ||
         (IsNormalFrame && (NumberOfPcmSampleBlocks != 7) && (NumberOfPcmSampleBlocks != 15) && 
          (NumberOfPcmSampleBlocks != 31) && (NumberOfPcmSampleBlocks != 63) && (NumberOfPcmSampleBlocks != 127)) )
    {
        FRAME_ERROR("NumberOfPcmSampleBlocks out of range (%d)\n",
                    NumberOfPcmSampleBlocks);
        return FrameParserError;
    }

    if (PrimaryFrameByteSize < 95 || PrimaryFrameByteSize > 16383) 
    {
        FRAME_ERROR("Invalid Frame Size\n", PrimaryFrameByteSize);
        return FrameParserError;
    }
    
    ParsedFrameHeader->CoreSize = PrimaryFrameByteSize + 1;
    ParsedFrameHeader->CoreSamplingFrequency = DTSSampleFreqs[CoreAudioSamplingFrequency];
    
    ParsedFrameHeader->CoreNumberOfSamples = (NumberOfPcmSampleBlocks + 1) * 32;
    
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
///
/// This function parses the substream extension common asset header of a 
/// dts-hd stream .The goal is to retireve the sampling frequency of this stream
/// but also the type of coding components inside (e.g. xll, xch, ...)
///
/// \return void

void FrameParser_AudioDtshd_c::ParseExtensionSubstreamAssetHeader( BitStreamClass_c *Bits,
                                                                   unsigned int * SamplingFrequency,
                                                                   DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                                                   unsigned int nuBits4ExSSFsize,
                                                                   unsigned char SelectedAudioPresentation)
{
    bool bStaticFieldsPresent = Bits->Get(1);
    unsigned int nuNumAudioPresnt, nuNumAssets;
    bool bMixMetadataEnabl = false, bMixMetadataPresent = false;
    unsigned int nuNumMixOutConfigs = 0;
    unsigned int nuNumMixOutCh[4] ={ 0, 0, 0 ,0};
    unsigned int nuTotalNumChs = 0;
    bool bOne2OneMapChannels2Speakers = false;
    
    if (bStaticFieldsPresent)
    {
        FRAME_DEBUG("Static Field present\n");
        unsigned int Temp;
        Bits->FlushUnseen(5); //nuRefClockCode, nuExtSSFrameDurationCode
        Temp = Bits->Get(1);  // bTimeStampFlag
        if (Temp)
        {
            Bits->FlushUnseen(36);
        }
        nuNumAudioPresnt = Bits->Get(3) + 1;
        nuNumAssets = Bits->Get(3) + 1;
        unsigned int nuActiveExSSMask[8];
        for (unsigned int nAuPr = 0; nAuPr < nuNumAudioPresnt; nAuPr ++)
        {
            nuActiveExSSMask[nAuPr] = Bits->Get(ParsedFrameHeader->SubStreamId + 1);
        }
        for (unsigned int nAuPr = 0; nAuPr < nuNumAudioPresnt; nAuPr ++)
        {
            for (int nSS = 0; nSS < (ParsedFrameHeader->SubStreamId + 1); nSS++)
            {
                if ((nuActiveExSSMask[nAuPr] >> nSS & 0x1) == 1)
                {
                    Bits->FlushUnseen(8); // nuActiveAssetMask
                }
            }
        }
        bMixMetadataEnabl = Bits->Get(1);
        if (bMixMetadataEnabl)
        {
            Bits->FlushUnseen(2); //nuMixMetadataAdjLevel
            unsigned int nuBits4MixOutMask = (Bits->Get(2) + 1) << 2;
            nuNumMixOutConfigs = Bits->Get(2) + 1;
            unsigned int nuMixOutChMasks[4];
            for (unsigned int ns = 0; ns < nuNumMixOutConfigs; ns++)
            {
                nuMixOutChMasks[ns] = Bits->Get(nuBits4MixOutMask);
                nuNumMixOutCh[ns] = FrameParser_AudioDtshd_c::NumSpkrTableLookUp(nuMixOutChMasks[ns]);
            }
        }
    }
    else
    {
        nuNumAudioPresnt = nuNumAssets = 1;
    }

    FRAME_DEBUG("Number of audio presentations: %d, assets: %d\n", nuNumAudioPresnt, nuNumAssets);

    for (unsigned int nAst = 0; nAst < nuNumAssets; nAst++)
    {
        ParsedFrameHeader->AssetSize[nAst] = Bits->Get(nuBits4ExSSFsize); // nuAssetFsize
    }
    bool bEmbededStereoFlag = false, bEmbededSixChFlag = false;
    
    unsigned char * Ptr;
    unsigned int BitsInBytes;

    Bits->GetPosition(&Ptr, &BitsInBytes);

    for (unsigned int nAst = 0; nAst < nuNumAssets; nAst++)
    {
        unsigned int nuAssetDescriptFsize;
        
        nuAssetDescriptFsize = Bits->Get(9)+1;
        Bits->FlushUnseen(3);//unsigned int nuAssetIndex = Bits->Get(3);

        if (bStaticFieldsPresent)
        {
            bool bAssetTypeDescrPresent = Bits->Get(1);
            if (bAssetTypeDescrPresent)
            {
                Bits->FlushUnseen(4); // nuAssetTypeDescriptor = Bits->Get(4);
            }
            bool bLanguageDescrPresent = Bits->Get(1);
            if (bLanguageDescrPresent)
            {
                Bits->FlushUnseen(24); // LanguageDescriptor = Bits->Get(24);
            }
            bool bInfoTextPresent = Bits->Get(1);
            if (bInfoTextPresent)
            {
                unsigned int nuInfoTextByteSize = Bits->Get(10)+1;
                Bits->FlushUnseen(nuInfoTextByteSize*8); //InfoTextString
            }
            Bits->FlushUnseen(5); // unsigned int nuBitResolution = Bits->Get(5) + 1;
            *SamplingFrequency = DTSHDSampleFreqs[Bits->Get(4)]; // nuMaxSampleRate
            nuTotalNumChs = Bits->Get(8)+1;
            bOne2OneMapChannels2Speakers = Bits->Get(1);
            if (bOne2OneMapChannels2Speakers)
            {
                if (nuTotalNumChs>2)
                {
                    bEmbededStereoFlag = Bits->Get(1);
                }
                else
                {
                    bEmbededStereoFlag = false;
                }
                if (nuTotalNumChs>6)
                {
                    bEmbededSixChFlag = Bits->Get(1);
                }
                else
                {
                    bEmbededSixChFlag = false;
                }
                bool bSpkrMaskEnabled = Bits->Get(1);
                unsigned int nuNumBits4SAMask = 0;
                if (bSpkrMaskEnabled)
                {
                    nuNumBits4SAMask = (Bits->Get(2)+1)<<2;
                    Bits->FlushUnseen(nuNumBits4SAMask);// unsigned int nuSpkrActivityMask = Bits->Get(nuNumBits4SAMask);
                }
                unsigned int nuNumSpkrRemapSets = Bits->Get(3);
                unsigned int nuStndrSpkrLayoutMask[7];
                for (unsigned int ns=0; ns<nuNumSpkrRemapSets; ns++)
                {
                    nuStndrSpkrLayoutMask[ns] = Bits->Get(nuNumBits4SAMask);
                }
                for (unsigned int ns=0;ns<nuNumSpkrRemapSets; ns++)
                {
                    unsigned int nuNumSpeakers = FrameParser_AudioDtshd_c::NumSpkrTableLookUp(nuStndrSpkrLayoutMask[ns]);
                    unsigned int nuNumDecCh4Remap[7];
                    unsigned int nuRemapDecChMask[25][7];
                    nuNumDecCh4Remap[ns] = Bits->Get(5)+1;
                    for (unsigned int nCh=0; nCh<nuNumSpeakers; nCh++)
                    { // Output channel loop
                        nuRemapDecChMask[ns][nCh] = Bits->Get(nuNumDecCh4Remap[ns]);
                        unsigned int nCoef = FrameParser_AudioDtshd_c::CountBitsSet_to_1 (nuRemapDecChMask[ns][nCh]);
                        for (unsigned int nc=0; nc<nCoef; nc++)
                        {
                            Bits->FlushUnseen(5);// nuSpkrRemapCodes[ns][nCh][nc] = Bits->Get(5);
                        }
                    } // End output channel loop
                } // End nuNumSpkrRemapSets loop   
            } // End of if (bOne2OneMapChannels2Speakers) 
            else
            { // No speaker feed case 
                bEmbededStereoFlag = false;
                bEmbededSixChFlag = false;
                Bits->FlushUnseen(3);//nuRepresentationType = Bits->Get(3);
            }
        }//End of if (bStaticFieldsPresent)

        // parsing of dynamic metadata 
        {
            bool bDRCCoefPresent = Bits->Get(1);
            if (bDRCCoefPresent)
            {
                Bits->FlushUnseen(8); // nuDRCCode = Bits->Get(8);
            }
            bool bDialNormPresent = Bits->Get(1);
            if (bDialNormPresent)
            {
                Bits->FlushUnseen(5); //nuDialNormCode = Bits->Get(5);
            }
            if (bDRCCoefPresent && bEmbededStereoFlag)
            {
                Bits->FlushUnseen(8);//   nuDRC2ChDmixCode = Bits->Get(8);
            }

            if (bMixMetadataEnabl)
            {
                bMixMetadataPresent = Bits->Get(1);
            }
            else
            {
                bMixMetadataPresent = false;
            }

            if (bMixMetadataPresent)
            {
                FRAME_DEBUG("Mix metadata present\n");
                
                Bits->FlushUnseen(1 + 6); //bExternalMixFlag, nuPostMixGainAdjCode
                unsigned int nuControlMixerDRC = Bits->Get(2);

                if (nuControlMixerDRC <3)
                {
                    Bits->FlushUnseen(3);// nuLimit4EmbededDRC = Bits->Get(3);
                }
                if (nuControlMixerDRC ==3)
                {
                    Bits->FlushUnseen(8); //nuCustomDRCCode = Bits->Get(8);
                }
                bool bEnblPerChMainAudioScale = Bits->Get(1);

                for (unsigned int ns=0; ns<nuNumMixOutConfigs; ns++)
                {
                    if (bEnblPerChMainAudioScale)
                    {
                        for (unsigned int nCh=0; nCh<nuNumMixOutCh[ns]; nCh++)
                        {
                            Bits->FlushUnseen(6);//nuMainAudioScaleCode[ns][nCh] = Bits->Get(6);
                        }
                    }
                    else
                    {
                        Bits->FlushUnseen(6); //nuMainAudioScaleCode[ns][0] = Bits->Get(6);
                    }
                }
                unsigned int nEmDM = 1;
                unsigned int nDecCh[4];
                nDecCh[0] = nuTotalNumChs;
                if (bEmbededSixChFlag)
                {
                    nDecCh[nEmDM] = 6;
                    nEmDM = nEmDM + 1;
                }
                if (bEmbededStereoFlag)
                {
                    nDecCh[nEmDM] = 2;
                    nEmDM = nEmDM + 1;
                }

                unsigned int nuMixMapMask[4][4][8];
                
                for (unsigned int ns=0; ns<nuNumMixOutConfigs; ns++)
                { // Configuration Loop
                    for ( unsigned int nE=0; nE<nEmDM; nE++)
                    { // Embeded downmix loop
                        for (unsigned int nCh=0; nCh<nDecCh[nE]; nCh++)
                        { // Supplemental Channel Loop
                            nuMixMapMask[ns][nE][nCh]= Bits->Get(nuNumMixOutCh[ns]);
                            unsigned int nuNumMixCoefs[4][4][8];
                            nuNumMixCoefs[ns][nE][nCh] = FrameParser_AudioDtshd_c::CountBitsSet_to_1(nuMixMapMask[ns][nE][nCh]);
                            for (unsigned int nC=0; nC<nuNumMixCoefs[ns][nE][nCh]; nC++)
                            {
                                Bits->FlushUnseen(6); // nuMixCoeffs[ns][nE][nCh][nC] = Bits->Get(6);
                            }
                        } // End of Embeded downmix loop
                    } // End supplemental channel loop      
                } // End configuration loop         
            }// End if (bMixMetadataPresent)        
        } // end of dynamic metadata parsing
        
        //parsing of decoder navigation data    
        {
            unsigned int nuCodingMode = Bits->Get(2);
            unsigned int nuCoreExtensionMask = 0;
            
            switch (nuCodingMode) 
            {
                case 0:
                {
                    nuCoreExtensionMask = Bits->Get(12);
                    if (nuCoreExtensionMask & DTS_EXSUBSTREAM_CORE)
                    {
                        ParsedFrameHeader->ExtSubCoreCodingComponentSize[nAst] = Bits->Get(14)+1; // nuExSSCoreFsize
                    }
                    bool bExSSCoreSyncPresent = Bits->Get(1);
                    if (bExSSCoreSyncPresent)
                    {
                        Bits->FlushUnseen(2);//nuExSSCoreSyncDistInFrames = 1<<(Bits->Get(2));
                    }
                    if (nuCoreExtensionMask & DTS_EXSUBSTREAM_XBR)
                    {
                        Bits->FlushUnseen(14);//nuExSSXBRFsize = Bits->Get(14)+1;
                    }
                    if (nuCoreExtensionMask & DTS_EXSUBSTREAM_XXCH)
                    {
                        Bits->FlushUnseen(14);//nuExSSXXCHFsize = Bits->Get(14)+1;
                    }
                    if (nuCoreExtensionMask & DTS_EXSUBSTREAM_X96)
                    {
                        Bits->FlushUnseen(12);//nuExSSX96Fsize = Bits->Get(12)+1;
                    }
                    if (nuCoreExtensionMask & DTS_EXSUBSTREAM_LBR)
                    {
                        Bits->FlushUnseen(14);//nuExSSLBRFsize = Bits->Get(14)+1;
                        bool bExSSLBRSyncPresent = Bits->Get(1);
                        if (bExSSLBRSyncPresent)
                        {
                            Bits->FlushUnseen(2);//nuExSSLBRSyncDistInFrames = 1<<(Bits->Get(2));
                        }
                    }
                    if (nuCoreExtensionMask & DTS_EXSUBSTREAM_XLL)
                    {

                        Bits->FlushUnseen(nuBits4ExSSFsize); //nuExSSXLLFsize = Bits->Get(nuBits4ExSSFsize)+1;
                        bool bExSSXLLSyncPresent = Bits->Get(1);

                        if (bExSSXLLSyncPresent)
                        {
                            Bits->FlushUnseen(4); //nuPeakBRCntrlBuffSzkB = Bits->Get(4)<<4;
                            unsigned int nuBitsInitDecDly = Bits->Get(5)+1;
                            Bits->FlushUnseen(nuBitsInitDecDly); //nuInitLLDecDlyFrames=Bits->Get(nuBitsInitDecDly);
                            Bits->FlushUnseen(nuBits4ExSSFsize); //nuExSSXLLSyncOffset=Bits->Get(nuBits4ExSSFsize);
                        }
                    }
                    if (nuCoreExtensionMask & RESERVED_1)
                    {
                        Bits->FlushUnseen(16); //Ignore = Bits->Get(16);
                    }
                    if (nuCoreExtensionMask & RESERVED_2)
                    {
                        Bits->FlushUnseen(16);//Ignore = Bits->Get(16);
                    }
                    ParsedFrameHeader->CodingComponent[nAst] = (DtshdCodingComponentType_t)nuCoreExtensionMask;
                    break;
                }
                case 1:
                {
                    Bits->FlushUnseen(nuBits4ExSSFsize); //nuExSSXLLFsize = Bits->Get(nuBits4ExSSFsize)+1;
                    bool bExSSXLLSyncPresent = Bits->Get(1);
                    if (bExSSXLLSyncPresent)
                    {
                        Bits->FlushUnseen(4); //nuPeakBRCntrlBuffSzkB = Bits->Get(4)<<4;
                        unsigned int nuBitsInitDecDly = Bits->Get(5)+1;
                        Bits->FlushUnseen(nuBitsInitDecDly); //nuInitLLDecDlyFrames=Bits->Get(nuBitsInitDecDly);
                        Bits->FlushUnseen(nuBits4ExSSFsize); //nuExSSXLLSyncOffset=Bits->Get(nuBits4ExSSFsize);
                    }
                    ParsedFrameHeader->CodingComponent[nAst] = DTS_EXSUBSTREAM_XLL;
                    break;
                }
                case 2:
                {
                    Bits->FlushUnseen(14); //nuExSSLBRFsize = Bits->Get(14)+1;
                    bool bExSSLBRSyncPresent = Bits->Get(1);
                    if (bExSSLBRSyncPresent)
                    {
                        Bits->FlushUnseen(2); //nuExSSLBRSyncDistInFrames = 1<<(Bits->Get(2));
                    }
                    ParsedFrameHeader->CodingComponent[nAst] = DTS_EXSUBSTREAM_LBR;
                    break;
                }
                case 3:
                {
                    Bits->FlushUnseen(14); //nuExSSAuxFsize = Bits->Get(14)+1;
                    Bits->FlushUnseen(8); //nuAuxCodecID = Bits->Get(8);
                    bool bExSSAuxSyncPresent = Bits->Get(1);
                    if (bExSSAuxSyncPresent)
                    {
                        Bits->FlushUnseen(3); //nuExSSAuxSyncDistInFrames = Bits->Get(3)+1;
                    }
                    break;
                }
                default:
                    break;
            }

            if ( ((nuCodingMode==0) && (nuCoreExtensionMask & DTS_EXSUBSTREAM_XLL)) || (nuCodingMode==1) )
            {
                Bits->FlushUnseen(3); //nuDTSHDStreamID = Bits->Get(3);
            }
            bool bOnetoOneMixingFlag = false;
            
            if ((bOne2OneMapChannels2Speakers == true) && (bMixMetadataEnabl ==true) && (bMixMetadataPresent==false))
            {
                bOnetoOneMixingFlag = Bits->Get(1);
            }
            if (bOnetoOneMixingFlag) 
            {
                bool bEnblPerChMainAudioScale = Bits->Get(1);
                for (unsigned int ns=0; ns<nuNumMixOutConfigs; ns++)
                {
                    if (bEnblPerChMainAudioScale)
                    {
                        for (unsigned int nCh=0; nCh<nuNumMixOutCh[ns]; nCh++)
                        {
                            Bits->FlushUnseen(6); //nuMainAudioScaleCode[ns][nCh] = Bits->Get(6);
                        }
                    }
                    else
                    {
                        Bits->FlushUnseen(6);// nuMainAudioScaleCode[ns][0] = Bits->Get(6);
                    }
                }
            } // End ofbOnetoOneMixingFlag==true condition
            //            bDecodeAssetInSecondaryDecoder = Bits->Get(1);
            //            Reserved = Bits->Get(¡Ä);
            //            ByteAlign = Bits->Get(0 ¡Ä 7);
        }
        
        ParsedFrameHeader->NumberOfAssets++;
        // add the asset header size to the last asset header pointer
        Ptr += nuAssetDescriptFsize;
        Bits->SetPointer(Ptr);
    } //number of asset header loop 


    {
        // check for the presence of any compatible core substream
        bool bBcCorePresent[8];
        unsigned int CoreExtSSIndex = 0, CoreAssetIndex = 0;
        unsigned int CoreOffset = 0, CoreSize = 0;
        
        for (unsigned int nAuPr=0; nAuPr<nuNumAudioPresnt; nAuPr++)
        {
            bBcCorePresent[nAuPr] = Bits->Get(1);
        }
        for (unsigned int nAuPr=0; nAuPr<nuNumAudioPresnt; nAuPr++)
        {
            if (bBcCorePresent[nAuPr])
            {
                if (SelectedAudioPresentation == nAuPr)
                {
                    CoreExtSSIndex = Bits->Get(2);
                }
                else
                {
                    Bits->FlushUnseen(2);
                }
            }
        }
        for (unsigned int nAuPr=0; nAuPr<nuNumAudioPresnt; nAuPr++)
        {
            if (bBcCorePresent[nAuPr])
            {
                if (SelectedAudioPresentation == nAuPr)
                {
                    CoreAssetIndex = Bits->Get(3);
                }
                else
                {
                    Bits->FlushUnseen(3);
                }
            }
        }

        // now check if any backward core substream exists in our 
        // current selected presentation/extension couple
        if (nuNumAudioPresnt > 1)
        {
            if (bBcCorePresent[SelectedAudioPresentation])
            {
                // check if this is the extension substream selected
                if (CoreExtSSIndex == ParsedFrameHeader->SubStreamId)
                {
                    // get the core substream offset
                    for (unsigned int astIdx = 0; astIdx < CoreAssetIndex; astIdx ++)
                    {
                        CoreOffset += ParsedFrameHeader->AssetSize[astIdx];
                    }
                    CoreSize = ParsedFrameHeader->ExtSubCoreCodingComponentSize[CoreAssetIndex];
                }
            }
        }
        else 
        {
            // in case the number of audio presentation is 1, 
            // the number of assets should be 1 also
            if (ParsedFrameHeader->CodingComponent[0] & DTS_EXSUBSTREAM_CORE)
            {
                CoreSize = ParsedFrameHeader->ExtSubCoreCodingComponentSize[0];
                // the coding componenet is generally the first.
                CoreOffset = 0;
            }
        }
        ParsedFrameHeader->SubStreamCoreOffset = CoreOffset + ParsedFrameHeader->ExtensionHeaderSize;
        ParsedFrameHeader->SubStreamCoreSize = CoreSize;
    }
}

// /////////////////////////////////////////////////////////////////////////
///
/// This static function returns the number of channels 
/// represented by a channel mask (each bit indicating a channel)
///
/// \return the number of channels from the channel mask

unsigned int FrameParser_AudioDtshd_c::NumSpkrTableLookUp(unsigned int ChannelMask)
{
    unsigned int NumberOfChannels = 0;
    
    for (int i = 0; i < 16; i++)
    {
        if (ChannelMask & (1 << i))
        {
            NumberOfChannels += NumberOfChannelsLookupTable[i];
        }
    }
    return NumberOfChannels;
}

// /////////////////////////////////////////////////////////////////////////
///
/// This static function returns the number of bits 
/// set to one in a 32 bit value
///
/// \return the number of bits set to one
unsigned int FrameParser_AudioDtshd_c::CountBitsSet_to_1(unsigned int ChannelMask)
{
    unsigned int NumberOfBits = 0;
    
    for (int i = 0; i < 32; i++)
    {
        if ((ChannelMask >> i) & 0x1)
        {
            NumberOfBits ++;
        }
    }
    return NumberOfBits;
}

// /////////////////////////////////////////////////////////////////////////
///
/// This function returns the number of samples contained in very specific 
/// dtshd streams that have no core. In that case, this function parses the 
/// xll or lbr asset header and retrieves the number of samples and the sampling 
/// frequency for lbr
///
/// \returns void

void FrameParser_AudioDtshd_c::GetSubstreamOnlyNumberOfSamples(BitStreamClass_c * Bits, 
                                                               DtshdAudioParsedFrameHeader_t * ParsedFrameHeader, 
                                                               unsigned char * FrameHeaderBytes)
{
    // set the pointer to the extension data (skip the header)
    unsigned char * BasePtr = FrameHeaderBytes + ParsedFrameHeader->ExtensionHeaderSize;
    
    Bits->SetPointer(BasePtr);

    unsigned int Sync = Bits->Get(32);
    
    // in such an extension (i.e. without core), one can find xll, lbr or core substream coding component only
    switch (Sync)
    {
        case DTSHD_START_CODE_XLL:
        {
            Bits->FlushUnseen(4); //nVersion
            Bits->FlushUnseen(8); //nHeaderSize
            unsigned int nBits4FrameFsize = Bits->Get(5) + 1;
            Bits->FlushUnseen(nBits4FrameFsize); //nLLFrameSize
            Bits->FlushUnseen(4); // nNumChSetsInFrame
            unsigned int nSegmentsInFrame = 1 << Bits->Get(4);
            unsigned int nSmplInSeg = 1 << Bits->Get(4);
            unsigned int NbSamples = nSmplInSeg * nSegmentsInFrame;
            unsigned int ratio = ParsedFrameHeader->ExtensionSamplingFrequency / 96000;
            if (ratio)
            {
                NbSamples *= ratio;
            }
            ParsedFrameHeader->NumberOfSamples = NbSamples;
            break;
        }
        case DTSHD_START_CODE_LBR:
        {
            unsigned int nLBRSampleRateCode = 0;
            // Extract LBR header type:
            if (Bits->Get(8) == 2)
            { // LBR decoder initialization data follows:
                nLBRSampleRateCode = Bits->Get(8);
            }
            unsigned int nSampleRate = DTSHDSampleFreqs[nLBRSampleRateCode];
            unsigned int FreqRange;
            if (nSampleRate < 14000)
            {
                FreqRange = 0;
            } 
            else if (nSampleRate < 28000)
            {
                FreqRange = 1;
            }
            else if (nSampleRate < 50000)
            {
                FreqRange = 2;
            }
            else if (nSampleRate < 100000)
            {
                FreqRange = 3;
            }
            else
            {
                FreqRange = 4;
            }
            // magic function (taken from the firmware to get the number of samples value...)
            ParsedFrameHeader->NumberOfSamples = 16 * (1 << (6 + FreqRange));
            //            ParsedFrameHeader->ExtensionSamplingFrequency = nSampleRate;
            break;
        }
        case DTSHD_START_CODE_SUBSTREAM_CORE:
        {
            FrameParser_AudioDtshd_c::ParseCoreHeader(Bits, 
                                                      ParsedFrameHeader,
                                                      DTSHD_START_CODE_CORE);

            ParsedFrameHeader->NumberOfSamples = ParsedFrameHeader->CoreNumberOfSamples;
        }
        break;
        default:
            FRAME_ERROR("Found coding component different from xll, lbr or core in extension-only substream (Sync: 0x%x)!\n", Sync);
            break;
    }
}

///////////////////////////////////////////////////////////////////////////
///
///     Constructor
///
FrameParser_AudioDtshd_c::FrameParser_AudioDtshd_c( void )
{
    Configuration.FrameParserName		= "AudioDtshd";

    Configuration.StreamParametersCount		= 32;
    Configuration.StreamParametersDescriptor	= &DtshdAudioStreamParametersBuffer;

    Configuration.FrameParametersCount		= 32;
    Configuration.FrameParametersDescriptor	= &DtshdAudioFrameParametersBuffer;

    //

    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
///     Destructor
///
FrameParser_AudioDtshd_c::~FrameParser_AudioDtshd_c( void )
{
    Halt();
    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
///     The Reset function release any resources, and reset all variable
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::Reset(  void )
{
    memset( &CurrentStreamParameters, 0, sizeof(CurrentStreamParameters) );
    FirstTime = true;
    DecodeLowestExtensionId = true;
    DecodeExtensionId = 0; // is zero by default, but should be a user configuration
    SelectedAudioPresentation = 0; // is zero by default, but should be a user configuration

    return FrameParser_Audio_c::Reset();
}


////////////////////////////////////////////////////////////////////////////
///
///     The register output ring function
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::RegisterOutputBufferRing(       Ring_t          Ring )
{
    //
    // Clear our parameter pointers
    //

    StreamParameters                    = NULL;
    FrameParameters                     = NULL;

    //
    // Pass the call down the line
    //

    return FrameParser_Audio_c::RegisterOutputBufferRing( Ring );
}


////////////////////////////////////////////////////////////////////////////
///
/// Parse the frame header and store the results for when we emit the frame.
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::ReadHeaders( void )
{
	FrameParserStatus_t Status;
	
    //
    // Perform the common portion of the read headers function
    //

    FrameParser_Audio_c::ReadHeaders();

    //
	
    Status = ParseFrameHeader( BufferData, &ParsedFrameHeader, BufferLength );
    if( Status != FrameParserNoError )
    {
    	FRAME_ERROR("Failed to parse frame header, bad collator selected?\n");
    	return Status;
    }
    
    if (ParsedFrameHeader.Length != BufferLength)
    {
    	FRAME_ERROR("Buffer length (%d) is inconsistant with frame header (%d), bad collator selected?\n",
    	            BufferLength, ParsedFrameHeader.Length);
    	return FrameParserError;
    }

    FrameToDecode = true;
 
    Status = GetNewFrameParameters( (void **) &FrameParameters );
    if( Status != FrameParserNoError )
    {
    	FRAME_ERROR( "Cannot get new frame parameters\n" );
    	return Status;
    }

    // Nick inserted some default values here
    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = true;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;
    ParsedFrameParameters->KeyFrame                                     = true;
    ParsedFrameParameters->ReferenceFrame                               = false;

    ParsedFrameParameters->NewFrameParameters		 = true;
    ParsedFrameParameters->SizeofFrameParameterStructure = sizeof(DtshdAudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure       = FrameParameters;
    
    {
        unsigned int Offset, CoreSize;
        bool IsSubStreamCore;
        // give info to the codec about he backward compatible core in the stream
        if (ParsedFrameHeader.NumberOfCoreSubStreams == 1)
        {
            Offset = 0;
            CoreSize = ParsedFrameHeader.CoreSize;
            IsSubStreamCore = false;
        }
        else
        {
            Offset = ParsedFrameHeader.SubStreamCoreOffset;
            CoreSize = ParsedFrameHeader.SubStreamCoreSize;
            IsSubStreamCore = true;
        }
        FrameParameters->IsSubStreamCore = IsSubStreamCore;
        FrameParameters->CoreSize = CoreSize;
        FrameParameters->BcCoreOffset = Offset;
    }
    
    ParsedAudioParameters->Source.BitsPerSample = 0; // filled in by codec
    ParsedAudioParameters->Source.ChannelCount = 0;  // filled in by codec

    ParsedAudioParameters->Source.SampleRateHz = ParsedFrameHeader.SamplingFrequency;
    ParsedAudioParameters->SampleCount = ParsedFrameHeader.NumberOfSamples;

    ParsedAudioParameters->BackwardCompatibleProperties.SampleRateHz = ParsedFrameHeader.CoreSamplingFrequency;
    ParsedAudioParameters->BackwardCompatibleProperties.SampleCount = ParsedFrameHeader.CoreNumberOfSamples;
    
    if (ParsedFrameHeader.NumberOfExtSubStreams > 0)
    {
        bool IsMasterAudio = false;
        bool IsLbr = false;
        
        for (int AstIdx = 0; AstIdx < ParsedFrameHeader.NumberOfAssets; AstIdx++)
        {
            if (ParsedFrameHeader.CodingComponent[AstIdx] & DTS_EXSUBSTREAM_XLL)
            {
                IsMasterAudio = true;
            }
            if (ParsedFrameHeader.CodingComponent[AstIdx] & DTS_EXSUBSTREAM_LBR)
            {
                IsLbr = true;
            }
        }

        if (IsLbr)
        {
            ParsedAudioParameters->OriginalEncoding = AudioOriginalEncodingDtshdLBR;
        }
        else if (IsMasterAudio)
        {
            ParsedAudioParameters->OriginalEncoding = AudioOriginalEncodingDtshdMA;
        }
        else
        {
            ParsedAudioParameters->OriginalEncoding = AudioOriginalEncodingDtshd;
        }
    }
    else
    {
        ParsedAudioParameters->OriginalEncoding = AudioOriginalEncodingDts;
    }

    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     The reset reference frame list function
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::ResetReferenceFrameList( void )
{
    FRAME_DEBUG(">><<");
    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL );

    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DTSHD audio.
///
/// \copydoc FrameParser_Audio_c::PurgeQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::PurgeQueuedPostDecodeParameterSettings( void )
{
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DTSHD audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::ProcessQueuedPostDecodeParameterSettings( void )
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
/// For DTSHD audio these can be determined immediately (although it the first
/// frame for decode does not contain a PTS we must synthesize one).
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::GeneratePostDecodeParameterSettings( void )
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

    if( CodedFrameParameters->PlaybackTimeValid )
    {
        ParsedFrameParameters->NativePlaybackTime       = CodedFrameParameters->PlaybackTime;
        TranslatePlaybackTimeNativeToNormalized( CodedFrameParameters->PlaybackTime, &ParsedFrameParameters->NormalizedPlaybackTime );
    }

    if( CodedFrameParameters->DecodeTimeValid )
    {
        ParsedFrameParameters->NativeDecodeTime         = CodedFrameParameters->DecodeTime;
        TranslatePlaybackTimeNativeToNormalized( CodedFrameParameters->DecodeTime, &ParsedFrameParameters->NormalizedDecodeTime );
    }

    //
    // Sythesize the presentation time if required
    //
    
    Status = HandleCurrentFrameNormalizedPlaybackTime();
    if( Status != FrameParserNoError )
    {
    	return Status;
    }
    
    //
    // We can't fail after this point so this is a good time to provide a display frame index
    //
    
    ParsedFrameParameters->DisplayFrameIndex		 = NextDisplayFrameIndex++;

    //
    // Use the super-class utilities to complete our housekeeping chores
    //
    
    // no call to HandleUpdateStreamParameters() because UpdateStreamParameters is always false
    FRAME_ASSERT( false == UpdateStreamParameters && NULL == StreamParametersBuffer );

    GenerateNextFrameNormalizedPlaybackTime(ParsedFrameHeader.NumberOfSamples,
                                            ParsedFrameHeader.SamplingFrequency);

    //
    //DumpParsedFrameParameters( ParsedFrameParameters, __PRETTY_FUNCTION__ );
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DTSHD audio.
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::PrepareReferenceFrameList( void )
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DTSHD audio.
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::UpdateReferenceFrameList( void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DTSHD audio.
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::ProcessReverseDecodeUnsatisfiedReferenceStack( void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DTSHD audio.
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::ProcessReverseDecodeStack(			void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DTSHD audio.
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::PurgeReverseDecodeUnsatisfiedReferenceStack(	void )
{
	
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DTSHD audio.
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::PurgeReverseDecodeStack(			void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for DTSHD audio.
///
FrameParserStatus_t   FrameParser_AudioDtshd_c::TestForTrickModeFrameDrop(			void )
{
    return FrameParserNoError;
}


