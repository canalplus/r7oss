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

Source file name ; vc1.h
Author ;           Mark C

Definition of the constants/macros that define useful things associated with 
VC-1 streams.


Date        Modification                                    Name
----        ------------                                    --------
02-Mar-07   Created                                         Mark C

************************************************************************/

#ifndef H_VC1
#define H_VC1

// Definition of Pes codes
#define VC1_MAX_SLICE_COUNT                             68                      // 1088/16

// Definition of start code values 
#define VC1_END_OF_SEQUENCE                             0x0A
#define VC1_SLICE_START_CODE                            0x0B
#define VC1_FIELD_START_CODE                            0x0C
#define VC1_FRAME_START_CODE                            0x0D
#define VC1_ENTRY_POINT_HEADER_CODE                     0x0E
#define VC1_SEQUENCE_HEADER_CODE                        0x0F

#define VC1_SEQUENCE_LAYER_METADATA_START_CODE          0x80
#define VC1_SIMPLE_PROFILE                              0x00
#define VC1_MAIN_PROFILE                                0x01
#define VC1_ADVANCED_PROFILE                            0x03

// An end of list code that we append 
#define MPEG2_END_OF_START_CODE_LIST                    GREATEST_SYSTEM_START_CODE

// Definition of picture_coding_type values
// Must match with VC1 MME API enum
typedef enum
{
    VC1_PICTURE_CODING_TYPE_NONE,
    VC1_PICTURE_CODING_TYPE_I,
    VC1_PICTURE_CODING_TYPE_P,
    VC1_PICTURE_CODING_TYPE_B,
    VC1_PICTURE_CODING_TYPE_BI,
    VC1_PICTURE_CODING_TYPE_SKIPPED
} Vc1PictureCodingType_t;

// Definition of frame_code_mode types
// Must match with VC1 MME API enum
typedef enum
{
    VC1_FCM_PROGRESSIVE,
    VC1_FCM_FRAME_INTERLACED,
    VC1_FCM_FIELD_INTERLACED
} Vc1PictureSyntax_t;

typedef enum
{
    VC1_MV_MODE_MV_HALF_PEL_BI,
    VC1_MV_MODE_MV,
    VC1_MV_MODE_MV_HALF_PEL,
    VC1_MV_MODE_MV_MIXED,
    VC1_MV_MODE_INTENSITY_COMP,
} Vc1MvMode_t;

typedef enum
{
    VC1_INTENSITY_COMP_NONE,
    VC1_INTENSITY_COMP_TOP,
    VC1_INTENSITY_COMP_BOTTOM,
    VC1_INTENSITY_COMP_BOTH
} Vc1IntensityFlag_t;
#define VC1_LUMASCALE_SHIFT     0
#define VC1_LUMASHIFT_SHIFT     16
#define VC1_NO_INTENSITY_COMP   0xffffffff


// Definition of matrix coefficient values

#define  VC1_MATRIX_COEFFICIENTS_FORBIDDEN              0
#define  VC1_MATRIX_COEFFICIENTS_BT709                  1
#define  VC1_MATRIX_COEFFICIENTS_UNSPECIFIED            2
#define  VC1_MATRIX_COEFFICIENTS_RESERVED               3
#define  VC1_MATRIX_COEFFICIENTS_FCC                    4
#define  VC1_MATRIX_COEFFICIENTS_BT470_BGI              5
#define  VC1_MATRIX_COEFFICIENTS_SMPTE_170M             6
#define  VC1_MATRIX_COEFFICIENTS_SMPTE_240M             7

// ////////////////////////////////////////////////////////////////////////////////////
//
// The header definitions
//

//

typedef struct Vc1VideoSequence_s
{
        unsigned int    profile;              //  2 bits - Adv, Main, Simple
        unsigned int    level;                //  3 bits - Profile level
        unsigned int    colordiff_format;     //  2 bits - Color Difference Format (1 = 4:2:0)
        unsigned int    frmrtq_postproc;      /*  3 bits */
        unsigned int    bitrtq_postproc;      /*  5 bits */
        unsigned int    postprocflag;         /*  1 bits */
        unsigned int    max_coded_width;      // 12 bits - Maximum width of picture in sequence
        unsigned int    max_coded_height;     // 12 bits - Maximum height of picture in sequence
        unsigned int    pulldown;             //  1 bit  - Determine is rptfrm, rff & tff present
        unsigned int    interlace;            //  1 bit  - Interlace content
        unsigned int    tfcntrflag;           /*  1 bit */
        unsigned int    finterpflag;          /*  1 bit */
        unsigned int    reserved;             //  1 bit  - Reserved Advanced Profile Flag
        unsigned int    psf;                  //  1 bit  - Progressive Segmented Frames
        unsigned int    display_ext;          //  1 bit  - Display size present
        unsigned int    disp_horiz_size;      // 14 bits - Horizontal display size, not picture size
        unsigned int    disp_vert_size;       // 14 bits - Vertical display size, not picture size
        unsigned int    aspect_ratio_flag;    /*  1 bit */
        unsigned int    aspect_ratio;         /*  4 bits */
        unsigned int    aspect_horiz_size;    /*  8 bits */
        unsigned int    aspect_vert_size;     /*  8 bits */
        unsigned int    frame_rate_flag;      /*  1 bit */
        unsigned int    frame_rate_ind;       /*  1 bit */
        unsigned int    frameratenr;          /*  8 bits */
        unsigned int    frameratedr;          /*  4 bits */
        unsigned int    framerateexp;         /* 16 bits */
        unsigned int    color_format_flag;    /*  1 bit */
        unsigned int    color_prim;           /*  8 bits */
        unsigned int    transfer_char;        /*  8 bits */
        unsigned int    matrix_coef;          /*  8 bits */
        unsigned int    hrd_param_flag;       /*  1 bit */
        unsigned int    hrd_num_leaky_buckets;/*  5 bits */
        /* The remaining elements in this structure are for simple and main profile.
           They will be provided by the Sequence Layer Data Structure see Annex J and L */
        unsigned int    multires;             /* 1 bit */
        unsigned int    syncmarker;           /* 1 bit */
        unsigned int    rangered;             /* 1 bit */
        unsigned int    maxbframes;           /* 3 bits */
        unsigned int    cbr;                  /* 1 bit */
} Vc1VideoSequence_t;


//

typedef struct Vc1VideoEntryPoint_s
{
        unsigned int    broken_link;
        unsigned int    closed_entry;
        unsigned int    panscan_flag;
        unsigned int    refdist_flag;
        unsigned int    loopfilter;
        unsigned int    fastuvmc;
        unsigned int    extended_mv;
        unsigned int    dquant;
        unsigned int    vstransform;
        unsigned int    overlap;
        unsigned int    quantizer;
        unsigned int    coded_size_flag;
        unsigned int    coded_width;
        unsigned int    coded_height;
        unsigned int    extended_dmv;
        unsigned int    range_mapy_flag;
        unsigned int    range_mapy;
        unsigned int    range_mapuv_flag;
        unsigned int    range_mapuv;
} Vc1VideoEntryPoint_t;

//

typedef struct Vc1VideoPicture_s
{
        unsigned int    fcm;                            // Frame Coding Mode
        unsigned int    ptype;                          // Picture Type
        unsigned int    fptype;                         // Field Picture Type
        unsigned int    tfcntr;                         // Temporal Ref Frame Counter
        unsigned int    rptfrm;                         // Repeat Frame Count
        unsigned int    tff;                            // Top Field First
        unsigned int    rff;                            // Repeat First Field
        unsigned int    first_field;                    // NOT IN STREAM : Needed to indentify first/second field
        unsigned int    ps_present;                     // Pan & Scan Flags Present
        unsigned int    ps_window_count;                // No of Pan & Scan Windows
        unsigned int    ps_hoffset[4];
        unsigned int    ps_voffset[4];
        unsigned int    ps_width[4];
        unsigned int    ps_height[4];
        unsigned int    rndctrl;                        // Rounding control bit
        unsigned int    uvsamp;
        unsigned int    refdist;
        unsigned int    backward_refdist;
        unsigned int    bfraction_numerator;
        unsigned int    bfraction_denominator;
        unsigned int    picture_layer_size;
        unsigned int    interpfrm;
        unsigned int    frmcnt;
        unsigned int    rangeredfrm;
        unsigned int    bf;
        unsigned int    pqindex;
        unsigned int    halfqp;
        unsigned int    pquant;
        unsigned int    pquantizer;
        unsigned int    postproc;
        unsigned int    numref;
        unsigned int    reffield;
        unsigned int    mvrange;
        unsigned int    dmvrange;
        unsigned int    respic;
        unsigned int    transacfrm;
        unsigned int    transacfrm2;
        unsigned int    transdctab;
        unsigned int    mvmode;
        unsigned int    mvmode2;
        unsigned int    intensity_comp_field;
        unsigned int    intensity_comp_top;
        unsigned int    intensity_comp_bottom;
} Vc1VideoPicture_t;

//

typedef struct Vc1VideoSlice_s
{
        unsigned int    no_slice_headers;                                               // NOT IN STREAM
        unsigned int    slice_start_code[VC1_MAX_SLICE_COUNT];  // NOT IN STREAM
        unsigned int    slice_addr[VC1_MAX_SLICE_COUNT];                //  9 bits
} Vc1VideoSlice_t;


// ////////////////////////////////////////////////////////////////////////////////////
//
// The conglomerate structures used by the player
//

typedef enum Vc1StreamType_e
{
    Vc1StreamTypeVc1 = 0,
    Vc1StreamTypeWMV = 1
} Vc1StreamType_t;

//

typedef struct Vc1StreamParameters_s
{
    bool                        UpdatedSinceLastFrame;

    bool                        SequenceHeaderPresent;
    bool                        EntryPointHeaderPresent;

    Vc1StreamType_t             StreamType;

    Vc1VideoSequence_t          SequenceHeader;
    Vc1VideoEntryPoint_t        EntryPointHeader;
} Vc1StreamParameters_t;

#define BUFFER_VC1_STREAM_PARAMETERS        "Vc1StreamParameters"
#define BUFFER_VC1_STREAM_PARAMETERS_TYPE   {BUFFER_VC1_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(Vc1StreamParameters_t)}

//

typedef struct Vc1FrameParameters_s
{
    unsigned int                ForwardReferenceIndex;
    unsigned int                BackwardReferenceIndex;

    bool                        SliceHeaderPresent;
    bool                        PictureHeaderPresent;

    Vc1VideoPicture_t           PictureHeader;
    Vc1VideoSlice_t             SliceHeaderList;

} Vc1FrameParameters_t;

#define BUFFER_VC1_FRAME_PARAMETERS        "Vc1FrameParameters"
#define BUFFER_VC1_FRAME_PARAMETERS_TYPE   {BUFFER_VC1_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(Vc1FrameParameters_t)}

//

#endif

