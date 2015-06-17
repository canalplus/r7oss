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

//#define DUMP_HEADERS
//#define DUMP_REFLISTS

#include "ring_generic.h"
#include "frame_parser_video_h264_mvc.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoH264_MVC_c"

static BufferDataDescriptor_t     MVCStreamParametersDescriptor = BUFFER_MVC_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     MVCFrameParametersDescriptor = BUFFER_MVC_FRAME_PARAMETERS_TYPE;

#define BUFFER_SUBSET_SEQUENCE_PARAMETER_SET        "MVCSubsetSequenceParameterSet"
#define BUFFER_SUBSET_SEQUENCE_PARAMETER_SET_TYPE    {BUFFER_SUBSET_SEQUENCE_PARAMETER_SET, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(MVCSubSequenceParameterSetHeader_t)}

static BufferDataDescriptor_t     SubsetSequenceParameterSetDescriptor = BUFFER_SUBSET_SEQUENCE_PARAMETER_SET_TYPE;

#define u(n)                    Bits.Get(n)
#define ue(v)                   Bits.GetUe()
#define se(v)                   Bits.GetSe()

#define Log2Ceil(v)             (32 - __lzcntw(v))

#define Assert(L)        if( !(L) ) {                    \
    SE_WARNING("Check failed at line %d\n", __LINE__ );  \
    Stream->MarkUnPlayable();              \
    return FrameParserStreamUnplayable;  }

#define AssertAntiEmulationOk()                                \
    if( TestAntiEmulationBuffer() != FrameParserNoError ) {    \
        SE_WARNING( "Anti Emulation Test fail\n");            \
        Stream->MarkUnPlayable();                \
        return FrameParserStreamUnplayable; }

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoH264_MVC_c::FrameParser_VideoH264_MVC_c(void)
    : FrameParser_VideoH264_c()
    , SubsetSequenceParameterSetType()
    , SubsetSequenceParameterSetPool(NULL)
    , SubsetSequenceParameterSetTable()
    , ReadNewSubsetSPS(false)
    , MVCStreamParameters(NULL)
    , MVCFrameParameters(NULL)
    , isAnchor(false)
    , isInterview(false)
    , ReferenceDepFrames()
    , NumReferenceDepFrames(0)
    , NumLongTermDep(0)
    , NumShortTermDep(0)
    , ReferenceDepFrameList()
    , ReferenceDepFrameListShortTerm()
    , ReferenceDepFrameListLongTerm()
{
    Configuration.FrameParserName               = "VideoMVC";
    Configuration.StreamParametersDescriptor    = &MVCStreamParametersDescriptor;
    Configuration.FrameParametersDescriptor     = &MVCFrameParametersDescriptor;
    Configuration.MaxReferenceFrameCount        = H264_MAX_REFERENCE_FRAMES;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoH264_MVC_c::~FrameParser_VideoH264_MVC_c(void)
{
    Halt();

    if (SubsetSequenceParameterSetPool != NULL)
    {
        for (int i = 0; i < H264_STANDARD_MAX_SEQUENCE_PARAMETER_SETS; i++)
            if (SubsetSequenceParameterSetTable[i].Buffer != NULL)
            {
                SubsetSequenceParameterSetTable[i].Buffer->DecrementReferenceCount();
            }

        BufferManager->DestroyPool(SubsetSequenceParameterSetPool);
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      The read headers stream specific function
//


FrameParserStatus_t   FrameParser_VideoH264_MVC_c::ReadSingleHeader(unsigned int Code, unsigned int UnitLength)
{
    FrameParserStatus_t     Status = FrameParserNoError;

    switch (nal_unit_type)
    {
    case NALU_TYPE_SUB_SPS:
        CheckAntiEmulationBuffer(UnitLength);
        Status = ReadNalSubSequenceParameterSet();
        ReadNewSubsetSPS    = true;
        break;

    case NALU_TYPE_PREFIX:
        CheckAntiEmulationBuffer(UnitLength);
        Status = ReadNalPrefix(Code, false);
        SE_INFO(group_frameparser_video,  "PREFIX unit type %d\n", nal_unit_type);
        break;

    case NALU_TYPE_IDR:
        SeenAnIDR        = true;
        BehaveAsIfSeenAnIDR = true;

    // fallthrough

    case NALU_TYPE_AUX_SLICE:
    case NALU_TYPE_SLICE:
        if (!BehaveAsIfSeenAnIDR)
        {
            int Policy  = Player->PolicyValue(Playback, Stream, PolicyH264AllowNonIDRResynchronization);

            if (Policy != PolicyValueApply)
            {
                SE_ERROR("Not seen an IDR\n");
                break;
            }
        }

        CheckAntiEmulationBuffer(DEFAULT_ANTI_EMULATION_REQUEST);
        Status = ReadNalSliceHeader(UnitLength); // base picture
        MVCFrameParameters = (MVCFrameParameters_t *)FrameParameters; // allocated buffer is big enough for MVC frame, starts with H264 frame

        if (Status == FrameParserNoError)
        {
            MVCFrameParameters->isInterview = isInterview;    // inter_view_flag
        }

        break;

    case NALU_TYPE_SLICE_EXT:
        CheckAntiEmulationBuffer(DEFAULT_ANTI_EMULATION_REQUEST);
        Status = ReadNalPrefix(Code, true); // First 3 bytes of the extension slice

        if (Status == FrameParserNoError)
        {
            Status = ReadNalExtSliceHeader(UnitLength);    // dependent picture slice header
        }

        if (Status == FrameParserNoError)
        {
            MVCFrameParameters->isAnchor = isAnchor;    // inter_view_flag
        }

        // Commit for decode if status is OK, and this is not the first slice
        // (IE we haven't already got the frame to decode flag, and its a first slice)
        if ((Status == FrameParserNoError) && !FrameToDecode)
        {
            MVCFrameParameters->DepDataOffset = ExtractStartCodeOffset(Code);
            Status                  = CommitFrameForDecode();
            SEIPictureTiming.Valid          = 0;        // any picture timing applied only to this decode
        }

        break;

    default:
        Status = FrameParser_VideoH264_c::ReadSingleHeader(Code, UnitLength);
        break;
    }

    return Status;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Connect Extension
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::Connect(Port_c *Port)
{
    FrameParserStatus_t     Status;
    BufferStatus_t          BufStatus;
    //
    // Pass the call on down (we need the frame parameters count obtained by the lower level function).
    //
    Status = FrameParser_VideoH264_c::Connect(Port);

    if (Status != FrameParserNoError)
    {
        goto compInError;
    }

    //
    // Attempt to register the type for subset sequence set
    //
    BufStatus      = BufferManager->FindBufferDataType(SubsetSequenceParameterSetDescriptor.TypeName, &SubsetSequenceParameterSetType);

    if (BufStatus != BufferNoError)
    {
        BufStatus  = BufferManager->CreateBufferDataType(&SubsetSequenceParameterSetDescriptor, &SubsetSequenceParameterSetType);

        if (BufStatus != BufferNoError)
        {
            SE_ERROR("Failed to create the subset sequence parameter set buffer type (status:%d)\n",
                     BufStatus);
            Status = FrameParserError;
            goto compInError;
        }
    }

    //
    // Create the pools of sequence and picture parameter sets
    //
    BufStatus = BufferManager->CreatePool(&SubsetSequenceParameterSetPool, SubsetSequenceParameterSetType, (H264_MAX_SEQUENCE_PARAMETER_SETS + H264_MAX_REFERENCE_FRAMES + 1));

    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to create a pool of subset sequence parameter sets (status:%d)\n",
                 BufStatus);
        Status = FrameParserError;
        goto compInError;
    }

    SE_INFO(group_frameparser_video,  "SUB_SPS Pool\n");
    return FrameParserNoError;
compInError:
//TODO Disconnect
    SetComponentState(ComponentInError);
    return Status;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Accessory function to compute if the profile is "high"
//      Called by FrameParser_VideoH264_c::ParseNalSequenceParameterSet() when parsing a subset SPS
//
bool FrameParser_VideoH264_MVC_c::isHighProfile(unsigned int profile_idc)
{
    return FrameParser_VideoH264_c::isHighProfile(profile_idc) || (profile_idc == MVC_PROFILE_IDC_STEREO_HIGH);
}


static unsigned int RoundLog2(int val)
{
    unsigned int ret = 0;
    int val_square = val * val;

    while ((1 << (ret + 1)) <= val_square)
    {
        ++ret;
    }

    ret = (ret + 1) >> 1;
    return ret;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//       Returns the size of the dpb depending on level and picture size
//
unsigned int FrameParser_VideoH264_MVC_c::ComputeDpbSize(MVCSubSequenceParameterSetHeader_t *header)
{
    unsigned int size = 0;
    H264SequenceParameterSetHeader_t *active_sps = &header->h264_header;
    unsigned int pic_size = (active_sps->pic_width_in_mbs_minus1 + 1) *
                            (active_sps->pic_height_in_map_units_minus1 + 1) * (active_sps->frame_mbs_only_flag ? 1 : 2) * 384;

    switch (active_sps->level_idc)
    {
    case 9:   size = 152064;   break;

    case 10:  size = 152064;   break;

    case 11:
        if (!(active_sps->profile_idc >= H264_PROFILE_IDC_HIGH || active_sps->profile_idc == H264_PROFILE_IDC_CAVLC444) &&
            (active_sps->constrained_set3_flag == 1))
        {
            size = 152064;
        }
        else
        {
            size = 345600;
        }

        break;

    case 12:  size = 912384;   break;

    case 13:  size = 912384;   break;

    case 20:  size = 912384;   break;

    case 21:  size = 1824768;  break;

    case 22:  size = 3110400;  break;

    case 30:  size = 3110400;  break;

    case 31:  size = 6912000;  break;

    case 32:  size = 7864320;  break;

    case 40:  size = 12582912; break;

    case 41:  size = 12582912; break;

    case 42:  size = 13369344; break;

    case 50:  size = 42393600; break;

    case 51:  size = 70778880; break;

    case 52:  size = 70778880; break;

    default:
        SE_ERROR("Undefined level\n");
        break;
    }

    if (size == 0 || pic_size == 0)
    {
        SE_ERROR("Undefined size: %d or pic_size: %d\n",
                 size, pic_size);
        return 0;
    }

    size /= pic_size;

    /*if(p_Vid->profile_idc == MVC_HIGH || p_Vid->profile_idc == STEREO_HIGH) */
    if (active_sps->profile_idc == MVC_PROFILE_IDC_STEREO_HIGH)
    {
        int num_views = header->num_views_minus1 + 1;
        size = min(2 * size, max(1, RoundLog2(num_views)) << 5) / num_views;
    }
    else
    {
        size = min(size, 16);
    }

    if (active_sps->vui_parameters_present_flag && active_sps->vui_seq_parameters.bitstream_restriction_flag)
    {
        unsigned int size_vui;

        if (active_sps->vui_seq_parameters.max_dec_frame_buffering > size)
        {
            SE_ERROR("max_dec_frame_buffering larger than MaxDpbSize\n");
        }

        size_vui = max(1, active_sps->vui_seq_parameters.max_dec_frame_buffering);

        if (size_vui < size)
        {
            SE_INFO(group_frameparser_video,  "Warning: max_dec_frame_buffering(%d) is less than DPB size(%d) calculated from Profile/Level\n",
                    size_vui, size);
        }

        size = size_vui;
    }

//SE_INFO(group_frameparser_video,  "Computed dpb size: %d\n", size);
    return size;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Read SEIs for MVC
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::ReadSeiPayload(
    unsigned int          PayloadType,
    unsigned int          PayloadSize)
{
    switch (PayloadType)
    {
    case SEI_MVC_SCALABLE_NESTING:
    case SEI_VIEW_SCALABILITY_INFO:
#ifdef DUMP_HEADERS
        SE_INFO(group_frameparser_video,  "Unhandled payload type (type = %d - size = %d)\n", PayloadType, PayloadSize);
#endif
        break;

    default:
        return FrameParser_VideoH264_c::ReadSeiPayload(PayloadType, PayloadSize);
        break;
    }

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a Suset sequence parameter set (7.3.2.1.3 of the specification)
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::ReadNalSubSequenceParameterSet(void)
{
    Buffer_t                            TmpBuffer;
    MVCSubSequenceParameterSetHeader_t  *Header;
    FrameParserStatus_t                 Status;
    unsigned int                        ViewIndex;
    unsigned int                        AnchorIndex;
    unsigned int                        bit_equal_to_1;

    //
    // Get a subset sequence parameter set
    //
    BufferStatus_t BufStatus = SubsetSequenceParameterSetPool->GetBuffer(&TmpBuffer);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to get a subset sequence parameter set buffer (status:%d)\n",
                 BufStatus);
        return FrameParserError;
    }

    TmpBuffer->ObtainDataReference(NULL, NULL, (void **)(&Header));
    if (Header == NULL)
    {
        TmpBuffer->DecrementReferenceCount();
        SE_ERROR("ObtainDataReference failed\n");
        return FrameParserError;
    }

    memset(Header, 0, sizeof(MVCSubSequenceParameterSetHeader_t));
    SetDefaultSubSequenceParameterSet(Header);
    Status = ParseNalSequenceParameterSet(&Header->h264_header); // common part between H264 & MVC

    if (Status != FrameParserNoError)
    {
        TmpBuffer->DecrementReferenceCount();
        Stream->MarkUnPlayable();
        return FrameParserError;
    }

    //
    // MVC-specific parsing
    //
    bit_equal_to_1 = u(1);

    if (bit_equal_to_1 != 1)
    {
        SE_ERROR("Assert failed (l.%d)\n", __LINE__);
        TmpBuffer->DecrementReferenceCount();
        Stream->MarkUnPlayable();
        return FrameParserStreamUnplayable;
    }

    Header->num_views_minus1 = ue(v);

    if (Header->num_views_minus1 != 1)
    {
        SE_ERROR("Assert failed (l.%d)\n", __LINE__);
        TmpBuffer->DecrementReferenceCount();
        Stream->MarkUnPlayable();
        return FrameParserStreamUnplayable;
    }

    for (ViewIndex = 0; ViewIndex < Header->num_views_minus1 + 1; ViewIndex++)
    {
        Header->view_id[ViewIndex] = ue(v);

        if (Header->view_id[ViewIndex] >= MVC_MAX_VIEW_ID)
        {
            SE_ERROR("Assert failed (l.%d)\n", __LINE__);
            TmpBuffer->DecrementReferenceCount();
            Stream->MarkUnPlayable();
            return FrameParserStreamUnplayable;
        }
    }

    for (ViewIndex = 1; ViewIndex < Header->num_views_minus1 + 1; ViewIndex++)
    {
        Header->num_anchor_refs_l0[ViewIndex] = ue(v);

        for (AnchorIndex = 0; AnchorIndex < Header->num_anchor_refs_l0[ViewIndex]; AnchorIndex++)
        {
            Header->anchor_ref_l0[ViewIndex][AnchorIndex] = ue(v);
        }

        Header->num_anchor_refs_l1[ViewIndex] = ue(v);

        for (AnchorIndex = 0; AnchorIndex < Header->num_anchor_refs_l1[ViewIndex]; AnchorIndex++)
        {
            Header->anchor_ref_l1[ViewIndex][AnchorIndex] = ue(v);
        }
    }

    for (ViewIndex = 1; ViewIndex < Header->num_views_minus1 + 1; ViewIndex++)
    {
        Header->num_non_anchor_refs_l0[ViewIndex] = ue(v);

        for (AnchorIndex = 0; AnchorIndex < Header->num_non_anchor_refs_l0[ViewIndex]; AnchorIndex++)
        {
            Header->non_anchor_ref_l0[ViewIndex][AnchorIndex] = ue(v);
        }

        Header->num_non_anchor_refs_l1[ViewIndex] = ue(v);

        for (AnchorIndex = 0; AnchorIndex < Header->num_non_anchor_refs_l1[ViewIndex]; AnchorIndex++)
        {
            Header->non_anchor_ref_l1[ViewIndex][AnchorIndex] = ue(v);
        }
    }

    Header->h264_header.max_dpb_size = ComputeDpbSize(Header);

    //
    // This header is complete, if we have one with the same ID then release it,
    // otherwise, just save this one.
    //

    if (SubsetSequenceParameterSetTable[Header->h264_header.seq_parameter_set_id].Buffer != NULL)
    {
        SubsetSequenceParameterSetTable[Header->h264_header.seq_parameter_set_id].Buffer->DecrementReferenceCount();
    }

    SubsetSequenceParameterSetTable[Header->h264_header.seq_parameter_set_id].Buffer        = TmpBuffer;
    SubsetSequenceParameterSetTable[Header->h264_header.seq_parameter_set_id].Header        = Header;
    AssertAntiEmulationOk();
    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Function to initialize an SUB SPS header into a default state
//


FrameParserStatus_t   FrameParser_VideoH264_MVC_c::SetDefaultSubSequenceParameterSet(MVCSubSequenceParameterSetHeader_t *Header)
{
    SetDefaultSequenceParameterSet(&Header->h264_header);
    Header->num_views_minus1 = 0;
    Header->view_id[0] = 0;
    Header->num_anchor_refs_l0[0] = 0;
    Header->num_anchor_refs_l1[0] = 0;
    Header->num_non_anchor_refs_l0[0] = 0;
    Header->num_non_anchor_refs_l1[0] = 0;
    return FrameParserNoError;
}

static void ZeroPrefix(unsigned char *BufferData, unsigned int Code)
{
    /*
    unsigned char* Prefix = BufferData + ExtractStartCodeOffset(Code);

    // Before: 0 0 1 <14> <P1> <P2> <P3>
    //  After: 0 0 0   0    0    0    0
    for (int i = 0; i < 7; i++)
        Prefix[i] = 0;
    */
}

static void PatchPrefixForSlice(unsigned char *BufferData, unsigned int Code, unsigned int nal_unit_type)
{
    /*
    unsigned char* Prefix = BufferData + ExtractStartCodeOffset(Code);
    unsigned int   StartCode = ExtractStartCodeCode(Code);

    // Before: 0   0   1  <~20>  <P1> <P2> <P3>  <Slice...>
    //  After: 0   0   0    0    0     1   <1/5> <Slice...>

    unsigned int newStartCode = (StartCode & 0xE0) | nal_unit_type;
    Prefix[0] = 0;
    Prefix[1] = 0;
    Prefix[2] = 0;
    Prefix[3] = 0;
    Prefix[4] = 0;
    Prefix[5] = 1;
    Prefix[6] = newStartCode;
    */
}


FrameParserStatus_t   FrameParser_VideoH264_MVC_c::ReadNalPrefix(unsigned int Code, bool isSlice)
{
    int value;
    value       = u(1);  // svc_extension_flag
    Assert(value == 0);  // 0 = MVC, 1 = SVC
    value       = u(1);  // non_idr_flag
    nal_unit_type = value ? NALU_TYPE_SLICE : NALU_TYPE_IDR;
    value       = u(6);  // priority_id
    value       = u(10); // view_id
    value       = u(3);  // temporal_id
    isAnchor    = u(1);  // anchor_pic_flag
    isInterview = u(1);  // inter_view_flag
    value       = u(1);  // reserved
    AssertAntiEmulationOk();

    // Patch for non-MVC aware preprocessors
    if (isSlice)
    {
        PatchPrefixForSlice(BufferData, Code, nal_unit_type);
    }
    else
    {
        ZeroPrefix(BufferData, Code);
    }

    return FrameParserNoError;
}


FrameParserStatus_t   FrameParser_VideoH264_MVC_c::ReadNalExtSliceHeader(unsigned int UnitLength)
{
    FrameParserStatus_t   Status;
    H264SliceHeader_t     *Header; // extended slice header
    unsigned int          first_mb_in_slice;
    unsigned int          slice_type;
    unsigned int sps_id, pps_id;

    // frame parameters should have been allocated upon parsing the base picture first slice
    if (FrameParameters == NULL || MVCFrameParameters == NULL)
    {
        return FrameParserError;
    }

    //
    // Check the first macroblock in slice field,
    // return if this is not the first slice in a picture.
    //
    first_mb_in_slice             = ue(v);
    slice_type                    = ue(v);

    if (first_mb_in_slice != 0)
    {
#ifdef DUMP_HEADERS
        SE_INFO(group_frameparser_video,  "Ext Slice Header :-\n");
        SE_INFO(group_frameparser_video,  " first_mb_in_slice                          = %6d\n", first_mb_in_slice);
        SE_INFO(group_frameparser_video,  " slice_type                                 = %6d\n", slice_type);
#endif
        // accumulate data for the preprocessing of the input buffer
        MVCFrameParameters->DepSlicesLength += UnitLength + 4; // Slice 20 payload + 4-byte startcode
        return FrameParserNoError;
    }

    Header = &MVCFrameParameters->DepSliceHeader;
    SetDefaultSliceHeader(Header);
    Header->nal_unit_type         = nal_unit_type;
    Header->nal_ref_idc           = nal_ref_idc;
    Header->first_mb_in_slice     = first_mb_in_slice;
    Header->slice_type            = slice_type;
    // initialize inout buffer length to the length of the first slice
    MVCFrameParameters->DepSlicesLength = UnitLength + 4; // Slice 20 payload + 4-byte startcode
    pps_id = ue(v);
    Assert((pps_id < H264_STANDARD_MAX_PICTURE_PARAMETER_SETS));
    Header->pic_parameter_set_id = pps_id;

    if (PictureParameterSetTable[pps_id].Buffer == NULL)
    {
        return FrameParserError;
    }

    Header->PictureParameterSet   = PictureParameterSetTable[pps_id].Header;
    sps_id = Header->PictureParameterSet->seq_parameter_set_id;
    Header->SequenceParameterSet = &(SubsetSequenceParameterSetTable[sps_id].Header->h264_header);

    if (Header->SequenceParameterSet == NULL)
    {
        SE_ERROR("PPS %d references unexistant subset SPS %d\n", pps_id, sps_id);
        return FrameParserError;
    }

    Header->SequenceParameterSetExtension = NULL; // SPS Ext is not managed for MVC ?? JLX
    Status = ParseNalSliceHeader(Header);
    return Status;
}

// /////////////////////////////////////////////////////////////////////////////
//
//    Private - function responsible for reading reference picture re-ordering data
//    Data are only read in order to be able to parse ther emaining of the slice header,
//    i.e. reordering data is thrown away
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::ReadRefPicListReordering(void)
{
    int i, flag, index;

    if (!SLICE_TYPE_IS(SliceHeader->slice_type, H264_SLICE_TYPE_I) && !SLICE_TYPE_IS(SliceHeader->slice_type, H264_SLICE_TYPE_SI))
    {
        flag = u(1); // flag L0

        if (flag)
        {
            CheckAntiEmulationBuffer(REF_LIST_REORDER_ANTI_EMULATION_REQUEST);

            for (i = 0; i < H264_MAX_REF_PIC_REORDER_OPERATIONS; i++)
            {
                index = ue(v);
                Assert(inrange(index, 0, 5));

                if (index == 3)
                {
                    break;
                }

                ue(v);
            }

            if (i >= H264_MAX_REF_PIC_REORDER_OPERATIONS)
            {
                SE_ERROR("Too many L0 list re-ordering commands - exceeds soft restriction (%d)\n", H264_MAX_REF_PIC_REORDER_OPERATIONS);
                return FrameParserError;
            }
        }
    }

    if (SLICE_TYPE_IS(SliceHeader->slice_type, H264_SLICE_TYPE_B))
    {
        flag = u(1);

        if (flag)
        {
            CheckAntiEmulationBuffer(REF_LIST_REORDER_ANTI_EMULATION_REQUEST);

            for (i = 0; i < H264_MAX_REF_PIC_REORDER_OPERATIONS; i++)
            {
                index = ue(v);
                Assert(inrange(index, 0, 5));

                if (index == 3)
                {
                    break;
                }

                ue(v);
            }

            if (i >= H264_MAX_REF_PIC_REORDER_OPERATIONS)
            {
                SE_ERROR("Too many L1 list re-ordering commands - exceeds soft restriction (%d)\n", H264_MAX_REF_PIC_REORDER_OPERATIONS);
                return FrameParserError;
            }
        }
    }

//
    AssertAntiEmulationOk();
//
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////////
//
//    Private - function responsible for decting limits of frames
//
FrameParserStatus_t   FrameParser_VideoH264_MVC_c::PresentCollatedHeader(
    unsigned char          StartCode,
    unsigned char         *HeaderBytes,
    FrameParserHeaderFlag_t     *Flags)
{
    FrameParserStatus_t Status;
    unsigned char    NalUnitType    = (StartCode & 0x1f);
    Status = FrameParser_VideoH264_c::PresentCollatedHeader(StartCode, HeaderBytes, Flags);

    if (Status == FrameParserNoError && NalUnitType == NALU_TYPE_PREFIX)
    {
        *Flags    |= FrameParserHeaderFlagPartitionPoint;
    }

    return Status;
}
// /////////////////////////////////////////////////////////////////////////
//
//      The read headers stream specific function
//
FrameParserStatus_t   FrameParser_VideoH264_MVC_c::ReadHeaders(void)
{
    isInterview = true; // default value when no prefix nal unit
    return FrameParser_VideoH264_c::ReadHeaders();
}
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Decoded reference picture marking process" in the specification
//
FrameParserStatus_t   FrameParser_VideoH264_MVC_c::PrepareReferenceFrameList(void)
{
    unsigned int        i, j;
    FrameParserStatus_t        Status;
    // bool            ApplyTwoRefTestForBframes;
    // Calculate the reference picture lists for the base view picture
    Status = FrameParser_VideoH264_c::PrepareReferenceFrameList();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // Calculate the reference picture lists for the dependent view picture
    //
    SliceHeader        = &((MVCFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->DepSliceHeader;
    NumReferenceDepFrames    = SliceHeader->SequenceParameterSet->num_ref_frames;

    //
    // Perform the calculation of the lists
    //

    if (!SliceHeader->SequenceParameterSet->frame_mbs_only_flag && SliceHeader->field_pic_flag)
    {
        Status    = CalculateReferencePictureListsField();
    }
    else
    {
        Status    = CalculateReferencePictureListsFrame();
    }

    //
    // Translate the indices in the list from ReferenceFrames indices to
    // decode frame indices as used throughout the rest of the player.
    //

    for (i = 0; i < H264_NUM_REF_FRAME_LISTS; i++)
        for (j = 0; j < ReferenceDepFrameList[i].EntryCount; j++)
        {
            ReferenceDepFrameList[i].EntryIndicies[j] = ReferenceDepFrames[ReferenceDepFrameList[i].EntryIndicies[j]].DecodeFrameIndex;
        }

    //
    // Adds interview reference to the dependent ref frame list
    //
    SliceHeader = &((MVCFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SliceHeader;

    if (MVCFrameParameters->isInterview)
        for (i = 0; i < H264_NUM_REF_FRAME_LISTS; i++)
        {
            j = ReferenceDepFrameList[i].EntryCount++;
            ReferenceDepFrameList[i].ReferenceDetails[j].PictureNumber    = SliceHeader->frame_num;
            ReferenceDepFrameList[i].ReferenceDetails[j].PicOrderCnt    = SliceHeader->PicOrderCnt;
            ReferenceDepFrameList[i].ReferenceDetails[j].PicOrderCntTop    = SliceHeader->PicOrderCntTop;
            ReferenceDepFrameList[i].ReferenceDetails[j].PicOrderCntBot    = SliceHeader->PicOrderCntBot;
            ReferenceDepFrameList[i].ReferenceDetails[j].UsageCode        = REF_PIC_USE_FRAME;
            ReferenceDepFrameList[i].EntryIndicies[j]                        = ParsedFrameParameters->DecodeFrameIndex;
        }

    // Dump lists
#ifdef DUMP_LISTS

    for (i = 0; i < H264_NUM_REF_FRAME_LISTS; i++)
    {
        SE_DEBUG(group_frameparser_video, " dep ref pic list %d (DFI/fn/poc): ", i);

        for (j = 0; j < ReferenceDepFrameList[i].EntryCount; j++)
        {
            SE_DEBUG(group_frameparser_video, "%d/%d/%d, ", ReferenceDepFrameList[i].EntryIndicies[j], ReferenceDepFrameList[i].ReferenceDetails[j].PictureNumber,
                     ReferenceDepFrameList[i].ReferenceDetails[j].PicOrderCnt);
        }

        SE_DEBUG(group_frameparser_video, "\n");
    }

#endif
    //
    // Copy the calculated reference picture list into the MVC frame parameters
    //
    memcpy(&((MVCFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->ReferenceDepFrameList, ReferenceDepFrameList, H264_NUM_REF_FRAME_LISTS * sizeof(ReferenceFrameList_t));
    //
    return Status;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Decoded reference picture marking process" in the specification
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::CalculateReferencePictureListsFrame(void)
{
    unsigned int    i, j;
    unsigned int    MaxFrameNum;
    int             FrameNumWrap;
    //
    // First we calculate Picture numbers for each reference frame
    // These can be re-used in the marking process.
    //
    MaxFrameNum = 1 << (SliceHeader->SequenceParameterSet->log2_max_frame_num_minus4 + 4);

    for (i = 0; i < NumReferenceDepFrames + 1; i++)
        if (ReferenceDepFrames[i].Usage != NotUsedForReference)
        {
            FrameNumWrap        = (ReferenceDepFrames[i].FrameNum > SliceHeader->frame_num) ?
                                  ReferenceDepFrames[i].FrameNum - MaxFrameNum :
                                  ReferenceDepFrames[i].FrameNum;
            ReferenceDepFrames[i].FrameNumWrap   = FrameNumWrap;
            ReferenceDepFrames[i].PicNum         = FrameNumWrap;
            ReferenceDepFrames[i].LongTermPicNum = ReferenceDepFrames[i].LongTermFrameIdx;
        }

    //
    // Now create the lists
    //
    ReferenceDepFrameList[P_REF_PIC_LIST].EntryCount      = 0;
    ReferenceDepFrameList[B_REF_PIC_LIST_0].EntryCount    = 0;
    ReferenceDepFrameList[B_REF_PIC_LIST_1].EntryCount    = 0;
    InitializePSliceReferencePictureListFrame();
    InitializeBSliceReferencePictureListsFrame();

    //
    // Fill in the data fields
    //

    for (i = 0; i < H264_NUM_REF_FRAME_LISTS; i++)
        for (j = 0; j < ReferenceDepFrameList[i].EntryCount; j++)
        {
            unsigned int    I        = ReferenceDepFrameList[i].EntryIndicies[j];
            bool        LongTerm    = ReferenceDepFrameList[i].ReferenceDetails[j].LongTermReference;
            ReferenceDepFrameList[i].ReferenceDetails[j].PictureNumber    = LongTerm ? ReferenceDepFrames[I].LongTermPicNum : ReferenceDepFrames[I].PicNum;
            ReferenceDepFrameList[i].ReferenceDetails[j].PicOrderCnt    = ReferenceDepFrames[I].PicOrderCnt;
            ReferenceDepFrameList[i].ReferenceDetails[j].PicOrderCntTop    = ReferenceDepFrames[I].PicOrderCntTop;
            ReferenceDepFrameList[i].ReferenceDetails[j].PicOrderCntBot    = ReferenceDepFrames[I].PicOrderCntBot;
            ReferenceDepFrameList[i].ReferenceDetails[j].UsageCode        = REF_PIC_USE_FRAME;
        }

#ifdef REFERENCE_PICTURE_LIST_PROCESSING

    if ((SliceHeader->ref_pic_list_reordering.ref_pic_list_reordering_flag_l0 != 0) ||
        (SliceHeader->ref_pic_list_reordering.ref_pic_list_reordering_flag_l1 != 0))
    {
        SE_ERROR("This implementation does not support reference picture list re-ordering\n");
    }

#endif
//
    return FrameParserNoError;
}
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - calculate reference picture lists for field decodes
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::CalculateReferencePictureListsField(void)
{
    unsigned int    i, j;
    unsigned int    MaxFrameNum;
    int             FrameNumWrap;
    //
    // First we calculate Picture numbers for each reference frame
    // These can be re-used in the marking process.
    //
    MaxFrameNum         = 1 << (SliceHeader->SequenceParameterSet->log2_max_frame_num_minus4 + 4);

    for (i = 0; i < (NumReferenceDepFrames + 1); i++)
        if (ReferenceDepFrames[i].Usage != NotUsedForReference)
        {
            FrameNumWrap        = (ReferenceDepFrames[i].FrameNum > SliceHeader->frame_num) ?
                                  ReferenceDepFrames[i].FrameNum - MaxFrameNum :
                                  ReferenceDepFrames[i].FrameNum;
            ReferenceDepFrames[i].FrameNumWrap    = FrameNumWrap;
            ReferenceDepFrames[i].PicNum          = (2 * FrameNumWrap);
            ReferenceDepFrames[i].LongTermPicNum  = (2 * ReferenceDepFrames[i].LongTermFrameIdx);
        }

    //
    // Now create the lists
    //
    ReferenceDepFrameList[P_REF_PIC_LIST].EntryCount      = 0;
    ReferenceDepFrameList[B_REF_PIC_LIST_0].EntryCount    = 0;
    ReferenceDepFrameList[B_REF_PIC_LIST_1].EntryCount    = 0;
    InitializePSliceReferencePictureListField();
    InitializeBSliceReferencePictureListsField();

    //
    // Fill in the data fields
    //

    for (i = 0; i < H264_NUM_REF_FRAME_LISTS; i++)
        for (j = 0; j < ReferenceDepFrameList[i].EntryCount; j++)
        {
            unsigned int    I        = ReferenceDepFrameList[i].EntryIndicies[j];
            bool        LongTerm    = ReferenceDepFrameList[i].ReferenceDetails[j].LongTermReference;
            bool        Top        = (ReferenceDepFrameList[i].ReferenceDetails[j].UsageCode == REF_PIC_USE_FIELD_TOP);
            unsigned int    ParityOffset    = (Top == (SliceHeader->bottom_field_flag == 0)) ? 1 : 0;
            ReferenceDepFrameList[i].ReferenceDetails[j].PictureNumber        = (LongTerm ? ReferenceDepFrames[I].LongTermPicNum : ReferenceDepFrames[I].PicNum) + ParityOffset;
            ReferenceDepFrameList[i].ReferenceDetails[j].PicOrderCnt        = Top ? ReferenceDepFrames[I].PicOrderCntTop : ReferenceDepFrames[I].PicOrderCntBot;
            ReferenceDepFrameList[i].ReferenceDetails[j].PicOrderCntTop    = ReferenceDepFrames[I].PicOrderCntTop;
            ReferenceDepFrameList[i].ReferenceDetails[j].PicOrderCntBot    = ReferenceDepFrames[I].PicOrderCntBot;
        }

#ifdef REFERENCE_PICTURE_LIST_PROCESSING

    if ((SliceHeader->ref_pic_list_reordering.ref_pic_list_reordering_flag_l0 != 0) ||
        (SliceHeader->ref_pic_list_reordering.ref_pic_list_reordering_flag_l1 != 0))
    {
        SE_ERROR("This implementation does not support reference picture list re-ordering\n");
    }

#endif
    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Initialization process for the reference picture list for P and SP slices in frames"
//

#define BUBBLE_UP( L, F, O, N )                                                 \
    if( (N) > 1 )                                                           \
    {                                                                       \
        unsigned int        i,j;                                            \
                                        \
        for( i=1; i<(N); i++ )                                              \
        for( j=0; j<((N)-i); j++ )                                      \
            if( ReferenceDepFrames[L[(O)+j]].F > ReferenceDepFrames[L[(O)+j+1]].F )   \
            {                                                           \
            unsigned int    Tmp;                                    \
                                        \
            Tmp             = L[(O)+j];                             \
            L[(O)+j]        = L[(O)+j+1];                           \
            L[(O)+j+1]      = Tmp;                                  \
            }                                                           \
    }

#define BUBBLE_DOWN( L, F, O, N )                                                       \
    if( (N) > 1 )                                                           \
    {                                                                       \
        unsigned int        i,j;                                            \
                                        \
        for( i=1; i<(N); i++ )                                              \
        for( j=0; j<((N)-i); j++ )                                      \
            if( ReferenceDepFrames[L[(O)+j]].F < ReferenceDepFrames[L[(O)+j+1]].F )   \
            {                                                           \
            unsigned int    Tmp;                                    \
                                        \
            Tmp             = L[(O)+j];                             \
            L[(O)+j]        = L[(O)+j+1];                           \
            L[(O)+j+1]      = Tmp;                                  \
            }                                                           \
    }


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Initialization process for reference picture lists in fields" in the specification
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::InitializeReferencePictureListField(
    ReferenceFrameList_t    *ShortTermList,
    ReferenceFrameList_t    *LongTermList,
    unsigned int         MaxListEntries,
    ReferenceFrameList_t    *List)
{
    unsigned int    SameParityIndex;
    unsigned int    AlternateParityIndex;
    unsigned int    SameParityUsedForShortTermReference;
    unsigned int    AlternateParityUsedForShortTermReference;
    unsigned int    SameParityUsedForLongTermReference;
    unsigned int    AlternateParityUsedForLongTermReference;
    unsigned int    SameParityUse;
    unsigned int    AlternateParityUse;

    //
    // Calculate useful values
    //

    if (!SliceHeader->bottom_field_flag)
    {
        SameParityUsedForShortTermReference             = UsedForTopShortTermReference;
        AlternateParityUsedForShortTermReference        = UsedForBotShortTermReference;
        SameParityUsedForLongTermReference              = UsedForTopLongTermReference;
        AlternateParityUsedForLongTermReference         = UsedForBotLongTermReference;
        SameParityUse                                   = REF_PIC_USE_FIELD_TOP;
        AlternateParityUse                              = REF_PIC_USE_FIELD_BOT;
    }
    else
    {
        SameParityUsedForShortTermReference             = UsedForBotShortTermReference;
        AlternateParityUsedForShortTermReference        = UsedForTopShortTermReference;
        SameParityUsedForLongTermReference              = UsedForBotLongTermReference;
        AlternateParityUsedForLongTermReference         = UsedForTopLongTermReference;
        SameParityUse                                   = REF_PIC_USE_FIELD_BOT;
        AlternateParityUse                              = REF_PIC_USE_FIELD_TOP;
    }

    //
    // Insert the entries for the short term list
    //
    List->EntryCount    = 0;

    for (SameParityIndex = 0,           AlternateParityIndex = 0;
         (SameParityIndex < ShortTermList->EntryCount) || (AlternateParityIndex < ShortTermList->EntryCount);
         SameParityIndex++,             AlternateParityIndex++)
    {
        //
        // Append next reference field of same parity in list, if any exist
        //
        while ((SameParityIndex < ShortTermList->EntryCount) &&
               ((ReferenceDepFrames[ShortTermList->EntryIndicies[SameParityIndex]].Usage & SameParityUsedForShortTermReference) == 0))
        {
            SameParityIndex++;
        }

        if (SameParityIndex < ShortTermList->EntryCount)
        {
            List->EntryIndicies[List->EntryCount]                          = ShortTermList->EntryIndicies[SameParityIndex];
            List->ReferenceDetails[List->EntryCount].LongTermReference = false;
            List->ReferenceDetails[List->EntryCount++].UsageCode       = SameParityUse;
        }

        //
        // Append next reference field of alternate parity in list, if any exist
        //

        while ((AlternateParityIndex < ShortTermList->EntryCount) &&
               ((ReferenceDepFrames[ShortTermList->EntryIndicies[AlternateParityIndex]].Usage & AlternateParityUsedForShortTermReference) == 0))
        {
            AlternateParityIndex++;
        }

        if (AlternateParityIndex < ShortTermList->EntryCount)
        {
            List->EntryIndicies[List->EntryCount]                          = ShortTermList->EntryIndicies[AlternateParityIndex];
            List->ReferenceDetails[List->EntryCount].LongTermReference = false;
            List->ReferenceDetails[List->EntryCount++].UsageCode       = AlternateParityUse;
        }
    }

    //
    // Insert the entries for the long term list
    //

    for (SameParityIndex = 0,           AlternateParityIndex = 0;
         (SameParityIndex < LongTermList->EntryCount) || (AlternateParityIndex < LongTermList->EntryCount);
         SameParityIndex++,             AlternateParityIndex++)
    {
        //
        // Append next reference field of same parity in list, if any exist
        //
        while ((SameParityIndex < LongTermList->EntryCount) &&
               ((ReferenceDepFrames[LongTermList->EntryIndicies[SameParityIndex]].Usage & SameParityUsedForLongTermReference) == 0))
        {
            SameParityIndex++;
        }

        if (SameParityIndex < LongTermList->EntryCount)
        {
            List->EntryIndicies[List->EntryCount]                          = LongTermList->EntryIndicies[SameParityIndex];
            List->ReferenceDetails[List->EntryCount].LongTermReference = true;
            List->ReferenceDetails[List->EntryCount++].UsageCode       = SameParityUse;
        }

        //
        // Append next reference field of alternate parity in list, if any exist
        //

        while ((AlternateParityIndex < LongTermList->EntryCount) &&
               ((ReferenceDepFrames[LongTermList->EntryIndicies[AlternateParityIndex]].Usage & AlternateParityUsedForLongTermReference) == 0))
        {
            AlternateParityIndex++;
        }

        if (AlternateParityIndex < LongTermList->EntryCount)
        {
            List->EntryIndicies[List->EntryCount]                          = LongTermList->EntryIndicies[AlternateParityIndex];
            List->ReferenceDetails[List->EntryCount].LongTermReference = true;
            List->ReferenceDetails[List->EntryCount++].UsageCode       = AlternateParityUse;
        }
    }

    //
    // Limit the list
    //

    if (List->EntryCount > MaxListEntries)
    {
        List->EntryCount  = MaxListEntries;
    }

//
    return FrameParserNoError;
}
FrameParserStatus_t   FrameParser_VideoH264_MVC_c::InitializePSliceReferencePictureListFrame(void)
{
    unsigned int            i;
    unsigned int            Count;
    unsigned int            NumActiveReferences;
    ReferenceFrameList_t    *List;
    //
    // Calculate the limit on active indices
    //
#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences         = SliceHeader->PictureParameterSet->num_ref_idx_l0_active_minus1 + 1;

    if (SliceHeader->num_ref_idx_active_override_flag)
    {
        NumActiveReferences     = SliceHeader->num_ref_idx_l0_active_minus1 + 1;
    }

#else
    NumActiveReferences     = NumReferenceDepFrames;
#endif
    //
    // Obtain an descending list of PicNums (this is, no doubt,
    // not the fastest ordering method but given the size of
    // the lists (4 ish) it is probably not a problem).
    //
    List    = &ReferenceDepFrameList[P_REF_PIC_LIST];

    if (NumShortTermDep != 0)
    {
        Count   = 0;

        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if (ComplimentaryReferencePair(ReferenceDepFrames[i].Usage) &&
                ((ReferenceDepFrames[i].Usage & AnyUsedForShortTermReference) != 0))
            {
                List->EntryIndicies[Count]                = i;
                List->ReferenceDetails[Count].LongTermReference    = false;
                Count++;
            }

        BUBBLE_DOWN(List->EntryIndicies, PicNum, 0, Count);
        List->EntryCount    = Count;
    }

    //
    // Now add in the long term indices
    //

    if (NumLongTermDep != 0)
    {
        Count   = 0;

        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if (ComplimentaryReferencePair(ReferenceDepFrames[i].Usage) &&
                ((ReferenceDepFrames[i].Usage & AnyUsedForLongTermReference) != 0))
            {
                List->EntryIndicies[List->EntryCount + Count]                = i;
                List->ReferenceDetails[List->EntryCount + Count].LongTermReference    = true;
                Count++;
            }

        BUBBLE_UP(List->EntryIndicies, LongTermPicNum, List->EntryCount, Count);
        List->EntryCount    = min(List->EntryCount + Count, NumActiveReferences);
    }

//
    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Initialization process for the reference picture list for P and SP slices in fields"
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::InitializePSliceReferencePictureListField(void)
{
    unsigned int    NumActiveReferences;
    unsigned int    Count;
    unsigned int    i;
    //
    // Calculate the limit on active indices
    //
#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences         = SliceHeader->PictureParameterSet->num_ref_idx_l0_active_minus1 + 1;

    if (SliceHeader->num_ref_idx_active_override_flag)
    {
        NumActiveReferences         = SliceHeader->num_ref_idx_l0_active_minus1 + 1;
    }

#else
    NumActiveReferences         = 2 * NumReferenceDepFrames + 1;
#endif
    //
    // Obtain a descending list of PicNums (this is, no doubt,
    // not the fastest ordering method but given the size of
    // the lists (4 ish) it is probably not a problem).
    //
    ReferenceFrameListShortTerm[0].EntryCount    = 0;
    ReferenceFrameListLongTerm.EntryCount        = 0;

    if (NumShortTerm != 0)
    {
        Count   = 0;

        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if ((ReferenceDepFrames[i].Usage & AnyUsedForShortTermReference) != 0)
            {
                ReferenceFrameListShortTerm[0].EntryIndicies[Count++]     = i;
            }

        BUBBLE_DOWN(ReferenceFrameListShortTerm[0].EntryIndicies, FrameNumWrap, 0, Count);
        ReferenceFrameListShortTerm[0].EntryCount = Count;
    }

    //
    // Now add in the long term indices
    //

    if (NumLongTerm != 0)
    {
        Count   = 0;

        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if ((ReferenceDepFrames[i].Usage & AnyUsedForLongTermReference) != 0)
            {
                ReferenceFrameListLongTerm.EntryIndicies[Count++]   = i;
            }

        BUBBLE_UP(ReferenceFrameListLongTerm.EntryIndicies, LongTermPicNum, 0, Count);
        ReferenceFrameListLongTerm.EntryCount = Count;
    }

    //
    // Finally process these to generate ReferenceFrameList 0
    //
    InitializeReferencePictureListField(&ReferenceFrameListShortTerm[0],
                                        &ReferenceFrameListLongTerm,
                                        NumActiveReferences,
                                        &ReferenceDepFrameList[P_REF_PIC_LIST]);
//
    return FrameParserNoError;
}
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Initialization process for the reference picture list for B slices in frames"
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::InitializeBSliceReferencePictureListsFrame(void)
{
    unsigned int         i;
    unsigned int         Count;
    unsigned int         LowerEntries;
    unsigned int         NumActiveReferences0;
    unsigned int         NumActiveReferences1;
    ReferenceFrameList_t    *List0;
    ReferenceFrameList_t    *List1;
    //
    // Calculate the limit on active indices
    //
#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences0        = SliceHeader->PictureParameterSet->num_ref_idx_l0_active_minus1 + 1;

    if (SliceHeader->num_ref_idx_active_override_flag)
    {
        NumActiveReferences0        = SliceHeader->num_ref_idx_l0_active_minus1 + 1;
    }

#else
    NumActiveReferences0        = NumReferenceDepFrames;
#endif
//
#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences1        = SliceHeader->PictureParameterSet->num_ref_idx_l1_active_minus1 + 1;

    if (SliceHeader->num_ref_idx_active_override_flag)
    {
        NumActiveReferences1        = SliceHeader->num_ref_idx_l1_active_minus1 + 1;
    }

#else
    NumActiveReferences1        = NumReferenceDepFrames;
#endif
    //
    // First we construct list 0, and derive list 1 from it
    //
    // Obtain descending list of short term reference
    // frames with lower picnum than the current frame,
    // and the ascending list of frames with higher
    // picnum than the current frame.
    List0    = &ReferenceDepFrameList[B_REF_PIC_LIST_0];
    List1    = &ReferenceDepFrameList[B_REF_PIC_LIST_1];

    if (NumShortTermDep != 0)
    {
        Count   = 0;

        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if (ComplimentaryReferencePair(ReferenceDepFrames[i].Usage) &&
                ((ReferenceDepFrames[i].Usage & AnyUsedForShortTermReference) != 0) &&
                (ReferenceDepFrames[i].PicOrderCnt  <= SliceHeader->PicOrderCnt))
            {
                List0->EntryIndicies[Count]                = i;
                List0->ReferenceDetails[Count].LongTermReference    = false;
                Count++;
            }

        BUBBLE_DOWN(List0->EntryIndicies, PicOrderCnt, 0, Count);
        LowerEntries     = Count;
        Count            = 0;

        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if (ComplimentaryReferencePair(ReferenceDepFrames[i].Usage) &&
                ((ReferenceDepFrames[i].Usage & AnyUsedForShortTermReference) != 0) &&
                (ReferenceDepFrames[i].PicOrderCnt  > SliceHeader->PicOrderCnt))
            {
                List0->EntryIndicies[LowerEntries + Count]                = i;
                List0->ReferenceDetails[LowerEntries + Count].LongTermReference    = false;
                Count++;
            }

        BUBBLE_UP(List0->EntryIndicies, PicOrderCnt, LowerEntries, Count);
        List0->EntryCount = LowerEntries + Count;

        //
        // Now copy this list (in two sepearate portions),
        // into reference list 1.
        //

        for (i = 0; i < Count; i++)
        {
            List1->EntryIndicies[i]             = List0->EntryIndicies[LowerEntries + i];
            List1->ReferenceDetails[i].LongTermReference = false;
        }

        for (i = 0; i < LowerEntries; i++)
        {
            List1->EntryIndicies[Count + i]    = List0->EntryIndicies[i];
            List1->ReferenceDetails[Count + i].LongTermReference = false;
        }
    }

    //
    // Now append the long term references to list 0, and copy them to list 1
    //

    if (NumLongTerm != 0)
    {
        Count   = 0;

        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if (ComplimentaryReferencePair(ReferenceDepFrames[i].Usage) &&
                ((ReferenceDepFrames[i].Usage & AnyUsedForLongTermReference) != 0))
            {
                List0->EntryIndicies[List0->EntryCount + Count]                          = i;
                List0->ReferenceDetails[List0->EntryCount + Count].LongTermReference = true;
                Count++;
            }

        BUBBLE_UP(List0->EntryIndicies, LongTermPicNum, List0->EntryCount, Count);

        for (i = 0; i < Count; i++)
        {
            int index = List0->EntryCount + i;
            List1->EntryIndicies[index] = List0->EntryIndicies[index];
            List1->ReferenceDetails[index].LongTermReference = true;
        }

        List0->EntryCount += Count;
    }

    //
    // Now limit the length of both lists as specified by the active flags
    //
    List1->EntryCount    = min(List0->EntryCount, NumActiveReferences1);
    List0->EntryCount    = min(List0->EntryCount, NumActiveReferences0);

    //
    // Finally, if the lists are identical, and have more than
    // 1 entry, then we swap the first two entries of list 1.
    //

    if ((List1->EntryCount > 1) &&
        (List1->EntryCount == List0->EntryCount) &&
        (memcmp(List0->EntryIndicies, List1->EntryIndicies, List1->EntryCount * sizeof(unsigned int)) == 0))
    {
        unsigned int    Tmp;
        unsigned int    TmpBool;
        Tmp            = List1->EntryIndicies[0];
        List1->EntryIndicies[0]    = List1->EntryIndicies[1];
        List1->EntryIndicies[1]    = Tmp;
        TmpBool                            = List1->ReferenceDetails[0].LongTermReference;
        List1->ReferenceDetails[0].LongTermReference    = List1->ReferenceDetails[1].LongTermReference;
        List1->ReferenceDetails[1].LongTermReference    = TmpBool;
    }

//
    return FrameParserNoError;
}
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Initialization process for the reference picture list for B slices in fields"
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::InitializeBSliceReferencePictureListsField(void)
{
    unsigned int    i;
    unsigned int    Count;
    unsigned int    LowerEntries;
    unsigned int    NumActiveReferences0;
    unsigned int    NumActiveReferences1;
    //
    // Calculate the limit on active indices
    //
#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences0        = SliceHeader->PictureParameterSet->num_ref_idx_l0_active_minus1 + 1;

    if (SliceHeader->num_ref_idx_active_override_flag)
    {
        NumActiveReferences0        = SliceHeader->num_ref_idx_l0_active_minus1 + 1;
    }

#else
    NumActiveReferences0        = 2 * NumReferenceDepFrames + 1;
#endif
//
#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences1        = SliceHeader->PictureParameterSet->num_ref_idx_l1_active_minus1 + 1;

    if (SliceHeader->num_ref_idx_active_override_flag)
    {
        NumActiveReferences1        = SliceHeader->num_ref_idx_l1_active_minus1 + 1;
    }

#else
    NumActiveReferences1        = 2 * NumReferenceDepFrames + 1;
#endif
    //
    // First we construct list 0, and derive list 1 from it
    //
    // Obtain descending list of short term reference
    // frames with lower picnum than the current frame,
    // and the ascending list of frames with higher
    // picnum than the current frame.
    ReferenceFrameListShortTerm[0].EntryCount    = 0;
    ReferenceFrameListShortTerm[1].EntryCount    = 0;
    ReferenceFrameListLongTerm.EntryCount        = 0;
    LowerEntries             = 0;
    Count                    = 0;

    if (NumShortTerm != 0)
    {
        Count   = 0;

        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if (((ReferenceDepFrames[i].Usage & AnyUsedForShortTermReference) != 0) &&
                (ReferenceDepFrames[i].PicOrderCnt <= SliceHeader->PicOrderCnt))
            {
                ReferenceFrameListShortTerm[0].EntryIndicies[Count++] = i;
            }

        BUBBLE_DOWN(ReferenceFrameListShortTerm[0].EntryIndicies, PicOrderCnt, 0, Count);
        LowerEntries            = Count;
        Count                   = 0;

        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if (((ReferenceDepFrames[i].Usage & AnyUsedForShortTermReference) != 0) &&
                (ReferenceDepFrames[i].PicOrderCnt  > SliceHeader->PicOrderCnt))
            {
                ReferenceFrameListShortTerm[0].EntryIndicies[LowerEntries + Count++]  = i;
            }

        BUBBLE_UP(ReferenceFrameListShortTerm[0].EntryIndicies, PicOrderCnt, LowerEntries, Count);
        ReferenceFrameListShortTerm[0].EntryCount = LowerEntries + Count;

        //
        // Now copy this list (in two separate portions),
        // into reference list 1.
        //

        for (i = 0; i < Count; i++)
        {
            ReferenceFrameListShortTerm[1].EntryIndicies[i]        =  ReferenceFrameListShortTerm[0].EntryIndicies[LowerEntries + i];
        }

        for (i = 0; i < LowerEntries; i++)
        {
            ReferenceFrameListShortTerm[1].EntryIndicies[Count + i]    =  ReferenceFrameListShortTerm[0].EntryIndicies[i];
        }

        ReferenceFrameListShortTerm[1].EntryCount             = ReferenceFrameListShortTerm[0].EntryCount;
    }

    //
    // Now generate the long term references
    //

    if (NumLongTerm != 0)
    {
        Count   = 0;

        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if ((ReferenceDepFrames[i].Usage & AnyUsedForLongTermReference) != 0)
            {
                ReferenceFrameListLongTerm.EntryIndicies[Count++]   = i;
            }

        BUBBLE_UP(ReferenceFrameListLongTerm.EntryIndicies, LongTermPicNum, 0, Count);
        ReferenceFrameListLongTerm.EntryCount = Count;
    }

    //
    // Dump the lists for posterity
    //
#ifdef DUMP_REFLISTS
    SE_INFO(group_frameparser_video,  "Reference Dep frame lists (%s) PicOrderCnt = %d :-\n",
            (!SliceHeader->bottom_field_flag ? "Top field" : "Bottom field"), SliceHeader->PicOrderCnt);
    SE_INFO(group_frameparser_video,  "ReferenceFrameListShortTerm[0].EntryCount - %d entries (Lower %d Upper %d):-\n", ReferenceFrameListShortTerm[0].EntryCount, LowerEntries, Count);

    for (i = 0; i < ReferenceFrameListShortTerm[0].EntryCount; i++)
    {
        SE_INFO(group_frameparser_video,  " DecodeIndex %5d, FrameNumber %4d PicOrderCnt %4d (%4d)\n",
                ReferenceFrames[ReferenceFrameListShortTerm[0].EntryIndicies[i]].DecodeFrameIndex,
                ReferenceFrames[ReferenceFrameListShortTerm[0].EntryIndicies[i]].FrameNum,
                ReferenceFrames[ReferenceFrameListShortTerm[0].EntryIndicies[i]].PicOrderCntTop, ReferenceFrames[ReferenceFrameListShortTerm[0].EntryIndicies[i]].PicOrderCntBot);
    }

    SE_INFO(group_frameparser_video,  "ReferenceFrameListShortTerm[1].EntryCount - %d entries (Lower %d Upper %d:-\n", ReferenceFrameListShortTerm[1].EntryCount, LowerEntries, Count);

    for (i = 0; i < ReferenceFrameListShortTerm[1].EntryCount; i++)
    {
        SE_INFO(group_frameparser_video,  " DecodeIndex %5d, FrameNumber %4d PicOrderCnt %4d (%4d)\n",
                ReferenceFrames[ReferenceFrameListShortTerm[1].EntryIndicies[i]].DecodeFrameIndex,
                ReferenceFrames[ReferenceFrameListShortTerm[1].EntryIndicies[i]].FrameNum,
                ReferenceFrames[ReferenceFrameListShortTerm[1].EntryIndicies[i]].PicOrderCntTop, ReferenceFrames[ReferenceFrameListShortTerm[1].EntryIndicies[i]].PicOrderCntBot);
    }

    SE_INFO(group_frameparser_video,  "ReferenceFrameListLongTerm.EntryCount - %d entries (Count %d):-\n", ReferenceFrameListLongTerm.EntryCount, Count);

    for (i = 0; i < ReferenceFrameListLongTerm.EntryCount; i++)
    {
        SE_INFO(group_frameparser_video,  " DecodeIndex %5d, FrameNumber %4d PicOrderCnt %4d (%4d)\n",
                ReferenceFrames[ReferenceFrameListLongTerm.EntryIndicies[i]].DecodeFrameIndex,
                ReferenceFrames[ReferenceFrameListLongTerm.EntryIndicies[i]].FrameNum,
                ReferenceFrames[ReferenceFrameListLongTerm.EntryIndicies[i]].PicOrderCntTop, ReferenceFrames[ReferenceFrameListLongTerm.EntryIndicies[i]].PicOrderCntBot);
    }

#endif
    //
    // Process these to generate RefPicLists
    //
    InitializeReferencePictureListField(&ReferenceFrameListShortTerm[0],
                                        &ReferenceFrameListLongTerm,
                                        NumActiveReferences0,
                                        &ReferenceDepFrameList[B_REF_PIC_LIST_0]);
    InitializeReferencePictureListField(&ReferenceFrameListShortTerm[1],
                                        &ReferenceFrameListLongTerm,
                                        NumActiveReferences1,
                                        &ReferenceDepFrameList[B_REF_PIC_LIST_1]);

    //
    // Finally, if the lists are identical, and have more than
    // 1 entry, then we swap the first two entries of list 1.
    //

    if ((ReferenceDepFrameList[B_REF_PIC_LIST_1].EntryCount > 1) &&
        (ReferenceDepFrameList[B_REF_PIC_LIST_1].EntryCount == ReferenceDepFrameList[B_REF_PIC_LIST_0].EntryCount) &&
        (memcmp(ReferenceDepFrameList[B_REF_PIC_LIST_0].EntryIndicies, ReferenceDepFrameList[B_REF_PIC_LIST_1].EntryIndicies, ReferenceDepFrameList[B_REF_PIC_LIST_1].EntryCount * sizeof(unsigned int)) == 0))
    {
        unsigned int    Tmp;
        unsigned int    TmpBool;
        Tmp                                                      = ReferenceDepFrameList[B_REF_PIC_LIST_1].EntryIndicies[0];
        ReferenceDepFrameList[B_REF_PIC_LIST_1].EntryIndicies[0] = ReferenceDepFrameList[B_REF_PIC_LIST_1].EntryIndicies[1];
        ReferenceDepFrameList[B_REF_PIC_LIST_1].EntryIndicies[1] = Tmp;
        Tmp                                                                        = ReferenceDepFrameList[B_REF_PIC_LIST_1].ReferenceDetails[0].UsageCode;
        ReferenceDepFrameList[B_REF_PIC_LIST_1].ReferenceDetails[0].UsageCode  = ReferenceDepFrameList[B_REF_PIC_LIST_1].ReferenceDetails[1].UsageCode;
        ReferenceDepFrameList[B_REF_PIC_LIST_1].ReferenceDetails[1].UsageCode  = Tmp;
        TmpBool                                                                            = ReferenceDepFrameList[B_REF_PIC_LIST_1].ReferenceDetails[0].LongTermReference;
        ReferenceDepFrameList[B_REF_PIC_LIST_1].ReferenceDetails[0].LongTermReference  = ReferenceDepFrameList[B_REF_PIC_LIST_1].ReferenceDetails[1].LongTermReference;
        ReferenceDepFrameList[B_REF_PIC_LIST_1].ReferenceDetails[1].LongTermReference  = TmpBool;
    }

    return FrameParserNoError;
}

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::CalculatePicOrderCnts(void)
{
    FrameParserStatus_t Status = FrameParser_VideoH264_c::CalculatePicOrderCnts();
    // copy POCs into dependent slice header
    H264SliceHeader_t *DepHeader    = &MVCFrameParameters->DepSliceHeader;
    DepHeader->PicOrderCntTop       = SliceHeader->PicOrderCntTop;
    DepHeader->PicOrderCntBot       = SliceHeader->PicOrderCntBot;
    DepHeader->PicOrderCnt          = SliceHeader->PicOrderCnt;
    DepHeader->ExtendedPicOrderCnt  = SliceHeader->ExtendedPicOrderCnt;
    return Status;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to manage a reference frame list in forward play
//      we only record a reference frame as such on the last field, in order to
//      ensure the correct management of reference frames in the codec, we immediately
//      inform the codec of a release on the first field of a field picture.
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::ForPlayUpdateReferenceFrameList(void)
{
    // Call ancestor for base view references
    FrameParserStatus_t Status = FrameParser_VideoH264_c::ForPlayUpdateReferenceFrameList();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // Update the reference frame list of the dependent view frames
    //
    // Set SliceHeader to the dependent view slice header
    SliceHeader = &MVCFrameParameters->DepSliceHeader;

    if (SliceHeader->nal_ref_idc != 0)  // reference frame
    {
        if (NumReferenceDepFrames > SliceHeader->SequenceParameterSet->num_ref_frames)
        {
            SE_ERROR("Detected NumReferenceDepFrames change %d->%d\n - reseting reference frame list\n",
                     NumReferenceDepFrames, SliceHeader->SequenceParameterSet->num_ref_frames);
            ResetReferenceFrameList();
        }

        NumReferenceDepFrames    = SliceHeader->SequenceParameterSet->num_ref_frames;
        return MarkReferenceDepPictures(true);
    }

//
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The reset reference frame list function
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::ResetReferenceFrameList(void)
{
    FrameParserStatus_t Status = FrameParser_VideoH264_c::ResetReferenceFrameList();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    memset(&ReferenceDepFrames, 0, sizeof(H264ReferenceFrameData_t)  * (H264_MAX_REFERENCE_FRAMES + 1));
    NumReferenceDepFrames                          = 0;
    NumLongTermDep                                 = 0;
    NumShortTermDep                                = 0;
    ReferenceDepFrameList[0].EntryCount            = 0;
    ReferenceDepFrameList[1].EntryCount            = 0;
    ReferenceDepFrameListShortTerm[0].EntryCount   = 0;
    ReferenceDepFrameListShortTerm[1].EntryCount   = 0;
    ReferenceDepFrameListLongTerm.EntryCount       = 0;
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Decoded reference picture marking process" in the specification
//

static SliceType_t SliceTypeTranslation[]  = { SliceTypeP, SliceTypeB, SliceTypeI, SliceTypeP, SliceTypeI, SliceTypeP, SliceTypeB, SliceTypeI, SliceTypeP, SliceTypeI };

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::MarkReferenceDepPictures(bool    ActuallyReleaseReferenceFrames)
{
    unsigned int             i, j, k;
    unsigned int             Entry;
    unsigned int             CurrentEntry;
    unsigned int             FrameNum;
    unsigned int             MaxFrameNum;
    int                      FrameNumWrap;
    int                      LowestFrameNumWrap;
    H264DecRefPicMarking_t  *MMC;
    bool                     Top;
    bool                     Field;
    bool                     Idr;
    bool                     BaseIdr;
    bool                     Iframe;
    bool                     LongTerm;
    bool                     ProcessControl;
    int                      PicNumX;
    bool                     SecondFieldEntry;
    //
    unsigned int             UsedForShortTermReference;
    unsigned int             OtherFieldUsedForShortTermReference;
    unsigned int             UsedForLongTermReference;
    unsigned int             OtherFieldUsedForLongTermReference;
    unsigned int             FieldUsed;
    unsigned int             OtherFieldUsed;
    unsigned int             ReleaseCode;
    unsigned int             LongTermIndexAssignedTo;
    //
    // Derive useful flags
    //
    Field          = (SliceHeader->field_pic_flag != 0);
    Top            = Field && (SliceHeader->bottom_field_flag == 0);
    BaseIdr        = (MVCFrameParameters->SliceHeader.nal_unit_type == NALU_TYPE_IDR); // used to release all dep frames in DPB when base view is IDR
    Idr            = (SliceHeader->nal_unit_type == NALU_TYPE_IDR); // dep view frame is an IDR (not allowed by standard ?)
    Iframe         = SliceTypeTranslation[SliceHeader->slice_type] == SliceTypeI;
    LongTerm       = (SliceHeader->dec_ref_pic_marking.long_term_reference_flag != 0);
    MMC            = &SliceHeader->dec_ref_pic_marking;
    ProcessControl = MMC->adaptive_ref_pic_marking_mode_flag;
    MaxFrameNum    = 1 << (SliceHeader->SequenceParameterSet->log2_max_frame_num_minus4 + 4);
    FrameNum       = SliceHeader->frame_num;

    //
    // Setup appropriate masking values
    //

    if (Field)
    {
        if (Top)
        {
            UsedForShortTermReference           = UsedForTopShortTermReference;
            OtherFieldUsedForShortTermReference = UsedForBotShortTermReference;
            UsedForLongTermReference            = UsedForTopLongTermReference;
            OtherFieldUsedForLongTermReference  = UsedForBotLongTermReference;
        }
        else
        {
            UsedForShortTermReference           = UsedForBotShortTermReference;
            OtherFieldUsedForShortTermReference = UsedForTopShortTermReference;
            UsedForLongTermReference            = UsedForBotLongTermReference;
            OtherFieldUsedForLongTermReference  = UsedForTopLongTermReference;
        }

        FieldUsed                               = UsedForShortTermReference             | UsedForLongTermReference;
        OtherFieldUsed                          = OtherFieldUsedForShortTermReference   | OtherFieldUsedForLongTermReference;
    }
    else
    {
        UsedForShortTermReference               = UsedForTopShortTermReference | UsedForBotShortTermReference;
        UsedForLongTermReference                = UsedForTopLongTermReference  | UsedForBotLongTermReference;
        OtherFieldUsedForShortTermReference     = 0;
        OtherFieldUsedForLongTermReference      = 0;
    }

    //
    // Is this a field of a pre-existing frame
    //
    CurrentEntry        = 0;
    SecondFieldEntry    = false;

    if (Field)
    {
        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if ((ReferenceDepFrames[i].Usage != NotUsedForReference) &&
                ReferenceDepFrames[i].Field &&
                (ReferenceDepFrames[i].FrameNum == FrameNum) &&
                ((ReferenceDepFrames[i].DecodeFrameIndex + 1) == ParsedFrameParameters->DecodeFrameIndex))
            {
                if (((ReferenceDepFrames[i].Usage & FieldUsed)      != 0) ||
                    ((ReferenceDepFrames[i].Usage & OtherFieldUsed) == 0))
                {
                    SE_ERROR("Duplicate reference field, or invalid reference list entry\n");
                    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, MVC_ENCODE_DEP_FRAME_INDEX(ParsedFrameParameters->DecodeFrameIndex));
                    return FrameParserStreamSyntaxError;
                }

                if (Top)
                {
                    ReferenceDepFrames[i].PicOrderCntTop             = SliceHeader->PicOrderCntTop;
                }
                else
                {
                    ReferenceDepFrames[i].PicOrderCntBot             = SliceHeader->PicOrderCntBot;
                }

                ReferenceDepFrames[i].PicOrderCnt                    = min(ReferenceDepFrames[i].PicOrderCntTop, ReferenceDepFrames[i].PicOrderCntBot);

                if (LongTerm)
                {
                    ReferenceDepFrames[i].Usage                     |= UsedForLongTermReference;
                    ReferenceDepFrames[i].LongTermFrameIdx           = FrameNum;

                    if ((ReferenceDepFrames[i].Usage & OtherFieldUsedForLongTermReference) == 0)
                    {
                        NumLongTermDep++;
                    }
                }
                else
                {
                    ReferenceDepFrames[i].Usage                     |= UsedForShortTermReference;
                    ReferenceDepFrames[i].FrameNum                   = FrameNum;

                    if ((ReferenceDepFrames[i].Usage & OtherFieldUsedForShortTermReference) == 0)
                    {
                        NumShortTermDep++;
                    }
                }

                // Release immediately, since the buffer is held by the first field
                if (ActuallyReleaseReferenceFrames)
                {
                    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, MVC_ENCODE_DEP_FRAME_INDEX(ParsedFrameParameters->DecodeFrameIndex));
                }

                CurrentEntry        = i;
                SecondFieldEntry    = true;
            }
    }

    //
    // If we have a non-pre-existing IDR release everyone
    //

    if (!SecondFieldEntry && BaseIdr)
    {
        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if (ReferenceDepFrames[i].Usage != NotUsedForReference)
            {
                if (ActuallyReleaseReferenceFrames)
                {
                    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, MVC_ENCODE_DEP_FRAME_INDEX(ReferenceDepFrames[i].DecodeFrameIndex));
                }

                ReferenceDepFrames[i].Usage = NotUsedForReference;
            }

        NumLongTermDep     = 0;
        NumShortTermDep    = 0;
    }

    //
    // Insert the current frame
    //

    if (!SecondFieldEntry)
    {
        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if (ReferenceDepFrames[i].Usage == NotUsedForReference)
            {
                CurrentEntry                                  = i;
                ReferenceDepFrames[i].Field                   = Field;
                ReferenceDepFrames[i].ParsedFrameParameters   = ParsedFrameParameters;
                ReferenceDepFrames[i].DecodeFrameIndex        = ParsedFrameParameters->DecodeFrameIndex;
                ReferenceDepFrames[i].PicOrderCnt             = SliceHeader->PicOrderCnt;
                ReferenceDepFrames[i].PicOrderCntTop          = SliceHeader->PicOrderCntTop;
                ReferenceDepFrames[i].PicOrderCntBot          = SliceHeader->PicOrderCntBot;
                ReferenceDepFrames[i].ExtendedPicOrderCnt     = SliceHeader->ExtendedPicOrderCnt;
                ReferenceDepFrames[i].FrameNum                = FrameNum;

                if (LongTerm)
                {
                    ReferenceDepFrames[i].Usage               = UsedForLongTermReference;
                    ReferenceDepFrames[i].LongTermFrameIdx    = FrameNum;
                    NumLongTermDep++;

                    if (Idr)
                    {
                        MaxLongTermFrameIdx                   = 0;
                    }
                }
                else
                {
                    ReferenceDepFrames[i].Usage               = UsedForShortTermReference;
                    NumShortTermDep++;

                    if (Idr)
                    {
                        MaxLongTermFrameIdx                   = NO_LONG_TERM_FRAME_INDICES;
                    }
                }

                break;
            }

        if (i == (NumReferenceDepFrames + 1))
        {
            SE_ERROR("No place for frame - Implementation error\n");
            ResetReferenceFrameList();
            return FrameParserError;
        }
    }

    //
    // Now apply the appropriate list management algorithms
    //

    if (!Idr)
    {
        if (!ProcessControl)
        {
            //
            // Follow "Sliding window decoded reference picture marking process"
            //
            if ((NumLongTermDep + NumShortTermDep) > NumReferenceDepFrames)
            {
                LowestFrameNumWrap      = 0x7fffffff;
                Entry                   = 0;

                for (i = 0; i < (NumReferenceDepFrames + 1); i++)
                    if ((i != CurrentEntry) &&
                        ((ReferenceDepFrames[i].Usage & AnyUsedForShortTermReference) != 0))
                    {
                        FrameNumWrap    = (ReferenceDepFrames[i].FrameNum > FrameNum) ?
                                          ReferenceDepFrames[i].FrameNum - MaxFrameNum :
                                          ReferenceDepFrames[i].FrameNum;

                        if (FrameNumWrap < LowestFrameNumWrap)
                        {
                            LowestFrameNumWrap  = FrameNumWrap;
                            Entry               = i;
                        }
                    }

                ReleaseDepReference(ActuallyReleaseReferenceFrames, Entry, AnyUsedForShortTermReference);

                if (NumShortTermDep > 100) { SE_ERROR("NumShortTermDep references has gone negative\n"); }

                if (NumLongTermDep > 100) { SE_ERROR("NumLongTermDep references has gone negative\n"); }
            }
        }
        else
        {
            //
            // Follow "Adaptive memory control decoded reference picture marking process"
            //
            // First re-calculate picnum and long term pic num
            // NOTE forward play this may already be calculated, but in reverse it isn't.
            //
            MaxFrameNum         = 1 << (SliceHeader->SequenceParameterSet->log2_max_frame_num_minus4 + 4);

            for (i = 0; i < (NumReferenceDepFrames + 1); i++)
                if (ReferenceDepFrames[i].Usage != NotUsedForReference)
                {
                    FrameNumWrap                         = (ReferenceDepFrames[i].FrameNum > SliceHeader->frame_num) ?
                                                           ReferenceDepFrames[i].FrameNum - MaxFrameNum :
                                                           ReferenceDepFrames[i].FrameNum;
                    ReferenceDepFrames[i].PicNum         = Field ? (2 * FrameNumWrap) : FrameNumWrap;
                    ReferenceDepFrames[i].LongTermPicNum = Field ? (2 * ReferenceDepFrames[i].LongTermFrameIdx) : ReferenceDepFrames[i].LongTermFrameIdx;
                }

#ifdef DUMP_REFLISTS
            SE_INFO(group_frameparser_video,  "List of reference frames before MMCO (%d %d):-\n", NumShortTermDep, NumLongTermDep);

            for (i = 0; i < (NumReferenceDepFrames + 1); i++)
                if (ReferenceDepFrames[i].Usage != NotUsedForReference)
                    SE_INFO(group_frameparser_video,  " DFI %5d, F/F %s, Usage %c%c, FrameNumber %4d (%4d), LongTermIndex %4d\n",
                            ReferenceDepFrames[i].DecodeFrameIndex,
                            (ReferenceDepFrames[i].Field ? "Field" : "Frame"),
                            (((ReferenceDepFrames[i].Usage & UsedForTopLongTermReference) != 0) ? 'L' : (((ReferenceDepFrames[i].Usage & UsedForTopShortTermReference) != 0) ? 'S' : 'x')),
                            (((ReferenceDepFrames[i].Usage & UsedForBotLongTermReference) != 0) ? 'L' : (((ReferenceDepFrames[i].Usage & UsedForBotShortTermReference) != 0) ? 'S' : 'x')),
                            ReferenceDepFrames[i].FrameNum, ReferenceDepFrames[i].PicNum,
                            ((ReferenceDepFrames[i].Usage & AnyUsedForLongTermReference) != 0) ? ReferenceDepFrames[i].LongTermFrameIdx : 9999);

#endif

            for (i = 0; ; i++)
            {
                if (MMC->memory_management_control_operation[i] == H264_MMC_END)
                {
                    break;
                }

#ifdef DUMP_REFLISTS
                SE_INFO(group_frameparser_video,  "MMCO(%d %d %d) - %d - %3d %3d %3d %3d\n", Field, Top, FrameNum,
                        MMC->memory_management_control_operation[i],
                        MMC->difference_of_pic_nums_minus1[i],
                        MMC->long_term_pic_num[i],
                        MMC->long_term_frame_idx[i],
                        MMC->max_long_term_frame_idx_plus1[i]);
#endif
#define FRAME_MMCO    0
#define FIELD_MMCO    128

                switch (MMC->memory_management_control_operation[i] + (Field ? FIELD_MMCO : FRAME_MMCO))
                {
                case H264_MMC_MARK_SHORT_TERM_UNUSED_FOR_REFERENCE + FRAME_MMCO:
                    PicNumX = FrameNum - (MMC->difference_of_pic_nums_minus1[i] + 1);

                    for (j = 0; j < (NumReferenceDepFrames + 1); j++)
                        if (((ReferenceDepFrames[j].Usage & AnyUsedForShortTermReference) != 0) &&
                            (ReferenceDepFrames[j].PicNum == PicNumX))
                        {
                            ReleaseDepReference(ActuallyReleaseReferenceFrames, j, AnyUsedForShortTermReference);
                            break;
                        }

                    break;

                case H264_MMC_MARK_SHORT_TERM_UNUSED_FOR_REFERENCE + FIELD_MMCO:
                    PicNumX = 2 * FrameNum + 1 - (MMC->difference_of_pic_nums_minus1[i] + 1);

                    for (j = 0; j < (NumReferenceDepFrames + 1); j++)
                        if (((ReferenceDepFrames[j].Usage & AnyUsedForShortTermReference) != 0) &&
                            ((ReferenceDepFrames[j].PicNum == PicNumX) || ((ReferenceDepFrames[j].PicNum + 1) == PicNumX)))
                        {
                            ReleaseCode    = (ReferenceDepFrames[j].PicNum == PicNumX) ?
                                             OtherFieldUsedForShortTermReference :
                                             UsedForShortTermReference;
                            ReleaseDepReference(ActuallyReleaseReferenceFrames, j, ReleaseCode);
                            break;
                        }

                    break;

                case H264_MMC_MARK_LONG_TERM_UNUSED_FOR_REFERENCE  + FRAME_MMCO:
                    for (j = 0; j < (NumReferenceDepFrames + 1); j++)
                        if (((ReferenceDepFrames[j].Usage & AnyUsedForLongTermReference) != 0) &&
                            (ReferenceDepFrames[j].LongTermPicNum == MMC->long_term_pic_num[i]))
                        {
                            ReleaseDepReference(ActuallyReleaseReferenceFrames, j, AnyUsedForLongTermReference);
                            break;
                        }

                    break;

                case H264_MMC_MARK_LONG_TERM_UNUSED_FOR_REFERENCE  + FIELD_MMCO:
                    for (j = 0; j < (NumReferenceDepFrames + 1); j++)
                        if (((ReferenceDepFrames[j].Usage & AnyUsedForLongTermReference) != 0) &&
                            ((ReferenceDepFrames[j].LongTermPicNum == MMC->long_term_pic_num[i]) ||
                             ((ReferenceDepFrames[j].LongTermPicNum + 1) == MMC->long_term_pic_num[i])))
                        {
                            ReleaseCode    = (ReferenceDepFrames[j].LongTermPicNum == MMC->long_term_pic_num[i]) ?
                                             OtherFieldUsedForLongTermReference :
                                             UsedForLongTermReference;
                            ReleaseDepReference(ActuallyReleaseReferenceFrames, j, ReleaseCode);
                            break;
                        }

                    break;

                case H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_SHORT_TERM_PICTURE + FRAME_MMCO:

                    // Release any frame already using the index
                    for (j = 0; j < (NumReferenceDepFrames + 1); j++)
                        if (((ReferenceDepFrames[j].Usage & AnyUsedForLongTermReference) != 0) &&
                            (ReferenceDepFrames[j].LongTermFrameIdx == MMC->long_term_frame_idx[i]))
                        {
                            ReleaseDepReference(ActuallyReleaseReferenceFrames, j, AnyUsedForLongTermReference);
                            break;
                        }

                    // Now assign the index
                    PicNumX = FrameNum - (MMC->difference_of_pic_nums_minus1[i] + 1);

                    for (j = 0; j < (NumReferenceDepFrames + 1); j++)
                        if (((ReferenceDepFrames[j].Usage & AnyUsedForShortTermReference) != 0) &&
                            (ReferenceDepFrames[j].PicNum == PicNumX))
                        {
                            ReferenceDepFrames[j].LongTermFrameIdx      = MMC->long_term_frame_idx[i];
                            ReferenceDepFrames[j].Usage                ^= UsedForShortTermReference;
                            ReferenceDepFrames[j].Usage                |= UsedForLongTermReference;
                            NumShortTermDep--;
                            NumLongTermDep++;
                            break;
                        }

                    break;

                case H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_SHORT_TERM_PICTURE + FIELD_MMCO:
                    // Here we release after the assignment, because we cannot release any complimentary field
                    // Now assign the index
                    PicNumX         = 2 * FrameNum + 1 - (MMC->difference_of_pic_nums_minus1[i] + 1);
                    LongTermIndexAssignedTo    = INVALID_INDEX;

                    for (j = 0; j < (NumReferenceDepFrames + 1); j++)
                        if (((ReferenceDepFrames[j].Usage & AnyUsedForShortTermReference) != 0) &&
                            ((ReferenceDepFrames[j].PicNum == PicNumX) || ((ReferenceDepFrames[j].PicNum + 1) == PicNumX)))
                        {
                            LongTermIndexAssignedTo                 = j;
                            ReferenceDepFrames[j].LongTermFrameIdx  = MMC->long_term_frame_idx[i];

                            if (ReferenceDepFrames[j].PicNum == PicNumX)
                            {
                                ReferenceDepFrames[j].Usage        ^= OtherFieldUsedForShortTermReference;
                                ReferenceDepFrames[j].Usage        |= OtherFieldUsedForLongTermReference;
                            }
                            else
                            {
                                ReferenceDepFrames[j].Usage        ^= UsedForShortTermReference;
                                ReferenceDepFrames[j].Usage        |= UsedForLongTermReference;
                            }

                            if ((ReferenceDepFrames[j].Usage & AnyUsedForShortTermReference) == 0)
                            {
                                NumShortTermDep--;    // Now no short terms
                            }

                            if ((ReferenceDepFrames[j].Usage & AnyUsedForLongTermReference) != AnyUsedForLongTermReference)
                            {
                                NumLongTermDep++;    // Not gone to both fields long term
                            }

                            break;
                        }

                    // Release any frame already using the index
                    for (j = 0; j < (NumReferenceDepFrames + 1); j++)
                        if (((ReferenceDepFrames[j].Usage & AnyUsedForLongTermReference) != 0) &&
                            (ReferenceDepFrames[j].LongTermFrameIdx == MMC->long_term_frame_idx[i]) &&
                            (j != LongTermIndexAssignedTo))
                        {
                            ReleaseDepReference(ActuallyReleaseReferenceFrames, j, AnyUsedForLongTermReference);
                            break;
                        }

                    break;

//

                case H264_MMC_SPECIFY_MAX_LONG_TERM_INDEX + FRAME_MMCO:
                case H264_MMC_SPECIFY_MAX_LONG_TERM_INDEX + FIELD_MMCO:

                    // Release any frames greater than the max index
                    for (j = 0; j < (NumReferenceDepFrames + 1); j++)
                        if (((ReferenceDepFrames[j].Usage & AnyUsedForLongTermReference) != 0) &&
                            ((MMC->max_long_term_frame_idx_plus1[i] == 0) || (ReferenceDepFrames[j].LongTermFrameIdx > (MMC->max_long_term_frame_idx_plus1[i] - 1))))
                        {
                            ReleaseDepReference(ActuallyReleaseReferenceFrames, j, AnyUsedForLongTermReference);
                        }

                    MaxLongTermFrameIdx     = MMC->max_long_term_frame_idx_plus1[i] ?
                                              (MMC->max_long_term_frame_idx_plus1[i] - 1) :
                                              NO_LONG_TERM_FRAME_INDICES;
                    break;

//

                case H264_MMC_CLEAR + FRAME_MMCO:
                case H264_MMC_CLEAR + FIELD_MMCO:
                    for (j = 0; j < (NumReferenceDepFrames + 1); j++)
                        if (j != CurrentEntry)
                        {
                            ReleaseDepReference(ActuallyReleaseReferenceFrames, j, AnyUsedForReference);
                        }

                    //
                    // Post process current picture on a clear.
                    //
                    ReferenceDepFrames[CurrentEntry].PicOrderCnt     = 0;
                    ReferenceDepFrames[CurrentEntry].PicOrderCntTop -= SliceHeader->PicOrderCnt;
                    ReferenceDepFrames[CurrentEntry].PicOrderCntBot -= SliceHeader->PicOrderCnt;
                    ReferenceDepFrames[CurrentEntry].FrameNum        = 0;
                    AccumulatedFrameNumber                           = 0;
                    PrevFrameNum                                     = 0;

                    NumLongTermDep                                   = LongTerm ? 1 : 0;
                    NumShortTermDep                                  = LongTerm ? 0 : 1;
                    MaxLongTermFrameIdx                              = NO_LONG_TERM_FRAME_INDICES;
                    break;

                case H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_CURRENT_DECODED_PICTURE + FRAME_MMCO:
                case H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_CURRENT_DECODED_PICTURE + FIELD_MMCO:

                    // Release any frame already using the index
                    for (j = 0; j < (NumReferenceDepFrames + 1); j++)
                        if (((ReferenceDepFrames[j].Usage & AnyUsedForLongTermReference) != 0)      &&
                            (ReferenceDepFrames[j].LongTermFrameIdx == MMC->long_term_frame_idx[i]) &&
                            (CurrentEntry != j))
                        {
                            ReleaseDepReference(ActuallyReleaseReferenceFrames, j, AnyUsedForLongTermReference);
                            break;
                        }

                    // Now assign the index
                    ReferenceDepFrames[CurrentEntry].LongTermFrameIdx   = MMC->long_term_frame_idx[i];
                    ReferenceDepFrames[CurrentEntry].Usage             &= ~UsedForShortTermReference;
                    ReferenceDepFrames[CurrentEntry].Usage             |= UsedForLongTermReference;

                    if (!LongTerm)
                    {
                        if ((ReferenceDepFrames[CurrentEntry].Usage & AnyUsedForShortTermReference) == 0)
                        {
                            NumShortTermDep--;    // Now no short terms
                        }

                        if (!Field || ((ReferenceDepFrames[CurrentEntry].Usage & AnyUsedForLongTermReference) != AnyUsedForLongTermReference))
                        {
                            NumLongTermDep++;    // Not gone to both fields long term
                        }
                    }

                    break;
                }

                // re-calculate picnum and long term pic num as MMCO operations may change the references inserted in DPB
                for (k = 0; k < (NumReferenceDepFrames + 1); k++)
                    if (ReferenceDepFrames[k].Usage != NotUsedForReference)
                    {
                        FrameNumWrap                         = (ReferenceDepFrames[k].FrameNum > SliceHeader->frame_num) ?
                                                               ReferenceDepFrames[k].FrameNum - MaxFrameNum :
                                                               ReferenceDepFrames[k].FrameNum;
                        ReferenceDepFrames[k].PicNum         = Field ? (2 * FrameNumWrap) : FrameNumWrap;
                        ReferenceDepFrames[k].LongTermPicNum = Field ?
                                                               (2 * ReferenceDepFrames[k].LongTermFrameIdx) : ReferenceDepFrames[k].LongTermFrameIdx;
                    }
            }
        }

        //
        // Perform error recovery management of the reference frame lists.
        // We do somthing here when we think the list has become stale, or overflows its limits
        //
        // First expire old reference frames in a broadcast environment (No IDR, Using MMCO).
        // Second attack list overflow.
        //

        if (!SecondFieldEntry && ProcessControl && !SeenAnIDR && Iframe)
        {
            for (i = 0; i < (NumReferenceDepFrames + 1); i++)
                if ((ReferenceDepFrames[i].Usage != NotUsedForReference) &&
                    (ReferenceDepFrames[i].DecodeFrameIndex < LastReferenceIframeDecodeIndex))
                {
                    ReleaseDepReference(true, i, AnyUsedForReference);
                }

            LastReferenceIframeDecodeIndex    = ReferenceDepFrames[CurrentEntry].DecodeFrameIndex;
        }

        if ((NumShortTermDep + NumLongTermDep) > (NumReferenceDepFrames + 1))
        {
            SE_ERROR("there are %d+%d ref frames, allowed is %d - reseting list of reference fraems\n",
                     NumShortTermDep, NumLongTermDep, NumReferenceDepFrames);
            ResetReferenceFrameList();
        }
        else if ((NumShortTermDep + NumLongTermDep) > NumReferenceDepFrames)
        {
            SE_ERROR("After MMCO operations, \nthere are more than the allowed number of reference frames\nThe oldest will be discarded\n");

            for (i = 0, j = 1; j < (NumReferenceDepFrames + 1); j++)
                if ((ReferenceDepFrames[i].Usage == NotUsedForReference) ||
                    (ReferenceDepFrames[i].DecodeFrameIndex > ReferenceDepFrames[j].DecodeFrameIndex))
                {
                    i = j;
                }

            ReleaseDepReference(true, i, AnyUsedForReference);
        }

#ifdef DUMP_REFLISTS
        SE_INFO(group_frameparser_video,  "\nList of reference frames After MMCO (%d %d):-\n", NumShortTermDep, NumLongTermDep);

        for (i = 0; i < (NumReferenceDepFrames + 1); i++)
            if (ReferenceDepFrames[i].Usage != NotUsedForReference)
                SE_INFO(group_frameparser_video,  " DFI %5d, F/F %s, Usage %c%c, FrameNumber %4d, LongTermIndex %4d\n",
                        ReferenceDepFrames[i].DecodeFrameIndex,
                        (ReferenceDepFrames[i].Field ? "Field" : "Frame"),
                        (((ReferenceDepFrames[i].Usage & UsedForTopLongTermReference) != 0) ?
                         'L' : (((ReferenceDepFrames[i].Usage & UsedForTopShortTermReference) != 0) ? 'S' : 'x')),
                        (((ReferenceDepFrames[i].Usage & UsedForBotLongTermReference) != 0) ?
                         'L' : (((ReferenceDepFrames[i].Usage & UsedForBotShortTermReference) != 0) ? 'S' : 'x')),
                        ReferenceDepFrames[i].FrameNum,
                        ((ReferenceDepFrames[i].Usage & AnyUsedForLongTermReference) != 0) ? ReferenceDepFrames[i].LongTermFrameIdx : 9999);

#endif
    }

#ifdef DUMP_REFLISTS
    SE_INFO(group_frameparser_video, "\nList of dependent reference frames (%d+%d=%d):-\n",
            NumShortTermDep, NumLongTermDep, NumReferenceDepFrames);

    for (i = 0; i < (NumReferenceDepFrames + 1); i++)
        if (ReferenceDepFrames[i].Usage != NotUsedForReference)
            SE_INFO(group_frameparser_video, " DFI %5d, F/F %s, Usage %c%c, FrameNumber %4d, POC %4d, LongTermIndex %4d\n",
                    ReferenceDepFrames[i].DecodeFrameIndex,
                    (ReferenceDepFrames[i].Field ? "Field" : "Frame"),
                    (((ReferenceDepFrames[i].Usage & UsedForTopLongTermReference) != 0) ?
                     'L' : (((ReferenceDepFrames[i].Usage & UsedForTopShortTermReference) != 0) ? 'S' : 'x')),
                    (((ReferenceDepFrames[i].Usage & UsedForBotLongTermReference) != 0) ?
                     'L' : (((ReferenceDepFrames[i].Usage & UsedForBotShortTermReference) != 0) ? 'S' : 'x')),
                    ReferenceDepFrames[i].FrameNum,
                    ReferenceDepFrames[i].PicOrderCnt,
                    ((ReferenceDepFrames[i].Usage & AnyUsedForLongTermReference) != 0) ? ReferenceDepFrames[i].LongTermFrameIdx : 9999);

#endif
    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Release reference frame or field
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::ReleaseDepReference(bool          ActuallyRelease,
                                                                       unsigned int      Entry,
                                                                       unsigned int      ReleaseUsage)
{
    unsigned int    OldUsage = ReferenceDepFrames[Entry].Usage;
    ReferenceDepFrames[Entry].Usage    = ReferenceDepFrames[Entry].Usage & (~ReleaseUsage);
#ifdef DUMP_REFLISTS
    SE_INFO(group_frameparser_video,  "MMCO - Release %d (%d) - %04x => %04x\n", ReferenceDepFrames[Entry].FrameNum, Entry, ReleaseUsage, ReferenceDepFrames[Entry].Usage);
#endif

    if ((ReferenceDepFrames[Entry].Usage == NotUsedForReference) && ActuallyRelease)
    {
        Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, MVC_ENCODE_DEP_FRAME_INDEX(ReferenceDepFrames[Entry].DecodeFrameIndex));
        // ProcessDeferredDFIandPTSUpto( ReferenceDepFrames[Entry].ExtendedPicOrderCnt ); // processing only done on base view
    }

    if (((OldUsage & AnyUsedForShortTermReference) != 0) &&
        ((ReferenceDepFrames[Entry].Usage & AnyUsedForShortTermReference) == 0))
    {
        NumShortTermDep--;
    }

    if (((OldUsage & AnyUsedForLongTermReference) != 0) &&
        ((ReferenceDepFrames[Entry].Usage & AnyUsedForLongTermReference) == 0))
    {
        NumLongTermDep--;
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Boolean function to evaluate whether or not the stream
//      parameters are new.
//

bool   FrameParser_VideoH264_MVC_c::NewStreamParametersCheck(void)
{
// For mono-delta MVC decode, stream parameters must be sent to the LX
// before each decode because the params are different for the base and dep view.
// So the Frame Parser must always resend the stream parameters to the Codec for each frame
// In the future, if there is dual-delta decode (1 delta for base, 1 delta for dependent),
// then the more sophisticated "change" check can be used again
#if 0
    bool Different;
    // Check differences for SPS & PPS of the base view
    Different = FrameParser_VideoH264_c::NewStreamParametersCheck();
    // StreamParameters points to MVCStreamParameters_t structure
    MVCStreamParameters = (MVCStreamParameters_t *) StreamParameters;

    //
    // If we haven't overwritten any, and the pointers are unchanged, then
    // they must the same, otherwise do a mem compare to check for differences.
    //

    if (ReadNewSubsetSPS || &(MVCStreamParameters->SubsetSequenceParameterSet->h264_header) != MVCFrameParameters->DepSliceHeader.SequenceParameterSet)
    {
        Different = true;
    }

    if (ReadNewPPS || MVCStreamParameters->DepPictureParameterSet != MVCFrameParameters->DepSliceHeader.PictureParameterSet)
    {
        Different = true;
    }

    ReadNewSubsetSPS = false;
    return Different;
#else
    return true;
#endif
}

// /////////////////////////////////////////////////////////////////////////
//
//      Copy SPS, Subset SPS and the 2 PPS into StreamParameters
//      (that then goes thru ParsedFrameParameters to Codec)
//

FrameParserStatus_t FrameParser_VideoH264_MVC_c::PrepareNewStreamParameters(void)
{
    FrameParserStatus_t    Status;
    Buffer_t PPSBuffer, SPSBuffer;
    // Prepare SPS & PPS for base view
    Status = FrameParser_VideoH264_c::PrepareNewStreamParameters();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    // StreamParameters points to a MVCStreamParameters_t structure
    MVCStreamParameters = (MVCStreamParameters_t *) StreamParameters;
    // Prepare Subset SPS & PPS for dependent view
    MVCStreamParameters->SubsetSequenceParameterSet    = (MVCSubSequenceParameterSetHeader_t *)(MVCFrameParameters->DepSliceHeader.SequenceParameterSet);
    MVCStreamParameters->DepPictureParameterSet        = MVCFrameParameters->DepSliceHeader.PictureParameterSet;
    PPSBuffer = PictureParameterSetTable[MVCStreamParameters->DepPictureParameterSet->pic_parameter_set_id].Buffer;
    SPSBuffer = SubsetSequenceParameterSetTable[MVCStreamParameters->SubsetSequenceParameterSet->h264_header.seq_parameter_set_id].Buffer;
    //
    // Attach SPS to PPS
    //
    PPSBuffer->AttachBuffer(SPSBuffer);
    //
    // Attach the picture parameter set, note the sequence
    // parameter set, and its extension, is already attached
    // to the picture parameter set.
    //
    StreamParametersBuffer->AttachBuffer(PPSBuffer);

    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Patch up ParsedFrameParameters for the MVC Stream parameters
//

FrameParserStatus_t   FrameParser_VideoH264_MVC_c::CommitFrameForDecode(void)
{
    FrameParserStatus_t              Status;
    Status = FrameParser_VideoH264_c::CommitFrameForDecode();
    ParsedFrameParameters->SizeofStreamParameterStructure = sizeof(MVCStreamParameters_t);
    return Status;
}
