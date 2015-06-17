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
 *              BD-RLE8 FILE HEADER FIXUP
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



static DFBResult
rle_fixup_file_header (char *filename, long force_bdmode)
{
    //    Buffer and size in bytes
    void                       *buffer         = NULL;
    unsigned long               buffer_size    = 0;


    //    Image file elements and information
    unsigned long               width          = 0;
    unsigned long               height         = 0;
    unsigned long               depth          = 0;

    //    Image file boolean attributes
    unsigned long               rawmode        = 0;
    unsigned long               bdmode         = 0;
    unsigned long               topfirst       = 0;

    unsigned long               payload_size   = 0;
    unsigned long               colormap_size  = 0;

    void                        *payload       = NULL;
    void                        *colormap      = NULL;

    int                         status;

    //    We actually read the file contents into memory and collect details
    //    using a helper function (see "unit_test_rle_helpers.h").
    //    NOTE: rle_load_image will allocate payload and colormap buffers
    //          NULL buffer pointers means internal "allocation requested"
    //          Those buffers must be released.
    status  = rle_load_image   (filename, &width, &height, &depth,
                                &rawmode, &bdmode, &topfirst, &buffer_size,
                                &payload, &payload_size,
                                &colormap, &colormap_size);

    if (!status)
    {
        printf ("%s: Couldn't load image file %s\n",__FUNCTION__,filename);
        return status;
    }


    if (force_bdmode) bdmode=1;

    //DISPLAY_INFO (" colormap : %d\n",       (int)   colormap);
    //DISPLAY_INFO (" colormap_size : %d\n",  (int)   colormap_size);
    //DISPLAY_INFO (" payload : %d\n",        (int)   payload);
    //DISPLAY_INFO (" payload_size : %d\n",   (int)   payload_size);


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

    //    Now we can release colormap & payload buffers eventually allocated
    //    previously by  "rle_load_image"  helper function (since "buffer" has
    //    just been set by  "rle_build_packet")
    if (colormap)    free(colormap);  colormap = NULL;   colormap_size = 0;
    if (payload)     free(payload);   payload  = NULL;   payload_size  = 0;


    //    Write full buffer to new file
    {
        FILE  *file;
        char newfilename[512];
        sprintf (newfilename, "%s.%s", filename, "RLE");

        file = fopen(newfilename, "wb");
        if (!file)
        {
            DISPLAY_INFO ("Can't fix up : %s\n", newfilename);
            return 0;
        }
        else
        {
            DISPLAY_INFO ("Writing full buffer to : %s\n", newfilename);
            fwrite (buffer, sizeof(char), buffer_size, file);
            fclose (file);
        }
    }

    //  Release the main buffer, we don't need it anymore.
    if (buffer)      free(buffer);    buffer   = NULL;   buffer_size   = 0;

    return DFB_OK;
}



//------------------------------------------------------------------------------
//  Unit Test main
//------------------------------------------------------------------------------
int main (int argc, char **argv)
{
    int j;

    //    File name to load logo image from
    char *filename      =   NULL;
    long force_bdmode   =   1;

    DISPLAY_INFO ("Fixing up %d files\n",argc);
    for (j=1; j<argc; j++)
    {
        filename     =     argv[j];
        DISPLAY_INFO ("Fixing up : %s\n",filename);
        //    Process file
        DFBCHECK ( rle_fixup_file_header (filename, force_bdmode));
    }

    return 0;
}
