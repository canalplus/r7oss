#ifndef __IDIRECTFBIMAGEPROVIDER_PNG_H__
#define __IDIRECTFBIMAGEPROVIDER_PNG_H__

#if defined(PNG_PROVIDER_USE_MME)
/* HW Decode Requirements */
#include <mme.h>
#include <semaphore.h>

#define PNGDEC_MME_VERSION 11
#define PNGDECODE_PROFILING_ENABLE
#include <PNGDecode_interface.h>
#endif /* PNG_PROVIDER_USE_MME */


/* private data struct of IDirectFBImageProvider_PNG */
typedef struct
{
  struct _MMECommon common;

  int               stage;
  int               rows;

  png_structp       png_ptr;
  png_infop         info_ptr;

  int               bpp;
  int               color_type;
  png_uint_32       color_key;
  bool              color_keyed;

  int               pitch;
  u32               palette[256];
  DFBColor          colors[256];

  DFBRectangle      rect;
  DFBRegion         clip;

  /* thread stuff */
  DIRenderFlags     flags;
  pthread_mutex_t   lock;
  pthread_cond_t    cond;
  DirectThread     *thread;
  IDirectFBSurface *destination;
  DFBResult         thread_res;

#if defined(PNG_PROVIDER_USE_MME)
  PNGDecode_TransformParams_t       TransformParams;
  PNGDecode_TransformReturnParams_t ReturnParams;

  PNGDecode_GlobalTransformReturnParams_t GlobalReturnParams;

  MME_Command_t SetGlobalCommand;

  sem_t global_event;
#endif /* PNG_PROVIDER_USE_MME */
} IDirectFBImageProvider_PNG_data;



#if defined(PNG_PROVIDER_USE_MME)
/****************************************************************************/
static inline const char *
get_png_error_string (PNGDecode_ErrorCodes_t e)
{
  static const char *PNGErrorType_strings[] = {
    "PNGDECODE_NO_ERROR",
    "PNGDECODE_MEMEORY_ALLOCATION_ERROR",
    "PNGDECODE_CRC_ERROR",
    "PNGDECODE_INVALID_STREAM",
    "PNGDECODE_INTERNAL_ERROR",
    "PNGDECODE_INVALID_ARGUMENT",
    "PNGDECODE_STREAM_ERROR"
  };

  return (((unsigned int) e) < D_ARRAY_SIZE (PNGErrorType_strings))
         ? PNGErrorType_strings[e]
         : "* Unknown PngError code *";
}
#endif /* PNG_PROVIDER_USE_MME */



#endif /* __IDIRECTFBIMAGEPROVIDER_PNG_H__ */
