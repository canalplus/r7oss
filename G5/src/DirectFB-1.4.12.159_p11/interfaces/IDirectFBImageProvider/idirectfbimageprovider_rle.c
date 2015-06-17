/*
 * Copyright (C) 2006-2009 ST-Microelectronics R&D
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <gfx/convert.h>

#include <misc/gfx_util.h>
#include <misc/util.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/interface.h>

#include <display/idirectfbsurface.h>
#include <media/idirectfbimageprovider.h>

#if defined(RLE_PROVIDER_HW)
#include <sys/ioctl.h>
#include <fbdev/fbdev.h>
#include <linux/stmfb.h>
#endif

/*******************************************************************************
 *
 *                      TYPES DEFINITION SECTION
 *
 *******************************************************************************/
//#define PERFORMANCE_MONITORING
#define ALLOW_FORCE_SOFTWARE
#define ALLOW_DUMP_RAW_TO_FILE

#define MAX_STREAMING_COMMANDS          1
#define MME_STREAMING_BUFFERSIZE        (1024 * 1024) /* Maximum size */

D_DEBUG_DOMAIN (HWRLE,      "RLE",      "STM hardware-/MME- based RLE decoder");
D_DEBUG_DOMAIN (HWRLE_MME,  "RLE/MME",  "STM MME-based RLE decoder (MME)");
D_DEBUG_DOMAIN (HWRLE_SEMA, "RLE/SEMA", "STM MME-based RLE decoder (semaphores");
D_DEBUG_DOMAIN (HWRLE_TIME, "RLE/Time", "STM MME-based RLE decoder (timing");
#define MME_DEBUG_DOMAIN  HWRLE_MME
#define SEMA_DEBUG_DOMAIN HWRLE_SEMA
#define MME_TEXT_DOMAIN "RLE"

#include "debug_helper.h"
#define fetch_data   buffer_to_mme_copy
struct _MMECommon;
static void
_imageprovider_update_transform_params (struct _MMECommon           * const mme,
                                        void                        * const params,
                                        const CoreSurfaceBufferLock * const lock)
{
}
#include "mme_helper.h"
#include "sema_helper.h"
#include "idirectfbimageprovider_rle.h"


/*******************************************************************************
 *
 *                      INTERFACE DEFINITION SECTION
 *
 *******************************************************************************/

static DFBResult Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult Construct( IDirectFBImageProvider *thiz,
                            ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, RLE )


/*******************************************************************************
 *
 *                      PRIVATE STATIC SECTION     (Little-Endian specific code)
 *
 *******************************************************************************/

static u8
rle_input_byte (const u8 ** const pptr,
                const u8  * const end)
{
  return (*pptr < end) ? *(*pptr)++ : 0;
}

static void
rle_output_byte (u8       ** const pptr,
                 const u8  * const end,
                 u8         byte)
{
  if (*pptr < end)
    *(*pptr)++ = byte;
}

static u8 *
rle_set_position (u8   * const ptr,
                  uint  x,
                  uint  y,
                  uint  w,
                  uint  h,
                  uint  top)
{
  return &ptr[( x + (top ? y : (h-1-y)) * w)];
}



static u16
read_le_16 (const u8 ** const pptr)
{
  const u8 * const ptr = *pptr;
  *pptr += 2;
  return (ptr[1] << 8) | ptr[0];
}

static u32
read_le_32 (const u8 ** const pptr)
{
  const u8 * const ptr = *pptr;
  *pptr += 4;
  return (ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | ptr[0];
}

static void
__attribute__((unused))
write_le_16 (u8  ** const pptr,
             u16   data)
{
  u8 * const ptr = *pptr;
  *pptr += 2;
  ptr[0] = (data >> 0) & 0xff;
  ptr[1] = (data >> 8) & 0xff;
}

static void
__attribute__((unused))
write_le_32 (u8  ** const pptr,
             u32   data)
{
  u8 * const ptr = *pptr;
  *pptr += 4;
  ptr[0] = (data >>  0) & 0xff;
  ptr[1] = (data >>  8) & 0xff;
  ptr[2] = (data >> 16) & 0xff;
  ptr[3] = (data >> 24) & 0xff;
}

/*******************************************************************************
 *
 *                      PRIVATE STATIC SECTION      (RLE/RGB specific code)
 *
 *******************************************************************************/
static DFBResult
rle_decode_header (IDirectFBImageProvider_RLE_data *data)
{
#   define READ_LE_16(ptr,data)    {data=read_le_16(&ptr);TRACE(data);}
#   define READ_LE_32(ptr,data)    {data=read_le_32(&ptr);TRACE(data);}
#   define WRITE_LE_16(ptr,data)   write_le_16    (&ptr,(u16)data)
#   define WRITE_LE_32(ptr,data)   write_le_32    (&ptr,(u32)data)
#   define IS_MAGIC(ptr,l,h)       ((ptr[0]==l) &&  (ptr[1]==h))
#   define SET_MAGIC(ptr,l,h)      {ptr[0]=(u8)l; ptr[1]=(u8)h;}

  DFBResult  ret;
  u8         preamble[RLE_PREAMBLE_SIZE];
  u8         header[RLE_BIHEADER_SIZE];
  const u8  *hptr;
  u32        bihsize;

  /* Preamble section */
  memset (preamble, 0, RLE_PREAMBLE_SIZE);
  ret = buffer_to_ptr_copy (data->common.base.buffer, preamble,
                            RLE_PREAMBLE_SIZE);
  if (ret)
    return ret;
  hptr = preamble;

  /* Check first 2 bytes: Magic */
  if (!IS_MAGIC (hptr, 'R', 'L') && !IS_MAGIC (hptr, 'B', 'M'))
    {
      D_ERROR ("Invalid magic (%c%c)!\n", hptr[0], hptr[1]);
      return DFB_UNSUPPORTED;
    }

  /* Offset:00 --- 2 bytes: Magic */
  READ_LE_16 (hptr, data->magic);

  /* Offset:02 --- 4 bytes: FileSize */
  READ_LE_32 (hptr, data->file_size);

  /* Offset:06 --- 4 bytes: Reserved */
  READ_LE_32 (hptr, data->reserved);

  /* Offset:10 --- 4 bytes: DataOffset */
  READ_LE_32 (hptr, data->img_offset);
  if (data->img_offset < 54)
    {
      D_ERROR ("Invalid offset %08x!\n", data->img_offset);
      return DFB_UNSUPPORTED;
    }


  /* Header section */
  memset (header, 0, RLE_BIHEADER_SIZE);
  ret = buffer_to_ptr_copy (data->common.base.buffer, header,
                            RLE_BIHEADER_SIZE);
  if (ret)
    return ret;
  hptr = header;

  /* Offset:14 --- 4 bytes: HeaderSize */
  READ_LE_32 (hptr, bihsize);
  if (bihsize < 40)
    {
      D_ERROR ("Invalid image header size %d!\n", bihsize);
      return DFB_UNSUPPORTED;
    }

  /* Offset:18 --- 4 bytes: Width */
  READ_LE_32 (hptr, data->width);
  if (data->width < 1 || data->width > 0xffff)
    {
      D_ERROR ("Invalid width %d!\n", data->width);
      return DFB_UNSUPPORTED;
    }

  /* Offset:22 --- 4 bytes: Height */
  READ_LE_32 (hptr, data->height_dib);
  /* Deal with top-first mode DIB (negative height means just that ...) */
  data->topfirst = data->height_dib < 0;
  data->height   = data->topfirst ? -data->height_dib : data->height_dib;
  if (data->height < 1 || data->height > 0xffff)
    {
      D_ERROR ("Invalid height %d!\n", data->height);
      return DFB_UNSUPPORTED;
    }

  /* Offset:26 --- 2 bytes: Planes */
  READ_LE_16 (hptr, data->num_planes);
  if (data->num_planes != 1)
    {
      D_ERROR ("Unsupported number of planes %d!\n", data->num_planes);
      return DFB_UNSUPPORTED;
    }

  /* Offset:28 --- 2 bytes: Depth */
  READ_LE_16 (hptr, data->depth);
  switch (data->depth)
    {
    case 1:
    case 4:
    case 8:
      data->indexed = true;
      /* fall through */
    case 16:
    case 24:
    case 32:
      break;

    default:
      D_ERROR ("Unsupported depth %d!\n", data->depth);
      return DFB_UNSUPPORTED;
    }

  /* Offset:30 --- 4 bytes: Compression */
  READ_LE_32 (hptr, data->compression);
  switch (data->compression)
    {
    case RLEIC_NONE:
      data->compressed = false;
      break;

    case RLEIC_BD_RLE8:
      if (data->depth != 8)
        {
          D_ERROR ("Unsupported compression %d with depth %d!\n",
                   data->compression, data->depth);
          return DFB_UNSUPPORTED;
        }
      data->compressed = true;
      break;

    case RLEIC_RLE8:
      if (data->depth != 8)
        {
          D_ERROR ("Unsupported compression %d with depth %d!\n",
                   data->compression, data->depth);
          return DFB_UNSUPPORTED;
        }
      data->compressed = true;
      break;

    case RLEIC_H2_DVD:
      if (data->depth != 8)
        {
          D_ERROR ("Unsupported compression %d with depth %d!\n",
                   data->compression, data->depth);
          return DFB_UNSUPPORTED;
        }
      data->compressed = true;
      D_INFO ("Detected H2DVD File\n");
      break;

    case RLEIC_H8_DVD:
      if (data->depth != 8)
        {
          D_ERROR ("Unsupported compression %d with depth %d!\n",
                   data->compression, data->depth);
          return DFB_UNSUPPORTED;
        }
      data->compressed = true;
      D_INFO ("Detected H8-DVD File\n");
      break;

    case RLEIC_RLE4:        /* Unimplemented as of version 1.1.0    */
      if (data->depth != 4)
        {
          D_ERROR ("Unsupported compression %d with depth %d!\n",
                   data->compression, data->depth);
          return DFB_UNSUPPORTED;
        }
      data->compressed = true;
      //break;

    case RLEIC_BITFIELDS:   /* Unimplemented (no plans for support) */
      data->compressed = false;

    default:
      D_ERROR ("Unsupported compression - %d!\n", data->compression);
      return DFB_UNSUPPORTED;
    }

  /* Offset:34 --- 4 bytes: CompressedSize */
  READ_LE_32 (hptr, data->payload_size);

  /* Offset:38 --- 4 bytes: HorizontalResolution */
  READ_LE_32 (hptr, data->h_resolution);

  /* Offset:42 --- 4 bytes: VerticalResolution */
  READ_LE_32 (hptr, data->v_resolution);

  /* Offset:46 --- 4 bytes: UsedColors */
  READ_LE_32 (hptr, data->num_colors);

  /* Offset:50 --- 4 bytes: ImportantColors (don't care ...) */
  READ_LE_32 (hptr, data->imp_colors);

  /* Offset:54 --- bihsize-40 bytes: Skip remaining bytes */
  if (bihsize > 40)
    {
      bihsize -= 40;
      while (bihsize--)
        {
          u8 b;
          ret = buffer_to_ptr_copy (data->common.base.buffer, &b, 1);
          if (ret)
            return ret;
        }
    }


  /* Fixup source data pitch - used for any source raster-scan image */
  data->pitch        = (((data->width*data->depth + 7) >> 3) + 3) & ~3;

  /* Special mode colormap-less indexed source picture */
  data->no_palette   = data->indexed && (data->img_offset == RLE_PALETTE_IMAGE_OFFSET);

  /* Setup number of colors anyway ... */
  if (data->num_colors == 0)
    data->num_colors = 1 << data->depth;


  /* Offset:16+bihsize     --- 4 x num_colors bytes: Palette management*/
  if (data->indexed && !data->no_palette)
    {
      uint      pal_size = data->num_colors;
      void     *src;
      DFBColor  c;
      int       i;

      data->palette = src = D_MALLOC (pal_size * 4);
      if (!data->palette)
        return D_OOM ();

      ret = buffer_to_ptr_copy (data->common.base.buffer, src,
                                data->num_colors * 4);
      if (ret)
        return ret;

      for (i = 0; i < data->num_colors; ++i)
        {
          c.a = 0xff;
          c.r = ((u8 *) src)[i * 4 + 2];
          c.g = ((u8 *) src)[i * 4 + 1];
          c.b = ((u8 *) src)[i * 4 + 0];

          data->palette[i] = c;
        }
    }

  return DFB_OK;
}

static DFBResult
rle_decode_rgb_row (IDirectFBImageProvider_RLE_data *data,
                    int                              row)
{
  DFBResult  ret;
  u8         local_buf[data->pitch];
  const u8  *buf;
  u32       * const dst = data->common.image + row * data->width;
  int        i;

  if (data->indexed && data->no_palette)
    return DFB_INVARG;

  switch (data->compression)
    {
    case RLEIC_NONE:
    case RLEIC_RLE4:
    case RLEIC_RLE8:
    case RLEIC_BD_RLE8:
    case RLEIC_H2_DVD:
    case RLEIC_H8_DVD:
      break;

    case RLEIC_BITFIELDS: /* Unimplemented */
    default:
      D_ERROR ("Unsupported compression %d!\n", data->compression);
      return DFB_UNSUPPORTED;
    }

  if (data->indexed)
    {
      /* Source indexes picked up indirectly from decomp. buffer */
      D_ASSERT (data->image_indexes != NULL);
      if (data->depth > 8)
        return DFB_INVARG;
      buf = data->image_indexes + row * data->width;
    }
  else
    {
      /* Source (RGB or indexes) picked up directly from payload */
      D_ASSERT (data->common.base.buffer != NULL);
      ret = buffer_to_ptr_copy (data->common.base.buffer, local_buf,
                                data->pitch);
      if (ret)
        return ret;
      buf = local_buf;
    }

  D_ASSERT (data->common.image != NULL);

  switch (data->depth)
    {
    case 1:
    case 4:
    case 8:
      for (i = 0; i < data->width; ++i)
        {
          u8 byte = buf[i];
          DFBColor c = data->palette[byte];
          dst[i] = c.b | (c.g << 8) | (c.r << 16) | (c.a << 24);
        }
      break;

    case 16:
      for (i = 0; i < data->width; i++)
        {
          u32 r, g, b;
          u16 c;

          c = buf[i*2+0] | (buf[i*2+1]<<8);
          r = (c >> 10) & 0x1f;
          g = (c >>  5) & 0x1f;
          b = (c      ) & 0x1f;
          r = (r << 3) | (r >> 2);
          g = (g << 3) | (g >> 2);
          b = (b << 3) | (b >> 2);

          dst[i] = b | (g<<8) | (r<<16) | 0xff000000;
        }
      break;

    case 24:
      for (i = 0; i < data->width; i++)
        dst[i] = (0
                  | 0xff000000
                  | (buf[i * 3 + 2] << 16)
                  | (buf[i * 3 + 1] <<  8)
                  | (buf[i * 3 + 0]      )
                 );
      break;

    case 32:
      for (i = 0; i < data->width; i++)
        dst[i] = (0
                  | 0xff000000
                  | (buf[i * 4 + 2] << 16)
                  | (buf[i * 4 + 1] <<  8)
                  | (buf[i * 4 + 0]      )
                 );
      break;

    default:
      return DFB_UNSUPPORTED;
    }

  return DFB_OK;
}



static DFBResult
rle_expand_image_indexes (IDirectFBImageProvider_RLE_data *data)
{
  DFBResult ret;
  u8        buf[data->pitch];
  int       row;

  switch (data->compression)
    {
    case RLEIC_NONE:
      break;

    case RLEIC_RLE4:
    case RLEIC_RLE8:
    case RLEIC_BD_RLE8:
    case RLEIC_BITFIELDS: /* Unimplemented */
    default:
      D_ERROR ("Unsupported compression %d!\n", data->compression);
      return DFB_UNSUPPORTED;
    }

  D_ASSERT (data->common.base.buffer != NULL);
  D_ASSERT (data->image_indexes != NULL);


  for (row = 0; row < data->height; ++row)
    {
      int y = data->topfirst ? row : data->height-1-row;
      u8 * const dst = data->image_indexes + y * data->width;

      /* Source (RGB or indexes) picked up directly from payload */
      ret = buffer_to_ptr_copy (data->common.base.buffer, buf, data->pitch);
      if (ret)
        return ret;

      switch (data->depth)
        {
        int i;

        case 1:
          for (i = 0; data->width; ++i)
            dst[i] = buf[i >> 3] & (0x80 >> (i & 7));
          break;

        case 4:
          for (i = 0; i < data->width; ++i)
            dst[i] = buf[i >> 1] & (0xf0 >> (i & 1));
          break;

        case 8:
          for (i = 0; i < data->width; ++i)
            dst[i] = buf[i];
          break;

        default:
          return DFB_UNSUPPORTED;
        }
    }

  return DFB_OK;
}



static DFBResult
rle_decode_BI_RLE8 (const u8 * const buffer,
                    uint      size,
                    u8       * const image,
                    uint      w,
                    uint      h,
                    uint      top)
{
  const u8 *buffer_ptr = buffer;
  const u8 * const buffer_end = buffer + size;

  u16  run_length;
  u8   byte;
  u8   pixel;

  u16  x = 0;
  u16  y = 0;

  u8       *image_ptr = rle_set_position (image, 0, y, w, h, top);
  const u8 * const image_end = image + (h * w);

  D_DEBUG_AT (HWRLE, "%s: input: %u bytes, output: %ux%u pixels\n",
              __FUNCTION__, size, w, h);

  while (buffer_ptr < buffer_end)
    {
      byte = rle_input_byte (&buffer_ptr, buffer_end);

      if (byte > 0)
        {
          pixel = rle_input_byte (&buffer_ptr, buffer_end);

          for (run_length = byte; run_length > 0 && x < w; --run_length, ++x)
            rle_output_byte (&image_ptr, image_end, pixel);

          continue;
        }

      byte = rle_input_byte (&buffer_ptr, buffer_end);

      switch (byte)
        {
        case 0:
          x = 0;
          ++y;
          image_ptr = rle_set_position (image, x, y, w, h, top);
          break;

        case 1:
          buffer_ptr = buffer_end;
          break;

        case 2:
          x += rle_input_byte (&buffer_ptr, buffer_end);
          y += rle_input_byte (&buffer_ptr, buffer_end);
          image_ptr = rle_set_position (image, x, y, w, h, top);
          break;

        default:
          for (run_length = byte; run_length > 0 && x < w; --run_length, ++x)
            {
              pixel = rle_input_byte (&buffer_ptr, buffer_end);
              rle_output_byte (&image_ptr, image_end, pixel);
            }
          buffer_ptr += run_length;
          if (byte & 1)
            rle_input_byte (&buffer_ptr, buffer_end);
        }
    }

  return DFB_OK;
}


static unsigned int
rle_get_n_bits (const u8       ** const buffer_ptr_ptr,
                const u8        * const buffer_end,
                unsigned short   n)
{
  unsigned int dest = 0;
  static unsigned short currentbit = 7; /* Start at left most bit */
  static unsigned short srcbyte;

  if (!n)
    return 0; /* 0 bits == 0 */

  /* Re-Align to Byte Boundary */
  if (n > sizeof (unsigned int) * 8)
    {
      currentbit = 7;
      return 0;
    }

  for ( ; n; --n) /* iterate until n or currentbit runs out */
    {
      unsigned int tmp;

      if (currentbit == 7)
        srcbyte = rle_input_byte (buffer_ptr_ptr, buffer_end);

      //printf(".");
      tmp = (srcbyte & (1 << currentbit));
      tmp = tmp >> currentbit;
      tmp = tmp << (n - 1);

      if (currentbit)
        --currentbit;
      else
        currentbit = 7;

      dest |= tmp;
    }

  //printf("dest = %x : n = %d, currentbit = %d\n", dest, n, currentbit);

  return dest;
}


static u8
__attribute__((unused))
rle_fetch_half_byte (const u8 ** const buffer_ptr_ptr,
                     const u8  * const buffer_end)
{
  static bool top = 1;
  static u8   byte = 0;

  if (top)
    {
      byte = rle_input_byte (buffer_ptr_ptr, buffer_end);
      D_DEBUG_AT (HWRLE, "%s: byte: {%x}\n", __FUNCTION__, byte);
      top = 0;
      return byte >> 4;
    }
  else
    {
      top = 1;
      return byte & 0x0f;
    }
}

static DFBResult
rle_decode_H2_DVD (const u8 * const buffer,
                   uint      size,
                   u8       * const image,
                   uint      w,
                   uint      h)
{
  const u8 *buffer_ptr = buffer;
  const u8 * const buffer_end = buffer + size;

  u16 run_length;
  u8  pixel;

  uint x = 0;
  uint y = 0;

  u8       *image_ptr = image;
  const u8 * const image_end = image + (h * w);

  D_DEBUG_AT (HWRLE, "%s: input: %u bytes, output: %ux%u pixels\n",
              __FUNCTION__, size, w, h);

  while (buffer_ptr < buffer_end)
    {
      unsigned int v, t;

      v = 0;

      for (t = 1; v < t && t <= 0x40; t <<= 2)
        v = (v << 4) | rle_get_n_bits (&buffer_ptr, buffer_end, 4);

      pixel = v & 3;

      if (v < 4)  /* Code for fill rest of line */
        run_length = w - x;
      else
        run_length = v >> 2;


      //run_length = MIN( w - x, run_length );

      //printf("[x:%d r:%d p:%d]", x, run_length, pixel);

      while (run_length--)
        {
          rle_output_byte (&image_ptr, image_end, pixel);
          ++x;

          /* EOL Check */
          if (x >= w)
            {
              //D_ERROR ("Run passed EOL x=%d, w=%d, y=%d\n",x, w, y);
              x = 0;
              ++y;
            }

          if (y > h)
            {
              D_ERROR ("%s: Run passed EOF y/h: %d/%d, run_length: %d\n",
                       __FUNCTION__, y, h, run_length);
              return DFB_OK;
            }
        }
    }

  return DFB_OK;
}



static DFBResult
rle_decode_H8_DVD (const u8 * const buffer,
                   uint      size,
                   u8       * const image,
                   uint      w,
                   uint      h)
{
  const u8 *buffer_ptr = buffer;
  const u8 * const buffer_end = buffer + size;

  uint x = 0;
  uint y = 0;

  u8       *image_ptr = image;
  const u8 * const image_end = image + (h * w);

  D_DEBUG_AT (HWRLE, "%s: input: %u bytes, output: %ux%u pixels\n",
              __FUNCTION__, size, w, h);

  while (buffer_ptr < buffer_end)
    {
      u8  pixel;
      u32 run_length;

      bool run = rle_get_n_bits (&buffer_ptr, buffer_end, 1);

      /* Read Pixel Value */
      bool eightbit = rle_get_n_bits (&buffer_ptr, buffer_end, 1);

      if (eightbit)
        pixel = rle_get_n_bits (&buffer_ptr, buffer_end, 8);
      else
        pixel = rle_get_n_bits (&buffer_ptr, buffer_end, 2);

      /* Establish Run-Length */
      if (run)
        {
          if (rle_get_n_bits (&buffer_ptr, buffer_end, 1) /* b4 or b10 */)
            {
              run_length = rle_get_n_bits (&buffer_ptr, buffer_end, 7);
              if (run_length)
                run_length += 9;
              else
                /* Run till EOL */
                run_length = w - x;
            }
          else
            run_length = rle_get_n_bits (&buffer_ptr, buffer_end, 3) + 2;
        }
      else
        run_length = 1;

      //printf("[x:%d y:%d r:0x%x p:%d]", x, y, run_length, pixel);

      run_length = MIN (w - x, run_length); /* Run shouldn't over run a line! */

      while (run_length-- > 0)
        {
          rle_output_byte (&image_ptr, image_end, pixel);
          ++x;

          /* EOL Check */
          if (x >= w)
            {
              //D_ERROR ("Run passed EOL x=%d, w=%d, y=%d\n",x, w, y);
              x = 0;
              ++y;

              /* Align bit stream */
              rle_get_n_bits (&buffer_ptr, buffer_end, -1);
            }

          if (y >= h)
            {
              D_ERROR ("%s: Run passed EOF y/h: %d/%d, run_length: %d\n",
                       __FUNCTION__, y, h, run_length);
              return DFB_OK;
            }
        }
    }

  D_DEBUG_AT (HWRLE, "%s: decode completed @ x/y: %u/%u\n",
              __FUNCTION__, x , y);

  return DFB_OK;
}


static DFBResult
rle_decode_BD_RLE8 (const u8 * const buffer,
                    uint      size,
                    u8       * const image,
                    uint      w,
                    uint      h)
{
  const u8 *buffer_ptr = buffer;
  const u8 * const buffer_end = buffer + size;

  const u8 long_run_length = 0x40;
  const u8 non_zero_run    = 0x80;

  uint x = 0;
  uint y = 0;

  u8       *image_ptr = image;
  const u8 * const image_end = image + (h * w);

  D_DEBUG_AT (HWRLE, "%s: input: %u bytes, output: %ux%u pixels\n",
              __FUNCTION__, size, w, h);

  while (buffer_ptr < buffer_end)
    {
      u8  pixel;
      u8  byte;
      u16 run_length;

      pixel = rle_input_byte (&buffer_ptr, buffer_end);
      if (pixel > 0)
        {
          rle_output_byte (&image_ptr, image_end, pixel);
          ++x;
          continue;
        }

      byte = rle_input_byte (&buffer_ptr, buffer_end);
      if (byte == 0)
        {
          if (y < h)
            {
              x = 0;
              ++y;
              image_ptr = image + (w * y);
              continue;
            }
          else
            break;
        }

      if (byte & long_run_length)
        {
          run_length   = byte & 0x3f;
          run_length <<= 8;
          run_length  |= rle_input_byte (&buffer_ptr, buffer_end);
        }
      else
        run_length = byte & 0x3f;

      if (byte & non_zero_run)
        pixel = rle_input_byte (&buffer_ptr, buffer_end);
      else
        pixel = 0;

      while (run_length--)
        {
          rle_output_byte (&image_ptr, image_end, pixel);
          ++x;
        }
    }

  return DFB_OK;
}


static DFBResult
rle_decode_BI_RLE4 (const u8 * const buffer,
                    uint      size,
                    u8       * const data_ptr,
                    uint      w,
                    uint      h,
                    uint      top)
{
  D_WARN ("Unimplemented compression format in %s!\n", __FUNCTION__);

  return DFB_UNIMPLEMENTED;
}



static DFBResult
rle_decode_index_block (IDirectFBImageProvider_RLE_data *data)
{
  DFBResult  ret    = DFB_OK;
  uint       size   = data->payload_size;
  u8        *dst    = data->image_indexes;
  uint       width  = data->width;
  uint       height = data->height;
  uint       top    = data->topfirst ? 1 : 0;
  u8         buf[size];

  switch (data->compression)
    {
    case RLEIC_NONE:
      switch (data->depth)
        {
        case 1:
        case 4:
        case 8:
          break;

        default:
          return DFB_UNSUPPORTED;
        }

      ret = rle_expand_image_indexes (data);
      break;

    case RLEIC_RLE8:
      ret = buffer_to_ptr_copy (data->common.base.buffer, buf, size);
      if (ret)
        return ret;

      if (data->depth != 8)
        {
          D_ERROR ("%s: Unsupported compression %d with depth %d!\n",
                   __FUNCTION__, data->compression, data->depth);
          return DFB_UNSUPPORTED;
        }

      ret = rle_decode_BI_RLE8 (buf, size, dst, width, height, top);
      break;

    case RLEIC_BD_RLE8:
      ret = buffer_to_ptr_copy (data->common.base.buffer, buf, size);
      if (ret)
        return ret;

      if (data->depth != 8)
        {
          D_ERROR ("%s: Unsupported compression %d with depth %d!\n",
                   __FUNCTION__, data->compression, data->depth);
          return DFB_UNSUPPORTED;
        }

      ret = rle_decode_BD_RLE8 (buf, size, dst, width, height);
      break;

    case RLEIC_H2_DVD:
      ret = buffer_to_ptr_copy (data->common.base.buffer, buf, size);
      if (ret)
        return ret;

      if (data->depth != 8)
        {
          D_ERROR ("%s: Unsupported compression %d with depth %d!\n",
                   __FUNCTION__, data->compression, data->depth);
          return DFB_UNSUPPORTED;
        }

      ret = rle_decode_H2_DVD (buf, size, dst, width, height);
      break;

    case RLEIC_H8_DVD:
      ret = buffer_to_ptr_copy (data->common.base.buffer, buf, size);
      if (ret)
        return ret;

      if (data->depth != 8)
        {
          D_ERROR ("%s: Unsupported compression %d with depth %d!\n",
                   __FUNCTION__, data->compression, data->depth);
          return DFB_UNSUPPORTED;
        }

      ret = rle_decode_H8_DVD (buf, size, dst, width, height);
      break;

    case RLEIC_RLE4:      /* Unimplemented as of version 1.0.0 */
      ret = buffer_to_ptr_copy (data->common.base.buffer, buf, size);
      if (ret)
        return ret;

      if (data->depth != 4)
        {
          D_ERROR ("%s: Unsupported compression %d with depth %d!\n",
                   __FUNCTION__, data->compression, data->depth);
          return DFB_UNSUPPORTED;
        }

      ret = rle_decode_BI_RLE4 (buf, size, dst, width, height, top);
      break;

    default:
      D_ERROR ("%s: Unsupported compression %d!\n",
               __FUNCTION__, data->compression);
      return DFB_UNSUPPORTED;
    }

  return ret;
}


/*******************************************************************************
 *
 *                      PRIVATE STATIC SECTION     (Hardware Decode Section)
 *
 *******************************************************************************/

#if defined(RLE_PROVIDER_HW)
#include <../gfxdrivers/stgfx/stgfx.h>
#include <../gfxdrivers/stgfx2/stm_gfxdriver.h>
#include <dlfcn.h>
static STMFB_GFXMEMORY_PARTITION
bdisp_surface_pool_get_partition (const CoreSurfacePool * const pool)
{
  unsigned i;

  switch (dfb_system_get_accelerator ())
    {
    case FB_ACCEL_ST_GAMMA:
    case FB_ACCEL_ST_BDISP:
      {
      const STGFXDeviceData * const stdev = dfb_gfxcard_get_device_data ();
      for (i = 0; i < D_ARRAY_SIZE (stdev->aux_pools); ++i)
        if (stdev->aux_pools[i] == pool)
          return STMFBGP_GFX0 + i;
      }
      break;

    case FB_ACCEL_ST_BDISP_USER:
      {
      const STGFX2DeviceData * const stdev = dfb_gfxcard_get_device_data ();
      for (i = 0; i < D_ARRAY_SIZE (stdev->aux_pools); ++i)
        if (stdev->aux_pools[i] == pool)
          return STMFBGP_GFX0 + i;
      }
      break;

    default:
      return STMFBGP_FRAMEBUFFER;
    }

  return STMFBGP_FRAMEBUFFER;
}


static DFBResult
stgfb_hardware_rle_decode (IDirectFBImageProvider * const thiz,
                           IDirectFBSurface       * const destination,
                           DFBRectangle           * const dest_rect)
{
  DFBResult             dfbret;
  CoreSurfaceBufferLock lock;

  DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_RLE)

  IDirectFBSurface_data * const dst_data = destination->priv;

  switch (data->compression)
    {
    case RLEIC_BD_RLE8:
      /* Supported BDisp Decode */
      break;

    case RLEIC_H2_DVD:	/* Currently Unsupported in BDisp */
    case RLEIC_H8_DVD:  /* Move when available */
    default:
      return DFB_UNSUPPORTED;
    }

  /* BDisp decode follows ... */
  D_DEBUG_AT (HWRLE, "%s: attempting BDisp decode\n", __FUNCTION__);

  /* We must ensure the parameters are correct for the Blit, or the blitter
     will hang */

  /* Ensure dest_rect matches the appropriate width and height */
  if (dest_rect->w != data->width
      || dest_rect->h != data->height)
    {
      D_ERROR ("%s: invalid arguments: dest w/h: %d/%d, w/h: %u/%u\n",
               __FUNCTION__, 
               dest_rect->w, dest_rect->h, data->width, data->height);
      return DFB_INVARG;
    }

  /* We only need to create and load the raw RLE data once. After that, we
     can just blit this quickly to dest. */
  if (!data->compressed_data)
    {
      CoreSurfaceConfig config;

      config.flags = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
      config.size.w = data->payload_size;
      config.size.h = 1;
      config.format = DSPF_A8; /* any 8bpp format would work here */
      config.caps   = DSCAPS_VIDEOONLY;
      #ifdef DIRECT_BUILD_DEBUG
      config.caps   |= DSCAPS_SHARED;
      #endif

      if (dfb_surface_create (data->common.core,
                              &config,
                              CSTF_NONE,
                              0,
                              NULL,
                              &data->compressed_data))
        return DFB_NOVIDEOMEMORY;

      /**** copy data onto raw surface ****/

      /* Set Data Buffer pointer ... */
      data->common.buffer->SeekTo (data->common.buffer, data->img_offset);

      dfbret = dfb_surface_lock_buffer (data->compressed_data, CSBR_BACK,
                                        CSAID_CPU, CSAF_WRITE, &lock);
      if (dfbret)
        return dfbret;

      dfbret = buffer_to_ptr_copy (data->common.buffer, lock.addr,
                                   data->payload_size);
      dfb_surface_unlock_buffer (data->compressed_data, &lock);
      if (dfbret)
        return dfbret;
    }

  /* the following code tries to do the stretch blit through stgfx2, if that
     fails, the normal approach through the ioctl() interface is done for the
     kernel based driver */
  bool direct_success = false;
  CoreSurfaceBufferLock lock_src, lock_dst;

  dfbret = dfb_surface_lock_buffer (data->compressed_data, CSBR_BACK,
                                    CSAID_GPU, CSAF_READ, &lock_src);
  if (dfbret)
    return dfbret;
  dfbret = dfb_surface_lock_buffer (dst_data->surface, CSBR_BACK,
                                    CSAID_GPU, CSAF_WRITE, &lock_dst);
  if (dfbret)
    {
      dfb_surface_unlock_buffer (data->compressed_data, &lock_src);
      return dfbret;
    }

  if (likely (data->rle_stretch))
    direct_success = data->rle_stretch (dfb_gfxcard_get_driver_data (),
                                        dfb_gfxcard_get_device_data (),
                                        lock_src.phys,
                                        data->compressed_data->config.size.w,
                                        &lock_dst,
                                        dest_rect);
  if (likely (direct_success))
    ;
  else
    {
      static const SURF_FMT  rleic_to_surf[] = { SURF_RLD_BD, SURF_RLD_H2,
                                                 SURF_RLD_H8 };
      STMFBIO_BLT_DATA       blit;
      /* this is needed to obtain framebuffer fd */
      const FBDev           * const dfb_fbdev = (FBDev *) dfb_system_data ();

      /* Create command structure to send to FBDev */
      /* Request decode operation to destination surface */

      /* Perform a Copy Blit. STGFB will treat RLE as a surface type. */
      blit.operation = BLT_OP_COPY;

      /* Inform copy complex that this is an RLE Decode */
      blit.ulFlags = BLT_OP_FLAGS_RLE_DECODE;

      blit.srcMemBase = bdisp_surface_pool_get_partition (lock_src.allocation->pool);
      blit.srcOffset  = lock_src.offset;
      blit.srcPitch   = data->compressed_data->config.size.w;
      blit.srcFormat  = rleic_to_surf[data->compression - RLEIC_BD_RLE8];

      blit.dstMemBase = bdisp_surface_pool_get_partition (lock_dst.allocation->pool);
      blit.dstOffset  = lock_dst.offset;
      blit.dstPitch   = lock_dst.pitch;

      blit.src_top    = 0;
      blit.src_bottom = data->compressed_data->config.size.h;
      blit.src_left   = 0;
      blit.src_right  = 0;

      /* (data->indexed) ? DSPF_LUT8 : DSPF_RGB32; */
      blit.dstFormat  = (data->indexed) ? SURF_CLUT8 : SURF_ARGB8888;

      if (dest_rect)
        {
          blit.dst_left   = dest_rect->x;
          blit.dst_top    = dest_rect->y;
          blit.dst_right  = dest_rect->x + dest_rect->w;
          blit.dst_bottom = dest_rect->y + dest_rect->h;
        }
      else
        {
          blit.dst_left   = 0;
          blit.dst_top    = 0;
          blit.dst_right  = dst_data->surface->config.size.w;
          blit.dst_bottom = dst_data->surface->config.size.h;
        }

      /* Send the ioctl */
      D_DEBUG_AT (HWRLE,
                  "%s: flags: %.8lx src base/l/r/t/b: %d/%hu/%hu/%hu/%hu "
                  "dst base/l/r/t/b: %d/%hu/%hu/%hu/%hu\n", __FUNCTION__,
                  blit.ulFlags,
                  blit.srcMemBase, blit.src_left, blit.src_right,
                  blit.src_top, blit.src_bottom,
                  blit.dstMemBase, blit.dst_left, blit.dst_right,
                  blit.dst_top, blit.dst_bottom);
      D_DEBUG_AT (HWRLE, "%s: src offs/pitch/fmt: %.8lx/%lu/%d "
                  "dst offs/pitch/fmt: %.8lx/%lu/%d\n", __FUNCTION__,
                  blit.srcOffset, blit.srcPitch, blit.srcFormat,
                  blit.dstOffset, blit.dstPitch, blit.dstFormat);

#ifdef ALLOW_DUMP_RAW_TO_FILE
      if (getenv ("RLE_DUMP_RAW_TO_FILE"))
        {
          char fn[1024];
          FILE *f;
          const STMFBIO_BLT_DATA * const r = &blit;

          snprintf (fn, sizeof (fn), "/tmp/rle/raw_%05d.txt", data->idx);
          if ((f = fopen (fn, "w+")) != NULL)
            {
              fprintf (f,
                       "op %d, flg %.8lx, col %.8lx, alph %.8lx, key %.8lx, msk %.8lx\n"
                       "src: %u + %lu, pitch %lu\n"
                       "dst: %u + %lu, pitch %lu\n"
                       "src/dst fmt %d/%d"
                       "src: t/p/l/r %hu %hu %hu %hu"
                       "dst: t/p/l/r %hu %hu %hu %hu\n",
                       r->operation, r->ulFlags,
                       r->colour, r->globalAlpha, r->colourKey, r->planeMask,
                       r->srcMemBase, r->srcOffset, r->srcPitch,
                       r->dstMemBase, r->dstOffset, r->dstPitch,
                       r->srcFormat, r->dstFormat,
                       r->src_top, r->src_bottom, r->src_left, r->src_right,
                       r->dst_top, r->dst_bottom, r->dst_left, r->dst_right);
              fflush (f);
              fclose (f);
            }
          else
            fprintf (stderr, "Failed to open file %05d\n", data->idx);
        }
#endif /* ALLOW_DUMP_RAW_TO_FILE */

      int ret;
      if ((ret = ioctl (dfb_fbdev->fd, STMFBIO_BLT, &blit)) == 0)
        {
          D_DEBUG_AT (HWRLE,
                      "%s: success: input: %d bytes, output: %dx%d pixels\n",
                      __FUNCTION__, data->compressed_data->config.size.w,
                      dst_data->surface->config.size.w,
                      dst_data->surface->config.size.h);
          dfbret = DFB_OK;
        }
      else
        {
          D_DEBUG_AT (HWRLE,
                      "%s: failed: %d input: %d bytes, output: %dx%d pixels\n",
                      __FUNCTION__, errno, data->compressed_data->config.size.w,
                      dst_data->surface->config.size.w,
                      dst_data->surface->config.size.h);
          dfbret = DFB_FAILURE;
        }
    }
  dfb_surface_unlock_buffer (dst_data->surface, &lock_dst);
  dfb_surface_unlock_buffer (data->compressed_data, &lock_src);

  if (dfbret == DFB_OK)
    {
      deb_gettimeofday (&data->common.endtime, NULL);
      deb_timersub (&data->common.endtime, &data->common.starttime,
                    &data->common.endtime);
      D_DEBUG_AT (HWRLE_TIME, "  -> BDispII RLE decode total time %lu.%06lu\n",
                  data->common.endtime.tv_sec, data->common.endtime.tv_usec);
    }

  return dfbret;
}
#else /* RLE_PROVIDER_HW */
#define stgfb_hardware_rle_decode(thiz,destination,dest_rect) DFB_UNIMPLEMENTED
#endif /* RLE_PROVIDER_HW */



#if defined(RLE_PROVIDER_USE_MME)
static void
TransformerCallback (MME_Event_t    Event,
                     MME_Command_t *CallbackData,
                     void          *UserData)
{
  const RLEDecode_TransformReturnParams_t * const transform_result =
    CallbackData->CmdStatus.AdditionalInfo_p;
  struct _MMECommon * const mme = (struct _MMECommon *) UserData;

  D_DEBUG_AT (HWRLE, "%sTransformerCallback: Event: %d: (%s)%s\n",
              RED, Event, get_mme_event_string (Event), BLACK);
  D_DEBUG_AT (HWRLE, "  -> CallbackData->CmdStatus.State: %d (%s)\n",
              CallbackData->CmdStatus.State,
              get_mme_state_string (CallbackData->CmdStatus.State));
  D_DEBUG_AT (HWRLE, "  -> CallbackData->CmdStatus.CmdId: %u (%.8x)\n",
              CallbackData->CmdStatus.CmdId,
              CallbackData->CmdStatus.CmdId);
  D_DEBUG_AT (HWRLE, "  -> CallbackData->CmdStatus.Error: %d (%s)\n",
              CallbackData->CmdStatus.Error,
              get_mme_error_string (CallbackData->CmdStatus.Error));
  D_DEBUG_AT (HWRLE, "  -> CallbackData->CmdStatus.AdditionalInfoSize: %u\n",
              CallbackData->CmdStatus.AdditionalInfoSize);
  D_DEBUG_AT (HWRLE, "  -> mme->decode_success: %d\n", mme->decode_success);

  switch (Event)
    {
    case MME_COMMAND_COMPLETED_EVT:
      switch (CallbackData->CmdStatus.State)
        {
        case MME_COMMAND_COMPLETED:
          if (CallbackData->CmdStatus.CmdId
              == mme->TransformCommand.CmdStatus.CmdId)
            {
              deb_gettimeofday (&mme->endtime, NULL);
              deb_timersub (&mme->endtime, &mme->starttime, &mme->endtime);
              D_DEBUG_AT (HWRLE_TIME, "  -> MME decode total time %lu.%06lu\n",
                          mme->endtime.tv_sec, mme->endtime.tv_usec);

              D_DEBUG_AT (HWRLE, "  -> expanded bytes: %u\n",
                          transform_result->BytesWritten);

              D_DEBUG_AT (HWRLE, "  -> RLE result code: %d (%s)\n",
                          transform_result->ErrorType,
                          get_rle_error_string (transform_result->ErrorType));

              mme->decode_success = 1;
              sema_signal_event (&mme->decode_event);
            }
          else
            {
              /* buffer completed */
              struct _MMEHelper_buffer *buf =
                container_of (CallbackData->DataBuffers_p,
                              struct _MMEHelper_buffer, buffer);

              D_DEBUG_AT (HWRLE, "  -> buffer %p completed (container @ %p)\n",
                          CallbackData->DataBuffers_p[0], buf);

              sema_signal_event (&buf->sema);
            }
          break;

        case MME_COMMAND_FAILED:
          D_WARN ("(%5d) %s: command %u (%.8x) failed: error %d (%s)",
                  direct_gettid (), mme->name,
                  CallbackData->CmdStatus.CmdId,
                  CallbackData->CmdStatus.CmdId,
                  CallbackData->CmdStatus.Error,
                  get_mme_error_string (CallbackData->CmdStatus.Error));

          if (CallbackData->CmdStatus.CmdId
              == mme->TransformCommand.CmdStatus.CmdId)
            {
              mme->decode_success = -1;

              /* On Failure allow main flow to continue and tidy up */
              sema_signal_event (&mme->decode_event);
            }
          break;

        case MME_COMMAND_IDLE:
        case MME_COMMAND_PENDING:
        case MME_COMMAND_EXECUTING:
        default:
          D_WARN ("(%5d) %s: command %u (%.8x) completed in unknown state: %d (%s)",
                  direct_gettid (), mme->name,
                  CallbackData->CmdStatus.CmdId, CallbackData->CmdStatus.CmdId,
                  CallbackData->CmdStatus.State,
                  get_mme_state_string (CallbackData->CmdStatus.State));
          break;
        }
      break; /* MME_COMMAND_COMPLETED_EVT */

    case MME_DATA_UNDERFLOW_EVT:
      ++mme->n_underflows;
      break; /* MME_DATA_UNDERFLOW_EVT */

    case MME_NOT_ENOUGH_MEMORY_EVT:
    case MME_NEW_COMMAND_EVT:
    default:
      D_WARN ("(%5d) %s: unhandled event %d (%s) occured on command %u (%.8x)",
              direct_gettid (), mme->name, Event,
              get_mme_event_string (Event), CallbackData->CmdStatus.CmdId,
              CallbackData->CmdStatus.CmdId);

      D_WARN ("(%5d) %s: CallbackData->CmdStatus.AdditionalInfoSize: %u",
              direct_gettid (), mme->name, CallbackData->CmdStatus.AdditionalInfoSize);

      if (CallbackData->CmdStatus.CmdId == mme->TransformCommand.CmdStatus.CmdId)
        {
          D_WARN ("(%5d) %s: transform_result->bytes_written: %u",
                  direct_gettid (), mme->name, transform_result->BytesWritten);
          D_WARN ("(%5d) %s: result code: %d (%s)\n", direct_gettid (),
                  mme->name, transform_result->ErrorType,
                  get_rle_error_string (transform_result->ErrorType));
        }
      break;
    } /* switch (Event) */
}


static DFBResult
abort_transformer (struct _MMECommon * const mme)
{
  IDirectFBImageProvider_RLE_data * const data =
    container_of (mme, IDirectFBImageProvider_RLE_data, common);

  if (!mme)
       return DFB_THIZNULL;

  if (!data)
       return DFB_DEAD;

  return DFB_OK;
}



static DFBResult
send_rle_image (struct _MMECommon * const data)
{
  DFBResult res = MME_SUCCESS;

  unsigned int i;

  mme_helper_calculate_packets (data);
  D_ASSERT (data->packets == 1);

  /* Clean buffer pointers, as we create based on them being empty! */
  memset (&data->SendDataBuffers, 0, sizeof (data->SendDataBuffers));

  D_DEBUG_AT (HWRLE, "%s: iterating %d packets for %f chunks of data out of "
              "%d bytes\n", __FUNCTION__,
              data->packets,
              (float) ((float) data->bytes / (float) MME_STREAMING_BUFFERSIZE),
              data->bytes);
  //data->InDataBuffers = (MME_DataBuffer_t **)D_MALLOC(sizeof(MME_DataBuffer_t *) * MAX_STREAMING_BUFFERS);

  for (i = 0; i < data->packets; ++i)
    {
      unsigned int this_read = MIN (data->bytes, MME_STREAMING_BUFFERSIZE);
      unsigned int currentbuffer = i % MAX_STREAMING_COMMANDS;
      struct _MMEHelper_buffer * const buffer =
        &data->SendDataBuffers[currentbuffer];

      D_DEBUG_AT (HWRLE, "%s sending packet %u through buffer %u\n",
                  __FUNCTION__, i, currentbuffer);

      if (!buffer->buffer)
        {
          res = _alloc_send_buffer (data->Handle, this_read, buffer);
          if (res != DFB_OK)
            return res;
        }
      else
        {
          /* Wait for a buffer to be available (maybe in streaming version!) */
          while ((sema_wait_event (&buffer->sema) == -1)
                 && errno == EINTR)
            ;
        }

      /* Some checking - We've waited ... should we continue ...*/
      if (data->decode_success < 0)
        {
          D_WARN ("(%5d) ImageProvider/%s: error sending data. Transform "
                  "error reported from callback",
                  direct_gettid (), data->name);
          sema_signal_event (&buffer->sema);
          return DFB_FAILURE;
        }

      if (buffer_to_mme_copy (data->base.buffer, buffer->buffer,
                              this_read) != DFB_OK)
        {
          D_WARN ("(%5d) Fetching %u bytes of data failed: res: %d",
                  direct_gettid (), this_read, res);
          sema_signal_event (&buffer->sema);
          /* hm, DirectFB (always?) returns DFB_FAILURE here... */
          return DFB_IO;
        }

      /* Subtract read data amount */
      data->bytes -= this_read;

      /* send the command */
      D_DEBUG_AT (HWRLE, "%s: Send Data Command\n", __FUNCTION__);

      res = MME_SendCommand (data->Handle, &buffer->command);

      D_DEBUG_AT (HWRLE, "%s: sent data: %d bytes remaining\n",
                  __FUNCTION__, data->bytes);

      if (res != MME_SUCCESS)
        {
          D_ERROR ("%s: MME_SendCommand failed: buffer %u: %s\n",
                   __FUNCTION__, currentbuffer, get_mme_error_string (res));
          res = DFB_FAILURE;
        }
    }

  return res;
}

static DFBResult
lx_multicom_rle_decode (IDirectFBImageProvider *thiz,
                        IDirectFBSurface       *destination,
                        const DFBRectangle     *dest_rect)
{
  int                    err;

  DFBRegion              clip;
  DFBRectangle           rect;
  DFBSurfacePixelFormat  format;
  IDirectFBSurface_data *dst_data;
  CoreSurface           *dst_surface;
  CoreSurfaceBufferLock  lock;

  DFBResult              ret;

  dst_data = (IDirectFBSurface_data *) destination->priv;
  DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_RLE)

  /* Initialise Multicom */
  /* Send *WHOLE FILE* (LXRLEDecode has no streaming support) */
  /* Request Transform (with output buffer) */
  /* Hope it works */

  /* Any problems - just bail out and let something else do it */

  if (!dst_data)
    return DFB_DEAD;

  dst_surface = dst_data->surface;

  if (!dst_surface)
    return DFB_DESTROYED;

  /* Need to check the pitch is the same as the width - or we'll be writing in the wrong buffer space */
  /* If the pitch is not the same as the width - let SH4 handle it until the LX decoder is more advanced ! */

  err = destination->GetPixelFormat (destination, &format);
  if (err)
    return err;

  dfb_region_from_rectangle (&clip, &dst_data->area.current);

  if (dest_rect)
    {
      if (dest_rect->w < 1 || dest_rect->h < 1)
        return DFB_INVARG;

      rect = *dest_rect;
      rect.x += dst_data->area.wanted.x;
      rect.y += dst_data->area.wanted.y;

      if (!dfb_rectangle_region_intersects (&rect, &clip))
        return DFB_OK;
    }
  else
    rect = dst_data->area.wanted;

  data->common.name = "hwRLE";
  static const char * const transformers[] = { "RLE_DECODER",
                                               "RLE_DECODER4",
                                               "RLE_DECODER3",
                                               "RLE_DECODER2",
                                               "RLE_DECODER1",
                                               "RLE_DECODER0",
                                               NULL };
  ret = mme_helper_init_transformer (&data->common, transformers, 0, NULL, NULL);
  if (ret == DFB_OK)
    {
      bool locked = false;

      sema_init_event (&data->common.decode_event, 0); /* Alerts to completion */

      ret = send_rle_image (&data->common);
      if (ret == DFB_OK)
        {
          ret = mme_helper_start_transformer (&data->common,
                                              sizeof (RLEDecode_TransformReturnParams_t),
                                              &data->ReturnParams,
                                              0, NULL,
                                              dst_surface, &lock);
          if (ret == DFB_OK)
            {
              D_DEBUG_AT (HWRLE, "%s: waiting for completion\n", __FUNCTION__);

              locked = true;

              /* Wait until the decode is complete */
              while ((sema_wait_event (&data->common.decode_event) == -1)
                     && errno == EINTR)
                ;

              D_DEBUG_AT (HWRLE, "%s: performed LX decode\n", __FUNCTION__);
            }
        }

      mme_helper_deinit_transformer (&data->common);

      if (likely (locked))
        dfb_surface_unlock_buffer (dst_surface, &lock);

      if (data->common.OutDataBuffers)
        {
          D_FREE (data->common.OutDataBuffers);
          data->common.OutDataBuffers = NULL;
        }

      unsigned i;
      for (i = 0; i < D_ARRAY_SIZE (data->common.SendDataBuffers); ++i)
        {
          if (data->common.SendDataBuffers[i].buffer)
            {
              MME_FreeDataBuffer (data->common.SendDataBuffers[i].buffer);
              data->common.SendDataBuffers[i].buffer = NULL;

              sema_close_event (&data->common.SendDataBuffers[i].sema);
            }
        }

      sema_close_event (&data->common.decode_event);
    }

  return ret;
}
#else /* RLE_PROVIDER_USE_MME */
#define lx_multicom_rle_decode(thiz,destination,dest_rect) DFB_UNIMPLEMENTED
#endif

#if defined(RLE_PROVIDER_HW) || defined(RLE_PROVIDER_USE_MME)
static DFBResult
hardware_rle_decode (IDirectFBImageProvider * const thiz,
                     IDirectFBSurface       * const destination,
                     DFBRectangle           * const dest_rect)

{
  DFBResult ret;

  DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_RLE)

  /* 1) try the BDisp in STGFB.
     2) Then where supported try an LX ...
     3) Finally - return - and let the image provider deal with the image
        on the SH4
  */

  /* store the current data buffer position for software fallback should we
     not succeed */
  unsigned int bufpos;
  data->common.base.buffer->GetPosition (data->common.base.buffer,
                                         &bufpos);

  deb_gettimeofday (&data->common.starttime, NULL);

  switch (data->compression)
    {
    case RLEIC_BD_RLE8:
#ifdef ALLOW_FORCE_SOFTWARE
      if (getenv ("RLE_FORCE_SOFTWARE"))
        {
          ret = -1;
          break;
        }
#endif /* ALLOW_FORCE_SOFTWARE */

      ret = stgfb_hardware_rle_decode (thiz, destination, dest_rect);
      break;

      /* BD_RLE8 decoding seems to be b0rken on the LX */
    case RLEIC_H2_DVD:
    case RLEIC_H8_DVD:
      ret = stgfb_hardware_rle_decode (thiz, destination, dest_rect);
      if (unlikely (ret))
        ret = lx_multicom_rle_decode (thiz, destination, dest_rect );
      break;

    default:
      ret = DFB_UNSUPPORTED;
      break;
    }

  /* Restore Buffer location to continue software decode */
  data->common.base.buffer->SeekTo (data->common.base.buffer, bufpos);

  return ret;
}
#else /* RLE_PROVIDER_HW || RLE_PROVIDER_USE_MME */
#define hardware_rle_decode(thiz,destination,dest_rect) DFB_UNIMPLEMENTED
#endif /* RLE_PROVIDER_HW || RLE_PROVIDER_USE_MME */


/*******************************************************************************
 *
 *                      PUBLIC INTERFACE STATIC SECTION
 *
 *******************************************************************************/

static DFBResult
IDirectFBImageProvider_RLE_RenderTo (IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *dest_rect)
{
  IDirectFBSurface_data  *dst_data;
  CoreSurface            *dst_surface;
  DFBRectangle            rect;
  DFBRegion               clip;
  CoreSurfaceBufferLock   lock;
  DIRenderCallbackResult  cb_result = DIRCR_OK;
  DFBResult               ret       = DFB_OK;

  DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_RLE)

  if (!destination)
    return DFB_INVARG;

  dst_data = destination->priv;
  if (!dst_data || !dst_data->surface)
    return DFB_DESTROYED;

  dst_surface = dst_data->surface;

  if (dest_rect)
    {
      if (dest_rect->w < 1 || dest_rect->h < 1)
        return DFB_INVARG;
      rect = *dest_rect;
      rect.x += dst_data->area.wanted.x;
      rect.y += dst_data->area.wanted.y;
    }
  else
    rect = dst_data->area.wanted;

  dfb_region_from_rectangle (&clip, &dst_data->area.current);
  if (!dfb_rectangle_region_intersects (&rect, &clip))
    return DFB_OK;

  /* Attempt a decode using the Any hardware acceleration available */
  if (DFB_OK == hardware_rle_decode (thiz, destination, &rect))
    return DFB_OK;

  /* Hardware wasn't capable or happy. Lets try ourselves */

  /* Software Decode */
  ret = dfb_surface_lock_buffer (dst_surface, CSBR_BACK, CSAID_CPU,
                                 CSAF_WRITE, &lock);
  if (ret)
    return ret;


  /* Decompress RLE coded data to packed indexes into local buffer. */
  if (data->indexed && !data->image_indexes)
    {
      struct timeval __attribute__ ((unused)) start, end;

      data->image_indexes = D_MALLOC (data->width*data->height);

      if (!data->image_indexes)
        {
          dfb_surface_unlock_buffer (dst_surface, &lock);
          return D_OOM ();
        }

      /* Source indexes will be picked up indirectly from decomp. buffer */
      data->common.base.buffer->SeekTo (data->common.base.buffer,
                                        data->img_offset);

      deb_gettimeofday (&start, NULL);

      ret = rle_decode_index_block (data);
      if (ret)
        {
          dfb_surface_unlock_buffer (dst_surface, &lock);
          return ret;
        }

      deb_gettimeofday (&end, NULL);
      deb_timersub (&end, &start, &end);
      D_DEBUG_AT (HWRLE_TIME, "%s: SW RLE decode took %lu.%06lus\n",
                  __FUNCTION__, end.tv_sec, end.tv_usec);
    }

  /* Special indexed to LUT8 loading. */
  if (data->indexed && dst_surface->config.format == DSPF_LUT8)
    {
      bool direct;

      if (!data->no_palette)
        {
          IDirectFBPalette *palette;
          ret = destination->GetPalette (destination, &palette);
          if (ret)
            {
              dfb_surface_unlock_buffer (dst_surface, &lock);
              return ret;
            }
          palette->SetEntries (palette, data->palette, data->num_colors, 0);
          palette->Release (palette);
        }

      /* Limitation for direct LUT8 access is to blit complete surface only */
      direct = (rect.x == 0
                && rect.y == 0
                && rect.w == dst_surface->config.size.w
                && rect.h == dst_surface->config.size.h
                && rect.w == data->width
                && rect.h == data->height);

      if (direct)
        {
          int  y, row;

          for (row = 0; row < data->height && cb_result == DIRCR_OK; ++row)
            {
              y = data->topfirst ? row : data->height-1-row;

              direct_memcpy (lock.addr + lock.pitch * y,
                             data->image_indexes + data->width * y,
                             data->width);

              if (data->common.base.render_callback)
                {
                  DFBRectangle r = (DFBRectangle) { 0, y, data->width, 1 };

                  cb_result = data->common.base.render_callback (&r,
                                                                 data->common.base.render_callback_context);
                }
            }

          if (cb_result != DIRCR_OK)
            ret = DFB_INTERRUPTED;

          dfb_surface_unlock_buffer (dst_surface, &lock);
          return ret;
        }
    }


  /* Invalid indexed to non-direct LUT8 loading without attached colormap. */
  if (data->indexed && data->no_palette)
    {
      dfb_surface_unlock_buffer (dst_surface, &lock);
      return DFB_INVARG;
    }


  /* Convert to ARGB and use generic loading code. */
  if (!data->common.image)
    {
      bool direct = (rect.w == data->width
                     && rect.h == data->height
                     && data->common.base.render_callback);
      int  y, row;

      data->common.image = D_MALLOC (data->width*data->height * 4);
      if (!data->common.image)
        {
          dfb_surface_unlock_buffer (dst_surface, &lock);
          return D_OOM ();
        }

      if (!data->indexed)
        /* Source (RGB or indexes) picked up directly from payload */
        data->common.base.buffer->SeekTo (data->common.base.buffer,
                                          data->img_offset);

      for (row = 0; row < data->height && cb_result == DIRCR_OK; ++row)
        {
          y = data->topfirst ? row : data->height-1-row;

          ret = rle_decode_rgb_row (data, y);
          if (ret)
            break;

          if (direct)
            {
              DFBRectangle r = { rect.x, rect.y+y, data->width, 1 };

              dfb_copy_buffer_32 (data->common.image + y * data->width,
                                  lock.addr, lock.pitch, &r, dst_surface,
                                  &clip );

              if (data->common.base.render_callback)
                {
                  r = (DFBRectangle) { 0, y, data->width, 1 };
                  cb_result = data->common.base.render_callback (&r,
                                                                 data->common.base.render_callback_context);
                }
            }
        }

      if (!direct)
        {
          dfb_scale_linear_32 (data->common.image, data->width, data->height,
                               lock.addr, lock.pitch, &rect, dst_surface,
                               &clip);

          if (data->common.base.render_callback)
            {
              DFBRectangle r = { 0, 0, data->width, data->height };

              data->common.base.render_callback (&r,
                                                 data->common.base.render_callback_context);
            }
        }

      if (cb_result != DIRCR_OK)
        {
          D_FREE (data->common.image);
          data->common.image = NULL;
          ret = DFB_INTERRUPTED;
        }
    }
  else
    {
      dfb_scale_linear_32 (data->common.image, data->width, data->height,
                           lock.addr, lock.pitch, &rect, dst_surface, &clip);

      if (data->common.base.render_callback)
        {
          DFBRectangle r = {0, 0, data->width, data->height};
          data->common.base.render_callback (&r,
                                             data->common.base.render_callback_context);
        }
    }

  //dfb_surface_dump (data->common.core, dst_surface, "results", "RLE-H2");

  dfb_surface_unlock_buffer (dst_surface, &lock);
  return ret;
}

static void
IDirectFBImageProvider_RLE_Destruct (IDirectFBImageProvider *thiz)
{
  IDirectFBImageProvider_RLE_data *data = thiz->priv;

#if defined(RLE_PROVIDER_HW)
  dlclose (data->stgfx2_handle);
#endif /* RLE_PROVIDER_HW */

  if (data->common.image)
    D_FREE (data->common.image);
  if (data->image_indexes)
    D_FREE (data->image_indexes);
  if (data->palette)
    D_FREE (data->palette);

#if defined(RLE_PROVIDER_HW)
  if (data->compressed_data)
    {
      dfb_surface_unref (data->compressed_data);
      data->compressed_data = NULL;
    }
#endif

#if defined(RLE_PROVIDER_USE_MME)
  if (data->common.decode_surface)
    dfb_surface_unref( data->common.decode_surface );
#endif
}

static DFBResult
IDirectFBImageProvider_RLE_GetSurfaceDescription (IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *desc)
{
  DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_RLE)

  if (!desc)
    return DFB_INVARG;

  desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_CAPS;
  desc->width       = data->width;
  desc->height      = data->height;
  desc->pixelformat = (data->indexed) ? DSPF_LUT8 : DSPF_RGB32;
  desc->caps        = DSCAPS_NONE;

  return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_RLE_GetImageDescription (IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *desc)
{
  DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_RLE)

  if (!desc)
    return DFB_INVARG;

  desc->caps = DICAPS_NONE;

  return DFB_OK;
}

/*******************************************************************************
 *
 *              PUBLIC INTERFACE EXPORTED SYMBOLS SECTION
 *
 *******************************************************************************/

static DFBResult
Probe (IDirectFBImageProvider_ProbeContext *ctx)
{
  /* 2 bytes of Magic */
  if (IS_MAGIC (ctx->header,'R','L')
      || IS_MAGIC (ctx->header,'B','M'))
    return DFB_OK;

  return DFB_UNSUPPORTED;
}

#ifdef ALLOW_DUMP_RAW_TO_FILE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
static char filename[4096];
static const char *
_get_filename (const char * const directory,
               const char * const prefix,
               int        * const idx)
{
  unsigned int num = 0;

  if (prefix)
    {
      /* find the lowest unused index */
      int found = false;
      while (!found && ++num < 100000)
        {
          snprintf (filename, sizeof (filename),
                    "%s/%s_%05d.rle", directory, prefix, num);
          if (access (filename, F_OK) != 0)
            /* found an available filename */
            found = true;
        }

      if (!found)
        {
          D_ERROR ("DirectFBImageProvider_RLE:: "
                   "couldn't find an unused index for RLE dump!\n");
          return NULL;
        }
    }

  if (idx)
    *idx = num;

  return filename;
}
#endif /* ALLOW_DUMP_RAW_TO_FILE */

static DFBResult
Construct (IDirectFBImageProvider *thiz,
           ...)
{
  DFBResult ret;

  IDirectFBDataBuffer *buffer;
  CoreDFB             *core;
  va_list              tag;

  DIRECT_ALLOCATE_INTERFACE_DATA (thiz, IDirectFBImageProvider_RLE)

  va_start (tag, thiz);
  buffer = va_arg (tag, IDirectFBDataBuffer *);
  core = va_arg (tag, CoreDFB *);
  va_end (tag);

  data->common.base.ref    = 1;
  data->common.base.buffer = buffer;
  data->common.base.core   = core;

#if defined(RLE_PROVIDER_HW)
  if (dfb_system_get_accelerator () == FB_ACCEL_ST_BDISP_USER)
    {
      data->stgfx2_handle = dlopen ("libdirectfb_stgfx2.so", RTLD_LAZY);
      data->rle_stretch = dlsym (data->stgfx2_handle,
                                 "bdisp_aq_StretchBlit_RLE");
    }
#endif

  buffer->AddRef (buffer);

#ifdef ALLOW_DUMP_RAW_TO_FILE
  if (getenv ("RLE_DUMP_RAW_TO_FILE"))
  {
  const char * const fn = _get_filename ("/tmp/rle", "raw", &data->idx);
  if (fn)
    {
      unsigned char buf[1024];
      unsigned int len = sizeof (buf);

      int dump_fd = open (fn, O_RDWR | O_CREAT | O_TRUNC, 0644);
      if (dump_fd != -1)
        {
          DFBResult res;
          unsigned int ret_len = len;
          unsigned int total = 0;

          while (len)
            {
              res = buffer->WaitForData (buffer, len);
              if (res == DFB_OK)
                res = buffer->GetData (buffer, len, buf, &ret_len);

              if (res)
                break;

              write (dump_fd, buf, ret_len);

              len = ret_len;

              total += ret_len;
            }
          D_DEBUG_AT (HWRLE, "%s: wrote %d bytes to %d\n",
                      __FUNCTION__, total, data->idx);

          fsync (dump_fd);
          close (dump_fd);
          buffer->SeekTo (buffer, 0);
        }
    }
  sync ();
  sync ();
  sync ();
  }
#endif /* ALLOW_DUMP_RAW_TO_FILE */

  ret = rle_decode_header (data);
  if (ret)
    {
      IDirectFBImageProvider_RLE_Destruct (thiz);
      return ret;
    }

  data->common.base.Destruct = IDirectFBImageProvider_RLE_Destruct;

  thiz->RenderTo              = IDirectFBImageProvider_RLE_RenderTo;
  thiz->GetImageDescription   = IDirectFBImageProvider_RLE_GetImageDescription;
  thiz->GetSurfaceDescription = IDirectFBImageProvider_RLE_GetSurfaceDescription;

  return DFB_OK;
}
