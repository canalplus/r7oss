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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <png.h>
#include <string.h>
#include <stdarg.h>

#include <directfb.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/palette.h>
#include <core/surface.h>

#include <core/CoreSurface.h>

#include <misc/gfx_util.h>
#include <misc/util.h>

#include <gfx/clip.h>
#include <gfx/convert.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/thread.h>
#include <direct/messages.h>
#include <direct/util.h>

#if !defined(PNG_PROVIDER_USE_MME)
#undef USE_MME
#endif


#include "debug_helper.h"

D_DEBUG_DOMAIN (HWPNG,      "PNG",      "STM MME-based PNG decoder");
D_DEBUG_DOMAIN (HWPNG_MME,  "PNG/MME",  "STM MME-based PNG decoder (MME)");
D_DEBUG_DOMAIN (HWPNG_SEMA, "PNG/SEMA", "STM MME-based PNG decoder (semaphores");
D_DEBUG_DOMAIN (HWPNG_TIME, "PNG/Time", "STM MME-based PNG decoder (timing");
#define MME_DEBUG_DOMAIN  HWPNG_MME
#define SEMA_DEBUG_DOMAIN HWPNG_SEMA
#define MME_TEXT_DOMAIN "PNG"

#define fetch_data   buffer_to_mme_copy
#include "mme_helper.h"
#include "idirectfbimageprovider_png.h"
#if defined(PNG_PROVIDER_USE_MME)
#include "sema_helper.h"
static DFBResult PNG_HardwareRenderTo( IDirectFBImageProvider_PNG_data *data,
                                       CoreSurface                     *dst_surface,
                                       DFBSurfacePixelFormat            format,
                                       DFBRectangle                    *rect,
                                       const DFBRegion                 *clip );
#else /* PNG_PROVIDER_USE_MME */
#define PNG_HardwareRenderTo(data,dst_surface, \
                             format, rect, clip) DFB_NOSUCHINSTANCE
#endif /* PNG_PROVIDER_USE_MME */


D_DEBUG_DOMAIN( imageProviderPNG,  "ImageProvider/PNG",  "libPNG based image decoder" );

#if PNG_LIBPNG_VER < 10400
#define trans_color  trans_values
#define trans_alpha  trans
#endif

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... );

#include <direct/interface_implementation.h>

#if defined(PNG_PROVIDER_USE_MME)
DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, hwPNG )
#else /* PNG_PROVIDER_USE_MME */
DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, swPNG )
#endif /* PNG_PROVIDER_USE_MME */

enum {
     STAGE_ABORT = -2,
     STAGE_ERROR = -1,
     STAGE_START =  0,
     STAGE_INFO,
     STAGE_IMAGE,
     STAGE_END
};


static DFBResult
IDirectFBImageProvider_PNG_SetRenderFlags ( IDirectFBImageProvider *thiz,
                                            DIRenderFlags           flags );

static DFBResult
IDirectFBImageProvider_PNG_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *destination_rect );

static DFBResult
IDirectFBImageProvider_PNG_Sync( IDirectFBImageProvider    *thiz,
                                 DFBImageProviderSyncFlags  flags );

static DFBResult
IDirectFBImageProvider_PNG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *dsc );

static DFBResult
IDirectFBImageProvider_PNG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *dsc );

static void *
PNGrenderThread( DirectThread *thread, void *driver_data );


/* Called at the start of the progressive load, once we have image info */
static void
png_info_callback (png_structp png_read_ptr,
                   png_infop   png_info_ptr);

/* Called for each row; note that you will get duplicate row numbers
   for interlaced PNGs */
static void
png_row_callback  (png_structp png_read_ptr,
                   png_bytep   new_row,
                   png_uint_32 row_num,
                   int         pass_num);

/* Called after reading the entire image */
static void
png_end_callback  (png_structp png_read_ptr,
                   png_infop   png_info_ptr);

/* Pipes data into libpng until stage is different from the one specified. */
static DFBResult
push_data_until_stage (IDirectFBImageProvider_PNG_data *data,
                       int                              stage,
                       int                              buffer_size);

/**********************************************************************************************************************/

static void
IDirectFBImageProvider_PNG_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_PNG_data *data =
                              (IDirectFBImageProvider_PNG_data*)thiz->priv;

     if (data->thread) {
          /* terminate the decoding thread, if necessary... */
          direct_thread_cancel( data->thread );
          direct_thread_join( data->thread );
          direct_thread_destroy( data->thread );

          pthread_mutex_destroy( &data->lock );
          pthread_cond_destroy( &data->cond );
     }

     if (data->png_ptr) {
          png_destroy_read_struct( &data->png_ptr, &data->info_ptr, NULL );
          data->png_ptr = NULL;
     }

     /* Deallocate image data. */
     if (data->common.image)
          D_FREE( data->common.image );

#if defined(PNG_PROVIDER_USE_MME)
     if (data->common.decode_surface) {
          dfb_gfxcard_wait_serial( &data->common.serial );
          dfb_surface_unref( data->common.decode_surface );
     }
#endif
}

/**********************************************************************************************************************/

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (!png_sig_cmp( ctx->header, 0, 8 ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... )
{
     DFBResult ret = DFB_FAILURE;

     IDirectFBDataBuffer *buffer;
     CoreDFB             *core;
     va_list              tag;

     D_DEBUG_AT( imageProviderPNG, "%s(%d)\n", __FUNCTION__, __LINE__ );

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_PNG)

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );

     data->common.base.ref    = 1;
     data->common.base.buffer = buffer;
     data->common.base.core   = core;

     /* Increase the data buffer reference counter. */
     buffer->AddRef( buffer );

     /* Create the PNG read handle. */
     data->png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING,
                                             NULL, NULL, NULL );
     if (!data->png_ptr)
          goto error;

     if (setjmp( png_jmpbuf(data->png_ptr) )) {
          D_ERROR( "ImageProvider/PNG: Error reading header!\n" );
          goto error;
     }

     /* Create the PNG info handle. */
     data->info_ptr = png_create_info_struct( data->png_ptr );
     if (!data->info_ptr)
          goto error;

     /* Setup progressive image loading. */
     png_set_progressive_read_fn( data->png_ptr, data,
                                  png_info_callback,
                                  png_row_callback,
                                  png_end_callback );

     /* Read until info callback is called. */
     ret = push_data_until_stage( data, STAGE_INFO, 64 );
     if (ret)
          goto error;

     data->common.base.Destruct = IDirectFBImageProvider_PNG_Destruct;

     thiz->SetRenderFlags        = IDirectFBImageProvider_PNG_SetRenderFlags;
     thiz->RenderTo              = IDirectFBImageProvider_PNG_RenderTo;
     thiz->Sync                  = IDirectFBImageProvider_PNG_Sync;
     thiz->GetImageDescription   = IDirectFBImageProvider_PNG_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_PNG_GetSurfaceDescription;

     return DFB_OK;

error:
     if (data->png_ptr)
          png_destroy_read_struct( &data->png_ptr, &data->info_ptr, NULL );

     buffer->Release( buffer );

     if (data->common.image)
          D_FREE( data->common.image );

     DIRECT_DEALLOCATE_INTERFACE(thiz);

     return ret;
}

/**********************************************************************************************************************/

static DFBResult
IDirectFBImageProvider_PNG_SetRenderFlags( IDirectFBImageProvider *thiz,
                                           DIRenderFlags           flags )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_PNG)

     /* if we have decoded the image already, don't do anything... */
     if (data->common.image
#if defined(PNG_PROVIDER_USE_MME)
         || data->common.decode_surface
#endif /* PNG_PROVIDER_USE_MME */
        )
          return DFB_UNSUPPORTED;

     if (!(flags & DIRENDER_NONE) && data->thread) {
          /* terminate the decoding thread, if necessary... */
          direct_thread_cancel( data->thread );
          direct_thread_join( data->thread );
          direct_thread_destroy( data->thread );
          data->thread = NULL;

          pthread_cond_destroy( &data->cond );
          pthread_mutex_destroy( &data->lock );
     }
     else if (flags & DIRENDER_BACKGROUND && !data->thread) {
          /* or create it */
          pthread_cond_init( &data->cond, NULL );
          pthread_mutex_init( &data->lock, NULL );
          /* as long as we haven't even started yet, we are in INIT state */
          data->thread_res = DFB_INIT;
          data->thread = direct_thread_create( DTT_DEFAULT, PNGrenderThread,
                                               thiz, "PNG_DECODER?" );
     }

     data->flags = flags;

     return DFB_OK;
}


static DFBResult
PNG_SoftwareRenderTo( IDirectFBImageProvider_PNG_data *data,
                      CoreSurface                     *dst_surface,
                      DFBSurfacePixelFormat            format,
                      DFBRectangle                    *rect,
                      const DFBRegion                 *clip )
{

     DFBResult    ret = DFB_OK;
     png_infop    info;
     int          x, y;
     DFBRectangle clipped;

     D_ASSERT( data != NULL );
     D_ASSERT( dst_surface != NULL );
     D_ASSERT( clip != NULL );
     D_ASSERT( rect != NULL );

     D_DEBUG_AT(imageProviderPNG,"%s(%d)\n",__FUNCTION__,__LINE__);

     info = data->info_ptr;

     if (setjmp( png_jmpbuf(data->png_ptr) )) {
          D_ERROR( "ImageProvider/PNG: Error during decoding!\n" );

          if (data->stage < STAGE_IMAGE)
               return DFB_FAILURE;

          data->stage = STAGE_ERROR;
     }

     /* Read until image is completely decoded. */
     if (data->stage != STAGE_ERROR) {
          ret = push_data_until_stage( data, STAGE_END, 16384 );
          if (ret)
               return ret;
     }

     clipped = *rect;

     if (!dfb_rectangle_intersect_by_region( &clipped, clip ))
          return DFB_INVAREA;

     /* actual rendering */
     if (0    &&   // FIXME
           rect->w == data->common.width && rect->h == data->common.height &&
         (data->color_type == PNG_COLOR_TYPE_RGB || data->color_type == PNG_COLOR_TYPE_RGBA) &&
         (dst_surface->config.format == DSPF_RGB32 || dst_surface->config.format == DSPF_ARGB) &&
         !(dst_surface->config.caps & DSCAPS_PREMULTIPLIED))
     {
          //ret = dfb_surface_write_buffer( dst_surface, CSBR_BACK,
          //                                data->common.image +
          //                                   (clipped.x - rect->x) * 4 +
          //                                   (clipped.y - rect->y) * data->common.width * 4,
          //                                data->common.width * 4, &clipped );
     }
     else {
          CoreSurfaceBufferLock lock;

          int bit_depth = bit_depth = png_get_bit_depth(data->png_ptr,data->info_ptr);

          ret = dfb_surface_lock_buffer( dst_surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
          if (ret)
               return ret;

          switch (data->color_type) {
               case PNG_COLOR_TYPE_PALETTE:
                   if (dst_surface->config.format == DSPF_LUT8 && bit_depth == 8) {
                         /*
                          * Special indexed PNG to LUT8 loading.
                          */

                         /* FIXME: Limitation for LUT8 is to load complete surface only. */
                         dfb_clip_rectangle( clip, rect );
                         if (rect->x == 0 && rect->y == 0 &&
                             rect->w == dst_surface->config.size.w  &&
                             rect->h == dst_surface->config.size.h &&
                             rect->w == data->common.width         &&
                             rect->h == data->common.height)
                         {
                              for (y = 0; y < data->common.height; y++)
                                   direct_memcpy( (u8*)lock.addr + lock.pitch * y,
                                                  (u8*)data->common.image + data->pitch * y,
                                                  data->common.width );

                              break;
                         }
                    }
                    /* fall through */

               case PNG_COLOR_TYPE_GRAY: {
                    /*
                     * Convert to ARGB and use generic loading code.
                     */
                    if (data->bpp == 16) {
                         /* in 16 bit grayscale,  conversion to RGB32 is already done! */

                         for (x = 0; x < 256; x++)
                              data->palette[x] = 0xff000000 | (x << 16) | (x << 8) | x;

                         dfb_scale_linear_32( data->common.image, data->common.width, data->common.height,
                                              lock.addr, lock.pitch, rect, dst_surface, clip );
                         break;
                    }

                    // FIXME: allocates four additional bytes because the scaling functions
                    //        in src/misc/gfx_util.c have an off-by-one bug which causes
                    //        segfaults on darwin/osx (not on linux)
                    int size = data->common.width * data->common.height * 4 + 4;

                    /* allocate image data */
                    void *image_argb = D_MALLOC( size );

                    if (!image_argb) {
                         D_ERROR( "DirectFB/ImageProvider_PNG: Could not "
                                  "allocate %d bytes of system memory!\n", size );
                         ret = DFB_NOSYSTEMMEMORY;
                    }
                    else {
                         if (data->color_type == PNG_COLOR_TYPE_GRAY) {
                              int num = 1 << bit_depth;

                              for (x = 0; x < num; x++) {
                                   int value = x * 255 / (num - 1);

                                   data->palette[x] = 0xff000000 | (value << 16) | (value << 8) | value;
                              }
                         }

                         switch (bit_depth) {
                              case 8:
                                   for (y = 0; y < data->common.height; y++) {
                                        u8  *S = (u8*)data->common.image + data->pitch * y;
                                        u32 *D = (u32*)((u8*)image_argb  + data->common.width * y * 4);

                                        for (x = 0; x < data->common.width; x++)
                                             D[x] = data->palette[ S[x] ];
                                   }
                                   break;

                              case 4:
                                   for (y = 0; y < data->common.height; y++) {
                                        u8  *S = (u8*)data->common.image + data->pitch * y;
                                        u32 *D = (u32*)((u8*)image_argb  + data->common.width * y * 4);

                                        for (x = 0; x < data->common.width; x++) {
                                             if (x & 1)
                                                  D[x] = data->palette[ S[x>>1] & 0xf ];
                                             else
                                                  D[x] = data->palette[ S[x>>1] >> 4 ];
                                        }
                                   }
                                   break;

                              case 2:
                                   for (y = 0; y < data->common.height; y++) {
                                        int  n = 6;
                                        u8  *S = (u8*)data->common.image + data->pitch * y;
                                        u32 *D = (u32*)((u8*)image_argb  + data->common.width * y * 4);

                                        for (x = 0; x < data->common.width; x++) {
                                             D[x] = data->palette[ (S[x>>2] >> n) & 3 ];

                                             n = (n ? n - 2 : 6);
                                        }
                                   }
                                   break;

                              case 1:
                                   for (y = 0; y < data->common.height; y++) {
                                        int  n = 7;
                                        u8  *S = (u8*)data->common.image + data->pitch * y;
                                        u32 *D = (u32*)((u8*)image_argb  + data->common.width * y * 4);

                                        for (x = 0; x < data->common.width; x++) {
                                             D[x] = data->palette[ (S[x>>3] >> n) & 1 ];

                                             n = (n ? n - 1 : 7);
                                        }
                                   }
                                   break;

                              default:
                                   D_ERROR( "ImageProvider/PNG: Unsupported indexed bit depth %d!\n",
                                            bit_depth );
                         }

                         dfb_scale_linear_32( image_argb, data->common.width, data->common.height,
                                              lock.addr, lock.pitch, rect, dst_surface, clip );

                         D_FREE( image_argb );
                    }
                    break;
               }

               default:
                    /*
                     * Generic loading code.
                     */
                    dfb_scale_linear_32( data->common.image, data->common.width, data->common.height,
                                         lock.addr, lock.pitch, rect, dst_surface, clip );
                    break;
          }

          dfb_surface_unlock_buffer( dst_surface, &lock );
     }

     if (data->stage != STAGE_END)
          ret = DFB_INCOMPLETE;

     return ret;
}


static DFBResult
PNG_RenderTo( IDirectFBImageProvider_PNG_data *data,
              CoreSurface                     *dst_surface,
              DFBSurfacePixelFormat            format,
              DFBRectangle                    *rect,
              const DFBRegion                 *clip )
{
     DFBResult res;

     D_ASSERT( data != NULL );
     D_ASSERT( dst_surface != NULL );
     D_ASSERT( clip != NULL );
     D_ASSERT( rect != NULL );

     res = PNG_HardwareRenderTo (data, dst_surface, format, rect, clip);
     if (res != DFB_OK) {
          if (unlikely (res == DFB_IO)) {
               /* IO error - there's no point in retrying */
               D_DEBUG_AT (HWPNG, "hardware decode failed: %d (%s)\n",
                           res, DirectFBErrorString (res));
               return res;
          }

          if (res == DFB_UNSUPPORTED)
               D_DEBUG_AT (HWPNG, "doing software decode since it's faster "
                           "than hardware for this image\n");
          else
               D_DEBUG_AT (HWPNG, "hardware decode failed: %d (%s) - "
                           "attempting software fallback\n",
                           res, DirectFBErrorString (res));

          res = PNG_SoftwareRenderTo (data, dst_surface, format, rect, clip);
          if (unlikely (res != DFB_OK))
               D_DEBUG_AT (HWPNG, "software decode failed: %d (%s)\n",
                           res, DirectFBErrorString (res));
     }

     return res;
}

static DFBResult
IDirectFBImageProvider_PNG_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *dest_rect )
{
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     DFBSurfacePixelFormat  format;
     DFBResult              ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_PNG)

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
                    pthread_mutex_unlock (&data->lock);
               return DFB_INVARG;
          }

          data->rect = *dest_rect;
          data->rect.x += dst_data->area.wanted.x;
          data->rect.y += dst_data->area.wanted.y;
     }
     else {
          data->rect = dst_data->area.wanted;
     }

     if (!D_FLAGS_IS_SET (data->flags, DIRENDER_BACKGROUND)) {
          /* threaded operation was not requested */
          data->thread_res = PNG_RenderTo( data, dst_surface, format,
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
          data->thread = direct_thread_create( DTT_DEFAULT, PNGrenderThread,
                                               thiz, "PNG" );
     }

     D_ASSERT( data->destination == NULL );

     destination->AddRef( destination );
     data->destination = destination;

     pthread_cond_signal( &data->cond );
     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_PNG_Sync( IDirectFBImageProvider    *thiz,
                                 DFBImageProviderSyncFlags  flags )
{
     DFBResult res;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_PNG)

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
IDirectFBImageProvider_PNG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *dsc )
{
     DFBSurfacePixelFormat primary_format = dfb_primary_layer_pixelformat();

     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_PNG)

     dsc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->width  = data->common.width;
     dsc->height = data->common.height;

     if (data->color_type & PNG_COLOR_MASK_ALPHA)
          dsc->pixelformat = DFB_PIXELFORMAT_HAS_ALPHA(primary_format) ? primary_format : DSPF_ARGB;
     else
          dsc->pixelformat = DSPF_RGB24;

     if (data->color_type == PNG_COLOR_TYPE_PALETTE) {
          dsc->flags |= DSDESC_PALETTE;

          dsc->palette.entries = data->colors;  /* FIXME */
          dsc->palette.size    = 256;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_PNG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *dsc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_PNG)

     if (!dsc)
          return DFB_INVARG;

     dsc->caps = DICAPS_NONE;

     if (data->color_type & PNG_COLOR_MASK_ALPHA)
          dsc->caps |= DICAPS_ALPHACHANNEL;

     if (data->color_keyed) {
          dsc->caps |= DICAPS_COLORKEY;

          dsc->colorkey_r = (data->color_key & 0xff0000) >> 16;
          dsc->colorkey_g = (data->color_key & 0x00ff00) >>  8;
          dsc->colorkey_b = (data->color_key & 0x0000ff);
     }

     return DFB_OK;
}

static void
render_cleanup( void *cleanup_data )
{
     IDirectFBImageProvider           *thiz = cleanup_data;
     IDirectFBImageProvider_PNG_data *data;

     D_MAGIC_ASSERT( (IAny*)thiz, DirectInterface );
     data = (IDirectFBImageProvider_PNG_data *) thiz->priv;
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
PNGrenderThread( DirectThread *thread, void *driver_data )
{
     IDirectFBImageProvider          *thiz = driver_data;
     IDirectFBImageProvider_PNG_data *data;
     IDirectFBSurface_data           *dst_data;
     CoreSurface                     *dst_surface;
     DFBSurfacePixelFormat            format;
     DFBResult                        res;

     D_MAGIC_ASSERT( (IAny*)thiz, DirectInterface );
     data = (IDirectFBImageProvider_PNG_data *) thiz->priv;
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

     res = PNG_RenderTo( data, dst_surface, format, &data->rect, &data->clip );

     pthread_cleanup_pop( 1 );

     /* in case we exit normally, apply the real return value */
     data->thread_res = res;

     return NULL;
}

/**********************************************************************************************************************/

#define MAXCOLORMAPSIZE 256

static int SortColors (const void *a, const void *b)
{
     return (*((const u8 *) a) - *((const u8 *) b));
}

/*  looks for a color that is not in the colormap and ideally not
    even close to the colors used in the colormap  */
static u32 FindColorKey( int n_colors, u8 *cmap )
{
     u32   color = 0xFF000000;
     u8    csort[n_colors];
     int   i, j, index, d;

     if (n_colors < 1)
          return color;

     for (i = 0; i < 3; i++) {
          direct_memcpy( csort, cmap + (n_colors * i), n_colors );
          qsort( csort, n_colors, 1, SortColors );

          for (j = 1, index = 0, d = 0; j < n_colors; j++) {
               if (csort[j] - csort[j-1] > d) {
                    d = csort[j] - csort[j-1];
                    index = j;
               }
          }
          if ((csort[0] - 0x0) > d) {
               d = csort[0] - 0x0;
               index = n_colors;
          }
          if (0xFF - (csort[n_colors - 1]) > d) {
               index = n_colors + 1;
          }

          if (index < n_colors)
               csort[0] = csort[index] - (d/2);
          else if (index == n_colors)
               csort[0] = 0x0;
          else
               csort[0] = 0xFF;

          color |= (csort[0] << (8 * (2 - i)));
     }

     return color;
}

/* Called at the start of the progressive load, once we have image info */
static void
png_info_callback( png_structp png_read_ptr,
                   png_infop   png_info_ptr )
{
     int                              i,ret;
     IDirectFBImageProvider_PNG_data *data;
     png_int_32 width;
     png_int_32 height;

     u32 bpp1[2] = {0, 0xff};
     u32 bpp2[4] = {0, 0x55, 0xaa, 0xff};
     u32 bpp4[16] = {0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

     D_UNUSED_P( png_info_ptr );

     D_DEBUG_AT( imageProviderPNG, "%s(%d)\n", __FUNCTION__, __LINE__ );

     data = png_get_progressive_ptr( png_read_ptr );

     /* error stage? */
     if (data->stage < 0)
          return;

     /* set info stage */
     data->stage = STAGE_INFO;

     ret = png_get_IHDR( data->png_ptr, data->info_ptr,
                         (png_uint_32 *)&width, (png_uint_32 *)&height, &data->bpp, &data->color_type,
                         NULL, NULL, NULL );

     /* Let's not do anything with badly sized or corrupted images */
     if ( (height == 0) || (width == 0) || (ret != 1) )
         return;

     data->common.width = width;
     data->common.height = height;

     if (png_get_valid( data->png_ptr, data->info_ptr, PNG_INFO_tRNS )) {
          data->color_keyed = true;

          /* generate color key based on palette... */
          if (data->color_type == PNG_COLOR_TYPE_PALETTE) {
               u32            key;
               png_colorp     palette;
               png_bytep      trans_alpha;
               png_color_16p  trans_color;
               u8             cmap[3][MAXCOLORMAPSIZE];
               int            num_palette = 0, num_colors = 0, num_trans = 0;

               D_DEBUG_AT( imageProviderPNG, "%s(%d) - num_trans %d\n",
                           __FUNCTION__, __LINE__, num_trans );

               if (png_get_PLTE( data->png_ptr, data->info_ptr, &palette, &num_palette )) {
                    if (png_get_tRNS( data->png_ptr, data->info_ptr,
                                      &trans_alpha, &num_trans, &trans_color )) {
                         num_colors = MIN( MAXCOLORMAPSIZE,num_palette );

                         for (i = 0; i < num_colors; i++) {
                              cmap[0][i] = palette[i].red;
                              cmap[1][i] = palette[i].green;
                              cmap[2][i] = palette[i].blue;
                         }

                         key = FindColorKey( num_colors, &cmap[0][0] );

                         for (i = 0; i < num_trans; i++) {
                              if (!trans_alpha[i]) {
                                   palette[i].red   = (key & 0xff0000) >> 16;
                                   palette[i].green = (key & 0x00ff00) >>  8;
                                   palette[i].blue  = (key & 0x0000ff);
                              }
                         }

                         data->color_key = key;
                    }
               }

          }
          else if (data->color_type == PNG_COLOR_TYPE_GRAY) {
               /* ...or based on trans gray value */
               png_bytep     trans_alpha;
               png_color_16p trans_color;
               int           num_trans = 0;

               D_DEBUG_AT( imageProviderPNG, "%s(%d)\n", __FUNCTION__, __LINE__ );

               if (png_get_tRNS( data->png_ptr, data->info_ptr,
                                 &trans_alpha, &num_trans, &trans_color )) {
                    switch (data->bpp) {
                         case 1:
                              data->color_key = (((bpp1[trans_color[0].gray]) << 16) |
                                                 ((bpp1[trans_color[0].gray]) << 8) |
                                                 ((bpp1[trans_color[0].gray])));
                              break;
                         case 2:
                              data->color_key = (((bpp2[trans_color[0].gray]) << 16) |
                                                 ((bpp2[trans_color[0].gray]) << 8) |
                                                 ((bpp2[trans_color[0].gray])));
                              break;
                         case 4:
                              data->color_key = (((bpp4[trans_color[0].gray]) << 16) |
                                                 ((bpp4[trans_color[0].gray]) << 8) |
                                                 ((bpp4[trans_color[0].gray])));
                              break;
                         case 8:
                              data->color_key = (((trans_color[0].gray & 0x00ff) << 16) |
                                                 ((trans_color[0].gray & 0x00ff) << 8) |
                                                 ((trans_color[0].gray & 0x00ff)));
                              break;
                         case 16:
                         default:
                              data->color_key = (((trans_color[0].gray & 0xff00) << 8) |
                                                 ((trans_color[0].gray & 0xff00)) |
                                                 ((trans_color[0].gray & 0xff00) >> 8));
                              break;
                    }
               }
          }
          else {
               /* ...or based on trans rgb value */
               png_bytep     trans_alpha;
               png_color_16p trans_color;
               int           num_trans = 0;

               D_DEBUG_AT( imageProviderPNG, "%s(%d)\n", __FUNCTION__, __LINE__ );

               if (png_get_tRNS( data->png_ptr, data->info_ptr,
                                 &trans_alpha, &num_trans, &trans_color )) {
                    switch(data->bpp) {
                         case 1:
                              data->color_key = (((bpp1[trans_color[0].red]) << 16) |
                                                 ((bpp1[trans_color[0].green]) << 8) |
                                                 ((bpp1[trans_color[0].blue])));
                              break;
                         case 2:
                              data->color_key = (((bpp2[trans_color[0].red]) << 16) |
                                                 ((bpp2[trans_color[0].green]) << 8) |
                                                 ((bpp2[trans_color[0].blue])));
                              break;
                         case 4:
                              data->color_key = (((bpp4[trans_color[0].red]) << 16) |
                                                 ((bpp4[trans_color[0].green]) << 8) |
                                                 ((bpp4[trans_color[0].blue])));
                              break;
                         case 8:
                              data->color_key = (((trans_color[0].red & 0x00ff) << 16) |
                                                 ((trans_color[0].green & 0x00ff) << 8) |
                                                 ((trans_color[0].blue & 0x00ff)));
                              break;
                         case 16:
                         default:
                              data->color_key = (((trans_color[0].red & 0xff00) << 8) |
                                                 ((trans_color[0].green & 0xff00)) |
                                                 ((trans_color[0].blue & 0xff00) >> 8));
                              break;
                    }
               }
          }
     }

     switch (data->color_type) {
          case PNG_COLOR_TYPE_PALETTE: {
               png_colorp     palette;
               png_bytep      trans_alpha;
               png_color_16p  trans_color;
               int            num_palette = 0, num_colors = 0, num_trans = 0;

               png_get_PLTE( data->png_ptr, data->info_ptr, &palette, &num_palette );

               png_get_tRNS( data->png_ptr, data->info_ptr,
                             &trans_alpha, &num_trans, &trans_color );

               num_colors = MIN( MAXCOLORMAPSIZE, num_palette );

               for (i = 0; i < num_colors; i++) {
                    data->colors[i].a = (i < num_trans) ? trans_alpha[i] : 0xff;
                    data->colors[i].r = palette[i].red;
                    data->colors[i].g = palette[i].green;
                    data->colors[i].b = palette[i].blue;

                    data->palette[i] = PIXEL_ARGB( data->colors[i].a,
                                                   data->colors[i].r,
                                                   data->colors[i].g,
                                                   data->colors[i].b );
               }

               data->pitch = (data->common.width + 7) & ~7;
               break;
          }

          case PNG_COLOR_TYPE_GRAY:
               if (data->bpp < 16) {
                    data->pitch = data->common.width;
                    break;
               }

               /* fall through */
          case PNG_COLOR_TYPE_GRAY_ALPHA:
               png_set_gray_to_rgb( data->png_ptr );

               /* fall through */
          default:
               data->pitch = data->common.width * 4;

               if (!data->color_keyed)
                    png_set_strip_16( data->png_ptr ); /* if it is color keyed we will handle conversion ourselves */

#ifdef WORDS_BIGENDIAN
               if (!(data->color_type & PNG_COLOR_MASK_ALPHA))
                    png_set_filler( data->png_ptr, 0xFF, PNG_FILLER_BEFORE );

               png_set_swap_alpha( data->png_ptr );
#else
               if (!(data->color_type & PNG_COLOR_MASK_ALPHA))
                    png_set_filler( data->png_ptr, 0xFF, PNG_FILLER_AFTER );

               png_set_bgr( data->png_ptr );
#endif
               break;
     }

     png_set_interlace_handling( data->png_ptr );

     /* Update the info to reflect our transformations */
     png_read_update_info( data->png_ptr, data->info_ptr );
}

/* Called for each row; note that you will get duplicate row numbers
   for interlaced PNGs */
static void
png_row_callback( png_structp png_read_ptr,
                  png_bytep   new_row,
                  png_uint_32 row_num,
                  int         pass_num )
{
     IDirectFBImageProvider_PNG_data *data;

     D_DEBUG_AT( imageProviderPNG, "%s(%d)\n", __FUNCTION__, __LINE__ );

     data = png_get_progressive_ptr( png_read_ptr );

     /* error stage? */
     if (data->stage < 0)
          return;

     D_UNUSED_P( pass_num );

     /* set image decoding stage */
     data->stage = STAGE_IMAGE;

     /* check image data pointer */
     if (!data->common.image) {
          // FIXME: allocates four additional bytes because the scaling functions
          //        in src/misc/gfx_util.c have an off-by-one bug which causes
          //        segfaults on darwin/osx (not on linux)
          int size = data->pitch * data->common.height + 4;

          /* allocate image data */
          data->common.image = D_CALLOC( 1, size );
          if (!data->common.image) {
               D_ERROR( "DirectFB/ImageProvider_PNG: Could not "
                        "allocate %d bytes of system memory!\n", size );

               /* set error stage */
               data->stage = STAGE_ERROR;

               return;
          }
     }

     /* write to image data */
     if (data->bpp == 16 && data->color_keyed) {
          u8 *dst = (u8*)((u8*)data->common.image + row_num * data->pitch);
          u8 *src = (u8*)new_row;

          if (src) {
               int src_advance = 8;
               int src16_advance = 4;
               int dst32_advance = 1;
               int src16_initial_offset = 0;
               int dst32_initial_offset = 0;

               if (!(row_num % 2)) { /* even lines 0,2,4 ... */
                    switch (pass_num) {
                         case 1:
                              dst32_initial_offset = 4;
                              src16_initial_offset = 16;
                              src_advance = 64;
                              src16_advance = 32;
                              dst32_advance = 8;
                              break;
                         case 3:
                              dst32_initial_offset = 2;
                              src16_initial_offset = 8;
                              src_advance = 32;
                              src16_advance = 16;
                              dst32_advance = 4;
                              break;
                         case 5:
                              dst32_initial_offset = 1;
                              src16_initial_offset = 4;
                              src_advance = 16;
                              src16_advance = 8;
                              dst32_advance = 2;
                              break;
                         default:
                              break;
                    }
               }


               png_bytep      trans;
               png_color_16p  trans_color;
               int            num_trans = 0;

               png_get_tRNS( data->png_ptr, data->info_ptr,
                             &trans, &num_trans, &trans_color );

               u16 *src16 = (u16*)src + src16_initial_offset;
               u32 *dst32 = (u32*)dst + dst32_initial_offset;

               int remaining = data->common.width - dst32_initial_offset;

               while (remaining > 0) {
                    int keyed = 0;
#ifdef WORDS_BIGENDIAN
                    u16 comp_r = src16[1];
                    u16 comp_g = src16[2];
                    u16 comp_b = src16[3];
                    u32 pixel32 = src[1] << 24 | src[3] << 16 | src[5] << 8 | src[7];
#else
                    u16 comp_r = src16[2];
                    u16 comp_g = src16[1];
                    u16 comp_b = src16[0];

                    u32 pixel32 = src[6] << 24 | src[4] << 16 | src[2] << 8 | src[0];
#endif
                    /* is the pixel supposted to match the color key in 16 bit per channel resolution? */
                    if (((comp_r == trans_color[0].gray) && (data->color_type == PNG_COLOR_TYPE_GRAY)) ||
                        ((comp_g == trans_color[0].green) && (comp_b == trans_color[0].blue) && (comp_r == trans_color[0].red)))
                         keyed = 1;

                    /*
                     *  if the pixel was not supposed to get keyed but the colorkey matches in the reduced
                     *  color space, then toggle the least significant blue bit
                     */
                    if (!keyed && (pixel32 == (0xff000000 | data->color_key))) {
                         D_ONCE( "ImageProvider/PNG: adjusting pixel data to protect it from being keyed!\n" );
                         pixel32 ^= 0x00000001;
                    }

                    *dst32 = pixel32;

                    src16 += src16_advance;
                    src   += src_advance;
                    dst32 += dst32_advance;
                    remaining-= dst32_advance;
               }
          }
     }
     else
         png_progressive_combine_row( data->png_ptr, (png_bytep)((u8*)data->common.image + row_num * data->pitch), new_row );

     /* increase row counter, FIXME: interlaced? */
     data->rows++;

     if (data->common.base.render_callback) {
          DIRenderCallbackResult r;
          DFBRectangle rect = { 0, row_num, data->common.width, 1 };

          r = data->common.base.render_callback( &rect,
                                                 data->common.base.render_callback_context );
          if (r != DIRCR_OK)
               data->stage = STAGE_ABORT;
     }
}

/* Called after reading the entire image */
static void
png_end_callback   (png_structp png_read_ptr,
                    png_infop   png_info_ptr)
{
     IDirectFBImageProvider_PNG_data *data;

     D_UNUSED_P( png_info_ptr );

     D_DEBUG_AT( imageProviderPNG, "%s(%d)\n", __FUNCTION__, __LINE__ );

     data = png_get_progressive_ptr( png_read_ptr );

     /* error stage? */
     if (data->stage < 0)
          return;

     /* set end stage */
     data->stage = STAGE_END;
}

/* Pipes data into libpng until stage is different from the one specified. */
static DFBResult
push_data_until_stage (IDirectFBImageProvider_PNG_data *data,
                       int                              stage,
                       int                              buffer_size)
{
     DFBResult            ret;
     IDirectFBDataBuffer *buffer = data->common.base.buffer;

     while (data->stage < stage) {
          unsigned int  len;
          unsigned char buf[buffer_size];

          if (data->stage < 0)
               return DFB_FAILURE;

          while (buffer->HasData( buffer ) == DFB_OK) {
               D_DEBUG_AT( imageProviderPNG,
                           "Retrieving data (up to %d bytes)...\n",
                           buffer_size );

               ret = buffer->GetData( buffer, buffer_size, buf, &len );
               if (ret)
                    return ret;

               D_DEBUG_AT( imageProviderPNG, "  -> got %d bytes\n", len );

               png_process_data( data->png_ptr, data->info_ptr, buf, len );

               D_DEBUG_AT( imageProviderPNG, "  -> processed %d bytes\n", len );

               /* are we there yet? */
               if (data->stage < 0 || data->stage >= stage) {
                    switch (data->stage) {
                         case STAGE_ABORT: return DFB_INTERRUPTED;
                         case STAGE_ERROR: return DFB_FAILURE;
                         default:          return DFB_OK;
                    }
               }
          }

          D_DEBUG_AT( imageProviderPNG, "Waiting for data...\n" );

          if (buffer->WaitForData( buffer, 1 ) == DFB_EOF)
               return DFB_FAILURE;
     }

     return DFB_OK;
}



#if defined(PNG_PROVIDER_USE_MME)
static void
TransformerCallback (MME_Event_t    Event,
                     MME_Command_t *CallbackData,
                     void          *UserData)
{
  static const char *color_types[] = { "gray", "?", "rgb", "palette",
                                       "alpha", "?", "rgba" };
  static const char *interlace_types[] = { "none", "adam7" };

  const PNGDecode_TransformReturnParams_t * const transform_result =
    CallbackData->CmdStatus.AdditionalInfo_p;
  const PNGDecode_GlobalTransformReturnParams_t * const global_transform_result =
    CallbackData->CmdStatus.AdditionalInfo_p;
  struct _MMECommon * const mme = (struct _MMECommon *) UserData;
  IDirectFBImageProvider_PNG_data * const data =
    container_of (mme, IDirectFBImageProvider_PNG_data, common);

  if (!mme->name_set)
    {
      char name[20];
      snprintf (name, sizeof (name), "MME (%s)", mme->transformer_name);
      direct_thread_set_name (name);
      mme->name_set = true;
    }

  D_DEBUG_AT (HWPNG, "%sTransformerCallback: Event: %d: (%s)%s\n",
              RED, Event, get_mme_event_string (Event), BLACK);
  D_DEBUG_AT (HWPNG, "  -> CallbackData->CmdStatus.State: %d (%s)\n",
              CallbackData->CmdStatus.State,
              get_mme_state_string (CallbackData->CmdStatus.State));
  D_DEBUG_AT (HWPNG, "  -> CallbackData->CmdStatus.CmdId: %u (%.8x)\n",
              CallbackData->CmdStatus.CmdId,
              CallbackData->CmdStatus.CmdId);
  D_DEBUG_AT (HWPNG, "  -> CallbackData->CmdStatus.Error: %d (%s)\n",
              CallbackData->CmdStatus.Error,
              get_mme_error_string (CallbackData->CmdStatus.Error));
  D_DEBUG_AT (HWPNG, "  -> CallbackData->CmdStatus.AdditionalInfoSize: %u\n",
              CallbackData->CmdStatus.AdditionalInfoSize);
  D_DEBUG_AT (HWPNG, "  -> mme->decode_success: %d\n", mme->decode_success);

  switch (Event)
    {
    case MME_COMMAND_COMPLETED_EVT:
      pthread_mutex_lock (&mme->pending_commands_lock);
      direct_hash_remove (mme->pending_commands,
                          CallbackData->CmdStatus.CmdId);
      if (likely (CallbackData->CmdStatus.CmdId != mme->TransformCommand.CmdStatus.CmdId
                  && CallbackData->CmdStatus.CmdId != data->SetGlobalCommand.CmdStatus.CmdId))
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
              D_DEBUG_AT (HWPNG_TIME, "  -> total time %lu.%06lu\n",
                          mme->endtime.tv_sec, mme->endtime.tv_usec);

              D_DEBUG_AT (HWPNG, "  -> expanded bytes: %u\n",
                          transform_result->BytesWritten);

              D_DEBUG_AT (HWPNG, "  -> PNG result code: %d (%s)\n",
                          transform_result->ErrorType,
                          get_png_error_string (transform_result->ErrorType));

              if (transform_result->Cycles)
                {
                  D_DEBUG_AT (HWPNG, "  -> profiling data: cyc/bu/i/d/n: "
                                     "%u/%u/%u/%u/%u\n",
                              transform_result->Cycles,
                              transform_result->Bundles,
                              transform_result->ICacheMiss,
                              transform_result->DCacheMiss,
                              transform_result->NopBundles);
                }

              mme->decode_success = 1;
              sema_signal_event (&mme->decode_event);

              /* in case of bogus data (too much), abort pending data
                 buffers */
              abort_transformer (mme);
            }
          else if (unlikely (CallbackData->CmdStatus.CmdId
                             == data->SetGlobalCommand.CmdStatus.CmdId))
            {
              D_DEBUG_AT (HWPNG, "  -> size: %ux%u @ %ubpp\n",
                          global_transform_result->PictureWidth,
                          global_transform_result->PictureHeight,
                          global_transform_result->BitDepth);
              D_DEBUG_AT (HWPNG, "  -> ColorType      : %d (%s)\n",
                          global_transform_result->ColorType,
                          (global_transform_result->ColorType
                           < D_ARRAY_SIZE (color_types))
                          ? color_types[global_transform_result->ColorType]
                          : "?" );
              D_DEBUG_AT (HWPNG, "  -> InterlaceType  : %d (%s)\n",
                          global_transform_result->InterlaceType,
                          (global_transform_result->InterlaceType
                           < D_ARRAY_SIZE (interlace_types))
                          ? interlace_types[global_transform_result->InterlaceType]
                          : "?");
              D_DEBUG_AT (HWPNG, "  -> CompressionType: %d\n", global_transform_result->CompressionType);
              D_DEBUG_AT (HWPNG, "  -> FilterMethod   : %d\n", global_transform_result->FilterMethod);
              D_DEBUG_AT (HWPNG, "  -> ErrorType      : %d (%s)\n", global_transform_result->ErrorType, get_png_error_string (global_transform_result->ErrorType));

              D_DEBUG_AT (HWPNG, "  -> ColorFormatOutput: %d\n", global_transform_result->ColorFormatOutput);
              D_DEBUG_AT (HWPNG, "  -> ColorKey: %c\n", global_transform_result->HaveColorKey ? 'y' : 'n');
              if (global_transform_result->HaveColorKey)
                  D_DEBUG_AT (HWPNG, "  -> ColorKey (xRGB): %.6x\n", global_transform_result->ColorKey);
              D_DEBUG_AT (HWPNG, "  -> pitch: %d\n", global_transform_result->pitch);

              sema_signal_event (&data->global_event);
            }
          else
            {
              /* buffer completed */
              struct _MMEHelper_buffer *buf =
                container_of (CallbackData->DataBuffers_p,
                              struct _MMEHelper_buffer, buffer);

              D_DEBUG_AT (HWPNG, "  -> buffer %p completed (container @ %p)\n",
                          CallbackData->DataBuffers_p[0], buf);

              sema_signal_event (&buf->sema);
            }
          break; /* MME_COMMAND_COMPLETED */

        case MME_COMMAND_FAILED:
          if (CallbackData->CmdStatus.Error != MME_COMMAND_ABORTED)
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
              D_DEBUG_AT (HWPNG, "  -> transform->ErrorType: %d (%s)\n",
                          transform_result->ErrorType,
                          get_png_error_string (transform_result->ErrorType));

              mme->decode_success = -1;
              sema_signal_event (&mme->decode_event);
            }
          else if (likely (CallbackData->CmdStatus.CmdId
                   == data->SetGlobalCommand.CmdStatus.CmdId))
            {
              /* global transform params command failed */
              D_DEBUG_AT (HWPNG, "  -> global->ErrorType: %d (%s)\n",
                          global_transform_result->ErrorType,
                          get_png_error_string (global_transform_result->ErrorType));

              mme->decode_success = -1;
              sema_signal_event (&data->global_event);
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
      ++mme->n_underflows;
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
          D_DEBUG_AT (HWPNG, "  -> no more data available, signalling fail\n");
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
          D_WARN ("(%5d) %s: transform_result->bytes_written: %u",
                  direct_gettid (), mme->name, transform_result->BytesWritten);
          D_WARN ("(%5d) %s: result code: %d (%s)\n", direct_gettid (),
                  mme->name, transform_result->ErrorType,
                  get_png_error_string (transform_result->ErrorType));
        }
      else if (CallbackData->CmdStatus.CmdId == data->SetGlobalCommand.CmdStatus.CmdId)
        {
          D_WARN ("(%5d) %s: size: %ux%u @ %ubpp\n",
                  direct_gettid (), mme->name,
                  global_transform_result->PictureWidth,
                  global_transform_result->PictureHeight,
                  global_transform_result->BitDepth);
          D_WARN ("(%5d) %s: ColorType      : %d (%s)\n",
                  direct_gettid (), mme->name,
                  global_transform_result->ColorType,
                  (global_transform_result->ColorType
                   < D_ARRAY_SIZE (color_types))
                  ? color_types[global_transform_result->ColorType]
                  : "?" );
          D_WARN ("(%5d) %s: InterlaceType  : %d (%s)\n",
                  direct_gettid (), mme->name,
                  global_transform_result->InterlaceType,
                      (global_transform_result->InterlaceType
                       < D_ARRAY_SIZE (interlace_types))
                      ? interlace_types[global_transform_result->InterlaceType]
                      : "?");
          D_WARN ("(%5d) %s: CompressionType: %d\n", direct_gettid (), mme->name, global_transform_result->CompressionType);
          D_WARN ("(%5d) %s: FilterMethod   : %d\n", direct_gettid (), mme->name, global_transform_result->FilterMethod);
          D_WARN ("(%5d) %s: ErrorType      : %d (%s)\n", direct_gettid (), mme->name, global_transform_result->ErrorType, get_png_error_string (global_transform_result->ErrorType));

          D_DEBUG_AT (HWPNG, "  -> ColorFormatOutput: %d\n", global_transform_result->ColorFormatOutput);
          D_DEBUG_AT (HWPNG, "  -> ColorKey: %c\n", global_transform_result->HaveColorKey ? 'y' : 'n');
          if (global_transform_result->HaveColorKey)
              D_DEBUG_AT (HWPNG, "  -> ColorKey (xRGB): %.6x\n", global_transform_result->ColorKey);
          D_DEBUG_AT (HWPNG, "  -> pitch: %d\n", global_transform_result->pitch);
        }
      break;
    }
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
  __attribute__((unused))
  IDirectFBImageProvider_PNG_data * const data =
    container_of (mme, IDirectFBImageProvider_PNG_data, common);
  MME_CommandId_t    CmdId = key;

  D_DEBUG_AT (HWPNG, "aborting %s command %u (%.8x)\n",
              ((CmdId == mme->TransformCommand.CmdStatus.CmdId)
               ? "(main transform)"
               : ((CmdId == data->SetGlobalCommand.CmdStatus.CmdId)
                  ? "(global)"
                  : "(buffer)"
                 )
              ), CmdId, CmdId);

  MME_ERROR res = MME_AbortCommand (mme->Handle, CmdId);
  if (res != MME_SUCCESS)
    {
      /* this would mean the command is about to finish, but we didn't
         notice yet... */
      D_WARN ("(%5d) MME_AbortCommand(%x, %.8x) for %s failed: %d (%s)\n",
              direct_gettid (), mme->Handle, CmdId,
              mme->name, res, get_mme_error_string (res));
    }

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


static DFBResult
set_global_transform_params (IDirectFBImageProvider_PNG_data *data)
{
  MME_ERROR ret;

  data->SetGlobalCommand.StructSize          = sizeof (MME_Command_t);
  data->SetGlobalCommand.CmdCode             = MME_SET_GLOBAL_TRANSFORM_PARAMS;
  data->SetGlobalCommand.CmdEnd              = MME_COMMAND_END_RETURN_NOTIFY;
  data->SetGlobalCommand.DueTime             = (MME_Time_t) 0;
  data->SetGlobalCommand.NumberInputBuffers  = 0;
  data->SetGlobalCommand.NumberOutputBuffers = 0;
  data->SetGlobalCommand.DataBuffers_p       = NULL;

  /* clear the commandstatus from the previous run */
  memset (&( data->SetGlobalCommand.CmdStatus), 0, sizeof (MME_CommandStatus_t));

  data->SetGlobalCommand.CmdStatus.AdditionalInfoSize = sizeof (PNGDecode_GlobalTransformReturnParams_t);
  data->SetGlobalCommand.CmdStatus.AdditionalInfo_p = &data->GlobalReturnParams;

  data->SetGlobalCommand.NumberInputBuffers = 0;
  data->SetGlobalCommand.NumberOutputBuffers = 0;

  /* set parameters - just settings flags to 0 should make this compatible
     with old transformer versions, 0 means PNGDECODE_OF_EXPAND */
//  data->GlobalParams.flags = PNGDECODE_PARAM_FORMAT;
//  data->GlobalParams.format = PNGDECODE_OF_EXPAND;
//  data->SetGlobalCommand.ParamSize = sizeof (PNGDecode_GlobalParams_t);
//  data->SetGlobalCommand.Param_p   = &data->GlobalParams;
  data->SetGlobalCommand.ParamSize = 0;
  data->SetGlobalCommand.Param_p   = NULL;

  D_DEBUG_AT (HWPNG, "sending global command\n");

  pthread_mutex_lock (&data->common.pending_commands_lock);
  ret = MME_SendCommand (data->common.Handle, &data->SetGlobalCommand);
  if (ret != MME_SUCCESS)
    {
      pthread_mutex_unlock (&data->common.pending_commands_lock);
      D_INFO ("%s: MME_SendCommand() failed: %d (%s)\n",
              data->common.name, ret, get_mme_error_string (ret));
      return DFB_FAILURE;
    }

  direct_hash_insert (data->common.pending_commands,
                      data->SetGlobalCommand.CmdStatus.CmdId,
                      (void *) 1);
  D_DEBUG_AT (HWPNG, "sent packet's CmdId is %u (%.8x)\n",
              data->SetGlobalCommand.CmdStatus.CmdId,
              data->SetGlobalCommand.CmdStatus.CmdId);
  pthread_mutex_unlock (&data->common.pending_commands_lock);

  return DFB_OK;
}


static void
_imageprovider_update_transform_params (struct _MMECommon           * const mme,
                                        void                        * const params,
                                        const CoreSurfaceBufferLock * const lock)
{
  IDirectFBImageProvider_PNG_data * const data =
    container_of (mme, IDirectFBImageProvider_PNG_data, common);

  if (data->GlobalReturnParams.pitch != lock->pitch)
    {
      PNGDecode_TransformParams_t * const p = params;

      p->flags |= PNGDECODE_PARAM_PITCH;
      p->pitch = lock->pitch;
    }
}


#define HWREND_THRESHOLD 20000

static pthread_mutex_t transformer_index_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int transformer_index;
static DFBResult
PNG_HardwareRenderTo( IDirectFBImageProvider_PNG_data *data,
                      CoreSurface                     *dst_surface,
                      DFBSurfacePixelFormat            format,
                      DFBRectangle                    *rect,
                      const DFBRegion                 *clip )
{
  DFBResult              dfbret = dfbret;
  CoreSurfaceBufferLock  lock;
  unsigned int           bufpos = 0;

  D_ASSERT( data != NULL );
  D_ASSERT( dst_surface != NULL);
  D_ASSERT( rect != NULL );
  D_ASSERT( clip != NULL );

  /* FIXME: check if the image is big enough to warrant a HW decode */
  if ((dst_surface->config.size.w * dst_surface->config.size.h) < HWREND_THRESHOLD)
    return DFB_UNSUPPORTED;

  /* only decode if we haven't decoded before */
  if (data->common.decode_success <= 0)
    {
      MME_TransformerCapability_t cap;
      bool                        surface_locked = false;

      data->common.name = "hwPNG";
      static const char  *transformers0[] = { PNGDECODER_MME_TRANSFORMER_NAME"0", NULL };
      static const char  *transformers1[] = { PNGDECODER_MME_TRANSFORMER_NAME"1", NULL };
      static const char  *transformers2[] = { PNGDECODER_MME_TRANSFORMER_NAME"2", NULL };
      static const char  *transformers3[] = { PNGDECODER_MME_TRANSFORMER_NAME"3", NULL };
      static const char  *transformers4[] = { PNGDECODER_MME_TRANSFORMER_NAME"4", NULL };
      static const char  *transformers5[] = { PNGDECODER_MME_TRANSFORMER_NAME, NULL };
      static const char **transformers[] = { transformers0, transformers1,
                                             transformers2, transformers3,
                                             transformers4, transformers5 };

      unsigned i;
      transformer_index = -1;
      pthread_mutex_lock (&transformer_index_mutex);
      for (i = 0; i < D_ARRAY_SIZE (transformers); ++i)
        {
          ++transformer_index;
          transformer_index %= D_ARRAY_SIZE (transformers);

          PNGDecode_InitTransformerParams_t png_init;
          png_init.flags = PNGDECODE_PARAM_FORMAT;
          png_init.format = PNGDECODE_OF_EXPAND;
          dfbret = mme_helper_init_transformer (&data->common,
                                                transformers[transformer_index],
                                                sizeof (png_init),
                                                &png_init, NULL);
          if (dfbret == DFB_OK)
            {
              /* need to init this because old transformer versions didn't
                 touch it at all. */
              cap.Version = 0;
              _mme_helper_get_capability (&data->common,
                                          *transformers[transformer_index],
                                          &cap);

              if (cap.Version != 11)
                {
                  /* incompatible with old transformer, it has just way too
                     many bugs! */
                  mme_helper_deinit_transformer (&data->common);
                  dfbret = DFB_FAILURE;
                  continue;
                }

              data->common.transformer_name = *transformers[transformer_index];
              if (data->flags & DIRENDER_BACKGROUND)
                direct_thread_set_name (*transformers[transformer_index]);

              break;
            }
        }
      pthread_mutex_unlock (&transformer_index_mutex);

      if (dfbret != DFB_OK)
        return dfbret;

      sema_init_event (&data->common.decode_event, 0); /* Alerts to completion */
      sema_init_event (&data->global_event, 0);

      {
      /* store the current data buffer position for software fallback should
         we not succeed */
      data->common.base.buffer->GetPosition (data->common.base.buffer,
                                             &bufpos);

      /* as an optimization we first send one packet, otherwise
         SET_GLOBAL_TRANSFORM_COMMAND would through a data underflow event */
      mme_helper_calculate_packets (&data->common);
      dfbret = mme_helper_send_packets (&data->common, 1);
      if (dfbret != DFB_OK)
        goto err_abort_commands;

      dfbret = set_global_transform_params (data);
      if (dfbret != DFB_OK)
        goto err_abort_commands;

      D_DEBUG_AT (HWPNG, "  -> waiting for global transform to finish\n");
      while ((sema_wait_event (&data->global_event) == -1)
             && errno == EINTR)
        ;
      D_DEBUG_AT (HWPNG, "    -> global transform finished (failed: %c)\n",
                  (data->common.decode_success != 0) ? 'y' : 'n');

      /* the transformer encountered an error */
      if (data->common.decode_success != 0)
        {
          dfbret = DFB_FAILURE;
          goto err_abort_commands;
        }

      if (!data->common.decode_surface)
        {
          /* create a destination surface to MME-decode the image onto before
             stretchblit ... and if we can't create a surface in video
             memory then we can't blit */
          CoreSurfaceConfig  config;
          CorePalette       *palette = NULL;

          switch (data->GlobalReturnParams.ColorFormatOutput)
            {
            case PNGDECODE_COLOR_TYPE_PALETTE:
              config.format = DSPF_LUT8;

              dfbret = dfb_palette_create (data->common.base.core,
                                           256, &palette);
              if (dfbret != DFB_OK)
                goto err_abort_commands;

              direct_memcpy (palette->entries,
                             data->GlobalReturnParams.palette,
                             256 * sizeof (DFBColor));
              dfb_palette_update (palette, 0, 256 - 1);
              break;

            case PNGDECODE_COLOR_TYPE_RGB:
              config.format = DSPF_RGB24;
              break;

            case PNGDECODE_COLOR_TYPE_RGB_ALPHA:
            default:
              config.format = DSPF_ARGB;
              break;
            }
          config.flags = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
          config.size.w = data->common.width;
          config.size.h = data->common.height;
          config.caps   = DSCAPS_VIDEOONLY;
          #ifdef DIRECT_BUILD_DEBUG
          config.caps   |= DSCAPS_SHARED;
          #endif

          dfbret = dfb_surface_create (data->common.base.core, &config,
                                       CSTF_NONE, 0, palette,
                                       &data->common.decode_surface);
          if (palette)
            dfb_palette_unref (palette);

          if (dfbret != DFB_OK)
            goto err_abort_commands;
        }

      /* tell the transformer what to do and where to put the result */
      data->TransformParams.flags = 0;
      dfbret = mme_helper_start_transformer (&data->common,
                                             sizeof (data->ReturnParams),
                                             &data->ReturnParams,
                                             sizeof (data->TransformParams),
                                             &data->TransformParams,
                                             data->common.decode_surface,
                                             &lock);
      if (dfbret != DFB_OK)
        {
          D_DEBUG_AT (HWPNG, "couldn't start transformer\n");
          goto err_abort_commands;
        }

      surface_locked = true;

      /* Now send the rest of the data */
      D_DEBUG_AT (HWPNG, "Transform sent, sending data buffers now\n");
      dfbret = mme_helper_send_packets (&data->common, -1);
      if (dfbret != DFB_OK)
        {
          D_DEBUG_AT (HWPNG, "couldn't send all buffers\n");
          goto err_abort_commands;
        }

      /* wait until the decode is complete */
      D_DEBUG_AT (HWPNG, "waiting for completion\n");
      while ((sema_wait_event (&data->common.decode_event) == -1)
             && errno == EINTR)
        ;
      /* the transformer encountered an error */
      if (data->common.decode_success != 1)
        {
          dfbret = DFB_FAILURE;
          goto err_abort_commands;
        }
      }

err_abort_commands:
      {
        /* we need to do this explicitly here because the generic code will
           not call abort_transformer() if no TransformCommand is pending.
           Unfortunately in the PNG case, we always send a buffer before the
           transform command. This buffer would otherwise not be aborted.
           Should probably change the code to be more in line with JPEG. */
        abort_transformer (&data->common);
        /* wait till all commands are aborted and acknowledged */
        while (data->common.n_pending_buffers
               || direct_hash_lookup (data->common.pending_commands,
                                      data->common.TransformCommand.CmdStatus.CmdId))
          usleep (1);

      if (surface_locked)
        {
          mme_helper_deinit_transformer (&data->common);

          dfb_surface_unlock_buffer (data->common.decode_surface, &lock);
        }


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
      sema_close_event (&data->global_event);

      if (data->common.decode_success != 1)
        {
          /* if we saw corrupt data, there's no point in trying libpng, just
             display the result as far as possible. */
          if (data->common.TransformCommand.CmdStatus.State == MME_COMMAND_FAILED
              && data->common.TransformCommand.CmdStatus.Error == MME_INVALID_ARGUMENT)
            {
              const PNGDecode_TransformReturnParams_t * __restrict r =
                &data->ReturnParams;

              switch (r->ErrorType)
                {
                case PNGDECODE_CRC_ERROR:
                case PNGDECODE_INVALID_STREAM:
                case PNGDECODE_STREAM_ERROR:
                  /* just assume invalid data and display what we have */
                  D_DEBUG_AT (HWPNG, "faking success due to corrupt data\n");
                  data->common.decode_success = 1;

                default:
                  break;
                }
            }
          else if (dfbret == DFB_IO)
            {
              D_DEBUG_AT (HWPNG, "faking success due to EOF\n");
              dfbret = DFB_OK;
              data->common.decode_success = 1;
            }

          if (data->common.decode_success != 1)
            data->common.base.buffer->SeekTo (data->common.base.buffer,
                                              bufpos);
        }
      }
    }


  if (data->common.decode_success == 1)
    {
      /* stretch blit the decoded image to the destination surface and size */
      mme_helper_stretch_blit (&data->common, data->common.decode_surface,
                               dst_surface, rect);
      dfbret = DFB_OK;

      if (data->GlobalReturnParams.HaveColorKey)
        {
          data->color_keyed = true;
          data->color_key = data->GlobalReturnParams.ColorKey;
        }
    }

  return dfbret;
}
#endif /* PNG_PROVIDER_USE_MME */
