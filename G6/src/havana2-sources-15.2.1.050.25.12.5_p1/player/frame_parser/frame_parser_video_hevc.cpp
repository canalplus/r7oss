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
#include "hevc.h"
#include "frame_parser_video_hevc.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoHevc_c"

// Increase antiemulation buffer size because some hevc SPS exceed 1024 bytes...
#undef ANTI_EMULATION_BUFFER_SIZE
#define ANTI_EMULATION_BUFFER_SIZE      3072

#define more_rbsp_data() Bits.MoreRsbpData()
#define GetBits(x,n) Bits.Get(n)


//#define DUMP_BITSTREAM
#define u_v(x,n,t,...) fp_u_v(n,t)
#define ue_v(x,t,...) fp_ue_v(t)
#define u_1(x,t,...) fp_u_1(t)
#define se_v(x,t,...) fp_se_v(t)

int32_t FrameParser_VideoHevc_c::fp_ue_v(const char *tracestring, ...)
{
    int32_t val;
    val = Bits.GetUe();
#ifdef DUMP_BITSTREAM
    SE_ERROR(tracestring, val, 0, 0, 0);
    SE_ERROR("%d", val);
#endif
    return val;
}

int32_t FrameParser_VideoHevc_c::fp_se_v(const char *tracestring, ...) //!< Arguments to the format string
{
    int32_t val;
    val = Bits.GetSe();
#ifdef DUMP_BITSTREAM
    SE_ERROR(tracestring, val, 0, 0, 0);
    SE_ERROR("%d", val);
#endif
    return val;
}


int32_t FrameParser_VideoHevc_c::fp_u_v(int32_t len, const char *tracestring, ...) //!< Arguments to the format string
{
    int32_t val;
    val = Bits.Get(len);
#ifdef DUMP_BITSTREAM
    SE_ERROR(tracestring, val, 0, 0, 0);
    SE_ERROR("%d", val);
#endif
    return val;
}

int32_t FrameParser_VideoHevc_c::fp_u_1(const char *tracestring, ...) //!< Arguments to the format string
{
    int32_t val;
    val = Bits.Get(1);
#ifdef DUMP_BITSTREAM
    SE_ERROR(tracestring, val, 0, 0, 0);
    SE_ERROR("%d", val);
#endif
    return val;
}


// Makefile
#define BYTES_PER_PIXEL 1

//defines.h
#define LOG2_MAX_CB_SIZE 6 //!< Log2(Maximum size of CB supported in luma pixels)
#define LOG2_MAX_TU_SIZE 5 //!< Log2(Maximum transform size supported)
#define MAX_SCALING_SIZEID      4 //!< Maximum number of SizeID signifying different transformation size for scaling.
#define MAX_SCALING_MATRIXID    6 //!< Maximum number of MatrixID for scaling.

//Other
#define trace(a,b,...)
#define assert(x)
#define trace_reset(x)

// END OF STHM' defines

static BufferDataDescriptor_t HevcStreamParametersDescriptor = BUFFER_HEVC_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t HevcFrameParametersDescriptor  = BUFFER_HEVC_FRAME_PARAMETERS_TYPE;

//
#define BUFFER_VIDEO_PARAMETER_SET      "HEVCVideoParameterSet"
#define BUFFER_VIDEO_PARAMETER_SET_TYPE     {BUFFER_VIDEO_PARAMETER_SET, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(HevcVideoParameterSet_t)}

#define BUFFER_SEQUENCE_PARAMETER_SET       "HEVCSequenceParameterSet"
#define BUFFER_SEQUENCE_PARAMETER_SET_TYPE  {BUFFER_SEQUENCE_PARAMETER_SET, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(HevcSequenceParameterSet_t)}

#define BUFFER_PICTURE_PARAMETER_SET        "HEVCPictureParameterSet"
#define BUFFER_PICTURE_PARAMETER_SET_TYPE   {BUFFER_PICTURE_PARAMETER_SET, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(HevcPictureParameterSet_t)}

static BufferDataDescriptor_t VideoParameterSetDescriptor    = BUFFER_VIDEO_PARAMETER_SET_TYPE;
static BufferDataDescriptor_t SequenceParameterSetDescriptor = BUFFER_SEQUENCE_PARAMETER_SET_TYPE;
static BufferDataDescriptor_t PictureParameterSetDescriptor  = BUFFER_PICTURE_PARAMETER_SET_TYPE;

static unsigned int     HEVCAspectRatioValues[][2]     =
{
    {  1,  1 }, // Not strictly specified but better to force square pixel when unspecified
    {  1,  1 },
    { 12, 11 },
    { 10, 11 },
    { 16, 11 },
    { 40, 33 },
    { 24, 11 },
    { 20, 11 },
    { 32, 11 },
    { 80, 33 },
    { 18, 11 },
    { 15, 11 },
    { 64, 33 },
    {160, 99 },
    {  4,  3 },
    {  3,  2 },
    {  2,  1 }
};

#define TOTAL_HEVC_LEVELS 13
static int HevcLevelIdc[] =
{
    10 * 3,
    20 * 3,
    21 * 3,
    30 * 3,
    31 * 3,
    40 * 3,
    41 * 3,
    50 * 3,
    51 * 3,
    52 * 3,
    60 * 3,
    61 * 3,
    62 * 3
};

static int HevcMaxSliceSegmentsPerPicture[] =
{
    16,
    16,
    20,
    30,
    40,
    75,
    75,
    200,
    200,
    200,
    600,
    600,
    600
};

#define HEVCAspectRatios(N) Rational_t(HEVCAspectRatioValues[N][0],HEVCAspectRatioValues[N][1])

#define Hevc_clip(val, min, max)  ((val < min) ? (min) : ((val > max)?(max):(val)))

#define AssertAntiEmulationOk()                             \
    if( TestAntiEmulationBuffer() != FrameParserNoError ) { \
        SE_WARNING( "Anti Emulation Test fail\n");         \
        Stream->MarkUnPlayable();             \
        FrameParameters = NULL;             \
        return FrameParserStreamUnplayable; }


#define IsNalUnitSlice(NalUnitType) ((NalUnitType == NAL_UNIT_CODED_SLICE_TRAIL_N) ||   \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_TRAIL_R ) ||   \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_TSA_N) ||  \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_TLA_R)   ||   \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_STSA_N)  ||   \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_STSA_R) || \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_RADL_N) || \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_RADL_R)  ||   \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_RASL_N)  ||   \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_RASL_R)  ||   \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_BLA_W_LP)   ||    \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_BLA_W_RADL) ||    \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_BLA_N_LP)   ||    \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL) ||    \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP) ||  \
                     (NalUnitType == NAL_UNIT_CODED_SLICE_CRA)      ||  \
                     (NalUnitType == NAL_UNIT_RESERVED_IRAP_VCL22)  ||  \
                     (NalUnitType == NAL_UNIT_RESERVED_IRAP_VCL23))


// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoHevc_c::FrameParser_VideoHevc_c(void)
    : FrameParser_Video_c()
    , CTBAddress(0)
    , short_term_ref_pic_set_idx(0)
    , st_rps()
    , lt_rps()
    , delta_poc_msb_present_flag()
    , delta_poc_msb_cycle_lt()
    , slice_types(0)
    , pic_output_flag(false)
    , pic_parameter_set_id(0)
    , no_output_of_rasl_pics_flag(false)
    , no_output_of_prior_pics_flag(false)
    , pic_order_cnt_lsb(0)
    , has_picture_digest(false)
    , picture_digest()
    , mVPS(NULL)
    , mSPS(NULL)
    , mPPS(NULL)
    , mParsedStartCodeNum(0)
    , mSecondPass(false)
    , PictureCount(0)
    , ReadNewVPS(false)
    , ReadNewSPS(false)
    , ReadNewPPS(false)
    , VideoParameterSetType()
    , VideoParameterSetPool(NULL)
    , VideoParameterSetTable()
    , SequenceParameterSetType()
    , SequenceParameterSetPool(NULL)
    , SequenceParameterSetTable()
    , PictureParameterSetType()
    , PictureParameterSetPool(NULL)
    , PictureParameterSetTable()
    , CopyOfSequenceParameterSet()
    , CopyOfVideoParameterSet()
    , CopyOfPictureParameterSet()
    , StreamParameters(NULL)
    , FrameParameters(NULL)
    , DefaultPixelAspectRatio(1)
    , SeenAnIDR(false)
    , BehaveAsIfSeenAnIDR(false)
    , mDecodeStartedOnRAP(false)
    , mMissingRef(false)
    , SliceHeader(NULL)
    , SeenProbableReversalPoint(false)
    , NumReferenceFrames(0)
    , ReferenceFrames()
    , mDeferredList()
    , mDeferredListEntries(0)
    , mOrderedDeferredList()
    , PicOrderCntOffset(0x100000000ULL)      // Guarantee the extended value never goes negative
    , PicOrderCntOffsetAdjust(0)
    , PrevPicOrderCntMsb(0)
    , PrevPicOrderCntLsb(0)
    , DisplayOrderByDpbValues(false)
    , DpbValuesInvalidatedByPTS(false)
    , ValidPTSSequence(false)
    , LastExitPicOrderCntMsb(0)
    , mUserData()
{
    if (InitializationStatus != FrameParserNoError)
    {
        return;
    }

    Configuration.FrameParserName               = "VideoHEVC";
    Configuration.StreamParametersCount         = HEVC_STREAM_PARAMETERS_COUNT;
    Configuration.StreamParametersDescriptor    = &HevcStreamParametersDescriptor;
    Configuration.FrameParametersCount          = HEVC_FRAME_PARAMETERS_COUNT;
    Configuration.FrameParametersDescriptor     = &HevcFrameParametersDescriptor;
    Configuration.MaxReferenceFrameCount        = HEVC_MAX_DPB_SIZE;
    Configuration.MaxUserDataBlocks             = 3;

    Configuration.SupportSmoothReversePlay      = true; // dynamic config..

    mMaxFrameWidth  = HEVC_MAX_FRAME_WIDTH;
    mMaxFrameHeight = HEVC_MAX_FRAME_HEIGHT;

    // TODO CL refine usage of these fields : are they actually needed if I take STHM' pieces of code ???
#if 0
    PrepareScannedScalingLists();
#endif

    // TODO(pht) move to a FinalizeInit method
    mAntiEmulationBufferMaxSize = ANTI_EMULATION_BUFFER_SIZE;
    delete [] AntiEmulationBuffer;
    AntiEmulationBuffer = new unsigned char[mAntiEmulationBufferMaxSize];
    if (!AntiEmulationBuffer)
    {
        SE_ERROR("Unable to allocate AntiEmulationBuffer\n");
        InitializationStatus = FrameParserError;
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoHevc_c::~FrameParser_VideoHevc_c(void)
{
    Halt();

    //
    // Destroy the parameter set pools - picture parameter set first,
    // since it may have attached sequence parameter sets.
    //

    unsigned int    i;

    if (VideoParameterSetPool != NULL)
    {
        for (i = 0; i < HEVC_STANDARD_MAX_VIDEO_PARAMETER_SETS; i++)
            if (VideoParameterSetTable[i].Buffer != NULL)
            {
                VideoParameterSetTable[i].Buffer->DecrementReferenceCount();
            }

        BufferManager->DestroyPool(VideoParameterSetPool);
    }

    if (PictureParameterSetPool != NULL)
    {
        for (i = 0; i < HEVC_STANDARD_MAX_PICTURE_PARAMETER_SETS; i++)
            if (PictureParameterSetTable[i].Buffer != NULL)
            {
                PictureParameterSetTable[i].Buffer->DecrementReferenceCount();
            }

        BufferManager->DestroyPool(PictureParameterSetPool);
    }

    if (SequenceParameterSetPool != NULL)
    {
        for (i = 0; i < HEVC_STANDARD_MAX_SEQUENCE_PARAMETER_SETS; i++)
            if (SequenceParameterSetTable[i].Buffer != NULL)
            {
                SequenceParameterSetTable[i].Buffer->DecrementReferenceCount();
            }

        BufferManager->DestroyPool(SequenceParameterSetPool);
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Method to connect to neighbor
//

FrameParserStatus_t FrameParser_VideoHevc_c::Connect(Port_c *Port)
{
    FrameParserStatus_t Status;
    BufferStatus_t      BufStatus;
    //
    // Clear our parameter pointers
    //
    StreamParameters                    = NULL;
    FrameParameters                     = NULL;
    DeferredParsedFrameParameters       = NULL;
    DeferredParsedVideoParameters       = NULL;
    //
    // Pass the call on down (we need the frame parameters count obtained by the lower level function).
    //
    Status = FrameParser_Video_c::Connect(Port);

    if (Status != FrameParserNoError)
    {
        SetComponentState(ComponentInError);
        return Status;
    }

    //
    // Attempt to register the types for video, sequence and picture parameter sets
    //
    BufStatus = BufferManager->FindBufferDataType(VideoParameterSetDescriptor.TypeName, &VideoParameterSetType);

    if (BufStatus != BufferNoError)
    {
        BufStatus = BufferManager->CreateBufferDataType(&VideoParameterSetDescriptor, &VideoParameterSetType);

        if (BufStatus != BufferNoError)
        {
            SE_ERROR("Failed to create the video parameter set buffer type (status:%d)\n", BufStatus);
            goto bail;
        }
    }

    BufStatus = BufferManager->FindBufferDataType(SequenceParameterSetDescriptor.TypeName, &SequenceParameterSetType);

    if (BufStatus != BufferNoError)
    {
        BufStatus = BufferManager->CreateBufferDataType(&SequenceParameterSetDescriptor, &SequenceParameterSetType);

        if (BufStatus != BufferNoError)
        {
            SE_ERROR("Failed to create the sequence parameter set buffer type (status:%d)\n", BufStatus);
            goto bail;
        }
    }

    BufStatus = BufferManager->FindBufferDataType(PictureParameterSetDescriptor.TypeName, &PictureParameterSetType);

    if (BufStatus != BufferNoError)
    {
        BufStatus = BufferManager->CreateBufferDataType(&PictureParameterSetDescriptor, &PictureParameterSetType);

        if (BufStatus != BufferNoError)
        {
            SE_ERROR("Failed to create the picture parameter set buffer type (status:%d)\n", BufStatus);
            goto bail;
        }
    }

    //
    // Create the pools of video, sequence and picture parameter sets
    //
    BufStatus = BufferManager->CreatePool(&VideoParameterSetPool, VideoParameterSetType, HEVC_MAX_VIDEO_PARAMETER_SETS);

    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to create a pool of sequence parameter sets (status:%d)\n", BufStatus);
        goto bail;
    }

    BufStatus = BufferManager->CreatePool(&SequenceParameterSetPool, SequenceParameterSetType, HEVC_MAX_SEQUENCE_PARAMETER_SETS);

    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to create a pool of sequence parameter sets (status:%d)\n", BufStatus);
        goto FailedCreateSPSPool;
    }

    BufStatus = BufferManager->CreatePool(&PictureParameterSetPool, PictureParameterSetType, HEVC_MAX_PICTURE_PARAMETER_SETS);

    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to create a pool of picture parameter sets (status:%d)\n", BufStatus);
        goto FailedCreatePPSPool;
    }

    return FrameParserNoError;
FailedCreatePPSPool:
    BufferManager->DestroyPool(SequenceParameterSetPool);
FailedCreateSPSPool:
    BufferManager->DestroyPool(VideoParameterSetPool);
bail:
//TODO Disconnect
    SetComponentState(ComponentInError);
    return FrameParserError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The three helper functions for the collator, to allow it to
//  determine which headers form frame boundaries.
//

FrameParserStatus_t   FrameParser_VideoHevc_c::ResetCollatedHeaderState(void)
{
    // Until the reversible collator, there is no state to reset.
    SeenProbableReversalPoint   = false;
    return FrameParserNoError;
}

// -----------------------

unsigned int FrameParser_VideoHevc_c::RequiredPresentationLength(unsigned char StartCode)
{
    unsigned char   NalUnitType = ((StartCode >> 1) & 0x3f);
    unsigned int    ExtraBytes  = 0;

    // Needed to determine if slice is first slice
    if (IsNalUnitSlice(NalUnitType))
    {
        ExtraBytes = 2;
    }

    return ExtraBytes;
}

// -----------------------

FrameParserStatus_t   FrameParser_VideoHevc_c::PresentCollatedHeader(
    unsigned char StartCode, unsigned char *HeaderBytes,
    FrameParserHeaderFlag_t  *Flags)
{
    unsigned char   NalUnitType = ((StartCode >> 1) & 0x3f);
    bool        Slice;
    bool        FirstSlice;

    *Flags  = 0;

    Slice = IsNalUnitSlice(NalUnitType);
    // NAL Header is two-byte long and HeaderBytes starts one byte after end of start code
    FirstSlice = Slice && ((HeaderBytes[1] & 0x80) != 0); // first_slice_segment_in_pic_flag is Msb of 2nd byte following the startcode
    //

//TODO CL : check NAL_UNIT_SPS VPS are HEVC entry point as good as SPS was for H264 ????
    if (SeenProbableReversalPoint && (Slice || (NalUnitType == NAL_UNIT_VPS) || (NalUnitType == NAL_UNIT_SPS)))
    {
        *Flags              |= FrameParserHeaderFlagConfirmReversiblePoint;

        if (NalUnitType == NAL_UNIT_SPS)
            *Flags |= FrameParserHeaderFlagPossibleReversiblePoint
                      | FrameParserHeaderFlagPartitionPoint; // Make this the reversible point

        SeenProbableReversalPoint    = false;
    }

// TODO CL : this condition may be reworked too, not yet sure IDR, CRA & BLA are the good and/or only ones...
    if ((NalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL) ||
        (NalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP) ||
        (NalUnitType == NAL_UNIT_CODED_SLICE_CRA) ||
        (NalUnitType == NAL_UNIT_CODED_SLICE_BLA_W_LP) ||
        (NalUnitType == NAL_UNIT_CODED_SLICE_BLA_W_RADL) ||
        (NalUnitType == NAL_UNIT_CODED_SLICE_BLA_N_LP))
    {
        *Flags              |= FrameParserHeaderFlagPossibleReversiblePoint;
        SeenProbableReversalPoint    = true;
    }

    if (FirstSlice)
    {
        *Flags    |= FrameParserHeaderFlagPartitionPoint;
    }

//
//    SE_ERROR( "PresentCollatedHeader: NAL %d, Flags 0x%x, Slice %d, FirstSlice %d (%02x%02x%02x%02x)",
//            NalUnitType, *Flags, Slice, FirstSlice, HeaderBytes[0],
//            HeaderBytes[1], HeaderBytes[2], HeaderBytes[3]);
    return FrameParserNoError;
}

/*! \brief Sort a rps in the increasing order of the poc
 *
 * \param[in,out] rps rps to be sorted
 */
void FrameParser_VideoHevc_c::SortRpsIncreasingOrder(rps_t *rps)
{
    uint8_t i;
    uint8_t length = rps->num_pics;
    rps_elem_t elem_temp;
    bool unchanged;

    if (length <= 1)
    {
        return;
    }

    // Bubble sort
    while (1)
    {
        unchanged = true;

        for (i = 1; i < length; i++)
        {
            if (rps->elem[i - 1].delta_poc > rps->elem[i].delta_poc)
            {
                // Switch elem[i-1] and elem[i]
                elem_temp = rps->elem[i - 1];
                rps->elem[i - 1] = rps->elem[i];
                rps->elem[i] = elem_temp;
                unchanged = false;
            }
        }

        if (unchanged)
        {
            return;
        }

        length--;
    }
}

/*! \brief Read and predict a short term RPS
 *
 * \param[in] s Bitstream to be parsed.
 *
 * \param[out] cur_st_rps Short term RPS to be read and predicted.
 *
 * \param[in] ref_st_rps Array of already predicted short term RPS.
 *
 * \param[in] i Number of already predicted short term RPS
 */
void FrameParser_VideoHevc_c::short_term_ref_pic_set(st_rps_t *cur_st_rps, st_rps_t *ref_st_rps, uint8_t i, bool slice_not_sps)
{
    uint8_t delta_idx_minus1;
    uint8_t i0, i1, j, RIdx, num_pics_refRPS, num_pics_s0_refRPS, num_pics_s1_refRPS;
    int32_t abs_delta_rps_minus1, DeltaRPS, DPoc, delta_poc_minus1;
    bool    inter_RPS_flag, delta_rps_sign, ref_idc0, ref_idc1;
    int32_t prev;

    if (i > 0)
    {
        inter_RPS_flag = u_1(s,  "RPS[%2d]: inter_ref_pic_set_prediction_flag", i);
    }
    else
    {
        inter_RPS_flag = false;
    }

    // The current RPS is predicted from a reference RPS
    if (inter_RPS_flag)
    {
        // Compute the index of the reference RPS
        if (slice_not_sps)
        {
            delta_idx_minus1 = ue_v(s,  "RPS[%2d]: delta_idx_minus1", i);
            // Limit the range of delta_idx_minus1 to [0, stRpsIdx − 1]
            if (delta_idx_minus1 > (i - 1))
            {
                delta_idx_minus1 = i - 1;
            }
        }
        else
        {
            delta_idx_minus1 = 0;
        }

        RIdx = i - (delta_idx_minus1 + 1);
        // Point to the reference RPS
        ref_st_rps += RIdx;
        // Compute the number of pictures in the reference RPS
        num_pics_s0_refRPS = ref_st_rps->rps_s0.num_pics;
        num_pics_s1_refRPS = ref_st_rps->rps_s1.num_pics;
        num_pics_refRPS = num_pics_s0_refRPS + num_pics_s1_refRPS;
        // Compute the POC difference between the POCs of the pictures inside the reference RPS
        // and the POCs of the pictures inside the current RPS
        delta_rps_sign       = u_1(s,  "RPS[%2d]: delta_rps_sign", i);
        abs_delta_rps_minus1 = ue_v(s,  "RPS[%2d]: abs_delta_rps_minus1", i);
        DeltaRPS = abs_delta_rps_minus1 + 1;
        DeltaRPS = (delta_rps_sign ? -DeltaRPS : DeltaRPS);

        // Compute the current RPS.
        // For each picture in the reference RPS, the syntax elements ref_idc0/ref_idc1
        // indicate if the reference picture is used to predict a picture in the current RPS.
        // There is one extra loop run, where ref_idc0/ref_idc1 indicate if we add a picture
        // in the current RPS which is predicted from picture with POC = 0
        if (num_pics_refRPS > HEVC_MAX_REFERENCE_INDEX)
        {
            num_pics_refRPS = HEVC_MAX_REFERENCE_INDEX;    /* ERC :  Recover & Continue */
        }
        if (num_pics_s0_refRPS > HEVC_MAX_REFERENCE_INDEX)
        {
            num_pics_s0_refRPS = HEVC_MAX_REFERENCE_INDEX;
        }

        for (i0 = i1 = j = 0; j <= num_pics_refRPS; j++)
        {
            ref_idc0 = u_1(s,  "RPS[%2d]: used_by_curr_pic_flag[%d]", i, j);
            ref_idc1 = 0;

            if (ref_idc0 == 0)
            {
                ref_idc1 = u_1(s,  "RPS[%2d]: use_delta_flag[%d]", i, j);
            }

            // The picture in the reference RPS is used to predict a picture in the current RPS
            if (ref_idc0 || ref_idc1)
            {
                // Compute the predicted picture POC
                if (j < num_pics_s0_refRPS)
                {
                    DPoc = -ref_st_rps->rps_s0.elem[j].delta_poc + DeltaRPS;
                }
                else if (j < num_pics_refRPS)
                {
                    DPoc = ref_st_rps->rps_s1.elem[j - num_pics_s0_refRPS].delta_poc + DeltaRPS;
                }
                else
                {
                    DPoc = DeltaRPS;
                }

                // Add the predicted picture inside the current RPS
                if (DPoc < 0)
                {
                    cur_st_rps->rps_s0.elem[i0].delta_poc = -DPoc;
                    cur_st_rps->rps_s0.elem[i0].used_by_curr_pic_flag = ref_idc0;
                    i0++;
                }
                else
                {
                    cur_st_rps->rps_s1.elem[i1].delta_poc = DPoc;
                    cur_st_rps->rps_s1.elem[i1].used_by_curr_pic_flag = ref_idc0;
                    i1++;
                }
            }
        }

        cur_st_rps->rps_s0.num_pics = i0;
        cur_st_rps->rps_s1.num_pics = i1;
        // The current RPS is explicitly given
    }
    else
    {
        cur_st_rps->rps_s0.num_pics          = ue_v(s,  "RPS[%2d]: num_negative_pics", i);
        cur_st_rps->rps_s1.num_pics          = ue_v(s,  "RPS[%2d]: num_positive_pics", i);

        if (cur_st_rps->rps_s0.num_pics > HEVC_MAX_REFERENCE_INDEX)
        {
            cur_st_rps->rps_s0.num_pics = HEVC_MAX_REFERENCE_INDEX ;    /* ERC : Recover & Continue */
        }

        if (cur_st_rps->rps_s1.num_pics > HEVC_MAX_REFERENCE_INDEX)
        {
            cur_st_rps->rps_s1.num_pics = HEVC_MAX_REFERENCE_INDEX ;    /* ERC : Recover & Continue */
        }

        for (prev = j = 0; j < cur_st_rps->rps_s0.num_pics; j++)
        {
            delta_poc_minus1                   = ue_v(s,  "RPS[%2d]: delta_poc_s0_minus1[%d]", i, j);
            cur_st_rps->rps_s0.elem[j].used_by_curr_pic_flag = u_1(s,  "RPS[%2d]: used_by_curr_pic_s0_flag[%d]", i, j);
            cur_st_rps->rps_s0.elem[j].delta_poc = prev + (delta_poc_minus1 + 1);
            prev = cur_st_rps->rps_s0.elem[j].delta_poc;
        }

        for (prev = j = 0; j < cur_st_rps->rps_s1.num_pics; j++)
        {
            delta_poc_minus1                   = ue_v(s,  "RPS[%2d]: delta_poc_s1_minus1[%d]", i, j);
            cur_st_rps->rps_s1.elem[j].used_by_curr_pic_flag = u_1(s,  "RPS[%2d]: used_by_curr_pic_s1_flag[%d]", i, j);
            cur_st_rps->rps_s1.elem[j].delta_poc = prev + (delta_poc_minus1 + 1);
            prev = cur_st_rps->rps_s1.elem[j].delta_poc;
        }
    }

    // Sort the arrays
    SortRpsIncreasingOrder(&(cur_st_rps->rps_s0));
    SortRpsIncreasingOrder(&(cur_st_rps->rps_s1));
}

void FrameParser_VideoHevc_c::parseProfileTier(int32_t i)
{
    int32_t j;
    u_v(s, 2, "PTL: XXX_profile_space[%d]", i);
    // Avoid CHECKED_RETURN coverity errors
    (void) u_1(s   , "PTL: XXX_tier_flag[%d]", i);
    u_v(s, 5, "PTL: XXX_profile_idc[%d]", i);

    for (j = 0; j < 32; j++)
    {
        (void)u_1(s   , "PTL: XXX_profile_compatibility_flag[%d][%2d]", i, j);
    }

    (void)u_1(s   , "PTL: general_progressive_source_flag");
    (void)u_1(s   , "PTL: general_interlaced_source_flag");
    (void)u_1(s   , "PTL: general_non_packed_constraint_flag");
    (void)u_1(s   , "PTL: general_frame_only_constraint_flag");
    u_v(s, 16, "PTL: XXX_reserved_zero_44bits[%d][0..15]", i);
    u_v(s, 16, "PTL: XXX_reserved_zero_44bits[%d][16..31]", i);
    u_v(s, 12, "PTL: XXX_reserved_zero_44bits[%d][32..43]", i);
}

int32_t FrameParser_VideoHevc_c::parsePTL(bool profilePresentFlag, uint8_t maxNumSubLayersMinus1)
{
    uint8_t i;
    int32_t profile[MAX_TLAYER], level[MAX_TLAYER], level_idc;

    if (profilePresentFlag)
    {
        parseProfileTier(0);
    }

    level_idc                                   = u_v(s, 8, "PTL: general_level_idc");

    for (i = 0; i < maxNumSubLayersMinus1; i++)
    {
        if (profilePresentFlag)
        {
            profile[i]                          = u_1(s   , "PTL: sub_layer_profile_present_flag[%d]", i);
        }

        level[i]                                = u_1(s   , "PTL: sub_layer_level_present_flag[%d]", i);
    }

    if (maxNumSubLayersMinus1 > 0)
        for (; i < 8; i++)
        {
            u_v(s, 2, "PTL: reserved_zero_2bits");
        }

    for (i = 0; i < maxNumSubLayersMinus1; i++)
    {
        if (profilePresentFlag && profile[i])
        {
            parseProfileTier(i + 1);
        }

        if (level[i])
        {
            u_v(s, 8, "PTL: sub_layer_level_idc[%d]", i);
        }
    }

    return level_idc;
}

void FrameParser_VideoHevc_c::ReadHRD(HevcHrd_t *hrd, bool commonInfPresentFlag, uint8_t max_temporal_layers)
{
    uint8_t i, j, nalOrVcl;

    if (commonInfPresentFlag)
    {
        hrd->nal_hrd_parameters_present_flag = u_1(s,   "HRD: nal_hrd_parameters_present_flag");
        hrd->vcl_hrd_parameters_present_flag = u_1(s,   "HRD: vcl_hrd_parameters_present_flag");

        if (hrd->nal_hrd_parameters_present_flag || hrd->vcl_hrd_parameters_present_flag)
        {
            hrd->sub_pic_cpb_params_present_flag = u_1(s,   "HRD: sub_pic_cpb_params_present_flag");

            if (hrd->sub_pic_cpb_params_present_flag)
            {
                hrd->tick_divisor_minus2 = u_v(s,  8, "HRD: tick_divisor_minus2");
                hrd->du_cpb_removal_delay_length_minus1 = u_v(s,  5, "HRD: du_cpb_removal_delay_length_minus1");
                hrd->sub_pic_cpb_params_in_pic_timing_sei_flag = u_1(s, "sub_pic_cpb_params_in_pic_timing_sei_flag");
                hrd->dpb_output_delay_du_length_minus1 = u_v(s,  5, "HRD: dpb_output_delay_du_length_minus1");
            }

            hrd->bit_rate_scale = u_v(s,  4, "HRD: bit_rate_scale");
            hrd->cpb_size_scale = u_v(s,  4, "HRD: cpb_size_scale");

            if (hrd->sub_pic_cpb_params_present_flag)
            {
                hrd->cpb_size_du_scale = u_v(s,  4, "HRD: cpb_size_du_scale");
            }

            hrd->initial_cpb_removal_delay_length_minus1 = u_v(s,  5, "HRD: initial_cpb_removal_delay_length_minus1");
            hrd->au_cpb_removal_delay_length_minus1 = u_v(s,  5, "HRD: au_cpb_removal_delay_length_minus1");
            hrd->dpb_output_delay_length_minus1 = u_v(s,  5, "HRD: dpb_output_delay_length_minus1");
        }
    }

    for (i = 0; i < max_temporal_layers; i ++)
    {
        hrd->fixed_pic_rate_general_flag[i] = u_1(s,   "HRD: fixed_pic_rate_general_flag[%d]", i);

        if (!hrd->fixed_pic_rate_general_flag[i])
        {
            hrd->fixed_pic_rate_within_cvs_flag[i] = u_1(s,   "HRD: fixed_pic_rate_within_cvs_flag[%d]", i);
        }
        else
        {
            hrd->fixed_pic_rate_within_cvs_flag[i] = true;
        }

        if (hrd->fixed_pic_rate_within_cvs_flag[i])
        {
            hrd->elemental_duration_in_tc_minus1[i] = ue_v(s,   "HRD: elemental_duration_in_tc_minus1[%d]", i);
            hrd->low_delay_hrd_flag[i] = false;
        }
        else
        {
            hrd->low_delay_hrd_flag[i] = u_1(s,   "HRD: low_delay_hrd_flag[%d]", i);
        }

        if (!hrd->low_delay_hrd_flag[i])
        {
            hrd->cpb_cnt_minus1[i] = ue_v(s,   "HRD: cpb_cnt_minus1[%d]", i);
        }
        else
        {
            hrd->cpb_cnt_minus1[i] = 0;
        }

        for (nalOrVcl = 0; nalOrVcl < 2; nalOrVcl ++)
        {
            if (((nalOrVcl == 0) && (hrd->nal_hrd_parameters_present_flag)) ||
                ((nalOrVcl == 1) && (hrd->vcl_hrd_parameters_present_flag)))
            {
                for (j = 0; j < (hrd->cpb_cnt_minus1[i] + 1); j ++)
                {
                    hrd->bit_rate_value_minus1[i][nalOrVcl][j] = ue_v(s,   "HRD: bit_rate_value_minus1[%d][%d][%d]", i, nalOrVcl, j);
                    hrd->cpb_size_value_minus1[i][nalOrVcl][j] = ue_v(s,   "HRD: cpb_size_value_minus1[%d][%d][%d]", i, nalOrVcl, j);

                    if (hrd->sub_pic_cpb_params_present_flag)
                    {
                        hrd->bit_rate_du_value_minus1[i][nalOrVcl][j] = ue_v(s,   "HRD: bit_rate_du_value_minus1[%d][%d][%d]", i, nalOrVcl, j);
                        hrd->cpb_size_du_value_minus1[i][nalOrVcl][j] = ue_v(s,   "HRD: cpb_size_du_value_minus1[%d][%d][%d]", i, nalOrVcl, j);
                    }

                    hrd->cbr_flag[i][nalOrVcl][j] = u_1(s,   "HRD: cbr_flag[%d][%d][%d]", i, nalOrVcl, j);
                }
            }
        }
    }
}

// TODO CL : Sublayers are not supported yet, nor vps_max_dec_pic_buffering, vps_num_reorder_pics & vps_max_latency_increase
FrameParserStatus_t FrameParser_VideoHevc_c::InterpretVPS(HevcVideoParameterSet_t *vps)
{
    uint32_t i, j;
    bool subLayerOrderingInfoPresentFlag, commonInfPresentFlag;
    uint32_t vps_max_op_sets, vps_max_nuh_reserved_zero_layer_id, vps_num_hrd_parameters;

    vps->vps_video_parameter_set_id = u_v(s, 4, "VPS: vps_video_parameter_set_id");
    if (vps->vps_video_parameter_set_id >= HEVC_MAX_VIDEO_PARAMETER_SETS)
    {
        SE_ERROR("VPS: vps_video_parameter_set_id (%d) exceeds maximum (%d)\n",
                 vps->vps_video_parameter_set_id, HEVC_MAX_VIDEO_PARAMETER_SETS - 1);
        /* Not Recoverable*/
        return FrameParserError;
    }

    u_v(s, 2, "VPS: vps_reserved_three_2bits");
    u_v(s, 6, "VPS: vps_reserved_zero_6bits");
    vps->vps_max_temporal_layers                = u_v(s, 3, "VPS: vps_max_sub_layers_minus1") + 1;
    (void)u_1(s   , "VPS: vps_temporal_id_nesting_flag");
    u_v(s, 16, "VPS: vps_reserved_ffff_16bits");
    parsePTL(true, vps->vps_max_temporal_layers - 1);
    subLayerOrderingInfoPresentFlag             = u_1(s   , "VPS: vps_sub_layer_ordering_info_present_flag");

    for (i = 0; i < vps->vps_max_temporal_layers; i++)
    {
        ue_v(s   , "VPS: vps_max_dec_pic_buffering_minus1[%d]", i);
        ue_v(s   , "VPS: vps_num_reorder_pics[%d]", i);
        ue_v(s   , "VPS: vps_max_latency_increase[%d]", i);

        if (!subLayerOrderingInfoPresentFlag)
        {
            break;
        }
    }

    vps_max_nuh_reserved_zero_layer_id          = u_v(s, 6, "VPS: vps_max_nuh_reserved_zero_layer_id");

    if (vps_max_nuh_reserved_zero_layer_id >= MAX_VPS_NUH_RESERVED_ZERO_LAYER_ID_PLUS1)
    {
        SE_ERROR("VPS: vps_max_nuh_reserved_zero_layer_id exceeds maximum\n");
    }

    vps_max_op_sets                             = ue_v(s   , "VPS: vps_max_op_sets_minus1") + 1;

    if (vps_max_op_sets > MAX_VPS_OP_SETS_PLUS1)
    {
        SE_WARNING("VPS: vps_max_op_sets exceeds maximum\n");
        vps_max_op_sets = MAX_VPS_OP_SETS_PLUS1; /* ERC : Recover & Continue */
    }

    for (j = 1; j < vps_max_op_sets; j++)
        for (i = 0; i <= vps_max_nuh_reserved_zero_layer_id; i++)
        {
            (void)u_1(s   , "VPS: layer_id_included_flag[%d][%d]", j, i);
        }

    vps->vps_timing_info_present_flag = u_1(s, "VPS: vps_timing_info_present_flag");

    if (vps->vps_timing_info_present_flag)
    {
        vps->vps_num_units_in_tick = u_v(s,  32, "VPS: vps_num_units_in_tick");
        vps->vps_time_scale = u_v(s,  32, "VPS: vps_time_scale");
        vps->vps_poc_proportional_to_timing_flag = u_1(s, "VPS: vps_poc_proportional_to_timing_flag");

        if (vps->vps_poc_proportional_to_timing_flag)
        {
            vps->vps_num_ticks_poc_diff_one_minus1 = ue_v(s, "VPS: vps_num_ticks_poc_diff_one_minus1");
        }

        vps_num_hrd_parameters = ue_v(s   , "VPS: vps_num_hrd_parameters");
        if (vps_num_hrd_parameters > MAX_VPS_NUM_HRD_PARAMETERS)
        {
            SE_ERROR("VPS: vps_num_hrd_parameters exceeds maximum\n");
            return FrameParserError;
        }
        for (i = 0; i < vps_num_hrd_parameters; i++)
        {
            ue_v(s   , "VPS: hrd_layer_set_idx[%d]", i);

            if (i > 0)
            {
                commonInfPresentFlag = u_1(s, "VPS: cprms_present_flag[%d]", i);
            }
            else
            {
                commonInfPresentFlag = false;
            }

            ReadHRD(vps->hrd + i, commonInfPresentFlag, vps->vps_max_temporal_layers);
        }
    }

    if (u_1(s   , "VPS: vps_extension_flag"))
    {
        while (more_rbsp_data())
        {
            (void)u_1(s   , "VPS: vps_extension_data_flag");
        }
    }
    return FrameParserNoError;
}

/*!
 ************************************************************************
 * \brief
 *    Parse a VPS, and store it at its specified vps_video_parameter_set_id slot
 ************************************************************************
 */
FrameParserStatus_t FrameParser_VideoHevc_c::ProcessVPS()
{
    Buffer_t TmpBuffer;
    HevcVideoParameterSet_t *vps;
    unsigned int vps_id;
    FrameParserStatus_t Status;

    // Get a new buffer
    BufferStatus_t BufStatus = VideoParameterSetPool->GetBuffer(&TmpBuffer);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to get a video parameter set buffer (status:%d)\n", BufStatus);
        return FrameParserError;
    }

    TmpBuffer->ObtainDataReference(NULL, NULL, (void **)(&vps));
    if (!vps)
    {
        TmpBuffer->DecrementReferenceCount();
        SE_ERROR("ObtainDataReference failed\n");
        return FrameParserError;
    }

    memset(vps, 0, sizeof(*vps));
    // Read VPS
    Status = InterpretVPS(vps);
    if (Status != FrameParserNoError)
    {
        TmpBuffer->DecrementReferenceCount();
        SE_ERROR("Non Recoverable Error in InterpretVPS (status:%d)\n", Status);
        return FrameParserError;
    }

    vps_id = vps->vps_video_parameter_set_id;
    if (vps_id >= HEVC_MAX_VIDEO_PARAMETER_SETS)
    {
        TmpBuffer->DecrementReferenceCount();
        SE_ERROR("vps_id wrong value\n");
        return FrameParserError;
    }
    /* Checks vps index boundary for VideoParameterSetTable (probable implementation error ?) */
    if (vps_id >= HEVC_STANDARD_MAX_VIDEO_PARAMETER_SETS)
    {
        TmpBuffer->DecrementReferenceCount();
        SE_ERROR("vps_id out of bounds for VPS array !\n");
        return FrameParserError;
    }
    // If we already have a buffer with the same VPS ID, then release it
    if (VideoParameterSetTable[vps_id].Buffer != NULL)
    {
        VideoParameterSetTable[vps_id].Buffer->DecrementReferenceCount();
    }

    // Save buffer and pointer
    VideoParameterSetTable[vps_id].Buffer = TmpBuffer;
    VideoParameterSetTable[vps_id].VPS = vps;
    ReadNewVPS = true;
    return FrameParserNoError;
}

void FrameParser_VideoHevc_c::scaling_list_syntax(scaling_list_t *scaling_list,
                                                  uint8_t sizeID, uint8_t matrixID)
{
    int16_t nextCoef;
    uint8_t i;
    uint8_t coefNum = (sizeID ? 64 : 16);

    if (sizeID > 1)
    {
        nextCoef = se_v(s, "SL: scaling_list_dc_coef_minus8[%d][%d]", sizeID, matrixID);
        (*scaling_list)[sizeID][matrixID].scaling_list_dc_coef_minus8 = nextCoef;
        nextCoef += 8;
    }
    else
    {
        nextCoef = 8;
    }

    for (i = 0; i < coefNum; i++)
    {
        (*scaling_list)[sizeID][matrixID].scaling_list_delta_coef[i] = se_v(s, "SL: scaling_list_delta_coef[%d]", i);
    }
}

void FrameParser_VideoHevc_c::scaling_list_param(scaling_list_t *scaling_list) //!< [out] Decoded scaling list structure)
{
    bool tmp;
    uint8_t sizeID, matrixID, max_matrixID, val;

    for (sizeID = 0; sizeID < MAX_SCALING_SIZEID; sizeID++)
    {
        max_matrixID = (sizeID == 3) ? 2 : MAX_SCALING_MATRIXID;

        for (matrixID = 0; matrixID < max_matrixID; matrixID++)
        {
            tmp = u_1(s, "SL: scaling_list_pred_mode_flag[%d][%d]", sizeID, matrixID);
            (*scaling_list)[sizeID][matrixID].scaling_list_pred_mode_flag = tmp;

            if (tmp)
            {
                scaling_list_syntax(scaling_list, sizeID, matrixID);
            }
            else
            {
                val = ue_v(s, "SL: scaling_list_pred_matrix_id_delta[%d][%d]", sizeID, matrixID);
                (*scaling_list)[sizeID][matrixID].scaling_list_pred_matrix_id_delta = val;
            }
        }
    }
}

void FrameParser_VideoHevc_c::ReadVUI(HevcVui_t *vui, uint8_t  max_temporal_layers)
{
    vui->aspect_ratio_info_present_flag = u_1(s,   "VUI: aspect_ratio_info_present_flag");

    if (vui->aspect_ratio_info_present_flag)
    {
        vui->aspect_ratio_idc = u_v(s, 8, "VUI: aspect_ratio_idc");

        if (vui->aspect_ratio_idc == 255)
        {
            vui->sar_width  = u_v(s, 16, "VUI: sar_width");
            vui->sar_height = u_v(s, 16, "VUI: sar_height");
        }
    }

    vui->overscan_info_present_flag = u_1(s,   "VUI: overscan_info_present_flag");

    if (vui->overscan_info_present_flag)
    {
        vui->overscan_appropriate_flag = u_1(s, "VUI: overscan_appropriate_flag");
    }

    vui->video_signal_type_present_flag = u_1(s,   "VUI: video_signal_type_present_flag");

    if (vui->video_signal_type_present_flag)
    {
        vui->video_format = u_v(s, 3, "VUI: video_format");
        vui->video_full_range_flag = u_1(s,   "VUI: video_full_range_flag");
        vui->colour_description_present_flag = u_1(s,   "VUI: colour_description_present_flag");

        if (vui->colour_description_present_flag)
        {
            vui->colour_primaries = u_v(s, 8, "VUI: colour_primaries");
            vui->transfer_characteristics = u_v(s, 8, "VUI: transfer_characteristics");
            vui->matrix_coefficients = u_v(s, 8, "VUI: matrix_coefficients");
        }
    }

    vui->chroma_loc_info_present_flag = u_1(s,   "VUI: chroma_loc_info_present_flag");

    if (vui->chroma_loc_info_present_flag)
    {
        vui->chroma_sample_loc_type_top_field = ue_v(s,   "VUI: chroma_sample_loc_type_top_field");
        vui->chroma_sample_loc_type_bottom_field = ue_v(s,   "VUI: chroma_sample_loc_type_bottom_field");
    }

    vui->neutral_chroma_indication_flag = u_1(s,   "VUI: neutral_chroma_indication_flag");
    vui->field_seq_flag = u_1(s,   "VUI: field_seq_flag");
    vui->frame_field_info_present_flag = u_1(s,   "VUI: frame_field_info_present_flag");
    vui->default_display_window_flag = u_1(s,   "VUI: default_display_window_flag");

    if (vui->default_display_window_flag)
    {
        vui->def_disp_win_left_offset = ue_v(s, "VUI: def_disp_win_left_offset");
        vui->def_disp_win_right_offset = ue_v(s, "VUI: def_disp_win_right_offset");
        vui->def_disp_win_top_offset = ue_v(s, "VUI: def_disp_win_top_offset");
        vui->def_disp_win_bottom_offset = ue_v(s, "VUI: def_disp_win_bottom_offset");
    }

    vui->timing_info_present_flag = u_1(s, "VUI: vui_timing_info_present_flag");

    if (vui->timing_info_present_flag)
    {
        vui->num_units_in_tick = u_v(s,  32, "VUI: vui_num_units_in_tick");
        vui->time_scale = u_v(s,  32, "VUI: vui_time_scale");
        vui->poc_proportional_to_timing_flag = u_1(s, "VUI: vui_poc_proportional_to_timing_flag");

        if (vui->poc_proportional_to_timing_flag)
        {
            vui->num_ticks_poc_diff_one_minus1 = ue_v(s, "VUI: vui_num_ticks_poc_diff_one_minus1");
        }

        vui->hrd_parameters_present_flag  = u_1(s, "VUI: hrd_parameters_present_flag");

        if (vui->hrd_parameters_present_flag)
        {
            ReadHRD(&vui->hrd, true, max_temporal_layers);
        }
    }

    vui->bitstream_restriction_flag = u_1(s,   "VUI: bitstream_restriction_flag");

    if (vui->bitstream_restriction_flag)
    {
        vui->tiles_fixed_structure_flag = u_1(s,   "VUI: tiles_fixed_structure_flag");
        vui->motion_vectors_over_pic_boundaries_flag = u_1(s,   "VUI: motion_vectors_over_pic_boundaries_flag");
        vui->restricted_ref_pic_lists_flag          = u_1(s,   "VUI: restricted_ref_pic_lists_flag");
        vui->min_spatial_segmentation_idc = ue_v(s, "VUI: min_spatial_segmentation_idc");

        if (vui->min_spatial_segmentation_idc >= 4096)
        {
            SE_ERROR("VUI: min_spatial_segmentation_idc value exceeds maximum\n");
        }

        vui->max_bytes_per_pic_denom = ue_v(s,   "VUI: max_bytes_per_pic_denom");
        vui->max_bits_per_mincu_denom = ue_v(s,   "VUI: max_bits_per_mincu_denom");
        vui->log2_max_mv_length_horizontal = ue_v(s,   "VUI: log2_max_mv_length_horizontal");
        vui->log2_max_mv_length_vertical = ue_v(s,   "VUI: log2_max_mv_length_vertical");
    }
}

// fill sps with content of p
FrameParserStatus_t FrameParser_VideoHevc_c::InterpretSPS(HevcSequenceParameterSet_t *sps, int32_t max_level_idc)
{
    bool sps_sub_layer_ordering_info_present_flag;
    uint32_t i, tmp;
    uint32_t max_coding_block_size;
    uint8_t  log2_diff_max_min_coding_block_size;

    assert(sps != 0);
    sps->is_valid = false;
    sps->sps_video_parameter_set_id = u_v(s, 4, "SPS: sps_video_parameter_set_id");

    if (sps->sps_video_parameter_set_id >= HEVC_STANDARD_MAX_VIDEO_PARAMETER_SETS)
    {
        SE_ERROR("SPS: sps_video_parameter_set_id (%d) exceeds maximum (%d)\n", sps->sps_video_parameter_set_id, HEVC_STANDARD_MAX_VIDEO_PARAMETER_SETS - 1);
        sps->sps_video_parameter_set_id = 0;  /* Recover from Error : set id to zero */
    }

    sps->sps_max_sub_layers = 1 + u_v(s, 3, "SPS: sps_max_sub_layers_minus1");
    tmp                     = u_1(s,   "SPS: sps_temporal_id_nesting_flag");

    if (sps->sps_max_sub_layers == 1 && tmp == 0)
    {
        SE_ERROR("SPS: temporal_id_nesting_flag must be 1 when sps_max_sub_layers=1\n");
        /* Not Recoverable*/
        return FrameParserError;
    }

    sps->level_idc = parsePTL(true, sps->sps_max_sub_layers - 1);

    if ((sps->level_idc == 0) || (sps->level_idc > max_level_idc))
    {
        SE_ERROR("SPS: level_idc is 0\n"); // , fallback to max level %d\n", max_level_idc);
        sps->level_idc = max_level_idc; /* ERC: Recover & Continue */
    }

    sps->sps_seq_parameter_set_id = ue_v(s,   "SPS: sps_seq_parameter_set_id");

    if (sps->sps_seq_parameter_set_id >= HEVC_STANDARD_MAX_SEQUENCE_PARAMETER_SETS)
    {
        SE_ERROR("SPS: seq_parameter_set_id (%d) exceeds maximum (%d)\n", sps->sps_seq_parameter_set_id, HEVC_STANDARD_MAX_SEQUENCE_PARAMETER_SETS - 1);
        /* Not Recoverable*/
        return FrameParserError;
    }

    sps->chroma_format_idc              = ue_v(s,   "SPS: chroma_format_idc");

    if (sps->chroma_format_idc == 3)
    {
        sps->separate_colour_plane_flag = u_1(s,   "SPS: separate_colour_plane_flag");
    }
    else
    {
        sps->separate_colour_plane_flag = false;
    }

    if (sps->chroma_format_idc != 1)
    {
        sps->chroma_format_idc = 1;
        sps->separate_colour_plane_flag = false;
        SE_ERROR("SPS: chroma_format_idc value (%d) not supported\n", sps->chroma_format_idc);
    }

    sps->pic_width_in_luma_samples              = ue_v(s,   "SPS: pic_width_in_luma_samples");

    if (sps->pic_width_in_luma_samples > HEVC_STANDARD_MAX_PIC_WIDTH_IN_LUMA_SAMPLE)
    {
        SE_ERROR("SPS: pic_width_in_luma_samples exceeds maximum\n");
        /* Not Recoverable*/
        return FrameParserError;
    }

    sps->pic_height_in_luma_samples             = ue_v(s,   "SPS: pic_height_in_luma_samples");

    if (sps->pic_height_in_luma_samples > HEVC_STANDARD_MAX_PIC_HEIGHT_IN_LUMA_SAMPLE)
    {
        SE_ERROR("SPS: pic_height_in_luma_samples exceeds maximum\n");
        /* Not Recoverable*/
        return FrameParserError;
    }

    tmp                                         = u_1(s,   "SPS: conformance_window_flag");

    if (tmp > 0)
    {
        sps->conf_win_left_offset               = ue_v(s,   "SPS: conf_win_left_offset");
        sps->conf_win_right_offset              = ue_v(s,   "SPS: conf_win_right_offset");

        if (2 * (sps->conf_win_left_offset + sps->conf_win_right_offset) >= sps->pic_width_in_luma_samples)
        {
            SE_ERROR("SPS: horizontal cropped area is larger than the picture width\n");
            sps->conf_win_left_offset = 0;
            sps->conf_win_right_offset = 0;
        }

        sps->conf_win_top_offset                = ue_v(s,   "SPS: conf_win_top_offset");
        sps->conf_win_bottom_offset             = ue_v(s,   "SPS: conf_win_bottom_offset");

        if (2 * (sps->conf_win_top_offset + sps->conf_win_bottom_offset) >= sps->pic_height_in_luma_samples)
        {
            SE_ERROR("SPS: vertical cropped area is larger than the picture height\n");
            sps->conf_win_top_offset = 0;
            sps->conf_win_bottom_offset = 0;
        }
    }
    else
    {
        sps->conf_win_left_offset = sps->conf_win_right_offset = sps->conf_win_top_offset = sps->conf_win_bottom_offset = 0;
    }

    sps->bit_depth_luma_minus8                  = ue_v(s,   "SPS: bit_depth_luma_minus8");
    sps->bit_depth_chroma_minus8                = ue_v(s,   "SPS: bit_depth_chroma_minus8");
    if ((sps->bit_depth_luma_minus8 != 0) || (sps->bit_depth_chroma_minus8 != 0))
    {
        SE_ERROR("SPS: 10 bit bit_depth not supported");
        /* Not Recoverable*/
        return FrameParserError;
    }
    sps->log2_max_pic_order_cnt_lsb             = ue_v(s,   "SPS: log2_max_pic_order_cnt_lsb_minus4") + 4;

    if (sps->log2_max_pic_order_cnt_lsb > 16)
    {
        SE_ERROR("SPS: log2_max_pic_order_cnt_lsb (%d) exceeds maximum (16)\n", sps->log2_max_pic_order_cnt_lsb);
        sps->log2_max_pic_order_cnt_lsb = 16; /* ERC : Recover & continue */
    }

    sps_sub_layer_ordering_info_present_flag    = u_1(s,   "SPS: sps_sub_layer_ordering_info_present_flag");

    for (i = 0; i < sps->sps_max_sub_layers; i++)
    {
        sps->sps_max_dec_pic_buffering[i]       = ue_v(s,   "SPS: sps_max_dec_pic_buffering_minus1[%d]", i) + 1;
        sps->sps_num_reorder_pics[i]            = ue_v(s,   "SPS: sps_num_reorder_pics[%d]", i);
        sps->sps_max_latency_increase[i]        = ue_v(s,   "SPS: sps_max_latency_increase[%d]", i);

        if (!sps_sub_layer_ordering_info_present_flag)
        {
            for (i = 1; i < sps->sps_max_sub_layers; i++)
            {
                sps->sps_max_dec_pic_buffering[i] = sps->sps_max_dec_pic_buffering[0];
                sps->sps_num_reorder_pics[i]      = sps->sps_num_reorder_pics[0];
                sps->sps_max_latency_increase[i]  = sps->sps_max_latency_increase[0];
            }

            break;
        }
    }

    sps->log2_min_coding_block_size             = ue_v(s,   "SPS: log2_min_coding_block_size_minus3") + 3;

    if (sps->log2_min_coding_block_size > HEVC_LOG2_MAX_CU_SIZE)
    {
        SE_ERROR("SPS: log2_min_coding_block_size (%d) exceeds maximum (6)\n", sps->log2_min_coding_block_size);
        /* Not Recoverable*/
        return FrameParserError;
    }

    log2_diff_max_min_coding_block_size         = ue_v(s,   "SPS: log2_diff_max_min_coding_block_size");
    sps->log2_max_coding_block_size = sps->log2_min_coding_block_size + log2_diff_max_min_coding_block_size;

    if (sps->log2_max_coding_block_size > HEVC_LOG2_MAX_CU_SIZE)
    {
        SE_ERROR("SPS: log2_max_coding_block_size (%d) exceeds maximum (6)\n", sps->log2_max_coding_block_size);
        /* Not Recoverable*/
        return FrameParserError;
    }

    max_coding_block_size = 1 << sps->log2_max_coding_block_size;
    sps->log2_min_transform_block_size          = ue_v(s,   "SPS: log2_min_transform_block_size_minus2") + 2;

    if (sps->log2_min_transform_block_size > HEVC_LOG2_MAX_TU_SIZE)
    {
        SE_ERROR("SPS: log2_min_transform_block_size (%d) exceeds maximum (5)\n", sps->log2_min_transform_block_size);
        /* Not Recoverable*/
        return FrameParserError;
    }

    if (sps->log2_min_transform_block_size >= sps->log2_min_coding_block_size)
    {
        SE_ERROR("SPS: log2_min_coding_block_size (%d) must be greater than log2_min_transform_block_size (%d)\n"
                 , sps->log2_min_coding_block_size, sps->log2_min_transform_block_size);
        /* Not Recoverable*/
        return FrameParserError;
    }

    for (tmp = 0; (max_coding_block_size >> log2_diff_max_min_coding_block_size) > ((uint32_t)1 << (tmp + sps->log2_min_transform_block_size)); tmp++);

    sps->max_coding_block_depth = log2_diff_max_min_coding_block_size + tmp;
    sps->log2_max_transform_block_size = ue_v(s,   "SPS: log2_diff_max_min_transform_block_size");
    sps->log2_max_transform_block_size += sps->log2_min_transform_block_size;

    if (sps->log2_max_transform_block_size > HEVC_LOG2_MAX_TU_SIZE)
    {
        SE_ERROR("SPS: log2_max_transform_block_size (%d) exceeds maximum (5)\n", sps->log2_max_transform_block_size);
        /* Not Recoverable*/
        return FrameParserError;
    }

    sps->max_transform_hierarchy_depth_inter    = ue_v(s,   "SPS: max_transform_hierarchy_depth_inter");

    if (sps->max_transform_hierarchy_depth_inter > sps->max_coding_block_depth)
    {
        SE_ERROR("SPS: max_transform_hierarchy_depth_inter (%d) exceeds maximum (%d)\n",
                 sps->max_transform_hierarchy_depth_inter, sps->max_coding_block_depth);
        /* Not Recoverable*/
        return FrameParserError;
    }

    sps->max_transform_hierarchy_depth_intra    = ue_v(s,   "SPS: max_transform_hierarchy_depth_intra");

    if (sps->max_transform_hierarchy_depth_intra > sps->max_coding_block_depth)
    {
        SE_ERROR("SPS: max_transform_hierarchy_depth_intra (%d) exceeds maximum (%d)\n",
                 sps->max_transform_hierarchy_depth_intra, sps->max_coding_block_depth);
        /* Not Recoverable*/
        return FrameParserError;
    }

    sps->scaling_list_enable_flag               = u_1(s,   "SPS: scaling_list_enable_flag");

    if (sps->scaling_list_enable_flag)
    {
        sps->sps_scaling_list_data_present_flag = u_1(s,   "SPS: sps_scaling_list_data_present_flag");

        if (sps->sps_scaling_list_data_present_flag)
        {
            scaling_list_param(& sps->scaling_list);
        }
    }

    sps->amp_enabled_flag                       = u_1(s,   "SPS: amp_enabled_flag");
    sps->sample_adaptive_offset_enabled_flag    = u_1(s,   "SPS: sample_adaptive_offset_enabled_flag");
    sps->pcm_enabled_flag                       = u_1(s,   "SPS: pcm_enabled_flag");

    if (sps->pcm_enabled_flag)
    {
        sps->pcm_bit_depth_luma_minus1          = u_v(s, 4, "SPS: pcm_bit_depth_luma_minus1");

        if (sps->pcm_bit_depth_luma_minus1 >= sps->bit_depth_luma_minus8 + 8)
        {
            SE_ERROR("SPS: luma PCM bitdepth exceeds maximum\n");
        }

        sps->pcm_bit_depth_chroma_minus1        = u_v(s, 4, "SPS: pcm_bit_depth_chroma_minus1");

        if (sps->pcm_bit_depth_chroma_minus1 >= sps->bit_depth_chroma_minus8 + 8)
        {
            SE_ERROR("SPS: luma PCM bitdepth exceeds maximum\n");
            sps->pcm_bit_depth_chroma_minus1 = sps->bit_depth_chroma_minus8 + 7; /* ERC : Recover & Continue */
        }

        sps->log2_min_pcm_coding_block_size     = ue_v(s,   "SPS: log2_min_pcm_coding_block_size_minus3") + 3;

        if (sps->log2_min_pcm_coding_block_size > 7)
        {
            SE_ERROR("SPS: log2_min_pcm_coding_block_size (%d) exceeds maximum (7)\n", sps->log2_min_pcm_coding_block_size);
            sps->log2_min_pcm_coding_block_size = 7; /* ERC : Recover & Continue */
        }

        sps->log2_max_pcm_coding_block_size     = ue_v(s,   "SPS: log2_diff_max_min_pcm_coding_block_size") + sps->log2_min_pcm_coding_block_size;
        sps->pcm_loop_filter_disable_flag       = u_1(s,   "SPS: pcm_loop_filter_disable_flag");
    }

    sps->num_short_term_ref_pic_sets            = ue_v(s,   "SPS: num_short_term_ref_pic_sets");

    if (sps->num_short_term_ref_pic_sets > HEVC_MAX_SHORT_TERM_RPS)
    {
        SE_ERROR("SPS: Maximum number of RPS exceeded\n");
        /* Not Recoverable*/
        return FrameParserError;
    }

    for (i = 0; i < sps->num_short_term_ref_pic_sets; i++)
    {
        short_term_ref_pic_set(sps->st_rps + i, sps->st_rps, i, false);
    }

    tmp = 0;

    while ((1 << tmp) < sps->num_short_term_ref_pic_sets)
    {
        tmp++;
    }

    sps->log2_num_short_term_ref_pic_sets = tmp;
    sps->long_term_ref_pics_present_flag        = u_1(s,   "SPS: long_term_ref_pics_present_flag");
    sps->num_long_term_ref_pics_sps = 0;

    if (sps->long_term_ref_pics_present_flag)
    {
        sps->num_long_term_ref_pics_sps = ue_v(s, "SPS: num_long_term_ref_pics_sps");
        if (sps->num_long_term_ref_pics_sps > 32)
        {
            SE_ERROR("SPS: num_long_term_ref_pics_sps (%d) exceeds maximum (32)\n", sps->num_long_term_ref_pics_sps);
            /* Not Recoverable*/
            return FrameParserError;
        }
        tmp = 0;

        while ((1 << tmp) < sps->num_long_term_ref_pics_sps)
        {
            tmp++;
        }

        sps->log2_num_long_term_ref_pics_sps = tmp;

        for (i = 0; i < sps->num_long_term_ref_pics_sps; i++)
        {
            sps->lt_ref_pic_poc_lsb_sps[ i ]    = u_v(s, sps->log2_max_pic_order_cnt_lsb, "SPS: lt_ref_pic_poc_lsb_sps[%d]", i);
            sps->used_by_curr_pic_lt_sps_flag[ i ] =  u_1(s,   "SPS: used_by_curr_pic_lt_sps_flag[%d]", i);
        }
    }

    sps->sps_temporal_mvp_enabled_flag          = u_1(s,   "SPS: sps_temporal_mvp_enable_flag");
    sps->sps_strong_intra_smoothing_enable_flag = u_1(s,   "SPS: sps_strong_intra_smoothing_enable_flag");
    tmp                                         = u_1(s,   "SPS: vui_parameters_present_flag");

    if (tmp)
    {
        ReadVUI(&sps->vui, sps->sps_max_sub_layers);
    }

    tmp                                         = u_1(s,   "SPS: sps_extension_flag");

    if (tmp)
    {
        while (more_rbsp_data())
        {
            (void)u_1(s   , "SPS: sps_extension_data_flag");
        }
    }

#if BYTES_PER_PIXEL==1

    if (sps->bit_depth_luma_minus8 > 0 || sps->bit_depth_chroma_minus8 > 0)
    {
        SE_ERROR("Bitdepth not supported (luma:%d, chroma:%d)\n", sps->bit_depth_luma_minus8 + 8, sps->bit_depth_chroma_minus8 + 8);
    }

#endif
    tmp = (sps->pic_width_in_luma_samples + max_coding_block_size - 1) / max_coding_block_size;
    sps->pic_width_in_ctb = tmp;
    tmp  = (sps->pic_height_in_luma_samples + max_coding_block_size - 1) / max_coding_block_size;
    sps->pic_height_in_ctb = tmp;
    tmp *= sps->pic_width_in_ctb;
    sps->pic_size_in_ctb = tmp;

// TODO CL : this kind of check must be revisited to know how we'll import defines.h and where...
//    if(sps->pic_size_in_ctb>MAX_PIC_SIZE_IN_MIN_CTB_32x32)
//        SE_ERROR("SPS: picture size in number of CTB exceeds the maximum\n");
    for (sps->iReqBitsOuter = 0; tmp > ((uint32_t)1 << sps->iReqBitsOuter); sps->iReqBitsOuter++);

    sps->is_valid = true;
    return FrameParserNoError;
}

FrameParserStatus_t FrameParser_VideoHevc_c::InterpretPPS(HevcPictureParameterSet_t *pps)
{
    int32_t i, tmp1, tmp2;
    assert(pps != 0);
    pps->is_valid = false;

    pps->pps_pic_parameter_set_id = ue_v(s,  "PPS: pps_pic_parameter_set_id");
    if (pps->pps_pic_parameter_set_id >= HEVC_STANDARD_MAX_PICTURE_PARAMETER_SETS)
    {
        SE_ERROR("PPS: pps_pic_parameter_set_id (%d) exceeds maximum (%d)\n", pps->pps_pic_parameter_set_id, HEVC_STANDARD_MAX_PICTURE_PARAMETER_SETS - 1);
    }

    pps->pps_seq_parameter_set_id = ue_v(s,  "PPS: pps_seq_parameter_set_id");
    if (pps->pps_seq_parameter_set_id >= HEVC_STANDARD_MAX_SEQUENCE_PARAMETER_SETS)
    {
        SE_ERROR("PPS: pps_seq_parameter_set_id (%d) exceeds maximum (%d)\n", pps->pps_seq_parameter_set_id, HEVC_STANDARD_MAX_SEQUENCE_PARAMETER_SETS - 1);
        /* Not Recoverable*/
        return FrameParserError;
    }

    if (SequenceParameterSetTable[pps->pps_seq_parameter_set_id].SPS == NULL)
    {
        SE_ERROR("PPS: SPS Not found\n");
        /* Not Recoverable*/
        return FrameParserError;
    }

    pps->dependent_slice_segments_enabled_flag = u_1(s,  "PPS: dependent_slice_segments_enabled_flag");
    pps->output_flag_present_flag             = u_1(s,  "PPS: output_flag_present_flag");
    pps->num_extra_slice_header_bits          = u_v(s, 3, "PPS: num_extra_slice_header_bits");
    pps->sign_data_hiding_flag                = u_1(s,  "PPS: sign_data_hiding_flag");
    pps->cabac_init_present_flag              = u_1(s,  "PPS: cabac_init_present_flag");

    pps->num_ref_idx_l0_default_active        = ue_v(s,  "PPS: num_ref_idx_l0_default_active_minus1") + 1;
    if ((pps->num_ref_idx_l0_default_active < 1) || (pps->num_ref_idx_l0_default_active > 15))
    {
        SE_ERROR("PPS: num_ref_idx_l0_default_active (%d) exceeds limits (1-15)\n", pps->num_ref_idx_l0_default_active);
        pps->num_ref_idx_l0_default_active = Hevc_clip(pps->num_ref_idx_l0_default_active, 1, 15);
    }

    pps->num_ref_idx_l1_default_active        = ue_v(s,  "PPS: num_ref_idx_l1_default_active_minus1") + 1;
    if ((pps->num_ref_idx_l1_default_active < 1) || (pps->num_ref_idx_l1_default_active > 15))
    {
        SE_ERROR("PPS: num_ref_idx_l1_default_active (%d) exceeds limits (1-15)\n", pps->num_ref_idx_l1_default_active);
        pps->num_ref_idx_l1_default_active = Hevc_clip(pps->num_ref_idx_l1_default_active, 1, 15);
    }

    pps->init_qp_minus26 = se_v(s,  "PPS: init_qp_minus26");

    /* init_qp_minus26 limits :  8 bits: -26 to +25 inclusive
       10 bits: -38 to +25 inclusive, 10 bits case Not Supported in Hades1 */
    if ((pps->init_qp_minus26 < -26) || (pps->init_qp_minus26 > 25))
    {
        SE_ERROR("PPS: init_qp_minus26 (%d) exceeds limits (-25 to 25)\n", pps->init_qp_minus26);
        pps->init_qp_minus26 = Hevc_clip(pps->init_qp_minus26, -26, 25);
    }

    pps->constrained_intra_pred_flag          = u_1(s,  "PPS: constrained_intra_pred_flag");
    pps->transform_skip_enabled_flag          = u_1(s,  "PPS: transform_skip_enabled_flag");
    pps->cu_qp_delta_enabled_flag             = u_1(s,  "PPS: cu_qp_delta_enabled_flag");

    if (pps->cu_qp_delta_enabled_flag)
    {
        pps->diff_cu_qp_delta_depth           = ue_v(s,  "PPS: diff_cu_qp_delta_depth");
#if 0
        /* AA :  To be checked : not working for the moment */
        tmp1 = SequenceParameterSetTable[pps->pps_seq_parameter_set_id].SPS->log2_max_coding_block_size /
               - SequenceParameterSetTable[pps->pps_seq_parameter_set_id].SPS->log2_min_coding_block_size;

        if (pps->diff_cu_qp_delta_depth > tmp1)
        {
            SE_ERROR("PPS: diff_cu_qp_delta_depth (%d) exceeds limit %d \n", pps->init_qp_minus26, tmp1);
            pps->diff_cu_qp_delta_depth = tmp1;
        }
#endif
    }
    else
    {
        pps->diff_cu_qp_delta_depth = 0;
    }

    pps->pps_cb_qp_offset = se_v(s,  "PPS: pps_cb_qp_offset");
    if ((pps->pps_cb_qp_offset < -12) || (pps->pps_cb_qp_offset > 12))
    {
        SE_ERROR("PPS: pps_cb_qp_offset (%d) exceeds limits -12 -> 12\n", pps->pps_cb_qp_offset);
        pps->pps_cb_qp_offset = Hevc_clip(pps->pps_cb_qp_offset, -12, 12);
    }

    pps->pps_cr_qp_offset = se_v(s,  "PPS: pps_cr_qp_offset");
    if ((pps->pps_cr_qp_offset < -12) || (pps->pps_cr_qp_offset > 12))
    {
        SE_ERROR("PPS: pps_cr_qp_offset (%d) exceeds limits -12 -> 12\n", pps->pps_cr_qp_offset);
        pps->pps_cr_qp_offset = Hevc_clip(pps->pps_cr_qp_offset, -12, 12);
    }
    pps->pps_slice_chroma_qp_offsets_present_flag = u_1(s,  "PPS: pps_slice_chroma_qp_offsets_present_flag");
    pps->weighted_pred_flag[I_SLICE]          = false;
    pps->weighted_pred_flag[P_SLICE]          = u_1(s,  "PPS: weighted_pred_flag");
    pps->weighted_pred_flag[B_SLICE]          = u_1(s,  "PPS: weighted_bipred_flag");
    pps->transquant_bypass_enable_flag        = u_1(s,  "PPS: transquant_bypass_enable_flag");
    pps->tiles_enabled_flag                   = u_1(s,  "PPS: tiles_enabled_flag");
    pps->entropy_coding_sync_enabled_flag     = u_1(s,  "PPS: entropy_coding_sync_enabled_flag");

    if (pps->tiles_enabled_flag)
    {
        pps->num_tile_columns = ue_v(s,  "PPS: num_tile_columns_minus1") + 1;
        if (pps->num_tile_columns > HEVC_MAX_TILE_COLUMNS)
        {
            SE_ERROR("PPS: Number of tile columns (%d) exceeds maximum (%d)\n", pps->num_tile_columns, HEVC_MAX_TILE_COLUMNS);
            /* Not Recoverable*/
            return FrameParserError;
        }

        pps->num_tile_rows = ue_v(s,  "PPS: num_tile_rows_minus1") + 1;
        if (pps->num_tile_rows > HEVC_MAX_TILE_ROWS)
        {
            SE_ERROR("PPS: Number of tile rows (%d) exceeds maximum (%d)\n", pps->num_tile_rows, HEVC_MAX_TILE_ROWS);
            /* Not Recoverable*/
            return FrameParserError;
        }

        pps->uniform_spacing_flag = u_1(s,  "PPS: uniform_spacing_flag");
        tmp1 = tmp2 = 0;
        if (!pps->uniform_spacing_flag)
        {
            for (i = 0; i < pps->num_tile_columns - 1; i++)
            {
                pps->tile_width[i] = ue_v(s,  "PPS: column_width_minus1[%d]", i) + 1;
                tmp1 += pps->tile_width[i];
            }

            for (i = 0; i < pps->num_tile_rows - 1; i++)
            {
                pps->tile_height[i] = ue_v(s,  "PPS: row_height_minus1[%d]", i) + 1;
                tmp2 += pps->tile_height[i];
            }
        }

        if (tmp1 >= SequenceParameterSetTable[pps->pps_seq_parameter_set_id].SPS->pic_width_in_ctb)
        {
            SE_ERROR("PPS: Sum of tail width (%d) exceeds picture width in ctb (%d)\n",
                     tmp1, SequenceParameterSetTable[pps->pps_seq_parameter_set_id].SPS->pic_width_in_ctb);
        }

        if (tmp2 >= SequenceParameterSetTable[pps->pps_seq_parameter_set_id].SPS->pic_height_in_ctb)
        {
            SE_ERROR("PPS: Sum of tail height (%d) exceeds picture width in ctb (%d)\n",
                     tmp2, SequenceParameterSetTable[pps->pps_seq_parameter_set_id].SPS->pic_height_in_ctb);
        }

        pps->loop_filter_across_tiles_enabled_flag = u_1(s,  "PPS: loop_filter_across_tiles_enabled_flag");
        tmp1 = pps->num_tile_columns * pps->num_tile_rows;
        tmp2 = 0;

        while (tmp1)
        {
            tmp2++;
            tmp1 >>= 1;
        }

        pps->tile_index_size = tmp2;
    }
    else
    {
        pps->num_tile_columns = 1;
        pps->num_tile_rows = 1;
    }

    pps->pps_loop_filter_across_slices_enabled_flag = u_1(s,  "PPS: loop_filter_across_slices_enabled_flag");
    pps->deblocking_filter_control_present_flag = u_1(s,  "PPS: deblocking_filter_control_present_flag");
    pps->deblocking_filter_override_enabled_flag = false;
    pps->pps_deblocking_filter_disable_flag = false;
    pps->pps_beta_offset_div2 = 0;
    pps->pps_tc_offset_div2 = 0;

    if (pps->deblocking_filter_control_present_flag)
    {
        pps->deblocking_filter_override_enabled_flag = u_1(s,  "PPS: deblocking_filter_override_enabled_flag");
        pps->pps_deblocking_filter_disable_flag = u_1(s,  "PPS: pps_deblocking_filter_disable_flag");

        if (!pps->pps_deblocking_filter_disable_flag)
        {
            pps->pps_beta_offset_div2 = se_v(s,  "PPS: pps_beta_offset_div2");
            if ((pps->pps_beta_offset_div2 < -6) || (pps->pps_beta_offset_div2 > 6))
            {
                SE_ERROR("PPS: invalid pps_beta_offset_div2 value\n");
                pps->pps_beta_offset_div2 = Hevc_clip(pps->pps_beta_offset_div2, -6, 6);
            }

            pps->pps_tc_offset_div2 = se_v(s,  "PPS: pps_tc_offset_div2");
            if ((pps->pps_tc_offset_div2 < -6) || (pps->pps_tc_offset_div2 > 6))
            {
                SE_ERROR("PPS: invalid pps_tc_offset_div2 value\n");
                pps->pps_tc_offset_div2 = Hevc_clip(pps->pps_tc_offset_div2, -6, 6);
            }
        }
    }

    pps->pps_scaling_list_data_present_flag = u_1(s,  "PPS: pps_scaling_list_data_present_flag");
    if (pps->pps_scaling_list_data_present_flag)
    {
        scaling_list_param(& pps->scaling_list);
    }

    pps->lists_modification_present_flag        = u_1(s,  "PPS: lists_modification_present_flag");
    pps->log2_parallel_merge_level_minus2       = ue_v(s,  "PPS: log2_parallel_merge_level_minus2");
    if (pps->log2_parallel_merge_level_minus2 > SequenceParameterSetTable[pps->pps_seq_parameter_set_id].SPS->log2_max_coding_block_size)
    {
        SE_ERROR("PPS: invalid log2_parallel_merge_level_minus2 value\n");
        pps->log2_parallel_merge_level_minus2 = SequenceParameterSetTable[pps->pps_seq_parameter_set_id].SPS->log2_max_coding_block_size;
    }

    pps->slice_segment_header_extension_flag    = u_1(s,  "PPS: slice_segment_header_extension_flag");
    tmp1                                        = u_1(s,  "PPS: pps_extension_flag");

    if (tmp1)
    {
        while (more_rbsp_data())
        {
            (void)u_1(s   , "PPS: pps_extension_data_flag");
        }
    }

    pps->is_valid = true;
    return FrameParserNoError;
}

/*!
 ************************************************************************
 * \brief
 *    Parse a SPS, and store it at its specified sps_id slot
 ************************************************************************
 */
FrameParserStatus_t FrameParser_VideoHevc_c::ProcessSPS()
{
    Buffer_t TmpBuffer;
    HevcSequenceParameterSet_t *sps;
    FrameParserStatus_t Status;
    unsigned int sps_id;

    // Get a new buffer
    BufferStatus_t BufStatus = SequenceParameterSetPool->GetBuffer(&TmpBuffer);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to get a sequence parameter set buffer (status:%d)\n", BufStatus);
        return FrameParserError;
    }

    TmpBuffer->ObtainDataReference(NULL, NULL, (void **)(&sps));
    if (sps == NULL)
    {
        TmpBuffer->DecrementReferenceCount();
        SE_ERROR("ObtainDataReference failed\n");
        return FrameParserError;
    }

    memset(sps, 0, sizeof(HevcSequenceParameterSet_t));
    // Read SPS
    Status = InterpretSPS(sps, HADES_MAX_LEVEL_IDC);
    if (Status != FrameParserNoError)
    {
        SE_ERROR("Non Recoverable Error in InterpretSPS (status:%d)\n", Status);
        TmpBuffer->DecrementReferenceCount();
        return FrameParserError;
    }

    sps_id = sps->sps_seq_parameter_set_id;
    if (sps_id >= HEVC_STANDARD_MAX_SEQUENCE_PARAMETER_SETS)
    {
        TmpBuffer->DecrementReferenceCount();
        SE_ERROR("sps_id wrong value\n");
        return FrameParserError;
    }

    // Set to active is same as previous SPS
    if ((SequenceParameterSetTable[sps_id].Buffer != NULL) &&
        SequenceParameterSetTable[sps_id].SPS->is_valid &&
        SequenceParameterSetTable[sps_id].SPS->is_active)
    {
        // skip is_valid & is_active
        unsigned int header = sizeof(sps->is_valid) + sizeof(sps->is_active);

        if (memcmp((char *)sps + header, (char *)SequenceParameterSetTable[sps_id].SPS + header,
                   sizeof(*sps) - header) == 0)
        {
            sps->is_active = true;
        }
        // else -> copy should occur in CommitFrameForDecode
    }

    // If we already have a buffer with the same SPS ID, then release it
    if (SequenceParameterSetTable[sps_id].Buffer != NULL)
    {
        SequenceParameterSetTable[sps_id].Buffer->DecrementReferenceCount();
    }

    // Save buffer and pointer
    sps->is_valid = true;
    sps->is_active = false;
    SequenceParameterSetTable[sps_id].Buffer = TmpBuffer;
    SequenceParameterSetTable[sps_id].SPS = sps;
    ReadNewSPS = true;
    return FrameParserNoError;
}

/*!
 ************************************************************************
 * \brief
 *    Parse a PPS, and store it at its specified pps_id slot
 ************************************************************************
 */
FrameParserStatus_t FrameParser_VideoHevc_c::ProcessPPS()
{
    Buffer_t TmpBuffer;
    HevcPictureParameterSet_t *pps;
    FrameParserStatus_t Status;
    unsigned int pps_id;

    // Get a new buffer
    BufferStatus_t BufStatus = PictureParameterSetPool->GetBuffer(&TmpBuffer);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to get a picture parameter set buffer (status:%d)\n", BufStatus);
        return FrameParserError;
    }

    TmpBuffer->ObtainDataReference(NULL, NULL, (void **)(&pps));
    if (!pps)
    {
        TmpBuffer->DecrementReferenceCount();
        SE_ERROR("ObtainDataReference failed\n");
        return FrameParserError;
    }

    memset(pps, 0, sizeof(*pps));
    // Read PPS
    Status = InterpretPPS(pps);
    if (Status != FrameParserNoError)
    {
        SE_ERROR("Non Recoverable Error in InterpretPPS (status:%d)\n", Status);
        TmpBuffer->DecrementReferenceCount();
        return FrameParserError;
    }
    pps_id = pps->pps_pic_parameter_set_id;
    if (pps_id >= HEVC_STANDARD_MAX_PICTURE_PARAMETER_SETS)
    {
        TmpBuffer->DecrementReferenceCount();
        SE_ERROR("pps_id wrong value\n");
        return FrameParserError;
    }

    // If we already have a buffer with the same PPS ID, then release it
    if (PictureParameterSetTable[pps_id].Buffer != NULL)
    {
        PictureParameterSetTable[pps_id].Buffer->DecrementReferenceCount();
    }

    // Save buffer and pointer
    PictureParameterSetTable[pps_id].Buffer = TmpBuffer;
    PictureParameterSetTable[pps_id].PPS = pps;
    ReadNewPPS = true;
    return FrameParserNoError;
}

#define MD5 0
#define SEI_MD5_SIZE  1+NUM_COMPONENTS*16

FrameParserStatus_t FrameParser_VideoHevc_c::ParseSeiPictureDigest(uint8_t digest[NUM_COMPONENTS][16], int32_t size)
{
    uint8_t i, j, method;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  " :-\n");
#endif

    if (size != SEI_MD5_SIZE)
    {
#ifdef DUMP_HEADERS
        SE_INFO(group_frameparser_video,  " PayloadSize(%d) ! %d\n", size, SEI_MD5_SIZE);
#endif
        return FrameParserNoError;
    }

    method = u_v(bitstream, 8, "payload_byte");

    if (method != MD5)
    {
#ifdef DUMP_HEADERS
        SE_INFO(group_frameparser_video,  " Method(%d) ! MD5\n", method);
#endif
        return FrameParserNoError;
    }

    for (j = 0; j < NUM_COMPONENTS; j++)
    {
        for (i = 0; i < 16; i++)
        {
            digest[j][i] = u_v(bitstream, 8, "payload_byte");
        }

        trace(9, "MD5[%d] : 0x", j);

        for (i = 0; i < 16; i++)
        {
            trace(0, "%02x", digest[j][i]);
        }

        trace(0, "\n");
    }
    has_picture_digest = true;

    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in user data Registered ITU T T35 message
//

FrameParserStatus_t   FrameParser_VideoHevc_c::ReadSeiUserDataRegisteredITUTT35Message(unsigned int       PayloadSize)
{
    unsigned char       *Pointer;
    unsigned int    BitsInByte;
    FrameParserStatus_t Status = FrameParserNoError;

    memset(&mUserData, 0, sizeof(mUserData));
    mUserData.Is_registered |= 1; // Set "is_registered" flag
    Bits.GetPosition(&Pointer, &BitsInByte);
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "UserData Registered ITU T T35 Message :-\n");
#endif
    // Parse Itu_t_t35_country_code, Itu_t_t35_country_code_extension_byte and Itu_t_t35_provider_code values
    mUserData.Itu_t_t35_country_code = GetBits(parser, 8);

    if (mUserData.Itu_t_t35_country_code == 0xff)
    {
        mUserData.Itu_t_t35_country_code_extension_byte = GetBits(parser, 8);
    }

    mUserData.Itu_t_t35_provider_code = GetBits(parser, 16);
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Itu_t_t35_country_code = %2d Itu_t_t35_country_code_extension_byte = %2d Itu_t_t35_provider_code = %d\n",
            mUserData.Itu_t_t35_country_code, mUserData.Itu_t_t35_country_code_extension_byte, mUserData.Itu_t_t35_provider_code);
#endif
    Status   = ReadUserData(PayloadSize, Pointer);

    AssertAntiEmulationOk();

    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in user data Unregistered message
//

FrameParserStatus_t   FrameParser_VideoHevc_c::ReadSeiUserDataUnregisteredMessage(unsigned int        PayloadSize)
{
    unsigned char       *Pointer;
    unsigned int    BitsInByte;
    unsigned int i;
    FrameParserStatus_t Status = FrameParserNoError;

    memset(&mUserData, 0, sizeof(mUserData));
    Bits.GetPosition(&Pointer, &BitsInByte);
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  " UserData Unregistered Message :-\n");
    SE_INFO(group_frameparser_video,  " uuid_iso_iec_11578                          = ");
#endif

    for (i = 0; i < 16; i++)
    {
        mUserData.uuid_iso_iec_11578[i] = GetBits(parser, 8);
#ifdef DUMP_HEADERS
        SE_INFO(group_frameparser_video,  "%x ", mUserData.uuid_iso_iec_11578[i]);
#endif
    }

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "\n");
#endif
    Status   = ReadUserData(PayloadSize, Pointer);

    AssertAntiEmulationOk();

    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read SEI
//

FrameParserStatus_t   FrameParser_VideoHevc_c::ReadSeiPayload(
    unsigned int          PayloadType,
    unsigned int          PayloadSize,
    uint8_t nal_unit_type)
{
    /* prefix sei */
    if (nal_unit_type == NAL_UNIT_PREFIX_SEI)
    {
#ifdef DUMP_HEADERS
        SE_INFO(group_frameparser_video,  "NAL_UNIT_PREFIX_SEI\n");
#endif
        switch (PayloadType)
        {

        case SEI_USER_DATA_REGISTERED_ITU_T_T35:
            return ReadSeiUserDataRegisteredITUTT35Message(PayloadSize);

        case SEI_USER_DATA_UNREGISTERED:
            return ReadSeiUserDataUnregisteredMessage(PayloadSize);

        case SEI_PIC_TIMING:
        case SEI_PAN_SCAN_RECT:
        case SEI_FRAME_PACKING_ARRANGEMENT:
        case SEI_BUFFERING_PERIOD:
        case SEI_FILLER_PAYLOAD:
        case SEI_RECOVERY_POINT:
        case SEI_SCENE_INFO:
        case SEI_FULL_FRAME_SNAPSHOT:
        case SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START:
        case SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END:
        case SEI_FILM_GRAIN_CHARACTERISTICS:
        case SEI_POST_FILTER_HINT:
        default:
#ifdef DUMP_HEADERS
            SE_INFO(group_frameparser_video,  "Unhandled payload type (type = %d - size = %d)\n", PayloadType, PayloadSize);
#endif
            break;
        }
    }
    else
    {
        /* suffix sei */
        SE_ASSERT(nal_unit_type == NAL_UNIT_SUFFIX_SEI);
#ifdef DUMP_HEADERS
        SE_INFO(group_frameparser_video,  "NAL_UNIT_SUFFIX_SEI\n");
#endif
        switch (PayloadType)
        {

        case SEI_USER_DATA_REGISTERED_ITU_T_T35:
            return ReadSeiUserDataRegisteredITUTT35Message(PayloadSize);

        case SEI_USER_DATA_UNREGISTERED:
            return ReadSeiUserDataUnregisteredMessage(PayloadSize);

        case SEI_PICTURE_DIGEST:
            return   ParseSeiPictureDigest(picture_digest, PayloadSize);

        case SEI_FILLER_PAYLOAD:
        case SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END:
        case SEI_POST_FILTER_HINT:

        default:
#ifdef DUMP_HEADERS
            SE_INFO(group_frameparser_video,  "Unhandled payload type (type = %d - size = %d)\n", PayloadType, PayloadSize);
#endif
            break;
        }
    }
    return FrameParserNoError;
}


/*!
 ************************************************************************
 * \brief
 *    Parse a SEI message, decode MD5 sum if present
 ************************************************************************
 */
FrameParserStatus_t FrameParser_VideoHevc_c::ProcessSEI(unsigned int   UnitLength, uint8_t nal_unit_type)
{
    int32_t payload_type;
    int32_t payload_size;
    uint8_t tmp_byte;
    unsigned char       *Pointer;
    unsigned int         BitsInByte;
    FrameParserStatus_t  Status;
    unsigned int         ConsumedSize;
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video, "Process SEI :- \n");
#endif

    ConsumedSize = 0;
    do
    {
        payload_type = 0;
        do
        {
            tmp_byte = u_v(buf, 8, "payload_type_byte");
            payload_type += tmp_byte;
            ConsumedSize++;
        }
        while (tmp_byte == 0xFF);
        trace(9, "payload_type = %d\n", payload_type);

        payload_size = 0;
        do
        {
            tmp_byte = u_v(buf, 8, "payload_size_byte");
            payload_size += tmp_byte;
            ConsumedSize++;
        }
        while (tmp_byte == 0xFF);

        trace(9, "payload_size = %d\n", payload_size);
#ifdef DUMP_HEADERS
        SE_INFO(group_frameparser_video, "SEI (Payload type = %d  size = %d) \n", payload_type, payload_size);
#endif
        if (payload_size > (mAntiEmulationBufferMaxSize - 32))
        {
            SE_ERROR("Payload size outside supported range (type = %d - size = %d)\n", payload_type, payload_size);
            return FrameParserError;
        }
        ConsumedSize += payload_size;

        // Check Payload size does not take us beyond UnitLength
        if (ConsumedSize > UnitLength)
        {
            SE_ERROR("Payload would consume more data than is present (type = %d - size = %d) Unit Length %d\n", payload_type, payload_size, UnitLength);
            break;
        }

        CheckAntiEmulationBuffer(payload_size + 4);

        Bits.GetPosition(&Pointer, &BitsInByte);
        Status = ReadSeiPayload(payload_type, payload_size, nal_unit_type);

        if (Status != FrameParserNoError)
        {
            SE_ERROR("ReadSeiPayload returned with Error \n");
            return Status;
        }

        Bits.SetPointer(Pointer + payload_size);

    }
    while (more_rbsp_data());

    return FrameParserNoError;
}

bool FrameParser_VideoHevc_c::parse_start_of_slice()
{
    int32_t val;
    uint8_t temporal_id, slice_pic_parameter_set_id, nal_unit_type;
    bool    idr_flag, irap_flag, reference_flag, first_slice_segment_in_pic_flag;
    bool    no_rasl_output = false, rasl_flag = false;
    // Parse first byte
    val = GetBits(parser, 8) >> 1;
    nal_unit_type = val & 0x3f;
    reference_flag = false;
    irap_flag = false;
    idr_flag = false;

    switch (nal_unit_type)
    {
    case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
    case NAL_UNIT_CODED_SLICE_IDR_N_LP:
        mDecodeStartedOnRAP = false;
        idr_flag = true;

    // fallthrough
    case NAL_UNIT_CODED_SLICE_BLA_W_LP:
    case NAL_UNIT_CODED_SLICE_BLA_N_LP:
    case NAL_UNIT_CODED_SLICE_BLA_W_RADL:
        no_rasl_output = !idr_flag; // HM/spec mismatch : should be true regardless of idr_flag

    case NAL_UNIT_CODED_SLICE_CRA:
        irap_flag = true;
        reference_flag = true;
        break;

    case NAL_UNIT_CODED_SLICE_TRAIL_R:
    case NAL_UNIT_CODED_SLICE_TLA_R:
    case NAL_UNIT_CODED_SLICE_STSA_R:
    case NAL_UNIT_CODED_SLICE_RADL_R:
        reference_flag = true;

    // fallthrough
    case NAL_UNIT_CODED_SLICE_TRAIL_N:
    case NAL_UNIT_CODED_SLICE_TSA_N:
    case NAL_UNIT_CODED_SLICE_STSA_N:
    case NAL_UNIT_CODED_SLICE_RADL_N:
        mDecodeStartedOnRAP = false;
        break;

    case NAL_UNIT_CODED_SLICE_RASL_R:
        reference_flag = true;

    // fallthrough
    case NAL_UNIT_CODED_SLICE_RASL_N:
        rasl_flag = true;
        break;

    default:
        SE_ERROR("SH: unexpected nal_unit_type (%d)\n", nal_unit_type & 0x3f);
        return false; /* ERC : Skip Slice and Continue */
    }

    // Parse second byte
    val = GetBits(parser, 8);
    temporal_id = (val & 7) - 1;
    first_slice_segment_in_pic_flag = u_1(parser, "SH: first_slice_segment_in_pic_flag");

    if (irap_flag)
    {
        (void)u_1(parser, "SH: no_output_of_prior_pics_flag");  // no_output_of_prior_pics_flag
    }

    slice_pic_parameter_set_id = ue_v(parser, "SH: slice_pic_parameter_set_id");

    if (slice_pic_parameter_set_id > 64)
    {
        SE_ERROR("SH: unexpected slice_pic_parameter_set_id (%d)\n", slice_pic_parameter_set_id);
        return false; /* ERC : Skip Slice and Continue */
    }

    if (first_slice_segment_in_pic_flag)
    {
        SliceHeader->idr_flag = idr_flag;
        SliceHeader->nal_unit_type = nal_unit_type;
        SliceHeader->rap_flag = irap_flag;
        SliceHeader->rasl_flag = rasl_flag;
        SliceHeader->temporal_id = temporal_id;
        SliceHeader->reference_flag = reference_flag;

        if (irap_flag)
        {
            no_output_of_rasl_pics_flag = no_rasl_output;
        }

        pic_parameter_set_id = slice_pic_parameter_set_id;
    }

    return first_slice_segment_in_pic_flag;
}

void FrameParser_VideoHevc_c::parse_dependent_slice_header(
    bool      *first_slice_segment_in_pic_flag,
    bool      *dependent_slice_segment_flag,
    uint32_t  *slice_segment_address)
{
    *first_slice_segment_in_pic_flag = parse_start_of_slice();

    if (*first_slice_segment_in_pic_flag)
    {
        *slice_segment_address = 0;
        *dependent_slice_segment_flag = false;
    }
    else
    {
        if (PictureParameterSetTable[pic_parameter_set_id].PPS == NULL)
        {
            if (!FirstDecodeAfterInputJump)
            {
                SE_ERROR("No appropriate picture parameter set seen\n");
            }
            return;
        }

        if (mSPS == NULL)
        {
            if (!FirstDecodeAfterInputJump)
            {
                SE_ERROR("No appropriate sequence parameter set seen\n");
            }
            return;
        }

        if (PictureParameterSetTable[pic_parameter_set_id].PPS->dependent_slice_segments_enabled_flag)
        {
            *dependent_slice_segment_flag = u_1(bitstream, "SH: dependent_slice_segment_flag");
        }
        else
        {
            *dependent_slice_segment_flag = false;
        }

        if (*first_slice_segment_in_pic_flag && *dependent_slice_segment_flag)
        {
            SE_ERROR("Inappropriate dependent_slice_segment_flag\n");
            *dependent_slice_segment_flag = false; /* ERC : Recover & Continue */
        }

        *slice_segment_address = u_v(bitstream, mSPS->iReqBitsOuter, "SH: slice_segment_address");

        if (*slice_segment_address == 0)
        {
            SE_ERROR("SH: slice_segment_address has forbidden value 0");
            return;
        }
    }
}

/*!
 ************************************************************************
 * \brief
 *    Parse the syntax elements of slice_header down to RPS data
 ************************************************************************
 */
FrameParserStatus_t FrameParser_VideoHevc_c::FirstPartOfSliceHeader(bool *first_slice_segment_in_pic_flag)

{
    bool dependent_slice_segment_flag;
    uint32_t local_pic_order_cnt_lsb, slice_segment_address;
    uint8_t num_long_term_sps;
    uint8_t num_long_term_pics;
    uint32_t local_delta_poc_msb_cycle_lt;
    int32_t val, i;
    bool local_pic_output_flag;

    parse_dependent_slice_header(first_slice_segment_in_pic_flag, &dependent_slice_segment_flag, &slice_segment_address);
    if (!(*first_slice_segment_in_pic_flag))
    {
        // no need for parsing all slices, the first one is enough to populate frame parser structures
        return FrameParserNoError;
    }


    if (dependent_slice_segment_flag)
    {
        SE_ERROR("SH: dependent_slice_segment_flag must be 0 when first_slice_segment_in_pic_flag is 1");
        dependent_slice_segment_flag = 0; /* ERC : Recover & Continue */
    }

    mPPS = PictureParameterSetTable[pic_parameter_set_id].PPS;
    if (mPPS == NULL)
    {
        if (!FirstDecodeAfterInputJump)
        {
            SE_ERROR("No appropriate picture parameter set seen\n");
        }
        return FrameParserError;
    }
    if (mPPS->pps_pic_parameter_set_id != pic_parameter_set_id)
    {
        SE_ERROR("No appropriate picture parameter set seen\n");
        return FrameParserError;
    }

    mSPS = SequenceParameterSetTable[mPPS->pps_seq_parameter_set_id].SPS;
    if (mSPS == NULL)
    {
        if (!FirstDecodeAfterInputJump)
        {
            SE_ERROR("No appropriate sequence parameter set seen\n");
        }
        return FrameParserError;
    }
    if (mSPS->sps_seq_parameter_set_id != mPPS->pps_seq_parameter_set_id)
    {
        return FrameParserError;
    }

    mVPS = VideoParameterSetTable[mSPS->sps_video_parameter_set_id].VPS;
    for (i = 0; i < mPPS->num_extra_slice_header_bits; i++)
    {
        val = u_1(bitstream, "SH: slice_reserved_undetermined_flag[%d]", i);
    }

    val = ue_v(bitstream, "SH: slice_type");
    trace(9, "slice_type = %c_SLICE\n", "BPI"[val]);
    slice_types |= 1 << val;

    if (mPPS->output_flag_present_flag)
    {
        local_pic_output_flag = u_1(bitstream, "SH: pic_output_flag");
    }
    else
    {
        local_pic_output_flag = true;
    }

    if (*first_slice_segment_in_pic_flag)
    {
        if (SliceHeader->rasl_flag && no_output_of_rasl_pics_flag)
        {
            local_pic_output_flag = false;
        }

        pic_output_flag = local_pic_output_flag;
    }
    /* NOTE CL deadcode, FirstPartOfSliceHeader must be reworked according to latest STHM' after bringup
        else if (pic_output_flag != local_pic_output_flag)
        {
            SE_ERROR("SH: pic_output_flag does not match the value of the first slice segment");
        }
    */
    if (SliceHeader->idr_flag)
    {
        if (val != I_SLICE)
        {
            SE_ERROR("SH: slice type must be INTRA in an IDR picture");
            val = I_SLICE; /* ERC : Recover & Continue */
        }

        lt_rps.num_pics = 0;
        pic_order_cnt_lsb = 0;
        return FrameParserNoError;
    }

    local_pic_order_cnt_lsb = u_v(bitstream, mSPS->log2_max_pic_order_cnt_lsb, "SH: pic_order_cnt_lsb");

    if (*first_slice_segment_in_pic_flag)
    {
        pic_order_cnt_lsb = local_pic_order_cnt_lsb;
    }
    /* NOTE CL deadcode, FirstPartOfSliceHeader must be reworked according to latest STHM' after bringup
        else if (pic_order_cnt_lsb != local_pic_order_cnt_lsb)
        {
            SE_ERROR("SH: pic_order_cnt_lsb does not match the value of the first slice segment");
        }
    */
    val = u_1(bitstream, "SH: short_term_ref_pic_set_sps_flag");

    if (val)
    {
        if (mSPS->log2_num_short_term_ref_pic_sets > 0)
        {
            short_term_ref_pic_set_idx = u_v(bitstream, mSPS->log2_num_short_term_ref_pic_sets, "SH: short_term_ref_pic_set_idx");
        }
        else
        {
            short_term_ref_pic_set_idx = 0;
        }
    }
    else
    {
        // Read the short term RPS defined at slice level
        short_term_ref_pic_set(&(st_rps), mSPS->st_rps, mSPS->num_short_term_ref_pic_sets, true);
        short_term_ref_pic_set_idx = mSPS->num_short_term_ref_pic_sets;
    }

    if (mSPS->long_term_ref_pics_present_flag)
    {
        if (mSPS->num_long_term_ref_pics_sps > 0)
        {
            num_long_term_sps = ue_v(bitstream, "SH: num_long_term_sps");
        }
        else
        {
            num_long_term_sps = 0;
        }
        if (num_long_term_sps > mSPS->num_long_term_ref_pics_sps)
        {
            SE_ERROR("SH: num_long_term_sps not in range !\n");
            return FrameParserError;; /* Not Recoverable */
        }

        num_long_term_pics = ue_v(bitstream, "SH: num_long_term_pics");
        if (((num_long_term_pics + num_long_term_sps) < 0) || ((num_long_term_sps + num_long_term_pics) > 15))
        {
            SE_ERROR("SH: num_long_term_pics not in range !\n");
            return FrameParserError; /* Not Recoverable */
        }

        val = 0;
        local_delta_poc_msb_cycle_lt = 0;
        for (i = 0; i < num_long_term_sps; i++)
        {
            if (mSPS->num_long_term_ref_pics_sps > 1)
            {
                val = u_v(bitstream, mSPS->log2_num_long_term_ref_pics_sps, "SH: lt_idx_sps[%d]", i);
            }
            if ((val < 0) || (val > (mSPS->num_long_term_ref_pics_sps - 1)))
            {
                SE_ERROR("SH: lt_idx_sps not in range !\n");
                return FrameParserError; /* Not Recoverable */
            }

            lt_rps.elem[i].delta_poc = mSPS->lt_ref_pic_poc_lsb_sps[val];
            lt_rps.elem[i].used_by_curr_pic_flag = mSPS->used_by_curr_pic_lt_sps_flag[val];
            delta_poc_msb_present_flag[i] = u_1(bitstream, "SH: delta_poc_msb_present_flag[%d]", i);

            if (delta_poc_msb_present_flag[i])
            {
                //local_delta_poc_msb_cycle_lt += ue_v(bitstream, "SH: delta_poc_msb_cycle_lt[%d]", i);
                val = ue_v(bitstream, "SH: delta_poc_msb_cycle_lt[%d]", i);
                if ((val < 0) || (val > 16383))
                {
                    SE_ERROR("SH: local_delta_poc_msb_cycle_lt not in range !\n");
                    return FrameParserError; /* Not Recoverable */
                }
                local_delta_poc_msb_cycle_lt += val;
                delta_poc_msb_cycle_lt[i] = local_delta_poc_msb_cycle_lt;
            }
        }

        num_long_term_pics += num_long_term_sps;
        lt_rps.num_pics = num_long_term_pics;
        local_delta_poc_msb_cycle_lt = 0;

        for (i = i; i < num_long_term_pics; i++) // continuing with current i value !
        {
            lt_rps.elem[i].delta_poc = u_v(bitstream, mSPS->log2_max_pic_order_cnt_lsb, "SH: poc_lsb_lt[%d]", i);
            lt_rps.elem[i].used_by_curr_pic_flag = u_1(bitstream, "SH: used_by_curr_pic_lt_flag[%d]", i);
            delta_poc_msb_present_flag[i] = u_1(bitstream, "SH: delta_poc_msb_present_flag[%d]", i);

            if (delta_poc_msb_present_flag[i])
            {
                local_delta_poc_msb_cycle_lt += ue_v(bitstream, "SH: delta_poc_msb_cycle_lt[%d]", i);
                delta_poc_msb_cycle_lt[i] = local_delta_poc_msb_cycle_lt;
            }
        }
    }

    return FrameParserNoError;
}

void FrameParser_VideoHevc_c::init_tile_size()
{
    int32_t  i, tmp1, tmp2, tmp3;

    if (mPPS->tiles_enabled_flag)
    {
        if (mPPS->uniform_spacing_flag)
        {
            tmp1 = tmp3 = 0;

            for (i = 0; i < mPPS->num_tile_columns - 1; i++)
            {
                tmp2 = tmp3;
                tmp1 += mSPS->pic_width_in_ctb;
                tmp3 = tmp1 / mPPS->num_tile_columns;
                mPPS->tile_width[i] = tmp3 - tmp2;
            }

            tmp1 = tmp3 = 0;

            for (i = 0; i < mPPS->num_tile_rows - 1; i++)
            {
                tmp2 = tmp3;
                tmp1 += mSPS->pic_height_in_ctb;
                tmp3 = tmp1 / mPPS->num_tile_rows;
                mPPS->tile_height[i] = tmp3 - tmp2;
            }
        }
    }

    tmp1 = 0;

    for (i = 0; i < mPPS->num_tile_columns - 1; i++)
    {
        tmp1 += mPPS->tile_width[i];
    }

    mPPS->tile_width[i] = mSPS->pic_width_in_ctb - tmp1;
    tmp1 = 0;

    for (i = 0; i < mPPS->num_tile_rows - 1; i++)
    {
        tmp1 += mPPS->tile_height[i];
    }

    mPPS->tile_height[i] = mSPS->pic_height_in_ctb - tmp1;
}

// /////////////////////////////////////////////////////////////////////////////
//
//    The function responsible for reading a slice header
//

FrameParserStatus_t   FrameParser_VideoHevc_c::ReadNalSliceHeader(unsigned int SliceSegmentsInCurrentPicture)
{
    FrameParserStatus_t   Status;
    bool              first_slice_segment_in_pic_flag;
    bool              first_slice_detected = false;
    unsigned int      LevelIndex;
    //
    // If this is the first slice of a picture, initialize a new slice header
    //

    if (FrameParameters == NULL)
    {
        Status  = GetNewFrameParameters((void **)&FrameParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }

        SliceHeader = &FrameParameters->SliceHeader;
// TODO CL here operates normally SetDefaultSliceHeader... Just reset SliceHeader->Valid for now
//    Header        = SliceHeader;
//    SetDefaultSliceHeader( Header );
        SliceHeader->Valid = 0;
        first_slice_detected = true;
    }

    //
    // Return if this is not the first slice in a picture.
    //
    Status = FirstPartOfSliceHeader(&first_slice_segment_in_pic_flag);
    if (Status == FrameParserError)
    {
        return FrameParserError;
    }

    SliceHeader->first_mb_in_slice = first_slice_segment_in_pic_flag;
    if (mPPS == NULL)
    {
        if (!FirstDecodeAfterInputJump)
        {
            SE_ERROR("No appropriate picture parameter set seen\n");
        }
        return FrameParserError;
    }
    if (mSPS == NULL)
    {
        if (!FirstDecodeAfterInputJump)
        {
            SE_ERROR("No appropriate sequence parameter set seen\n");
        }
        return FrameParserError;
    }
    if (mVPS == NULL)
    {
        if (!FirstDecodeAfterInputJump)
        {
            SE_ERROR("No appropriate video parameter set seen\n");
        }
        return FrameParserError;
    }

    // Get level index
    for (LevelIndex = 0; LevelIndex < TOTAL_HEVC_LEVELS; LevelIndex ++)
        if (HevcLevelIdc[LevelIndex] == mSPS->level_idc)
        {
            break;
        }

    if ((LevelIndex < TOTAL_HEVC_LEVELS) && (SliceSegmentsInCurrentPicture > HevcMaxSliceSegmentsPerPicture[LevelIndex]))
    {
        SE_DEBUG(group_frameparser_video,  "HEVC: Slice segments in current picture %d more than allowed by level %d\n",
                 SliceSegmentsInCurrentPicture, HevcMaxSliceSegmentsPerPicture[LevelIndex]);
        return FrameParserStreamSyntaxError;
    }

    if (!first_slice_segment_in_pic_flag)
    {
        if (first_slice_detected)
        {
            SE_ERROR("HEVC: missing first slice of the picture\n");
            return FrameParserError;
        }

        return FrameParserNoError;
    }

    slice_types = 0;
    SliceHeader->VideoParameterSet = mVPS;
    SliceHeader->PictureParameterSet = mPPS;
    SliceHeader->SequenceParameterSet = mSPS;
    SliceHeader->scaling_list = (mPPS->pps_scaling_list_data_present_flag ? &mPPS->scaling_list : &mSPS->scaling_list);
    SliceHeader->output_flag = pic_output_flag;
    init_tile_size();
    SliceHeader->Valid = 1;
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The read headers stream specific function
//

FrameParserStatus_t   FrameParser_VideoHevc_c::ReadHeaders(void)
{
    unsigned int            sc_num;
    unsigned int            Code;
    unsigned int            nal_unit_type;
    unsigned int            UnitLength;
    unsigned int            TotalSlicesLength = 0;
    unsigned int            NonSliceNALUnits = 0;
    FrameParserStatus_t     Status;
    bool                    IgnoredNal = false;
    bool                    IsSlice;
    unsigned int            SliceSegmentsInCurrentPicture = 0;
    bool                    EnounteredFrameParserStreamSyntaxError = false;
    bool                    break_sei_sc = false;

    if (FirstDecodeAfterInputJump)
    {
        SeenAnIDR           = false;
        BehaveAsIfSeenAnIDR     = false;

        //
        // Ensure pic order cnt stuff will work for whatever direction we are playing in
        //

        if (PlaybackDirection == PlayBackward)
        {
            if (PicOrderCntOffset <= 0x7fffffffffffffffull)
            {
                PicOrderCntOffset   = 0xffffffff00000000ull;
            }

            PicOrderCntOffsetAdjust = 0xffffffff00000000ull;
        }
        else
        {
            if (PicOrderCntOffset > 0x7fffffffffffffffull)
            {
                PicOrderCntOffset   = 0x100000000ull;
            }

            PicOrderCntOffsetAdjust = 0x100000000ull;
        }
    }

    //The second pass of Read Headers handles Prefix SEI NAL of next Frame.
    if (mSecondPass == false)
    {
        //Reset the ParsedSC for first pass of Read headers.
        mParsedStartCodeNum = 0;
    }

    SE_DEBUG(group_frameparser_video, "Parse Start StartCode %d Total StartCode %d Pass %d for Frame %d\n", mParsedStartCodeNum, StartCodeList->NumberOfStartCodes, (mSecondPass ? 2 : 1),
             NextDecodeFrameIndex);

    sc_num = mParsedStartCodeNum;

    while (sc_num < StartCodeList->NumberOfStartCodes)
    {
        //break_sei_sc set to true when no slice ahead of Prefix SEI NAL
        if (break_sei_sc == true)
        {
            break;
        }

        Code            = StartCodeList->StartCodes[sc_num];
        UnitLength
            = ((sc_num != StartCodeList->NumberOfStartCodes - 1) ? ExtractStartCodeOffset(StartCodeList->StartCodes[sc_num + 1])
               : BufferLength) - ExtractStartCodeOffset(Code) - 3;

        if ((ExtractStartCodeCode(Code) & 0x80) != 0)
        {
            SE_ERROR("Invalid forbidden zero bit value\n");
            FrameParameters = NULL;
            return FrameParserHeaderSyntaxError;
        }

        nal_unit_type           = (ExtractStartCodeCode(Code) >> 1) & 0x3f;
//        SE_INFO(group_frameparser_video,  "ReadHeaders NAL unit %d type=%d size=%d bytepos=%08x",
//                sc_num, nal_unit_type, UnitLength,
//                ExtractStartCodeOffset(StartCodeList->StartCodes[sc_num]) + 3);
        //
        // Process the header, with a specific preload of data into the anti emulation buffer, dependent on header type
        //
        // Ignore start code (3 bytes)
        LoadAntiEmulationBuffer(BufferData + ExtractStartCodeOffset(Code) + 3);
        IsSlice = false;
        Status = FrameParserNoError;

        switch (nal_unit_type)
        {
        case NAL_UNIT_ACCESS_UNIT_DELIMITER:
            break;

        case NAL_UNIT_VPS:
            CheckAntiEmulationBuffer(UnitLength);
            Bits.Get(16); // Drop NAL header
            Status = ProcessVPS();
            break;

        case NAL_UNIT_SPS:
            CheckAntiEmulationBuffer(UnitLength);
            Bits.Get(16); // Drop NAL header
            Status = ProcessSPS();
            break;

        case NAL_UNIT_PPS:
            CheckAntiEmulationBuffer(UnitLength);
            Bits.Get(16); // Drop NAL header
            Status = ProcessPPS();
            break;

        case NAL_UNIT_PREFIX_SEI:
        {
            bool found_slice = false;
            unsigned int tempCode;
            unsigned int temp_nal_unit_type;

            // Do complete StartCode List parse if second pass or no Slice is present(like the First Buffer with SPS/PPS/SEI)
            if (FrameToDecode == false || mSecondPass == true)
            {
                found_slice = true;
            }

            for (unsigned int j = sc_num + 1; (j < StartCodeList->NumberOfStartCodes && found_slice == false) ; j++)
            {
                tempCode = StartCodeList->StartCodes[j];
                if ((ExtractStartCodeCode(tempCode) & 0x80) != 0)
                {
                    SE_ERROR("Parsing Prefix SEI in Frame %d, found Invalid forbidden zero bit value\n", NextDecodeFrameIndex);
                    found_slice = true;
                }
                temp_nal_unit_type = (ExtractStartCodeCode(tempCode) >> 1) & 0x3f;
                if (IsNalUnitSlice(temp_nal_unit_type))
                {
                    found_slice = true;
                }
            }
            // If no Slice is found, it means this Prefix SEI belongs to the next frame, so break the StartCode parse until frame is queued for decode
            if (found_slice == false)
            {
                SE_VERBOSE(group_frameparser_video, "Breaking Prefix SEI Parse at StartCode number %d Frame %d\n", sc_num, NextDecodeFrameIndex);
                break_sei_sc = true;
                sc_num--; // to counter the increment in while loop
                break;
            }
        }
        // fallthrough

        case NAL_UNIT_SUFFIX_SEI:

            // update user data number with the saved user data index as we can lose it if user data and SEI NAL aren't
            // in the same buffer (ParsedFrameParameters->UserDataNumber become zero)

            ParsedFrameParameters->UserDataNumber = UserDataIndex;
            CheckAntiEmulationBuffer(min(UnitLength, DEFAULT_ANTI_EMULATION_REQUEST));
            Bits.Get(16); // Drop NAL header
            // "UnitLength - 2 " is done for the 2 byte SEI NAL header which is a part of UnitLength but dropped before calling Process SEI
            Status = ProcessSEI(UnitLength - 2, nal_unit_type);
            break;

        case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
        case NAL_UNIT_CODED_SLICE_IDR_N_LP:
        case NAL_UNIT_CODED_SLICE_BLA_W_LP:
        case NAL_UNIT_CODED_SLICE_BLA_N_LP:
        case NAL_UNIT_CODED_SLICE_BLA_W_RADL:
        case NAL_UNIT_CODED_SLICE_CRA:

        case NAL_UNIT_CODED_SLICE_TRAIL_R:
        case NAL_UNIT_CODED_SLICE_TRAIL_N:
        case NAL_UNIT_CODED_SLICE_TLA_R:
        case NAL_UNIT_CODED_SLICE_TSA_N:
        case NAL_UNIT_CODED_SLICE_STSA_R:
        case NAL_UNIT_CODED_SLICE_STSA_N:
        case NAL_UNIT_CODED_SLICE_RADL_R:
        case NAL_UNIT_CODED_SLICE_RADL_N:
        case NAL_UNIT_CODED_SLICE_RASL_R:
        case NAL_UNIT_CODED_SLICE_RASL_N:

            // Count the total number of slices present in the picture
            SliceSegmentsInCurrentPicture++;

            // Add start code. Leave it to Codec to remove extra 0x00 byte in case of a 4-byte start code.
            TotalSlicesLength += UnitLength + 3;
            // Also accumulate any non-slice NAL unit found between this slice and the previous one
            TotalSlicesLength += NonSliceNALUnits;
            NonSliceNALUnits = 0;
            IsSlice = true;
            CheckAntiEmulationBuffer(min(UnitLength, DEFAULT_ANTI_EMULATION_REQUEST));
            Status      = ReadNalSliceHeader(SliceSegmentsInCurrentPicture);

            // Commit for decode if status is OK, and this is not the first slice
            // (IE we haven't already got the frame to decode flag, and its a first slice)
            if ((Status == FrameParserNoError) && !FrameToDecode)
            {

                // update user data number with the saved user data index as we can lose it if user data and slice data aren't
                // in the same buffer (ParsedFrameParameters->UserDataNumber become zero)
                ParsedFrameParameters->UserDataNumber = UserDataIndex;
                ParsedFrameParameters->DataOffset   = ExtractStartCodeOffset(Code);
                Status              = CommitFrameForDecode();
                has_picture_digest = false;
            }
            // Reset User Data Index if Error from ReadNalSliceHeader or CommitFrameForDecode
            if ((Status != FrameParserNoError) && !FrameToDecode)
            {
                UserDataIndex = 0;
            }

            break;

        case NAL_UNIT_FILLER_DATA:
            // Ignore Filler data
            IgnoredNal = true;
            break;

        default:
            IgnoredNal = true;
            SE_DEBUG(group_frameparser_video,  "Ignored NAL unit type %d\n", nal_unit_type);
            break;
        }

        if (!IsSlice)
        {
            NonSliceNALUnits += UnitLength + 3;
        }

        if (!IgnoredNal)
        {
            //SE_INFO(group_frameparser_video,  "Parsed NAL unit type %d\n", nal_unit_type);
        }

        AssertAntiEmulationOk();

        if (Status != FrameParserNoError)
        {
            IncrementErrorStatistics(Status);

            if (Status == FrameParserStreamSyntaxError)
            {
                EnounteredFrameParserStreamSyntaxError = true;
            }

        }
        sc_num++;
    }
    //Set the StartCode Counter for next pass of Read Headers for parsing Prefix SEI NAL of next Frame
    mParsedStartCodeNum = sc_num;

    if (EnounteredFrameParserStreamSyntaxError)
    {
        // Each frame has its own FrameParameters which are obtained when we read in headers pertaining to that frame
        // If any syntax error is encountered during ReadHeaders, this frame is not submitted for decode
        // But if the error is detected in a NALU after the last VCL NALU of current Frame with no error, at this point
        // we have valid FrameParamters and now if immediate next frame erroneous which has first slice missing,
        // ReadNalSliceHeader will not be able to detect this and try to submit this slice with previous frame's
        // FrameParameters to CommitFrameForDecode. Here even though the new SPS NALU might have been arrived but
        // the FrameParameters will be still pointing to the old SPS
        // set the FrameParameters to NULL to avoid referencing a SPS buffer with RefCount 0 which can give
        // random sps id(if that buffer is reused somewhere) and result in NULL pointer in that index in SPSTable
        FrameParameters = NULL;
        SE_INFO(group_frameparser_video, "Syntax error in stream\n");
        return FrameParserStreamSyntaxError;
    }

    // We clear the FrameParameters pointer, a new one will be obtained
    // before/if we read in headers pertaining to the next frame. This
    // will generate an error should I accidentally write code that
    // accesses this data when it should not.
    //
    // FrameParameters are NULL during second pass
    if (FrameToDecode && !mSecondPass)
    {
        FrameParameters->CodedSlicesSize = TotalSlicesLength;
        FrameParameters             = NULL;
    }

    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read Remaining Start Code - Prefix SEI NAL
//

FrameParserStatus_t FrameParser_VideoHevc_c::ReadRemainingStartCode(
    void)
{
    FrameParserStatus_t     Status = FrameParserNoError;
    //Set second pass flag
    mSecondPass = true;

    Status = ReadHeaders();

    //Reset second pass flag
    mSecondPass = false;

    return Status;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - a supplemental enhancement record
//

FrameParserStatus_t FrameParser_VideoHevc_c::ReadPlayer2ContainerParameters(
    void)
{
// empty method for now
// TODO CL must rework all this content !!!
#if 0
    unsigned char   ParametersVersion;
    unsigned int    FieldsPresentMask;
    unsigned int    PixelAspectRatioNumerator;
    unsigned int    PixelAspectRatioDenominator;
    unsigned int    TimeScale;
    unsigned int    TimeDelta;
//
    ParametersVersion   = Bits.Get(8);
    MarkerBits(8, 0xff);

    switch (ParametersVersion)
    {
    case 0x01:
        FieldsPresentMask   = Bits.Get(16);
        MarkerBits(8, 0xff);

        if ((FieldsPresentMask & 0x0002) != 0)
        {
            PixelAspectRatioNumerator    = Bits.Get(16) << 16;
            MarkerBits(8, 0xff);
            PixelAspectRatioNumerator   |= Bits.Get(16);
            MarkerBits(8, 0xff);
            PixelAspectRatioDenominator  = Bits.Get(16) << 16;
            MarkerBits(8, 0xff);
            PixelAspectRatioDenominator |= Bits.Get(16);
            MarkerBits(8, 0xff);
            DefaultPixelAspectRatio = Rational_t(PixelAspectRatioNumerator, PixelAspectRatioDenominator);
        }

        //
        // Check if the default frame rate is present
        //

        if ((FieldsPresentMask & 0x0001) == 0)
        {
            break;
        }

    case 0x00:
        TimeScale        = Bits.Get(16) << 16;
        MarkerBits(8, 0xff);
        TimeScale       |= Bits.Get(16);
        MarkerBits(8, 0xff);
        TimeDelta        = Bits.Get(16) << 16;
        MarkerBits(8, 0xff);
        TimeDelta       |= Bits.Get(16);
        MarkerBits(8, 0xff);
        ContainerFrameRate      = Rational_t(TimeScale, TimeDelta);
        break;

    default:
        SE_ERROR("Unsupported version of the container parameters record (%02x)\n", ParametersVersion);
        break;
    }

//
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Player 2 container parameter message :-\n");
    SE_INFO(group_frameparser_video,  " TimeScale                                  = %6d\n", TimeScale);
    SE_INFO(group_frameparser_video,  " TimeDelta                                  = %6d\n", TimeDelta);
#endif
//
#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - determine the pic order count
//
FrameParserStatus_t FrameParser_VideoHevc_c::CalculatePicOrderCnts(void)
{
    uint32_t  MaxPicOrderCntLsb;
    int32_t   PicOrderCntMsb;
    // SE_INFO(group_frameparser_video,  "JLX poc: IDR %d, lsb %d\n", SliceHeader->idr_flag, SliceHeader->pic_order_cnt_lsb);
    //
    // Obtain the previous PicOrderCnt values
    //
    SliceHeader->EntryPicOrderCntMsb        = PrevPicOrderCntMsb;

    if (SliceHeader->idr_flag)
    {
        PrevPicOrderCntMsb              = 0;
        PrevPicOrderCntLsb              = 0;
        SliceHeader->ExitPicOrderCntMsbForced   = 1;
        PicOrderCntMsb                          = 0;
        SliceHeader->pic_order_cnt_lsb = 0;
        SliceHeader->PicOrderCnt = 0;
    }
    else
    {
        //
        // Calculate the PicOrderCntMsb
        //
        MaxPicOrderCntLsb       = 1 << (mSPS->log2_max_pic_order_cnt_lsb);

        if ((SliceHeader->pic_order_cnt_lsb < PrevPicOrderCntLsb) &&
            ((PrevPicOrderCntLsb - SliceHeader->pic_order_cnt_lsb) >= (MaxPicOrderCntLsb / 2)))
        {
            PicOrderCntMsb = PrevPicOrderCntMsb + MaxPicOrderCntLsb;
        }
        else if ((SliceHeader->pic_order_cnt_lsb > PrevPicOrderCntLsb) &&
                 ((SliceHeader->pic_order_cnt_lsb - PrevPicOrderCntLsb) > (MaxPicOrderCntLsb / 2)))
        {
            PicOrderCntMsb = PrevPicOrderCntMsb - MaxPicOrderCntLsb;
        }
        else
        {
            PicOrderCntMsb = PrevPicOrderCntMsb;
        }

        //
        // And calculate the appropriate field order counts
        //
        SliceHeader->PicOrderCnt = PicOrderCntMsb + SliceHeader->pic_order_cnt_lsb;
    }

    //
    // Update the PicOrderCnt values for use on the next frame
    //
    if (!SliceHeader->temporal_id)
    {
        PrevPicOrderCntMsb              = PicOrderCntMsb;
        PrevPicOrderCntLsb              = SliceHeader->pic_order_cnt_lsb;
        LastExitPicOrderCntMsb          = PicOrderCntMsb;
        SliceHeader->ExitPicOrderCntMsb     = PicOrderCntMsb;
    }

    //
    // Now calculate the extended pic order count used for frame re-ordering
    // this is based on pic order count, or on the dpb output delay if picture
    // timing messages are present.
    //
    if (SliceHeader->idr_flag)
    {
        PicOrderCntOffset = PicOrderCntOffset + PicOrderCntOffsetAdjust;  // Add 2^32 to cater for negative pic order counts in the next sequence.
#if 0 // TODO CL : manage BaseDpb for extendePOC computation
        BaseDpbValue  = 0;
#endif
    }

#if 0 // TODO CL : for now no SEI handling then no dpb_output_delay available
    SliceHeader->ExtendedPicOrderCnt    = DisplayOrderByDpbValues ?
                                          SliceHeader->PicOrderCnt = pic_order_cnt_msb + pic_order_cnt_lsb;

    if ((nal_ref_flag) && (! temporal_id))
    {
        prev_pic_order_cnt_msb = pic_order_cnt_msb;
    }

#else
    SliceHeader->ExtendedPicOrderCnt    = PicOrderCntOffset + SliceHeader->PicOrderCnt;
#endif
//
    // SE_INFO(group_frameparser_video,  "JLX poc: lsb %d, msb %u, poc %d, epoc %llu\n", SliceHeader->pic_order_cnt_lsb, PicOrderCntMsb, SliceHeader->PicOrderCnt, SliceHeader->ExtendedPicOrderCnt);
#ifdef DUMP_REFLISTS
    SE_INFO(group_frameparser_video,  "xxx CalculatePicOrderCnts DecodeFrameIndex %d PocLsb %d  POC %d ExtPoc (%016llx)\n",
            NextDecodeFrameIndex, SliceHeader->pic_order_cnt_lsb, SliceHeader->PicOrderCnt, SliceHeader->ExtendedPicOrderCnt);
#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Update the list of references according to the Reference Picture Sets,
//      then prepare the reference frame lists L0 and L1
//

FrameParserStatus_t   FrameParser_VideoHevc_c::PrepareReferenceFrameList(void)
{
    unsigned int    RefFrameIndex;
    unsigned int    PicSetIndex;
    unsigned int    RefPicIndex;
    unsigned int        num_pic_set[MAX_NUM_PIC_SET];               // number of items in pic_sets[][]
    unsigned int        pic_sets[MAX_NUM_PIC_SET][HEVC_MAX_REFERENCE_INDEX];    // indexes to ReferenceFrames[]
    bool            IsStillReference[HEVC_MAX_DPB_SIZE];        // specifies that the picture is in the 5 RPS

    //
    // Update references: taken from update_dpb_before_decoding() in STHM'
    //
    if (SliceHeader->rap_flag)
    {
        int DecodeKeyFramesOnly = ((Player->PolicyValue(Playback, Stream, PolicyTrickModeDomain) == PolicyValueTrickModeDecodeKeyFrames)
                                   || (Player->PolicyValue(Playback, Stream, PolicyStreamOnlyKeyFrames) == PolicyValueApply));

        if ((SliceHeader->idr_flag) || DecodeKeyFramesOnly)
        {
            // Remove all references
            for (RefFrameIndex = 0; RefFrameIndex < HEVC_MAX_DPB_SIZE; RefFrameIndex++)
            {
                ReferenceFrames[RefFrameIndex].Used = false;
            }
            NumReferenceFrames = 0;
            Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL);
        }

        // Empty ref lists
        ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_0].EntryCount = 0;
        ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_1].EntryCount = 0;
        ParsedFrameParameters->NumberOfReferenceFrameLists = 0;
        return FrameParserNoError;
    }

    // Prepare the RPS
    build_pic_sets(num_pic_set, pic_sets);

    // picture with missing reference will be treated differently in codec class
    if (mMissingRef)
    {
        SE_WARNING("Missing ref(s). It is unusual if stream is not corrupted stream.\n");
        ParsedFrameParameters->PictureHasMissingRef = true;
    }
    else
    {
        ParsedFrameParameters->PictureHasMissingRef = false;
    }

    // Compute the list of references in the 5 RPS
    for (RefFrameIndex = 0; RefFrameIndex < HEVC_MAX_DPB_SIZE; RefFrameIndex++)
    {
        IsStillReference[RefFrameIndex] = false;
    }

    for (PicSetIndex = 0; PicSetIndex < MAX_NUM_PIC_SET; PicSetIndex++)
        for (RefPicIndex = 0; RefPicIndex < num_pic_set[PicSetIndex]; RefPicIndex ++)
        {
            RefFrameIndex = pic_sets[PicSetIndex][RefPicIndex];
            if (RefFrameIndex != INVALID_INDEX)
            {
                IsStillReference[RefFrameIndex] = true;
            }
        }

    // Release the references that are no longer in the 5 RPS
    for (RefFrameIndex = 0; RefFrameIndex < HEVC_MAX_DPB_SIZE; RefFrameIndex++)
        if (ReferenceFrames[RefFrameIndex].Used && ! IsStillReference[RefFrameIndex])
        {
            Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrames[RefFrameIndex].DecodeFrameIndex);
            ProcessDeferredDFIandPTSUpto(ReferenceFrames[RefFrameIndex].ExtendedPicOrderCnt);
            ReferenceFrames[RefFrameIndex].Used = false;
            -- NumReferenceFrames;
        }

    //
    // Prepare the initial reference picture lists: taken from mme_ctbe_api_set in STHM'
    //
    // Build the initial reference picture list 0
    ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_0].EntryCount = 0;
    AddRPSEntries(num_pic_set, pic_sets, ST_CURR0, &ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_0]);
    AddRPSEntries(num_pic_set, pic_sets, ST_CURR1, &ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_0]);
    AddRPSEntries(num_pic_set, pic_sets, LT_CURR,  &ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_0]);
    // Build the initial reference picture list 1
    ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_1].EntryCount = 0;
    AddRPSEntries(num_pic_set, pic_sets, ST_CURR1, &ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_1]);
    AddRPSEntries(num_pic_set, pic_sets, ST_CURR0, &ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_1]);
    AddRPSEntries(num_pic_set, pic_sets, LT_CURR,  &ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_1]);
    ParsedFrameParameters->NumberOfReferenceFrameLists  = HEVC_NUM_REF_FRAME_LISTS;
    // SE_INFO(group_frameparser_video, "JLX: %d: L0 %d, L1 %d\n", SliceHeader->PicOrderCnt, ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_0].EntryCount, ParsedFrameParameters->ReferenceFrameList[REF_PIC_LIST_1].EntryCount);
    return FrameParserNoError;
}

void FrameParser_VideoHevc_c::build_pic_sets(unsigned int num_pic_set[MAX_NUM_PIC_SET],  unsigned int pic_sets[MAX_NUM_PIC_SET][HEVC_MAX_REFERENCE_INDEX])
{
    // poc[i] = POC of the reference pictures of the picture set i
    int32_t  ref_poc[MAX_NUM_PIC_SET][HEVC_MAX_REFERENCE_INDEX];
    int32_t  tmp_poc;
    uint8_t  i, j, k;
    bool msb_present_flag;
    bool curr_msb_present_flag[HEVC_MAX_REFERENCE_INDEX];
    bool foll_msb_present_flag[HEVC_MAX_REFERENCE_INDEX];
    int32_t  poc = SliceHeader->PicOrderCnt;
    uint32_t max_pic_order_cnt_lsb = 1 << (mSPS->log2_max_pic_order_cnt_lsb);
    st_rps_t *local_st_rps;
    mMissingRef = false;

    if (short_term_ref_pic_set_idx == mSPS->num_short_term_ref_pic_sets)
    {
        local_st_rps = &st_rps;    // RPS in slice header
    }
    else
    {
        local_st_rps = mSPS->st_rps + short_term_ref_pic_set_idx;    // RPS in SPS
    }

    // -----------------------------------
    // Build the sets of reference picture POCs

    for (i = 0, j = 0, k = 0; i < local_st_rps->rps_s0.num_pics; i++)
    {
        tmp_poc = poc - local_st_rps->rps_s0.elem[i].delta_poc;

        if (local_st_rps->rps_s0.elem[i].used_by_curr_pic_flag)
        {
            ref_poc[ST_CURR0][j++] = tmp_poc;
        }
        else
        {
            ref_poc[ST_FOLL][k++] = tmp_poc;
        }
    }

    num_pic_set[ST_CURR0] = j;

    for (i = 0, j = 0; i < local_st_rps->rps_s1.num_pics; i++)
    {
        tmp_poc = poc + local_st_rps->rps_s1.elem[i].delta_poc;

        if (local_st_rps->rps_s1.elem[i].used_by_curr_pic_flag)
        {
            ref_poc[ST_CURR1][j++] = tmp_poc;
        }
        else
        {
            ref_poc[ST_FOLL][k++] = tmp_poc;
        }
    }

    num_pic_set[ST_CURR1] = j;
    num_pic_set[ST_FOLL] = k;
    poc &= ~(max_pic_order_cnt_lsb - 1);

    for (i = 0, j = 0, k = 0; i < lt_rps.num_pics; i++)
    {
        tmp_poc = lt_rps.elem[i].delta_poc;
        msb_present_flag = delta_poc_msb_present_flag[i];

        if (msb_present_flag)
        {
            tmp_poc += poc - delta_poc_msb_cycle_lt[i] * max_pic_order_cnt_lsb;
        }

        if (lt_rps.elem[i].used_by_curr_pic_flag)
        {
            ref_poc[LT_CURR][j] = tmp_poc;
            curr_msb_present_flag[j++] = msb_present_flag;
        }
        else
        {
            ref_poc[LT_FOLL][k] = tmp_poc;
            foll_msb_present_flag[k++] = msb_present_flag;
        }
    }

    num_pic_set[LT_CURR] = j;
    num_pic_set[LT_FOLL] = k;
    // -----------------------------------
    // Build the sets of reference pictures
    build_lt_pic_set(pic_sets[ LT_CURR], num_pic_set[ LT_CURR], ref_poc[ LT_CURR], curr_msb_present_flag);
    build_lt_pic_set(pic_sets[ LT_FOLL], num_pic_set[ LT_FOLL], ref_poc[ LT_FOLL], foll_msb_present_flag);
    build_st_pic_set(pic_sets[ST_CURR0], num_pic_set[ST_CURR0], ref_poc[ST_CURR0]);
    build_st_pic_set(pic_sets[ST_CURR1], num_pic_set[ST_CURR1], ref_poc[ST_CURR1]);
    build_st_pic_set(pic_sets[ ST_FOLL], num_pic_set[ ST_FOLL], ref_poc[ ST_FOLL]);
}

void FrameParser_VideoHevc_c::build_lt_pic_set(unsigned int pic_set[MAX_NUM_PIC_SET], uint8_t num_pic, int32_t *poc, bool *msb_present_flag)
{
    uint8_t i;
    unsigned int RefFrameIndex;

    for (i = 0; i < num_pic; i++)
    {
        pic_set[i] = INVALID_INDEX;

        for (RefFrameIndex = 0; RefFrameIndex < HEVC_MAX_DPB_SIZE; RefFrameIndex++)
            if (ReferenceFrames[RefFrameIndex].Used &&
                ((msb_present_flag[i] && ReferenceFrames[RefFrameIndex].PicOrderCnt == poc[i]) ||
                 (!msb_present_flag[i] && ReferenceFrames[RefFrameIndex].PicOrderCnt_lsb == poc[i])))
            {
                pic_set[i] = RefFrameIndex;
                ReferenceFrames[RefFrameIndex].IsLongTerm = true;
            }

        if (pic_set[i] == INVALID_INDEX)
        {
            if (ParsedFrameParameters->FirstParsedParametersAfterInputJump && ParsedFrameParameters->IndependentFrame)
            {
                SE_INFO(group_frameparser_video, "pic %d cannot find POC %d preceeding a RAP, skipping long term reference\n", NextDecodeFrameIndex, poc[i]);
            }
            else
            {
                SE_ERROR("pic %d cannot find POC %d, skipping long term reference\n", NextDecodeFrameIndex, poc[i]);
                mMissingRef = true;
            }
        }
    }
}

void FrameParser_VideoHevc_c::build_st_pic_set(unsigned int pic_set[MAX_NUM_PIC_SET], uint8_t num_pic, int32_t *poc)
{
    uint8_t i;
    unsigned int RefFrameIndex;

    for (i = 0; i < num_pic; i++)
    {
        pic_set[i] = INVALID_INDEX;

        for (RefFrameIndex = 0; RefFrameIndex < HEVC_MAX_DPB_SIZE; RefFrameIndex++)
            if (ReferenceFrames[RefFrameIndex].Used && ReferenceFrames[RefFrameIndex].PicOrderCnt == poc[i])
            {
                pic_set[i] = RefFrameIndex;
                ReferenceFrames[RefFrameIndex].IsLongTerm = false;
            }

        if (pic_set[i] == INVALID_INDEX)
        {
            if (ParsedFrameParameters->FirstParsedParametersAfterInputJump && ParsedFrameParameters->IndependentFrame)
            {
                SE_INFO(group_frameparser_video, "pic %d cannot find POC %d preceeding a RAP, skipping short term reference\n", NextDecodeFrameIndex, poc[i]);
            }
            else
            {
                SE_ERROR("pic %d cannot find POC %d, skipping short term reference\n", NextDecodeFrameIndex, poc[i]);
                mMissingRef = true;
            }
        }
    }
}

void FrameParser_VideoHevc_c::AddRPSEntries(unsigned int num_pic_set[MAX_NUM_PIC_SET],
                                            unsigned int pic_sets[MAX_NUM_PIC_SET][HEVC_MAX_REFERENCE_INDEX],
                                            int PicSetIndex,
                                            ReferenceFrameList_t *rfl
                                           )
{
    unsigned int RefPicIndex;

    for (RefPicIndex = 0; RefPicIndex < num_pic_set[PicSetIndex]; RefPicIndex ++)
    {
        unsigned int RefFrameIndex = pic_sets[PicSetIndex][RefPicIndex];
        if (RefFrameIndex != INVALID_INDEX)
        {
            rfl->EntryIndicies   [rfl->EntryCount]                   = ReferenceFrames[RefFrameIndex].DecodeFrameIndex;
            rfl->ReferenceDetails[rfl->EntryCount].PicOrderCnt       = ReferenceFrames[RefFrameIndex].PicOrderCnt;
            rfl->ReferenceDetails[rfl->EntryCount].LongTermReference = ReferenceFrames[RefFrameIndex].IsLongTerm;
            ++ (rfl->EntryCount);
        }
    }
}

FrameParserStatus_t FrameParser_VideoHevc_c::ForPlayUpdateReferenceFrameList()
{
    unsigned long RefFrameIndex;

    //
    // Add current reference frame
    //
    if (SliceHeader->reference_flag || ((unsigned int) SliceHeader->temporal_id < (unsigned int) mSPS->sps_max_sub_layers - 1))
    {
        for (RefFrameIndex = 0; RefFrameIndex < HEVC_MAX_DPB_SIZE; RefFrameIndex++)
            if (! ReferenceFrames[RefFrameIndex].Used)
            {
                ReferenceFrames[RefFrameIndex].Used = true;
                ReferenceFrames[RefFrameIndex].PicOrderCnt_lsb = SliceHeader->pic_order_cnt_lsb;
                ReferenceFrames[RefFrameIndex].PicOrderCnt = SliceHeader->PicOrderCnt;
                ReferenceFrames[RefFrameIndex].ExtendedPicOrderCnt = SliceHeader->ExtendedPicOrderCnt;
                ReferenceFrames[RefFrameIndex].DecodeFrameIndex = ParsedFrameParameters->DecodeFrameIndex;
                ReferenceFrames[RefFrameIndex].IsLongTerm = false;
                break;
            }

        if (RefFrameIndex == HEVC_MAX_DPB_SIZE)
        {
            SE_ERROR("no free slot for new reference frame\n");
            return FrameParserError;
        }
    }

    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Release reference frame or field
//

// Ignore this method for now
// TODO CL must rework all this content !!!
FrameParserStatus_t   FrameParser_VideoHevc_c::ReleaseReference(bool          ActuallyRelease,
                                                                unsigned int      Entry,
                                                                unsigned int      ReleaseUsage)
{
    // JLX: not used anymore, remove ?
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The reset reference frame list function
//

FrameParserStatus_t   FrameParser_VideoHevc_c::ResetReferenceFrameList(void)
{
    unsigned int RefFrameIndex;

    // Remove all references
    for (RefFrameIndex = 0; RefFrameIndex < HEVC_MAX_DPB_SIZE; RefFrameIndex++)
    {
        ReferenceFrames[RefFrameIndex].Used = false;
    }

    NumReferenceFrames = 0;
    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL);
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to purge deferred post decode parameter
//      settings, these consist of the display frame index, and presentation
//      time. Here we flush everything we have.
//

FrameParserStatus_t FrameParser_VideoHevc_c::ForPlayPurgeQueuedPostDecodeParameterSettings(
    void)
{
    ProcessDeferredDFIandPTSUpto(0xffffffffffffffffull);
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to process deferred post decode parameter
//      settings, these consist of the display frame index, and presentation
//      time, if we have an IDR or a B frame, we can process any frames with a
//  lower pic order count, we also allow the processing of all frames up to
//  a pic order cnt when a reference frame falls out of the sliding window
//  in reference framme marking.
//

FrameParserStatus_t FrameParser_VideoHevc_c::ForPlayProcessQueuedPostDecodeParameterSettings(
    void)
{
    //
    // Can we process any of these
    //
//TODO CL check if these NAL_UNIT_CODED_SLICE_xx tests are appropriate !!!
    if (ParsedFrameParameters->FirstParsedParametersForOutputFrame &&
        ((SliceHeader->nal_unit_type == NAL_UNIT_CODED_SLICE_CRA) ||
         (SliceHeader->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL) ||
         (SliceHeader->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP)))
    {
        ProcessDeferredDFIandPTSUpto(SliceHeader->ExtendedPicOrderCnt);
    }
    else
    {
        // Simplified C.5.2 from Hevc bumping process to process deferred DFI & PTS
        // The number of pictures in the DPB with temporal_id lower than or equal to the temporal_id of the current picture is equal
        //    to sps_max_dec_pic_buffering[ temporal_id ].
        if ((SliceHeader->SequenceParameterSet->is_valid) && (mDeferredListEntries > SliceHeader->SequenceParameterSet->sps_max_dec_pic_buffering[0]))
        {
            ProcessDeferredDFIandPTSUpto((mDeferredList[mOrderedDeferredList[0]].ExtendedPicOrderCnt) + 1);
        }
    }

//
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to generate the post decode parameter
//      settings, these consist of the display frame index, and presentation
//      time, both of which may be deferred if the information is unavailable.
//
//      For hevc, we allow the processing of all frames that are IDRs or
//  non-reference frames, all others are deferred
//

FrameParserStatus_t FrameParser_VideoHevc_c::ForPlayGeneratePostDecodeParameterSettings(
    void)
{

    bool DecodeKeyFramesOnly = ((Player->PolicyValue(Playback, Stream, PolicyTrickModeDomain) == PolicyValueTrickModeDecodeKeyFrames) ||
                                (Player->PolicyValue(Playback, Stream, PolicyStreamOnlyKeyFrames) == PolicyValueApply));

    //
    // Default setting
    //
    InitializePostDecodeParameterSettings();

    if (ParsedFrameParameters->IndependentFrame && DecodeKeyFramesOnly)
    {
        //Incase of I-Only injection calculate DFI and pts , defer in other cases
        CalculateFrameIndexAndPts(ParsedFrameParameters, ParsedVideoParameters);
    }
    else
    {
        //
        // Defer pts generation, and perform dts generation
        //
        DeferDFIandPTSGeneration(Buffer, ParsedFrameParameters,
                                 ParsedVideoParameters, SliceHeader->ExtendedPicOrderCnt);
    }

    SE_DEBUG(group_frameparser_video, "Frametype %s, DFI = %d DecodeKeyFramesOnly=%d IndependentFrame=%d PicorderCount=%lld",
             ParsedFrameParameters->KeyFrame ? "I" : ParsedFrameParameters->ReferenceFrame ? "P" : "B",
             ParsedFrameParameters->DisplayFrameIndex, DecodeKeyFramesOnly, ParsedFrameParameters->IndependentFrame, SliceHeader->ExtendedPicOrderCnt);

    CalculateDts(ParsedFrameParameters, ParsedVideoParameters);
//
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific override function for doing a decode, we force
//  the appropriate record to be generated in the extended picorder
//  cnt table, then pass to the usual fn.
//
//  Note we only enter the deferred PTS tanble if the queue was sucessful
//

FrameParserStatus_t   FrameParser_VideoHevc_c::RevPlayQueueFrameForDecode(void)
{
    FrameParserStatus_t Status;

    CalculateDts(ParsedFrameParameters, ParsedVideoParameters);

    Status  = FrameParser_Video_c::RevPlayQueueFrameForDecode();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    DeferDFIandPTSGeneration(Buffer, ParsedFrameParameters,
                             ParsedVideoParameters, SliceHeader->ExtendedPicOrderCnt);
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific override function for processing decode stacks, performs the
//  standard play, then reinitializes variables appropriate for the next block.
//

FrameParserStatus_t   FrameParser_VideoHevc_c::RevPlayProcessDecodeStacks(void)
{
    FrameParserStatus_t Status;
    Status  = FrameParser_Video_c::RevPlayProcessDecodeStacks();

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // After a reverse jump, setup the data appropriately
    //
    BehaveAsIfSeenAnIDR     = false;
    PrevPicOrderCntMsb          = 0;
    PrevPicOrderCntLsb          = 0;
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//

FrameParserStatus_t FrameParser_VideoHevc_c::RevPlayGeneratePostDecodeParameterSettings(
    void)
{
    HevcSliceHeader_t *SliceHeader = &((HevcFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SliceHeader;
    ProcessDeferredDFIandPTSDownto(SliceHeader->ExtendedPicOrderCnt);
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//

FrameParserStatus_t FrameParser_VideoHevc_c::RevPlayPurgeQueuedPostDecodeParameterSettings(
    void)
{
    ProcessDeferredDFIandPTSDownto(0ull);
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to add a frame to the reference
//      frame list in reverse play. In hevc this is identical to
//  the forward play mechanism.
//
FrameParserStatus_t FrameParser_VideoHevc_c::RevPlayAppendToReferenceFrameList(
    void)
{
    unsigned long RefFrameIndex;

    //
    // Add current reference frame
    //
    if (SliceHeader->reference_flag || ((unsigned int) SliceHeader->temporal_id < (unsigned int) mSPS->sps_max_sub_layers - 1))
    {
        for (RefFrameIndex = 0; RefFrameIndex < HEVC_MAX_DPB_SIZE; RefFrameIndex++)
            if (! ReferenceFrames[RefFrameIndex].Used)
            {
                ReferenceFrames[RefFrameIndex].Used = true;
                ReferenceFrames[RefFrameIndex].PicOrderCnt_lsb = SliceHeader->pic_order_cnt_lsb;
                ReferenceFrames[RefFrameIndex].PicOrderCnt = SliceHeader->PicOrderCnt;
                ReferenceFrames[RefFrameIndex].ExtendedPicOrderCnt = SliceHeader->ExtendedPicOrderCnt;
                ReferenceFrames[RefFrameIndex].DecodeFrameIndex = ParsedFrameParameters->DecodeFrameIndex;
                ReferenceFrames[RefFrameIndex].IsLongTerm = false;
                break;
            }

        if (RefFrameIndex == HEVC_MAX_DPB_SIZE)
        {
            SE_ERROR("no free slot for new reference frame\n");
            return FrameParserError;
        }
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to remove a frame from the reference
//      frame list in reverse play.
//
//      Note, we only inserted the reference frame in the list on the last
//      field but we need to inform the codec we are finished with it on both
//      fields (for field pictures).
//

FrameParserStatus_t FrameParser_VideoHevc_c::RevPlayRemoveReferenceFrameFromList(
    void)
{
    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE,
                                              CodecFnReleaseReferenceFrame,
                                              ParsedFrameParameters->DecodeFrameIndex);
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to junk the reference frame list
//

FrameParserStatus_t   FrameParser_VideoHevc_c::RevPlayJunkReferenceFrameList(void)
{
    unsigned int RefFrameIndex;

    // Remove all references
    for (RefFrameIndex = 0; RefFrameIndex < HEVC_MAX_DPB_SIZE; RefFrameIndex++)
    {
        ReferenceFrames[RefFrameIndex].Used = false;
    }

    NumReferenceFrames = 0;

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function for reverse play
//
//  I really struggle to explain what this does. Put simply when we jump
//  backwards a sequence at a time, any positive jump in the pic order
//  counts that occurs during the processing of a sequence, needs
//  to be reflected in those frames from the follow on sequence that
//  were held due to unsatisfied reference frames (IE Open groups)
//

FrameParserStatus_t FrameParser_VideoHevc_c::RevPlayNextSequenceFrameProcess(
    void)
{
    HevcSliceHeader_t       *SliceHeader;
    unsigned int             Adjustment;

    SliceHeader = &((HevcFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SliceHeader;
    Adjustment  = LastExitPicOrderCntMsb - SliceHeader->EntryPicOrderCntMsb;

    if (Adjustment != 0)
    {
        ProcessDeferredDFIandPTSDownto(SliceHeader->ExtendedPicOrderCnt);
        SliceHeader->PicOrderCnt        += Adjustment;
        SliceHeader->ExtendedPicOrderCnt    += (long long)Adjustment;
    }

    LastExitPicOrderCntMsb           = SliceHeader->ExitPicOrderCntMsb;

    if (!SliceHeader->ExitPicOrderCntMsbForced)
    {
        LastExitPicOrderCntMsb        += Adjustment;
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      On a slice code, we should have garnered all the data
//      we require we for a frame decode, this function records that fact.
//

FrameParserStatus_t   FrameParser_VideoHevc_c::CommitFrameForDecode(void)
{
// TODO CL must rework all this content !!!
    FrameParserStatus_t         Status;
    HevcSequenceParameterSet_t *SPS;
    PictureStructure_t          PictureStructure;
    SliceType_t                 SliceType;
    MatrixCoefficientsType_t    MatrixCoefficients;
    Rational_t                  PixelAspectRatio;

    if (!SliceHeader->first_mb_in_slice)
    {
        return FrameParserError;
    }

    if (Buffer == NULL)
    {
        // Basic check: before attach stream/frame param to Buffer
        SE_ERROR("No current buffer to commit to decode\n");
        return FrameParserError;
    }

    PictureStructure = StructureFrame;

    // 1 <-- B slice
    // 2 <-- P slice
    // 4 <-- I slice
    // slice_types is obtained by OR operation of all slice types present in picture
    // 1/3/5/7 --> B picture [as B picture can contain I and P type slice as well ]
    // 2/6 --> P picture [as P picture can contain I type slice as well ]
    // 4 --> I picture [I picture can contain only I type slice]
    SliceType           = (slice_types & 0x1) ? SliceTypeB : (slice_types == 0x4) ? SliceTypeI : SliceTypeP;
    PictureStructure        = StructureFrame;
    //
    // Construct stream parameters
    //
    ParsedFrameParameters->NewStreamParameters = NewStreamParametersCheck();
    if (ParsedFrameParameters->NewStreamParameters)
    {
        Status = PrepareNewStreamParameters();

        if (Status != FrameParserNoError)
        {
            SE_ERROR("PrepareNewStreamParameters failed\n");
            return Status;
        }
    }

    ParsedFrameParameters->SizeofStreamParameterStructure = sizeof(HevcStreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure = StreamParameters;
    FrameParameters->SliceHeader.PictureParameterSet = StreamParameters->PictureParameterSet;
    FrameParameters->SliceHeader.VideoParameterSet = StreamParameters->VideoParameterSet;
    SPS = StreamParameters->SequenceParameterSet;
    if (!SPS)
    {
        SE_DEBUG(group_frameparser_video,  "SPS not set!\n");
        return FrameParserError;
    }
    FrameParameters->SliceHeader.SequenceParameterSet = SPS;
    FrameParameters->SliceHeader.pic_order_cnt_lsb = pic_order_cnt_lsb;
//    decode_poc(img,img->sps);
//
//    dec_picture->is_output = false;
//    dec_picture->is_reference = nal_ref_flag;
//    dec_picture->is_long_term = false;
//    dec_picture->pic_order_cnt_lsb = pic_order_cnt_lsb;
//    dec_picture->poc = img->poc;
//
//    build_pic_sets(dpb, (img->short_term_ref_pic_set_idx == img->sps->num_short_term_ref_pic_sets ? &(img->st_rps) : img->sps->st_rps + img->short_term_ref_pic_set_idx),
//            &(img->lt_rps), img->poc, (1<<(img->sps->log2_max_pic_order_cnt_lsb)), img->idr_flag);
//
//    update_dpb_before_decoding(dpb, img->idr_flag, img->no_output_of_prior_pics_flag);
    //
    // Check that the profile and level is supported
    // TODO CL : adapt this to HEVC
    // profile is located in SPS->sps.profile_idc
    //
#if 0

    if ((SPS->sps.profile_idc != HEVC_PROFILE_IDC_MAIN) &&
        (SPS->sps.profile_idc != HEVC_PROFILE_IDC_HIGH))
    {
        SE_ERROR("Unsupported profile (profile_idc = %d, constrained_set1_flag = %d)\n", SPS->profile_idc, SPS->constrained_set1_flag);
        Stream->MarkUnPlayable();
        return FrameParserError;
    }

#endif

    uint8_t ratio_idc = SPS->vui.aspect_ratio_idc;
    if ((ratio_idc > MAX_LEGAL_HEVC_ASPECT_RATIO_CODE) &&
        (ratio_idc != HEVC_ASPECT_RATIO_EXTENDED_SAR))
    {
        SE_ERROR("vui_seq_parameters has illegal value (%02x) for aspect_ratio_idc according to hevc standard\n", ratio_idc);
        // NOTE CL force aspect ratio to unspecified instead of quitting with error
        SPS->vui.aspect_ratio_idc = HEVC_ASPECT_RATIO_UNSPECIFIED;
//    Stream->MarkUnPlayable();
//    return FrameParserHeaderSyntaxError;
    }

    if (SPS->vui.aspect_ratio_idc == HEVC_ASPECT_RATIO_UNSPECIFIED)
    {
        PixelAspectRatio  = DefaultPixelAspectRatio;
    }
    else if (SPS->vui.aspect_ratio_idc == HEVC_ASPECT_RATIO_EXTENDED_SAR)
    {
        if (SPS->vui.sar_height != 0)
        {
            PixelAspectRatio    = Rational_t(SPS->vui.sar_width, SPS->vui.sar_height);
        }
        else
        {
            PixelAspectRatio    = DefaultPixelAspectRatio;
        }
    }
    else
    {
        PixelAspectRatio    = HEVCAspectRatios(SPS->vui.aspect_ratio_idc);
    }

    //
    // Calculate the crop units so any crop adjustment can be applied correctly
    // to frame width/height and pan/scan offsets.
    //
    CalculateCropUnits();
    //
    // Calculate the pic order counts.
    // Must be done before copying the slice header,
    // as these are recorded in there.
    //
    Status  = CalculatePicOrderCnts();

    if (Status != FrameParserNoError)
    {
        SE_ERROR("Failed to calculate pic order counts\n");
        return Status;
    }

    //
    // Fill out the frame parameters structure
    //
    ParsedFrameParameters->NewFrameParameters           = true;
    ParsedFrameParameters->SizeofFrameParameterStructure
        = sizeof(HevcFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure      = FrameParameters;
    //
    // Deduce the matrix coefficients for colour conversions
    //
    // TODO CL : to rework with HEVC standard, for now set BT709 by default
    MatrixCoefficients = MatrixCoefficients_ITU_R_BT709;
    //
    // Record the stream and frame parameters into the appropriate structure
    //
    ParsedFrameParameters->FirstParsedParametersForOutputFrame
        = FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump
        = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;
    ParsedFrameParameters->KeyFrame = SliceHeader->idr_flag || SliceHeader->rap_flag; // add rap_flag to cope with trickmodes
    ParsedFrameParameters->IndependentFrame = SliceHeader->idr_flag || SliceHeader->rap_flag;
    ParsedFrameParameters->ReferenceFrame = SliceHeader->reference_flag;
//
    ParsedVideoParameters->SliceType             = SliceType;
    ParsedVideoParameters->FirstSlice        = true;

    // start new decode on IDR or RAP
    // TODO : handle RASL pictures properly after a RAP (CRA picture for instance)
    if ((FirstDecodeAfterInputJump) || (PlaybackDirection == PlayBackward))
    {
        if (!SliceHeader->rap_flag)
        {
            return FrameParserError;
        }
        else
        {
            // First decodable picture was not an IDR
            mDecodeStartedOnRAP = !SliceHeader->idr_flag;
        }
    }
    // deal with non decodable leading pictures
    if (mDecodeStartedOnRAP && SliceHeader->rasl_flag)
    {
        return FrameParserError;
    }

    // Note: the STHM' computes CRC without sps.conf_win_*
    ParsedVideoParameters->Content.DisplayWidth  = SPS->pic_width_in_luma_samples;
    ParsedVideoParameters->Content.DisplayHeight = SPS->pic_height_in_luma_samples;

    // Display window : Conformance window + Default Display window
    ParsedVideoParameters->Content.DisplayX       = (SliceHeader->CropUnitX * (SPS->conf_win_left_offset + SPS->vui.def_disp_win_left_offset));
    ParsedVideoParameters->Content.DisplayY       = (SliceHeader->CropUnitY * (SPS->conf_win_top_offset + SPS->vui.def_disp_win_top_offset));

    /* Avoid negative value for DisplayWidth (negative value will be perceived high postive as variable DisplayWidth is unsigned integer) */
    if (((SliceHeader->CropUnitX * (SPS->conf_win_left_offset + SPS->vui.def_disp_win_left_offset)) +
         (SliceHeader->CropUnitX * (SPS->conf_win_right_offset + SPS->vui.def_disp_win_right_offset))) < ParsedVideoParameters->Content.DisplayWidth)
    {
        ParsedVideoParameters->Content.DisplayWidth -= (SliceHeader->CropUnitX * (SPS->conf_win_left_offset + SPS->vui.def_disp_win_left_offset));
        ParsedVideoParameters->Content.DisplayWidth -= (SliceHeader->CropUnitX * (SPS->conf_win_right_offset + SPS->vui.def_disp_win_right_offset));
    }

    /* Avoid negative value for DisplayHeight (negative value will be perceived high postive as variable DisplayHeight is unsigned integer) */
    if (((SliceHeader->CropUnitY * (SPS->conf_win_top_offset + SPS->vui.def_disp_win_top_offset)) +
         (SliceHeader->CropUnitY * (SPS->conf_win_bottom_offset + SPS->vui.def_disp_win_bottom_offset)))  < ParsedVideoParameters->Content.DisplayHeight)
    {
        ParsedVideoParameters->Content.DisplayHeight -= (SliceHeader->CropUnitY * (SPS->conf_win_top_offset + SPS->vui.def_disp_win_top_offset));
        ParsedVideoParameters->Content.DisplayHeight -= (SliceHeader->CropUnitY * (SPS->conf_win_bottom_offset + SPS->vui.def_disp_win_bottom_offset));
    }

    ParsedVideoParameters->Content.Width         = ParsedVideoParameters->Content.DisplayWidth;
    ParsedVideoParameters->Content.Height        = ParsedVideoParameters->Content.DisplayHeight;
    ParsedVideoParameters->Content.DecodeWidth   = (SPS->pic_width_in_luma_samples  + 15) & ~15; // according to STHM'::init_dpb()
    ParsedVideoParameters->Content.DecodeHeight  = (SPS->pic_height_in_luma_samples + 15) & ~15; // according to STHM'::init_dpb()
    Status = CheckForResolutionConstraints(ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);

    if (Status != FrameParserNoError)
    {
        SE_ERROR("Unsupported resolution %d x %d\n", ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);
        return Status;
    }

    ParsedVideoParameters->Content.OverscanAppropriate                  = false; // TODO CL : vui overscan_appropriate_flag != 0;
    ParsedVideoParameters->Content.PixelAspectRatio                     = PixelAspectRatio;
    ParsedVideoParameters->Content.VideoFullRange           = false; // TODO CL : vui video_full_range_flag;
    ParsedVideoParameters->Content.ColourMatrixCoefficients
        = MatrixCoefficients;

    StreamEncodedFrameRate = (SPS->vui.num_units_in_tick == 0) ? INVALID_FRAMERATE : Rational_t(SPS->vui.time_scale, SPS->vui.num_units_in_tick);
    //
    // Get the framerate we will actually use.
    //
    ParsedVideoParameters->Content.FrameRate    = ResolveFrameRate();
    ParsedVideoParameters->PictureStructure = PictureStructure;
    // TODO CL : for now assuming hevc is always frame encoded and displayed as progressive
    ParsedVideoParameters->Content.Progressive  = true;
    ParsedVideoParameters->InterlacedFrame  = false;
    ParsedVideoParameters->TopFieldFirst    = true;
    ParsedVideoParameters->DisplayCount[0]  = 1;
    ParsedVideoParameters->DisplayCount[1]  = 0;
#if 0 // TODO CL : manage BaseDpb for extendePOC computation
    BaseDpbAdjustment               = 2;
#endif
    //
    // If this is the first slice, then adjust the BaseDpbValue
    //
#if 0 // TODO CL : manage BaseDpb for extendePOC computation
    BaseDpbValue    += BaseDpbAdjustment;
#endif
    //
    // Record our claim on both the frame and stream parameters
    //
    Buffer->AttachBuffer(StreamParametersBuffer);
    Buffer->AttachBuffer(FrameParametersBuffer);
// TODO CL : Handle this later
#if 0
    //
    // Check whether or not we can safely handle reverse play,
    //      Cannot reverse with B reference frames
    //

    if (Configuration.SupportSmoothReversePlay
        && (ParsedFrameParameters->ReferenceFrame && (SliceType
                                                      == SliceTypeB)))
    {
        Configuration.SupportSmoothReversePlay  = false;
        Stream->OutputTimer->SetSmoothReverseSupport(Configuration.SupportSmoothReversePlay);
        SE_INFO(group_frameparser_video, "Detected inability to handle smooth reverse\n");
    }

#endif
    //
    // Finally set the appropriate flag and return
    //
    FrameToDecode           = true;
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Boolean function to evaluate whether or not the stream
//      parameters are new.
//

bool   FrameParser_VideoHevc_c::NewStreamParametersCheck(void)
{
    bool Different = false;

    //
    // If we haven't got any previous then they must be new.
    //

    if ((StreamParameters == NULL) || (SliceHeader == NULL))
    {
        return true;
    }

    //
    // If we haven't overwritten any, and the pointers are unchanged, then
    // they must the same, otherwise do a mem compare to check for differences.
    //
    if (ReadNewSPS || (StreamParameters->SequenceParameterSet
                       != SliceHeader->SequenceParameterSet))
    {
        if (SliceHeader->SequenceParameterSet)
        {
            Different = memcmp(&CopyOfSequenceParameterSet,
                               SliceHeader->SequenceParameterSet,
                               sizeof(HevcSequenceParameterSet_t)) != 0;
        }
        else
        {
            Different = true;
            goto bail;
        }
    }

    if (!Different && (ReadNewVPS || (StreamParameters->VideoParameterSet
                                      != SliceHeader->VideoParameterSet)))
    {
        if (SliceHeader->VideoParameterSet)
        {
            Different = memcmp(&CopyOfVideoParameterSet,
                               SliceHeader->VideoParameterSet,
                               sizeof(HevcVideoParameterSet_t)) != 0;
        }
        else
        {
            Different = true;
            goto bail;
        }
    }

    if (!Different && (ReadNewPPS || (StreamParameters->PictureParameterSet
                                      != SliceHeader->PictureParameterSet)))
    {
        if (SliceHeader->PictureParameterSet)
        {
            Different = memcmp(&CopyOfPictureParameterSet,
                               SliceHeader->PictureParameterSet,
                               sizeof(HevcPictureParameterSet_t)) != 0;
        }
        else
        {
            Different = true;
            goto bail;
        }
    }
    //
    // Do we need to copy over new versions
    //

    if (Different && SliceHeader->SequenceParameterSet && SliceHeader->VideoParameterSet && SliceHeader->PictureParameterSet)
    {
        memcpy(&CopyOfSequenceParameterSet, SliceHeader->SequenceParameterSet,
               sizeof(HevcSequenceParameterSet_t));
        memcpy(&CopyOfVideoParameterSet, SliceHeader->VideoParameterSet,
               sizeof(HevcVideoParameterSet_t));
        memcpy(&CopyOfPictureParameterSet, SliceHeader->PictureParameterSet,
               sizeof(HevcPictureParameterSet_t));
    }
    else
    {
        //
        // Reduces the number of times we do the check when streams
        // resend the same picture parameter set, without changing it.
        //
        StreamParameters->SequenceParameterSet = SliceHeader->SequenceParameterSet;
        StreamParameters->VideoParameterSet    = SliceHeader->VideoParameterSet;
        StreamParameters->PictureParameterSet  = SliceHeader->PictureParameterSet;
    }

bail:
    ReadNewSPS = false;
    ReadNewVPS = false;
    ReadNewPPS = false;
    return (Different);
}

// /////////////////////////////////////////////////////////////////////////
//
//      Boolean function to evaluate whether or not the stream
//      parameters are new.
//

FrameParserStatus_t   FrameParser_VideoHevc_c::PrepareNewStreamParameters(void)
{
    uint8_t seq_parameter_set_id, video_parameter_set_id;

    FrameParserStatus_t Status = GetNewStreamParameters((void **)(&StreamParameters));
    if (Status != FrameParserNoError)
    {
        return Status;
    }

    SE_ASSERT(PictureParameterSetTable[pic_parameter_set_id].Buffer != NULL);
    PictureParameterSetTable[pic_parameter_set_id].Buffer->ObtainDataReference(
        NULL, NULL, (void **)(&StreamParameters->PictureParameterSet));
    SE_ASSERT(StreamParameters->PictureParameterSet != NULL);

    seq_parameter_set_id = PictureParameterSetTable[pic_parameter_set_id].PPS->pps_seq_parameter_set_id;
    SE_ASSERT(SequenceParameterSetTable[seq_parameter_set_id].Buffer != NULL);

    SequenceParameterSetTable[seq_parameter_set_id].Buffer->ObtainDataReference(
        NULL, NULL, (void **)(&StreamParameters->SequenceParameterSet));
    SE_ASSERT(StreamParameters->SequenceParameterSet != NULL);

    video_parameter_set_id = SequenceParameterSetTable[seq_parameter_set_id].SPS->sps_video_parameter_set_id;
    SE_ASSERT(VideoParameterSetTable[video_parameter_set_id].Buffer != NULL);

    VideoParameterSetTable[video_parameter_set_id].Buffer->ObtainDataReference(
        NULL, NULL, (void **)(&StreamParameters->VideoParameterSet));
    SE_ASSERT(StreamParameters->VideoParameterSet != NULL);

    //
    // Attach the picture parameter set, the sequence parameter set and the video parameter set
    //
    StreamParametersBuffer->AttachBuffer(PictureParameterSetTable [pic_parameter_set_id].Buffer);
    StreamParametersBuffer->AttachBuffer(SequenceParameterSetTable[seq_parameter_set_id].Buffer);
    StreamParametersBuffer->AttachBuffer(VideoParameterSetTable[video_parameter_set_id].Buffer);

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to insert an entry into the deferred DFI/PTS list
//  in an ordered position
//

void FrameParser_VideoHevc_c::DeferDFIandPTSGeneration(Buffer_t Buffer,
                                                       ParsedFrameParameters_t *ParsedFrameParameters,
                                                       ParsedVideoParameters_t *ParsedVideoParameters,
                                                       unsigned long long   ExtendedPicOrderCnt)
{
// TODO CL must rework all this content, note there is no field encoding in HEVC and frames are always progresive
    unsigned int    i, deferred_idx;
    unsigned int    Index;
//unsigned char   SpecialCaseDpb;
    unsigned int    MaxDeferals;
    unsigned int    HeldReferenceFrames;
#if 0 // TODO CL what was this policy for exactly in H264 ???

    if (ParsedFrameParameters->ReferenceFrame)
    {
        SpecialCaseDpb      = Player->PolicyValue(Playback, Stream, PolicyH264TreatDuplicateDpbValuesAsNonReferenceFrameFirst);

        if (SpecialCaseDpb == PolicyValueApply)
        {
            ExtendedPicOrderCnt++;
        }
    }

#endif
    //
    // Fill in our list entry
    //
    Buffer->IncrementReferenceCount();
    Buffer->GetIndex(&Index);

    if (mDeferredList[Index].Buffer != NULL)
    {
        SE_ERROR("DeferredList[%d].Buffer already released\n", Index);
    }

    mDeferredList[Index].Buffer          = Buffer;
    mDeferredList[Index].ParsedFrameParameters   = ParsedFrameParameters;
    mDeferredList[Index].ParsedVideoParameters   = ParsedVideoParameters;
    mDeferredList[Index].ExtendedPicOrderCnt = ExtendedPicOrderCnt;

    //
    // Where do we sit
    //

    for (i = 0; i < mDeferredListEntries; i++)
    {
        deferred_idx = mOrderedDeferredList[i];
        if (deferred_idx == INVALID_INDEX)
        {
            continue;
        }
        if (ExtendedPicOrderCnt < mDeferredList[deferred_idx].ExtendedPicOrderCnt)
        {
            break;
        }
    }

    if (i < mDeferredListEntries)
    {
        memmove(&mOrderedDeferredList[i + 1], &mOrderedDeferredList[i], (mDeferredListEntries - i) * sizeof(unsigned int));
    }

    mOrderedDeferredList[i]  = Index;
    mDeferredListEntries++;
    //
    // Perform PTS validation - check for a jump in PTS of the wrong direction
    //              then range check it to less than 1 second to
    //              allow a major jump (IE loop, add insert etc)
    //

    if ((ValidTime(ParsedFrameParameters->NormalizedPlaybackTime) && ValidPTSSequence))
    {
#define INVALID_PTS_SEQUENCE_THRESHOLD 1000000
        unsigned long long  PTS = ParsedFrameParameters->NormalizedPlaybackTime;
        ParsedFrameParameters_t    *tmpPFP;
        // Check those before me in the list
        if (i != 0)
        {
            deferred_idx = mOrderedDeferredList[i - 1];
            if (deferred_idx != INVALID_INDEX)
            {
                tmpPFP = mDeferredList[deferred_idx].ParsedFrameParameters;
                if (ValidTime(tmpPFP->NormalizedPlaybackTime) &&
                    (PTS < tmpPFP->NormalizedPlaybackTime) &&
                    ((tmpPFP->NormalizedPlaybackTime - PTS) < INVALID_PTS_SEQUENCE_THRESHOLD))
                {
                    ValidPTSSequence = false;
                }
            }
            else
            {
                ValidPTSSequence = false;
            }
        }
        // Check those after me in the list
        if (i < (mDeferredListEntries - 1))
        {
            deferred_idx = mOrderedDeferredList[i + 1];
            if (deferred_idx != INVALID_INDEX)
            {
                tmpPFP = mDeferredList[deferred_idx].ParsedFrameParameters;
                if (ValidTime(tmpPFP->NormalizedPlaybackTime) &&
                    (PTS > tmpPFP->NormalizedPlaybackTime) &&
                    ((PTS - tmpPFP->NormalizedPlaybackTime) < INVALID_PTS_SEQUENCE_THRESHOLD))
                {
                    ValidPTSSequence    = false;
                }
            }
            else
            {
                ValidPTSSequence = false;
            }
        }
    }
    else
    {
        // Current frame has invalid PTS
        ValidPTSSequence    = false;
    }

#if 0 // TODO CL : for now no SEI handling then no dpb_output_delay available
    if ((Player->PolicyValue(Playback, Stream, PolicyH264ValidateDpbValuesAgainstPTSValues) == PolicyValueApply) &&
        !ValidPTSSequence && !DpbValuesInvalidatedByPTS)
    {
        SE_ERROR("HEvc Dpb values incompatible with PTS ordering, falling back to frame re-ordering based on PicOrderCnt values\n");
        DpbValuesInvalidatedByPTS        = true;
        DisplayOrderByDpbValues      = false;
        PicOrderCntOffset           += PicOrderCntOffsetAdjust;
    }
#endif

    //
    // Limit the deferral process with respect to the number of available decode buffers
    //
    //      Start with the basic buffer count,
    //      Subtract 2 for buffers committed to manifestor (display and de-interlace)
    //      Then for each reference frame already allocated a display index, but held
    //      as a reference subtract one more.
    //

    if (PlaybackDirection == PlayForward)
    {
        Stream->GetDecodeBufferManager()->GetEstimatedBufferCount(&MaxDeferals);
    }
    else
    {
        MaxDeferals   = HEVC_CODED_FRAME_COUNT;
    }

// TODO CL : Not yet quite understood part taken from H264 and reused here....
#define FRAMES_HELD_BY_MANIFESTOR   0
    HeldReferenceFrames  = 0;

    //SE_INFO(group_frameparser_video,  "Defer - %4d, Ref = %4d, Max = %4d\n", mDeferredListEntries, HeldReferenceFrames, MaxDeferals );

    if ((mDeferredListEntries + FRAMES_HELD_BY_MANIFESTOR + HeldReferenceFrames) >= MaxDeferals)
    {
        SE_ERROR("Unable to defer, too many outstanding\nThere may be too few decode buffers to handle this stream (%d + %d + %d >= %d)\n",
                 mDeferredListEntries, FRAMES_HELD_BY_MANIFESTOR, HeldReferenceFrames, MaxDeferals);

        if (PlaybackDirection == PlayForward)
        {
            ProcessDeferredDFIandPTSUpto((mDeferredList[mOrderedDeferredList[0]].ExtendedPicOrderCnt) + 1);
        }
        else
        {
            ProcessDeferredDFIandPTSDownto((mDeferredList[mOrderedDeferredList[mDeferredListEntries - 1]].ExtendedPicOrderCnt) + 1);
        }
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to process entries in the deferred DFI/PTS list
//

void FrameParser_VideoHevc_c::ProcessDeferredDFIandPTSUpto(
    unsigned long long ExtendedPicOrderCnt)
{
// TODO CL must rework all this content, note there is no field encoding in HEVC and frames are always progresive
    unsigned int       i, j;
    unsigned int       Index;
    unsigned int       LeastPlaybackTimeIndex;
    unsigned long long LeastPlaybackTime;
    unsigned int       deferred_idx;

    i = 0;
    while (i < mDeferredListEntries)
    {
        deferred_idx = mOrderedDeferredList[i];
        if (deferred_idx == INVALID_INDEX)
        {
            i++;
            continue;
        }

        if (mDeferredList[deferred_idx].ExtendedPicOrderCnt >= ExtendedPicOrderCnt)
        {
            break;
        }

        //
        // Check through the PTS values, is anyone earlier
        //
        LeastPlaybackTimeIndex  = i;
        LeastPlaybackTime   = mDeferredList[deferred_idx].ParsedFrameParameters->NormalizedPlaybackTime;

        if (ValidTime(LeastPlaybackTime) && ValidPTSSequence)
        {
            for (j = i + 1; j < mDeferredListEntries; j++)
                if ((mOrderedDeferredList[j] != INVALID_INDEX) &&
                    ValidTime(mDeferredList[mOrderedDeferredList[j]].ParsedFrameParameters->NormalizedPlaybackTime) &&
                    (mDeferredList[mOrderedDeferredList[j]].ParsedFrameParameters->NormalizedPlaybackTime < LeastPlaybackTime))
                {
                    LeastPlaybackTimeIndex  = j;
                    LeastPlaybackTime       = mDeferredList[mOrderedDeferredList[j]].ParsedFrameParameters->NormalizedPlaybackTime;
                }
        }

        //
        // Process the appropriate one
        //
        Index = mOrderedDeferredList[LeastPlaybackTimeIndex];
        CalculateFrameIndexAndPts(mDeferredList[Index].ParsedFrameParameters, mDeferredList[Index].ParsedVideoParameters);
// TODO CL must rework SetupPanScanValues Later
//  SetupPanScanValues( DeferredList[Index].ParsedFrameParameters, mDeferredList[Index].ParsedVideoParameters );

        if (mDeferredList[Index].Buffer != NULL)
        {
            mDeferredList[Index].Buffer->DecrementReferenceCount();
        }
        else
        {
            SE_ERROR("DeferredList[%d].Buffer already released\n", Index);
        }
        mDeferredList[Index].Buffer  = NULL;

        //
        // Now move on if we did not find an earlier one, otherwise clear that entry in the list
        //

        if (LeastPlaybackTimeIndex == i)
        {
            i++;
        }
        else
        {
            mOrderedDeferredList[LeastPlaybackTimeIndex]   = INVALID_INDEX;
        }
    }

    //
    // Shuffle up the ordered list
    //
    if (i != 0)
    {
        for (j = 0; i < mDeferredListEntries; i++)
        {
            if (mOrderedDeferredList[i] != INVALID_INDEX)
            {
                mOrderedDeferredList[j++] = mOrderedDeferredList[i];
            }
        }
        mDeferredListEntries = j;
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to process entries in the deferred DFI/PTS list
//

void FrameParser_VideoHevc_c::ProcessDeferredDFIandPTSDownto(
    unsigned long long ExtendedPicOrderCnt)
{
// TODO CL must rework all this content, note there is no field encoding in HEVC and frames are always progresive
    unsigned int       i, j;
    unsigned int       Index;
    unsigned int       GreatestPlaybackTimeIndex;
    unsigned long long GreatestPlaybackTime;
    unsigned int       deferred_idx;

    while (mDeferredListEntries > 0)
    {
        deferred_idx = mOrderedDeferredList[mDeferredListEntries - 1];
        if (deferred_idx == INVALID_INDEX)
        {
            mDeferredListEntries--;
            continue;
        }

        if (mDeferredList[deferred_idx].ExtendedPicOrderCnt <= ExtendedPicOrderCnt)
        {
            break;
        }
        //
        // Check through the PTS values, is anyone later
        //
        GreatestPlaybackTimeIndex   = mDeferredListEntries - 1;
        GreatestPlaybackTime        = mDeferredList[deferred_idx].ParsedFrameParameters->NormalizedPlaybackTime;

        if (ValidTime(GreatestPlaybackTime))
        {
            for (i = 0; i < (mDeferredListEntries - 1); i++)
                if ((mOrderedDeferredList[i] != INVALID_INDEX) &&
                    ValidTime(mDeferredList[mOrderedDeferredList[i]].ParsedFrameParameters->NormalizedPlaybackTime) &&
                    (mDeferredList[mOrderedDeferredList[i]].ParsedFrameParameters->NormalizedPlaybackTime > GreatestPlaybackTime))
                {
                    GreatestPlaybackTimeIndex   = i;
                    GreatestPlaybackTime    = mDeferredList[mOrderedDeferredList[i]].ParsedFrameParameters->NormalizedPlaybackTime;
                }
        }

        //
        // Process the appropriate one
        //
        Index = mOrderedDeferredList[GreatestPlaybackTimeIndex];
        CalculateFrameIndexAndPts(mDeferredList[Index].ParsedFrameParameters, mDeferredList[Index].ParsedVideoParameters);
// TODO CL must rework SetupPanScanValues Later
//  SetupPanScanValues( mDeferredList[Index].ParsedFrameParameters, mDeferredList[Index].ParsedVideoParameters );
        mDeferredList[Index].Buffer->DecrementReferenceCount();
        mDeferredList[Index].Buffer  = NULL;

        //
        // Now move on if we did not find an earlier one, otherwise clear that entry in the list
        //

        if (GreatestPlaybackTimeIndex == (mDeferredListEntries - 1))
        {
            mDeferredListEntries--;
        }
        else
        {
            mOrderedDeferredList[GreatestPlaybackTimeIndex]    = INVALID_INDEX;
        }
    }

    //
    // Shuffle down the ordered list (just closing any invalid indices up)
    //
    if (mDeferredListEntries != 0)
    {
        for (i = j = 0; i < mDeferredListEntries; i++)
        {
            if (mOrderedDeferredList[i] != INVALID_INDEX)
            {
                mOrderedDeferredList[j++] = mOrderedDeferredList[i];
            }
        }
        mDeferredListEntries = j;
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to process the pan scan parameters in output order
//

// Ignore this method for now
// TODO CL must rework all this content !!!
#if 0
void   FrameParser_VideoHevc_c::SetupPanScanValues(ParsedFrameParameters_t   *ParsedFrameParameters,
                                                   ParsedVideoParameters_t  *ParsedVideoParameters)
{
    unsigned int                     i;
    bool                             PanScanIsOn;
    HevcSliceHeader_t               *SliceHeader;
    HevcSEIPanScanRectangle_t       *SEIPanScanRectangle;
    unsigned int                     FrameWidthInMbs;
    unsigned int                     FrameHeightInMbs;
    unsigned int                     Left;
    unsigned int                     Right;
    unsigned int                     Top;
    unsigned int                     Bottom;
//
    SliceHeader     = &((HevcFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SliceHeader;
    SEIPanScanRectangle = &((HevcFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SEIPanScanRectangle;

    //
    // Process the message
    //

    if (SEIPanScanRectangle->Valid &&
        (inrange(SEIPanScanRectangle->pan_scan_rect_id, 0, 255) || inrange(SEIPanScanRectangle->pan_scan_rect_id, 512, 0x7fffffff)))
    {
        //
        // Is this just a cancel flag
        //
        if (SEIPanScanRectangle->pan_scan_rect_cancel_flag)
        {
            PanScanState.Count          = 0;
        }
        else
        {
            PanScanState.Count                  = SEIPanScanRectangle->pan_scan_cnt_minus1 + 1;
            PanScanState.RepetitionPeriod       = SEIPanScanRectangle->pan_scan_rect_repetition_period;
            FrameWidthInMbs     = SliceHeader->SequenceParameterSet->pic_width_in_mbs_minus1 + 1;
            FrameHeightInMbs    = ((2 - SliceHeader->SequenceParameterSet->frame_mbs_only_flag) *
                                   (SliceHeader->SequenceParameterSet->pic_height_in_map_units_minus1 + 1)) /
                                  (1 + SliceHeader->field_pic_flag);

            for (i = 0; i < PanScanState.Count; i++)
            {
                // The pan-scan rectangle coordinates are relative to the cropping rectangle of the SPS.
                // They are expressed in 1/16th of luma sample.
                Left    = 16 * SliceHeader->CropUnitX * SliceHeader->SequenceParameterSet->frame_cropping_rect_left_offset +
                          SEIPanScanRectangle->pan_scan_rect_left_offset[i];
                Right   = 16 * (16 * FrameWidthInMbs - SliceHeader->CropUnitX * SliceHeader->SequenceParameterSet->frame_cropping_rect_right_offset) +
                          SEIPanScanRectangle->pan_scan_rect_right_offset[i]; // -1 ommited to simplify PanScanState.Width calculation
                Top     = 16 * SliceHeader->CropUnitY * SliceHeader->SequenceParameterSet->frame_cropping_rect_top_offset +
                          SEIPanScanRectangle->pan_scan_rect_top_offset[i];
                Bottom  = 16 * (16 * FrameHeightInMbs - SliceHeader->CropUnitY * SliceHeader->SequenceParameterSet->frame_cropping_rect_bottom_offset) +
                          SEIPanScanRectangle->pan_scan_rect_bottom_offset[i]; // -1 ommited to simplify PanScanState.Height calculation

                PanScanState.Width[i]               = Right - Left;
                PanScanState.Height[i]              = Bottom - Top;
                PanScanState.HorizontalOffset[i]    = Left;
                PanScanState.VerticalOffset[i]      = Top;
            }
        }
    }

    //
    // setup the values
    //
    ParsedVideoParameters->PanScanCount = PanScanIsOn ? PanScanState.Count : 0;

    for (i = 0; i < ParsedVideoParameters->PanScanCount; i++)
    {
        ParsedVideoParameters->PanScan[i].DisplayCount      = 1;
        ParsedVideoParameters->PanScan[i].Width             = PanScanState.Width[i] / 16;
        ParsedVideoParameters->PanScan[i].Height            = PanScanState.Height[i] / 16;
        ParsedVideoParameters->PanScan[i].HorizontalOffset  = PanScanState.HorizontalOffset[i];
        ParsedVideoParameters->PanScan[i].VerticalOffset    = PanScanState.VerticalOffset[i];
    }

    //
    // Do we want to turn it off
    //

    if (PanScanState.RepetitionPeriod == 0)
    {
        PanScanState.Count    = 0;
    }
}
#endif

// /////////////////////////////////////////////////////////////////////////
//
//      Function to reset the frame packing arrangement parameters and the 3D property
//

// Ignore this method for now
// TODO CL must rework all this content !!!
#if 0
void   FrameParser_VideoHevc_c::ResetPictureFPAValuesAnd3DVideoProperty(ParsedVideoParameters_t  *ParsedVideoParameters)
{
    FramePackingArrangementState.Output3DVideoProperty.Stream3DFormat = Stream3DNone;
    FramePackingArrangementState.Output3DVideoProperty.Frame0IsLeft = true;
    FramePackingArrangementState.Output3DVideoProperty.IsFrame0 = true;
    /* reset internal variable for the persistence of the frame packing arrangement settings */
    FramePackingArrangementState.RepetitionPeriod = 1;
    FramePackingArrangementState.Persistence = 0;
    FramePackingArrangementState.CanResetFPASetting = false;
}
#endif

// /////////////////////////////////////////////////////////////////////////
//
//      Function to process the frame packing arrangement parameters in output order
//

// Ignore this method for now
// TODO CL must rework all this content !!!
#if 0
void   FrameParser_VideoHevc_c::SetupFramePackingArrangementValues(ParsedFrameParameters_t  *ParsedFrameParameters,
                                                                   ParsedVideoParameters_t  *ParsedVideoParameters)
{
    HevcSEIFramePackingArrangement_t       *SEIFramePackingArrangement;
    SEIFramePackingArrangement  = &((HevcFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SEIFramePackingArrangement;

    //
    // Process the message
    //
    if (SEIFramePackingArrangement->Valid)
    {
        if (SEIFramePackingArrangement->frame_packing_arr_cancel_flag)
        {
            ResetPictureFPAValuesAnd3DVideoProperty(ParsedVideoParameters);
        }
        else
        {
            FramePackingArrangementState.RepetitionPeriod = SEIFramePackingArrangement->frame_packing_arr_repetition_period;
            FramePackingArrangementState.CanResetFPASetting = (FramePackingArrangementState.RepetitionPeriod > 1) ? true : false;
            FramePackingArrangementState.Output3DVideoProperty.Frame0IsLeft = (SEIFramePackingArrangement->frame_packing_arr_content_interpretation_type == 2) ? false : true;

            switch (SEIFramePackingArrangement->frame_packing_arr_type)
            {
            case HEVC_FRAME_PACKING_ARRANGEMENT_TYPE_SIDE_BY_SIDE :
                FramePackingArrangementState.Output3DVideoProperty.Stream3DFormat = Stream3DFormatSidebysideHalf;
                break;

            case HEVC_FRAME_PACKING_ARRANGEMENT_TYPE_TOP_BOTTOM :
                FramePackingArrangementState.Output3DVideoProperty.Stream3DFormat = Stream3DFormatStackedHalf;
                break;

            case HEVC_FRAME_PACKING_ARRANGEMENT_TYPE_FRAME_SEQUENTIAL :
                if (!SEIFramePackingArrangement->frame_packing_arr_quincunx_sampling_flag)
                {
                    FramePackingArrangementState.Output3DVideoProperty.Stream3DFormat = Stream3DFormatFrameSeq;
                    FramePackingArrangementState.Output3DVideoProperty.IsFrame0 = SEIFramePackingArrangement->frame_packing_arr_current_frame_is_frame0_flag;
                    break;
                }
                else
                {
                    SE_INFO(group_frameparser_video,  "Stream conformance requires that quincunx_sampling_flag must be 0 for frame sequential SEI message\n");
                }

            default :
                FramePackingArrangementState.Output3DVideoProperty.Stream3DFormat = Stream3DNone;
                break;
            }
        }
    }

    ParsedVideoParameters->Content.Output3DVideoProperty.Frame0IsLeft = FramePackingArrangementState.Output3DVideoProperty.Frame0IsLeft;
    ParsedVideoParameters->Content.Output3DVideoProperty.Stream3DFormat = FramePackingArrangementState.Output3DVideoProperty.Stream3DFormat;
    ParsedVideoParameters->Content.Output3DVideoProperty.IsFrame0 = FramePackingArrangementState.Output3DVideoProperty.IsFrame0;
}
#endif

// Ignore this method for now
// TODO CL must rework all this content !!!
#if 0
void   FrameParser_VideoHevc_c::ReportFramePackingArrangementValues(ParsedVideoParameters_t  *ParsedVideoParameters)
{
    SE_INFO(group_frameparser_video,  "ReportFramePackingArrangementValues\n");

    switch (ParsedVideoParameters->Content.Output3DVideoProperty.Stream3DFormat)
    {
    case Stream3DFormatSidebysideHalf :
        SE_INFO(group_frameparser_video,  " 3D format:                  %3d\n", ParsedVideoParameters->Content.Output3DVideoProperty.Stream3DFormat);
        SE_INFO(group_frameparser_video,  " 3D Frame0IsLeft:            %3d\n", ParsedVideoParameters->Content.Output3DVideoProperty.Frame0IsLeft);
        break;

    case Stream3DFormatStackedHalf :
        SE_INFO(group_frameparser_video,  " 3D format:                  %3d\n", ParsedVideoParameters->Content.Output3DVideoProperty.Stream3DFormat);
        SE_INFO(group_frameparser_video,  " 3D Frame0IsLeft:            %3d\n", ParsedVideoParameters->Content.Output3DVideoProperty.Frame0IsLeft);
        break;

    case Stream3DFormatFrameSeq :
        SE_INFO(group_frameparser_video,  " 3D format:                  %3d\n", ParsedVideoParameters->Content.Output3DVideoProperty.Stream3DFormat);
        SE_INFO(group_frameparser_video,  " 3D Frame0IsLeft:            %3d\n", ParsedVideoParameters->Content.Output3DVideoProperty.Frame0IsLeft);
        SE_INFO(group_frameparser_video,  " 3D IsFrame0:                %3d\n", ParsedVideoParameters->Content.Output3DVideoProperty.IsFrame0);

    default :
        SE_INFO(group_frameparser_video,  " 3D format:                  %3d\n", ParsedVideoParameters->Content.Output3DVideoProperty.Stream3DFormat);
        break;
    }

    SE_INFO(group_frameparser_video,  " 3D RepetitionPeriod:       %3d\n", FramePackingArrangementState.RepetitionPeriod);
    SE_INFO(group_frameparser_video,  " 3D Persistence:            %3d\n", FramePackingArrangementState.Persistence);
    SE_INFO(group_frameparser_video,  " 3D CanResetFPASetting:     %3d\n", FramePackingArrangementState.CanResetFPASetting);
}
#endif

// /////////////////////////////////////////////////////////////////////////
//
//      Function to process the pan scan parameters in output order
//

void   FrameParser_VideoHevc_c::CalculateCropUnits(void)
{
// The variables CropUnitX and CropUnitY are derived as follows (table 6.1 nom) :
//     CropUnitX = SubWidthC
//     CropUnitY = SubHeightC

    switch (SliceHeader->SequenceParameterSet->chroma_format_idc)
    {
    case 0:
        SliceHeader->CropUnitX          = 1;
        SliceHeader->CropUnitY          = 1;
        break;

    case 1:
        SliceHeader->CropUnitX          = 2;
        SliceHeader->CropUnitY          = 2;
        break;

    case 2:
        SliceHeader->CropUnitX          = 2;
        SliceHeader->CropUnitY          = 1;
        break;

    case 3:
        SliceHeader->CropUnitX          = 1;
        SliceHeader->CropUnitY          = 1;
        break;

    default:
        SE_ERROR("Unknown chroma_format_idc value (%d) \n", SliceHeader->SequenceParameterSet->chroma_format_idc);
        break;

    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to process the user data parameters specific to hevc
//

bool FrameParser_VideoHevc_c::ReadAdditionalUserDataParameters(void)
{
    if (ParsedFrameParameters == NULL)
    {
        return false;
    }

    UserDataAdditionalParameters_t *UserDataAdditionalParam = &AccumulatedUserData[ParsedFrameParameters->UserDataNumber].UserDataAdditionalParameters;

    UserDataAdditionalParam->Available = true;
    UserDataAdditionalParam->Length = sizeof(HevcUserDataParameters_t);
    // set the codec ID
    AccumulatedUserData[ParsedFrameParameters->UserDataNumber].UserDataGenericParameters.CodecId = USER_DATA_HEVC_CODEC_ID;
    memcpy(&(UserDataAdditionalParam->CodecUserDataParameters.HevcUserDataParameters), &mUserData, sizeof(HevcUserDataParameters_t));
    return true;
}

