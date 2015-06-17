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

#ifndef H_FRAME_PARSER_VIDEO_H264
#define H_FRAME_PARSER_VIDEO_H264

#include "h264.h"
#include "frame_parser_video.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoH264_c"

#define DEFAULT_ANTI_EMULATION_REQUEST                  32
#define SCALING_LIST_ANTI_EMULATION_REQUEST             128
#define HRD_PARAMETERS_ANTI_EMULATION_REQUEST           64
#define VUI_PARAMETERS_ANTI_EMULATION_REQUEST           32
#define SPS_EXTENSION_ANTI_EMULATION_REQUEST            16
#define REF_LIST_REORDER_ANTI_EMULATION_REQUEST         128
#define PRED_WEIGHT_TABLE_ANTI_EMULATION_REQUEST        256
#define MEM_MANAGEMENT_ANTI_EMULATION_REQUEST           64
#define H264_MAX_FRAME_WIDTH                            4096
#define H264_MAX_FRAME_HEIGHT                           2400

typedef struct SequenceParameterSetPair_s
{
    H264SequenceParameterSetHeader_t             SequenceParameterSetHeader;
    H264SequenceParameterSetExtensionHeader_t    SequenceParameterSetExtensionHeader;
} SequenceParameterSetPair_t;

//

typedef struct SequenceParameterSetEntry_s
{
    Buffer_t                                     Buffer;
    H264SequenceParameterSetHeader_t            *Header;
    H264SequenceParameterSetExtensionHeader_t   *ExtensionHeader;
} SequenceParameterSetEntry_t;

//

typedef struct PictureParameterSetEntry_s
{
    Buffer_t                             Buffer;
    H264PictureParameterSetHeader_t     *Header;
} PictureParameterSetEntry_t;

#define NotUsedForReference             0
#define UsedForTopShortTermReference    1
#define UsedForTopLongTermReference     2
#define UsedForBotShortTermReference    4
#define UsedForBotLongTermReference     8

#define AnyUsedForShortTermReference    (UsedForTopShortTermReference | UsedForBotShortTermReference)
#define AnyUsedForLongTermReference     (UsedForTopLongTermReference | UsedForBotLongTermReference)

#define AnyUsedForReference             (AnyUsedForShortTermReference | AnyUsedForLongTermReference)

#define ComplimentaryReferencePair(u)   ((((u) | ((u) >> 1)) & 5) == 5)

//

typedef struct H264ReferenceFrameData_s
{
    ParsedFrameParameters_t    *ParsedFrameParameters;      // Player pointer, used in checking resource usage
    unsigned int            DecodeFrameIndex;               // Player value
    bool                    Field;                          // Allow us to distinguish between two fields and a frame - useful during a switch between frame/field decodes
    unsigned int            Usage;                          // Usage codes

    unsigned int            LongTermFrameIdx;               // H264 value
    unsigned int            FrameNum;                       // H264 value

    int                     FrameNumWrap;                   // H264 derived every time we prepare a ref list
    int                     PicNum;                         // H264

    unsigned int            LongTermPicNum;                 // H264 value

    int                     PicOrderCnt;                    // H264 value - both fields
    int                     PicOrderCntTop;
    int                     PicOrderCntBot;

    unsigned long long      ExtendedPicOrderCnt;
} H264ReferenceFrameData_t;

//

typedef struct H264DeferredDFIandPTSList_s
{
    Buffer_t                     Buffer;
    ParsedFrameParameters_t     *ParsedFrameParameters;
    ParsedVideoParameters_t     *ParsedVideoParameters;
    unsigned long long           ExtendedPicOrderCnt;
    unsigned int                 PicOrderCntLsb;
} H264DeferredDFIandPTSList_t;

typedef struct PanScanState_s
{
    unsigned int                 Count;
    unsigned int                 RepetitionPeriod;
    unsigned int                 Width[H264_SEI_MAX_PAN_SCAN_VALUES];
    unsigned int                 Height[H264_SEI_MAX_PAN_SCAN_VALUES];
    int                          HorizontalOffset[H264_SEI_MAX_PAN_SCAN_VALUES];
    int                          VerticalOffset[H264_SEI_MAX_PAN_SCAN_VALUES];
} PanScanState_t;

typedef struct FramePackingArrangementState_s
{
    int                          Persistence;
    int                          RepetitionPeriod;
    bool                         CanResetFPASetting;
    Output3DVideoProperty_t      Output3DVideoProperty;
    FramePackingArrangementState_s(void)
        : Persistence(0), RepetitionPeriod(0), CanResetFPASetting(false), Output3DVideoProperty() {}
} FramePackingArrangementState_t;

// These variables are specific to H264 and are needed to solve bug 33435:
// aim is to send to manifestor pictures as soon as possible. For that,
// POC (Picture Order Count) is used if the POC step between pictures is constant.
typedef struct H264PictureOrderCountInfo_s
{
    unsigned long long    GreatestPOCToDisplay;
    unsigned long long    LastPOC;
    unsigned int          POCStep;
    unsigned int          NbPictWithPOCStepConstant;
    unsigned int          NbPictToTryBeforeIncreasingPOC;
    bool                  NonReferenceFramesPresent;
    bool                  LastFrameWasAReference;
} H264PictureOrderCountInfo_t;

class FrameParser_VideoH264_c : public FrameParser_Video_c
{
public:
    FrameParser_VideoH264_c(void);
    ~FrameParser_VideoH264_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);

    FrameParserStatus_t   ResetCollatedHeaderState(void);
    unsigned int          RequiredPresentationLength(unsigned char       StartCode);
    FrameParserStatus_t   PresentCollatedHeader(unsigned char            StartCode,
                                                unsigned char           *HeaderBytes,
                                                FrameParserHeaderFlag_t *Flags);

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
    FrameParserStatus_t   ForPlayCheckForReferenceReadyForManifestation(void);

    FrameParserStatus_t   RevPlayQueueFrameForDecode(void);
    FrameParserStatus_t   RevPlayProcessDecodeStacks(void);
    FrameParserStatus_t   RevPlayGeneratePostDecodeParameterSettings(void);
    FrameParserStatus_t   RevPlayPurgeQueuedPostDecodeParameterSettings(void);
    FrameParserStatus_t   RevPlayAppendToReferenceFrameList(void);
    FrameParserStatus_t   RevPlayRemoveReferenceFrameFromList(void);
    FrameParserStatus_t   RevPlayJunkReferenceFrameList(void);
    FrameParserStatus_t   RevPlayNextSequenceFrameProcess(void);

    bool                  ReadAdditionalUserDataParameters(void);
    void                  UpdateMaxDisplayablePicOrderCount(ParsedFrameParameters_t *ParsedFrameParameters);
    void                  CalculateFrameIndexAndPts(ParsedFrameParameters_t *ParsedFrame, ParsedVideoParameters_t *ParsedVideo);

protected:
    bool                                          ReadNewSPS;
    bool                                          ReadNewSPSExtension;
    bool                                          ReadNewPPS;

    BufferType_t                                  SequenceParameterSetType;
    BufferPool_t                                  SequenceParameterSetPool;
    SequenceParameterSetEntry_t                   SequenceParameterSetTable[H264_STANDARD_MAX_SEQUENCE_PARAMETER_SETS];

    BufferType_t                                  PictureParameterSetType;
    BufferPool_t                                  PictureParameterSetPool;
    PictureParameterSetEntry_t                    PictureParameterSetTable[H264_STANDARD_MAX_PICTURE_PARAMETER_SETS];

    H264SequenceParameterSetHeader_t              CopyOfSequenceParameterSet;           // Copies of last set stream parameters components
    H264SequenceParameterSetExtensionHeader_t     CopyOfSequenceParameterSetExtension;
    H264PictureParameterSetHeader_t               CopyOfPictureParameterSet;

    H264StreamParameters_t                       *StreamParameters;
    H264FrameParameters_t                        *FrameParameters;

    Rational_t                                    DefaultPixelAspectRatio;

    bool                                          SeenAnIDR;
    bool                                          BehaveAsIfSeenAnIDR;          // for use in IDR free streams (broadcast BBC)
    H264SliceHeader_t                            *SliceHeader;
    H264SEIPictureTiming_t                        SEIPictureTiming;
    H264SEIPanScanRectangle_t                     SEIPanScanRectangle;
    H264SEIFramePackingArrangement_t              SEIFramePackingArrangement;


    // otherwise just re-use the last established list.

    unsigned int                                  AccumulatedFrameNumber;
    bool                                          AccumulatedReferenceField;
    ParsedVideoParameters_t                      *AccumulatedParsedVideoParameters;

    H264PictureOrderCountInfo_t                   PictureOrderCountInfo;

    bool                                          SeenProbableReversalPoint;

    unsigned int                                  LastReferenceIframeDecodeIndex;

    //
    // Reference Frames
    //    Thats N frames
    //    +1 for adding one before performing selection process for deletion
    //

    unsigned int                                  NumReferenceFrames;

    unsigned int                                  MaxLongTermFrameIdx;
    unsigned int                                  NumLongTerm;
    unsigned int                                  NumShortTerm;

    ReferenceFrameList_t                          ReferenceFrameList[H264_NUM_REF_FRAME_LISTS];
    ReferenceFrameList_t                          ReferenceFrameListShortTerm[2];
    ReferenceFrameList_t                          ReferenceFrameListLongTerm;

    H264ReferenceFrameData_t                      ReferenceFrames[H264_MAX_REFERENCE_FRAMES + 1];

    H264DeferredDFIandPTSList_t                   mDeferredList[H264_CODED_FRAME_COUNT];
    unsigned int                                  mDeferredListEntries;
    unsigned int                                  mOrderedDeferredList[H264_CODED_FRAME_COUNT];

    bool                                          FirstFieldSeen;                       // Deductions about interlaced and topfield first flags
    bool                                          FixDeducedFlags;
    unsigned long long                            LastFieldExtendedPicOrderCnt;
    bool                                          DeducedInterlacedFlag;
    bool                                          DeducedTopFieldFirst;

    bool                                          ForceInterlacedProgressive;
    bool                                          ForcedInterlacedFlag;

    PanScanState_t                                PanScanState;
    FramePackingArrangementState_t                FramePackingArrangementState;

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

    int                                           PrevPicOrderCntMsb;                   // Set for use on the next pass through
    unsigned int                                  PrevPicOrderCntLsb;
    unsigned int                                  PrevFrameNum;
    unsigned int                                  PrevFrameNumOffset;
    unsigned long long                            PicOrderCntOffset;                    // Offset increased every IDR to allow Displayframe indices to be derived
    unsigned long long                            PicOrderCntOffsetAdjust;
    bool                                          SeenDpbValue;
    unsigned int                                  BaseDpbValue;
    unsigned int                                  LastCpbDpbDelaysPresentFlag;

    bool                                          DisplayOrderByDpbValues;
    bool                                          DpbValuesInvalidatedByPTS;

    bool                                          ValidPTSSequence;          // Set to false when we found an invalid PTS or a jump in PTS of the wrong direction

    //
    // Copies of context variables that are used when we
    // are trying to handle reverse decode
    //

    unsigned int                  LastExitPicOrderCntMsb;       // Last msb actually seen by calculate pic order cnts

    H264UserDataParameters_t      UserData;

    //For error recovery from invalid level_idc
    unsigned int                  PreviousSpsTotalMbs[H264_STANDARD_MAX_SEQUENCE_PARAMETER_SETS];
    unsigned int                  PreviousSpsLevelIdc[H264_STANDARD_MAX_SEQUENCE_PARAMETER_SETS];
    unsigned int                  LevelCorrectionDone;

    // Functions

    FrameParserStatus_t   CommitFrameForDecode(void);

    FrameParserStatus_t   SetDefaultSliceHeader(H264SliceHeader_t        *Header);

    void                  PrepareScannedScalingLists(void);
    FrameParserStatus_t   ParseScalingList(unsigned int    *ScalingList,
                                           unsigned int    *Default,
                                           unsigned int     SizeOfScalingList,
                                           unsigned int    *UseDefaultScalingMatrixFlag);
    FrameParserStatus_t   UpdateScalingList(unsigned int sps_id, unsigned int pps_id);

    FrameParserStatus_t   ReadHrdParameters(H264HrdParameters_t              *Header);
    FrameParserStatus_t   ReadVUISequenceParameters(H264VUISequenceParameters_t      *Header);
    FrameParserStatus_t   ParseNalSequenceParameterSet(H264SequenceParameterSetHeader_t *Header);
    FrameParserStatus_t   ReadNalSequenceParameterSet(void);
    FrameParserStatus_t   ReadNalSequenceParameterSetExtension(void);
    FrameParserStatus_t   ReadNalPictureParameterSet(void);
    FrameParserStatus_t   ReadPredWeightTable(void);
    FrameParserStatus_t   ReadDecRefPicMarking(void);
    FrameParserStatus_t   ReadNalSliceHeader(unsigned int UnitLength);
    FrameParserStatus_t   ParseNalSliceHeader(H264SliceHeader_t  *Header);
    FrameParserStatus_t   ReadSeiPictureTimingMessage(void);
    FrameParserStatus_t   ReadSeiPanScanMessage(void);
    FrameParserStatus_t   ReadSeiFramePackingArrangementMessage(void);
    FrameParserStatus_t   ReadSeiUserDataRegisteredITUTT35Message(unsigned int    PayloadSize);
    FrameParserStatus_t   ReadSeiUserDataUnregisteredMessage(unsigned int         PayloadSize);

    FrameParserStatus_t   ReadNalSupplementalEnhancementInformation(unsigned int  UnitLength);
    FrameParserStatus_t   ReadPlayer2ContainerParameters(void);


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

    FrameParserStatus_t   ReleaseReference(bool                      ActuallyRelease,
                                           unsigned int              Entry,
                                           unsigned int              ReleaseUsage);

    FrameParserStatus_t   MarkReferencePictures(bool                      ActuallyReleaseReferenceFrames);

    void                  ProcessDeferredDFIandPTSUpto(unsigned long long        ExtendedPicOrderCnt);
    void                  ProcessDeferredDFIandPTSDownto(unsigned long long      ExtendedPicOrderCnt);
    void                  DeferDFIandPTSGeneration(Buffer_t                  Buffer,
                                                   ParsedFrameParameters_t  *ParsedFrameParameters,
                                                   ParsedVideoParameters_t  *ParsedVideoParameters,
                                                   unsigned long long        ExtendedPicOrderCnt);

    void                  SetupPanScanValues(ParsedFrameParameters_t  *ParsedFrameParameters,
                                             ParsedVideoParameters_t  *ParsedVideoParameters);
    void                  SetupFramePackingArrangementValues(ParsedFrameParameters_t  *ParsedFrameParameters,
                                                             ParsedVideoParameters_t  *ParsedVideoParameters);
    void                  ResetPictureFPAValuesAnd3DVideoProperty(ParsedVideoParameters_t  *ParsedVideoParameters);
    void                  ReportFramePackingArrangementValues(ParsedVideoParameters_t  *ParsedVideoParameters);
    void                  CalculateCropUnits(void);
    FrameParserStatus_t   SetDefaultSequenceParameterSet(H264SequenceParameterSetHeader_t *Header);
    unsigned int          ComputeDpbSize(H264SequenceParameterSetHeader_t *Header);
    void                  ComputePOCStep(unsigned long long      ExtendedPicOrderCnt);

    virtual FrameParserStatus_t ReadSeiPayload(unsigned int PayloadType,
                                               unsigned int PayloadSize);
    virtual FrameParserStatus_t ReadSingleHeader(unsigned int Code, unsigned int UnitLength);
    virtual bool        isHighProfile(unsigned int profile_idc);
    virtual FrameParserStatus_t ReadRefPicListReordering(void);
    virtual FrameParserStatus_t CalculatePicOrderCnts(void);
    virtual bool                NewStreamParametersCheck(void);
    virtual FrameParserStatus_t PrepareNewStreamParameters(void);

private:
    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoH264_c);
};

#endif
