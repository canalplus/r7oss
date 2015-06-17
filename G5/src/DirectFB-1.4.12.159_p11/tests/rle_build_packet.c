/*
 * Copyright (C) 2006 ST-Microelectronics R&D <alain.clement@st.com>
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

/*
 *              RLE PACKET HELPER LIBRARY
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "rle_build_packet.h"

/*******************************************************************************
 *
 *   rle_build_packet:
 *
 *             DirectFB static memory buffer setup for RLE Image Provider
 *
 *   RLE Image Provider expects its DirectFB buffer holding header data+colormap
 *   (RLE_OVERHEAD) and the RLE payload. This helper function sets up such a
 *   buffer given a minimal set of parameters. The application must account for
 *   RLE_HEADER_SIZE bytes at the beginning, then colormap bytes (variable,
 *   dependent on depth) then the payload. The application must supply the
 *   colormap but has the flexibility of submitting the payload data for copy
 *   in the allocated I/O buffer (non NULL payload parameter), or collecting the
 *   data from source ahead of time into the I/O buffer, thus avoiding a copy
 *   step, as long as the payload sections make room for RLE_OVERHEAD(depth)
 *   reserved bytes at the beginning of "buffer".
 *
 *   Note 1:The colormap is not required to be supplied. In such a case (NULL
 *          pointer), the colormap section gets skipped (absent from header).
 *
 *   Note 2:BMP/DIB row addressing normally starts from the bottom. A negative
 *          height means images start from the top of the bitmap (weirdness ?)
 *          "topfirstmode" flag is provided here to get a start from the top of
 *          the bitmap. This behavior is normally applicable to non RLE
 *          compressed images, but extended to any format in our RLE Image
 *          Provider implementation.
 *
 *     -------------------------------------------------------------------------
 *                             DirectFB buffer Mapping
 *     -------------------------------------------------------------------------
 *
 *         RLE_PREAMBLE
 *             (14)
 *     -------------------    RLE_HEADER
 *                             (54)
 *         RLE_BIHEADER
 *             (40)
 *     --------------------------------------    RLE_OVERHEAD
 *                                                 (54-310)
 *                         RLE_PALETTE
 *                             (0-256)
 *     ------------------------------------------------------  RLE_DFB_BUFFER
 *                                                              (Packet Size)
 *
 *
 *                                             RLE_PAYLOAD
 *
 *                      rawmode = Raw-Pixels (1) or Run-Length coded (0)
 *                      bdmode  = BD-RLE     (1) or Legacy-RLE coded (0)
 *
 *     -------------------------------------------------------------------------
 *
 *******************************************************************************/

DFBResult
rle_build_packet (
    u8        *buffer,       /*    I/O  :    Packet buffer            */
    u32        buffer_size,  /*    I    :    Packet size              */
    u32        width,        /*    I    :    Image width              */
    u32        height,       /*    I    :    Image height             */
    u32        depth,        /*    I    :    Pixels depth             */
    u32        rawmode,      /*    I    :    Raw mode flag      (0/1) */
    u32        bdmode,       /*    I    :    BD-RLE mode flag   (0/1) */
    u32        topfirstmode, /*    I    :    Topfirst mode flag (0/1) */
    DFBColor  *colormap,     /*    I    :    Palette / NULL           */
    u32        colormap_size,/*    I    :    Palette size             */
    void      *payload,      /*    I    :    Payload / NULL           */
    u32        payload_size  /*    I    :    Payload size             */
)
{
	/*
	 * RLE Compression identifier (see BMP/RLE header - Offset:30 --- DWORD)
	 */
	typedef enum {
	     RLEIC_NONE      = 0,   /* Implemented      as of version 1.0.0 */
	     RLEIC_RLE8      = 1,   /* Implemented      as of version 1.0.0 */
	     RLEIC_RLE4      = 2,   /* Unimplemented */
	     RLEIC_BITFIELDS = 3,   /* Unimplemented */
	     RLEIC_BD_RLE8   = 4    /* Implemented      as of version 1.1.0 */
	} RLEImageCompression;





     /* Little-endian portable data bytes order ... */
     #define RLE_HEADER_SET_MAGIC(ptr,l,h)       {      \
             ptr[0]=(u8)l;                              \
             ptr[1]=(u8)h;                              \
             }
     #define RLE_HEADER_WRITE_LE_16(ptr,data)    {      \
            *ptr++    =    (data>>0 ) & 0xff;           \
            *ptr++    =    (data>>8 ) & 0xff;           \
     }
     #define RLE_HEADER_WRITE_LE_32(ptr,data)    {      \
            *ptr++    =    (data>>0 ) & 0xff;           \
            *ptr++    =    (data>>8 ) & 0xff;           \
            *ptr++    =    (data>>16) & 0xff;           \
            *ptr++    =    (data>>24) & 0xff;           \
     }

     #define RLE_PALETTE_SIZE          (colormap_size & ~0x03)
     #define RLE_OVERHEAD              (RLE_HEADER_SIZE+RLE_PALETTE_SIZE)

     #define MIN(a,b)   (((a)<(b)) ? (a) : (b))

     DFBResult ret = DFB_OK;
     u8  *hptr;
     u32  bihsize          = RLE_BIHEADER_SIZE;
     u32  num_colors       = RLE_PALETTE_SIZE/4;
     u32  imp_colors       = RLE_PALETTE_SIZE/4;
     u32  img_offset       = RLE_OVERHEAD;
     int  payload_max_size = MIN(buffer_size-RLE_OVERHEAD,payload_size);
     int  height_dib       = topfirstmode ? -height : height;

     RLEImageCompression  compression;

     /* Check for any payload/buffer size discrepancy */
     if (payload_max_size<0) {
         return DFB_INVARG;
     }

     /* Switch compression mode */
     if (rawmode) {
         switch (depth) {
              case 16:
              case 24:
              case 32:
              case 8:
              case 4:
              case 1:
                  compression = RLEIC_NONE;
                  break;
              default:
                   return DFB_INVARG;
         }
     }
     else {
         switch (depth) {
              case 8:
                   if (!bdmode)     {
                       compression = RLEIC_RLE8;
                       break;
                   }
                   else             {
                       compression = RLEIC_BD_RLE8;
                       break;
                   }
              case 4:
                   if (!bdmode)     {
                       compression = RLEIC_RLE4;
                       break;
                   }
              case 1:
              case 16:
              case 24:
              case 32:
              default:
                   return DFB_INVARG;
         }
     }

     /* Cleanup preamble, header and eventual palette area */
     memset( buffer, 0,  img_offset);

     /* Pointer to buffer header start */
     hptr = buffer;

     /* Offset:00 --- 2 bytes: Magic - Zero for now */
     RLE_HEADER_WRITE_LE_16 (hptr, 0);

     /* Offset:02 --- 4 bytes: FileSize */
     RLE_HEADER_WRITE_LE_32 (hptr, buffer_size);

     /* Offset:06 --- 4 bytes: Reserved */
     RLE_HEADER_WRITE_LE_32 (hptr, 0);

     /* Offset:10 --- 4 bytes: DataOffset */
     RLE_HEADER_WRITE_LE_32 (hptr, img_offset);

     /* Offset:14 --- 4 bytes: HeaderSize */
     RLE_HEADER_WRITE_LE_32 (hptr, bihsize);

     /* Offset:18 --- 4 bytes: Width */
     RLE_HEADER_WRITE_LE_32 (hptr, width);

     /* Offset:22 --- 4 bytes: Height (negative value means top first mode) */
     RLE_HEADER_WRITE_LE_32 (hptr, height_dib);

     /* Offset:26 --- 2 bytes: Planes */
     RLE_HEADER_WRITE_LE_16 (hptr, 1);

     /* Offset:28 --- 2 bytes: Depth */
     RLE_HEADER_WRITE_LE_16 (hptr, depth);

     /* Offset:30 --- 4 bytes: Compression */
     RLE_HEADER_WRITE_LE_32 (hptr, compression);

     /* Offset:34 --- 4 bytes: CompressedSize */
     RLE_HEADER_WRITE_LE_32 (hptr, payload_max_size);

     /* Offset:38 --- 4 bytes: HorizontalResolution (don't care ...) */
     RLE_HEADER_WRITE_LE_32 (hptr, 0);

     /* Offset:42 --- 4 bytes: VerticalResolution (don't care ...) */
     RLE_HEADER_WRITE_LE_32 (hptr, 0);

     /* Offset:46 --- 4 bytes: UsedColors */
     RLE_HEADER_WRITE_LE_32 (hptr, num_colors);

     /* Offset:50 --- 4 bytes: ImportantColors (don't care ...) */
     RLE_HEADER_WRITE_LE_32 (hptr, imp_colors);

     /* Offset:54 --- 4 x num_colors bytes: Palette */
     if (colormap_size>0 && colormap) {
          int i;
          for (i = 0; i < num_colors; i++) {

               if (i>=colormap_size/4){
                    RLE_HEADER_WRITE_LE_32 (hptr, 0);
                    continue;
               }

               ((u8*)hptr)[i*4+3] = colormap[i].a;
               ((u8*)hptr)[i*4+2] = colormap[i].r;
               ((u8*)hptr)[i*4+1] = colormap[i].g;
               ((u8*)hptr)[i*4+0] = colormap[i].b;
          }
     }

     /* Offset:54 + 4 x num_colors ---  bytes: Payload */
     if (payload_max_size>0 && payload) {
         memcpy( buffer+img_offset, payload, payload_max_size);
     }

     /* Set first 2 bytes: Magic - valid header now */
     RLE_HEADER_SET_MAGIC(buffer,'R','L');

     return ret;
}




