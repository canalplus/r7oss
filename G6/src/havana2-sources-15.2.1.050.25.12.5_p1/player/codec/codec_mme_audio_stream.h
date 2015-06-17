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

#ifndef H_CODEC_MME_AUDIO_STREAM
#define H_CODEC_MME_AUDIO_STREAM

#include "codec_mme_audio.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioStream_c"
#define DEFAULT_SENDBUF_TRIGGER_TRANSFORM_COUNT         8               /* No. buffers with firmware before issuing transform */
#define DEFAULT_SENDBUF_DECODE_CONTEXT_COUNT            (DEFAULT_SENDBUF_TRIGGER_TRANSFORM_COUNT+4)
#define DEFAULT_COMMAND_CONTEXT_COUNT                   DEFAULT_SENDBUF_DECODE_CONTEXT_COUNT

#define MAXIMUM_STALL_PERIOD                            250000          /* 1/4 second */
#define STREAM_BASE_TRANSCODE_BUFFER_INDEX              1  // 0 For PCM output in stream base 1 for transcoded output

/* To prevent the lock up in waiting for the EOF from the FW(may be due to some FW error) we need to generate Auto EOF
   after callback of the 4 transfrom commands after entire input is consumed. */

#define WAIT_FOR_TRANSFORM_COMMANDS_BEFORE_AUTO_EOF     4

typedef struct StreamAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t    StreamParameters;
} StreamAudioCodecStreamParameterContext_t;

typedef struct
{
    MME_LxAudioDecoderFrameStatus_t                DecStatus;
    MME_PcmProcessingFrameExtCommonStatus_t        PcmStatus;
} MME_LxAudioDecoderFrameExtendedStreamStatus_t;

typedef struct StreamAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_StreamingBufferParams_t         DecodeParameters;
    MME_LxAudioDecoderFrameExtendedStreamStatus_t     DecodeStatus;
    uint32_t                            TranscodeBufferIndex;
    uint32_t                            AuxBufferIndex;
    uint32_t                            CompressedFrameBufferIndex[AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_SCATTER_PAGES];
} StreamAudioCodecDecodeContext_t;

typedef struct StreamAudioCodecTransformContext_s
{
    CodecBaseDecodeContext_t                          BaseContext;

    MME_LxAudioDecoderFrameParams_t                   DecodeParameters;
    MME_LxAudioDecoderFrameExtendedStreamStatus_t     DecodeStatus;
    uint32_t                                          TranscodeBufferIndex;
    uint32_t                                          AuxBufferIndex;
    uint32_t                                          CompressedFrameBufferIndex[AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_SCATTER_PAGES];
} StreamAudioCodecTransformContext_t;

class Codec_MmeAudioStream_c : public Codec_MmeAudio_c
{
public:
    Codec_MmeAudioStream_c(void);
    ~Codec_MmeAudioStream_c(void);

    CodecStatus_t   Halt(void);

    CodecStatus_t   FillOutDecodeContext(void);
    void            FinishedDecode(void);
    CodecStatus_t   Connect(Port_c *Port);

    void            CallbackFromMME(MME_Event_t     Event,
                                    MME_Command_t  *Command);
    CodecStatus_t   SendMMEDecodeCommand(void);

    CodecStatus_t   Input(Buffer_t                  CodedBuffer);
    CodecStatus_t   DiscardQueuedDecodes(void);
    CodecStatus_t   CheckForMarkerFrameStall(void);
    CodecStatus_t   OutputPartialDecodeBuffers(void);

    void            TransformThread(void);

protected:
    int                         SendbufTriggerTransformCount;

    bool                        NeedToMarkStreamUnplayable;
    CodecBaseDecodeContext_t   *TransformContext;

    /// The presence of the worker thread means we must be careful not to manipulate the pool
    /// while we are iterating over its members.
    OS_Mutex_t                  DecodeContextPoolMutex;

    eAccDecoderId               DecoderId;

    virtual CodecStatus_t       InitializeDataTypes(void);

    void                        PresetIOBuffers(void);
    virtual void                SetCommandIO(void);

    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);
    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t       *Context);
    CodecStatus_t   DumpSetStreamParameters(void                           *Parameters);
    CodecStatus_t   DumpDecodeParameters(void                           *Parameters);
    void            FillStreamMetadata(ParsedAudioParameters_t *AudioParameters, MME_LxAudioDecoderFrameStatus_t *Status);

private:
    bool                        TransformThreadRunning;
    OS_Event_t                  TransformThreadTerminated;
    OS_Mutex_t                  InputMutex;
    OS_Event_t                  IssueTransformCommandEvent;
    OS_Event_t                  IssueSendBufferEvent;

    BufferDataDescriptor_t     *mTransformContextDescriptor;
    BufferType_t                mTransformContextType;
    BufferPool_t                TransformContextPool;
    Buffer_t                    TransformContextBuffer;

    allocator_device_t          TransformCodedFrameMemoryDevice;
    void                       *TransformCodedFrameMemory[3];
    BufferPool_t                TransformCodedFramePool;

    unsigned int                CurrentDecodeFrameIndex;

    ParsedFrameParameters_t     SavedParsedFrameParameters;
    PlayerSequenceNumber_t      SavedSequenceNumberStructure;
    PlayerSequenceNumber_t      MarkerFrameSavedSequenceNumberStructure;

    bool                        InPossibleMarkerStallState;
    unsigned long long          TimeOfEntryToPossibleMarkerStallState;
    unsigned int                StallStateSendBuffersCommandsIssued;
    unsigned int                StallStateTransformCommandsCompleted;

    unsigned int                SendBuffersCommandsIssued;
    unsigned int                SendBuffersCommandsCompleted;
    unsigned int                TransformCommandsIssued;
    unsigned int                TransformCommandsCompleted;

    unsigned long long          LastNormalizedPlaybackTime;
    bool                        EofMarkerFrameReceived;
    bool                        DontIssueEof;
    bool                        PutMarkerFrameToTheRing;
    unsigned int                TransformCommandsToIssueBeforeAutoEof;
    unsigned int                SendBuffersCommandsIssuedTillMarkerFrame;

    DISALLOW_COPY_AND_ASSIGN(Codec_MmeAudioStream_c);

    // Helper methods for the playback thread
    void            AttachCodedFrameBuffer(void);
    CodecStatus_t   FillOutTransformContext(void);
    CodecStatus_t   FillOutTransformCommand(void);
    CodecStatus_t   SendTransformCommand(void);
    void            FinishedTransform(void);
    //CodecStatus_t   AbortSendBuffersCommands(   void );
    CodecStatus_t   SendMMETransformCommand(void);
    //CodecStatus_t   AbortTransformCommands(     void );
    CodecStatus_t   AbortMMECommands(BufferPool_t            CommandContextPool);
};

#endif
