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

#ifndef H_HAVANA_STREAM
#define H_HAVANA_STREAM

#include "player.h"
#include "player_types.h"
#include "buffer_generic.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "havana_playback.h"
#include "havana_user_data.h"
#include "mixer_transformer.h"
#include "mixer_mme.h"

#undef TRACE_TAG
#define TRACE_TAG   "HavanaStream_c"

#define MAX_MANIFESTORS                 16

#define DVP_CODED_FRAME_COUNT           64
#define DVP_MAXIMUM_FRAME_SIZE          128
#define DVP_FRAME_MEMORY_SIZE           (DVP_CODED_FRAME_COUNT * DVP_MAXIMUM_FRAME_SIZE)
#define DVP_MEMORY_PARTITION            "vid-raw-input"

#define DIM_ALIGN 0x1F

// Need to align both width and height to estimate the correct number of available buffers
#define SD_420_BUFFER_SIZE              ((3*BUF_ALIGN_UP(PLAYER_SD_FRAME_WIDTH, DIM_ALIGN)*BUF_ALIGN_UP(PLAYER_SD_FRAME_HEIGHT, DIM_ALIGN))/2)
#define HD_720P_420_BUFFER_SIZE         ((3*BUF_ALIGN_UP(PLAYER_HD720P_FRAME_WIDTH, DIM_ALIGN)*BUF_ALIGN_UP(PLAYER_HD720P_FRAME_HEIGHT, DIM_ALIGN))/2)
#define HD_420_BUFFER_SIZE              ((3*BUF_ALIGN_UP(PLAYER_HD_FRAME_WIDTH, DIM_ALIGN)*BUF_ALIGN_UP(PLAYER_HD_FRAME_HEIGHT, DIM_ALIGN))/2)
#define HD_444_BUFFER_SIZE              (3*BUF_ALIGN_UP(PLAYER_HD_FRAME_WIDTH, DIM_ALIGN)*BUF_ALIGN_UP(PLAYER_HD_FRAME_HEIGHT, DIM_ALIGN))
#define UHD_4K2K_420_BUFFER_SIZE        ((3*BUF_ALIGN_UP(PLAYER_4K2K_FRAME_WIDTH, DIM_ALIGN)*BUF_ALIGN_UP(PLAYER_4K2K_FRAME_HEIGHT, DIM_ALIGN))/2)
#define UHD_4K2K_444_BUFFER_SIZE        (3*BUF_ALIGN_UP(PLAYER_4K2K_FRAME_WIDTH, DIM_ALIGN)*BUF_ALIGN_UP(PLAYER_4K2K_FRAME_HEIGHT, DIM_ALIGN))
#define UHD_420_BUFFER_SIZE             ((3*BUF_ALIGN_UP(PLAYER_UHD_FRAME_WIDTH, DIM_ALIGN)*BUF_ALIGN_UP(PLAYER_UHD_FRAME_HEIGHT, DIM_ALIGN))/2)
#define UHD_444_BUFFER_SIZE             (3*BUF_ALIGN_UP(PLAYER_UHD_FRAME_WIDTH, DIM_ALIGN)*BUF_ALIGN_UP(PLAYER_UHD_FRAME_HEIGHT, DIM_ALIGN))

/// Player wrapper class responsible for managing a stream.
class HavanaStream_c
{
public:
    HavanaStream_c(void);
    ~HavanaStream_c(void);

    HavanaStatus_t              Init(class HavanaPlayer_c           *InitHavanaPlayer,
                                     class Player_c                 *InitPlayer,
                                     class HavanaPlayback_c         *InitHavanaPlayback,
                                     stm_se_media_t                  Media,
                                     stm_se_stream_encoding_t        InitEncoding,
                                     unsigned int                    Id);
    HavanaStatus_t              AddManifestor(class Manifestor_c             *Manifestor);
    HavanaStatus_t              RemoveManifestor(class Manifestor_c             *Manifestor);
    HavanaStatus_t              FindManifestorByCapability(unsigned int                    Capability,
                                                           class Manifestor_c            **MatchingManifestor);

    void                        LockManifestors(void) { OS_LockMutex(&ManifestorLock); }
    Manifestor_t               *GetManifestors(void) { return Manifestor; }
    void                        UnlockManifestors(void) { OS_UnLockMutex(&ManifestorLock); }

    PlayerStreamType_t          StreamType(void);


    HavanaStatus_t              InjectData(const void                     *Data,
                                           unsigned int                    DataLength,
                                           PlayerInputDescriptor_t         InjectedDataDescriptor);

    HavanaStatus_t              Discontinuity(bool                            ContinuousReverse,
                                              bool                            SurplusData,
                                              bool                            EndOfStream);
    HavanaStatus_t              Drain(bool                            Discard);
    HavanaStatus_t              Enable(bool                            Manifest);
    HavanaStatus_t              GetEnable(bool                           *Manifest);
    HavanaStatus_t              SetOption(PlayerPolicy_t                   Option,
                                          int                             Value);
    HavanaStatus_t              GetOption(PlayerPolicy_t                   Option,
                                          int                            *Value);
    HavanaStatus_t              CheckOption(stm_se_ctrl_t                   Option,
                                            PlayerPolicy_t                 *PlayerPolicy);
    HavanaStatus_t              Step(void);
    HavanaStatus_t              SetPlayInterval(unsigned long long              Start,
                                                unsigned long long              End);
    HavanaStatus_t              CheckEvent(struct PlayerEventRecord_s     *PlayerEvent);
    HavanaStatus_t              GetStatistics(struct statistics_s *Statistics);
    HavanaStatus_t              ResetStatistics(void);
    HavanaStatus_t              GetAttributes(struct attributes_s *Attributes);
    HavanaStatus_t              ResetAttributes(void);
    HavanaStatus_t              GetPlayInfo(stm_se_play_stream_info_t      *PlayInfo);
    HavanaStatus_t              Switch(stm_se_stream_encoding_t        Encoding);
    HavanaStatus_t              GetDecodeBuffer(stm_pixel_capture_format_t    Format,
                                                unsigned int                  Width,
                                                unsigned int                  Height,
                                                Buffer_t                      *Buffer,
                                                uint32_t                      *LumaAddress,
                                                uint32_t                      *ChromaOffset,
                                                unsigned int                  *Stride,
                                                bool                          NonBlockingInCaseOfFailure);
    HavanaStatus_t              ReturnDecodeBuffer(Buffer_t Buffer);

    stream_buffer_capture_callback      RegisterBufferCaptureCallback(stm_se_event_context_h         Context,
                                                                      stream_buffer_capture_callback Callback);
    HavanaStatus_t              GetElementaryBufferLevel(stm_se_ctrl_play_stream_elementary_buffer_level_t    *ElementaryBufferLevel);
    HavanaStatus_t              GetCompoundControl(stm_se_ctrl_t                   Ctrl,
                                                   void                           *Value);

    HavanaStatus_t              SetCompoundControl(stm_se_ctrl_t                   Ctrl,
                                                   void                           *Value);
    HavanaStatus_t              GetControl(stm_se_ctrl_t                   Ctrl,
                                           int                            *Value);

    HavanaStatus_t              SetControl(stm_se_ctrl_t                   Ctrl,
                                           int                             Value);

    HavanaStatus_t              SetAlarm(stm_se_play_stream_alarm_t     alarm,
                                         bool  enable,
                                         void *value);

    HavanaStatus_t              SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger);

    HavanaStatus_t              ResetDiscardTrigger(void);

    HavanaStatus_t              UpdateStreamParams(unsigned int Config);
    unsigned int                GetTransformerId(void);

    HavanaStatus_t              LowPowerEnter(void);
    HavanaStatus_t              LowPowerExit(void);

    class HavanaPlayback_c     *GetHavanaPlayback()
    {
        return HavanaPlayback;
    }

    class PlayerStream_c       *GetStream(void) { return PlayerStream; }

    class UserDataSource_c      UserDataSender;

    stm_object_h                GetAudioPassThrough(void) { return (stm_object_h) AudioPassThrough; }
    HavanaStatus_t              AddAudioPlayer(stm_object_h sink);
    HavanaStatus_t              RemoveAudioPlayer(stm_object_h sink);
    HavanaStatus_t              DeletePassThrough(void);

private:
    unsigned int                TransformerId;
    // used to protect concurrent HavanaStream function calls (prevent
    // race conditions, unexpected pointer changes, etc.)
    OS_Mutex_t                  InputLock;
    // used to protect Manifestor[] table - i.e. concurrent call of manifestor
    // detach and get info raises NULL pointer crash which should not happen
    OS_Mutex_t                  ManifestorLock;

    PlayerStreamType_t          PlayerStreamType;
    PlayerStream_t              PlayerStream;

    class HavanaPlayer_c       *HavanaPlayer;
    class HavanaPlayback_c     *HavanaPlayback;
    class HavanaCapture_c      *HavanaCapture;

    class Player_c             *Player;
    class Collator_c           *Collator;
    class FrameParser_c        *FrameParser;
    class Codec_c              *Codec;
    class OutputTimer_c        *OutputTimer;
    class DecodeBufferManager_c *DecodeBufferManager;
    class ManifestationCoordinator_c *ManifestationCoordinator;
    class BufferPool_c         *DecodeBufferPool;

    class Manifestor_c         *Manifestor[MAX_MANIFESTORS];

    OS_Mutex_t                  AudioPassThroughLock;
    unsigned int                NbAudioPlayerSinks; // Number of Players Attached for PassThrough
    stm_object_h                AudioPlayers[MAX_AUDIO_PLAYERS];
    class Mixer_Mme_c          *AudioPassThrough;   // Mixer object enabling the PassThrough

    HavanaStatus_t              CreatePassThrough(void);
    HavanaStatus_t              AttachToPassThrough(stm_object_h sink);
    HavanaStatus_t              DetachFromPassThrough(stm_object_h sink);

    stm_se_stream_encoding_t    Encoding;

    HavanaStatus_t              FindManifestor(class Manifestor_c             *ManifestorToFind,
                                               unsigned int                   *Index);
    HavanaStatus_t              ConfigureDecodeBufferManager(void);


    DISALLOW_COPY_AND_ASSIGN(HavanaStream_c);
};

#endif
