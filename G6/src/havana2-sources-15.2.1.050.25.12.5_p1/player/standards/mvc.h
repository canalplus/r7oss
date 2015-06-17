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

#ifndef H_MVC
#define H_MVC

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

//
// Constants defining the NAL unit types
//

#define NALU_TYPE_PREFIX                                14              // MVC prefix (for base view slices)
#define NALU_TYPE_SUB_SPS                               15              // Subset Sequence Parameter Set
#define NALU_TYPE_SLICE_EXT                             20              // Slice Coded Dependent View

//
// Constants defining the supplemental
//

#define SEI_MVC_SCALABLE_NESTING            37
#define SEI_VIEW_SCALABILITY_INFO           38

//
// Profile constants
//

#define MVC_PROFILE_IDC_STEREO_HIGH                    128

//
// Memory management control operations
//

// MVC specifics

// Encode frame index. 0 and FFFFFFFF have special meanings
#define MVC_ENCODE_DEP_FRAME_INDEX(I)   ( (unsigned int) ( -(int)((I)+10)) )
#define MVC_IS_DEP_FRAME_INDEX(I)   ((int)(I) < 0)
#define MVC_DECODE_DEP_FRAME_INDEX(I)   ( (unsigned int) (-((int)(I))-10) )


// We support Stereo(2) MVC view streams
#define MVC_MAX_NUM_VIEWS_MINUS1 1
#define MVC_MAX_VIEW_ID 1024 // 10 bits according to annex H


// ////////////////////////////////////////////////////////////////////////////
//
//  Definitions of types matching MVC headers
//

//
// The SubSet Sequence parameter set structure
//

typedef struct MVCSubSequenceParameterSetHeader_s
{
    H264SequenceParameterSetHeader_t h264_header;

    /* MVC Extension*/
    unsigned int                num_views_minus1;
    unsigned int                view_id[MVC_MAX_NUM_VIEWS_MINUS1 + 1];
    unsigned int                num_anchor_refs_l0[MVC_MAX_NUM_VIEWS_MINUS1 + 1];
    unsigned int                num_anchor_refs_l1[MVC_MAX_NUM_VIEWS_MINUS1 + 1];
    unsigned int                num_non_anchor_refs_l0[MVC_MAX_NUM_VIEWS_MINUS1 + 1];
    unsigned int                num_non_anchor_refs_l1[MVC_MAX_NUM_VIEWS_MINUS1 + 1];
    unsigned int                anchor_ref_l0[MVC_MAX_NUM_VIEWS_MINUS1 + 1][MVC_MAX_NUM_VIEWS_MINUS1 + 1];
    unsigned int                anchor_ref_l1[MVC_MAX_NUM_VIEWS_MINUS1 + 1][MVC_MAX_NUM_VIEWS_MINUS1 + 1];
    unsigned int                non_anchor_ref_l0[MVC_MAX_NUM_VIEWS_MINUS1 + 1][MVC_MAX_NUM_VIEWS_MINUS1 + 1];
    unsigned int                non_anchor_ref_l1[MVC_MAX_NUM_VIEWS_MINUS1 + 1][MVC_MAX_NUM_VIEWS_MINUS1 + 1];
} MVCSubSequenceParameterSetHeader_t;


//
// Reference picture list re-ordering data (slice header sub-structure)
//

typedef struct MVCRefPicListReordering_s
{
    H264RefPicListReordering_t  h264RefPicListreordering;
    unsigned int                l0_abs_diff_view_idx_minus1[H264_MAX_REF_PIC_REORDER_OPERATIONS];       // ue(v)
    unsigned int                l1_abs_diff_view_idx_minus1[H264_MAX_REF_PIC_REORDER_OPERATIONS];       // ue(v)
} MVCRefPicListReordering_t;


// ////////////////////////////////////////////////////////////////////////////
//
//  Specific defines and constants for the codec layer
//

typedef struct MVCStreamParameters_s
{
    // Caution: must begin with the same members as H264StreamParameters_t

    // base picture
    H264SequenceParameterSetHeader_t            *SequenceParameterSet;
    H264SequenceParameterSetExtensionHeader_t   *SequenceParameterSetExtension;
    H264PictureParameterSetHeader_t             *PictureParameterSet;

    // dependent picture
    H264PictureParameterSetHeader_t             *DepPictureParameterSet;
    MVCSubSequenceParameterSetHeader_t          *SubsetSequenceParameterSet;
} MVCStreamParameters_t;

#define BUFFER_MVC_STREAM_PARAMETERS           "MVCStreamParameters"
#define BUFFER_MVC_STREAM_PARAMETERS_TYPE      {BUFFER_MVC_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(MVCStreamParameters_t)}

//

typedef struct MVCFrameParameters_s
{
    H264SliceHeader_t                           SliceHeader;
    H264SEIPanScanRectangle_t                   SEIPanScanRectangle;            // Current pan scan rectangle.
    H264SEIFramePackingArrangement_t            SEIFramePackingArrangement;

    bool                    isInterview;    // inter_view_flag of base picture is 1
    H264SliceHeader_t                           DepSliceHeader; // dependent view slice header info
    unsigned int                                DepDataOffset;  // same as ParsedFrameParameters->DataOffset, but for dep picture
    unsigned int                DepSlicesLength;// size of input buffer for preprocessor (sum of length of all slices 20)
    bool                    isAnchor;   // anchor_pic_flag == 1

    // Reference frame lists for the dependent view picture
    // Note: if a reference has the same decode index as the decoded picture, it means the reference is base view picture
    ReferenceFrameList_t                ReferenceDepFrameList[MAX_REFERENCE_FRAME_LISTS];

    // Will be used by Codec to prepare the decode commands
    Buffer_t                    BasePreProcessorBuffer;
    Buffer_t                    DepPreProcessorBuffer;
} MVCFrameParameters_t;

#define BUFFER_MVC_FRAME_PARAMETERS        "MVCFrameParameters"
#define BUFFER_MVC_FRAME_PARAMETERS_TYPE   {BUFFER_MVC_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(MVCFrameParameters_t)}

#endif
