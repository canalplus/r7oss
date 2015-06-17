///
/// @file     : JPEGDECHW_VideoTransformerTypes.h
///
/// @brief    : JPEG Video Decoder specific types for MME
///
/// @par OWNER: 
///
/// @author   : Prabhat Awasthi
///
/// @par SCOPE:
///
/// @date     : 2008-08-01
///
/// &copy; 2006 ST Microelectronics. All Rights Reserved.
///

#ifndef _JPEGDECHW_VIDEOTRANSFORMERTYPES_H_
#define _JPEGDECHW_VIDEOTRANSFORMERTYPES_H_

#include "stddefs.h"

/*===============================================
               JPEG_DECODER
===============================================*/

#define JPEGHWDEC_MME_TRANSFORMER_NAME    "JPEG_DECODER_HW"

/*
JPEGHW_MME_VERSION:
Identifies the MME API version of the JPEGHW firmware.
If wants to build the firmware for old MME API version, change this string correspondingly.
*/
#ifndef JPEGHW_MME_VERSION
	#define JPEGHW_MME_VERSION	 17		/* Latest MME API version */ 		
#endif

#if (JPEGHW_MME_VERSION >= 17)
    #define JPEGHWDEC_MME_API_VERSION "1.7"
#elif (JPEGHW_MME_VERSION >= 16)
    #define JPEGHWDEC_MME_API_VERSION "1.6"
#elif (JPEGHW_MME_VERSION >= 15)
    #define JPEGHWDEC_MME_API_VERSION "1.5"
#elif (JPEGHW_MME_VERSION >= 14)
    #define JPEGHWDEC_MME_API_VERSION "1.4"
#elif (JPEGHW_MME_VERSION >= 13)
    #define JPEGHWDEC_MME_API_VERSION "1.3"
#elif (JPEGHW_MME_VERSION >= 12)
    #define JPEGHWDEC_MME_API_VERSION "1.2"
#elif (JPEGHW_MME_VERSION >= 11)
    #define JPEGHWDEC_MME_API_VERSION "1.1"
#else
    #define JPEGHWDEC_MME_API_VERSION "1.0"
#endif


#define JPEGDECHW_NUMBER_OF_CEH_INTERVALS 32

/*
** JPEGDECHW_CompressedData_t :
**   Defines the address type for the JPEGDECHW compressed data
*/
typedef U32 *JPEGDECHW_CompressedData_t;


/*
** JPEGDECHW_LumaAddress_t :
**   Defines the address type for the JPEGDECHW decoded Luma data’s
**   Bits 0 to 7 shall be set to 0 
*/
typedef U32 *JPEGDECHW_LumaAddress_t;


/*
** JPEGDECHW_ChromaAddress_t :
**   Defines the address type for the JPEGDECHW decoded Chroma data’s 
**   Bits 0 to 7 shall be set to 0
*/
typedef U32 *JPEGDECHW_ChromaAddress_t;


/*
** JPEGDECHW_DecodedBufferAddress_t :
**   Defines the addresses where the decoded picture/additional info related to the block 
**   structures will be stored 
*/
typedef struct _JPEGDECHWDECODEDBUFFERADDRESS_T_
{
	JPEGDECHW_LumaAddress_t Luma_p;
	JPEGDECHW_ChromaAddress_t Chroma_p;
#if (JPEGHW_MME_VERSION < 15) /* Not required For Reference reconstruction block*/
	JPEGDECHW_LumaAddress_t LumaDecimated_p;
	JPEGDECHW_ChromaAddress_t ChromaDecimated_p;
#endif
}	JPEGDECHW_DecodedBufferAddress_t;


#if (JPEGHW_MME_VERSION >= 15)
/*
** JPEGDECHW_DisplayBufferAddress_t :
** Defines the addresses (used by Display Reconstruction block) where the pictures to be displayed will be stored 
*/
typedef struct _JPEGDECHWDISPLAYBUFFERADDRESS_T_
{
	U32							StructSize;               /* Size of the structure in bytes */
    JPEGDECHW_LumaAddress_t		DisplayLuma_p;            /* address of the Luma buffer     */
	JPEGDECHW_ChromaAddress_t	DisplayChroma_p;          /* address of the Chroma buffer   */ 
	JPEGDECHW_LumaAddress_t		DisplayDecimatedLuma_p;   /* address of the decimated Luma buffer */
	JPEGDECHW_ChromaAddress_t	DisplayDecimatedChroma_p; /* address of the decimated Chroma buffer */ 
}	JPEGDECHW_DisplayBufferAddress_t;
#endif


#if (JPEGHW_MME_VERSION >= 15)
/*
** JPEGDECHW_RcnRefDispEnable_t :
** Used for enabling Main/Aux outputs for both Display & Reference reconstruction blocks 
*/
typedef enum _JPEGDECHW_RCNREFDISPENABLE_T_
{
	JPEGDECHW_DISP_AUX_EN               = 0x00000010,  /* Enable decimated (for display) reconstruction             */
	JPEGDECHW_DISP_MAIN_EN              = 0x00000020,  /* Enable main (for display) reconstruction                  */
	JPEGDECHW_DISP_AUX_MAIN_EN          = 0x00000030,  /* Enable both main & decimated (for display) reconstruction */
	JPEGDECHW_REF_MAIN_EN  				= 0x00000100,  /* Enable only reference output(ex. for Trick modes)*/
	JPEGDECHW_REF_MAIN_DISP_AUX_EN      = 0x00000110,  /* Enable reference output with decimated (for display) reconstruction  */
	JPEGDECHW_REF_MAIN_DISP_MAIN_EN     = 0x00000120,  /* Enable reference output with main (for display) reconstruction  */
	JPEGDECHW_REF_MAIN_DISP_MAIN_AUX_EN = 0x00000130   /* Enable reference output with main & decimated (for display) reconstruction */
}	JPEGDECHW_RcnRefDispEnable_t;

#else

/*
** JPEGDECHW_MainAuxEnable_t :
** Used for enabling Main/Aux outputs
*/
typedef enum _JPEGDECHW_MAINAUXENABLE_T_
{
	JPEGDECHW_AUXOUT_EN       = 0x00000010, /* enable decimated reconstruction */
	JPEGDECHW_MAINOUT_EN      = 0x00000020, /* enable main reconstruction */
	JPEGDECHW_AUX_MAIN_OUT_EN = 0x00000030 /* enable both main & decimated reconstruction */
}	JPEGDECHW_MainAuxEnable_t;

#endif


/*
** JPEGDECHW_HorizontalDeciFactor _t :
** Identifies the horizontal decimation factor
*/
typedef enum _JPEGDECHW_HORIZONTALDECIFACTOR_T_
{
	JPEGDECHW_HDEC_1 = 0x00000000, /* no resize */
    JPEGDECHW_HDEC_ADVANCED_2 = 0x00000101, /* Advanced H/2 resize using improved 8-tap filters */
    JPEGDECHW_HDEC_ADVANCED_4 = 0x00000102  /* Advanced H/4 resize using improved 8-tap filters */
}	JPEGDECHW_HorizontalDeciFactor_t;


/*
** JPEGDECHW_VerticalDeciFactor _t :
** Identifies the vertical decimation factor
*/
typedef enum _JPEGDECHW_VERTICALDECIFACTOR_T_
{
	JPEGDECHW_VDEC_1 = 0x00000000, /* no resize */
	JPEGDECHW_VDEC_ADVANCED_2_PROG = 0x00000204, /* V/2 , progressive resize */
	JPEGDECHW_VDEC_ADVANCED_2_INT = 0x000000208 /* V/2 , interlaced resize */
}	JPEGDECHW_VerticalDeciFactor_t;


/*
** JPEGDECHW_VideoDecodeInitParams_t :
** Identifies the Initialization parameters for the transformer.
*/
typedef struct _JPEGDECHW_VIDEODECODEINITPARAMS_T_
{
	JPEGDECHW_CompressedData_t CircularBufferBeginAddr_p;
	JPEGDECHW_CompressedData_t CircularBufferEndAddr_p;
}	JPEGDECHW_VideoDecodeInitParams_t;


/*
** JPEGDECHW_DecodingError_t :
** Status of the decoding process
*/
typedef enum
{
	/* The firmware has been sucessful */
#if (JPEGHW_MME_VERSION >= 13)
	JPEG_DECODER_NO_ERROR							= 0,
	JPEG_DECODER_UNDEFINED_HUFF_TABLE				= 1,
	JPEG_DECODER_UNSUPPORTED_MARKER					= 2,
	JPEG_DECODER_UNABLE_ALLOCATE_MEMORY				= 3,
	JPEG_DECODER_NON_SUPPORTED_SAMP_FACTORS			= 4,
	JPEG_DECODER_BAD_PARAMETER						= 5,
	JPEG_DECODER_DECODE_ERROR						= 6,
	JPEG_DECODER_BAD_RESTART_MARKER					= 7,
	JPEG_DECODER_UNSUPPORTED_COLORSPACE				= 8,
	JPEG_DECODER_BAD_SOS_SPECTRAL					= 9,
	JPEG_DECODER_BAD_SOS_SUCCESSIVE					= 10,
	JPEG_DECODER_BAD_HEADER_LENGHT					= 11,
	JPEG_DECODER_BAD_COUNT_VALUE					= 12,
	JPEG_DECODER_BAD_DHT_MARKER						= 13,
	JPEG_DECODER_BAD_INDEX_VALUE					= 14,
	JPEG_DECODER_BAD_NUMBER_HUFFMAN_TABLES			= 15,
	JPEG_DECODER_BAD_QUANT_TABLE_LENGHT				= 16,
	JPEG_DECODER_BAD_NUMBER_QUANT_TABLES			= 17,
	JPEG_DECODER_BAD_COMPONENT_COUNT				= 18,
	JPEG_DECODER_DIVIDE_BY_ZERO_ERROR				= 19,
	JPEG_DECODER_NOT_JPG_IMAGE						= 20,
	JPEG_DECODER_UNSUPPORTED_ROTATION_ANGLE			= 21,
	JPEG_DECODER_UNSUPPORTED_SCALING				= 22,
	JPEG_DECODER_INSUFFICIENT_OUTPUTBUFFER_SIZE		= 23,
	JPEG_DECODER_BAD_HWCFG_GP_VERSION_VALUE			= 24,
	JPEG_DECODER_BAD_VALUE_FROM_RED					= 25,
	JPEG_DECODER_BAD_SUBREGION_PARAMETERS			= 26,
	JPEG_DECODER_PROGRESSIVE_DECODE_NOT_SUPPORTED	= 27,
    JPEG_DECODER_ERROR_TASK_TIMEOUT					= 28
#else
	JPEG_DECODER_NO_ERROR = 0,
    JPEG_DECODER_ERROR_NOT_RECOVERED = 4,
    JPEG_DECODER_ERROR_TASK_TIMEOUT = 8
#endif
}	JPEGDECHW_DecodingError_t;


/*
** JPEGDECHW_DecodingMode_t :
** Identifies the decoding mode.
*/
typedef enum
{
    JPEGDECHW_NORMAL_DECODE = 0,
    /* Other values to be added later */
}	JPEGDECHW_DecodingMode_t;


/*
** JPEGDECHW_AdditionalFlags_t :
** Identifies the different flags that will be passed to JPEG firmware
*/
#if (JPEGHW_MME_VERSION >= 14)
typedef enum
{
    JPEGDECHW_ADDITIONAL_FLAG_NONE = 0,
    JPEGDECHW_ADDITIONAL_FLAG_CEH  = 1, /* Request firmware to return values of the CEH registers */
	JPEGDECHW_ADDITIONAL_FLAG_RASTER = 64, /* Output storage of Auxillary reconstruction in Raster format. */
	JPEGDECHW_ADDITIONAL_FLAG_420MB = 128 /* Output storage of Auxillary reconstruction in 420MB format. */
}	JPEGDECHW_AdditionalFlags_t;
#elif (JPEGHW_MME_VERSION >= 12)
typedef enum
{
    JPEGDECHW_ADDITIONAL_FLAG_NONE = 0,
    JPEGDECHW_ADDITIONAL_FLAG_CEH  = 1, /* Request firmware to return values of the CEH registers */
	JPEGDECHW_ADDITIONAL_FLAG_RASTER = 64 /* Output storage of Auxillary reconstruction in Raster format. */
}	JPEGDECHW_AdditionalFlags_t;
#else
typedef enum
{
    JPEGDECHW_ADDITIONAL_FLAG_NONE = 0,
    JPEGDECHW_ADDITIONAL_FLAG_CEH  = 1 /* Request firmware to return values of the CEH registers */
}	JPEGDECHW_AdditionalFlags_t;
#endif


/*
** JPEGDECHW_VideoDecodeReturnParams_t :
** Identifies the parameters to be returned back to the driver by decoder.
*/
typedef struct _JPEGDECHWVIDEODECODERETURNPARAMS_T_
{
	/* profiling info */
#if (JPEGHW_MME_VERSION >= 17)
    U32 DecodeTimeInMicros;
#endif
#if (JPEGHW_MME_VERSION <= 16) || defined (DUMP_PROFILE_INFO)
	U32 pm_cycles;
	U32 pm_dmiss;
	U32 pm_imiss;
	U32 pm_bundles;
	U32 pm_pft;
#endif
	JPEGDECHW_DecodingError_t ErrorCode;
    // CEHRegisters[] is an array where values of the
    // Contrast Enhancement Histogram (CEH) registers will be stored.
    // CEHRegisters[0] will correspond to register MBE_CEH_0_7, CEHRegisters[1] will
    // correspond to register MBE_CEH_8_15., CEHRegisters[2], correspond to register
    // MBE_CEH_16_23.
    // Note that elements of this array will be updated only if
    // VC9_TransformParam_t.AdditionalFlags will have the flag
    // VC9_ADDITIONAL_FLAG_CEH set. They will remain unchanged otherwise.
	U32 CEHRegisters[JPEGDECHW_NUMBER_OF_CEH_INTERVALS];
#ifdef JPEGHW_DEBUG
	S32 LastDecodedMB;
#endif	
}	JPEGDECHW_VideoDecodeReturnParams_t;


/*
** JPEGDECHW_VideoDecodeCapabilityParams_t :
** Transformer capability parameters.
*/
typedef struct _JPEGDECHW_VIDEODECODECAPABILITYPARAMS_T_
{
	U32 api_version;	// Omega2 frame buffer size (luma+chroma)
}	JPEGDECHW_VideoDecodeCapabilityParams_t;


/*
** JPEGDECHW_VideoDecodeParams_t :
** Parameters to be sent along with the Transform command
*/
typedef struct _JPEGDECHW_VIDEODECODEPARAMS_T_
{
	JPEGDECHW_CompressedData_t		PictureStartAddr_p;
	JPEGDECHW_CompressedData_t		PictureEndAddr_p;
	JPEGDECHW_DecodedBufferAddress_t DecodedBufferAddr;
#if (JPEGHW_MME_VERSION >= 15)
	JPEGDECHW_DisplayBufferAddress_t	DisplayBufferAddr;
#endif
#if (JPEGHW_MME_VERSION >= 15)
	JPEGDECHW_RcnRefDispEnable_t		MainAuxEnable;	/* enable Main and/or Aux outputs */ 
#else
	JPEGDECHW_MainAuxEnable_t			MainAuxEnable;  /* enable Main and/or Aux outputs */ 
#endif
	JPEGDECHW_HorizontalDeciFactor_t HorizontalDecimationFactor;
	JPEGDECHW_VerticalDeciFactor_t	VerticalDecimationFactor;
	U32	xvalue0;				/* The X(0) coordinate for subregion decoding                                                 */
	U32	xvalue1;				/* The X(1) coordinate for subregion decoding                                                 */
	U32	yvalue0;				/* The Y(0) coordinate for subregion decoding                                                 */
	U32	yvalue1;				/* The Y(1) coordinate for subregion decoding                                                */
	JPEGDECHW_DecodingMode_t		DecodingMode;
#if (JPEGHW_MME_VERSION >= 12)
    U32 AdditionalFlags;		/* Additonal Flags */
#else
    JPEGDECHW_AdditionalFlags_t     AdditionalFlags; /* Additonal Flags */
#endif
#if (JPEGHW_MME_VERSION >= 16)
	U32									FieldFlag; /* Determines Frame/Field Scan */
#endif
#ifdef JPEG_PIC_SEQ_DEBUG_INFO
	U8 *DebugBufferPtr;
#endif
}	JPEGDECHW_VideoDecodeParams_t;


#ifdef JPEG_PIC_SEQ_DEBUG_INFO
/*
** JPEG_DebugPicture_t :
** Parameters to be sent along with the Transform command
*/
typedef struct _JPEG_DEBUGPICTURE_T_
{
	U32 PicStdConfig;
	U32 PictureWidthWord;
	U32 PictureHeightWord;
	U32 PicType_IPF;
	U32 PictureWidthWord_IPF;
	U32 PictureHeightWord_IPF;
}	JPEG_DebugPicture_t;
#endif

#ifdef JPEG_MB_DEBUG_INFO
/*
** JPEG_DebugMb_t :
** Parameters to be sent along with the Transform command
*/
typedef struct _JPEG_DEBUGMB_T_
{
	U32 SMB_WORD_0;
	U32 SMB_WORD_1;
	U32 MbType;
	U32 LmbWord2;
	U32 Data2PipeWord[192];
	U32 BitBuf_LSB[6];
	U32 BitBuf_MSB[6];
}	JPEG_DebugMb_t;
#endif

#endif /* _JPEGDECHW_VIDEOTRANSFORMERTYPES_H_ */

