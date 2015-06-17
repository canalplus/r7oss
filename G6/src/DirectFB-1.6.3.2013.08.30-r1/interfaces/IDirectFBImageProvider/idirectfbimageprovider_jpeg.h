#ifndef __IDIRECTFBIMAGEPROVIDER_JPEG_H__
#define __IDIRECTFBIMAGEPROVIDER_JPEG_H__

#if defined(JPEG_PROVIDER_USE_MME)
/* HW Decode Requirements */
#include <mme.h>
#include <semaphore.h>

#include <JPEG_VideoTransformerTypes.h>
//#include <JPEGDECHW_VideoTransformerTypes.h>
#endif /* JPEG_PROVIDER_USE_MME */

/* The following define controls prescaling
 *
 * PRE_SCALE_QUALITY_OPTIMISED : Better Quality
 * When defined pre-scale will scale to larger than the requested
 * size and use the blitter to reduce down.
 * - This option will take precedence if both options are defined.
 *
 * PRE_SCALE_SPACE_OPTIMISED : Faster and Less Memory
 * When defined pre-scale will scale below requested size, and
 * use the blitter to enlarge
 *
 * Removing both defines will safely disable the pre-scaler
 */
#define PRE_SCALE_QUALITY_OPTIMISED
//#define PRE_SCALE_SPEED_SPACE_OPTIMISED



/* private data struct of IDirectFBImageProvider_JPEG */
typedef struct
{
  /* hardware decode specifics */
  struct _MMECommon common;

  DFBRectangle      rect;
  DFBRegion         clip;

  /* thread stuff */
  DIRenderFlags     flags;
  pthread_mutex_t   lock;
  pthread_cond_t    cond;
  DirectThread     *thread;
  IDirectFBSurface *destination;
  DFBResult         thread_res;

#if defined(JPEG_PROVIDER_USE_MME)
  int  num_components;
  bool progressive_mode;

  JPEGDEC_TransformParams_t       OutputParams;
  JPEGDEC_TransformReturnParams_t ReturnParams;

//  JPEGDECHW_VideoDecodeParams_t       hw_decode_params;
//  JPEGDECHW_VideoDecodeReturnParams_t hw_return_params;
#endif /* JPEG_PROVIDER_USE_MME */
} IDirectFBImageProvider_JPEG_data;


#if defined(JPEG_PROVIDER_USE_MME)
static inline const char *
get_jpeg_error_string (unsigned int e)
{
  static const char *JpegErrorType_strings[] = {
    "Successfully Decoded", /* JPEG_NO_ERROR */
    "UNDEFINED_HUFF_TABLE",
    "UNSUPPORTED_MARKER",
    "UNABLE_ALLOCATE_MEMORY",
    "NON_SUPPORTED_SAMP_FACTORS",
    "BAD_PARAMETER",
    "DECODE_ERROR",
    "BAD_RESTART_MARKER",
    "UNSUPPORTED_COLORSPACE",
    "BAD_SOS_SPECTRAL",
    "BAD_SOS_SUCCESSIVE",
    "BAD_HEADER_LENGTH",
    "BAD_COUNT_VALUE",
    "BAD_DHT_MARKER",
    "BAD_INDEX_VALUE",
    "BAD_NUMBER_HUFFMAN_TABLES",
    "BAD_QUANT_TABLE_LENGHT",
    "BAD_NUMBER_QUANT_TABLES",
    "BAD_COMPONENT_COUNT",
    "DIVIDE_BY_ZERO_ERROR",
    "NOT_JPG_IMAGE",
    "UNSUPPORTED_ROTATION_ANGLE",
    "UNSUPPORTED_SCALING",
    "INSUFFICIENT_OUTPUTBUFFER_SIZE"
  };

  return ((e < D_ARRAY_SIZE (JpegErrorType_strings))
          ? JpegErrorType_strings[e]
          : "* Unknown JpegError code *");
}

static inline const char *
get_jpeghw_error_string (unsigned int e)
{
  static const char *JpegHWErrorType_strings[] = {
    "Successfully Decoded", /* JPEG_DECODER_NO_ERROR */
    "", "", "", "", "", "", "", "",
    "JPEG_DECODER_ERROR_TASK_TIMEOUT"
  };

  return ((e < D_ARRAY_SIZE (JpegHWErrorType_strings))
          ? JpegHWErrorType_strings[e]
          : "* Unknown JpegHWError code *");
}
#endif /* JPEG_PROVIDER_USE_MME */


#endif /* __IDIRECTFBIMAGEPROVIDER_JPEG_H__ */
