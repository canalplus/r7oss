
#ifndef RLEDECODERINTERFACE_H
#define RLEDECODERINTERFACE_H

#define RLEDECODER_MME_TRANSFORMER_NAME "RLEDECODER"

#if (RLEDECODER_MME_VERSION==10)
    #define RLEDECODER_MME_API_VERSION "1.0"
#else
    #define RLEDECODER_MME_API_VERSION "undefined"
#endif

#define RLEDECODE_COLOR_MASK_PALETTE    1
#define RLEDECODE_COLOR_MASK_COLOR       2
#define RLEDECODE_COLOR_MASK_ALPHA       4

typedef enum
{
	RLEDECODE_COLOR_TYPE_GRAY = 0,
	RLEDECODE_COLOR_TYPE_PALETTE =  (RLEDECODE_COLOR_MASK_COLOR | RLEDECODE_COLOR_MASK_PALETTE),
	RLEDECODE_COLOR_TYPE_RGB =  (RLEDECODE_COLOR_MASK_COLOR),
	RLEDECODE_COLOR_TYPE_RGB_ALPHA = (RLEDECODE_COLOR_MASK_COLOR | RLEDECODE_COLOR_MASK_ALPHA),
	RLEDECODE_COLOR_TYPE_GRAY_ALPHA =  (RLEDECODE_COLOR_MASK_ALPHA)
}RLEDecode_ColorType_t;

typedef enum
{
	RLEDECODE_INTERLACE_NONE = 0,
	RLEDECODE_INTERLACE_ADAM7 = 1
}RLEDecode_InterlaceType_t;

typedef enum
{
	RLEDECODE_COMPRESSION_TYPE_BASE = 0
}RLEDecode_CompressionType_t;


typedef enum
{
	RLEDECODE_FILTER_TYPE_BASE = 0
}RLEDecode_FilterMethod_t;

typedef enum
{
	RLEDECODE_OUTPUTFORMAT_ARGB = 0
}RLEDecode_OutputFormat_t;

typedef enum
{
	RLEDECODE_NO_ERROR,
	RLEDECODE_MEMEORY_ALLOCATION_ERROR,
	RLEDECODE_CRC_ERROR,
	RLEDECODE_INVALID_STREAM,
	RLEDECODE_INTERNAL_ERROR
} RLEDecode_ErrorCodes_t;


typedef struct 
{	/* Picture Width in pixels */
	unsigned int  PictureWidth;
	
	/* Picture Height in pixels */ 
	unsigned int  PictureHeight;
	
	/* Number of bits per sample or palette index */ 
	unsigned int  BitDepth;
 	
	/* Color type as encoded in the bitstream */
	RLEDecode_ColorType_t ColorType;	
	 
	/* Transmission order of image data */
	RLEDecode_InterlaceType_t InterlaceType;

	/* Only compression type 0 is supported */ 
	RLEDecode_CompressionType_t CompressionType;

	/* Only Filter method 0 is supported */
	RLEDecode_FilterMethod_t FilterMethod; 
	
	RLEDecode_ErrorCodes_t ErrorType;

} RLEDecode_GlobalTransformReturnParams_t; 


typedef struct 
{	/* Indicates seven step display in case if interlaced 	pictures */
  	unsigned int		ProgressiveDisplayFlag;
	RLEDecode_OutputFormat_t OutputFormat;
	
} RLEDecode_GlobalParams_t;

typedef struct 
{	/* Number of bytes written by the decoder into the output 	buffer*/
  	unsigned int	BytesWritten;
	/* Error encountered while decoding the picture */
	RLEDecode_ErrorCodes_t ErrorType;

#ifdef RLEDECODE_PROFILING_ENABLE
	unsigned int Cycles;
	unsigned int Bundles;
	unsigned int ICacheMiss;
	unsigned int DCacheMiss;
	unsigned int NopBundles;
#endif

}RLEDecode_TransformReturnParams_t;

typedef struct
{
	unsigned int caps_len;
}caps_len_t;

typedef struct
{
	unsigned char *caps;
}caps_t;

#endif /* RLEDECODERINTERFACE_H */

