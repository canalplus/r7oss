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

Source file name : frame_parser_video_h264.h
Author :           Nick

Definition of the frame parser video h264 class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
27-Jul-07   Created                                         Nick

************************************************************************/

#ifndef H_FRAME_PARSER_VIDEO_H264
#define H_FRAME_PARSER_VIDEO_H264

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "h264.h"
#include "frame_parser_video.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define DEFAULT_ANTI_EMULATION_REQUEST                  32
#define SCALING_LIST_ANTI_EMULATION_REQUEST             128
#define HRD_PARAMETERS_ANTI_EMULATION_REQUEST           64
#define VUI_PARAMETERS_ANTI_EMULATION_REQUEST           32
#define SPS_EXTENSION_ANTI_EMULATION_REQUEST            16
#define REF_LIST_REORDER_ANTI_EMULATION_REQUEST         128
#define PRED_WEIGHT_TABLE_ANTI_EMULATION_REQUEST        256
#define MEM_MANAGEMENT_ANTI_EMULATION_REQUEST           64

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

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

//

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
    ParsedFrameParameters_t    *ParsedFrameParameters;		// Player pointer, used in checking resource usage
    unsigned int        	DecodeFrameIndex;               // Player value
    bool                	Field;                          // Allow us to distinguish between two fields and a frame - useful during a switch between frame/field decodes
    unsigned int        	Usage;                          // Usage codes

    unsigned int        	LongTermFrameIdx;               // H264 value
    unsigned int        	FrameNum;                       // H264 value

    int                 	FrameNumWrap;                   // H264 derived every time we prepare a ref list
    int                 	PicNum;                         // H264 

    unsigned int        	LongTermPicNum;                 // H264 value

    int                 	PicOrderCnt;                    // H264 value - both fields
    int                 	PicOrderCntTop;
    int                 	PicOrderCntBot;

    unsigned long long  	ExtendedPicOrderCnt;
} H264ReferenceFrameData_t;

//

typedef struct H264DeferredDFIandPTSList_s
{
    Buffer_t                     Buffer;
    ParsedFrameParameters_t     *ParsedFrameParameters;
    ParsedVideoParameters_t     *ParsedVideoParameters;
    unsigned long long           ExtendedPicOrderCnt;
} H264DeferredDFIandPTSList_t;



typedef struct PanScanState_s
{
    unsigned int        Count;
    unsigned int        RepetitionPeriod;
    unsigned int        Width[H264_SEI_MAX_PAN_SCAN_VALUES];
    unsigned int        Height[H264_SEI_MAX_PAN_SCAN_VALUES];
    int                 HorizontalOffset[H264_SEI_MAX_PAN_SCAN_VALUES];
    int                 VerticalOffset[H264_SEI_MAX_PAN_SCAN_VALUES];
} PanScanState_t;

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class FrameParser_VideoH264_c : public FrameParser_Video_c
{
private:

    // Data

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

    bool					  UserSpecifiedDefaultFrameRate;
    Rational_t                                    DefaultFrameRate;
    Rational_t                                    DefaultPixelAspectRatio;

    bool                                          SeenAnIDR;
    bool                                          BehaveAsIfSeenAnIDR;			// for use in IDR free streams (broadcast BBC)
    H264SliceHeader_t				 *SliceHeader;
    H264SEIPictureTiming_t                        SEIPictureTiming;
    H264SEIPanScanRectangle_t                     SEIPanScanRectangle;
    // otherwise just re-use the last established list.

    unsigned int                                  AccumulatedFrameNumber;
    bool                                          AccumulatedReferenceField;
    ParsedVideoParameters_t                      *AccumulatedParsedVideoParameters;

    unsigned int				  FrameSliceType;			// Slice type of first slice in frame
    bool					  FrameSliceTypeVaries;			// indicator that slice type varies over the slices in a frame

    bool					  SeenProbableReversalPoint;

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

    H264DeferredDFIandPTSList_t                   DeferredList[H264_CODED_FRAME_COUNT];
    unsigned int                                  DeferredListEntries;
    unsigned int                                  OrderedDeferredList[H264_CODED_FRAME_COUNT];

    bool                                          FirstFieldSeen;                       // Deductions about interlaced and topfield first flags
    bool                                          FixDeducedFlags;
    unsigned long long                            LastFieldExtendedPicOrderCnt;
    bool                                          DeducedInterlacedFlag;
    bool                                          DeducedTopFieldFirst;

    bool					  ForceInterlacedProgressive;
    bool					  ForcedInterlacedFlag;

    PanScanState_t                                PanScanState;

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
    unsigned long long				  PicOrderCntOffsetAdjust;
    bool                                          SeenDpbValue;
    unsigned int                                  BaseDpbValue;
    unsigned int                                  LastCpbDpbDelaysPresentFlag;

    bool					  DisplayOrderByDpbValues;
    bool					  DpbValuesInvalidatedByPTS;

    //
    // Copies of context variables that are used when we 
    // are trying to handle reverse decode
    //

    unsigned int				  LastExitPicOrderCntMsb;		// Last msb actually seen by calculate pic order cnts

    // Functions

    bool                  NewStreamParametersCheck(     void );
    FrameParserStatus_t   PrepareNewStreamParameters(   void );
    FrameParserStatus_t   CommitFrameForDecode(         void );

    FrameParserStatus_t   SetDefaultSequenceParameterSet( H264SequenceParameterSetHeader_t *Header );
    FrameParserStatus_t   SetDefaultSliceHeader( 	H264SliceHeader_t		 *Header );

    void                  PrepareScannedScalingLists(   void );
    FrameParserStatus_t   ReadScalingList(              bool                              ScalingListPresent,
							unsigned int                     *ScalingList,
							unsigned int                     *Default,
							unsigned int                     *Fallback,
							unsigned int                      SizeOfScalingList,
							unsigned int                     *UseDefaultScalingMatrixFlag );
    FrameParserStatus_t   ReadHrdParameters(            H264HrdParameters_t              *Header );
    FrameParserStatus_t   ReadVUISequenceParameters(    H264VUISequenceParameters_t      *Header );
    FrameParserStatus_t   ReadNalSequenceParameterSet(  void );
    FrameParserStatus_t   ReadNalSequenceParameterSetExtension( void );
    FrameParserStatus_t   ReadNalPictureParameterSet(   void );
    FrameParserStatus_t   ReadRefPicListReordering(     void );
    FrameParserStatus_t   ReadPredWeightTable(          void );
    FrameParserStatus_t   ReadDecRefPicMarking(         void );
    FrameParserStatus_t   ReadNalSliceHeader(           void );
    FrameParserStatus_t   ReadSeiPictureTimingMessage(  void );
    FrameParserStatus_t   ReadSeiPanScanMessage(        void );
    FrameParserStatus_t   ReadSeiPayload(               unsigned int                      PayloadType,
							unsigned int                      PayloadSize );
    FrameParserStatus_t   ReadNalSupplementalEnhancementInformation( unsigned int	  UnitLength );
    FrameParserStatus_t   ReadPlayer2ContainerParameters( void );

    FrameParserStatus_t   CalculatePicOrderCnts(        void );

    FrameParserStatus_t   CalculateReferencePictureListsFrame(          void );
    FrameParserStatus_t   CalculateReferencePictureListsField(          void );
    FrameParserStatus_t   InitializePSliceReferencePictureListFrame(    void );
    FrameParserStatus_t   InitializePSliceReferencePictureListField(    void );
    FrameParserStatus_t   InitializeBSliceReferencePictureListsFrame(   void );
    FrameParserStatus_t   InitializeBSliceReferencePictureListsField(   void );
    FrameParserStatus_t   InitializeReferencePictureListField(          ReferenceFrameList_t     *ShortTermList,
									ReferenceFrameList_t     *LongTermList,
									unsigned int              MaxListEntries,
									ReferenceFrameList_t     *List );

    FrameParserStatus_t   ReleaseReference(                             bool                      ActuallyRelease,
									unsigned int              Entry,
									unsigned int              ReleaseUsage );

    FrameParserStatus_t   MarkReferencePictures(                        bool                      ActuallyReleaseReferenceFrames );

    void                  ProcessDeferredDFIandPTSUpto(                 unsigned long long        ExtendedPicOrderCnt );
    void                  ProcessDeferredDFIandPTSDownto(               unsigned long long        ExtendedPicOrderCnt );
    void                  DeferDFIandPTSGeneration(                     Buffer_t                  Buffer,
									ParsedFrameParameters_t  *ParsedFrameParameters,
									ParsedVideoParameters_t  *ParsedVideoParameters,
									unsigned long long        ExtendedPicOrderCnt );

    void                  SetupPanScanValues(                           ParsedFrameParameters_t  *ParsedFrameParameters,
									ParsedVideoParameters_t  *ParsedVideoParameters );
    void                  CalculateCropUnits(                           void );
public:

    //
    // Constructor function
    //

    FrameParser_VideoH264_c( void );
    ~FrameParser_VideoH264_c( void );

    //
    // Overrides for component base class functions
    //

    FrameParserStatus_t   Reset(                void );

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   RegisterOutputBufferRing(     Ring_t          Ring );

    FrameParserStatus_t   ResetCollatedHeaderState(	void );
    unsigned int	  RequiredPresentationLength(	unsigned char		  StartCode );
    FrameParserStatus_t   PresentCollatedHeader(	unsigned char		  StartCode,
							unsigned char		 *HeaderBytes,
							FrameParserHeaderFlag_t	 *Flags );

    //
    // Stream specific functions
    //

    FrameParserStatus_t   ReadHeaders(                                          void );
    FrameParserStatus_t   ResetReferenceFrameList(                              void );
    FrameParserStatus_t   PrepareReferenceFrameList(                            void );

    FrameParserStatus_t   ForPlayUpdateReferenceFrameList(                      void );
    FrameParserStatus_t   ForPlayProcessQueuedPostDecodeParameterSettings(      void );
    FrameParserStatus_t   ForPlayGeneratePostDecodeParameterSettings(           void );
    FrameParserStatus_t   ForPlayPurgeQueuedPostDecodeParameterSettings(        void );

    FrameParserStatus_t   RevPlayQueueFrameForDecode( 				void );
    FrameParserStatus_t   RevPlayProcessDecodeStacks(                           void );
    FrameParserStatus_t   RevPlayGeneratePostDecodeParameterSettings(           void );
    FrameParserStatus_t   RevPlayPurgeQueuedPostDecodeParameterSettings(        void );
    FrameParserStatus_t   RevPlayAppendToReferenceFrameList(                    void );
    FrameParserStatus_t   RevPlayRemoveReferenceFrameFromList(                  void );
    FrameParserStatus_t   RevPlayJunkReferenceFrameList(                        void );
    FrameParserStatus_t   RevPlayNextSequenceFrameProcess(                      void );
};

#endif

