/************************************************************************
Copyright (C) 2007, 2009, 2010 STMicroelectronics. All Rights Reserved.

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
 * Implementation of linux dvb v4l2 control device
************************************************************************/

/******************************
 * INCLUDES
 ******************************/
#include <asm/semaphore.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/pgtable.h>

#include <linux/module.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include <linux/dvb/dmx.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/bpa2.h>
#include <linux/delay.h>
#include <linux/freezer.h>

#include "stm_v4l2.h"
#include "stmvout.h"

#include <include/stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>
#include <stmdisplayplane.h>
#include <stmdisplayblitter.h>

#include "dvb_module.h"
#include "backend.h"

#include "dvb_v4l2.h"
#include "dvb_v4l2_export.h"

#define DVB_V4L2_MAX_BUFFERS 10

extern struct DvbContext_s *DvbContext;

/******************************
 * GLOBAL VARIABLES
 ******************************/

// Input surfaces to user

struct ldvb_v4l2_capture {
	struct task_struct                      *thread;
	volatile unsigned long                   physical_address;
	unsigned long                            size;
	unsigned long                            stride;
	unsigned long                            buffer_format;
	volatile unsigned long                   flags;
	int                                      width;
	int                                      height;
	volatile int                             complete;
};

struct ldvb_v4l2_description {
	char                       name[32];
	//int audioId (when implemented the id associated with audio description)
	int                        deviceId;
	int                        virtualId;
	int                        inuse;
	struct linuxdvb_v4l2      *priv;
};

static struct ldvb_v4l2_description g_ldvb_v4l2_device[] = {
	{"LINUXDVB_0",0,0,0,NULL},
	{"LINUXDVB_1",0,0,0,NULL},
	{"LINUXDVB_2",0,0,0,NULL}
};

struct linuxdvb_v4l2 {
	struct v4l2_input               input;
	struct v4l2_crop                crop;
	struct v4l2_buffer              buffer[DVB_V4L2_MAX_BUFFERS];
	void                            *address[DVB_V4L2_MAX_BUFFERS];
	struct ldvb_v4l2_capture        *capture;
	unsigned int                    blank;
};

struct bpa2_part     *partition = NULL;

typedef struct {
  struct v4l2_fmtdesc fmt;
  int    depth;
} format_info;

static format_info stmfb_v4l2_mapping_info [] =
{
  /*
   * This isn't strictly correct as the V4L RGB565 format has red
   * starting at the least significant bit. Unfortunately there is no V4L2 16bit
   * format with blue starting at the LSB. It is recognised in the V4L2 API
   * documentation that this is strange and that drivers may well lie for
   * pragmatic reasons.
   */
  [SURF_RGB565]     = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "RGB-16 (5-6-5)", V4L2_PIX_FMT_RGB565 },    16},

  [SURF_ARGB1555]     = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "RGBA-16 (5-5-5-1)", V4L2_PIX_FMT_BGRA5551 }, 16},

  [SURF_ARGB4444]     = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "RGBA-16 (4-4-4-4)", V4L2_PIX_FMT_BGRA4444 }, 16},

  [SURF_RGB888]     = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "RGB-24 (B-G-R)", V4L2_PIX_FMT_BGR24 },     24},

  [SURF_ARGB8888]   = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "ARGB-32 (8-8-8-8)", V4L2_PIX_FMT_BGR32 },  32}, /* Note that V4L2 doesn't define
									   * the alpha channel
									   */

  [SURF_BGRA8888]   = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "BGRA-32 (8-8-8-8)", V4L2_PIX_FMT_RGB32 },  32}, /* Bigendian BGR as BTTV driver,
									   * not as V4L2 spec
									   */

  [SURF_YCBCR422R]  = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "YUV 4:2:2 (U-Y-V-Y)", V4L2_PIX_FMT_UYVY }, 16},

  [SURF_YUYV]       = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "YUV 4:2:2 (Y-U-Y-V)", V4L2_PIX_FMT_YUYV }, 16},

  [SURF_YCBCR422MB] = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "YUV 4:2:2MB", V4L2_PIX_FMT_STM422MB },     8}, /* Bits per pixel for Luma only */

  [SURF_YCBCR420MB] = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "YUV 4:2:0MB", V4L2_PIX_FMT_STM420MB },     8}, /* Bits per pixel for Luma only */

  [SURF_YUV420]     = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "YUV 4:2:0 (YUV)", V4L2_PIX_FMT_YUV420 },   8}, /* Bits per pixel for Luma only */

  [SURF_YVU420]     = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "YUV 4:2:0 (YVU)", V4L2_PIX_FMT_YVU420 },   8}, /* Bits per pixel for Luma only */

  [SURF_YUV422P]    = {{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			 "YUV 4:2:2 (YUV)", V4L2_PIX_FMT_YUV422P },  8}, /* Bits per pixel for Luma only */

  /* Make sure the array covers all the SURF_FMT enumerations  */
  [SURF_END]         = {{ 0, 0, 0, "", 0 }, 0 }
};


unsigned long GetPhysicalContiguous(unsigned long ptr,size_t size)
{
	struct mm_struct *mm = current->mm;
	//struct vma_area_struct *vma = find_vma(mm, ptr);
	unsigned virt_base =  (ptr / PAGE_SIZE) * PAGE_SIZE;
	unsigned phys_base = 0;

	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *ptep, pte;

	spin_lock(&mm->page_table_lock);

	pgd = pgd_offset(mm, virt_base);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		goto out;

	pmd = pmd_offset((pud_t*)pgd, virt_base);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		goto out;

	ptep = pte_offset_map(pmd, virt_base);

	if (!ptep)
		goto out;

	pte = *ptep;

	if (pte_present(pte)) {
		phys_base = __pa(page_address(pte_page(pte)));
	}

	if (!phys_base)
		goto out;

	spin_unlock(&mm->page_table_lock);
	return phys_base + (ptr - virt_base);

out:
	spin_unlock(&mm->page_table_lock);
	return 0;
}

void* stm_v4l2_findbuffer(unsigned long userptr, unsigned int size,int device)
{
	int i;
	unsigned long result;
	unsigned long virtual;

	if (!g_ldvb_v4l2_device[device].inuse)
		return 0;

	result = GetPhysicalContiguous(userptr,size);

	virtual = (unsigned long)phys_to_virt(result);

	if (result) {
		struct linuxdvb_v4l2 *ldvb = g_ldvb_v4l2_device[device].priv;

		for (i=0;i<DVB_V4L2_MAX_BUFFERS;i++)
		{
			if (ldvb->buffer[i].length &&
			    (result >= ldvb->buffer[i].m.offset) &&
			    ((result + size) < (ldvb->buffer[i].m.offset + ldvb->buffer[i].length)))
				return (void*)virtual;
		}
	}

	return 0;
}

volatile stm_display_buffer_t *ManifestorLastDisplayedBuffer;
EXPORT_SYMBOL(ManifestorLastDisplayedBuffer);
DECLARE_WAIT_QUEUE_HEAD(g_ManifestorLastWaitQueue);
EXPORT_SYMBOL(g_ManifestorLastWaitQueue);
static DECLARE_WAIT_QUEUE_HEAD(buffer_blitted);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
#define wait_event_freezable_timeout(wq, condition, timeout)      \
({                                                                \
	long __retval = timeout;                                  \
	do {                                                      \
		__retval = wait_event_interruptible_timeout(wq,   \
				(condition) || freezing(current), \
				__retval);                        \
	} while (try_to_freeze());                                \
	__retval;                                                 \
})
#endif

static int linuxdvb_v4l2_capture_thread (void *data)
{
	struct linuxdvb_v4l2 * const ldvb = data;
	stm_display_buffer_t       buffer;
	struct stmcore_display_pipeline_data     pipeline_data;
	stm_blitter_operation_t op;
	stm_rect_t              dstrect;
	stm_rect_t              srcrect;
	stm_display_buffer_t *last = NULL;

	set_freezable ();

	memset(&op,0,sizeof(op));

	dstrect.top  = 0;
	dstrect.left = 0;

	// Get a handle to the blitter
	stmcore_get_display_pipeline (0, &pipeline_data);

	while (1) {
		int ret;
		stm_display_buffer_t *ptr;

		ret = wait_event_freezable_timeout (g_ManifestorLastWaitQueue,
						    /* condition is:
						       a new buffer is
						       available... */
						    (last != ManifestorLastDisplayedBuffer
						     /* ...and the previous
						        buffer has been
						        dequeued by
						        userspace... */
						     && ldvb->capture->complete == 0
						     /* ...and we know where
						        to capture to... */
						     && ldvb->capture->physical_address)
						    /* ...or we should
						       terminate */
						    || kthread_should_stop(),
						    HZ);

		if (kthread_should_stop ())
			break;

		if (ret <= 0)
			/* timeout expired (0) or -ERESTARTSYS returned */
			continue;

		ptr = ManifestorLastDisplayedBuffer;

		last = ptr;

		if (!ptr)
			continue;

		{
			memcpy(&buffer,ptr,sizeof(stm_display_buffer_t));

#if 0
			printk("%s:%d  %d %d %d %d\n",__FUNCTION__,__LINE__,
			       ldvb->capture->width,
			       ldvb->capture->height,
			       ldvb->capture->stride,
			       ldvb->capture->size);
#endif

			dstrect.right  = ldvb->capture->width;
			dstrect.bottom = ldvb->capture->height;

			/* Convert the source origin into something useful to
			   the blitter. The incoming coordinates can be in
			   either whole integer or multiples of a 16th or a
			   32nd of a pixel/scanline. */
			switch (buffer.src.ulFlags & (STM_PLANE_SRC_XY_IN_32NDS
						      | STM_PLANE_SRC_XY_IN_16THS))
			{
			case STM_PLANE_SRC_XY_IN_32NDS:
				srcrect.left = (buffer.src.Rect.x << (1 * 10)) / 32;
				srcrect.top  = (buffer.src.Rect.y << (1 * 10)) / 32;
				break;
			case STM_PLANE_SRC_XY_IN_16THS:
				srcrect.left = (buffer.src.Rect.x << (1 * 10)) / 16;
				srcrect.top  = (buffer.src.Rect.y << (1 * 10)) / 16;
				break;
			case 0:
				srcrect.left = buffer.src.Rect.x << (1 * 10);
				srcrect.top  = buffer.src.Rect.y << (1 * 10);
				break;
			default:
				printk("%s:%d Error during blitter operation\n",__FUNCTION__,__LINE__);
				continue;
			}

			srcrect.right  = srcrect.left + buffer.src.Rect.width;
			srcrect.bottom = srcrect.top + buffer.src.Rect.height;

			op.ulFlags = ((buffer.src.ulFlags & STM_PLANE_SRC_COLORSPACE_709)
				      ? STM_BLITTER_FLAGS_SRC_COLOURSPACE_709
				      : 0);
			op.ulFlags |= ldvb->capture->flags;
			op.ulFlags |= STM_BLITTER_FLAGS_SRC_XY_IN_FIXED_POINT;
			op.srcSurface.ulMemory       = buffer.src.ulVideoBufferAddr;
			op.srcSurface.ulSize         = buffer.src.ulVideoBufferSize;
			op.srcSurface.ulWidth        = buffer.src.ulStride / (buffer.src.ulPixelDepth / 8);
			op.srcSurface.ulHeight       = buffer.src.ulTotalLines;
			op.srcSurface.ulStride       = buffer.src.ulStride;
			op.srcSurface.format         = buffer.src.ulColorFmt;
			op.srcSurface.ulChromaOffset = buffer.src.chromaBufferOffset;

			op.dstSurface.ulMemory = ldvb->capture->physical_address;
			op.dstSurface.ulSize   = ldvb->capture->size;
			op.dstSurface.ulWidth  = ldvb->capture->width;
			op.dstSurface.ulHeight = ldvb->capture->height;
			op.dstSurface.ulStride = ldvb->capture->stride;
			op.dstSurface.format   = ldvb->capture->buffer_format;

			if (stm_display_blitter_blit(pipeline_data.blitter_kernel, &op, &srcrect, &dstrect))
				printk("%s:%d Error during blitter operation\n",__FUNCTION__,__LINE__);

			ldvb->capture->complete = 1;
			wake_up_interruptible (&buffer_blitted);
		}
	}

	ldvb->capture->complete = -1;
	wake_up_all (&buffer_blitted);

	return 0;
}

int linuxdvb_ioctl(struct stm_v4l2_handles *handle,struct stm_v4l2_driver *driver, int device, enum _stm_v4l2_driver_type type, struct file *file, unsigned int cmd, void *arg)
{
	struct linuxdvb_v4l2 *ldvb = handle->v4l2type[type].handle;

	// Need a mutex here

	switch(cmd)
	{
	case VIDIOC_ENUMINPUT:
	{
		struct v4l2_input* input = arg;
		int index = input->index - driver->index_offset[device];

		// check consistency of index
		if (index < 0 || index >= ARRAY_SIZE (g_ldvb_v4l2_device))
			goto err_inval;

		strcpy(input->name, g_ldvb_v4l2_device[index].name);

		break;
	}

	case VIDIOC_S_INPUT:
	{
		int id = (*(int *) arg) - driver->index_offset[device];

		// Note: an invalid value is not ERROR unless the Registration Driver Interface tells so. 
		if (id < 0 || id >= ARRAY_SIZE (g_ldvb_v4l2_device))
			goto err_inval;

		// check if resource already in use
		if (g_ldvb_v4l2_device[id].inuse)
		{
			DVB_ERROR("Device already in use \n");
			goto err_inval;
		}

		// allocate handle for driver registration
		handle->v4l2type[type].handle = kmalloc(sizeof(struct linuxdvb_v4l2),GFP_KERNEL);
		if (!handle->v4l2type[type].handle)
		{
			DVB_ERROR("kmalloc failed\n");
			return -ENODEV;
		}

		ldvb = handle->v4l2type[type].handle;
		memset(ldvb,0,sizeof(struct linuxdvb_v4l2));

		ldvb->input.index = id;

		g_ldvb_v4l2_device[id].inuse = 1;
		g_ldvb_v4l2_device[id].priv = ldvb;

		break;
	}

	case VIDIOC_G_INPUT:
	{
		if (ldvb == NULL) {
			DVB_ERROR("driver handle NULL. Need to call VIDIOC_S_INPUT first. \n");
			return -ENODEV;
		}

		*((int *) arg) = (ldvb->input.index
				  + driver->index_offset[device]);

		break;
	}

	case VIDIOC_S_CROP:
	{
		struct v4l2_crop *crop = (struct v4l2_crop*)arg;

		if (ldvb == NULL) {
			DVB_ERROR("driver handle NULL. Need to call VIDIOC_S_INPUT first. \n");
			return -ENODEV;
		}

		if (!crop->type) {
			DVB_ERROR("crop->type = %d\n", crop->type);
			return -EINVAL;
		}

		// get crop values
		ldvb->crop.type = crop->type;
		ldvb->crop.c = crop->c;


		if (crop->type == V4L2_BUF_TYPE_VIDEO_OVERLAY)
			VideoSetOutputWindow (&DvbContext->DeviceContext[ldvb->input.index],
					      crop->c.left, crop->c.top, crop->c.width, crop->c.height);
		else if (crop->type == V4L2_BUF_TYPE_PRIVATE+1)
			VideoSetInputWindow (&DvbContext->DeviceContext[ldvb->input.index],
					     crop->c.left, crop->c.top, crop->c.width, crop->c.height);

		break;
	}

	case VIDIOC_CROPCAP:
	{
		struct v4l2_cropcap *cropcap = (struct v4l2_cropcap*)arg;
		video_size_t         video_size;

		if (ldvb == NULL) {
			printk("%s Error: driver handle NULL. Need to call VIDIOC_S_INPUT first. \n", __FUNCTION__);
			return -ENODEV;
		}

		//if (cropcap->type == V4L2_BUF_TYPE_PRIVATE+1) {
			VideoIoctlGetSize (&DvbContext->DeviceContext[ldvb->input.index], &video_size);
			cropcap->bounds.left                    = 0;
			cropcap->bounds.top                     = 0;
			cropcap->bounds.width                   = video_size.w;
			cropcap->bounds.height                  = video_size.h;

			VideoGetPixelAspectRatio (&DvbContext->DeviceContext[ldvb->input.index],
						  &cropcap->pixelaspect.numerator, &cropcap->pixelaspect.denominator);

			//printk("%s VIDIOC_CROPCAP, type = %d\n", __FUNCTION__, cropcap->type);
		//}
		break;
	}

	case VIDIOC_QUERYBUF:
	{
		struct v4l2_buffer *buf = arg;

		if (ldvb == NULL) {
			DVB_ERROR("driver handle NULL. Need to call VIDIOC_S_INPUT first. \n");
			return -ENODEV;
		}

		if (buf->index > DVB_V4L2_MAX_BUFFERS || buf->index < 0)
			goto err_inval;

		if (ldvb->buffer[buf->index].length == 0)
			goto err_inval;

		*buf = ldvb->buffer[buf->index];

		break;
	}

	case VIDIOC_STREAMON:
	{
		static const char task_name[] = "kv4l2_dvp capture";

		if (!ldvb)
			goto err_inval;

		if (!ldvb->capture)
			goto err_inval;

		if (!ldvb->capture->buffer_format)
			goto err_inval;

		if (ldvb->capture->thread)
			goto err_inval;

		ldvb->capture->thread = kthread_run (linuxdvb_v4l2_capture_thread,
						     ldvb, "%s", task_name);

		break;
	}

	case VIDIOC_STREAMOFF:
	{
		if (!ldvb)
			goto err_inval;

		if (!ldvb->capture)
			goto err_inval;

		if (!ldvb->capture->thread)
			goto err_inval;

		kthread_stop (ldvb->capture->thread);
		ldvb->capture->thread = NULL;

		break;
	}

	case VIDIOC_S_FMT:
	{
		struct v4l2_format *fmt = arg;
		int n,surface = -1;;

		if (!ldvb)
			goto err_inval;

		if (!fmt)
			goto err_inval;

		if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			goto err_inval;

		if (!ldvb->capture) {
			ldvb->capture = kmalloc(sizeof(struct ldvb_v4l2_capture),GFP_KERNEL);
			memset(ldvb->capture,0,sizeof(struct ldvb_v4l2_capture));
		}

		for (n=0;n<sizeof(stmfb_v4l2_mapping_info)/sizeof(format_info);n++)
			if (stmfb_v4l2_mapping_info[n].fmt.pixelformat == fmt->fmt.pix.pixelformat)
				surface = n;

		if (surface == -1) {
			// Not sure if freeing this is correct..
			kfree(ldvb->capture);
			ldvb->capture = NULL;
			goto err_inval;
		}

		ldvb->capture->buffer_format = surface;
		ldvb->capture->width         = fmt->fmt.pix.width;
		ldvb->capture->height        = fmt->fmt.pix.height;
		if (!fmt->fmt.pix.bytesperline)
			fmt->fmt.pix.bytesperline = (stmfb_v4l2_mapping_info[surface].depth * fmt->fmt.pix.width) / 8;

		ldvb->capture->stride = fmt->fmt.pix.bytesperline;

		break;
	}

	case VIDIOC_QBUF:
	{
		struct v4l2_buffer *buf = arg;
		unsigned long addr              = 0;

		if (!ldvb)
			goto err_inval;
		if (!ldvb->capture)
			goto err_inval;
		if (!buf)
			goto err_inval;
		if (buf->memory != V4L2_MEMORY_USERPTR)
			goto err_inval;

		// Currently we support capture of only one buffer
		// at a time, anything else doesn't make sense.
		if (ldvb->capture->physical_address)
			return -EIO;

		addr = GetPhysicalContiguous(buf->m.userptr,buf->length);

		if (!addr) return -EIO;

		ldvb->capture->physical_address = addr;
		ldvb->capture->size             = buf->length;
		ldvb->capture->flags = ((buf->flags & V4L2_BUF_FLAG_FULLRANGE)
					? STM_BLITTER_FLAGS_DST_FULLRANGE
					: 0);

		break;
	}

	case VIDIOC_DQBUF:
	{
		//struct v4l2_buffer *buf = arg;

		if (!arg)
			goto err_inval;

		if (!ldvb)
			goto err_inval;

		if (!ldvb->capture)
			goto err_inval;

		if (!ldvb->capture->physical_address) // If there is no physical address
			if (!ldvb->capture->complete) // And we are not complete
				return -EIO;          // Return an IO error, please queue a buffer...

		if ((file->f_flags & O_NONBLOCK) == O_NONBLOCK)
			if (ldvb->capture->complete==0)
				return -EAGAIN;

		// Otherwise loop until the blit has been completed
		if (wait_event_interruptible (buffer_blitted,
					      ldvb->capture->complete != 0))
			return -ERESTARTSYS;

		if (ldvb->capture->complete != 1)
			/* capture thread aborted */
			return -EIO;

		ldvb->capture->physical_address = 0;           // Mark as done, so we can queue a new buffer
		ldvb->capture->complete         = 0;

		break;
	}

	// We use the buf type private to allow obtaining physically contiguous buffers.
	// We also need this to get capture buffers so we can do capture of mpeg2 stream etc.
	case VIDIOC_REQBUFS:          //_IOWR ('V',  8, struct v4l2_requestbuffers)
	{
		struct v4l2_requestbuffers *rb = arg;
		int n,m,count;
		void *data;
		unsigned int dataSize;

		if (ldvb == NULL) {
			DVB_ERROR("driver handle NULL. Need to call VIDIOC_S_INPUT first. \n");
			return -ENODEV;
		}

		switch (rb->type)
		{
			case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			{
				if (rb->memory != V4L2_MEMORY_USERPTR)
					goto err_inval;

				if (!ldvb->capture)
					return -EIO;

				break;
			}

			case V4L2_BUF_TYPE_PRIVATE:
			{
				/* Private only supports MMAP buffers */
				if (rb->memory != V4L2_MEMORY_MMAP)
					goto err_inval;

				/* Reserved field is used for size of buffer */
				if (rb->reserved[0] == 0)
					goto err_inval;

				/* Injitially we can give zero buffers */
				count = rb->count;
				rb->count = 0;

				/* Go through the count and see how many buffer we can give */
				for(m=0;m<count;m++)
				{
					/* See if we have any free */
					for (n=0;n<DVB_V4L2_MAX_BUFFERS;n++)
						if (ldvb->buffer[n].length == 0) break;

					/* If not return IO error */
					if (n == DVB_V4L2_MAX_BUFFERS) {
						if (m==0) return -EIO;
						else return 0;
					}

					partition = bpa2_find_part("v4l2-coded-video-buffers");

					/* fallback to big phys area */
					if (partition == NULL)
						partition = bpa2_find_part("bigphysarea");

					if (partition == NULL)
					{
						DVB_ERROR("Failed to find any suitable bpa2 partitions while trying to allocate memory\n");
						return -ENOMEM;
					}

					dataSize = (rb->reserved[0] + (PAGE_SIZE-1)) / PAGE_SIZE ;

					/* Let's see if we can allocate some memory */
					data = (void*)bpa2_alloc_pages(partition,dataSize,4,GFP_KERNEL);

					if (!data) {
						if (m==0) return -EIO;
						else return 0;
					}

					/* Now we know everything good fill the info in */
					ldvb->buffer[n].index     = n;
					ldvb->buffer[n].length    = rb->reserved[0];
					ldvb->buffer[n].m.offset  = (unsigned int)data;
					ldvb->buffer[n].type      = rb->type;
					ldvb->address[n]          = ioremap_cache((unsigned int)data,dataSize);

					rb->count = m+1;
				}

				break;
			}

			default:
				return -EINVAL;
		};

		break;
	}

	case VIDIOC_S_CTRL:
	{
	    ULONG  ctrlid               = 0;
	    ULONG  ctrlvalue            = 0;
	    struct v4l2_control* pctrl  = arg;
	    int ret                     = 0;

	    ctrlid      = pctrl->id;
	    ctrlvalue   = pctrl->value;

	    switch (ctrlid)
	    {
		case V4L2_CID_STM_BLANK:
		{
			if (ldvb == NULL) {
				DVB_ERROR("driver handle NULL. Need to call VIDIOC_S_INPUT first. \n");
				return -ENODEV;
			}

			ret = DvbStreamEnable (DvbContext->DeviceContext[ldvb->input.index].VideoStream, ctrlvalue);
			if( ret < 0 )
			{
			    DVB_ERROR( "StreamEnable failed (ctrlvalue = %lu)\n", ctrlvalue);
			    return -EINVAL;
			}

			ldvb->blank     = ctrlvalue;

			break;
		}

		/*
		    case V4L2_CID_STM_SRC_COLOUR_MODE:
		    case V4L2_CID_STM_FULL_RANGE :
		    case V4L2_CID_AUDIO_MUTE:
		    case V4L2_CID_STM_AUDIO_SAMPLE_RATE:
		    case V4L2_CID_STM_BACKGROUND_RED:
		    case V4L2_CID_STM_BACKGROUND_GREEN:
		    case V4L2_CID_STM_BACKGROUND_BLUE:
		 */

		default:
		{
			DVB_ERROR("Control Id not handled. \n");
			return -ENODEV;
		}
	    }

	    break;
	}

	case VIDIOC_G_CTRL:
	{
	    struct v4l2_control* pctrl = arg;

	    switch (pctrl->id)
	    {
		case V4L2_CID_STM_BLANK:
		    pctrl->value = ldvb->blank;

		default:
		{
			DVB_ERROR("Control Id not handled. \n");
			return -ENODEV;
		}
	    }

	    break;
	}


	default:
		return -ENODEV;
	}

	return 0;

err_inval:
	return -EINVAL;
}


static int linuxdvb_close(struct stm_v4l2_handles *handle, enum _stm_v4l2_driver_type type, struct file *file)
{
	int i;
	struct linuxdvb_v4l2 *ldvb = handle->v4l2type[type].handle;

	if (!ldvb) return 0;

	if (partition)
		for (i=0;i<DVB_V4L2_MAX_BUFFERS;i++)
			if (ldvb->buffer[i].length && ldvb->buffer[i].m.offset)
				bpa2_free_pages(partition,ldvb->buffer[i].m.offset);

	g_ldvb_v4l2_device[ldvb->input.index].inuse = 0;

	if (ldvb->capture) {
		// signal thread to stop
		if (ldvb->capture->thread) {
			kthread_stop (ldvb->capture->thread);
			ldvb->capture->thread = NULL;
		}

		kfree(ldvb->capture);
		ldvb->capture = NULL;
	}

	handle->v4l2type[type].handle = NULL;
	kfree(ldvb);

	return 0;
}

static struct page* linuxdvb_vm_nopage(struct vm_area_struct *vma, unsigned long vaddr, int *type)
{
	struct page *page;
	void *page_addr;
	unsigned long page_frame;

	if (vaddr > vma->vm_end)
		return NOPAGE_SIGBUS;

	/*
	 * Note that this assumes an identity mapping between the page offset and
	 * the pfn of the physical address to be mapped. This will get more complex
	 * when the 32bit SH4 address space becomes available.
	 */
	page_addr = (void*)((vaddr - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT));

	page_frame = ((unsigned long)page_addr >> PAGE_SHIFT);

	if(!pfn_valid(page_frame))
		return NOPAGE_SIGBUS;

	page = virt_to_page(__va(page_addr));

	get_page(page);

	if (type)
		*type = VM_FAULT_MINOR;
	return page;
}

static void linuxdvb_vm_open(struct vm_area_struct *vma)
{
	//DVB_DEBUG("/n");
}

static void linuxdvb_vm_close(struct vm_area_struct *vma)
{
	//DVB_DEBUG("/n");
}

static struct vm_operations_struct linuxdvb_vm_ops_memory =
{
	.open     = linuxdvb_vm_open,
	.close    = linuxdvb_vm_close,
	.nopage   = linuxdvb_vm_nopage,
};

static int linuxdvb_mmap(struct stm_v4l2_handles *handle, enum _stm_v4l2_driver_type type, struct file *file, struct vm_area_struct*  vma)
{
	struct linuxdvb_v4l2 *ldvb = handle->v4l2type[type].handle;
	int n;

	if (!(vma->vm_flags & VM_WRITE))
		return -EINVAL;

//  if (!(vma->vm_flags & VM_SHARED))
//    return -EINVAL;

	for (n=0;n<DVB_V4L2_MAX_BUFFERS;n++)
		if (ldvb->buffer[n].length && (ldvb->buffer[n].m.offset == (vma->vm_pgoff*PAGE_SIZE)))
			break;

	if (n==DVB_V4L2_MAX_BUFFERS)
		return -EINVAL;

	if (ldvb->buffer[n].length != (vma->vm_end - vma->vm_start))
		return -EINVAL;

	vma->vm_flags       |= /*VM_RESERVED | VM_IO |*/ VM_DONTEXPAND | VM_LOCKED;
//  vma->vm_page_prot    = pgprot_noncached(vma->vm_page_prot);
	vma->vm_private_data = ldvb;

	vma->vm_ops = &linuxdvb_vm_ops_memory;

	return 0;
}

struct stm_v4l2_driver linuxdvb_overlay = {
	.type         = STM_V4L2_VIDEO_INPUT,
	.ioctl        = linuxdvb_ioctl,
	.close        = linuxdvb_close,
	.poll         = NULL,
	.mmap         = linuxdvb_mmap,
	.name         = "Linux DVB Overlay",
};

void linuxdvb_v4l2_init(void)
{
	int i;

	for (i=0;i<STM_V4L2_MAX_DEVICES;i++) {
		linuxdvb_overlay.control_range[i].first = V4L2_CID_STM_DVBV4L2_FIRST;
		linuxdvb_overlay.control_range[i].last  = V4L2_CID_STM_DVBV4L2_LAST;
		linuxdvb_overlay.n_indexes[i] = ARRAY_SIZE (g_ldvb_v4l2_device);
	}

	stm_v4l2_register_driver(&linuxdvb_overlay);
}

