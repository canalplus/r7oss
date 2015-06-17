/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : dvb_demux.c
Author :           Julian

Implementation of linux dvb demux hooks

Date        Modification                                    Name
----        ------------                                    --------
01-Nov-06   Created                                         Julian

************************************************************************/

#include <linux/module.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>

#include "dvb_demux.h"          /* provides kernel demux types */

#include "dvb_module.h"
#include "dvb_audio.h"
#include "dvb_video.h"
#include "dvb_dmux.h"
#include "backend.h"

/********************************************************************************
 *  This file contains the hook functions which allow the player to use the built-in
 *  kernel demux device so that in-mux non audio/video streams can be read out of
 *  the demux device.
 ********************************************************************************/

/*{{{  COMMENT DmxWrite*/
#if 0
/********************************************************************************
 *  \brief      Write user data into player and kernel filter engine
 *              DmxWrite is called by the dvr device write function.  It allows us
 *              to intercept data writes from the user and de blue ray them.
 *              Data is injected into the kernel first to preserve user context.
 ********************************************************************************/
int DmxWrite (struct dmx_demux* Demux, const char* Buffer, size_t Count)
{
    size_t                      DataLeft        = Count;
    int                         Result          = 0;
    unsigned int                Offset          = 0;
    unsigned int                Written         = 0;
    struct dvb_demux*           DvbDemux        = (struct dvb_demux*)Demux->priv;
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)DvbDemux->priv;

    if (((Count % TRANSPORT_PACKET_SIZE) == 0) || ((Count % BLUERAY_PACKET_SIZE) != 0))
        Context->DmxWrite (Demux, Buffer, Count);
    else
    {
        Offset      = sizeof(unsigned int);
        while (DataLeft > 0)
        {
            Result          = Context->DmxWrite (Demux, Buffer+Offset, TRANSPORT_PACKET_SIZE);
            Offset         += BLUERAY_PACKET_SIZE;
            DataLeft       -= BLUERAY_PACKET_SIZE;
            if (Result < 0)
                return Result;
            else if (Result != TRANSPORT_PACKET_SIZE)
                return Written + Result;
            else
                Written    += BLUERAY_PACKET_SIZE;
        }
    }

    return DemuxInjectFromUser (Context->DemuxStream, Buffer, Count);  /* Pass data to player before putting into the demux */

}
#endif
/*}}}*/
#if 0
/*{{{  StartFeed*/
/********************************************************************************
 *  \brief      Set up player to receive transport stream
 *              StartFeed is called by the demux device immediately before starting
 *              to demux data.
 ********************************************************************************/
static const unsigned int AudioId[DVB_MAX_DEVICES_PER_ADAPTER]  = {DMX_TS_PES_AUDIO0, DMX_TS_PES_AUDIO1, DMX_TS_PES_AUDIO2, DMX_TS_PES_AUDIO3};
static const unsigned int VideoId[DVB_MAX_DEVICES_PER_ADAPTER]  = {DMX_TS_PES_VIDEO0, DMX_TS_PES_VIDEO1, DMX_TS_PES_VIDEO2, DMX_TS_PES_VIDEO3};
int StartFeed (struct dvb_demux_feed* Feed)
{
    struct dvb_demux*           DvbDemux        = Feed->demux;
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)DvbDemux->priv;
    struct DvbContext_s*        DvbContext      = Context->DvbContext;
    int                         Result          = 0;
    int                         i;
    unsigned int                Video           = false;
    unsigned int                Audio           = false;

    if (Feed->pes_type > DMX_TS_PES_OTHER)
        return -EINVAL;

    switch (Feed->type)
    {
        case DMX_TYPE_TS:
            for (i = 0; i < DVB_MAX_DEVICES_PER_ADAPTER; i++)
            {
                if (Feed->pes_type == AudioId[i])
                {
                    Audio       = true;
                    break;
                }
                if (Feed->pes_type == VideoId[i])
                {
                    Video       = true;
                    break;
                }
            }
            if (!Audio && !Video)
                return 0;

            mutex_lock (&(DvbContext->Lock));
            if ((Context->Playback == NULL) && (Context->DemuxContext->Playback == NULL))
            {
                Result      = DvbPlaybackCreate (&Context->Playback);
                if (Result < 0)
                    return Result;
                if (Context != Context->DemuxContext)
                    Context->DemuxContext->Playback    = Context->Playback;
            }
            if ((Context->DemuxStream == NULL) && (Context->DemuxContext->DemuxStream == NULL))
            {
                Result      = DvbPlaybackAddDemux (Context->Playback, Context->DemuxContext->Id, &Context->DemuxStream);
                if (Result < 0)
                {
                    mutex_unlock (&(DvbContext->Lock));
                    return Result;
                }
                if (Context != Context->DemuxContext)
                    Context->DemuxContext->DemuxStream  = Context->DemuxStream;
            }

            if (Video)
            {
                struct DeviceContext_s* VideoContext    = &Context->DvbContext->DeviceContext[i];

                VideoContext->DemuxContext      = Context;
                VideoIoctlSetId (VideoContext, Feed->pid);
                VideoIoctlPlay (VideoContext);
            }
            else
            {
                struct DeviceContext_s* AudioContext    = &Context->DvbContext->DeviceContext[i];

                AudioContext->DemuxContext      = Context;
                AudioIoctlSetId (AudioContext, Feed->pid);
                AudioIoctlPlay (AudioContext);
            }
            mutex_unlock (&(DvbContext->Lock));

            break;
        case DMX_TYPE_SEC:
            break;
        default:
            return -EINVAL;
    }

    return 0;
}
/*}}}*/
#else
/*{{{  StartFeed*/
/********************************************************************************
 *  \brief      Set up player to receive transport stream
 *              StartFeed is called by the demux device immediately before starting
 *              to demux data.
 ********************************************************************************/
static const unsigned int AudioId[DVB_MAX_DEVICES_PER_ADAPTER]  = {DMX_TS_PES_AUDIO0, DMX_TS_PES_AUDIO1, DMX_TS_PES_AUDIO2, DMX_TS_PES_AUDIO3};
static const unsigned int VideoId[DVB_MAX_DEVICES_PER_ADAPTER]  = {DMX_TS_PES_VIDEO0, DMX_TS_PES_VIDEO1, DMX_TS_PES_VIDEO2, DMX_TS_PES_VIDEO3};
int StartFeed (struct dvb_demux_feed* Feed)
{
    struct dvb_demux*                   DvbDemux        = Feed->demux;
    struct dmxdev_filter*               Filter          = (struct dmxdev_filter*)Feed->feed.ts.priv;
    struct dmx_pes_filter_params*       Params          = &Filter->params.pes;
    struct DeviceContext_s*             Context         = (struct DeviceContext_s*)DvbDemux->priv;
    struct DvbContext_s*                DvbContext      = Context->DvbContext;
    int                                 Result          = 0;
    int                                 i;
    unsigned int                        Video           = false;
    unsigned int                        Audio           = false;

    DVB_DEBUG ("(demux%d)\n", Context->Id);

    switch (Feed->type)
    {
        case DMX_TYPE_TS:
            if (Feed->pes_type > DMX_TS_PES_OTHER)
                return -EINVAL;

            for (i = 0; i < DVB_MAX_DEVICES_PER_ADAPTER; i++)
            {
                if (Feed->pes_type == AudioId[i])
                {
                    Audio       = true;
                    break;
                }
                if (Feed->pes_type == VideoId[i])
                {
                    Video       = true;
                    break;
                }
            }
            if (!Audio && !Video)
            {
                /*mutex_unlock (&(DvbContext->Lock));  This doesn't look right we haven't taken it yet*/
                return 0;
            }

            mutex_lock (&(DvbContext->Lock));
            if ((Video && !Context->VideoOpenWrite) || (Audio && !Context->AudioOpenWrite))
            {
                mutex_unlock (&(DvbContext->Lock));
                return -EBADF;
            }

            if ((Context->Playback == NULL) && (Context->SyncContext->Playback == NULL))
            {
                Result      = DvbPlaybackCreate (&Context->Playback);
                if (Result < 0)
                {
                    mutex_unlock (&(DvbContext->Lock));
                    return Result;
                }
                Context->SyncContext->Playback      = Context->Playback;
                if (Context->PlaySpeed != DVB_SPEED_NORMAL_PLAY)
                {
                    Result      = VideoIoctlSetSpeed (Context, Context->PlaySpeed);
                    if (Result < 0)
                        return Result;
                }
            }
            else if (Context->Playback == NULL)
                Context->Playback               = Context->SyncContext->Playback;
            else if (Context->SyncContext->Playback == NULL)
                Context->SyncContext->Playback  = Context->Playback;
            else if (Context->Playback != Context->SyncContext->Playback)
                DVB_ERROR ("Context playback not equal to sync context playback\n");

            if (Context->DemuxStream == NULL)
            {
                Result      = DvbPlaybackAddDemux (Context->Playback, Context->DemuxContext->Id, &Context->DemuxStream);
                if (Result < 0)
                {
                    mutex_unlock (&(DvbContext->Lock));
                    return Result;
                }
            }

            if (Video)
            {
                struct DeviceContext_s* VideoContext    = &Context->DvbContext->DeviceContext[i];

                VideoContext->DemuxContext      = Context;
                VideoIoctlSetId (VideoContext, Feed->pid | (Params->flags & DMX_FILTER_BY_PRIORITY_MASK));
                VideoIoctlPlay (VideoContext);
                if ((Context->VideoPlayInterval.start != DVB_TIME_NOT_BOUNDED) ||
                    (Context->VideoPlayInterval.end   != DVB_TIME_NOT_BOUNDED))
                    VideoIoctlSetPlayInterval (Context, &Context->AudioPlayInterval);
            }
            else
            {
                struct DeviceContext_s*         AudioContext    = &Context->DvbContext->DeviceContext[i];

                AudioContext->DemuxContext      = Context;
                AudioIoctlSetId (AudioContext, Feed->pid | (Params->flags & DMX_FILTER_BY_PRIORITY_MASK));
                AudioIoctlPlay (AudioContext);
                if ((Context->AudioPlayInterval.start != DVB_TIME_NOT_BOUNDED) ||
                    (Context->AudioPlayInterval.end   != DVB_TIME_NOT_BOUNDED))
                    AudioIoctlSetPlayInterval (Context, &Context->AudioPlayInterval);
            }
            mutex_unlock (&(DvbContext->Lock));

            break;
        case DMX_TYPE_SEC:
            break;
        default:
            return -EINVAL;
    }

    return 0;
}
/*}}}*/
#endif
/*{{{  StopFeed*/
/********************************************************************************
 *  \brief      Shut down this feed
 *              StopFeed is called by the demux device immediately after finishing
 *              demuxing data.
 ********************************************************************************/
int StopFeed (struct dvb_demux_feed* Feed)
{
    struct dvb_demux*           DvbDemux        = Feed->demux;
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)DvbDemux->priv;
    struct DvbContext_s*        DvbContext      = Context->DvbContext;
    /*int                         Result          = 0;*/

    switch (Feed->type)
    {
        case DMX_TYPE_TS:
            mutex_lock (&(DvbContext->Lock));

            if (((Feed->pes_type == DMX_TS_PES_VIDEO) && !Context->VideoOpenWrite) ||
                ((Feed->pes_type == DMX_TS_PES_AUDIO) && !Context->AudioOpenWrite))
            {
                mutex_unlock (&(DvbContext->Lock));
                return -EBADF;
            }

            switch (Feed->pes_type)
            {
                case DMX_TS_PES_VIDEO:
                    VideoIoctlStop (Context, Context->VideoState.video_blank);
                    break;
                case DMX_TS_PES_AUDIO:
                    AudioIoctlStop (Context);
                    break;
                case DMX_TS_PES_TELETEXT:
                case DMX_TS_PES_PCR:
                case DMX_TS_PES_OTHER:
                    break;
                default:
                    mutex_unlock (&(DvbContext->Lock));
                    return -EINVAL;
            }
            mutex_unlock (&(DvbContext->Lock));
            /*
            if ((Context->AudioId == DEMUX_INVALID_ID) && (Context->VideoId == DEMUX_INVALID_ID) &&
                (Context->DemuxStream != NULL))
            {
                Result      = DvbPlaybackRemoveDemux (Context->Playback, Context->DemuxStream);
                Context->DemuxContext->DemuxStream      = NULL;
                if (Context != Context->DemuxContext)
                    Context->DemuxContext->DemuxStream  = NULL;
            }
            */
            break;
        case DMX_TYPE_SEC:
            break;
        default:
            return -EINVAL;
    }
    return 0;
}
/*}}}*/

