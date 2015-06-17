#ifndef PNGDECODERINTERFACE_H
#define PNGDECODERINTERFACE_H

#define PNGDECODER_MME_TRANSFORMER_NAME "PNGDECODER"

#if (PNGDEC_MME_VERSION == 11)
    #define PNGDECODER_MME_API_VERSION "1.1"
#elif (PNGDEC_MME_VERSION == 10)
    #define PNGDECODER_MME_API_VERSION "1.0"
#else
    #define PNGDECODER_MME_API_VERSION "undefined"
#endif

#define PNGDECODE_COLOR_MASK_PALETTE    1
#define PNGDECODE_COLOR_MASK_COLOR       2
#define PNGDECODE_COLOR_MASK_ALPHA       4

#define PNGDECODE_MAX_PALETTE  256

typedef enum
{
	PNGDECODE_COLOR_TYPE_GRAY = 0 /* PNG_COLOR_TYPE_GRAY */,
	PNGDECODE_COLOR_TYPE_PALETTE = PNGDECODE_COLOR_MASK_COLOR | PNGDECODE_COLOR_MASK_PALETTE /* PNG_COLOR_TYPE_PALETTE */,
	PNGDECODE_COLOR_TYPE_RGB =  PNGDECODE_COLOR_MASK_COLOR /* PNG_COLOR_TYPE_RGB */,
	PNGDECODE_COLOR_TYPE_RGB_ALPHA = PNGDECODE_COLOR_MASK_COLOR | PNGDECODE_COLOR_MASK_ALPHA /* PNG_COLOR_TYPE_RGB_ALPHA */,
	PNGDECODE_COLOR_TYPE_GRAY_ALPHA = PNGDECODE_COLOR_MASK_ALPHA /* PNG_COLOR_TYPE_GRAY_ALPHA*/ 
}PNGDecode_ColorType_t;

typedef enum
{
	PNGDECODE_INTERLACE_NONE = 0 /* PNG_INTERLACE_NONE */,
	PNGDECODE_INTERLACE_ADAM7 = 1 /* PNG_INTERLACE_ADAM7 */
}PNGDecode_InterlaceType_t;

typedef enum
{
	PNGDECODE_COMPRESSION_TYPE_BASE = 0 /* PNG_COMPRESSION_TYPE_BASE */
}PNGDecode_CompressionType_t;


typedef enum
{
	PNGDECODE_FILTER_TYPE_BASE = 0 /* PNG_FILTER_TYPE_BASE */
}PNGDecode_FilterMethod_t;

/* for an MME_TRANSFORM command the ones marked with a star or plus denote
   possibly corrupt data when seen together with MME_INVALID_ARGUMENT. A plus
   is given for PNGDEC_MME_VERSION == 10 and a star for
   PNGDEC_MME_VERSION == 11. In theory, PNGDECODE_MEMORY_ALLOCATION_ERROR
   could denote corrupt data, too, but it's unlikely any data was decoded
   at all, thus it doesn't make sense to display the result, either try
   again or use libpng on the host! For version 10,
   PNGDECODE_INTERNAL_ERROR is returned in more occasions than just data
   corruption. */
typedef enum
{
	PNGDECODE_NO_ERROR,
	PNGDECODE_MEMORY_ALLOCATION_ERROR,
	PNGDECODE_CRC_ERROR               /* +* */,
	PNGDECODE_INVALID_STREAM          /* +* */,
	PNGDECODE_INTERNAL_ERROR          /* + */
#if (PNGDEC_MME_VERSION > 10)
	,PNGDECODE_INVALID_ARGUMENT
    ,PNGDECODE_STREAM_ERROR           /* * */
#endif
} PNGDecode_ErrorCodes_t;


typedef struct 
{
    /* Picture Width in pixels */
	unsigned int  PictureWidth;
	
	/* Picture Height in pixels */ 
	unsigned int  PictureHeight;
	
    /* bitstream information: */
	/* Number of bits per sample or palette index */ 
	unsigned int  BitDepth;
	/* Color type as encoded in the bitstream */
	PNGDecode_ColorType_t ColorType;	
	/* Transmission order of image data */
	PNGDecode_InterlaceType_t InterlaceType;
	/* Only compression type 0 is supported */ 
	PNGDecode_CompressionType_t CompressionType;
	/* Only Filter method 0 is supported */
	PNGDecode_FilterMethod_t FilterMethod; 
	
	PNGDecode_ErrorCodes_t ErrorType;

#if (PNGDEC_MME_VERSION > 10)
    /* transformer return params: */
    /* Color type as returned by the transformer:
       - PNGDECODE_COLOR_TYPE_PALETTE (LUT8)
       - PNGDECODE_COLOR_TYPE_RGB (RGB24)
       - PNGDECODE_COLOR_TYPE_RGB_ALPHA (ARGB32) */
    PNGDecode_ColorType_t ColorFormatOutput;

    int HaveColorKey;
    unsigned int ColorKey; /* 32 bit xRGB */

    unsigned int pitch;

    unsigned char palette[4 * PNGDECODE_MAX_PALETTE]; /* ARGB ARGB ARGB */
#endif
} PNGDecode_GlobalTransformReturnParams_t;


#if (PNGDEC_MME_VERSION == 10)

typedef enum
{
    /* always ARGB */
    PNGDECODE_OUTPUTFORMAT_ARGB = 0
} PNGDecode_OutputFormat_t;

typedef struct
{
    /* indicates seven step display in case of interlaced pictures */
    unsigned int             ProgressiveDisplayFlag;
    PNGDecode_OutputFormat_t OutputFormat;
} PNGDecode_GlobalParams_t;

typedef struct
{
    unsigned int caps_len;
} caps_len_t;

typedef struct
{
    unsigned char *caps;
} caps_t;

#elif (PNGDEC_MME_VERSION > 10)

typedef enum
{
    /* PNGDecode_OutputFormat_t, valid in PNGDecode_GlobalParams_t and
       PNGDecode_InitTransformerParams_t */
    PNGDECODE_PARAM_FORMAT      = 0x00000001,
    /* pitch, only valid in PNGDecode_TransformParams_t */
    PNGDECODE_PARAM_PITCH       = 0x00000002
} PNGDecode_Params_Flags_t;

typedef enum
{
    /* expand everything to RGB24 (if the image has no alpha components, else
       to ARGB32) */
    PNGDECODE_OF_EXPAND,
    /* keep indexed images:
       - LUT8 for all (non alpha) greyscale (any bitdepth) and 8bit palette
         images
       - RGB24 for all other non alpha images
       - ARGB32 for alpha images */
    PNGDECODE_OF_COMPRESSED,
    /* always expand to ARGB32 */
    PNGDECODE_OF_EXPAND_FORCE_ALPHA
} PNGDecode_OutputFormat_t;

typedef struct
{
    PNGDecode_Params_Flags_t flags;
    /* if not specified, PNGDECODE_OF_EXPAND is assumed */
    PNGDecode_OutputFormat_t format;
    /* if not specified, the pitch will be as returned in
       PNGDecode_GlobalTransformReturnParams_t */
    int pitch;
} PNGDecode_InitTransformerParams_t,
  PNGDecode_TransformParams_t,
  PNGDecode_GlobalParams_t;

#endif


typedef struct
{	/* Number of bytes written by the decoder into the output buffer*/
	unsigned int BytesWritten;
	/* Error encountered while decoding the picture */
	PNGDecode_ErrorCodes_t ErrorType;

#if defined(PNGDECODE_PROFILING_ENABLE)
	unsigned int Cycles;
	unsigned int Bundles;
	unsigned int ICacheMiss;
	unsigned int DCacheMiss;
	unsigned int NopBundles;
#endif
} PNGDecode_TransformReturnParams_t;


#endif /* PNGDECODERINTERFACE_H */
