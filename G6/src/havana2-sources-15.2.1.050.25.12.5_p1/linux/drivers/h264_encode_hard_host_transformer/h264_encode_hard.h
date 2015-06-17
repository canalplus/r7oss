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

#ifndef H_H264_ENCODE_HARD
#define H_H264_ENCODE_HARD

#include <linux/spinlock.h>

#include "hva_registers.h"

#include "H264ENCHW_VideoTransformerTypes.h"

/*************/
/* Constants */
/*************/

#define ENCODE_BUFFER_ALIGNMENT 32
#define H264_ENCODE_MAX_SIZE_X 1920
#define H264_ENCODE_MAX_SIZE_Y 1088
#define H264_ENCODE_BRC_DATA_SIZE (5*16)
#define H264_ENCODE_TEMPORAL_DATA_SIZE ((H264_ENCODE_MAX_SIZE_X/16)*(H264_ENCODE_MAX_SIZE_Y/16)*16)
#define H264_ENCODE_SPATIAL_DATA_SIZE H264_ENCODE_TEMPORAL_DATA_SIZE

// Default parameter
#define H264_ENCODE_DEFAULT_FRAMERATE_NUM 25
#define H264_ENCODE_DEFAULT_FRAMERATE_DEN 1

// Default header configuration
#define ENABLE_H264ENC_ACCESS_UNIT_DELIMITER 1
#define ENABLE_H264ENC_SPS_PPS_AT_EACH_IDR   1

#define MAX_PARTITION_NAME_SIZE  64 // Follow definition of ALLOCATOR_MAX_PARTITION_NAME_SIZE
#define HVA_MME_PARTITION "vid-enc-data"

//As proposed by hardware team
#define HVA_HIF_MIF_CFG_RECOMMENDED_VALUE 0x0

#define HVA_ENCODE_DONT_USE_STRICT_HRD_COMPLIANCY 0
#define HVA_ENCODE_USE_STRICT_HRD_COMPLIANCY 1

//As proposed by Dario for streaming UC
#define HVA_ENCODE_RECOMMENDED_SLICE_SIZE 4096

//Buffer size constants
//Max MacroBlocs number (x & y)
#define MAX_MBX ((H264_ENCODE_MAX_SIZE_X / 16)+1)
#define MAX_MBY ((H264_ENCODE_MAX_SIZE_Y / 16)+1)

//Window in reference picture in YUV 420 MB-tiled format
#define SEARCH_WINDOW_BUFFER_MAX_SIZE   ((4*MAX_MBX+42)*256*3/2)

//4 lines of pixels (in Luma, Chroma blue and Chroma red) of top MB for deblocking
#define LOCAL_RECONSTRUCTED_BUFFER_MAX_SIZE (4*16*MAX_MBX*2)

//Context buffer
#define CTX_MB_BUFFER_MAX_SIZE (MAX_MBX*16*8)

//Cabac context buffer
#define CABAC_CONTEXT_BUFFER_MAX_SIZE (MAX_MBX*16)

// Defining this depth causes the driver to limit the number of commands sent
#define HVA_CMD_FIFO_DEPTH 8

#define HVA_STATUS_HW_FIFO_SIZE 16
#define LEVEL_SFL_BITS_MASK 0xF

//Task descriptor field masks definition
#define FRAME_DIMENSION_BITS_MASK 0x3FFF
#define PIC_ORDER_CNT_BITS_MASK   0x3
#define LOG2MAX_BITS_MASK         0xF
#define INTRA_FLAG_BITS_MASK      0x1
#define SLICE_MB_SIZE_BITS_MASK   0x1FFFFFFF
#define IR_PARAM_BITS_MASK        0xFF
#define ENCODER_COMPLEXITY_MASK   0x5
#define QP_BITS_MASK              0x3F
#define PICTURE_CODING_TYPE_MASK  0x1;
#define IDR_FLAG_MASK             0x1;
#define FIRST_PICT_MASK           0x1;
#define IDR_TOGGLE_MASK           0x1;
#define ENTROPY_CODING_MODE_MASK  0x1;
#define CHROMA_QP_INDEX_OFFSET_MASK 0x1F;
#define DISABLE_DEBLOCKING_MASK   0x3;
#define SLICE_ALPHA_MASK          0xF;
#define SLICE_BETA_MASK           0xF;
#define BITSTREAM_OFFSET_MASK     0x7F;

// constants for overflow in frame rate
#define OVERFLOW_LIMIT_NUM 32
#define OVERFLOW_LIMIT_DEN 31

// Filler NALU has header length = 48 bits
// 4 bytes header, 1 byte idc+type, 1 byte trailing bits
#define FILLER_DATA_HEADER_SIZE 6

typedef enum {
	H264ENCHARD_NO_ERROR               = 0,
	H264ENCHARD_ERROR                  = 1,
	H264ENCHARD_NO_SDRAM_MEMORY        = 2,
	H264ENCHARD_NO_HVA_ERAM_MEMORY     = 3,
	H264ENCHARD_CMD_DISCARDED          = 4,
	H264ENCHARD_HW_ERROR               = 5,
	H264ENCHARD_ENCODE_DEFERRED        = 6,
} H264EncodeHardStatus_t;

static inline const char *StringifyEncodeStatus(H264EncodeHardStatus_t aEncodeStatus)
{
	switch (aEncodeStatus) {
		ENTRY(H264ENCHARD_NO_ERROR);
		ENTRY(H264ENCHARD_ERROR);
		ENTRY(H264ENCHARD_NO_SDRAM_MEMORY);
		ENTRY(H264ENCHARD_NO_HVA_ERAM_MEMORY);
		ENTRY(H264ENCHARD_CMD_DISCARDED);
		ENTRY(H264ENCHARD_HW_ERROR);
		ENTRY(H264ENCHARD_ENCODE_DEFERRED);
	default: return "<unknown encode status>";
	}
}

typedef void    *H264EncodeHardHandle_t;

typedef enum {
	HVA_ENCODE_NO_CONSTRAINT_TO_CLOSE_SLICE  = 0,
	HVA_ENCODE_SLICE_MB_SIZE_LIMIT           = 1,
	HVA_ENCODE_SLICE_BYTE_SIZE_LIMIT         = 2,
	HVA_ENCODE_SLICE_OR_BYTE_SIZE_LIMIT      = 3
} H264EncodeSliceSizeType_t;

static inline const char *StringifySliceSizeType(H264EncodeSliceSizeType_t aSliceSizeType)
{
	switch (aSliceSizeType) {
		ENTRY(HVA_ENCODE_NO_CONSTRAINT_TO_CLOSE_SLICE);
		ENTRY(HVA_ENCODE_SLICE_MB_SIZE_LIMIT);
		ENTRY(HVA_ENCODE_SLICE_BYTE_SIZE_LIMIT);
		ENTRY(HVA_ENCODE_SLICE_OR_BYTE_SIZE_LIMIT);
	default: return "<unknown slice size type>";
	}
}

// Global hva driver data to store:
// - clk structure (and avoid costly clk_get() calls outside of .probe() )
// - hva device
struct HvaDriverData {
	struct clk       *clk;
	int               clk_ref_count;
	spinlock_t        clk_lock;
	struct device    *dev;
	int               spurious_irq_count;
	unsigned int      max_freq;
};

typedef struct H264EncodeHardSequenceParams_s {
	uint16_t  frameWidth;               // relates to VideoEncoderControl port.width
	uint16_t  frameHeight;              // relates to VideoEncoderControl port.height
	uint16_t  picOrderCntType;          // relate with SPS
	uint16_t  log2MaxFrameNumMinus4;    // relate with SPS
	uint16_t  useConstrainedIntraFlag;  // relate with SPS
	uint16_t  intraRefreshType;
	uint16_t  irParamOption;
	uint32_t  maxSumNumBitsInNALU;
	uint16_t  brcType;
	uint32_t  cpbBufferSize;
	uint32_t  bitRate;
	uint16_t  framerateNum;
	uint16_t  framerateDen;
	uint16_t  transformMode;
	uint16_t  encoderComplexity;
	uint16_t  quant;                   //used for VBR as Qp init value (for fixed Qp also but not envisoned)
	uint16_t  samplingMode;
	uint16_t  delay;
	uint16_t  qpmin;
	uint16_t  qpmax;
	uint16_t  sliceNumber;             // relate to VideoEncoderControl port.slice

	uint8_t   profileIdc;                 // relates with SPS
	uint8_t   levelIdc;                   // relates with SPS

	uint8_t   vuiParametersPresentFlag;   // relates with SPS
	uint8_t   vuiAspectRatioIdc;          // relates with VUI
	uint16_t  vuiSarWidth;                // relates with VUI
	uint16_t  vuiSarHeight;               // relates with VUI
	uint8_t   vuiOverscanAppropriateFlag; // relates with VUI
	uint8_t   vuiVideoFormat;             // relates with VUI
	uint8_t   vuiVideoFullRangeFlag;      // relates with VUI
	uint8_t   vuiColorStd;                // relates with VUI
	uint8_t   seiPicTimingPresentFlag;    // relates with SEI
	uint8_t   seiBufPeriodPresentFlag;    // relates with SEI

} H264EncodeHardSequenceParams_t;

typedef struct H264EncodeHardFrameParams_s {
	uint16_t  pictureCodingType;              /* LS  1 bits used by HW */
	uint16_t  idrFlag;                        /* LS  1 bits used by HW */
	uint32_t  frameNum;                       /* LS 32 bits used by HW */
	uint16_t  firstPictureInSequence;         /* LS  1 bits used by HW */
	uint16_t  disableDeblockingFilterIdc;     /* LS  2 bits used by HW */
	int16_t   sliceAlphaC0OffsetDiv2;         /* LS  4 bits used by HW */ /*/!\ signed! */
	int16_t   sliceBetaOffsetDiv2;            /* LS  4 bits used by HW */ /*/!\ signed! */
	uint32_t  nonVCLNALUSize;                 /* LS 32 bits used by HW */
	uint32_t  lastBPAUts;                     /* LS 32 bits used by HW */ /* TBC */
	uint32_t  nalFinalArrivalTime;            /* LS 32 bits used by HW */
	uint32_t  timestamp;                      /* LS 32 bits used by HW */
	uint16_t  delay;                          /* LS 16 bits used by HW */
	uint32_t  addrSourceBuffer;               /* LS 32 bits used by HW */ /*Frame Buffer to be allocated by MME user*/
	uint32_t  addrOutputBitstreamStart;       /* LS 32 bits used by HW */ /*Bitstream Buffer to be allocated by MME user*/
	uint32_t  addrOutputBitstreamEnd;         /* LS 32 bits used by HW */
	uint16_t  bitstreamOffset;                /* LS  6 bits used by HW */
	uint8_t   seiRecoveryPtPresentFlag;       // relates to SEI
	uint16_t  seiRecoveryFrameCnt;            // relates to Recovery Pt SEI
	uint8_t   seiBrokenLinkFlag;              // relates to Recovery Pt SEI
	uint8_t   seiUsrDataT35PresentFlag;       // relates to SEI
	uint8_t   seiT35CountryCode;              // relates to Usr Data T35 SEI
	uint32_t  seiAddrT35PayloadByte;          // relates to Usr Data T35 SEI
	uint32_t  seiT35PayloadSize;              // relates to Usr Data T35 SEI
	uint16_t  log2MaxFrameNumMinus4;          // relates to slice header (frame_num)
	uint16_t  idrPicId;                       // relates to slice header (idr_pic_id)
} H264EncodeHardFrameParams_t;

typedef struct H264EncodeHardTaskDescriptor_s {
	/* Width in pixels of the buffer containing the input frame */
	uint16_t  frameWidth;                     /* LS 14 bits used by HW */
	/* Height in pixels of the buffer containing the input frame */
	uint16_t  frameHeight;                    /* LS 14 bits used by HW */
	/* Reserved */
	uint32_t  reserved0; /* not used (frame_num) */
	/* 0 = I type, 1 = P type */
	uint16_t  pictureCodingType;              /* LS  1 bits used by HW */
	/* Reserved */
	uint16_t  reserved1; /* not used (idr_flag) */
	/* POC mode, as defined in H264 std : can be 0,1,2 */
	uint16_t  picOrderCntType;                /* LS  2 bits used by HW */
	/* flag telling to encoder that this is the first picture in a video sequence. Used for VBR */
	uint16_t  firstPictureInSequence;         /* LS  1 bits used by HW */
	/* 0 = no constraint to close the slice
	   1= a slice is closed as soon as the slice_mb_size limit is reached
	   2= a slice is closed as soon as the slice_byte_size limit is reached
	   3 = a slice is closed as soon as either the slice_byte_size limit or the slice_mb_size limit is reached */
	uint16_t  sliceSizeType;                  /* LS  2 bits used by HW */
	/* Reserved */
	uint16_t  reserved2; /* not used (idr_toggle) */
	/* defines the slice size in number of macroblocks (used when slice_size_type=1 or slice_size_type=3) */
	uint32_t  sliceMbSize;                    /* LS 29 bits used by HW */
	/* defines the number of macroblocks per frame to be refreshed by AIR algorithm OR the refresh period by CIR algorithm */
	uint16_t  irParamOption;                  /* LS 8 bits used by HW */
	/* Enables the adaptive intra refresh algorithm. Disable=0 / Adaptative=1 and Cycle=2 as intra refresh */
	uint16_t  intraRefreshType;               /* LS 2 bits used by HW */
	/* constrained_intra_pred_flag from PPS */
	uint16_t  useConstrainedIntraFlag;        /* LS  1 bits used by HW */
	/* controls the use of 4x4/8x8 transform mode.
	   0: no T8x8 allowed.
	   1: T8x8 allowed.*/
	uint16_t  transformMode;                  /* LS  1 bits used by HW */
	/* 0: specifies that all luma and chroma block edges of the slice are filtered.
	   1: specifies that deblocking is disabled for all block edges of the slice.
	   2: specifies that all luma and chroma block edges of the slice are filtered with exception of the block edges
	      that coincide with slice boundaries */
	uint16_t  disableDeblockingFilterIdc;     /* LS  2 bits used by HW */
	/* to be written in slice header, controls deblocking */
	int16_t  sliceAlphaC0OffsetDiv2;          /* LS  4 bits used by HW */ /*/!\ signed! */
	/* to be written in slice header, controls deblocking */
	int16_t  sliceBetaOffsetDiv2;             /* LS  4 bits used by HW */ /*/!\ signed! */
	/* encoder complexity control (IME).
	   Cannes/Monaco
	   00 = I_16x16, P_16x16, Full ME Complexity
	   01 = I_16x16, I_NxN, P_16x16, Full ME Complexity
	   02 = I_16x16, I_NXN, P_16x16, P_WxH, Full ME Complexity
	   03 = T8x8 only, Full Me complexity
	   04 = I_16x16, P_16x16, Reduced ME Complexity
	   05 = I_16x16, I_NxN, P_16x16, Reduced ME Complexity
	   06 = I_16x16, I_NXN, P_16x16, P_WxH, Reduced ME Complexity
	   07 = T8x8 only, Reduced ME Complexity
	   08 = I_16x16, P_16x16, Fast ME
	   09 = I_16x16, I_NxN, P_16x16, Fast ME
	   10 = I_16x16, I_NXN, P_16x16, P_WxH, Fast ME
	   11 = I_16x16, I_NXN, P_16x16, P_WxH, Fast ME + T8x8 */
	uint16_t  encoderComplexity;              /* LS  4 bits used by HW */
	/* Common value of parameter chroma_qp_index_offset and second_chroma_qp_index_offset
	   coming from picture parameter set (PPS see [H.264 STD] §7.4.2.2) */
	int16_t  chromaQpIndexOffset;             /* LS  5 bits used by HW */
	/* Encoding mode: 0 = CAVLC, 1 = CABAC */
	uint16_t  entropyCodingMode;              /* LS  1 bits used by HW */
	/* selects the bit-rate control algorithm
	   0 = constant Qp, (no BRC)
	   1 = CBR
	   2 = VBR*/
	uint16_t  brcType;                        /* LS  3 bits used by HW */
	/* Quantization param used in case of fix QP encoding (no BRC)
	   Quantization param to initialize variable bitrate controller when brc_type = 2 */
	uint16_t  quant;                          /* LS  6 bits used by HW */
	/* size of non-VCL NALUs (SPS, PPS, filler), used by BRC */
	uint32_t  nonVCLNALUSize;                 /* LS 32 bits used by HW */
	/* size of Coded Picture Buffer, used by BRC */
	uint32_t  cpbBufferSize;                  /* LS 32 bits used by HW */
	/* target bitrate, for BRC */
	uint32_t  bitRate;                        /* LS 32 bits used by HW */
	/* min QP threshold */
	uint16_t  qpmin;                          /* LS  6 bits used by HW */
	/* max QP threshold */
	uint16_t  qpmax;                          /* LS  6 bits used by HW */
	/* target framerate, used by BRC */
	uint16_t  framerateNum;                   /* LS 16 bits used by HW */
	uint16_t  framerateDen;                   /* LS 16 bits used by HW */
	/* End-to-End Initial Delay */
	uint16_t  delay;                          /* LS 12 bits used by HW */
	/* Flag for HDR compliancy (1) May impact quality encoding */
	uint16_t  strictHRDCompliancy;            /* LS  1 bits used by HW */
	/* address of input frame buffer for current frame */
	uint32_t  addrSourceBuffer;               /* LS 32 bits used by HW */ /*Frame Buffer to be allocated by MME user*/
	/* address of reference frame buffer */
	uint32_t  addrFwdRefBuffer;               /* LS 32 bits used by HW */
	/* address of reconstructed frame buffer */
	uint32_t  addrRecBuffer;                  /* LS 32 bits used by HW */
	/* output bitstream: start address */
	uint32_t  addrOutputBitstreamStart;       /* LS 32 bits used by HW */ /*Bitstream Buffer to be allocated by MME user*/
	/* output bitstream: end address */
	uint32_t  addrOutputBitstreamEnd;         /* LS 32 bits used by HW */
	/* address of external search window */
	uint32_t  addrExternalSw;                 /* LS 32 bits used by HW */
	/* Context picture buffer address */
	uint32_t  addrLctx;                       /* LS 32 bits used by HW */
	/* address of local reconstructed buffer */
	uint32_t  addrLocalRecBuffer;             /* LS 32 bits used by HW */
	/* address of spatial context */
	uint32_t  addrSpatialContext;             /* LS 32 bits used by HW */
	/* offset in bits between aligned bitstream start address and first bit to be written by HVA . Range value is [0..63] */
	uint16_t  bitstreamOffset;                /* LS  6 bits used by HW */
	/* BIT[0:2] Input picture format:
	     0 = YUV 420 raster semi-planar = two planes (one for Y, one for interleaved Cb and Cr : YYYY... / CbCr...)
	     1 = YUV 422 raster interleaved coplanar = one plane (Y, Cb and Cr interleaved : CbYCrYCbYCrY..)
	     2 = YUV 420 MB semi-planar = two planes, MB column wise order arranged (one for Y, one for interleaved Cb and Cr : YYYY... / CbCr...)
	     3 = RGB 888 coplanar
	     4 = ARGB 8888 coplanar
	     5-6-7 = reserved
	   BIT[3] Swap
	   - In case of YUV 420 semiplanar/MB:
	     0 = CbCrCbCr (Cb is byte 0)
	     1 = CrCbCrCb (Cr is byte 0)
	   - In case of YUV 422 coplanar:
	     0 = YCbYCr (Cb is byte 1)
	     1 = YCrYCb (Cr is byte 1)
	   - In case of RGB or ARGB:
	     0 = RGB (R is byte 0)
	     1 = BGR (B is byte 0)
	   BIT[4] Alpha position:
	     0 = ARGB (A is byte 0)
	     1 = RGBA (A is byte 3)
	   BIT [5-7] Reserved */
	uint16_t  samplingMode;                   /* LS  8 bits used by HW */
	/* address of output parameters structure */
	uint32_t  addrParamInout;                 /* LS 32 bits used by HW */
	/* address to the coefficient of the inverse scaling matrix */
	uint32_t  addrScalingMatrix;              /* LS 32 bits used by HW */
	/* address to the coefficient of the direct scaling matrix */
	uint32_t  addrScalingMatrixDir;           /* LS 32 bits used by HW */
	/* context CABAC data in raw format */
	uint32_t  addrCabacContextBuffer;         /* LS 32 bits used by HW */
	/* Reserved */
	uint32_t  reserved3; /* not used (addr_dct_bitstream_start) */
	/* Reserved */
	uint32_t  reserved4; /* not used (addr_dct_bitstream_end) */
	/* Input information about the horizontal global displacement of the encoded frame versus the previous one */
	int16_t  GmvX;         /* LS 16 bits used by HW */
	/* Input information about the vertical global displacement of the encoded frame versus the previous one */
	int16_t  GmvY;         /* LS 16 bits used by HW */
	/* Width in pels of the window to be encoded inside the input frame */
	uint16_t  windowWidth;                    /* LS 14 bits used by HW */
	/* Height in pels of the window to be encoded inside the input frame */
	uint16_t  windowHeight;                   /* LS 14 bits used by HW */
	/* Horizontal offset in pels for input window within input frame */
	uint16_t  windowHorizontalOffset;         /* LS 14 bits used by HW */
	/* Vertical offset in pels for input window within input frame */
	uint16_t  windowVerticalOffset;           /* LS 14 bits used by HW */
	/* Map of QP offset for the Region of Interest algorithm and also used for Error map.
	   Bit 0-6 used for qp offset (value -64 to 63).
	   Bit 7 used to force intra */
	uint32_t  addrRoi;                        /* LS 32 bits used by HW */
	/* Slice header fix parts to be filled in the slice header generation */
	uint32_t  addrSliceHeader;                /* LS 32 bits used by HW */
	/* size in bits of the Slice header fix parts */
	uint16_t  SliceHeaderSizeInBits;          /* LS 16 bits used by HW */
	/* Slice header offset where to insert FirstMbInSlice */
	uint16_t  SliceHeaderOffset0;             /* LS 16 bits used by HW */
	/* Slice header offset where to insert slice_qp_delta
	   If 0, no insertion */
	uint16_t  SliceHeaderOffset1;             /* LS 16 bits used by HW */
	/* Slice header offset where to insert NumMBSinSliceMinus1
	   If 0, no insertion */
	uint16_t  SliceHeaderOffset2;             /* LS 16 bits used by HW */
	/* Reserved */
	uint32_t  reserved5; /* not used (addr_motion_vector_map) */
	/* Reserved */
	uint32_t  reserved6; /* not used (addr_tunneling_buffer) */
	/* Reserved */
	uint16_t  reserved7; /* not used (tunneling_enabled) */
	/* Reserved */
	uint16_t  reserved8; /* not used (tunneling_mb_nb) */
	/* Enable "slice ready" interrupt after each slice */
	uint16_t  sliceSynchroEnable;             /* LS  1 bits used by HW */
	/* Maximum number of slice in a frame (0 is strictly forbidden) */
	uint16_t  maxSliceNumber;                 /* LS 16 bits used by HW */
	/* Four coefficients (C0C1C2C3) to convert from RGB to YUV for the Y component.
	   Y = C0*R + C1*G + C2*B + C3 (C0 is on byte 0) */
	uint32_t  rgb2YuvYCoeff;                  /* LS 32 bits used by HW */
	/* Four coefficients (C0C1C2C3) to convert from RGB to YUV for the U (Cb) component.
	   U = C0*R + C1*G + C2*B + C3 (C0 is on byte 0) */
	uint32_t  rgb2YuvUCoeff;                  /* LS 32 bits used by HW */
	/* Four coefficients (C0C1C2C3) to convert from RGB to YUV for the V (Cr) component.
	   V = C0*R + C1*G + C2*B + C3 (C0 is on byte 0) */
	uint32_t  rgb2YuvVCoeff;                  /* LS 32 bits used by HW */
	/* defines the maximum slice size in bytes (used when slice_size_type=2 or slice_size_type=3). */
	uint32_t  sliceByteSize;                  /* LS 29 bits used by HW */
	/* Maximum number of intra macroblock in a frame for the AIR algorithm */
	uint16_t  maxAirIntraMbNb;                /* LS 16 bits used by HW */
	/* Disable skipping in the Bitrate Controller */
	uint16_t  brcNoSkip;                      /* LS  1 bits used by HW */
	/* address of temporal context */
	uint32_t  addrTemporalContext;            /* LS 32 bits used by HW */
	/* address of static buffer for BRC parameters */
	uint32_t  addrBrcInOutParameter;          /* LS 32 bits used by HW */
} H264EncodeHardTaskDescriptor_t;

typedef struct H264EncodeHardParamOutSlice_s {
	uint32_t sliceSize;
	uint32_t sliceStartTime;
	uint32_t sliceEndTime;
	uint32_t sliceNum;
} H264EncodeHardParamOutSlice_t;

typedef struct H264EncodeHardParamOut_s {
	/* size in bytes of the current frame, including headers but not including stuffing bits */
	uint32_t  bitstreamSize;
	/* VP8 DCT coefficient partition size */
	uint32_t  dctBitstreamSize;
	/* number of stuffing bits inserted by the encoder */
	uint32_t  stuffingBits;
	/* removal time of current frame (nb of ticks 1/framerate) */
	uint32_t  removalTime;
	/* HVC task timestamp when FPC schedules its start */
	uint32_t  hvcStartTime;
	/* HVC task timestamp when FPC receives its end notification
	   So that, difference hvc_stop_time - hvc_start_time gives hvc task duration */
	uint32_t  hvcStopTime;
	/* Number of slice already output in DDR */
	uint32_t  sliceCount;
	/* Reserved */
	uint32_t  reserved0;
	/* Size in bits of the different slices */
	//H264EncodeHardParamOutSlice_t paramOutSlice[H264_ENCODE_MAX_SIZE_X / 16 * H264_ENCODE_MAX_SIZE_Y / 16];
	H264EncodeHardParamOutSlice_t paramOutSlice[0];
} H264EncodeHardParamOut_t;

typedef enum HVACmdType {
	HVA_H264_BASELINE_DEC    = 0x0,
	HVA_H264_HIGHPROFILE_DEC = 0x01,
	HVA_H264_ENC             = 0x02,
	//HVA_JPEG_ENC             = 0x03,
	/*SW synchro task (reserved in HW)*/
	HVA_SYNC_HVC             = 0x4,
	HVA_SYNC_HEC_HVC         = 0x5,
	//SYNC_HJE             = 0x6,
	/*  RESERVED             = 0x7,*/
	HVA_REMOVE_CLIENT        = 0x08,
	HVA_FREEZE_CLIENT        = 0x09,
	HVA_START_CLIENT         = 0x0A,
	HVA_FREEZE_ALL           = 0x0B,
	HVA_START_ALL            = 0x0C,
	HVA_REMOVE_ALL           = 0x0D
} HVACmdType_t;

static inline const char *StringifyHvaCmdType(HVACmdType_t aHvaCmdType)
{
	switch (aHvaCmdType) {
		ENTRY(HVA_H264_BASELINE_DEC);
		ENTRY(HVA_H264_HIGHPROFILE_DEC);
		ENTRY(HVA_H264_ENC);
		ENTRY(HVA_SYNC_HVC);
		ENTRY(HVA_SYNC_HEC_HVC);
		ENTRY(HVA_REMOVE_CLIENT);
		ENTRY(HVA_FREEZE_CLIENT);
		ENTRY(HVA_START_CLIENT);
		ENTRY(HVA_FREEZE_ALL);
		ENTRY(HVA_START_ALL);
		ENTRY(HVA_REMOVE_ALL);
	default: return "<unknown hva command type>";
	}
}

#ifdef __cplusplus
extern "C" {
#endif

int  H264HwEncodeProbe(struct device *dev);
void H264HwEncodeRemove(struct device *dev);

H264EncodeHardStatus_t H264HwEncodeInit(H264EncodeHardInitParams_t            *InitParams,
                                        H264EncodeHardHandle_t                *Handle);

H264EncodeHardStatus_t H264HwEncodeSetSequenceParams(H264EncodeHardHandle_t                 Handle,
                                                     H264EncodeHardSequenceParams_t        *SequenceParams);

H264EncodeHardStatus_t H264HwEncodeEncodeFrame(H264EncodeHardHandle_t Handle,
                                               H264EncodeHardFrameParams_t *FrameParams);

H264EncodeHardStatus_t H264HwEncodeTerminate(H264EncodeHardHandle_t Handle);

void H264HwEncodeSetRegistersConfig(void);

//only for test purpose
void H264HwEncodeInternalTest(void);


#ifdef __cplusplus
}
#endif

#endif // H_H264_ENCODE_HARD
