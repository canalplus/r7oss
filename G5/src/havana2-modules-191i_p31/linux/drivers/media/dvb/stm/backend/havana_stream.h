/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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

Source file name : havana_stream.h derived from havana_player.h
Author :           Nick

Definition of the implementation of stream module for havana.


Date        Modification                                    Name
----        ------------                                    --------
14-Feb-07   Created                                         Julian

************************************************************************/

#ifndef H_HAVANA_STREAM
#define H_HAVANA_STREAM

#include "osdev_user.h"
#include "backend_ops.h"
#include "player.h"
#include "player_types.h"
#include "buffer_generic.h"
#include "player_generic.h"
#include "havana_playback.h"

/*      Debug printing macros   */
#ifndef ENABLE_STREAM_DEBUG
#define ENABLE_STREAM_DEBUG             1
#endif

#define STREAM_DEBUG(fmt, args...)      ((void) (ENABLE_STREAM_DEBUG && \
                                            (report(severity_note, "HavanaStream_c::%s: " fmt, __FUNCTION__, ##args), 0)))

/* Output trace information off the critical path */
#define STREAM_TRACE(fmt, args...)      (report(severity_note, "HavanaStream_c::%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define STREAM_ERROR(fmt, args...)      (report(severity_error, "HavanaStream_c::%s: " fmt, __FUNCTION__, ##args))

#define STREAM_SPECIFIC_EVENTS          (EventSourceFrameRateChangeManifest | EventSourceSizeChangeManifest |   \
                                         EventFailedToDecodeInTime | EventFailedToDeliverDataInTime |           \
                                         EventTrickModeDomainChange | EventVsyncOffsetMeasured |                \
                                         EventFirstFrameManifested |  EventStreamUnPlayable |                   \
                                         EventOutputSizeChangeManifest | EventFatalHardwareFailure)

#define MAX_SOURCE_WIDTH                1920
#define MAX_SOURCE_HEIGHT               1088
#define MAX_SCALING_FACTOR              2
#define SCALING_THRESHOLD_WIDTH         (MAX_SOURCE_WIDTH/MAX_SCALING_FACTOR)
#define SCALING_THRESHOLD_HEIGHT        (MAX_SOURCE_HEIGHT/MAX_SCALING_FACTOR)

/* Position and size of PIP Window - based on the display size */
#define PIP_FACTOR_N                            5
#define PIP_FACTOR_D                            16
#define PIP_X_N                                (PIP_FACTOR_D-PIP_FACTOR_N)
#define PIP_X_OFFSET                            48
#define PIP_Y_OFFSET                            32


#define DVP_CODED_FRAME_COUNT           64
#define DVP_MAXIMUM_FRAME_SIZE          128
#define DVP_FRAME_MEMORY_SIZE           (DVP_CODED_FRAME_COUNT * DVP_MAXIMUM_FRAME_SIZE)
#define DVP_MEMORY_PARTITION            "BPA2_Region0"

#if defined (CONFIG_CPU_SUBTYPE_STX7200)

#define FIRST_AUDIO_COPROCESSOR '3'
#define NUMBER_AUDIO_COPROCESSORS 2

#else

// on some systems the defaults burnt into the codec are sufficient
#define FIRST_AUDIO_COPROCESSOR '\0'
#define NUMBER_AUDIO_COPROCESSORS 0 /* UNKNOWN */

#endif

//#define CLONING_MANIFESTOR_TEST
#ifdef CLONING_MANIFESTOR_TEST
#include "manifestor_clone.h"
#include "manifestor_clone_dummy.h"
#endif

/// Player wrapper class responsible for managing a stream.
class HavanaStream_c
{
private:
    unsigned int                StreamId;
    unsigned int                DemuxId;
    unsigned int                TransformerId;
    OS_Mutex_t                  InputLock;
    bool                        LockInitialised;

    bool                        Manifest;

    PlayerStreamType_t          PlayerStreamType;
    PlayerStream_t              PlayerStream;
    DemultiplexorContext_t      DemuxContext;

    class HavanaPlayer_c*       HavanaPlayer;

    class Player_c*             Player;
    class Demultiplexor_c*      Demultiplexor;
    class Collator_c*           Collator;
    class FrameParser_c*        FrameParser;
    class Codec_c*              Codec;
    class OutputTimer_c*        OutputTimer;
    class Manifestor_c*         Manifestor;
    class BufferPool_c*         DecodeBufferPool;

#ifdef CLONING_MANIFESTOR_TEST
    class Manifestor_Clone_c*	CloningManifestor;
    class Manifestor_c*		CloneManifestor;
#endif

    class Collator_c*           ReplacementCollator;
    class FrameParser_c*        ReplacementFrameParser;
    class Codec_c*              ReplacementCodec;

    PlayerPlayback_t            PlayerPlayback;

    stream_event_signal_callback        EventSignalCallback;
    context_handle_t            CallbackContext;
    struct stream_event_s       StreamEvent;

public:

                                HavanaStream_c                 (void);
                               ~HavanaStream_c                 (void);

    HavanaStatus_t              Init                           (class HavanaPlayer_c*           HavanaPlayer,
                                                                class Player_c*                 Player,
                                                                PlayerPlayback_t                PlayerPlayback,
                                                                char*                           Media,
                                                                char*                           Format,
                                                                char*                           Encoding,
                                                                unsigned int                    SurfaceId);
    HavanaStatus_t              InjectData                     (const unsigned char*            Data,
                                                                unsigned int                    DataLength);
    HavanaStatus_t              InjectDataPacket               (const unsigned char*            Data,
                                                                unsigned int                    DataLength,
                                                                bool                            PlaybackTimeValid,
                                                                unsigned long long              PlaybackTime);
    HavanaStatus_t              Discontinuity                  (bool                            ContinuousReverse,
                                                                bool                            SurplusData);
    HavanaStatus_t              Drain                          (bool                            Discard);
    HavanaStatus_t              Enable                         (bool                            Manifest);
    HavanaStatus_t              SetId                          (unsigned int                    DemuxId,
                                                                unsigned int                    Id);
    HavanaStatus_t              ChannelSelect                  (channel_select_t                Channel);
    HavanaStatus_t              SetOption                      (play_option_t                   Option,
                                                                unsigned int                    Value);
    HavanaStatus_t              GetOption                      (play_option_t                   Option,
                                                                unsigned int*                   Value);
    HavanaStatus_t              MapOption                      (play_option_t                   Option,
                                                                PlayerPolicy_t*                 PlayerPolicy);
    HavanaStatus_t              Step                           (void);
    HavanaStatus_t              SetOutputWindow                (unsigned int                    X,
                                                                unsigned int                    Y,
                                                                unsigned int                    Width,
                                                                unsigned int                    Height);
    HavanaStatus_t              SetInputWindow                 (unsigned int                    X,
                                                                unsigned int                    Y,
                                                                unsigned int                    Width,
                                                                unsigned int                    Height);
    HavanaStatus_t              SetPlayInterval                (play_interval_t*                PlayInterval);
    HavanaStatus_t              CheckEvent                     (struct PlayerEventRecord_s*     PlayerEvent);
    HavanaStatus_t              GetPlayInfo                    (struct play_info_s*             PlayInfo);
    HavanaStatus_t              Switch                         (char*                           Format,
                                                                char*                           Encoding);
    HavanaStatus_t              GetDecodeBuffer                (buffer_handle_t*                DecodeBuffer,
                                                                unsigned char**                 Data,
                                                                unsigned int                    Format,
                                                                unsigned int                    DimensionCount,
                                                                unsigned int                    Dimensions[],
                                                                unsigned int*                   Index,
                                                                unsigned int*                   Stride);
    HavanaStatus_t              ReturnDecodeBuffer             (buffer_handle_t                 DecodeBuffer);
    HavanaStatus_t              GetDecodeBufferPoolStatus      (unsigned int* BuffersInPool, 
                                                                unsigned int* BuffersWithNonZeroReferenceCount);
    HavanaStatus_t              GetOutputWindow                (unsigned int*                   X,
                                                                unsigned int*                   Y,
                                                                unsigned int*                   Width,
                                                                unsigned int*                   Height);
    HavanaStatus_t              GetPlayerEnvironment           (PlayerPlayback_t*               PlayerPlayback,
                                                                PlayerStream_t*                 PlayerStream);
    stream_event_signal_callback        RegisterEventSignalCallback    (context_handle_t                Context,
                                                                        stream_event_signal_callback    EventSignalCallback);
};

#endif

