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

#ifndef H_COLLATOR2_BASE
#define H_COLLATOR2_BASE

#include "collator_common.h"

#define MAXIMUM_PARTITION_POINTS    64
#define MINIMUM_ACCUMULATION_HEADROOM   1024

#undef  TRACE_TAG
#define TRACE_TAG "Collator2_Base_c"

typedef struct Collator2Configuration_s
{
    const char      *CollatorName;         // Text in messages

    bool             GenerateStartCodeList;        // Start code list control
    unsigned int     MaxStartCodes;

    unsigned int     StreamIdentifierMask;         // For PES indicates the pes stream identifier
    unsigned int     StreamIdentifierCode;

    unsigned int     SubStreamIdentifierMask;      // For PES of type extended_stream_id indicates the pes sub stream identifier (set to zero if no filtering is to be done)
    unsigned int     SubStreamIdentifierCode;

    unsigned char    IgnoreCodesRangeStart;        // Start codes to ignore, IE all but first slice header
    unsigned char    IgnoreCodesRangeEnd;          // start > end ignores none

    bool             InsertFrameTerminateCode; // if set causes a terminal code to be inserted into the
    unsigned char    TerminalCode;         // buffer, but not recorded in the start code list.

    unsigned int     ExtendedHeaderLength;     // Number of bytes of extended PES header (to be skipped)
} Collator2Configuration_t;

typedef struct PartitionPoint_s
{
    unsigned char        *PartitionBase;
    unsigned int          PartitionSize;

    bool                  Glitch;

    CodedFrameParameters_t CodedFrameParameters;

    unsigned int          StartCodeListIndex;
    unsigned int          NumberOfStartCodes;

    Buffer_t              Buffer;
    FrameParserHeaderFlag_t   FrameFlags;

} PartitionPoint_t;

class Collator2_Base_c : public Collator_Common_c
{
public:
    Collator2_Base_c(void); // TODO(pht) shall take Collator2Configuration_t *configuration in input param
    ~Collator2_Base_c(void);

    CollatorStatus_t   Halt(void);

    //
    // Collator class functions
    //

    CollatorStatus_t   Connect(Port_c *Port);

    CollatorStatus_t   FrameFlush(void);

    CollatorStatus_t   DiscardAccumulatedData(void);

    CollatorStatus_t   SetAlarmParsedPts(void);

    CollatorStatus_t   InputJump(bool                      SurplusDataInjected,
                                 bool                      ContinuousReverseJump,
                                 bool                      FromDiscontinuityControl);

    CollatorStatus_t   NonBlockingWriteSpace(unsigned int        *Size);

    //
    // Function required to be supplied by our inheritors
    //

    virtual CollatorStatus_t   ProcessInputForward(
        unsigned int  DataLength,
        void         *Data,
        unsigned int *DataLengthRemaining) = 0;

    virtual CollatorStatus_t   ProcessInputBackward(
        unsigned int  DataLength,
        void         *Data,
        unsigned int *DataLengthRemaining) = 0;

protected:
    Collator2Configuration_t Configuration;

    OS_Mutex_t        PartitionLock;
    OS_Mutex_t        InputJumpLock;


    bool              NonBlockingInput;                 // Recorded by InputEntry
    bool              ExtendCodedFrameBufferAtEarliestOpportunity;

    bool              DiscardingData;                   // On startup or when in error

    BufferPool_t      CodedFrameBufferPool;
    unsigned int      MaximumCodedFrameSize;
    Buffer_t          CodedFrameBuffer;
    unsigned int      CodedFrameBufferFreeSpace;
    unsigned int      CodedFrameBufferUsedSpace;
    unsigned char    *CodedFrameBufferBase;

    PlayDirection_t   PlayDirection;

    unsigned int        StartCodeReadPartBeforeControlData;
    bool                StartCodeCutByControl;
    bool                NeedToUpdateRemainingData;
    bool                FoundControlInHeader;
    bool                NeedToStepToSCTreatment;
    unsigned int        StoreCodeOffset;
    unsigned char       CutStartCode;

    unsigned int        LargestFrameSeen;
    unsigned int        NextWriteInStartCodeList;
    unsigned int        NextReadInStartCodeList;
    StartCodeList_t    *AccumulatedStartCodeList;
    unsigned int        PartitionPointUsedCount;
    unsigned int        PartitionPointMarkerCount;
    unsigned int        PartitionPointSafeToOutputCount;
    PartitionPoint_t    PartitionPoints[MAXIMUM_PARTITION_POINTS];
    PartitionPoint_t   *NextPartition;

    Stack_t             ReverseFrameStack;
    Stack_t             TemporaryHoldingStack;

    unsigned long long  LimitHandlingLastPTS;
    bool                LimitHandlingJumpInEffect;
    unsigned long long  LimitHandlingJumpAt;

    unsigned long long  LastFramePreGlitchPTS;
    unsigned int        FrameSinceLastPTS;

    bool                InputEntryLive;
    unsigned long long  InputEntryTime;
    unsigned long long  InputExitTime;
    bool  AlarmParsedPtsSet;
    bool  DiscontinuityControlFound;

    CollatorStatus_t   AccumulateData(unsigned int              Length,
                                      unsigned char            *Data);

    CollatorStatus_t   AccumulateStartCode(PackedStartCode_t      Code);

    void               CheckForGlitchPromotion(PartitionPoint_t  *Descriptor);

    virtual void       EmptyCurrentPartition(void);
    virtual void       MoveCurrentPartitionBoundary(int           Bytes);
    virtual void       AccumulateOnePartition(void);
    virtual void       InitializePartition(void);

    CollatorStatus_t   OutputOnePartition(PartitionPoint_t   *Descriptor);

    CollatorStatus_t   PerformOnePartition(PartitionPoint_t  *Descriptor,
                                           Buffer_t         *NewPartition,
                                           Buffer_t         *OutputPartition,
                                           unsigned int          SizeOfFirstPartition);

    CollatorStatus_t   PartitionOutput(unsigned int MinimumAcceptableSize);

    CollatorStatus_t   InputEntry(PlayerInputDescriptor_t    *Input,
                                  unsigned int          DataLength,
                                  void             *Data,
                                  bool              NonBlocking);

    CollatorStatus_t   InputExit(void);

    void               MonitorPCRTiming(unsigned long long    PTS);

private:
    virtual CollatorStatus_t   DoInput(PlayerInputDescriptor_t *Input,
                                       unsigned int             DataLength,
                                       void                    *Data,
                                       bool                     NonBlocking,
                                       unsigned int            *DataLengthRemaining);

    DISALLOW_COPY_AND_ASSIGN(Collator2_Base_c);
};

#endif
