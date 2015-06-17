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
#ifndef ENCODE_STREAM_H
#define ENCODE_STREAM_H

#include "encoder.h"
#include "release_buffer_interface.h"
#include "encode_stream_interface.h"
#include "encode_coordinator_interface.h"
#include "allocinline.h"

// Our processing C-Stubs declarations
OS_TaskEntry(EncoderProcessInputToPreprocessor);
OS_TaskEntry(EncoderProcessPreprocessorToCoder);
OS_TaskEntry(EncoderProcessCoderToOutput);

#define ENCODER_MAX_INPUT_BUFFERS                   64
#define ENCODE_STREAM_PROCESS_NB                    3   // Encoder creates/manages 3 threads for each encode stream: Input, Preproc, Coder
#define ENCODE_STREAM_MAX_EVENT_WAIT                50  // Ms

#define ENCODE_STREAM_MAX_PTOC_MESSAGES             8
#define ENCODE_STREAM_MAX_CTOO_MESSAGES             8

#define ENCODER_STREAM_AUDIO_MAX_PREPROC_BUFFERS        12  // 6 for SRC * 2 double buffering
#define ENCODER_STREAM_AUDIO_MAX_CODED_BUFFERS          8   // (roundup(4096/1536) + 1 drain delay) *2 double buffering
#define ENCODER_STREAM_AUDIO_MAX_CPU                    2   // Number of the MAX CPU on which Audio Encoder / Audio PreProcessor can run

#define ENCODER_STREAM_VIDEO_MAX_PREPROC_ALLOC_BUFFERS  4   // 2 for deinterlacing, 1 additional for intermediate, 1 for coder input; higher needed for sosphicated FRC
#define ENCODER_STREAM_VIDEO_MAX_PREPROC_BUFFERS        (8+ENCODER_STREAM_VIDEO_MAX_PREPROC_ALLOC_BUFFERS)  // 4 for video FRC upsampling*2 + ENCODER_STREAM_VIDEO_MAX_PREPROC_ALLOCATION_BUFFERS
#define ENCODER_STREAM_VIDEO_MAX_CODED_BUFFERS          2   // 1 for coder output, 1 for mem sink

#define ENCODER_STREAM_NULL_MAX_PREPROC_BUFFERS         2   // 1 for preproc output, 1 for coder input
#define ENCODER_STREAM_NULL_MAX_CODED_BUFFERS           2   // 1 for coder output, 1 for mem sink

#define ENCODER_STREAM_MAX_PREPROC_BUFFERS          (max(ENCODER_STREAM_AUDIO_MAX_PREPROC_BUFFERS, ENCODER_STREAM_VIDEO_MAX_PREPROC_BUFFERS))
#define ENCODER_STREAM_MAX_CODED_BUFFERS            (max(ENCODER_STREAM_AUDIO_MAX_CODED_BUFFERS, ENCODER_STREAM_VIDEO_MAX_CODED_BUFFERS))

/// This should move to Encoder_Generic.h when Control Structures are implemented
#define ENCODER_MAX_CONTROL_STRUCTURE_BUFFERS           512

#define BUFFER_PREPROC_ALLOCATION        "PreprocAllocationBuffer"
#define BUFFER_PREPROC_ALLOCATION_TYPE   {BUFFER_PREPROC_ALLOCATION, BufferDataTypeBase, AllocateFromSuppliedBlock, 4, 64, false, false, 0}
#define BUFFER_PREPROC_FRAME             "PreprocFrameBuffer"
#define BUFFER_PREPROC_FRAME_TYPE        {BUFFER_PREPROC_FRAME, BufferDataTypeBase, NoAllocation, 1, 0, false, false, 0}

#define BUFFER_CODED_FRAME          "EncodedFrameBuffer"
#define BUFFER_CODED_FRAME_TYPE     {BUFFER_CODED_FRAME, BufferDataTypeBase, AllocateFromSuppliedBlock, 4, 64, false, false, 0}

#define METADATA_INTERNAL_ENCODE_FRAME_PARAMETERS         "InternalEncodeFrameParameters"
#define METADATA_INTERNAL_ENCODE_FRAME_PARAMETERS_TYPE    {METADATA_INTERNAL_ENCODE_FRAME_PARAMETERS, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(__stm_se_frame_metadata_t)}

// Internal pixel aspect ratio defines for undefined display aspect ratio
// Values should be unique but not cause error
#define PIXEL_ASPECT_RATIO_NUM_UNSPECIFIED   0
#define PIXEL_ASPECT_RATIO_DEN_UNSPECIFIED   1

#define ALIGN_UP(x,y) ((((x) + (y-1))/(y))*(y))

typedef struct EncodeBufferRecord_s
{
    Buffer_t             Buffer;
    unsigned long long   SequenceNumber;

    bool                 ReleasedBuffer;

    union
    {
        EncodeControlStructure_t    *ControlStructure;
    };
} EncodeBufferRecord_t;

class EncodeStream_c : public EncodeStreamInterface_c
{
public:
    explicit EncodeStream_c(stm_se_encode_stream_media_t media);
    ~EncodeStream_c(void);

    //
    // EncodeStream class functions
    //

    //
    // Port interface
    //
    EncoderStatus_t     Connect(ReleaseBufferInterface_c *ReleaseBufferItf, Port_c **inputRing);
    EncoderStatus_t     Disconnect();
    EncoderStatus_t     Flush(void);

    bool                IsConnected(void)   { return mPortConnected; }
    Port_c             *GetInputPort(void)  { return mInputRing; }
    stm_se_encode_stream_media_t GetMedia(void) { return mMedia; }

    Encoder_c          *GetEncoderObject(void)  { Encoder_t Encoder; GetEncoder(&Encoder); return Encoder; }
    EncoderStatus_t     Input(Buffer_t  Buffer);

    EncoderStatus_t          GetStatistics(encode_stream_statistics_t *ExposedStatistics);
    EncoderStatus_t          ResetStatistics(void);
    EncodeStreamStatistics_t &EncodeStreamStatistics() { return Statistics; }

    //
    // Managing In Sequence Calls
    //

    EncoderStatus_t     CallInSequence(
        EncodeSequenceType_t      SequenceType,
        EncodeSequenceValue_t     SequenceValue,
        EncodeComponentFunction_t Fn,
        ...);

    EncoderStatus_t     PerformInSequenceCall(
        EncodeControlStructure_t *ControlStructure);

    EncoderStatus_t     AccumulateControlMessage(
        Buffer_t                          Buffer,
        EncodeControlStructure_t         *Message,
        unsigned int                     *MessageCount,
        unsigned int                      MessageTableSize,
        EncodeBufferRecord_t             *MessageTable);

    EncoderStatus_t     ProcessControlMessage(
        Buffer_t                          Buffer,
        EncodeControlStructure_t         *Message);

    EncoderStatus_t     ProcessAccumulatedControlMessages(
        unsigned int                     *MessageCount,
        unsigned int                      MessageTableSize,
        EncodeBufferRecord_t             *MessageTable,
        unsigned long long                SequenceNumber,
        unsigned long long                Time);

    //
    // Managing Processes, output rings and tasks
    //

    EncoderStatus_t StartStream(void);
    EncoderStatus_t StopStream(void);

    void            ProcessInputToPreprocessor(void);
    void            ProcessPreprocessorToCoder(void);
    void            ProcessCoderToOutput(void);

    EncoderStatus_t MarkStreamUnEncodable(void);

    EncoderStatus_t ManageMemoryProfile(void);

    EncoderStatus_t RegisterBufferManager(BufferManager_t  BufferManager);

    void            GetClassList(Encode_t                 *Encode,
                                 Preproc_t                *Preproc           = NULL,
                                 Coder_t                  *Coder             = NULL,
                                 Transporter_t            *Transporter       = NULL,
                                 EncodeCoordinator_t      *EncodeCoordinator = NULL);

    //
    // Managing sink attachment/detachment
    //

    EncoderStatus_t     AddTransport(stm_object_h  sink);
    EncoderStatus_t     RemoveTransport(stm_object_h  sink);

    //
    // Managing Controls
    //

    EncoderStatus_t     GetControl(stm_se_ctrl_t                     Control,
                                   void                             *Data);

    EncoderStatus_t     SetControl(stm_se_ctrl_t                     Control,
                                   const void                       *Data);

    EncoderStatus_t     GetCompoundControl(stm_se_ctrl_t             Control,
                                           void                     *Data);

    EncoderStatus_t     SetCompoundControl(stm_se_ctrl_t             Control,
                                           const void               *Data);

    EncoderStatus_t     GetEncoder(Encoder_t                        *Encoder);

    //
    // Managing discontinuity
    //

    EncoderStatus_t     InjectDiscontinuity(stm_se_discontinuity_t Discontinuity);

    //
    // Managing Events
    //

    EncoderStatus_t     SignalEvent(stm_se_encode_stream_event_t      Event);

    //
    // Encode streams linked list management
    //

    EncodeStream_t      GetNext(void);
    void                SetNext(EncodeStream_t Stream);

    //
    // Low power functions
    //

    EncoderStatus_t     LowPowerEnter(void);
    EncoderStatus_t     LowPowerExit(void);

    //
    // Memory Profile management
    //

    VideoEncodeMemoryProfile_t  GetVideoEncodeMemoryProfile(void);
    void                        SetVideoEncodeMemoryProfile(VideoEncodeMemoryProfile_t MemoryProfile);
    surface_format_t            GetVideoInputColorFormatForecasted(void);
    void                        SetVideoInputColorFormatForecasted(surface_format_t ForecastedFormat);

    // Use of "AudioEncodeNo" is to decide which audio stream run on which Audio CPU provided capability on that CPU.
    uint32_t          AudioEncodeNo;

private:
    stm_se_encode_stream_media_t mMedia;

    OS_Mutex_t        Lock;

    // Synchronize between data and discontinuity inject process
    OS_Mutex_t        SyncProcess;

    EncodeStream_t    Next;

    BufferManager_t   BufferManager;

    Ring_t            mInputRing;
    Ring_t            PreprocOutputRing;
    Ring_t            CoderOutputRing;

    EncodeStreamStatistics_t   Statistics;

    VideoEncodeMemoryProfile_t VideoEncodeMemoryProfile;
    surface_format_t           VideoEncodeInputColorFormatForecasted;

    unsigned int      CodedMemorySize;
    unsigned int      CodedFrameMaximumSize;
    char              CodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    bool              Terminating;
    unsigned int      ProcessRunningCount;
    OS_Mutex_t        StartStopLock;
    OS_Event_t        StartStopEvent;

    /// Should the worst ever happen, we can use this member to mark the stream as un-encodable
    bool              UnEncodable;

    // Buffer Types
    BufferType_t      PreprocFrameBufferType;
    BufferType_t      PreprocFrameAllocType;
    BufferType_t      CodedFrameBufferType;
    BufferType_t      BufferInputBufferType;

    BufferType_t      InputMetaDataBufferType;
    BufferType_t      EncodeCoordinatorMetaDataBufferType;

    BufferType_t      MetaDataSequenceNumberType;
    BufferType_t      BufferEncoderControlStructureType;

    unsigned int      MarkerInPreprocFrameIndex;
    unsigned int      MarkerInCodedFrameIndex;

    unsigned long long NextBufferSequenceNumber;

    bool              DiscardingUntilMarkerFramePtoC;   ///< To allow frame drops between Preprocessor and Coder
    bool              DiscardingUntilMarkerFrameCtoO;   ///< To allow frame drops between Coder and Output

    // Port management variables
    bool                             mPortConnected;
    ReleaseBufferInterface_c        *mReleaseBufferCallBack;

    // Low power data
    bool              IsLowPowerState;    // HPS/CPS: indicates when SE is in low power state
    OS_Event_t        LowPowerEnterEvent; // HPS/CPS: event used to synchronize with the PreprocessorToCoder thread on low power entry
    OS_Event_t        LowPowerExitEvent;  // HPS/CPS: event used to synchronize with the PreprocessorToCoder thread on low power exit

    // Accumulated messages in each process
    EncodeBufferRecord_t  AccumulatedBeforePtoCControlMessages[ENCODE_STREAM_MAX_PTOC_MESSAGES];
    EncodeBufferRecord_t  AccumulatedAfterPtoCControlMessages[ENCODE_STREAM_MAX_PTOC_MESSAGES];

    EncodeBufferRecord_t  AccumulatedBeforeCtoOControlMessages[ENCODE_STREAM_MAX_CTOO_MESSAGES];
    EncodeBufferRecord_t  AccumulatedAfterCtoOControlMessages[ENCODE_STREAM_MAX_CTOO_MESSAGES];

    Transporter_t         FindTransporter(stm_object_h   Sink);

    EncoderStatus_t       DelegateGetControl(stm_se_ctrl_t   Control,
                                             void           *Data);

    EncoderStatus_t       DelegateSetControl(stm_se_ctrl_t   Control,
                                             const void     *Data);

    EncoderStatus_t       DelegateGetCompoundControl(stm_se_ctrl_t   Control,
                                                     void           *Data);

    EncoderStatus_t       DelegateSetCompoundControl(stm_se_ctrl_t   Control,
                                                     const void     *Data);

    EncoderStatus_t       ReleaseInputBuffer(Buffer_t Buffer);

    DISALLOW_COPY_AND_ASSIGN(EncodeStream_c);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class EncodeStream_c
\brief An EncodeStream encapsulates the encode pipeline of an individual stream.

The EncodeStream creates and manages the control threads, and data output rings, and initiates data flow between components.

\note  In 'Player' internals, this object most closely resembles ::PlayerStream_c
*/

#endif /* ENCODE_STREAM_H */
