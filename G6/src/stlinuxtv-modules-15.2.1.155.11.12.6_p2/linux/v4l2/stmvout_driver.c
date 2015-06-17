/***********************************************************************
 *
 * File: linux/kernel/drivers/media/video/stmvout_driver.c
 * Copyright (c) 2000-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * V4L2 video output device driver for ST SoC display subsystems.
 *
\***********************************************************************/

#include <linux/slab.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-common.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <asm/page.h>

/* For bpa2 buffer access from user space, we must have the access file ops. */
#include <linux/bpa2.h>
#include <asm/io.h>
#if defined(CONFIG_ARM) || defined(CONFIG_HCE_ARCH_ARM)
#include <asm/mach/map.h>
#endif

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>

#include "stmedia.h"
#include "linux/stm/stmedia_export.h"
#include "stmvout_driver.h"
#include "linux/stmvout.h"
#include "stmvout_mc.h"
#include "linux/dvb/dvb_v4l2_export.h"
#include "stm_v4l2_common.h"

/* #include "stm_v4l2.h" */

#if 0				/* no longer */
#include "__out_driver.h"
static int video_nr = -1;
module_param(video_nr, int, 0444);
#endif

#undef DEBUG_SHOW_OUTPUTS
#undef DEBUG_SHOW_PLANES

/* Global structures
 * Should be converted to double linked list sometime in the future...
 * also suggest to change voutData in voutPlane
 *
 *     g_voutPlug: describes the Display output entitites
 *     g_voutRoot: describes the root entity (/dev/video1) of stvout
 *                 device. This structure contains all the MC pads
 *                 connecting Display Planes.
 *
 *     TODO: info and queues relevant to buffer management and
 *           user open controls, should be moved to /dev/video1
 *           entity.
 *           Names should be more evocative.
 */
static struct output_plug g_voutPlug[STMEDIA_MAX_OUTPUTS];

struct stm_v4l2_output_root {
	struct video_device video1;
	struct media_pad pads[STMEDIA_MAX_PLANES];	/* all SOURCE pads */
	struct _stvout_device pDev[STMEDIA_MAX_PLANES];	/* shortcut to real output device */
};

static struct stm_v4l2_output_root g_voutRoot;	/* /dev/video1 */

struct stm_v4l2_vout_fh {
	struct v4l2_fh fh;
	open_data_t *pOpenData;
};

struct format_info {
	stm_pixel_capture_format_t cap_format;
	stm_pixel_capture_color_space_t color_space;
	int depth;
	stm_pixel_format_t plane_format;
};

/*
 * Formats are classified by priority order where YCbCr888 is the recommanded
 * fmt.
 */
static const struct format_info format_mapping[] = {
	{STM_PIXEL_FORMAT_YUV,
	 STM_PIXEL_CAPTURE_BT709,
	 24,			/* bits per pixel for luma only */
	 SURF_CRYCB888},

	{STM_PIXEL_FORMAT_YCbCr422R,
	 STM_PIXEL_CAPTURE_BT709,
	 16,			/* bits per pixel for luma only */
	 SURF_YCBCR422R},

	{STM_PIXEL_FORMAT_RGB888,
	 STM_PIXEL_CAPTURE_RGB_VIDEORANGE,
	 24,
	 SURF_RGB888},

	{STM_PIXEL_FORMAT_ARGB8888,
	 STM_PIXEL_CAPTURE_RGB_VIDEORANGE,
	 32,
	 SURF_ARGB8888},

	{STM_PIXEL_FORMAT_ARGB8565,
	 STM_PIXEL_CAPTURE_RGB_VIDEORANGE,
	 32,
	 SURF_ARGB8565},

	{STM_PIXEL_FORMAT_RGB565,
	 STM_PIXEL_CAPTURE_RGB_VIDEORANGE,
	 16,
	 SURF_RGB565},

	{STM_PIXEL_FORMAT_ARGB1555,
	 STM_PIXEL_CAPTURE_RGB_VIDEORANGE,
	 16,
	 SURF_ARGB1555},

	{STM_PIXEL_FORMAT_ARGB4444,
	 STM_PIXEL_CAPTURE_RGB_VIDEORANGE,
	 16,
	 SURF_ARGB4444},

	{STM_PIXEL_FORMAT_YUV_NV16,
	 STM_PIXEL_CAPTURE_BT709,
	 8,			/* bits per pixel for luma only */
	 SURF_YCBCR422MB},

	{STM_PIXEL_FORMAT_YUV_NV12,
	 STM_PIXEL_CAPTURE_BT709,
	 8,			/* bits per pixel for luma only */
	 SURF_YCBCR420MB},
};

/* ----------------------------------------------------------------------- */

static int init_device_locked(struct _stvout_device *const device)
{
	int ret;

	stmvout_init_buffer_queues(device);

	if ((ret = stmvout_allocate_clut(device)) < 0)
		return ret;

	/* Note: with Media Controller, at this point, Display planes are
	 *       already opened (display plane handle is valid).
	 */

	return 0;
}

static int
stmvout_close_locked(struct _open_data *open_data,
		     struct _stvout_device *device)
{
	unsigned long flags;

	if (open_data->isFirstUser) {
		debug_msg("%s: turning off streaming and releasing buffers\n",
			  __FUNCTION__);

		/* Discard the pending queue */
		debug_msg("%s: delete buffers from pending queue\n",
			  __FUNCTION__);
		write_lock_irqsave(&device->pendingStreamQ.lock, flags);
		stmvout_delete_buffers_from_list(&device->pendingStreamQ.list);
		write_unlock_irqrestore(&device->pendingStreamQ.lock, flags);

		/* Turn off streaming in case it's on. */
		if (stmvout_streamoff(device) < 0) {
			if (signal_pending(current))
				return -ERESTARTSYS;

			printk(KERN_ERR
			       "Failed to turn off streaming but there wasn't a signal pending, that should not happen");
			BUG();
		}

		if (!stmvout_deallocate_mmap_buffers(device)) {
			printk(KERN_ERR
			       "failed to unmap/deallocate buffers!\n");
			BUG();
		}

	}

	if (device->open_count == 1) {
		debug_msg("%s: last user closed device\n", __FUNCTION__);
		/*
		 * With Media Controller, Display planes are kept always open since
		 * the construction of entities and MC graph (init stage).
		 * Should never close the plane handle
		 */

		stmvout_deallocate_clut(device);
	}

	device->open_count--;
	open_data->isOpen = 0;
	open_data->isFirstUser = 0;
	open_data->pDev = NULL;
	open_data->padId = 0;

	return 0;
}

static int stmvout_close(struct file *file)
{
	struct v4l2_fh *fh = file->private_data;
	struct stm_v4l2_vout_fh *vout_fh = container_of(fh, struct stm_v4l2_vout_fh, fh);
	open_data_t *pOpenData = vout_fh->pOpenData;

	struct _stvout_device *device;
	int ret = 0;

	if (!pOpenData) {
		debug_msg("%s BUG: Opendata not set!\n", __FUNCTION__);
		goto free_fh;
	}

	debug_msg("%s in.\n", __FUNCTION__);

	/* get shortcut to output plane */
	/* was: device = pOpenData->pDev; */
	device = pOpenData->pDev;

	/* Note that this is structured to allow the call to be interrupted by a
	   signal and then restarted without corrupting the internal state. */
	if (down_interruptible(&device->devLock))
		return -ERESTARTSYS;

	ret = stmvout_close_locked(pOpenData, device);

	list_del(&pOpenData->open_list);
	up(&device->devLock);

	kfree(pOpenData);

free_fh:
	/*
	 * Remove V4L2 file handlers
	 */
	v4l2_fh_del(fh);
	v4l2_fh_exit(fh);

	file->private_data = NULL;
	kfree(vout_fh);

	debug_msg("%s out.\n", __FUNCTION__);

	return ret;
}

/* Media Controller Helper:
 * Starting from Plane descriptor, follows the ENABLED link which
 * connects the Plane to Display output sub-device and gets the
 * output Display handle.
 */
stm_display_output_h stmvout_get_output_display_handle(stvout_device_t * pDev)
{
	struct media_pad *downStream_pad = NULL;
	struct output_plug *outputPlug;

	/* via Media Controller get the SINK (downstream) entity connected
	 * to the source PAD of this entity (pDev)
	 */
	downStream_pad =
	    media_entity_remote_source(&pDev->pads[SOURCE_PAD_INDEX]);
	if (downStream_pad == NULL)
		return NULL;	/* link doesn't exists or is not ENABLED */

	/* having the output media entity, get its container! */
	outputPlug = output_entity_to_output_plug(downStream_pad->entity);

	/* and finally return the Display output handle */
	return (outputPlug->hOutput);
}

static int stmvout_get_interface( stm_display_source_h source,
					 stm_display_source_interface_params_t *iface_params,
					 void **hiface)
{
	int ret = 0;

	ret = stm_display_source_get_interface(source,
					       *iface_params,
					       hiface);
	if (ret) {
		debug_msg("%s: get queue interface failed\n",
			  __PRETTY_FUNCTION__);
		*hiface = NULL;
		return signal_pending(current) ? -ERESTARTSYS : ret;
	}

	return 0;
}

static int stmvout_release_interface( void *hiface,
			stm_display_source_interfaces_t interface_type)
{

	if(interface_type == STM_SOURCE_QUEUE_IFACE)
		return stm_display_source_queue_release(hiface);
	else
		return stm_display_source_pixelstream_release(hiface);
}

static int stmvout_get_iface_source( stm_display_device_h hDisplay,
					     stm_display_plane_h hPlane,
					     stm_display_source_interface_params_t *iface_params,
					     struct stm_source_info *src_info)
{
	int ret = 0;
	uint32_t sourceID = 0;
	uint32_t status = 0;
	void *hiface;
	stm_display_source_h hSource;

	for(sourceID = 0;;sourceID++){
		ret = stm_display_device_open_source(hDisplay,
						    sourceID,
						    &hSource);
		if (ret){
			if (ret == -ENODEV) {
				debug_msg
				    ("%s: Failed to get a source for plane \n",
				     __FUNCTION__);
				return -EBUSY;
			}
			return signal_pending(current) ? -ERESTARTSYS : ret;
		}

		/* Try to get an interface on this source */
		ret = stmvout_get_interface( hSource, iface_params,
						    &hiface);
		if (ret) {
			debug_msg("%s: get interface failed\n",
				  __PRETTY_FUNCTION__);
			stm_display_source_close(hSource);

			if (ret == -EOPNOTSUPP)
				continue;

			return signal_pending(current) ? -ERESTARTSYS : ret;
		}

		ret = stm_display_source_get_status(hSource, &status);
		if ((ret != 0) ||
			((ret == 0)
			&& (status & STM_STATUS_SOURCE_CONNECTED))) {
			/* This source is already used to feed another plane */
			stmvout_release_interface(hiface, iface_params->interface_type);
			stm_display_source_close(hSource);
			continue;
		}

		/* try to connect the source on the prefered plane */
		ret = stm_display_plane_connect_to_source(hPlane,
							  hSource);
		if (ret) {
			/* Cleanup and try the next source */
			stmvout_release_interface(hiface, iface_params->interface_type);
			stm_display_source_close(hSource);
			continue;
		}

		debug_msg("%s: Plane %p successfully connected to Source %p\n",
			     __FUNCTION__, hPlane, hSource);
		break;	/* break the loop as this source is ok */
	}

	src_info->source = hSource;
	src_info->iface_handle = hiface;
	src_info->iface_type = iface_params->interface_type;
	/* The below values are zero for queue buffer sources */
	src_info->pixeltype = iface_params->interface_params.pixel_stream_params->source_type;
	src_info->pixel_inst = iface_params->interface_params.pixel_stream_params->instance_number;
	return 0;
}

static inline int stmvout_get_number_of_outputs(void)
{
	int i;

	for (i = 0; i < STMEDIA_MAX_PLANES; i++) {
		if (g_voutRoot.pDev[i].name[0] == '\0')
			break;
	}
	return (i);
}

static int stmvout_vidioc_querycap(struct file *file, void *fh,
						struct v4l2_capability *cap)
{
	strlcpy(cap->driver, "Planes", sizeof(cap->driver));
	strlcpy(cap->card, "STMicroelectronics", sizeof(cap->card));
	strlcpy(cap->bus_info, "", sizeof(cap->bus_info));

	cap->version = LINUX_VERSION_CODE;
	cap->capabilities = (0
			     | V4L2_CAP_VIDEO_OUTPUT
			     | V4L2_CAP_STREAMING);
	return 0;
}

static int get_vout_device_locked(void *fh, stvout_device_t **pDev, bool *isFirstUser)
{
	struct stm_v4l2_vout_fh *vout_fh = container_of(fh, struct stm_v4l2_vout_fh, fh);
	open_data_t *pOpenData = vout_fh->pOpenData;
	struct semaphore *pLock = NULL;

	if(!pOpenData)
		return -EINVAL;

	BUG_ON(pOpenData->pDev == NULL);
		/* get shortcut to real video device (plane) */
	*pDev = pOpenData->pDev;
	pLock = &((*pDev)->devLock);

	if (down_interruptible(pLock))
		return -ERESTARTSYS;

	if(isFirstUser){
		if (pOpenData->isFirstUser)
			*isFirstUser = true;
		else
			*isFirstUser = false;
	}
	return 0;
}

static void put_vout_device(stvout_device_t *pDev)
{
	struct semaphore *pLock = NULL;
	if(pDev){
		pLock = &(pDev->devLock);
		up(pLock);
	}
}


static int stmvout_vidioc_enum_fmt_vid_out(struct file *file, void *fh,
					    struct v4l2_fmtdesc *f)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	ret = get_vout_device_locked(fh, &pDev, NULL);
	if(ret)
		goto error;

	ret = stmvout_enum_buffer_format(pDev, f);
error:
	put_vout_device(pDev);
	return ret;
}

static int stmvout_vidioc_g_fmt_vid_out(struct file *file, void *fh,
					struct v4l2_format *fmt)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	ret = get_vout_device_locked(fh, &pDev, NULL);
	if(ret)
		goto error;

	fmt->fmt.pix = pDev->bufferFormat.fmt.pix;
error:
	put_vout_device(pDev);
	return ret;
}

static int stmvout_vidioc_s_fmt_vid_out(struct file *file, void *fh,
				struct v4l2_format *fmt)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	bool isFirstUser = false;
	ret = get_vout_device_locked(fh, &pDev, &isFirstUser);
	if(ret)
		goto error;

	/* Check ioctl is legal */
	if (!isFirstUser) {
		err_msg
		    ("%s: VIDIOC_S_FMT: changing IO format not available on "
		     "this file descriptor\n",
		     __FUNCTION__);
		ret = -EINVAL;
		goto error;
	}

	if (pDev->isStreaming) {
		err_msg
		    ("%s: VIDIOC_S_FMT: device is streaming data, cannot "
		     "change format\n", __FUNCTION__);
		ret = -EINVAL;
		goto error;
	}

	ret = stmvout_set_buffer_format(pDev, fmt, 1);
error:
	put_vout_device(pDev);
	return ret;
}

static int stmvout_vidioc_try_fmt_vid_out(struct file *file, void *fh,
				  struct v4l2_format *fmt)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	ret = get_vout_device_locked(fh, &pDev, NULL);
	if(ret)
		goto error;

	ret = stmvout_set_buffer_format(pDev, fmt, 0);
error:
	put_vout_device(pDev);
	return ret;
}

static int stmvout_vidioc_cropcap(struct file *file, void *fh,
				struct v4l2_cropcap *a)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	ret = get_vout_device_locked(fh, &pDev, NULL);
	if(ret)
		goto error;

	if (a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		debug_msg("%s: VIDIOC_CROPCAP: invalid crop type %d\n",
					     __FUNCTION__, a->type);
		ret = -EINVAL;
		goto error;
	}
	ret = stmvout_get_display_size(pDev, a);
error:
	put_vout_device(pDev);
	return ret;
}

static int stmvout_vidioc_g_crop(struct file *file, void *fh,
					struct v4l2_crop *crop)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	ret = get_vout_device_locked(fh, &pDev, NULL);
	if(ret)
		goto error;

	switch (crop->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		crop->c = pDev->outputCrop.c;
		break;

	case V4L2_BUF_TYPE_PRIVATE:
		crop->c = pDev->srcCrop.c;
		break;

	default:
		err_msg("%s: VIDIOC_G_CROP: invalid crop type %d\n",
		     __FUNCTION__, crop->type);
		ret = -EINVAL;
	}
error:
	put_vout_device(pDev);
	return ret;
}

static int STM_V4L2_FUNC(stmvout_vidioc_s_crop,
		struct file *file, void *fh, struct v4l2_crop *crop)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	ret = get_vout_device_locked(fh, &pDev, NULL);
	if(ret)
		goto error;

	switch (crop->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		if ((ret = stmvout_set_output_crop(pDev,
				(const struct v4l2_crop *) crop)) < 0)
			ret = -EINVAL;
		break;

	case V4L2_BUF_TYPE_PRIVATE:
		/*
		 * This is a private interface for specifying the crop of the
		 * source buffer to be displayed, as opposed to the crop on the
		 * screen. V4L2 has no way of doing this but we need to be able to
		 * to do this where:
		 *
		 * 1. the actual image size is only known once you start decoding
		 *    (generally too late to set the buffer format and allocate
		 *    display buffers)
		 * 2. the buffer dimensions may be larger than the image size due to
		 *    HW restrictions (e.g. Macroblock formats, 720 pixel wide video
		 *    must be in a 736 pixel wide buffer to have an even number of
		 *    macroblocks)
		 * 3. the image size can change dynamically in the stream.
		 * 4. we need to support pan/scan and center cutout with widescreen->
		 *    4:3 scaling
		 *
		 */
		if ((ret = stmvout_set_buffer_crop(pDev,
					(const struct v4l2_crop *) crop)) < 0)
			ret = -EINVAL;
		break;

	default:
		err_msg("%s: VIDIOC_S_CROP: invalid buffer type %d\n",
		     __FUNCTION__, crop->type);
		ret = -EINVAL;
	}
error:
	put_vout_device(pDev);
	return ret;
}

int stmvout_vidioc_g_parm(struct file *file, void *fh,
					struct v4l2_streamparm *sp)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	ret = get_vout_device_locked(fh, &pDev, NULL);
	if(ret)
		goto error;

	if (sp->type != V4L2_BUF_TYPE_VIDEO_OUTPUT){
		ret = -EINVAL;
		goto error;
	}

	/*
	 * The output streaming params are only relevant when using write(),
	 * not when using real streaming buffers. As we do not support the
	 * write() interface this is always zeroed.
	 */
	memset(&sp->parm.output, 0,
	       sizeof(struct v4l2_outputparm));
error:
	put_vout_device(pDev);
	return ret;
}

int stmvout_vidioc_reqbufs(struct file *file, void *fh,
					struct v4l2_requestbuffers *req)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	bool isFirstUser = false;
	unsigned long flags;
	ret = get_vout_device_locked(fh, &pDev, &isFirstUser);
	if(ret)
		goto error;

	if (!isFirstUser) {
		err_msg("%s: VIDIOC_REQBUFS: illegal in non-io open\n",
		     __FUNCTION__);
		ret = -EBUSY;
		goto error;
	}

	if (req->type != V4L2_BUF_TYPE_VIDEO_OUTPUT
	    || (req->memory != V4L2_MEMORY_MMAP
		&& req->memory != V4L2_MEMORY_USERPTR)) {
		err_msg("%s: VIDIOC_REQBUFS: unsupported type (%d) of memory "
		     "parameters (%d)\n", __FUNCTION__, req->type, req->memory);
		ret = -EINVAL;
		goto error;
	}

	if (pDev->bufferFormat.type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		err_msg
		    ("%s: VIDIOC_REQBUFS: pixel format not set, cannot allocate "
		     "buffers\n", __FUNCTION__);
		ret = -EINVAL;
		goto error;
	}

	/*
	 * A subsequent call to VIDIOC_REQBUFS is allowed to change the number of
	 * buffers mapped. This is however, not allowed if any number of buffers are
	 * already mapped.
	 */
	write_lock_irqsave(&pDev->pendingStreamQ.lock, flags);
	stmvout_delete_buffers_from_list(&pDev->pendingStreamQ.list);
	write_unlock_irqrestore(&pDev->pendingStreamQ.lock, flags);

	if (!stmvout_deallocate_mmap_buffers(pDev)) {
		err_msg
		    ("%s: VIDIOC_REQBUFS: video buffer(s) still mapped, cannot "
		     "change\n", __FUNCTION__);
		ret = -EBUSY;
		goto error;
	}

	if (req->memory == V4L2_MEMORY_MMAP && req->count != 0
	    && !stmvout_allocate_mmap_buffers(pDev, req)) {
		err_msg("%s: VIDIOC_REQBUFS: unable to allocate video buffers\n",
		     __FUNCTION__);
		ret = -ENOMEM;
		goto error;
	}
error:
	put_vout_device(pDev);
	return ret;
}

int stmvout_vidioc_querybuf(struct file *file, void *fh,
							struct v4l2_buffer *pBuf)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	bool isFirstUser = false;
	streaming_buffer_t *pStrmBuf;

	ret = get_vout_device_locked(fh, &pDev, &isFirstUser);
	if(ret)
		goto error;

	pStrmBuf = &pDev->streamBuffers[pBuf->index];

	if (!isFirstUser) {
		err_msg("%s: VIDIOC_QUERYBUF: IO not available on this file "
		     "descriptor\n", __FUNCTION__);
		ret = -EBUSY;
		goto error;
	}

	if (pBuf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		err_msg("%s: VIDIOC_QUERYBUF: illegal stream type\n",
		     __FUNCTION__);
		ret = -EINVAL;
		goto error;
	}

	if (pBuf->index >= pDev->n_streamBuffers || !pStrmBuf->physicalAddr) {
		err_msg
		("%s: VIDIOC_QUERYBUF: bad parameter v4l2_buffer.index: %u\n",
		__FUNCTION__, pBuf->index);
		ret = -EINVAL;
		goto error;
	}

	*pBuf = pStrmBuf->vidBuf;
error:
	put_vout_device(pDev);
	return ret;
}

int stmvout_vidioc_qbuf(struct file *file, void *fh,
						struct v4l2_buffer *pBuf)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	bool isFirstUser = false;

	ret = get_vout_device_locked(fh, &pDev, &isFirstUser);
	if(ret)
		goto error;



	if (!isFirstUser) {
		err_msg
		    ("%s: VIDIOC_QBUF: IO not available on this file descriptor\n",
		     __FUNCTION__);
		ret = -EBUSY;
		goto error;
	}

	if (pBuf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		err_msg("%s: VIDIOC_QBUF: illegal stream type\n", __FUNCTION__);
		ret = -EINVAL;
		goto error;
	}

	if (pBuf->memory != V4L2_MEMORY_MMAP
			&& pBuf->memory != V4L2_MEMORY_USERPTR) {
		err_msg("%s: VIDIOC_QBUF: illegal memory type %d\n",
						__FUNCTION__, pBuf->memory);
		ret = -EINVAL;
		goto error;
	}

	if (pDev->outputCrop.type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		err_msg
		    ("%s: VIDIOC_QBUF: no valid output crop set, cannot queue "
		     "buffer\n", __FUNCTION__);
		ret = -EINVAL;
		goto error;
	}

	/* Media Controller coherency check:
	 * also checks if the Plane is connected to an Output */
	if ((stmvout_get_output_display_handle(pDev)) == NULL) {
		err_msg
		    ("%s: VIDIOC_QBUF: while plane has no output connected!\n",
		     __FUNCTION__);
		ret = -EINVAL;
		goto error;
	}

	ret = stmvout_queue_buffer(pDev, pBuf);
error:
	put_vout_device(pDev);
	return ret;

}

int stmvout_vidioc_dqbuf(struct file *file, void *fh,
						struct v4l2_buffer *pBuf)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	bool isFirstUser = false;

	ret = get_vout_device_locked(fh, &pDev, &isFirstUser);
	if(ret)
		goto error;

	if (!isFirstUser) {
		err_msg
		    ("%s: VIDIOC_DQBUF: IO not available on this file descriptor\n",
		     __FUNCTION__);
		ret = -EBUSY;
		goto error;
	}

	if (pBuf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		err_msg("%s: VIDIOC_DQBUF: illegal stream type\n", __FUNCTION__);
		ret = -EINVAL;
		goto error;
	}

	/*
	 * The API documentation states that the memory field must be set
	 * correctly by the application for the call to succeed. It isn't clear
	 * under what circumstances this would ever be needed and the requirement
	 * seems a bit over zealous, but we honour it anyway. I bet users will
	 * miss the small print and log lots of bugs that this call doesn't work.
	 */
	if (pBuf->memory != V4L2_MEMORY_MMAP
		&& pBuf->memory != V4L2_MEMORY_USERPTR) {
		err_msg("%s: VIDIOC_DQBUF: illegal memory type %d\n",
		     __FUNCTION__, pBuf->memory);
		ret = -EINVAL;
		goto error;
	}

	/* Media Controller coherency check:
	 * also checks if the Plane is connected to an Output */
	if ((stmvout_get_output_display_handle(pDev)) == NULL) {
		err_msg
		    ("%s: VIDIOC_DQBUF: while plane has no output connected!\n",
		     __FUNCTION__);
		ret = -EINVAL;
		goto error;
	}

	/*
	 * We can release the driver lock now as stmvout_dequeue_buffer is safe
	 * (the buffer queues have their own access locks), this makes
	 * the logic a bit simpler particularly in the blocking case.
	 */
	put_vout_device(pDev);

	if ((file->f_flags & O_NONBLOCK) == O_NONBLOCK) {
		debug_msg("%s: VIDIOC_DQBUF: Non-Blocking dequeue\n",
			__FUNCTION__);
		if ((ret = stmvout_dequeue_buffer(pDev, pBuf)) != 1)
			return -EAGAIN;
		return 0;
	} else {
		/*debug_msg ("%s: VIDIOC_DQBUF: Blocking dequeue\n", __FUNCTION__); */
		return wait_event_interruptible(pDev->wqBufDQueue,
					stmvout_dequeue_buffer(pDev, pBuf));
	}
error:
	put_vout_device(pDev);
	return ret;
}


int stmvout_vidioc_streamon(struct file *file, void *fh,
						enum v4l2_buf_type type)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	bool isFirstUser = false;
	ret = get_vout_device_locked(fh, &pDev, &isFirstUser);
	if(ret)
		goto error;

	if (type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		err_msg
		    ("%s: VIDIOC_STREAMON: illegal stream type %d\n",
		     __FUNCTION__, type);
		ret = -EINVAL;
		goto error;
	}

	if (pDev->isStreaming) {
		err_msg("%s: VIDIOC_STREAMON: device already streaming\n",
		     __FUNCTION__);
		ret = -EBUSY;
		goto error;
	}
#ifdef _STRICT_V4L2_API
	if (!stmvout_has_queued_buffers(pDev)) {
		/* The API states that at least one buffer must be queued before
		   STREAMON succeeds. */
		err_msg("%s: VIDIOC_STREAMON: no buffers queued on stream\n",
		     __FUNCTION__);
		ret = -EINVAL;
		goto error;
	}
#endif

	if ((ret = stmvout_streamon(pDev)) < 0)
		goto error;

	debug_msg("%s: VIDIOC_STREAMON: Starting the stream\n",
		  __FUNCTION__);

	pDev->isStreaming = 1;
	wake_up_interruptible(&(pDev->wqStreamingState));

	/* Make sure any frames already on the pending queue are written to the
	   hardware */
	stmvout_send_next_buffer_to_display(pDev);
error:
	put_vout_device(pDev);
	return ret;
}

int stmvout_vidioc_streamoff(struct file *file, void *fh, enum v4l2_buf_type type)
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	bool isFirstUser = false;
	ret = get_vout_device_locked(fh, &pDev, &isFirstUser);
	if(ret)
		goto error;

	if (type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		err_msg("%s: VIDIOC_STREAMOFF: illegal stream type\n",
		     __FUNCTION__);
		ret = -EINVAL;
	}

	ret = stmvout_streamoff(pDev);
error:
	put_vout_device(pDev);
	return ret;
}

int stmvout_vidioc_g_output(struct file *file, void *fh, unsigned int *output)
{
	struct stm_v4l2_vout_fh *vout_fh = container_of(fh, struct stm_v4l2_vout_fh, fh);
	open_data_t *pOpenData = vout_fh->pOpenData;

	*output = -1;	/* a priori output is not set! */
	/* exit if not yet set */
	if(!pOpenData)
		return 0;


	/* simply return the in use PAD id of /dev/video1 */
	*output = pOpenData->padId;

	/* Now let we assume this output still connected,
	 * In reality this request should be protected
	 * against simultaneous MC graph changes
	 */
	 return 0;
}

int stmvout_vidioc_s_output(struct file *file, void *fh, unsigned int output)
{
	struct stm_v4l2_vout_fh *vout_fh = container_of(fh, struct stm_v4l2_vout_fh, fh);
	open_data_t *pOpenData = vout_fh->pOpenData;
	stvout_device_t *pDev = NULL;
	struct semaphore *pLock = NULL;

	stvout_device_t *const new_dev = &g_voutRoot.pDev[output];
	struct list_head *list = &new_dev->openData.open_list;
	struct semaphore *new_lock = NULL;
	open_data_t *open_data = NULL;
	int ret = 0;


	if(pOpenData){
		BUG_ON(pOpenData->pDev == NULL);
		/* get shortcut to real video device (plane) */
		pDev = pOpenData->pDev;
		pLock = &(pDev->devLock);

		if (down_interruptible(pLock))
			return -ERESTARTSYS;
	}

	if (output >= stmvout_get_number_of_outputs()) {
		err_msg
		    ("S_OUTPUT: Output number %d out of range %d...%d\n",
		     output, 0,
		     (stmvout_get_number_of_outputs() - 1));
		ret = -EINVAL;
		goto error;
	}

	/* Is the max open count is reached? */
	if (new_dev->open_count > MAX_OPENS){
		ret = -EBUSY;
		goto error;
	}

	/* same output as is already set? */
	if (new_dev == pDev){
		goto error;
	}


	/* if current output is busy, we can't change it. */
	if (pDev) {
		/* it's busy if the first opener is streaming or has buffers queued.
		   Following openers may not start streaming/queueing buffers, so we
		   only need to do this check for the first user.
		   Following openers may change the output associated with their
		   filehandle, though. */
		if (pOpenData->isFirstUser && (pDev->isStreaming
			|| stmvout_has_queued_buffers(pDev))) {
			err_msg
			    ("S_OUTPUT: Output %d is active, cannot disassociate\n",
			     output);
			ret = -EBUSY;
			goto error;
		}
	}

	open_data = kzalloc(sizeof(open_data_t), GFP_KERNEL);
	if (!open_data) {
		printk(KERN_ERR"%s(): Unable to allocate memory for open data\n",
		       __func__);
		ret = -ENOMEM;
		goto error;
	}

	/* we should first check if we will be able to support another open on
	   the new device and allocate it for use by ourselves, so that we
	   don't end up without any device in case this fails... */
	new_lock = &new_dev->devLock;
	if (down_interruptible(new_lock)){
		kfree(open_data);
		ret = -ERESTARTSYS;
		goto error;
	}

	open_data->isOpen = 1;
	open_data->pDev = new_dev;
	open_data->padId = output;
	if (list_empty(list)) {
		open_data->isFirstUser = 1;
	} else {
		open_data->isFirstUser = 0;
	}
	list_add_tail(&(open_data->open_list), list);

	new_dev->open_count++;

	if (new_dev->open_count == 1) {
		if ((ret = init_device_locked(new_dev)) != 0) {
			up(new_lock);
			kfree(open_data);
			goto error;
		}
	}

	/* we have the device now, release the old one */
	if (pDev) {
		if ((ret =  stmvout_close_locked(pOpenData, pDev)) != 0) {
			up(new_lock);
			/* FIXME hows the pOpenData removed from the llist?*/
			kfree(open_data);
			goto error;
		}
		up(pLock);
	}

	pDev = new_dev;
	pLock = new_lock;

	debug_msg("v4l2_open: isFirstUser: %c\n",
		  open_data->isFirstUser ? 'y' : 'n');

	((struct v4l2_fh *)fh)->ctrl_handler = &pDev->ctrl_handler;

	vout_fh->pOpenData = open_data;

error:
	if (pDev) {
		up(pLock);
	}
	return ret;
}

int stmvout_vidioc_enum_output(struct file *file, void *fh,
						struct v4l2_output *output)
{
	int index = output->index;
	int ret = 0;
	if (index < 0 || index >= stmvout_get_number_of_outputs())
		return -EINVAL;

	snprintf(output->name, sizeof(output->name),
		 "%s", g_voutRoot.pDev[index].name);
	output->type = V4L2_OUTPUT_TYPE_ANALOG;
	output->audioset = 0;
	output->modulator = 0;

	/* Returned standard = 0 if the Display plane has not been linked
	 * with an output (Media Controller).
	 */
	if ((ret = stmvout_get_supported_standards(&g_voutRoot.pDev[index],
					     &output->std)) < 0)
		output->std = 0;

	return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
static long stmvout_vidioc_private_ioctl(struct file *file, void *fh,
				bool valid_prio, int cmd, void *arg)
#else
static long stmvout_vidioc_private_ioctl(struct file *file, void *fh,
				bool valid_prio, unsigned int cmd, void *arg)
#endif
{
	stvout_device_t *pDev = NULL;
	int ret = 0;
	ret = get_vout_device_locked(fh, &pDev, NULL);
	if(ret)
		goto error;

	switch(cmd){
	case VIDIOC_G_OUTPUT_STD:
		{
			v4l2_std_id *const std_id = arg;

			if ((ret =
			     stmvout_get_current_standard(pDev, std_id)) < 0)
				goto error;
		}
		break;

	case VIDIOC_S_OUTPUT_STD:
		{
			v4l2_std_id *const std_id = arg;

			if ((ret =
			     stmvout_set_current_standard(pDev, *std_id)) < 0)
				goto error;
		}
		break;

	case VIDIOC_ENUM_OUTPUT_STD:
		{
			struct v4l2_standard *const std = arg;

			if ((ret = stmvout_get_display_standard(pDev, std)) < 0)
				goto error;
		}
		break;

	case VIDIOC_S_OUTPUT_ALPHA:
		{
			unsigned int alpha = *(unsigned int *)arg;

			if (alpha > 255)
				goto error;

			pDev->globalAlpha = alpha;
			if ((ret = stm_display_plane_set_control(pDev->hPlane,
				PLANE_CTRL_TRANSPARENCY_VALUE,
				pDev->globalAlpha)) !=0) {
				err_msg("Cannot set transparency (%d)\n",ret);
				goto error;
			}
			if( pDev->alpha_control != CONTROL_ON) {
				if ((ret = stm_display_plane_set_control(pDev->hPlane,
					PLANE_CTRL_TRANSPARENCY_STATE,
					CONTROL_ON)) !=0) {
					err_msg("Cannot set transparency state to ON (%d)\n",ret);
					goto error;
				}
				pDev->alpha_control = CONTROL_ON;
			} else if ( pDev->globalAlpha == 255 ) {
				if ((ret = stm_display_plane_set_control(pDev->hPlane,
					PLANE_CTRL_TRANSPARENCY_STATE,
					CONTROL_OFF)) !=0) {
					err_msg("Cannot set transparency state to OFF (%d)\n",ret);
					goto error;
				}
				pDev->alpha_control = CONTROL_OFF;
			}
		}
		break;
	default:
		ret = -ENOTTY;
		goto error;
	}

error:
	put_vout_device(pDev);
	return ret;
}

/**********************************************************
 * mmap helper functions
 **********************************************************/
static void stmvout_vm_open(struct vm_area_struct *vma)
{
	streaming_buffer_t *pBuf = vma->vm_private_data;

	debug_msg("%s: %p [count=%d,vma=%08lx-%08lx]\n", __FUNCTION__, pBuf,
		  pBuf->mapCount, vma->vm_start, vma->vm_end);

	pBuf->mapCount++;
}

static void stmvout_vm_close(struct vm_area_struct *vma)
{
	streaming_buffer_t *const pBuf = vma->vm_private_data;

	debug_msg("%s: %p [count=%d,vma=%08lx-%08lx]\n", __FUNCTION__, pBuf,
		  pBuf->mapCount, vma->vm_start, vma->vm_end);

	if (--pBuf->mapCount == 0) {
		debug_msg("%s: %p clearing mapped flag\n", __FUNCTION__, pBuf);
		pBuf->vidBuf.flags &= ~V4L2_BUF_FLAG_MAPPED;
	}

	return;
}

/* VSOC - HCE - for bpa2 buffer access from user space, we must have the      */
/* access file ops. This should also be done on SoC for access thru debugger! */
static int stmvout_vm_access(struct vm_area_struct *vma, unsigned long addr,
			void *buf, int len, int write)
{
	void *page_addr;
	unsigned long page_frame;

	/*
	* Validate access limits
	*/
	if (addr >= vma->vm_end)
		return 0;
	if (len > vma->vm_end - addr)
		len = vma->vm_end - addr;
	if (len == 0)
		return 0;

	/*
	 * Note that this assumes an identity mapping between the page offset and
	 * the pfn of the physical address to be mapped. This will get more complex
	 * when the 32bit SH4 address space becomes available.
	*/
	page_addr = (void*)((addr - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT));

	page_frame = ((unsigned long) page_addr >> PAGE_SHIFT);

	if (pfn_valid (page_frame)) {
		if (write)
			memcpy(__va(page_addr), buf, len);
		else
			memcpy(buf, __va(page_addr), len);
		return len;
	}

#if defined(CONFIG_ARM) || defined(CONFIG_HCE_ARCH_ARM)
	{
		void __iomem *ioaddr = __arm_ioremap((unsigned long)page_addr, len, MT_MEMORY);
		if (write)
			memcpy_toio(ioaddr, buf, len);
		else
			memcpy_fromio(buf, ioaddr, len);
		iounmap(ioaddr);
	}
	return len;
/*#elif defined(CONFIG_HAVE_IOREMAP_PROT) FIXME for stlinuxtv unknown symbol while module loading in case of hdk7108
    return generic_access_phys(vma, addr, buf, len, write);*/
#else
	return 0;
#endif
}

static int stmvout_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	void *page_addr;
	unsigned long page_frame;
	unsigned long vaddr = (unsigned long)vmf->virtual_address;

	if (vaddr > vma->vm_end)
		return VM_FAULT_SIGBUS;

	/*
	 * Note that this assumes an identity mapping between the page offset and
	 * the pfn of the physical address to be mapped. This will get more complex
	 * when the 32bit SH4 address space becomes available.
	 */
	page_addr =
	    (void *)((vaddr - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT));

	page_frame = ((unsigned long)page_addr >> PAGE_SHIFT);

	if (!pfn_valid(page_frame))
		return VM_FAULT_SIGBUS;

	page = virt_to_page(__va(page_addr));

	get_page(page);

	vmf->page = page;
	return 0;
}

static struct vm_operations_struct stmvout_vm_ops_memory = {
	.open = stmvout_vm_open,
	.close = stmvout_vm_close,
	.fault = stmvout_vm_fault,
	.access = stmvout_vm_access,
};

static struct vm_operations_struct stmvout_vm_ops_ioaddr = {
	.open = stmvout_vm_open,
	.close = stmvout_vm_close,
	.fault = NULL,
	.access = stmvout_vm_access,
};

/* --------------------------------------------------------------- */
/*  File Operations */

static int stmvout_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct stm_v4l2_vout_fh *vout_fh = container_of(file->private_data, struct stm_v4l2_vout_fh, fh);
	open_data_t *pOpenData = vout_fh->pOpenData;

	stvout_device_t *pDev;
	streaming_buffer_t *pBuf;

	if (!pOpenData) {
		debug_msg("%s bug: file private data not set!\n", __FUNCTION__);
		return -ENODEV;
	}

	if (!(vma->vm_flags & VM_WRITE)) {
		debug_msg("mmap app bug: PROT_WRITE please\n");
		return -EINVAL;
	}

	if (!(vma->vm_flags & VM_SHARED)) {
		debug_msg("mmap app bug: MAP_SHARED please\n");
		return -EINVAL;
	}

	BUG_ON(pOpenData == NULL);

	/* was: pDev = pOpenData->pDev; */
	pDev = pOpenData->pDev;

	if (down_interruptible(&pDev->devLock))
		return -ERESTARTSYS;

	if (!pOpenData->isFirstUser) {
		err_msg
		    ("mmap() another open file descriptor exists and is doing IO\n");
		goto err_busy;
	}

	debug_msg("v4l2_mmap offset = 0x%08X\n", (int)vma->vm_pgoff);
	pBuf =
	    stmvout_get_buffer_from_mmap_offset(pDev,
						vma->vm_pgoff * PAGE_SIZE);
	if (!pBuf) {
		/* no such buffer */
		err_msg("%s: Invalid offset parameter\n", __FUNCTION__);
		goto err_inval;
	}

	debug_msg("v4l2_mmap pBuf = 0x%08lx\n", (unsigned long)pBuf);

	if (pBuf->vidBuf.length != (vma->vm_end - vma->vm_start)) {
		/* wrong length */
		err_msg("%s: Wrong length parameter\n", __FUNCTION__);
		goto err_inval;
	}

	if (!pBuf->cpuAddr) {
		/* not requested */
		err_msg("%s: Buffer is not available for mapping\n",
			__FUNCTION__);
		goto err_inval;
	}

	if (pBuf->mapCount > 0) {
		err_msg("%s: Buffer is already mapped\n", __FUNCTION__);
		goto err_busy;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
	vma->vm_flags |= VM_RESERVED | VM_IO | VM_DONTEXPAND | VM_LOCKED;
#else
	vma->vm_flags |= VM_DONTDUMP | VM_IO | VM_DONTEXPAND | VM_LOCKED;
#endif
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_private_data = pBuf;

	if (virt_addr_valid(pBuf->cpuAddr)) {
		debug_msg("%s: remapping memory.\n", __FUNCTION__);
		vma->vm_ops = &stmvout_vm_ops_memory;
	} else {
		debug_msg("%s: remapping IO space.\n", __FUNCTION__);
		/* Note that this assumes an identity mapping between the page offset and
		   the pfn of the physical address to be mapped. This will get more
		   complex when the 32bit SH4 address space becomes available. */
		if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				    (vma->vm_end - vma->vm_start),
				    vma->vm_page_prot)) {
			up(&pDev->devLock);
			return -EAGAIN;
		}

		vma->vm_ops = &stmvout_vm_ops_ioaddr;
	}

	pBuf->mapCount = 1;
	pBuf->vidBuf.flags |= V4L2_BUF_FLAG_MAPPED;

	up(&pDev->devLock);

	return 0;

err_busy:
	up(&pDev->devLock);
	return -EBUSY;

err_inval:
	up(&pDev->devLock);
	return -EINVAL;
}

/************************************************
 * v4l2_poll   TODO
 ************************************************/
static unsigned int stmvout_poll(struct file *file, poll_table * table)
{
	struct stm_v4l2_vout_fh *vout_fh = container_of(file->private_data, struct stm_v4l2_vout_fh, fh);
	open_data_t *pOpenData = vout_fh->pOpenData;

	stvout_device_t *pDev;
	unsigned int mask;

	if (!pOpenData) {
		debug_msg("%s bug: file private data not set!\n", __FUNCTION__);
		return -ENODEV;
	}

	BUG_ON(pOpenData == NULL);

	/* was: pDev = pOpenData->pDev; */
	pDev = pOpenData->pDev;

	if (down_interruptible(&pDev->devLock))
		return -ERESTARTSYS;

	/* Add DQueue wait queue to the poll wait state */
	poll_wait(file, &pDev->wqBufDQueue, table);
	poll_wait(file, &pDev->wqStreamingState, table);

	if (!pOpenData->isFirstUser)
		mask = POLLERR;
	else if (!pDev->isStreaming)
		mask = POLLOUT;	/* Write is available when not streaming */
	else if (stmvout_has_completed_buffers(pDev))
		mask = POLLIN;	/* Indicate we can do a DQUEUE ioctl */
	else
		mask = 0;

	up(&pDev->devLock);

	return mask;
}

/* =======================================================================
 * Create Output Plug sub-devices
 *
 * Output sub-device operations:
 */
unsigned int get_stkpi_voutPlug_ctrl_name(int id)
{
	return 0;
}

static int stmedia_voutPlug_queryctrl(struct v4l2_subdev *sd,
				      struct v4l2_queryctrl *qc)
{
	return 0;
}

static int stmedia_voutPlug_get_ctrl(struct v4l2_subdev *sd,
				     struct v4l2_control *ctrl)
{
	struct output_plug *output;
	unsigned int ctrlname;
	unsigned int val;
	int ret;

	output = v4l2_get_subdevdata(sd);
	ctrlname = get_stkpi_voutPlug_ctrl_name(ctrl->id);
	if (ctrlname == 0)
		return -EINVAL;

	ret = stm_display_output_get_control(output->hOutput, ctrlname, &val);
	if (ret)
		return ret;

	ctrl->value = val;

	return 0;
}

static int stmedia_voutPlug_set_ctrl(struct v4l2_subdev *sd,
				     struct v4l2_control *ctrl)
{
	struct output_plug *output;
	unsigned int ctrlname;
	int ret;

	output = v4l2_get_subdevdata(sd);
	ctrlname = get_stkpi_voutPlug_ctrl_name(ctrl->id);
	if (ctrlname == 0)
		return -EINVAL;

	ret =
	    stm_display_output_set_control(output->hOutput,
					   ctrlname, ctrl->value);

	return ret;
}

static int stmedia_voutPlug_querymenu(struct v4l2_subdev *sd,
				      struct v4l2_querymenu *qm)
{
	return 0;
}

static const struct v4l2_subdev_core_ops stmedia_voutPlug_core_ops = {
	.queryctrl = &stmedia_voutPlug_queryctrl,
	.g_ctrl = &stmedia_voutPlug_get_ctrl,
	.s_ctrl = &stmedia_voutPlug_set_ctrl,
	.querymenu = &stmedia_voutPlug_querymenu,
};

/* -----------------------------------------------------------------------
 * Output link operations:
 */
/*
 * output_setup_compo_capture_format: Setup the pixel capture Buffer format
 * depending on the Plane capabilities where the Sink is connected.
 */
static int output_setup_compo_capture_format( struct output_plug *outputPlug,
						  stm_pixel_capture_input_params_t params,
						  stm_object_h sink)
{
	stm_display_plane_h       pPlane = NULL;
	const stm_pixel_format_t *plane_formats;
	int                       n_plane_Formats;
	int32_t  id  = -1;
	uint32_t fmt = 0;
	int found = -1;
	int ret;

	/* Configure the capture buffer format */
	ret = stm_display_source_get_connected_plane_id((stm_display_source_h)sink,
			                                &id, 1);
	if( (ret < 0) || (id < 0) ) {
		printk(KERN_ERR "%s: Source is not connected to any plane?! (%d)\n",
				__func__, ret);
		return ret;
	}

	ret = stm_display_device_open_plane(outputPlug->hDisplay, id, &pPlane);
	if (ret) {
		printk(KERN_ERR "%s: Failed to retreive the plane handle from the" \
				" source (%d)\n",
				__func__, ret);
		return ret;
	}

	/* get supported plane formats */
	n_plane_Formats = stm_display_plane_get_image_formats(pPlane,
			                                      &plane_formats);
	if (n_plane_Formats == 0) {
		printk(KERN_ERR "%s: failed to get supported plane formats\n",
			  __func__);
		return -EIO;
	}

	debug_msg("%s: Got %d supported plane formats\n",
		  __func__, n_plane_Formats);

	/* Check if the plane did support recommanded capture format */
	for(fmt=0; ((fmt<ARRAY_SIZE(format_mapping)) && (found < 0)); fmt++) {
		for(id=0; id < n_plane_Formats; ++id) {
			/*
			 * For tunneled Main compo capture we rely on RGB888 format which is
			 * the most efficient one between Main Compo and GDPs
			 * Set the pixel capture generated format as follow:
			 * 	- RGB888
			 * 	- Main compo resolution (1 = 1 capture without resize)
			 * 	- Progressive
			 */
			debug_msg("%s: formats[%d] = (%d), format_map[%d] = (%d)\n",
				__func__, id, plane_formats[id], fmt,
				format_mapping[fmt].plane_format);
			if(plane_formats[id] == format_mapping[fmt].plane_format)
			{
				debug_msg("%s: found a supported plane format (%d)\n",
					__func__, plane_formats[id]);
				found = fmt;
				break;
			}
		}
	}

	/* No matching between Capture and the Plane supported formats */
	if(found < 0) {
		printk(KERN_ERR "%s: failed to find suitable plane format\n",
			__func__);
		return -EINVAL;
	}

	outputPlug->pixel_capture_format.format =
					format_mapping[found].cap_format;
	outputPlug->pixel_capture_format.color_space =
					format_mapping[found].color_space;

	outputPlug->pixel_capture_format.stride =
			(outputPlug->pixel_capture_format.width *
			 format_mapping[found].depth / 8);

	/*
	 * 422 Raster (interleaved luma and chroma), we need an even number
	 * of luma samples (pixels) per line
	 */
	if(outputPlug->pixel_capture_format.format == STM_PIXEL_FORMAT_YCbCr422R) {
		outputPlug->pixel_capture_format.width +=
					        outputPlug->pixel_capture_format.width % 2;
	}

	ret = stm_pixel_capture_set_format( outputPlug->pixel_capture,
					    outputPlug->pixel_capture_format );
	if (ret){
		printk(KERN_ERR "%s: failed to set format (%d)\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

/*
 * output_setup_compo_capture_io_windows: Setup the pixel capture Input and
 * Output windows depending on the Plane capabilities where the Sink is
 * connected.
 */
static int output_setup_compo_capture_io_windows(struct output_plug *outputPlug,
						  stm_pixel_capture_input_params_t params,
						  stm_object_h sink)
{
	stm_pixel_capture_input_window_capabilities_t iwin_caps;
	stm_display_plane_h      pPlane = NULL;
#ifdef V4L2_VOUT_CAPTURE_SUPPORT_PLANE_RESIZE
	stm_plane_capabilities_t plane_caps = 0;
#endif
	stm_pixel_capture_rect_t input_rect;
	stm_rect_t output_rect;
	int id = -1;
	int ret;

	/* Configure the input window */
	/*
	 * Set the pixel capture IP Input window as the minimum between the
	 * maximum possible by the IP itself and the Main compo resolution.
	 */
	ret = stm_pixel_capture_get_input_window_capabilities(
						outputPlug->pixel_capture,
						&iwin_caps);
	if (ret){
		printk(KERN_ERR "%s: Unable to get input window caps (%d)\n",
				__func__, ret);
		return ret;
	}

	input_rect.x = input_rect.y = 0;
	input_rect.width = min(params.active_window.width,
			       iwin_caps.max_input_window_area.width);
	input_rect.height = min(params.active_window.height,
			        iwin_caps.max_input_window_area.height);

	if((input_rect.width == 0) || (input_rect.height == 0)){
		printk(KERN_ERR "%s: Invalid capture input ranges (%dx%d)\n",
				__func__, input_rect.width, input_rect.height);
		return -EINVAL;
	}

	ret = stm_pixel_capture_set_input_window(outputPlug->pixel_capture,
						 input_rect);
	if (ret) {
		printk(KERN_ERR "%s: Unable to set input window (%d)\n",
				__func__, ret);
		return ret;
	}

	/* Configure the output window */
	/*
	 * We would prefer to process the whole resize using the capture IP but
	 * doing a one big downscaling would result in much worst display quality.
	 * We can be performing the downscaling job in two steps depending on the
	 * plane hw capabilities (full or lite plane hw version).
	 * Using the Plane Caps we can check if the plane support or not big
	 * downscaling ops.
	 */
	ret = stm_display_source_get_connected_plane_id((stm_display_source_h)sink,
			                                &id, 1);
	if((ret < 0) || (id < 0)) {
		printk(KERN_ERR "%s: Source is not connected to any plane?! (%d)\n",
				__func__, ret);
		return ret;
	}

	ret = stm_display_device_open_plane(outputPlug->hDisplay, id, &pPlane);
	if (ret) {
		printk(KERN_ERR "%s: Failed to retreive the plane handle from the" \
				" source (%d)\n",
				__func__, ret);
		return ret;
	}

	/* Get current plane output rectangle configuration */
	ret = stm_display_plane_get_compound_control(pPlane,
							 PLANE_CTRL_OUTPUT_WINDOW_VALUE,
							 &output_rect);
	if (ret) {
		printk(KERN_ERR "%s: failed to get plane output window value\n",
			  __func__);
		return -EIO;
	}

	/* By default try to process the resize using the CAP hardware */
	outputPlug->pixel_capture_format.width  = output_rect.width;
	outputPlug->pixel_capture_format.height = output_rect.height;

#ifdef V4L2_VOUT_CAPTURE_SUPPORT_PLANE_RESIZE
	/* Get plane capabilities */
	ret = stm_display_plane_get_capabilities(pPlane, &plane_caps);
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to retreive sink %p caps (%d)\n",
				__func__, sink, ret);
		return ret;
	}

	if((plane_caps & PLANE_CAPS_VIDEO) ||
	   (plane_caps & PLANE_CAPS_GRAPHICS_BEST_QUALITY)) {
		/* First, if the downscaling factor is bigger than 1/2, we limit the
		 * pixel capture to downscale to the half of the input resolution.
		 * The Plane hardware will then resize captured bufer to fit the AUX
		 * display resolution.
		 *
		 * Second, we come check if the first configuration which is respecting
		 * CAPTURE hardware limitation will be respecting the Plane's hardware
		 * limitation too. If it is the case then we process with this hardware
		 * configurations otherwise we need to choose which hardware will be
		 * used to process downscaling outside hardware limitations. We prefer
		 * to do this using CAPTURE hardware which is giving better quality with
		 * less memory usage.
		 *
		 * This allow to NOT do a one big downscale which would result in much
		 * bad quality with big amount of memory allocated for capture.
		 */
#define fixedpointONE  (1L << 16)           /* 1 in 16.16 fixed point */
#define fixedpointHALF (fixedpointONE / 2)  /* 1/2 in 16.16 fixed point */
		uint32_t zoom_factor = fixedpointONE;

		debug_msg("%s: Optimize display Quality using both CAPTURE and" \
				" Plane hardware\n",
				__func__);

		/*
		 * First check for capture HW constraint and calculate downscaling
		 * factor.
		 */
		zoom_factor =  output_rect.height * fixedpointONE / input_rect.height;
		if(zoom_factor < fixedpointHALF) {
			/* We are going to exceed CAPTURE downscale hardware limitation */
			if(params.flags == STM_PIXEL_CAPTURE_BUFFER_INTERLACED) {
				debug_msg("%s: Limit CAPTURE downscale to 1/2\n",
						__func__);
				outputPlug->pixel_capture_format.height = input_rect.height/2;
			}
		}

		/*
		 * Now check if we are respecting Plane's HW constraint.
		 */
		zoom_factor = output_rect.height * fixedpointONE
					  / outputPlug->pixel_capture_format.height;
		if(zoom_factor < fixedpointHALF) {
			/* We are going to exceed Plane's downscale hardware limitation */
			if(params.flags == STM_PIXEL_CAPTURE_BUFFER_INTERLACED) {
				debug_msg("%s: Optimize display Quality using both Plane and" \
						" CAP\n",
						__func__);
				outputPlug->pixel_capture_format.height = output_rect.height*2;
			}
		}
#undef fixedpointHALF
#undef fixedpointONE
	}
#endif

	/* We are always capturing frames using the compo capture hardware */
	outputPlug->pixel_capture_format.flags = 0;

	return 0;
}

/*
 * output_config_compo_capture: Configure the pixel capture IP based on the
 * current status of the Main compo and depending on the Sink capabilities.
 * This function should be called everytime the Main Compo configuration
 * change
 */
static int output_config_compo_capture(struct output_plug *outputPlug,
			                stm_object_h sink)
{
	stm_display_mode_t mode_line = { STM_TIMING_MODE_RESERVED };
	stm_pixel_capture_input_params_t params;
	stm_ycbcr_colorspace_t colorspace;
	stm_rational_t displayAspectRatio;
	int ret;

	/*
	 * Retrieve the Main compo resolution information. This is necessary
	 * in order to configure the pixel capture IP
	 */
	ret = stm_display_output_get_current_display_mode( outputPlug->hOutput,
							   &mode_line );
	if (ret) {
		printk(KERN_ERR "%s: Unable to get current display mode (%d)\n",
				__func__, ret);
		return ret;
	}

	params.active_window.x = mode_line.mode_params.active_area_start_pixel;
	params.active_window.y = mode_line.mode_params.active_area_start_line;
	params.active_window.width = mode_line.mode_params.active_area_width;
	params.active_window.height = mode_line.mode_params.active_area_height;
	params.src_frame_rate = mode_line.mode_params.vertical_refresh_rate;
	params.vtotal = mode_line.mode_timing.vsync_width;
	params.htotal = mode_line.mode_timing.hsync_width;

	if (mode_line.mode_params.scan_type == STM_PROGRESSIVE_SCAN)
		params.flags = 0;
	else
		params.flags = STM_PIXEL_CAPTURE_BUFFER_INTERLACED;

	/*
	 * Retrieve the Main compo colorspace information
	 */
	ret = stm_display_output_get_control( outputPlug->hOutput,
					      OUTPUT_CTRL_YCBCR_COLORSPACE,
					      &colorspace);
	if (ret){
		printk(KERN_ERR "%s: Unable to get current colorspace (%d)\n",
				__func__, ret);
		return ret;
	}

	switch(colorspace){
	case STM_YCBCR_COLORSPACE_AUTO_SELECT:
		params.color_space = (mode_line.mode_params.output_standards &
				      STM_OUTPUT_STD_HD_MASK)?
				      STM_PIXEL_CAPTURE_BT709:
				      STM_PIXEL_CAPTURE_BT601;
		break;
	case STM_YCBCR_COLORSPACE_709:
		params.color_space = STM_PIXEL_CAPTURE_BT709;
		break;
	case STM_YCBCR_COLORSPACE_601:
		params.color_space = STM_PIXEL_CAPTURE_BT601;
		break;
	default:
		params.color_space = STM_PIXEL_CAPTURE_RGB;
		break;
	}

	/*
	 * Retrieve the Main compo Aspect Ratio information
	 */
	ret = stm_display_output_get_compound_control( outputPlug->hOutput,
					OUTPUT_CTRL_DISPLAY_ASPECT_RATIO,
					&displayAspectRatio);
	if (ret){
		printk(KERN_ERR "%s: Unable to get current aspect-ratio (%d)\n",
				__func__, ret);
		return ret;
	}

	params.pixel_aspect_ratio.numerator   = displayAspectRatio.numerator;
	params.pixel_aspect_ratio.denominator = displayAspectRatio.denominator;

	/*
	 * Set the pixel capture IP input information
	 * (this is tighly coupled with the Main compo current resolution etc)
	 */
	ret = stm_pixel_capture_set_input_params(outputPlug->pixel_capture,
						 params);
	if (ret){
		printk(KERN_ERR "%s: failed to set input params (%d)\n",
				__func__, ret);
		return ret;
	}

	/*
	 * Set the pixel capture IP Input/Output windows
	 */
	ret = output_setup_compo_capture_io_windows(outputPlug, params, sink);
	if (ret){
		printk(KERN_ERR "%s: failed to set capture IOWidows (%d)\n",
				__func__, ret);
		return ret;
	}

	/*
	 * For tunneled Main compo capture we rely on YCbCr422 RSB format which is
	 * the most efficient one between Main Compo, GDPs and Video planes.
	 * Set the pixel capture generated format as follow:
	 * 	- Default format is YCbCr422RSB
	 * 	  --> If not supported try other formats (from format_mapping[] array)
	 * 	- Resolution depends on the capture and plane hardware capabilities:
	 * 	   1 - Default is Aux compo resolution (capture with resize)
	 * 	   2 - If CAPTURE doesn't support this configuration:
	 * 	    --> Half Main compo resolution (1 = 1/2 capture with resize)
	 * 	   3 - If PLANE doesn't support this configuration:
	 * 	    --> Double Aux compo resolution (capture with resize outside limits)
	 * 	- Progressive
	 */
	ret = output_setup_compo_capture_format(outputPlug, params, sink);
	if (ret){
		printk(KERN_ERR "%s: failed to set format (%d)\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

/*
 * alloc_compo_buffer: Allocate a bpa2 buffer and return its physical address.
 * This function is only here to take care of the HACK to avoid 64MB boundary
 * crossing within buffers
 */
static unsigned long alloc_compo_buffer( struct bpa2_part * bpa2_part,
					 unsigned long size,
					 unsigned long pagesize)
{
	unsigned long phys;

	/* Try to get the pages */
	phys = bpa2_alloc_pages(bpa2_part, pagesize, 1, GFP_KERNEL);
	if (!phys)
		return phys;

	/* most buffers may not cross a 64MB boundary */
#define PAGE64MB_SIZE  (1<<26)
	if ((phys & ~(PAGE64MB_SIZE - 1))
	    != ((phys + size) & ~(PAGE64MB_SIZE - 1))) {
		printk
		    (KERN_INFO "%s: %8.lx + %lu (%lu pages) crosses a 64MB" \
			" boundary, retrying!\n", __FUNCTION__, phys,
			size, pagesize);

		/* FIXME: this is a hack until the point we get an updated
		 * BPA2 driver. Will be removed again! */
		/* free and immediately reserve, hoping nobody requests
		 * bpa2 memory in between... */
		bpa2_free_pages(bpa2_part, phys);
		phys = bpa2_alloc_pages(bpa2_part, pagesize,
						     PAGE64MB_SIZE / PAGE_SIZE,
						     GFP_KERNEL);
		if (!phys)
			return phys;
		printk("%s: new start: %.8lx\n", __FUNCTION__,
		       phys);
	}

	if ((phys & ~(PAGE64MB_SIZE - 1))
	    != ((phys + size) & ~(PAGE64MB_SIZE - 1))) {
		/* can only happen, if somebody else requested bpa2 memory
		 * while we were busy here */
		printk
		    (KERN_INFO "%s: %8.lx + %lu (%lu pages) crosses a 64MB" \
			" boundary again! Ignoring this time...\n",
			 __FUNCTION__, phys, size, pagesize);
	}

	return phys;
}

/*
 * output_enable_compo_capture: This function enable/configure the datapath
 * between the Main Compo and the selected sink (GDP Source)
 */
static int output_enable_compo_capture( struct output_plug *outputPlug,
					stm_object_h sink,
					struct v4l2_subdev_crop *iwin )
{
	int ret;
	int i;
	struct bpa2_part *bpa2_part;
	const char *partname = {DD_BPA2_PARTITION};
	const char *input = {"stm_input_mix1"};
	const char *name;
	unsigned int size;
	unsigned int pagesize;
	stm_pixel_capture_buffer_descr_t * buf;
	bool input_found = false;

	/*
	 * Open the COMPO pixel capture object
	 */
	ret = stm_pixel_capture_open(STM_PIXEL_CAPTURE_COMPO,
					0,
					&outputPlug->pixel_capture);
	if (ret){
		printk(KERN_ERR "%s: failed to open pixel capture (%d)\n",
				__func__, ret);
		goto failed_open;
	}

	/*
	 * Search for the "stm_input_mix1" input of the compo pixel capture
	 * This is the output of the main compo
	 */
	for (i=0;; i++) {
		ret = stm_pixel_capture_enum_inputs(outputPlug->pixel_capture,
						   i, &name);
		if (ret)
			break;

		if (!strcmp(input, name)){
			input_found = true;
			break;
		}

		continue;
	}

	if (!input_found)
		goto failed_set_input;

	/*
	 * Select the stm_input_mix1 input of the compo pixel capture
	 */
	ret = stm_pixel_capture_set_input(outputPlug->pixel_capture, i);
	if (ret){
		printk(KERN_ERR "%s: failed to set input (%d)\n",
				__func__, ret);
		goto failed_set_input;
	}

	/*
	 * We LOCK that input of the pixel capture for our purpose
	 * FIXME: lock here might not be what we really want.
	 */
	ret = stm_pixel_capture_lock(outputPlug->pixel_capture);
	if (ret){
		printk(KERN_ERR "%s: failed to lock pixel_capture (%d)\n",
				__func__, ret);
		goto failed_lock;
	}

	/*
	 * Configure the pixel capture IP based on the status of the Main compo
	 */
	ret = output_config_compo_capture(outputPlug, sink);
	if (ret){
		printk(KERN_ERR "%s: failed to configure pixel capture (%d)\n",
				__func__, ret);
		goto failed_config;
	}

	/*
	 * Allocate buffers to be given to the pixel capture
	 */
	bpa2_part = bpa2_find_part(partname);
	if (!bpa2_part) {
		printk(KERN_ERR "%s: failed to get memory partition (%d)\n",
				__func__, ret);
		goto failed_find_part;
	}

	size = outputPlug->pixel_capture_format.stride *
		outputPlug->pixel_capture_format.height;
	pagesize = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	size = pagesize * PAGE_SIZE;

	for(i=0; i<outputPlug->nb_buffers; i++) {
		uint32_t buffer_address = 0;
		buf = &outputPlug->pixel_capture_buffer[i];

		memset(buf, 0, sizeof(stm_pixel_capture_buffer_descr_t));
		buf->cap_format = outputPlug->pixel_capture_format;
		buf->length = size;
		buf->bytesused = size;

		buffer_address = alloc_compo_buffer(bpa2_part, size,
						      pagesize);
		if (!buffer_address){
			printk(KERN_ERR "%s: failed to allocate buffer %d (size = %d | " \
					"pagesize = %d)\n",
					__func__, i, size, pagesize);
			goto failed_buffer_alloc;
		}

		switch (buf->cap_format.format) {
		case STM_PIXEL_FORMAT_RGB888:
		case STM_PIXEL_FORMAT_RGB565:
		case STM_PIXEL_FORMAT_ARGB8888:
		case STM_PIXEL_FORMAT_ARGB4444:
		case STM_PIXEL_FORMAT_ARGB1555:
		case STM_PIXEL_FORMAT_ARGB8565:
			buf->rgb_address  = buffer_address;
			break;

		case STM_PIXEL_FORMAT_YCbCr422R:
		case STM_PIXEL_FORMAT_YUV:
			buf->luma_address  = buffer_address;
			buf->chroma_offset = 0;
			break;

		case STM_PIXEL_FORMAT_YUV_NV12:
		case STM_PIXEL_FORMAT_YUV_NV16:
			buf->luma_address  = buffer_address;
			buf->chroma_offset = buf->cap_format.width * buf->cap_format.height;
			break;

		case STM_PIXEL_FORMAT_NONE:
		default:
			err_msg("%s: invalid capture pixel format type %d\n",
				 __FUNCTION__, buf->cap_format.format);
			goto failed_buffer_queue;
		}
	}

	/*
	 * Give all buffers to the pixel capture
	 */
	for(i=0; i<outputPlug->nb_buffers; i++) {
		buf = &outputPlug->pixel_capture_buffer[i];

		ret = stm_pixel_capture_queue_buffer(outputPlug->pixel_capture,
							buf);
		if (ret) {
			printk(KERN_ERR "%s: failed to queue buffer #%d (%d)\n",
					__func__, i, ret);
			goto failed_buffer_queue;
		}
	}

	/*
	 * Attach the pixel capture object to the sink
	 */
	ret = stm_pixel_capture_attach(outputPlug->pixel_capture, sink);
	if (ret) {
		printk(KERN_ERR "%s: failed to attach to plane (%d)\n",
			__func__, ret);
		goto failed_attach;
	}

	/* Start the pixel-capture */
	ret = stm_pixel_capture_start(outputPlug->pixel_capture);
	if (ret) {
		printk(KERN_ERR "%s: failed to start capture (%d)\n",
			__func__, ret);
		goto failed_start;
	}

	/* Report the Input window for the plane */
	iwin->rect.left = iwin->rect.top = 0;
	iwin->rect.width = outputPlug->pixel_capture_format.width;
	iwin->rect.height = outputPlug->pixel_capture_format.height;

	return 0;

failed_start:
	ret = stm_pixel_capture_detach(outputPlug->pixel_capture, sink);
	if (ret)
		printk(KERN_ERR "%s: failed to derach to plane (%d)\n",
			__func__, ret);
failed_attach:
	i = outputPlug->nb_buffers;
failed_buffer_queue:
	while(i--)
		stm_pixel_capture_dequeue_buffer(outputPlug->pixel_capture,
				 &outputPlug->pixel_capture_buffer[i]);
	i = outputPlug->nb_buffers;
failed_buffer_alloc:
	while(i--)
		bpa2_free_pages(bpa2_part,
				outputPlug->pixel_capture_buffer[i]
				.rgb_address);
failed_find_part:
failed_config:
	stm_pixel_capture_unlock(outputPlug->pixel_capture);
failed_lock:
failed_set_input:
	stm_pixel_capture_close(outputPlug->pixel_capture);
	outputPlug->pixel_capture = NULL;
failed_open:
	return ret;
}

/*
 * output_disable_compo_capture: This function disable the datapath
 * between the Main Compo and the selected sink (GDP)
 */
static void output_disable_compo_capture( struct output_plug *outputPlug,
					  stm_object_h sink )
{
	int ret;
	int i;
	struct bpa2_part *bpa2_part;
	const char *partname = {DD_BPA2_PARTITION};

	/*
	 * Stop the pixel capture object - this also dequeue all buffers
	 */
	ret = stm_pixel_capture_stop( outputPlug->pixel_capture );
	if (ret)
		printk(KERN_ERR "%s: failed to stop the pixel capture (%d)\n",
				__func__, ret);

	/*
	 * Detach the pixel capture object from the sink
	 */
	ret = stm_pixel_capture_detach( outputPlug->pixel_capture, sink );
	if (ret)
		printk(KERN_ERR "%s: failed to detach from sink (%d)\n",
				__func__, ret);

	/*
	 * Free the bpa2 allocated buffers
	 */
	bpa2_part = bpa2_find_part(partname);
	if (bpa2_part) {
		for (i=0; i<outputPlug->nb_buffers; i++){
			bpa2_free_pages(bpa2_part,
					outputPlug->pixel_capture_buffer[i]
					.rgb_address);
		}
	}

	/*
	 * Unlock the pixel capture object
	 */
	ret = stm_pixel_capture_unlock(outputPlug->pixel_capture);
	if (ret)
		printk(KERN_ERR "%s: failed to unlock (%d)\n",
				__func__, ret);

	/*
	 * Close the pixel capture object
	 */
	stm_pixel_capture_close(outputPlug->pixel_capture);
}

static int output_media_source_link_setup(struct media_entity *entity,
					 const struct media_pad *local,
					 const struct media_pad *remote,
					 u32 flags)
{
	struct output_plug *outputPlug;
	struct stmedia_v4l2_subdev *sd =
			entity_to_stmedia_v4l2_subdev(remote->entity);
	struct v4l2_subdev *v4l2_sd =
			media_entity_to_v4l2_subdev(remote->entity);
        struct v4l2_subdev_crop i_win;
	unsigned int id = 0;

	int ret;

	/* having the output media entity, get its container! */
	outputPlug = output_entity_to_output_plug(entity);

	if (flags & MEDIA_LNK_FL_ENABLED) {
		/* Only support a single connection after the compo */
		if (stm_media_find_remote_pad(local,
					      MEDIA_LNK_FL_ENABLED, &id)){
			/* There is already something connected - refuse */
			return -EBUSY;
		}

		/* we need to set up the sink first so that plane is ready
		 * to be attached from compo */
		ret = media_entity_call(remote->entity, link_setup,
					remote, local, flags);
		if (ret < 0)
			return ret;

		/*
		 * Setup the number of buffers to be used for capture depending on plane
		 * type.
		 */
		if(remote->entity->type == MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO)
			outputPlug->nb_buffers = DD_VID_BUFFERS_NB;
		else
			outputPlug->nb_buffers = DD_GFX_BUFFERS_NB;
		printk( KERN_ERR "%s: will allocated %d buffers for capture.\n",
				__func__, outputPlug->nb_buffers);

		ret = output_enable_compo_capture(outputPlug, sd->stm_obj,
						  &i_win);
		if (ret) {
			/* Reset the sink as well */
			media_entity_call(remote->entity, link_setup,
					  remote, local, 0);
			return ret;
		}

		/* Set plane input window */
		i_win.pad = 0;
		ret = v4l2_subdev_call(v4l2_sd, pad, set_crop, NULL, &i_win);
		if (ret)
			printk( KERN_ERR "%s: failed to set window (%d)\n",
				__func__, ret );
	} else {
		output_disable_compo_capture(outputPlug, sd->stm_obj);
	}

	return 0;
}

/**
 * output_connect_compo() - connect output to compo
 */
static int output_connect_compo(const struct media_pad *local,
					const struct media_pad *remote)
{
	int ret = 0;
	unsigned int colorspace;
	stm_display_mode_t mode;
	stm_display_mode_parameters_t *params;
	stm_display_mode_timing_t *timing;
	stm_rational_t aspect_ratio;
	struct v4l2_dv_timings dv_timings;
	struct v4l2_mbus_framefmt mbus_fmt;
	struct v4l2_subdev *compo_sd;
	struct output_plug *output;

	output = output_entity_to_output_plug(local->entity);
	compo_sd = media_entity_to_v4l2_subdev(remote->entity);

	/*
	 * Get the display mode
	 */
	ret = stm_display_output_get_current_display_mode(output->hOutput,
								&mode);
	if (ret) {
		pr_err("%s(): failed to get display mode\n", __func__);
		goto connect_failed;
	}

	memset(&dv_timings, 0, sizeof(struct v4l2_dv_timings));
	/*
	 * Convert the values to v4l2 compliant structure
	 */
	params = &mode.mode_params;
	timing = &mode.mode_timing;
	dv_timings.bt.interlaced = (params->scan_type == STM_PROGRESSIVE_SCAN ?
					V4L2_FIELD_NONE: V4L2_FIELD_INTERLACED);
	dv_timings.bt.pixelclock = timing->pixel_clock_freq;

	dv_timings.bt.width = params->active_area_width;
	dv_timings.bt.hfrontporch = params->active_area_start_pixel;
	dv_timings.bt.hsync = timing->hsync_width;
	dv_timings.bt.hbackporch = timing->pixels_per_line - (dv_timings.bt.width
					+ dv_timings.bt.hfrontporch + dv_timings.bt.hsync);

	dv_timings.bt.height = params->active_area_height;
	dv_timings.bt.vfrontporch = params->active_area_start_line;
	dv_timings.bt.vsync = timing->vsync_width;
	dv_timings.bt.vbackporch = timing->lines_per_frame - (dv_timings.bt.height
					+ dv_timings.bt.vfrontporch + dv_timings.bt.vsync);

	/*
	 * Set dv timings of compo subdev
	 */
	ret = v4l2_subdev_call(compo_sd, video, s_dv_timings, &dv_timings);
	if (ret) {
		pr_err("%s(): failed to set dv timing of compo subdev\n", __func__);
		goto connect_failed;
	}

	/*
	 * Prepare information for s_mbus_fmt of compo subdev
	 */
	ret = stm_display_output_get_control(output->hOutput,
					OUTPUT_CTRL_YCBCR_COLORSPACE,
					&colorspace);
	if (ret) {
		pr_err("%s(): failed to get the display colorspace\n", __func__);
		goto connect_failed;
	}

	/*
	 * FIXME: No way to propagate the pixel aspect ratio
	 */
	ret = stm_display_output_get_compound_control(output->hOutput,
						OUTPUT_CTRL_DISPLAY_ASPECT_RATIO,
						&aspect_ratio);
	if (ret) {
		pr_err("%s(): failed to get the pixel aspect ratio\n", __func__);
		goto connect_failed;
	}

	/*
	 * Configure the mbus fmt
	 */
	memset(&mbus_fmt, 0, sizeof(mbus_fmt));
	mbus_fmt.width = params->active_area_width;
	mbus_fmt.height = params->active_area_height;
	mbus_fmt.field = (params->scan_type == STM_PROGRESSIVE_SCAN ?
				V4L2_FIELD_NONE: V4L2_FIELD_INTERLACED);

	switch (colorspace) {
	case STM_YCBCR_COLORSPACE_AUTO_SELECT:
		mbus_fmt.colorspace = (params->output_standards &
				STM_OUTPUT_STD_HD_MASK) ? V4L2_COLORSPACE_REC709:
				V4L2_COLORSPACE_SMPTE170M;
		mbus_fmt.code = (mbus_fmt.colorspace == V4L2_COLORSPACE_REC709) ?
						V4L2_MBUS_FMT_YUV8_1X24:
						V4L2_MBUS_FMT_RGB888_1X24;
		break;

	case STM_YCBCR_COLORSPACE_709:
		mbus_fmt.colorspace = V4L2_COLORSPACE_REC709;
		mbus_fmt.code = V4L2_MBUS_FMT_YUV8_1X24;
		break;

	case STM_YCBCR_COLORSPACE_601:
		mbus_fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
		mbus_fmt.code = V4L2_MBUS_FMT_YUV8_1X24;
		break;

	default:
		mbus_fmt.colorspace = V4L2_COLORSPACE_SRGB;
		mbus_fmt.code = V4L2_MBUS_FMT_RGB888_1X24;
		break;
	}

	ret = v4l2_subdev_call(compo_sd, video, s_mbus_fmt, &mbus_fmt);
	if (ret)
		pr_err("%s(): failed to set mbus fmt of compo subdev\n", __func__);

connect_failed:
	return ret;
}

/**
 * stmedia_voutPlug_media_link_setup() - output link setup for tunneled compo
 *  @setups/starts the tunneled grab from compositor to plane. The data
 *  @path is output->plane. It can also setup a new data path where the
 *  @captured data is going to be pushed to be streaming engine. This
 *  @path is realized with data connection to compo subdev i.e.
 *  @output->compo->v4l2.video.
 *  @NOTE: the link setup between plane -> output is not performed here
 */
static int stmedia_voutPlug_media_link_setup(struct media_entity *entity,
					     const struct media_pad *local,
					     const struct media_pad *remote,
					     u32 flags)
{
	int ret = 0;

	switch (remote->entity->type) {
	case MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO:
	case MEDIA_ENT_T_V4L2_SUBDEV_PLANE_GRAPHIC:
		/*
		 * Tunneled compo capture to video/gfx
		 */
		if (local->flags == MEDIA_PAD_FL_SOURCE)
			ret = output_media_source_link_setup(entity,
							local, remote, flags);

		break;

	case MEDIA_ENT_T_V4L2_SUBDEV_COMPO:
		/*
		 * Tunneled compo configuration for compo capture
		 */
		if (flags & MEDIA_LNK_FL_ENABLED)
			ret = output_connect_compo(local, remote);

		break;

	default:
		break;
	}

	return ret;
}

static const struct media_entity_operations stmvout_voutPlug_media_ops = {
	.link_setup = stmedia_voutPlug_media_link_setup,
};

/* PAD operations:
 * TODO: It my be used in future
 */
static const struct v4l2_subdev_pad_ops stmedia_voutPlug_pad_ops = {
};

/* V4L2 sub-device operations include sub-device core operation
 * and media PAD operations*/
static const struct v4l2_subdev_ops stmedia_voutPlug_ops = {
	.core = &stmedia_voutPlug_core_ops,
	.pad = &stmedia_voutPlug_pad_ops,
};

/*
 * Search for a free slot in g_voutPlug table
 * A slot with a null name is assumed to be free.
 * Return the index or -1 in case of overflow.
 */
static inline int stmvout_get_free_voutPlug(void)
{
	int i;
	for (i = 0; i < STMEDIA_MAX_OUTPUTS; i++) {
		if (g_voutPlug[i].name[0] == '\0') {
			/* ... just to be sure, not mandatory */
			memset(&g_voutPlug[i], '\0', sizeof(g_voutPlug[i]));
			return (i);
		}
	}
	return (-1);
}

/*
 * Search a plane entity by name. Return the entity address
 * Return NULL if not found.
 */
struct media_entity *stmvout_get_voutPlug_entity_by_name(char *name)
{
	int i;
	for (i = 0; i < STMEDIA_MAX_OUTPUTS; i++) {
		if (g_voutPlug[i].name[0] != '\0') {
			if (strncmp
			    (g_voutPlug[i].name, name,
			     sizeof(g_voutPlug[i].name)) == 0)
				return (&g_voutPlug[i].sdev.entity);
		}
	}
	return (NULL);
}

static void stmvout_show_voutPlug(void)
{
#ifdef DEBUG_SHOW_OUTPUTS
	int i;
	printk(" %s:%s =====  MC output table ======\n", PKMOD, __func__);
	printk("  (In) represent MC input PAD flags\n");
	printk("  disp.H stand for Display handle; S for subdevice addr.\n");
	for (i = 0; i < STMEDIA_MAX_OUTPUTS; i++) {
		if (g_voutPlug[i].name[0] == '\0')
			break;	/* end of table */
		printk(" [%1d] %16s:%d type 0x%x disp.H %p S %p (%lx)\n",
		       i, g_voutPlug[i].name, g_voutPlug[i].id,
		       g_voutPlug[i].type,
		       g_voutPlug[i].hOutput, &g_voutPlug[i].sdev,
		       g_voutPlug[i].pads[0].flags);
	}
#endif
}

/*
 * Store a output object:
 * initialize & register a V4L2 subdev and media controller entity
 * Return 0 if ok, not zero in all other cases.
 */
static int stmvout_add_voutPlug_subdev(int Output,
				       const char *name,
				       stm_display_output_h hOutput,
				       stm_display_device_h hDisplay)
{
	int slot, ret = 0;	/* default is ok */
	char *msg = "???";

	struct v4l2_subdev *sd = NULL;
	struct media_entity *me = NULL;
	struct media_pad *pad = NULL;

	if ((slot = stmvout_get_free_voutPlug()) == -1) {
		msg = "g_voutPlug table OVERFLOW";
		ret = -255;	/* arbitrary */
		goto drop_voutPlug;
	}

	/* there is a free slot, try to record the output! */
	sd = &g_voutPlug[slot].sdev;
	me = &sd->entity;
	pad = g_voutPlug[slot].pads;

	/* store both: Display Plane ID and Pipeline ID */
	strlcpy(g_voutPlug[slot].name, name, sizeof(g_voutPlug[slot].name));
	g_voutPlug[slot].id = Output;	/* TODO: useful ? */
	g_voutPlug[slot].hOutput = hOutput;
	g_voutPlug[slot].hDisplay = hDisplay;
	/*    g_voutPlug[slot].users     = 0; */
	mutex_init(&g_voutPlug[slot].lock);

	/* initialize output sub-device:
	 *    - name =  keep Display plane names
	 *    - sub-device private data --> this entity descriptor
	 */
	v4l2_subdev_init(sd, &stmedia_voutPlug_ops);
	strlcpy(sd->name, g_voutPlug[slot].name, sizeof(sd->name));
	v4l2_set_subdevdata(sd, &g_voutPlug[slot]);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	/* Prepare PADs
	 * Generally outputs have one pad only (a SINK), however the first
	 * compositor of some devices also has a output PAD where it is
	 * possible to capture composite frames.
	 */
	g_voutPlug[slot].input_pads = 1;
	g_voutPlug[slot].output_pads = 0;
	pad[0].flags = MEDIA_PAD_FL_SINK;

	/* The Main compo output has the compo capture capabilities
	 * FIXME, should do the check via a capabilities rather the name
	 */
	if ( !strcmp(name, "analog_hdout0") ){
		g_voutPlug[slot].output_pads = 1;
		pad[1].flags = MEDIA_PAD_FL_SOURCE;
	}

	/* assume NO extra link initializing the entity */
	ret = media_entity_init(me,
				(g_voutPlug[slot].input_pads +
				 g_voutPlug[slot].output_pads), pad, 0);
	if (ret < 0) {
		msg = "initializing output Plug sub-device";
		goto drop_voutPlug;
	}

	/* complete the entity setup */
	me->ops = &stmvout_voutPlug_media_ops;
	me->type = MEDIA_ENT_T_V4L2_SUBDEV_OUTPUT;

	/* Register the sub-device  */
	ret = stm_media_register_v4l2_subdev(sd);
	if (ret < 0) {
		msg = "registering output Plug sub-device";
		goto drop_voutPlug;
	}
	debug_mc("MC: [%d] + %s/%d CD display %p;  pads I/O %d/%d\n",
		 slot, g_voutPlug[slot].name, g_voutPlug[slot].id,
		 g_voutPlug[slot].hDisplay,
		 g_voutPlug[slot].input_pads, g_voutPlug[slot].output_pads);
	return (0);

drop_voutPlug:
	/* this is signaled as an error, however the initialization skips the
	 * entity and attempts to go ahead!
	 * If not overflow, erase the g_voutData slot content!
	 */
	printk(KERN_ERR "vout Plug %s: MC failure - %s (%d)\n", name, msg, ret);
	if (slot != -1)
		memset(&g_voutPlug[slot], '\0', sizeof(g_voutPlug[slot]));
	return (ret);
}

/**
 * stmvout_remove_voutPlug_subdev
 * Remove the output subdev entries
 */

static void stmvout_remove_voutPlug_subdev(int count)
{
	struct v4l2_subdev *sd = &g_voutPlug[count].sdev;
	struct media_entity *me = &sd->entity;

	stm_media_unregister_v4l2_subdev(sd);

	media_entity_cleanup(me);

	memset(&g_voutPlug[count], 0, sizeof(g_voutPlug[count]));
}

/**
 * stmvout_probe_media_outputs
 * This initializes the media output. This function
 * is called during module_init.
 */
int stmvout_probe_media_outputs(void)
{
	stm_display_device_h hDisplay;
	stm_display_output_h hOutput;
	int Display, Output, ret = 0;

	debug_mc("==== %s executing!\n", __func__);
	/* scan all existing displays...
	 * Assume no more Displays when the open fails
	 */
	for (Display = 0;; Display++) {
		ret = stm_display_open_device(Display, &hDisplay);
		if (ret) {
			if (Display == 0) {
				printk(KERN_ERR
				       "%s: SoC without Display !?!?\n",
				       __func__);
				ret = -EFAULT;
				break;
			} else {
				/* Ok, assume no more display, just exit from loop */
				ret = 0;
				break;
			}
		}

		/* Search for all outputs of a Display, These will be part of MC graph.
		 * - Get plane capabilities, name and type.
		 * - Use name extracted from Display as MC entity and sub-device name.
		 * - A display without outputs is accepted (make sense?)
		 * - Valid outputs are kept open
		 * - failure of "open output" mean the Display has no more outputs
		 * - failure of any other "output operation" just skip the output
		 */
		for (Output = 0;; Output++) {
			const char *name = "";
			char *msg = "???";

			/* if the open fails, assumes no more planes! */
			if (stm_display_device_open_output
			    (hDisplay, Output, &hOutput))
				break;

			if (stm_display_output_get_name(hOutput, &name)) {
				msg = "has no name";
				goto skip_output;
			}

			/* record the output, this may fail:
			 *   - because of output table overflow
			 *   - because a V4L registering primitive fails
			 * in any case skip the output and try to continue!
			 */
			if (stmvout_add_voutPlug_subdev
			    (Output, name, hOutput, hDisplay)) {
				msg = "V4L2 registration failed";
				goto skip_output;
			}
			continue;
skip_output:
			/* close "bad" output plugs */
			stm_display_output_close(hOutput);
			printk(KERN_WARNING
			       "output plug (%d) %s skipped: %s! (%d)\n",
			       Output, name, msg, ret);

		}

		printk(KERN_INFO "%s: Display %d: %d output Plug added\n",
		       __func__, Display, Output);
	}

	stmvout_show_voutPlug();	/* debug only */

	return (ret);
}

/**
 * stmvout_release_media_outputs
 * This function closes all the output devices opened by the driver.
 * See: stmvout_probe_media_outputs
 */

static void stmvout_release_media_outputs(void)
{
	stm_display_device_h hDevice;
	stm_display_output_h hOutput;
	int index, count;

	for (index = 0; index < STMEDIA_MAX_OUTPUTS; index++) {
		if (!g_voutPlug[index].hDisplay)
			continue;

		hDevice = g_voutPlug[index].hDisplay;
		for (count = index; count < STMEDIA_MAX_OUTPUTS; count++) {
			if (g_voutPlug[count].hDisplay != hDevice)
				continue;

			hOutput = g_voutPlug[count].hOutput;
			stmvout_remove_voutPlug_subdev(count);
			stm_display_output_close(hOutput);
		}
		stm_display_device_close(hDevice);
	}
}

/* =======================================================================
 * Plane subdevice operations:
 */

static const struct v4l2_subdev_core_ops stmvout_core_ops = {
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
};

static int stmedia_plane_pad_get_crop(struct v4l2_subdev *sd,
				      struct v4l2_subdev_fh *fh,
				      struct v4l2_subdev_crop *crop)
{
	struct _stvout_device *plane;
	stm_rect_t rect;
	int ret;

	/* FIXME - this is not really good implementation, we should handle which field properly */

	if (crop->pad > 1)
		return -EINVAL;

	plane = v4l2_get_subdevdata(sd);

	ret = stm_display_plane_get_compound_control(plane->hPlane,
						     (crop->pad == 0)?PLANE_CTRL_INPUT_WINDOW_VALUE:PLANE_CTRL_OUTPUT_WINDOW_VALUE,
						     &rect);
	if (ret){
		debug_msg("%s: failed to get plane window value\n",
			  __FUNCTION__);
		return -EIO;
	}

	crop->rect.left = rect.x;
	crop->rect.top = rect.y;
	crop->rect.width = rect.width;
	crop->rect.height = rect.height;

	return 0;
}

/**
 * stmedia_plane_pad_set_crop() - configure input/output window dimensions
 * This subdev callback is executed in response to VIDIOC_S_FMT and
 * VIDIOC_S_CROP on V4L2 AV capture device. In response to these,
 * decoder finds out the respective plane connected and then perform
 * the operation on it.
 */
static int stmedia_plane_pad_set_crop(struct v4l2_subdev *sd,
				      struct v4l2_subdev_fh *fh,
				      struct v4l2_subdev_crop *crop)
{
	struct _stvout_device *plane;
	stm_compound_control_range_t range;
	stm_plane_mode_t mode;
	stm_rect_t rect;
	int ret;

	/*
	 * v4l2_subdev_crop.which indicates whether it's a try control
	 * or set control. At the moment, only set control is supported
	 */

	if (crop->pad > 1) {
		ret = -EINVAL;
		goto subdev_s_crop_done;
	}

	plane = v4l2_get_subdevdata(sd);

	/*
	 * Fill in the rectangle parameters for window configuration
	 */
	rect.x = crop->rect.left;
	rect.y = crop->rect.top;
	rect.width = crop->rect.width;
	rect.height = crop->rect.height;

	/*
	 * First check here, if the plane is configured in auto mode or
	 * manual mode. In auto mode, VIBE can override the values, and
	 * in manual mode, the only error possible is out of bounds
	 * values, which will be taken care. The point here is to indicate
	 * the user that AUTO MODE may not be setting the expected values.
	 */
	ret = stm_display_plane_get_control(plane->hPlane,
				(crop->pad == 0 ? PLANE_CTRL_INPUT_WINDOW_MODE :
				PLANE_CTRL_OUTPUT_WINDOW_MODE),
				&mode);
	if (ret) {
		err_msg("%s(): Failed to get mode for %s window\n",
				__func__, (crop->pad == 0 ? "Input" : "Output"));
		goto subdev_s_crop_done;
	}

	/*
	 * Throw a warning in case the plane is configured in AUTO mode
	 */
	if (mode ==  AUTO_MODE)
		printk(KERN_WARNING "%s(): %s() plane is configured in AUTO mode"
			"Expected cropping may not be achieved\n", __func__,
			(crop->pad == 0) ? "input" : "output");

	/*
	 * Get the range of the input/output window and adjust the request
	 * in accordance to these parameters
	 */
	ret = stm_display_plane_get_compound_control_range(plane->hPlane,
					(crop->pad == 0 ? PLANE_CTRL_INPUT_WINDOW_VALUE:
					PLANE_CTRL_OUTPUT_WINDOW_VALUE),
					&range);
	if (ret) {
		err_msg("%s(): Failed to get plane window range\n", __func__);
		goto subdev_s_crop_done;
	}

	/*
	 * Compare the control range values with the user-specified values
	 * and find out the correct values to set.
	 * TODO: At the moment, rectangle max values contain a value of
	 * (x,y) as (0,0). So, there is no need to cap here the (x,y) values.
	 * It has also been noticed that out of bounds values of (x,y) are
	 * handled by CoreDisplay but not of width and height. So, in case,
	 * there is an update in behaviour of CoreDisplay API, then we need
	 * not get the range, and directly call the API to sent the cropping
	 * parameters.
	 */
	if (rect.x < range.min_val.x)
		rect.x = range.min_val.x;

	if (rect.y < range.min_val.y)
		rect.y = range.min_val.y;

	/*
	 * If we set the width/height to CoreDisplay min, then, CoreDisplay
	 * errors out, so, adding 1 to make CoreDisplay happy
	 */
	if (rect.width <= range.min_val.width)
		rect.width = range.min_val.width + 1;
	else if (rect.width > range.max_val.width)
		rect.width = range.max_val.width;

	if (rect.height <= range.min_val.height)
		rect.height = range.min_val.height + 1;
	else if (rect.height > range.max_val.height)
		rect.height = range.max_val.height;

	ret = stm_display_plane_set_compound_control(plane->hPlane,
					(crop->pad == 0) ? PLANE_CTRL_INPUT_WINDOW_VALUE :
					PLANE_CTRL_OUTPUT_WINDOW_VALUE,
					&rect);
	if (ret) {
		err_msg("%s: failed to set plane window value\n", __func__);
		goto subdev_s_crop_done;
	}

	/*
	 * It's VIDIOC_S_FMT for output plane, so, update the cropping
	 * parameters with the latest value
	 */
	if (crop->pad == 1) {
		ret = stm_display_plane_get_compound_control(plane->hPlane,
					PLANE_CTRL_OUTPUT_WINDOW_VALUE,
					&rect);
		if (ret) {
			err_msg("%s(): failed to get output window params\n", __func__);
			goto subdev_s_crop_done;
		}

		crop->rect.left = rect.x;
		crop->rect.top = rect.y;
		crop->rect.width = rect.width;
		crop->rect.height = rect.height;
	}

subdev_s_crop_done:
	return ret;
}

static const struct v4l2_subdev_pad_ops stmedia_plane_pad_ops = {
	.get_crop = &stmedia_plane_pad_get_crop,
	.set_crop = &stmedia_plane_pad_set_crop,
};

/** map_mbusfmt_to_pixelstream() - Maps the mbusfmt to the pixelstream timing
 * 			           structure.
 * @pre The system should be initialized.
 * @param[in] *fmt the mbus fmt structure
 * @param[out] *pixelstream_param The pixel stream timing structure.
 * @return 0 if mappping successful.
 *     -EINVAL if error.
 */
static int map_mbusfmt_to_pixelstream(struct v4l2_mbus_framefmt *fmt,
					  stm_display_source_pixelstream_params_t
					  *pixelstream_param)
{
	/* this will replace the existing flags set by s_dv_timings.
	But its OK and it ensures that flags are reset*/
	pixelstream_param->flags = 0;
	pixelstream_param->visible_area.width = fmt->width;
	pixelstream_param->visible_area.height = fmt->height;

	switch(fmt->code) {
		case V4L2_MBUS_FMT_STM_RGB8_3x8:
		case V4L2_MBUS_FMT_STM_RGB10_3x10:
		case V4L2_MBUS_FMT_STM_RGB12_3x12:
			pixelstream_param->colorType = STM_PIXELSTREAM_SRC_RGB;
			break;
		case V4L2_MBUS_FMT_YUYV8_2X8:
		case V4L2_MBUS_FMT_YUYV10_2X10:
			pixelstream_param->colorType = STM_PIXELSTREAM_SRC_YUV_422;
			break;
		case V4L2_MBUS_FMT_STM_YUV8_3X8:
		case V4L2_MBUS_FMT_STM_YUV10_3X10:
		case V4L2_MBUS_FMT_STM_YUV12_3X12:
			pixelstream_param->colorType = STM_PIXELSTREAM_SRC_YUV_444;
			break;
	}
	switch(fmt->colorspace) {
		case V4L2_COLORSPACE_REC709:
			pixelstream_param->flags |=
			    STM_PIXELSTREAM_SRC_COLORSPACE_709;
			break;
		default:
			break;
	}

	if (fmt->field == V4L2_FIELD_INTERLACED) {
		pixelstream_param->flags |= STM_PIXELSTREAM_SRC_INTERLACED;
	}

	return 0;
}


/** map_dvtimings_to_pixelstream() - Maps the dvtiming info to the pixelstream timing
 * 			           structure.
 * @pre The system should be initialized.
 * @param[in] *timings the dv_timings structure
 * @param[out] *pixelstream_param The pixel stream timing structure.
 * @return 0 if mappping successful.
 *     -EINVAL if error.
 */
static int map_dvtimings_to_pixelstream(struct v4l2_dv_timings *timings,
					  stm_display_source_pixelstream_params_t
					  *pixelstream_param)
{
	uint32_t total_lines = 0;

	pixelstream_param->htotal = timings->bt.width + timings->bt.hsync + timings->bt.hfrontporch + timings->bt.hbackporch;
	pixelstream_param->vtotal = timings->bt.height + timings->bt.vsync + timings->bt.vfrontporch + timings->bt.vbackporch;
	total_lines =
	    pixelstream_param->htotal * pixelstream_param->vtotal;
	if(total_lines){
		timings->bt.pixelclock *= 1000;/* we want frame rate multiplied by 1000*/
		do_div(timings->bt.pixelclock, total_lines);
		/* do_div has strange sematics*/
		pixelstream_param->src_frame_rate = timings->bt.pixelclock;
	}
	else
		pixelstream_param->src_frame_rate = 0;

	pixelstream_param->active_window.x = timings->bt.hfrontporch;
	pixelstream_param->active_window.y = timings->bt.vfrontporch;
	pixelstream_param->active_window.width = timings->bt.width;
	pixelstream_param->active_window.height = timings->bt.height;
	pixelstream_param->visible_area.x = 0;
	pixelstream_param->visible_area.y = 0;
	pixelstream_param->visible_area.width = timings->bt.width;
	pixelstream_param->visible_area.height = timings->bt.height;
	pixelstream_param->pixel_aspect_ratio.numerator = 1;/* probably this should move to s_crop*/
	pixelstream_param->pixel_aspect_ratio.denominator = 1;

	if (timings->bt.interlaced){
		pixelstream_param->flags |= STM_PIXELSTREAM_SRC_INTERLACED;
		pixelstream_param->visible_area.height *= 2; /* for interlaced need to multiply by 2 */
	}else {
		pixelstream_param->flags &= ~STM_PIXELSTREAM_SRC_INTERLACED;
	}

	return 0;
}

static int stmedia_plane_video_s_dv_timings(struct v4l2_subdev *sd,
			struct v4l2_dv_timings *timings)
{
	struct _stvout_device *plane;
	int ret;
	struct stm_source_info *src_info;
	if (!timings)
		return -EINVAL;

	plane = v4l2_get_subdevdata(sd);
	src_info = plane->source_info;

	if(src_info->iface_type != STM_SOURCE_PIXELSTREAM_IFACE)
		return -EINVAL;

	ret = map_dvtimings_to_pixelstream(timings, &(src_info->pixel_param));

	return ret;
}

static int stmedia_plane_video_s_mbus_fmt(struct v4l2_subdev *sd,
						struct v4l2_mbus_framefmt *fmt)
{
	struct _stvout_device *plane;
	struct stm_source_info *src_info;
	stm_display_source_pixelstream_params_t *param;
	int ret;

	if (!fmt)
		return -EINVAL;

	plane = v4l2_get_subdevdata(sd);

	src_info = plane->source_info;
	param = &src_info->pixel_param;

	if(src_info->iface_type != STM_SOURCE_PIXELSTREAM_IFACE)
		return -EINVAL;

	ret = map_mbusfmt_to_pixelstream(fmt, param);

	if (src_info->pixeltype == 0) /* 0 stands for PIX_HDMI source*/
		param->flags |= STM_PIXELSTREAM_SRC_CAPTURE_BASED_ON_DE;

	printk(KERN_DEBUG "%s(): Will try to set following pixelstream params\n", __func__);
	printk(KERN_DEBUG "--> htotal = %d\n", param->htotal);
	printk(KERN_DEBUG "--> vtotal = %d\n", param->vtotal);
	printk(KERN_DEBUG "--> source frame rate = %d\n", param->src_frame_rate);
	printk(KERN_DEBUG "--> flags = 0x%x\n", param->flags);
	printk(KERN_DEBUG "--> active window height = %d\n", param->active_window.height);
	printk(KERN_DEBUG "--> active window width = %d\n", param->active_window.width);
	printk(KERN_DEBUG "--> active window X = %d\n", param->active_window.x);
	printk(KERN_DEBUG "--> active window Y = %d\n", param->active_window.y);
	printk(KERN_DEBUG "%s(): pixelstream params ends\n", __func__);

	ret = stm_display_source_pixelstream_set_input_params
					(src_info->iface_handle, param);
	if (ret)
		printk(KERN_ERR "%s(): Failed to set pixelstream params\n", __func__);

	return ret;
}

static const struct v4l2_subdev_video_ops stmedia_plane_video_ops = {
	.s_dv_timings = &stmedia_plane_video_s_dv_timings,
	.s_mbus_fmt = &stmedia_plane_video_s_mbus_fmt,
};


/* -----------------------------------------------------------------------
 * Media link operations:
 */
static int stmedia_plane_link_to_output(struct media_entity *local,
					struct media_entity *remote,
					unsigned int state)
{
	struct _stvout_device *stvout_device;
	struct output_plug *output_plug;
	int ret;

	stvout_device = plane_entity_to_stvout_device(local);
	output_plug = output_entity_to_output_plug(remote);

	if (state == 1) {
		ret =
		    stm_display_plane_connect_to_output(stvout_device->hPlane,
							output_plug->hOutput);
	} else {
		ret =
		    stm_display_plane_disconnect_from_output(stvout_device->
							     hPlane,
							     output_plug->
							     hOutput);
	}

	if (ret)
		return -EIO;

	return 0;
}

static int plane_media_source_link_setup(struct media_entity *entity,
					 const struct media_pad *local,
					 const struct media_pad *remote,
					 u32 flags)
{
	if (flags & MEDIA_LNK_FL_ENABLED) {
		if (media_entity_remote_source((struct media_pad *)local))
			return -EINVAL;

		/* Only in case of Output connect we need to do something special */
		if (remote->entity->type == MEDIA_ENT_T_V4L2_SUBDEV_OUTPUT) {
			/* We are requested to connect the plane to the output */
			return stmedia_plane_link_to_output(entity,
							    remote->entity, 1);
		}
	} else {
		/* Only in case of Output connect we need to do something special */
		if (remote->entity->type == MEDIA_ENT_T_V4L2_SUBDEV_OUTPUT) {
			/* We are requested to disconnect the plane from the output */
			return stmedia_plane_link_to_output(entity,
							    remote->entity, 0);
		}
	}

	/* NOT REACHED */
	return 0;
}

struct subdev_name_fbtype {
	char *name;
	int type;
};


struct subdev_name_fbtype subdev_names[] = {
	{STM_HDMIRX_SUBDEV, 0},
	{STM_VXI_SUBDEV, 2},
};

char* search_subdev_name(const char *name, int *index)
{
	char *substr;
	if(*index == (ARRAY_SIZE(subdev_names))){
		*index = -1;
		return NULL;
	}

	substr = strstr(name, subdev_names[*index].name);
	if(!substr) {
		(*index)++;
		substr = search_subdev_name(name, index);
	}

	return substr;
}

static int getpixeltype_inst_from_name(const char *name, int *type, int *inst)
{
	char *substr;
	int ret = -EINVAL, index = 0;

	substr = search_subdev_name(name, &index);
	if(!substr){
		printk(KERN_ALERT"\nsearch subdev failed");
		return -EINVAL;
	}
	substr = substr + strlen(subdev_names[index].name);
	*type = subdev_names[index].type;
	ret = kstrtou32( substr, 10, inst);
	if(ret)
		printk(KERN_ALERT"conversion of instance failed");

	return ret;
}

static int stmvout_put_source_info(struct stm_source_info **src_info)
{
	int src_usage_count = 0;

	src_usage_count =
		stm_display_source_get_connected_plane_id((*src_info)->source,
							NULL, 0);

	debug_msg("Planes connected to source : %d\n", src_usage_count);

	/* return if source is still used */
	if (src_usage_count){
		*src_info = NULL;
		return 0;
	}
	if((*src_info)->iface_handle)
		stmvout_release_interface((*src_info)->iface_handle, (*src_info)->iface_type);
	stm_display_source_close((*src_info)->source);

	kfree(*src_info);
	*src_info = NULL;
	return 0;
}


static int stmvout_get_source_info(struct stm_source_info **src_info,
				stm_display_device_h hDisplay,
				stm_display_plane_h hPlane,
				stm_display_source_interface_params_t *src_param,
				u32 remote_source_type)
{
	int ret = 0;

	/* this is the case that we are reusing some src_info*/
	if(*src_info){
		/* we increment the ref count so that src_info is not freed from under our feet !*/
		ret = stm_display_plane_connect_to_source(hPlane, (*src_info)->source);
		return ret;
	}

	/* we need to allocate a new source info */
	*src_info = kzalloc(sizeof(**src_info), GFP_KERNEL);
	if(!*src_info){
		return -ENOMEM;
	}

	ret = stmvout_get_iface_source(hDisplay, hPlane, src_param,
		*src_info);
	if(ret) {
		kfree(*src_info);
		return ret;
	}

	if ((remote_source_type == MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER) ||
				(remote_source_type == MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER)) {
		/* For this case the manifestor will allocate the queue interface */
		stmvout_release_interface((*src_info)->iface_handle, STM_SOURCE_QUEUE_IFACE);
		(*src_info)->iface_handle = NULL;
	}

	return ret;
}

static int stmvout_plane_disconnect_from_src(stvout_device_t *vout)
{
	int ret;

	if(vout->source_info == NULL) /* Already freed ?*/
		return 0;

	debug_msg("we are disconecting the source %p \n", vout->hSource);


	if(down_interruptible(&vout->devLock))
		return -ERESTARTSYS;

	ret = stm_display_plane_disconnect_from_source(vout->hPlane, vout->hSource);
	if(ret) {
		up(&vout->devLock);
		return -EINVAL;
	}

	ret = stmvout_put_source_info(&(vout->source_info));
	if(ret) {
		up(&vout->devLock);
		return -EINVAL;
	}

	vout->hQueueInterface = NULL;
	vout->hSource= NULL;
	vout->stmedia_sdev.stm_obj = NULL;

	up(&vout->devLock);
	return 0;
}

static int stmvout_plane_connect_to_src(stvout_device_t *vout, struct stm_source_info *source_info,
				stm_display_source_interface_params_t *src_param, u32 remote_source_type)
{

	int ret = 0;
	struct v4l2_control ctrl;

	if(vout->source_info)
		return -EINVAL;

	if(down_interruptible(&vout->devLock))
		return -ERESTARTSYS;


	/* Increment the ref count  and allocate if NULL*/
	ret = stmvout_get_source_info(&(source_info), vout->hDisplay, vout->hPlane, src_param, remote_source_type);
	if(ret){
		up(&vout->devLock);
		return ret;
	}
	vout->source_info = source_info;
	vout->hSource = vout->source_info->source;

	if(vout->source_info->iface_type == STM_SOURCE_QUEUE_IFACE) {
		vout->hQueueInterface =
				vout->source_info->iface_handle;
	}else {
		/* set plane control ignore aspect ratio conversion. Ideally user has to set
		from application. Since we have internal problems in pixel interface, we set it.*/
		ctrl.id = V4L2_CID_STM_ASPECT_RATIO_CONV_MODE;
		ctrl.value = VCSASPECT_RATIO_CONV_IGNORE;
		ret = v4l2_subdev_call(&vout->stmedia_sdev.sdev, core, s_ctrl, &ctrl);
		if (ret)
			printk(KERN_ALERT"set plane control ignore aspect ratio conversion Failed");
	}

	vout->stmedia_sdev.stm_obj = vout->hSource;
	debug_msg("Allocated a new source %p\n", vout->hSource);
	up(&vout->devLock);

	return 0;
}

static int get_ifacetype_and_inst(struct media_entity *entity,
			stm_display_source_interface_params_t *src_param)
{
	int instance, type, ret = 0;
	stm_display_pixel_stream_params_t  *pixel_params;

	switch(entity->type) {
	case MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER:
	case MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER:
	case MEDIA_ENT_T_V4L2_SUBDEV_OUTPUT:
	case MEDIA_ENT_T_DEVNODE_V4L:
		src_param->interface_type = STM_SOURCE_QUEUE_IFACE;
		break;
	case MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX:
	case MEDIA_ENT_T_V4L2_SUBDEV_VXI:
		src_param->interface_type = STM_SOURCE_PIXELSTREAM_IFACE;

		ret = getpixeltype_inst_from_name(entity->name, &type, &instance);
		if(ret < 0)
			return -EINVAL;
		pixel_params = src_param->interface_params.pixel_stream_params;
		pixel_params->instance_number = instance;
		pixel_params->source_type = type;

		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static bool check_srcinfo_usable(struct stm_source_info *src_info,
			stm_display_source_interface_params_t *src_param, u32 remote_source_type)
{
	stm_display_pixel_stream_params_t  *pix_params = src_param->interface_params.pixel_stream_params;

	/* Shortcut optimisation w/o checking type of interface!! */
	if ((src_info->iface_type == src_param->interface_type)
		&& (pix_params->source_type == src_info->pixeltype)
		&& (pix_params->instance_number == src_info->pixel_inst)){
		if ((remote_source_type == MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER) ||
				(remote_source_type == MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER)) { /*Special case for video decoder*/
				if(src_info->iface_handle)
					return false;
			}
			else {
				if(!src_info->iface_handle)
					return false;
			}
			return true;
	}
	return false;
}

/*
 * plane_config_for_compo_capture: Helper function which configure the
 * plane properly when being used for tunneled compo capture
 */
static int plane_config_for_compo_capture(struct _stvout_device *stvout_device)
{
	stm_display_output_h hOutput;
	stm_display_mode_t mode_line = { STM_TIMING_MODE_RESERVED };
	stm_rect_t o_win;
	int ret;

	/* Set the INPUT mode to manual */
	ret = stm_display_plane_set_control(stvout_device->hPlane,
				    PLANE_CTRL_INPUT_WINDOW_MODE,
				    MANUAL_MODE);
	if (ret){
		printk(KERN_ERR "%s: failed to set input mode (%d)\n",
				__func__, ret);
		goto failed;
	}

	/* Get the output resolution and set the Plane output window */
	hOutput = stmvout_get_output_display_handle( stvout_device );
	if (!hOutput){
		printk(KERN_ERR "%s: failed to get handle\n",
				__func__);
		goto failed;
	}

	ret = stm_display_output_get_current_display_mode( hOutput,
						&mode_line);
	if (ret){
		printk(KERN_ERR "%s: failed to get mode (%d)\n",
				__func__, ret);
		goto failed;
	}

	o_win.x = o_win.y = 0;
	o_win.width = mode_line.mode_params.active_area_width;
	o_win.height = mode_line.mode_params.active_area_height;
	ret = stm_display_plane_set_compound_control(
					stvout_device->hPlane,
					PLANE_CTRL_OUTPUT_WINDOW_VALUE,
					&o_win);
	if (ret){
		printk(KERN_ERR "%s: failed to set output win (%d)\n",
				__func__, ret);
		goto failed;
	}

	ret = stm_display_plane_set_control(stvout_device->hPlane,
				    PLANE_CTRL_OUTPUT_WINDOW_MODE,
				    MANUAL_MODE);
	if (ret){
		printk(KERN_ERR "%s: failed to set output mode (%d)\n",
				__func__, ret);
		goto failed;
	}

	return 0;

failed:
	return ret;
}

/**********************************************************************
* Create source for display plane and connect it.
* 1. Connect display source to display plane
* 2. Only one plane can be connected to display source
* 3. If the source is already available, do nothing
because connection is already there
**********************************************************************/
static int plane_media_sink_link_setup(struct media_entity *entity,
					 const struct media_pad *local,
					 const struct media_pad *remote,
					 u32 flags)
{
	int id = 0;
	int ret;
	struct media_pad *temp_pad=NULL;
	struct _stvout_device *stvout_device;
	stm_display_source_interface_params_t src_param = {0};
	 stm_display_pixel_stream_params_t  pix_params = {0};

	stvout_device = plane_entity_to_stvout_device(entity);

	if (!(flags & MEDIA_LNK_FL_ENABLED)) {

	/*
	 * do nothing, as for cannes only one source is allowed to connect to plane.
	 * Sharing of source is not allowed.
	 */
		return 0;
	}

	/*
	 * On cannes We accept to have several decoders connected temporarly to a single plane but
	 * we must fail if the same decoder is connected twice to handle the fact that in
	 * STM implementation the orginal mediacontroller call on any source pad generates
	 * a call to the sink pad . First call on the pad must accept the connection,
	 * 2nd call must fail
	 */

	/*
	 * Let's review all currently connected remote pad and fail only if one of the
	 * already connected pad is passed again in this function as the *remote
	 */

	temp_pad =
            stm_media_find_remote_pad((struct media_pad *)local,
                                      MEDIA_LNK_FL_ENABLED, &id);
	while (temp_pad != NULL) 	{

		if (temp_pad == remote)
			return -EBUSY;

		temp_pad =
		stm_media_find_remote_pad((struct media_pad *)local,
					MEDIA_LNK_FL_ENABLED, &id);

	}

	/* Do some specific initialization of the plane */
	if (remote->entity->type == MEDIA_ENT_T_V4L2_SUBDEV_OUTPUT) {
		ret = plane_config_for_compo_capture(stvout_device);
		if (ret)
			return ret;
	}

	src_param.interface_params.pixel_stream_params = &pix_params;
        ret = get_ifacetype_and_inst(remote->entity, &src_param);
        if(ret)
                return -EINVAL;

	/* Check if the source is already created for this plane */
	if(stvout_device->source_info) {
		if(check_srcinfo_usable(stvout_device->source_info, &src_param,
						remote->entity->type)){
			stvout_device->stmedia_sdev.stm_obj = stvout_device->hSource;
			debug_msg
			("Plane link setup: resuing the same source %p\n",
			stvout_device->stmedia_sdev.stm_obj);
			return 0;
		}

		/* we have to free the current source info*/
		ret = stmvout_plane_disconnect_from_src(stvout_device);
		if(ret)
			return ret;

	}

	ret = stmvout_plane_connect_to_src(stvout_device, NULL, &src_param, remote->entity->type);

	debug_msg("Plane link setup: Plane %s is connected to source %p\n", local->entity->name, stvout_device->stmedia_sdev.stm_obj);
	return ret;
}

static int stmedia_plane_media_link_setup(struct media_entity *entity,
					  const struct media_pad *local,
					  const struct media_pad *remote,
					  u32 flags)
{
	debug_link("%s: ===> linking entity %s --> %s\n", __func__,
		   entity->name, remote->entity->name);

	/* We only handle the SOURCE pad - the one connected to the output */
	if (local->flags == MEDIA_PAD_FL_SOURCE)
		return plane_media_source_link_setup(entity,
						     local,
						     remote,
						     flags);
	else
		return plane_media_sink_link_setup(entity,
					local, remote, flags);

}

static const struct media_entity_operations stmvout_plane_media_ops = {
	.link_setup = stmedia_plane_media_link_setup,
};

/* V4L2 sub-device operations include sub-device core operation
 * and media PAD operations*/
static const struct v4l2_subdev_ops stmedia_plane_ops = {
	.core      = &stmvout_core_ops,
	.pad	   = &stmedia_plane_pad_ops,
	.video     = &stmedia_plane_video_ops,
};

/*
 * Search for a free slot in g_voutData table
 * Assumption: a slot which has a null name is free!
 * Return the index or -1 in case of overflow.
 */
static inline int stmvout_get_free_plane(void)
{
	int i;
	for (i = 0; i < STMEDIA_MAX_PLANES; i++) {
		if (g_voutRoot.pDev[i].name[0] == '\0') {
			/* ... just to be sure, not mandatory */
			memset(&g_voutRoot.pDev[i], '\0',
			       sizeof(g_voutRoot.pDev[i]));
			return (i);
		}
	}
	return (-1);
}

/*
 * Search for shared planes
 * Scans the entire g_voutData table to search a plane with "name"
 * already exists. Search only different Display.
 * Returns the intdex of first Shared (if any), returns 0 if not found
 */
static int stmvout_plane_already_added(stm_display_device_h hDisplay,
				       const char *name)
{
	int i;
	for (i = 0; i < STMEDIA_MAX_PLANES; i++) {
		if (g_voutRoot.pDev[i].hDisplay == hDisplay)
			if (strncmp
			    (g_voutRoot.pDev[i].name, name,
			     sizeof(g_voutRoot.pDev[i].name)) == 0)
				return (i);
	}
	return (0);
}

static void stmvout_show_planes(void)
{
#ifdef DEBUG_SHOW_PLANES
	int i;
	printk(" %s:%s =====  MC plane table ======\n", PKMOD, __func__);
	printk("  (0xIn, 0xOut) represent MC PAD flag for Input Output\n");
	printk("  disp.H stand for Display handle; S for subdevice addr.\n");
	for (i = 0; i < STMEDIA_MAX_PLANES; i++) {
		if (g_voutRoot.pDev[i].name[0] == '\0')
			break;	/* end of table */
		printk(" [%2d] %14s:%d type 0x%x disp.H %p S %p (%lx,%lx)\n",
		       i, g_voutRoot.pDev[i].name, g_voutRoot.pDev[i].id,
		       g_voutRoot.pDev[i].type,
		       g_voutRoot.pDev[i].hPlane, &g_voutRoot.pDev[i].stmedia_sdev.sdev,
		       g_voutRoot.pDev[i].pads[0].flags,
		       g_voutRoot.pDev[i].pads[1].flags);
	}
#endif
}

/*
 * Configure a OUTPUT plane:
 * initialize & register a V4L2 subdev and media controller entity
 * Return 0 if ok, not zero in all other cases.
 */
static int stmvout_add_plane_subdev(int Plane,
				    const char *name,
				    stm_display_plane_h hPlane,
				    unsigned int type,
				    stm_display_device_h hDisplay)
{
	int slot, ret = 0;	/* default is ok */
	char *msg = "???";

	struct v4l2_subdev *sd = NULL;
	struct media_entity *me = NULL;
	struct media_pad *pad = NULL;

	debug_mc("%s ========== plane %d %s\n", __func__, Plane, name);

	if ((slot = stmvout_get_free_plane()) == -1) {
		msg = "g_voutRoot.pDev table OVERFLOW";
		ret = -255;	/* arbitrary */
		goto drop_plane;
	}

	/* there is an available slot! ... go ahead */
	sd = &g_voutRoot.pDev[slot].stmedia_sdev.sdev;
	me = &sd->entity;
	pad = g_voutRoot.pDev[slot].pads;

	/* save all useful references to Display */
	strlcpy(g_voutRoot.pDev[slot].name, name,
		sizeof(g_voutRoot.pDev[slot].name));
	g_voutRoot.pDev[slot].id = Plane;	/* TODO: useful ? */
	g_voutRoot.pDev[slot].hPlane = hPlane;
	g_voutRoot.pDev[slot].hDisplay = hDisplay;
	g_voutRoot.pDev[slot].type = type;	/* video or graphic plane */
	g_voutRoot.pDev[slot].source_info = NULL;

	init_waitqueue_head(&g_voutRoot.pDev[slot].wqBufDQueue);
	init_waitqueue_head(&g_voutRoot.pDev[slot].wqStreamingState);
	sema_init(&g_voutRoot.pDev[slot].devLock, 1);

	/* Set plane initially to opaque, and do not connect plane
	 * to output, this will happen later  */
	g_voutRoot.pDev[slot].globalAlpha = 255;	/* Initially set to opaque */
	g_voutRoot.pDev[slot].alpha_control = CONTROL_OFF;
	g_voutRoot.pDev[slot].currentOutputNum = -1;

	/* initialize output plane sub-device:
	 *    - name =  keep Display plane names
	 *    - sub-device private data --> back to this output object
	 */
	v4l2_subdev_init(sd, &stmedia_plane_ops);
	strlcpy(sd->name, g_voutRoot.pDev[slot].name, sizeof(sd->name));
	v4l2_set_subdevdata(sd, &g_voutRoot.pDev[slot]);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	/* Prepare PADs (output planes have two pads: a SINK and a SOURCE) */
	g_voutRoot.pDev[slot].input_pads = 1;
	g_voutRoot.pDev[slot].output_pads = 1;
	pad[0].flags = MEDIA_PAD_FL_SINK;
	pad[1].flags = MEDIA_PAD_FL_SOURCE;

	/* assume NO extra link initializing the entity */
	debug_mc("    [%d] adding entity %p pads %d (extra %d)\n",
		 slot, me,
		 (g_voutRoot.pDev[slot].input_pads +
		  g_voutRoot.pDev[slot].output_pads), 0);

	ret = media_entity_init(me,
				(g_voutRoot.pDev[slot].input_pads +
				 g_voutRoot.pDev[slot].output_pads), pad, 0);
	if (ret < 0) {
		msg = "initializing output sub-device";
		goto drop_plane;
	}

	/* complete the entity setup */
	me->ops = &stmvout_plane_media_ops;
	if (type == STMEDIA_PLANE_TYPE_VIDEO) {
		me->type = MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO;
	} else if (type == STMEDIA_PLANE_TYPE_GRAPHICS) {
		me->type = MEDIA_ENT_T_V4L2_SUBDEV_PLANE_GRAPHIC;
	} else {
		me->type = MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VBI;
	}

	/* Initialize the control handler*/
	ret = stvmout_init_ctrl_handler(&g_voutRoot.pDev[slot]);
	if (ret < 0) {
		msg = "Control handler init failed";
		goto drop_plane;
	}

	sd->ctrl_handler = &g_voutRoot.pDev[slot].ctrl_handler;
	/* Register the sub-device  */
	ret = stm_media_register_v4l2_subdev(sd);
	if (ret < 0) {
		msg = "registering output sub-device";
		goto drop_plane;
	}

	debug_mc("MC: [%d] + %s/%d CD (display %p, plane %p) pads I/O %d/%d\n",
		 slot, g_voutRoot.pDev[slot].name, g_voutRoot.pDev[slot].id,
		 g_voutRoot.pDev[slot].hDisplay, g_voutRoot.pDev[slot].hPlane,
		 g_voutRoot.pDev[slot].input_pads,
		 g_voutRoot.pDev[slot].output_pads);

	return (0);		/* ok */

drop_plane:
	/* this is signaled as an error, however the initialization skips the
	 * entity and attempts to go ahead!
	 * If not overflow, erase the g_voutData slot content!
	 */
	printk(KERN_ERR "output %s: MC failure - %s (%d)\n", name, msg, ret);
	if (slot != -1)
		memset(&g_voutRoot.pDev[slot], '\0',
		       sizeof(g_voutRoot.pDev[slot]));
	return (ret);
}

/**
 * stmvout_remove_plane_subdev
 * This removes the sub-device from the v4l2-device and cleans up
 * the registration with media entity driver
 */
static void stmvout_remove_plane_subdev(int count)
{
	struct v4l2_subdev *sd = &g_voutRoot.pDev[count].stmedia_sdev.sdev;
	struct media_entity *me =  &sd->entity;

	v4l2_ctrl_handler_free(&g_voutRoot.pDev[count].ctrl_handler);

	stm_media_unregister_v4l2_subdev(sd);
	media_entity_cleanup(me);

	/* Reset the gvoutData particular device context */
	memset(&g_voutRoot.pDev[count], 0, sizeof(g_voutRoot.pDev[count]));
}

/**
 * stmvout_probe_media_planes
 * This function probes for the display planes
 */

static int stmvout_probe_media_planes(void)
{
	stm_display_device_h hDisplay = NULL;
	stm_display_plane_h hPlane = NULL;
	int Display, Plane, ret = 0;

	debug_mc("==== %s executing!\n", __func__);
	/* scan all existing displays...
	 * There should be only one! ... but let we ignore this limit
	 * since harmless for MC logic
	 */

	memset(&g_voutRoot, 0, sizeof(struct stm_v4l2_output_root));

	for (Display = 0;; Display++)
		/* while (true) */
	{
		ret = stm_display_open_device(Display, &hDisplay);
		if (ret) {
			if (Display == 0) {
				printk(KERN_ERR
				       "%s: SoC without Display !?!?\n",
				       __func__);
				ret = -EFAULT;
				break;
			} else {
				/* assume no more display, just exit from loop */
				ret = 0;
				break;
			}
		}

		/* Search for all planes of a Display, these will be part of MC graph.
		 * - Get plane capabilities, name and type.
		 * - Use name extracted from Display as MC entity and sub-device name.
		 * - A display without planes is accepted
		 * - Valid planes are kept opened
		 * - failure of "open plane" mean the Display has no more planes
		 * - failure of any other "plane operation" just skip the plane
		 */
		for (Plane = 0;; Plane++) {
			stm_plane_capabilities_t plane_caps;
			unsigned int type;
			const char *name = "";
			char *msg = "???";

			/* if the open fails, assumes no more planes! */
			if (stm_display_device_open_plane
			    (hDisplay, Plane, &hPlane))
				break;

			if (stm_display_plane_get_name(hPlane, &name)) {
				msg = "has no name";
				goto skip_plane;
			}

			if (stm_display_plane_get_capabilities
			    (hPlane, &plane_caps)) {
				msg = "has no capabilities";
				goto skip_plane;
			}

			if (plane_caps & PLANE_CAPS_VIDEO)
				type = STMEDIA_PLANE_TYPE_VIDEO;
			else if (plane_caps & PLANE_CAPS_GRAPHICS)
				type = STMEDIA_PLANE_TYPE_GRAPHICS;
			else if (plane_caps & PLANE_CAPS_VBI)
				type = STMEDIA_PLANE_TYPE_VBI;
			else {
				msg = "nor graphic nor video nor vbi type";
				goto skip_plane;
			}

			/* record the plane, this may fail:
			 *   - because of plane table overflow
			 *   - because a V4L registering primitive fails
			 * in any case skip the plane and try to continue!
			 */

			/* Search first if this plane has already been added, in
			 * principle this should never happen!
			 * The assumption is that different displays do not share
			 * planes, moreover plane names are always different
			 * Likely no longer useful with the current Display model
			 * while the old Display had the concept of shared planes
			 * among outputs.
			 */
			if ((ret = stmvout_plane_already_added(hDisplay, name))) {
				msg = "already added to MC";
				goto skip_plane;
			}

			if (stmvout_add_plane_subdev
			    (Plane, name, hPlane, type, hDisplay)) {
				msg = "V4L2 registration failed";
				goto skip_plane;
			}

			continue;

skip_plane:
			/* close "bad" planes */
			stm_display_plane_close(hPlane);
			printk(KERN_WARNING "plane (%d) %s skipped: %s! (%d)\n",
			       Plane, name, msg, ret);
		}

		printk(KERN_INFO "%s: Display %d  found %d planes\n",
		       __func__, Display, Plane);
	}

	stmvout_show_planes();	/* debug only */

	return (ret);
}

/**
 * stmvout_release_media_planes
 * This function closes the probed media planes. The probing
 * is managed by stmvout_probe_media_planes.
 */
static void stmvout_release_media_planes(void)
{
	int index, count;
	stm_display_device_h hDevice;
	stm_display_plane_h hPlane;

	/* Get the display device and close all the display planes associated with it */
	for (index = 0; index < STMEDIA_MAX_PLANES; index++) {
		if (!g_voutRoot.pDev[index].hDisplay)
			continue;

		/* Find all the planes of this particular display device */
		hDevice = g_voutRoot.pDev[index].hDisplay;
		for (count = index; count < STMEDIA_MAX_PLANES; count++) {
			if (g_voutRoot.pDev[count].hDisplay != hDevice)
				continue;

			if(g_voutRoot.pDev[count].hSource){
				stmvout_plane_disconnect_from_src(&(g_voutRoot.pDev[count]));
			}

			hPlane = g_voutRoot.pDev[count].hPlane;
			stmvout_remove_plane_subdev(count);
			stm_display_plane_close(hPlane);
		}
		stm_display_device_close(hDevice);
	}
}


/* =======================================================================
 * V4l2 output Video device (root  as /dev/video1)
 */

/*
 * It should exist a set of objects (one per /dev/video1 PAD) shared among
 * all the users of /dev/video1.  The object is feed when the user perform
 * VIDIO SET OUTPUT, at that point it know the PAD to work with.
 * It has also to keep the count of all users sharing that PAD
 */
static int stm_v4l2_video_output_open(struct file *file)
{
	struct video_device *viddev = &g_voutRoot.video1;
	struct stm_v4l2_vout_fh *vout_fh;

	/* Allocate memory */
	vout_fh = kzalloc(sizeof(struct stm_v4l2_vout_fh), GFP_KERNEL);
	if (NULL == vout_fh) {
		printk("%s: nomem on v4l2 open\n", __FUNCTION__);
		return -ENOMEM;
	}
	v4l2_fh_init(&vout_fh->fh, viddev);
	file->private_data = &vout_fh->fh;
	vout_fh->pOpenData = NULL;
	v4l2_fh_add(&vout_fh->fh);
	debug_mc("%s (file %p)\n", __func__, file);
	return 0;
}

/*
 * These are V4L2 vout ioctl ops
 */
struct v4l2_ioctl_ops stm_v4l2_video_output_ioctl_ops = {
	.vidioc_querycap = stmvout_vidioc_querycap,
	.vidioc_enum_fmt_vid_out = stmvout_vidioc_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out = stmvout_vidioc_g_fmt_vid_out,
	.vidioc_s_fmt_vid_out = stmvout_vidioc_s_fmt_vid_out,
	.vidioc_try_fmt_vid_out = stmvout_vidioc_try_fmt_vid_out,
	.vidioc_cropcap = stmvout_vidioc_cropcap,
	.vidioc_g_crop = stmvout_vidioc_g_crop,
	.vidioc_s_crop = stmvout_vidioc_s_crop,
	.vidioc_g_parm = stmvout_vidioc_g_parm,
	.vidioc_reqbufs = stmvout_vidioc_reqbufs,
	.vidioc_querybuf = stmvout_vidioc_querybuf,
	.vidioc_qbuf = stmvout_vidioc_qbuf,
	.vidioc_dqbuf = stmvout_vidioc_dqbuf,
	.vidioc_streamon = stmvout_vidioc_streamon,
	.vidioc_streamoff = stmvout_vidioc_streamoff,
	.vidioc_g_output = stmvout_vidioc_g_output,
	.vidioc_s_output = stmvout_vidioc_s_output,
	.vidioc_enum_output = stmvout_vidioc_enum_output,
	.vidioc_default = stmvout_vidioc_private_ioctl,
};

static struct v4l2_file_operations stm_v4l2_video_output_fops = {
	.owner = THIS_MODULE,
	.read = NULL,
	.write = NULL,
	.poll = stmvout_poll,
	.ioctl = NULL,
	.unlocked_ioctl = video_ioctl2,
	.mmap = stmvout_mmap,	/* stm_v4l2_video_output_mmap, */
	.open = stm_v4l2_video_output_open,
	.release = stmvout_close,
};

static int stm_v4l2_video_output_media_link_setup(struct media_entity *entity,
						  const struct media_pad *local,
						  const struct media_pad
						  *remote, u32 flags)
{
	debug_link("%s: ===> linking entity %s --> %s\n", __func__,
		   entity->name, remote->entity->name);
	return 0;
}

static const struct media_entity_operations stm_v4l2_video_output_entity_ops = {
	.link_setup = stm_v4l2_video_output_media_link_setup,
};

static void stm_v4l2_video_output_release(struct video_device *outvid)
{
	/* Nothing to do, but need by V4L2 stack */
}

static int stmvout_create_video_output_device(void)
{
	int i, ret = 0;
	struct media_pad *pads = NULL;

	for (i = 0; i < STMEDIA_MAX_PLANES; i++)
		INIT_LIST_HEAD(&g_voutRoot.pDev[i].openData.open_list);

	/* Initialize output video device (/dev/video1) */
	strncpy(g_voutRoot.video1.name, "STM Video Output",
		sizeof(g_voutRoot.video1.name));
	g_voutRoot.video1.fops = &stm_v4l2_video_output_fops;
	g_voutRoot.video1.minor = -1;
	g_voutRoot.video1.release = stm_v4l2_video_output_release;
	g_voutRoot.video1.entity.ops = &stm_v4l2_video_output_entity_ops;
	g_voutRoot.video1.ioctl_ops = &stm_v4l2_video_output_ioctl_ops;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
	g_voutRoot.video1.vfl_dir = VFL_DIR_TX;
#endif

	/* for each plane in g_voutData (which has a SINK pad) creates a
	 * a corresponding SOURCE pad of video output device
	 */
	pads = g_voutRoot.pads;
	for (i = 0; i < STMEDIA_MAX_PLANES; i++) {
		/* consider a null name as end of plane table */
		if (g_voutRoot.pDev[i].name[0] == '\0')
			break;
		pads[i].flags = MEDIA_PAD_FL_SOURCE;
	}

	/* initialize the /dev/video1 entity
	 * "i" is the number of PADs
	 */
	ret =
	    media_entity_init(&g_voutRoot.video1.entity, i, g_voutRoot.pads, 0);
	if (ret < 0) {
		printk(KERN_ERR
		       "%s: failed to initialize the video output driver (%d)\n",
		       __func__, ret);
		goto end;
	}
	/* In our device model, Video output is /dev/video1 */
	ret = stm_media_register_v4l2_video_device(&g_voutRoot.video1,
						   VFL_TYPE_GRABBER, 1);
	if (ret < 0)
		printk(KERN_ERR
		       "%s: failed to register the video output driver (%d)\n",
		       __func__, ret);

end:
	return (ret);
}

/**
 * stmvout_remove_video_output_device
 * This unregisters the ST V4L2 video device.
 */

static void stmvout_remove_video_output_device(void)
{
	stm_media_unregister_v4l2_video_device(&g_voutRoot.video1);

	media_entity_cleanup(&g_voutRoot.video1.entity);

	memset(&g_voutRoot.video1, 0, sizeof(g_voutRoot.video1));
}

/* =======================================================================
 * V4l2 output Creation of Media Controller Links:
 *    - from planes to output plugs
 *    - from video device (/dev/video1) to Planes
 */
static void stmvout_link_plane_to_all_outputs(stm_display_plane_h hPlane,
					      struct media_entity *ePlane,
					      char *name)
{

	int output, ret, display_disconnect;
	struct media_entity *eOutput = NULL;
	stm_display_output_h hOutput;

	for (output = 0; output < STMEDIA_MAX_OUTPUTS; output++) {
		display_disconnect = true;

		/* assume end of Output set if output handle is NULL */
		hOutput = g_voutPlug[output].hOutput;
		if (!hOutput)
			break;

		/* try to connect the plane with the output, if Display
		 * allows it, create the disabled MC link
		 */
		ret = stm_display_plane_connect_to_output(hPlane, hOutput);
		switch (ret) {
		case (-EALREADY):	/* assume this is GDP0 -> FB, link as well */
		case (-EBUSY):	/* assume this is a Shared FB plane, link as well */
			display_disconnect = false;
		case 0:
			break;
		default:
			continue;	/* assume this link is not allowed, continue */
		}
		/* get the MC output entity */
		eOutput = &g_voutPlug[output].sdev.entity;

		/* create the media link */
		ret = media_entity_create_link(ePlane, SOURCE_PAD_INDEX,
					       eOutput, SINK_PAD_INDEX, 0);
		if (ret < 0) {
			/* Most likely that's an internal error */
			printk(KERN_ERR "%s: failed to create link (%d)\n",
			       __func__, ret);
			BUG();
		}

		/* disconnect the plane, restore Display state */
		if (display_disconnect)
			ret =
			    stm_display_plane_disconnect_from_output(hPlane,
								     hOutput);
	}
}

static int stmvout_create_video_output_links(void)
{
	int plane, ret = 0;
	unsigned int flags = 0;

	/* Step1: link planes SRC PAD to output plugs SINK PAD:
	 *     Let the Display HW layer to drive this, attempt to connect
	 *     each plane with each output, if the Display allows it than
	 *     creates a DISABLED Media Controller link.
	 */
	for (plane = 0; plane < STMEDIA_MAX_PLANES; plane++) {
		stm_display_plane_h hPlane;
		struct media_entity *ePlane = NULL;
		if ((hPlane = g_voutRoot.pDev[plane].hPlane)) {
			ePlane = &g_voutRoot.pDev[plane].stmedia_sdev.sdev.entity;
			stmvout_link_plane_to_all_outputs(hPlane, ePlane,
							  g_voutRoot.
							  pDev[plane].name);
		}
	}

	/* Step2: links all planes SINK PADs  with output video device
	 *        (/dev/video1) SOURCE PADs
	 */
	for (plane = 0; plane < STMEDIA_MAX_PLANES; plane++) {
		struct media_entity *src, *ePlane = NULL;
		src = &g_voutRoot.video1.entity;	/* /dev/video1 */

		/* if the plane exists ... */
		if (g_voutRoot.pDev[plane].hPlane) {
			ePlane = &g_voutRoot.pDev[plane].stmedia_sdev.sdev.entity;
			ret =
			    media_entity_create_link(src, plane, ePlane, 0, flags);
			if (ret < 0) {
				/* Most likely that's an internal error */
				printk(KERN_ERR
				       "%s: failed to create link (%d)\n",
				       __func__, ret);
				BUG();
			}
		}
	}

	return (ret);
}

static int stmvout_create_compo_link(void)
{
	int output, ret = 0;
	struct media_entity *eOutput = NULL;
	struct media_entity *sink;

	/* Search for the Main compo */
	for (output = 0; output < STMEDIA_MAX_OUTPUTS; output++) {
		if (!g_voutPlug[output].hDisplay)
			continue;

		/* FIXME - shouldn't rely on string compare */
		if (!strcmp(g_voutPlug[output].name, "analog_hdout0")){
			eOutput = &g_voutPlug[output].sdev.entity;
			break;
		}
	}

	if (!eOutput) {
		/* That shouldn't happen, we always have a main compo */
		printk(KERN_ERR "Couldn't find the main compo\n");
		return -1;
	}

	/* Create a link between the Main compo and all GDP planes */
	sink = stm_media_find_entity_with_type_first( MEDIA_ENT_T_V4L2_SUBDEV_PLANE_GRAPHIC);
	while (sink) {
		ret = media_entity_create_link(eOutput, 1, sink, 0, 0);
		if (ret < 0) {
			printk(KERN_ERR "failed output->plane link\n");
			/* FIXME ... */
			return -1;
		}

		sink = stm_media_find_entity_with_type_next( sink, MEDIA_ENT_T_V4L2_SUBDEV_PLANE_GRAPHIC);
	}

	/* Create a link between the Main compo and all VID planes */
	sink = stm_media_find_entity_with_type_first( MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO);
	while (sink) {
		ret = media_entity_create_link(eOutput, 1, sink, 0, 0);
		if (ret < 0) {
			printk(KERN_ERR "failed output->plane link\n");
			/* FIXME ... */
			return -1;
		}

		sink = stm_media_find_entity_with_type_next( sink, MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO);
	}

	return (ret);
}

/**
 * stmvout_remove_compo_capture_links
 * This function check the compo capture link and disable it if needed
 */

static void stmvout_remove_compo_capture_links(void)
{
	int count;
	struct media_entity * entity = NULL;
	struct media_pad *local, *remote;
	unsigned int id = 0;

	for (count = 0; count < STMEDIA_MAX_OUTPUTS; count++) {
		if (g_voutPlug[count].hOutput == NULL)
			continue;

		if (!strcmp(g_voutPlug[count].name, "analog_hdout0")){
			entity = &g_voutPlug[count].sdev.entity;
			local = &entity->pads[SOURCE_PAD_INDEX];
			break;
		}
	}

	if (!entity)
		/* That shouldn't happen - nothing to do */
		return;

	remote = stm_media_find_remote_pad(local, MEDIA_LNK_FL_ENABLED, &id);
	if (!remote)
		/* Not connected - nothing to do */
		return;

	/* Fire the media controller link detach */
	media_entity_call(entity, link_setup,
				remote, local, 0);
}

/**
 * stmvout_remove_video_output_links
 * This function removes
 */

static void stmvout_remove_video_output_links(void)
{
	int index, count;
	stm_display_output_h hOutput;
	stm_display_plane_h hPlane;

	for (index = 0; index < STMEDIA_MAX_PLANES; index ++) {
		hPlane = g_voutRoot.pDev[index].hPlane;
		for (count = 0; count < STMEDIA_MAX_OUTPUTS; count++) {
			hOutput = g_voutPlug[count].hOutput;
			if ((hOutput == NULL) || (hPlane == NULL))
				continue;

			stm_display_plane_disconnect_from_output(hPlane, hOutput);
		}
	}
}

stm_display_plane_h stmvout_get_plane_hdl_from_entity(struct media_entity *
						      entity)
{
	return plane_entity_to_stvout_device(entity)->hPlane;
}

EXPORT_SYMBOL(stmvout_get_plane_hdl_from_entity);

stm_display_output_h stmvout_get_output_hdl_from_connected_plane_entity(struct
									media_entity
									*
									entity)
{
	struct media_pad *downStream_pad = NULL;
	struct output_plug *outputPlug;

	downStream_pad =
	    media_entity_remote_source(&entity->pads[SOURCE_PAD_INDEX]);
	if (downStream_pad == NULL)
		return NULL;

	outputPlug = output_entity_to_output_plug(downStream_pad->entity);

	return (outputPlug->hOutput);
}

EXPORT_SYMBOL(stmvout_get_output_hdl_from_connected_plane_entity);

static void __exit stmvout_exit_mc(void)
{
	stmvout_remove_compo_capture_links();

	stmvout_remove_video_output_links();

	stmvout_remove_video_output_device();

	stmvout_release_media_outputs();

	stmvout_release_media_planes();
}

static int __init stmvout_init_mc(void)
{
	int ret;

	if ((ret = stmvout_probe_media_planes()))
		return (ret);
	if ((ret = stmvout_probe_media_outputs()))
		return (ret);
	/* create the V4L2 output gate: /dev/video1 */
	if ((ret = stmvout_create_video_output_device()))
		return (ret);

	/* Now create all possible MC links, but none is ENABLED!
	 * Mean at the end of this initialization any output operation
	 * performed via /dev/video1 will succeed!
	 * The creation of "default output graph" (or customized graph)
	 * is thought to be in the scope of user application setup, most
	 * likely immediatilly after the module loading.
	 * The "output graph setup" uses the /dev/media device.
	 */

	ret = stmvout_create_video_output_links();
	if (ret)
		return ret;

	ret = stmvout_create_compo_link();
	if (ret)
		return ret;

	return (ret);
}

module_init(stmvout_init_mc);
module_exit(stmvout_exit_mc);

MODULE_LICENSE("GPL");
