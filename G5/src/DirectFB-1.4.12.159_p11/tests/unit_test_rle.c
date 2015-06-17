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
 *              RLE IMAGE PROVIDER UNIT TEST
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <directfb.h>
#include <direct/messages.h>

#include "unit_test_rle_helpers.h"
#include "rle_build_packet.h"

#define DFBFAIL(err)                                           \
    {                                                          \
        fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
        DirectFBErrorFatal( #err, err );                         \
    }

#define DFBCHECK(x)                                            \
  {                                                            \
    DFBResult err = x;                                         \
                                                               \
    if (err != DFB_OK)        DFBFAIL(err);                    \
  }

#define DISPLAY_INFO(x...) do {                                \
                              if (!direct_config->quiet)       \
                                   direct_messages_info( x );  \
                           } while (0)


// WE'RE USING A CLEVER APPROACH FOR THE RLE PACKET BUILDER NOW
#define CLEVER_APPROACH

// WE'RE USING THE RLE PACKET BUILDER ONLY FOR NOW (DIRECT FILE ACCESS)
#define USE_PACKET_BUILDER_ONLY

// WE WANT TO DISPLAY SUBTITLES
#define SUBTITLES_MODE


//------------------------------------------------------------------------------
//    Pointer to user-managed buffer holding RLE packet data bytes
//------------------------------------------------------------------------------
static void                 *buffer            = NULL;

#   ifdef CLEVER_APPROACH

    static  DFBColor        *color_palette     = NULL;
    static  unsigned int     number_of_colors  = 0;

#   endif



//------------------------------------------------------------------------------
//  Destroy DirectFB data buffer
//------------------------------------------------------------------------------
static DFBResult
rle_destroy_databuffer (IDirectFBDataBuffer *databuffer)
{
    //  Release the databuffer, we don't need it anymore.
    if (databuffer)     databuffer->Release (databuffer);
    //  Release the static buffer, we don't need it anymore.
    if (buffer)         free(buffer);           buffer         = NULL;

#   ifdef CLEVER_APPROACH

    //  Release the static colormap, we don't need it anymore.
    if (color_palette)  free(color_palette);    color_palette  = NULL;
    number_of_colors = 0;

#   endif

    return DFB_OK;
}




//------------------------------------------------------------------------------
//  Create a DirectFB static data buffer holding our RLE data (read from a file)
//------------------------------------------------------------------------------
static DFBResult
rle_build_databuffer (IDirectFB *dfb, char *filename,
                                      IDirectFBDataBuffer **pdatabuffer)
{
    //  An Image provider instance can also be created from a directfb buffer
    IDirectFBDataBuffer        *databuffer     = NULL;

    //    Buffer description
    DFBDataBufferDescription    buffer_dsc;

    //    Image file elements and information
    unsigned long               width          = 0;
    unsigned long               height         = 0;
    unsigned long               depth          = 0;

    //    Image file boolean attributes
    unsigned long               rawmode    	   = 0;
    unsigned long               bdmode         = 0;
    unsigned long               topfirst       = 0;

      //     Size in bytes
    unsigned long               buffer_size    = 0;

    unsigned long               payload_size   = 0;
    unsigned long               colormap_size  = 0;

    void                        *payload       = NULL;
    void                        *colormap      = NULL;

    int                         success;

    //    We actually read the file contents into memory and collect details
    //    using a helper function (see "unit_test_rle_helpers.h").
    //    NOTE: rle_load_image will allocate payload and colormap buffers
    //          NULL buffer pointers means internal "allocation requested"
    //          Those buffers must be released.
    success = rle_load_image   (filename, &width, &height, &depth,
                                &rawmode, &bdmode, &topfirst, &buffer_size,
                                &payload, &payload_size,
                                &colormap, &colormap_size);

    if (!success)
    {
        DISPLAY_INFO ("%s: Couldn't load image file %s\n",__FUNCTION__,filename);
        return DFB_UNSUPPORTED;
    }


#   ifdef CLEVER_APPROACH

    //    Let's stash our colormap in global space first
    {
        int   i;

        number_of_colors = colormap_size/4;

        color_palette = malloc (number_of_colors*4);
        if (!color_palette) return DFB_FAILURE;

        for (i = 0; i < number_of_colors; i++)
        {
               DFBColor c;

               c.a = 0xff;
               c.r = ((u8*)colormap)[i*4+2];
               c.g = ((u8*)colormap)[i*4+1];
               c.b = ((u8*)colormap)[i*4+0];

               color_palette[i] = c;
        }
    }

    //    We won't attach the colormap to the RLE packet :
    //    RLE header and payload only
    buffer_size = RLE_HEADER_SIZE + payload_size;

    //    Static memory data buffer (packet) allocation.
    buffer = malloc (buffer_size);
    if (!buffer) return DFB_FAILURE;

    //    Let's paste our payload for encapsulation
    //    (could have been read from the PES stream fragments as well).
    memcpy (buffer+RLE_HEADER_SIZE, payload, payload_size);

    //    We only want to setup the header, thus completing the encapsulation
    //    We are using another helper function (see "rle_build_packet.h").
    //    NULL payload and colormap means that we don't want the helper
    //    function to copy anything. Zero "colormap_size" means that we don't
    //    intend to attach any colormap to the packet.
    //    "payload_size" must be set though" ...
    DFBCHECK ( rle_build_packet (buffer, buffer_size, width, height, depth,
                                                    rawmode, bdmode, topfirst,
                                                    NULL, 0,
                                                    NULL, payload_size));

    //    NOTE: FEEL FREE TO TWEAK THE HELPER FUNCTION ABOVE IN ORDER TO
    //          ACHIEVE TRUE ZERO-COPY ENCAPSULATION (PERHAPS USING THE
    //          PAYLOAD OFFSET). YOU GET THE IDEA ...
    //          THE RLE IMAGE PROVIDER DOESN'T HANDLE PAYLOAD FRAGMENTS,
    //          SO THE PAYLOAD MUST USE A CONTIGUOUS SEGMENT IN THE RLE
    //          PACKET.

#   else

    //    We attach the colormap to the RLE packet :
    //    RLE header, colormap (if available) and payload together
    buffer_size = RLE_HEADER_SIZE + colormap_size + payload_size;

    //    Static memory data buffer allocation.
    buffer = malloc (buffer_size);
    if (!buffer) return DFB_FAILURE;

    //    We now rebuild the whole image buffer, setting up the header,
    //    copying the (eventual) collected colormap and payload.
    //    We are using another helper function (see "rle_build_packet.h").
    DFBCHECK ( rle_build_packet (buffer, buffer_size, width, height, depth,
    												rawmode, bdmode, topfirst,
                                                    colormap, colormap_size,
                                                    payload, payload_size));

    //    Note: we avoided colormap & payload copies by adopting a
    //    more clever approach previously ...

#   endif

    //    Now we can release colormap & payload buffers eventually allocated
    //    previously by  "rle_load_image"  helper function (since "buffer" has
    //    just been set by  "rle_build_packet")
    if (colormap)    free(colormap);  colormap = NULL;   colormap_size = 0;
    if (payload)     free(payload);   payload  = NULL;   payload_size  = 0;

    //    An Image Provider can be obtained from a directfb data buffer that
    //    we specify and set up by ourselves using a static memory buffer
    buffer_dsc.flags            =    DBDESC_MEMORY;
    buffer_dsc.file             =    NULL;
    buffer_dsc.memory.data      =    buffer;
    buffer_dsc.memory.length    =    buffer_size;
    DFBCHECK (dfb->CreateDataBuffer (dfb, &buffer_dsc, &databuffer));
    //    Note: if no description had been specified (NULL), a streamed data
    //    buffer would have been created instead.

    //    Return newly created databuffer
    *pdatabuffer = databuffer;

    return DFB_OK;
}



//------------------------------------------------------------------------------
//  Unit Test main
//------------------------------------------------------------------------------
int main (int argc, char **argv)
{
    int i, j;
    DFBResult rle_build_databuffer_err;

    //    File name to load logo image from
    char *filename                           = NULL;

    //    Basic directfb elements
    IDirectFB               *dfb             = NULL;
    IDirectFBSurface        *primary         = NULL;
    int                      screen_width    = 0;
    int                      screen_height   = 0;

    //    The image is to be loaded into a surface that we can blit from.
    IDirectFBSurface         *logo           = NULL;

    //    Loading an image is done with an Image Provider.
    IDirectFBImageProvider   *provider       = NULL;

    //    An Image provider instance can also be created from a directfb buffer
    IDirectFBDataBuffer      *databuffer     = NULL;

    //    Surface description
    DFBSurfaceDescription     surface_dsc;



    //    Initialize directfb first
    DFBCHECK (DirectFBInit (&argc, &argv));
    DFBCHECK (DirectFBCreate (&dfb));
    DFBCHECK (dfb->SetCooperativeLevel (dfb, DFSCL_FULLSCREEN));


    //  Create primary surface
    surface_dsc.flags = DSDESC_CAPS;
    surface_dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
    DFBCHECK (dfb->CreateSurface( dfb, &surface_dsc, &primary ));
    DFBCHECK (primary->GetSize (primary, &screen_width, &screen_height));

    if (argc==1)
    {
        argv[1] = "./data/directfb.rle";
        argc++;
    }

    DISPLAY_INFO ("Rendering %d files\n",argc-1);
    for (j=1; j<argc; j++)
    {

        //
        //  --- WE CREATE OUR IMAGE PROVIDER INSTANCE HERE
        //
        filename     =     argv[j];
        DISPLAY_INFO ("Rendering : %s\n",filename);

        //  We create a directfb data buffer holding RLE image contents that we
        //  pick up from a file (could get the RLE contents from memory as well).
        //  "rle_build_databuffer" details the process of dealing with a memory
        //  RLE packet as a matter of fact.
        rle_build_databuffer_err = rle_build_databuffer (dfb, filename, &databuffer);
        if (rle_build_databuffer_err == DFB_OK) {
            //  We want to create an Image Provider tied to a directfb data buffer.
            //  DirectFB will find (or not) an Image Provider for the data type
            //  depending on Image Providers probe method (sniffing data headers)
            DFBCHECK (databuffer->CreateImageProvider (databuffer, &provider));
        }
        else {
#       ifdef   USE_PACKET_BUILDER_ONLY
            DFBFAIL(rle_build_databuffer_err);
#       else
            //  We could also create an Image Provider by passing a filename.
            //  DirectFB will find (or not) an Image Provider matching the file type.
            DFBCHECK (dfb->CreateImageProvider (dfb, filename, &provider));
#       endif
        }



        //    Get a surface description from the provider. It will contain the width,
        //    height, bits per pixel and the flag for an alphachannel if the image
        //    has one. If the image has no alphachannel the bits per pixel is set to
        //    the bits per pixel of the primary layer to use simple blitting without
        //    pixel format conversion.
        DFBCHECK (provider->GetSurfaceDescription (provider, &surface_dsc));

        //    Create a surface based on the description of the provider.
        DFBCHECK (dfb->CreateSurface( dfb, &surface_dsc, &logo ));

        //    Let the provider render to our surface. Image providers are supposed
        //    to support every destination pixel format and size. If the size
        //    differs the image will be scaled (bilinear). The last parameter allows
        //    to specify an optional destination rectangle. We use NULL here so that
        //    our image covers the whole logo surface.
        DFBCHECK (provider->RenderTo (provider, logo, NULL));
        //    Note: RLE Image Provider allows for direct non-scaled LUT-8 surface
        //          rendering without any attached colormap.

        #ifdef CLEVER_APPROACH
        //    Let's setup our logo surface palette outside of the RLE Image
        //    Provider if we got a colormap from rle_build_databuffer ...
        if (color_palette)
        {
            IDirectFBPalette *palette;
            DFBCHECK (logo->GetPalette (logo, &palette));
            palette->SetEntries (palette, color_palette, number_of_colors, 0);
            palette->Release (palette);
        }
        #endif

        //
        //  --- WE GET RID OF OUR IMAGE PROVIDER INSTANCE HERE
        //
        //    Release the provider, we don't need it anymore.
        provider->Release (provider);           provider    = NULL;
        //    Destroy the databuffer as well, we don't need it anymore.
        rle_destroy_databuffer (databuffer);    databuffer  = NULL;


#   ifndef SUBTITLES_MODE
        //    We want to let the logo slide in on the left and slide out on the
        //    right.
        for (i = -surface_dsc.width; i < screen_width; i++)
#   else
        //    We want to let the logo slide in on the right and slide out on the
        //    left.
        for (i = screen_width-1; i >= -surface_dsc.width; i--)
#   endif
        {
            //    Clear the screen.
            DFBCHECK (primary->FillRectangle (primary, 0, 0,
                                              screen_width, screen_height));

            //    Blit the logo vertically centered with "i" as the X coordinate.
            //    NULL means that we want to blit the whole surface.
            DFBCHECK (primary->Blit (primary, logo, NULL, i,
                                     (screen_height - surface_dsc.height) / 2));

            //    Flip the front and back buffer, but wait for the vertical
            //    retrace to avoid tearing.
            DFBCHECK (primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC));

            if (argc < 3)
            {
                usleep(1000*5);
            }
        }

        //    Release the image.
        if (logo)
        {
            logo->Release (logo);
        }

   }

   //    Release everything else
   primary->Release (primary);
   dfb->Release (dfb);

   return 0;
}
