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

Source file name : dvb_dvr.c
Author :           Julian

Implementation of linux dvb dvr input device

Date        Modification                                    Name
----        ------------                                    --------
01-Nov-06   Created                                         Julian

************************************************************************/

#include <linux/module.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include <linux/dvb/dmx.h>

#include "dvb_module.h"
#include "dvb_dvr.h"
#include "backend.h"

#include "dvb_v4l2.h"

static int      DvrOpen        (struct inode*           Inode,
				struct file*            File);
static int      DvrRelease     (struct inode*           Inode,
				struct file*            File);
static ssize_t  DvrWrite       (struct file*            File,
				const char __user*      Buffer,
				size_t                  Count,
				loff_t*                 ppos);


static struct file_operations   OriginalDvrFops;
static struct file_operations   DvrFops;

static struct dvb_device        DvrDevice =
{
	priv:            NULL,
	users:           1,
	readers:         1,
	writers:         1,
	fops:            &DvrFops,
};

struct dvb_device* DvrInit (struct file_operations* KernelDvrFops)
{
    /*
     * Copy the fops table from the standard dvr fops.  We use our own versions of
     * write, open and release so we can convert blueray content and detect when
     * the device is opened and closed allowing us to create/destroy the player demux.
     */
    memcpy (&OriginalDvrFops, KernelDvrFops, sizeof (struct file_operations));
    memcpy (&DvrFops, KernelDvrFops, sizeof (struct file_operations));

    DvrFops.owner                      = THIS_MODULE;
    DvrFops.open                       = DvrOpen;
    DvrFops.release                    = DvrRelease;
    DvrFops.write                      = DvrWrite;

    return &DvrDevice;
}


static int DvrOpen     (struct inode*     Inode,
			struct file*      File)
{
    struct dvb_device*          DvbDevice       = (struct dvb_device*)File->private_data;
    struct dmxdev*              DmxDevice       = (struct dmxdev*)DvbDevice->priv;
    struct dvb_demux*           DvbDemux        = (struct dvb_demux*)DmxDevice->demux->priv;
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)DvbDemux->priv;

    DVB_DEBUG("\n");
#if 0
    if ((Context->Playback == NULL) && (Context->DemuxContext->Playback == NULL))
    {
	Result      = DvbPlaybackCreate (&Context->Playback);
	if (Result < 0)
	    return Result;
	Result      = DvbPlaybackSetSpeed (Context->Playback, Context->PlaySpeed);
	if (Result < 0)
	    return Result;
	if (Context != Context->DemuxContext)
	    Context->DemuxContext->Playback    = Context->Playback;
    }
    if ((Context->DemuxStream == NULL) && (Context->DemuxContext->DemuxStream == NULL))
    {
	Result      = DvbPlaybackAddDemux (Context->Playback, Context->DemuxContext->Id, &Context->DemuxStream);
	if (Result < 0)
	    return Result;
	if (Context != Context->DemuxContext)
	    Context->DemuxContext->DemuxStream  = Context->DemuxStream;
    }
#endif

    Context->StartOffset = -1;
    Context->EndOffset   = -1;

    return OriginalDvrFops.open (Inode, File);

}


static int DvrRelease  (struct inode*  Inode,
			struct file*   File)
{
    struct dvb_device*          DvbDevice       = (struct dvb_device*)File->private_data;
    struct dmxdev*              DmxDevice       = (struct dmxdev*)DvbDevice->priv;
    struct dvb_demux*           DvbDemux        = (struct dvb_demux*)DmxDevice->demux->priv;
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)DvbDemux->priv;
    int                         Result          = 0;

    DVB_DEBUG("\n");

    if (Context->DemuxStream != NULL)
    {
	Result      = DvbPlaybackRemoveDemux (Context->Playback, Context->DemuxStream);
	Context->DemuxStream                    = NULL;
	/*
	if (Context != Context->DemuxContext)
	    Context->DemuxContext->DemuxStream  = NULL;
	*/
    }

    /* Check to see if audio and video have also finished so we can release the playback */
    if ((Context->AudioStream == NULL) && (Context->VideoStream == NULL) && (Context->Playback != NULL))
    {
	/* Check to see if our playback has already been deleted by the demux context */
	if (Context->DemuxContext->Playback != NULL)
	{
	    /* Try and delete playback then set our demux to Null if succesful or not.  If we fail someone else
	       is still using it but we are done. */
	    if (DvbPlaybackDelete (Context->Playback) == 0)
		DVB_TRACE("Playback deleted successfully\n");
	}

	Context->Playback                       = NULL;
	Context->StreamType                     = STREAM_TYPE_TRANSPORT;
	Context->PlaySpeed                      = DVB_SPEED_NORMAL_PLAY;
	Context->AudioPlayInterval.start        = DVB_TIME_NOT_BOUNDED;
	Context->AudioPlayInterval.end          = DVB_TIME_NOT_BOUNDED;
	Context->VideoPlayInterval.start        = DVB_TIME_NOT_BOUNDED;
	Context->VideoPlayInterval.end          = DVB_TIME_NOT_BOUNDED;
	Context->SyncContext                    = Context;
    }

    Context->StreamType         = STREAM_TYPE_TRANSPORT;

    return OriginalDvrFops.release (Inode, File);
}

static ssize_t DvrWrite (struct file *File, const char __user* Buffer, size_t Count, loff_t* ppos)
{
    struct dvb_device*          DvbDevice       = (struct dvb_device*)File->private_data;
    struct dmxdev*              DmxDevice       = (struct dmxdev*)DvbDevice->priv;
    struct dvb_demux*           DvbDemux        = (struct dvb_demux*)DmxDevice->demux->priv;
    struct DeviceContext_s*     Context         = (struct DeviceContext_s*)DvbDemux->priv;
    int                         Result          = 0;

    if (!DmxDevice->demux->write)
	return -EOPNOTSUPP;

    if ((File->f_flags & O_ACCMODE) != O_WRONLY)
	return -EINVAL;

    if (mutex_lock_interruptible (&DmxDevice->mutex))
	return -ERESTARTSYS;

#if 0 // We should enable this for security reasons, just want to check the impact because it can be performed inline.
    if (!access_ok(VERIFY_READ, Buffer, Count))
	return -EINVAL;
#endif

    /*
     * Assume that we have a blueray packet if content size is divisible by 192 but not by 188
     * If ordinary ts call demux write function on whole lot.  If bdts give data to write function
     * 188 bytes at a time.
     */
    if ((((Count % TRANSPORT_PACKET_SIZE) == 0) || ((Count % BLUERAY_PACKET_SIZE) != 0)) && !Context->EncryptionOn)
    {
	// Nicks modified version of chris's patch to reduce the injected size to chunks of a suitable size, and corectly accumulate the result as in blu-ray example below
	size_t                  Transfer;
	size_t                  RemainingSize   = Count;
	const char __user*      BufferPointer   = Buffer;
	while( RemainingSize != 0 )
	{
	    Transfer             = min( RemainingSize, (0x10000 - (0x10000 % TRANSPORT_PACKET_SIZE)) );         // limit to whole number of packets less than 64kb
	    Result              += DmxDevice->demux->write (DmxDevice->demux, BufferPointer, Transfer);

	    RemainingSize       -= Transfer;
	    BufferPointer       += Transfer;
	}

	mutex_unlock (&DmxDevice->mutex);

	if (Context->DemuxStream)
	{
		mutex_lock (&(Context->VideoWriteLock));
		Result  = DvbStreamInject (Context->DemuxStream, Buffer, Count);
		mutex_unlock (&(Context->VideoWriteLock));
	}

	return Result;

    }
    else if ( (Count % (192*32)) || (!Context->EncryptionOn))
    {
	int n;

	for (n=0;n<Count;n+=192)
		Result += DmxDevice->demux->write (DmxDevice->demux, &Buffer[n+4], TRANSPORT_PACKET_SIZE) + 4;
	mutex_unlock (&DmxDevice->mutex);

	if (Context->DemuxStream)
	{
		mutex_lock (&(Context->VideoWriteLock));
		Result  = DvbStreamInject (Context->DemuxStream, Buffer, Count);
		mutex_unlock (&(Context->VideoWriteLock));
	}
	return Result;
    }
    else
    {
	int n;
	unsigned long ptr = (unsigned long)stm_v4l2_findbuffer((unsigned long)Buffer,Count,0);

	// We need to support partial injection, rather nastally unfortunatley
	unsigned char *out = Context->dvr_out;
	unsigned int Size  = Count;
	int StartOffset = 0;

	if (ptr && (ptr & ~31))
	{
		//tkdma_bluray_decrypt_data(&Context->dvr_out[0],(void*)ptr,Count / (32*192),0); // Need to do an interruptibility test
	} else {
	  copy_from_user(Context->dvr_in,&Buffer[0],Count);
	  //copy_from_user(&Context->dvr_out[16],Buffer,16);
	  //tkdma_bluray_decrypt_data(&Context->dvr_out[0],Context->dvr_in,Count / (32*192),0); //Need to do an interruptibility test       
	}

	if (Context->StartOffset>0 && Context->StartOffset < Count)
	{
		out += Context->StartOffset;
		Size -= Context->StartOffset;
		StartOffset = Context->StartOffset;
		Context->StartOffset = -1;
	}

	if (Context->EndOffset>0 && Context->EndOffset < Count && Context->EndOffset > StartOffset)
	{
		Size -= Context->EndOffset - StartOffset;
		Context->EndOffset = -1;
	}

#if 1
	for (n=0;n<Size;n+=192)
	     Result += DmxDevice->demux->write (DmxDevice->demux, &out[n+4], TRANSPORT_PACKET_SIZE) + 4;
#endif

	mutex_unlock (&DmxDevice->mutex);

	if (Context->DemuxStream)
	{
		mutex_lock (&(Context->VideoWriteLock));
		Result  = DvbStreamInject (Context->DemuxStream, &out[0], Size);
		mutex_unlock (&(Context->VideoWriteLock));
	}
	return Result;
    }

}

