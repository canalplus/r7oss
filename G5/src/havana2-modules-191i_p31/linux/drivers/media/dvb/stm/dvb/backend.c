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

Source file name : backend.c - linuxdvb backend engine for driving player
Author :           Julian


Date        Modification                                    Name
----        ------------                                    --------
31-Jan-07   Created                                         Julian

************************************************************************/

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/bpa2.h>
#include <linux/mutex.h>

#include "pes.h"
#include "dvb_module.h"
#include "backend.h"
#include "backend_ops.h"

/*{{{  static data*/
static unsigned char            ASFHeaderObjectGuid[]   = {0x30, 0x26, 0xb2, 0x75,
                                                           0x8e, 0x66, 0xcf, 0x11,
                                                           0xa6, 0xd9, 0x00, 0xaa,
                                                           0x00, 0x62, 0xce, 0x6c};
/*}}}*/

/* The backend context provides access to the backend operations table */
struct BackendContext_s
{
    char*                               Name;
    struct dvb_backend_operations*      Ops;
};

/*{{{  register_backend*/
static struct BackendContext_s*         Backend;

int register_dvb_backend       (char*                           Name,
                                struct dvb_backend_operations*  Ops)
{
    if (Backend == NULL)
    {
        BACKEND_ERROR("Cannot register backend %s - not created\n", Name);
        return -ENOMEM;
    }
    Backend->Ops        = Ops;
    Backend->Name       = kzalloc (strlen (Name) + 1,  GFP_KERNEL);
    if (Backend->Name != NULL)
        strcpy (Backend->Name, Name);

    return 0;

}
EXPORT_SYMBOL(register_dvb_backend);
/*}}}*/


/*{{{  DvbBackendInit*/
int DvbBackendInit (void)
{
    Backend     = kzalloc (sizeof (struct BackendContext_s), GFP_KERNEL);
    if (Backend == NULL)
    {
        BACKEND_ERROR("Unable to create backend context - no memory\n");
        return -ENOMEM;
    }

    Backend->Ops                = NULL;

    return 0;
}
/*}}}*/
/*{{{  DvbBackendDelete*/
int DvbBackendDelete (void)
{
    if (Backend == NULL)
    {
        BACKEND_ERROR("Unable to delete backend context - not created\n");
        return -EINVAL;
    }

    if (Backend->Name != NULL)
        kfree (Backend->Name);
    Backend->Name       = NULL;

    kfree (Backend);
    Backend     = NULL;
    return 0;
}
/*}}}*/

/*{{{  DvbPlaybackCreate*/
int DvbPlaybackCreate  (struct PlaybackContext_s**      Playback)
{
    int         Result;


    if (Backend->Ops == NULL)
        return -ENODEV;

    if (*Playback != NULL)
        return -EINVAL;

    *Playback   = kzalloc (sizeof(struct PlaybackContext_s), GFP_KERNEL);
    if (*Playback == NULL)
    {
        BACKEND_ERROR("Unable to create playback context - insufficient memory\n");
        return -ENOMEM;
    }
    Result      = Backend->Ops->playback_create (&(*Playback)->Handle);
    if (Result < 0)
    {
        BACKEND_ERROR("Unable to create playback context\n");
        kfree (*Playback);
        *Playback       = NULL;
        return Result;
    }

    mutex_init (&((*Playback)->Lock));
    (*Playback)->UsageCount     = 0;

    BACKEND_TRACE("Playback %p\n", *Playback);

    return Result;
}
/*}}}*/
/*{{{  DvbPlaybackDelete*/
int DvbPlaybackDelete  (struct PlaybackContext_s*       Playback)
{
    int         Result  = 0;

    if (Playback == NULL)
        return -EINVAL;

    BACKEND_TRACE("Playback %p, Usage = %d\n", Playback, Playback->UsageCount);

    mutex_lock (&(Playback->Lock));

    if (Playback->UsageCount != 0)
    {
        BACKEND_TRACE("Cannot delete playback - usage = %d\n", Playback->UsageCount);
        mutex_unlock (&(Playback->Lock));
        return -EINVAL;
    }

    if (Playback->Handle != NULL)
    {
        Result = Backend->Ops->playback_delete (Playback->Handle);
        if (Result < 0)
            BACKEND_ERROR("Failed to delete playback context\n");
    }
    mutex_unlock (&(Playback->Lock));

    if (Result == 0)
        kfree (Playback);
    return Result;
}
/*}}}*/

/*{{{  DvbPlaybackAddStream*/
int DvbPlaybackAddStream       (struct PlaybackContext_s*       Playback,
                                char*                           Media,
                                char*                           Format,
                                char*                           Encoding,
                                unsigned int                    DemuxId,
                                unsigned int                    SurfaceId,
                                struct StreamContext_s**        Stream)
{
    int                         Result;
    unsigned int                NewStream       = false;
    unsigned int                Demux           = false;

    BACKEND_DEBUG ("%p\n", Playback);

    if (Backend->Ops == NULL)
        return -ENODEV;

    if (Playback == NULL)                                       /* No playback to start stream in */
        return -EINVAL;

    if ((*Stream != NULL) && ((*Stream)->Handle != NULL))       /* Device already has this stream */
        return -EINVAL;

    if (*Stream == NULL)
    {
        *Stream     = kzalloc (sizeof(struct StreamContext_s), GFP_KERNEL);
        if (*Stream == NULL)
        {
            BACKEND_ERROR("Unable to create stream context - insufficient memory\n");
            return -ENOMEM;
        }

        if (Media == NULL)
        {
            (*Stream)->BufferLength             = DEMUX_BUFFER_SIZE;
            (*Stream)->Inject                   = Backend->Ops->demux_inject_data;
            (*Stream)->Delete                   = Backend->Ops->playback_remove_demux;
            Demux                               = true;
        }
        else
        {
            if (DemuxId == DEMUX_INVALID_ID)
                (*Stream)->BufferLength         = (strcmp (Media, BACKEND_AUDIO_ID) == 0) ? AUDIO_STREAM_BUFFER_SIZE : VIDEO_STREAM_BUFFER_SIZE;
            else
            /* The stream is part of a demux so it doesn't need its own buffer */
            {
                (*Stream)->BufferLength         = 0;
                (*Stream)->Buffer               = NULL;
                if (strcmp (Encoding, BACKEND_AUTO_ID) == 0)    /* default to mpeg2 play */
                {
                    BACKEND_DEBUG("Transport stream - Defaulting to mpeg2\n");
                    Encoding                    = BACKEND_MPEG2_ID;
                }
            }

            (*Stream)->Inject                   = Backend->Ops->stream_inject_data;
            (*Stream)->InjectPacket             = Backend->Ops->stream_inject_data_packet;
            (*Stream)->Delete                   = Backend->Ops->playback_remove_stream;
        }

        if ((*Stream)->BufferLength != 0)
        {
            (*Stream)->Buffer                   = bigphysarea_alloc ((*Stream)->BufferLength);
            if ((*Stream)->Buffer == NULL)
            {
                BACKEND_ERROR("Unable to create stream buffer - insufficient memory\n");
                kfree (*Stream);
                *Stream                         = NULL;
                return -ENOMEM;
            }
        }
        NewStream       = true;
    }

    mutex_lock (&(Playback->Lock));
    if ((Encoding != NULL) && (strcmp (Encoding, BACKEND_AUTO_ID) == 0))
    {
        (*Stream)->Handle       = NULL;
        Result                  = STREAM_INCOMPLETE;
    }
    else if (Demux)
        Result  = Backend->Ops->playback_add_demux     (Playback->Handle,
                                                        DemuxId,
                                                        &(*Stream)->Handle);
    else
        Result  = Backend->Ops->playback_add_stream    (Playback->Handle,
                                                        Media,
                                                        Format,
                                                        Encoding,
                                                        SurfaceId,
                                                        &(*Stream)->Handle);

    if (Result < 0)
    {
        BACKEND_ERROR("Unable to create stream context\n");
        if ((*Stream)->Buffer != NULL)
            bigphysarea_free_pages ((*Stream)->Buffer);
        kfree (*Stream);
        *Stream         = NULL;
    }
    else
    {
        mutex_init (&((*Stream)->Lock));
        if (NewStream)
        {
            Playback->UsageCount++;

            (*Stream)->DataToWrite      = 0;
        }
    }
    mutex_unlock (&(Playback->Lock));

    BACKEND_DEBUG ("%p: Usage = %d\n", Playback, Playback->UsageCount);
    return Result;
}
/*}}}*/
/*{{{  DvbPlaybackRemoveStream*/
int DvbPlaybackRemoveStream    (struct PlaybackContext_s*       Playback,
                                struct StreamContext_s*         Stream)
{
    int         Result  = 0;

    BACKEND_DEBUG ("%p: Usage = %d\n", Playback, Playback->UsageCount);

    if ((Playback == NULL) || (Stream == NULL))
    {
        BACKEND_ERROR("Unable to remove stream (%p) from playback (%p) if either is NULL\n", Stream, Playback);
        return -EINVAL;
    }

    mutex_lock (&(Playback->Lock));

    if (Stream->Handle != NULL)
    {
        Result = Stream->Delete (Playback->Handle, Stream->Handle);
        if (Result < 0)
            BACKEND_ERROR("Failed to remove stream from playback\n");
    }

    if (Stream->Buffer != NULL)
        bigphysarea_free_pages (Stream->Buffer);
    kfree (Stream);

    Playback->UsageCount--;
    mutex_unlock (&(Playback->Lock));

    return Result;
}
/*}}}*/
/*{{{  DvbPlaybackSetSpeed*/
int DvbPlaybackSetSpeed        (struct PlaybackContext_s*       Playback,
                                unsigned int                    Speed)
{
    int                 Result;

    if (Playback == NULL)
        return -EINVAL;

    Result      = Backend->Ops->playback_set_speed (Playback->Handle, Speed);
    if (Result < 0)
        BACKEND_ERROR("Unable to set speed\n");

    return Result;
}
/*}}}*/
/*{{{  DvbPlaybackGetSpeed*/
int DvbPlaybackGetSpeed        (struct PlaybackContext_s*       Playback,
                                unsigned int*                   Speed)
{
    int                 Result;

    if (Playback == NULL)                       /* No playback to start audio in */
        return -EINVAL;

    Result      = Backend->Ops->playback_get_speed (Playback->Handle, Speed);
    if (Result < 0)
        BACKEND_ERROR("Unable to access speed\n");

    return Result;
}
/*}}}*/
/*{{{  DvbPlaybackSetOption*/
int DvbPlaybackSetOption       (struct PlaybackContext_s*       Playback,
                                play_option_t                   Option,
                                unsigned int                    Value)
{
    int         Result;

    if ((Playback == NULL) || (Playback->Handle == NULL))           /* No playback to set option on */
        return -EINVAL;

    Result      = Backend->Ops->playback_set_option (Playback->Handle, Option, Value);
    if (Result < 0)
        BACKEND_ERROR("Unable to set stream option\n");

    return Result;
}
/*}}}*/
/*{{{  DvbPlaybackSetNativePlaybackTime*/
int DvbPlaybackSetNativePlaybackTime   (struct PlaybackContext_s*       Playback,
                                        unsigned long long              NativeTime,
                                        unsigned long long              SystemTime)
{
    int Result  = 0;

    if (Backend->Ops == NULL)
        return -ENODEV;

    if (Playback == NULL)
        return -EINVAL;

    if (NativeTime == SystemTime)
    {
        BACKEND_DEBUG ("NativeTime == SystemTime -> no need for sync call \n");
        return 0;
    }

    Result  = Backend->Ops->playback_set_native_playback_time  (Playback->Handle,
                                                                NativeTime,
                                                                SystemTime);
    if (Result < 0)
        BACKEND_ERROR("Unable to set native playback time\n");

    return Result;
}
/*}}}*/
/*{{{  DvbPlayerGetPlayerEnvironment*/
int DvbPlaybackGetPlayerEnvironment    (struct PlaybackContext_s*  Playback,
                                        playback_handle_t*         playerplayback)
{
    int         Result = 0;

    if (Playback == NULL) return -EINVAL;

    Result      = Backend->Ops->playback_get_player_environment (Playback->Handle, playerplayback);
    if (Result < 0)
        BACKEND_ERROR("Unable to get player environment\n");

    return Result;
}
/*}}}*/
/*{{{  DvbPlaybackSetClockDataPoint*/
int DvbPlaybackSetClockDataPoint       (struct PlaybackContext_s*       Playback,
                                        dvb_clock_data_point_t*         ClockData)
{
    int Result  = 0;

    if (Backend->Ops == NULL)
        return -ENODEV;

    if (Playback == NULL)
        return -EINVAL;

    Result  = Backend->Ops->playback_set_clock_data_point      (Playback->Handle, (clock_data_point_t*)ClockData);
    if (Result < 0)
        BACKEND_ERROR("Unable to set clock data point\n");

    return Result;
}
/*}}}*/

/*{{{  DvbStreamEnable*/
int DvbStreamEnable    (struct StreamContext_s*         Stream,
                        unsigned int                    Enable)
{
    int         Result = 0;

    if ((Stream == NULL) || (Stream->Handle == NULL))
        return -EINVAL;

    Result      = Backend->Ops->stream_enable (Stream->Handle, Enable);
    if (Result < 0)
        BACKEND_ERROR("Unable to enable stream\n");

    return Result;
}
/*}}}*/
/*{{{  DvbStreamSetId*/
int DvbStreamSetId     (struct StreamContext_s*         Stream,
                        unsigned int                    DemuxId,
                        unsigned int                    Id)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))           /* No stream to set id */
        return -EINVAL;

    Result      = Backend->Ops->stream_set_id (Stream->Handle, DemuxId, Id);
    if (Result < 0)
        BACKEND_ERROR("Unable to set Id\n");

    return Result;
}
/*}}}*/
/*{{{  DvbStreamChannelSelect*/
int DvbStreamChannelSelect     (struct StreamContext_s*         Stream,
                                channel_select_t                Channel)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))           /* No stream to set id */
        return -EINVAL;

    Result      = Backend->Ops->stream_channel_select (Stream->Handle, Channel);
    if (Result < 0)
        BACKEND_ERROR("Unable to select channel\n");

    return Result;
}
/*}}}*/
/*{{{  DvbStreamSetOption*/
int DvbStreamSetOption (struct StreamContext_s*         Stream,
                        play_option_t                   Option,
                        unsigned int                    Value)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))           /* No playback to set option on */
        return -EINVAL;

    Result      = Backend->Ops->stream_set_option (Stream->Handle, Option, Value);
    if (Result < 0)
        BACKEND_ERROR("Unable to set stream option\n");

    return Result;
}
/*}}}*/
/*{{{  DvbStreamGetOption*/
int DvbStreamGetOption  (struct StreamContext_s*         Stream,
                        play_option_t                   Option,
                        unsigned int*                   Value)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))           /* No stream to get option on */
        return -EINVAL;

    Result      = Backend->Ops->stream_get_option (Stream->Handle, Option, Value);
    if (Result < 0)
        BACKEND_ERROR("Unable to set stream option\n");

    return Result;
}
/*}}}*/
/*{{{  DvbStreamGetPlayInfo*/
int DvbStreamGetPlayInfo       (struct StreamContext_s*         Stream,
                                struct play_info_s*             PlayInfo)
{
    if ((Stream == NULL) || (Stream->Handle == NULL))
        return -EINVAL;

    return Backend->Ops->stream_get_play_info (Stream->Handle, PlayInfo);
}
/*}}}*/
/*{{{  DvbStreamSetPlayInterval*/
int DvbStreamSetPlayInterval   (struct StreamContext_s*    Stream,
                                dvb_play_interval_t*       PlayInterval)
{
    int                 Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))           /* No playback to set option on */
        return -EINVAL;

    Result      = Backend->Ops->stream_set_play_interval (Stream->Handle, (play_interval_t*)PlayInterval);
    if (Result < 0)
        BACKEND_ERROR("Unable to set play interval\n");

    return Result;
}
/*}}}*/

/*{{{  DvbStreamInject*/
/*
  StreamInject is used by the Video and Audio devices to inject TS,PES
  (or program stream or system stream) into the player.
*/
int DvbStreamInject            (struct StreamContext_s*         Stream,
                                const unsigned char*            Buffer,
                                unsigned int                    Length)
{
    int         Result              = 0;

    mutex_lock (&(Stream->Lock));
    Result      = Stream->Inject (Stream->Handle, Buffer, Length);
    mutex_unlock (&(Stream->Lock));

    return Result;
}
/*}}}*/
/*{{{  DvbStreamInjectPacket*/
/*
  StreamInject is used by the Video and Audio devices to inject TS,PES
  (or program stream or system stream) into the player.
*/
int DvbStreamInjectPacket      (struct StreamContext_s*         Stream,
                                const unsigned char*            Buffer,
                                unsigned int                    Length,
                                bool                            PresentationTimeValid,
                                unsigned long long              PresentationTime)
{
    int         Result              = 0;

    mutex_lock (&(Stream->Lock));
    Result      = Stream->InjectPacket (Stream->Handle, Buffer, Length, PresentationTimeValid, PresentationTime);
    mutex_unlock (&(Stream->Lock));

    return Result;
}
/*}}}*/
/*{{{  DvbStreamDrain*/
int DvbStreamDrain     (struct StreamContext_s*         Stream,
                        unsigned int                    Discard)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))           /* No stream to drain */
        return -EINVAL;

    Result      = Backend->Ops->stream_drain (Stream->Handle, Discard);
    if (Result < 0)
        BACKEND_ERROR("Unable to drain stream\n");

    return Result;
}
/*}}}*/
/*{{{  DvbStreamStep*/
int DvbStreamStep      (struct StreamContext_s*         Stream)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))           /* No stream to step */
        return -EINVAL;

    Result      = Backend->Ops->stream_step (Stream->Handle);
    if (Result < 0)
        BACKEND_ERROR("Unable to step stream\n");

    return Result;
}
/*}}}*/
/*{{{  DvbStreamSwitch*/
int DvbStreamSwitch            (struct StreamContext_s*         Stream,
                                char*                           Format,
                                char*                           Encoding)
{
    BACKEND_DEBUG ("%p\n", Stream);

    if ((Stream == NULL) || (Stream->Handle == NULL))       /* No stream */
        return -EINVAL;

    return Backend->Ops->stream_switch (Stream->Handle, Format, Encoding);

}
/*}}}*/
/*{{{  DvbStreamDiscontinuity*/
int DvbStreamDiscontinuity     (struct StreamContext_s*         Stream,
                                discontinuity_t                 Discontinuity)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))
        return -EINVAL;

    Result      = Backend->Ops->stream_discontinuity (Stream->Handle, Discontinuity);
    if (Result < 0)
        BACKEND_ERROR("Unable to inject discontinuity\n");

    return Result;
}
/*}}}*/
/*{{{  DvbStreamRegisterEventSignalCallback*/
stream_event_signal_callback DvbStreamRegisterEventSignalCallback      (struct StreamContext_s*         Stream,
                                                                        struct DeviceContext_s*         Context,
                                                                        stream_event_signal_callback    Callback)
{
    if ((Stream == NULL) || (Stream->Handle == NULL))
        return NULL;

    return Backend->Ops->stream_register_event_signal_callback (Stream->Handle, (context_handle_t)Context, Callback);
}
/*}}}*/
/*{{{  DvbStreamGetDecodeBuffer*/
int DvbStreamGetDecodeBuffer   (struct StreamContext_s*         Stream,
                                buffer_handle_t*                Buffer,
                                unsigned char**                 Data,
                                unsigned int                    Format,
                                unsigned int                    DimensionCount,
                                unsigned int                    Dimensions[],
                                unsigned int*                   Index,
                                unsigned int*                   Stride)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))
        return -EINVAL;

    Result      = Backend->Ops->stream_get_decode_buffer (Stream->Handle, Buffer, Data, Format, DimensionCount, Dimensions, Index, Stride);
    if (Result < 0)
        BACKEND_ERROR("Failed to access decode buffer (%d)\n", Result);

    return Result;
}
/*}}}*/
/*{{{  DvbStreamReturnDecodeBuffer*/
int DvbStreamReturnDecodeBuffer   (struct StreamContext_s*         Stream,
                                   buffer_handle_t*                Buffer)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))
        return -EINVAL;

    Result      = Backend->Ops->stream_return_decode_buffer (Stream->Handle, Buffer);
    if (Result < 0)
        BACKEND_ERROR("Failed to access decode buffer (%d)\n", Result);

    return Result;
}

/*}}}*/
/*{{{  DvbStreamGetDecodeBufferPoolStatus*/
int DvbStreamGetDecodeBufferPoolStatus     (struct StreamContext_s*         Stream,
                                            unsigned int*                   BuffersInPool,
                                            unsigned int*                   BuffersWithNonZeroReferenceCount)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))
        return -EINVAL;

    Result      = Backend->Ops->stream_get_decode_buffer_pool_status (Stream->Handle, BuffersInPool,BuffersWithNonZeroReferenceCount);
    if (Result < 0)
        BACKEND_ERROR("Failed to access decode buffer pool (%d)\n", Result);

    return Result;
}

/*}}}*/
/*{{{  DvbStreamSetOutputWindow*/
int DvbStreamSetOutputWindow   (struct StreamContext_s*         Stream,
                                unsigned int                    X,
                                unsigned int                    Y,
                                unsigned int                    Width,
                                unsigned int                    Height)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))           /* No stream to set id */
        return -EINVAL;

    Result      = Backend->Ops->stream_set_output_window (Stream->Handle, X, Y, Width, Height);
    if (Result < 0)
        BACKEND_ERROR("Unable to set output window (%d)\n", Result);

    return Result;
}
/*}}}*/
/*{{{  DvbStreamSetInputWindow*/
int DvbStreamSetInputWindow    (struct StreamContext_s*         Stream,
                                unsigned int                    X,
                                unsigned int                    Y,
                                unsigned int                    Width,
                                unsigned int                    Height)
{
    int         Result;

    if ((Stream == NULL) || (Stream->Handle == NULL))           /* No stream to set id */
        return -EINVAL;

    Result      = Backend->Ops->stream_set_input_window (Stream->Handle, X, Y, Width, Height);
    if (Result < 0)
        BACKEND_ERROR("Unable to set input window (%d)\n", Result);

    return Result;
}
/*}}}*/

/*{{{  DvbStreamIdentifyAudio*/
/*
   If the first three bytes of Header data contain a MPEG start code (0, 0, 1) then
   this function will use only the first four bytes of the header, otherwise this
   function assumes that there are at least 24 bytes of Header data to use to
   determine the type of the stream. It is the callers responsiblity to
   guarantee this (since we don't have a length argument).
*/
int DvbStreamIdentifyAudio     (struct StreamContext_s*         Stream,
                                unsigned int*                   Id)
{
    int                 Status  = 0;
    unsigned char*      Header  = Stream->Buffer;

    *Id         = AUDIO_ENCODING_NONE;
    /* first check for PES start code */
    if ((Header[0] == 0x00) && (Header[1] == 0x00) && (Header[2] == 0x01))
    {
        if (IS_PES_START_CODE_AUDIO(Header[3]))
            /* TODO: need to automagically detect MPEG layer (e.g. 0xfff vs. 0xffe) */
            *Id         = AUDIO_ENCODING_MPEG1;
        else if (IS_PES_START_CODE_PRIVATE_STREAM_1(Header[3]))
        {
            /* find the length of the PES header */
            unsigned char PesHeaderDataLength   = Header[8];
            if (PesHeaderDataLength > 15)
            {
                BACKEND_ERROR ("PES header data length is too long (%2x)\n", PesHeaderDataLength);
                Status  = -EINVAL;
            }
            else
            {
                /* extract the sub-stream identifier */
                unsigned char SubStreamIdentifier = Header[9 + PesHeaderDataLength];

                if (IS_PRIVATE_STREAM_1_AC3(SubStreamIdentifier))
                    *Id         = AUDIO_ENCODING_AC3;
                else if(IS_PRIVATE_STREAM_1_DTS(SubStreamIdentifier))
                    *Id         = AUDIO_ENCODING_DTS;
                else if (IS_PRIVATE_STREAM_1_LPCM(SubStreamIdentifier))
                    *Id         = AUDIO_ENCODING_LPCM;
                else if (IS_PRIVATE_STREAM_1_SDDS(SubStreamIdentifier))
                {
                    BACKEND_ERROR("Cannot decode SDDS audio\n");
                    Status      = -EINVAL;
                }
                else
                {
                    BACKEND_ERROR("Unexpected sub stream identifier in private data stream (%2x)\n", SubStreamIdentifier);
                    Status      = -EINVAL;
                }
            }
        }
        else
        {
            BACKEND_ERROR("Failed to determine PES data encoding (PES hdr 00 00 01 %02x)\n", Header[3]);
            Status      = -EINVAL;
        }
    }
    else if (memcmp(Header, ASFHeaderObjectGuid, 16) == 0)
    {
        *Id     = AUDIO_ENCODING_WMA;
        Status  = -EINVAL;
    }
    else
    {
        BACKEND_ERROR("Cannot identify Unknown stream format %02x %02x %02x %02x %02x %02x %02x %02x\n",
                      Header[0], Header[1], Header[2], Header[3], Header[4], Header[5], Header[6], Header[7]);
        Status  = -EINVAL;
    }

    return Status;
}
/*}}}*/
/*{{{  DvbStreamIdentifyVideo*/
int DvbStreamIdentifyVideo     (struct StreamContext_s*         Stream,
                                unsigned int*                   Id)
{
    int                 Status  = 0;
    unsigned char*      Header  = Stream->Buffer;

    *Id         = VIDEO_ENCODING_NONE;
    /* check for PES start code */
    if ((Header[0] == 0x00) && (Header[1] == 0x00) && (Header[2] == 0x01))
    {
        /*if (IS_PES_START_CODE_VIDEO(Header[3]))*/
            *Id         = VIDEO_ENCODING_MPEG2;
    }
    else
    {
        *Id             = VIDEO_ENCODING_MPEG2;
        /*
        BACKEND_ERROR("Cannot identify Unknown stream format %02x %02x %02x %02x %02x %02x %02x %02x\n",
                      Header[0], Header[1], Header[2], Header[3], Header[4], Header[5], Header[6], Header[7]);
        Status  = -EINVAL;
        */
    }

    return Status;
}
/*}}}*/
/*{{{  DvbStreamGetFirstBuffer*/
int DvbStreamGetFirstBuffer    (struct StreamContext_s*         Stream,
                                const char __user*              Buffer,
                                unsigned int                    Length)
{
    int         CopyAmount;

    mutex_lock (&(Stream->Lock));
    CopyAmount      = Stream->BufferLength;
    if (CopyAmount >= Length)
        CopyAmount  = Length;

    copy_from_user (Stream->Buffer, Buffer, CopyAmount);

    mutex_unlock (&(Stream->Lock));

    return CopyAmount;

}
/*}}}*/
/*{{{  DvbStreamGetPlayerEnvironment*/
int DvbStreamGetPlayerEnvironment          (struct StreamContext_s*     Stream,
                                            playback_handle_t*          playerplayback,
                                            stream_handle_t*            playerstream)
{
    int         Result;

    if (Stream == NULL) return -EINVAL;

    Result      = Backend->Ops->stream_get_player_environment (Stream->Handle, playerplayback, playerstream);
    if (Result < 0)
        BACKEND_ERROR("Unable to get player environment\n");

    return Result;
}
/*}}}*/
/*{{{  DvbDisplayDelete*/
int DvbDisplayDelete   (char*           Media,
                        unsigned int    SurfaceId)
{
    int         Result  = 0;

    if (Backend->Ops == NULL)
        return -ENODEV;

    BACKEND_DEBUG("Media %s, SurfaceId  = %d\n", Media, SurfaceId);

    Result = Backend->Ops->display_delete (Media, SurfaceId);
    if (Result < 0)
        BACKEND_ERROR("Failed to close %s surface\n", Media);

    return Result;
}
/*}}}*/

/*{{{  DisplaySynchronize*/
int DvbDisplaySynchronize   (char*           Media,
                             unsigned int    SurfaceId)
{
    int         Result  = 0;

    if (Backend->Ops == NULL)
        return -ENODEV;

    Result = Backend->Ops->display_synchronize (Media, SurfaceId);
    if (Result < 0)
        BACKEND_ERROR("Failed to synchronize %s surface\n", Media);

    return Result;
}
/*}}}*/


