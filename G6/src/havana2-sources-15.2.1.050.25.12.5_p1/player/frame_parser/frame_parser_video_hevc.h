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

#ifndef H_FRAME_PARSER_VIDEO_HEVC
#define H_FRAME_PARSER_VIDEO_HEVC

#include "hevc.h"
#include "frame_parser_video.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoHevc_c"

// from defines.h
//! Component identifiers
enum
{
    Lu,  //!< Luma component
    Ch,  //!< Chroma (blue and red) component
    Cb = Ch, //!< Chroma blue component
    Cr, //!< Chroma red component
    NUM_COMPONENTS //!< Number of components
};
//parset.h

// TODO CL rework/refine these define
#define DEFAULT_ANTI_EMULATION_REQUEST                  32
#define SCALING_LIST_ANTI_EMULATION_REQUEST             128
#define HRD_PARAMETERS_ANTI_EMULATION_REQUEST           64
#define VUI_PARAMETERS_ANTI_EMULATION_REQUEST           32
#define SPS_EXTENSION_ANTI_EMULATION_REQUEST            16
#define REF_LIST_REORDER_ANTI_EMULATION_REQUEST         128
#define PRED_WEIGHT_TABLE_ANTI_EMULATION_REQUEST        256
#define MEM_MANAGEMENT_ANTI_EMULATION_REQUEST           64
#define HEVC_MAX_FRAME_WIDTH                            4096
#define HEVC_MAX_FRAME_HEIGHT                           2400

// TODO: VF Temporary until we have levels and all
#define HEVC_MAX_DPB_SIZE 20

typedef struct HevcSequenceParameterSetEntry_s
{
    Buffer_t                              Buffer;
    HevcSequenceParameterSet_t            *SPS;
} HevcSequenceParameterSetEntry_t;

//

typedef struct HevcPictureParameterSetEntry_s
{
    Buffer_t                      Buffer;
    HevcPictureParameterSet_t     *PPS;
} HevcPictureParameterSetEntry_t;

typedef struct HevcVideoParameterSetEntry_s
{
    Buffer_t                      Buffer;
    HevcVideoParameterSet_t      *VPS;
} HevcVideoParameterSetEntry_t;

typedef struct HevcReferenceFrameData_s
{
    bool            Used;
    unsigned int        DecodeFrameIndex;
    int                 PicOrderCnt_lsb;
    int                 PicOrderCnt;
    unsigned long long  ExtendedPicOrderCnt;
    bool        IsLongTerm;
} HevcReferenceFrameData_t;

//
// TODO CL DPB reordering still to be clarified but I guess it's safer to keep the same logical as H264 parser...
typedef struct HevcDeferredDFIandPTSList_s
{
    Buffer_t                     Buffer;
    ParsedFrameParameters_t     *ParsedFrameParameters;
    ParsedVideoParameters_t     *ParsedVideoParameters;
    unsigned long long           ExtendedPicOrderCnt;
} HevcDeferredDFIandPTSList_t;


// TODO CL P&S management to be addressed later...
#if 0
typedef struct PanScanState_s
{
    unsigned int        Count;
    unsigned int        RepetitionPeriod;
    unsigned int        Width[HEVC_SEI_MAX_PAN_SCAN_VALUES];
    unsigned int        Height[HEVC_SEI_MAX_PAN_SCAN_VALUES];
    int                 HorizontalOffset[HEVC_SEI_MAX_PAN_SCAN_VALUES];
    int                 VerticalOffset[HEVC_SEI_MAX_PAN_SCAN_VALUES];
} PanScanState_t;
#endif

// TODO CL 3D management to be addressed later if actually necessary...
#if 0
typedef struct FramePackingArrangementState_s
{
    int                         Persistence;
    int                         RepetitionPeriod;
    bool                        CanResetFPASetting;
    Output3DVideoProperty_t     Output3DVideoProperty;
} FramePackingArrangementState_t;
#endif

class FrameParser_VideoHevc_c : public FrameParser_Video_c
{
public:

    //
    // Constructor function
    //

    FrameParser_VideoHevc_c(void);
    ~FrameParser_VideoHevc_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);

    FrameParserStatus_t   ResetCollatedHeaderState(void);
    unsigned int          RequiredPresentationLength(unsigned char        StartCode);
    FrameParserStatus_t   PresentCollatedHeader(unsigned char         StartCode,
                                                unsigned char        *HeaderBytes,
                                                FrameParserHeaderFlag_t  *Flags);

    //
    // Stream specific functions
    //

    FrameParserStatus_t   ReadHeaders(void);
    FrameParserStatus_t   ResetReferenceFrameList(void);
    FrameParserStatus_t   PrepareReferenceFrameList(void);
    FrameParserStatus_t   ForPlayUpdateReferenceFrameList(void);

    FrameParserStatus_t   ForPlayProcessQueuedPostDecodeParameterSettings(void);
    FrameParserStatus_t   ForPlayGeneratePostDecodeParameterSettings(void);
    FrameParserStatus_t   ForPlayPurgeQueuedPostDecodeParameterSettings(void);

    FrameParserStatus_t   RevPlayQueueFrameForDecode(void);
    FrameParserStatus_t   RevPlayProcessDecodeStacks(void);
    FrameParserStatus_t   RevPlayGeneratePostDecodeParameterSettings(void);
    FrameParserStatus_t   RevPlayPurgeQueuedPostDecodeParameterSettings(void);
    FrameParserStatus_t   RevPlayAppendToReferenceFrameList(void);
    FrameParserStatus_t   RevPlayRemoveReferenceFrameFromList(void);
    FrameParserStatus_t   RevPlayJunkReferenceFrameList(void);
    FrameParserStatus_t   RevPlayNextSequenceFrameProcess(void);

    bool                  ReadAdditionalUserDataParameters(void);

// Additional members from STHM' 7.2::
// Starts here

private:
    FrameParserStatus_t ProcessSPS();
    FrameParserStatus_t ProcessPPS();
    FrameParserStatus_t ProcessVPS();

    // from hl_syntax.c
    void SortRpsIncreasingOrder(rps_t *rps);
    void short_term_ref_pic_set(st_rps_t *cur_st_rps, st_rps_t *ref_st_rps, uint8_t i, bool slice_not_sps);
    void parseProfileTier(int32_t i);
    int32_t parsePTL(bool profilePresentFlag, uint8_t maxNumSubLayersMinus1);
    void ReadHRD(HevcHrd_t *hrd, bool commonInfPresentFlag, uint8_t max_temporal_layers);
    void ReadVUI(HevcVui_t *vui, uint8_t  max_temporal_layers);
    FrameParserStatus_t InterpretVPS(HevcVideoParameterSet_t *vps);
    void scaling_list_syntax(scaling_list_t *scaling_list, uint8_t sizeID, uint8_t matrixID);
    void scaling_list_param(scaling_list_t *scaling_list);
    FrameParserStatus_t InterpretSPS(HevcSequenceParameterSet_t *sps, int32_t max_level_idc);
    FrameParserStatus_t InterpretPPS(HevcPictureParameterSet_t *pps);
    bool parse_start_of_slice();
    void parse_dependent_slice_header(bool *first_slice_segment_in_pic_flag,
                                      bool      *dependent_slice_segment_flag,
                                      uint32_t  *slice_segment_address);

    FrameParserStatus_t FirstPartOfSliceHeader(bool *first_slice_segment_in_pic_flag);
    FrameParserStatus_t ReadNalSliceHeader(unsigned int SliceSegmentsInCurrentPicture);
    FrameParserStatus_t ReadRemainingStartCode(void);

    // from access_unit.c
    void init_tile_size();

private:
    // from decoder_t

    // slice data
    //! Address of the CTB containing the start of the current slice
    uint32_t CTBAddress;
    //! Index of the short term RPS inside the SPS referenced by a slice
    uint8_t  short_term_ref_pic_set_idx;
    //! Short term RPS defined at slice level
    st_rps_t st_rps;
    //! Long term RPS defined at slice level
    rps_t    lt_rps;
    bool     delta_poc_msb_present_flag[HEVC_MAX_REFERENCE_INDEX]; //!< Syntax element from SPS or slice header
    uint32_t delta_poc_msb_cycle_lt[HEVC_MAX_REFERENCE_INDEX]; //!< DeltaPocMsbCycleLt variable computed from the slice header

    // picture data
    uint8_t slice_types; // TODO: Remove
    //! Signals that the current picture is to be displayed (false <=> VP8 golden frames ?)
    bool     pic_output_flag;
    //! Identifier of the PPS referenced by the current picture, index to pps_set
    uint8_t  pic_parameter_set_id;
    //! Signals that subsequent RASL pictures are not to be output
    bool     no_output_of_rasl_pics_flag;
    //! Signals that pictures already present in the DPB when starting the decode on an IDR are not to be displayed
    bool     no_output_of_prior_pics_flag;
    //! Syntax element of the slice header used to compute the complete poc value
    uint32_t pic_order_cnt_lsb;

    //! Signals that the current access unit contains a MD5 sum wrapped in a SEI message
    bool has_picture_digest;
    //! MD5 sum of the expected values of decoded pixels (Y plane -> Cb plane -> Cr plane)
    uint8_t picture_digest[NUM_COMPONENTS][16];
    HevcVideoParameterSet_t           *mVPS; //! Pointer to the VPS currently in use
    HevcSequenceParameterSet_t        *mSPS; //! Pointer to the SPS currently in use
    HevcPictureParameterSet_t         *mPPS; //! Pointer to the PPS currently in use

    unsigned int  mParsedStartCodeNum;       //! Number of Parsed Start Codes
    bool          mSecondPass;               //! Is it second pass of ReadHeaders

// Additional members from STHM' ::
// Ends here
    int32_t fp_ue_v(const char *tracestring, ...);
    int32_t fp_se_v(const char *tracestring, ...);
    int32_t fp_u_v(int32_t len, const char *tracestring, ...);
    int32_t fp_u_1(const char *tracestring,        ...);

private:
    int PictureCount;
    // Data

    bool                                          ReadNewVPS;
    bool                                          ReadNewSPS;
    bool                                          ReadNewPPS;

    BufferType_t                                  VideoParameterSetType;
    BufferPool_t                                  VideoParameterSetPool;
    HevcVideoParameterSetEntry_t                  VideoParameterSetTable[HEVC_STANDARD_MAX_VIDEO_PARAMETER_SETS];

    BufferType_t                                  SequenceParameterSetType;
    BufferPool_t                                  SequenceParameterSetPool;
    HevcSequenceParameterSetEntry_t               SequenceParameterSetTable[HEVC_STANDARD_MAX_SEQUENCE_PARAMETER_SETS];

    BufferType_t                                  PictureParameterSetType;
    BufferPool_t                                  PictureParameterSetPool;
    HevcPictureParameterSetEntry_t                PictureParameterSetTable[HEVC_STANDARD_MAX_PICTURE_PARAMETER_SETS];

    HevcSequenceParameterSet_t                    CopyOfSequenceParameterSet;
    HevcVideoParameterSet_t                       CopyOfVideoParameterSet;
    HevcPictureParameterSet_t                     CopyOfPictureParameterSet;

    HevcStreamParameters_t                       *StreamParameters;
    HevcFrameParameters_t                        *FrameParameters;

    Rational_t                                    DefaultPixelAspectRatio; // TODO CL usage to be refined in parsing later... shall be set to 1:1 for now I guess

    bool                                          SeenAnIDR;
    bool                                          BehaveAsIfSeenAnIDR;  // was a H264 trick for use in IDR free streams (broadcast BBC), is it still necessary ???
    bool                                          mDecodeStartedOnRAP;
    bool                                          mMissingRef;
    HevcSliceHeader_t                            *SliceHeader;

// TODO CL SEI management to be addressed later...
#if 0
    HEVCSEIPictureTiming_t                        SEIPictureTiming;
    HevcSEIPanScanRectangle_t                     SEIPanScanRectangle;
    HevcSEIFramePackingArrangement_t              SEIFramePackingArrangement;
#endif

    bool                                         SeenProbableReversalPoint; // requested by collator2 for trickmode

#if 0
    unsigned int                  LastReferenceIframeDecodeIndex; // was used for reference list error recovery
#endif

    //
    // Reference Frames
    //
    unsigned int                                  NumReferenceFrames;
    HevcReferenceFrameData_t                      ReferenceFrames[HEVC_MAX_DPB_SIZE];
    // ReferenceFrameList_t                          ReferenceFrameLists[HEVC_NUM_REF_FRAME_LISTS];

    HevcDeferredDFIandPTSList_t                   mDeferredList[HEVC_CODED_FRAME_COUNT];
    unsigned int                                  mDeferredListEntries;
    unsigned int                                  mOrderedDeferredList[HEVC_CODED_FRAME_COUNT];

// TODO CL P&S management to be addressed later...
//    PanScanState_t                                PanScanState;
// TODO CL 3D management to be addressed later if actually necessary...
//    FramePackingArrangementState_t                FramePackingArrangementState;

// TODO CL look at variables used for H264 to keep the ones still needed in HEC parser based on STHM' source code...
    unsigned long long                            PicOrderCntOffset;                    // Offset increased every IDR to allow Displayframe indices to be derived
    unsigned long long                            PicOrderCntOffsetAdjust;
    int                                           PrevPicOrderCntMsb;                   // Set for use on the next pass through
    unsigned int                                  PrevPicOrderCntLsb;
#if 0
    //
    // H264 context variables
    //

    unsigned int                                  nal_ref_idc;                          // Part of Nal header
    unsigned int                                  nal_unit_type;                        // Part of Nal header

    unsigned int                                  CpbDpbDelaysPresentFlag;
    unsigned int                                  CpbRemovalDelayLength;
    unsigned int                                  DpbOutputDelayLength;
    unsigned int                                  PicStructPresentFlag;
    unsigned int                                  TimeOffsetLength;

    unsigned int                                  PrevFrameNum;
    unsigned int                                  PrevFrameNumOffset;
    bool                                          SeenDpbValue;
    unsigned int                                  BaseDpbValue;
    unsigned int                                  LastCpbDpbDelaysPresentFlag;

#endif

    // TODO CL these flags should be revisited when SEI parsing will be added...
    bool                                          DisplayOrderByDpbValues;
    bool                                          DpbValuesInvalidatedByPTS;

    bool                                          ValidPTSSequence;          // Set to false when we found an invalid PTS or a jump in PTS of the wrong direction

    //
    // Copies of context variables that are used when we
    // are trying to handle reverse decode
    //

    unsigned int                                 LastExitPicOrderCntMsb;       // Last msb actually seen by calculate pic order cnts

//  Userdata parameters
    HevcUserDataParameters_t            mUserData;

    // Functions

    bool                  NewStreamParametersCheck(void);
    FrameParserStatus_t   PrepareNewStreamParameters(void);
    FrameParserStatus_t   CommitFrameForDecode(void);

#if 0 // TODO CL clarify what is still actually needed for implementation based on STHM' code
    void                  PrepareScannedScalingLists(void);
    FrameParserStatus_t   ReadScalingList(bool                              ScalingListPresent,
                                          unsigned int                     *ScalingList,
                                          unsigned int                     *Default,
                                          unsigned int                     *Fallback,
                                          unsigned int                      SizeOfScalingList,
                                          unsigned int                     *UseDefaultScalingMatrixFlag);

#endif

// TODO CL clarify what is still actually needed for implementation based on STHM' code
    FrameParserStatus_t   SetDefaultSequenceParameterSet(HevcSequenceParameterSet_t     *sps);
    FrameParserStatus_t   SetDefaultSliceHeader(HevcSliceHeader_t        *Header);

#if 0 // TODO CL clarify what is still actually needed for implementation based on STHM' code
    FrameParserStatus_t   ReadHrdParameters(HEVCHrdParameters_t              *Header);
    FrameParserStatus_t   ReadVUISequenceParameters(HEVCVUISequenceParameters_t      *Header);
    FrameParserStatus_t   ReadNalSequenceParameterSet(void);
    FrameParserStatus_t   ReadNalSequenceParameterSetExtension(void);
    FrameParserStatus_t   ReadNalPictureParameterSet(void);
    FrameParserStatus_t   ReadRefPicListReordering(void);
    FrameParserStatus_t   ReadPredWeightTable(void);
    FrameParserStatus_t   ReadDecRefPicMarking(void);
#endif


    FrameParserStatus_t   ReadSeiPayload(unsigned int                      PayloadType,
                                         unsigned int                      PayloadSize,
                                         uint8_t nal_unit_type);
    FrameParserStatus_t   ReadSeiUserDataRegisteredITUTT35Message(unsigned int        PayloadSize);
    FrameParserStatus_t   ReadSeiUserDataUnregisteredMessage(unsigned int         PayloadSize);
    FrameParserStatus_t   ParseSeiPictureDigest(uint8_t digest[NUM_COMPONENTS][16],
                                                int32_t size);

    FrameParserStatus_t   ProcessSEI(unsigned int   UnitLength,
                                     uint8_t        nal_unit_type);

#if 0 // TODO CL SEI to be addressed later
    FrameParserStatus_t   ReadSeiPictureTimingMessage(void);
    FrameParserStatus_t   ReadSeiPanScanMessage(void);
    FrameParserStatus_t   ReadSeiFramePackingArrangementMessage(void);
#endif

// TODO CL refine Player2 specific Parameters passed to Frame Parser must rework
    FrameParserStatus_t   ReadPlayer2ContainerParameters(void);

// TODO CL refine Poc management based on STHM' code reused
    FrameParserStatus_t   CalculatePicOrderCnts(void);

// TODO CL refine reference management based on STHM' code reused
    void          build_pic_sets(unsigned int num_pic_set[MAX_NUM_PIC_SET],  unsigned int pic_sets[MAX_NUM_PIC_SET][HEVC_MAX_REFERENCE_INDEX]);
    void          AddRPSEntries(unsigned int num_pic_set[MAX_NUM_PIC_SET],  unsigned int pic_sets[MAX_NUM_PIC_SET][HEVC_MAX_REFERENCE_INDEX], int PicSetIndex, ReferenceFrameList_t *rfl);
    void          build_lt_pic_set(unsigned int pic_set[MAX_NUM_PIC_SET], uint8_t num_pic, int32_t *poc, bool *msb_present_flag);
    void          build_st_pic_set(unsigned int pic_set[MAX_NUM_PIC_SET], uint8_t num_pic, int32_t *poc);

#if 0
    FrameParserStatus_t   CalculateReferencePictureListsFrame(void);
    FrameParserStatus_t   CalculateReferencePictureListsField(void);
    FrameParserStatus_t   InitializePSliceReferencePictureListFrame(void);
    FrameParserStatus_t   InitializePSliceReferencePictureListField(void);
    FrameParserStatus_t   InitializeBSliceReferencePictureListsFrame(void);
    FrameParserStatus_t   InitializeBSliceReferencePictureListsField(void);
    FrameParserStatus_t   InitializeReferencePictureListField(ReferenceFrameList_t     *ShortTermList,
                                                              ReferenceFrameList_t     *LongTermList,
                                                              unsigned int              MaxListEntries,
                                                              ReferenceFrameList_t     *List);

#endif
    FrameParserStatus_t   ReleaseReference(bool                      ActuallyRelease,
                                           unsigned int              Entry,
                                           unsigned int              ReleaseUsage);

    FrameParserStatus_t   MarkReferencePictures(bool                      ActuallyReleaseReferenceFrames);

    void                  ProcessDeferredDFIandPTSUpto(unsigned long long        ExtendedPicOrderCnt);
    void                  ProcessDeferredDFIandPTSDownto(unsigned long long        ExtendedPicOrderCnt);
    void                  DeferDFIandPTSGeneration(Buffer_t                  Buffer,
                                                   ParsedFrameParameters_t  *ParsedFrameParameters,
                                                   ParsedVideoParameters_t  *ParsedVideoParameters,
                                                   unsigned long long        ExtendedPicOrderCnt);

#if 0
    void                  SetupPanScanValues(ParsedFrameParameters_t  *ParsedFrameParameters,
                                             ParsedVideoParameters_t  *ParsedVideoParameters);
    void                  SetupFramePackingArrangementValues(ParsedFrameParameters_t  *ParsedFrameParameters,
                                                             ParsedVideoParameters_t  *ParsedVideoParameters);
    void                  ResetPictureFPAValuesAnd3DVideoProperty(ParsedVideoParameters_t  *ParsedVideoParameters);
    void                  ReportFramePackingArrangementValues(ParsedVideoParameters_t  *ParsedVideoParameters);
#endif

    void                  CalculateCropUnits(void);

    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoHevc_c);
};

#endif
