/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef H_VP8
#define H_VP8

#include <mme.h>

#define VP8_MB_FEATURE_TREE_PROBS       3
#define VP8_MAX_MB_SEGMENTS             4
#define VP8_MAX_REF_LF_DELTAS           4
#define VP8_MAX_MODE_LF_DELTAS          4

#define VP8_FRAME_START_CODE            0x9d012a

#define VP8_NUM_REF_FRAME_LISTS         3
#define VP8_REFRESH_GOLDEN_FRAME        1
#define VP8_REFRESH_ALTERNATE_FRAME     1
#define VP8_REFRESH_LAST_FRAME          1

typedef enum
{
    VP8_NO_BUFFER_TO_GOLDEN             = 0,
    VP8_LAST_FRAME_TO_GOLDEN            = 1,
    VP8_ALTERNATE_FRAME_TO_GOLDEN       = 2
} VP8_COPY_BUFFER_TO_GOLDEN;

typedef enum
{
    VP8_NO_BUFFER_TO_ALTERNATE          = 0,
    VP8_LAST_FRAME_TO_ALTERNATE         = 1,
    VP8_GOLDEN_FRAME_TO_ALTERNATE       = 2
} VP8_COPY_BUFFER_TO_ALTERNATE;

// Macroblock level features
typedef enum
{
    VP8_MB_LVL_ALT_Q            = 0,    // Use alternate Quantizer ....
    VP8_MB_LVL_ALT_LF           = 1,    // Use alternate loop filter value...
    VP8_MB_LVL_MAX              = 2,    // Number of MB level features supported
} VP8_MB_LVL_FEATURES;


// Definition of picture_coding_type values
typedef enum
{
    VP8_PICTURE_CODING_TYPE_I,
    VP8_PICTURE_CODING_TYPE_P,
    VP8_PICTURE_CODING_TYPE_NONE,
} VP8PictureCodingType_t;

typedef struct Vp8VideoSequence_s
{
    unsigned int                version;
    unsigned int                profile;
    unsigned int                encoded_width;
    unsigned int                encoded_height;
    unsigned int                decode_width;
    unsigned int                decode_height;
    unsigned int                horizontal_scale;
    unsigned int                vertical_scale;
    unsigned int                colour_space;
    unsigned int                clamping;
} Vp8VideoSequence_t;

typedef struct Vp8VideoPicture_s
{
    unsigned int                ptype;
    unsigned int                show_frame;
    unsigned int                first_part_size;
    unsigned int                filter_type;
    unsigned int                loop_filter_level;
    unsigned int                sharpness_level;
    unsigned int                refresh_golden_frame;
    unsigned int                refresh_alternate_frame;
    unsigned int                copy_buffer_to_golden;
    unsigned int                copy_buffer_to_alternate;
    unsigned int                refresh_last_frame;

} Vp8VideoPicture_t;

typedef struct Vp8StreamParameters_s
{
    bool                        UpdatedSinceLastFrame;

    bool                        SequenceHeaderPresent;

    Vp8VideoSequence_t          SequenceHeader;
} Vp8StreamParameters_t;

#define BUFFER_VP8_STREAM_PARAMETERS            "Vp8StreamParameters"
#define BUFFER_VP8_STREAM_PARAMETERS_TYPE       {BUFFER_VP8_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(Vp8StreamParameters_t)}

typedef struct Vp8FrameParameters_s
{
    unsigned int                ForwardReferenceIndex;
    unsigned int                BackwardReferenceIndex;

    bool                        PictureHeaderPresent;

    Vp8VideoPicture_t           PictureHeader;

} Vp8FrameParameters_t;

#define BUFFER_VP8_FRAME_PARAMETERS             "Vp8FrameParameters"
#define BUFFER_VP8_FRAME_PARAMETERS_TYPE        {BUFFER_VP8_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(Vp8FrameParameters_t)}


/* The following structure is passed in by the application layer */
typedef struct Vp8MetaData_s
{
    unsigned int         Codec;
    unsigned int         Width;
    unsigned int         Height;
    unsigned int         Duration;
    unsigned int         FrameRate;              /* x1000000 */
} Vp8MetaData_t;


#endif

