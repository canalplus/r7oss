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

#include "collator_base.h"

#define INTER_INPUT_STALL_WARNING_TIME      500000      // Say half a second

#define BUFFER_MAX_EXPECTED_WAIT_PERIOD     30000000ULL // Say half a minute
#define MAX_COLLATOR_BUFFER_USED        2       // Maximun number of buffer used by the collator

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//

Collator_Base_c::Collator_Base_c(void)
    : Collator_Common_c()
    , Lock()
    , InputJumpLock()
    , Configuration()
    , CodedFrameBufferPool(NULL)
    , CodedFrameBuffer(NULL)
    , MaximumCodedFrameSize(0)
    , AccumulatedDataSize(0)
    , BufferBase(NULL)
    , CodedFrameParameters(NULL)
    , StartCodeList(NULL)
    , Glitch(false)
    , LastFramePreGlitchPTS(INVALID_TIME)
    , FrameSinceLastPTS(0)
    , InputEntryDepth(0)
    , InputExitPerformFrameFlush(false)
    , InputEntryLive(false)
    , InputEntryTime(INVALID_TIME)
    , InputExitTime(INVALID_TIME)
    , AlarmParsedPtsSet(false)
{
    OS_InitializeMutex(&Lock);
    OS_InitializeMutex(&InputJumpLock);
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

Collator_Base_c::~Collator_Base_c(void)
{
    Collator_Base_c::Halt();
    OS_TerminateMutex(&Lock);
    OS_TerminateMutex(&InputJumpLock);
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

CollatorStatus_t   Collator_Base_c::Halt(void)
{
    unsigned int Count;
    //
    // Make sure input is not still running
    //
    OS_LockMutex(&Lock);

    for (Count = 0; (InputEntryDepth != 0); Count++)
    {
        OS_UnLockMutex(&Lock);
        OS_SleepMilliSeconds(10);
        OS_LockMutex(&Lock);

        if ((Count % 100) == 99)
        {
            SE_FATAL("Input still ongoing\n");
            break;
        }
    }

    //
    // Tidy up
    //

    if (CodedFrameBuffer != NULL)
    {
        CodedFrameBuffer->DecrementReferenceCount(IdentifierCollator);
        CodedFrameBuffer        = NULL;
    }

    CodedFrameBufferPool        = NULL;
    BufferBase                  = NULL;
    CodedFrameParameters        = NULL;
    StartCodeList               = NULL;
    mOutputPort                 = NULL;
    BaseComponentClass_c::Halt();

    OS_UnLockMutex(&Lock);
    return CollatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Set an alarm on chunk ID detection to send a message
//  with the following PTS
//

CollatorStatus_t    Collator_Base_c::SetAlarmParsedPts(void)
{
    return  CollatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Method to connect to neighbor
//

CollatorStatus_t   Collator_Base_c::Connect(Port_c *Port)
{
    PlayerStatus_t  Status;

    if (Port == NULL)
    {
        SE_ERROR("Incorrect input param\n");
        return CollatorError;
    }
    if (mOutputPort != NULL)
    {
        SE_WARNING("Port already connected\n");
    }

    mOutputPort = Port;

    //
    // Obtain the class list, and the coded frame buffer pool
    //
    CodedFrameBufferPool = Stream->GetCodedFrameBufferPool(&MaximumCodedFrameSize);
    //
    // Attach the coded frame parameters to every element of the pool
    //
    Status = CodedFrameBufferPool->AttachMetaData(Player->MetaDataCodedFrameParametersType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to attach coded frame descriptor to all coded frame buffers\n");
        CodedFrameBufferPool    = NULL;
        return Status;
    }

    //
    // Attach optional start code list
    //

    if (Configuration.GenerateStartCodeList)
    {
        Status = CodedFrameBufferPool->AttachMetaData(Player->MetaDataStartCodeListType,
                                                      SizeofStartCodeList(Configuration.MaxStartCodes));
        if (Status != BufferNoError)
        {
            SE_ERROR("Failed to attach start code list to all coded frame buffers\n");
            CodedFrameBufferPool        = NULL;
            return Status;
        }
    }

    //
    // Acquire an operating buffer
    //
    Status      = GetNewBuffer();

    if (Status != CollatorNoError)
    {
        CodedFrameBufferPool    = NULL;
        return Status;
    }

    //
    // Go live
    //
    SetComponentState(ComponentRunning);
    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Frame Flush function
//

CollatorStatus_t   Collator_Base_c::FrameFlush(void)
{
    CollatorStatus_t    Status = CollatorNoError;
    OS_LockMutex(&Lock);

    if (InputEntryDepth != 0)
    {
        InputExitPerformFrameFlush    = true;
    }
    else
    {
        Status    = InternalFrameFlush();
    }

    OS_UnLockMutex(&Lock);
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Frame Flush implementation
//

CollatorStatus_t   Collator_Base_c::InternalFrameFlush(void)
{
    CollatorStatus_t  Status;
    unsigned char     TerminalStartCode[4];
    Buffer_t          BufferToOutput;

    if ((AccumulatedDataSize == 0) &&
        !CodedFrameParameters->StreamDiscontinuity &&
        !CodedFrameParameters->ContinuousReverseJump)
    {
        return CollatorNoError;
    }

    if ((AccumulatedDataSize != 0) && Configuration.InsertFrameTerminateCode)
    {
        TerminalStartCode[0]    = 0x00;
        TerminalStartCode[1]    = 0x00;
        TerminalStartCode[2]    = 0x01;
        TerminalStartCode[3]    = Configuration.TerminalCode;
        Status  = AccumulateData(4, TerminalStartCode);
        if (Status != CollatorNoError)
        {
            SE_ERROR("Failed to add terminal start code\n");
            DiscardAccumulatedData();
        }
    }

    if (CodedFrameBuffer == NULL)
    {
        return CollatorNoError;
    }

    CodedFrameBuffer->SetUsedDataSize(AccumulatedDataSize);
    BufferStatus_t BufStatus = CodedFrameBuffer->ShrinkBuffer(max(AccumulatedDataSize, 1));
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to shrink the operating buffer to size (%08x)\n", BufStatus);
    }

    //
    // we check if the policy 'ReduceCollatedData' is set then we ask how many 'max' reference frame
    // are required to decode correctly this stream
    // Only if we are in forward mode
    //
    bool ReduceCollatedData = (Player->PolicyValue(Playback, Stream, PolicyReduceCollatedData) == PolicyValueApply);
    if (ReduceCollatedData)
    {
        unsigned int      TotalBuffers;
        unsigned int      BuffersUsed;
        //
        // We need to compute 'N' which is the min number of buffers required for a correct decoding
        // N = max collator buffer used(*1) + the max number of reference frames ; (*1) = 2
        // If the number of bufferUsed is equal to N then we wait a sufficient decode frame period to
        // allow release of buffer after decode and by the way get a bufferUsed < 'N'
        //
        unsigned int MaxRefFrames;
        unsigned int MaxBufferForReduceCollatedData = 0;
        unsigned long long EntryTime;
        //
        // Need to know how many buffer are used
        //
        CodedFrameBufferPool->GetPoolUsage(&TotalBuffers, &BuffersUsed);
        MaxRefFrames = Stream->GetFrameParser()->GetMaxReferenceCount();
        //
        // Compute 'N'
        //
        MaxBufferForReduceCollatedData = MAX_COLLATOR_BUFFER_USED + MaxRefFrames;
        SE_DEBUG(group_collator_video, "MaxBufferForReduceCollatedData = %d, MaxRefFrames =%d BuffersUsed =%d\n", MaxBufferForReduceCollatedData, MaxRefFrames, BuffersUsed);

        if (BuffersUsed >= MaxBufferForReduceCollatedData)
        {
            //
            // Wait until a buffer be released
            //
            EntryTime  = OS_GetTimeInMicroSeconds();

            while (true)
            {
                //
                // Sleep a while before check again
                // Each 100ms is fast enough
                OS_SleepMilliSeconds(100);
                CodedFrameBufferPool->GetPoolUsage(&TotalBuffers, &BuffersUsed);

                if (BuffersUsed < MaxBufferForReduceCollatedData)
                {
                    break;
                }

                //
                // The wait can last until a decoded buffer has been freed specfically when
                // all decode buffers are queued to manifestor.
                // We can assume that decoded buffers will be released before playstream delete
                //
                if ((OS_GetTimeInMicroSeconds() - EntryTime) > BUFFER_MAX_EXPECTED_WAIT_PERIOD)
                {
                    SE_DEBUG(group_collator_video, "Waiting for buffer : Reduce collated buffer (%d) in use (%d)", MaxBufferForReduceCollatedData, BuffersUsed);
                    EntryTime = OS_GetTimeInMicroSeconds();
                }
            }
        }
    }

    //
    // In order to ensure that we always have a buffer,
    // we get a new buffer before outputting the current one.
    // Also in order to ensure we do not punt out one buffer twice
    // we need to lock the three lines of code between the copy of
    // CodedFrameBuffer, and its actual output.
    //
    CodedFrameParameters->CollationTime = OS_GetTimeInMicroSeconds();
    CheckForGlitchPromotion();
    DelayForInjectionThrottling(CodedFrameParameters);
    BufferToOutput  = CodedFrameBuffer;
    Status = GetNewBuffer();
    if (Status != CollatorNoError) { return Status; }

    Status = InsertInOutputPort(BufferToOutput);

    return Status;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The discard all accumulated data function
//

CollatorStatus_t   Collator_Base_c::DiscardAccumulatedData(void)
{
    if (CodedFrameBuffer != NULL)
    {
        AccumulatedDataSize = 0;
        CodedFrameBuffer->SetUsedDataSize(AccumulatedDataSize);

        if (Configuration.GenerateStartCodeList)
        {
            StartCodeList->NumberOfStartCodes       = 0;
        }

        memset(CodedFrameParameters, 0, sizeof(CodedFrameParameters_t));

        // Set default SourceTimeFormat to TimeFormatPts
        CodedFrameParameters->SourceTimeFormat = TimeFormatPts;
    }

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      This function is called when we enter input
//  for a specific collator

CollatorStatus_t   Collator_Base_c::InputEntry(
    PlayerInputDescriptor_t  *Input,
    unsigned int          DataLength,
    void             *Data,
    bool              NonBlocking)
{
    //
    // Perform Input stall checking for live playback
    //
    InputEntryLive  = (Player->PolicyValue(Playback, Stream, PolicyLivePlayback) == PolicyValueApply);
    InputEntryTime  = OS_GetTimeInMicroSeconds();

    if (InputEntryLive &&
        ValidTime(InputExitTime) &&
        ((InputEntryTime - InputExitTime) > INTER_INPUT_STALL_WARNING_TIME))
        SE_INFO(GetGroupTrace(), "(%d) - Stall warning, time between input exit and next entry = %lldms\n",
                Configuration.GenerateStartCodeList, (InputEntryTime - InputExitTime) / 1000);

    OS_LockMutex(&Lock);
    InputEntryDepth++;
    OS_UnLockMutex(&Lock);

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This function is called when we exit input
//  for a specific collator

CollatorStatus_t   Collator_Base_c::InputExit(void)
{
    CollatorStatus_t    Status;
//
    OS_LockMutex(&Lock);
    Status      = CollatorNoError;

    if ((InputEntryDepth == 1) && InputExitPerformFrameFlush)
    {
        Status              = InternalFrameFlush();
        InputExitPerformFrameFlush  = false;
    }

    InputEntryDepth--;
    OS_UnLockMutex(&Lock);
    //
    // Perform Input stall checking for live playback
    //
    InputExitTime   = OS_GetTimeInMicroSeconds();

    if (InputEntryLive &&
        ((InputExitTime - InputEntryTime) > INTER_INPUT_STALL_WARNING_TIME))
        SE_INFO(GetGroupTrace(), "(%d) - Stall warning, time between input entry and exit = %lldms\n",
                Configuration.GenerateStartCodeList, (InputExitTime - InputEntryTime) / 1000);

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Handle a jump
//

CollatorStatus_t   Collator_Base_c::InputJump(
    bool                      SurplusDataInjected,
    bool                      ContinuousReverseJump,
    bool                      FromDiscontinuityControl)
{
    bool    UseInputJumpLockMutex = true;

    AssertComponentState(ComponentRunning);

    // To avoid throttling mechanism when a flush is on-going
    SetDrainingStatus(true);

    if (FromDiscontinuityControl)
    {
        // called from Input() context so the lock is already locked
        UseInputJumpLockMutex = false;
    }

    if (UseInputJumpLockMutex)
    {
        // If we are running Input(), wait until it finish, then resume Drain process
        if (OS_MutexIsLocked(&InputJumpLock))
        {
            SE_INFO(GetGroupTrace(), "InputJumpLock locked: Input process is running! Awaiting unlock..\n");
        }

        OS_LockMutex(&InputJumpLock);
    }

    if (SurplusDataInjected)
    {
        DiscardAccumulatedData();
    }
    else
    {
        FrameFlush();
    }

    CodedFrameParameters->StreamDiscontinuity           = true;
    CodedFrameParameters->FlushBeforeDiscontinuity      = SurplusDataInjected;
    CodedFrameParameters->ContinuousReverseJump         = ContinuousReverseJump;
    FrameFlush();
    /* Reset any pending notification of chunk ID in case of discontinuity */
    AlarmParsedPtsSet = false;

    if (UseInputJumpLockMutex)
    {
        // Unlock to resume Input() function
        OS_UnLockMutex(&InputJumpLock);
    }

    SetDrainingStatus(false);

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Get a new buffer
//

CollatorStatus_t   Collator_Base_c::GetNewBuffer(void)
{
    BufferStatus_t Status;

    Status = CodedFrameBufferPool->GetBuffer(&CodedFrameBuffer, IdentifierCollator, MaximumCodedFrameSize);
    if (Status != BufferNoError)
    {
        CodedFrameBuffer = NULL;
        SE_ERROR("Failed to obtain an operating buffer\n");
        return CollatorError;
    }

    AccumulatedDataSize = 0;
    CodedFrameBuffer->ObtainDataReference(NULL, NULL, (void **)(&BufferBase));
    SE_ASSERT(BufferBase != NULL); // cannot be empty

    CodedFrameBuffer->ObtainMetaDataReference(Player->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters));
    SE_ASSERT(CodedFrameParameters != NULL);

    memset(CodedFrameParameters, 0, sizeof(CodedFrameParameters_t));
    // Set default SourceTimeFormat to TimeFormatPts.
    CodedFrameParameters->SourceTimeFormat = TimeFormatPts;

    if (Configuration.GenerateStartCodeList)
    {
        CodedFrameBuffer->ObtainMetaDataReference(Player->MetaDataStartCodeListType, (void **)(&StartCodeList));
        SE_ASSERT(StartCodeList != NULL);

        StartCodeList->NumberOfStartCodes       = 0;
    }

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - The accumulate data into the buffer function
//

CollatorStatus_t   Collator_Base_c::AccumulateData(
    unsigned int              Length,
    unsigned char            *Data)
{
    if ((AccumulatedDataSize + Length) > MaximumCodedFrameSize)
    {
        SE_ERROR("Buffer overflow. (%d > %d)\n", (AccumulatedDataSize + Length) , MaximumCodedFrameSize);
        return CollatorBufferOverflow;
    }

    memcpy(BufferBase + AccumulatedDataSize, Data, Length);
    AccumulatedDataSize += Length;
    CodedFrameBuffer->SetUsedDataSize(AccumulatedDataSize);
    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - The accumulate data into the buffer function
//

CollatorStatus_t   Collator_Base_c::AccumulateStartCode(PackedStartCode_t      Code)
{
    if (!Configuration.GenerateStartCodeList)
    {
        return CollatorNoError;
    }

    if ((StartCodeList->NumberOfStartCodes + 1) > Configuration.MaxStartCodes)
    {
        SE_ERROR("Start code list overflow\n");
        return CollatorBufferOverflow;
    }

    StartCodeList->StartCodes[StartCodeList->NumberOfStartCodes++]      = Code;
    return CollatorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///     Copy any important information held in the input descriptor to the
///     coded frame parameters.
///
///     If the input descriptor provides any hints about playback or
///     presentation time then initialize Collator_Base_c::CodedFrameParameters
///     with these values.
///
///     \param Input The input descriptor
///
void Collator_Base_c::ActOnInputDescriptor(PlayerInputDescriptor_t   *Input)
{
    if (Input->PlaybackTimeValid && !CodedFrameParameters->PlaybackTimeValid)
    {
        CodedFrameParameters->PlaybackTimeValid = true;
        CodedFrameParameters->PlaybackTime      = Input->PlaybackTime;
        CodedFrameParameters->SourceTimeFormat  = Input->SourceTimeFormat;
    }

    if (Input->DecodeTimeValid && !CodedFrameParameters->DecodeTimeValid)
    {
        CodedFrameParameters->DecodeTimeValid   = true;
        CodedFrameParameters->DecodeTime        = Input->DecodeTime;
        CodedFrameParameters->SourceTimeFormat  = Input->SourceTimeFormat;
    }

    CodedFrameParameters->DataSpecificFlags     |= Input->DataSpecificFlags;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Private - Check if we should promote a glitch to a full blown
//        discontinuity. This works by checking pts flow around
//        the glitch.
//

void   Collator_Base_c::CheckForGlitchPromotion(void)
{
    long long        DeltaPTS;
    long long        Range;

    if (CodedFrameParameters->PlaybackTimeValid)
    {
        //
        // Handle any glitch promotion
        //     We promote if there is a glitch, and if the
        //     PTS varies by more than the maximum of 1/4 second, and 40ms times the number of frames that have passed since a pts.
        //
        if (Glitch &&
            (ValidTime(LastFramePreGlitchPTS)))
        {
            DeltaPTS = CodedFrameParameters->PlaybackTime - LastFramePreGlitchPTS;
            Range    = max(22500, (3600 * FrameSinceLastPTS));

            if (!inrange(DeltaPTS, -Range, Range))
            {
                SE_INFO(GetGroupTrace(), "(%d) Promoted\n", Configuration.GenerateStartCodeList);
                CodedFrameParameters->StreamDiscontinuity           = true;
                CodedFrameParameters->FlushBeforeDiscontinuity      = false;
                CodedFrameParameters->ContinuousReverseJump         = false;
            }
        }

        //
        // Remember the frame pts
        //
        Glitch          = false;
        LastFramePreGlitchPTS   = CodedFrameParameters->PlaybackTime;
        FrameSinceLastPTS   = 0;
    }

    FrameSinceLastPTS++;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Check the difference between an injected PTS and
//          the current PCR
//

void   Collator_Base_c::MonitorPCRTiming(unsigned long long   PTS)
{
    PlayerStatus_t      Status;
    unsigned long long  PCR;
    unsigned long long  SystemTime;
    unsigned long long  LatencyPTS;
    unsigned long long  ManifestPTS;
    long long       UpstreamLatency     = 0;
    long long       DownstreamLatency   = 0;
    unsigned long long  StreamDelay;

    //
    // anything to do
    //

    if ((Player->PolicyValue(Playback, Stream, PolicyLivePlayback) != PolicyValueApply) ||
        (Player->PolicyValue(Playback, Stream, PolicyMonitorPCRTiming) != PolicyValueApply) ||
        (Player->PolicyValue(Playback, Stream, PolicyRunningDevlog) == PolicyValueApply))
    {
        return;
    }

    //
    // Get the stream delay
    //
    Status  = Stream->GetOutputTimer()->GetStreamStartDelay(&StreamDelay);

    if (Status != OutputTimerNoError)
    {
        StreamDelay   = 0;
    }

    //
    // Do we have a pcr to compare against
    //
    Status  = Player->ClockRecoveryEstimate(Playback, &PCR, &SystemTime);

    if (Status == PlayerNoError)
    {
        LatencyPTS  = PTS;

        if (((PTS ^ PCR) & 0x100000000) != 0)
        {
            if (((PTS & 0x180000000) == 0x180000000) && ((PCR & 0x180000000) == 0))
            {
                PCR       += 0x200000000;
            }

            if (((PCR & 0x180000000) == 0x180000000) && ((PTS & 0x180000000) == 0))
            {
                LatencyPTS    += 0x200000000;
            }
        }

        UpstreamLatency = (((long long)LatencyPTS - (long long)PCR) * 300) / 27;

        if (!inrange(UpstreamLatency, 0, 4000000ll))
        {
            SE_INFO(GetGroupTrace(), "(%d) - Upstream latency is %5lldms (%5lld)\n",
                    Configuration.GenerateStartCodeList, UpstreamLatency / 1000, StreamDelay / 1000);
        }
    }

    //
    // What PTS do we believe should be on display
    //
    Status  = Player->RetrieveNativePlaybackTime(Playback, &ManifestPTS);

    if (Status == PlayerNoError)
    {
        if (((PTS ^ ManifestPTS) & 0x100000000) != 0)
        {
            if (((PTS & 0x180000000) == 0x180000000) && ((ManifestPTS & 0x180000000) == 0))
            {
                ManifestPTS   += 0x200000000;
            }

            if (((ManifestPTS & 0x180000000) == 0x180000000) && ((PTS & 0x180000000) == 0))
            {
                PTS       += 0x200000000;
            }
        }

        DownstreamLatency   = (((long long)PTS - (long long)ManifestPTS) * 300) / 27;

        if (!inrange(DownstreamLatency, 0, 4000000ll) || !inrange((UpstreamLatency + DownstreamLatency), 0, 4000000ll))
        {
            SE_INFO(GetGroupTrace(), "(%d) - Downstream latency is %5lldms, Total Latency is %5lldms (%5lld)\n",
                    Configuration.GenerateStartCodeList, DownstreamLatency / 1000,
                    (UpstreamLatency + DownstreamLatency) / 1000, StreamDelay / 1000);
        }
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Check value in the ignore codes ranges
//

bool  Collator_Base_c::IsCodeTobeIgnored(unsigned char Value)
{
    for (int i = 0 ; i < Configuration.IgnoreCodesRanges.NbEntries ; i++)
        if ((Value >= Configuration.IgnoreCodesRanges.Table[i].Start) && (Value <= Configuration.IgnoreCodesRanges.Table[i].End))
        {
            return true;
        }
    return false;
}
