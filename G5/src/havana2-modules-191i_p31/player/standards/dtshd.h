/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

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

Source file name : dtshd.h
Author :           Sylvain

Definition of the types and constants that are used by several components
dealing with dtshd audio decode/display for havana.


Date        Modification                                    Name
----        ------------                                    --------
24-Apr-06   Created                                         Mark
06-June-07  Ported to player2                               Sylvain

************************************************************************/

#ifndef H_DTSHD_
#define H_DTSHD_

#include "pes.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//
#define  IEC61937_DTS_TYPE_1     11
#define  IEC61937_DTS_TYPE_2     12
#define  IEC61937_DTS_TYPE_3     13

#define DTSHD_HEAD_BLOCK_SIZE                   32 //dts has 32 audio samples per block

#define DTSHD_START_CODE_CORE                   0x7ffe8001
#define DTSHD_START_CODE_CORE_14_1              0x1fffe800
#define DTSHD_START_CODE_CORE_14_2              0x1
#define DTSHD_START_CODE_CORE_14_2_EXT          0x07f
#define DTSHD_START_CODE_MASK                   0xfffffffful
#define DTSHD_START_CODE_CORE_EXT               0x3f
#define DTSHD_START_CODE_CORE_EXT_MASK          0x3f
#define DTSHD_START_CODE_CORE_14_EXT_MASK       0xfff
#define DTSHD_START_CODE_XCH                    0x5a5a5a5a
#define DTSHD_START_CODE_XXCH                   0x47004a03
#define DTSHD_START_CODE_X96K                   0x1d95f262
#define DTSHD_START_CODE_XBR                    0x655e315e
#define DTSHD_START_CODE_LBR                    0x0a801921
#define DTSHD_START_CODE_XLL                    0x41a29547
#define DTSHD_START_CODE_SUBSTREAM              0x64582025
#define DTSHD_START_CODE_SUBSTREAM_CORE         0x02b09261

#define DTSHD_SYNCHRO_BYTES_NEEDED              6 // from 4 to 6 bytes: dts extention substream is 4 bytes while 14 bit core synchro takes 6 bytes...
#define DTSHD_RAW_SYNCHRO_BYTES_NEEDED          5


#define DTSHD_EXTRA_INFO_SIZE                   4
#define DTSHD_FRAME_HEADER_SIZE                 (DTSHD_SYNCHRO_BYTES_NEEDED + DTSHD_EXTRA_INFO_SIZE)

#define DTSHD_MAX_SUBSTREAM_COUNT               4
#define DTSHD_MAX_ASSET_PER_SUBSTREAM_COUNT     8

#define MIXER_DTSHD_PC_REP_PERIOD_2048       (2 << 8)
#define MIXER_DTSHD_PC_REP_PERIOD_4096       (3 << 8)
#define MIXER_DTSHD_PC_REP_PERIOD_8192       (4 << 8)

// ////////////////////////////////////////////////////////////////////////////
//
//  Definitions of types matching AC3 audio headers
//

////////////////////////////////////////////////////////////////
///
/// DTSHD audio stream type
///
///
typedef enum
{
  TypeDtshdCore,       ///< frame is a dtshd core substream
  TypeDtshdExt         ///< frame is a dtshd extension substream
} DtshdStreamType_t;

typedef enum
{
  ParseForSynchro,     /// < Parse frame to get frame extension length and type, and perform crc
  ParseExtended,       /// < Parse frame to get frame extension length and type, but don't compute crc, and get asset info
  ParseSmart,          /// < The parsing type is to be decided smartly
} DtshdParseType_t;


typedef struct DtshdParseModel_s
{
    unsigned int ParseType:2; // of type DtshdParseType_t
    unsigned int ParseExtendedIndex:2; // index fo the extension to be parsed
    unsigned int Unused:4;
} DtshdParseModel_t;

typedef enum
{
  CodeDtshdCore      = 0x001,      ///< coding component is core within the core substream
  CodeDtshdXch       = 0x008 ,     ///< coding component is Xch within the core substream
  CodeDtshdXXch      = 0x002 ,     ///< coding component is XXch within the core substream
  CodeDtshdX96K      = 0x004 ,     ///< coding component is X96K within the core substream
  CodeDtshdExSSCore  = 0x010,      ///< coding component is Xch within the extension substream
  CodeDtshdExtSSXXch = 0x040,      ///< coding component is XXch within the extension substream
  CodeDtshdExtSSX96K = 0x080,      ///< coding component is X96K within the extension substream
  CodeDtshdExtSSXBR  = 0x020,      ///< coding component is XBR within the extension substream
  CodeDtshdExtSSLBR  = 0x100,      ///< coding component is LBR within the extension substream
  CodeDtshdExtSSXLL  = 0x200       ///< coding component is XLL within the extension substream
} DtshdCodingComponentType_t;

typedef enum
{
    DTS_CORE_EXTENSION_XCH  = 0x0,
    DTS_CORE_EXTENSION_X96  = 0x2,
    DTS_CORE_EXTENSION_XXCH = 0x6
} DtshdCoreExtensionType_t;

#define DTS_EXSUBSTREAM_CORE CodeDtshdExSSCore
#define DTS_EXSUBSTREAM_XBR  CodeDtshdExtSSXBR
#define DTS_EXSUBSTREAM_XXCH CodeDtshdExtSSXXch
#define DTS_EXSUBSTREAM_X96  CodeDtshdExtSSX96K
#define DTS_EXSUBSTREAM_LBR  CodeDtshdExtSSLBR
#define DTS_EXSUBSTREAM_XLL  CodeDtshdExtSSXLL
#define RESERVED_1           0x400
#define RESERVED_2           0x800

////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the DTSHD audio frame header.
///
typedef struct DtshdAudioParsedFrameHeader_s
{
    // Directly interpretted values
    DtshdStreamType_t   Type;
    unsigned char       SubStreamId;
    unsigned int        SamplingFrequency; ///< Sampling frequency in Hz.
    
    // Derived values
    unsigned int        NumberOfSamples;                   ///< Number of samples per channel within the frame.
    unsigned int        CoreNumberOfSamples;               ///< Number of samples per channel within the core substream
    unsigned int        Length;                            ///< Length of frame in bytes (including header).
    unsigned int        CoreSize;                          ///< Length of the core substream in bytes
    unsigned int        CoreSamplingFrequency;             ///< Sampling frequency of the core substream
    unsigned char       NumberOfCoreSubStreams;            ///< number of core substreams (1 max...)
    bool                HasCoreExtensions;                 ///< true if core substream contains extensions (dts96, ...)
    DtshdCoreExtensionType_t CoreExtensionType;            ///< Type of core extension (if present)
    unsigned char       NumberOfExtSubStreams;             ///< number of extension substreams (4 max)
    unsigned char       NumberOfAssets;                    ///< number of asset in the extension substream
    unsigned int        ExtensionHeaderSize;               ///< Size of the whole extension header to access directly the asset data
    unsigned int        ExtensionSamplingFrequency;        ///< Sampling Frequency of this extension substream
    unsigned int        SubStreamCoreOffset;                        ///< Offset from the start of the DTS-HD substream to the backward compatible core substream
    unsigned int        SubStreamCoreSize;                 ///< Size of the backward compatible core substream
    unsigned int        AssetSize[DTSHD_MAX_ASSET_PER_SUBSTREAM_COUNT];      ///< Size of each asset in the extension
    unsigned int        ExtSubCoreCodingComponentSize[DTSHD_MAX_ASSET_PER_SUBSTREAM_COUNT];      ///< size of extension substeam coding component in each asset
    DtshdCodingComponentType_t CodingComponent[DTSHD_MAX_ASSET_PER_SUBSTREAM_COUNT];      ///< type of coding component
} DtshdAudioParsedFrameHeader_t;


typedef struct DtshdAudioSyncFrameHeader_s
{
    DtshdStreamType_t   Type;
    unsigned char       SubStreamId;
} DtshdAudioSyncFrameHeader_t;

////////////////////////////////////////////////////////////////

typedef struct DtshdAudioStreamParameters_s
{
    unsigned int Unused;
} DtshdAudioStreamParameters_t;

#define BUFFER_DTSHD_AUDIO_STREAM_PARAMETERS        "DtshdAudioStreamParameters"
#define BUFFER_DTSHD_AUDIO_STREAM_PARAMETERS_TYPE   {BUFFER_DTSHD_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(DtshdAudioStreamParameters_t)}

//

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to DTS-HD audio.
///
///
typedef struct DtshdAudioFrameParameters_s
{
    /// Stream type   
    bool                IsSubStreamCore;
    /// Size of the core substream (in bytes)
    unsigned int        CoreSize;
    /// Offset tot the backward compatible core substream location
    unsigned int        BcCoreOffset;
    
} DtshdAudioFrameParameters_t;

#define BUFFER_DTSHD_AUDIO_FRAME_PARAMETERS        "DtshdAudioFrameParameters"
#define BUFFER_DTSHD_AUDIO_FRAME_PARAMETERS_TYPE   {BUFFER_DTSHD_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(DtshdAudioFrameParameters_t)}

#endif /* H_DTSHD_ */
