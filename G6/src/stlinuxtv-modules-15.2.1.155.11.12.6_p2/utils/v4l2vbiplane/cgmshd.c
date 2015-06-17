/***********************************************************************
 *
 * File: linux/tests/v4l2vbiplane/cgmshd.c
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

/******************************************************************************/
/* Library includes                                                           */
/******************************************************************************/
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>

#include <linux/fb.h>
#include <linux/videodev2.h>

#include "linux/include/linux/stmvout.h"
#include "utils/v4l2_helper.h"

/******************************************************************************/
/* Modules includes                                                           */
/******************************************************************************/

#include "cgmshd.h"

/******************************************************************************/
/* Private constants                                                          */
/******************************************************************************/

#define PIXEL_LOGICAL_0        0x800F0F0F   /* ARGB: 80=opaque, 0F =  6% x FF */
#define PIXEL_LOGICAL_1        0x80B0B0B0   /* ARGB: 80=opaque, B0 = 70% x FF */


/******************************************************************************/
/* Private macros                                                             */
/******************************************************************************/

/******************************************************************************/
/* Private types                                                              */
/******************************************************************************/

typedef struct
{
    unsigned long    StartPosition;       /* Position of Start Symbol in pixels */
    unsigned long    BitWidthStart;       /* Width of Start Symbol bit in pixels */
    unsigned long    BitCountStart;       /* Number of bits for Start Symbol */
    unsigned long    BitWidthHeader;      /* Width of Header bit in pixels */
    unsigned long    BitCountHeader;      /* Number of bits for Header */
    unsigned long    BitWidthPayload;     /* Width of Payload bit in pixel */
    unsigned long    BitCountPayload;     /* Number of bits for Payload */

} CGMSHDi_Waveform_t;

typedef struct
{
    int    ByteOffset;
    int    BitOffset;

} PayloadOffset_t;


/******************************************************************************/
/* Private functions prototypes                                               */
/******************************************************************************/

/******************************************************************************/
/* Public variables declarations                                              */
/******************************************************************************/

/******************************************************************************/
/* Private variables declarations                                             */
/******************************************************************************/

static const CGMSHDi_Waveform_t CgmsWaveforms[] =
{  /* Blank    Start     Header      Payload  */
     {   33,      26, 2,    26, 6,      26,  14   }       /* CGMS_HD_480P60000  */
  ,  {    4,      58, 2,    58, 6,      58,  14   }       /* CGMS_HD_720P60000  */
  ,  {  116,      77, 2,    77, 6,      77,  14   }       /* CGMS_HD_1080I60000 */
  ,  {   33,       4, 2,     4, 6,       4, 128   }       /* CGMS_TYPE_B_HD_480P60000  */
  ,  {    4,       8, 2,     8, 6,       8, 128   }       /* CGMS_TYPE_B_HD_720P60000  */
  ,  {  116,      10, 2,    10, 6,      10, 128   }       /* CGMS_TYPE_B_HD_1080I60000 */
};

static const PayloadOffset_t HeaderPosition[] =
{  /* Byte, Bit */
     { 0, 3 }           /* CGMS_HD_480P60000  */
  ,  { 0, 3 }           /* CGMS_HD_720P60000  */
  ,  { 0, 3 }           /* CGMS_HD_1080I60000 */
  ,  { 0, 5 }           /* CGMS_TYPE_B_HD_480P60000  */
  ,  { 0, 5 }           /* CGMS_TYPE_B_HD_720P60000  */
  ,  { 0, 5 }           /* CGMS_TYPE_B_HD_1080P60000 */
};

static const PayloadOffset_t PayloadPosition[] =
{  /* Byte, Bit */
     { 1, 5 }           /* CGMS_HD_480P60000  */
  ,  { 1, 5 }           /* CGMS_HD_720P60000  */
  ,  { 1, 5 }           /* CGMS_HD_1080I60000 */
  ,  { 1, 7 }           /* CGMS_TYPE_B_HD_480P60000  */
  ,  { 1, 7 }           /* CGMS_TYPE_B_HD_720P60000  */
  ,  { 1, 7 }           /* CGMS_TYPE_B_HD_1080P60000 */
};


/******************************************************************************/
/* File private functions                                                     */
/******************************************************************************/

/******************************************************************************/
/* Name        : DrawBit                                                      */
/* Description : Draws a logical CGMS bit into bitmap                         */
/*                                                                            */
/* Parameters  : IN  Pixel_pp - Pointer to pointer to pixel                   */
/*               IN  BitValue - 0 or 1                                        */
/*               IN  BitWidth - CGMS bit width in pixel                       */
/*               OUT Pixel_pp - Pointer to pixel incremented                  */
/*                                                                            */
/* Assumptions : Valid parameters                                             */
/* Limitations :                                                              */
/* Returns     : Nothing                                                      */
/******************************************************************************/
static void DrawBit( unsigned long** Pixel_pp, unsigned char BitValue, unsigned long BitWidth )
{
    int  i;
    unsigned long  PixelValue = PIXEL_LOGICAL_0;

    /* Set pixel value according to requested BitValue */
    if( BitValue != 0 )
    {
        PixelValue = PIXEL_LOGICAL_1;
    }

    /* Draws bit ie: BitWidth pixels set at PixelValue */
    for( i=0; i<BitWidth; i++ )
    {
        **Pixel_pp = PixelValue;
        (*Pixel_pp)++;
    }
}

/******************************************************************************/
/* Name        : DrawPadding                                                  */
/* Description : Draws blank padding present before Start Symbol              */
/*                                                                            */
/* Parameters  : IN  Pixel_pp - Pointer to pointer to pixel                   */
/*               IN  Standard - Requested CGMS standard                       */
/*               OUT Pixel_pp - Pointer to pixel incremented                  */
/*                                                                            */
/* Assumptions : Valid parameters                                             */
/* Limitations :                                                              */
/* Returns     : Nothing                                                      */
/******************************************************************************/
static void DrawPadding( unsigned long** Pixel_pp, cgms_standard_t Standard )
{
    int  i;
    unsigned long  PaddingWidth = CgmsWaveforms[Standard].StartPosition;

    /* Draws blank interval ie: BlankWidth pixels set at logical 0 */
    for( i=0; i<PaddingWidth; i++ )
    {
        **Pixel_pp = PIXEL_LOGICAL_0;
        (*Pixel_pp)++;
    }
}

/******************************************************************************/
/* Name        : DrawStartSymbol                                              */
/* Description : Draws the Start Symbol                                       */
/*                                                                            */
/* Parameters  : IN  Pixel_pp - Pointer to pointer to pixel                   */
/*               IN  Standard - Requested CGMS standard                       */
/*               OUT Pixel_pp - Pointer to pixel incremented                  */
/*                                                                            */
/* Assumptions : Valid parameters                                             */
/* Limitations :                                                              */
/* Returns     : Nothing                                                      */
/******************************************************************************/
static void DrawStartSymbol( unsigned long** Pixel_pp, cgms_standard_t Standard )
{
    unsigned long  BitWidthStart = CgmsWaveforms[Standard].BitWidthStart;

    /* Draws Start Symbol ie: Logical 1 then Logical 0 */
    DrawBit( Pixel_pp, 1, BitWidthStart );
    DrawBit( Pixel_pp, 0, BitWidthStart );
}

/******************************************************************************/
/* Name        : DrawHeader                                                   */
/* Description : Draws the Header                                             */
/*                                                                            */
/* Parameters  : IN  Pixel_pp - Pointer to pointer to pixel                   */
/*               IN  Standard - Requested CGMS standard                       */
/*               IN  Data_p   - Header data                                   */
/*               OUT Pixel_pp - Pointer to pixel incremented                  */
/*                                                                            */
/* Assumptions : Valid parameters                                             */
/* Limitations :                                                              */
/* Returns     : Nothing                                                      */
/******************************************************************************/
static void DrawHeader( unsigned long**             Pixel_pp
                      , cgms_standard_t             Standard
                      , const unsigned char* const  Data_p
                      )
{
    /* Header waveform characteristics */
    unsigned long  BitCountHeader = CgmsWaveforms[Standard].BitCountHeader;
    unsigned long  BitWidthHeader = CgmsWaveforms[Standard].BitWidthHeader;

    /* Byte and bit offset in Data_p to reach Header data */
    int  ByteOffset = HeaderPosition[Standard].ByteOffset;
    int  BitOffset  = HeaderPosition[Standard].BitOffset;

    int  i;

    for( i=0; i<BitCountHeader; i++ )
    {
        /* Check if we need to move to next byte in Data_p */
        if( BitOffset < 0 )
        {
            /* points to next byte in Data_p */
            BitOffset = 7;
            ByteOffset++;
        }

        /* Retrieve bit value from Data_p */
        if( ( ( Data_p[ByteOffset] >> BitOffset ) & 0x01 ) == 1 )
        {
            /* Bit 1 to be drawn */
            DrawBit( Pixel_pp, 1, BitWidthHeader );
        }
        else
        {
            /* Bit 0 to be drawn */
            DrawBit( Pixel_pp, 0, BitWidthHeader );
        }

        /* Next bit */
        BitOffset--;
    }
}

/******************************************************************************/
/* Name        : DrawPayload                                                  */
/* Description : Draws the Payload                                            */
/*                                                                            */
/* Parameters  : IN  Pixel_pp - Pointer to pointer to pixel                   */
/*               IN  Standard - Requested CGMS standard                       */
/*               IN  Data_p   - Payload data                                  */
/*               OUT Pixel_pp - Pointer to pixel incremented                  */
/*                                                                            */
/* Assumptions : Valid parameters                                             */
/* Limitations :                                                              */
/* Returns     : Nothing                                                      */
/******************************************************************************/
static void DrawPayload( unsigned long**            Pixel_pp
                       , cgms_standard_t            Standard
                       , const unsigned char* const Data_p
                       )
{
    /* Header waveform characteristics */
    unsigned long  BitCountPayload = CgmsWaveforms[Standard].BitCountPayload;
    unsigned long  BitWidthPayload = CgmsWaveforms[Standard].BitWidthPayload;

    /* Byte and bit offset in Data_p to reach Payload data */
    int  ByteOffset = PayloadPosition[Standard].ByteOffset;
    int  BitOffset  = PayloadPosition[Standard].BitOffset;

    int  i;

    for( i=0; i<BitCountPayload; i++ )
    {
        /* Check if we need to move to next byte in Data_p */
        if( BitOffset < 0 )
        {
            /* points to next byte in Data_p */
            BitOffset = 7;
            ByteOffset++;
        }

        /* Retrieve bit value from Data_p */
        if( ( ( Data_p[ByteOffset] >> BitOffset ) & 0x01 ) == 1 )
        {
            /* Bit 1 to be drawn */
            DrawBit( Pixel_pp, 1, BitWidthPayload );
        }
        else
        {
            /* Bit 0 to be drawn */
            DrawBit( Pixel_pp, 0, BitWidthPayload );
        }

        /* Next bit */
        BitOffset--;
    }
}

/******************************************************************************/
/* Name        : DuplicateLine                                                */
/* Description : Copy line 1 content to line 2                                */
/*                                                                            */
/* Parameters  : IN  PixelDest_p - Destination                                */
/*               IN  PixelSrc_p - Source                                      */
/*               IN  Standard - Requested CGMS standard                       */
/*                                                                            */
/* Assumptions : Valid parameters                                             */
/* Limitations :                                                              */
/* Returns     : Nothing                                                      */
/******************************************************************************/
static void DuplicateLine( unsigned long*   PixelDest_p
                         , unsigned long*   PixelSrc_p
                         , cgms_standard_t  Standard
                         )
{
    int SizeInBytes;

    /* Compute how many bytes to be copied */
    /* Padding + Start Symbol + Header + Payload in pixels */
    /* One pixel is 4 bytes */
    SizeInBytes =   CgmsWaveforms[Standard].StartPosition
                  + CgmsWaveforms[Standard].BitCountStart   * CgmsWaveforms[Standard].BitWidthStart
                  + CgmsWaveforms[Standard].BitCountHeader  * CgmsWaveforms[Standard].BitWidthHeader
                  + CgmsWaveforms[Standard].BitCountPayload * CgmsWaveforms[Standard].BitWidthPayload;
    SizeInBytes *= sizeof(unsigned long);

    /* Do the copy */
    memcpy( PixelDest_p, PixelSrc_p, SizeInBytes );
}


/******************************************************************************/
/* Module public functions                                                    */
/******************************************************************************/

/******************************************************************************/
/* Name        : CGMSHD_DrawWaveform                                          */
/* Description : Draw Payload waveform in Bitmap according to standard        */
/*                                                                            */
/* Parameters  : IN  Standard - Requested CGMS standard                       */
/*               IN  Data_p - CGMS Payload                                    */
/*                                                                            */
/* Assumptions : Vbi_p is not NULL                                            */
/* Limitations :                                                              */
/* Returns     : Error code                                                   */
/******************************************************************************/
void CGMSHD_DrawWaveform( cgms_standard_t Standard
                        , unsigned char  *Data_p
                        , int BytesPerLine
                        , unsigned long  *BufferData_p
                        )
{
    unsigned long* PixelField1_p = NULL;    /* Always first bitmap line */
    unsigned long* PixelField2_p = NULL;    /* Second bitmap line only if interlaced */

    /* Get bitmap lines to be drawn */
    /* Interlaced  mode means two identical lines on field 1 & 2 */
    /* Progressive mode means one line only on frame 0 ie field 1 here */

    PixelField1_p = BufferData_p;

    if(    ( Standard == CGMS_HD_1080I60000        )
        || ( Standard == CGMS_TYPE_B_HD_1080I60000 )   )
    {
        PixelField2_p = (BufferData_p + BytesPerLine/4);
    }

    /* We first draw line 0 (field one) and duplicate it if needed */
    /* CGMS waveform is composed of:  */
    /* Blank padding, Start Symbol, Header, Payload */
    DrawPadding    ( &PixelField1_p, Standard );            /* Fixed pattern */
    DrawStartSymbol( &PixelField1_p, Standard );            /* Fixed pattern */
    DrawHeader     ( &PixelField1_p, Standard, Data_p );    /* From Data_p */
    DrawPayload    ( &PixelField1_p, Standard, Data_p );    /* From Data_p */

    /* Duplicate first line if interlaced */
    if( PixelField2_p )
    {
        /* PixelField1_p modified during first line drawing */
        /* Reset it */
        PixelField1_p = BufferData_p;
        DuplicateLine( PixelField2_p, PixelField1_p, Standard );
    }
}
