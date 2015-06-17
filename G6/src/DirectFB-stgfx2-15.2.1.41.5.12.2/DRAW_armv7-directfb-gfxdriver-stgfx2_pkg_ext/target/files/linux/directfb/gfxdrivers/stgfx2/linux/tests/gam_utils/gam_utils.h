/*
 * gam_util.h
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * GAMMA Utilities (Loading and Saving GAM files for test purposes)
 */

#ifndef __STM_BLITTER_GAMMA_UTILS_H__
#define __STM_BLITTER_GAMMA_UTILS_H__

#include <linux/stm/blitter.h>

/* Local definitions for GAM header files */
/* -------------------------------------- */
#define BLITTER_ENDIANESS_LITTLE       0x0
#define BLITTER_ENDIANESS_BIG          0x1
#define BLITTER_GAMFILE_HEADER_SIZE    0x6
#define BLITTER_GAMFILE_CK_HEADER_SIZE 0x8
#define BLITTER_GAMFILE_SIGNATURE      0x444F
#define BLITTER_422FILE_SIGNATURE      0x422F
#define BLITTER_420FILE_SIGNATURE      0x420F
#define BLITTER_GAMFILE_PROPERTIES_LSB 0x01
#define BLITTER_GAMFILE_PROPERTIES     0x00
#  define BLITTER_GAMFILE_PROPERTIES_FULL_ALPHA    0xff /* only for alpha formats */
#  define BLITTER_GAMFILE_PROPERTIES_SQUARE_PIXEL  0x01 /* for macroblock */
#  define BLITTER_GAMFILE_PROPERTIES_PAL16_9       0x03 /* for macroblock */
#  define BLITTER_GAMFILE_PROPERTIES_PAL4_3        0x08 /* for macroblock */
#  define BLITTER_GAMFILE_PROPERTIES_NTSC16_9      0x06 /* for macroblock */
#  define BLITTER_GAMFILE_PROPERTIES_NTSC4_3       0x0c /* for macroblock */
#  define BLITTER_GAMFILE_PROPERTIES_BYTESWAP      0x10 /* for macroblock */
#define BLITTER_422FILE_PROPERTIES    (BLITTER_GAMFILE_PROPERTIES_SQUARE_PIXEL \
                                       | BLITTER_GAMFILE_PROPERTIES_BYTESWAP)
#define BLITTER_420FILE_PROPERTIES    (BLITTER_GAMFILE_PROPERTIES_PAL16_9 \
                                       | BLITTER_GAMFILE_PROPERTIES_BYTESWAP)
/* Known bitmap types */
enum gamfile_type {
  BLITTER_GAMFILE_UNKNOWN     = 0x0000,
  BLITTER_GAMFILE_RGB565      = 0x0080,
  BLITTER_GAMFILE_RGB888      = 0x0081,
  BLITTER_GAMFILE_xRGB8888    = 0x0082,
  BLITTER_GAMFILE_ARGB8565    = 0x0084,
  BLITTER_GAMFILE_ARGB8888    = 0x0085,
  BLITTER_GAMFILE_ARGB1555    = 0x0086,
  BLITTER_GAMFILE_ARGB4444    = 0x0087,
  BLITTER_GAMFILE_CLUT1       = 0x0088,
  BLITTER_GAMFILE_CLUT2       = 0x0089,
  BLITTER_GAMFILE_CLUT4       = 0x008a,
  BLITTER_GAMFILE_CLUT8       = 0x008b,
  BLITTER_GAMFILE_ACLUT44     = 0x008c,
  BLITTER_GAMFILE_ACLUT88     = 0x008d,
  BLITTER_GAMFILE_CLUT8_4444  = 0x008e,
  BLITTER_GAMFILE_YCBCR888    = 0x0090,
  BLITTER_GAMFILE_YCBCR101010 = 0x0091,
  BLITTER_422FILE_YCBCR422R   = 0x0092,
  BLITTER_420FILE_YCBCR420MB  = 0x0094,
  BLITTER_422FILE_YCBCR422MB  = 0x0095,
  BLITTER_GAMFILE_AYCBCR8888  = 0x0096,
  BLITTER_GAMFILE_A1          = 0x0098,
  BLITTER_GAMFILE_A8          = 0x0099,
  BLITTER_GAMFILE_I420        = 0x009e,
  BLITTER_420FILE_YCBCR420R2B = 0x00b0,
  BLITTER_422FILE_YCBCR422R2B = 0x00b1,
  BLITTER_GAMFILE_XY          = 0x00c8,
  BLITTER_GAMFILE_XYL         = 0x00c9,
  BLITTER_GAMFILE_XYC         = 0x00cA,
  BLITTER_GAMFILE_XYLC        = 0x00cb,
  BLITTER_GAMFILE_RGB101010   = 0x0101,
};

//#define LOAD_DESTINATION_GAMFILE

typedef struct bitmap_s
{
  stm_blitter_surface_format_t     format;
  stm_blitter_surface_colorspace_t colorspace;
  size_t                           buffer_size;
  stm_blitter_dimension_t          size;
  unsigned long                    pitch;
  stm_blitter_surface_address_t    buffer_address;
  unsigned char                   *memory;
} bitmap_t;

typedef struct GamPictureHeader_s
{
    unsigned short HeaderSize;
    unsigned short Signature;
    unsigned short Type; /* enum gamfile_type */
    unsigned short Properties;
    unsigned int   PictureWidth; /* For 4:2:0 R2B this is a OR between Pitch
                                    and Width */
    unsigned int   PictureHeight;
    unsigned int   LumaSize;
    unsigned int   ChromaSize;
} GamPictureHeader_t;

int load_gam_file(const char *FileName, bitmap_t *Bitmap_p);
int save_gam_file(const char *FileName, const bitmap_t *Bitmap_p);
void free_gam_file(bitmap_t *Bitmap_p);

#endif /* __STM_BLITTER_GAMMA_UTILS_H__ */
