/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/time.h>
#include <assert.h>

#include <pthread.h>

#include <directfb.h>

#include <direct/types.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <idirectfb.h>

#include <core/surface.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbvideoprovider.h>

#include <misc/gfx_util.h>


static DFBResult Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult Construct( IDirectFBVideoProvider *thiz,
                            ... );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, Gif )

/*****************************************************************************/

#define MAXCOLORMAPSIZE 256

#define CM_RED   0
#define CM_GREEN 1
#define CM_BLUE  2

#define MAX_LWZ_BITS 12

#define INTERLACE     0x40
#define LOCALCOLORMAP 0x80

#define BitSet(byte, bit) (((byte) & (bit)) == (bit))

#define LM_to_uint(a,b) (((b)<<8)|(a))

typedef struct {
     int                            ref;      /* reference counter */

     IDirectFBDataBuffer           *buffer;
     DFBBoolean                     seekable;
     
     IDirectFBSurface              *destination;
     IDirectFBSurface_data         *dst_data;
     DFBRectangle                   dst_rect;

     DFBColor                       palette[MAXCOLORMAPSIZE];

     CoreSurface                   *decode_surface;
     CoreSurfaceBufferLock          buffer_lock;

     DirectThread                  *thread;
     pthread_mutex_t                lock;
     pthread_cond_t                 cond;
     
     int                            paused;

     DFBVideoProviderStatus         status;
     DFBVideoProviderPlaybackFlags  flags;
     double                         speed;
     
     unsigned int                   start_pos;

     unsigned int                   frame;
     unsigned int                   target_frame; /* only in DVPLAY_PACED */
     unsigned int                   last_frame; /* only in DVPLAY_PACED */

     char                           Version[4];
     unsigned int                   Width;
     unsigned int                   Height;
     u8                             ColorMap[3][MAXCOLORMAPSIZE];
     unsigned int                   BitPixel;
     unsigned int                   ColorResolution;
     u32                            Background;
     unsigned int                   AspectRatio;

     int                            transparent;
     unsigned int                   delayTime;
     int                            inputFlag;
     int                            disposal;

     u8                             buf[280];
     int                            curbit, lastbit, done, last_byte;

     int                            fresh;
     int                            code_size, set_code_size;
     int                            max_code, max_code_size;
     int                            firstcode, oldcode;
     int                            clear_code, end_code;
     int                            table[2][(1<< MAX_LWZ_BITS)];
     int                            stack[(1<<(MAX_LWZ_BITS))*2], *sp;

     DVFrameCallback                callback;
     void                          *callback_ctx;

     CoreDFB                       *core;
} IDirectFBVideoProvider_GIF_data;

#define GIFERRORMSG(x, ...) \
     D_ERROR( "IDirectFBVideoProvider_GIF: " #x "!\n", ## __VA_ARGS__ )

#define GIFDEBUGMSG(x, ...) \
     D_DEBUG( "IDirectFBVideoProvider_GIF: " #x "!\n", ## __VA_ARGS__ )

/*****************************************************************************/

static int ZeroDataBlock = 0;

static DFBResult
FetchData( IDirectFBDataBuffer *buffer, void *data, unsigned int len )
{
     DFBResult ret = DFB_OK;

     do {
          unsigned int read = 0;
          
          ret = buffer->WaitForData( buffer, len );
          if (ret == DFB_OK)
               ret = buffer->GetData( buffer, len, data, &read );
          if (ret)
               break;
               
          data += read;
          len  -= read;
     } while (len);
     
     return ret;
}

static int ReadColorMap( IDirectFBDataBuffer *buffer, int number,
                         DFBColor palette[MAXCOLORMAPSIZE] )
{
     int  i;
     u8   rgb[3*number];
     
     if (FetchData( buffer, rgb, sizeof(rgb) )) {
          GIFERRORMSG("bad colormap");
          return -1;
     }

     for (i = 0; i < number; ++i) {
          palette[i].r = rgb[i*3+0];
          palette[i].g = rgb[i*3+1];
          palette[i].b = rgb[i*3+2];
          palette[i].a = 0xff;
     }
     
     return 0;
}

static int GetDataBlock(IDirectFBDataBuffer *buffer, u8 *buf)
{
     unsigned char count;

     if (FetchData( buffer, &count, 1 )) {
          GIFERRORMSG("error in getting DataBlock size");
          return -1;
     }
     ZeroDataBlock = (count == 0);

     if ((count != 0) && FetchData( buffer, buf, count )) {
          GIFERRORMSG("error in reading DataBlock");
          return -1;
     }

     return count;
}

static int GetCode(IDirectFBVideoProvider_GIF_data *data, int code_size, int flag)
{
     int i, j, ret;
     unsigned char count;

     if (flag) {
          data->curbit = 0;
          data->lastbit = 0;
          data->done = false;
          return 0;
     }

     if ( (data->curbit+code_size) >= data->lastbit) {
          if (data->done) {
               if (data->curbit >= data->lastbit) {
                    GIFERRORMSG("ran off the end of my bits");
               }
               return -1;
          }
          data->buf[0] = data->buf[data->last_byte-2];
          data->buf[1] = data->buf[data->last_byte-1];

          if ((count = GetDataBlock( data->buffer, &data->buf[2] )) == 0) {
               data->done = true;
          }

          data->last_byte = 2 + count;
          data->curbit = (data->curbit - data->lastbit) + 16;
          data->lastbit = (2+count) * 8;
     }

     ret = 0;
     for (i = data->curbit, j = 0; j < code_size; ++i, ++j) {
          ret |= ((data->buf[ i / 8 ] & (1 << (i % 8))) != 0) << j;
     }
     data->curbit += code_size;

     return ret;
}

static int DoExtension( IDirectFBVideoProvider_GIF_data *data, int label )
{
     unsigned char buf[256] = { 0 };
     char *str;

     switch (label) {
          case 0x01:              /* Plain Text Extension */
               str = "Plain Text Extension";
               break;
          case 0xff:              /* Application Extension */
               str = "Application Extension";
               break;
          case 0xfe:              /* Comment Extension */
               str = "Comment Extension";
               while (GetDataBlock( data->buffer, (u8*) buf ) != 0)
                    GIFDEBUGMSG("gif comment: %s", buf);
               return false;
          case 0xf9:              /* Graphic Control Extension */
               str = "Graphic Control Extension";
               (void) GetDataBlock( data->buffer, (u8*) buf );
               data->disposal  = (buf[0] >> 2) & 0x7;
               data->inputFlag = (buf[0] >> 1) & 0x1;
               if (LM_to_uint( buf[1], buf[2] ))
                    data->delayTime = LM_to_uint( buf[1], buf[2] ) * 10000;
               if ((buf[0] & 0x1) != 0)
                    data->transparent = buf[3];
               else
                    data->transparent = -1;
               while (GetDataBlock( data->buffer, (u8*) buf ) != 0)
                    ;
               return false;
          default:
               str = (char*) buf;
               snprintf(str, 256, "UNKNOWN (0x%02x)", label);
          break;
     }

     GIFDEBUGMSG("got a '%s' extension", str );

     while (GetDataBlock( data->buffer, (u8*) buf ) != 0);

     return 0;
}

static int LWZReadByte( IDirectFBVideoProvider_GIF_data *data, int flag, int input_code_size )
{
     int code, incode;
     int i;

     if (flag) {
          data->set_code_size = input_code_size;
          data->code_size = data->set_code_size+1;
          data->clear_code = 1 << data->set_code_size ;
          data->end_code = data->clear_code + 1;
          data->max_code_size = 2*data->clear_code;
          data->max_code = data->clear_code+2;

          GetCode(data, 0, true);

          data->fresh = true;

          for (i = 0; i < data->clear_code; ++i) {
               data->table[0][i] = 0;
               data->table[1][i] = i;
          }
          for (; i < (1<<MAX_LWZ_BITS); ++i) {
               data->table[0][i] = data->table[1][0] = 0;
          }
          data->sp = data->stack;

          return 0;
     }
     else if (data->fresh) {
          data->fresh = false;
          do {
               data->firstcode = data->oldcode = GetCode( data, data->code_size, false );
          } while (data->firstcode == data->clear_code);

          return data->firstcode;
     }

     if (data->sp > data->stack) {
          return *--data->sp;
     }

     while ((code = GetCode( data, data->code_size, false )) >= 0) {
          if (code == data->clear_code) {
               for (i = 0; i < data->clear_code; ++i) {
                    data->table[0][i] = 0;
                    data->table[1][i] = i;
               }
               for (; i < (1<<MAX_LWZ_BITS); ++i) {
                    data->table[0][i] = data->table[1][i] = 0;
               }
               data->code_size = data->set_code_size+1;
               data->max_code_size = 2*data->clear_code;
               data->max_code = data->clear_code+2;
               data->sp = data->stack;
               data->firstcode = data->oldcode = GetCode( data, data->code_size, false );

               return data->firstcode;
          }
          else if (code == data->end_code) {
               int count;
               u8 buf[260];

               if (ZeroDataBlock) {
                    return -2;
               }

               while ((count = GetDataBlock( data->buffer, buf )) > 0)
                    ;

               if (count != 0)
                    GIFERRORMSG("missing EOD in data stream (common occurence)");

               return -2;
          }

          incode = code;

          if (code >= data->max_code) {
               *data->sp++ = data->firstcode;
               code = data->oldcode;
          }

          while (code >= data->clear_code) {
               *data->sp++ = data->table[1][code];
               if (code == data->table[0][code]) {
                    GIFERRORMSG("circular table entry BIG ERROR");
               }
               code = data->table[0][code];
          }

          *data->sp++ = data->firstcode = data->table[1][code];

          if ((code = data->max_code) <(1<<MAX_LWZ_BITS)) {
               data->table[0][code] = data->oldcode;
               data->table[1][code] = data->firstcode;
               ++data->max_code;
               if ((data->max_code >= data->max_code_size)
                   && (data->max_code_size < (1<<MAX_LWZ_BITS)))
               {
                    data->max_code_size *= 2;
                    ++data->code_size;
               }
          }

          data->oldcode = incode;

          if (data->sp > data->stack) {
               return *--data->sp;
          }
     }
     return code;
}

static int ReadImage( IDirectFBVideoProvider_GIF_data *data, 
                      int left, int top, int width, int height,
                      int pitch, DFBColor palette[MAXCOLORMAPSIZE],
                      bool interlace, bool ignore )
{
     u8   c;
     int  v;
     int  xpos = 0, ypos = 0, pass = 0;
     u32 *image = image, *dst = dst;

     /*
     **  Initialize the decompression routines
     */
     if (FetchData( data->buffer, &c, 1 ))
          GIFERRORMSG("EOF / read error on image data");

     if (LWZReadByte( data, true, c ) < 0)
          GIFERRORMSG("error reading image");

     /*
     **  If this is an "uninteresting picture" ignore it.
     */
     if (ignore && !data->disposal) {
          GIFDEBUGMSG("skipping image...");

          while (LWZReadByte( data, false, c ) >= 0)
               ;
          return 0;
     }
     
     switch (data->disposal) {
          case 2:
               GIFDEBUGMSG("restoring to background color is unsupported");
               break;
          case 3:
               GIFERRORMSG("restoring to previous frame is unsupported");
               break;
          default:
               break;
     }
     
     dst = image = ((u32 *)(data->buffer_lock.addr)
                    + (top * (pitch / 4) + left));

     GIFDEBUGMSG("reading %dx%d at %dx%d %sGIF image",
                 width, height, left, top, interlace ? " interlaced " : "" );

     while ((v = LWZReadByte( data, false, c )) >= 0 ) {
          if (v != data->transparent) {
               dst[xpos] = (0xFF000000         |
                            palette[v].r << 16 |
                            palette[v].g << 8  |
                            palette[v].b);
          }

          ++xpos;
          if (xpos == width) {
               xpos = 0;
               if (interlace) {
                    switch (pass) {
                         case 0:
                         case 1:
                              ypos += 8;
                              break;
                         case 2:
                              ypos += 4;
                              break;
                         case 3:
                              ypos += 2;
                              break;
                    }

                    if (ypos >= height) {
                         ++pass;
                         switch (pass) {
                              case 1:
                                   ypos = 4;
                                   break;
                              case 2:
                                   ypos = 2;
                                   break;
                              case 3:
                                   ypos = 1;
                              break;
                              default:
                                   goto fini;
                         }
                    }
               }
               else {
                    ++ypos;
               }

               dst = image + ypos * (pitch >> 2);
          }

          if (ypos >= height) {
               break;
          }
     }

fini:

     if (LWZReadByte( data, false, c ) >= 0) {
          GIFERRORMSG("too much input data, ignoring extra...");
          //while (LWZReadByte( data, false, c ) >= 0);
     }

     return 0;
}

static void GIFReset( IDirectFBVideoProvider_GIF_data *data )
{
     data->transparent = -1;
     data->delayTime   = 1000000; /* default: 1s */
     data->inputFlag   = -1;
     data->disposal    = 0;
}

static DFBResult GIFReadHeader( IDirectFBVideoProvider_GIF_data *data )
{
     DFBResult ret;
     u8        buf[7];
     
     ret = FetchData( data->buffer, buf, 6 );
     if (ret) {
          GIFERRORMSG("error reading header");
          return ret;
     }

     if (memcmp( buf, "GIF", 3 )) {
          GIFERRORMSG("bad magic");
          return DFB_UNSUPPORTED;
     }
     
     memcpy( data->Version, &buf[3], 3 );
     data->Version[3] = '\0';
     
     ret = FetchData( data->buffer, buf, 7 );
     if (ret) {
          GIFERRORMSG("error reading screen descriptor");
          return ret;
     }

     data->Width           = LM_to_uint( buf[0], buf[1] );
     data->Height          = LM_to_uint( buf[2], buf[3] );
     data->BitPixel        = 2 << (buf[4] & 0x07);
     data->ColorResolution = (((buf[4] & 0x70) >> 3) + 1);
     data->Background      = buf[5];
     data->AspectRatio     = buf[6];
     if (data->AspectRatio)
          data->AspectRatio = ((data->AspectRatio + 15) << 8) >> 6;
     else
          data->AspectRatio = (data->Width << 8) / data->Height;

     if (BitSet(buf[4], LOCALCOLORMAP)) {    /* Global Colormap */
          if (ReadColorMap( data->buffer, data->BitPixel, data->palette )) {
               GIFERRORMSG("error reading global colormap");
               return DFB_FAILURE;
          }
     }
     
     return DFB_OK;
}

static DFBResult GIFReadFrame( IDirectFBVideoProvider_GIF_data *data )
{
     u8    buf[16], c;
     int   top, left;
     int   width, height;
     DFBColor  local_palette[MAXCOLORMAPSIZE];
     bool  useGlobalColormap;
     int   ignore = 0;
     int   num_frames = 0;

     data->curbit = data->lastbit = data->done = data->last_byte = 0;

     data->fresh = 
     data->code_size = data->set_code_size =
     data->max_code = data->max_code_size = 
     data->firstcode = data->oldcode =
     data->clear_code = data->end_code = 0;

     dfb_surface_lock_buffer( data->decode_surface,
                              CSBR_BACK, CSAID_CPU, CSAF_WRITE,
                              &data->buffer_lock );

     if (data->flags & DVPLAY_PACED
         && data->target_frame != -1) {
          ignore = data->target_frame != (data->frame + 1);
          num_frames = data->target_frame - data->frame - 1;
     }

     if (!data->frame)
          num_frames++;

     for (; num_frames >= 0;) {
          DFBResult ret;
          
          ret = FetchData( data->buffer, &c, 1);
          if (ret) {
               GIFERRORMSG("EOF / read error on image data" );
               dfb_surface_unlock_buffer( data->decode_surface,
                                          &data->buffer_lock );
               return DFB_EOF;
          }

          if (c == ';') { /* GIF terminator */
               dfb_surface_unlock_buffer( data->decode_surface,
                                          &data->buffer_lock );
               return DFB_EOF;
          }

          if (c == '!') { /* Extension */
               if (FetchData( data->buffer, &c, 1)) {
                    GIFERRORMSG("EOF / read error on extention function code");
                    dfb_surface_unlock_buffer( data->decode_surface,
                                               &data->buffer_lock );
                    return DFB_EOF;
               }
               DoExtension( data, c );
               continue;
          } 

          if (c != ',') { /* Not a valid start character */
               GIFERRORMSG("bogus character 0x%02x, ignoring", (int) c );
               continue;
          }
               
          ret = FetchData( data->buffer, buf, 9 );
          if (ret) {
               GIFERRORMSG("couldn't read left/top/width/height");
               dfb_surface_unlock_buffer( data->decode_surface,
                                          &data->buffer_lock );
               return ret;
          }
               
          left   = LM_to_uint( buf[0], buf[1] );
          top    = LM_to_uint( buf[2], buf[3] );
          width  = LM_to_uint( buf[4], buf[5] );
          height = LM_to_uint( buf[6], buf[7] );

          useGlobalColormap = !BitSet( buf[8], LOCALCOLORMAP );

          if (!useGlobalColormap) {
               int bitPixel = 2 << (buf[8] & 0x07);
               if (ReadColorMap( data->buffer, bitPixel, local_palette ))
                    GIFERRORMSG("error reading local colormap");
          }

          data->frame++;
          num_frames--;


          if (ReadImage( data, left, top, width, height,
                         data->buffer_lock.pitch,
                         (useGlobalColormap ?
                         data->palette : local_palette),
                         BitSet( buf[8], INTERLACE ), ignore )) {
               GIFERRORMSG("error reading image");
               dfb_surface_unlock_buffer( data->decode_surface,
                                          &data->buffer_lock );
               return DFB_FAILURE;
          }
     }
     
     dfb_surface_unlock_buffer( data->decode_surface, &data->buffer_lock );

     return DFB_OK;
}

/*****************************************************************************/

static void
IDirectFBVideoProvider_GIF_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_GIF_data *data = thiz->priv;
     
     thiz->Stop( thiz );
    
     if (data->thread) {
          direct_thread_cancel( data->thread );
          pthread_mutex_lock( &data->lock );
          pthread_cond_signal( &data->cond );
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }

     if (data->buffer)
          data->buffer->Release( data->buffer );

     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;
          data->dst_data    = NULL;
     }

     if (data->decode_surface) {
          dfb_surface_unref( data->decode_surface );
          data->decode_surface = NULL;
     }

     pthread_cond_destroy( &data->cond );
     pthread_mutex_destroy( &data->lock );
          
     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBVideoProvider_GIF_AddRef( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBVideoProvider_GIF_Release( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )

     if (--data->ref == 0)
          IDirectFBVideoProvider_GIF_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GIF_GetCapabilities( IDirectFBVideoProvider       *thiz,
                                            DFBVideoProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )
     
     if (!caps)
          return DFB_INVARG;
          
     *caps = DVCAPS_BASIC | DVCAPS_SCALE | DVCAPS_SPEED;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GIF_GetSurfaceDescription( IDirectFBVideoProvider *thiz,
                                                  DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )
     
     if (!desc)
          return DFB_INVARG;
     
     desc->width = data->Width;
     desc->height = data->Height;
     desc->pixelformat = DSPF_ARGB;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GIF_GetStreamDescription( IDirectFBVideoProvider *thiz,
                                                 DFBStreamDescription   *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )
     
     if (!desc)
          return DFB_INVARG;
          
     desc->caps = DVSCAPS_VIDEO;
     
     snprintf( desc->video.encoding,
               DFB_STREAM_DESC_ENCODING_LENGTH, "GIF %s", data->Version );
     desc->video.framerate = 0;
     desc->video.aspect    = (double)data->AspectRatio/256.0;
     desc->video.bitrate   = 0;
     
     desc->title[0] = desc->author[0] = 
     desc->album[0] = desc->genre[0] = desc->comment[0] = 0;
     desc->year = 0;
     
     return DFB_OK;
}

static void
clear_decode_surface(IDirectFBVideoProvider_GIF_data *data)
{
     CardState     state;
     DFBRectangle  rect = { .x = 0,
                            .y = 0,
                            .w = data->Width,
                            .h = data->Height };

     DFBColor      color = { .r = 0,
                             .g = 0,
                             .b = 0,
                             .a = 0 };

     DFBRegion     clip  = { .x1 = 0,
                             .y1 = 0,
                             .x2 = data->Width - 1,
                             .y2 = data->Height - 1 };

     /* init a fillrect state */
     dfb_state_init( &state, data->core );
     dfb_state_set_color( &state, &color );
     dfb_state_set_source( &state, NULL );
     dfb_state_set_destination( &state, data->decode_surface );
     dfb_state_set_clip( &state, &clip );

     dfb_gfxcard_fillrectangles( &rect, 1, &state );

     /* wait for the h/w to complete */
     dfb_gfxcard_wait_serial( &state.serial );

     /* remove the state */
     dfb_state_set_source( &state, NULL );
     dfb_state_set_destination( &state, NULL );
     dfb_state_destroy( &state );
}

static void*
GIFVideo( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_GIF_data *data = arg;
     
     pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, NULL );

     data->frame = 0;

     clear_decode_surface( data );

     while (!direct_thread_is_canceled( self )) {
          DFBResult              ret;
          DFBRectangle           rect;
          DFBRegion              clip;
          
          pthread_mutex_lock( &data->lock );
          
          if (direct_thread_is_canceled( self )) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          /* check if the requested frame doesn't exist */
          if (data->flags & DVPLAY_PACED
              && data->last_frame
              && data->target_frame > data->last_frame) {
               /* notify the caller */
               data->target_frame = -1;
               data->callback( data->callback_ctx );
               /* wait for the next call */
               data->status = DVSTATE_STOP;
               pthread_cond_wait( &data->cond, &data->lock );
               pthread_mutex_unlock( &data->lock );
               continue;
          }

          /* restart decoding if the newly requested frame precedes or is
             the last decoded one when in DVPLAY_PACED mode */
          if (data->flags & DVPLAY_PACED
              && data->target_frame != -1
              && data->target_frame < data->frame) {
               GIFReset( data );
               data->frame = 0;
               data->buffer->SeekTo( data->buffer, data->start_pos );
          }

          ret = GIFReadFrame( data );
          if (ret) { 
               if (ret == DFB_EOF) {
                    data->last_frame = data->frame;
                    GIFReset( data );
                    data->frame = 0;
                    if (data->flags & DVPLAY_LOOPING) {
                         data->buffer->SeekTo( data->buffer, data->start_pos );
                    }
                    else {
                         data->status = DVSTATE_FINISHED;
                         pthread_mutex_unlock( &data->lock );
                         break;
                    }
               }
               /* bail out if stream is invalid */
               if (data->flags & DVPLAY_PACED) {
                    /* notify the caller */
                    data->target_frame = -1; /* error */
                    data->callback( data->callback_ctx );
                    /* wait for the next call */
                    data->status = DVSTATE_STOP;
                    pthread_cond_wait( &data->cond, &data->lock );
               }
               pthread_mutex_unlock( &data->lock );
               continue;
          }
          
          rect = (data->dst_rect.w == 0)
                 ? data->dst_data->area.wanted : data->dst_rect;          
          dfb_region_from_rectangle( &clip, &data->dst_data->area.current );
          
          CardState     state;
          CoreSurface  *dst_surface;
          DFBRectangle  srect = { .x = 0,
                                  .y = 0,
                                  .w = data->Width,
                                  .h = data->Height, };

          dst_surface = data->dst_data->surface;
          D_MAGIC_ASSERT( dst_surface, CoreSurface );

          /* init a state, so that we can use gfxcard/blit to do the
             format conversion. */
          dfb_state_init( &state, data->core );
          dfb_state_set_source( &state, data->decode_surface );
          dfb_state_set_destination( &state, dst_surface );
          dfb_state_set_clip( &state, &clip );

          dfb_gfxcard_stretchblit( &srect, &rect, &state );

          /* remove the state */
          dfb_state_set_source( &state, NULL );
          dfb_state_set_destination( &state, NULL );
          dfb_state_destroy( &state );

          if (data->callback)
               data->callback( data->callback_ctx );

          if (!(data->flags & DVPLAY_PACED))
          {
               if (!data->speed)
                    pthread_cond_wait( &data->cond, &data->lock );
               else {
                    struct timespec ts;
                    struct timeval  tv;
                    unsigned long   us;

                    gettimeofday( &tv, NULL );

                    us = data->delayTime;
                    if (data->speed != 1.0)
                         us = ((double)us / data->speed + .5);
                    us += tv.tv_usec;

                    ts.tv_sec  = tv.tv_sec + us/1000000;
                    ts.tv_nsec = (us%1000000) * 1000;

                    pthread_cond_timedwait( &data->cond, &data->lock, &ts );
               }
          } else {
               data->status = DVSTATE_STOP;
               pthread_cond_wait( &data->cond, &data->lock );
          }
          
          pthread_mutex_unlock( &data->lock );
     }
     
     return (void*)0;
}

static DFBResult
IDirectFBVideoProvider_GIF_PlayTo( IDirectFBVideoProvider *thiz,
                                   IDirectFBSurface       *destination,
                                   const DFBRectangle     *dest_rect,
                                   DVFrameCallback         callback,
                                   void                   *ctx )
{
     IDirectFBSurface_data *dst_data;
     DFBRectangle           rect = { 0, 0, 0, 0 };
     DFBResult              ret;

     DFBVideoProviderPacedPlaybackCtx *Ctx =
               (DFBVideoProviderPacedPlaybackCtx *)ctx;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )
     
     if (!destination)
          return DFB_INVARG;
          
     dst_data = destination->priv;
     if (!dst_data || !dst_data->surface)
          return DFB_DESTROYED;

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;
          
          rect = *dest_rect;
          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;
     }          
     
     if (data->flags & DVPLAY_PACED) {
          if (!Ctx || !callback)
               return DFB_INVARG;
     }

     pthread_mutex_lock( &data->lock );
     
     if (data->status == DVSTATE_FINISHED) {
          ret = data->buffer->SeekTo( data->buffer, data->start_pos );
          if (ret) {
               pthread_mutex_unlock( &data->lock );
               return ret;
          }
     }
     data->status = DVSTATE_PLAY;
     
     if (data->destination)
          data->destination->Release( data->destination );
     
     destination->AddRef( destination );
     data->destination = destination;
     data->dst_data    = dst_data;
     data->dst_rect    = rect;

     data->callback     = callback;
     data->callback_ctx = ctx;
     if (data->flags & DVPLAY_PACED)
          data->target_frame = Ctx->num_frame;
     
     if (!data->decode_surface) {
          ret = dfb_surface_create_simple( data->core,
                                           data->Width, data->Height,
                                           DSPF_ARGB, DSCS_RGB,
                                           DSCAPS_NONE, CSTF_NONE, 0,
                                           NULL, &data->decode_surface );
          if (ret != DFB_OK) {
               pthread_mutex_unlock( &data->lock );
               return ret;
          }
     }

     /* Playback has already been started by a previous call to PlayTo() */
     if (data->thread && (data->flags & DVPLAY_PACED)) {
          data->paused = 0;
          pthread_cond_signal( &data->cond );
          pthread_mutex_unlock( &data->lock );
          return DFB_OK;
     }

     if (!data->thread) {
          data->thread = direct_thread_create( DTT_DEFAULT, GIFVideo,
                                              (void*)data, "GIF Video" );
     }
     
     pthread_mutex_unlock( &data->lock );
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GIF_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )

     if (data->flags & DVPLAY_PACED) {
        /* sync with the decoding thread */
        pthread_mutex_lock( &data->lock );
        assert( data->status == DVSTATE_STOP );
        pthread_mutex_unlock( &data->lock );
        return DFB_OK;
     } else {
          if (data->thread) {
               direct_thread_cancel( data->thread );
               pthread_mutex_lock( &data->lock );
               pthread_cond_signal( &data->cond );
               pthread_mutex_unlock( &data->lock );
               direct_thread_join( data->thread );
               direct_thread_destroy( data->thread );
               data->thread = NULL;
          }

          if (data->destination) {
               data->destination->Release( data->destination );
               data->destination = NULL;
               data->dst_data = NULL;
          }

          data->status = DVSTATE_STOP;
     }
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GIF_GetStatus( IDirectFBVideoProvider *thiz,
                                      DFBVideoProviderStatus *status )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )
     
     if (!status)
          return DFB_INVARG;
          
     *status = data->status;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GIF_SeekTo( IDirectFBVideoProvider *thiz,
                                   double                  seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )
     
     if (seconds < 0.0)
          return DFB_INVARG;
          
     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_GIF_GetPos( IDirectFBVideoProvider *thiz,
                                   double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )
     
     if (!seconds)
          return DFB_INVARG;
          
     *seconds = 0.0;
          
     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_GIF_GetLength( IDirectFBVideoProvider *thiz,
                                      double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )
     
     if (!seconds)
          return DFB_INVARG;
          
     *seconds = 0.0;
          
     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_GIF_SetPlaybackFlags( IDirectFBVideoProvider        *thiz,
                                             DFBVideoProviderPlaybackFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )
     
     if (flags & ~(DVPLAY_LOOPING | DVPLAY_PACED))
          return DFB_UNSUPPORTED;
          
     if (flags & DVPLAY_LOOPING && !data->seekable)
          return DFB_UNSUPPORTED;

     if (flags & DVPLAY_PACED)
          data->target_frame = -1;
          
     data->flags = flags;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GIF_SetSpeed( IDirectFBVideoProvider *thiz,
                                     double                  multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )
     
     if (multiplier < 0.0)
          return DFB_INVARG;
    
     if (data->speed != multiplier) {
          pthread_mutex_lock( &data->lock ); 
          data->speed = multiplier;
          pthread_cond_signal( &data->cond );
          pthread_mutex_unlock( &data->lock );
     }
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GIF_GetSpeed( IDirectFBVideoProvider *thiz,
                                     double                 *multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GIF )
     
     if (!multiplier)
          return DFB_INVARG;
          
     *multiplier = data->speed;
     
     return DFB_OK;
}

/* exported symbols */
static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     if (!memcmp( ctx->header, "GIF89", 5 ))
          return DFB_OK;
          
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           ... )
{
     DFBResult ret;

     IDirectFBDataBuffer *buffer;
     CoreDFB             *core;
     va_list              tag;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_GIF )

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );

     data->core   = core;
     data->ref    = 1;
     data->status = DVSTATE_STOP;
     data->buffer = buffer;
     data->speed  = 1.0;
     
     buffer->AddRef( buffer );
     data->seekable = (buffer->SeekTo( buffer, 0 ) == DFB_OK);
     
     GIFReset( data );
     ret = GIFReadHeader( data );
     if (ret) {
          IDirectFBVideoProvider_GIF_Destruct( thiz );
          return ret;
     }
     
     data->buffer->GetPosition( data->buffer, &data->start_pos );
     
     pthread_mutex_init( &data->lock, NULL );
     pthread_cond_init( &data->cond, NULL );
     
     data->paused = 0;
     data->decode_surface = NULL;
     data->last_frame = 0;

     thiz->AddRef                = IDirectFBVideoProvider_GIF_AddRef;
     thiz->Release               = IDirectFBVideoProvider_GIF_Release;
     thiz->GetCapabilities       = IDirectFBVideoProvider_GIF_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_GIF_GetSurfaceDescription;
     thiz->GetStreamDescription  = IDirectFBVideoProvider_GIF_GetStreamDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_GIF_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_GIF_Stop;
     thiz->GetStatus             = IDirectFBVideoProvider_GIF_GetStatus;
     thiz->SeekTo                = IDirectFBVideoProvider_GIF_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_GIF_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_GIF_GetLength;
     thiz->SetPlaybackFlags      = IDirectFBVideoProvider_GIF_SetPlaybackFlags;
     thiz->SetSpeed              = IDirectFBVideoProvider_GIF_SetSpeed;
     thiz->GetSpeed              = IDirectFBVideoProvider_GIF_GetSpeed;
     
     return DFB_OK;
}
