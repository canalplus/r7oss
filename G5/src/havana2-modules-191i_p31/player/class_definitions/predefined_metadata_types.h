/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : predefined_metadata_types.h
Author :           Nick

Definition of the pre-defined meta data types for player 2.



Date        Modification                                    Name
----        ------------                                    --------
01-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_PREDEFINED_METADATA_TYPES
#define H_PREDEFINED_METADATA_TYPES

// //////////////////////////////////////////////////////////////////////
//
//      Input type for the player input
//

typedef enum 
{
    MuxTypeUnMuxed           = 0,
    MuxTypeTransportStream
} PlayerInputMuxType_t;


typedef struct PlayerInputDescriptor_s
{
    PlayerInputMuxType_t    MuxType;
    DemultiplexorContext_t  DemultiplexorContext;       // To be set for multiplexed streams
    PlayerStream_t          UnMuxedStream;              // To be set for data destined for a specific stream.

    bool                    PlaybackTimeValid;
    bool                    DecodeTimeValid;
    unsigned long long      PlaybackTime;
    unsigned long long      DecodeTime;

    unsigned int            DataSpecificFlags;
} PlayerInputDescriptor_t;

//

#define METADATA_PLAYER_INPUT_DESCRIPTOR        "PlayerInputDescriptor"
#define METADATA_PLAYER_INPUT_DESCRIPTOR_TYPE   {METADATA_PLAYER_INPUT_DESCRIPTOR, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(PlayerInputDescriptor_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Coded frame parameters - output from a collator
//

typedef struct CodedFrameParameters_s
{
    bool                    StreamDiscontinuity;
    bool                    FlushBeforeDiscontinuity;
    bool                    ContinuousReverseJump;
    bool                    FollowedByStreamTerminate;

    bool                    PlaybackTimeValid;
    bool                    DecodeTimeValid;
    unsigned long long      PlaybackTime;
    unsigned long long      DecodeTime;

    unsigned int            DataSpecificFlags;
} CodedFrameParameters_t;

//

#define METADATA_CODED_FRAME_PARAMETERS         "CodedFrameParameters"
#define METADATA_CODED_FRAME_PARAMETERS_TYPE    {METADATA_CODED_FRAME_PARAMETERS, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(CodedFrameParameters_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Start code list - optional output from a collator - not fixed length
//

typedef unsigned long long PackedStartCode_t;
#define PackStartCode(Offset,Code)                      ((PackedStartCode_t)((((PackedStartCode_t)Offset) << 8) | (PackedStartCode_t)(Code)))

#define ModifyStartCodeOffset(Value,Modification)       ((Value) + ((PackedStartCode_t)Modification) << 8))
#define ExtractStartCodeOffset(Value)                   ((unsigned int)((Value) >> 8))
#define ExtractStartCodeCode(Value)                     ((unsigned int)((Value) & 0xff))

typedef struct StartCodeList_s
{
    unsigned int         NumberOfStartCodes;
    PackedStartCode_t    StartCodes[1];
} StartCodeList_t;

//

#define SizeofStartCodeList(Entries)            (sizeof(StartCodeList_t) + ((Entries-1) * sizeof(PackedStartCode_t)))
#define METADATA_START_CODE_LIST                "StartCodeList"
#define METADATA_START_CODE_LIST_TYPE           {METADATA_START_CODE_LIST, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, false, false, 0}


// //////////////////////////////////////////////////////////////////////
//
//      Parsed frame parameters - output from a frame parser for all streams
//

#define MAX_REFERENCE_FRAME_LISTS               3               // H264 P B0 B1
#define MAX_ENTRIES_IN_REFERENCE_FRAME_LIST     32              // H264 16 frames - 32 fields

#include "h264.h"

typedef struct ReferenceFrameList_s
{
    unsigned int                EntryCount;
    unsigned int                EntryIndicies[MAX_ENTRIES_IN_REFERENCE_FRAME_LIST];
    H264ReferenceDetails_t      H264ReferenceDetails[MAX_ENTRIES_IN_REFERENCE_FRAME_LIST];      // Added to support H264
} ReferenceFrameList_t;

//

typedef struct ParsedFrameParameters_s
{
    unsigned int            DecodeFrameIndex;
    unsigned int            DisplayFrameIndex;
    unsigned long long      NativePlaybackTime;
    unsigned long long      NormalizedPlaybackTime;
    unsigned long long      NativeDecodeTime;
    unsigned long long      NormalizedDecodeTime;

    unsigned int            DataOffset;
    bool                    CollapseHolesInDisplayIndices;
    bool                    FirstParsedParametersForOutputFrame;
    bool                    FirstParsedParametersAfterInputJump;
    bool                    SurplusDataInjected;
    bool                    ContinuousReverseJump;
    bool                    KeyFrame;
    bool                    IndependentFrame;
    bool                    ReferenceFrame;
    unsigned int            NumberOfReferenceFrameLists;
    ReferenceFrameList_t    ReferenceFrameList[MAX_REFERENCE_FRAME_LISTS];

    bool                    NewStreamParameters;
    unsigned int            SizeofStreamParameterStructure;
    void                   *StreamParameterStructure;

    bool                    NewFrameParameters;
    bool                    ApplySubstandardDecode;
    unsigned int            SizeofFrameParameterStructure;
    void                   *FrameParameterStructure;
} ParsedFrameParameters_t;

static inline void DumpParsedFrameParameters( ParsedFrameParameters_t *ParsedFrameParameters, const char *tag )
{
    report( severity_note, "%s: ParsedFrameParameters @ %p\n", tag, ParsedFrameParameters );
    report( severity_note, "\tDisplayFrameIndex                      %d\n", (int) ParsedFrameParameters->DisplayFrameIndex );
    report( severity_note, "\tNewStreamParameters                    %s\n", ParsedFrameParameters->NewStreamParameters ? "true" : "false" );
    report( severity_note, "\tNewFrameParameters                     %s\n", ParsedFrameParameters->NewFrameParameters ? "true" : "false" );
}

//

#define METADATA_PARSED_FRAME_PARAMETERS                "ParsedFrameParameters"
#define METADATA_PARSED_FRAME_PARAMETERS_TYPE           {METADATA_PARSED_FRAME_PARAMETERS, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(ParsedFrameParameters_t)}

#define METADATA_PARSED_FRAME_PARAMETERS_REFERENCE      "ParsedFrameParametersReference"
#define METADATA_PARSED_FRAME_PARAMETERS_REFERENCE_TYPE {METADATA_PARSED_FRAME_PARAMETERS_REFERENCE, MetaDataTypeBase, NoAllocation, 0, 0, true, false, sizeof(ParsedFrameParameters_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Additional parsed frame parameters for a video stream
//

#define MAX_PAN_SCAN_VALUES    3             // H264 has max seen value

//

#define SliceTypeI              0
#define SliceTypeP              1
#define SliceTypeB              2

typedef unsigned int SliceType_t;

//

#define StructureEmpty          0
#define StructureTopField       1
#define StructureBottomField    2
#define StructureFrame          3

typedef unsigned int PictureStructure_t;

//

#define MatrixCoefficients_Undefined            0
#define MatrixCoefficients_ITU_R_BT601          1
#define MatrixCoefficients_ITU_R_BT709          2
#define MatrixCoefficients_ITU_R_BT470_2_M      3
#define MatrixCoefficients_ITU_R_BT470_2_BG     4
#define MatrixCoefficients_SMPTE_170M           5
#define MatrixCoefficients_SMPTE_240M           6
#define MatrixCoefficients_FCC                  7

typedef unsigned int MatrixCoefficientsType_t;

//

typedef struct VideoDisplayParameters_s
{
    unsigned int                Width;
    unsigned int                Height;
    unsigned int                DisplayWidth;
    unsigned int                DisplayHeight;
    bool                        Progressive;
    bool                        OverscanAppropriate;
    bool                        VideoFullRange;                         // Indicate black level and range of colour values.
    MatrixCoefficientsType_t    ColourMatrixCoefficients;               // Define the standard used in RGB <=> YCbCr inter-convertion
    Rational_t                  PixelAspectRatio;
    Rational_t                  FrameRate;
} VideoDisplayParameters_t;

//

typedef struct PanScan_s
{
    unsigned int                Count;
    unsigned int                DisplayCount[MAX_PAN_SCAN_VALUES];      // In fields or frames depending on the progressive nature of the sequence
    int                         HorizontalOffset[MAX_PAN_SCAN_VALUES];
    int                         VerticalOffset[MAX_PAN_SCAN_VALUES];
}PanScan_t;

//

typedef struct ParsedVideoParameters_s
{
    VideoDisplayParameters_t    Content;
    SliceType_t                 SliceType;
    PictureStructure_t          PictureStructure;
    bool                        FirstSlice;             // Always true for a non-slice based decode
    bool                        InterlacedFrame;        // Note can have non-interlaced frame from a non-progressive source
    bool                        TopFieldFirst;          // Interlaced frames only 
    unsigned int                DisplayCount[2];        // First/second field count or frame count for progressive content
    PanScan_t                   PanScan;
} ParsedVideoParameters_t;

//

#define METADATA_PARSED_VIDEO_PARAMETERS        "ParsedVideoParameters"
#define METADATA_PARSED_VIDEO_PARAMETERS_TYPE   {METADATA_PARSED_VIDEO_PARAMETERS, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(ParsedVideoParameters_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Additional parsed frame parameters for an audio stream
//


/// A pretty meaningless structure. I'm still trying to figure out if its useful.
typedef struct AudioDisplayParameters_s
{
    unsigned int Unused;
} AudioDisplayParameters_t;

typedef struct AudioSurfaceParameters_s
{
    unsigned int                BitsPerSample;
    unsigned int                ChannelCount; ///< Number of interleaved channels in the raw buffer

    /// Sampling frequency (in Hz).
    ///
    /// \todo The sample rate is not a intrinsic of the display surface, rather it is a transient stream
    ///       parameter.
    unsigned int                SampleRateHz;
} AudioSurfaceParameters_t;

typedef struct StreamMetadata_s
{
    unsigned int                FrontMatrixEncoded;
    unsigned int                RearMatrixEncoded;
    unsigned int                MixLevel;
    unsigned int                DialogNorm;
    unsigned int                LfeGain;
} StreamMetadata_t;

#define MAX_MIXING_OUTPUT_CONFIGURATION 3
#define MAX_NB_CHANNEL_COEFF 8

// The following structures shall be used if the decoded audio stream contains some mixing metadata (Blu Ray case)
typedef struct MixingOutputConfiguration_s
{
	int                 AudioMode;                                    //< Primary Channel Audio Mode
	unsigned short      PrimaryAudioGain[MAX_NB_CHANNEL_COEFF];        //< unsigned Q3.13 gain to be applied to each channel of primary stream
	unsigned short      SecondaryAudioPanCoeff[MAX_NB_CHANNEL_COEFF]; //< unsigned Q3.13 panning coefficients to be applied to secondary mono stream
} MixingOutputConfiguration_t;

typedef struct MixingMetadata_s
{
    bool                               IsMixingMetadataPresent;                       //< does the stream embedd some mixing metadata?
	unsigned short                     PostMixGain;                                   //< unsigned Q3.13 gain to be applied to output of mixed primary and secondary
	unsigned short                     NbOutMixConfig;                                //< Number of mix output configurations  
	MixingOutputConfiguration_t        MixOutConfig[MAX_MIXING_OUTPUT_CONFIGURATION];//< This array is extensible according to NbOutMixConfig
} MixingMetadata_t;

typedef enum 
{
    AudioOriginalEncodingUnknown = 0,
    AudioOriginalEncodingAc3,     ///< Indicates AC3 is present in CodedDataBuffer
    AudioOriginalEncodingDdplus,  ///< Indicates DD+ is present in CodedDataBuffer, and AC3 in TranscodedDatabuffer
    AudioOriginalEncodingDts,     ///< Indicates DTS is present in CodedDataBuffer
    AudioOriginalEncodingDtshd,   ///< Indicates DTSHD is present in CodedDataBuffer, and DTS in TranscodedDatabuffer
    AudioOriginalEncodingDtshdMA, ///< Indicates DTSHD Master Audio is present in CodedDataBuffer, and DTS in TranscodedDatabuffer
    AudioOriginalEncodingDtshdLBR,///< Indicates DTSHD LBR is present in CodedDataBuffer, and TranscodedDatabuffer is void
    AudioOriginalEncodingTrueHD   ///< Indicates Dolby TrueHD is present in CodedDataBuffer, and TranscodedDatabuffer is void
} AudioOriginalEncoding_t;

// This structure keeps track of a backward compatible part of a
// stream (e.g DTS core part of DTS-HD stream)
typedef struct BackwardCompatibleProperties_s
{
    unsigned int                SampleRateHz;
    unsigned int                SampleCount;
} BackwardCompatibleProperties_t;


typedef struct ParsedAudioParameters_s
{
    AudioSurfaceParameters_t       Source;
    unsigned int                   SampleCount;
    bool                           decErrorStatus;
    int                            Organisation;     ///< Indicates the organisation of the channels (e.g. 5.1, 2.0)
    AudioOriginalEncoding_t        OriginalEncoding;
    StreamMetadata_t               StreamMetadata;   ///< Stream metadata mainly used by fatpipe 
    MixingMetadata_t               MixingMetadata;   ///< Mixing metadata used by BD Mixer (present in secondary streams)
    BackwardCompatibleProperties_t BackwardCompatibleProperties; // Properties of the backaward compatible part of a stream
} ParsedAudioParameters_t;

//

#define METADATA_PARSED_AUDIO_PARAMETERS        "ParsedAudioParameters"
#define METADATA_PARSED_AUDIO_PARAMETERS_TYPE   {METADATA_PARSED_AUDIO_PARAMETERS, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(ParsedAudioParameters_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Output timing information specific to video - 
//      generated by timing module, and fed back from manifestor.
//

#define OUTPUT_TIMING_COMMON                                    \
	unsigned long long      SystemPlaybackTime;             \
	unsigned long long      ExpectedDurationTime;           \
	unsigned long long      ActualSystemPlaybackTime;       \
								\
	Rational_t              OutputRateAdjustment;           \
	Rational_t              SystemClockAdjustment;          \
								\
	bool                    TimingGenerated;



typedef struct VideoOutputTiming_s
{
    OUTPUT_TIMING_COMMON

    bool                        Interlaced;
    bool                        TopFieldFirst;     // Interlaced frames only 
    unsigned int                DisplayCount[2];     // Non-interlaced use [0] only
    PanScan_t                   PanScan;
} VideoOutputTiming_t;

//

#define METADATA_VIDEO_OUTPUT_TIMING            "VideoOutputTiming"
#define METADATA_VIDEO_OUTPUT_TIMING_TYPE       {METADATA_VIDEO_OUTPUT_TIMING, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(VideoOutputTiming_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Output timing information specific to Audio - 
//      generated by timing module, and fed back from manifestor.
//

typedef struct AudioOutputTiming_s
{
    OUTPUT_TIMING_COMMON

    unsigned int                DisplayCount;
} AudioOutputTiming_t;

//

#define METADATA_AUDIO_OUTPUT_TIMING            "AudioOutputTiming"
#define METADATA_AUDIO_OUTPUT_TIMING_TYPE       {METADATA_AUDIO_OUTPUT_TIMING, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(AudioOutputTiming_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Video output descriptor, attached by manifestor as an
//      information feedback mechanism.
//

typedef struct VideoOutputSurfaceDescriptor_s
{
    unsigned int                DisplayWidth;
    unsigned int                DisplayHeight;
    bool                        Progressive;
    Rational_t                  FrameRate;
} VideoOutputSurfaceDescriptor_t;

//

#define METADATA_VIDEO_OUTPUT_SURFACE_DESCRIPTOR       "VideoOutputSurfaceDescriptor"
#define METADATA_VIDEO_OUTPUT_SURFACE_DESCRIPTOR_TYPE   {METADATA_VIDEO_OUTPUT_SURFACE_DESCRIPTOR, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(VideoOutputSurfaceDescriptor_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Audio output surface descriptor, attached by manifestor as an
//      information feedback mechanism.
//

typedef struct AudioOutputSurfaceDescriptor_s
{
    unsigned int                BitsPerSample;
    unsigned int                ChannelCount;
    unsigned int                SampleRateHz;
} AudioOutputSurfaceDescriptor_t;

//

#define METADATA_AUDIO_OUTPUT_SURFACE_DESCRIPTOR        "AudioOutputSurfaceDescriptor"
#define METADATA_AUDIO_OUTPUT_SURFACE_DESCRIPTOR_TYPE   {METADATA_AUDIO_OUTPUT_SURFACE_DESCRIPTOR, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(AudioOutputSurfaceDescriptor_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Buffer structure record
//

#define MAX_BUFFER_DIMENSIONS   5
#define MAX_BUFFER_COMPONENTS   5

typedef enum
{
    FormatUnknown                       = 0,
    FormatMarkerFrame,
    FormatAudio,
    FormatVideo420_MacroBlock,
    FormatVideo420_PairedMacroBlock,
//    FormatVideo420_Raster_Y_CbCr,
//    FormatVideo420_Raster_Y_Cb_Cr,
    FormatVideo422_Raster,
    FormatVideo420_Planar,
    FormatVideo420_PlanarAligned,
    FormatVideo422_Planar,
    FormatVideo8888_ARGB,
    FormatVideo888_RGB,
    FormatVideo565_RGB,
    FormatVideo422_YUYV
} BufferFormat_t;

//

typedef struct BufferStructure_s
{
    BufferFormat_t      Format;
    unsigned int        DimensionCount;
    unsigned int        Dimension[MAX_BUFFER_DIMENSIONS];
    unsigned int        ComponentCount;
    unsigned int        ComponentOffset[MAX_BUFFER_COMPONENTS];
    unsigned int        ComponentBorder[MAX_BUFFER_DIMENSIONS];
    unsigned int        Strides[MAX_BUFFER_DIMENSIONS - 1][MAX_BUFFER_COMPONENTS];
    unsigned int        Size;
    unsigned int        DecimatedSize;
    bool                DecimationRequired;
} BufferStructure_t;

//

#define METADATA_BUFFER_STRUCTURE       "BufferStructure"
#define METADATA_BUFFER_STRUCTURE_TYPE  {METADATA_BUFFER_STRUCTURE, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(BufferStructure_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Trick mode configuration parameters from the codec
//      information feedback mechanism.
//

typedef struct CodecTrickModeParameters_s
{
    Rational_t                  EmpiricalMaximumDecodeFrameRateShortIntegration;        // Observed values out of the transformer
    Rational_t                  EmpiricalMaximumDecodeFrameRateLongIntegration;         // Observed values out of the transformer

    bool                        SubstandardDecodeSupported;
    Rational_t                  SubstandardDecodeRateIncrease;

    unsigned int                DefaultGroupSize;
    unsigned int                DefaultGroupReferenceFrameCount;        // Including the key frame 

} CodecTrickModeParameters_t;

//

#define METADATA_CODEC_TRICK_MODE_PARAMETERS            "CodecTrickModeParameters"
#define METADATA_CODEC_TRICK_MODE_PARAMETERS_TYPE       {METADATA_CODEC_TRICK_MODE_PARAMETERS, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(CodecTrickModeParameters_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Buffer type for video post processing controls
//

typedef struct VideoPostProcessingControl_s
{
    bool                        RangeMapLumaPresent;            // Range mapping
    unsigned int                RangeMapLuma;
    bool                        RangeMapChromaPresent;          // Range mapping
    unsigned int                RangeMapChroma;
} VideoPostProcessingControl_t;

//

#define BUFFER_VIDEO_POST_PROCESSING_CONTROL            "VideoPostProcessingControl"
#define BUFFER_VIDEO_POST_PROCESSING_CONTROL_TYPE       {BUFFER_VIDEO_POST_PROCESSING_CONTROL, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(VideoPostProcessingControl_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Buffer type for audio post processing controls
//

typedef struct AudioPostProcessingControl_s
{
    bool                        NoneDefined;
} AudioPostProcessingControl_t;

//

#define BUFFER_AUDIO_POST_PROCESSING_CONTROL            "AudioPostProcessingControl"
#define BUFFER_AUDIO_POST_PROCESSING_CONTROL_TYPE       {BUFFER_AUDIO_POST_PROCESSING_CONTROL, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(AudioPostProcessingControl_t)}

//

#endif


