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

Source file name ; h263.h
Author ;           Julian

Definition of the constants/macros that define useful things associated with
H263 streams.


Date        Modification                                    Name
----        ------------                                    --------
20-May-08   Created                                         Julian

************************************************************************/

#ifndef H_H263
#define H_H263


#define H263_PICTURE_START_CODE                         0x01

// Definition of source format
// Must match specification in 5.1.3
typedef enum
{
    H263_FORMAT_FORBIDDEN,
    H263_FORMAT_SUB_QCIF,
    H263_FORMAT_QCIF,
    H263_FORMAT_CIF,
    H263_FORMAT_4CIF,
    H263_FORMAT_16CIF,
    H263_FORMAT_RESERVED1,
    H263_FORMAT_RESERVED2,
} H263SourceFormat_t;

#define H263_WIDTH_SUB_QCIF                             128
#define H263_WIDTH_QQVGA                                160
#define H263_WIDTH_QCIF                                 176
#define H263_WIDTH_QVGA                                 320
#define H263_WIDTH_CIF                                  352
#define H263_WIDTH_4CIF                                 704
#define H263_WIDTH_16CIF                               1408

#define H263_HEIGHT_SUB_QCIF                             96
#define H263_HEIGHT_QQVGA                               120
#define H263_HEIGHT_QCIF                                144
#define H263_HEIGHT_QVGA                                240
#define H263_HEIGHT_CIF                                 288
#define H263_HEIGHT_4CIF                                576
#define H263_HEIGHT_16CIF                              1152

//

// Definition of picture_coding_type values
// Must match specification in 5.1.3
typedef enum
{
    H263_PICTURE_CODING_TYPE_I,
    H263_PICTURE_CODING_TYPE_P,
    H263_PICTURE_CODING_TYPE_NONE,
} H263PictureCodingType_t;

//

typedef struct H263VideoSequence_s
{
    unsigned int                format;
    unsigned int                width;
    unsigned int                height;
} H263VideoSequence_t;

//

typedef struct H263VideoPicture_s
{
    unsigned int                version;                // Header version - h263 = 0, flv1 = 0 or 1
    unsigned int                tref;                   // temporal reference 5.1.2
    unsigned int                ssi;                    // split screen indicator
    unsigned int                dci;                    // document camera indicator
    unsigned int                fpr;                    // freeze picture release
    unsigned int                format;                 // source format
    unsigned int                ptype;
    unsigned int                optional_bits;
    unsigned int                pquant;                 // quantizer 5.1.4
    unsigned int                cpm;                    // continuous presence multipoint 5.1.5
    unsigned int                psbi;                   // picture sub bitstream 5.1.6
    unsigned int                trb;                    // temporal ref for b pictures 5.1.7
    unsigned int                dbquant;                // quantization for b pictures 5.1.8
    unsigned int                dflag;                  // deblocking flag (flv1)
    unsigned int                bit_offset;             // bit offset of first macroblock
} H263VideoPicture_t;

//

typedef struct H263StreamParameters_s
{
    bool                        UpdatedSinceLastFrame;

    bool                        SequenceHeaderPresent;

    H263VideoSequence_t         SequenceHeader;
} H263StreamParameters_t;

#define BUFFER_H263_STREAM_PARAMETERS           "H263StreamParameters"
#define BUFFER_H263_STREAM_PARAMETERS_TYPE      {BUFFER_H263_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(H263StreamParameters_t)}

//

typedef struct H263FrameParameters_s
{
    unsigned int                ForwardReferenceIndex;
    unsigned int                BackwardReferenceIndex;

    bool                        PictureHeaderPresent;

    H263VideoPicture_t          PictureHeader;

} H263FrameParameters_t;

#define BUFFER_H263_FRAME_PARAMETERS            "H263FrameParameters"
#define BUFFER_H263_FRAME_PARAMETERS_TYPE       {BUFFER_H263_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(H263FrameParameters_t)}

//

#endif

