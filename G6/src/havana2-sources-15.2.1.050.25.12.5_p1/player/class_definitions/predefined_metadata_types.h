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

#ifndef H_PREDEFINED_METADATA_TYPES
#define H_PREDEFINED_METADATA_TYPES

// //////////////////////////////////////////////////////////////////////
//
//      Input type for the player input
//

typedef struct PlayerInputDescriptor_s
{
    PlayerStream_t          UnMuxedStream;              // To be set for data destined for a specific stream.

    bool                    PlaybackTimeValid;
    bool                    DecodeTimeValid;
    unsigned long long      PlaybackTime;
    unsigned long long      DecodeTime;
    PlayerTimeFormat_t      SourceTimeFormat;

    unsigned int            DataSpecificFlags;
} PlayerInputDescriptor_t;

typedef struct ADMetaData_s
{
    unsigned int            ADFadeValue;
    unsigned int            ADPanValue;
    unsigned int            ADGainCenter;
    unsigned int            ADGainFront;
    unsigned int            ADGainSurround;
    bool                    ADValidFlag;
    bool                    ADInfoAvailable;
} ADMetaData_t;
//

#define METADATA_PLAYER_INPUT_DESCRIPTOR        "PlayerInputDescriptor"
#define METADATA_PLAYER_INPUT_DESCRIPTOR_TYPE   {METADATA_PLAYER_INPUT_DESCRIPTOR, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(PlayerInputDescriptor_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Coded frame parameters - output from a collator
//

typedef struct CodedFrameParameters_s
{
    bool            StreamDiscontinuity;
    bool            FlushBeforeDiscontinuity;
    bool            ContinuousReverseJump;
    bool            FollowedByStreamTerminate;

    bool            PlaybackTimeValid;
    bool            DecodeTimeValid;

    unsigned long long  PlaybackTime;
    unsigned long long  DecodeTime;
    PlayerTimeFormat_t  SourceTimeFormat;

    unsigned long long  CollationTime;
    unsigned int        DataSpecificFlags;
    bool                IsMpeg4p2MetaDataPresent;
    //AD related info structure
    ADMetaData_t        ADMetaData;
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
//      User Data - output from a frame parser
//

#define MAXIMUM_USER_DATA_BUFFERS  4
#define MAX_USER_DATA_SIZE   2048

#include "user_data.h"


// User data additional parameters
typedef struct UserDataAdditionalParameters_s
{
    unsigned char                     Length;
    bool                              Available;

    // This structure is formated
    CodecUserDataParameters_t         CodecUserDataParameters;
} UserDataAdditionalParameters_t;


// User data ganeric parameters
// This structure is formated
typedef struct UserDataGenericParameters_s
{
    char                              Reserved[4];  // reserved = 32 'STUD'
    unsigned                          BlockLength: 16; // block_length=16
    unsigned                          HeaderLength: 16; // header_length=16
    unsigned                          Reserved_1: 1; // reserved=1
    unsigned                          PaddingBytes: 5; // padding bytes=5
    unsigned                          StreamAbridgement: 1; // stream_abridgement=1
    unsigned                          Overflow: 1; // buffering_overflow=1
    unsigned                          CodecId: 8; // codec_id=8
    unsigned                          Reserved_2: 6; // reserved=6
    unsigned                          IsThereAPts: 1; // is_there_a_pts=1
    unsigned                          IsPtsInterpolated: 1; // is_pts_interpolated=1
    unsigned                          Reserved_3: 7; // reserved=7
    unsigned                          Pts_Msb: 1; // pts_msb=1
    unsigned                          Pts: 32; // PTS=32
} UserDataGenericParameters_t;

// Global User data structure
typedef struct UserData_s
{
    UserDataGenericParameters_t       UserDataGenericParameters;            // Generic parameters
    UserDataAdditionalParameters_t    UserDataAdditionalParameters;         // Additional parameters specific to codec
    unsigned char                     UserDataPayload[MAX_USER_DATA_SIZE];  // payload
} UserData_t;



#define SizeofUserData(Entries)       (Entries * (sizeof(UserDataGenericParameters_t) + sizeof(UserDataAdditionalParameters_t) + MAX_USER_DATA_SIZE))
#define METADATA_USER_DATA            "UserData"
#define METADATA_USER_DATA_TYPE       {METADATA_USER_DATA, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, false, false, 0}


// //////////////////////////////////////////////////////////////////////
//
//      Parsed frame parameters - output from a frame parser for all streams
//

#define MAX_REFERENCE_FRAME_LISTS               3               // H264 P B0 B1
#define MAX_ENTRIES_IN_REFERENCE_FRAME_LIST     32              // H264 16 frames - 32 fields

#include "refdetails.h"

typedef struct ReferenceFrameList_s
{
    unsigned int                EntryCount;
    unsigned int                EntryIndicies[MAX_ENTRIES_IN_REFERENCE_FRAME_LIST];
    ReferenceDetails_t          ReferenceDetails[MAX_ENTRIES_IN_REFERENCE_FRAME_LIST];      // Added to support H264
} ReferenceFrameList_t;

typedef struct ParsedFrameParameters_s
{
    unsigned int            DecodeFrameIndex;
    unsigned int            DisplayFrameIndex;
    unsigned long long      NativePlaybackTime;
    unsigned long long      NormalizedPlaybackTime;
    unsigned long long      NativeDecodeTime;
    unsigned long long      NormalizedDecodeTime;
    PlayerTimeFormat_t      NativeTimeFormat;
    bool                    SpecifiedPlaybackTime;
    unsigned long long      CollationTime;

    unsigned int            DataOffset;
    bool                    CollapseHolesInDisplayIndices;
    bool                    FirstParsedParametersForOutputFrame;
    bool                    FirstParsedParametersAfterInputJump;
    bool                    SurplusDataInjected;
    bool                    ContinuousReverseJump;
    bool                    KeyFrame;
    bool                    IndependentFrame;
    bool                    ReferenceFrame;
    bool                    StillPicture;
    unsigned int            NumberOfReferenceFrameLists;
    ReferenceFrameList_t    ReferenceFrameList[MAX_REFERENCE_FRAME_LISTS];

    bool                    NewStreamParameters;
    unsigned int            SizeofStreamParameterStructure;
    void                   *StreamParameterStructure;

    bool                    NewFrameParameters;
    bool                    ApplySubstandardDecode;
    unsigned int            SizeofFrameParameterStructure;
    void                   *FrameParameterStructure;
    unsigned char           UserDataNumber;
    ADMetaData_t            ADMetaData;
    bool                    EofMarkerFrame;
    bool                    Discard_picture;
    bool                    PictureHasMissingRef;
} ParsedFrameParameters_t;

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

/* 3D structures */
// This data structure is defining the characteristics of a possible flipping of one of the two stereo views.
typedef enum Stvid3DFlipping_e
{
    Stream3DNoFlipping,
    Stream3DFlippingFrame0,
    Stream3DFlippingFrame1
} Stream3DFlipping_t;

// The data structure defines the packing arrangement scheme of a decoded frame.
typedef enum
{
    Stream3DNone,
    Stream3DFormatFrameSeq,
    Stream3DFormatStackedHalf,
    Stream3DFormatSidebysideHalf,
    // Starting this point, all other format are not supported so far. There are mentioned for information only.
    Stream3DFormatSidebysideFull,
    Stream3dFormatStackedFrame,
    Stream3dFormatField_Alternate,
    Stream3dFormatPictureInterleave,
    Stream3dFormatlD,
    Stream3dFormatlDGGminusd,
    Stream3dFormat2IndepStreams,
    Stream3dFormatFrameSeqllRR
} Stream3DFormat_t;

// This data structure is specifying the 3D frame arrangement a decoded frame.
typedef struct output_3d_videoproperty_s
{
    Stream3DFormat_t           Stream3DFormat;           /* All possible frame packing arrangement schemes */
    bool                       IsFrame0;                 /* TRUE if current picture is frame 0 */
    bool                       Frame0IsLeft;             /* The frame 0 is left view (value TRUE) or a right one (value FALSE). */
    output_3d_videoproperty_s()
        : Stream3DFormat(Stream3DNone)
        , IsFrame0(false)
        , Frame0IsLeft(false)
    {}
} Output3DVideoProperty_t;

//

typedef struct VideoDisplayParameters_s
{
    unsigned int                Width;                      // Width/Height represent the actual frame size after decode (without the storage pitch).
    unsigned int                Height;
    unsigned int                DecodeWidth;                // DecodeWidth/DecodeHeight are specific values used by the decoder (default to Width/Height).
    unsigned int                DecodeHeight;               // Can be specified by a frame parser to adjust the decode window (e.g. decode 1088 lines but render 1080).
    unsigned int                DisplayWidth;               // DisplayWidth/DisplayHeight represent the size of a "window of interest" display rectangle (default to Width/Height).
    unsigned int                DisplayHeight;              // e.g. in MPEG-2: it contains the active display size info, extracted from the sequence_display_extension.
    unsigned int                DisplayX;                   // DisplayX/DisplayY represent the offset of a "window of interest" display rectangle (default to 0/0).
    unsigned int                DisplayY;
    bool                        Progressive;
    bool                        OverscanAppropriate;
    bool                        VideoFullRange;             // Indicate black level and range of colour values.
    MatrixCoefficientsType_t    ColourMatrixCoefficients;   // Define the standard to be used for RGB <=> YCbCr conversions
    Rational_t                  PixelAspectRatio;
    Rational_t                  FrameRate;
    Output3DVideoProperty_t     Output3DVideoProperty;

    VideoDisplayParameters_s(void)
        : Width(0)
        , Height(0)
        , DecodeWidth(0)
        , DecodeHeight(0)
        , DisplayWidth(0)
        , DisplayHeight(0)
        , DisplayX(0)
        , DisplayY(0)
        , Progressive(false)
        , OverscanAppropriate(false)
        , VideoFullRange(false)
        , ColourMatrixCoefficients(0)
        , PixelAspectRatio()
        , FrameRate()
        , Output3DVideoProperty()
    {}
} VideoDisplayParameters_t;

typedef struct PanScan_s
{
    unsigned int                DisplayCount;      // In fields or frames depending on the progressive nature of the sequence
    unsigned int                Width;             // Width of the display rectangle expressed in pixels (may be different from DisplayWidth)
    unsigned int                Height;            // Heigth of the display rectangle expressed in lines (may be different from DisplayHeight)
    int                         HorizontalOffset;  // These offsets give the position of the top left corner of the display rectangle within the
    int                         VerticalOffset;    // full decoded frame rectangle. They are expressed in 1/16th of pixels/lines unit,
    // as defined by most of the video codec standards.
} PanScan_t;

typedef struct ParsedVideoParameters_s
{
    VideoDisplayParameters_t    Content;
    SliceType_t                 SliceType;
    PictureStructure_t          PictureStructure;
    bool                        FirstSlice;             // Always true for a non-slice based decode
    bool                        InterlacedFrame;        // Note can have non-interlaced frame from a non-progressive source
    bool                        TopFieldFirst;          // Interlaced frames only
    unsigned int                DisplayCount[2];        // First/second field count or frame count for progressive content
    unsigned int                PanScanCount;                   // Specifies the number of available pan-scan data, so the useful size of the following table
    PanScan_t                   PanScan[MAX_PAN_SCAN_VALUES];   // Table storing the parsed pan-scan data
} ParsedVideoParameters_t;

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
    MixingOutputConfiguration_t        MixOutConfig[MAX_MIXING_OUTPUT_CONFIGURATION]; //< This array is extensible according to NbOutMixConfig
    ADMetaData_t                       ADPESMetaData;                                 //Audio Description related parameter
} MixingMetadata_t;

typedef struct DownMixTable_s
{
    bool             IsTablePresent ; // Is true when DownMix table is present
// Description of the table
    unsigned char    NChIn          ; // No. of Input channels
    unsigned char    NChOut         ; // No. of output channels
    unsigned int     InMode         ; // Inmode for DMix
    unsigned int     OutMode        ; // Outmode after DMix
    short            DownMixTableCoeff[MAX_NB_CHANNEL_COEFF][MAX_NB_CHANNEL_COEFF]; // Coefficients of the table
} DownMixTable_t;

#define MAX_SUPPORTED_DMIX_TABLE 2  // Max table to be exported from Codec to Manifestor

typedef enum
{
    AudioOriginalEncodingUnknown = 0,
    AudioOriginalEncodingAc3,     ///< Indicates AC3 is present in CodedDataBuffer
    AudioOriginalEncodingDdplus,  ///< Indicates DD+ is present in CodedDataBuffer, and AC3 in TranscodedDatabuffer
    AudioOriginalEncodingDts,     ///< Indicates DTS is present in CodedDataBuffer
    AudioOriginalEncodingDtshd,   ///< Indicates DTSHD is present in CodedDataBuffer, and DTS in TranscodedDatabuffer
    AudioOriginalEncodingDtshdMA, ///< Indicates DTSHD Master Audio is present in CodedDataBuffer, and DTS in TranscodedDatabuffer
    AudioOriginalEncodingDtshdLBR,///< Indicates DTSHD LBR is present in CodedDataBuffer, and TranscodedDatabuffer is void
    AudioOriginalEncodingTrueHD,  ///< Indicates Dolby TrueHD is present in CodedDataBuffer, and TranscodedDatabuffer is void
    AudioOriginalEncodingAAC,     ///< Indicates MP2-AAC is present in CodedDataBuffer, and TranscodedDatabuffer is void
    AudioOriginalEncodingHEAAC_960, ///< Indicates MP4-HEAAC is present in CodedDataBuffer, and TranscodedDatabuffer is void
    AudioOriginalEncodingHEAAC_1024,///< Indicates MP4-HEAAC is present in CodedDataBuffer, and TranscodedDatabuffer is void
    AudioOriginalEncodingHEAAC_1920,///< Indicates MP4-HEAAC is present in CodedDataBuffer, and TranscodedDatabuffer is void
    AudioOriginalEncodingHEAAC_2048,///< Indicates MP4-HEAAC is present in CodedDataBuffer, and TranscodedDatabuffer is void
    AudioOriginalEncodingDPulse,  ///< Indicates DolbyPulse is present in CodedDataBuffer, and AC3 inTranscodedDatabuffer
    AudioOriginalEncodingSPDIFIn_Compressed, ///< Indicates compressed data from SPDIFIn is present in CodedDataBuffer
    AudioOriginalEncodingSPDIFIn_Pcm, ///< Indicates PCM data from SPDIFIn is present in CodedDataBuffer
    AudioOriginalEncodingMax      ///< Max bypass supported. Do not update below this line
} AudioOriginalEncoding_t;

// This structure keeps track of a backward compatible part of a
// stream (e.g DTS core part of DTS-HD stream)
typedef struct BackwardCompatibleProperties_s
{
    unsigned int                SampleRateHz;
    unsigned int                SampleCount;
} BackwardCompatibleProperties_t;

typedef struct SpdifInProperties_s
{
    stm_se_audio_stream_type_t     SpdifInLayout;              ///<
    unsigned int                   SpdifInStreamType;          ///< When OriginalEncoding is SPDIFIn_Comprssed it tell the compressed StreamType
    unsigned short                 PaOffSetInCompressedBuffer; ///< When OriginalEncoding is SPDIFIn_Comprssed then offset of the Pa in Compressed buffer
    unsigned int                   ChannelCount;               ///< Number of channels in the SPDIFIn
    int                            Organisation;               ///< Channel_allocation (converted as per AC_MODE) used in multichannel PCM in
    unsigned int                   SamplingFrequency;          ///< Sampling Frequency converted to ISO Integer value in Hz
    bool                           DownMixInhibit;
    unsigned char                  LevelShiftValue;
    unsigned char                  LfePlaybackLevel;
} SpdifInProperties_t;

typedef struct ParsedAudioParameters_s
{
    AudioSurfaceParameters_t       Source;
    unsigned int                   SampleCount;
    bool                           Emphasis;
    int                            Organisation;     ///< Indicates the organisation of the channels (e.g. 5.1, 2.0)
    AudioOriginalEncoding_t        OriginalEncoding;
    SpdifInProperties_t            SpdifInProperties; /// Used when OriginalEncoding is SPDIFIn_Comprssed or SPDIFIn_Pcm
    StreamMetadata_t               StreamMetadata;   ///< Stream metadata mainly used by fatpipe
    MixingMetadata_t               MixingMetadata;   ///< Mixing metadata used by BD Mixer (present in secondary streams)
    BackwardCompatibleProperties_t BackwardCompatibleProperties; // Properties of the backward compatible part of a stream
    DownMixTable_t                 DownMixTable[MAX_SUPPORTED_DMIX_TABLE]; // Downmix table reported by FW to be used by mixer to set the user defined downmix table
} ParsedAudioParameters_t;

#define METADATA_PARSED_AUDIO_PARAMETERS        "ParsedAudioParameters"
#define METADATA_PARSED_AUDIO_PARAMETERS_TYPE   {METADATA_PARSED_AUDIO_PARAMETERS, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(ParsedAudioParameters_t)}

// //////////////////////////////////////////////////////////////////////
//
//      Output timing information generic
//      generated by timing module, and fed back from manifestor.
//

typedef struct ManifestationOutputTiming_s
{
    unsigned long long  SystemPlaybackTime;
    unsigned long long  ExpectedDurationTime;
    unsigned long long  ActualSystemPlaybackTime;  // Manifestor generated

    Rational_t          OutputRateAdjustment;
    Rational_t          SystemClockAdjustment;

    bool                TimingValid;

    bool                Interlaced;             // Video
    bool                TopFieldFirst;          // Video Interlaced frames only
    unsigned int        DisplayCount[2];        // Non-interlaced and audio use [0] only
} ManifestationOutputTiming_t;

typedef struct OutputTiming_s
{
    bool                  RetimeBuffer;            // Used to signal buffer needs re-timing
    unsigned long long    BaseSystemPlaybackTime;  // Used in manifestor of last resort
    unsigned int          HighestTimingIndex;
    ManifestationOutputTiming_t  ManifestationTimings[MAXIMUM_MANIFESTATION_TIMING_COUNT];
} OutputTiming_t;

#define METADATA_OUTPUT_TIMING            "OutputTiming"
#define METADATA_OUTPUT_TIMING_TYPE       {METADATA_OUTPUT_TIMING, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(OutputTiming_t)}

typedef enum
{
    SURFACE_OUTPUT_TYPE_VIDEO_MAIN,
    SURFACE_OUTPUT_TYPE_VIDEO_AUX,
    SURFACE_OUTPUT_TYPE_VIDEO_PIP,
    SURFACE_OUTPUT_TYPE_VIDEO_NO_OUTPUT,
} OutputType_t;

// //////////////////////////////////////////////////////////////////////
//
//      Generic output surface descriptor used by manifestation coordinator
//

typedef struct OutputSurfaceDescriptor_s
{
    PlayerStreamType_t  StreamType;
    bool                ClockPullingAvailable;
    bool                MasterCapable;                  // Are we capable of being a master clock (I suspect everyone might be, but am not sure)
    bool                InheritRateAndTypeFromSource;   // For surfaces that do not impose there own rate and scantype/channel information
    OutputType_t        OutputType;                     // Type of the surface
    bool                PercussiveCapable;              // Can the surface do AVSync percussive adjustments

    bool                IsSlavedSurface;                // Is the surface a slave surface behind a master surface (e.g MDTP -> FVDP on Orly HW)
    struct OutputSurfaceDescriptor_s   *MasterSurface;  // when surface is a slave surface, points to the master surface descriptor

    unsigned int            DisplayWidth;               // Video only fields
    unsigned int            DisplayHeight;
    bool                    Progressive;
    Rational_t              FrameRate;

    unsigned int            BitsPerSample;              // Audio only fields
    unsigned int            ChannelCount;
    unsigned int            SampleRateHz;

    OutputSurfaceDescriptor_s(void)
        : StreamType(StreamTypeNone)
        , ClockPullingAvailable(false)
        , MasterCapable(false)
        , InheritRateAndTypeFromSource(false)
        , OutputType(SURFACE_OUTPUT_TYPE_VIDEO_NO_OUTPUT)
        , PercussiveCapable(false)
        , IsSlavedSurface(false)
        , MasterSurface(NULL)
        , DisplayWidth(0)
        , DisplayHeight(0)
        , Progressive(false)
        , FrameRate()
        , BitsPerSample(0)
        , ChannelCount(0)
        , SampleRateHz(0)
    {}
} OutputSurfaceDescriptor_t;


// //////////////////////////////////////////////////////////////////////
//
//      Trick mode configuration parameters from the codec
//      information feedback mechanism.
//

typedef struct CodecTrickModeParameters_s
{
    Rational_t      EmpiricalMaximumDecodeFrameRateShortIntegration;        // Observed values out of the transformer
    Rational_t      EmpiricalMaximumDecodeFrameRateLongIntegration;         // Observed values out of the transformer
    // Observed values computed from the point the picture enter the decoder thread (Input function) to the point where it is decoded
    // These values are used to set the threshold to go to I only decode mode
    Rational_t      DecodeFrameRateShortIntegrationForIOnly;
    Rational_t      DecodeFrameRateLongIntegrationForIOnly;
    bool            SubstandardDecodeSupported;
    Rational_t      SubstandardDecodeRateIncrease;

    unsigned int    DefaultGroupSize;
    unsigned int    DefaultGroupReferenceFrameCount;        // Including the key frame

    CodecTrickModeParameters_s(void)
        : EmpiricalMaximumDecodeFrameRateShortIntegration()
        , EmpiricalMaximumDecodeFrameRateLongIntegration()
        , DecodeFrameRateShortIntegrationForIOnly()
        , DecodeFrameRateLongIntegrationForIOnly()
        , SubstandardDecodeSupported(false)
        , SubstandardDecodeRateIncrease()
        , DefaultGroupSize(0)
        , DefaultGroupReferenceFrameCount(0)
    {}
} CodecTrickModeParameters_t;

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

#endif
