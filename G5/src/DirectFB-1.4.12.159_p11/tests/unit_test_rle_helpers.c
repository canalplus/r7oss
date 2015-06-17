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
 *              RLE/BMP FILE FORMAT HELPER LIBRARY
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "unit_test_rle_helpers.h"

typedef    unsigned long    DATA32;

/*******************************************************************************
 *
 *                      RLE/BMP TYPE DEFINITIONS
 *
 *******************************************************************************/

typedef struct tagRGBQUAD {
   unsigned char       rgbBlue;
   unsigned char       rgbGreen;
   unsigned char       rgbRed;
   unsigned char       rgbReserved;
} RGBQUAD;


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


/*******************************************************************************
 *
 *                      RLE/BMP LITTLE-ENDIAN UTILITIES
 *
 *******************************************************************************/

static int    ReadleShort             (FILE * file, unsigned short *ret)
{
   unsigned char       b[2];

   if (fread(b, sizeof(unsigned char), 2, file) != 2) return 0;

   *ret = (b[1] << 8) | b[0];
   return 1;
}

static int    ReadleSignedShort       (FILE * file, signed short *ret)
{
   unsigned char       b[2];

   if (fread(b, sizeof(unsigned char), 2, file) != 2) return 0;

   *ret = (b[1] << 8) | b[0];
   return 1;
}

static int    ReadleLong              (FILE * file, unsigned long *ret)
{
   unsigned char       b[4];

   if (fread(b, sizeof(unsigned char), 4, file) != 4) return 0;

   *ret = (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
   return 1;
}

static int    ReadleSignedLong        (FILE * file, signed long *ret)
{
   unsigned char       b[4];

   if (fread(b, sizeof(unsigned char), 4, file) != 4) return 0;

   *ret = (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
   return 1;
}

/*******************************************************************************
 *
 *                      RLE/BMP FILE FORMAT HELPER FUNCTION
 *
 *******************************************************************************/

int  rle_load_image        (char              *filename,
                            unsigned long     *width_ptr,
                            unsigned long     *height_ptr,
                            unsigned long     *depth_ptr,
                            unsigned long     *rawmode_ptr,
                            unsigned long     *bdmode_ptr,
                            unsigned long     *topfirst_ptr,
                            unsigned long     *buffer_size_ptr,
                            void             **payload_ptr,
                            unsigned long     *payload_size_ptr,
                            void             **colormap_ptr,
                            unsigned long     *colormap_size_ptr)
{
   FILE               *f;
   char                type[2];
   unsigned long       size, offset, headSize, comp, imgsize;
   unsigned short      tmpShort, planes, bitcount, ncols;
   signed short        tmpSignedShort;
   signed long         tmpSignedLong;
   unsigned long       topfirst;
   unsigned long       i, w, h;
   RGBQUAD             rgbQuads[256];
   unsigned long       rmask = 0xff, gmask = 0xff, bmask = 0xff;
   unsigned long       rshift = 0, gshift = 0, bshift = 0;

   const    unsigned long    preambleSize = 14;

    f = fopen(filename, "rb");
	if (!f)
	{
		return 0;
	}

	/* HEADER */
	{
		struct stat statbuf;

		if (stat(filename, &statbuf)== -1)
		{
			fclose(f);
			return 0;
		}
		size = statbuf.st_size;

		if (fread(type, 1, 2, f)!= 2)
		{
			fclose(f);
			return 0;
		}

#       define IS_MAGIC(ptr,l,h)       ((ptr[0]==l) &&  (ptr[1]==h))
		if (!IS_MAGIC(type, 'R','L') && !IS_MAGIC(type, 'B','M'))
		{
			fclose(f);
			return 0;
		}

		fseek(f, 8, SEEK_CUR);
		ReadleLong(f, &offset);
		ReadleLong(f, &headSize);
		if (offset >= size || offset < headSize + preambleSize)
		{
			fclose(f);
			return 0;
		}

		/* HEADER DATA */
		switch (headSize)
		{
		case 40:
			ReadleLong(f, &w);
			tmpSignedLong = 0;
			ReadleSignedLong(f, &tmpSignedLong);
			topfirst = tmpSignedLong < 0 ? 1 : 0;
			h = topfirst ? -tmpSignedLong : tmpSignedLong;
			ReadleShort(f, &planes);
			ReadleShort(f, &bitcount);
			ReadleLong(f, &comp);
			ReadleLong(f, &imgsize);
			imgsize = size - offset;

			fseek(f, 16, SEEK_CUR);
			break;

		case 12:
			ReadleShort(f, &tmpShort);
			w = tmpShort;
			tmpSignedShort = 0;
			ReadleSignedShort(f, &tmpSignedShort);
			topfirst = tmpSignedShort < 0 ? 1 : 0;
			h = topfirst ? -tmpSignedShort : tmpSignedShort;
			ReadleShort(f, &planes);
			ReadleShort(f, &bitcount);
			imgsize = size - offset;
			comp = RLEIC_NONE;
			//break;                    /* UNSUPPORTED BACKWARD COMPAT. */
		default:
			fclose(f);
			return 0;

		}

		if (planes!=1)
		{
			fclose(f);
			return 0;
		}

		if ((w < 1) || (h < 1) || (w > 8192) || (h > 8192))
		{
			fclose(f);
			return 0;
		}

		ncols = 0;

		if (bitcount < 16)
		{
			/* COLORMAP DATA */
			switch (headSize)
			{
			case 40:
				ncols = (offset - headSize - preambleSize) / 4;
				if (ncols > 256)
					ncols = 256;
				fread(rgbQuads, 4, ncols, f);
				break;

			case 12:
				ncols = (offset - headSize - preambleSize) / 3;
				if (ncols > 256)
					ncols = 256;
				for (i = 0; i < ncols; i++)
					fread(&rgbQuads[i], 3, 1, f);
				//break;              /* UNSUPPORTED BACKWARD COMPAT. */
			default:
				fclose(f); /* UNSUPPORTED BACKWARD COMPAT. */
				return 0;

			}
		}
		else if (bitcount == 16 || bitcount == 32)
		{
			if (comp == RLEIC_BITFIELDS)
			{
				int bit;

				ReadleLong(f, &bmask);
				ReadleLong(f, &gmask);
				ReadleLong(f, &rmask);
				for (bit = bitcount - 1; bit >= 0; bit--)
				{
					if (bmask & (1 << bit))
						bshift = bit;
					if (gmask & (1 << bit))
						gshift = bit;
					if (rmask & (1 << bit))
						rshift = bit;
				}
				fclose(f); /* UNSUPPORTED BACKWARD COMPAT. */
				return 0;
			}
			else if (bitcount == 16)
			{
				rmask = 0x7C00;
				gmask = 0x03E0;
				bmask = 0x001F;
				rshift = 10;
				gshift = 5;
				bshift = 0;
			}
			else if (bitcount == 32)
			{
				rmask = 0x00FF0000;
				gmask = 0x0000FF00;
				bmask = 0x000000FF;
				rshift = 16;
				gshift = 8;
				bshift = 0;
			}
		}

	}

	if (payload_ptr)
	{
		fseek(f, offset, SEEK_SET);
        if (!*payload_ptr)
        {
            *payload_ptr = malloc(imgsize);
        }
        else if (!payload_size_ptr || *payload_size_ptr < imgsize)
        {
            return 0;
        }


		if (!*payload_ptr)
		{
			fclose(f);
			return 0;
		}

		fread(*payload_ptr, imgsize, 1, f);
	}

    fclose(f);



	if (colormap_ptr && bitcount < 16)
	{

        if (ncols==0)
        {
            char palname[512];
            sprintf (palname, "%s.%s", filename, "pal");
            f = fopen(palname, "rb");
            if (f)
            {
                fread(rgbQuads, 4, ncols=256, f);
                fclose(f);
            }
        }


		if (ncols>0)
		{
            if (!*colormap_ptr)
            {
                *colormap_ptr = malloc(sizeof(RGBQUAD)*ncols);

                if (!*colormap_ptr)
                {
                     return 0;
                }
            }
            else if (!colormap_size_ptr
                    || *colormap_size_ptr < sizeof(RGBQUAD)*ncols)
            {
                return 0;
            }

			memcpy(*colormap_ptr, rgbQuads, sizeof(RGBQUAD)*ncols);
		}
		else
		{
			*colormap_ptr = NULL;
		}
	}



    if (payload_size_ptr)
        *payload_size_ptr = imgsize;

	if (colormap_size_ptr)
		*colormap_size_ptr = sizeof(RGBQUAD)*ncols;

    if (width_ptr)
		*width_ptr = w;
	if (height_ptr)
		*height_ptr = h;
	if (depth_ptr)
		*depth_ptr = bitcount;
	if (rawmode_ptr)
		*rawmode_ptr  = comp == RLEIC_NONE		? 1:0;
	if (bdmode_ptr)
		*bdmode_ptr   = comp == RLEIC_BD_RLE8	? 1:0;
	if (topfirst_ptr)
		*topfirst_ptr = comp == RLEIC_BD_RLE8   ? 1:topfirst;
	if (buffer_size_ptr)
		*buffer_size_ptr =preambleSize+headSize+imgsize+sizeof(RGBQUAD)*ncols;

	return 1;
}

