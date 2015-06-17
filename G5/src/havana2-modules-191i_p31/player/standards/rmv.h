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

Source file name ; rmv.h
Author ;           Julian

Definition of the constants/macros that define useful things associated with
Real Media video streams.


Date        Modification                                    Name
----        ------------                                    --------
24-Oct-2008 Created                                         Julian

************************************************************************/

#ifndef H_RMV
#define H_RMV


#define RV20_BITSTREAM_VERSION                  2
#define RV30_BITSTREAM_VERSION                  3
#define RV40_BITSTREAM_VERSION                  4
#define RV89_BITSTREAM_VERSION_BITS             0xf0000000
#define RV89_BITSTREAM_VERSION_SHIFT            28

#define RV8_BITSTREAM_VERSION                   RV30_BITSTREAM_VERSION
#define RV9_BITSTREAM_VERSION                   RV40_BITSTREAM_VERSION

#define RV89_BITSTREAM_MINOR_BITS               0x0ff00000
#define RV89_BITSTREAM_MINOR_SHIFT              20

#define RV8_BITSTREAM_MINOR_VERSION             2
#define RV9_BITSTREAM_MINOR_VERSION             0
#define RV89_RAW_BITSTREAM_MINOR_VERSION        0x80

#define RV20_SPO_UNRESTRICTEDMV_FLAG            0x00000001
#define RV20_SPO_EXTENDEDMVRANGE_FLAG           0x00000002
#define RV20_SPO_ADVMOTIONPRED_FLAG             0x00000004
#define RV20_SPO_ADVINTRA_FLAG                  0x00000008
#define RV20_SPO_INLOOPDEBLOCK_FLAG             0x00000010
#define RV20_SPO_SLICEMODE_FLAG                 0x00000020
#define RV20_SPO_SLICESHAPE_FLAG                0x00000040      /* 0: free running; 1: rect */
#define RV20_SPO_SLICEORDER_FLAG                0x00000080      /* 0: sequential; 1: arbitrary */
#define RV20_SPO_REFPICTSELECTION_FLAG          0x00000100
#define RV20_SPO_INDEPENDENTSEGMENT_FLAG        0x00000200
#define RV20_SPO_ALTVLCTAB_FLAG                 0x00000400
#define RV20_SPO_MODCHROMAQUANT_FLAG            0x00000800
#define RV20_SPO_BFRAMES_FLAG                   0x00001000
#define RV20_SPO_DEBLOCK_STRENGTH_BITS          0x0000e000
#define RV20_SPO_NUMRESAMPLE_IMAGES_BITS        0x00070000      /* Max 8 RPR image sizes */
#define RV20_SPO_NUMRESAMPLE_IMAGES_SHIFT       16
#define RV20_SPO_FRUFLAG_FLAG                   0x00080000      /* FRU BOOL: if 1 then OFF */
#define RV20_SPO_FLIP_FLIP_INTL_FLAG            0x00100000
#define RV20_SPO_INTERLACE_FLAG                 0x00200000
#define RV20_SPO_MULTIPASS_FLAG                 0x00400000
#define RV20_SPO_INV_TELECINE_FLAG              0x00800000
#define RV20_SPO_INV_VBR_ENCODE_FLAG            0x01000000

#define RV24_SPO_SLICEMODE_FLAG                 0x00000020
#define RV40_SPO_BFRAMES_FLAG                   0x00001000
#define RV40_SPO_FRUFLAG_FLAG                   0x00080000      /* FRU BOOL: if 1 then OFF */
#define RV40_SPO_MULTIPASS_FLAG                 0x00400000
#define RV40_SPO_INV_VBR_ENCODE_FLAG            0x01000000

#define RV40_ENCODED_SIZE_WIDTH_BITS            0xffff0000
#define RV40_ENCODED_SIZE_WIDTH_SHIFT           14
#define RV40_ENCODED_SIZE_HEIGHT_BITS           0x0000ffff
#define RV40_ENCODED_SIZE_HEIGHT_SHIFT          2

#define RMV_FRAME_HEADER_TYPE_BITS              0xc0
#define RMV_FRAME_HEADER_TYPE_SHIFT             6
#define RMV_FRAME_HEADER_NUM_PACKETS_BITS       0x3f80
#define RMV_FRAME_HEADER_NUM_PACKETS_SHIFT      7
#define RMV_FRAME_HEADER_PACKET_NUMBER_BITS     0x7f

#define RMV_FORMAT_INFO_HEADER_SIZE             26

#define RMV_TEMPORAL_REF_RANGE                  0x2000
#define RMV_TEMPORAL_REF_HALF_RANGE             0x1000

#define RMV_MAX_SUPPORTED_DECODE_WIDTH          720
#define RMV_MAX_SUPPORTED_DECODE_HEIGHT         576

// Definition of picture_coding_type values
typedef enum
{
    RMV_PICTURE_CODING_TYPE_I,
    RMV_PICTURE_CODING_TYPE_FORCED_I,
    RMV_PICTURE_CODING_TYPE_P,
    RMV_PICTURE_CODING_TYPE_B,
} RMVPictureCodingType_t;


typedef struct RmvVideoSequence_s
{
    unsigned int                Length;
    unsigned int                MOFTag;
    unsigned int                SubMOFTag;
    unsigned int                Width;
    unsigned int                Height;
    unsigned int                BitCount;
    unsigned int                PadWidth;
    unsigned int                PadHeight;
    unsigned int                FramesPerSecond;        /* 32 bit fixed (16/16) */
    unsigned int                OpaqueDataSize;
    unsigned int                SPOFlags;
    unsigned int                BitstreamVersion;
    unsigned int                BitstreamMinorVersion;
    unsigned int                NumRPRSizes;
    unsigned int                RPRSize[18];            /* Same size as array in RV89Decode_Interface.h */
    unsigned int                MaxWidth;
    unsigned int                MaxHeight;
} RmvVideoSequence_t;


typedef struct RmvVideoPicture_s
{
    unsigned int                BitstreamVersion;
    unsigned int                PictureCodingType;
    unsigned int                ECC;
    unsigned int                Quant;
    unsigned int                Deblock;
    unsigned int                RvTr;
    unsigned int                PctSize;
    unsigned int                Mba;
    unsigned int                RType;
} RmvVideoPicture_t;

#if 0
/* The following structure is passed in by the application layer */
typedef struct RmfFormatInfo_s
{
    unsigned int                Length;
    unsigned int                MOFTag;
    unsigned int                SubMOFTag;
    unsigned int                Width;
    unsigned int                Height;
    unsigned int                BitCount;
    unsigned int                PadWidth;
    unsigned int                PadHeight;
    unsigned int                FramesPerSecond;        /* 32 bit fixed (16/16) */
    unsigned int                OpaqueDataSize;
    /*
    unsigned char*              OpaqueData;
    */
} RmfFormatInfo_t;
#endif

//

typedef struct RmvStreamParameters_s
{
    bool                        UpdatedSinceLastFrame;

    bool                        SequenceHeaderPresent;

    RmvVideoSequence_t          SequenceHeader;
} RmvStreamParameters_t;

#define BUFFER_RMV_STREAM_PARAMETERS            "RmvStreamParameters"
#define BUFFER_RMV_STREAM_PARAMETERS_TYPE       {BUFFER_RMV_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(RmvStreamParameters_t)}

//

#define RMV_MAX_SEGMENTS                        128
typedef struct RmvVideoSegment_s
{
        unsigned int    Valid;
        unsigned int    Offset;
} RmvVideoSegment_t;

typedef struct RmvVideoSegmentList_s
{
        unsigned int            NumSegments;
        RmvVideoSegment_t       Segment[RMV_MAX_SEGMENTS];
} RmvVideoSegmentList_t;


typedef struct RmvFrameParameters_s
{
    unsigned int                ForwardReferenceIndex;
    unsigned int                BackwardReferenceIndex;

    bool                        SegmentListPresent;
    bool                        PictureHeaderPresent;

    RmvVideoPicture_t           PictureHeader;
    RmvVideoSegmentList_t       SegmentList;

} RmvFrameParameters_t;

#define BUFFER_RMV_FRAME_PARAMETERS             "RmvFrameParameters"
#define BUFFER_RMV_FRAME_PARAMETERS_TYPE        {BUFFER_RMV_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(RmvFrameParameters_t)}

//


#endif

