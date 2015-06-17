/************************************************************************
Copyright (C) 2007, 2008 STMicroelectronics. All Rights Reserved.

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
 * V4L2 dvp output device driver for ST SoC display subsystems header file
************************************************************************/

#ifndef DVB_DVP_AUDIO_H_
#define DVB_DVP_AUDIO_H_

#include "dvb_avr.h"
#include "dvb_avr_audio_internals.h"

typedef enum {
  AVR_HDMI_LAYOUT0 = 2, 
  AVR_HDMI_LAYOUT1 = 8, 
  AVR_HDMI_HBRA    = 3
} avr_hdmi_layout_t;


typedef enum {
	EMERGENCY_MUTE_REASON_NONE,
	EMERGENCY_MUTE_REASON_USER,
	EMERGENCY_MUTE_REASON_ACCELERATED,
	EMERGENCY_MUTE_REASON_SAMPLE_RATE_CHANGE,
	EMERGENCY_MUTE_REASON_ERROR
} avr_audio_emergency_mute_reason_t;

typedef enum 
{
    AVR_LATENCY_MODE_DEFAULT, // default latency mode, will result into a 100ms latency
    AVR_LATENCY_MODE_SPDIF,   // SPDIF-restricted mode (AC3 and some DTS modes), will result into a 60ms latency
    AVR_LATENCY_MODE_PCM,     // PCM-mode, will result into a 20ms latency
    AVR_LATENCY_MODE_MAX
} avr_audio_latency_mode_t;

typedef enum {
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_44100 = 0,
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_48000 = 1,
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_88200 = 2,
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_176400 = 3,
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_96000 = 4,
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_192000 = 5,
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_32000 = 6,
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_16000 = 7,
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_22050 = 8,
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_24000 = 9,
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_64000 = 10,
	AVR_AUDIO_DISCRETE_SAMPLE_RATE_128000 = 11
} avr_audio_discrete_sample_rate_t;

typedef struct dvp_v4l2_audio_handle_s 
{
    avr_v4l2_shared_handle_t *SharedContext;
    struct DeviceContext_s *DeviceContext;

    bool AacDecodeEnable;
    unsigned long CompensatoryLatency; //!< In milliseconds
    avr_audio_emergency_mute_reason_t EmergencyMuteReason;
    bool FormatRecogniserEnable;
    enum v4l2_avr_audio_channel_select ChannelSelect;
    int SilenceThreshold; //!< In decibels (0db is maximum power)
    unsigned long SilenceDuration; //!< In milliseconds

    struct class_device *EmergencyMuteClassDevice;

    unsigned int SampleRate;
    unsigned int PreviousSampleRate;
    avr_audio_discrete_sample_rate_t DiscreteSampleRate;

    unsigned int      AudioInputId;   // id of the pcm reader we're connected to
    avr_hdmi_layout_t HdmiLayout;     // {layout0 = 0 / layout1 = 1 / hbra = 3}
    unsigned int      HdmiAudioMode;  // { direct from info frame }
    bool              EmphasisFlag;

    struct semaphore ThreadStatusSemaphore;
    struct task_struct *ThreadHandle;
    volatile bool ThreadShouldStop;
    volatile bool ThreadWaitingForStop; // true if the thread is idling waiting to be asked to stop

    // class device for the sysfs mirrorred player stream
    struct class_device StreamClassDevice;
    struct class_device * PlaybackClassDevice; // pointer to a structure created by the sysfs_player
    bool IsSysFsStructureCreated;

    MME_LxAudioDecoderHDInfo_t AudioDecoderTransformCapability;

    unsigned long PostMortemPhysicalAddress;
    MME_TimeLogPostMortem_t *PostMortemBuffer;
    unsigned long SecondaryPostMortemPhysicalAddress;
    MME_TimeLogPostMortem_t *SecondaryPostMortemBuffer;
    bool PostMortemForce;
    unsigned int PostMortemAlreadyIssued; // must not be bool (tas doesn't work on not-aligned values)

    bool IsLLTransformerInitialized;
    MME_TransformerHandle_t LLTransformerHandle;
    int MasterLatencyWhenInitialized;

    ksnd_pcm_t *SoundcardHandle[AVR_LOW_LATENCY_MAX_OUTPUT_CARDS];

    struct semaphore    DecoderStatusSemaphore;
    volatile bool UpdateMixerSettings;
    volatile bool GotTransformCommandCallback;
    volatile bool GotSetGlobalCommandCallback;
    volatile bool NeedAnotherSetGlobalCommand;
    volatile bool GotSendBufferCommandCallback;

    MME_Command_t                              TransformCommand, SetGlobalCommand, SendBufferCommands[AVR_LOW_LATENCY_MAX_TOTAL_SEND_BUFFERS];
    MME_LowlatencySpecializedGlobalParams_t    SetGlobalParams;
    MME_LowlatencyTransformParams_t            TransformParams;
    MME_LowlatencySpecializedTransformStatus_t TransformStatus, SetGlobalStatus;

    MME_DataBuffer_t   DataBuffers[AVR_LOW_LATENCY_MAX_TOTAL_SEND_BUFFERS];
    MME_DataBuffer_t  * DataBufferList[AVR_LOW_LATENCY_MAX_TOTAL_SEND_BUFFERS];
    MME_ScatterPage_t  ScatterPages[AVR_LOW_LATENCY_MAX_TOTAL_SEND_BUFFERS];
 
    // this block of data is protected by the DecoderStatusSemaphore semaphore
    LLDecoderStatus_t LLDecoderStatus;
    // end of this block of data is protected

    struct 
    {
        BufferPool_t  Pool;
        BufferEntry_t Entries[AVR_LOW_LATENCY_MAX_LOG_INPUT_SEND_BUFFERS];
        //        bool          IsInitalized;
    } InputPool;
    
    struct 
    {
        BufferPool_t  Pool;
        BufferEntry_t Entries[AVR_LOW_LATENCY_MAX_LOG_OUTPUT_SEND_BUFFERS];
        //        bool          IsInitalized;
    } OutputPool;

    BufferEntry_t * BufferEntries[AVR_LOW_LATENCY_MAX_TOTAL_SEND_BUFFERS];

    wait_queue_head_t WaitQueue;

    atomic_t CommandsInFlight;
    atomic_t ReuseOfGlobals;
    atomic_t ReuseOfTransform;
} avr_v4l2_audio_handle_t;

avr_v4l2_audio_handle_t *AvrAudioNew(avr_v4l2_shared_handle_t *SharedContext);
int AvrAudioDelete(avr_v4l2_audio_handle_t *AudioContext);

bool AvrAudioGetAacDecodeEnable(avr_v4l2_audio_handle_t *AudioContext);
void AvrAudioSetAacDecodeEnable(avr_v4l2_audio_handle_t *AudioContext, bool Enabled);

long long AvrAudioGetCompensatoryLatency(avr_v4l2_audio_handle_t *AudioContext);
void AvrAudioSetCompensatoryLatency(avr_v4l2_audio_handle_t *AudioContext, long long MicroSeconds);

avr_audio_emergency_mute_reason_t AvrAudioGetEmergencyMuteReason(avr_v4l2_audio_handle_t *AudioContext);
int AvrAudioSetEmergencyMuteReason(avr_v4l2_audio_handle_t *Context, avr_audio_emergency_mute_reason_t reason);

bool AvrAudioGetFormatRecogniserEnable(avr_v4l2_audio_handle_t *AudioContext);
void AvrAudioSetFormatRecogniserEnable(avr_v4l2_audio_handle_t *AudioContext, bool Enabled);

int  AvrAudioGetInput(avr_v4l2_audio_handle_t *AudioContext);
void AvrAudioSetInput(avr_v4l2_audio_handle_t *AudioContext, unsigned int InputId);

void AvrAudioSetHdmiLayout   (avr_v4l2_audio_handle_t *AudioContext, int layout);
int  AvrAudioGetHdmiLayout   (avr_v4l2_audio_handle_t *AudioContext);

void AvrAudioSetHdmiAudioMode(avr_v4l2_audio_handle_t *AudioContext, int audio_mode);
int  AvrAudioGetHdmiAudioMode(avr_v4l2_audio_handle_t *AudioContext);

void AvrAudioSetEmphasis     (avr_v4l2_audio_handle_t *AudioContext, bool Emphasis);
bool AvrAudioGetEmphasis     (avr_v4l2_audio_handle_t *AudioContext);

void AvrAudioSetChannelSelect(avr_v4l2_audio_handle_t *AudioContext,
			      enum v4l2_avr_audio_channel_select ChannelSelect);
enum v4l2_avr_audio_channel_select AvrAudioGetChannelSelect(avr_v4l2_audio_handle_t *AudioContext);

void AvrAudioSetSilenceThreshold(avr_v4l2_audio_handle_t *AudioContext, int Threshold);
int  AvrAudioGetSilenceThreshold(avr_v4l2_audio_handle_t *AudioContext);

void AvrAudioSetSilenceDuration(avr_v4l2_audio_handle_t *AudioContext, int Duration);
unsigned long  AvrAudioGetSilenceDuration(avr_v4l2_audio_handle_t *AudioContext);

int AvrAudioIoctlOverlayStart(avr_v4l2_audio_handle_t *AudioContext);
int AvrAudioIoctlOverlayStop(avr_v4l2_audio_handle_t *AudioContext);

void AvrAudioMixerSettingsChanged(avr_v4l2_audio_handle_t *AudioContext);

#endif /*DVB_DVP_AUDIO_H_*/
