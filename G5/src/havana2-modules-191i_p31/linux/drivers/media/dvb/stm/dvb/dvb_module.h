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

Source file name : dvb_module.h - streamer device definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-03   Created                                         Julian

************************************************************************/

#ifndef H_DVB_MODULE
#define H_DVB_MODULE

#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include "linux/dvb/stm_ioctls.h"

#include "dvbdev.h"
#include "dmxdev.h"
#include "dvb_demux.h"

#include "backend.h"
#include "dvb_video.h"
#include "dvb_audio.h"
#include "dvp.h"

#ifndef false
#define false   0
#define true    1
#endif

/* Enable/disable debug-level macros.
 *
 * This really should be off by default (for checked in versions of the player) if you
 * *really* need your message to hit the console every time then that is what DVB_TRACE()
 * is for.
 */
#ifndef ENABLE_DVB_DEBUG
#define ENABLE_DVB_DEBUG                0
#endif

#define DVB_DEBUG(fmt, args...)         ((void) (ENABLE_DVB_DEBUG && \
                                                 (printk(KERN_INFO "%s: " fmt, __FUNCTION__,##args), 0)))

/* Output trace information off the critical path */
#define DVB_TRACE(fmt, args...)         (printk(KERN_NOTICE "%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define DVB_ERROR(fmt, args...)         (printk(KERN_CRIT "ERROR in %s: " fmt, __FUNCTION__, ##args))

#define DVB_ASSERT(x) do if(!(x)) printk(KERN_CRIT "%s: Assertion '%s' failed at %s:%d\n", \
                                         __FUNCTION__, #x, __FILE__, __LINE__); while(0)

#define DVB_MAX_DEVICES_PER_ADAPTER     4

struct DemuxBuffer_s
{
    unsigned int                OutPtr;
    unsigned int                InPtr;
    struct task_struct*         InjectionThread;
    unsigned int                DemuxInjecting;
};

struct Rectangle_s
{
    unsigned int                X;
    unsigned int                Y;
    unsigned int                Width;
    unsigned int                Height;
};

struct DeviceContext_s
{
    unsigned int                Id;


    struct dvb_device*          AudioDevice;
    struct audio_status         AudioState;
    unsigned int                AudioId;
    audio_encoding_t            AudioEncoding;
    struct StreamContext_s*     AudioStream;
    unsigned int                AudioOpenWrite;
    struct mutex                AudioWriteLock;
    struct mutex*               ActiveAudioWriteLock;
    unsigned int                AudioCaptureStatus;
    audio_play_interval_t       AudioPlayInterval;

    struct dvb_device*          VideoDevice;
    struct video_status         VideoState;
    unsigned int                VideoId;
    video_encoding_t            VideoEncoding;
    video_size_t                VideoSize;
    unsigned int                FrameRate;
    struct StreamContext_s*     VideoStream;
    unsigned int                VideoOpenWrite;
    struct VideoEvent_s         VideoEvents;
    struct mutex                VideoWriteLock;
    struct mutex*               ActiveVideoWriteLock;
    unsigned int                VideoCaptureStatus;
    video_play_interval_t       VideoPlayInterval;

    unsigned int                PlayOption[DVB_OPTION_MAX];
    unsigned int                PlayValue[DVB_OPTION_MAX];
    struct Rectangle_s          VideoOutputWindow;
    struct Rectangle_s          VideoInputWindow;
    struct Ratio_s              PixelAspectRatio;


    struct DeviceContext_s*     DemuxContext;           /* av can be wired to different demux - default is self */
    struct DeviceContext_s*     SyncContext;            /* av can be synchronised to a different device - default is self */

    struct dvb_demux            DvbDemux;
    struct dmxdev               DmxDevice;
    struct dmx_frontend         MemoryFrontend;

    struct StreamContext_s*     DemuxStream;

    struct PlaybackContext_s*   Playback;
    stream_type_t               StreamType;
    int                         PlaySpeed;

    struct dvb_device*          CaDevice;
    unsigned char *dvr_in;
    unsigned char *dvr_out;
    unsigned int EncryptionOn;

    unsigned int StartOffset;
    unsigned int EndOffset;

    struct DvbContext_s*        DvbContext;
};

struct DvbContext_s
{
    void*                       PtiPrivate;

    struct dvb_device           DvbDevice;
    struct dvb_adapter          DvbAdapter;

    struct mutex                Lock;

    struct DeviceContext_s      DeviceContext[DVB_MAX_DEVICES_PER_ADAPTER];

};

long DvbGenericUnlockedIoctl(struct file *, unsigned int, unsigned long);

#endif
