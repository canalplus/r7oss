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

#ifndef H_FRAME_PARSER_VIDEO
#define H_FRAME_PARSER_VIDEO

#include "frame_parser_base.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_Video_c"

#define INVALID_FRAMERATE           0
#define DELTA_MIN_FRAME_WIDTH       64
#define DELTA_MAX_FRAME_WIDTH       1920
#define DELTA_MIN_FRAME_HEIGHT      64
#define DELTA_MAX_FRAME_HEIGHT      1088

/// Framework to unify the approach to video frame parsing.
class FrameParser_Video_c : public FrameParser_Base_c
{
public:
    FrameParser_Video_c(void);
    ~FrameParser_Video_c(void);

    FrameParserStatus_t   Halt(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);
    FrameParserStatus_t   Input(Buffer_t    CodedBuffer);
    FrameParserStatus_t   ResetReverseFailureCounter(void);

    //
    // Extensions to the class overriding the base implementations
    // NOTE in order to keep the names reasonably short, in the following
    // functions specifically for forward playback will be prefixed with
    // ForPlay and functions specific to reverse playback will be prefixed with
    // RevPlay.
    //

    virtual FrameParserStatus_t   ProcessBuffer(void);

    virtual FrameParserStatus_t   ForPlayProcessFrame(void);
    virtual FrameParserStatus_t   ForPlayQueueFrameForDecode(void);

    virtual FrameParserStatus_t   RevPlayProcessFrame(void);
    virtual FrameParserStatus_t   RevPlayQueueFrameForDecode(void);
    virtual FrameParserStatus_t   RevPlayProcessDecodeStacks(void);
    virtual FrameParserStatus_t   RevPlayPurgeDecodeStacks(void);
    virtual FrameParserStatus_t   RevPlayClearResourceUtilization(void);
    virtual FrameParserStatus_t   RevPlayCheckResourceUtilization(void);
    virtual FrameParserStatus_t   RevPlayPurgeUnsatisfiedReferenceStack(void);

    virtual FrameParserStatus_t   InitializePostDecodeParameterSettings(void);
    virtual void                  CalculateFrameIndexAndPts(ParsedFrameParameters_t     *ParsedFrame,
                                                            ParsedVideoParameters_t     *ParsedVideo);
    virtual void                  CalculateDts(ParsedFrameParameters_t      *ParsedFrame,
                                               ParsedVideoParameters_t     *ParsedVideo);

    virtual FrameParserStatus_t   CheckForResolutionConstraints(unsigned int Width, unsigned int Height);
    virtual FrameParserStatus_t   CheckForResolutionBasedOnMemProfile(unsigned int Width, unsigned int Height);
    virtual FrameParserStatus_t   CheckForResolutionBasedOnCodec(unsigned int Width, unsigned int Height);

    //
    // Extensions to the class to be fulfilled by my inheritors,
    // these are required to support the process buffer override
    //

    virtual FrameParserStatus_t   ReadHeaders(void) {return FrameParserNoError;}
    virtual FrameParserStatus_t   PrepareReferenceFrameList(void) {return FrameParserNoError;}
    virtual FrameParserStatus_t   ResetReferenceFrameList(void);

    virtual FrameParserStatus_t   ForPlayUpdateReferenceFrameList(void) {return FrameParserNoError;}
    virtual FrameParserStatus_t   ForPlayProcessQueuedPostDecodeParameterSettings(void);
    virtual FrameParserStatus_t   ForPlayGeneratePostDecodeParameterSettings(void);
    virtual FrameParserStatus_t   ForPlayPurgeQueuedPostDecodeParameterSettings(void);
    virtual FrameParserStatus_t   ForPlayCheckForReferenceReadyForManifestation(void) {return FrameParserNoError;}
    virtual FrameParserStatus_t   ReadRemainingStartCode(void) {return FrameParserNoError;}

    virtual FrameParserStatus_t   RevPlayGeneratePostDecodeParameterSettings(void);
    virtual FrameParserStatus_t   RevPlayPurgeQueuedPostDecodeParameterSettings(void) {return FrameParserNoError;}
    virtual FrameParserStatus_t   RevPlayAppendToReferenceFrameList(void);
    virtual FrameParserStatus_t   RevPlayRemoveReferenceFrameFromList(void);
    virtual FrameParserStatus_t   RevPlayJunkReferenceFrameList(void);
    virtual FrameParserStatus_t   RevPlayNextSequenceFrameProcess(void) {return FrameParserNoError;}

    virtual void                  StoreTemporalReferenceForLastRecordedFrame(ParsedFrameParameters_t *ParsedFrame) {}

protected:
    ParsedVideoParameters_t  *ParsedVideoParameters;

    int                       NextDisplayFieldIndex;        // Can be negative in reverse play
    int                       NextDecodeFieldIndex;
    bool                      CollapseHolesInDisplayIndices;

    bool                      NewStreamParametersSeenButNotQueued;

    Stack_t                   ReverseDecodeUnsatisfiedReferenceStack;
    Stack_t                   ReverseDecodeSingleFrameStack;
    Stack_t                   ReverseDecodeStack;

    bool                      CodedFramePlaybackTimeValid;
    unsigned long long        CodedFramePlaybackTime;
    bool                      CodedFrameDecodeTimeValid;
    unsigned long long        CodedFrameDecodeTime;

    unsigned int              LastRecordedPlaybackTimeDisplayFieldIndex;
    unsigned long long        LastRecordedNormalizedPlaybackTime;
    unsigned int              LastRecordedDecodeTimeFieldIndex;
    unsigned long long        LastRecordedNormalizedDecodeTime;

    ReferenceFrameList_t      ReferenceFrameList;
    Ring_t                    ReverseQueuedPostDecodeSettingsRing;
    bool                      FirstDecodeOfFrame;
    PictureStructure_t        AccumulatedPictureStructure;
    PictureStructure_t        OldAccumulatedPictureStructure;

    Buffer_t                  DeferredCodedFrameBuffer;
    ParsedFrameParameters_t  *DeferredParsedFrameParameters;
    ParsedVideoParameters_t  *DeferredParsedVideoParameters;
    Buffer_t                  DeferredCodedFrameBufferSecondField;
    ParsedFrameParameters_t  *DeferredParsedFrameParametersSecondField;
    ParsedVideoParameters_t  *DeferredParsedVideoParametersSecondField;

    Rational_t                LastFieldRate;

    unsigned int              NumberOfUtilizedFrameParameters;      // Record of utilized resources for reverse play
    unsigned int              NumberOfUtilizedStreamParameters;
    unsigned int              NumberOfUtilizedDecodeBuffers;
    bool                      RevPlayDiscardingState;
    unsigned int              RevPlayAccumulatedFrameCount;
    unsigned int              RevPlayDiscardedFrameCount;
    unsigned int              RevPlaySmoothReverseFailureCount;

    unsigned int              AntiEmulationContent;
    unsigned char            *AntiEmulationBuffer;
    unsigned int              mAntiEmulationBufferMaxSize;
    unsigned char            *AntiEmulationSource;

    Rational_t                LastSeenContentFrameRate;
    unsigned int              LastSeenContentWidth;
    unsigned int              LastSeenContentHeight;

    bool                      StandardPTSDeducedFrameRate;
    Rational_t                LastStandardPTSDeducedFrameRate;

    int                       DeduceFrameRateElapsedFields;
    long long                 DeduceFrameRateElapsedTime;

    Rational_t                DefaultFrameRate;
    Rational_t                ContainerFrameRate;
    Rational_t                PTSDeducedFrameRate;
    Rational_t                StreamEncodedFrameRate;
    Rational_t                LastResolvedFrameRate;

    bool                      LastReverseDecodeWasIndependentFirstField;

    bool                      GotSequenceEndCode;

    unsigned int              mMinFrameWidth;
    unsigned int              mMaxFrameWidth;
    unsigned int              mMinFrameHeight;
    unsigned int              mMaxFrameHeight;

    // Functions

    void                  LoadAntiEmulationBuffer(unsigned char    *Pointer);
    void                  CheckAntiEmulationBuffer(unsigned int      Size);
    void                  DeduceFrameRateFromPresentationTime(long long int   MicroSecondsPerFrame);
    void                  ResetDeferredParameters(void);
    FrameParserStatus_t   TestAntiEmulationBuffer(void);
    Rational_t            ResolveFrameRate(void);

private:
    unsigned int RelayfsIndex;

    DISALLOW_COPY_AND_ASSIGN(FrameParser_Video_c);
};

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Video_c::ResetReferenceFrameList(                  void )
///     \brief Reset the reference frame list after stream discontinuity.
///
///     Called by FrameParser_Base_c::ProcessBuffer whenever a stream discontinuity occurs.
///     This implies that all deferred post decode parameter settings will be deferred.
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Video_c::PrepareReferenceFrameList(                        void )
///     \brief Prepare the reference frame list.
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///

#endif

