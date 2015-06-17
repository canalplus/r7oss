/*
   (c) Copyright 2012 STMicroelectronics R&D

based on code:
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Ilyes Gouta <ilyes.gouta@st.com>
   Based on code by:
              Andre' Draszik <andre.draszik@st.com>,
              Denis Oliver Kropp <dok@directfb.org>,
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
#include <sys/ioctl.h>

#include <directfb.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/layers.h>

#include <core/CoreSurface.h>
#include <core/palette.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/thread.h>
#include <direct/messages.h>

#include <misc/gfx_util.h>
#include <misc/util.h>

#include <linux/videodev2.h>

#include "idirectfbimageprovider_hwjpeg_v4l2.h"

D_DEBUG_DOMAIN (HWJPEG_V4L2, "HWJPEG_V4L2", "STM h/w JPEG V4L2 decoder");

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, hwJPEG )

static DFBResult
IDirectFBImageProvider_hwJPEG_RenderTo( IDirectFBImageProvider *thiz,
                                        IDirectFBSurface       *destination,
                                        const DFBRectangle     *destination_rect );

static DFBResult
IDirectFBImageProvider_hwJPEG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                     DFBSurfaceDescription  *dsc );

static DFBResult
IDirectFBImageProvider_hwJPEG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                   DFBImageDescription    *dsc );

static DFBResult
IDirectFBImageProvider_hwJPEG_SetRenderFlags( IDirectFBImageProvider *thiz,
                                              DIRenderFlags flags );

static DFBResult
JPEG_hwRenderTo( IDirectFBImageProvider_hwJPEG_data *data,
                 CoreSurface                        *dst_surface,
                 DFBRectangle                       *rect,
                 const DFBRegion                    *clip );

static void
IDirectFBImageProvider_hwJPEG_Destruct( IDirectFBImageProvider *thiz )
{
    IDirectFBImageProvider_hwJPEG_data *data =
            (IDirectFBImageProvider_hwJPEG_data*)thiz->priv;

     if (data->decode_surface) {
          dfb_gfxcard_wait_serial( &data->serial );
          dfb_surface_unref( data->decode_surface );
          data->decode_surface = NULL;
     }
}

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     /* Look of the Jpeg SOI marker */
     if (ctx->header[0] == 0xff && ctx->header[1] == 0xd8) {
          /* Look for JFIF or Exif strings, also could look at header[3:2] for APP0(0xFFE0),
           * APP1(0xFFE1) or even other APPx markers.
           */
          if (strncmp ((char*) ctx->header + 6, "JFIF", 4) == 0 ||
              strncmp ((char*) ctx->header + 6, "Exif", 4) == 0 ||
              strncmp ((char*) ctx->header + 6, "VVL", 3) == 0 ||
              strncmp ((char*) ctx->header + 6, "WANG", 4) == 0)
               return DFB_OK;

          /* Else look for Quantization table marker or Define Huffman table marker,
           * useful for EXIF thumbnails that have no APPx markers.
           */
          if (ctx->header[2] == 0xff && (ctx->header[3] == 0xdb || ctx->header[3] == 0xc4))
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
     IDirectFBDataBuffer *buffer;
     CoreDFB             *core;
     va_list              tag;
     struct v4l2_fmtdesc  fmtdesc;

     DFBRegion region = {
          .x1 = 0,
          .y1 = 0,
          .x2 = 255,
          .y2 = 255
     };

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBImageProvider_hwJPEG )

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );

     D_ASSERT( core != NULL );

     data->fd = open( "/dev/video0", O_RDWR );

     if (data->fd < 0) {
         D_DEBUG_AT( HWJPEG_V4L2,
                     "%s: couldn't open /dev/video0\n",
                     __FUNCTION__ );
         DIRECT_DEALLOCATE_INTERFACE( thiz );
         return DFB_FAILURE;
     }

     memset( &fmtdesc, 0, sizeof(fmtdesc) );

     fmtdesc.index = 0;
     fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

     if (ioctl( data->fd, VIDIOC_ENUM_FMT, &fmtdesc ) < 0) {
          D_DEBUG_AT( HWJPEG_V4L2,
                      "%s: couldn't enumerate output formats on /dev/video0\n",
                      __FUNCTION__ );
          close( data->fd );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     close( data->fd );

     if (fmtdesc.pixelformat != V4L2_PIX_FMT_JPEG) {
          D_DEBUG_AT( HWJPEG_V4L2,
                      "%s: /dev/video0 doesn't accept JPEG streams\n",
                      __FUNCTION__ );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     data->base.ref    = 1;
     data->base.buffer = buffer;
     data->base.core   = core;

     /* check if the h/w decoder can really decode the JPEG.
        It will return the recommended destination buffer width, height
        and pixel format on success. */
     if (JPEG_hwRenderTo( data, NULL, NULL, &region ) != DFB_OK) {
          D_DEBUG_AT( HWJPEG_V4L2,
                      "%s: /dev/video0 can't decode the JPEG picture\n",
                      __FUNCTION__ );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     buffer->AddRef( buffer );

     data->base.Destruct = IDirectFBImageProvider_hwJPEG_Destruct;

     thiz->RenderTo = IDirectFBImageProvider_hwJPEG_RenderTo;
     thiz->Sync = NULL;
     thiz->GetImageDescription =
               IDirectFBImageProvider_hwJPEG_GetImageDescription;
     thiz->SetRenderFlags = IDirectFBImageProvider_hwJPEG_SetRenderFlags;
     thiz->GetSurfaceDescription =
               IDirectFBImageProvider_hwJPEG_GetSurfaceDescription;

     return DFB_OK;
}

static void
JPEG_stretchblit( IDirectFBImageProvider_hwJPEG_data *data,
                  CardState                          *state,
                  DFBRectangle                       *src_rect,
                  DFBRectangle                       *dst_rect )
{
     D_DEBUG_AT( HWJPEG_V4L2, "StretchBlit %dx%d -> %dx%d (%s -> %s)\n",
                 src_rect->w, src_rect->h, dst_rect->w, dst_rect->h,
                 dfb_pixelformat_name( state->source->config.format),
                 dfb_pixelformat_name( state->destination->config.format ) );

     /* thankfully this is intelligent enough to do a simple blit if
        possible */
     dfb_gfxcard_stretchblit( src_rect, dst_rect, state );
     /* we need to remember the serial in case a new surface is created
        (and written to) at the same address as the decode_surface before the
        blit operation is finished. This can happen if we get into the
        destructor before the blit operation is finished, which can happen on
        fast CPUs. */
     data->serial = state->serial;
}

DFBResult
JPEG_hwRenderTo( IDirectFBImageProvider_hwJPEG_data *data,
                 CoreSurface                        *dst_surface,
                 DFBRectangle                       *rect,
                 const DFBRegion                    *clip )
{
     DFBResult              ret = DFB_OK;
     unsigned int           length;
     unsigned int           ret_read;
     int                    type;
     int                    size = 0;

     CoreSurfaceConfig      config;

     IDirectFBDataBuffer   *buffer;

     struct v4l2_requestbuffers reqbufs;

     struct v4l2_format src_format;
     struct v4l2_format dst_format;

     struct v4l2_buffer srcbuf;
     struct v4l2_buffer dstbuf;

     CoreSurfaceBufferLock src_lock = { .pitch = 0 };
     CoreSurfaceBufferLock dst_lock = { .pitch = 0 };

     D_ASSERT( data != NULL );
     D_ASSERT( clip != NULL );

     D_ASSERT( data->base.core != NULL );

     if (!data->base.core)
          return DFB_FAILURE;

     if (data->decode_surface) {
           dfb_gfxcard_wait_serial( &data->serial );
           dfb_surface_unref( data->decode_surface );
           data->decode_surface = NULL;
     }

     D_ASSERT( data->source_surface == NULL );
     D_ASSERT( data->decode_surface == NULL );

     data->fd = open( "/dev/video0", O_RDWR );

     if (data->fd < 0) {
          D_DEBUG_AT( HWJPEG_V4L2,
                      "%s: couldn't open /dev/video0\n",
                      __FUNCTION__ );
          return DFB_FAILURE;
     }

     data->source_surface = NULL;

     data->width = -1;
     data->height = -1;

     buffer = data->base.buffer;

     if (buffer->GetLength( buffer, &length )) {
          D_ERROR( "%s: failed getting source length\n",
                   __FUNCTION__ );
          close( data->fd );
          return DFB_UNSUPPORTED;
     }

     config.size.w = 4 * sysconf(_SC_PAGESIZE);
     config.size.h = length / config.size.w + 1;

     if ((ret = dfb_surface_create_simple( data->base.core,
                                           config.size.w,
                                           config.size.h,
                                           DSPF_BYTE,
                                           DSCS_BT601,
                                           DSCAPS_VIDEOONLY,
                                           CSTF_EXTERNAL,
                                           0,
                                           NULL,
                                           &data->source_surface ))) {
          D_ERROR( "%s: failed to create the source surface\n",
                   __FUNCTION__ );

          close( data->fd );
          return ret;
     }

     D_DEBUG_AT( HWJPEG_V4L2, "allocated a %s surface (%dx%d) for JPEG data\n",
                 dfb_pixelformat_name( data->source_surface->config.format ),
                 config.size.w, config.size.h );

     if ((ret = dfb_surface_lock_buffer( data->source_surface,
                                         CSBR_BACK, CSAID_CPU,
                                         CSAF_WRITE, &src_lock ))) {
          D_ERROR( "%s: couldn't lock source buffer\n",
                   __FUNCTION__ );

          dfb_surface_unref( data->source_surface );
          data->source_surface = NULL;

          close( data->fd );
          return ret;
     }

     buffer->SeekTo( buffer, 0 );

     if ((ret = buffer->GetData( buffer, length, src_lock.addr, &ret_read ))) {
          D_ERROR( "%s: couldn't upload source data to video memory\n",
                   __FUNCTION__ );
          goto fini;
     }


     memset( &reqbufs, 0, sizeof( reqbufs ) );

     reqbufs.count = 1;
     reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     reqbufs.memory = V4L2_MEMORY_USERPTR;

     if (ioctl( data->fd, VIDIOC_REQBUFS, &reqbufs ) < 0) {
          D_ERROR( "%s: couldn't request capture buffers\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     reqbufs.count = 1;
     reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
     reqbufs.memory = V4L2_MEMORY_USERPTR;

     if (ioctl( data->fd, VIDIOC_REQBUFS, &reqbufs ) < 0) {
          D_ERROR( "%s: couldn't request output buffers\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

     if (ioctl( data->fd, VIDIOC_STREAMON, &type ) < 0) {
          D_ERROR( "%s: couldn't toggle streaming on\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

     if (ioctl( data->fd, VIDIOC_STREAMON, &type ) < 0) {
          D_ERROR( "%s: couldn't toggle streaming on\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     memset( &src_format, 0, sizeof( src_format ) );

     src_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
     src_format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;

     if (ioctl( data->fd, VIDIOC_S_FMT, &src_format ) < 0) {
          D_ERROR( "%s: couldn't set output format\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     memset( &srcbuf, 0, sizeof( srcbuf ) );

     srcbuf.index = 0;
     srcbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
     srcbuf.memory = V4L2_MEMORY_USERPTR;
     srcbuf.m.userptr = (unsigned long)src_lock.addr;
     srcbuf.length = length;

     if (ioctl( data->fd, VIDIOC_QBUF, &srcbuf ) < 0) {
          D_ERROR( "%s: couldn't queue source buffer\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     memset( &src_format, 0, sizeof( src_format ) );

     src_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

     if (ioctl( data->fd, VIDIOC_G_FMT, &src_format ) < 0) {
          D_ERROR( "%s: couldn't get source format\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     D_DEBUG_AT( HWJPEG_V4L2, "JPEG picture is a %dx%d w/ fourcc 0x%08x\n",
                 src_format.fmt.pix.width, src_format.fmt.pix.height,
                 src_format.fmt.pix.pixelformat );

     src_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     src_format.fmt.pix.width = rect ? rect->w : src_format.fmt.pix.width;
     src_format.fmt.pix.height = rect ? rect->h : src_format.fmt.pix.height;

     dst_format = src_format;

     if (ioctl( data->fd, VIDIOC_TRY_FMT, &dst_format ) < 0) {
          D_ERROR( "%s: couldn't get suggested capture format\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     if (dst_format.fmt.pix.pixelformat != V4L2_PIX_FMT_NV12
         && dst_format.fmt.pix.pixelformat != V4L2_PIX_FMT_NV16
         && dst_format.fmt.pix.pixelformat != V4L2_PIX_FMT_NV24) {
          D_ERROR( "%s: capture format isn't a valid NV fourcc\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     config.flags = CSCONF_SIZE | CSCONF_FORMAT;

     switch (dst_format.fmt.pix.pixelformat) {
     case V4L2_PIX_FMT_NV12:
          config.format = DSPF_NV12;
          break;
     case V4L2_PIX_FMT_NV16:
          config.format = DSPF_NV16;
          break;
     case V4L2_PIX_FMT_NV24:
          config.format = DSPF_NV24;
          break;
     default:
          D_DEBUG_AT( HWJPEG_V4L2, "invalid pixelformat!\n" );
          ret = DFB_FAILURE;
          goto fini;
     }

     D_DEBUG_AT( HWJPEG_V4L2, "h/w decoder suggested %dx%d %s buffer "
                 "(%dx%d requested)\n",
                 dst_format.fmt.pix.width,
                 dst_format.fmt.pix.height,
                 dfb_pixelformat_name( config.format ),
                 src_format.fmt.pix.width,
                 src_format.fmt.pix.height );

     /* Construct() will provide a dst_surface == NULL so once we're here
        the JPEG header has been parsed and the recommended width, height and
        pixel format have been retrieved. */
     if (!dst_surface) {
          data->width = dst_format.fmt.pix.width;
          data->height = dst_format.fmt.pix.height;
          data->pixelformat = config.format;
          ret = DFB_OK;
          goto fini;
     }

     /* specify the pitch returned by VIDIOC_TRY_FMT as a width */
     config.size.w = dst_format.fmt.pix.bytesperline;
     config.size.h = dst_format.fmt.pix.height;

     if ((ret = dfb_surface_create_simple( data->base.core,
                                           config.size.w,
                                           config.size.h,
                                           config.format,
                                           DSCS_BT601,
                                           DSCAPS_VIDEOONLY,
                                           CSTF_EXTERNAL,
                                           0,
                                           NULL,
                                           &data->decode_surface ))) {
          D_ERROR( "%s: failed to create the nv1x decode surface\n",
                   __FUNCTION__ );
          goto fini;
     }

     D_DEBUG_AT( HWJPEG_V4L2, "allocated a %s surface (%dx%d) for decode\n",
                 dfb_pixelformat_name( data->decode_surface->config.format ),
                 config.size.w, config.size.h );

     dfb_surface_calc_buffer_size( data->decode_surface, 1, 1, NULL, &size );

     src_format.fmt.pix.sizeimage = size;
     src_format.fmt.pix.pixelformat = dst_format.fmt.pix.pixelformat;

     /* VIDIOC_S_FMT takes the original requested dimensions */
     if (ioctl( data->fd, VIDIOC_S_FMT, &src_format ) < 0) {
          D_ERROR( "%s: couldn't set capture format\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     if ((ret = dfb_surface_lock_buffer( data->decode_surface,
                                         CSBR_BACK, CSAID_GPU,
                                         CSAF_WRITE, &dst_lock ))) {
          D_ERROR( "%s: couldn't lock source buffer\n",
                   __FUNCTION__ );
          goto fini;
     }

     D_DEBUG_AT( HWJPEG_V4L2, "locked h/w decode buffer @ 0x%08x, pitch: %d\n",
                 (unsigned int)dst_lock.addr, dst_lock.pitch );

     if (dst_lock.pitch != dst_format.fmt.pix.bytesperline) {
          D_ERROR( "%s: unaligned decode surface pitch!\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     memset( &dstbuf, 0, sizeof( dstbuf ) );

     /* planar capture */
     dstbuf.index = 0;
     dstbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     dstbuf.memory = V4L2_MEMORY_USERPTR;
     dstbuf.m.userptr = (unsigned long)dst_lock.addr;
     dstbuf.length = dst_format.fmt.pix.sizeimage;

     if (ioctl( data->fd, VIDIOC_QBUF, &dstbuf ) < 0) {
          D_ERROR( "%s: couldn't queue capture buffer\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     if (ioctl( data->fd, VIDIOC_DQBUF, &dstbuf ) < 0) {
          D_ERROR( "%s: couldn't dequeue capture buffer\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     memset( &dst_format, 0, sizeof( dst_format ) );

     dst_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

     if (ioctl( data->fd, VIDIOC_G_FMT, &dst_format ) < 0) {
          D_ERROR( "%s: couldn't get capture final format\n",
                   __FUNCTION__ );
          ret = DFB_FAILURE;
          goto fini;
     }

     D_DEBUG_AT( HWJPEG_V4L2, "decoded picture is a %dx%d %s buffer "
                 "(pitch: %d)\n", dst_format.fmt.pix.width,
                 dst_format.fmt.pix.height,
                 (dst_format.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12) ? "NV12"
                 : ((dst_format.fmt.pix.pixelformat == V4L2_PIX_FMT_NV16)
                    ? "NV16" : "NV24"),
                 dst_format.fmt.pix.bytesperline );

fini:
     if (data->decode_surface && dst_lock.pitch)
          dfb_surface_unlock_buffer( data->decode_surface, &dst_lock );

     type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

     if (ioctl( data->fd, VIDIOC_STREAMOFF, &type ) < 0)
          D_ERROR( "%s: couldn't turn streaming off\n",
                   __FUNCTION__ );

     type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

     if (ioctl( data->fd, VIDIOC_STREAMOFF, &type ) < 0)
          D_ERROR( "%s: couldn't turn streaming off\n",
                   __FUNCTION__ );

     if (data->source_surface) {
          if (src_lock.pitch)
               dfb_surface_unlock_buffer( data->source_surface, &src_lock );

          dfb_surface_unref( data->source_surface );
          data->source_surface = NULL;
     }

     close( data->fd );

     /* don't blit to destination if something went wrong */
     if (data->decode_surface && ret == DFB_OK) {
          CardState    state;
          DFBRectangle src_rect = {
               .x = 0,
               .y = 0,
               .w = dst_format.fmt.pix.width,
               .h = dst_format.fmt.pix.height
          };

          /* use DFB to convert the intermediary NV format to the
             final destination format, applying any necessary additional
             clip/stretch */
          dfb_state_init( &state, data->base.core );
          dfb_state_set_source( &state, data->decode_surface );
          dfb_state_set_destination( &state, dst_surface );
          dfb_state_set_clip( &state, clip );

          JPEG_stretchblit( data, &state, &src_rect, rect );

          /* remove the state */
          dfb_state_set_source( &state, NULL );
          dfb_state_set_destination( &state, NULL );
          dfb_state_destroy( &state );

          if (data->base.render_callback) {
               DFBRectangle r = { 0, 0,
                                  dst_format.fmt.pix.width,
                                  dst_format.fmt.pix.height };
               data->base.render_callback( &r,
                                           data->base.render_callback_context );
          }
     }

     return ret;
}

static DFBResult
IDirectFBImageProvider_hwJPEG_RenderTo( IDirectFBImageProvider *thiz,
                                        IDirectFBSurface       *destination,
                                        const DFBRectangle     *dest_rect )
{
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_hwJPEG)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     dfb_region_from_rectangle( &data->clip, &dst_data->area.current );

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;

          data->rect = *dest_rect;
          data->rect.x += dst_data->area.wanted.x;
          data->rect.y += dst_data->area.wanted.y;

          if (!dfb_rectangle_region_intersects( &data->rect, &data->clip ))
               return DFB_OK;
     }
     else {
          data->rect = dst_data->area.wanted;
     }

     return JPEG_hwRenderTo( data, dst_surface, &data->rect, &data->clip );
}

static DFBResult
IDirectFBImageProvider_hwJPEG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                     DFBSurfaceDescription  *dsc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_hwJPEG)

     if (!dsc)
          return DFB_INVARG;

     if (data->width < 0
         || data->height < 0)
          return DFB_INVARG;

     dsc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->width  = data->width;
     dsc->height = data->height;
     dsc->pixelformat = data->pixelformat;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_hwJPEG_SetRenderFlags( IDirectFBImageProvider *thiz,
                                              DIRenderFlags flags )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_hwJPEG)

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBImageProvider_hwJPEG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                   DFBImageDescription    *dsc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_hwJPEG)

     if (!dsc)
          return DFB_INVARG;

     dsc->caps = DICAPS_NONE;

     return DFB_OK;
}
