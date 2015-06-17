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

#ifndef H_COLLATOR_BASE
#define H_COLLATOR_BASE

#include "player.h"
#include "collator_common.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator_Base_c"

#define MAX_IGNORE_CODE_RANGES  5

typedef struct IgnoreRange_s
{
    unsigned char Start;
    unsigned char End;  // Start and End are part of the ignored codes list
} IgnoreRange_t;

typedef struct IgnoreCodesRanges_s
{
    int NbEntries;
    IgnoreRange_t  Table[MAX_IGNORE_CODE_RANGES];
} IgnoreCodesRanges_t;


typedef struct CollatorConfiguration_s
{
    bool                  GenerateStartCodeList;        // Start code list control
    unsigned int          MaxStartCodes;

    unsigned int          StreamIdentifierMask;         ///< For PES indicates the pes stream identifier
    unsigned int          StreamIdentifierCode;

    unsigned int          SubStreamIdentifierMask;      ///< For PES of type extended_stream_id indicates the pes sub stream identifier (set to zero if no filtering is to be done)
    unsigned int          SubStreamIdentifierCodeStart;
    unsigned int          SubStreamIdentifierCodeStop;

    unsigned char         BlockTerminateMask;           ///< Which Start codes indicate frame complete
    unsigned char         BlockTerminateCode;

    unsigned char         StreamTerminateFlushesFrame;  // Use a stream termination code to force a frame flush (helps display of last frame)
    unsigned char         StreamTerminationCode;

    IgnoreCodesRanges_t   IgnoreCodesRanges;             // Start codes to ignore

    bool                  InsertFrameTerminateCode;     // if set causes a terminal code to be inserted into the
    unsigned char         TerminalCode;                 // buffer, but not recorded in the start code list.

    unsigned int          ExtendedHeaderLength;         ///< Number of bytes of extended PES header (to be skipped)

    bool                  DeferredTerminateFlag;        // Terminate after finding Terminal Code

    bool          DetermineFrameBoundariesByPresentationToFrameParser;  // Ask frame parser about frame boundaries
} CollatorConfiguration_t;

class Collator_Base_c : public Collator_Common_c
{
public:
    Collator_Base_c(void);  // TODO(pht) shall take CollatorConfiguration_t *configuration in input param
    ~Collator_Base_c(void);

    //
    // Override for component base class set module parameters function
    //

    CollatorStatus_t   Halt(void);

    //
    // Collator class functions
    //

    CollatorStatus_t   Connect(Port_c *Port);

    CollatorStatus_t   FrameFlush(void);
    CollatorStatus_t   DiscardAccumulatedData(void);

    CollatorStatus_t   InputJump(bool                      SurplusDataInjected,
                                 bool                      ContinuousReverseJump,
                                 bool                      FromDiscontinuityControl);

    CollatorStatus_t   SetAlarmParsedPts(void);

    //
    // Internal function that may well be overridden (or supplemented)
    //

    virtual CollatorStatus_t   InternalFrameFlush(void);

protected:
    OS_Mutex_t               Lock;
    OS_Mutex_t               InputJumpLock;

    CollatorConfiguration_t  Configuration;

    BufferPool_t             CodedFrameBufferPool;
    Buffer_t                 CodedFrameBuffer;
    unsigned int             MaximumCodedFrameSize;

    unsigned int             AccumulatedDataSize;
    unsigned char           *BufferBase;
    CodedFrameParameters_t  *CodedFrameParameters;
    StartCodeList_t         *StartCodeList;

    bool                     Glitch;
    unsigned long long       LastFramePreGlitchPTS;
    unsigned int             FrameSinceLastPTS;

    unsigned int             InputEntryDepth;
    bool                     InputExitPerformFrameFlush;
    bool                     InputEntryLive;
    unsigned long long       InputEntryTime;
    unsigned long long       InputExitTime;
    bool                     AlarmParsedPtsSet;

    CollatorStatus_t   GetNewBuffer(void);

    CollatorStatus_t   AccumulateData(unsigned int              Length,
                                      unsigned char            *Data);

    CollatorStatus_t   AccumulateStartCode(PackedStartCode_t         Code);

    void               ActOnInputDescriptor(PlayerInputDescriptor_t  *Input);

    void               CheckForGlitchPromotion(void);

    CollatorStatus_t   InputEntry(PlayerInputDescriptor_t    *Input,
                                  unsigned int          DataLength,
                                  void             *Data,
                                  bool              NonBlocking);
    CollatorStatus_t   InputExit(void);

    void               MonitorPCRTiming(unsigned long long    PTS);

    bool               IsCodeTobeIgnored(unsigned char Value);

private:
    DISALLOW_COPY_AND_ASSIGN(Collator_Base_c);

};

#endif
