/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

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

Source file name ; mjpeg.h
Author ;           Julian

Definition of the constants/macros that define useful things associated with 
AVS streams.


Date        Modification                                    Name
----        ------------                                    --------
28-Jul-09   Created                                         Julian

************************************************************************/

#ifndef H_MJPEG
#define H_MJPEG

#define MJPEG_MAX_FRAME_SIZE                    0x10000

#define MJPEG_MAX_VIDEO_WIDTH                   720
#define MJPEG_MAX_VIDEO_HEIGHT                  576

#define MJPEG_MAX_QUANTIZATION_MATRICES         4
#define MJPEG_MAX_COMPONENTS                    5
#define MJPEG_QUANTIZATION_MATRIX_SIZE          64

#define MJPEG_SIZEOF_HUFFMAN_BITS_FIELD         16
#define MJPEG_MAX_HUFFMAN_DATA                  256
#define MJPEG_MAX_SUPPORTED_HUFFMAN_TABLES      4

// codes

#define MJPEG_SOF_0                             0xc0
#define MJPEG_SOF_1                             0xc1
#define MJPEG_SOF_2                             0xc2
#define MJPEG_SOF_3                             0xc3
#define MJPEG_SOF_5                             0xc5
#define MJPEG_SOF_6                             0xc6
#define MJPEG_SOF_7                             0xc7
#define MJPEG_SOF_8                             0xc8
#define MJPEG_JPG                               0xc8
#define MJPEG_SOF_9                             0xc9
#define MJPEG_SOF_10                            0xca
#define MJPEG_SOF_11                            0xcb
#define MJPEG_SOF_13                            0xcd
#define MJPEG_SOF_14                            0xce
#define MJPEG_SOF_15                            0xcf

#define MJPEG_DHT                               0xc4
#define MJPEG_DAC                               0xcc

#define MJPEG_RST_0                             0xd0
#define MJPEG_RST_1                             0xd1
#define MJPEG_RST_2                             0xd2
#define MJPEG_RST_3                             0xd3
#define MJPEG_RST_4                             0xd4
#define MJPEG_RST_5                             0xd5
#define MJPEG_RST_6                             0xd6
#define MJPEG_RST_7                             0xd7

#define MJPEG_SOI                               0xd8
#define MJPEG_EOI                               0xd9
#define MJPEG_SOS                               0xda

#define MJPEG_DQT                               0xdb
#define MJPEG_DNL                               0xdc
#define MJPEG_DRI                               0xdd
#define MJPEG_DHP                               0xde
#define MJPEG_EXP                               0xdf

#define MJPEG_APP_0                             0xe0
#define MJPEG_APP_1                             0xe1
#define MJPEG_APP_2                             0xe2
#define MJPEG_APP_3                             0xe3
#define MJPEG_APP_4                             0xe4
#define MJPEG_APP_5                             0xe5
#define MJPEG_APP_6                             0xe6
#define MJPEG_APP_7                             0xe7
#define MJPEG_APP_8                             0xe8
#define MJPEG_APP_9                             0xe9
#define MJPEG_APP_10                            0xea
#define MJPEG_APP_11                            0xeb
#define MJPEG_APP_12                            0xec
#define MJPEG_APP_13                            0xed
#define MJPEG_APP_14                            0xee
#define MJPEG_APP_15                            0xef

#define MJPEG_JPG_0                             0xf0
#define MJPEG_JPG_13                            0xfd
#define MJPEG_COM                               0xfe

#define MJPEG_NULL                              0x00
#define MJPEG_MARKER                            0xff


// ////////////////////////////////////////////////////////////////////////////////////
//
//  Definitions of types matching mjpeg headers
//

typedef unsigned short int MjpegQuantizationMatrix_t[MJPEG_QUANTIZATION_MATRIX_SIZE];

//

typedef struct MjpegFrameComponentInfo_s
{
    unsigned int                id;                             // 1=Y, 2=Cb, 3=Cr, 4=L, 5=Q
    unsigned int                horizontal_sampling_factor;
    unsigned int                vertical_sampling_factor;
    unsigned int                quantization_table_index;
} MjpegFrameComponentInfo_t;

//

typedef struct MjpegStartOfFrame_s
{
    unsigned int                Precision;
    unsigned int                FrameHeight;
    unsigned int                FrameWidth;
    unsigned int                NumberOfComponents;
    MjpegFrameComponentInfo_t   Components[MJPEG_MAX_COMPONENTS];
} MjpegStartOfFrame_t;

//

typedef struct MjpegHuffmanTable_s
{
    unsigned int                Id;
    unsigned int                DataSize;
    unsigned char               BitsTable[MJPEG_SIZEOF_HUFFMAN_BITS_FIELD];
    unsigned char               DataTable[MJPEG_MAX_HUFFMAN_DATA];
} MjpegHuffmanTable_t;

//

typedef struct MjpegScanComponentInfo_s
{
    unsigned int                Id;                             // 1=Y, 2=Cb, 3=Cr, 4=L, 5=Q
    unsigned int                HuffmanDCIndex;
    unsigned int                HuffmanACIndex;
} MjpegScanComponentInfo_t;

//

typedef struct MjpegStartOfScan_s
{
    unsigned int                NumberOfComponents;
    MjpegScanComponentInfo_t    Components[MJPEG_MAX_COMPONENTS];
    unsigned int                StartOfSpectralSelection;
    unsigned int                EndOfSpectralSelection;
    unsigned int                ApproximationBitPositionHigh;
    unsigned int                ApproximationBitPositionLow;
} MjpegStartOfScan_t;

// ////////////////////////////////////////////////////////////////////////////
// 
//  Definition of the natural order matrix for coefficients
//

#ifdef DEFINE_MJPEG_NATURAL_COEFFICIENT_ORDER_MATRIX
static int MjpegCoefficientMatrixNaturalOrder[MJPEG_QUANTIZATION_MATRIX_SIZE] =
{
   0,   1,  8, 16,  9,  2,  3, 10,                              // This translates zigzag matrix indices,
   17, 24, 32, 25, 18, 11,  4,  5,                              // used in coefficient transmission,
   12, 19, 26, 33, 40, 48, 41, 34,                              // to natural order indices.
   27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36,
   29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46,
   53, 60, 61, 54, 47, 55, 62, 63,
};
#endif

// ////////////////////////////////////////////////////////////////////////////////////
//
// The header definitions
//


typedef struct MjpegVideoSequence_s
{
    unsigned int                time_scale;
    unsigned int                time_delta;
} MjpegVideoSequence_t;

typedef struct MjpegVideoPictureHeader_s
{
    unsigned int                length;
    unsigned int                sample_precision;
    unsigned int                frame_height;
    unsigned int                frame_width;
    unsigned int                number_of_components;
    MjpegFrameComponentInfo_t   components[MJPEG_MAX_COMPONENTS];
} MjpegVideoPictureHeader_t;


// ////////////////////////////////////////////////////////////////////////////////////
//
// The conglomerate structures used by the player
//

//

typedef struct MjpegStreamParameters_s
{
    bool                                UpdatedSinceLastFrame;

    bool                                SequenceHeaderPresent;

    MjpegVideoSequence_t                SequenceHeader;
} MjpegStreamParameters_t;

#define BUFFER_MJPEG_STREAM_PARAMETERS          "MjpegStreamParameters"
#define BUFFER_MJPEG_STREAM_PARAMETERS_TYPE     {BUFFER_MJPEG_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(MjpegStreamParameters_t)}

typedef struct MjpegFrameParameters_s
{
    bool                                    PictureHeaderPresent;

    MjpegVideoPictureHeader_t               PictureHeader;
} MjpegFrameParameters_t;

#define BUFFER_MJPEG_FRAME_PARAMETERS           "MjpegFrameParameters"
#define BUFFER_MJPEG_FRAME_PARAMETERS_TYPE      {BUFFER_MJPEG_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(MjpegFrameParameters_t)}


#endif

