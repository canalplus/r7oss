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

#ifndef _VP8_VIDEOTRANSFORMERTYPES_H_
#define _VP8_VIDEOTRANSFORMERTYPES_H_

#define VP8DECHW_MME_TRANSFORMER_NAME "VP8DEC_TRANSFORMER0"

#ifndef VP8DEC_MME_VERSION
#define VP8DEC_MME_VERSION	 11	  /* Latest MME API version */
#endif

#if (VP8DEC_MME_VERSION >= 11)
#define VP8DEC_MME_API_VERSION "1.1"
#else
#define VP8DEC_MME_API_VERSION "1.0"
#endif

#ifndef DEFINED_S8
#define DEFINED_S8
typedef signed char     S8;
#endif

#ifndef DEFINED_S16
#define DEFINED_S16
typedef signed short    S16;
#endif

#ifndef DEFINED_S32
#define DEFINED_S32
typedef signed int      S32;
#endif

#ifndef DEFINED_U8
#define DEFINED_U8
typedef unsigned char   U8;
#endif

#ifndef DEFINED_U32
#define DEFINED_U32
typedef unsigned int    U32;
#endif

#ifndef DEFINED_U16
#define DEFINED_U16
typedef unsigned short  U16;
#endif

#ifndef DEFINED_BOOL
#define DEFINED_BOOL
typedef int             BOOL;
#endif

/* VP8_CompressedData_t :
   Defines the address type for the VP8 compressed data
*/
typedef U8 *VP8_CompressedData_t;

typedef enum {
	VP8_NO_ERROR,				/* No Error condition detected -- Sucessfull decoding */
	VP8_DECODER_ERROR_TASK_TIMEOUT /* Decoder task timeout has occurred */
	/* More Error Codes to be added later */
} VP8_ERROR_CODES;

typedef struct {
	U32   CodedWidth;
	U32   CodedHeight;
} VP8_InitTransformerParam_t;

typedef struct {
	U32                          PictureStartOffset ;
	U32                          PictureStopOffset ;
	U32                          KeyFrame;
	U32                          ShowFrame;
	U32                          vpVersion;
	U32                          offsetToDctParts;/*partSize*/
	U32                          encoded_width;
	U32                          encoded_height;
	U32                          decode_width;
	U32                          decode_height;
	U32                          horizontal_scale;
	U32                          vertical_scale;
	U32                          colour_space;
	U32                          clamping;
	U32                          filter_type;
	U32                          loop_filter_level;
	U32                          sharpness_level;
} VP8_ParamPicture_t;


typedef struct {
	U32                        StructSize;            /* Size of the structure in bytes */
	VP8_ParamPicture_t         PictureParameters;      /* Picture parameters */
	U32                        CodedData;
	U32                        CodedData_Cp;
	U32                        CodedDataSize;
	U32                        FrameBuffer_Size;
	U32                        CurrentFB_Luma_p;
	U32                        CurrentFB_Chroma_p;
	U32                        LastRefFB_Luma_p;
	U32                        LastRefFB_Chroma_p;
	U32                        GoldenFB_Luma_p;
	U32                        GoldenFB_Chroma_p;
	U32                        AlternateFB_Luma_p;
	U32                        AlternateFB_Chroma_p;
	U32                        AdditionalFlags;        /* Additonal Flags for future uses */
} VP8_TransformParam_t;


typedef struct {
	VP8_ERROR_CODES Errortype;
	/* profiling info */
	U32 pm_cycles;
	U32 pm_dmiss;
	U32 pm_imiss;
	U32 pm_bundles;
	U32 pm_nop_bundles;
} VP8_ReturnParams_t;

typedef struct {
	U32 display_buffer_size;	/* Omega2 frame buffer size (luma+chroma) */
	U32 packed_buffer_size;	/* packed-data buffer size (for VIDA) */
	U32 packed_buffer_total;	/* number of packed buffers (for VIDA) */
	U32 IsHantroG1Here;
} VP8_CapabilityParams_t;

#endif /* _VP8_VIDEOTRANSFORMERTYPES_H_ */
