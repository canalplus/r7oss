/*
   (c) Copyright 2006-2010  ST Microelectronics R&D

based on code:
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

#include <directfb.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/surface.h>

#include <misc/gfx_util.h>
#include <misc/util.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/thread.h>
#include <direct/messages.h>

#include <setjmp.h>
#include <math.h>

#undef HAVE_STDLIB_H
#include <jpeglib.h>

#if !defined(JPEG_PROVIDER_USE_MME)
#undef USE_MME
#endif


#include "debug_helper.h"

D_DEBUG_DOMAIN (JPEG,      "JPEG",      "STM JPEG decoder");
D_DEBUG_DOMAIN (JPEG_RAW,  "JPEG/raw",  "STM JPEG decoder (raw decode)");
D_DEBUG_DOMAIN (JPEG_MME,  "JPEG/MME",  "STM JPEG decoder (MME)");
D_DEBUG_DOMAIN (JPEG_SEMA, "JPEG/SEMA", "STM JPEG decoder (semaphores");
D_DEBUG_DOMAIN (JPEG_TIME, "JPEG/Time", "STM JPEG decoder (timing");
#define MME_DEBUG_DOMAIN  JPEG_MME
#define SEMA_DEBUG_DOMAIN JPEG_SEMA
#define MME_TEXT_DOMAIN "JPEG"

#include "mme_helper.h"
#include "idirectfbimageprovider_jpeg.h"
#if defined(JPEG_PROVIDER_USE_MME)
#include "sema_helper.h"
static DFBResult JPEG_HardwareRenderTo( IDirectFBImageProvider_JPEG_data *data,
                                        CoreSurface                      *dst_surface,
                                        DFBSurfacePixelFormat             format,
                                        DFBRectangle                     *rect,
                                        const DFBRegion                  *clip );
#else /* JPEG_PROVIDER_USE_MME */
#define JPEG_HardwareRenderTo(data,dst_surface, \
                              format,rect,clip) DFB_NOSUCHINSTANCE
#endif /* JPEG_PROVIDER_USE_MME */


static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... );

#include <direct/interface_implementation.h>

#if defined(JPEG_PROVIDER_USE_MME)
DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, hwJPEG )
#else /* JPEG_PROVIDER_USE_MME */
DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, swJPEG )
#endif /* JPEG_PROVIDER_USE_MME */


static DFBResult
IDirectFBImageProvider_JPEG_SetFlags ( IDirectFBImageProvider *thiz,
                                       DFBImageProviderFlags   flags );

static DFBResult
IDirectFBImageProvider_JPEG_RenderTo( IDirectFBImageProvider *thiz,
                                      IDirectFBSurface       *destination,
                                      const DFBRectangle     *destination_rect );

static DFBResult
IDirectFBImageProvider_JPEG_Sync( IDirectFBImageProvider    *thiz,
                                  DFBImageProviderSyncFlags  flags );

static DFBResult
IDirectFBImageProvider_JPEG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                   DFBSurfaceDescription  *dsc);

static DFBResult
IDirectFBImageProvider_JPEG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                 DFBImageDescription    *dsc );

static void *
JPEGrenderThread( DirectThread *thread, void *driver_data );


#define JPEG_PROG_BUF_SIZE    0x10000

typedef struct {
     struct jpeg_source_mgr  pub; /* public fields */

     JOCTET                 *data;       /* start of buffer */

     IDirectFBDataBuffer    *buffer;

     int                     peekonly;
     int                     peekoffset;
} buffer_source_mgr;

typedef buffer_source_mgr * buffer_src_ptr;

static void
buffer_init_source (j_decompress_ptr cinfo)
{
     buffer_src_ptr src          = (buffer_src_ptr) cinfo->src;
     IDirectFBDataBuffer *buffer = src->buffer;

     buffer->SeekTo( buffer, 0 ); /* ignore return value */
}

static boolean
buffer_fill_input_buffer (j_decompress_ptr cinfo)
{
     DFBResult            ret;
     unsigned int         nbytes = 0;
     buffer_src_ptr       src    = (buffer_src_ptr) cinfo->src;
     IDirectFBDataBuffer *buffer = src->buffer;

     buffer->WaitForDataWithTimeout( buffer, JPEG_PROG_BUF_SIZE, 1, 0 );

     if (src->peekonly) {
          ret = buffer->PeekData( buffer, JPEG_PROG_BUF_SIZE,
                                  src->peekoffset, src->data, &nbytes );
          src->peekoffset += MAX( nbytes, 0 );
     }
     else {
          ret = buffer->GetData( buffer, JPEG_PROG_BUF_SIZE, src->data, &nbytes );
     }

     if (ret || nbytes <= 0) {
          /* Insert a fake EOI marker */
          src->data[0] = (JOCTET) 0xFF;
          src->data[1] = (JOCTET) JPEG_EOI;
          nbytes = 2;

          if (ret && ret != DFB_EOF)
               DirectFBError( "(DirectFB/ImageProvider_JPEG) GetData failed", ret );
     }

     src->pub.next_input_byte = src->data;
     src->pub.bytes_in_buffer = nbytes;

     return TRUE;
}

static void
buffer_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
     buffer_src_ptr src = (buffer_src_ptr) cinfo->src;

     if (num_bytes > 0) {
          while (num_bytes > (long) src->pub.bytes_in_buffer) {
               num_bytes -= (long) src->pub.bytes_in_buffer;
               (void)buffer_fill_input_buffer(cinfo);
          }
          src->pub.next_input_byte += (size_t) num_bytes;
          src->pub.bytes_in_buffer -= (size_t) num_bytes;
     }
}

static void
buffer_term_source (j_decompress_ptr cinfo)
{
}

static void
jpeg_buffer_src (j_decompress_ptr cinfo, IDirectFBDataBuffer *buffer, int peekonly)
{
     buffer_src_ptr src;

     cinfo->src = (struct jpeg_source_mgr *)
                  cinfo->mem->alloc_small ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                           sizeof (buffer_source_mgr));

     src = (buffer_src_ptr) cinfo->src;

     src->data = (JOCTET *)
                  cinfo->mem->alloc_small ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                           JPEG_PROG_BUF_SIZE * sizeof (JOCTET));

     src->buffer = buffer;
     src->peekonly = peekonly;
     src->peekoffset = 0;

     src->pub.init_source       = buffer_init_source;
     src->pub.fill_input_buffer = buffer_fill_input_buffer;
     src->pub.skip_input_data   = buffer_skip_input_data;
     src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
     src->pub.term_source       = buffer_term_source;
     src->pub.bytes_in_buffer   = 0; /* forces fill_input_buffer on first read */
     src->pub.next_input_byte   = NULL; /* until buffer loaded */
}

struct my_error_mgr {
     struct jpeg_error_mgr pub;     /* "public" fields */
     jmp_buf  setjmp_buffer;          /* for return to caller */
};

static void
jpeglib_panic(j_common_ptr cinfo)
{
     struct my_error_mgr *myerr = (struct my_error_mgr*) cinfo->err;
     longjmp(myerr->setjmp_buffer, 1);
}

static inline void
copy_line32( u32 *argb, const u8 *rgb, int width )
{
     while (width--) {
          *argb++ = 0xFF000000 | (rgb[0] << 16) | (rgb[1] << 8) | rgb[2];

          rgb += 3;
     }
}

static inline void
copy_line_nv16( u16 *yy, u16 *cbcr, const u8 *src_ycbcr, int width )
{
     int x;

     for (x=0; x<width/2; x++) {
#ifdef WORDS_BIGENDIAN
          yy[x] = (src_ycbcr[0] << 8) | src_ycbcr[3];

          cbcr[x] = (((src_ycbcr[1] + src_ycbcr[4]) << 7) & 0xff00) |
                     ((src_ycbcr[2] + src_ycbcr[5]) >> 1);
#else
          yy[x] = (src_ycbcr[3] << 8) | src_ycbcr[0];

          cbcr[x] = (((src_ycbcr[2] + src_ycbcr[5]) << 7) & 0xff00) |
                     ((src_ycbcr[1] + src_ycbcr[4]) >> 1);
#endif

          src_ycbcr += 6;
     }

     if (width & 1) {
          u8 *y = (u8*) yy;

          y[width-1] = src_ycbcr[0];

#ifdef WORDS_BIGENDIAN
          cbcr[x] = (src_ycbcr[1] << 8) | src_ycbcr[2];
#else
          cbcr[x] = (src_ycbcr[2] << 8) | src_ycbcr[1];
#endif
     }
}


static void
IDirectFBImageProvider_JPEG_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_JPEG_data *data =
                              (IDirectFBImageProvider_JPEG_data*)thiz->priv;

     if (data->thread) {
          /* terminate the decoding thread, if necessary... */
          direct_thread_cancel( data->thread );
          direct_thread_join( data->thread );
          direct_thread_destroy( data->thread );

          pthread_mutex_destroy( &data->lock );
          pthread_cond_destroy( &data->cond );
     }

     if (data->common.image)
          D_FREE( data->common.image );

     if (data->common.decode_surface)
          dfb_surface_unref( data->common.decode_surface );
}

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (ctx->header[0] == 0xff && ctx->header[1] == 0xd8) {
          if (strncmp ((char*) ctx->header + 6, "JFIF", 4) == 0 ||
              strncmp ((char*) ctx->header + 6, "Exif", 4) == 0)
               return DFB_OK;

          if (ctx->filename && strchr (ctx->filename, '.' ) &&
             (strcasecmp ( strchr (ctx->filename, '.' ), ".jpg" ) == 0 ||
              strcasecmp ( strchr (ctx->filename, '.' ), ".jpeg") == 0))
               return DFB_OK;
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... )
{
     struct jpeg_decompress_struct cinfo;
     struct my_error_mgr jerr;

     IDirectFBDataBuffer *buffer;
     CoreDFB             *core;
     va_list              tag;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_JPEG)

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );

     data->common.base.ref    = 1;
     data->common.base.buffer = buffer;
     data->common.base.core   = core;

     buffer->AddRef( buffer );

     cinfo.err = jpeg_std_error(&jerr.pub);
     jerr.pub.error_exit = jpeglib_panic;

     if (setjmp(jerr.setjmp_buffer)) {
          D_ERROR( "ImageProvider/JPEG: Error while reading headers!\n" );

          jpeg_destroy_decompress(&cinfo);
          buffer->Release( buffer );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     jpeg_create_decompress(&cinfo);
     jpeg_buffer_src(&cinfo, buffer, 1);
     jpeg_read_header(&cinfo, TRUE);
     jpeg_start_decompress(&cinfo);

     data->common.width = cinfo.output_width;
     data->common.height = cinfo.output_height;

#if defined(JPEG_PROVIDER_USE_MME)
     data->num_components = cinfo.num_components;
     data->progressive_mode = cinfo.progressive_mode;
#endif

     jpeg_abort_decompress(&cinfo);
     jpeg_destroy_decompress(&cinfo);

     data->common.base.Destruct = IDirectFBImageProvider_JPEG_Destruct;

     thiz->SetFlags = IDirectFBImageProvider_JPEG_SetFlags;
     thiz->RenderTo = IDirectFBImageProvider_JPEG_RenderTo;
     thiz->Sync = IDirectFBImageProvider_JPEG_Sync;
     thiz->GetImageDescription =IDirectFBImageProvider_JPEG_GetImageDescription;
     thiz->GetSurfaceDescription =
     IDirectFBImageProvider_JPEG_GetSurfaceDescription;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_SetFlags( IDirectFBImageProvider *thiz,
                                      DFBImageProviderFlags   flags )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     /* if we have decoded the image already, don't do anything... */
     if (data->common.image
         || data->common.decode_surface
        )
          return DFB_UNSUPPORTED;

     if (flags == DIPFLAGS_NONE && data->thread) {
          /* terminate the decoding thread, if necessary... */
          direct_thread_cancel( data->thread );
          direct_thread_join( data->thread );
          direct_thread_destroy( data->thread );
          data->thread = NULL;

          pthread_cond_destroy( &data->cond );
          pthread_mutex_destroy( &data->lock );
     }
     else if (flags == DIPFLAGS_BACKGROUND_DECODE && !data->thread) {
          /* or create it */
          pthread_cond_init( &data->cond, NULL );
          pthread_mutex_init( &data->lock, NULL );
          /* as long as we haven't even started yet, we are in INIT state */
          data->thread_res = DFB_INIT;
          data->thread = direct_thread_create( DTT_DEFAULT, JPEGrenderThread,
                                               thiz, "JPEG_DECODER?" );
     }
     data->flags = flags;

     return DFB_OK;
}

static void
JPEG_stretchblit (CardState    *state,
                  DFBRectangle *src_rect,
                  DFBRectangle *dst_rect)
{
  D_DEBUG_AT (JPEG, "StretchBlit %dx%d -> %dx%d (%s -> %s)\n",
              src_rect->w, src_rect->h, dst_rect->w, dst_rect->h,
              dfb_pixelformat_name (state->source->config.format),
              dfb_pixelformat_name (state->destination->config.format));

  /* thankfully this is intelligent enough to do a simple blit if possible */
  dfb_gfxcard_stretchblit (src_rect, dst_rect, state);
}

#define _ROUND_UP_8(x)  (((x)+ 7) & ~ 7)
#define _ROUND_UP_16(x) (((x)+15) & ~15)
static void
JPEG_setup_yuv444p( const struct jpeg_decompress_struct *cinfo,
                    CoreSurfaceConfig                   *config,
                    int                                 *pitch_y,
                    int                                 *pitch_cb,
                    int                                 *pitch_cr,
                    int                                 *offset_cb,
                    int                                 *offset_cr )
{
     config->format = DSPF_YUV444P;

     config->size.w = _ROUND_UP_8 (cinfo->cur_comp_info[0]
                                                      ->downsampled_width);
     config->size.h = _ROUND_UP_8 (cinfo->cur_comp_info[0]
                                                     ->downsampled_height);

     *pitch_y  = config->size.w;
     *pitch_cb = _ROUND_UP_8 (cinfo->cur_comp_info[1]->downsampled_width);
     *pitch_cr = _ROUND_UP_8 (cinfo->cur_comp_info[2]->downsampled_width);

     *offset_cb
          = *offset_cr
          = *pitch_y * config->size.h;
}

static DFBResult
JPEG_raw_decode( IDirectFBImageProvider_JPEG_data *data,
                 CoreSurface                      *dst_surface,
                 DFBRectangle                     *rect,
                 struct jpeg_decompress_struct    *cinfo,
                 CardState                        *state,
                 CoreSurfaceBufferLock            *lock )
{
     /* can we do a raw decode? */
     if (cinfo->num_components != 3
         || cinfo->jpeg_color_space != JCS_YCbCr
         || cinfo->data_precision != 8)
          return DFB_UNSUPPORTED;

     CoreSurfaceConfig config;
     int pitch_y, pitch_cb, pitch_cr;
     int offset_cb; /* cb from y */
     int offset_cr; /* cr from cb */
     int y_v_sampl = 1;
     int cb_v_sampl = 1;
     int cr_v_sampl = 1;

     const int v_samp[3] = { cinfo->cur_comp_info[0]->v_samp_factor,
                             cinfo->cur_comp_info[1]->v_samp_factor,
                             cinfo->cur_comp_info[2]->v_samp_factor };

     /* Worst case, is 4x4 chroma subsampling where an MCU is 32 lines. libjpeg
        won't decode more than one MCU in any one go. */
#undef _NO_STACK
#ifndef _NO_STACK
     JSAMPROW Yrows [cinfo->max_v_samp_factor * DCTSIZE];
     JSAMPROW Cbrows[cinfo->max_v_samp_factor * DCTSIZE];
     JSAMPROW Crrows[cinfo->max_v_samp_factor * DCTSIZE];
     JSAMPARRAY jpeg_buffer[3] = {
          [0] = Yrows,
          [1] = Cbrows,
          [2] = Crrows,
     };
#else
     /* valgrind can't check stack variables */
#warning this is not for production use, as we do not clean up on errors
     JSAMPROW *Yrows, *Cbrows, *Crrows;
     JSAMPARRAY *jpeg_buffer;

     /* over allocate, least trouble */
     Yrows = malloc (sizeof (JSAMPROW) * cinfo->max_v_samp_factor * DCTSIZE);
     Cbrows = malloc (sizeof (JSAMPROW) * cinfo->max_v_samp_factor * DCTSIZE);
     Crrows = malloc (sizeof (JSAMPROW) * cinfo->max_v_samp_factor * DCTSIZE);

     jpeg_buffer = malloc (3 * sizeof (JSAMPARRAY));
     jpeg_buffer[0] = Yrows;
     jpeg_buffer[1] = Cbrows;
     jpeg_buffer[2] = Crrows;
#endif
     void *yaddr, *cbaddr, *craddr;
     unsigned int i;

     DIRenderCallbackResult cb_result = DIRCR_OK;

     D_DEBUG_AT (JPEG_RAW, "trying raw decode\n");

     if (cinfo->max_h_samp_factor == 1
         && cinfo->max_v_samp_factor == 1
         && cinfo->cur_comp_info[0]->h_samp_factor == 1
         && v_samp[0] == 1
         && cinfo->cur_comp_info[1]->h_samp_factor == 1
         && v_samp[1] == 1
         && cinfo->cur_comp_info[2]->h_samp_factor == 1
         && v_samp[2] == 1) {
          /* The image is YCbCr 4:4:4 */
          D_DEBUG_AT (JPEG_RAW, "  -> 4:4:4 image\n");

          JPEG_setup_yuv444p (cinfo, &config, &pitch_y, &pitch_cb, &pitch_cr,
                              &offset_cb, &offset_cr);
     }
     else if (cinfo->max_h_samp_factor == 2
              && cinfo->max_v_samp_factor == 2
              && cinfo->cur_comp_info[0]->h_samp_factor == 2
              && v_samp[0] == 2
              && cinfo->cur_comp_info[1]->h_samp_factor == 1
              && v_samp[1] == 1
              && cinfo->cur_comp_info[2]->h_samp_factor == 1
              && v_samp[2] == 1) {
          /* The image is YCbCr 4:2:0 ... */
          D_DEBUG_AT (JPEG_RAW, "  -> 4:2:0 (I420) image\n");

          if ((cinfo->scale_num != cinfo->scale_denom)
              && (cinfo->cur_comp_info[1]->downsampled_width
                  == cinfo->cur_comp_info[0]->downsampled_width)
              && (cinfo->cur_comp_info[2]->downsampled_width
                  == cinfo->cur_comp_info[0]->downsampled_width)) {
               /* ... but because we are iDCT scaling during decompression it
                  might end up scaled to 4:4:4, as libjpeg can (and will) apply
                  different scaling factors for the three components. */
               D_DEBUG_AT (JPEG_RAW, "    -> iDCT %u/%u scaled to 4:4:4\n",
                           cinfo->scale_num, cinfo->scale_denom);

               JPEG_setup_yuv444p (cinfo, &config, &pitch_y, &pitch_cb, &pitch_cr,
                                   &offset_cb, &offset_cr);
          }
          else if ((cinfo->scale_num == cinfo->scale_denom)
                   || ((cinfo->cur_comp_info[1]->downsampled_width
                        == ((cinfo->cur_comp_info[0]->downsampled_width + 1)
                            / 2))
                       && (cinfo->cur_comp_info[2]->downsampled_width
                           == ((cinfo->cur_comp_info[0]->downsampled_width + 1)
                               / 2)))) {
               /* ... no iDCT scaling, which means it really ends up
                  as 4:2:0. */
               config.format = DSPF_I420;

               config.size.w = _ROUND_UP_16 (cinfo->cur_comp_info[0]
                                                           ->downsampled_width);
               config.size.h = _ROUND_UP_16 (cinfo->cur_comp_info[0]
                                                          ->downsampled_height);

               pitch_y = config.size.w;
               pitch_cb
                    = pitch_cr
                    = pitch_y / 2;

               offset_cb = pitch_y  * config.size.h;
               offset_cr = pitch_cb * config.size.h / 2;

               cb_v_sampl
                    = cr_v_sampl
                    = 2;
          }
          else {
               /* shouldn't happen, really */
               D_DEBUG_AT (JPEG_RAW, "  -> unsupported (unexpected)!\n");
               return DFB_UNSUPPORTED;
          }
     }
     else if (cinfo->max_h_samp_factor == 2
              && cinfo->max_v_samp_factor == 1
              && cinfo->cur_comp_info[0]->h_samp_factor == 2
              && v_samp[0] == 1
              && cinfo->cur_comp_info[1]->h_samp_factor == 1
              && v_samp[1] == 1
              && cinfo->cur_comp_info[2]->h_samp_factor == 1
              && v_samp[2] == 1) {
          /* The image is YCbCr 4:2:2 ... */
          D_DEBUG_AT (JPEG_RAW, "  -> 4:2:2 (YV16) image\n");

          if ((cinfo->scale_num != cinfo->scale_denom)
              && (cinfo->cur_comp_info[1]->downsampled_width
                  == cinfo->cur_comp_info[0]->downsampled_width)
              && (cinfo->cur_comp_info[2]->downsampled_width
                  == cinfo->cur_comp_info[0]->downsampled_width)) {
               /* ... iDCT scaling again, as above. */
               D_DEBUG_AT (JPEG_RAW, "    -> iDCT %u/%u scaled to 4:4:4\n",
                           cinfo->scale_num, cinfo->scale_denom);

               JPEG_setup_yuv444p (cinfo, &config, &pitch_y, &pitch_cb, &pitch_cr,
                                   &offset_cb, &offset_cr);
          }
          else if ((cinfo->scale_num == cinfo->scale_denom)
                   || ((cinfo->cur_comp_info[1]->downsampled_width
                        == ((cinfo->cur_comp_info[0]->downsampled_width + 1)
                            / 2))
                       && (cinfo->cur_comp_info[2]->downsampled_width
                           == ((cinfo->cur_comp_info[0]->downsampled_width + 1)
                               / 2)))) {
               /* ... no iDCT scaling, as above. */
               config.format = DSPF_YV16;

               config.size.w = _ROUND_UP_16 (cinfo->cur_comp_info[0]
                                                           ->downsampled_width);
               config.size.h = _ROUND_UP_16 (cinfo->cur_comp_info[0]
                                                          ->downsampled_height);

               pitch_y = config.size.w;
               pitch_cb
                    = pitch_cr
                    = pitch_y / 2;

               /* YV16 has Cr/Cb planes not Cb/Cr */
               offset_cb = (pitch_y + pitch_cr) * config.size.h;
               offset_cr = -pitch_cr * config.size.h;
          }
          else {
               /* shouldn't happen, really */
               D_DEBUG_AT (JPEG_RAW, "  -> unsupported (unexpected)!\n");
               return DFB_UNSUPPORTED;
          }
     }
     else {
          D_DEBUG_AT (JPEG_RAW, "  -> unsupported!\n");
          return DFB_UNSUPPORTED;
     }

     /* yes, we can handle this raw format! */

     config.flags = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
     #ifdef __SH4__
     config.caps = DSCAPS_VIDEOONLY;
     #else
     config.caps = 0;
     #endif
     #ifdef DIRECT_BUILD_DEBUG
     config.caps |= DSCAPS_SHARED;
     #endif
     if (dfb_surface_create (data->common.base.core, &config, CSTF_NONE, 0, NULL,
                             &data->common.decode_surface)) {
          D_ERROR ("failed to create temporary decode surface\n");
          return DFB_UNSUPPORTED;
     }

     if (dfb_surface_lock_buffer (data->common.decode_surface,
                                  CSBR_BACK, CSAID_CPU, CSAF_WRITE, lock)) {
          dfb_surface_unref (data->common.decode_surface);
          data->common.decode_surface = NULL;
          return DFB_UNSUPPORTED;
     }

     cinfo->raw_data_out = true;
     jpeg_start_decompress (cinfo);

     dfb_state_set_source (state, data->common.decode_surface);

     /* Initialize the various pointers to build a planar YUV buffer. */
     yaddr  = lock->addr;
     cbaddr = yaddr  + offset_cb;
     craddr = cbaddr + offset_cr;

     /* init. */
     for (i = 0; i < cinfo->max_v_samp_factor * DCTSIZE; ++i) {
          Yrows[i] = yaddr + i * pitch_y;
          Cbrows[i] = cbaddr + i * pitch_cb;
          Crrows[i] = craddr + i * pitch_cr;
     }

     while (cinfo->output_scanline < data->common.decoded_height
            && cb_result == DIRCR_OK) {
          int x = jpeg_read_raw_data (cinfo, jpeg_buffer,
                                      cinfo->max_v_samp_factor * DCTSIZE);
          if (x <= 0)
               /* Actually, x == 0 means that we don't have enough data to
                  continue decoding the picture. */
               break;

          D_DEBUG_AT (JPEG_RAW, "  -> decoded %d scanlines (out of %d (%d))\n",
                      cinfo->output_scanline, data->common.decoded_height,
                      config.size.h);

          /* increment the pointers by the number of decoded (luma)
             scanlines. */
          for (i = 0; i < cinfo->max_v_samp_factor * DCTSIZE; ++i) {
               Yrows[i] += (x * pitch_y) / y_v_sampl;
               Cbrows[i] += (x * pitch_cb) / cb_v_sampl;
               Crrows[i] += (x * pitch_cr) / cr_v_sampl;
          }

          if (data->common.base.render_callback) {
               DFBRectangle src_rect = {
                    .x = 0,
                    .y = cinfo->output_scanline - x,
                    .w = data->common.decoded_width,
                    .h = x,
               };
               DFBRectangle r = src_rect;
               float factor = (rect->h / (float) data->common.decoded_height);
               DFBRectangle dst_rect = {
                    .x = rect->x,
                    .y = (int) (src_rect.y * factor),
                    .w = rect->w,
                    .h = (int) (src_rect.h * factor),
               };

               D_DEBUG_AT (JPEG_RAW,
                           "  -> render callback %d,%d %dx%d -> %d,%d %dx%d\n",
                           src_rect.x, src_rect.y, src_rect.w,
                           src_rect.h, dst_rect.x, dst_rect.y,
                           dst_rect.w, dst_rect.h);

               JPEG_stretchblit (state, &src_rect, &dst_rect);

               cb_result = data->common.base.render_callback (&r,
                                                              data->common.base.render_callback_context);
          }
     }

     D_DEBUG_AT (JPEG_RAW, "  -> decoded %d scanlines (out of %d)\n",
                 cinfo->output_scanline, data->common.decoded_height);

     if (cinfo->output_scanline < data->common.decoded_height
         || cb_result != DIRCR_OK) {
          if (cb_result != DIRCR_OK)
               D_ERROR ("raw decode failed after %d of %d scanlines, "
                        "trying pure software\n",
                        cinfo->output_scanline, cinfo->output_height);
          jpeg_abort_decompress (cinfo);
          jpeg_destroy_decompress (cinfo);
          dfb_surface_unlock_buffer (data->common.decode_surface, lock);
          lock->pitch = 0;
          dfb_surface_unref (data->common.decode_surface);
          data->common.decode_surface = NULL;

          if (cb_result != DIRCR_OK)
               return DFB_INTERRUPTED;

          return DFB_FAILURE; /* restart */
     }

     jpeg_finish_decompress (cinfo);
     jpeg_destroy_decompress (cinfo);

     dfb_surface_unlock_buffer (data->common.decode_surface, lock);
     lock->pitch = 0;

     /* use DFB to convert raw YUV to destination format, and apply any
        necessary additional clip/stretch */
     {
     DFBRectangle src_rect = {
          .x = 0,
          .y = 0,
          .w = data->common.decoded_width,
          .h = data->common.decoded_height
     };

     JPEG_stretchblit (state, &src_rect, rect);

     if (data->common.base.render_callback)
          data->common.base.render_callback (&src_rect,
                                             data->common.base.render_callback_context);
     }

#ifdef _NO_STACK
     free (Yrows);
     free (Cbrows);
     free (Crrows);
     free (jpeg_buffer);
#endif
     return DFB_OK;
}

static DFBResult
JPEG_SoftwareRenderTo( IDirectFBImageProvider_JPEG_data *data,
                       CoreSurface                      *dst_surface,
                       DFBSurfacePixelFormat             format,
                       DFBRectangle                     *rect,
                       const DFBRegion                  *clip )
{
     DFBResult              ret;
     bool                   direct = false;
     CoreSurfaceBufferLock  lock = { .pitch = 0 };
     DIRenderCallbackResult cb_result = DIRCR_OK;
     bool                   try_raw = true;

     D_ASSERT( data != NULL );
     D_ASSERT( dst_surface != NULL );
     D_ASSERT( clip != NULL );
     D_ASSERT( rect != NULL );

     if (data->common.image &&
         (rect->x || rect->y || rect->w != data->common.decoded_width || rect->h != data->common.decoded_height)) {
           D_FREE( data->common.image );
           data->common.image          = NULL;
           data->common.decoded_width  = 0;
           data->common.decoded_height = 0;
     }

     /* actual loading and rendering */
     if (!data->common.image
         && !data->common.decode_surface) {
          struct jpeg_decompress_struct cinfo;
          struct my_error_mgr jerr;
          JSAMPARRAY buffer;      /* Output row buffer */
          int row_stride;         /* physical row width in output buffer */
          u32 *row_ptr;
          int y = 0;
          int uv_offset = 0;
          CardState state;
          CoreSurfaceBufferLock lock_raw = { .pitch = 0 };

          cinfo.err = jpeg_std_error(&jerr.pub);
          jerr.pub.error_exit = jpeglib_panic;

          if (setjmp(jerr.setjmp_buffer)) {
               D_ERROR( "ImageProvider/JPEG: Error during decoding!\n" );

               jpeg_destroy_decompress(&cinfo);

               if (try_raw) {
                    dfb_state_set_source (&state, NULL);
                    dfb_state_set_destination (&state, NULL);
                    dfb_state_destroy (&state);
               }
               if (data->common.decode_surface) {
                    if (lock_raw.pitch)
                         dfb_surface_unlock_buffer (data->common.decode_surface,
                                                    &lock_raw);
                    dfb_surface_unref (data->common.decode_surface);
                    data->common.decode_surface = NULL;
               }

               if (data->common.image) {
                    dfb_scale_linear_32( data->common.image, data->common.decoded_width, data->common.decoded_height,
                                         lock.addr, lock.pitch, rect, dst_surface, clip );
                    if (lock.pitch)
                         dfb_surface_unlock_buffer( dst_surface, &lock );
                    if (data->common.base.render_callback) {
                         DFBRectangle r = { 0, 0, data->common.decoded_width, data->common.decoded_height };

                         if (data->common.base.render_callback( &r, data->common.base.render_callback_context ) != DIRCR_OK)
                              return DFB_INTERRUPTED;
                    }

                    return DFB_INCOMPLETE;
               }
               else if (lock.pitch)
                    dfb_surface_unlock_buffer( dst_surface, &lock );

               return DFB_FAILURE;
          }

restart:
          jpeg_create_decompress(&cinfo);
          jpeg_buffer_src(&cinfo, data->common.base.buffer, 0);
          jpeg_read_header(&cinfo, TRUE);

#if JPEG_LIB_VERSION >= 70
          cinfo.scale_num = 8;
          cinfo.scale_denom = 8;
#else
          cinfo.scale_num = 1;
          cinfo.scale_denom = 1;
#endif
          jpeg_calc_output_dimensions(&cinfo);

          if (rect->x == 0 && rect->y == 0) {
#if JPEG_LIB_VERSION >= 70
               cinfo.scale_num = 8;
               while (cinfo.scale_num > 1) {
                    jpeg_calc_output_dimensions (&cinfo);
                    if (cinfo.output_width <= rect->w
                        || cinfo.output_height <= rect->h)
                         break;
                    --cinfo.scale_num;
               }
               jpeg_calc_output_dimensions (&cinfo);
#else
               while (cinfo.scale_denom < 8
                      && ((cinfo.output_width >> 1) >= rect->w)
                      && ((cinfo.output_height >> 1) >= rect->h)) {
                    cinfo.scale_denom <<= 1;
                    jpeg_calc_output_dimensions (&cinfo);
               }
#endif
          }

          cinfo.output_components = 3;

          data->common.decoded_width = cinfo.output_width;
          data->common.decoded_height = cinfo.output_height;

          cinfo.do_fancy_upsampling = FALSE;
          cinfo.do_block_smoothing = FALSE;

          if (try_raw) {
               /* init a state, so that we can use gfxcard/blit to convert YUV to
                  requested destination format */
               dfb_state_init (&state, data->common.base.core);
               dfb_state_set_destination (&state, dst_surface);
               dfb_state_set_clip (&state, clip);

               ret = JPEG_raw_decode (data, dst_surface, rect, &cinfo,
                                      &state, &lock_raw);

               /* remove the state */
               dfb_state_set_source (&state, NULL);
               dfb_state_set_destination (&state, NULL);
               dfb_state_destroy (&state);

               switch (ret) {
                    case DFB_OK:
                    case DFB_INTERRUPTED:
                         /* All ok, or callback signalled abort. */
                         return ret;

                    case DFB_UNSUPPORTED:
                         /* Not enough video memory or source image format not
                            supported. jpeg_start_decompress() was not yet
                            called, so we can just break. */
                         break;

                    default:
                         D_BUG ("JPEG_raw_decode() returned unknown "
                                "result %d\n", ret);
                    case DFB_FAILURE:
                         /* General failure during compression, we restart
                            trying a software decoding and in for that have to
                            reinitialise cinfo. */
                         try_raw = false;
                         goto restart;
               }
          }


          /* start using the full software decode path */
          D_WARN ("slow JPEG decode path");

          ret = dfb_surface_lock_buffer( dst_surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
          if (ret) {
               jpeg_abort_decompress(&cinfo);
               jpeg_destroy_decompress(&cinfo);
               return ret;
          }

          if (cinfo.output_width == rect->w && cinfo.output_height == rect->h)
               direct = true;

          switch (dst_surface->config.format) {
               case DSPF_NV16:
                    uv_offset = dst_surface->config.size.h * lock.pitch;

                    if (direct && !rect->x && !rect->y) {
                         D_INFO( "JPEG: Using YCbCr color space directly! (%dx%d)\n",
                                 cinfo.output_width, cinfo.output_height );
                         cinfo.out_color_space = JCS_YCbCr;
                         break;
                    }
                    D_INFO( "JPEG: Going through RGB color space! (%dx%d -> %dx%d @%d,%d)\n",
                            cinfo.output_width, cinfo.output_height, rect->w, rect->h, rect->x, rect->y );

               default:
                    cinfo.out_color_space = JCS_RGB;
                    break;
          }

          jpeg_start_decompress(&cinfo);

          row_stride = cinfo.output_width * 3;

          buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo,
                                              JPOOL_IMAGE, row_stride, 1);

          data->common.image = D_CALLOC( data->common.decoded_height, data->common.decoded_width * 4 );
          if (!data->common.image) {
               dfb_surface_unlock_buffer( dst_surface, &lock );
               jpeg_abort_decompress(&cinfo);
               jpeg_destroy_decompress(&cinfo);
               return D_OOM();
          }
          row_ptr = data->common.image;

          while (cinfo.output_scanline < cinfo.output_height && cb_result == DIRCR_OK) {
               jpeg_read_scanlines(&cinfo, buffer, 1);

               switch (dst_surface->config.format) {
                    case DSPF_NV16:
                         if (direct) {
                              copy_line_nv16( lock.addr, lock.addr + uv_offset, *buffer, rect->w );

                              lock.addr += lock.pitch;

                              if (data->common.base.render_callback) {
                                   DFBRectangle r = { 0, y, data->common.width, 1 };

                                   cb_result = data->common.base.render_callback( &r,
                                                                                  data->common.base.render_callback_context );
                              }
                              break;
                         }

                    default:
                         copy_line32( row_ptr, *buffer, data->common.decoded_width );

                         if (direct) {
                              DFBRectangle r = { rect->x, rect->y+y, rect->w, 1 };

                              dfb_copy_buffer_32( row_ptr, lock.addr, lock.pitch,
                                                  &r, dst_surface, clip );
                              if (data->common.base.render_callback) {
                                   r = (DFBRectangle){ 0, y, data->common.decoded_width, 1 };
                                   cb_result = data->common.base.render_callback( &r,
                                                                                  data->common.base.render_callback_context );
                              }
                         }
                         break;
               }

               row_ptr += data->common.width;
               y++;
          }

          if (!direct) {
               dfb_scale_linear_32( data->common.image, data->common.decoded_width, data->common.decoded_height,
                                    lock.addr, lock.pitch, rect, dst_surface, clip );
               if (data->common.base.render_callback) {
                    DFBRectangle r = { 0, 0, data->common.decoded_width, data->common.decoded_height };
                    cb_result = data->common.base.render_callback( &r, data->common.base.render_callback_context );
               }
          }

          if (cb_result != DIRCR_OK) {
               jpeg_abort_decompress(&cinfo);
               D_FREE( data->common.image );
               data->common.image = NULL;
          }
          else {
               jpeg_finish_decompress(&cinfo);
          }
          jpeg_destroy_decompress(&cinfo);

          dfb_surface_unlock_buffer( dst_surface, &lock );
     }
     else if (data->common.decode_surface) {
          CardState    state;
          DFBRectangle src_rect = {
               .x = 0,
               .y = 0,
               .w = data->common.decoded_width,
               .h = data->common.decoded_height
          };

          /* use DFB to convert raw YUV to destination format, and
             apply any necessary additional clip/stretch */
          dfb_state_init (&state, data->common.base.core);
          dfb_state_set_source (&state, data->common.decode_surface);
          dfb_state_set_destination (&state, dst_surface);
          dfb_state_set_clip (&state, clip);

          JPEG_stretchblit (&state, &src_rect, rect);

          /* remove the state */
          dfb_state_set_source (&state, NULL);
          dfb_state_set_destination (&state, NULL);
          dfb_state_destroy (&state);

          if (data->common.base.render_callback) {
               DFBRectangle r = { 0, 0, data->common.decoded_width, data->common.decoded_height };
               data->common.base.render_callback (&r,
                                                  data->common.base.render_callback_context);
          }
     }
     else {
          ret = dfb_surface_lock_buffer( dst_surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
          if (ret)
               return ret;

          dfb_scale_linear_32( data->common.image, data->common.decoded_width, data->common.decoded_height,
                               lock.addr, lock.pitch, rect, dst_surface, clip );
          if (data->common.base.render_callback) {
               DFBRectangle r = { 0, 0, data->common.decoded_width, data->common.decoded_height };
               data->common.base.render_callback( &r,
                                                  data->common.base.render_callback_context );
          }

          dfb_surface_unlock_buffer( dst_surface, &lock );
     }

     D_DEBUG_AT (JPEG, "software decoding finished\n");

     if (cb_result != DIRCR_OK)
          return DFB_INTERRUPTED;

     return DFB_OK;
}



static DFBResult
JPEG_RenderTo( IDirectFBImageProvider_JPEG_data *data,
               CoreSurface                      *dst_surface,
               DFBSurfacePixelFormat             format,
               DFBRectangle                     *rect,
               const DFBRegion                  *clip )
{
     DFBResult res;

     D_ASSERT( data != NULL );
     D_ASSERT( dst_surface != NULL );
     D_ASSERT( clip != NULL );
     D_ASSERT( rect != NULL );

     res = JPEG_HardwareRenderTo (data, dst_surface, format, rect, clip);
     if (res != DFB_OK) {
          if (res == DFB_IO) {
               /* IO error - there's no point in retrying */
               D_DEBUG_AT (JPEG, "hardware decode failed: %d (%s)\n",
                           res, DirectFBErrorString (res));
               return res;
          }

          D_DEBUG_AT (JPEG, "hardware decode failed: %d (%s) - "
                      "attempting software fallback\n",
                      res, DirectFBErrorString (res));

          res = JPEG_SoftwareRenderTo (data, dst_surface, format, rect, clip);
          if (unlikely (res != DFB_OK))
               D_DEBUG_AT (JPEG, "software decode failed: %d (%s)\n",
                           res, DirectFBErrorString (res));
     }

     return res;
}

static DFBResult
IDirectFBImageProvider_JPEG_RenderTo( IDirectFBImageProvider *thiz,
                                      IDirectFBSurface       *destination,
                                      const DFBRectangle     *dest_rect )
{
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     DFBSurfacePixelFormat  format;
     DFBResult              ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     ret = destination->GetPixelFormat( destination, &format );
     if (ret)
          return ret;

     if (data->thread)
          pthread_mutex_lock( &data->lock );

     dfb_region_from_rectangle( &data->clip, &dst_data->area.current );

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1) {
               if (data->thread)
                    pthread_mutex_unlock( &data->lock );
               return DFB_INVARG;
          }

          data->rect = *dest_rect;
          data->rect.x += dst_data->area.wanted.x;
          data->rect.y += dst_data->area.wanted.y;

          if (!dfb_rectangle_region_intersects( &data->rect, &data->clip )) {
               if (data->thread)
                    pthread_mutex_unlock( &data->lock );
               return DFB_OK;
          }
     }
     else {
          data->rect = dst_data->area.wanted;
     }

     if (!D_FLAGS_IS_SET (data->flags, DIPFLAGS_BACKGROUND_DECODE)) {
          /* threaded operation was not requested */
          data->thread_res = JPEG_RenderTo( data, dst_surface, format,
                                            &data->rect, &data->clip );
          if (data->thread)
               pthread_mutex_unlock( &data->lock );
          return data->thread_res;
     }

     if (!data->thread) {
          /* for the case that somebody does a RenderTo() twice on us, we
             have to create new thread, because the initial thread will have
             finished already */

          D_ASSERT( data->destination == NULL );

          /* as long as we haven't even started yet, we are in INIT state */
          data->thread_res = DFB_INIT;
          data->thread = direct_thread_create( DTT_DEFAULT, JPEGrenderThread,
                                               thiz, "JPEG" );
     }

     D_ASSERT( data->destination == NULL );

     destination->AddRef( destination );
     data->destination = destination;

     pthread_cond_signal( &data->cond );
     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_Sync( IDirectFBImageProvider    *thiz,
                                  DFBImageProviderSyncFlags  flags )
{
     DFBResult res;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     switch (flags)
       {
       case DIPSYNCFLAGS_TRYSYNC:
            if (data->thread) {
                 if (data->thread_res == DFB_INIT
                     || data->thread_res == DFB_BUSY) {
                      /* DFB_INIT (user didn't call RenderTo() yet)
                         DFB_BUSY (still busy decoding) */
                      return data->thread_res;
                 }
                 /* else we are done, either because of some error or because
                    we have processed all the data already */
            }
            /* fall through */

       case DIPSYNCFLAGS_SYNC:
            if (data->thread) {
                 direct_thread_join( data->thread );
                 direct_thread_destroy( data->thread );
                 data->thread = NULL;
            }
            break;

       default:
            return DFB_OK;
       }

     res = data->thread_res;
     data->thread_res = DFB_OK;
     return res;
}

static DFBResult
IDirectFBImageProvider_JPEG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                   DFBSurfaceDescription  *dsc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     dsc->flags  = DSDESC_WIDTH |  DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->width  = data->common.width;
     dsc->height = data->common.height;
     dsc->pixelformat = dfb_primary_layer_pixelformat();

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                 DFBImageDescription    *dsc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     if (!dsc)
          return DFB_INVARG;

     dsc->caps = DICAPS_NONE;

     return DFB_OK;
}

static void
render_cleanup( void *cleanup_data )
{
     IDirectFBImageProvider           *thiz = cleanup_data;
     IDirectFBImageProvider_JPEG_data *data;

     D_MAGIC_ASSERT( (IAny*)thiz, DirectInterface );
     data = (IDirectFBImageProvider_JPEG_data *) thiz->priv;
     D_ASSERT( data != NULL );

     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;
     }

     /* in case we get terminated from outside, set the state to DFB_DEAD */
     data->thread_res = DFB_DEAD;

     pthread_mutex_unlock( &data->lock );
}

static void *
JPEGrenderThread( DirectThread *thread, void *driver_data )
{
     IDirectFBImageProvider           *thiz = driver_data;
     IDirectFBImageProvider_JPEG_data *data;
     IDirectFBSurface_data            *dst_data;
     CoreSurface                      *dst_surface;
     DFBSurfacePixelFormat             format;
     DFBResult                         res;

     D_MAGIC_ASSERT( (IAny*)thiz, DirectInterface );
     data = (IDirectFBImageProvider_JPEG_data *) thiz->priv;
     D_ASSERT( data != NULL );

     pthread_mutex_lock( &data->lock );

     pthread_cleanup_push( render_cleanup, thiz );

     while (!data->destination) {
          pthread_cond_wait( &data->cond, &data->lock );
     }

     dst_data = (IDirectFBSurface_data*) data->destination->priv;
     D_ASSERT( dst_data != NULL );

     dst_surface = dst_data->surface;
     D_ASSERT( dst_surface != NULL );

     res = data->destination->GetPixelFormat( data->destination, &format );
     D_ASSERT( res == DFB_OK );

     /* as long as we haven't finished decoding we are busy */
     data->thread_res = DFB_BUSY;

     res = JPEG_RenderTo( data, dst_surface, format, &data->rect, &data->clip );

     pthread_cleanup_pop( 1 );

     /* in case we exit normally, apply the real return value */
     data->thread_res = res;

     return NULL;
}


#if defined(JPEG_PROVIDER_USE_MME)
static DFBResult
fetch_data (IDirectFBDataBuffer *buffer,
            MME_DataBuffer_t    *dbuf,
            int                  len)
{
  dbuf->ScatterPages_p[0].BytesUsed = 0;
  dbuf->ScatterPages_p[0].FlagsIn   = 0;
  dbuf->ScatterPages_p[0].FlagsOut  = 0;

  return buffer_to_ptr_copy (buffer, dbuf->ScatterPages_p[0].Page_p, len);
}


static void
TransformerCallback (MME_Event_t    Event,
                     MME_Command_t *CallbackData,
                     void          *UserData)
{
  const JPEGDEC_TransformReturnParams_t *transform_result =
    CallbackData->CmdStatus.AdditionalInfo_p;
//  const JPEGDECHW_VideoDecodeReturnParams_t *transform_result_hw =
//    CallbackData->CmdStatus.AdditionalInfo_p;
  struct _MMECommon * const mme = (struct _MMECommon *) UserData;
  IDirectFBImageProvider_JPEG_data * const jpeg =
    container_of (mme, IDirectFBImageProvider_JPEG_data, common);

//  if (CallbackData->CmdStatus.AdditionalInfo_p == &jpeg->hw_return_params)
//    {
//      transform_result_hw = CallbackData->CmdStatus.AdditionalInfo_p;
//      transform_result = NULL;
//    }
//  else
    {
      transform_result = CallbackData->CmdStatus.AdditionalInfo_p;
//      transform_result_hw = NULL;
    }

  if (!mme->name_set)
    {
      char name[25];
      snprintf (name, sizeof (name), "MME (%s)", mme->transformer_name);
      direct_thread_set_name (name);
      mme->name_set = true;
    }

  D_DEBUG_AT (JPEG_MME, "%sTransformerCallback: Event: %d: (%s)%s\n",
              RED, Event, get_mme_event_string (Event), BLACK);
  D_DEBUG_AT (JPEG_MME, "  -> CallbackData->CmdStatus.State: %d (%s)\n",
              CallbackData->CmdStatus.State,
              get_mme_state_string (CallbackData->CmdStatus.State));
  D_DEBUG_AT (JPEG_MME, "  -> CallbackData->CmdStatus.CmdId: %u (%.8x)\n",
              CallbackData->CmdStatus.CmdId,
              CallbackData->CmdStatus.CmdId);
  D_DEBUG_AT (JPEG_MME, "  -> CallbackData->CmdStatus.AdditionalInfoSize: %u\n",
              CallbackData->CmdStatus.AdditionalInfoSize);
  D_DEBUG_AT (JPEG_MME, "  -> mme->decode_success: %d\n", mme->decode_success);

  switch (Event)
    {
    case MME_COMMAND_COMPLETED_EVT:
      pthread_mutex_lock (&mme->pending_commands_lock);
      direct_hash_remove (mme->pending_commands,
                          CallbackData->CmdStatus.CmdId);
      if (likely (CallbackData->CmdStatus.CmdId != mme->TransformCommand.CmdStatus.CmdId))
        --mme->n_pending_buffers;
      pthread_mutex_unlock (&mme->pending_commands_lock);

      switch (CallbackData->CmdStatus.State)
        {
        case MME_COMMAND_COMPLETED:
          if (unlikely (CallbackData->CmdStatus.CmdId
                        == mme->TransformCommand.CmdStatus.CmdId))
            {
              deb_gettimeofday (&mme->endtime, NULL);
              deb_timersub (&mme->endtime, &mme->starttime, &mme->endtime);
              D_DEBUG_AT (JPEG_TIME, "  -> total time %lu.%06lu\n",
                          mme->endtime.tv_sec, mme->endtime.tv_usec);

              /* This info is available as data->ReturnParams. too!
                 mme->decoded_bytes  = (unsigned int) transform_result->bytes_written;
                 mme->decoded_height = (int) transform_result->decodedImageHeight;
                 mme->decoded_width  = (int) transform_result->decodedImageWidth; */

              if (transform_result)
                {
                  D_DEBUG_AT (JPEG_MME, "  -> expanded bytes: %u\n",
                              transform_result->bytes_written);
                  D_DEBUG_AT (JPEG_MME, "  -> decoded w/h: %dx%d\n",
                              transform_result->decodedImageWidth,
                              transform_result->decodedImageHeight);

                  if (jpeg->progressive_mode)
                    {
                      mme->width = transform_result->decodedImageWidth;
                      mme->height = transform_result->decodedImageHeight;
                    }

                  if (transform_result->Total_cycle)
                    D_DEBUG_AT (JPEG_MME,
                                "  -> cycles: total/dmiss/imiss: %u/%u/%u\n",
                                transform_result->Total_cycle,
                                transform_result->DMiss_Cycle,
                                transform_result->IMiss_Cycle);
                  D_DEBUG_AT (JPEG_MME, "  -> JPEG result code: %d (%s)\n",
                              transform_result->ErrorType,
                              get_jpeg_error_string (transform_result->ErrorType));
                }
//              else
//                {
//                  D_DEBUG_AT (JPEG_MME, "  -> cycles/dmiss/imiss: %u/%u/%u\n",
//                              transform_result_hw->pm_cycles,
//                              transform_result_hw->pm_dmiss,
//                              transform_result_hw->pm_imiss);
//                  D_DEBUG_AT (JPEG_MME, "  -> bundles/prefetches: %u/%u\n",
//                              transform_result_hw->pm_bundles,
//                              transform_result_hw->pm_pft);
//
//                  D_DEBUG_AT (JPEG_MME, "  -> JPEGHW result code: %d (%s)\n",
//                              transform_result_hw->ErrorCode,
//                              get_jpeghw_error_string (transform_result_hw->ErrorCode));
//                }

              mme->decode_success = 1;
              sema_signal_event (&mme->decode_event);

              /* in case of bogus data (too much), abort pending data
                 buffers */
              abort_transformer (mme);
            }
          else
            {
              /* buffer completed */
              struct _MMEHelper_buffer *buf =
                container_of (CallbackData->DataBuffers_p,
                              struct _MMEHelper_buffer, buffer);

              D_DEBUG_AT (JPEG_MME, "  -> buffer %p completed (container @ %p)\n",
                          CallbackData->DataBuffers_p[0], buf);

              sema_signal_event (&buf->sema);
            }
          break; /* MME_COMMAND_COMPLETED */

        case MME_COMMAND_FAILED:
          D_WARN ("(%5d) %s: command %u (%.8x) failed: error %d (%s)",
                  direct_gettid (), mme->name,
                  CallbackData->CmdStatus.CmdId,
                  CallbackData->CmdStatus.CmdId,
                  CallbackData->CmdStatus.Error,
                  get_mme_error_string (CallbackData->CmdStatus.Error));

          if (likely (CallbackData->CmdStatus.CmdId
                      == mme->TransformCommand.CmdStatus.CmdId))
            {
              /* transform command failed */
              D_WARN ("(%5d) %s: jpeg failure code: %d (%s)",
                      direct_gettid (), mme->name, transform_result->ErrorType,
                      get_jpeg_error_string (transform_result->ErrorType));
              switch (transform_result->ErrorType)
                {
                case UNSUPPORTED_COLORSPACE:
                case UNABLE_ALLOCATE_MEMORY:
                case UNSUPPORTED_SCALING:
                case INSUFFICIENT_OUTPUTBUFFER_SIZE:
                  /* the transformer doesn't support this, but pure libjpeg
                     should. */
                  mme->decode_success = -1;
                  break;

                case UNSUPPORTED_ROTATION_ANGLE:
                default:
                  /* fake a success, there's no point in retrying libjpeg, as
                     it already failed via MME. */
                  mme->decode_success = 1;
                  D_DEBUG_AT (JPEG_MME, "  -> faking success\n");
                  break;
              }

              sema_signal_event (&mme->decode_event);
            }
          else
            {
              /* buffer failed */
              struct _MMEHelper_buffer *buf =
                container_of (CallbackData->DataBuffers_p,
                              struct _MMEHelper_buffer, buffer);
              if (mme->decode_success == 0)
                /* only if we didn't succeed yet */
                mme->decode_success = -1;
              sema_signal_event (&buf->sema);
            }
          break; /* MME_COMMAND_FAILED */

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
      /* because of the way the transformer works (and how we communicate
         with it), we get exactly one underflow event on startup */
      D_DEBUG_AT (JPEG_MME, "  -> %sdata underflow on command %u (%.8x)\n",
                  (mme->n_underflows == 0) ? "(anticipated) " : "",
                  CallbackData->CmdStatus.CmdId, CallbackData->CmdStatus.CmdId);
      if (++mme->n_underflows > 1)
        {
          /* check if the IDirectFBDataBuffer definitely(!) hit an EOF and
             we have no buffers queued */
          bool end_of_data;
          char tmp;
          pthread_mutex_lock (&mme->pending_commands_lock);
          DFBResult has_data = mme->base.buffer->PeekData (mme->base.buffer,
                                                           1, 0, &tmp, NULL);
          /* n_pending_buffers is checked to be == 0 because the
             TransformCommand itself is not stored in there. */
          end_of_data = (has_data == DFB_EOF
                         && mme->n_pending_buffers == 0);
          pthread_mutex_unlock (&mme->pending_commands_lock);

          /* if so, signal completion */
          if (end_of_data)
            {
              D_DEBUG_AT (JPEG_MME, "  -> no more data available, signalling fail\n");
              mme->decode_success = -2;
              sema_signal_event (&mme->decode_event);
            }
        }
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
          if (transform_result)
            {
              D_WARN ("(%5d) %s: transform_result->bytes_written: %u",
                      direct_gettid (), mme->name, transform_result->bytes_written);
              D_WARN ("(%5d) %s: transform_result->decodedImageHeight: %d",
                      direct_gettid (), mme->name, transform_result->decodedImageHeight);
              D_WARN ("(%5d) %s: transform_result->decodedImageWidth: %d",
                      direct_gettid (), mme->name, transform_result->decodedImageWidth);
              if (transform_result->Total_cycle)
                D_WARN ("(%5d) %s: transform_result->cycles: total/dmiss/imiss: %u/%u/%u\n",
                        direct_gettid (), mme->name,
                        transform_result->Total_cycle,
                        transform_result->DMiss_Cycle,
                        transform_result->IMiss_Cycle);
              D_WARN ("(%5d) %s: result code: %d (%s)\n", direct_gettid (),
                      mme->name, transform_result->ErrorType,
                      get_jpeg_error_string (transform_result->ErrorType));
            }
//          else if (transform_result_hw)
//            {
//              D_WARN ("(%5d) %s: transform_result->cycles/dmiss/imiss: %u/%u/%u\n",
//                      direct_gettid (), mme->name,
//                      transform_result_hw->pm_cycles,
//                      transform_result_hw->pm_dmiss,
//                      transform_result_hw->pm_imiss);
//              D_WARN ("(%5d) %s: transform_result->bundles/prefetches: %u/%u\n",
//                          direct_gettid (), mme->name,
//                          transform_result_hw->pm_bundles,
//                          transform_result_hw->pm_pft);
//
//              D_WARN ("(%5d) %s: transform_result->JPEGHW result code: %d (%s)\n",
//                          direct_gettid (), mme->name,
//                          transform_result_hw->ErrorCode,
//                          get_jpeghw_error_string (transform_result_hw->ErrorCode));
//            }
        }
      break;
    }
}

#if 0
static void
TransformerCallback2 (MME_Event_t    Event,
                      MME_Command_t *CallbackData,
                      void          *UserData)
{
  const JPEGDECHW_VideoDecodeReturnParams_t * const transform_result_hw =
    CallbackData->CmdStatus.AdditionalInfo_p;
  struct _MMECommon * const mme = (struct _MMECommon *) UserData;

  if (!mme->name_set)
    {
      direct_thread_set_name ("MME (hwjpeg)");
      mme->name_set = true;
    }

  D_DEBUG_AT (JPEG_MME, "%sTransformerCallback2: Event: %d: (%s)%s\n",
              RED, Event, get_mme_event_string (Event), BLACK);
  D_DEBUG_AT (JPEG_MME, "  -> CallbackData->CmdStatus.State: %d (%s)\n",
              CallbackData->CmdStatus.State,
              get_mme_state_string (CallbackData->CmdStatus.State));
  D_DEBUG_AT (JPEG_MME, "  -> CallbackData->CmdStatus.CmdId: %u (%.8x)\n",
              CallbackData->CmdStatus.CmdId, CallbackData->CmdStatus.CmdId);
  D_DEBUG_AT (JPEG_MME, "  -> CallbackData->CmdStatus.AdditionalInfoSize: %u\n",
              CallbackData->CmdStatus.AdditionalInfoSize);
  D_DEBUG_AT (JPEG_MME, "  -> mme->decode_success: %d\n", mme->decode_success);
}
#endif


/* Pre-Scaling Calculations for JPEG Hardware Scaling
 * These functions should be used anywhere where SOURCE width and height are
   referenced */
static unsigned int
pre_scale_value (const IDirectFBImageProvider_JPEG_data *data,
                 const DFBRectangle                     *dest_rect)
{
  unsigned int scale;

  D_ASSERT( data != NULL );

#ifdef PRE_SCALE_SPEED_SPACE_OPTIMISED
  /* Speed and Space */
  if (data->common.width <= dest_rect->w || data->common.height <= dest_rect->h)
    scale = 1;
  else if ((data->common.width / 2 <= dest_rect->w) && (data->common.height / 2 <= dest_rect->h))
    scale = 2;
  else if ((data->common.width / 4 <= dest_rect->w) && (data->common.height / 4 <= dest_rect->h))
    scale = 4;
  else if ((data->common.width / 8 <= dest_rect->w) && (data->common.height / 8 <= dest_rect->h))
    scale = 8;
  else
    scale = 1;
#endif

#ifdef PRE_SCALE_QUALITY_OPTIMISED
  /* Quality */
  if ((data->common.width / 8 >= dest_rect->w) && (data->common.height / 8 >= dest_rect->h))
    scale = 8;
  else if ((data->common.width / 4 >= dest_rect->w) && (data->common.height / 4 >= dest_rect->h))
    scale = 4;
  else if ((data->common.width / 2 >= dest_rect->w) && (data->common.height / 2 >= dest_rect->h))
    scale = 2;
  else
    scale = 1;
#endif

#if !defined(PRE_SCALE_SPEED_SPACE_OPTIMISED) && !defined(PRE_SCALE_QUALITY_OPTIMISED)
  scale = 1;
#endif

  return scale;
}

static inline unsigned int
pre_scaled_width (const IDirectFBImageProvider_JPEG_data *data,
                  const DFBRectangle                     *dest_rect)
{
  D_ASSERT( data != NULL );

  unsigned int w = (data->common.width + 15) & ~15; /* Macro Block Rounding */
  return w / pre_scale_value (data, dest_rect);
}

static inline unsigned int
pre_scaled_height (const IDirectFBImageProvider_JPEG_data *data,
                   const DFBRectangle                     *dest_rect)
{
  D_ASSERT( data != NULL );

  unsigned int h = (data->common.height + 15) & ~15; /* Macro Block Rounding */
  return h / pre_scale_value (data, dest_rect);
}

/* warning: MME_AbortCommand() works only for the MME_TRANSFORM command;
   individual buffers can be aborted, too, but MME_TermTransformer() will
   not work afterwards anymore! */
static bool
pending_fe_func (DirectHash    *hash,
                 unsigned long  key,
                 void          *value,
                 void          *ctx)
{
  struct _MMECommon * const mme = ctx;
  MME_CommandId_t    CmdId = key;

  if (key != mme->TransformCommand.CmdStatus.CmdId)
    {
      if (direct_hash_lookup (hash, mme->TransformCommand.CmdStatus.CmdId))
        D_WARN ("(%5d) aborting MME command %u (%.8x) can lead to a crash "
                "somewhere in the JPEG transformer!",
                direct_gettid (), CmdId, CmdId);
    }
  else
    D_DEBUG_AT (JPEG_MME, "aborting (main transform) command %u (%.8x)\n",
                CmdId, CmdId);

  MME_ERROR res = MME_AbortCommand (mme->Handle, CmdId);
  if (res != MME_SUCCESS)
    D_WARN ("(%5d) MME_AbortCommand(%x, %.8x) for %s failed: %d (%s)\n",
            direct_gettid (), mme->Handle, CmdId,
            mme->name, res, get_mme_error_string (res));

  /* continue iterating */
  return true;
}

static DFBResult
abort_transformer (struct _MMECommon * const mme)
{
  if (!mme)
       return DFB_THIZNULL;

  /* FIXME: what about locking? */
  direct_hash_iterate (mme->pending_commands,
                       pending_fe_func,
                       mme);

  return DFB_OK;
}


static void
_imageprovider_update_transform_params (struct _MMECommon           * const mme,
                                        void                        * const params,
                                        const CoreSurfaceBufferLock * const lock)
{
  IDirectFBImageProvider_JPEG_data * const data =
    container_of (mme, IDirectFBImageProvider_JPEG_data, common);

  data->OutputParams.outputSettings.Pitch = lock->pitch;
  data->OutputParams.outputSettings.ROTATEFLAG |= 0x80000000;
}

/*
 *  Set up JPEG transform command
 *  Establish Decode Parameters,
 *  and prepare destination surface
 *
 */
static DFBResult
jpeg_start_transformer (IDirectFBImageProvider_JPEG_data *data,
                        CoreSurface                      *dst_surface,
                        CoreSurfaceBufferLock            *lock)
{
  DFBResult res;

  /* If height and width are NOT values divisible by 16 - then decode will not work as expected. */
  data->OutputParams.outputSettings.xvalue0 = 0;
  data->OutputParams.outputSettings.xvalue1 = 0;
  data->OutputParams.outputSettings.yvalue0 = 0;
  data->OutputParams.outputSettings.yvalue1 = 0;

  data->OutputParams.outputSettings.outputWidth = 0;  //Buffersize used to prevent bufferoverflows;
  data->OutputParams.outputSettings.outputHeight = 0; //Buffersize used to prevent bufferoverflows;
  data->OutputParams.outputSettings.ROTATEFLAG = 0;
  data->OutputParams.outputSettings.Rotatedegree = 0;
  data->OutputParams.outputSettings.HorizantalFlip = 0;
  data->OutputParams.outputSettings.VerticalFlip = 0;

  res = _mme_helper_start_transformer_core (&data->common,
                                            sizeof (data->ReturnParams),
                                            &data->ReturnParams,
                                            sizeof (data->OutputParams),
                                            &data->OutputParams,
                                            dst_surface, lock);
  if (res != DFB_OK)
    return res;

  _imageprovider_update_transform_params (&data->common,
                                          &data->OutputParams, lock);

  /* if pitch != width - we'll have issues with the hardware decode */
  /* FIXME: the comment makes no sense. And the D_ASSUME() does sth
     different... */
  D_ASSUME (lock->pitch != dst_surface->config.size.w);

  D_DEBUG_AT (MME_DEBUG_DOMAIN, "issuing MME_TRANSFORM\n");

  /* lock access to hash, because otherwise the callback could be called
     before we've had a chance to put the command id into the hash */
  D_ASSERT (&data->common.pending_commands != NULL);
  pthread_mutex_lock (&data->common.pending_commands_lock);

  res = MME_SendCommand (data->common.Handle, &data->common.TransformCommand);
  if (res != MME_SUCCESS)
    {
      pthread_mutex_unlock (&data->common.pending_commands_lock);

      D_WARN ("(%5d) %s: starting transformer failed: %d (%s)",
              direct_gettid (), data->common.name,
              res, get_mme_error_string (res));

      dfb_surface_unlock_buffer (dst_surface, lock);

      D_FREE (data->common.OutDataBuffers);
      data->common.OutDataBuffers = NULL;

      return DFB_FAILURE;
    }

  direct_hash_insert (data->common.pending_commands,
                      data->common.TransformCommand.CmdStatus.CmdId,
                      (void *) 1);
  D_DEBUG_AT (JPEG_MME, "sent packet's CmdId is %u (%.8x)\n",
              data->common.TransformCommand.CmdStatus.CmdId,
              data->common.TransformCommand.CmdStatus.CmdId);

  deb_gettimeofday (&data->common.starttime, NULL);

  pthread_mutex_unlock (&data->common.pending_commands_lock);

  return DFB_OK;
}

#if 0
#include <../gfxdrivers/stgfx/stgfx.h>
#include <../gfxdrivers/stgfx2/stm_gfxdriver.h>
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
#endif

#if 0
static DFBResult
jpeg_start_transformer2 (IDirectFBImageProvider_JPEG_data *data,
                         CoreSurface                      *dst_surface,
                         CoreSurfaceBufferLock            *lock)
{
  DFBResult             res;
  CoreSurfaceBufferLock raw_lock;
  CoreSurfaceBufferLock chr_lock;


  /* src surface */
  CoreSurface *raw_surface;

  /* FIXME: this is BAD (tm) -> think streaming media! */
  data->common.buffer->SeekTo (data->common.buffer, 0);
  /* find out the length of the buffer */
  unsigned int src_bytes;
  data->common.buffer->GetLength (data->common.buffer, &src_bytes);

  {
  CoreSurfaceConfig config = { .size.w = src_bytes,
                               .size.h = 1 };
  /* create temporary surface to store the compressed data in. */
  config.flags = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
  config.format = DSPF_LUT8;
  config.caps   = DSCAPS_VIDEOONLY;
  #ifdef DIRECT_BUILD_DEBUG
  config.caps   |= DSCAPS_SHARED;
  #endif
  if ((res = dfb_surface_create (data->common.core,
                                 &config,
                                 CSTF_NONE,
                                 0,
                                 NULL,
                                 &raw_surface)) != DFB_OK)
    goto out1;
  if ((res = dfb_surface_lock_buffer (raw_surface, CSBR_BACK, CSAID_CPU,
                                      CSAF_WRITE, &raw_lock)) != DFB_OK)
    goto out2;
  }

  {
  unsigned int sz;
  u8 *addr = raw_lock.addr;
  res = data->common.buffer->GetData (data->common.buffer, src_bytes, addr, &sz);
  if (sz != src_bytes)
    goto out3;
  printf("src data @ %.8lx\n", lock->phys);
  }
  /* FIXME: this is BAD (tm) -> think streaming media! */
  data->common.buffer->SeekTo (data->common.buffer, 0);
  res = dfb_surface_unlock_buffer (raw_surface, &raw_lock);


  CoreSurface *chr_surface;
  {
  CoreSurfaceConfig config = { .size.w = data->common.width,
                               .size.h = data->common.height };
  config.size.h += 2;
  /* create temporary surface to store the compressed data in. */
  config.flags = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
  config.format = DSPF_ARGB;
  config.caps   = DSCAPS_VIDEOONLY;
  #ifdef DIRECT_BUILD_DEBUG
  config.caps   |= DSCAPS_SHARED;
  #endif
  if ((res = dfb_surface_create (data->common.core,
                                 &config,
                                 CSTF_NONE,
                                 0,
                                 NULL,
                                 &chr_surface)) != DFB_OK)
    goto out4;
  }


  res = dfb_surface_lock_buffer (raw_surface, CSBR_FRONT, CSAID_ACCEL0,
                                 CSAF_READ, &raw_lock);
  res = dfb_surface_lock_buffer (data->common.decode_surface, CSBR_BACK, CSAID_ACCEL0,
                                 CSAF_WRITE, lock);
  res = dfb_surface_lock_buffer (chr_surface, CSBR_BACK, CSAID_ACCEL0,
                                 CSAF_WRITE, &chr_lock);

  /* should be aligned on 256byte boundary */
  D_ASSUME ((chr_lock.phys & 0xff) == 0);
  D_ASSUME ((lock->phys & 0xff) == 0);
  printf ("raw/dst/chr phys: 0x%.8lx 0x%.8lx 0x%.8lx\n",
          raw_lock.phys, lock->phys, chr_lock.phys);

  u32 raw_phys = raw_lock.phys;
  u32 dst_phys = (lock->phys + 255) & 0xffffff00;
  printf("dst_phys %.8x\n", dst_phys);
  u32 chr_phys = (chr_lock.phys + 255) & 0xffffff00;
  printf("chr_phys %.8x\n", chr_phys);

  raw_phys += 0x10000000;
  dst_phys += 0x10000000;
  chr_phys += 0x10000000;

  /* If height and width are NOT values divisible by 16 - then decode will not work as expected. */
  data->hw_decode_params.PictureStartAddr_p = (void *) raw_phys;
  data->hw_decode_params.PictureEndAddr_p   = (void *) (raw_phys + src_bytes);
  data->hw_decode_params.DecodedBufferAddr.Luma_p            = (void *) dst_phys;
  data->hw_decode_params.DecodedBufferAddr.Chroma_p          = (void *) chr_phys;
  data->hw_decode_params.DecodedBufferAddr.LumaDecimated_p   = (void *) (dst_phys + 512);
  data->hw_decode_params.DecodedBufferAddr.ChromaDecimated_p = (void *) (chr_phys + 512);
  data->hw_decode_params.MainAuxEnable = JPEGDECHW_MAINOUT_EN;
  data->hw_decode_params.HorizontalDecimationFactor = JPEGDECHW_HDEC_1;
  data->hw_decode_params.VerticalDecimationFactor   = JPEGDECHW_VDEC_1;
  data->hw_decode_params.xvalue0 = 0;
  data->hw_decode_params.yvalue0 = 0;
  data->hw_decode_params.xvalue1 = 0;
  data->hw_decode_params.yvalue1 = 0;
  data->hw_decode_params.DecodingMode = JPEGDECHW_NORMAL_DECODE;
  data->hw_decode_params.AdditionalFlags = JPEGDECHW_ADDITIONAL_FLAG_NONE;

  data->hw_decode_params.MainAuxEnable = JPEGDECHW_AUXOUT_EN;
  data->hw_decode_params.AdditionalFlags = 64; /* raster */

  /* set up the transform command */
  data->common.TransformCommand.StructSize = sizeof (MME_Command_t);
  data->common.TransformCommand.CmdCode    = MME_TRANSFORM;
  data->common.TransformCommand.CmdEnd     = MME_COMMAND_END_RETURN_NOTIFY;
  data->common.TransformCommand.DueTime    = (MME_Time_t) 0;
  data->common.TransformCommand.NumberInputBuffers  = 0;
  data->common.TransformCommand.NumberOutputBuffers = 0;
  data->common.TransformCommand.DataBuffers_p = NULL;
  data->common.TransformCommand.ParamSize  = sizeof (data->hw_decode_params);
  data->common.TransformCommand.Param_p    = &data->hw_decode_params;

  /* init the transform command */
  memset (&(data->common.TransformCommand.CmdStatus), 0, sizeof (MME_CommandStatus_t));
  data->common.TransformCommand.CmdStatus.AdditionalInfoSize = sizeof (data->hw_return_params);
  data->common.TransformCommand.CmdStatus.AdditionalInfo_p = &data->hw_return_params;

  D_DEBUG_AT (MME_DEBUG_DOMAIN, "issuing MME_TRANSFORM\n");

  /* lock access to hash, because otherwise the callback could be called
     before we've had a chance to put the command id into the hash */
  D_ASSERT (&data->common.pending_commands != NULL);
  pthread_mutex_lock (&data->common.pending_commands_lock);

  res = MME_SendCommand (data->common.Handle, &data->common.TransformCommand);
  if (res != MME_SUCCESS)
    {
      pthread_mutex_unlock (&data->common.pending_commands_lock);

      D_WARN ("(%5d) %s: starting transformer failed: %d (%s)",
              direct_gettid (), data->common.name,
              res, get_mme_error_string (res));

      res = DFB_FAILURE;

      goto out5;
    }

  direct_hash_insert (data->common.pending_commands,
                      data->common.TransformCommand.CmdStatus.CmdId,
                      (void *) 1);
  D_DEBUG_AT (JPEG_MME, "sent packet's CmdId is %u (%.8x)\n",
              data->common.TransformCommand.CmdStatus.CmdId,
              data->common.TransformCommand.CmdStatus.CmdId);

  deb_gettimeofday (&data->common.starttime, NULL);

  pthread_mutex_unlock (&data->common.pending_commands_lock);


  /* wait until the decode is complete (or until the abort was
     acknowledged) */
  D_DEBUG_AT (JPEG_MME, "waiting for completion\n");
  while ((sema_wait_event (&data->common.decode_event) == -1)
         && errno == EINTR)
    ;
  if (data->common.decode_success != 1)
    res = DFB_FAILURE;


  mme_helper_deinit_transformer (&data->common);


out5:
  dfb_surface_unlock_buffer (chr_surface, &chr_lock);
  dfb_surface_unref (chr_surface);
  dfb_surface_unlock_buffer (data->common.decode_surface, lock);


out4:
out3:
  dfb_surface_unlock_buffer (raw_surface, &raw_lock);
out2:
  dfb_surface_unref (raw_surface);
out1:
  return res;
}
#endif


static DFBResult
JPEG_HardwareRenderTo( IDirectFBImageProvider_JPEG_data *data,
                       CoreSurface                      *dst_surface,
                       DFBSurfacePixelFormat             format,
                       DFBRectangle                     *rect,
                       const DFBRegion                  *clip )
{
  DFBResult              dfbret = dfbret;
  CoreSurfaceBufferLock  lock;

  D_ASSERT( data != NULL );
  D_ASSERT( dst_surface != NULL );
  D_ASSERT( rect != NULL );
  D_ASSERT( clip != NULL );

  CoreSurfaceConfig config;

  config.size.w = pre_scaled_width (data, rect);
  config.size.h = pre_scaled_height (data, rect);

  if (unlikely (data->progressive_mode))
    {
      int src_width  = (data->common.width + 15) & ~15;
      int src_height = (data->common.height + 15) & ~15;
      int totalsize = data->num_components * src_width * src_height;
      int scale;

#define MAX_MEMORY_FOR_PROGRESSIVE_IMAGE  (1 * 1024 * 1024)
      if (totalsize < MAX_MEMORY_FOR_PROGRESSIVE_IMAGE)
        scale = 1;
      else if (totalsize/4 < MAX_MEMORY_FOR_PROGRESSIVE_IMAGE)
        scale = 2;
      else if (totalsize/16 < MAX_MEMORY_FOR_PROGRESSIVE_IMAGE)
        scale = 4;
      else if (totalsize/64 < MAX_MEMORY_FOR_PROGRESSIVE_IMAGE)
        scale = 8;
      else
        return DFB_UNSUPPORTED;

      data->common.width /= scale;
      data->common.height /= scale;
//      data->common.width = src_width / scale;
//      data->common.height = src_height / scale;

      config.size.w = src_width / scale;
      config.size.h = src_height / scale;
    }
  else
    {
      int pre_scale = pre_scale_value (data, rect);

      config.size.w = pre_scaled_width (data, rect);
      config.size.h = pre_scaled_height (data, rect);
      data->common.width /= pre_scale;
      data->common.height /= pre_scale;
    }

  /* FIXME: check if the image is big enough to warrant a HW decode */
  if (!data->common.decode_surface
      || config.size.w != data->common.decode_surface->config.size.w
      || config.size.h != data->common.decode_surface->config.size.h)
    {
      /* create destination surface to MME-decode the image onto before
         stretchblit ... and if we can't create a surface in video memory then
         we can't blit */
      /* Also, decide on the decode size.
         Limitations are:
          - closest match to a 1/1 1/2 1/4 1/8 reduction
          - AND a 16 pixel macroblock size. */
      config.flags = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
      config.format = DSPF_UYVY;
#ifdef __SH4__
      config.caps = DSCAPS_VIDEOONLY;
#else
      config.caps = 0;
#endif
      #ifdef DIRECT_BUILD_DEBUG
      config.caps   |= DSCAPS_SHARED;
      #endif

      D_DEBUG_AT (JPEG_MME, "%dx%d prescaled to %dx%d\n",
                  rect->w, rect->h, config.size.w, config.size.h);

      if (data->common.decode_surface)
        dfb_surface_unref (data->common.decode_surface);

      if (dfb_surface_create (data->common.base.core,
                              &config,
                              CSTF_NONE,
                              0,
                              NULL,
                              &data->common.decode_surface))
        {
          data->common.decode_surface = NULL;
          return DFB_NOVIDEOMEMORY;
        }

      data->common.decode_success = 0;
    }

  if (data->common.decode_success <= 0)
    {
      data->common.name = "hwJPEG";
      data->common.decode_success = 0;

#if 0
      data->common.name = "hwJPEGHW";
      {
      static const char * const transformers[] = { "JPEG_DECODER_HW1",
                                                   "JPEG_DECODER_HW0",
                                                   "JPEG_DECODER_HW4",
                                                   "JPEG_DECODER_HW3",
                                                   "JPEG_DECODER_HW2",
                                                   "JPEG_DECODER_HW",
                                                   NULL };
      unsigned int index;
      JPEGDECHW_VideoDecodeInitParams_t init;
      /* we should probably allocate some mme memory 2048*2048 */
      init.CircularBufferBeginAddr_p = (JPEGDECHW_CompressedData_t) 0x00000000;
      init.CircularBufferEndAddr_p   = (JPEGDECHW_CompressedData_t) 0xfffffff8;
//      init.CircularBufferBeginAddr_p = 0;
//      init.CircularBufferEndAddr_p   = 0;
      dfbret = mme_helper_init_transformer2 (&data->common, transformers,
                                             sizeof (init), &init, &index,
                                             &TransformerCallback);
      if (dfbret != DFB_OK)
        goto cont;

      MME_TransformerCapability_t cap;
      _mme_helper_get_capability (&data->common, transformers[index], &cap);

      if (data->flags & DIPFLAGS_BACKGROUND_DECODE)
        direct_thread_set_name (transformers[index]);

      /* store the current data buffer position for software fallback
         should we not succeed */
      unsigned int bufpos;
      data->common.buffer->GetPosition (data->common.buffer, &bufpos);

      sema_init_event (&data->common.decode_event, 0);

      /* Tell the transformer what to do and where to put result. depending on dst - may need to consider pitch!*/
      dfbret = jpeg_start_transformer2 (data, data->common.decode_surface,
                                        &lock);
      goto stretch;
      }

cont: ;
#endif

      static const char * const transformers[] = { JPEGDEC_MME_TRANSFORMER_NAME"0",
                                                   JPEGDEC_MME_TRANSFORMER_NAME"1",
                                                   JPEGDEC_MME_TRANSFORMER_NAME"2",
                                                   JPEGDEC_MME_TRANSFORMER_NAME"3",
                                                   JPEGDEC_MME_TRANSFORMER_NAME"4",
                                                   JPEGDEC_MME_TRANSFORMER_NAME,
                                                   NULL };
      unsigned int index;
      dfbret = mme_helper_init_transformer (&data->common, transformers,
                                            0, NULL, &index);
      if (dfbret == DFB_OK)
        {
           MME_TransformerCapability_t cap;
          _mme_helper_get_capability (&data->common,
                                      transformers[index], &cap);

#if 0
          if (cap.Version != 2)
            {
              /* incompatible with old transformer, it has just way too
                 many bugs! */
              mme_helper_deinit_transformer (&data->common);
              dfbret = DFB_FAILURE;
              continue;
            }

          data->common.transformer_name = *transformers[transformer_index];
          if (data->flags & DIPFLAGS_BACKGROUND_DECODE)
            direct_thread_set_name (*transformers[transformer_index]);

          break;
#endif
        }

      if (dfbret == DFB_OK)
        {
          bool locked = false;

          data->common.transformer_name = transformers[index];
          if (data->flags & DIPFLAGS_BACKGROUND_DECODE)
            direct_thread_set_name (transformers[index]);

          /* store the current data buffer position for software fallback
             should we not succeed */
          unsigned int bufpos;
          data->common.base.buffer->GetPosition (data->common.base.buffer,
                                                 &bufpos);

          sema_init_event (&data->common.decode_event, 0);

          /* Tell the transformer what to do and where to put result. depending on dst - may need to consider pitch!*/
          dfbret = jpeg_start_transformer (data, data->common.decode_surface,
                                           &lock);
          if (dfbret == DFB_OK)
            {
              locked = true;

              D_DEBUG_AT (JPEG_MME, "Transform sent, sending data buffers now\n");

              mme_helper_calculate_packets (&data->common);
              dfbret = mme_helper_send_packets (&data->common, -1);
              if (dfbret != DFB_OK)
                {
                  D_DEBUG_AT (JPEG_MME, "aborting (main) transform command %u (%.8x)\n",
                              data->common.TransformCommand.CmdStatus.CmdId,
                              data->common.TransformCommand.CmdStatus.CmdId);
                  MME_ERROR res = MME_AbortCommand (data->common.Handle,
                                                    data->common.TransformCommand.CmdStatus.CmdId);
                  if (res != MME_SUCCESS)
                    D_WARN ("(%5d) MME_AbortCommand(%x, %.8x) for %s failed: %d (%s)\n",
                            direct_gettid (), data->common.Handle,
                            data->common.TransformCommand.CmdStatus.CmdId,
                            data->common.name, res, get_mme_error_string (res));
                }

              /* wait until the decode is complete (or until the abort was
                 acknowledged) */
              D_DEBUG_AT (JPEG_MME, "waiting for completion\n");
              while ((sema_wait_event (&data->common.decode_event) == -1)
                     && errno == EINTR)
                ;
              if (data->common.decode_success != 1)
                dfbret = DFB_FAILURE;
            }
          else
            {
              if (data->common.decode_success < 0)
                dfbret = DFB_FAILURE;
              D_DEBUG_AT (JPEG_MME, "couldn't start transformer\n");
            }

          /* we always get an underflow on startup at the moment - update the
             value so only interesting underflow get printed ... */
          if (data->common.n_underflows)
            --data->common.n_underflows;

          abort_transformer (&data->common);
          /* wait till all commands are aborted and acknowledged */
          while (data->common.n_pending_buffers
                 || direct_hash_lookup (data->common.pending_commands,
                                        data->common.TransformCommand.CmdStatus.CmdId))
            usleep (1);

          mme_helper_deinit_transformer (&data->common);

          if (likely (locked))
            dfb_surface_unlock_buffer (data->common.decode_surface, &lock);

          if (data->common.OutDataBuffers)
            {
              D_FREE (data->common.OutDataBuffers);
              data->common.OutDataBuffers = NULL;
            }

          unsigned int i;
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

          data->common.base.buffer->SeekTo (data->common.base.buffer,
                                            bufpos);
        }
      else
        {
          D_ASSUME (data->common.decode_surface != NULL);
          dfb_surface_unref (data->common.decode_surface);
          data->common.decode_surface = NULL;
        }
    }

  if (data->common.decode_success == 1)
    {
      /* stretch blit the decoded image to the destination surface and size */
      mme_helper_stretch_blit (&data->common, data->common.decode_surface,
                               dst_surface, rect);
      dfbret = DFB_OK;
    }
  else
    {
      if (data->common.decode_surface)
        dfb_surface_unref (data->common.decode_surface);
      data->common.decode_surface = NULL;
    }

  return dfbret;
}
#endif /* JPEG_PROVIDER_USE_MME */
