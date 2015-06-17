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

#include "stack_generic.h"
#include "collator2_base.h"

#define MAXIMUM_ACCUMULATED_START_CODES     2048

#define INTER_INPUT_STALL_WARNING_TIME      500000      // Say half a second
#define BUFFER_MAX_EXPECTED_WAIT_PERIOD     30000000ULL     // Say half a minute

#define MAX_COLLATOR_BUFFER_USED        2       // Maximun number of buffer used by the collator

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//

Collator2_Base_c::Collator2_Base_c(void)
    : Collator_Common_c()
    , Configuration()
    , PartitionLock()
    , InputJumpLock()
    , NonBlockingInput(false)
    , ExtendCodedFrameBufferAtEarliestOpportunity(false)
    , DiscardingData(true)
    , CodedFrameBufferPool()
    , MaximumCodedFrameSize(0)
    , CodedFrameBuffer()
    , CodedFrameBufferFreeSpace(0)
    , CodedFrameBufferUsedSpace(0)
    , CodedFrameBufferBase(NULL)
    , PlayDirection(PlayForward)
    , StartCodeReadPartBeforeControlData()
    , StartCodeCutByControl(false)
    , NeedToUpdateRemainingData(false)
    , FoundControlInHeader(false)
    , NeedToStepToSCTreatment(false)
    , StoreCodeOffset(0)
    , CutStartCode(0)
    , LargestFrameSeen(0)
    , NextWriteInStartCodeList(0)
    , NextReadInStartCodeList(0)
    , AccumulatedStartCodeList(NULL)
    , PartitionPointUsedCount(0)
    , PartitionPointMarkerCount(0)
    , PartitionPointSafeToOutputCount(0)
    , PartitionPoints()
    , NextPartition(NULL)
    , ReverseFrameStack(NULL)
    , TemporaryHoldingStack(NULL)
    , LimitHandlingLastPTS(INVALID_TIME)
    , LimitHandlingJumpInEffect(false)
    , LimitHandlingJumpAt(INVALID_TIME)
    , LastFramePreGlitchPTS(INVALID_TIME)
    , FrameSinceLastPTS(0)
    , InputEntryLive(false)
    , InputEntryTime(INVALID_TIME)
    , InputExitTime(INVALID_TIME)
    , AlarmParsedPtsSet(false)
    , DiscontinuityControlFound(false)
{
    OS_InitializeMutex(&PartitionLock);
    OS_InitializeMutex(&InputJumpLock);

    // TODO(pht) when Configuration is passed as ctor parameter,
    // do allocation based on Configuration.GenerateStartCodeList
    // or since currently always true, remove GenerateStartCodeList parameter ?
    AccumulatedStartCodeList = (StartCodeList_t *)new unsigned char[SizeofStartCodeList(MAXIMUM_ACCUMULATED_START_CODES)];

    ReverseFrameStack     = new class StackGeneric_c(MAXIMUM_PARTITION_POINTS);
    TemporaryHoldingStack = new class StackGeneric_c(MAXIMUM_PARTITION_POINTS);
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

Collator2_Base_c::~Collator2_Base_c(void)
{
    Collator2_Base_c::Halt();

    delete ReverseFrameStack;
    delete TemporaryHoldingStack;

    delete[] AccumulatedStartCodeList;

    OS_TerminateMutex(&PartitionLock);
    OS_TerminateMutex(&InputJumpLock);
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

CollatorStatus_t   Collator2_Base_c::Halt(void)
{
    unsigned int    BufferAndFlag;
    Buffer_t        Buffer;
    StackStatus_t   Status = StackNoError;

    if (ReverseFrameStack != NULL)
    {
        while (ReverseFrameStack->NonEmpty())
        {
            Status = ReverseFrameStack->Pop(&BufferAndFlag);

            if (Status == StackNoError)
            {
                Buffer  = (Buffer_t)(BufferAndFlag & ~1);
                Buffer->DecrementReferenceCount(IdentifierCollator);
            }
            else
            {
                SE_ERROR("Pop failed with ReverseFrameStack\n");
                break;
            }
        }
    }

    if (TemporaryHoldingStack != NULL)
    {
        while (TemporaryHoldingStack->NonEmpty())
        {
            Status = TemporaryHoldingStack->Pop(&BufferAndFlag);

            if (Status == StackNoError)
            {
                Buffer  = (Buffer_t)(BufferAndFlag & ~1);
                Buffer->DecrementReferenceCount(IdentifierCollator);
            }
            else
            {
                SE_ERROR("Pop failed with TemporaryHoldingStack\n");
                break;
            }
        }
    }

    if (CodedFrameBuffer != NULL)
    {
        CodedFrameBuffer->DecrementReferenceCount(IdentifierCollator);
        CodedFrameBuffer        = NULL;
    }

    CodedFrameBufferPool    = NULL;
    CodedFrameBufferBase    = NULL;
    mOutputPort             = NULL;

    return BaseComponentClass_c::Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Set an alarm on chunk ID detection to send a message
//  with the following PTS
//

CollatorStatus_t    Collator2_Base_c::SetAlarmParsedPts(void)
{
    return  CollatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Method to connect to neighbor
//      TODO(pht) fixme => handle proper cleanups on error paths..
//

CollatorStatus_t   Collator2_Base_c::Connect(Port_c *Port)
{
    BufferStatus_t Status;

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
        CodedFrameBufferPool = NULL;
        return CollatorError;
    }

    //
    // If we wish to collect start codes, create our master list, and attach one to each buffer
    //

    if (Configuration.GenerateStartCodeList)
    {
        if (AccumulatedStartCodeList == NULL)
        {
            SE_ERROR("Failed to create accumulated start code list\n");
            CodedFrameBufferPool = NULL;
            return CollatorError;
        }

        memset(AccumulatedStartCodeList, 0, SizeofStartCodeList(MAXIMUM_ACCUMULATED_START_CODES));

        Status = CodedFrameBufferPool->AttachMetaData(Player->MetaDataStartCodeListType,
                                                      SizeofStartCodeList(Configuration.MaxStartCodes));
        if (Status != BufferNoError)
        {
            SE_ERROR("Failed to attach start code list to all coded frame buffers\n");
            CodedFrameBufferPool = NULL;
            return CollatorError;
        }
    }

    //
    // check allocation of the reverse play stacks
    //
    if ((ReverseFrameStack == NULL) || (TemporaryHoldingStack == NULL))
    {
        CodedFrameBufferPool    = NULL;
        SE_ERROR("Failed to obtain reverse collation stacks\n");
        return PlayerInsufficientMemory;
    }

    //
    // Acquire an operating buffer, hopefully all of the memory available to this pool
    //
    Status = CodedFrameBufferPool->GetBuffer(&CodedFrameBuffer, IdentifierCollator, MaximumCodedFrameSize, false, true);
    if (Status != BufferNoError)
    {
        CodedFrameBufferPool    = NULL;
        SE_ERROR("Failed to obtain an operating buffer\n");
        return CollatorError;
    }

    NonBlockingInput = false;
    ExtendCodedFrameBufferAtEarliestOpportunity = false;
    CodedFrameBufferUsedSpace = 0;
    CodedFrameBuffer->SetUsedDataSize(CodedFrameBufferUsedSpace);
    CodedFrameBuffer->ObtainDataReference(&CodedFrameBufferFreeSpace, NULL, (void **)(&CodedFrameBufferBase));
    SE_ASSERT(CodedFrameBufferBase != NULL); // cannot be empty
    InitializePartition();

    //
    // Go live
    //
    SetComponentState(ComponentRunning);
    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The input function
//

CollatorStatus_t   Collator2_Base_c::DoInput(PlayerInputDescriptor_t   *Input,
                                             unsigned int               DataLength,
                                             void                      *Data,
                                             bool                       NonBlocking,
                                             unsigned int              *DataLengthRemaining)
{
    CollatorStatus_t     Status;
    Rational_t       Speed;
    PlayDirection_t      Direction;
    PlayDirection_t      PreviousDirection;

    AssertComponentState(ComponentRunning);

    // If we are running InputJump(), wait until it finish, then resume data injection
    if (OS_MutexIsLocked(&InputJumpLock))
    {
        SE_INFO(group_collator_video, "InputJumpLock locked: Drain process is running! Awaiting unlock..\n");
    }

    OS_LockMutex(&InputJumpLock);

    //
    // Initialize return value
    //

    if (DataLengthRemaining != NULL)
    {
        *DataLengthRemaining  = DataLength;
    }

    //
    // Are we in reverse, and operating in reversible mode
    //
    PreviousDirection   = PlayDirection;
    PlayDirection   = PlayForward;
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

    if (Direction == PlayBackward)
    {
        int Policy = Player->PolicyValue(Playback, Stream, PolicyOperateCollator2InReversibleMode);

        if (Policy == PolicyValueApply)
        {
            PlayDirection   = PlayBackward;
        }
    }

    //
    // Check for switch of direction
    //

    if (PreviousDirection != PlayDirection)
    {
        if ((PartitionPointUsedCount != 0) || (NextPartition->PartitionSize != 0))
        {
            SE_ERROR("(%s) Attempt to switch direction without proper flushing\n", Configuration.CollatorName);
            PlayDirection   = PreviousDirection;
            // Unlock to allow InputJump() function to run if need
            OS_UnLockMutex(&InputJumpLock);
            return CollatorError;
        }

        InitializePartition();      // Force base to be re-calculated
    }

    //
    // Perform input entry activity, may result in a would block status
    //
    OS_LockMutex(&PartitionLock);
    Status  = InputEntry(Input, DataLength, Data, NonBlocking);

    //
    // Process the input
    //

    if (Status == CollatorNoError)
    {
        if (PlayDirection == PlayForward)
        {
            Status    = ProcessInputForward(DataLength, Data, DataLengthRemaining);
        }
        else
        {
            Status    = ProcessInputBackward(DataLength, Data, DataLengthRemaining);
        }
    }

    //
    // Handle exit
    //

    if (Status == CollatorNoError)
    {
        Status = InputExit();
    }

    OS_UnLockMutex(&PartitionLock);
    // Unlock to allow InputJump() function to run if need
    OS_UnLockMutex(&InputJumpLock);
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This function is called when we enter input
//  for a specific collator

CollatorStatus_t   Collator2_Base_c::InputEntry(
    PlayerInputDescriptor_t  *Input,
    unsigned int          DataLength,
    void             *Data,
    bool              NonBlocking)
{
    CollatorStatus_t    Status;
    //
    // Perform Input stall checking for live playback
    //
    InputEntryLive  = (Player->PolicyValue(Playback, Stream, PolicyLivePlayback) == PolicyValueApply);
    InputEntryTime  = OS_GetTimeInMicroSeconds();

    if (InputEntryLive &&
        ValidTime(InputExitTime) &&
        ((InputEntryTime - InputExitTime) > INTER_INPUT_STALL_WARNING_TIME))
        SE_INFO(group_collator_video, "(%s) - Stall warning, time between input exit and next entry = %lldms\n",
                Configuration.CollatorName, (InputEntryTime - InputExitTime) / 1000);

    //
    // If the input descriptor provides any hints about playback or
    // presentation time then initialize CodedFrameParameters
    // with these values.
    //

    if (Input->PlaybackTimeValid && !NextPartition->CodedFrameParameters.PlaybackTimeValid)
    {
        NextPartition->CodedFrameParameters.PlaybackTimeValid = true;
        NextPartition->CodedFrameParameters.PlaybackTime      = Input->PlaybackTime;
        NextPartition->CodedFrameParameters.SourceTimeFormat  = Input->SourceTimeFormat;
    }

    if (Input->DecodeTimeValid && !NextPartition->CodedFrameParameters.DecodeTimeValid)
    {
        NextPartition->CodedFrameParameters.DecodeTimeValid   = true;
        NextPartition->CodedFrameParameters.DecodeTime        = Input->DecodeTime;
    }

    NextPartition->CodedFrameParameters.DataSpecificFlags     |= Input->DataSpecificFlags;
    //
    // Do we have any activity to complete that would have caused us to block previously
    //
    Status      = CollatorNoError;
    NonBlockingInput    = NonBlocking;

    if (ExtendCodedFrameBufferAtEarliestOpportunity)
    {
        Status  = PartitionOutput(0);
    }

//
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This function is called when we exit input
//  for a specific collator

CollatorStatus_t   Collator2_Base_c::InputExit(void)
{
    CollatorStatus_t Status = CollatorNoError;

    if (PartitionPointSafeToOutputCount != 0)
    {
        Status  = PartitionOutput(0);
    }

    //
    // Perform Input stall checking for live playback
    //
    InputExitTime   = OS_GetTimeInMicroSeconds();

    if (InputEntryLive &&
        ((InputExitTime - InputEntryTime) > INTER_INPUT_STALL_WARNING_TIME))
        SE_INFO(group_collator_video, "(%s) - Stall warning, time between input entry and exit = %lldms\n",
                Configuration.CollatorName, (InputExitTime - InputEntryTime) / 1000);

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//    Protected - Support functions for adjusting partition boundary
//    to recognize data added or removed


void   Collator2_Base_c::EmptyCurrentPartition(void)
{
    MoveCurrentPartitionBoundary(-NextPartition->PartitionSize);
    NextWriteInStartCodeList        = NextPartition->StartCodeListIndex;        // Wind back the start code list
    NextPartition->NumberOfStartCodes   = 0;
}

// ------

void   Collator2_Base_c::MoveCurrentPartitionBoundary(int         Bytes)
{
    if (PlayDirection != PlayForward)
    {
        NextPartition->PartitionBase  -= Bytes;
    }

    NextPartition->PartitionSize    += Bytes;
    CodedFrameBufferUsedSpace       += Bytes;
    CodedFrameBufferFreeSpace       -= Bytes;
    CodedFrameBuffer->SetUsedDataSize(CodedFrameBufferUsedSpace);
}

// ------

void   Collator2_Base_c::AccumulateOnePartition(void)
{
    CollatorStatus_t    Status;
    unsigned char           TerminalStartCode[4];

    //
    // If discarding, preserve flags, but  dump data
    //

    if (DiscardingData)
    {
        EmptyCurrentPartition();
    }

    //
    // Do we have anything to accumulate
    //

    if ((NextPartition->PartitionSize == 0) &&
        !NextPartition->CodedFrameParameters.StreamDiscontinuity &&
        !NextPartition->CodedFrameParameters.ContinuousReverseJump)
    {
        return;
    }

//

    if (CodedFrameBuffer == NULL)
    {
        return;
    }

    //
    // Do we have any terminate code to append in forward play
    // NOTE we preload the terminate code in reverse play
    //

    if ((PlayDirection == PlayForward) &&
        (NextPartition->PartitionSize != 0) &&
        Configuration.InsertFrameTerminateCode)
    {
        TerminalStartCode[0]    = 0x00;
        TerminalStartCode[1]    = 0x00;
        TerminalStartCode[2]    = 0x01;
        TerminalStartCode[3]    = Configuration.TerminalCode;
        Status  = AccumulateData(4, TerminalStartCode);

        if (Status != CollatorNoError)
        {
            SE_ERROR("Failed to add terminal start code\n");
        }
    }

    if (NextPartition->PartitionSize > LargestFrameSeen)
    {
        LargestFrameSeen  = NextPartition->PartitionSize;
    }

    PartitionPointUsedCount++;

    if (PartitionPointUsedCount >= MAXIMUM_PARTITION_POINTS)
    {
        SE_ERROR("PartitionPointUsedCount is out of range (%d, max %d)\n", PartitionPointUsedCount, MAXIMUM_PARTITION_POINTS);
        return;
    }

    NextPartition->NumberOfStartCodes   = NextWriteInStartCodeList - NextPartition->StartCodeListIndex;
    InitializePartition();

    if ((PlayDirection != PlayForward) &&
        (NextPartition->PartitionSize != 0) &&
        Configuration.InsertFrameTerminateCode)
    {
        TerminalStartCode[0]    = 0x00;
        TerminalStartCode[1]    = 0x00;
        TerminalStartCode[2]    = 0x01;
        TerminalStartCode[3]    = Configuration.TerminalCode;
        Status  = AccumulateData(4, TerminalStartCode);

        if (Status != CollatorNoError)
        {
            SE_ERROR("Failed to add terminal start code\n");
        }
    }
}

// ------

void   Collator2_Base_c::InitializePartition(void)
{
    unsigned int    CodedFrameBufferSize;

    SE_PRECONDITION((PartitionPointUsedCount < MAXIMUM_PARTITION_POINTS));
    NextPartition           = &PartitionPoints[PartitionPointUsedCount];

    memset(NextPartition, 0, sizeof(PartitionPoint_t));
    // Set default SourceTimeFormat to TimeFormatPts.
    NextPartition->CodedFrameParameters.SourceTimeFormat = TimeFormatPts;
    NextPartition->StartCodeListIndex   = NextWriteInStartCodeList;

    if (PlayDirection == PlayForward)
    {
        NextPartition->PartitionBase  = CodedFrameBufferBase + CodedFrameBufferUsedSpace;
    }
    else
    {
        CodedFrameBuffer->ObtainDataReference(&CodedFrameBufferSize, NULL, NULL);
        NextPartition->PartitionBase  = CodedFrameBufferBase + CodedFrameBufferSize - CodedFrameBufferUsedSpace;
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This function handles the output of a partition. This
//  is easy when we are in forward play, but depends on the associated
//  header status bits when we are in reverse (IE we stack frames until
//  we hit a confirmed reversal point).
//
//  Note we stack the buffer, with the reversible point flag encoded in
//  bit 0, this assumes that buffer will be aligned to 16 bits at least
//  (32 expected). since this may not be true on future processors, we
//  test this assumption.
//

CollatorStatus_t   Collator2_Base_c::OutputOnePartition(PartitionPoint_t    *Descriptor)
{
    CollatorStatus_t         Status;
    Buffer_t                 Buffer;
    CodedFrameParameters_t  *Parameters;
    unsigned int             BufferAndFlag;

    Descriptor->Buffer->SetUsedDataSize(Descriptor->PartitionSize);

    if (PlayDirection == PlayForward)
    {
        Descriptor->Buffer->ObtainMetaDataReference(Player->MetaDataCodedFrameParametersType, (void **)(&Parameters));
        SE_ASSERT(Parameters != NULL);

        Parameters->CollationTime   = OS_GetTimeInMicroSeconds();
        CheckForGlitchPromotion(Descriptor);
        DelayForInjectionThrottling(&Descriptor->CodedFrameParameters);

        // Output the partition to the frame parser input ring
        Status = InsertInOutputPort(Descriptor->Buffer);
        if (Status != CollatorNoError) { return Status; }
    }
    else
    {
        // Treating reverse play case
        if (((unsigned int)Descriptor->Buffer & 0x1) != 0)
        {
            SE_FATAL("We assume word alignment of buffer structure - Implementation error\n");
        }

        CheckForGlitchPromotion(Descriptor);
        BufferAndFlag   = (unsigned int)Descriptor->Buffer |
                          (((Descriptor->FrameFlags & FrameParserHeaderFlagPossibleReversiblePoint) != 0) ? 1 : 0);
        ReverseFrameStack->Push(BufferAndFlag);

        if ((Descriptor->FrameFlags & FrameParserHeaderFlagConfirmReversiblePoint) != 0)
        {
            // Search for a reversible point.
            // In backward, we output the whole GOP at once.
            while (ReverseFrameStack->NonEmpty())
            {
                ReverseFrameStack->Pop(&BufferAndFlag);

                if ((BufferAndFlag & 1) != 0)
                {
                    break;
                }

                TemporaryHoldingStack->Push(BufferAndFlag);
            }

            if ((BufferAndFlag & 1) == 0)
            {
                SE_ERROR("Failed to find a reversible point\n");
            }
            else
            {
                //
                // Smooth reverse flag
                //
                Buffer  = (Buffer_t)(BufferAndFlag & ~1);
                Buffer->ObtainMetaDataReference(Player->MetaDataCodedFrameParametersType, (void **)(&Parameters));
                SE_ASSERT(Parameters != NULL);

                Parameters->CollationTime       = OS_GetTimeInMicroSeconds();
                Parameters->StreamDiscontinuity     = true;
                Parameters->ContinuousReverseJump   = true;

                // Output the reversible frame to the frame parser input ring
                Status = InsertInOutputPort(Buffer);
                if (Status != CollatorNoError) { return Status; }

                // Output the rest of the GOP to the frame parser input ring
                while (ReverseFrameStack->NonEmpty())
                {
                    ReverseFrameStack->Pop(&BufferAndFlag);
                    Buffer          = (Buffer_t)(BufferAndFlag & ~1);
                    Buffer->ObtainMetaDataReference(Player->MetaDataCodedFrameParametersType, (void **)(&Parameters));
                    SE_ASSERT(Parameters != NULL);

                    Parameters->CollationTime   = OS_GetTimeInMicroSeconds();

                    Status = InsertInOutputPort(Buffer);
                    if (Status != CollatorNoError) { return Status; }
                }
            }

            // Move TemporaryHoldingStack back to ReverseFrameStack
            while (TemporaryHoldingStack->NonEmpty())
            {
                TemporaryHoldingStack->Pop(&BufferAndFlag);
                ReverseFrameStack->Push(BufferAndFlag);
            }
        }
    }

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This function splits the coded data into two partitions,
//  and copied in the appropriate meta data.
//

CollatorStatus_t   Collator2_Base_c::PerformOnePartition(
    PartitionPoint_t    *Descriptor,
    Buffer_t        *NewPartition,
    Buffer_t        *OutputPartition,
    unsigned int         SizeOfFirstPartition)
{
    unsigned int         i;
    BufferStatus_t       Status;
    CodedFrameParameters_t  *CodedFrameParameters;
    StartCodeList_t     *StartCodeList;
    PackedStartCode_t    Code;
    unsigned long long   NewOffset;
    //
    // Do the partition
    //
    Status = CodedFrameBuffer->PartitionBuffer(SizeOfFirstPartition, false, NewPartition);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to partition a buffer\n");
        return Status;
    }

    CodedFrameBuffer->SetUsedDataSize(SizeOfFirstPartition);
    (*NewPartition)->SetUsedDataSize(CodedFrameBufferUsedSpace - SizeOfFirstPartition);
    //
    // Mark down the accumulated data size
    //
    CodedFrameBufferUsedSpace   -= Descriptor->PartitionSize;
    //
    // Copy in the coded frame parameters appropriate to this partition
    //
    Descriptor->Buffer  = *OutputPartition;
    Descriptor->Buffer->ObtainMetaDataReference(Player->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters));
    SE_ASSERT(CodedFrameParameters != NULL);

    memcpy(CodedFrameParameters, &Descriptor->CodedFrameParameters, sizeof(CodedFrameParameters_t));

    //
    // Copy in the start code list
    //

    if (Configuration.GenerateStartCodeList)
    {
        if (NextReadInStartCodeList != Descriptor->StartCodeListIndex)
        {
            SE_FATAL("Start code list in dubious condition (%2d %2d %d) - Implementation error\n", NextReadInStartCodeList, NextWriteInStartCodeList, Descriptor->StartCodeListIndex);
        }

        Descriptor->Buffer->ObtainMetaDataReference(Player->MetaDataStartCodeListType, (void **)(&StartCodeList));
        SE_ASSERT(StartCodeList != NULL);

        if (PlayDirection == PlayForward)
        {
            for (i = 0; i < Descriptor->NumberOfStartCodes; i++)
            {
                StartCodeList->StartCodes[i] = AccumulatedStartCodeList->StartCodes[(NextReadInStartCodeList + i) % MAXIMUM_ACCUMULATED_START_CODES];
            }
        }
        else
        {
            for (i = 0; i < Descriptor->NumberOfStartCodes; i++)
            {
                Code                = AccumulatedStartCodeList->StartCodes[(NextReadInStartCodeList + Descriptor->NumberOfStartCodes - 1 - i) % MAXIMUM_ACCUMULATED_START_CODES];
                NewOffset           = Descriptor->PartitionSize - ExtractStartCodeOffset(Code);
                StartCodeList->StartCodes[i]    = PackStartCode(NewOffset, ExtractStartCodeCode(Code));
            }
        }

        StartCodeList->NumberOfStartCodes    = Descriptor->NumberOfStartCodes;
        NextReadInStartCodeList         += Descriptor->NumberOfStartCodes;
    }

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This is a fairly crucial function, it takes the
//  accumulated data buffer and outputs the individual collated frames.
//  it also manages the acquisition of a new operating buffer when
//  appropriate.
//

CollatorStatus_t   Collator2_Base_c::PartitionOutput(unsigned int MinimumAcceptableSize)
{
    unsigned int      i;
    CollatorStatus_t  Status;
    unsigned int      SucessfullyOutputPartitions;
    unsigned int      Partition;
    unsigned int      TotalBuffers;
    unsigned int      BuffersUsed;
    unsigned int      KnownFreeBuffers;
    unsigned int      FirstPartitionSize;
    Buffer_t          NewBuffer;
    Buffer_t         *RetainedBuffer;
    Buffer_t         *OutputBuffer;
    unsigned int      CodedFrameBufferSize;
    unsigned int      AcceptableBufferSpace;
    unsigned int      MinimumSoughtSize;
    unsigned int      LargestFreeMemoryBlock;
    unsigned int      NewBufferSize;
    unsigned char    *NewBufferBase;
    unsigned char    *TransferFrom;
    unsigned char    *TransferTo;
    //
    // First we perform the actual partitioning
    //
    KnownFreeBuffers        = 0;
    SucessfullyOutputPartitions = 0;
    SE_PRECONDITION((PartitionPointUsedCount < MAXIMUM_PARTITION_POINTS));
    SE_PRECONDITION((PartitionPointSafeToOutputCount < MAXIMUM_PARTITION_POINTS));

    for (Partition = 0; Partition < PartitionPointSafeToOutputCount; Partition++)
    {
        //
        // Can we perform a partition, without blocking when we shouldn't
        //
        if (KnownFreeBuffers == 0)
        {
            //
            // we check if the policy 'ReduceCollatedData' is set then we ask how many 'max' reference frame
            // are required to decode correctly this stream
            // Only if we are in forward mode
            //
            bool ReduceCollatedData = Player->PolicyValue(Playback, Stream, PolicyReduceCollatedData) == PolicyValueApply;
            CodedFrameBufferPool->GetPoolUsage(&TotalBuffers, &BuffersUsed);

            if ((ReduceCollatedData) && (PlayDirection == PlayForward))
            {
                //
                // We need to compute 'N' which is the min number of buffers required for a correct decoding
                // N = max collator buffer used(*1) + the max number of reference frames ; (*1) = 2
                // If the number of bufferUsed is equal to N then we wait a suffisant decode frame period to
                // allow release of buffer after decode and by the way get a bufferUsed < 'N'
                //
                unsigned int MaxRefFrame;
                unsigned int MaxBuffersForReduceCollatedData = 0;
                unsigned long long EntryTime;
                //
                // Need to know how many buffer are used
                //
                MaxRefFrame = Stream->GetCollateTimeFrameParser()->GetMaxReferenceCount();
                //
                // Compute 'N'
                //
                MaxBuffersForReduceCollatedData = MAX_COLLATOR_BUFFER_USED + MaxRefFrame;
                SE_DEBUG(group_collator_video, "MaxBuffersForReduceCollatedData = %d, MaxRefFrame =%d BuffersUsed =%d\n",
                         MaxBuffersForReduceCollatedData, MaxRefFrame, BuffersUsed);

                if (BuffersUsed < MaxBuffersForReduceCollatedData)
                {
                    KnownFreeBuffers =  MaxBuffersForReduceCollatedData - BuffersUsed;
                }
                else
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

                        if (BuffersUsed < MaxBuffersForReduceCollatedData)
                        {
                            KnownFreeBuffers = MaxBuffersForReduceCollatedData - BuffersUsed;
                            SE_DEBUG(group_collator_video, "BuffersUsed=%d\n", BuffersUsed);
                            break;
                        }

                        //
                        // The wait can last until a decoded buffer has been freed specfically when
                        // all decode buffers are queued to manifestor.
                        // We can assume that decoded buffers will be released before playstream delete
                        //
                        if ((OS_GetTimeInMicroSeconds() - EntryTime) > BUFFER_MAX_EXPECTED_WAIT_PERIOD)
                        {
                            SE_DEBUG(group_collator_video, "Waiting for buffer : Reduce collated buffer (%d) in use (%d)", MaxBuffersForReduceCollatedData, BuffersUsed);
                            EntryTime = OS_GetTimeInMicroSeconds();
                        }
                    }
                }
            }
            else
            {
                KnownFreeBuffers    = (TotalBuffers - BuffersUsed);
            }
        }

        if ((KnownFreeBuffers == 0) && NonBlockingInput)
        {
            break;
        }

        //
        // Ok we can perform a partition
        //

        if (PlayDirection == PlayForward)
        {
            RetainedBuffer  = &NewBuffer;
            OutputBuffer    = &CodedFrameBuffer;
            FirstPartitionSize  = PartitionPoints[Partition].PartitionSize;
        }
        else
        {
            RetainedBuffer  = &CodedFrameBuffer;
            OutputBuffer    = &NewBuffer;
            FirstPartitionSize  = (PartitionPoints[Partition].PartitionBase - CodedFrameBufferBase);
        }

        Status = PerformOnePartition(&PartitionPoints[Partition], &NewBuffer, OutputBuffer, FirstPartitionSize);
        if (Status != CollatorNoError)
        {
            break;
        }

        KnownFreeBuffers--;
        Status = OutputOnePartition(&PartitionPoints[Partition]);
        if (Status != CollatorNoError)
        {
            break;
        }

        CodedFrameBuffer         = *RetainedBuffer;
        SucessfullyOutputPartitions++;
    }

    //
    // Now we have finished partitioning, do we need to compact the partitioning list
    // NOTE <= in loop copies the current partition also.
    //

    if (SucessfullyOutputPartitions != 0)
    {
        SE_PRECONDITION((PartitionPointUsedCount >= SucessfullyOutputPartitions));

        for (i = 0; i <= (PartitionPointUsedCount - SucessfullyOutputPartitions); i++)
        {
            PartitionPoints[i]    = PartitionPoints[Partition + i];
        }

        PartitionPointUsedCount     = PartitionPointUsedCount - SucessfullyOutputPartitions;

        if (PartitionPointMarkerCount >= SucessfullyOutputPartitions)
        {
            PartitionPointMarkerCount   = PartitionPointMarkerCount - SucessfullyOutputPartitions;
        } // else: happens as long as flush or discard did not take place; TODO(?) fixme

        PartitionPointSafeToOutputCount = PartitionPointSafeToOutputCount - SucessfullyOutputPartitions;
        NextPartition           = &PartitionPoints[PartitionPointUsedCount];
    }

    //
    // Try to extend the cumulative buffer we are playing with
    // no status checking
    CodedFrameBuffer->ExtendBuffer(NULL, (PlayDirection == PlayForward));

    CodedFrameBuffer->ObtainDataReference(&CodedFrameBufferSize, NULL, (void **)(&CodedFrameBufferBase));
    SE_ASSERT(CodedFrameBufferBase != NULL); // cannot be empty
    CodedFrameBufferFreeSpace = CodedFrameBufferSize - CodedFrameBufferUsedSpace;
    //
    // Has the extension led to a buffer of an acceptable size,
    // if not then can we get a new one, and transfer any data we have to it.
    //
    AcceptableBufferSpace           = ((NextPartition->PartitionSize > LargestFrameSeen) ?
                                       min((NextPartition->PartitionSize + (MaximumCodedFrameSize / 16)), MaximumCodedFrameSize) :
                                       (LargestFrameSeen + MINIMUM_ACCUMULATION_HEADROOM)) - NextPartition->PartitionSize;

    if (AcceptableBufferSpace < MinimumAcceptableSize)
    {
        AcceptableBufferSpace  = MinimumAcceptableSize + (MinimumAcceptableSize / 16);
    }

    MinimumSoughtSize               = CodedFrameBufferUsedSpace + AcceptableBufferSpace;
    ExtendCodedFrameBufferAtEarliestOpportunity = false;

    if (CodedFrameBufferFreeSpace < AcceptableBufferSpace)
    {
        CodedFrameBufferPool->GetPoolUsage(&TotalBuffers, &BuffersUsed, NULL, NULL, NULL, &LargestFreeMemoryBlock);
        KnownFreeBuffers    = (TotalBuffers - BuffersUsed);

        if (!NonBlockingInput || ((KnownFreeBuffers != 0) && (LargestFreeMemoryBlock >= MinimumSoughtSize)))
        {
            //
            // We are going to get a new buffer, minimise our use of the
            // current one (NOTE no shrink, it gives us jip when we are reversing).
            // No point holding onto memory that we aren't using.
            //
            if (CodedFrameBufferUsedSpace == 0)
            {
                CodedFrameBuffer->DecrementReferenceCount(IdentifierCollator);
            }

            //
            // Get a new one
            //
            BufferStatus_t BufferStatus = CodedFrameBufferPool->GetBuffer(&NewBuffer, IdentifierCollator, MinimumSoughtSize, false, true);
            if (BufferStatus != BufferNoError)
            {
                SE_ERROR("Failed to obtain a new operating buffer\n");
                return CollatorError;
            }

            //
            // Move to the new buffer
            //
            NewBuffer->ObtainDataReference(&NewBufferSize, NULL, (void **)(&NewBufferBase));
            SE_ASSERT(NewBufferBase != NULL); // cannot be empty

            if (PlayDirection == PlayForward)
            {
                TransferFrom    = CodedFrameBufferBase;
                TransferTo  = NewBufferBase;
            }
            else
            {
                TransferFrom    = CodedFrameBufferBase + CodedFrameBufferSize - CodedFrameBufferUsedSpace;
                TransferTo  = NewBufferBase + NewBufferSize - CodedFrameBufferUsedSpace;
            }

            if (CodedFrameBufferUsedSpace != 0)
            {
                memcpy(TransferTo, TransferFrom, CodedFrameBufferUsedSpace);
                // Having done the transfer we can now release the old buffer
                CodedFrameBuffer->DecrementReferenceCount(IdentifierCollator);
            }

            //
            // Move any current partition pointers to the new buffer
            //    NOTE it may be possible to have zero length partitions
            //         even if there is no actual data associated with
            //         them (discontinuities etc...)
            //

            for (i = 0; i <= PartitionPointUsedCount; i++)
            {
                PartitionPoints[i].PartitionBase  += TransferTo - TransferFrom;
            }

            //
            // Complete the transfer to the new buffer by updating pointers
            //
            CodedFrameBuffer                = NewBuffer;
            CodedFrameBufferBase            = NewBufferBase;
            CodedFrameBufferFreeSpace           = NewBufferSize - CodedFrameBufferUsedSpace;
        }
        else
        {
            ExtendCodedFrameBufferAtEarliestOpportunity = true;
        }
    }

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Frame Flush function
//

CollatorStatus_t   Collator2_Base_c::FrameFlush(void)
{
    CollatorStatus_t Status;
    bool PreservedNonBlocking;
    bool UsePartitionLockMutex = true;

    if (DiscontinuityControlFound)
    {
        /* Test if the PartitionLock mutex is already locked.
           In case of discontinuity control data, it is already locked by
           ProcessInputForward(), so it should not be locked again to avoid falling in
           deadlock case */
        if (OS_MutexIsLocked(&PartitionLock))
        {
            UsePartitionLockMutex = false;
        }
    }

    SetDrainingStatus(true);

    if (UsePartitionLockMutex)
    {
        OS_LockMutex(&PartitionLock);
    }

    AccumulateOnePartition();

    if (PlayDirection == PlayForward)
    {
        // Setting PartitionPointSafeToOutputCount to 0 leads to none of the frames
        // accumulated to be sent to parser (bug27479). To avoid that PartitionPointSafeToOutputCount is set to PartitionPointUsedCount.
        PartitionPointSafeToOutputCount = PartitionPointUsedCount;
    }
    else
    {
        // If we are flushing, then we should move the marker and safe to output pointers
        PartitionPointSafeToOutputCount = PartitionPointMarkerCount;
    }

    PartitionPointMarkerCount = PartitionPointUsedCount;
    PreservedNonBlocking      = NonBlockingInput;
    NonBlockingInput          = false;
    Status                    = PartitionOutput(0);
    NonBlockingInput          = PreservedNonBlocking;

    if (UsePartitionLockMutex)
    {
        OS_UnLockMutex(&PartitionLock);
    }
    SetDrainingStatus(false);

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The discard all accumulated data for the current partition function
//

CollatorStatus_t   Collator2_Base_c::DiscardAccumulatedData(void)
{
    if (CodedFrameBuffer != NULL)
    {
        OS_AssertMutexHeld(&PartitionLock);

        while (PartitionPointUsedCount > PartitionPointSafeToOutputCount)
        {
            EmptyCurrentPartition();
            PartitionPointUsedCount--;
            SE_PRECONDITION((PartitionPointUsedCount < MAXIMUM_PARTITION_POINTS));
            NextPartition       = &PartitionPoints[PartitionPointUsedCount];
        }

        PartitionPointMarkerCount   = PartitionPointSafeToOutputCount;
        EmptyCurrentPartition();
        InitializePartition();
    }

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Handle a jump
//      This function is also called in case of discontinuity marker detection.
//      If this function is to be modified and some new code needs to be done
//      under PartitionLock mutex protection, be aware that in the case of
//      discontinuity marker, this mutex is already taken by ProcessInputForward()
//      and should not be locked again to avoid falling in deadlock case.
//      You can check the code for that in DiscardAccumulatedData() or FrameFlush().
//

CollatorStatus_t   Collator2_Base_c::InputJump(
    bool                      SurplusDataInjected,
    bool                      ContinuousReverseJump,
    bool                      FromDiscontinuityControl)
{
    bool    UseInputJumpLockMutex = true;
    AssertComponentState(ComponentRunning);

    if (FromDiscontinuityControl)
    {
        /* Test if the InputJumpLock mutex is already locked.
           In case of discontinuity control data, it is already locked by
           ProcessInputForward(), so it should not be locked again to avoid falling in
           deadlock case */
        if (OS_MutexIsLocked(&InputJumpLock))
        {
            UseInputJumpLockMutex = false;
        }
    }

    // If we are running Input(), wait until it finish, then resume Drain process
    if ((OS_MutexIsLocked(&InputJumpLock)) && (!FromDiscontinuityControl))
    {
        SE_INFO(group_collator_video, "InputJumpLock locked: Input process is running! Awaiting unlock..\n");
    }

    if (UseInputJumpLockMutex)
    {
        OS_LockMutex(&InputJumpLock);
    }

    // set the call menber variable (DiscontinuityControlFound) for following called function
    DiscontinuityControlFound = FromDiscontinuityControl;

    if (SurplusDataInjected)
    {
        DiscardAccumulatedData();
    }
    else
    {
        FrameFlush();
    }

    Stream->GetCollateTimeFrameParser()->ResetCollatedHeaderState();
    NextPartition->CodedFrameParameters.StreamDiscontinuity           = true;
    NextPartition->CodedFrameParameters.FlushBeforeDiscontinuity      = SurplusDataInjected;
    NextPartition->CodedFrameParameters.ContinuousReverseJump         = ContinuousReverseJump;
    FrameFlush();
    /* Reset any pending notification of chunk ID in case of discontinuity */
    AlarmParsedPtsSet = false;
    // Reset DiscontinuityControlFound to default value before releasing the lock
    DiscontinuityControlFound = false;

    // Unlock to resume Input() function
    if (UseInputJumpLockMutex)
    {
        OS_UnLockMutex(&InputJumpLock);
    }

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Handle a glitch
//

CollatorStatus_t   Collator2_Base_c::NonBlockingWriteSpace(unsigned int      *Size)
{
    AssertComponentState(ComponentRunning);
    *Size   = (CodedFrameBufferFreeSpace > MINIMUM_ACCUMULATION_HEADROOM) ? 0 : (CodedFrameBufferFreeSpace - MINIMUM_ACCUMULATION_HEADROOM);
    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - The accumulate data into the buffer function
//

CollatorStatus_t   Collator2_Base_c::AccumulateData(
    unsigned int              Length,
    unsigned char            *Data)
{
    CollatorStatus_t    Status;

    if ((NextPartition->PartitionSize + Length) > MaximumCodedFrameSize)
    {
        SE_ERROR("Buffer overflow. (%d > %d)\n", (NextPartition->PartitionSize + Length) , MaximumCodedFrameSize);
        EmptyCurrentPartition();      // Dump any collected data in the current partition
        InitializePartition();
        return CollatorBufferOverflow;
    }

    if (CodedFrameBufferFreeSpace < Length)
    {
        Status  = PartitionOutput(Length);

        if (Status != CollatorNoError)
        {
            SE_ERROR("Output of partitions failed\n");
            return Status;
        }

        if (CodedFrameBufferFreeSpace < Length)
        {
            return CollatorWouldBlock;
        }
    }

    memcpy(NextPartition->PartitionBase + ((PlayDirection == PlayForward) ? NextPartition->PartitionSize : -Length), Data, Length);
    MoveCurrentPartitionBoundary(Length);

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - The accumulate data into the buffer function
//

CollatorStatus_t   Collator2_Base_c::AccumulateStartCode(PackedStartCode_t  Code)
{
    if (!Configuration.GenerateStartCodeList)
    {
        return CollatorNoError;
    }

    if ((NextWriteInStartCodeList - NextReadInStartCodeList) >= MAXIMUM_ACCUMULATED_START_CODES)
    {
        SE_ERROR("Start code list overflow\n");
        return CollatorBufferOverflow;
    }

    AccumulatedStartCodeList->StartCodes[(NextWriteInStartCodeList++) % MAXIMUM_ACCUMULATED_START_CODES] = Code;
    return CollatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Private - Check if we should promote a glitch to a full blown
//        discontinuity. This works by checking pts flow around
//        the glitch.
//

void   Collator2_Base_c::CheckForGlitchPromotion(PartitionPoint_t    *Descriptor)
{
    long long        DeltaPTS;
    long long        Range;

    if (Descriptor->CodedFrameParameters.PlaybackTimeValid)
    {
        //
        // Handle any glitch promotion
        //     We promote if there is a glitch, and if the
        //     PTS varies by more than the maximum of 1/4 second, and 40ms times the number of frames that have passed since a pts.
        //
        if (Descriptor->Glitch &&
            ValidTime(LastFramePreGlitchPTS))
        {
            DeltaPTS        = Descriptor->CodedFrameParameters.PlaybackTime - LastFramePreGlitchPTS;
            Range       = max(22500, (3600 * FrameSinceLastPTS));

            if (!inrange(DeltaPTS, -Range, Range))
            {
                SE_INFO(group_collator_video, "(%d) Promoted\n", Configuration.GenerateStartCodeList);
                Descriptor->CodedFrameParameters.StreamDiscontinuity           = true;
                Descriptor->CodedFrameParameters.FlushBeforeDiscontinuity      = false;
                Descriptor->CodedFrameParameters.ContinuousReverseJump         = false;
            }
        }

        //
        // Remember the frame pts
        //
        Descriptor->Glitch  = false;
        LastFramePreGlitchPTS   = Descriptor->CodedFrameParameters.PlaybackTime;
        FrameSinceLastPTS   = 0;
    }

    FrameSinceLastPTS++;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Check the difference between an injected PTS and
//          the current PCR
//

void   Collator2_Base_c::MonitorPCRTiming(unsigned long long      PTS)
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
            SE_INFO(group_collator_video, "(%s) - Upstream latency is %5lldms (%5lld)\n", Configuration.CollatorName, UpstreamLatency / 1000, StreamDelay / 1000);
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
            SE_INFO(group_collator_video, "(%s) - Downstream latency is %5lldms, Total Latency is %5lldms (%5lld)\n", Configuration.CollatorName, DownstreamLatency / 1000,
                    (UpstreamLatency + DownstreamLatency) / 1000, StreamDelay / 1000);
        }
    }
}
