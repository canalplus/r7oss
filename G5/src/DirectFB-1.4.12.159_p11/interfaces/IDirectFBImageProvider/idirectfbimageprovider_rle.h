#ifndef __IDIRECTFBIMAGEPROVIDER_RLE_H__
#define __IDIRECTFBIMAGEPROVIDER_RLE_H__

#include <stdarg.h>

#include <direct/messages.h>


#if defined(RLE_PROVIDER_USE_MME)
/* HW Decode Requirements */
#include <mme.h>
#include <semaphore.h>

#include <RLEDecode_interface.h>
#endif /* RLE_PROVIDER_USE_MME */
#if defined(RLE_PROVIDER_HW)
#include <../gfxdrivers/stgfx2/bdisp_accel.h>
#endif

#define THIS_MODULE         "IDirectFBImageProvider_RLE"


#ifdef  USE_MEDIA_TRACE
#define TRACE(data) \
    D_INFO(THIS_MODULE": %4.2xh (%6.2d) -> "#data"\n", (int)(data), (int)(data))
#else
#define TRACE(data)
#endif


/* local types */
typedef        unsigned char    uchar;
typedef        unsigned int     uint;
typedef        unsigned long    ulong;
typedef        unsigned short   ushort;



/*
 * RLE Compression identifier (see BMP/RLE header - Offset:30 --- DWORD)
 */
typedef enum {
  RLEIC_NONE      = 0,   /* Implemented      as of version 1.0.0 */
  RLEIC_RLE8      = 1,   /* Implemented      as of version 1.0.0 */
  RLEIC_RLE4      = 2,   /* Unimplemented */
  RLEIC_BITFIELDS = 3,   /* Unimplemented */
  RLEIC_BD_RLE8   = 4,   /* Implemented      as of version 1.1.0 */
  RLEIC_H2_DVD    = 5,   /* Implemented      as of version 1.1.3-7 */
  RLEIC_H8_DVD    = 6    /* Implemented      as of version 1.1.3-7 */
} RLEImageCompression;



/*
 * Header info
 */
#define RLE_PREAMBLE_SIZE         (14)
#define RLE_BIHEADER_SIZE         (40)
#define RLE_HEADER_SIZE           (RLE_PREAMBLE_SIZE+RLE_BIHEADER_SIZE)
#define RLE_PALETTE_IMAGE_OFFSET  (0x36)

/*
 * Private service data structure
 */
typedef struct IDirectFBImageProvider_RLE_data_t
{
  struct _MMECommon common;

  uint                   magic;
  uint                   file_size;
  uint                   reserved;
  uint                   img_offset;
  uint                   payload_size;

  uint                   width;
  uint                   height;
  uint                   depth;
  uint                   num_planes;
  uint                   num_colors;
  uint                   imp_colors;
  RLEImageCompression    compression;
  uint                   h_resolution;
  uint                   v_resolution;

  bool                   indexed;
  bool                   topfirst;
  bool                   no_palette;
  bool                   compressed;

  int                    height_dib;
  int                    pitch;

  DFBColor              *palette;
  u8                    *image_indexes;

#if defined(RLE_PROVIDER_HW)
  void                          *stgfx2_handle;
  bdisp_aq_StretchBlit_RLE_func  rle_stretch;
  CoreSurface           *compressed_data;
#endif

#if defined(RLE_PROVIDER_USE_MME)
  /* Hardware Variables */
  RLEDecode_TransformReturnParams_t ReturnParams;
#endif /* RLE_PROVIDER_USE_MME */

#ifdef ALLOW_DUMP_RAW_TO_FILE
  int idx;
#endif /* DUMP_RAW_TO_FILE */
} IDirectFBImageProvider_RLE_data;



#if defined(RLE_PROVIDER_USE_MME)
/****************************************************************************/
static inline const char *
get_rle_error_string (RLEDecode_ErrorCodes_t e)
{
  static const char *RLEErrorType_strings[] = {
    "RLEDECODE_NO_ERROR",
    "RLEDECODE_MEMEORY_ALLOCATION_ERROR",
    "RLEDECODE_CRC_ERROR",
    "RLEDECODE_INVALID_STREAM",
    "RLEDECODE_INTERNAL_ERROR"
  };

  return (((unsigned int) e) < D_ARRAY_SIZE (RLEErrorType_strings))
         ? RLEErrorType_strings[e]
         : "* Unknown RleError code *";
}
#endif /* RLE_PROVIDER_USE_MME */



#endif /* __IDIRECTFBIMAGEPROVIDER_RLE_H__ */
