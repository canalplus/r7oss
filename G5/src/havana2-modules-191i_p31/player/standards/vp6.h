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

Source file name ; vp6.h
Author ;           Julian

Definition of the constants/macros that define useful things associated with
VP6 streams.


Date        Modification                                    Name
----        ------------                                    --------
23-Jun-08   Created                                         Julian

************************************************************************/

#ifndef H_VP6
#define H_VP6



//

// Definition of picture_coding_type values
typedef enum
{
    VP6_PICTURE_CODING_TYPE_I,
    VP6_PICTURE_CODING_TYPE_P,
    VP6_PICTURE_CODING_TYPE_NONE,
} VP6PictureCodingType_t;

//

// Definition of profile
#define VP6_PROFILE_SIMPLE      0
#define VP6_PROFILE_ADVANCED    3

//

typedef struct Vp6VideoSequence_s
{
    unsigned int                version;
    unsigned int                profile;
    unsigned int                encoded_width;
    unsigned int                encoded_height;
    unsigned int                display_width;
    unsigned int                display_height;
    unsigned int                filter_mode;
    unsigned int                variance_threshold;
    unsigned int                max_vector_length;
    unsigned int                filter_selection;
} Vp6VideoSequence_t;

//

typedef struct Vp6VideoPicture_s
{
    unsigned int                ptype;
    unsigned int                pquant;
    unsigned int                golden_frame;
    unsigned int                deblock_filtering;
    /* The following four items are the final values from the range decoder in the frame parser */
    int                         high;
    int                         bits;
    unsigned long long          code_word;
    unsigned int                offset;
} Vp6VideoPicture_t;

//

typedef struct Vp6StreamParameters_s
{
    bool                        UpdatedSinceLastFrame;

    bool                        SequenceHeaderPresent;

    Vp6VideoSequence_t          SequenceHeader;
} Vp6StreamParameters_t;

#define BUFFER_VP6_STREAM_PARAMETERS            "Vp6StreamParameters"
#define BUFFER_VP6_STREAM_PARAMETERS_TYPE       {BUFFER_VP6_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(Vp6StreamParameters_t)}

//

typedef struct Vp6FrameParameters_s
{
    unsigned int                ForwardReferenceIndex;
    unsigned int                BackwardReferenceIndex;

    bool                        PictureHeaderPresent;

    Vp6VideoPicture_t           PictureHeader;

} Vp6FrameParameters_t;

#define BUFFER_VP6_FRAME_PARAMETERS             "Vp6FrameParameters"
#define BUFFER_VP6_FRAME_PARAMETERS_TYPE        {BUFFER_VP6_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(Vp6FrameParameters_t)}

//

/* The following structure is passed in by the application layer */
typedef struct Vp6MetaData_s
{
   unsigned int         Codec;                  /* VP6, VP6A */
   unsigned int         Width;
   unsigned int         Height;
   unsigned int         Duration;
   unsigned int         FrameRate;              /* x1000000 */
} Vp6MetaData_t;

#define CODEC_ID_VP6                            4
#define CODEC_ID_VP6_ALPHA                      5

#endif

