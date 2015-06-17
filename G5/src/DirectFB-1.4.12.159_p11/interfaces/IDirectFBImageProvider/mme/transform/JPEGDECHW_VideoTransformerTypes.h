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

#include "mme.h"
#define OS21_TASKS

/*===============================================
               JPEG_DECODER
===============================================*/

#define JPEGHWDEC_MME_TRANSFORMER_NAME    "JPEG_DECODER_HW"

#define JPEGHWDEC_MME_VERSION 1.1
#define JPEGDECHW_NUMBER_OF_CEH_INTERVALS 32

/*
** JPEGDECHW_CompressedData_t :
**   Defines the address type for the JPEGDECHW compressed data
*/
typedef MME_UINT *JPEGDECHW_CompressedData_t;


/*
** JPEGDECHW_LumaAddress_t :
**   Defines the address type for the JPEGDECHW decoded Luma data’s
**   Bits 0 to 7 shall be set to 0 
*/
typedef MME_UINT *JPEGDECHW_LumaAddress_t;


/*
** JPEGDECHW_ChromaAddress_t :
**   Defines the address type for the JPEGDECHW decoded Chroma data’s 
**   Bits 0 to 7 shall be set to 0
*/
typedef MME_UINT *JPEGDECHW_ChromaAddress_t;


/*
** JPEGDECHW_DecodedBufferAddress_t :
**   Defines the addresses where the decoded picture/additional info related to the block 
**   structures will be stored 
*/
typedef struct _JPEGDECHWDECODEDBUFFERADDRESS_T_
{
	JPEGDECHW_LumaAddress_t Luma_p;
	JPEGDECHW_ChromaAddress_t Chroma_p;
	JPEGDECHW_LumaAddress_t LumaDecimated_p;
	JPEGDECHW_ChromaAddress_t ChromaDecimated_p;
}	JPEGDECHW_DecodedBufferAddress_t;


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
	JPEG_DECODER_NO_ERROR = 0,
    JPEG_DECODER_ERROR_TASK_TIMEOUT = 8
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
typedef enum
{
    JPEGDECHW_ADDITIONAL_FLAG_NONE = 0,
    JPEGDECHW_ADDITIONAL_FLAG_CEH  = 1 /* Request firmware to return values of the CEH registers */
}	JPEGDECHW_AdditionalFlags_t;


/*
** JPEGDECHW_VideoDecodeReturnParams_t :
** Identifies the parameters to be returned back to the driver by decoder.
*/
typedef struct _JPEGDECHWVIDEODECODERETURNPARAMS_T_
{
	/* profiling info */
	MME_UINT pm_cycles;
	MME_UINT pm_dmiss;
	MME_UINT pm_imiss;
	MME_UINT pm_bundles;
	MME_UINT pm_pft;
	JPEGDECHW_DecodingError_t ErrorCode;
#ifdef OS21_TASKS
    MME_UINT DecodeTimeInMicros;
#endif
#ifdef CEH_SUPPORT
    // CEHRegisters[] is an array where values of the
    // Contrast Enhancement Histogram (CEH) registers will be stored.
    // CEHRegisters[0] will correspond to register MBE_CEH_0_7, CEHRegisters[1] will
    // correspond to register MBE_CEH_8_15., CEHRegisters[2], correspond to register
    // MBE_CEH_16_23.
    // Note that elements of this array will be updated only if
    // VC9_TransformParam_t.AdditionalFlags will have the flag
    // VC9_ADDITIONAL_FLAG_CEH set. They will remain unchanged otherwise.
	MME_UINT CEHRegisters[JPEGDECHW_NUMBER_OF_CEH_INTERVALS];
#endif
}	JPEGDECHW_VideoDecodeReturnParams_t;


/*
** JPEGDECHW_VideoDecodeCapabilityParams_t :
** Transformer capability parameters.
*/
typedef struct _JPEGDECHW_VIDEODECODECAPABILITYPARAMS_T_
{
	MME_UINT api_version;	// Omega2 frame buffer size (luma+chroma)
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
	JPEGDECHW_MainAuxEnable_t		MainAuxEnable;
	JPEGDECHW_HorizontalDeciFactor_t HorizontalDecimationFactor;
	JPEGDECHW_VerticalDeciFactor_t	VerticalDecimationFactor;
	MME_UINT	xvalue0;				/* The X(0) coordinate for subregion decoding                                                 */
	MME_UINT	xvalue1;				/* The X(1) coordinate for subregion decoding                                                 */
	MME_UINT	yvalue0;				/* The Y(0) coordinate for subregion decoding                                                 */
	MME_UINT	yvalue1;				/* The Y(1) coordinate for subregion decoding                                                */
	JPEGDECHW_DecodingMode_t		DecodingMode;
    JPEGDECHW_AdditionalFlags_t     AdditionalFlags; /* Additonal Flags for CEH */
}	JPEGDECHW_VideoDecodeParams_t;

#endif /* _JPEGDECHW_VIDEOTRANSFORMERTYPES_H_ */

