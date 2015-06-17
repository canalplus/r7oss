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

Source file name : codec_mme_audio_stream.h
Author :           Adam

Implementation of the basic stream based audio codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
27-May-09   Created (from codec_mme_audio_wma.h)            Julian

************************************************************************/

#ifndef H_CODEC_MME_AUDIO_STREAM
#define H_CODEC_MME_AUDIO_STREAM

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_audio.h"
#include "player_generic.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
#define DEFAULT_SENDBUF_TRIGGER_TRANSFORM_COUNT         8               /* No. buffers with firmware before issuing transform */
#define DEFAULT_SENDBUF_DECODE_CONTEXT_COUNT            (DEFAULT_SENDBUF_TRIGGER_TRANSFORM_COUNT+4)
#define DEFAULT_COMMAND_CONTEXT_COUNT                   DEFAULT_SENDBUF_DECODE_CONTEXT_COUNT

#define MAXIMUM_STALL_PERIOD                            250000          /* 1/4 second */

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct StreamAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t    StreamParameters;
} StreamAudioCodecStreamParameterContext_t;

typedef struct StreamAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_StreamingBufferParams_t         DecodeParameters;
    MME_LxAudioDecoderFrameStatus_t     DecodeStatus;
} StreamAudioCodecDecodeContext_t;

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Codec_MmeAudioStream_c : public Codec_MmeAudio_c
{
//! Stuff for MME_TRANFORM commands
private:

    bool                        TransformThreadRunning;
    OS_Thread_t                 TransformThreadId;
    OS_Event_t                  TransformThreadTerminated;
    OS_Mutex_t                  InputMutex;
    OS_Event_t                  IssueTransformCommandEvent;

    BufferPool_t                TransformContextPool;
    CodecBaseDecodeContext_t*   TransformContext;
    Buffer_t                    TransformContextBuffer;

    allocator_device_t          TransformCodedFrameMemoryDevice;
    void                       *TransformCodedFrameMemory[3];
    BufferPool_t                TransformCodedFramePool;

    unsigned int                CurrentDecodeFrameIndex;

    ParsedFrameParameters_t     SavedParsedFrameParameters;
    PlayerSequenceNumber_t      SavedSequenceNumberStructure;

    bool                        InPossibleMarkerStallState;
    unsigned long long          TimeOfEntryToPossibleMarkerStallState;
    int                         StallStateSendBuffersCommandsIssued;
    int                         StallStateTransformCommandsCompleted;

    int                         SendBuffersCommandsIssued;
    int                         SendBuffersCommandsCompleted;
    int                         TransformCommandsIssued;
    int                         TransformCommandsCompleted;

    unsigned long long          LastNormalizedPlaybackTime;

protected:

    // Data
    int                         SendbufTriggerTransformCount;

    bool                        NeedToMarkStreamUnplayable;

    /// The presence of the worker thread means we must be careful not to manipulate the pool
    /// while we are iterating over its members.
    OS_Mutex_t                  DecodeContextPoolMutex;

    eAccDecoderId               DecoderId;

    // Functions

private:

    // Helper methods for the playback thread
    CodecStatus_t   FillOutTransformContext(    void );
    CodecStatus_t   FillOutTransformCommand(    void );
    CodecStatus_t   SendTransformCommand(       void );
    void            FinishedTransform(          void );
    //CodecStatus_t   AbortSendBuffersCommands(   void );
    CodecStatus_t   SendMMETransformCommand(    void );
    //CodecStatus_t   AbortTransformCommands(     void );
    CodecStatus_t   AbortMMECommands(           BufferPool_t            CommandContextPool);

public:

    //
    // Constructor/Destructor methods
    //

    Codec_MmeAudioStream_c(             void );
    ~Codec_MmeAudioStream_c(            void );

    //
    // Overrides for component base class functions
    //

    CodecStatus_t   Halt(                       void );
    CodecStatus_t   Reset(                      void );

    CodecStatus_t   FillOutDecodeContext(       void );
    void            FinishedDecode(             void );
    CodecStatus_t   RegisterOutputBufferRing(   Ring_t                    Ring );

    void            CallbackFromMME(            MME_Event_t               Event,
                                                MME_Command_t            *Command );
    CodecStatus_t   SendMMEDecodeCommand(       void );

    CodecStatus_t   Input(                      Buffer_t                  CodedBuffer );
    CodecStatus_t   DiscardQueuedDecodes(       void );
    CodecStatus_t   CheckForMarkerFrameStall(   void );

    void            TransformThread(            void );

    //
    // Stream specific functions
    //

protected:

    CodecStatus_t   FillOutTransformerInitializationParameters( void );
    CodecStatus_t   FillOutSetStreamParametersCommand(          void );
    CodecStatus_t   FillOutDecodeCommand(                       void );
    CodecStatus_t   ValidateDecodeContext(                      CodecBaseDecodeContext_t       *Context );
    CodecStatus_t   DumpSetStreamParameters(                    void                           *Parameters );
    CodecStatus_t   DumpDecodeParameters(                       void                           *Parameters );


};
#endif
