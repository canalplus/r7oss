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
#ifndef H_MANIFESTOR_AUDIO
#define H_MANIFESTOR_AUDIO

#include <ACC_Transformers/Audio_DecoderTypes.h>
#include "player.h"
#include "manifestor_base.h"
#include "ring_generic.h"
#include "codec_mme_base.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_Audio_c"

typedef enum
{
    AudioBufferStateAvailable,
    AudioBufferStateQueued,
    AudioBufferStateNotQueued
} AudioBufferState_t;

////////////////////////////////////////////////////////////////////////////
///
/// Additional information about a buffer.
///
typedef struct AudioStreamBuffer_s
{
    unsigned int BufferIndex;
    unsigned int QueueCount;
    unsigned int TimeOfGoingOnDisplay;

    bool EventPending;

    AudioBufferState_t BufferState;

    class Buffer_c *Buffer;

    ParsedFrameParameters_t     *FrameParameters; ///< Frame parameters for the buffer.
    ParsedAudioParameters_t     *AudioParameters; ///< Audio parameters for the buffer.
    ManifestationOutputTiming_t *AudioOutputTiming; ///< Audio output timing of the buffer.

    bool QueueAsCodedData; ///< True if the encoded buffer should be enqueued for display.

    unsigned int NextIndex; ///< Index of the next buffer to be displayed or -1 if no such buffer exists.
} AudioStreamBuffer_t;


//
// The set parameters definitions
//

typedef enum
{
    ManifestorAudioMixerConfiguration     = BASE_MANIFESTOR,
    ManifestorAudioSetEmergencyMuteState,
} ManifestorParameterBlockType_t;

typedef struct ManifestorAudioParameterBlock_s
{
    ManifestorParameterBlockType_t ParameterType;

    union
    {
        void *Mixer;
        bool  EmergencyMute;
    };
} ManifestorAudioParameterBlock_t;


////////////////////////////////////////////////////////////////////////////
///
/// Framework for implementing audio manifestors.
///
class Manifestor_Audio_c : public Manifestor_Base_c
{
public:
    /* Constructor / Destructor */
    Manifestor_Audio_c(void);
    ~Manifestor_Audio_c(void);

    /* Overrides for component base class functions */
    ManifestorStatus_t   Halt(void);
    ManifestorStatus_t   Reset(void);

    /* Manifestor class functions */
    ManifestorStatus_t  Connect(Port_c *Port);
    ManifestorStatus_t  GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces);
    ManifestorStatus_t  GetNextQueuedManifestationTime(unsigned long long *Time, unsigned int *NumTimes);
    ManifestorStatus_t  DropNextQueuedBufferUnderLock(void);
    ManifestorStatus_t  ReleaseQueuedDecodeBuffers(void);
    void                ReleaseProcessingBuffers(void);
    ManifestorStatus_t  QueueDecodeBuffer(class Buffer_c *Buffer,
                                          ManifestationOutputTiming_t **TimingArray, unsigned int *NumTimes);
    ManifestorStatus_t  GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long     *Pts);

    unsigned int        GetBufferId(void);

    /* these virtual functions are implemented by the device specific part of the audio manifestor */
    virtual ManifestorStatus_t  OpenOutputSurface(class HavanaStream_c *stream,
                                                  stm_se_sink_input_port_t input_port = STM_SE_SINK_INPUT_PORT_PRIMARY) = 0;
    virtual ManifestorStatus_t  CloseOutputSurface(void) = 0;

    virtual ManifestorStatus_t  WaitForProcessingBufRingRelease() = 0;
    virtual ManifestorStatus_t  GetChannelConfiguration(enum eAccAcMode *AcMode) = 0;
    virtual bool                IsTranscodeNeeded(void) = 0;
    virtual bool                IsCompressedFrameNeeded(void) = 0;
    virtual ManifestorStatus_t  GetDRCParams(DRCParams_t *DRC) = 0;

protected:
    bool                                DisplayUpdatePending;

    /* Display Information */
    struct OutputSurfaceDescriptor_s    SurfaceDescriptor;

    /* Buffer information */
    RingGeneric_c                      *ProcessingBufRing;

    unsigned long long                  PtsToDisplay;

    /* Data shared with buffer release process */
    bool                                ForcedUnblock;

    /* Lock/Unlock methods required to call Dequeue/PeekBufferUnderLock methods */
    void                                LockBuffferQueue();
    void                                UnLockBuffferQueue();

    /* Dequeue/PeekBufferUnderLock must be called after calling LockBuffferQueue() */
    ManifestorStatus_t                  DequeueBufferUnderLock(AudioStreamBuffer_t **StreamBufPtr);
    RingStatus_t                        PeekBufferUnderLock(AudioStreamBuffer_t **StreamBufPtr);

    virtual ManifestorStatus_t          QueueBuffer(AudioStreamBuffer_t *StreamBufPtr) = 0;
    virtual ManifestorStatus_t          ReleaseBuffer(AudioStreamBuffer_t *StreamBufPtr) = 0;
    void                                ExtractAndReleaseProcessingBuf(void);

private:
    void                                ReleaseBuf(AudioStreamBuffer_t *StreamBufPtr);
    void                                FreeQueuedBufRing(void);
    void                                FreeProcessingBufRing(void);
    unsigned int                        NumberOfAllocated_StreamBuf; //Counter of AudioStreamBuffer_t allocated structures
    unsigned int                        NumberOfFreed_StreamBuf;     //Counter of AudioStreamBuffer_t freed structures

    RingGeneric_c                      *QueuedBufRing;
    OS_Mutex_t                          BufferQueueLock;
    OS_Event_t                          BufferQueueUpdated;
    unsigned int                        BufferQueueTail;

    unsigned int                        RelayfsIndex; //stores id from relayfs to differentiate manifestors
    /* Generic stream information*/

    DISALLOW_COPY_AND_ASSIGN(Manifestor_Audio_c);
};

#endif
