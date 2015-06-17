/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name ; theora.h
Author ;           Julian

Definition of the constants/macros that define useful things associated with
Ogg Theora streams.


Date        Modification                                    Name
----        ------------                                    --------
10-Mar-09   Created                                         Julian

************************************************************************/

#ifndef H_THEORA
#define H_THEORA

/* Incoming information header values */
#define THEORA_IDENTIFICATION_HEADER            0x80
#define THEORA_COMMENT_HEADER                   0x81
#define THEORA_SETUP_HEADER                     0x82

#define THEORA_PIXEL_FORMAT_420                 0x00
#define THEORA_PIXEL_FORMAT_RESERVED            0x01
#define THEORA_PIXEL_FORMAT_422                 0x02
#define THEORA_PIXEL_FORMAT_444                 0x03

// Definition of picture_coding_type values
typedef enum
{
    THEORA_PICTURE_CODING_TYPE_I,
    THEORA_PICTURE_CODING_TYPE_P,
    THEORA_PICTURE_CODING_TYPE_NONE,
} TheoraPictureCodingType_t;

typedef struct TheoraVideoSequence_s
{
    unsigned int                Version;
    unsigned int                MacroblockWidth;
    unsigned int                MacroblockHeight;
    unsigned int                DecodedWidth;
    unsigned int                DecodedHeight;
    unsigned int                DisplayWidth;
    unsigned int                DisplayHeight;
    unsigned int                FrameRateNumerator;
    unsigned int                FrameRateDenominator;
    unsigned int                PixelAspectRatioNumerator;
    unsigned int                PixelAspectRatioDenominator;
    unsigned int                ColourSpace;
    unsigned int                PixelFormat;

#if defined (PARSE_THEORA_HEADERS)
    /* Items needed by the codec */
    unsigned char               LoopFilterLimit[64];
    unsigned int                AcScale[64];
    unsigned short              DcScale[64];

    unsigned char               BaseMatrix[384][64];
    unsigned char               QRCount[2][3];
    unsigned char               QRSize [2][3][64];
    unsigned short              QRBase[2][3][64];

    int                         hti;
    unsigned int                HBits;
    int                         Entries;
    int                         HuffmanCodeSize;
    unsigned short              HuffmanTable[80][32][2];
#else
    /* Copy the 3 Theora headers as opaque data to the firmware */
    unsigned int                InfoHeaderBuffer[512];
    unsigned int                CommentHeaderBuffer[4096];
    unsigned int                SetupHeaderBuffer[98304];       /* 384*64*4 */
    unsigned int                InfoHeaderSize;
    unsigned int                CommentHeaderSize;
    unsigned int                SetupHeaderSize;
#endif

} TheoraVideoSequence_t;

//

typedef struct TheoraVideoPicture_s
{
    TheoraPictureCodingType_t      PictureType;
} TheoraVideoPicture_t;

//

typedef struct theoraStreamParameters_s
{
    bool                        UpdatedSinceLastFrame;

    bool                        SequenceHeaderPresent;

    TheoraVideoSequence_t          SequenceHeader;
} TheoraStreamParameters_t;

#define BUFFER_THEORA_STREAM_PARAMETERS            "TheoraStreamParameters"
#define BUFFER_THEORA_STREAM_PARAMETERS_TYPE       {BUFFER_THEORA_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(TheoraStreamParameters_t)}

//

typedef struct TheoraFrameParameters_s
{
    unsigned int                ForwardReferenceIndex;
    unsigned int                BackwardReferenceIndex;

    bool                        PictureHeaderPresent;

    TheoraVideoPicture_t        PictureHeader;

} TheoraFrameParameters_t;

#define BUFFER_THEORA_FRAME_PARAMETERS             "TheoraFrameParameters"
#define BUFFER_THEORA_FRAME_PARAMETERS_TYPE        {BUFFER_THEORA_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(TheoraFrameParameters_t)}

//


#endif

