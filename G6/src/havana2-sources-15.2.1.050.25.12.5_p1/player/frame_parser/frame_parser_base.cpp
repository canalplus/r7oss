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

// /////////////////////////////////////////////////////////////////////////
///
/// \class FrameParser_Base_c
/// \brief Framework to unify the approach to frame parsing.
///
/// This base class provided a framework on which to construct a frame parser.
/// Deriving from this class is, strictly speaking, optional, but nevertheless
/// is highly recommended. This most important framework provided by this class
/// is a sophisticated helper function, FrameParser_Base_c::ProcessBuffer, that
/// splits the complex process of managing incoming data (especially during
/// trick modes) into smaller more easily implemented pieces.

#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "frame_parser_base.h"
#include "parse_to_decode_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_Base_c"

#define DEFAULT_DECODE_BUFFER_COUNT     8

//
FrameParser_Base_c::FrameParser_Base_c(void)
    : Lock()
    , Configuration()
    , CodedFrameBufferPool(NULL)
    , FrameBufferCount(0)
    , DecodeBufferPool(NULL)
    , DecodeBufferCount(0)
    , mOutputPort(NULL)
    , BufferManager(NULL)
    , StreamParametersDescriptor(NULL)
    , StreamParametersType(NOT_SPECIFIED)
    , StreamParametersPool(NULL)
    , StreamParametersBuffer(NULL)
    , FrameParametersDescriptor(NULL)
    , FrameParametersType(NOT_SPECIFIED)
    , FrameParametersPool(NULL)
    , FrameParametersBuffer(NULL)
    , Buffer(NULL)
    , BufferLength(0)
    , BufferData(NULL)
    , CodedFrameParameters(NULL)
    , ParsedFrameParameters(NULL)
    , StartCodeList(NULL)
    , FirstDecodeAfterInputJump(true)
    , SurplusDataInjected(false)
    , ContinuousReverseJump(false)
    , NextDecodeFrameIndex(0)
    , NextDisplayFrameIndex(0)
    , NativeTimeBaseLine(INVALID_TIME)
    , LastNativeTimeUsedInBaseline(INVALID_TIME)
    , Bits()
    , FrameToDecode(false)
    , PlaybackSpeed(1)
    , PlaybackDirection(PlayForward)
    , AccumulatedUserData()
    , UserDataIndex(0)
{
    // Fill out default values for the configuration record
    Configuration.FrameParserName               = "Unspecified";
    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = NULL;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = NULL;
    Configuration.MaxReferenceFrameCount        = 32; // Only need limiting for specific codecs (IE h264)
    Configuration.SupportSmoothReversePlay      = false;
    Configuration.InitializeStartCodeList       = false;

    OS_InitializeMutex(&Lock);
}

//
FrameParser_Base_c::~FrameParser_Base_c(void)
{
    if (StreamParametersPool != NULL)
    {
        if (StreamParametersBuffer != NULL)
        {
            StreamParametersBuffer->DecrementReferenceCount(IdentifierFrameParser);
        }
        BufferManager->DestroyPool(StreamParametersPool);
    }

    if (FrameParametersPool != NULL)
    {
        if (FrameParametersBuffer != NULL)
        {
            FrameParametersBuffer->DecrementReferenceCount(IdentifierFrameParser);
        }
        BufferManager->DestroyPool(FrameParametersPool);
    }

    OS_TerminateMutex(&Lock);
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

FrameParserStatus_t   FrameParser_Base_c::Halt(void)
{
    PurgeQueuedPostDecodeParameterSettings();
    return BaseComponentClass_c::Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Method to connect to neighbor
//

FrameParserStatus_t   FrameParser_Base_c::Connect(Port_c *Port)
{
    FrameParserStatus_t Status;

    if (Port == NULL)
    {
        SE_ERROR("Incorrect input param\n");
        return FrameParserError;
    }
    if (mOutputPort != NULL)
    {
        SE_WARNING("Port already connected\n");
    }
    mOutputPort = Port;

    //
    // Obtain the class list, and the coded frame buffer pool
    //
    CodedFrameBufferPool = Stream->GetCodedFrameBufferPool();
    Player->GetDecodeBufferPool(Stream, &DecodeBufferPool);
    //
    // Obtain Buffer counts
    //
    CodedFrameBufferPool->GetPoolUsage(&FrameBufferCount, NULL, NULL, NULL, NULL);

    if (DecodeBufferPool != NULL)
    {
        Stream->GetDecodeBufferManager()->GetEstimatedBufferCount(&DecodeBufferCount);
    }
    else
    {
        DecodeBufferCount       = DEFAULT_DECODE_BUFFER_COUNT;
    }

    //
    // Attach the generic parsed frame parameters to every element of the pool.
    //
    BufferStatus_t BufStatus = CodedFrameBufferPool->AttachMetaData(Player->MetaDataParsedFrameParametersType);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to attach parsed frame parameters to all coded frame buffers (status: %d)\n", BufStatus);
        Status = FrameParserError;
        goto failedAttach;
    }

    //
    // Now create the frame and stream parameter buffers
    //
    Status = RegisterStreamAndFrameDescriptors();
    if (Status != FrameParserNoError)
    {
        SE_ERROR("Failed to register the parameter buffer types\n");
        goto bail;
    }

    if (StreamParametersPool == NULL)
    {
        BufStatus = BufferManager->CreatePool(&StreamParametersPool, StreamParametersType, Configuration.StreamParametersCount);

        if (BufStatus != BufferNoError)
        {
            SE_ERROR("Failed to create a pool of stream parameter buffers (status: %d)\n", BufStatus);
            Status = FrameParserError;
            goto bail;
        }
    }

    if (FrameParametersPool == NULL)
    {
        BufStatus = BufferManager->CreatePool(&FrameParametersPool, FrameParametersType, Configuration.FrameParametersCount);

        if (BufStatus != BufferNoError)
        {
            SE_ERROR("Failed to create a pool of frame parameter buffers (status: %d)\n", BufStatus);
            Status = FrameParserError;
            goto FailedCreateFPPool;
        }
    }

    StreamParametersBuffer      = NULL;
    FrameParametersBuffer       = NULL;
    //
    // Go live
    //
    //    SE_ERROR( "Setting component state to running\n");
    SetComponentState(ComponentRunning);
    return FrameParserNoError;
FailedCreateFPPool:
    BufferManager->DestroyPool(StreamParametersPool);
    StreamParametersPool = NULL;
bail:
    CodedFrameBufferPool->DetachMetaData(Player->MetaDataParsedFrameParametersType);
failedAttach:
    StreamParametersBuffer      = NULL;
    FrameParametersBuffer       = NULL;
    SetComponentState(ComponentInError);
    return Status;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The input function perform base operations
//

FrameParserStatus_t   FrameParser_Base_c::Input(Buffer_t CodedBuffer)
{
    //
    // Initialize context pointers
    //
    Buffer                  = NULL;
    BufferLength            = 0;
    BufferData              = NULL;
    CodedFrameParameters    = NULL;
    ParsedFrameParameters   = NULL;
    StartCodeList           = NULL;
    //
    // Obtain pointers to data associated with the buffer.
    //
    Buffer = CodedBuffer;
    BufferStatus_t BufStatus = Buffer->ObtainDataReference(NULL, &BufferLength, (void **)(&BufferData));
    if ((BufStatus != BufferNoError) && (BufStatus != BufferNoDataAttached))
    {
        SE_ERROR("Unable to obtain data reference (status:%d)\n", BufStatus);
        return FrameParserError;
    }

    Buffer->ObtainMetaDataReference(Player->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters));
    SE_ASSERT(CodedFrameParameters != NULL);

    Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    if (Configuration.InitializeStartCodeList)
    {
        Buffer->ObtainMetaDataReference(Player->MetaDataStartCodeListType, (void **)(&StartCodeList));
        SE_ASSERT(StartCodeList != NULL);
    }

    memset(ParsedFrameParameters, 0, sizeof(ParsedFrameParameters_t));
    ParsedFrameParameters->DecodeFrameIndex         = INVALID_INDEX;
    ParsedFrameParameters->DisplayFrameIndex        = INVALID_INDEX;
    ParsedFrameParameters->NativePlaybackTime       = INVALID_TIME;
    ParsedFrameParameters->NormalizedPlaybackTime   = INVALID_TIME;
    ParsedFrameParameters->NativeDecodeTime         = INVALID_TIME;
    ParsedFrameParameters->NormalizedDecodeTime     = INVALID_TIME;
    ParsedFrameParameters->NativeTimeFormat         = CodedFrameParameters->SourceTimeFormat;
    ParsedFrameParameters->SpecifiedPlaybackTime    = CodedFrameParameters->PlaybackTimeValid;
    ParsedFrameParameters->CollationTime            = CodedFrameParameters->CollationTime;
    ParsedFrameParameters->IndependentFrame         = true;         // Default a frame to being independent
    // to allow video decoders to mark single
    // I fields as non-independent.
    ParsedFrameParameters->StillPicture             = false;
    //
    // Gather any useful information about the policies in force
    //
    Player->GetPlaybackSpeed(Playback, &PlaybackSpeed, &PlaybackDirection);

    if (BufferLength != 0)
    {
        Stream->Statistics().BufferCountToFrameParser++;
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Translate times from a 90Khz native time to a microsecond
//      normalized time. We will move the implementation of this
//      if we see any codecs using non-90Khz times.
//
//      In my arithmetic I pop briefly into a 27Mhz timebase, there
//      is no real reason for this, it's just that historically our
//      chips extended the 90Khz to a 27Mhz wossname.
//

FrameParserStatus_t   FrameParser_Base_c::TranslatePlaybackTimeNativeToNormalized(
    unsigned long long        NativeTime,
    unsigned long long       *NormalizedTime,
    PlayerTimeFormat_t        NativeTimeFormat)
{
    if (NotValidTime(NativeTime))
    {
        *NormalizedTime = NativeTime;
        return FrameParserError;
    }

    if (TimeFormatUs == NativeTimeFormat)
    {
        // Nothing to do. Native time is already in microsecond.
        *NormalizedTime = NativeTime;
        return FrameParserNoError;
    }
    unsigned long long  WrapMask;
    unsigned long long  WrapOffset;
    //
    // There is a fudge here, native time is actually only 33 bits long,
    // whereas normalized time is 64 bits long. We maintain a normalized
    // baseline that handles the affects of wrapping, which we use to
    // adjust the calculated value.
    //
    //

    if (NotValidTime(LastNativeTimeUsedInBaseline))
    {
        NativeTimeBaseLine              = 0;
        LastNativeTimeUsedInBaseline    = NativeTime;
    }

    //
    // Adjust the baseline if there is any wrap.
    // First check for forward wrap, then reverse wrap.
    //
    WrapMask    = 0x0000000180000000ULL;
    WrapOffset  = 0x0000000200000000ULL;

    if (((LastNativeTimeUsedInBaseline & WrapMask) == WrapMask) &&
        ((NativeTime                   & WrapMask) == 0))
    {
        NativeTimeBaseLine      += WrapOffset;
    }
    else if (((LastNativeTimeUsedInBaseline & WrapMask) == 0) &&
             ((NativeTime                   & WrapMask) == WrapMask))
    {
        NativeTimeBaseLine      -= WrapOffset;
    }

    LastNativeTimeUsedInBaseline        = NativeTime;
    //
    // Calculate the Normalized time
    //
    *NormalizedTime     = (((NativeTimeBaseLine + NativeTime) * 300) + 13) / 27;
    Player->SetLastNativeTime(Playback, (NativeTimeBaseLine + NativeTime));
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Translate times from a 1 microsecond normalized time to a 90Khz
//      native time, assumes that the native time is only 33 bits.
//
//      In my arithmetic I pop briefly into a 27Mhz timebase, there
//      is no real reason for this, it's just that historically our
//      chips extended the 90Khz to a 27Mhz wossname.
//

FrameParserStatus_t   FrameParser_Base_c::TranslatePlaybackTimeNormalizedToNative(
    unsigned long long        NormalizedTime,
    unsigned long long       *NativeTime,
    PlayerTimeFormat_t        NativeTimeFormat)
{
    if (TimeFormatUs == NativeTimeFormat)
    {
        // Nothing to do. Native time is already in microsecond.
        *NativeTime = NormalizedTime;
        return FrameParserNoError;
    }
    *NativeTime = (((NormalizedTime * 27) + 150) / 300) & 0x00000001ffffffffULL;
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Increment statistic counters for different frame parsing errors
//

void FrameParser_Base_c::IncrementErrorStatistics(FrameParserStatus_t  Status)
{
    switch (Status)
    {
    case FrameParserError:
        Stream->Statistics().FrameParserError++;
        break;

    case FrameParserNoStreamParameters:
        Stream->Statistics().FrameParserNoStreamParametersError++;
        break;

    case FrameParserPartialFrameParameters:
        Stream->Statistics().FrameParserPartialFrameParametersError++;
        break;

    case FrameParserUnhandledHeader:
        Stream->Statistics().FrameParserUnhandledHeaderError++;
        break;

    case FrameParserHeaderSyntaxError:
        Stream->Statistics().FrameParserHeaderSyntaxError++;
        break;

    case FrameParserHeaderUnplayable:
        Stream->Statistics().FrameParserHeaderUnplayableError++;
        break;

    case FrameParserStreamSyntaxError:
        Stream->Statistics().FrameParserStreamSyntaxError++;
        break;

    case FrameParserFailedToCreateReversePlayStacks:
        Stream->Statistics().FrameParserFailedToCreateReversePlayStacksError++;
        break;

    case FrameParserFailedToAllocateBuffer:
        Stream->Statistics().FrameParserFailedToAllocateBufferError++;
        break;

    case FrameParserReferenceListConstructionDeferred:
        Stream->Statistics().FrameParserReferenceListConstructionDeferredError++;
        break;

    case FrameParserInsufficientReferenceFrames:
        Stream->Statistics().FrameParserInsufficientReferenceFramesError++;
        break;

    case FrameParserStreamUnplayable:
        Stream->Statistics().FrameParserStreamUnplayableError++;
        break;

    default:
        Stream->Statistics().FrameParserError++;
        SE_INFO(GetGroupTrace(), "unknown error %d; error count:%d\n", Status,
                Stream->Statistics().FrameParserError);
        break;
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Get/set frame indices, used on a stream switch,
//  to ensure monotonically increasing values for both.
//

FrameParserStatus_t   FrameParser_Base_c::GetNextDecodeFrameIndex(unsigned int   *Index)
{
    *Index  = NextDecodeFrameIndex;
    return FrameParserNoError;
}


FrameParserStatus_t   FrameParser_Base_c::SetNextFrameIndices(unsigned int    Value)
{
    NextDecodeFrameIndex    = Value;
    NextDisplayFrameIndex   = Value;
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
///
///      \brief Standard processing chain for single input buffer
///
///      Process buffer forms the heart of the framework to assist with implementing a
///      frame parser. This is the method that will call the protected virtual methods
///      of this class.
///
///      \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t   FrameParser_Base_c::ProcessBuffer(void)
{
    //
    // Handle discontinuity in input stream
    //

    if (CodedFrameParameters->StreamDiscontinuity)
    {
        SE_VERBOSE2(group_frameparser_video, group_player, "Stream 0x%p - Inserting discontinuity at parser level\n", Stream);
        PurgeQueuedPostDecodeParameterSettings();
        FirstDecodeAfterInputJump       = true;
        SurplusDataInjected             = CodedFrameParameters->FlushBeforeDiscontinuity;
        ContinuousReverseJump           = CodedFrameParameters->ContinuousReverseJump;
        Stream->GetParseToDecodeEdge()->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnOutputPartialDecodeBuffers);
    }

    //
    // Do we have something to do with this buffer
    //

    if (BufferLength == 0)
    {
        //
        // Check for a marker frame, identified by no flags and no data
        //
        if (!CodedFrameParameters->StreamDiscontinuity)
        {
            Buffer->IncrementReferenceCount(IdentifierProcessParseToDecode);
            mOutputPort->Insert((uintptr_t)Buffer);
            Stream->Statistics().FrameCountFromFrameParser++;
        }

        return FrameParserNoError;
    }

    //
    // Parse the headers
    //
    FrameToDecode = false;
    FrameParserStatus_t Status = ReadHeaders();
    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // Do we still have something to do
    //

    if (!FrameToDecode)
    {
        return FrameParserNoError;
    }

    //
    // Can we generate any queued index and pts values based on what we have seen
    //
    Status = ProcessQueuedPostDecodeParameterSettings();
    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // Calculate the display index/PTS values
    //
    Status = GeneratePostDecodeParameterSettings();
    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // Queue the frame for decode
    //
    Status = QueueFrameForDecode();
    if (Status != FrameParserNoError)
    {
        return Status;
    }

    FirstDecodeAfterInputJump           = false;
    SurplusDataInjected                 = false;
    ContinuousReverseJump               = false;

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
///
///      \brief Queue a frame for decode.
///
///      Take the currently active buffer, FrameParser_Base_c::Buffer and queue it for
///      decode.
///
///      This implementation provided by the FrameParser_Base_c supports simple forward
///      play by taking the active buffer and placing it directly on the output ring,
///      FrameParser_Base_c::mOutputPort
///
///      During reverse play this method is much more complex. Typically buffers must be
///      dispatched to the output ring in a different order during reverse playback. This means
///      that the implementation must squirrel the buffers away (and record their original order)
///      until FrameParser_Base_c::ProcessReverseDecodeStack (and
///      FrameParser_Base_c::ProcessReverseDecodeUnsatisfiedReferenceStack) are called.
///
///      \return Frame parser status code, FrameParserNoError indicates success.
///

FrameParserStatus_t   FrameParser_Base_c::QueueFrameForDecode(void)
{
    unsigned int    i;

    // Fill User data buffer before queuing to decode
    if (ParsedFrameParameters->UserDataNumber > 0)
    {
        FrameParserStatus_t Status = FillUserDataBuffer();
        if (Status != FrameParserNoError)
        {
            SE_ERROR("FillUserDataBuffer() error\n");
            return Status;
        }
    }

    //
    // Adjust the independent frame flag from it's default value,
    // or the value set in the specific frame parser.
    //

    if (ParsedFrameParameters->IndependentFrame)
    {
        for (i = 0; i < ParsedFrameParameters->NumberOfReferenceFrameLists; i++)
            if (ParsedFrameParameters->ReferenceFrameList[i].EntryCount != 0)
            {
                ParsedFrameParameters->IndependentFrame = false;
                break;
            }
    }

    //
    // Base implementation derives the decode index,
    // then queues the frame on the output ring.
    //
    ParsedFrameParameters->DecodeFrameIndex     = NextDecodeFrameIndex++;
#if 0
    SE_DEBUG(GetGroupTrace(), "Q %d (F = %d, K = %d, I = %d, R = %d)\n",
             ParsedFrameParameters->DecodeFrameIndex,
             ParsedFrameParameters->FirstParsedParametersForOutputFrame,
             ParsedFrameParameters->KeyFrame,
             ParsedFrameParameters->IndependentFrame,
             ParsedFrameParameters->ReferenceFrame);
#endif

    Buffer->IncrementReferenceCount(IdentifierProcessParseToDecode);
    mOutputPort->Insert((uintptr_t)Buffer);
    Stream->Statistics().FrameCountFromFrameParser++;

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
///
///     \brief Internal function to register the stream/frame parameter
///            meta data descriptors with the buffer manager.
///
///     Metadata generated by the frame parser and attached to buffers can, itself,
///     managed by the buffer manager to ensure that its lifetime is properly managed.
///     This method is used to register the metadata types used by a specific
///     frame parser.
///
///     \return Frame parser status code, FrameParserNoError indicates success.

FrameParserStatus_t   FrameParser_Base_c::RegisterStreamAndFrameDescriptors(void)
{
    Player->GetBufferManager(&BufferManager);

    BufferStatus_t Status = BufferManager->FindBufferDataType(Configuration.StreamParametersDescriptor->TypeName, &StreamParametersType);
    if (Status != BufferNoError)
    {
        Status  = BufferManager->CreateBufferDataType(Configuration.StreamParametersDescriptor, &StreamParametersType);

        if (Status != BufferNoError)
        {
            SE_ERROR("Failed to create the stream parameters buffer type\n");
            return FrameParserError;
        }
    }

    BufferManager->GetDescriptor(StreamParametersType, BufferDataTypeBase, &StreamParametersDescriptor);

    Status      = BufferManager->FindBufferDataType(Configuration.FrameParametersDescriptor->TypeName, &FrameParametersType);
    if (Status != BufferNoError)
    {
        Status  = BufferManager->CreateBufferDataType(Configuration.FrameParametersDescriptor, &FrameParametersType);
        if (Status != BufferNoError)
        {
            SE_ERROR("Failed to create the Frame parameters buffer type\n");
            return FrameParserError;
        }
    }

    BufferManager->GetDescriptor(FrameParametersType, BufferDataTypeBase, &FrameParametersDescriptor);

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
///
///     \brief Get a new stream parameters buffer.
///
///     Allocate, initialize with zeros and return a pointer to a block of data in which to store the stream
///     parameters. As a side effect the existing buffer, referenced via
///     FrameParser_Base_c::StreamParametersBuffer,
///     will have its reference count decreased, potentially causing it to be freed. It is the responsibility
///     of derived classes to make a claim on the existing buffer if they must refer to it later (or pass
///     it to another component that must refer to it later.
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t   FrameParser_Base_c::GetNewStreamParameters(void  **Pointer)
{
    if (StreamParametersBuffer != NULL)
    {
        StreamParametersBuffer->DecrementReferenceCount(IdentifierFrameParser);
        StreamParametersBuffer  = NULL;
    }

    BufferStatus_t Status = StreamParametersPool->GetBuffer(&StreamParametersBuffer, IdentifierFrameParser);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to allocate a buffer for the stream parameters\n");
        return FrameParserFailedToAllocateBuffer;
    }

    StreamParametersBuffer->ObtainDataReference(NULL, NULL, Pointer);
    if (Pointer == NULL)
    {
        StreamParametersBuffer->DecrementReferenceCount(IdentifierFrameParser);
        StreamParametersBuffer = NULL;
        return FrameParserError;
    }

    memset(*Pointer, 0, StreamParametersDescriptor->FixedSize);
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
///
///     \brief Get a new frame parameters buffer.
///
///     Allocate, initialise with zeros and return a pointer to a block of data in which to store the
///     frame parameters.
///     As a side effect the existing buffer, referenced via FrameParser_Base_c::FrameParametersBuffer,
///     will have its reference count decreased, potentially causing it to be freed. It is the responsibility
///     of derived classes to make a claim on the existing buffer if they must refer to it later (or pass
///     it to another component that must refer to it later.
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t   FrameParser_Base_c::GetNewFrameParameters(void   **Pointer)
{
    if (FrameParametersBuffer != NULL)
    {
        FrameParametersBuffer->DecrementReferenceCount(IdentifierFrameParser);
        FrameParametersBuffer   = NULL;
    }

    BufferStatus_t Status = FrameParametersPool->GetBuffer(&FrameParametersBuffer, IdentifierFrameParser);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to allocate a buffer to hold frame parameters\n");
        return FrameParserFailedToAllocateBuffer;
    }

    FrameParametersBuffer->ObtainDataReference(NULL, NULL, Pointer);
    if (Pointer == NULL)
    {
        FrameParametersBuffer->DecrementReferenceCount(IdentifierFrameParser);
        FrameParametersBuffer = NULL;
        return FrameParserError;
    }

    memset(*Pointer, 0, FrameParametersDescriptor->FixedSize);
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//     \brief Function to read user data buffer when a user data start code is detected.
//
//      We can read more than one user data start codes before parsing the coded frame so
//      This function accumulate parsed user datas parameters and the payload corresponding
//      to the user data start code in one element of a structured table before parsing
//      the coded frame.
//      Interpolated PTS will not processed here.
//
//

FrameParserStatus_t   FrameParser_Base_c::ReadUserData(unsigned int UserDataLength, unsigned char *InputBufferData)
{
    // Initialise user data table AccumulatedUserData when first user data start code detected
    if (UserDataIndex == 0)
    {
        // Initialise AccumulatedUserData buffer
        memset(AccumulatedUserData, 0, SizeofUserData(MAXIMUM_USER_DATA_BUFFERS));
    }

    if (UserDataIndex >= Configuration.MaxUserDataBlocks)
    {
        SE_ERROR("Maximum user data blocks exeeded\n");
        return FrameParserError;
    }

    if (UserDataLength > MAX_USER_DATA_SIZE)
    {
        SE_ERROR("length (%d bytes) exceeds allocated size. Copy will be clipped to %d bytes\n", UserDataLength, MAX_USER_DATA_SIZE);
        UserDataLength = MAX_USER_DATA_SIZE;
    }

    // AccumulatedUserData used to store all user data & payload
    // Then to be copied to final user data buffer
    // Reserved for debug
    memcpy((unsigned char *)AccumulatedUserData[UserDataIndex].UserDataGenericParameters.Reserved, "STUD", 4);
    // Padding bytes
    AccumulatedUserData[UserDataIndex].UserDataGenericParameters.PaddingBytes = 8 - (UserDataLength % 8);
    // block_length: if additional parameters exists, additional length will be added to this block length.
    // Padding bytes added to be aligned on 64 bits.
    AccumulatedUserData[UserDataIndex].UserDataGenericParameters.BlockLength = sizeof(UserDataGenericParameters_t) + UserDataLength;
    AccumulatedUserData[UserDataIndex].UserDataGenericParameters.BlockLength += AccumulatedUserData[UserDataIndex].UserDataGenericParameters.PaddingBytes;
    // header_length : if additional parameters exists, additional length will be added to header length.
    AccumulatedUserData[UserDataIndex].UserDataGenericParameters.HeaderLength = sizeof(UserDataGenericParameters_t);
    // flags to be updated later
    AccumulatedUserData[UserDataIndex].UserDataGenericParameters.StreamAbridgement = 0;
    AccumulatedUserData[UserDataIndex].UserDataGenericParameters.Overflow = 0;
    // Interpolated PTS to be filled later
    AccumulatedUserData[UserDataIndex].UserDataGenericParameters.IsThereAPts = 0;
    AccumulatedUserData[UserDataIndex].UserDataGenericParameters.IsPtsInterpolated = 0;
    AccumulatedUserData[UserDataIndex].UserDataGenericParameters.Pts_Msb = 0;
    AccumulatedUserData[UserDataIndex].UserDataGenericParameters.Pts = 0;

    // Is there a PTS ?
    if (CodedFrameParameters->PlaybackTimeValid)
    {
        // PTS found
        AccumulatedUserData[UserDataIndex].UserDataGenericParameters.Pts = (uint32_t) CodedFrameParameters->PlaybackTime;
        // Just one bit to be stored in pts_msb
        AccumulatedUserData[UserDataIndex].UserDataGenericParameters.Pts_Msb = CodedFrameParameters->PlaybackTime >> 32;
#if 0 //debug
        SE_DEBUG(GetGroupTrace(),  "***** PlaybackTime = %d *****\n", CodedFrameParameters->PlaybackTime);
#endif
        AccumulatedUserData[UserDataIndex].UserDataGenericParameters.IsThereAPts = 1;  // is_there_a_pts flag is set to 1
    }

    // Is there additional parameters
    if (ReadAdditionalUserDataParameters() == true)
    {
        // Update lengths
        AccumulatedUserData[UserDataIndex].UserDataGenericParameters.BlockLength +=
            AccumulatedUserData[UserDataIndex].UserDataAdditionalParameters.Length;
        AccumulatedUserData[UserDataIndex].UserDataGenericParameters.HeaderLength +=
            AccumulatedUserData[UserDataIndex].UserDataAdditionalParameters.Length;
    }

    // Accumulate user data payload
    memcpy((unsigned char *)AccumulatedUserData[UserDataIndex].UserDataPayload, InputBufferData, UserDataLength);
#if 0 //debug
    SE_DEBUG(GetGroupTrace(),  "++++++Input - block_length=%d, header_length=%d, UserDataIndex = %d, 0x%x 0x%x 0x%x\n",
             AccumulatedUserData[UserDataIndex].UserDataGenericParameters.BlockLength,
             AccumulatedUserData[UserDataIndex].UserDataGenericParameters.HeaderLength,
             UserDataIndex,
             *((unsigned char *)AccumulatedUserData[UserDataIndex].UserDataPayload),
             *((unsigned char *)AccumulatedUserData[UserDataIndex].UserDataPayload + 1),
             *((unsigned char *)AccumulatedUserData[UserDataIndex].UserDataPayload + 2));
#endif
    // Increment index for next user data start code
    UserDataIndex++;
    // Update the number of user data
    ParsedFrameParameters->UserDataNumber = UserDataIndex ;
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to Get & fill final user data buffer.
//
//      User data parameters and payload accummulated in the table will be
//      copied in the final formatted user data buffer.
//      For a coded frame, we can have Configuration.MaxUserDataBlocks user data blocks attached to it.
//

FrameParserStatus_t FrameParser_Base_c::FillUserDataBuffer(void)
{
    unsigned char  *UserDataBuffer;
    unsigned int   UserDataLength = 0;
    unsigned int   DataCounter, PayloadLength;
#if 0 //debug
    SE_DEBUG(GetGroupTrace(),  "++++++Input - %d user data filled\n",
             ParsedFrameParameters->UserDataNumber);
#endif

    // Calculate final user data length
    for (unsigned int Count = 0; Count < ParsedFrameParameters->UserDataNumber; Count++)
    {
        UserDataLength += AccumulatedUserData[Count].UserDataGenericParameters.BlockLength;
    }

    // Attach user data to the coded buffer
    BufferStatus_t Status = Buffer->AttachMetaData(Player->MetaDataUserDataType, UserDataLength);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to attach the user data to all coded frame buffers (status: %d)\n", Status);
        return FrameParserError;
    }

    // Get user data buffer (attached to coded buffer) to be filled
    Buffer->ObtainMetaDataReference(Player->MetaDataUserDataType, (void **)(&UserDataBuffer));
    SE_ASSERT(UserDataBuffer != NULL);

    // initialise User Data Buffer
    memset(UserDataBuffer, 0, UserDataLength);
    DataCounter = 0;

    // No more need for saved user data index
    UserDataIndex = 0;

    for (unsigned int Count = 0; Count < ParsedFrameParameters->UserDataNumber; Count++)
    {
        // Copy the generic parameters
        memcpy((unsigned char *)UserDataBuffer + DataCounter,
               (unsigned char *)&AccumulatedUserData[Count].UserDataGenericParameters,
               sizeof(UserDataGenericParameters_t));
        DataCounter += sizeof(UserDataGenericParameters_t);

        // Copy additional user data parameters to the formated user data buffer
        if ((AccumulatedUserData[Count].UserDataAdditionalParameters.Length > 0) &&
            (DataCounter + AccumulatedUserData[Count].UserDataAdditionalParameters.Length <= UserDataLength))
        {
            memcpy((unsigned char *)UserDataBuffer + DataCounter,
                   (unsigned char *)&AccumulatedUserData[Count].UserDataAdditionalParameters.CodecUserDataParameters,
                   AccumulatedUserData[Count].UserDataAdditionalParameters.Length);
            DataCounter += AccumulatedUserData[Count].UserDataAdditionalParameters.Length;
        }

        // Add padding
        DataCounter += AccumulatedUserData[Count].UserDataGenericParameters.PaddingBytes;
        // Calculate playload length
        PayloadLength = AccumulatedUserData[Count].UserDataGenericParameters.BlockLength - AccumulatedUserData[Count].UserDataGenericParameters.PaddingBytes;
        PayloadLength -= AccumulatedUserData[Count].UserDataGenericParameters.HeaderLength;

        if (DataCounter + PayloadLength <= UserDataLength)
        {
            // Copy the payload
            memcpy(UserDataBuffer + DataCounter,
                   (unsigned char *)AccumulatedUserData[Count].UserDataPayload,
                   PayloadLength);
            // Next user data block will be concatenated to this block
            DataCounter += PayloadLength;
        }
    }

    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Base_c::ReadHeaders(                                      void )
///     \brief Attempt to parse the headers of the currently active frame.
///
///     Read headers is responsible for scrutonizing the header either by using
///     the start code list or by directly integrating ::BufferData and ::BufferLength.
///
///     If a valid frame is found this method must also set ::FrameToDecode to true.
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Base_c::ProcessQueuedPostDecodeParameterSettings( void )
///     \brief Patch up any post decode parameters that could not be determined during initial parsing.
///
///     In some cases it may not be possible to fully analyse the state of a frame (its post decode
///     parameters) during analysis. Examples include I-frames within video streams where the display index
///     cannot be determined until the B-frame to P-frame transition within the stream.
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Base_c::PurgeQueuedPostDecodeParameterSettings(   void )
///     \brief Purge the queue of post decode parameters that could not be determined during initial parsing.
///
///     In some cases it may not be possible to fully analyse the state of a frame (its post decode
///     parameters) during analysis. Examples include I-frames within video streams where the display index
///     cannot be determined until the B-frame to P-frame transition within the stream.
///     \return Frame parser status code, FrameParserNoError indicates success.
///

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Base_c::GeneratePostDecodeParameterSettings(              void )
///     \brief Populate the post decode parameter block.
///
///     Populate FrameParser_Base_c::ParsedFrameParameters with the frame parameters (as they will be after
///     decode has taken place).
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///
// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Base_c::ReadAdditionalUserDataParameters(                 void )
///     \brief Process additional user data parameters specific to a codec
///
///     As some codecs supports specific user data additional parameters, it reads those parameters and fill structure (passed in parameters)
///
///     \return true indicates that additional parameters exist.
///

