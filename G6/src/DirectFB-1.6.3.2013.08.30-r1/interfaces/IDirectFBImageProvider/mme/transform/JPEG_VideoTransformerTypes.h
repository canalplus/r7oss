/*
   ST Microelectronics MME JPEG transformer - user API

   (c) Copyright 2010       STMicroelectronics Ltd.

   All rights reserved.
*/

#ifndef __JPEG_VIDEOTRANSFORMERTYPES_H__
#define __JPEG_VIDEOTRANSFORMERTYPES_H__


#define JPEGDEC_MME_TRANSFORMER_NAME "JPEG_Transformer"

#if JPEGDEC_MME_VERSION == 0
#  define JPEGDEC_MME_VERSION 1
#endif

#if (JPEGDEC_MME_VERSION == 20)
#  define JPEGDEC_MME_API_VERSION "2.0"
#elif (JPEGDEC_MME_VERSION == 1)
#  define JPEGDEC_MME_API_VERSION "1.0"
#else
#  error only API versions 1 or 2.0 are supported
#endif

/* Common driver error constants */
#if (JPEGDEC_MME_VERSION == 1)
#  define JPEGDEC_ERROR_ID   0
#  define JPEGDEC_ERROR_BASE (JPEG_ERROR_ID << 16)
#else
#  include <jerror.h>
#endif




#if (JPEGDEC_MME_VERSION == 1)
/* Structure for GetTransformerCapability() */
typedef struct
{
  /* for further expansion... */
  unsigned int dummy;
} JPEGDEC_TransformerInfo_t;

typedef struct
{
  unsigned int caps_len;
}JPEGDEC_Capability_t;
#elif (JPEGDEC_MME_VERSION == 20)
enum JPEGDEC_TransformFlags {
  JDIF_ROTATE90            = 0x00000001,
  JDIF_ROTATE270           = 0x00000002,
  JDIF_FLIP_HORIZONTAL     = 0x00000004,
  JDIF_FLIP_VERTICAL       = 0x00000008,
  JDIF_ALL                 = 0x0000000f,
};
#endif



/* Structure for the Transformer Initialization */
typedef struct
{
  /* coardinates for subregion decoding */
  unsigned int xvalue0;
  unsigned int xvalue1;
  unsigned int yvalue0;
  unsigned int yvalue1;

  /* Output width and height of the decoded image. Used for IDCT select and
     resizing (if done in the core). */
  int outputWidth;
  int outputHeight;

#if (JPEGDEC_MME_VERSION == 1)
  /* TRUE if Rotatedegree is other than zero degree
     Rotation clockwise/anticlockwise is required */
  int ROTATEFLAG;
  /* degree of rotation clockwise/anti-clockwise || 90 | 180 | 270 */
  int Rotatedegree;
  /* Flag set TRUE when vertical flip of the picture is required */
  int VerticalFlip;
  /* Flag set TRUE when vertical flip of the picture is required */
  int HorizantalFlip;

  /* the pitch. Valid only if ROTATEFLAG & 0x80000000 */
  int Pitch;
#elif (JPEGDEC_MME_VERSION == 20)
  /* you should really set this, otherwise some default value is used, which
     doesn't necessarily coincides with what you assume. */
  int pitch;
#endif
} JPEGDEC_InitParams_t;





typedef enum
{
  /* planar 3 buffer YCbCr, 4:4:4, 4:2:2 or 4:2:0 (not implemented) */
  /* 24 bit YUV planar (3 planes, Y Cb Cr, each 8 bit) */
  JPEGDEC_OF_YCbCr4xxp,
  /* 16 bit YUV        ( 8 bit Y plane followed by one
                        16 bit half width CbCr [15:0] plane) */
  JPEGDEC_OF_NV16,
  /* 16 bit YUV        (4 byte/ 2 pixel, macropixel contains YCbYCr [31:0]) */
  JPEGDEC_OF_UYVY,
  /* 24 bit VYU 4:4:4  (3 byte, Cr 8@16, Y 8@8, Cb 8@0) */
  JPEGDEC_OF_YCbCr444r, /* DSPF_VYU in DirectFB */
  /* 24 bit RGB        (3 byte, red 8@16, green 8@8, blue 8@0) */
  JPEGDEC_OF_RGB,
} JPEGDEC_OutputFormat_t;


/* Structure for the databuffers passed with TRANSFORM commands */
typedef struct
{
  /* Steering of size/widht/height of output picture */
  JPEGDEC_InitParams_t outputSettings;
#if (JPEGDEC_MME_VERSION == 20)
  JPEGDEC_OutputFormat_t OutputFormat;
  enum JPEGDEC_TransformFlags flags;
#endif
} JPEGDEC_TransformParams_t;


#if (JPEGDEC_MME_VERSION == 1)
typedef enum
{
  JPEG_NO_ERROR,
  UNDEFINED_HUFF_TABLE,
  UNSUPPORTED_MARKER,
  UNABLE_ALLOCATE_MEMORY,
  NON_SUPPORTED_SAMP_FACTORS,
  BAD_PARAMETER,
  DECODE_ERROR,
  BAD_RESTART_MARKER,
  UNSUPPORTED_COLORSPACE,
  BAD_SOS_SPECTRAL,
  BAD_SOS_SUCCESSIVE,
  BAD_HEADER_LENGHT,
  BAD_COUNT_VALUE,
  BAD_DHT_MARKER,
  BAD_INDEX_VALUE,
  BAD_NUMBER_HUFFMAN_TABLES,
  BAD_QUANT_TABLE_LENGHT,
  BAD_NUMBER_QUANT_TABLES,
  BAD_COMPONENT_COUNT,
  DIVIDE_BY_ZERO_ERROR,
  NOT_JPG_IMAGE,
  UNSUPPORTED_ROTATION_ANGLE,
  UNSUPPORTED_SCALING,
  INSUFFICIENT_OUTPUTBUFFER_SIZE
}JPEGDEC_ErrorCodes_t;
#endif


/* Structure for return parameters of TRANSFORM commands */
typedef struct
{
  /* bytes written to the o/p buffer sent back to the host */
  unsigned int bytes_written;
  /* scaled image output height / width */
  int decodedImageHeight;
  int decodedImageWidth;

#if (JPEGDEC_MME_VERSION == 1)
  unsigned int Total_cycle;
  unsigned int DMiss_Cycle;
  unsigned int IMiss_Cycle;
  JPEGDEC_ErrorCodes_t ErrorType;
#elif (JPEGDEC_MME_VERSION == 20)

  int pitch;

  J_MESSAGE_CODE ErrorType;
#  if defined(JPEGDEC_PROFILING_ENABLE)
  unsigned int Cycles;
  unsigned int Bundles;
  unsigned int ICacheMiss;
  unsigned int DCacheMiss;
  unsigned int NopBundles;
#  endif

#endif
} JPEGDEC_TransformReturnParams_t;



#endif /* __JPEG_VIDEOTRANSFORMERTYPES_H__ */
