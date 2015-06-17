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

#ifndef H_FRAME_PARSER_VIDEO_H264_MVC
#define H_FRAME_PARSER_VIDEO_H264_MVC

#include "frame_parser_video_h264.h"
#include "mvc.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoH264_MVC_c"

typedef struct SubsetSequenceParameterSetEntry_s
{
    Buffer_t                                  Buffer;
    MVCSubSequenceParameterSetHeader_t       *Header;
} SubsetSequenceParameterSetEntry_t;

class FrameParser_VideoH264_MVC_c : public FrameParser_VideoH264_c
{
public:
    FrameParser_VideoH264_MVC_c(void);
    ~FrameParser_VideoH264_MVC_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);
    FrameParserStatus_t   PresentCollatedHeader(unsigned char StartCode,
                                                unsigned char *HeaderBytes, FrameParserHeaderFlag_t *Flags);
    FrameParserStatus_t   ReadHeaders(void);

    FrameParserStatus_t   ReadRefPicListReordering(void);

    FrameParserStatus_t   ForPlayUpdateReferenceFrameList(void);
    FrameParserStatus_t   PrepareReferenceFrameList(void);
    FrameParserStatus_t   MarkReferenceDepPictures(bool ActuallyReleaseReferenceFrames);
    FrameParserStatus_t   CalculateReferencePictureListsFrame(void);
    FrameParserStatus_t   CalculateReferencePictureListsField(void);
    FrameParserStatus_t   InitializeReferencePictureListField(ReferenceFrameList_t *ShortTermList,
                                                              ReferenceFrameList_t *LongTermList, unsigned int MaxListEntries, ReferenceFrameList_t *List);
    FrameParserStatus_t   InitializePSliceReferencePictureListFrame(void);
    FrameParserStatus_t   InitializePSliceReferencePictureListField(void);
    FrameParserStatus_t   InitializeBSliceReferencePictureListsFrame(void);
    FrameParserStatus_t   InitializeBSliceReferencePictureListsField(void);
    FrameParserStatus_t   ReleaseDepReference(bool ActuallyRelease,
                                              unsigned int Entry, unsigned int ReleaseUsage);
    FrameParserStatus_t   ResetReferenceFrameList(void);
    FrameParserStatus_t   CalculatePicOrderCnts(void);
    bool                  NewStreamParametersCheck(void);
    FrameParserStatus_t   PrepareNewStreamParameters(void);
    FrameParserStatus_t   CommitFrameForDecode(void);
    unsigned int ComputeDpbSize(MVCSubSequenceParameterSetHeader_t *Header);

    //
    // Stream specific functions
    //

    FrameParserStatus_t   ReadSingleHeader(unsigned int Code, unsigned int UnitLength);
    bool                  isHighProfile(unsigned int profile_idc);
    FrameParserStatus_t   ReadSeiPayload(unsigned int PayloadType, unsigned int PayloadSize);

private:
    BufferType_t                                  SubsetSequenceParameterSetType;
    BufferPool_t                                  SubsetSequenceParameterSetPool;
    SubsetSequenceParameterSetEntry_t             SubsetSequenceParameterSetTable[H264_STANDARD_MAX_SEQUENCE_PARAMETER_SETS];
    bool                                          ReadNewSubsetSPS;

    MVCStreamParameters_t                         *MVCStreamParameters;
    MVCFrameParameters_t                          *MVCFrameParameters;

    bool                       isAnchor;    // is current access unit an anchor AU ?
    bool                       isInterview; // is base picture of current access unit an interview reference ?

    H264ReferenceFrameData_t                      ReferenceDepFrames[H264_MAX_REFERENCE_FRAMES + 1]; // non-base view ref frames
    unsigned int                                  NumReferenceDepFrames;
    unsigned int                                  NumLongTermDep;
    unsigned int                                  NumShortTermDep;
    ReferenceFrameList_t                          ReferenceDepFrameList[H264_NUM_REF_FRAME_LISTS];
    ReferenceFrameList_t                          ReferenceDepFrameListShortTerm[2];
    ReferenceFrameList_t                          ReferenceDepFrameListLongTerm;

    FrameParserStatus_t   SetDefaultSubSequenceParameterSet(MVCSubSequenceParameterSetHeader_t *Header);
    FrameParserStatus_t   ReadNalSubSequenceParameterSet(void);
    FrameParserStatus_t   ReadMVCRefPicListReordering(void);
    FrameParserStatus_t   ReadNalPrefix(unsigned int Code, bool isSlice);
    FrameParserStatus_t   ReadNalExtSliceHeader(unsigned int UnitLength);

    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoH264_MVC_c);
};

#endif
