/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

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

Source file name : backend.h - player access points
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
31-Jan-07   Created                                         Julian

************************************************************************/

#ifndef H_BACKEND
#define H_BACKEND

#include <linux/kthread.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include "linux/dvb/stm_ioctls.h"

#include "backend_ops.h"
#include "dvb_module.h"

#define MAX_STREAMS_PER_PLAYBACK        8
#define MAX_PLAYBACKS                   4

#define TRANSPORT_PACKET_SIZE           188
#define BLUERAY_PACKET_SIZE             (TRANSPORT_PACKET_SIZE+sizeof(unsigned int))
#define CONTINUITY_COUNT_MASK           0x0f000000
#define CONTINUITY_COUNT( H )           ((H & CONTINUITY_COUNT_MASK) >> 24)
#define PID_HIGH_MASK                   0x00001f00
#define PID_LOW_MASK                    0x00ff0000
#define PID( H )                        ((H & PID_HIGH_MASK) | ((H & PID_LOW_MASK) >> 16))

#define STREAM_HEADER_TYPE              0
#define STREAM_HEADER_FLAGS             1
#define STREAM_HEADER_SIZE              4
#define STREAM_LAST_PACKET_FLAG         0x01
#define AUDIO_STREAM_BUFFER_SIZE        (200*TRANSPORT_PACKET_SIZE)
#define VIDEO_STREAM_BUFFER_SIZE        (200*TRANSPORT_PACKET_SIZE)
#define DEMUX_BUFFER_SIZE               (512*TRANSPORT_PACKET_SIZE)

#define STREAM_INCOMPLETE               1
#define AUDIO_INCOMPLETE                (AUDIO_PAUSED+1)
#define VIDEO_INCOMPLETE                (VIDEO_FREEZED+1)

#define CodeToInteger(a,b,c,d)          ((a << 0) | (b << 8) | (c << 16) | (d << 24))

/*      Debug printing macros   */

#ifndef ENABLE_BACKEND_DEBUG
#define ENABLE_BACKEND_DEBUG            0
#endif

#define BACKEND_DEBUG(fmt, args...)  ((void) (ENABLE_BACKEND_DEBUG && \
                                            (printk("LinuxDvb:%s: " fmt, __FUNCTION__, ##args), 0)))

/* Output trace information off the critical path */
#define BACKEND_TRACE(fmt, args...)  (printk("LinuxDvb:%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define BACKEND_ERROR(fmt, args...)  (printk("ERROR:LinuxDvb:%s: " fmt, __FUNCTION__, ##args))

/* The stream context provides details on a single audio or video stream */
struct StreamContext_s
{
    struct mutex                Lock;
    struct DeviceContext_s*     Device;                 /* Linux DVB device bundle giving control and data */

    stream_handle_t             Handle;                 /* Opaque handle to access backend actions */
    /* Stream functions */
    int                       (*Inject)        (stream_handle_t         Stream,
                                                const unsigned char*    Data,
                                                unsigned int            DataLength);
    int                       (*InjectPacket)  (stream_handle_t         Stream,
                                                const unsigned char*    Data,
                                                unsigned int            DataLength,
                                                bool                    PresentationTimeValid,
                                                unsigned long long      PresentationTime);
    int                       (*Delete)        (playback_handle_t       Playback,
                                                stream_handle_t         Stream);


    unsigned int                BufferLength;
    unsigned char*              Buffer;
    unsigned int                DataToWrite;

#if defined (USE_INJECTION_THREAD)
    struct task_struct*         InjectionThread;        /* Injection thread management */
    unsigned int                Injecting;
    unsigned int                ThreadTerminated;
    wait_queue_head_t           BufferEmpty;            /*! Queue to wake up any waiters */
    struct semaphore            DataReady;
#endif
};

/*
   The playback context keeps track of the bundle of streams plyaing synchronously
   Every stream or demux added to the bundle ups the usage count. Every one removed reduces it.
   The playback can be deleted when the count returns to zero
*/
struct PlaybackContext_s
{
    struct mutex                Lock;
    playback_handle_t           Handle;
    unsigned int                UsageCount;
};


static inline int StreamBufferFree   (struct StreamContext_s*         Stream)
{
    return (Stream->DataToWrite == 0);
}

/* Entry point list */

int DvbBackendInit                         (void);
int DvbBackendDelete                       (void);

int DvbPlaybackCreate                      (struct PlaybackContext_s**      PlaybackContext);
int DvbPlaybackDelete                      (struct PlaybackContext_s*       Playback);
int DvbPlaybackAddStream                   (struct PlaybackContext_s*       Playback,
                                            char*                           Media,
                                            char*                           Format,
                                            char*                           Encoding,
                                            unsigned int                    DemuxId,
                                            unsigned int                    SurfaceId,
                                            struct StreamContext_s**        Stream);
int DvbPlaybackRemoveStream                (struct PlaybackContext_s*       Playback,
                                            struct StreamContext_s*         Stream);
int DvbPlaybackSetSpeed                    (struct PlaybackContext_s*       Playback,
                                            unsigned int                    Speed);
int DvbPlaybackGetSpeed                    (struct PlaybackContext_s*       Playback,
                                            unsigned int*                   Speed);
int DvbPlaybackSetNativePlaybackTime       (struct PlaybackContext_s*       Playback,
                                            unsigned long long              NativeTime,
                                            unsigned long long              SystemTime);
int DvbPlaybackSetOption                   (struct PlaybackContext_s*       Playback,
                                            play_option_t                   Option,
                                            unsigned int                    Value);
int DvbPlaybackGetPlayerEnvironment        (struct PlaybackContext_s*       Playback,
                                            playback_handle_t*              playerplayback);
int DvbPlaybackSetClockDataPoint           (struct PlaybackContext_s*       Playback,
                                            dvb_clock_data_point_t*         ClockData);

int DvbStreamEnable                        (struct StreamContext_s*         Stream,
                                            unsigned int                    Enable);
int DvbStreamSetId                         (struct StreamContext_s*         Stream,
                                            unsigned int                    DemuxId,
                                            unsigned int                    Id);
int DvbStreamChannelSelect                 (struct StreamContext_s*         Stream,
                                            channel_select_t                Channel);
int DvbStreamSetOption                     (struct StreamContext_s*         Stream,
                                            play_option_t                   Option,
                                            unsigned int                    Value);
int DvbStreamGetOption                     (struct StreamContext_s*         Stream,
                                            play_option_t                   Option,
                                            unsigned int*                   Value);
int DvbStreamGetPlayInfo                   (struct StreamContext_s*         Stream,
                                            struct play_info_s*             PlayInfo);
int DvbStreamDrain                         (struct StreamContext_s*         Stream,
                                            unsigned int                    Discard);
int DvbStreamDiscontinuity                 (struct StreamContext_s*         Stream,
                                            discontinuity_t                 Discontinuity);
int DvbStreamStep                          (struct StreamContext_s*         Stream);
int DvbStreamSwitch                        (struct StreamContext_s*         Stream,
                                            char*                           Format,
                                            char*                           Encoding);
int DvbStreamGetDecodeBuffer               (struct StreamContext_s*         Stream,
                                            buffer_handle_t*                Buffer,
                                            unsigned char**                 Data,
                                            unsigned int                    Format,
                                            unsigned int                    DimensionCount,
                                            unsigned int                    Dimensions[],
                                            unsigned int*                   Index,
                                            unsigned int*                   Stride);
int DvbStreamReturnDecodeBuffer            (struct StreamContext_s*         Stream,
                                            buffer_handle_t*                Buffer);
int DvbStreamGetDecodeBufferPoolStatus     (struct StreamContext_s*         Stream,
                                            unsigned int*                   BuffersInPool,
                                            unsigned int*                   BuffersWithNonZeroReferenceCount);
int DvbStreamSetOutputWindow               (struct StreamContext_s*         Stream,
                                            unsigned int                    X,
                                            unsigned int                    Y,
                                            unsigned int                    Width,
                                            unsigned int                    Height);
int DvbStreamSetInputWindow                (struct StreamContext_s*         Stream,
                                            unsigned int                    X,
                                            unsigned int                    Y,
                                            unsigned int                    Width,
                                            unsigned int                    Height);
int DvbStreamSetPlayInterval               (struct StreamContext_s*         Stream,
                                            dvb_play_interval_t*            PlayInterval);

int DvbStreamInject                        (struct StreamContext_s*         Stream,
                                            const unsigned char*            Buffer,
                                            unsigned int                    Length);

int DvbStreamInjectPacket                  (struct StreamContext_s*         Stream,
                                            const unsigned char*            Buffer,
                                            unsigned int                    Length,
                                            bool                            PresentationTimeValid,
                                            unsigned long long              PresentationTime);

int DvbStreamGetFirstBuffer                (struct StreamContext_s*         Stream,
                                            const char __user*              Buffer,
                                            unsigned int                    Length);
int DvbStreamIdentifyAudio                (struct StreamContext_s*         Stream,
                                            unsigned int*                   Id);
int DvbStreamIdentifyVideo                 (struct StreamContext_s*         Stream,
                                            unsigned int*                   Id);
int DvbStreamGetPlayerEnvironment          (struct StreamContext_s*         Stream,
                                            playback_handle_t*              playerplayback,
                                            stream_handle_t*                playerstream);
static inline int DvbPlaybackAddDemux      (struct PlaybackContext_s*       Playback,
                                            unsigned int                    DemuxId,
                                            struct StreamContext_s**        Demux)
{
    return DvbPlaybackAddStream (Playback, NULL, NULL, NULL, DemuxId, DemuxId, Demux);
}

static inline int DvbPlaybackRemoveDemux   (struct PlaybackContext_s*       Playback,
                                            struct StreamContext_s*         Demux)
{
    return DvbPlaybackRemoveStream (Playback, Demux);
}



stream_event_signal_callback   DvbStreamRegisterEventSignalCallback    (struct StreamContext_s*         Stream,
                                                                        struct DeviceContext_s*         Context,
                                                                        stream_event_signal_callback    CallBack);
int DvbDisplayDelete                       (char*                           Media,
                                            unsigned int                    SurfaceId);

int DvbDisplaySynchronize                  (char*                           Media,
                                            unsigned int                    SurfaceId);

#endif
