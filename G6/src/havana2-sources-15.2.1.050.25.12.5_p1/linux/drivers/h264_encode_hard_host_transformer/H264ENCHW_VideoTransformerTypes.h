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


#ifndef __H264ENCHW_VIDEO_TRANSFORMER_TYPES_H
#define __H264ENCHW_VIDEO_TRANSFORMER_TYPES_H

#define H264ENCHW_MME_TRANSFORMER_NAME    "H264ENCHW_TRANSFORMER"

#define ENTRY(enum) case enum: return #enum

typedef enum {
	HVA_ENCODE_SLICE_MODE_SINGLE,   // 1 only slice per encoded frame
	HVA_ENCODE_SLICE_MODE_MAX_MB,   // A slice is closed as soon as the slice_max_mb_size limit is reached
	HVA_ENCODE_SLICE_MODE_MAX_BYTES // A slice is closed as soon as the slice_max_byte_size limit is reached
} H264EncodeSliceMode_t;

static inline const char *StringifySliceMode(H264EncodeSliceMode_t aSliceMode)
{
	switch (aSliceMode) {
		ENTRY(HVA_ENCODE_SLICE_MODE_SINGLE);
		ENTRY(HVA_ENCODE_SLICE_MODE_MAX_MB);
		ENTRY(HVA_ENCODE_SLICE_MODE_MAX_BYTES);
	default: return "<unknown slice mode>";
	}
}

typedef enum {
	HVA_ENCODE_NO_BRC    = 0, //fixed Qp
	HVA_ENCODE_CBR       = 1,
	HVA_ENCODE_VBR       = 2
} H264EncodeBrcType_t;

static inline const char *StringifyBrcType(H264EncodeBrcType_t aBrcType)
{
	switch (aBrcType) {
		ENTRY(HVA_ENCODE_NO_BRC);
		ENTRY(HVA_ENCODE_CBR);
		ENTRY(HVA_ENCODE_VBR);
	default: return "<unknown brc type>";
	}
}

typedef enum {
	HVA_ENCODE_NO_T8x8_ALLOWED    = 0,
	HVA_ENCODE_T8x8_ALLOWED       = 1  // T8x8 partition
} H264EncodeTransformMode_t;

static inline const char *StringifyTransformMode(H264EncodeTransformMode_t aTransformMode)
{
	switch (aTransformMode) {
		ENTRY(HVA_ENCODE_NO_T8x8_ALLOWED);
		ENTRY(HVA_ENCODE_T8x8_ALLOWED);
	default: return "<unknown transform mode>";
	}
}

typedef enum {
	HVA_ENCODE_I_16x16_P_16x16                  = 0,
	HVA_ENCODE_I_16x16_I_NxN_P_16x16            = 1,
	HVA_ENCODE_I_16x16_I_NxN_P_16x16_P_WxH      = 2,
	HVA_ENCODE_I_16x16_P_16x16_LITE             = 4, // reduced complexity
	HVA_ENCODE_I_16x16_I_NxN_P_16x16_LITE       = 5, // reduced complexity
	HVA_ENCODE_I_16x16_I_NxN_P_16x16_P_WxH_LITE = 6  // reduced complexity
} H264EncodeEncoderComplexity_t;

static inline const char *StringifyEncoderComplexity(H264EncodeEncoderComplexity_t aEncoderComplexity)
{
	switch (aEncoderComplexity) {
		ENTRY(HVA_ENCODE_I_16x16_P_16x16);
		ENTRY(HVA_ENCODE_I_16x16_I_NxN_P_16x16);
		ENTRY(HVA_ENCODE_I_16x16_I_NxN_P_16x16_P_WxH);
		ENTRY(HVA_ENCODE_I_16x16_P_16x16_LITE);
		ENTRY(HVA_ENCODE_I_16x16_I_NxN_P_16x16_LITE);
		ENTRY(HVA_ENCODE_I_16x16_I_NxN_P_16x16_P_WxH_LITE);
	default: return "<unknown encoder complexity>";
	}
}

typedef enum {
	HVA_ENCODE_YUV420_SEMI_PLANAR    = 0,
	HVA_ENCODE_YUV422_RASTER         = 1,
	HVA_ENCODE_YUV420_MB_SEMI_PLANAR = 2
} H264EncodeSamplingMode_t;

static inline const char *StringifySamplingMode(H264EncodeSamplingMode_t aSamplingMode)
{
	switch (aSamplingMode) {
		ENTRY(HVA_ENCODE_YUV420_SEMI_PLANAR);
		ENTRY(HVA_ENCODE_YUV422_RASTER);
		ENTRY(HVA_ENCODE_YUV420_MB_SEMI_PLANAR);
	default: return "<unknown sampling mode>";
	}
}

static inline uint32_t HvaGetBitPerPixel(H264EncodeSamplingMode_t SamplingMode)
{
	uint32_t bitPerPixel = 0;

	switch (SamplingMode) {
	case HVA_ENCODE_YUV420_SEMI_PLANAR:
		bitPerPixel = 12;
		break;
	case HVA_ENCODE_YUV422_RASTER:
		bitPerPixel = 16;
		break;
	case HVA_ENCODE_YUV420_MB_SEMI_PLANAR:
		bitPerPixel = 12;
		break;
	default:
		return 0;
	}

	return bitPerPixel;
}

typedef enum {
	HVA_ENCODE_DISABLE_INTRA_REFRESH     = 0,
	HVA_ENCODE_ADAPTIVE_INTRA_REFRESH    = 1,
	HVA_ENCODE_CYCLIC_INTRA_REFRESH      = 2
} H264EncodeIntraRefreshType_t;

static inline const char *StringifyIntraRefreshType(H264EncodeIntraRefreshType_t aIntraRefreshType)
{
	switch (aIntraRefreshType) {
		ENTRY(HVA_ENCODE_DISABLE_INTRA_REFRESH);
		ENTRY(HVA_ENCODE_ADAPTIVE_INTRA_REFRESH);
		ENTRY(HVA_ENCODE_CYCLIC_INTRA_REFRESH);
	default: return "<unknown intra refresh type>";
	}
}

typedef enum {
	HVA_ENCODE_SPS_PROFILE_IDC_BASELINE = 66,
	HVA_ENCODE_SPS_PROFILE_IDC_MAIN     = 77,
	HVA_ENCODE_SPS_PROFILE_IDC_EXTENDED = 88,
	HVA_ENCODE_SPS_PROFILE_IDC_HIGH     = 100,
	HVA_ENCODE_SPS_PROFILE_IDC_HIGH_10  = 110,
	HVA_ENCODE_SPS_PROFILE_IDC_HIGH_422 = 122,
	HVA_ENCODE_SPS_PROFILE_IDC_HIGH_444 = 144
} H264EncodeSPSProfileIDC_t;

static inline const char *StringifyProfile(H264EncodeSPSProfileIDC_t aProfile)
{
	switch (aProfile) {
		ENTRY(HVA_ENCODE_SPS_PROFILE_IDC_BASELINE);
		ENTRY(HVA_ENCODE_SPS_PROFILE_IDC_MAIN);
		ENTRY(HVA_ENCODE_SPS_PROFILE_IDC_EXTENDED);
		ENTRY(HVA_ENCODE_SPS_PROFILE_IDC_HIGH);
		ENTRY(HVA_ENCODE_SPS_PROFILE_IDC_HIGH_10);
		ENTRY(HVA_ENCODE_SPS_PROFILE_IDC_HIGH_422);
		ENTRY(HVA_ENCODE_SPS_PROFILE_IDC_HIGH_444);
	default: return "<unknown profile>";
	}
}

typedef enum {
	HVA_ENCODE_SPS_LEVEL_IDC_1_0        = 10,
	HVA_ENCODE_SPS_LEVEL_IDC_1_B        = 101,
	HVA_ENCODE_SPS_LEVEL_IDC_1_1        = 11,
	HVA_ENCODE_SPS_LEVEL_IDC_1_2        = 12,
	HVA_ENCODE_SPS_LEVEL_IDC_1_3        = 13,
	HVA_ENCODE_SPS_LEVEL_IDC_2_0        = 20,
	HVA_ENCODE_SPS_LEVEL_IDC_2_1        = 21,
	HVA_ENCODE_SPS_LEVEL_IDC_2_2        = 22,
	HVA_ENCODE_SPS_LEVEL_IDC_3_0        = 30,
	HVA_ENCODE_SPS_LEVEL_IDC_3_1        = 31,
	HVA_ENCODE_SPS_LEVEL_IDC_3_2        = 32,
	HVA_ENCODE_SPS_LEVEL_IDC_4_0        = 40,
	HVA_ENCODE_SPS_LEVEL_IDC_4_1        = 41,
	HVA_ENCODE_SPS_LEVEL_IDC_4_2        = 42,
	HVA_ENCODE_SPS_LEVEL_IDC_5_0        = 50,
	HVA_ENCODE_SPS_LEVEL_IDC_5_1        = 51,
	HVA_ENCODE_SPS_LEVEL_IDC_5_2        = 52
} H264EncodeSPSLevelIDC_t;

static inline const char *StringifyLevel(H264EncodeSPSLevelIDC_t aLevel)
{
	switch (aLevel) {
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_1_0);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_1_B);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_1_1);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_1_2);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_1_3);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_2_0);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_2_1);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_2_2);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_3_0);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_3_1);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_3_2);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_4_0);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_4_1);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_4_2);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_5_0);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_5_1);
		ENTRY(HVA_ENCODE_SPS_LEVEL_IDC_5_2);
	default: return "<unknown level>";
	}
}

typedef enum {
	HVA_ENCODE_VUI_ASPECT_RATIO_UNSPECIFIED  = 0,
	HVA_ENCODE_VUI_ASPECT_RATIO_1_1          = 1,
	HVA_ENCODE_VUI_ASPECT_RATIO_12_11        = 2,
	HVA_ENCODE_VUI_ASPECT_RATIO_10_11        = 3,
	HVA_ENCODE_VUI_ASPECT_RATIO_16_11        = 4,
	HVA_ENCODE_VUI_ASPECT_RATIO_40_33        = 5,
	HVA_ENCODE_VUI_ASPECT_RATIO_24_11        = 6,
	HVA_ENCODE_VUI_ASPECT_RATIO_20_11        = 7,
	HVA_ENCODE_VUI_ASPECT_RATIO_32_11        = 8,
	HVA_ENCODE_VUI_ASPECT_RATIO_80_33        = 9,
	HVA_ENCODE_VUI_ASPECT_RATIO_18_11        = 10,
	HVA_ENCODE_VUI_ASPECT_RATIO_15_11        = 11,
	HVA_ENCODE_VUI_ASPECT_RATIO_64_33        = 12,
	HVA_ENCODE_VUI_ASPECT_RATIO_160_99       = 13,
	HVA_ENCODE_VUI_ASPECT_RATIO_4_3          = 14,
	HVA_ENCODE_VUI_ASPECT_RATIO_3_2          = 15,
	HVA_ENCODE_VUI_ASPECT_RATIO_2_1          = 16,
	HVA_ENCODE_VUI_ASPECT_RATIO_EXTENDED_SAR = 255
} H264EncodeVUIAspectRatio_t;

static inline const char *StringifyAspectRatio(H264EncodeVUIAspectRatio_t aAspectRatio)
{
	switch (aAspectRatio) {
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_UNSPECIFIED);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_1_1);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_12_11);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_10_11);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_16_11);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_40_33);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_24_11);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_20_11);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_32_11);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_80_33);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_18_11);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_15_11);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_64_33);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_160_99);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_4_3);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_3_2);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_2_1);
		ENTRY(HVA_ENCODE_VUI_ASPECT_RATIO_EXTENDED_SAR);
	default: return "<unknown aspect ratio>";
	}
}

typedef enum {
	HVA_ENCODE_VUI_VIDEO_FORMAT_COMPONENT   = 0,
	HVA_ENCODE_VUI_VIDEO_FORMAT_PAL         = 1,
	HVA_ENCODE_VUI_VIDEO_FORMAT_NTSC        = 2,
	HVA_ENCODE_VUI_VIDEO_FORMAT_SECAM       = 3,
	HVA_ENCODE_VUI_VIDEO_FORMAT_MAC         = 4,
	HVA_ENCODE_VUI_VIDEO_FORMAT_UNSPECIFIED = 5
} H264EncodeVUIVideoFormat_t;

static inline const char *StringifyVideoFormat(H264EncodeVUIVideoFormat_t aVideoFormat)
{
	switch (aVideoFormat) {
		ENTRY(HVA_ENCODE_VUI_VIDEO_FORMAT_COMPONENT);
		ENTRY(HVA_ENCODE_VUI_VIDEO_FORMAT_PAL);
		ENTRY(HVA_ENCODE_VUI_VIDEO_FORMAT_NTSC);
		ENTRY(HVA_ENCODE_VUI_VIDEO_FORMAT_SECAM);
		ENTRY(HVA_ENCODE_VUI_VIDEO_FORMAT_MAC);
		ENTRY(HVA_ENCODE_VUI_VIDEO_FORMAT_UNSPECIFIED);
	default: return "<unknown video format>";
	}
}

typedef enum {
	HVA_ENCODE_VUI_COLOR_STD_BT_709_5       = 1,
	HVA_ENCODE_VUI_COLOR_STD_UNSPECIFIED    = 2,
	HVA_ENCODE_VUI_COLOR_STD_BT_470_6_M     = 4,
	HVA_ENCODE_VUI_COLOR_STD_BT_470_6_BG    = 5,
	HVA_ENCODE_VUI_COLOR_STD_SMPTE_170M     = 6,
	HVA_ENCODE_VUI_COLOR_STD_SMPTE_240M     = 7
} H264EncodeVUIColorStd_t;

static inline const char *StringifyColorStd(H264EncodeVUIColorStd_t aColorStd)
{
	switch (aColorStd) {
		ENTRY(HVA_ENCODE_VUI_COLOR_STD_BT_709_5);
		ENTRY(HVA_ENCODE_VUI_COLOR_STD_UNSPECIFIED);
		ENTRY(HVA_ENCODE_VUI_COLOR_STD_BT_470_6_M);
		ENTRY(HVA_ENCODE_VUI_COLOR_STD_BT_470_6_BG);
		ENTRY(HVA_ENCODE_VUI_COLOR_STD_SMPTE_170M);
		ENTRY(HVA_ENCODE_VUI_COLOR_STD_SMPTE_240M);
	default: return "<unknown color standard>";
	}
}

//
// H264 sequence parameters required by MME to perform a sequence encode.
//
typedef struct {
	uint16_t  frameWidth;               // relates to VideoEncoderControl port.width
	uint16_t  frameHeight;              // relates to VideoEncoderControl port.height
	uint16_t  picOrderCntType;          // relates with SPS
	uint16_t  log2MaxFrameNumMinus4;    // relates with SPS
	bool useConstrainedIntraFlag;       // relates with SPS
	H264EncodeIntraRefreshType_t  intraRefreshType;
	uint16_t  irParamOption;            // Nb of MB to be refreshed for AIR or Refresh period for CIR
	uint32_t  maxSumNumBitsInNALU;
	H264EncodeBrcType_t  brcType;
	uint32_t  cpbBufferSize;            // VBR related: impacts on end to end delay (encode/decode)
	uint32_t  bitRate;
	uint16_t  framerateNum;
	uint16_t  framerateDen;
	H264EncodeTransformMode_t  transformMode;
	H264EncodeEncoderComplexity_t  encoderComplexity;  //advised to set it to max complexity '2'
	uint16_t  vbrInitQp;               // VBR: initial value of quantisation step
	H264EncodeSamplingMode_t  samplingMode;
	uint16_t  cbrDelay;                // expressed in ms
	uint16_t  qpmin;
	uint16_t  qpmax;
	uint16_t  sliceNumber;             // relate to VideoEncoderControl port.slice
	H264EncodeSPSProfileIDC_t   profileIdc;                   // relates with SPS encoding profile
	H264EncodeSPSLevelIDC_t     levelIdc;                     // relates with SPS encoding level
	bool                        vuiParametersPresentFlag;     // relates with SPS VUI parameter presence
	H264EncodeVUIAspectRatio_t  vuiAspectRatioIdc;            // relates with VUI aspact ratio
	uint16_t                    vuiSarWidth;                  // relates with VUI aspact ratio
	uint16_t                    vuiSarHeight;                 // relates with VUI aspact ratio
	bool                        vuiOverscanAppropriateFlag;   // relates with VUI overscan
	H264EncodeVUIVideoFormat_t  vuiVideoFormat;               // relates with VUI video format
	bool                        vuiVideoFullRangeFlag;        // relates with VUI full or standard range
	H264EncodeVUIColorStd_t     vuiColorStd;                  // relates with VUI color primaries
	// transfer characteristics and matrix coefficients
	bool                        seiPicTimingPresentFlag;      // relates with SEI picture timng
	bool                        seiBufPeriodPresentFlag;      // relates with SEI buffering period
} H264EncodeHard_SetGlobalParamSequence_t;


typedef enum {
	HVA_ENCODE_I_FRAME    = 0,
	HVA_ENCODE_P_FRAME    = 1
} H264EncodePictureCodingType_t;

static inline const char *StringifyPictureCodingType(H264EncodePictureCodingType_t aPictureCodingType)
{
	switch (aPictureCodingType) {
		ENTRY(HVA_ENCODE_I_FRAME);
		ENTRY(HVA_ENCODE_P_FRAME);
	default: return "<unknown picture coding type>";
	}
}

typedef enum {
	HVA_ENCODE_DEBLOCKING_ENABLE                             = 0,
	HVA_ENCODE_DEBLOCKING_DISABLE                            = 1,
	HVA_ENCODE_DEBLOCKING_ENABLE_EXCEPT_FOR_SLICE_BOUNDARIES = 2
} H264EncodeDeblocking_t;

static inline const char *StringifyDeblocking(H264EncodeDeblocking_t aDeblocking)
{
	switch (aDeblocking) {
		ENTRY(HVA_ENCODE_DEBLOCKING_ENABLE);
		ENTRY(HVA_ENCODE_DEBLOCKING_DISABLE);
		ENTRY(HVA_ENCODE_DEBLOCKING_ENABLE_EXCEPT_FOR_SLICE_BOUNDARIES);
	default: return "<unknown deblocking>";
	}
}

//
// H264 frame specific parameters required to perform an H264 frame encoding
//
typedef struct {
	H264EncodePictureCodingType_t  pictureCodingType;
	bool idrFlag;
	uint32_t  frameNum;
	bool firstPictureInSequence;
	H264EncodeDeblocking_t  disableDeblockingFilterIdc;
	int16_t   sliceAlphaC0OffsetDiv2;
	int16_t   sliceBetaOffsetDiv2;
	uint32_t  addrSourceBuffer;
	uint32_t  addrOutputBitstreamStart;       //This address should be 8 bytes aligned and is the aligned address from which HVA will write (offset will compensate alignment offset)
	uint32_t  addrOutputBitstreamEnd;         //This address is the buffer end address
	uint16_t  bitstreamOffset;                //offset in bits between aligned bitstream start address and first bit to be written by HVA . Range value is [0..63]
	bool     seiRecoveryPtPresentFlag;        // relates with SEI to represent OpenGOP
	uint16_t seiRecoveryFrameCnt;             // relates with open gop to specify next good frame
	bool     seiBrokenLinkFlag;               // relates with open gop to specify broken link
	bool     seiUsrDataT35PresentFlag;        // relates with SEI to represent close caption
	uint8_t  seiT35CountryCode;               // relates with close caption 0xB5
	uint32_t seiAddrT35PayloadByte;           // relates with close caption address of payload to specify provider code 0x31, user identifier 0x47413934, user_structure
	uint32_t seiT35PayloadSize;               // relates with close caption size of payload including the provider code 0x31, user identifier 0x47413934, user_structure
	uint16_t log2MaxFrameNumMinus4;                  // relates to flexible slice header (frame_num)
} H264EncodeHard_TransformParam_t;


//
// H264 encode transform capabilities
//
typedef struct {
	uint32_t    MaxXSize;
	uint32_t    MaxYSize;
	uint32_t MaxFrameRate;             // MaxFrameRate to be interpreted in sync with MaxXSize and MaxYSize
	uint32_t BufferAlignment;          //Buffer allocated by client (Frame & Bitstream buffers) must respect this alignment
	bool isT8x8Supported;
	bool areBFramesSupported;
	bool isSliceLossSupported;
	bool isIntraRefreshSupported;
	bool isMultiSliceEncodeSupported;
	bool isOpenGopSupported;
} H264EncodeHard_TransformerCapability_t;

typedef enum {
	H264ENCHARD_NO_HW_ERROR              = 0x0,
	H264ENCHARD_HW_LMI_ERR               = 0x1, //"Local Memory interface" error interrupt occurred
	H264ENCHARD_HW_EMI_ERR               = 0x2, //"External Memory interface" error interrupt occurred
	//H264ENCHARD_HW_HEC_MIF_ERR         = 0x3,
	H264ENCHARD_HW_SFL_OVERFLOW          = 0x4  //means command overflow (not real interrupt but SW check in interrupt handler of fifo level)
} H264EncodeHard_InterruptError_t;

static inline const char *StringifyInterruptError(H264EncodeHard_InterruptError_t aInterruptError)
{
	switch (aInterruptError) {
		ENTRY(H264ENCHARD_NO_HW_ERROR);
		ENTRY(H264ENCHARD_HW_LMI_ERR);
		ENTRY(H264ENCHARD_HW_EMI_ERR);
		ENTRY(H264ENCHARD_HW_SFL_OVERFLOW);
	default: return "<unknown interrupt error>";
	}
}

//
//WARNING: this transform status should be analyzed by client at processing's end to catch frame skip, etc...
//If MME Transformer processing OK, processCommand will return a MME_SUCCESS
//If HW interrupt occurs, MME Transformer processCommand will return MME_INTERNAL_ERROR or MME_COMMAND_STILL_EXECUTING
//
typedef enum {
	HVA_ENCODE_OK                        = 0x0,
	HVA_ENCODE_ABORT_BITSTREAM_OVERSIZE  = 0x2,  //Picture bitstream size overcomes max_bitstream_size
	HVA_ENCODE_FRAME_SKIPPED             = 0x4,  //referred to HRD model and BRC: refers to CPB Buffer Size.
	HVA_ENCODE_ABORT_SLICE_LIMIT_SIZE    = 0x5,  //One MB does not fit inside slice limit size for ‘slice mode’ 2 & 3: picture encode abort.
	HVA_ENCODE_TASK_LIST_FULL            = 0xF0, //HVA/FPC task list full: discard latest transform command
	HVA_ENCODE_UNKNOWN_COMMAND           = 0xF1  //Transform command not known by HVA/FPC
} H264EncodeHard_TransformStatus_t;

static inline const char *StringifyTransformStatus(H264EncodeHard_TransformStatus_t aTransformStatus)
{
	switch (aTransformStatus) {
		ENTRY(HVA_ENCODE_OK);
		ENTRY(HVA_ENCODE_ABORT_BITSTREAM_OVERSIZE);
		ENTRY(HVA_ENCODE_FRAME_SKIPPED);
		ENTRY(HVA_ENCODE_ABORT_SLICE_LIMIT_SIZE);
		ENTRY(HVA_ENCODE_TASK_LIST_FULL);
		ENTRY(HVA_ENCODE_UNKNOWN_COMMAND);
	default: return "<unknown transform status>";
	}
}

//
// Defines the H264 encoding additional info of command status
// the video driver will use this structure for MME_CommandStatus_t.AdditionalInfo_p
//
typedef struct {
	uint32_t StructSize;     // Size of the structure in bytes
	uint32_t bitstreamSize;  // Size in bytes of the current frame generated by HVA ( excluding headers and stuffing bits )
	uint32_t stuffingBits;   // Number of stuffing bits generated at the end of encoded frame
	uint32_t nonVCLNALUSize;   // Number of stuffing bits generated at the end of encoded frame
	uint32_t removalTime;
	uint32_t frameEncodeDuration; //Time in us for the HW to encode this frame
	H264EncodeHard_InterruptError_t hwError;
	H264EncodeHard_TransformStatus_t transformStatus;
} H264EncodeHard_AddInfo_CommandStatus_t;

typedef enum H264EncodeMemoryProfile_e {
	HVA_ENCODE_PROFILE_HD      = 3,
	HVA_ENCODE_PROFILE_720P    = 2,
	HVA_ENCODE_PROFILE_SD      = 1,
	HVA_ENCODE_PROFILE_CIF     = 0,
} H264EncodeMemoryProfile_t;

static inline const char *StringifyMemoryProfile(H264EncodeMemoryProfile_t aMemoryProfile)
{
	switch (aMemoryProfile) {
		ENTRY(HVA_ENCODE_PROFILE_HD);
		ENTRY(HVA_ENCODE_PROFILE_720P);
		ENTRY(HVA_ENCODE_PROFILE_SD);
		ENTRY(HVA_ENCODE_PROFILE_CIF);
	default: return "<unknown memory profile>";
	}
}

typedef struct H264EncodeHardInitParams_s {
	H264EncodeMemoryProfile_t MemoryProfile;
} H264EncodeHardInitParams_t;


#endif /* #ifndef __H264ENCHW_VIDEO_TRANSFORMER_TYPES_H */

