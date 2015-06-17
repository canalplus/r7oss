/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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

static int DvrOpen(struct inode *Inode, struct file *File);
static int DvrRelease(struct inode *Inode, struct file *File);
static ssize_t DvrWrite(struct file *File,
			const char __user * Buffer,
			size_t Count, loff_t * ppos);

static struct file_operations OriginalDvrFops;
static struct file_operations DvrFops;

static struct dvb_device DvrDevice = {
priv:	NULL,
users:	1,
readers:1,
writers:1,
fops:	&DvrFops,
};

struct dvb_device *DvrInit(const struct file_operations *KernelDvrFops)
{
	/*
	 * Copy the fops table from the standard dvr fops.  We use our own versions of
	 * write, open and release so we can convert blueray content and detect when
	 * the device is opened and closed allowing us to create/destroy the player demux.
	 */
	memcpy(&OriginalDvrFops, KernelDvrFops, sizeof(struct file_operations));
	memcpy(&DvrFops, KernelDvrFops, sizeof(struct file_operations));

	DvrFops.owner = THIS_MODULE;
	DvrFops.open = DvrOpen;
	DvrFops.release = DvrRelease;
	DvrFops.write = DvrWrite;

	return &DvrDevice;
}

static int DvrOpen(struct inode *Inode, struct file *File)
{

	DVB_DEBUG("\n");

	return OriginalDvrFops.open(Inode, File);

}

static int DvrRelease(struct inode *Inode, struct file *File)
{
	struct dvb_device *DvbDevice = (struct dvb_device *)File->private_data;
	struct dmxdev *DmxDevice = (struct dmxdev *)DvbDevice->priv;
	struct dvb_demux *DvbDemux = (struct dvb_demux *)DmxDevice->demux->priv;
	struct PlaybackDeviceContext_s *Context =
	    (struct PlaybackDeviceContext_s *)DvbDemux->priv;
	int Result;

	DVB_DEBUG("\n");

	if (Context->DemuxStream != NULL) {
		Result =
		    DvbPlaybackRemoveDemux(Context->Playback,
					   Context->DemuxStream);
		if (Result) {
			printk(KERN_ERR
			       "%s: failed to remove demux from playback (%d)\n",
			       __func__, Result);
			return Result;
		}
		Context->DemuxStream = NULL;
		/*
		   if (Context != Context->DemuxContext)
		   Context->DemuxContext->DemuxStream  = NULL;
		 */
	}
	mutex_lock(&Context->Playback_alloc_mutex);
	/* Try to delete the playback - it will fail if someone is still using it */
	if (Context->Playback != NULL) {
		if (DvbPlaybackDelete(Context->Playback) == 0){
			DVB_TRACE("Playback deleted successfully\n");
			Context->Playback = NULL;
		}
	}
	mutex_unlock(&Context->Playback_alloc_mutex);

	return OriginalDvrFops.release(Inode, File);
}

static ssize_t DvrWrite(struct file *File, const char __user * Buffer,
			size_t Count, loff_t * ppos)
{
	struct dvb_device *DvbDevice = (struct dvb_device *)File->private_data;
	struct dmxdev *DmxDevice = (struct dmxdev *)DvbDevice->priv;
	struct dvb_demux *DvbDemux = (struct dvb_demux *)DmxDevice->demux->priv;
	struct PlaybackDeviceContext_s *Context =
	    (struct PlaybackDeviceContext_s *)DvbDemux->priv;
	int Result = 0;

	if (!DmxDevice->demux->write)
		return -EOPNOTSUPP;

	if ((File->f_flags & O_ACCMODE) != O_WRONLY)
		return -EINVAL;

#if 0				/* We should enable this for security reasons, just want to check the impact because it can be performed inline. */
	if (!access_ok(VERIFY_READ, Buffer, Count))
		return -EINVAL;
#endif

	if (mutex_lock_interruptible(&DmxDevice->mutex))
		return -ERESTARTSYS;

	if (Count == 0) {
		mutex_unlock(&DmxDevice->mutex);
		return 0;
	}

	/*
	 * Assume that we have a blueray packet if content size is divisible by 192 but not by 188
	 * If ordinary ts call demux write function on whole lot.  If bdts give data to write function
	 * 188 bytes at a time.
	 */
	if (((Count % TRANSPORT_PACKET_SIZE) == 0)
	    || ((Count % BLUERAY_PACKET_SIZE) != 0)) {
		/* Nicks modified version of chris's patch to reduce the injected size to chunks of a suitable size, and corectly accumulate the result as in blu-ray example below */
		size_t Transfer;
		size_t RemainingSize = Count;
		const char __user *BufferPointer = Buffer;
		while (RemainingSize != 0) {
			Transfer = min(RemainingSize, (size_t) (0x10000 - (0x10000 % TRANSPORT_PACKET_SIZE)));	/* limit to whole number of packets less than 64kb */
			Result +=
			    DmxDevice->demux->write(DmxDevice->demux,
						    BufferPointer, Transfer);

			RemainingSize -= Transfer;
			BufferPointer += Transfer;
		}

		mutex_unlock(&DmxDevice->mutex);

		if (Context->DemuxStream) {
			mutex_lock(&(Context->DemuxWriteLock));
			Result =
			    dvb_stream_inject(Context->DemuxStream, Buffer,
					    Count);
			mutex_unlock(&(Context->DemuxWriteLock));
		}

		return Result;

	} else if (Count % (192 * 32)) {
		int n;

		for (n = 0; n < Count; n += 192)
			Result +=
			    DmxDevice->demux->write(DmxDevice->demux,
						    &Buffer[n + 4],
						    TRANSPORT_PACKET_SIZE) + 4;
		mutex_unlock(&DmxDevice->mutex);

		if (Context->DemuxStream) {
			mutex_lock(&(Context->DemuxWriteLock));
			Result =
			    dvb_stream_inject(Context->DemuxStream, Buffer,
					    Count);
			mutex_unlock(&(Context->DemuxWriteLock));
		}
		return Result;
	} else {
		int n;

		/* We need to support partial injection, rather nastally unfortunatley */
		unsigned char *out = Context->dvr_out;

		Result = copy_from_user(Context->dvr_in, &Buffer[0], Count);
		if (Result) {
			mutex_unlock(&DmxDevice->mutex);
			return -EFAULT;
		}

		for (n = 0; n < Count; n += 192)
			Result +=
			    DmxDevice->demux->write(DmxDevice->demux,
						    &out[n + 4],
						    TRANSPORT_PACKET_SIZE) + 4;

		mutex_unlock(&DmxDevice->mutex);

		if (Context->DemuxStream) {
			mutex_lock(&(Context->DemuxWriteLock));
			Result =
			    dvb_stream_inject(Context->DemuxStream, &out[0],
					    Count);
			mutex_unlock(&(Context->DemuxWriteLock));
		}
		return Result;
	}
}
