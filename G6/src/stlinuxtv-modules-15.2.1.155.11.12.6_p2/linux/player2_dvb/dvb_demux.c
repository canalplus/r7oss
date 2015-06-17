/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
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

#include "dvb_demux.h"		/* provides kernel demux types */

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

/*{{{  StartFeed*/
/********************************************************************************
 *  \brief      Set up player to receive transport stream
 *              StartFeed is called by the demux device immediately before starting
 *              to demux data.
 ********************************************************************************/

/*
 * These constants come from enum dmx_ts_pes (dvb-core/demux.h). The
 * audio/video enums are clubbed together to find out easily which
 * one is coming and at what instance
 */
static const unsigned int AudioId[] =
    { DMX_TS_PES_AUDIO0, DMX_TS_PES_AUDIO1, DMX_TS_PES_AUDIO2,
	DMX_TS_PES_AUDIO3, DMX_TS_PES_AUDIO4, DMX_TS_PES_AUDIO5,
	DMX_TS_PES_AUDIO6, DMX_TS_PES_AUDIO7, DMX_TS_PES_AUDIO8,
	DMX_TS_PES_AUDIO9, DMX_TS_PES_AUDIO10, DMX_TS_PES_AUDIO11,
	DMX_TS_PES_AUDIO12, DMX_TS_PES_AUDIO13, DMX_TS_PES_AUDIO14,
	DMX_TS_PES_AUDIO15, DMX_TS_PES_AUDIO16, DMX_TS_PES_AUDIO17,
	DMX_TS_PES_AUDIO18, DMX_TS_PES_AUDIO19, DMX_TS_PES_AUDIO20,
	DMX_TS_PES_AUDIO21, DMX_TS_PES_AUDIO22, DMX_TS_PES_AUDIO23,
	DMX_TS_PES_AUDIO24, DMX_TS_PES_AUDIO25, DMX_TS_PES_AUDIO26,
	DMX_TS_PES_AUDIO27, DMX_TS_PES_AUDIO28, DMX_TS_PES_AUDIO29,
	DMX_TS_PES_AUDIO30, DMX_TS_PES_AUDIO31
};

static const unsigned int VideoId[] =
    { DMX_TS_PES_VIDEO0, DMX_TS_PES_VIDEO1, DMX_TS_PES_VIDEO2,
	DMX_TS_PES_VIDEO3, DMX_TS_PES_VIDEO4, DMX_TS_PES_VIDEO5,
	DMX_TS_PES_VIDEO6, DMX_TS_PES_VIDEO7, DMX_TS_PES_VIDEO8,
	DMX_TS_PES_VIDEO9, DMX_TS_PES_VIDEO10, DMX_TS_PES_VIDEO11,
	DMX_TS_PES_VIDEO12, DMX_TS_PES_VIDEO13, DMX_TS_PES_VIDEO14,
	DMX_TS_PES_VIDEO15, DMX_TS_PES_VIDEO16, DMX_TS_PES_VIDEO17,
	DMX_TS_PES_VIDEO18, DMX_TS_PES_VIDEO19, DMX_TS_PES_VIDEO20,
	DMX_TS_PES_VIDEO21, DMX_TS_PES_VIDEO22, DMX_TS_PES_VIDEO23,
	DMX_TS_PES_VIDEO24, DMX_TS_PES_VIDEO25, DMX_TS_PES_VIDEO26,
	DMX_TS_PES_VIDEO27, DMX_TS_PES_VIDEO28, DMX_TS_PES_VIDEO29,
	DMX_TS_PES_VIDEO30, DMX_TS_PES_VIDEO31
};

int StartFeed(struct dvb_demux_feed *Feed)
{
	struct dvb_demux *DvbDemux = Feed->demux;
	struct dmxdev_filter *Filter =
	    (struct dmxdev_filter *)Feed->feed.ts.priv;
	struct dmx_pes_filter_params *Params = &Filter->params.pes;
	struct PlaybackDeviceContext_s *Context =
	    (struct PlaybackDeviceContext_s *)DvbDemux->priv;
	struct VideoDeviceContext_s *VideoDecoderContext = NULL;
	struct AudioDeviceContext_s *AudioDecoderContext = NULL;
	struct DvbContext_s *DvbContext = Context->DvbContext;
	int Result = 0;
	int i;

	DVB_DEBUG("(demux%d)\n", Context->Id);

	switch (Feed->type) {
	case DMX_TYPE_TS:
		if ((Feed->pes_type == DMX_TS_PES_OTHER) ||
		    (Feed->pes_type < 0) || (Feed->pes_type >= DMX_TS_PES_LAST))
			return -EINVAL;

		/*
		 * Find out the audio decoder context if pes_type is
		 * one of audio instance
		 */
		for (i = 0; i < ARRAY_SIZE(AudioId); i++) {
			if (Feed->pes_type == AudioId[i]) {
				AudioDecoderContext = &DvbContext->AudioDeviceContext[i];
				break;
			}
		}

		/*
		 * If there's no audio decoder context, then checkout if its
		 * video decoder context
		 */
		if (!AudioDecoderContext) {
			for (i = 0; i < ARRAY_SIZE(VideoId); i++) {
				if (Feed->pes_type == VideoId[i]) {
					VideoDecoderContext = &DvbContext->VideoDeviceContext[i];
					break;
				}
			}
		}

		if (!AudioDecoderContext && !VideoDecoderContext) {
			printk(KERN_ERR "Invalid feed type passed for demux start\n");
			return -EINVAL;
		}

		mutex_lock(&Context->Playback_alloc_mutex);
		if (Context->Playback == NULL) {
			Result =
			    DvbPlaybackCreate(Context->Id,
					      &Context->Playback);
			if (Result < 0) {
				mutex_unlock(&Context->Playback_alloc_mutex);
				return Result;
			}
		}
		mutex_unlock(&Context->Playback_alloc_mutex);

		if (Context->DemuxStream == NULL) {
			Result =
			    DvbPlaybackAddDemux(Context->Playback,
						Context->DvbContext->
						DvbAdapter->num,
						Context->Id,
						&Context->DemuxStream);
			if (Result < 0) {
				return Result;
			}
		}

		if (VideoDecoderContext) {

			mutex_lock(&VideoDecoderContext->viddev_mutex);

			if (!VideoDecoderContext->VideoOpenWrite) {
				mutex_unlock(&VideoDecoderContext->viddev_mutex);
				return -EBADF;
			}

			/* Change playback of the video decoder */
			VideoDecoderContext->playback_context = Context;

			mutex_lock(&VideoDecoderContext->vidops_mutex);

			if (VideoDecoderContext->PlaySpeed != DVB_SPEED_NORMAL_PLAY) {

				Result = VideoIoctlSetSpeed(VideoDecoderContext,
							 VideoDecoderContext->PlaySpeed, 0);


				if (Result < 0) {
					printk(KERN_ERR
					       "%s: failed to set the proper play speed\n",
					       __func__);
					mutex_unlock(&VideoDecoderContext->vidops_mutex);
					mutex_unlock(&VideoDecoderContext->viddev_mutex);
					return Result;
				}
			}

			VideoIoctlSetId(VideoDecoderContext,
					Feed->pid | (Params->flags &
						     DMX_FILTER_BY_PRIORITY_MASK));
			VideoIoctlPlay(VideoDecoderContext);
			if ((VideoDecoderContext->VideoPlayInterval.start !=
			     DVB_TIME_NOT_BOUNDED)
			    || (VideoDecoderContext->VideoPlayInterval.end !=
				DVB_TIME_NOT_BOUNDED))
				VideoIoctlSetPlayInterval(VideoDecoderContext,
							  &VideoDecoderContext->VideoPlayInterval);

			mutex_unlock(&VideoDecoderContext->vidops_mutex);
			mutex_unlock(&VideoDecoderContext->viddev_mutex);

		} else {

			mutex_lock(&AudioDecoderContext->auddev_mutex);

			if (!AudioDecoderContext->AudioOpenWrite) {
				mutex_unlock(&AudioDecoderContext->auddev_mutex);
				return -EBADF;
			}

			/* Change playback of the audio decoder */
			AudioDecoderContext->playback_context = Context;

			mutex_lock(&AudioDecoderContext->audops_mutex);

			if (AudioDecoderContext->PlaySpeed != DVB_SPEED_NORMAL_PLAY) {


				Result = AudioIoctlSetSpeed(AudioDecoderContext,
							 AudioDecoderContext->PlaySpeed, 0);

				if (Result < 0) {
					printk(KERN_ERR
					       "%s: failed to set the proper play speed\n",
					       __func__);
					mutex_unlock(&AudioDecoderContext->audops_mutex);
					mutex_unlock(&AudioDecoderContext->auddev_mutex);
					return Result;
				}
			}

			AudioSetDemuxId(AudioDecoderContext,
					Feed->pid | (Params->flags &
						     DMX_FILTER_BY_PRIORITY_MASK));
			AudioIoctlPlay(AudioDecoderContext);
			if ((AudioDecoderContext->AudioPlayInterval.start !=
			     DVB_TIME_NOT_BOUNDED)
			    || (AudioDecoderContext->AudioPlayInterval.end !=
				DVB_TIME_NOT_BOUNDED))
				AudioIoctlSetPlayInterval(AudioDecoderContext,
							  &AudioDecoderContext->AudioPlayInterval);

			mutex_unlock(&AudioDecoderContext->audops_mutex);
			mutex_unlock(&AudioDecoderContext->auddev_mutex);

		}

		break;
	case DMX_TYPE_SEC:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*}}}*/
/*{{{  StopFeed*/
/********************************************************************************
 *  \brief      Shut down this feed
 *              StopFeed is called by the demux device immediately after finishing
 *              demuxing data.
 ********************************************************************************/
int StopFeed(struct dvb_demux_feed *Feed)
{
	struct dvb_demux *DvbDemux = Feed->demux;
	struct dmxdev_filter *Filter =
	    (struct dmxdev_filter *)Feed->feed.ts.priv;
	struct dmx_pes_filter_params *Params = &Filter->params.pes;
	struct PlaybackDeviceContext_s *Context =
	    (struct PlaybackDeviceContext_s *)DvbDemux->priv;
	struct DvbContext_s *DvbContext = Context->DvbContext;
	struct VideoDeviceContext_s *VideoDecoderContext = NULL;
	struct AudioDeviceContext_s *AudioDecoderContext = NULL;
	int i;

	switch (Feed->type) {
	case DMX_TYPE_TS:
		if ((Feed->pes_type == DMX_TS_PES_OTHER) ||
		    (Feed->pes_type < 0) || (Feed->pes_type >= DMX_TS_PES_LAST))
			return -EINVAL;

		for (i = 0; i < ARRAY_SIZE(AudioId); i++) {
			if (Feed->pes_type == AudioId[i]) {
				AudioDecoderContext = &DvbContext->AudioDeviceContext[i];
				break;
			}
		}

		if (!AudioDecoderContext) {
			for (i = 0; i < ARRAY_SIZE(VideoId); i++) {
				if (Feed->pes_type == VideoId[i]) {
					VideoDecoderContext = &DvbContext->VideoDeviceContext[i];
					break;
				}
			}
		}

		if (!AudioDecoderContext && !VideoDecoderContext) {
			printk(KERN_ERR "Invalid feed type passed for demux stop\n");
			return -EINVAL;
		}

		if ((VideoDecoderContext && !VideoDecoderContext->VideoOpenWrite) ||
		    (AudioDecoderContext && !AudioDecoderContext->AudioOpenWrite)) {
			return -EBADF;
		}

		if (VideoDecoderContext) {

			mutex_lock(&VideoDecoderContext->viddev_mutex);

			if (!VideoDecoderContext->VideoOpenWrite) {
				mutex_unlock(&VideoDecoderContext->viddev_mutex);
				return -EBADF;
			}

			mutex_lock(&VideoDecoderContext->vidops_mutex);

			VideoIoctlSetId(VideoDecoderContext,
					0xffff | (Params->
						  flags &
						  DMX_FILTER_BY_PRIORITY_MASK));
			VideoIoctlClearBuffer(VideoDecoderContext);

			mutex_unlock(&VideoDecoderContext->vidops_mutex);
			mutex_unlock(&VideoDecoderContext->viddev_mutex);

		} else if (AudioDecoderContext) {

			mutex_lock(&AudioDecoderContext->auddev_mutex);

			if (!AudioDecoderContext->AudioOpenWrite) {
				mutex_unlock(&AudioDecoderContext->auddev_mutex);
				return -EBADF;
			}

			mutex_lock(&AudioDecoderContext->audops_mutex);

			AudioSetDemuxId(AudioDecoderContext,
					0xffff | (Params->
						  flags &
						  DMX_FILTER_BY_PRIORITY_MASK));
			AudioIoctlClearBuffer(AudioDecoderContext);

			mutex_unlock(&AudioDecoderContext->audops_mutex);
			mutex_unlock(&AudioDecoderContext->auddev_mutex);

		}

		break;
	case DMX_TYPE_SEC:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*}}}*/
