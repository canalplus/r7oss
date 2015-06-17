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

Source file name : dvb_video.c
Author :           Julian

Implementation of linux dvb video device

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-05   Created                                         Julian

************************************************************************/

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/bpa2.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>

#include "dvb_module.h"
#include "dvb_video.h"
#include "backend.h"

#include "dvb_v4l2.h"

/*{{{  prototypes*/
static int VideoOpen                    (struct inode           *Inode,
                                         struct file            *File);
static int VideoRelease                 (struct inode           *Inode,
                                         struct file            *File);
static int VideoIoctl                   (struct inode           *Inode,
                                         struct file            *File,
                                         unsigned int            IoctlCode,
                                         void                   *ParamAddress);
static ssize_t VideoWrite               (struct file            *File,
                                         const char __user      *Buffer,
                                         size_t                  Count,
                                         loff_t                 *ppos);
static unsigned int VideoPoll           (struct file*            File,
                                         poll_table*             Wait);

static int VideoIoctlSetDisplayFormat   (struct DeviceContext_s* Context,
                                         unsigned int            Format);
static int VideoIoctlSetFormat          (struct DeviceContext_s* Context,
                                         unsigned int            Format);
static int VideoIoctlCommand            (struct DeviceContext_s* Context,
                                         struct video_command*   VideoCommand);
int VideoIoctlSetPlayInterval           (struct DeviceContext_s* Context,
                                         video_play_interval_t*  PlayInterval);

static void VideoSetEvent               (struct DeviceContext_s* Context,
                                         struct stream_event_s*  Event);
/*}}}*/

/*{{{  static data*/
static struct file_operations VideoFops =
{
        owner:          THIS_MODULE,
        write:          VideoWrite,
        unlocked_ioctl: DvbGenericUnlockedIoctl,
        open:           VideoOpen,
        release:        VideoRelease,
        poll:           VideoPoll
};

static struct dvb_device VideoDevice =
{
        priv:            NULL,
        users:           8,
        readers:         7,
        writers:         1,
        fops:            &VideoFops,
        kernel_ioctl:    VideoIoctl,
};

/* Assign encodings to backend id's using the actual ID's rather on relying on the correct order */

static char* VideoContent[]     =
{
  [VIDEO_ENCODING_AUTO]     = BACKEND_AUTO_ID,
  [VIDEO_ENCODING_MPEG1]    = BACKEND_MPEG1_ID,
  [VIDEO_ENCODING_MPEG2]    = BACKEND_MPEG2_ID,
  [VIDEO_ENCODING_MJPEG]    = BACKEND_MJPEG_ID,
  [VIDEO_ENCODING_DIVX3]    = BACKEND_DIVX3_ID,
  [VIDEO_ENCODING_DIVX4]    = BACKEND_DIVX4_ID,
  [VIDEO_ENCODING_DIVX5]    = BACKEND_DIVX5_ID,  
  [VIDEO_ENCODING_MPEG4P2]  = BACKEND_MPEG4P2_ID,
  [VIDEO_ENCODING_H264]     = BACKEND_H264_ID,
  [VIDEO_ENCODING_WMV]      = BACKEND_WMV_ID,
  [VIDEO_ENCODING_VC1]      = BACKEND_VC1_ID,    
  [VIDEO_ENCODING_RAW]      = BACKEND_RAW_ID,
  [VIDEO_ENCODING_H263]     = BACKEND_H263_ID,
  [VIDEO_ENCODING_FLV1]     = BACKEND_FLV1_ID,
  [VIDEO_ENCODING_VP6]      = BACKEND_VP6_ID, 
  [VIDEO_ENCODING_RMV]      = BACKEND_RMV_ID,
  [VIDEO_ENCODING_DIVXHD]   = BACKEND_DIVXHD_ID,
  [VIDEO_ENCODING_AVS]      = BACKEND_AVS_ID,
  [VIDEO_ENCODING_VP3]      = BACKEND_VP3_ID,
  [VIDEO_ENCODING_THEORA]   = BACKEND_THEORA_ID,
  [VIDEO_ENCODING_COMPOCAP] = BACKEND_CAP_ID,
  [VIDEO_ENCODING_NONE]     = BACKEND_NONE_ID,
  [VIDEO_ENCODING_PRIVATE]  = BACKEND_DVP_ID
};

#define PES_VIDEO_START_CODE                            0xe0
#define MPEG2_SEQUENCE_END_CODE                         0xb7
#if defined (INJECT_WITH_PTS)
static const unsigned char Mpeg2VideoPesHeader[]  =
{
    0x00, 0x00, 0x01, PES_VIDEO_START_CODE,             /* header start code */
    0x00, 0x00,                                         /* Length word */
    0x80,                                               /* Marker (10), Scrambling control, Priority, Alignment, Copyright, Original/Copy */
    0x80,                                               /* Pts present, ESCR, ES_rate, DSM_trick, Add_copy, CRC_flag, Extension */
    0x05,                                               /* Pes header data length (pts) */
    0x21,                                               /* Marker bit + Pts top 3 bits + marker bit */
    0x00, 0x01,                                         /* next 15 bits + marker bit */
    0x00, 0x01                                          /* bottom 15 bits + marker bit */
};
#else
static const unsigned char Mpeg2VideoPesHeader[]  =
{
    0x00, 0x00, 0x01, PES_VIDEO_START_CODE,             /* header start code */
    0x00, 0x00,                                         /* Length word */
    0x80,                                               /* Marker (10), Scrambling control, Priority, Alignment, Copyright, Original/Copy */
    0x00,                                               /* Pts present, ESCR, ES_rate, DSM_trick, Add_copy, CRC_flag, Extension */
    0x00,                                               /* Pes header data length (no pts) */
};
#endif
static const unsigned char Mpeg2SequenceEnd[]  =
{
    0x00, 0x00, 0x01, MPEG2_SEQUENCE_END_CODE,          /* sequence end code */
    0xff, 0xff, 0xff, 0xff,                             /* padding */
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
};
/*}}}*/
/*{{{  Videoinit*/
struct dvb_device* VideoInit (struct DeviceContext_s* Context)
{
    int i;

    Context->VideoState.video_blank             = 0;
    Context->VideoState.play_state              = VIDEO_STOPPED;
    Context->VideoState.stream_source           = VIDEO_SOURCE_DEMUX;
    Context->VideoState.video_format            = VIDEO_FORMAT_4_3;
    Context->VideoState.display_format          = VIDEO_CENTER_CUT_OUT;

    Context->VideoSize.w                        = 0;
    Context->VideoSize.h                        = 0;
    Context->VideoSize.aspect_ratio             = 0;

    Context->FrameRate                          = 0;

    Context->VideoId                            = DEMUX_INVALID_ID;     /* CodeToInteger('v','i','d','s'); */
    Context->VideoEncoding                      = VIDEO_ENCODING_AUTO;
    Context->VideoStream                        = NULL;
    Context->VideoOpenWrite                     = 0;

    for (i = 0; i < DVB_OPTION_MAX; i++)
        Context->PlayOption[i]                  = DVB_OPTION_VALUE_INVALID;

    Context->VideoOutputWindow.X                = 0;
    Context->VideoOutputWindow.Y                = 0;
    Context->VideoOutputWindow.Width            = 0;
    Context->VideoOutputWindow.Height           = 0;

    Context->VideoInputWindow.X                 = 0;
    Context->VideoInputWindow.Y                 = 0;
    Context->VideoInputWindow.Width             = 0;
    Context->VideoInputWindow.Height            = 0;

    Context->VideoPlayInterval.start            = DVB_TIME_NOT_BOUNDED;
    Context->VideoPlayInterval.end              = DVB_TIME_NOT_BOUNDED;

    Context->PixelAspectRatio.Numerator         = 1;
    Context->PixelAspectRatio.Denominator       = 1;

    init_waitqueue_head (&Context->VideoEvents.WaitQueue);
    mutex_init (&Context->VideoEvents.Lock);
    Context->VideoEvents.Write                  = 0;
    Context->VideoEvents.Read                   = 0;
    Context->VideoEvents.Overflow               = 0;

    mutex_init   (&(Context->VideoWriteLock));
    Context->ActiveVideoWriteLock               = &(Context->VideoWriteLock);

    return &VideoDevice;
}
/*}}}*/
/*{{{  PlaybackInit*/
int PlaybackInit (struct DeviceContext_s* Context)
{
    if (Context->Playback == NULL)
        return -ENODEV;

    /* The master clock must be set before the first stream is created */
    if (Context->PlayOption[DVB_OPTION_MASTER_CLOCK] != DVB_OPTION_VALUE_INVALID)
    {
        /* reset the flag to indicate it has been set */
        Context->PlayOption[DVB_OPTION_MASTER_CLOCK]    = DVB_OPTION_VALUE_INVALID;

        return DvbPlaybackSetOption (Context->Playback, DVB_OPTION_MASTER_CLOCK, Context->PlayValue[DVB_OPTION_MASTER_CLOCK]);
    }
    return 0;
}
/*}}}*/
/*{{{  VideoSetOutputWindow*/
int VideoSetOutputWindow (struct DeviceContext_s*       Context,
                          unsigned int                  Left,
                          unsigned int                  Top,
                          unsigned int                  Width,
                          unsigned int                  Height)
{
    struct DvbContext_s*        DvbContext      = Context->DvbContext;
    int                         Result          = 0;

    mutex_lock (&(DvbContext->Lock));

    if (Context->VideoStream != NULL)
        Result  = DvbStreamSetOutputWindow (Context->VideoStream, Left, Top, Width, Height);

    /* Always update the Context and not only when there is no played media */
    Context->VideoOutputWindow.X            = Left;
    Context->VideoOutputWindow.Y            = Top;
    Context->VideoOutputWindow.Width        = Width;
    Context->VideoOutputWindow.Height       = Height;

    mutex_unlock (&(DvbContext->Lock));

    return Result;
}
/*}}}*/
/*{{{  VideoSetInputWindow*/
int VideoSetInputWindow  (struct DeviceContext_s*       Context,
                          unsigned int                  Left,
                          unsigned int                  Top,
                          unsigned int                  Width,
                          unsigned int                  Height)
{
    struct DvbContext_s*        DvbContext      = Context->DvbContext;
    int                         Result          = 0;

    DVB_DEBUG ("%dx%d at %d,%d\n", Width, Height, Left, Top);
    mutex_lock (&(DvbContext->Lock));

    if (Context->VideoStream != NULL)
        Result  = DvbStreamSetInputWindow (Context->VideoStream, Left, Top, Width, Height);
    else
    {
        Context->VideoInputWindow.X             = Left;
        Context->VideoInputWindow.Y             = Top;
        Context->VideoInputWindow.Width         = Width;
        Context->VideoInputWindow.Height        = Height;
    }
    mutex_unlock (&(DvbContext->Lock));

    return Result;
}
/*}}}*/
/*{{{  VideoGetPixelAspectRatio*/
int VideoGetPixelAspectRatio   (struct DeviceContext_s* Context,
                                unsigned int*           Numerator,
                                unsigned int*           Denominator)
{
    /*DVB_DEBUG("\n");*/

    *Numerator          = Context->PixelAspectRatio.Numerator;
    *Denominator        = Context->PixelAspectRatio.Denominator;

    return 0;
}
/*}}}*/

/*{{{  Ioctls*/
/*{{{  VideoIoctlStop*/
int VideoIoctlStop (struct DeviceContext_s* Context, unsigned int Mode)
{
    int Result  = 0;

    DVB_DEBUG ("(video%d)\n", Context->Id);
    /*if (Context->VideoState.play_state == VIDEO_PLAYING)*/
    if (Context->VideoStream != NULL)
    {
        struct mutex*   WriteLock       = Context->ActiveVideoWriteLock;

        /*DVB_DEBUG ("Mode = %x, Blank = %x\n", Mode, Context->VideoState.video_blank);*/
        DvbStreamSetOption (Context->VideoStream, PLAY_OPTION_VIDEO_BLANK, Mode ? PLAY_OPTION_VALUE_ENABLE : PLAY_OPTION_VALUE_DISABLE);

        /* Discard previously injected data to free the lock. */
        DvbStreamDrain (Context->VideoStream, true);

        /*mutex_lock (WriteLock);*/
        if (mutex_lock_interruptible (WriteLock) != 0)
            return -ERESTARTSYS;                /* Give up for now.  The stream will be removed later by the release */

        Result  = DvbPlaybackRemoveStream (Context->Playback, Context->VideoStream);
        if (Result == 0)
        {
            Context->VideoStream                = NULL;
            Context->VideoState.play_state      = VIDEO_STOPPED;
            Context->ActiveVideoWriteLock       = &(Context->VideoWriteLock);
            /*VideoInit (Context);*/
        }
        mutex_unlock (WriteLock);
    }
    DVB_DEBUG("Play state = %d\n", Context->VideoState.play_state);

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlPlay*/
int VideoIoctlPlay (struct DeviceContext_s* Context)
{
    int Result = 0;
    sigset_t newsigs, oldsigs;

    DVB_DEBUG ("(video%d)\n", Context->Id);
    if ((Context->VideoState.play_state == VIDEO_STOPPED) || (Context->VideoState.play_state == VIDEO_INCOMPLETE))
    {
        if (Context->Playback == NULL)
        {
            /*
               Check to see if we are wired to a demux.  If so the demux should create the playback and we will get
                another play call.  Just exit in this case.  If we are playing from memory we need to create a playback.
            */
            if (Context->VideoState.stream_source == VIDEO_SOURCE_DEMUX)
            {
                if (Context->DemuxContext->Playback == NULL)
                    return 0;
                else
                    Context->Playback   = Context->DemuxContext->Playback;
            }
            else if (Context->SyncContext->Playback == NULL)
            {
                Result      = DvbPlaybackCreate (&Context->Playback);
                if (Result < 0)
                    return Result;
                if (Context->PlaySpeed != DVB_SPEED_NORMAL_PLAY)
                {
                    Result      = VideoIoctlSetSpeed (Context, Context->PlaySpeed);
                    if (Result < 0)
                        return Result;
                }
                Context->SyncContext->Playback  = Context->Playback;
            }
            else
                Context->Playback       = Context->SyncContext->Playback;
        }
        PlaybackInit (Context);

        if ((Context->VideoStream == NULL) || (Context->VideoState.play_state == VIDEO_INCOMPLETE))
        {
            unsigned int DemuxId = (Context->VideoState.stream_source == VIDEO_SOURCE_DEMUX) ? Context->DemuxContext->Id : DEMUX_INVALID_ID;

#if defined(DEFAULT_TO_MPEG2)
            if (Context->VideoEncoding == VIDEO_ENCODING_AUTO)
                Context->VideoEncoding  = VIDEO_ENCODING_MPEG2; /* Video always defaults to MPEG2 if not set by user */
#endif

            /* a signal received in here can cause issues. Turn them off, just for this bit... */
            sigfillset(&newsigs);
            sigprocmask(SIG_BLOCK, &newsigs, &oldsigs);

            Result      = DvbPlaybackAddStream (Context->Playback,
                                                BACKEND_VIDEO_ID,
                                                BACKEND_PES_ID,
                                                VideoContent[Context->VideoEncoding],
                                                DemuxId,
                                                Context->Id,
                                                &Context->VideoStream);

            sigprocmask(SIG_SETMASK, &oldsigs, NULL);

            if (Result == STREAM_INCOMPLETE)
            {
                Context->VideoState.play_state    = VIDEO_INCOMPLETE;
                return 0;
            }
            if (((Context->VideoPlayInterval.start != DVB_TIME_NOT_BOUNDED) || (Context->VideoPlayInterval.end != DVB_TIME_NOT_BOUNDED)) &&
                (Result == 0))
                Result  = VideoIoctlSetPlayInterval (Context, &Context->VideoPlayInterval);
            if (Result == 0)
                Result  = VideoIoctlSetId            (Context, Context->VideoId);
            if (Result == 0)
                Result  = VideoIoctlSetDisplayFormat (Context, (unsigned int)Context->VideoState.display_format);
            if (Result == 0)
                Result  = VideoIoctlSetFormat        (Context, Context->VideoState.video_format);
            if (Result == 0)
                DvbStreamRegisterEventSignalCallback (Context->VideoStream, Context, (stream_event_signal_callback)VideoSetEvent);
            if (Result == 0)
            {
                unsigned int            i;
                struct video_command    VideoCommand;

                memset (&VideoCommand, 0, sizeof (struct video_command));
                VideoCommand.cmd                = VIDEO_CMD_SET_OPTION;
                for (i = 0; i < DVB_OPTION_MAX; i++)
                {
                    if (Context->PlayOption[i] != DVB_OPTION_VALUE_INVALID)
                    {
                        VideoCommand.option.option      = i;
                        VideoCommand.option.value       = Context->PlayValue[i];
                        Context->PlayOption[i]          = DVB_OPTION_VALUE_INVALID;
                        VideoIoctlCommand (Context, &VideoCommand);
                    }
                }
            }
            if ((Result == 0) &&  ((Context->VideoOutputWindow.X != 0)     || (Context->VideoOutputWindow.Y != 0) ||
                                   (Context->VideoOutputWindow.Width != 0) || (Context->VideoOutputWindow.Height != 0)))
                DvbStreamSetOutputWindow (Context->VideoStream, Context->VideoOutputWindow.X,     Context->VideoOutputWindow.Y,
                                                                Context->VideoOutputWindow.Width, Context->VideoOutputWindow.Height);
            if ((Result == 0) &&  ((Context->VideoInputWindow.X != 0)     || (Context->VideoInputWindow.Y != 0) ||
                                   (Context->VideoInputWindow.Width != 0) || (Context->VideoInputWindow.Height != 0)))
                DvbStreamSetInputWindow (Context->VideoStream, Context->VideoInputWindow.X,     Context->VideoInputWindow.Y,
                                                               Context->VideoInputWindow.Width, Context->VideoInputWindow.Height);

            /*
            If we are connected to a demux we will want to use the video write lock of the demux device
            (which could be us).
            */
            if ((Result == 0) && (Context->VideoState.stream_source == VIDEO_SOURCE_DEMUX))
                Context->ActiveVideoWriteLock   = &(Context->DemuxContext->VideoWriteLock);
        }
    }
    else
    {
        /* Play is used implicitly to exit slow motion and fast forward states so
           set speed to times 1 if video is playing or has been frozen */
        Result  = VideoIoctlSetSpeed (Context, DVB_SPEED_NORMAL_PLAY);
    }

    if (Result == 0)
    {
        /*VideoEnable (Context->PlayerContext, true);*/
        Context->VideoState.play_state = VIDEO_PLAYING;
    }

    DVB_DEBUG("(video%d) State = %d\n", Context->Id, Context->VideoState.play_state);
    return Result;
}
/*}}}*/
/*{{{  VideoIoctlFreeze*/
static int VideoIoctlFreeze (struct DeviceContext_s* Context)
{
    int Result  = 0;

    DVB_DEBUG ("(video%d)\n", Context->Id);
    if (Context->VideoState.play_state == VIDEO_PLAYING)
    {
        Result  = VideoIoctlSetSpeed (Context, DVB_SPEED_STOPPED);
        if (Result < 0)
            return Result;
        Context->VideoState.play_state  = VIDEO_FREEZED;
    }
    return 0;
}
/*}}}*/
/*{{{  VideoIoctlContinue*/
static int VideoIoctlContinue (struct DeviceContext_s* Context)
{
    int Result  = 0;

    DVB_DEBUG ("(video%d)\n", Context->Id);
    /* Continue is used implicitly to exit slow motion and fast forward states so
       set speed to times 1 if video is playing or has been frozen */
    if ((Context->VideoState.play_state == VIDEO_FREEZED) || (Context->VideoState.play_state == VIDEO_PLAYING))
    {
        Result  = VideoIoctlSetSpeed (Context, DVB_SPEED_NORMAL_PLAY);
        if (Result < 0)
            return Result;
        Context->VideoState.play_state  = VIDEO_PLAYING;
    }

    return 0;
}
/*}}}*/
/*{{{  VideoIoctlSelectSource*/
static int VideoIoctlSelectSource (struct DeviceContext_s* Context, video_stream_source_t Source)
{
    DVB_DEBUG ("(video%d)\n", Context->Id);
    Context->VideoState.stream_source = Source;
    if (Source == VIDEO_SOURCE_DEMUX)
        Context->StreamType = STREAM_TYPE_TRANSPORT;
    else
        Context->StreamType = STREAM_TYPE_PES;
    DVB_DEBUG("Source = %x\n", Context->VideoState.stream_source);

    return 0;
}
/*}}}*/
/*{{{  VideoIoctlSetBlank*/
static int VideoIoctlSetBlank (struct DeviceContext_s* Context, unsigned int Mode)
{
    int Result  = 0;

    DVB_DEBUG("(video%d) Blank = %d\n", Context->Id, Context->VideoState.video_blank);
    if (Context->VideoStream != NULL)
        Result  = DvbStreamSetOption (Context->VideoStream, PLAY_OPTION_VIDEO_BLANK, Mode ? PLAY_OPTION_VALUE_ENABLE : PLAY_OPTION_VALUE_DISABLE);

    if (Result != 0)
        return Result;

    Context->VideoState.video_blank = (int)Mode;

#ifdef TEST_BLANK
    /* Test to see if immediate blank works */
    if (Context->VideoStream != NULL)
    {
        DVB_DEBUG ("Enable = %d\n", Mode);
        Result  = DvbStreamEnable (Context->VideoStream, Mode);
    }
#endif

    return 0;
}
/*}}}*/
/*{{{  VideoIoctlGetStatus*/
static int VideoIoctlGetStatus (struct DeviceContext_s* Context, struct video_status* Status)
{
    DVB_DEBUG ("(video%d)\n", Context->Id);
    memcpy (Status, &Context->VideoState, sizeof(struct video_status));

    return 0;
}
/*}}}*/
/*{{{  VideoIoctlGetEvent*/
static int VideoIoctlGetEvent (struct DeviceContext_s* Context, struct video_event* VideoEvent, int FileFlags)
{
    int                         Result          = 0;
    struct VideoEvent_s*        EventList       = &Context->VideoEvents;
    struct DvbContext_s*        DvbContext      = Context->DvbContext;

    /*DVB_DEBUG("(video%d) EventList: Read %d, Write %d\n", Context->Id, EventList->Read, EventList->Write);*/
    if (EventList->Overflow != 0)
    {
        EventList->Overflow     = 0;
        return -EOVERFLOW;
    }

    if (EventList->Write == EventList->Read)
    {
        if ((FileFlags & O_NONBLOCK) != 0)
            return -EWOULDBLOCK;

        mutex_unlock (&(DvbContext->Lock));
        Result  = wait_event_interruptible (EventList->WaitQueue, EventList->Write != EventList->Read);
        mutex_lock (&(DvbContext->Lock));
        if (Result != 0)
            return -ERESTARTSYS;
    }

    /*
    mutex_unlock (&(DvbContext->Lock));
    */
    mutex_lock (&EventList->Lock);

    memcpy (VideoEvent, &EventList->Event[EventList->Read], sizeof (struct video_event));
    EventList->Read     = (EventList->Read + 1) % MAX_VIDEO_EVENT;

    mutex_unlock (&EventList->Lock);
    /*
    mutex_lock (&(DvbContext->Lock));
    */

    return 0;

}
/*}}}*/
/*{{{  VideoIoctlGetSize*/
int VideoIoctlGetSize (struct DeviceContext_s* Context, video_size_t* Size)
{
    DVB_DEBUG ("(video%d)\n", Context->Id);
    memcpy (Size, &Context->VideoSize, sizeof(video_size_t));

    return 0;
}
/*}}}*/
/*{{{  VideoIoctlSetDisplayFormat*/
static int VideoIoctlSetDisplayFormat (struct DeviceContext_s* Context, unsigned int Format)
{
    int Result  = 0;

    DVB_DEBUG("(video%d) Display format = %d\n", Context->Id, Format);

    if ((Format < VIDEO_PAN_SCAN) || (Format > VIDEO_ZOOM_4_3))
        return  -EINVAL;

    if (Context->VideoStream != NULL)
        Result  = DvbStreamSetOption (Context->VideoStream, PLAY_OPTION_VIDEO_DISPLAY_FORMAT, Format);

    if (Result != 0)
        return Result;

    Context->VideoState.display_format = (video_displayformat_t)Format;

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlStillPicture*/
static int VideoIoctlStillPicture (struct DeviceContext_s* Context, struct video_still_picture* Still)
{
    int                 Result          = 0;
    discontinuity_t     Discontinuity   = DISCONTINUITY_SKIP | DISCONTINUITY_SURPLUS_DATA;
    unsigned char*      Buffer          = NULL;
    unsigned int        Sync            = true;

    DVB_DEBUG ("(video%d)\n", Context->Id);
#if 0
    /* still picture required for blueray which is ts so we relax this restriction */
    if (Context->VideoState.stream_source == VIDEO_SOURCE_DEMUX)
    {
        DVB_ERROR("Stream source not VIDEO_SOURCE_MEMORY - cannot write I-frame\n");
        return -EPERM;          /* Not allowed to write to device if connected to demux */
    }
#endif

    if (Context->VideoState.play_state == VIDEO_STOPPED)
    {
        DVB_ERROR("Cannot inject still picture before play called\n");
        return -EPERM;
    }

    if (Context->VideoStream == NULL)
    {
        DVB_ERROR ("No video stream exists to be written to (previous VIDEO_PLAY failed?)\n");
        return -ENODEV;
    }


#if 0
    if (!access_ok (VERIFY_READ, Still->iFrame,  Still->size))
#else
    if (Still->iFrame == NULL)
#endif
    {
        DVB_ERROR("Invalid still picture data = %p, size = %d.\n", Still->iFrame,  Still->size);
        return -EFAULT;
    }

    Buffer      = bigphysarea_alloc (sizeof (Mpeg2VideoPesHeader) + Still->size + sizeof (Mpeg2SequenceEnd));
    if (Buffer == NULL)
    {
        DVB_ERROR("Unable to create still picture buffer - insufficient memory\n");
        return -ENOMEM;
    }

    if ((Context->PlaySpeed == 0) || (Context->PlaySpeed == DVB_SPEED_REVERSE_STOPPED))
    {
        Result  = VideoIoctlSetSpeed (Context, DVB_SPEED_NORMAL_PLAY);
        if (Result < 0)
        {
            bigphysarea_free_pages (Buffer);
            return Result;
        }
    }

    /* switch off sync */
    DvbStreamGetOption (Context->VideoStream, PLAY_OPTION_AV_SYNC, &Sync);
    DvbStreamSetOption (Context->VideoStream, PLAY_OPTION_AV_SYNC, PLAY_OPTION_VALUE_DISABLE);

    /*
    Firstly we drain the stream to allow any pictures in the pipeline to be displayed.  This
    is followed by a jump with discard to flush out any partial frames.
    */
    if (mutex_lock_interruptible (Context->ActiveVideoWriteLock) != 0)
        return -ERESTARTSYS;

    DvbStreamDrain (Context->VideoStream, false);
    Discontinuity       = DISCONTINUITY_SKIP | DISCONTINUITY_SURPLUS_DATA;
    DvbStreamDiscontinuity (Context->VideoStream, Discontinuity);

    DVB_DEBUG("Still picture at %p, size = %d.\n", Still->iFrame,  Still->size);

    memcpy (Buffer, Mpeg2VideoPesHeader, sizeof (Mpeg2VideoPesHeader));
    copy_from_user (Buffer + sizeof (Mpeg2VideoPesHeader), Still->iFrame, Still->size);
    memcpy (Buffer + sizeof (Mpeg2VideoPesHeader) + Still->size, Mpeg2SequenceEnd, sizeof (Mpeg2SequenceEnd));

    DvbStreamInject (Context->VideoStream, Buffer, sizeof (Mpeg2VideoPesHeader) + Still->size + sizeof (Mpeg2SequenceEnd));
    Discontinuity       = DISCONTINUITY_SKIP;
    DvbStreamDiscontinuity (Context->VideoStream, Discontinuity);
    DvbStreamDrain (Context->VideoStream, false);

    DvbStreamSetOption (Context->VideoStream, PLAY_OPTION_AV_SYNC, Sync);

    mutex_unlock (Context->ActiveVideoWriteLock);

    bigphysarea_free_pages (Buffer);

    return 0;
}
/*}}}*/
/*{{{  VideoIoctlSetSpeed*/
int VideoIoctlSetSpeed (struct DeviceContext_s* Context, int Speed)
{
    int DirectionChange;
    int Result;

    DVB_DEBUG ("(video%d)\n", Context->Id);
    if (Context->Playback == NULL)
    {
        Context->PlaySpeed      = Speed;
        return 0;
    }

    /* If changing direction we require a write lock */
    DirectionChange     = ((Speed * Context->PlaySpeed) < 0);
    if (DirectionChange)
    {
        /* Discard previously injected data to free the lock. */
        DvbStreamDrain (Context->VideoStream, true);
        if  (mutex_lock_interruptible (Context->ActiveVideoWriteLock) != 0)
            return -ERESTARTSYS;                /* Give up for now. */
    }

    Result      = DvbPlaybackSetSpeed (Context->Playback, Speed);
    if (Result >= 0)
        Result  = DvbPlaybackGetSpeed (Context->Playback, &Context->PlaySpeed);

    /* If changing direction release write lock*/
    if (DirectionChange)
        mutex_unlock (Context->ActiveVideoWriteLock);

    DVB_DEBUG("Speed set to %d\n", Context->PlaySpeed);

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlFastForward*/
static int VideoIoctlFastForward (struct DeviceContext_s* Context, int Frames)
{
    DVB_DEBUG ("(video%d)\n", Context->Id);
    return VideoIoctlSetSpeed (Context, DVB_SPEED_NORMAL_PLAY * (Frames+1));
}
/*}}}*/
/*{{{  VideoIoctlSlowMotion*/
static int VideoIoctlSlowMotion (struct DeviceContext_s* Context, unsigned int Times)
{
    DVB_DEBUG ("(video%d)\n", Context->Id);
    return VideoIoctlSetSpeed (Context, DVB_SPEED_NORMAL_PLAY / (Times+1));
}
/*}}}*/
/*{{{  VideoIoctlGetCapabilities*/
static int VideoIoctlGetCapabilities (struct DeviceContext_s* Context, int* Capabilities)
{
    DVB_DEBUG ("(video%d)\n", Context->Id);
    *Capabilities = VIDEO_CAP_MPEG1 | VIDEO_CAP_MPEG2;
    DVB_DEBUG("Capabilities returned = %x\n", *Capabilities);

    return 0;
}
/*}}}*/
/*{{{  VideoIoctlGetPts*/
static int VideoIoctlGetPts (struct DeviceContext_s* Context, unsigned long long int* Pts)
{
    int                 Result;
    struct play_info_s  PlayInfo;

    Result      = DvbStreamGetPlayInfo (Context->VideoStream, &PlayInfo);
    if (Result != 0)
        return Result;

    *Pts        = PlayInfo.pts;

    DVB_DEBUG ("(video%d) %llu\n", Context->Id, *Pts);
    return Result;
}
/*}}}*/
/*{{{  VideoIoctlGetFrameCount*/
static int VideoIoctlGetFrameCount (struct DeviceContext_s* Context, unsigned long long int* FrameCount)
{
    int                 Result;
    struct play_info_s  PlayInfo;

    /*DVB_DEBUG ("(video%d)\n", Context->Id);*/
    Result      = DvbStreamGetPlayInfo (Context->VideoStream, &PlayInfo);
    if (Result != 0)
        return Result;

    *FrameCount         = PlayInfo.frame_count;

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlGetPlayTime*/
static int VideoIoctlGetPlayTime (struct DeviceContext_s* Context, video_play_time_t* VideoPlayTime)
{
    struct play_info_s  PlayInfo;

    if (Context->VideoStream == NULL)
        return -EINVAL;

    DVB_DEBUG ("(video%d)\n", Context->Id);
    DvbStreamGetPlayInfo (Context->VideoStream, &PlayInfo);

    VideoPlayTime->system_time          = PlayInfo.system_time;
    VideoPlayTime->presentation_time    = PlayInfo.presentation_time;
    VideoPlayTime->pts                  = PlayInfo.pts;

    return 0;
}
/*}}}*/
/*{{{  VideoIoctlGetPlayInfo*/
static int VideoIoctlGetPlayInfo (struct DeviceContext_s* Context, video_play_info_t* VideoPlayInfo)
{
    struct play_info_s  PlayInfo;

    if (Context->VideoStream == NULL)
        return -EINVAL;

    DVB_DEBUG ("(video%d)\n", Context->Id);
    DvbStreamGetPlayInfo (Context->VideoStream, &PlayInfo);

    VideoPlayInfo->system_time          = PlayInfo.system_time;
    VideoPlayInfo->presentation_time    = PlayInfo.presentation_time;
    VideoPlayInfo->pts                  = PlayInfo.pts;
    VideoPlayInfo->frame_count          = PlayInfo.frame_count;

    return 0;
}
/*}}}*/
/*{{{  VideoIoctlSetId*/
int VideoIoctlSetId (struct DeviceContext_s* Context, int Id)
{
    int                 Result  = 0;
    unsigned int        DemuxId = (Context->VideoState.stream_source == VIDEO_SOURCE_DEMUX) ? Context->DemuxContext->Id : DEMUX_INVALID_ID;

    DVB_DEBUG("(video%d) Setting Video Id to 0x%04x, DemuxId = %d\n", Context->Id, Id, DemuxId);

    if (Context->VideoStream != NULL)
        Result  = DvbStreamSetId (Context->VideoStream, DemuxId, Id);

    if (Result != 0)
        return Result;

    Context->VideoId    = Id;

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlClearBuffer*/
static int VideoIoctlClearBuffer (struct DeviceContext_s* Context)
{
    int                 Result  = 0;
    dvb_discontinuity_t Discontinuity   = DVB_DISCONTINUITY_SKIP | DVB_DISCONTINUITY_SURPLUS_DATA;

    DVB_DEBUG ("(video%d)\n", Context->Id);
    /*
    Clear buffer is used to flush the current stream from the pipeline.  This is accomplished
    by draining the stream with discard and indicating that a discontinuity will occur.
    */
    DvbStreamDrain (Context->VideoStream, true);

    if (mutex_lock_interruptible (Context->ActiveVideoWriteLock) != 0)
        return -ERESTARTSYS;

    Result      = DvbStreamDrain (Context->VideoStream, true);
    if (Result == 0)
        Result  = DvbStreamDiscontinuity (Context->VideoStream, Discontinuity);
    mutex_unlock (Context->ActiveVideoWriteLock);

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlSetStreamType*/
static int VideoIoctlSetStreamType (struct DeviceContext_s* Context, unsigned int Type)
{
    DVB_DEBUG("(video%d) Set type to %d\n", Context->Id, Type);

    /*if ((Type < STREAM_TYPE_TRANSPORT) || (Type > STREAM_TYPE_PES))*/
    if ((Type < STREAM_TYPE_NONE) || (Type > STREAM_TYPE_RAW))
        return  -EINVAL;

    Context->StreamType     = (stream_type_t)Type;

    return 0;
}
/*}}}*/
/*{{{  VideoIoctlSetFormat*/
static int VideoIoctlSetFormat (struct DeviceContext_s* Context, unsigned int Format)
{
    int Result  = 0;

    DVB_DEBUG("(video%d) Format = %x\n", Context->Id, Format);

    if ((Format < VIDEO_FORMAT_4_3) || (Format > VIDEO_FORMAT_221_1))
        return  -EINVAL;

    if (Context->VideoStream != NULL)
        Result  = DvbStreamSetOption (Context->VideoStream, PLAY_OPTION_VIDEO_ASPECT_RATIO, Format);

    if (Result != 0)
        return Result;

    Context->VideoState.video_format     = (video_format_t)Format;

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlSetSystem*/
static int VideoIoctlSetSystem (struct DeviceContext_s* Context, video_system_t System)
{
    DVB_ERROR("Not supported\n");
    return -EPERM;
}
/*}}}*/
/*{{{  VideoIoctlSetHighlight*/
static int VideoIoctlSetHighlight (struct DeviceContext_s* Context, video_highlight_t* Highlight)
{
    DVB_ERROR("Not supported\n");
    return -EPERM;
}
/*}}}*/
/*{{{  VideoIoctlSetSpu*/
static int VideoIoctlSetSpu (struct DeviceContext_s* Context, video_spu_t* Spu)
{
    DVB_ERROR("Not supported\n");
    return -EPERM;
}
/*}}}*/
/*{{{  VideoIoctlSetSpuPalette*/
static int VideoIoctlSetSpuPalette (struct DeviceContext_s* Context, video_spu_palette_t* SpuPalette)
{
    DVB_ERROR("Not supported\n");
    return -EPERM;
}
/*}}}*/
/*{{{  VideoIoctlGetNavi*/
static int VideoIoctlGetNavi (struct DeviceContext_s* Context, video_navi_pack_t* NaviPacket)
{
    DVB_ERROR("Not supported\n");
    return -EPERM;
}
/*}}}*/
/*{{{  VideoIoctlSetAttributes*/
static int VideoIoctlSetAttributes (struct DeviceContext_s* Context, video_attributes_t Attributes)
{
    DVB_ERROR("Not supported\n");
    return -EPERM;
}
/*}}}*/
/*{{{  VideoIoctlGetFrameRate*/
static int VideoIoctlGetFrameRate (struct DeviceContext_s* Context, int* FrameRate)
{
    DVB_DEBUG ("(video%d)\n", Context->Id);

    *FrameRate  = Context->FrameRate;
    return 0;
}
/*}}}*/
/*{{{  VideoIoctlSetEncoding*/
#if 0
static int VideoIoctlSetEncoding (struct DeviceContext_s* Context, unsigned int Encoding)
{
    DVB_DEBUG("(video%d) Set encoding to %d\n", Context->Id, Encoding);

    if ((Encoding < VIDEO_ENCODING_AUTO) || (Encoding > VIDEO_ENCODING_PRIVATE))
        return  -EINVAL;

    if ((Context->VideoState.play_state != VIDEO_STOPPED) && (Context->VideoState.play_state != VIDEO_INCOMPLETE))
    {
        DVB_ERROR("Cannot change encoding after play has started\n");
        return -EPERM;
    }

    Context->VideoEncoding      = (video_encoding_t)Encoding;

    /* At this point we have received the missing piece of information which will allow the
     * stream to be fully populated so we can reissue the play. */
    if (Context->VideoState.play_state == VIDEO_INCOMPLETE)
        VideoIoctlPlay (Context);

    return 0;
}
#else
static int VideoIoctlSetEncoding (struct DeviceContext_s* Context, unsigned int Encoding)
{
    DVB_DEBUG("(video%d) Set encoding to %d\n", Context->Id, Encoding);

    if ((Encoding < VIDEO_ENCODING_AUTO) || (Encoding > VIDEO_ENCODING_PRIVATE))
        return  -EINVAL;

    if (Context->VideoEncoding == (video_encoding_t)Encoding)
        return 0;

    Context->VideoEncoding      = (video_encoding_t)Encoding;

    switch (Context->VideoState.play_state)
    {
        case VIDEO_STOPPED:
            return 0;
        case VIDEO_INCOMPLETE:
            /* At this point we have received the missing piece of information which will allow the
             * stream to be fully populated so we can reissue the play. */
            return VideoIoctlPlay (Context);
        default:
        {
            int         Result  = 0;
            sigset_t    Newsigs;
            sigset_t    Oldsigs;

            if ((Encoding <= VIDEO_ENCODING_AUTO) || (Encoding >= VIDEO_ENCODING_NONE))
            {
                DVB_ERROR("Cannot switch to undefined encoding after play has started\n");
                return  -EINVAL;
            }
            /* a signal received in here can cause issues. Turn them off, just for this bit... */
            sigfillset (&Newsigs);
            sigprocmask (SIG_BLOCK, &Newsigs, &Oldsigs);

            Result      = DvbStreamSwitch (Context->VideoStream,
                                           BACKEND_PES_ID,
                                           VideoContent[Context->VideoEncoding]);

            sigprocmask (SIG_SETMASK, &Oldsigs, NULL);
            /*
            if (Result == 0)
                Result  = VideoIoctlSetId            (Context, Context->VideoId);
            */
            return Result;
        }
    }
}
#endif
/*}}}*/
/*{{{  VideoIoctlFlush*/
static int VideoIoctlFlush (struct DeviceContext_s* Context)
{
    int                         Result  = 0;
    struct DvbContext_s*        DvbContext      = Context->DvbContext;

    DVB_DEBUG("(video%d), %p\n", Context->Id, Context->VideoStream);

    /* If the stream is frozen it cannot be drained so an error is returned. */
    if ((Context->PlaySpeed == 0) || (Context->PlaySpeed == DVB_SPEED_REVERSE_STOPPED))
        return -EPERM;

    if (mutex_lock_interruptible (Context->ActiveVideoWriteLock) != 0)
        return -ERESTARTSYS;
    mutex_unlock (&(DvbContext->Lock));                 /* release lock so non-writing ioctls still work while draining */

    Result      = DvbStreamDrain (Context->VideoStream, false);

    mutex_unlock (Context->ActiveVideoWriteLock);       /* release write lock so actions which have context lock can complete */
    mutex_lock (&(DvbContext->Lock));                   /* reclaim lock so can be released by outer function */

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlDiscontinuity*/
static int VideoIoctlDiscontinuity (struct DeviceContext_s* Context, video_discontinuity_t Discontinuity)
{
    int                         Result          = 0;
    struct DvbContext_s*        DvbContext      = Context->DvbContext;
    /*DVB_DEBUG("(video%d) %x\n", Context->Id, Discontinuity);*/

    /* If the stream is frozen a discontinuity cannot be injected. */
    if (Context->VideoState.play_state == VIDEO_FREEZED)
        return -EFAULT;

    /* If speed is zero and the mutex is unavailable a discontinuity cannot be injected. */
    if ((mutex_is_locked (Context->ActiveVideoWriteLock) != 0) &&
        ((Context->PlaySpeed == 0) || (Context->PlaySpeed == DVB_SPEED_REVERSE_STOPPED)))
        return -EFAULT;

    if (mutex_lock_interruptible (Context->ActiveVideoWriteLock) != 0)
        return -ERESTARTSYS;
    mutex_unlock (&(DvbContext->Lock));                 /* release lock so non-writing ioctls still work during discontinuity */

    Result  = DvbStreamDiscontinuity (Context->VideoStream, (discontinuity_t)Discontinuity);

    mutex_unlock (Context->ActiveVideoWriteLock);
    mutex_lock (&(DvbContext->Lock));                   /* reclaim lock so can be released by outer function */

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlStep*/
static int VideoIoctlStep (struct DeviceContext_s* Context)
{
    int Result  = 0;

    DVB_DEBUG ("(video%d)\n", Context->Id);
    /* Step can only be used when the speed is zero */
    if ((Context->VideoStream != NULL) &&
       ((Context->PlaySpeed == 0) || (Context->PlaySpeed == DVB_SPEED_REVERSE_STOPPED)))
        Result  = DvbStreamStep (Context->VideoStream);

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlSetPlayInterval*/
int VideoIoctlSetPlayInterval (struct DeviceContext_s* Context, video_play_interval_t* PlayInterval)
{
    DVB_DEBUG ("(video%d)\n", Context->Id);

    memcpy (&Context->VideoPlayInterval, PlayInterval, sizeof(video_play_interval_t));

    if (Context->VideoStream == NULL)
        return 0;

    return DvbStreamSetPlayInterval (Context->VideoStream, (play_interval_t*)PlayInterval);
}
/*}}}*/
/*{{{  VideoIoctlSetSyncGroup*/
static int VideoIoctlSetSyncGroup (struct DeviceContext_s* Context, unsigned int Group)
{
    int         Result  = 0;

    DVB_DEBUG("(video%d) Group %d\n", Context->Id, Group);
    if (Context != Context->SyncContext)
    {
        DVB_ERROR("Sync group already set - cannot reset\n");
        return -EPERM;
    }

    if ((Group < 0) || (Group >= DVB_MAX_DEVICES_PER_ADAPTER))
    {
        DVB_ERROR("Invalid sync group - out of range.\n");
        return -EPERM;
    }

    if (Context->SyncContext->Playback)
    {
        DVB_ERROR("Sync group already set - cannot reset\n");
        return -EPERM;
    }

    Context->SyncContext        = &Context->DvbContext->DeviceContext[Group];

    DVB_DEBUG("Sync group set to device %d\n", Group);

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlCommand*/
static int VideoIoctlCommand (struct DeviceContext_s* Context, struct video_command* VideoCommand)
{
    int         Result          = 0;
    int         ApplyLater      = 0;

    DVB_DEBUG ("(video%d)\n", Context->Id);
    if (VideoCommand->cmd == VIDEO_CMD_SET_OPTION)
    {
        if (VideoCommand->option.option >= DVB_OPTION_MAX)
        {
            DVB_ERROR ("Option %d, out of range\n", VideoCommand->option.option);
            return  -EINVAL;
        }
        /*
         *  Determine if the command should be applied to the playback or just the video stream.  Check if
         *  the stream or playback is valid.  If so apply the option to the stream.  If not check to see if
         *  there is a valid playback.  If so, apply the option to the playback if appropriate.  If not, save
         *  command for later.
         */

        DVB_DEBUG("Option %d, Value 0x%x\n", VideoCommand->option.option, VideoCommand->option.value);

        if ((VideoCommand->option.option == PLAY_OPTION_DISCARD_LATE_FRAMES) ||
            (VideoCommand->option.option == PLAY_OPTION_VIDEO_START_IMMEDIATE) ||
            (VideoCommand->option.option == PLAY_OPTION_PTS_SYMMETRIC_JUMP_DETECTION) ||
            (VideoCommand->option.option == PLAY_OPTION_PTS_FORWARD_JUMP_DETECTION_THRESHOLD) ||
            (VideoCommand->option.option == PLAY_OPTION_SYNC_START_IMMEDIATE) )
        {
            if (Context->Playback != NULL)
                Result = DvbPlaybackSetOption (Context->Playback, (play_option_t)VideoCommand->option.option, (unsigned int)VideoCommand->option.value);
            else
                ApplyLater = 1;
        }
        else
        {
            if (Context->VideoStream != NULL)
                Result  = DvbStreamSetOption (Context->VideoStream, (play_option_t)VideoCommand->option.option, (unsigned int)VideoCommand->option.value);
            else
                ApplyLater = 1;
        }

        if (ApplyLater)
        {
            Context->PlayOption[VideoCommand->option.option]    = VideoCommand->option.option;  /* save for later */
            Context->PlayValue[VideoCommand->option.option]     = VideoCommand->option.value;
            if (Context->Playback != NULL)                                                      /* apply to playback if appropriate */
                PlaybackInit (Context);
        }

    }
    if (Result != 0)
        return Result;

    return Result;
}
/*}}}*/
/*{{{  VideoIoctlSetClockDataPoint*/
int VideoIoctlSetClockDataPoint (struct DeviceContext_s* Context, video_clock_data_point_t* ClockData)
{
    DVB_DEBUG ("(video%d)\n", Context->Id);

    if (Context->Playback == NULL)
        return -ENODEV;

    return DvbPlaybackSetClockDataPoint (Context->Playback, (dvb_clock_data_point_t*)ClockData);
}
/*}}}*/
/*{{{  VideoIoctlSetTimeMapping*/
int VideoIoctlSetTimeMapping (struct DeviceContext_s* Context, video_time_mapping_t* TimeMapping)
{
    int                         Result          = 0;

    DVB_DEBUG ("(video%d)\n", Context->Id);
    if ((Context->Playback == NULL) || (Context->VideoStream == NULL))
        return -ENODEV;

    Result      = DvbStreamSetOption (Context->VideoStream, DVB_OPTION_EXTERNAL_TIME_MAPPING, DVB_OPTION_VALUE_ENABLE);

    return DvbPlaybackSetNativePlaybackTime (Context->Playback, TimeMapping->native_stream_time, TimeMapping->system_presentation_time);
}
/*}}}*/
/*}}}*/
/*{{{  VideoOpen*/
static int VideoOpen (struct inode*     Inode,
                      struct file*      File)
{
    struct dvb_device*          DvbDevice       = (struct dvb_device*)File->private_data;
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)DvbDevice->priv;
    struct DvbContext_s*        DvbContext      = Context->DvbContext;
    unsigned int                i;
    int                         Error;

    DVB_DEBUG ("Id %d\n", Context->Id);

    Error       = dvb_generic_open (Inode, File);
    if (Error < 0)
        return Error;

    mutex_lock (&(DvbContext->Lock));

    if ((File->f_flags & O_ACCMODE) != O_RDONLY)
    {
        Context->VideoState.play_state  = VIDEO_STOPPED;
        Context->VideoEvents.Write      = 0;
        Context->VideoEvents.Read       = 0;
        Context->VideoEvents.Overflow   = 0;
        for (i = 0; i < DVB_OPTION_MAX; i++)
            Context->PlayOption[i]      = DVB_OPTION_VALUE_INVALID;
        Context->VideoOpenWrite         = 1;
    }

    mutex_unlock (&(DvbContext->Lock));

    return 0;
}
/*}}}*/
/*{{{  VideoRelease*/
static int VideoRelease (struct inode*  Inode,
                         struct file*   File)
{
    struct dvb_device*          DvbDevice       = (struct dvb_device*)File->private_data;
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)DvbDevice->priv;
    struct DvbContext_s*        DvbContext      = Context->DvbContext;

    DVB_DEBUG ("Id %d\n", Context->Id);

    if ((File->f_flags & O_ACCMODE) != O_RDONLY)
    {
        mutex_lock (&(DvbContext->Lock));

        if (Context->VideoStream != NULL)
        {
            unsigned int    MutexIsLocked       = true;
            /* Discard previously injected data to free the lock. */
            DvbStreamDrain (Context->VideoStream, true);

            if (mutex_lock_interruptible (Context->ActiveVideoWriteLock) != 0)
                MutexIsLocked           = false;

            DvbPlaybackRemoveStream (Context->Playback, Context->VideoStream);
            Context->VideoStream        = NULL;

            if (MutexIsLocked)
                mutex_unlock (Context->ActiveVideoWriteLock);
        }

        DvbDisplayDelete (BACKEND_VIDEO_ID, Context->Id);

        /* Check to see if audio and demux have also finished so we can release the playback */
        if ((Context->AudioStream == NULL) && (Context->DemuxStream == NULL) && (Context->Playback != NULL))
        {
            /* Check to see if our playback has already been deleted by the demux context */
            if (Context->DemuxContext->Playback != NULL)
            {
                /* Try and delete playback then set our demux to Null if succesful or not.  If we fail someone else
                   is still using it but we are done with it. */
                if (DvbPlaybackDelete (Context->Playback) == 0)
                    DVB_DEBUG("Playback deleted successfully\n");
            }
            Context->Playback                   = NULL;
            Context->StreamType                 = STREAM_TYPE_TRANSPORT;
            Context->PlaySpeed                  = DVB_SPEED_NORMAL_PLAY;
            Context->VideoPlayInterval.start    = DVB_TIME_NOT_BOUNDED;
            Context->VideoPlayInterval.end      = DVB_TIME_NOT_BOUNDED;
            Context->SyncContext                = Context;

        }

        VideoInit (Context);
        mutex_unlock (&(DvbContext->Lock));
    }

    return dvb_generic_release (Inode, File);
}
/*}}}*/
/*{{{  VideoIoctl*/
static int VideoIoctl (struct inode*    Inode,
                       struct file*     File,
                       unsigned int     IoctlCode,
                       void*            Parameter)
{
    struct dvb_device*          DvbDevice       = (struct dvb_device*)File->private_data;
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)DvbDevice->priv;
    struct DvbContext_s*        DvbContext      = Context->DvbContext;
    int                         Result          = 0;

    /*DVB_DEBUG("VideoIoctl : Ioctl %08x\n", IoctlCode);*/

    /*
    if (((File->f_flags & O_ACCMODE) == O_RDONLY) &&
        (IoctlCode != VIDEO_GET_STATUS) && (IoctlCode != VIDEO_GET_EVENT) && (IoctlCode != VIDEO_GET_SIZE))
        return -EPERM;
    */
    if ((File->f_flags & O_ACCMODE) == O_RDONLY)
    {
        switch (IoctlCode)
        {
            case    VIDEO_GET_STATUS:
            case    VIDEO_GET_EVENT:
            case    VIDEO_GET_SIZE:
            case    VIDEO_GET_FRAME_RATE:
            /*  Not allowed as they require an active player
            case    VIDEO_GET_PTS:
            case    VIDEO_GET_PLAY_TIME:
            case    VIDEO_GET_FRAME_COUNT:
            */
                break;
            default:
                return -EPERM;
        }
    }

    if (!Context->VideoOpenWrite)       /* Check to see that somebody has the device open for write */
        return -EBADF;

    mutex_lock (&(DvbContext->Lock));
    switch (IoctlCode)
    {
        case VIDEO_STOP:                  Result = VideoIoctlStop               (Context, (unsigned int)Parameter);                       break;
        case VIDEO_PLAY:                  Result = VideoIoctlPlay               (Context);                                                break;
        case VIDEO_FREEZE:                Result = VideoIoctlFreeze             (Context);                                                break;
        case VIDEO_CONTINUE:              Result = VideoIoctlContinue           (Context);                                                break;
        case VIDEO_SELECT_SOURCE:         Result = VideoIoctlSelectSource       (Context, (video_stream_source_t)Parameter);              break;
        case VIDEO_SET_BLANK:             Result = VideoIoctlSetBlank           (Context, (unsigned int)Parameter);                       break;
        case VIDEO_GET_STATUS:            Result = VideoIoctlGetStatus          (Context, (struct video_status*)Parameter);               break;
        case VIDEO_GET_EVENT:             Result = VideoIoctlGetEvent           (Context, (struct video_event*)Parameter, File->f_flags); break;
        case VIDEO_GET_SIZE:              Result = VideoIoctlGetSize            (Context, (video_size_t*)Parameter);                      break;
        case VIDEO_SET_DISPLAY_FORMAT:    Result = VideoIoctlSetDisplayFormat   (Context, (video_displayformat_t)Parameter);              break;
        case VIDEO_STILLPICTURE:          Result = VideoIoctlStillPicture       (Context, (struct video_still_picture*)Parameter);        break;
        case VIDEO_FAST_FORWARD:          Result = VideoIoctlFastForward        (Context, (int)Parameter);                                break;
        case VIDEO_SLOWMOTION:            Result = VideoIoctlSlowMotion         (Context, (int)Parameter);                                break;
        case VIDEO_GET_CAPABILITIES:      Result = VideoIoctlGetCapabilities    (Context, (int*)Parameter);                               break;
        case VIDEO_GET_PTS:               Result = VideoIoctlGetPts             (Context, (unsigned long long int*)Parameter);            break;
        case VIDEO_GET_FRAME_COUNT:       Result = VideoIoctlGetFrameCount      (Context, (unsigned long long int*)Parameter);            break;
        case VIDEO_SET_ID:                Result = VideoIoctlSetId              (Context, (int)Parameter);                                break;
        case VIDEO_CLEAR_BUFFER:          Result = VideoIoctlClearBuffer        (Context);                                                break;
        case VIDEO_SET_STREAMTYPE:        Result = VideoIoctlSetStreamType      (Context, (unsigned int)Parameter);                       break;
        case VIDEO_SET_FORMAT:            Result = VideoIoctlSetFormat          (Context, (unsigned int)Parameter);                       break;
        case VIDEO_SET_SYSTEM:            Result = VideoIoctlSetSystem          (Context, (video_system_t)Parameter);                     break;
        case VIDEO_SET_HIGHLIGHT:         Result = VideoIoctlSetHighlight       (Context, (video_highlight_t*)Parameter);                 break;
        case VIDEO_SET_SPU:               Result = VideoIoctlSetSpu             (Context, (video_spu_t*)Parameter);                       break;
        case VIDEO_SET_SPU_PALETTE:       Result = VideoIoctlSetSpuPalette      (Context, (video_spu_palette_t*)Parameter);               break;
        case VIDEO_GET_NAVI:              Result = VideoIoctlGetNavi            (Context, (video_navi_pack_t*)Parameter);                 break;
        case VIDEO_SET_ATTRIBUTES:        Result = VideoIoctlSetAttributes      (Context, (video_attributes_t)Parameter);                 break;
        case VIDEO_GET_FRAME_RATE:        Result = VideoIoctlGetFrameRate       (Context, (int*)Parameter);                               break;
        case VIDEO_SET_ENCODING:          Result = VideoIoctlSetEncoding        (Context, (unsigned int)Parameter);                       break;
        case VIDEO_FLUSH:                 Result = VideoIoctlFlush              (Context);                                                break;
        case VIDEO_SET_SPEED:             Result = VideoIoctlSetSpeed           (Context, (int)Parameter);                                break;
        case VIDEO_DISCONTINUITY:         Result = VideoIoctlDiscontinuity      (Context, (video_discontinuity_t)Parameter);              break;
        case VIDEO_STEP:                  Result = VideoIoctlStep               (Context);                                                break;
        case VIDEO_SET_PLAY_INTERVAL:     Result = VideoIoctlSetPlayInterval    (Context, (video_play_interval_t*)Parameter);             break;
        case VIDEO_SET_SYNC_GROUP:        Result = VideoIoctlSetSyncGroup       (Context, (unsigned int)Parameter);                       break;
        case VIDEO_COMMAND:               Result = VideoIoctlCommand            (Context, (struct video_command*)Parameter);              break;
        case VIDEO_GET_PLAY_TIME:         Result = VideoIoctlGetPlayTime        (Context, (video_play_time_t*)Parameter);                 break;
        case VIDEO_GET_PLAY_INFO:         Result = VideoIoctlGetPlayInfo        (Context, (video_play_info_t*)Parameter);                 break;
        case VIDEO_SET_CLOCK_DATA_POINT:  Result = VideoIoctlSetClockDataPoint  (Context, (video_clock_data_point_t*)Parameter);          break;
        case VIDEO_SET_TIME_MAPPING:      Result = VideoIoctlSetTimeMapping     (Context, (video_time_mapping_t*)Parameter);              break;

        default:
            DVB_ERROR("Error - invalid ioctl %08x\n", IoctlCode);
            Result      = -ENOIOCTLCMD;
    }
    mutex_unlock (&(DvbContext->Lock));

    return Result;
}
/*}}}*/
/*{{{  VideoWrite*/
static ssize_t VideoWrite (struct file *File, const char __user* Buffer, size_t Count, loff_t* ppos)
{
    struct dvb_device*          DvbDevice       = (struct dvb_device*)File->private_data;
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)DvbDevice->priv;
    int                         Result          = 0;
    char*                       InBuffer        = &Context->dvr_in[16*2048];
    char*                       OutBuffer       = &Context->dvr_out[16*2048];
    void*                       PhysicalBuffer  = NULL;
    const unsigned char*        ClearData       = Buffer;

    if ((File->f_flags & O_ACCMODE) == O_RDONLY)
        return -EPERM;

    if (Context->VideoState.stream_source == VIDEO_SOURCE_DEMUX)
    {
        DVB_ERROR("Video stream source not VIDEO_SOURCE_MEMORY - cannot use write\n");
        return -EPERM;          /* Not allowed to write to device if connected to demux */
    }

    mutex_lock (Context->ActiveVideoWriteLock);

    if (Context->VideoStream == NULL)
    {
        DVB_ERROR ("No video stream exists to be written to (previous VIDEO_PLAY failed?)\n");
        mutex_unlock (Context->ActiveVideoWriteLock);
        return -ENODEV;
    }

    if (!StreamBufferFree (Context->VideoStream) && ((File->f_flags & O_NONBLOCK) != 0))
    {
        mutex_unlock (Context->ActiveVideoWriteLock);
        return -EWOULDBLOCK;
    }

    PhysicalBuffer      = stm_v4l2_findbuffer ((unsigned long)Buffer, Count, 0);
    if (Context->EncryptionOn) 
    {
        if ((Count % 2048) != 0)
        {
            DVB_ERROR ("Count size incorrect for decryption (%d)\n", Count);
            mutex_unlock (Context->ActiveVideoWriteLock);
            return -EINVAL;
        }

        if (PhysicalBuffer != NULL)
        {
                //tkdma_hddvd_decrypt_data (OutBuffer, PhysicalBuffer, Count/2048, TKDMA_VIDEO_CHANNEL, 0); // Got to check blocking
        }
        else
        {
            /*
               If the incoming data is aligned to 2K/4K we would be able to avoid
               this copy from user, we just decrypt max two packets at a time, certainly scatter gather
               would be easier.
            */
            copy_from_user (InBuffer, Buffer, Count);
            //tkdma_hddvd_decrypt_data (OutBuffer, InBuffer, Count/2048, TKDMA_VIDEO_CHANNEL, 0); // Got to check blocking
        }
        ClearData       = OutBuffer;
    }
    else if (PhysicalBuffer)
        ClearData       = PhysicalBuffer;

    if (Context->VideoState.play_state == VIDEO_INCOMPLETE)
    {
        DvbStreamGetFirstBuffer (Context->VideoStream, ClearData, Count);
        DvbStreamIdentifyVideo  (Context->VideoStream, &(Context->VideoEncoding));
        VideoIoctlPlay       (Context);

        if (Context->VideoState.play_state == VIDEO_INCOMPLETE)
            Context->VideoState.play_state    = VIDEO_STOPPED;
    }

    if (Context->VideoState.play_state != VIDEO_PLAYING)
    {
        DVB_ERROR("Error - video not playing - cannot write (state %x)\n", Context->VideoState.play_state);
        Result  = -EPERM;                                       /* Not allowed to write to device if paused */
    }
    else
    {
        /* We can infact always inject from Kernel, copy_from_user not necessary, however we should do
           an access_ok check above this */
         Result         = DvbStreamInject (Context->VideoStream, ClearData, Count);
    }

    mutex_unlock (Context->ActiveVideoWriteLock);

    return Result;
}
/*}}}*/
/*{{{  VideoPoll*/
static unsigned int VideoPoll (struct file* File, poll_table* Wait)
{
    struct dvb_device*          DvbDevice       = (struct dvb_device*)File->private_data;
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)DvbDevice->priv;
    unsigned int                Mask            = 0;

    /*DVB_DEBUG ("(video%d)\n", Context->Id);*/

#if defined (USE_INJECTION_THREAD)
    if (((File->f_flags & O_ACCMODE) != O_RDONLY) && (Context->VideoStream != NULL))
        poll_wait (File, &Context->VideoStream->BufferEmpty, Wait);
#endif

    poll_wait (File, &Context->VideoEvents.WaitQueue, Wait);

    if (Context->VideoEvents.Write != Context->VideoEvents.Read)
        Mask    = POLLPRI;

    if (((File->f_flags & O_ACCMODE) != O_RDONLY) && (Context->VideoStream != NULL))
    {
        if ((Context->VideoState.play_state == VIDEO_PLAYING) || (Context->VideoState.play_state == VIDEO_INCOMPLETE))
        {
            if (StreamBufferFree (Context->VideoStream))
                Mask   |= (POLLOUT | POLLWRNORM);
        }
    }
    return Mask;
}
/*}}}*/
/*{{{  VideoSetEvent*/
static void VideoSetEvent (struct DeviceContext_s* Context,
                           struct stream_event_s*  Event)
{
    struct VideoEvent_s*        EventList       = &Context->VideoEvents;
    unsigned int                Next;
    struct video_event*         VideoEvent;
    unsigned int                EventReceived   = false;

    /*DVB_DEBUG ("(video%d)\n", Context->Id);*/

    mutex_lock (&EventList->Lock);

    Next                        = (EventList->Write + 1) % MAX_VIDEO_EVENT;
    if (Next == EventList->Read)
    {
        EventList->Overflow     = true;
        EventList->Read         = (EventList->Read + 1) % MAX_VIDEO_EVENT;
    }

    VideoEvent                  = &(EventList->Event[EventList->Write]);
    VideoEvent->type            = (int)Event->code;
    VideoEvent->timestamp       = (time_t)Event->timestamp;
    switch (Event->code)
    {
        case STREAM_EVENT_SIZE_CHANGED:
            VideoEvent->u.size.w                        = Event->u.size.width;
            VideoEvent->u.size.h                        = Event->u.size.height;
            VideoEvent->u.size.aspect_ratio             = Event->u.size.aspect_ratio;

            Context->VideoSize.w                        = Event->u.size.width;
            Context->VideoSize.h                        = Event->u.size.height;
            Context->VideoSize.aspect_ratio             = Event->u.size.aspect_ratio;
            Context->VideoState.video_format            = Event->u.size.aspect_ratio;

            Context->PixelAspectRatio.Numerator         = Event->u.size.pixel_aspect_ratio_numerator;
            Context->PixelAspectRatio.Denominator       = Event->u.size.pixel_aspect_ratio_denominator;
            break;

        case STREAM_EVENT_FRAME_RATE_CHANGED:
            VideoEvent->u.frame_rate                    = Event->u.frame_rate;
            Context->FrameRate                          = Event->u.frame_rate;
            break;

        case STREAM_EVENT_FIRST_FRAME_ON_DISPLAY:
        case STREAM_EVENT_FRAME_DECODED_LATE:
        case STREAM_EVENT_DATA_DELIVERED_LATE:
            break;

        /* The code below uses the frame_rate to store the unplayable reason as this
           event is not defined in the standard structure */
        case STREAM_EVENT_STREAM_UNPLAYABLE:
            VideoEvent->u.frame_rate            = (unsigned int)(Event->u.reason);
            break;

        /* The code below uses the frame_rate to store the trick mode domain switched to
           as this event is not defined in the standard structure */
        case STREAM_EVENT_TRICK_MODE_CHANGE:
            VideoEvent->u.frame_rate            = (unsigned int)(Event->u.trick_mode_domain);
            break;

    }
    EventList->Write            = Next;
    EventReceived               = true;

    mutex_unlock (&EventList->Lock);

    if (EventReceived)
        wake_up_interruptible (&EventList->WaitQueue);
}
/*}}}*/

