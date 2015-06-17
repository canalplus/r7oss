/************************************************************************
Copyright (C) 2007, 2009, 2010 STMicroelectronics. All Rights Reserved.

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
 * Implementation of linux dvb v4l2 control device
************************************************************************/

#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-bpa2-contig.h>
#include <media/videobuf2-vmalloc.h>
#ifdef CONFIG_BPA2
#include <linux/bpa2.h>
#else
#error Kernel must have the BPA2 memory allocator configured
#endif

#include <include/stm_display.h>
#include <stm_memsink.h>
#include <linux/stm/blitter.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/io-mapping.h>

#include "stmvout.h"
#include "dvb_module.h"
#include "dvb_video.h"
#include "dvb_audio.h"
#include "display.h"
#include "dvb_v4l2_export.h"
#include "stmedia.h"
#include "stmedia_export.h"
#include "stm_v4l2_common.h"
#include "stm_v4l2_audio.h"

#define ENOTSUP EOPNOTSUPP

#define VIDEO_MAX_BUFFER_SIZE		3133696	/* 1920 * 1088 * 1.5 + 256 Bytes for capture_buffer header and padding */
#define AUDIO_MAX_BUFFER_SIZE		65792	/* 4 bytes * 8 channels * 2048 samples + 256 Bytes for capture_buffer header and padding */
#define USER_DATA_MAX_BUFFER_SIZE	2048

#define BPA2_PARTITION "v4l2-grab"
#define ALIGN_UP(x,y)  ((((x) + (y-1))/(y))*(y))
#define CSPACE_LIMIT_WIDTH 720

/*
 * The below macros verifies if the input and format have been set
 */
#define VIDEO		0x1
#define USER		0x2
#define AUDIO		0x4

/*
 * Video and user-data both have type as video data
 */
#define STM_V4L2_VIDEO_CONTROL	(VIDEO | USER)
#define STM_V4L2_AUDIO_CONTROL	AUDIO

#define CHECK_INPUT(input, data)						\
	do {									\
		if (!input || !input->priv) {					\
			DVB_ERROR("Input not set, call VIDIOC_S_INPUT\n");	\
			return -EINVAL;						\
		}								\
		if (!(input->flags & data))					\
			DVB_ERROR("Invalid input type specified\n");		\
	} while(0)

/*
 * The below macros:
 * a. CHECK_VIDEO_FMT_LOCKED
 * b. CHECK_AUDIO_FMT_LOCKED
 * c. CHECK_USER_FMT_LOCKED
 * are locked versions for checking if the VIDIOC_S_FMT had been called.
 */
#define CHECK_VIDEO_FMT_LOCKED(video, mutex)					\
	do {									\
		if (mutex_lock_interruptible(mutex))				\
			return -ERESTARTSYS;					\
										\
		if (!video->capture) {						\
			DVB_ERROR("Video fmt not set, call VIDIOC_S_FMT\n");	\
			mutex_unlock(mutex);					\
			return -EINVAL;						\
		}								\
										\
		mutex_unlock(mutex);						\
	} while(0)

#define CHECK_AUDIO_FMT_LOCKED(audio, mutex)					\
	do {									\
		if (mutex_lock_interruptible(mutex))				\
			return -ERESTARTSYS;					\
										\
		if (!audio->capture) {						\
			DVB_ERROR("Audio fmt not set, call VIDIOC_S_FMT\n");	\
			mutex_unlock(mutex);					\
			return -EINVAL;						\
		}								\
										\
		mutex_unlock(mutex);						\
	} while(0)

#define CHECK_USER_FMT_LOCKED(video, mutex)						\
	do {										\
		if (mutex_lock_interruptible(mutex))					\
			return -ERESTARTSYS;						\
											\
		if (!video->ud) {							\
			DVB_ERROR("User-data fmt not set, call VIDIOC_S_FMT\n");	\
			mutex_unlock(mutex);						\
			return -EINVAL;							\
		}									\
											\
		mutex_unlock(mutex);							\
	} while(0)

#define DELETE_VBLIST(plist)						\
	do {										\
		if (!list_empty(plist)) {							\
			struct stm_v4l2_vb2_buffer *stm_vb1, *stm_vb2;		\
														\
			list_for_each_entry_safe(stm_vb1, stm_vb2,			\
						plist, list) {						\
				list_del(&stm_vb1->list);					\
				kfree(stm_vb1);								\
			}											\
		}											\
	} while(0)

static unsigned int video_decoder_offset = 0;
static unsigned int audio_decoder_offset = 0;
static unsigned int nb_dec_video = 0;
static unsigned int nb_dec_audio = 0;

/******************************
 * GLOBAL VARIABLES
 ******************************/

struct stm_v4l2_vb2_buffer {
	struct vb2_buffer *vb2_buf;
	struct list_head list;
};

/*
 * To support "capture in zero_copy mode"
 * two structures (referenced by capture record):
 *    pool:    an array to keep track of user's v4l2_buffers. It is
 *             populated by user QBUF operations, and sent to player
 *             as soon as it achieve the negotiated number.
 *             While streaming poll is used to associate player frames
 *             to the original user buffers (the association is based
 *             on physical and user address of buffers)
 */

#define STM_ZC_FRAME_VALID	0x01	   /* valid, pushed by decoder */
#define STM_ZC_FRAME_TOUSER	0x02	   /* sent to user via DQBUF   */
#define EOS_FRAME(f)	(((f)->u.uncompressed.discontinuity == STM_SE_DISCONTINUITY_EOS))

struct stm_v4l2_zc_pool {
        struct   list_head  list;	   /* FIFO queue for DQBUF     */
	unsigned long	    bus_addr;      /* physical buffer address  */
	unsigned long	    ker_addr;      /* kernel buffer address    */
	unsigned int	    buf_len;       /* bufer size               */
	unsigned int	    buf_index;     /* v4l2_bufer index         */
        unsigned int        flags;         /* STM_ZC_VALID_FRAME       */
	stm_object_h        src_object;    /* to track player object   */
};

#define MAX_BUFFERS     32              /* largely enough */

struct stm_v4l2_video_buf {
	unsigned long                              intermediate_buffer_Phys;
	void                                      *intermediate_buffer_Virt;
	void                                      *physical_address;
	unsigned long                              payload_length;
	unsigned long long                         presentation_time;
	stm_se_uncompressed_frame_metadata_video_t uncompressed_metadata;
};

struct stm_v4l2_video_capture {
	/*
	 * This mutex is to synchronize dqbuf/streamoff with buffer push from decoder
	 */
	struct mutex mutex;

	int mapping_index;
	unsigned int flags;

	/* to support "capture in zero_copy mode" */
	struct zero_copy_s {
		unsigned int    buf_required;	 /* required by SE (min)    */
		unsigned int    buf_provided;	 /* provided by user        */
		unsigned int	buf_size;	 /* all bufs with same size */
		unsigned int    pool_idx;        /* index in buf_pool       */
		int             queue_ts;        /* queue threshold (max)   */

		/* describes all the frame buffers sent to player.
		 * slots are also used as linked list of nodes to gather
		 * frames coming from decoder (avoid heavy dynamic alloc/free)
		 */
	        struct    stm_v4l2_zc_pool   pool[MAX_BUFFERS];

		/* FIFO queue to gather frames waiting for DQBUF  */
	        struct    list_head          wdqbuf_frames;
	        wait_queue_head_t            wdqbuf_thread;

		/* result of connection: play_stream <-> STLinuxTV adt. */
	        struct stm_v4l2_input_description   *zc_instance; /* this sink  */
	        stm_se_play_stream_h		 source;      /* SE source  */

		/* Fake frame node (as pool) dedicate to queue the EOS (End Of
		 * Stream) frame. Unfortunately Player2 doesn't push an "empty
		 * frame", it pushes meta data with NO frame associate, so it
		 * must be managed offline
		 */
		struct    stm_v4l2_zc_pool   eos_frame;

		/*
		 * This is a pointer to the head of the stm_vb2 buffer list.
		 * This is required as push from decoder does not have input
		 * context and so access to stm_vb2 buffers is not possible
		 * without this
		 */
		struct stm_v4l2_vb2_buffer *stm_vb2;

	} zero;	/* unappy name! but just to distinguish... */

	stm_blitter_t *blitter;
	stm_memsink_h memsink_interface;

	struct {
		stm_blitter_surface_t *surface;
		stm_blitter_rect_t rect;
	} source;

	struct {
		stm_blitter_surface_format_t format;
		stm_blitter_surface_colorspace_t cspace;
		stm_blitter_dimension_t buffer_dimension;
		stm_blitter_surface_address_t buffer;
		unsigned long buffer_size;
		unsigned long pitch;
		stm_blitter_surface_t *surface;
		stm_blitter_rect_t rect;
	} destination;

	/* TBD(vgi): extend in a pool of buffer
	 * today only one buffer is supported */
	struct stm_v4l2_video_buf video_buf;
};

struct audio_surface {
	void *buffer;
	unsigned long length;
	struct v4l2_audio_format format;
};

struct stm_v4l2_audio_buf {
	unsigned long                              intermediate_buffer_Phys;
	void                                      *intermediate_buffer_Virt;
	void                                      *virtual_address;
	unsigned long                              payload_length;
	unsigned long long                         presentation_time;
	stm_se_uncompressed_frame_metadata_audio_t uncompressed_metadata;
};


struct stm_v4l2_audio_capture {
	stm_memsink_h memsink_interface;

	unsigned int flags;
	struct audio_surface user_surface;

	/* TBD(vgi): extend in a pool of buffer
	 * today only one buffer is supported */
	struct stm_v4l2_audio_buf audio_buf;
};

/* Structure used to handle VBI & UserData */
struct stm_v4l2_buffer {
	struct list_head list;
	struct v4l2_buffer buf;
};

#define	GRAB_ZERO_COPY_ENABLED	(1<<4)	/* zero copy grab use case */
struct stm_v4l2_ud {
	/* V4L2 ioctl are all protected by a lock for each ldvb so no need to
	 * have a mutex here .. as long as .. we do not have object push */
	unsigned int flags;
	stm_memsink_h memsink_interface;
};

/*
 * The main capture device structure encapsulating video device
 */
struct stm_v4l2_capture_device {
	struct video_device viddev;
	struct media_pad *pads;
	void *mem_bpa2_ctx;
};
static struct stm_v4l2_capture_device stm_v4l2_capture_dev;

typedef enum input_video_source_e{
	STM_V4L2_DVB_VIDEO,
	STM_V4L2_VIDEXTIN_VIDEO
}input_video_source_t;

struct stm_v4l2_input_description {
	unsigned int flags;
	/*
	 * Store separate files for each capture type: audio/video/user data.
	 * Use them to identify the type of capture for close or other purpose.
	 */
	struct file *aud_file;
	struct file *vid_file;
	struct file *usr_file;
	unsigned int users;
	struct mutex lock;
	unsigned int pad_id;
	input_video_source_t  input_source;
	union {
		struct AudioDeviceContext_s *audio_dec;
		struct VideoDeviceContext_s *video_dec;
		struct v4l2_subdev *vidextin_handle;
	};
	union {
	/*
	 * Only one video/audio capture is allowed per context, this union
	 * makes it clear which capture queue is being used. In video, it
	 * can be either user data or video capture or both at the same time.
	 */
		struct video_queue {
			struct vb2_queue *vid_queue;
			struct vb2_queue *usr_queue;
		} vq;
		struct vb2_queue *aud_queue;
	};

	union {
		struct stm_vb2_video {
			struct stm_v4l2_vb2_buffer stm_vb2_vid;
			struct stm_v4l2_vb2_buffer stm_vb2_usr;
		} vb2_vid;
		struct stm_v4l2_vb2_buffer stm_vb2_audio;
	};
	void *priv;
};

struct stm_v4l2_dvb_fh {
	struct v4l2_fh fh;
	struct stm_v4l2_input_description *input;
};

static struct stm_v4l2_input_description *g_stm_v4l2_input_device;

struct stm_v4l2_video_type {
	struct stm_v4l2_video_capture *capture;
	struct stm_v4l2_ud *ud;
};

struct stm_v4l2_audio_type {
	struct stm_v4l2_audio_capture *capture;
};

typedef struct {
	struct v4l2_fmtdesc fmt;
	int depth;
	stm_blitter_surface_format_t blt_format;
} format_info;

static const format_info stm_blitter_v4l2_mapping_info[STM_BLITTER_SF_COUNT] = {
	/*
	 * This isn't strictly correct as the V4L RGB565 format has red
	 * starting at the least significant bit. Unfortunately there is no V4L2 16bit
	 * format with blue starting at the LSB. It is recognised in the V4L2 API
	 * documentation that this is strange and that drivers may well lie for
	 * pragmatic reasons.
	 */
	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "RGB-16 (5-6-5)",
	  V4L2_PIX_FMT_RGB565},
	 16,
	 STM_BLITTER_SF_RGB565},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "RGBA-16 (5-5-5-1)",
	  V4L2_PIX_FMT_BGRA5551},
	 16,
	 STM_BLITTER_SF_ARGB1555},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "RGBA-16 (4-4-4-4)",
	  V4L2_PIX_FMT_BGRA4444},
	 16,
	 STM_BLITTER_SF_ARGB4444},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "RGB-24 (B-G-R)",
	  V4L2_PIX_FMT_BGR24},
	 24,
	 STM_BLITTER_SF_RGB24},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "ARGB-32 (8-8-8-8)",
	  V4L2_PIX_FMT_BGR32},
	 32,
	 STM_BLITTER_SF_ARGB},	/* Note that V4L2 does not define the alpha channel */

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "BGRA-32 (8-8-8-8)",
	  V4L2_PIX_FMT_RGB32},
	 32,
	 STM_BLITTER_SF_BGRA},	/* big endian BGR as BTTV driver, not as V4L2 spec */

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:2 (U-Y-V-Y)",
	  V4L2_PIX_FMT_UYVY},
	 16,
	 STM_BLITTER_SF_UYVY},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:2 (Y-U-Y-V)",
	  V4L2_PIX_FMT_YUYV},
	 16,
	 STM_BLITTER_SF_YUY2},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:2MB",
	  V4L2_PIX_FMT_STM422MB},
	 8,			/* bits per pixel for luma only */
	 STM_BLITTER_SF_YCBCR422MB},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:0MB",
	  V4L2_PIX_FMT_STM420MB},
	 8,			/* bits per pixel for luma only */
	 STM_BLITTER_SF_YCBCR420MB},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:0 (YUV)",
	  V4L2_PIX_FMT_YUV420},
	 8,			/* bits per pixel for luma only */
	 STM_BLITTER_SF_I420},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:0 (YVU)",
	  V4L2_PIX_FMT_YVU420},
	 8,			/* bits per pixel for luma only */
	 STM_BLITTER_SF_YV12},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:2 (YUV)",
	  V4L2_PIX_FMT_YUV422P},
	 8,			/* bits per pixel for luma only */
	 STM_BLITTER_SF_YV61},

/*
 * To support "capture in zero_copy mode":
 * This should be the "native pixelformat" produced by Video
 * decoders on Orly and 7108 (not clear if all codec or only
 * H264 and MPEG). Also supported by BLITTER
 * User can capture this format (set via VIDIOC_S_FMT):
 *   - with a blit copy  (codec frame buffers not shared)
 *   - without copy      (codec frames are provided by user)
 */
	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:0 (Raster)",
	  V4L2_PIX_FMT_NV12},
	 8,			/* bits per pixel for luma only */
	 STM_BLITTER_SF_NV12},
};

/*
 * Small helper to check if a pad is connected or not (linked and enabled)
 */
static int isInputLinkEnabled(struct stm_v4l2_input_description *input)
{
	struct media_pad *local;
	struct media_pad *remote;
	int id = 0;

	local = &stm_v4l2_capture_dev.pads[input->pad_id];
	remote = stm_media_find_remote_pad(local, MEDIA_LNK_FL_ENABLED, &id);

	if (!remote)
		return 0;

	return 1;
}


/*
 * Retrieve the LinuxDVB struct {Audio/Video}DeviceContext_s pointer from V4L2 input
 */
static struct AudioDeviceContext_s *v4l2_input_to_AudioDvbContext(struct stm_v4l2_input_description
								    *input)
{
	struct media_pad *local_pad;
	struct media_pad *remote_pad;
	struct media_entity *me;
	int id = 0;

	local_pad = &stm_v4l2_capture_dev.pads[input->pad_id];
	remote_pad = stm_media_find_remote_pad(local_pad, 0, &id);
	if (!remote_pad)
		return NULL;
	me = remote_pad->entity;

	return stm_media_entity_to_audio_context(me);
}

static struct VideoDeviceContext_s *v4l2_input_to_VideoDvbContext(struct stm_v4l2_input_description
								    *input)
{
	struct media_pad *local_pad;
	struct media_pad *remote_pad;
	struct media_entity *me;
	int id = 0;

	local_pad = &stm_v4l2_capture_dev.pads[input->pad_id];
	remote_pad = stm_media_find_remote_pad(local_pad, 0, &id);
	if (!remote_pad)
		return NULL;
	me = remote_pad->entity;

	return stm_media_entity_to_video_context(me);
}

static int read_data_from_memsink(stm_memsink_h memsink, bool nonblocking,
				  void *ptr, unsigned int max_size,
				  unsigned int *size);
static int video_stream_read_memsink(struct stm_v4l2_video_capture
				     *video_capture, bool nonblocking)
{
	int ret = 0;
	stm_se_capture_buffer_t *capture_buffer;
	unsigned int size;

	capture_buffer =
	    (stm_se_capture_buffer_t *)video_capture->video_buf.intermediate_buffer_Virt;
	/* Set the physical address & virtual address to
	 * where the streaming engine should write the real data */
	capture_buffer->physical_address = (void *)
	    video_capture->video_buf.intermediate_buffer_Phys + 256;
	capture_buffer->virtual_address =
	    video_capture->video_buf.intermediate_buffer_Virt + 256;
	capture_buffer->buffer_length = VIDEO_MAX_BUFFER_SIZE - 256;

	/* Read data from the memsink */
	ret = read_data_from_memsink(video_capture->memsink_interface,
				     nonblocking,
				     (void *)video_capture->video_buf.
				     intermediate_buffer_Virt,
				     VIDEO_MAX_BUFFER_SIZE, &size);
	if (ret)
		goto function_exit;

	/* Check received data */
	if (size < sizeof(stm_se_capture_buffer_t)) {
		printk(KERN_ERR
		"%s: received something shorter than a capture_buffer header !!! Trash it\n",
		       __func__);
		ret = -EIO;
		goto function_exit;
	}

	/* Check EOS flag */
	if (capture_buffer->u.uncompressed.discontinuity &
		STM_SE_DISCONTINUITY_EOS) {
	/* EOS frame should be length = 0  if not force it*/
		if(capture_buffer->payload_length != 0) {
			printk(KERN_WARNING
			"%s: received EOS with frame length != 0 \n",
				__func__);
			capture_buffer->payload_length = 0;
		}
	}

	if (capture_buffer->u.uncompressed.native_time_format
			!= TIME_FORMAT_US) {
		printk(KERN_ERR
		"%s: received unsupported presentation time !!Trash it \n",
				__func__);
		ret = -EIO;
		goto function_exit;
	}

	video_capture->video_buf.physical_address =
		capture_buffer->physical_address;
	video_capture->video_buf.payload_length =
		capture_buffer->payload_length;
	video_capture->video_buf.presentation_time =
	    capture_buffer->u.uncompressed.native_time;
	/* save metadata info for this buffer */
	memcpy(&video_capture->video_buf.uncompressed_metadata,
			&capture_buffer->u.uncompressed.video,
			sizeof(stm_se_uncompressed_frame_metadata_video_t));

function_exit:
	return ret;
}

static int video_stream_prepare_to_blit(struct stm_v4l2_video_capture
				     *video_capture)
{
	int ret = 0;
	stm_blitter_surface_format_t fmt;
	stm_blitter_surface_address_t addr;
	stm_blitter_dimension_t dim;
	stm_blitter_surface_colorspace_t cspace;
	stm_se_uncompressed_frame_metadata_video_t *metadata =
		&video_capture->video_buf.uncompressed_metadata;
	unsigned aligned_size_h, aligned_size_w, vAlign;


	if (video_capture->source.surface)
		stm_blitter_surface_put(video_capture->source.surface);
	video_capture->source.surface = NULL;

	/* Convert the source origin into something useful to the blitter.
	 * The incoming coordinates can be in either whole integer or multiples
	 * of a 16th or a 32nd of a pixel/scanline. */

	video_capture->source.rect.position.x =
		metadata->window_of_interest.x << (1 * 16);
	video_capture->source.rect.position.y =
		metadata->window_of_interest.y << (1 * 16);

	video_capture->source.rect.size.w =
		metadata->window_of_interest.width;
	video_capture->source.rect.size.h =
		metadata->window_of_interest.height;
	dim.w = metadata->video_parameters.width;
	dim.h = metadata->video_parameters.height;

	addr.base = (unsigned long)video_capture->video_buf.physical_address;

	/* Set the surface format and chroma buffer offset */

	vAlign = 32;
	if (metadata->vertical_alignment>=1)
		vAlign = metadata->vertical_alignment;
	aligned_size_w = metadata->pitch;
	aligned_size_h = ALIGN_UP(metadata->video_parameters.height, vAlign);
	addr.cb_offset = aligned_size_w * aligned_size_h;
	addr.cr_offset = addr.cb_offset;
	addr.cbcr_offset = aligned_size_w * aligned_size_h;

	switch (metadata->surface_format) {
	case SURFACE_FORMAT_VIDEO_8888_ARGB:
		fmt = STM_BLITTER_SF_ARGB;
		break;

	case SURFACE_FORMAT_VIDEO_888_RGB:
		fmt = STM_BLITTER_SF_RGB24;
		break;

	case SURFACE_FORMAT_VIDEO_565_RGB:
		fmt = STM_BLITTER_SF_RGB565;
		break;

	case SURFACE_FORMAT_VIDEO_422_RASTER:
		fmt = STM_BLITTER_SF_UYVY;
		break;

	case SURFACE_FORMAT_VIDEO_420_PLANAR:
		fmt = STM_BLITTER_SF_I420;
		break;

	case SURFACE_FORMAT_VIDEO_422_PLANAR:
		fmt = STM_BLITTER_SF_YV61;
		break;

	case SURFACE_FORMAT_VIDEO_422_YUYV:
		fmt = STM_BLITTER_SF_YUY2;
		break;

	case SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK:
	case SURFACE_FORMAT_VIDEO_420_MACROBLOCK:
		fmt = STM_BLITTER_SF_YCBCR420MB;
		break;

	case SURFACE_FORMAT_VIDEO_420_RASTER2B:
		fmt = STM_BLITTER_SF_NV12;
		break;

	default:
		printk(KERN_WARNING
		       "%s: Unsupported surface format %x\n",
		       __FUNCTION__, metadata->surface_format);
		ret = -EINVAL;
		goto function_exit;
	}

	/* Set the color Space */
	switch(metadata->video_parameters.colorspace)
	{
		case STM_SE_COLORSPACE_SMPTE170M:
			cspace = STM_BLITTER_SCS_BT601;
			break;
		case STM_SE_COLORSPACE_SMPTE240M:
			cspace = STM_BLITTER_SCS_BT709;
			break;
		case STM_SE_COLORSPACE_BT709:
			cspace = STM_BLITTER_SCS_BT709;
			break;
		case STM_SE_COLORSPACE_BT470_SYSTEM_M:
			cspace = STM_BLITTER_SCS_BT601;
			break;
		case STM_SE_COLORSPACE_BT470_SYSTEM_BG:
			cspace = STM_BLITTER_SCS_BT601;
			break;
		case STM_SE_COLORSPACE_SRGB:
			cspace = STM_BLITTER_SCS_RGB;
			break;
		default:
			cspace = (fmt & STM_BLITTER_SF_YCBCR)?
			(metadata->video_parameters.width>CSPACE_LIMIT_WIDTH?
			STM_BLITTER_SCS_BT601: STM_BLITTER_SCS_BT709):
			STM_BLITTER_SCS_RGB;
			break;
	}

	video_capture->source.surface
	    = stm_blitter_surface_new_preallocated(fmt,
			cspace, &addr, video_capture->video_buf.payload_length,
			&dim, metadata->pitch);
	if (IS_ERR(video_capture->source.surface)) {
		printk(KERN_WARNING
		       "%s: couldn't create source surface: %ld\n",
		       __FUNCTION__, PTR_ERR(video_capture->source.surface));
		video_capture->source.surface = NULL;
	}

function_exit:
	return ret;
}

static int video_stream_execute_blit(struct stm_v4l2_video_capture *capture)
{
	int ret = 0;

	if (capture->source.rect.position.x & 0xffff
	    || capture->source.rect.position.y & 0xffff) {
		DVB_DEBUG("blitting from %ld.%06ld,%ld.%06ld - %ldx%ld\n",
			  capture->source.rect.position.x >> 16,
			  ((capture->source.rect.position.x & 0xffff) *
			   1000000) / (1 << 16),
			  capture->source.rect.position.y >> 16,
			  ((capture->source.rect.position.y & 0xffff) *
			   1000000) / (1 << 16), capture->source.rect.size.w,
			  capture->source.rect.size.h);
	} else {
		DVB_DEBUG("blitting from %ld,%ld - %ldx%ld\n",
			  capture->source.rect.position.x >> 16,
			  capture->source.rect.position.y >> 16,
			  capture->source.rect.size.w,
			  capture->source.rect.size.h);
		DVB_DEBUG("  to %ld,%ld - %ldx%ld\n",
			  capture->destination.rect.position.x,
			  capture->destination.rect.position.y,
			  capture->destination.rect.size.w,
			  capture->destination.rect.size.h);
	}

	/* set blit flags - the source is _always_ in fixed point */
	BUG_ON(capture->destination.surface == NULL);
	BUG_ON(capture->source.surface == NULL);
	stm_blitter_surface_set_blitflags(capture->destination.surface,
					  (0 | STM_BLITTER_SBF_NONE
					   |
					   STM_BLITTER_SBF_SRC_XY_IN_FIXED_POINT));
	stm_blitter_surface_set_porter_duff(capture->destination.surface,
					    STM_BLITTER_PD_SOURCE);

	stm_blitter_surface_add_fence(capture->destination.surface);

	ret = stm_blitter_surface_stretchblit
	    (capture->blitter, capture->source.surface, &capture->source.rect,
	     capture->destination.surface, &capture->destination.rect,
	     1);
	if (ret == 0){
		/* success - wait for operation to finish */
		stm_blitter_serial_t serial;

		stm_blitter_surface_get_serial(capture->destination.surface,
					       &serial);
		stm_blitter_wait(capture->blitter, STM_BLITTER_WAIT_FENCE,
				 serial);
	} else
		printk(KERN_WARNING "%s: Error during blitter operation\n",
		       __FUNCTION__);

	/* Mark as done, so we can queue a new buffer */
	stm_blitter_surface_put(capture->destination.surface);
	capture->destination.surface = NULL;
	return ret;
}

static int audio_stream_read_memsink(struct stm_v4l2_audio_capture
				     *audio_capture, bool nonblocking)
{
	int ret = 0;
	stm_se_capture_buffer_t *capture_buffer;
	unsigned int size;

	capture_buffer =
	    (stm_se_capture_buffer_t *)audio_capture->
	    audio_buf.intermediate_buffer_Virt;
	/* Set the physical address & virtual address to
	 * where the streaming engine should write the real data */
	capture_buffer->physical_address =(void *)
	    audio_capture->audio_buf.intermediate_buffer_Phys + 256;
	capture_buffer->virtual_address =
	    audio_capture->audio_buf.intermediate_buffer_Virt + 256;
	capture_buffer->buffer_length = AUDIO_MAX_BUFFER_SIZE - 256;

	/* Read data from the memsink */
	ret = read_data_from_memsink(audio_capture->memsink_interface,
				     nonblocking,
				     (void *)audio_capture->
				     audio_buf.intermediate_buffer_Virt,
				     AUDIO_MAX_BUFFER_SIZE, &size);
	if (ret)
		goto function_exit;

	/* Check received data */
	if (size < sizeof(stm_se_capture_buffer_t)) {
		printk(KERN_ERR
		       "%s: received something shorter than"
		       " a capture_buffer header !!! Trash it\n",
		       __func__);
		ret = -EIO;
		goto function_exit;
	}

	/* Check EOS flag */
	if (capture_buffer->u.uncompressed.discontinuity &
		STM_SE_DISCONTINUITY_EOS) {
	/* EOS frame should be length = 0  if not force it*/
		if(capture_buffer->payload_length != 0) {
			printk(KERN_WARNING
			"%s: received EOS with frame length != 0 \n",
				__func__);
			capture_buffer->payload_length = 0;
		}
	}

	if (capture_buffer->u.uncompressed.native_time_format
			!= TIME_FORMAT_US) {
		printk(KERN_ERR
		"%s: received unsupported presentation time !!Trash it \n",
				__func__);
		ret = -EIO;
		goto function_exit;
	}

	audio_capture->audio_buf.virtual_address =
		capture_buffer->virtual_address;
	audio_capture->audio_buf.payload_length =
		capture_buffer->payload_length;
	audio_capture->audio_buf.presentation_time =
	    capture_buffer->u.uncompressed.native_time;
	/* save metadata info for this buffer */
	memcpy(&audio_capture->audio_buf.uncompressed_metadata,
			&capture_buffer->u.uncompressed.audio,
			sizeof(stm_se_uncompressed_frame_metadata_audio_t));

function_exit:
	return ret;
}

/* ======================================================================
 *                      Zero Copy helpers
 *
 * For time being, strategy is to isolate "grab zero copy" changes
 * and to continue to support also "standard" grab (further stage likley
 * merge the two, but this need a choice about Memory Sink)
 *
 * Basic helpers:
 * --------------
 */

/* checks if current use case is a "capture in zero_copy mode" */
static inline int zero_copy_enabled(struct stm_v4l2_input_description *input)
{
	struct stm_v4l2_video_type *video = input->priv;
	struct stm_v4l2_video_capture *capture;
	capture = video->capture;

	if ((capture) && (capture->flags & GRAB_ZERO_COPY_ENABLED))
		return 1;
	return 0;
}

/* Search a pushed object in pool */
static
struct stm_v4l2_zc_pool *_zc_kvaddr2pool_slot (
                             struct stm_v4l2_video_capture    *capture,
			     unsigned long                     kern_addr)
{
	int i;
	struct stm_v4l2_zc_pool *pool = capture->zero.pool;
	for (i = 0; i < capture->zero.pool_idx; i++, pool++)
		if (pool->ker_addr == kern_addr)
			return(pool);
	return NULL;
}

static
struct stm_v4l2_zc_pool *_zc_bus_addr2pool_slot (
                             struct stm_v4l2_video_capture    *capture,
			     unsigned long                     bus_addr)
{
	int i;
	struct stm_v4l2_zc_pool *pool = capture->zero.pool;
	for (i = 0; i < capture->zero.pool_idx; i++, pool++)
		if (pool->bus_addr == bus_addr)
			return(pool);
	return NULL;
}

/* Helpers (and callbacks) managing connection between capture device
 * and video player (concerns STKPI attach and registry)
 * -------------------------------------------------------------------
 *
 * Callback to play_stream to free frames.
 * It's a global reference, for time being assume a single callback is
 * shared among all capture instances
 */
int  (*stm_zc_release_bufs)(stm_object_h   src_object,
                                stm_object_h   released_object);

/* Second stage of attach:
 *      See VIDIOC_STREAMON: in "zero copy mode" it creates an
 *      instance of "v4l2 capture sink" and attaches this instance
 *      to play_stream (this funtion is triggered by stm_se_play_stream_attach)
 * Args:
 *    src_object : the player object V4l2 capture is attached to
 *    sink_object: this instance of "V4L2 capture sink"
 */
static int stm_v4l2_zc_attach_from_decoder (stm_object_h        src_object,
			                    stm_object_h        sink_object,
	        struct stm_data_interface_release_src *release_src)
{
	struct stm_v4l2_input_description *input = sink_object;
	struct stm_v4l2_video_type *video = input->priv;
	struct stm_v4l2_video_capture *capture = video->capture;

	_STRACE("play_stream attach capture device (%p, %p)!\n",
	        src_object, sink_object);

	/* Paranoia */
	if (!capture || (capture->zero.zc_instance != sink_object)) {
		DVB_ERROR("%s: internal error - ATTCH failed!\n", __func__);
		BUG();
		return -EIO;
	}

	/* save the true source handle (likley the SE manifestor for
         * zero copy grab)
         */
	capture->zero.source = src_object;

	/* save the callback to release a source object */
	stm_zc_release_bufs = release_src->release_data;
	BUG_ON(stm_zc_release_bufs == NULL);
	return 0;
}

/* This function is triggered by stm_se_play_stream_detach:
 *    Can be caused by a STREAMOFF or close action at V4L level.
 *    STREAMOFF should return all buffers to play_stream so this
 *    callback has almost nothing to do.
 */
static int stm_v4l2_zc_detach_from_decoder (stm_object_h   src_object,
					    stm_object_h   sink_object)
{
	struct stm_v4l2_input_description *input = sink_object;
	struct stm_v4l2_video_type *video = input->priv;
	struct stm_v4l2_video_capture *capture = video->capture;

	_STRACE("play_stream is detached!\n");
	capture->zero.source      = NULL;
	return 0;
}

/*
 * This functionin is triggered by player grab manifestor (and executed
 * in its context) to notify a new decoded frame.
 * It just queue the object and returns:
 *  - if the frame is corrupted simply returns an error to Player
 *  - if the frame is acceptable but the user application is not yet
 *    ready to stream (STREAMON) simply notifyes the player the frame
 *    is free.
 */
int stm_v4l2_zc_push_from_decoder (stm_object_h    sink_object,
			                  stm_object_h    pushed_object)
{
	struct stm_v4l2_input_description *input = sink_object;
	struct stm_v4l2_video_type *video = input->priv;
	struct stm_v4l2_video_capture *capture = video->capture;
	stm_se_capture_buffer_t    *frame;
	struct stm_v4l2_zc_pool         *p;   /* pool slot */
	struct stm_v4l2_vb2_buffer *stm_vb2;
	struct vb2_queue *q;

	frame   = (stm_se_capture_buffer_t *) pushed_object;

	/* Paranoia  */
	if (!capture || !pushed_object ||
            (capture->zero.zc_instance != sink_object) ||
            (capture->zero.source == NULL)) {
		DVB_ERROR("%s: Internal error - attach/detach failed (%p, %p, %p)!\n",
			  __func__, sink_object, pushed_object, capture);
		goto error_invalid_params;
	}

	if (mutex_lock_interruptible(&capture->mutex))
		return -ERESTARTSYS;

	stm_vb2 = list_first_entry(&capture->zero.stm_vb2->list,
					struct stm_v4l2_vb2_buffer, list);
	q = stm_vb2->vb2_buf->vb2_queue;

	/* User app. not yet (or no longer) ready to receive frames
	 * (a STREAMON has faile or STREAMOFF called)
	 * simply discard!
	 */
	if (!vb2_is_streaming(q)) {
		stm_zc_release_bufs(capture->zero.source, pushed_object);
		mutex_unlock(&capture->mutex);
		return 0;
	}

	/* should we prevent the decoded frame queue to overcome a
         * dangerous threshold ?
         * This could compromize the QoS since the decoder runs in
         * shortage of frames. Logic is not trivial since a threshold
         * depend on codec and standard!
	 */

	if (EOS_FRAME(frame)) {
	        /* treat End Of Stream frame (not in pool) */
		p = &capture->zero.eos_frame;
	} else {
		/* coming frame must be one of the pool! */
		p = _zc_bus_addr2pool_slot(capture, (unsigned long)frame->physical_address);
	}

	if (!p) {
		DVB_DEBUG("%s: Internal error - video decode pushes frames with bus addr 0x%lx not in pool!\n", __func__, (unsigned long)frame->physical_address);
	        mutex_unlock(&capture->mutex);
		goto push_error;
	}

	/* Init node link... (a pool slot is the node) */
	INIT_LIST_HEAD(&p->list);

        BUG_ON(p->src_object != NULL);

        p->src_object  = pushed_object;
	p->flags       = STM_ZC_FRAME_VALID;

	list_add_tail(&p->list, &capture->zero.wdqbuf_frames);

	_STRACE("frame %p: [%02d] p. %lx, !\n", frame,
	        p->buf_index, (unsigned long)frame->physical_address);
	wake_up_interruptible(&capture->zero.wdqbuf_thread);

	/*
	 * Buffer dequeue mechanism
	 */
	list_for_each_entry(stm_vb2, &capture->zero.stm_vb2->list, list) {
		if (EOS_FRAME(frame)) {
			if (stm_vb2->vb2_buf->state == VB2_BUF_STATE_ACTIVE) {
				/*
				 * EOS frame pushed by streaming engine is a
				 * dummy packet whose index turns out to be 0,
				 * and it is possible that the actual buffer
				 * with index 0 is not queued, so, mark the
				 * next available buffer as EOS and notify
				 * application to stop queueing and dequeueing
				 * the buffer.
				 */
				vb2_buffer_done(stm_vb2->vb2_buf, VB2_BUF_STATE_DONE);
				break;
			}
		} else {
			/*
			 * Find out which buffer has been pushed by streaming-
			 * engine and mark that buffer available to be dequeued.
			 */
			if (stm_vb2->vb2_buf->v4l2_buf.index == p->buf_index) {
				vb2_buffer_done(stm_vb2->vb2_buf,
							VB2_BUF_STATE_DONE);
				break;
			}
		}
	}

	mutex_unlock(&capture->mutex);
	return 0;

push_error:
	/* unusable frame! Let the player to decide what to do... */
	stm_zc_release_bufs(capture->zero.source, pushed_object);
error_invalid_params:
	return -EIO;
}

stm_data_interface_push_release_sink_t   stm_v4l2_zc_interface = {
		.connect    = stm_v4l2_zc_attach_from_decoder,
		.disconnect = stm_v4l2_zc_detach_from_decoder,
		.push_data  = stm_v4l2_zc_push_from_decoder,
};

/* capture device registered as an STKPI object!
 * Could be removed when Memsink will support push interface
 */
static	stm_object_h  zc_sink = NULL;

/* video_zc_init: called in two cases:
 *     1.  from VIDIOC_S_FMT, "capture zero copy" is accepted and a
 *         capture instance (input arg.) is initialized
 *     2.  at init time to register STLinuxTV  zero copy interface
 *         (in this case input arg = NULL)
 */
static int video_zc_init (struct stm_v4l2_video_capture *capture)
{
	int ret = 0;	/* by default, assume OK! */

	if (capture) {
		/* Init a new capture instance (see VIDIOC_S_FMT) */
		memset(&capture->zero, '\0', sizeof(struct zero_copy_s));

		INIT_LIST_HEAD(&capture->zero.wdqbuf_frames);
		init_waitqueue_head(&capture->zero.wdqbuf_thread);

		capture->flags |= GRAB_ZERO_COPY_ENABLED;

		/* Queue threshold: aim is to count frames waiting for
		 * DQBUF and eventually impose a limit to prevent the
		 * decoder to block because in shortage of buffers.
		 */
		capture->zero.queue_ts   = 0;

	}
	else
	{
		/* Called at device init time!
		 * For time being, since the Memsink doesn't support
		 * an asynch. push interface (as required by decoder
		 * to push out display frame) the ataptation acts as
		 * a sink and attaches its capture instance to decoder.
		 * To do this it registers a capture object with its
		 * push interface (shared by all instances).
		 * Register and deregister are triggered respectively
		 * by: stm_v4l2_capture_init and stm_v4l2_capture_exit
		 */

		ret = stm_registry_add_object(STM_REGISTRY_TYPES,  /* parent */
					      "v4l2_zc_sink", &zc_sink);
		if (ret) {
			DVB_ERROR("%s: failed to register V4L2 sink obj (%d)\n",
				   __func__, ret);
			return ret;
		}

		/* Register its interface (an attribute of object) */
		ret = stm_registry_add_attribute(
			(stm_object_h)&zc_sink,		  /* parent obj.      */
			STM_DATA_INTERFACE_PUSH_RELEASE,  /* tag (to inquiry) */
			STM_REGISTRY_ADDRESS,             /* data type of tag */
			&stm_v4l2_zc_interface,           /* value of tag     */
			sizeof(stm_v4l2_zc_interface));   /* size of tag      */
		if (ret) {
			DVB_ERROR("%s: failed to add V4L2 sink interface (%d)\n",
				  __func__, ret);
			if (stm_registry_remove_object(
                                                (stm_object_h)&zc_sink)) {
				/* should we care of a double error ? */
				DVB_ERROR("%s: registry remove failed!!\n",
					  __func__);
			};
			return ret;
		}
	}

	return ret;
}

void video_zc_exit (void)
{
	int ret = 0;	/* by default, assume OK! */

	/* Called when capture device is destroyed (exit)
	 * Releases all zero copy registered objects
	 * show the error, although no actions to recover!
	 * If happens likely because of internal inconsitencies
	 */

	ret =  stm_registry_remove_attribute(&zc_sink,
					     STM_DATA_INTERFACE_PUSH_RELEASE);
	if (ret)
		DVB_ERROR("%s: failed to remove zc-capture interface (%d)\n",
			   __func__, ret);

	ret = stm_registry_remove_object(&zc_sink);
	if (ret)
		DVB_ERROR("%s: failed to remove zc-capture obj (%d)\n",
			  __func__, ret);
}

/* VIDIOC_STREAMON zero copy helpers:
 * ----------------------------------
 */
int video_streamon_zc_capture(struct stm_v4l2_input_description *input)
{
	struct stm_v4l2_video_type *video = input->priv;
	char	                    name_of_instance[32];
	int ret;

	if (!video->capture->destination.format)
		/* FIXME: can this ever happen?? */
		return -EINVAL;

	/* protects the "zero grab capture" structure since it
	 * will be modified by connection handle!
	 */
	if (mutex_trylock(&(input->video_dec->DvbContext->Lock)) == 0) {
		DVB_ERROR
		    ("%s: failed to acquire DvbContext lock!\n", __func__);
		return -ENODEV;
        }

	if (input->video_dec->VideoStream == NULL) {
		mutex_unlock(&(input->video_dec->DvbContext->Lock));
		return -ENODEV;
	}

	/* for convenience, save a reference to "this capture instance" */
	video->capture->zero.zc_instance = input;

	_STRACE("VIDIOC_STREAMON register zero capture instance %p\n",
                video->capture->zero.zc_instance);

	/* and create a reference to "this zero copy capture instance"
	 * FIXME: name should be unique for instance!
	 */
	sprintf(name_of_instance, "DVB_ZERO_COPY_%p",
                video->capture->zero.zc_instance);
	ret = stm_registry_add_instance(
			   STM_REGISTRY_INSTANCES,    /* father */
			   &zc_sink,		      /* type   */
			   name_of_instance,
			   (stm_object_h) video->capture->zero.zc_instance);
	if(ret) {
		DVB_ERROR("%s: failed to register a new V4L2 sink instance (%d)\n",
                          __func__, ret);
		goto streamon_failed;
	}

	/* finally attach this instance to decoder (play_stream)
	 * Note: this action triggers the execution of
	 *	 stm_v4l2_zc_attach_from_decoder,
	 */
	ret = stm_se_play_stream_attach (
			input->video_dec->VideoStream->Handle,
			(stm_object_h) video->capture->zero.zc_instance,
                        STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);
	if (ret) {
		DVB_ERROR
		    ("%s: failed to attach play_stream to V4L2 sink (%d)\n",
		     __func__, ret);
		goto streamon_failed;
	}

	_STRACE("VIDIOC_STREAMON done!\n");
	/* reset STREAMING status */
	mutex_unlock(&(input->video_dec->DvbContext->Lock));
	return 0;

streamon_failed:
	/* to be sure reset reference to source */
	video->capture->zero.source = (stm_object_h)NULL;
	mutex_unlock(&(input->video_dec->DvbContext->Lock));
	/* map all registry and player errors to  -EIO */
	return -EIO;

}

int video_streamoff_zc_capture(struct stm_v4l2_input_description *input)
{
	struct stm_v4l2_video_type      *video = input->priv;
	struct stm_v4l2_video_capture   *capture;
	struct stm_v4l2_zc_pool         *pool;
	int i;
	int ret;

	/* capture lock prtects against races with QBUF, DQBUF and push
	 * frame from decoder
	 * DvbContext lock should protect against async. changes of
	 * decoder handles since referenced by detach hereafter
	 * Should we review the data structure to avoid this nasty
	 * sequence of locks?
	 * Likely this part should not be affected by a hgih rate of
	 * contentions, so a fine lock granulary it's only a cost!
	 */
	if (mutex_lock_interruptible(&video->capture->mutex))
		return -ERESTARTSYS;

	if (mutex_trylock(&(input->video_dec->DvbContext->Lock)) == 0) {
		DVB_ERROR
		    ("%s: failed to acquire DvbContext lock!\n", __func__);
		mutex_unlock(&video->capture->mutex);
		return -ENODEV;
        }

	/* reset STREAMING flag, any further push from decoder is returned */

	_STRACE("VIDIOC_STREAMOFF detaching play stream from v4l sink!\n");

	if (!video->capture->destination.format) {
		/* FIXME: can this ever happen?? */
		mutex_unlock(&(input->video_dec->DvbContext->Lock));
		mutex_unlock(&video->capture->mutex);
		return -EINVAL;
	}

	/* strict  check! */
	if ((!video->capture->zero.source) ||
            (!video->capture->zero.zc_instance)) {
		DVB_DEBUG("%s: Internal - fast capture bad status!\n", __func__);
		mutex_unlock(&(input->video_dec->DvbContext->Lock));
		mutex_unlock(&video->capture->mutex);
		return(ENODEV);
	}

	/* scan "zero copy" capture  pool and look for frame still
         * registered. Frames can be there for different reasons:
         * - waiting to be treated (not yet DQBUF)
         * - owned by user (waiting to be returned to SE)
         * STREAMOFF cannot wait for these frame (and most likley if
         * this happens mean the application hasn't correctly synchronized
         * its tasks). Before to detach from SE all the frames must be
         * returned (curious because in this case are not allocated
         * by SE, but SE logic still to be consolidate :-))
	 * From user point of view, frames captured but not completely
	 * treated should considered as lost.
	 */

	capture = video->capture;
	pool    = capture->zero.pool;

	for (i = 0; i < capture->zero.pool_idx; i++, pool++) {
		if (pool->src_object) {
                        _STRACE("flush pending frame %p ...\n",
				pool->src_object);
			stm_zc_release_bufs(capture->zero.source,
					    pool->src_object);
			pool->src_object = NULL;
		}
		/* reset FLAG and List head, this because a
		 * STREAMOF could be followed again by a STREAMON
		 */
		pool->flags = 0;
		INIT_LIST_HEAD(&pool->list);
	}

	/* output queue head become empty! */
	INIT_LIST_HEAD(&capture->zero.wdqbuf_frames);

	/* detach this zero capture instance from play_stream, this
	 * call triggers the execution of:
	 *    stm_v4l2_zc_detach_from_decoder
	 */
	if (input->video_dec->VideoStream &&
			input->video_dec->VideoStream->Handle) {
		ret = stm_se_play_stream_detach(input->video_dec->VideoStream->Handle,
				video->capture->zero.zc_instance);

		if (ret) {
			printk(KERN_ERR "%s: failed to detach decoder (zc capture)\n",
					__func__);
			mutex_unlock(&(input->video_dec->DvbContext->Lock));
			mutex_unlock(&capture->mutex);
			return -EIO;
		}
	}

	/* remove this instance from registry */
	if (stm_registry_remove_object(video->capture->zero.zc_instance)) {
		printk(KERN_ERR "%s: failed to unregister zc capture instance\n",
		       __func__);
		mutex_unlock(&(input->video_dec->DvbContext->Lock));
		mutex_unlock(&capture->mutex);
		return -EIO;
	}
	video->capture->zero.zc_instance = NULL;

	mutex_unlock(&(input->video_dec->DvbContext->Lock));
	mutex_unlock(&capture->mutex);

	_STRACE("STREAMOFF zero copy unregistered!\n");
	return 0;
}

/* VIDIOC_QBUF and VIDIOC_DQBUF "capture in zero copy" helpers -------------
 *
 * Send to decoder the buffer pool
 */
int video_qbuf_zc_send_to_player(struct stm_v4l2_video_capture  *capture,
				        stm_object_h		      player)
{
	struct stm_v4l2_zc_pool		*p;
	int				i;

	/* se_pinfo = streaming engine buffer pool info
	 * se_vref  = streaming engine video frame buffer reference
	 */
	stm_se_play_stream_decoded_frame_buffer_info_t         se_pinfo;
	stm_se_play_stream_decoded_frame_buffer_reference_t   *se_vref;

	se_vref = kzalloc(
	      (sizeof(stm_se_play_stream_decoded_frame_buffer_reference_t)
	       * capture->zero.buf_provided), GFP_KERNEL);
	if (se_vref == NULL)
	{
		DVB_ERROR("%s: VIDIOC_QBUF no space to transmit frame pool!\n",
                           __func__);
		return -EIO;
	}

	/* from internal list to streaming engine API */
	se_pinfo.number_of_buffers = capture->zero.buf_provided;
	se_pinfo.buffer_size	= capture->zero.buf_size;
	se_pinfo.buf_list       = se_vref;

	p = &capture->zero.pool[0];
	for (i = 0; i < capture->zero.buf_provided; i++, p++, se_vref++) {
		se_vref->virtual_address = (void *)p->ker_addr;
		se_vref->physical_address  = (void *)p->bus_addr;
                /* it's extremely unlikely that buffers have different size
                 * however ...
                 */
		se_vref->buffer_size  = p->buf_len;
		_STRACE(">>> k.vaddr %p  phys %p  len %u/0x%x\n",
			se_vref->virtual_address,
			se_vref->physical_address,
			se_vref->buffer_size,
			se_vref->buffer_size);
	}

	/* send to play stream */
	if (stm_se_play_stream_set_compound_control (
                                    player,
		                    STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS,
		                    (void *)&se_pinfo) != 0) {
		DVB_ERROR("%s: play stream set control has failed!\n",
                           __func__);
		kfree(se_pinfo.buf_list);
		return -EIO;
	}
	_STRACE("Sent %d buffers to SE\n", se_pinfo.number_of_buffers);
	kfree(se_pinfo.buf_list);
	return 0;
}


int video_qbuf_zc_capture(struct stm_v4l2_input_description *input,
			         struct vb2_buffer *vb)
{
	struct stm_v4l2_video_type    *video = input->priv;
	struct stm_v4l2_video_capture *capture;
	struct v4l2_buffer *buf = &vb->v4l2_buf;
	void *plane_cookie;

	capture = video->capture;

	/* protect against push race and STREAMON/OFF */
	if (mutex_lock_interruptible(&capture->mutex))
		return -ERESTARTSYS;

	/* check if pool is complete (enough QBUF), if not checks whether
         * user buffer is valid and hold it ...
         */
	if (capture->zero.pool_idx < capture->zero.buf_provided) {
		struct stm_v4l2_zc_pool		*p;
		int                              ret = 0;

		/* track this user buffer */
		p = &capture->zero.pool[capture->zero.pool_idx];

		/* checks for valid user address and contiguity */
		plane_cookie = vb2_plane_cookie(vb, 0);
		if ((!plane_cookie)||(plane_cookie && !(*(unsigned long *)plane_cookie))) {
			printk(KERN_ERR "%s: VIDIOC_QBUF user space addr. a bit wonky!\n", __func__);
			mutex_unlock(&capture->mutex);
			return -EINVAL;
		}
		p->bus_addr   = *(unsigned long *)plane_cookie;

		/* try to get kernel address (wanted by player)! */
		p->ker_addr   = (unsigned long)vb2_plane_vaddr(vb, 0);
		p->buf_len    = vb2_plane_size(vb, 0);
		p->buf_index  = buf->index;

		/* clear corresponding player frame (pushed object) */
		p->src_object = NULL;

		_STRACE("QBUF frame: [%d] k%lx p%lx (len 0x%x)!\n",
                        p->buf_index,
			p->ker_addr, p->bus_addr, p->buf_len);

		/* goto next free slot in pool */
		capture->zero.pool_idx++;

		/* When the pool is complete, it has to be sent to
		 * player. This must be done only once.
		 * Note: if the codec changes the application has the
		 *       responsibility to close and re-negotiate a
		 *       new frame pool.
                 */
		if (capture->zero.pool_idx == capture->zero.buf_provided) {
			ret = video_qbuf_zc_send_to_player (
					capture,
					input->video_dec->VideoStream->Handle);
		}
		mutex_unlock(&capture->mutex);

	} else {

		/* user thread is finally streaming! */

		stm_se_capture_buffer_t    *frame;
		struct stm_v4l2_zc_pool         *p;   /* pool slot */

		/* 1. from pool, get the buffer with this user address ... */
                p = _zc_kvaddr2pool_slot(capture, (unsigned long)vb2_plane_vaddr(vb, 0));

		if (!p) {
			printk(KERN_ERR " QBUF: buffer not in pool @ 0x%lx!\n", (unsigned long)vb2_plane_vaddr(vb, 0));
			mutex_unlock(&capture->mutex);
			return -EINVAL;
	        }

		/* 2. ...it should have a valid frame associate ...
		 *    (likely this first check can be removed)
		 */
		if (!p->src_object) {
			printk(KERN_ERR "Internal QBUF: not a free frame @ 0x%lx! at index: %d\n",
				p->ker_addr, buf->index);
			mutex_unlock(&capture->mutex);
			return -EINVAL;
		}

		frame = (stm_se_capture_buffer_t *) p->src_object;
		if ((unsigned long)frame->physical_address != p->bus_addr) {
			printk(KERN_ERR "Internal QBUF: invalid phys addr (frame @ 0x%lx, buf @ 0x%lx!\n",
				(unsigned long)frame->physical_address, p->bus_addr);
			/* however seems a frame, so return to player ... */
			p->src_object = NULL;
		        p->flags      = 0;
		        capture->zero.queue_ts--;
			stm_zc_release_bufs(capture->zero.source, frame);
			mutex_unlock(&capture->mutex);
			return -EINVAL;
		}

		/* 3. ... the object is a valid frame, return it to player */

		_STRACE("QBUF free object %p (frame @ 0x%lx, user 0x%lx)!\n",
                        p->src_object, p->bus_addr, p->ker_addr);


		p->flags      = 0;
		p->src_object = NULL;
	        capture->zero.queue_ts--;
		stm_zc_release_bufs(capture->zero.source, frame);
		mutex_unlock(&capture->mutex);
	}
	return 0;
}

static int video_stream_zc_set_metadata_in_v4l2_buf(
		stm_se_capture_buffer_t *frame,
		struct v4l2_buffer *buf)
{
	struct v4l2_video_uncompressed_metadata v4l2_metadata;
	stm_se_uncompressed_frame_metadata_video_t *se_metadata =
		&frame->u.uncompressed.video;
	void*  out_metadata_p;
	unsigned long long temporary_time;
	int ret = 0;

	/* Convert PTS into sec/usec.: the macro do_div() modifies its
	 * first argument, therefore using a temporary is good practice.
	 */
	temporary_time         = frame->u.uncompressed.native_time;;
	buf->timestamp.tv_usec = do_div(temporary_time, USEC_PER_SEC);
	buf->timestamp.tv_sec  = temporary_time;

	v4l2_metadata.framerate_num    = se_metadata->frame_rate.framerate_num;
	v4l2_metadata.framerate_den    = se_metadata->frame_rate.framerate_den;

	/* Set the field information */
	if (se_metadata->video_parameters.scan_type ==
			STM_SE_SCAN_TYPE_PROGRESSIVE)
		buf->field = V4L2_FIELD_NONE;
	else {
		if (se_metadata->top_field_first)
			buf->field = V4L2_FIELD_INTERLACED_TB;
		else
			buf->field = V4L2_FIELD_INTERLACED_BT;
	}

	out_metadata_p = (void*)buf->reserved;
	if (out_metadata_p != NULL) {
		ret = copy_to_user(out_metadata_p, &v4l2_metadata,
				sizeof(struct v4l2_video_uncompressed_metadata));
		if (ret) {
			DVB_ERROR("can't copy to user\n");
		}
	}
	return ret;
}

int video_dqbuf_zc_capture(struct stm_v4l2_input_description *input,
			          bool nonblocking, struct vb2_buffer *vb)
{
	struct stm_v4l2_video_type    *video = input->priv;
	struct stm_v4l2_video_capture *capture;
	struct stm_v4l2_zc_pool       *pool_slot;
	stm_se_capture_buffer_t       *frame;   /* from decoder */
	struct v4l2_buffer *buf = &vb->v4l2_buf;
	int ret;

	capture = video->capture;

	/* To capture in "zero copy" mode, means no crop and no different
	 * destination surfaces. More, if the "zero copy" mode is selected,
         * then GET/SET CROP or any control to modify the output should be
         * refused.
	 * if (!capture->destination.surface)
	 *	return -EIO;
	 */

	if (mutex_lock_interruptible(&capture->mutex))
		return -ERESTARTSYS;

	ret = list_empty(&capture->zero.wdqbuf_frames);

	mutex_unlock(&capture->mutex);

	if (ret) {
		/* Nothing queued yet and caller doesn't want to wait! */
		if (nonblocking) {
			return -EAGAIN;
		}

		/* Nothing queued yet and caller want to wait! */
                _STRACE("VIDIOC_DQBUF empty queue, wait!\n");
		if ( wait_event_interruptible(
			capture->zero.wdqbuf_thread,
			(!list_empty(&capture->zero.wdqbuf_frames))) != 0) {
                        _STRACE("VIDIOC_DQBUF empty queue interrupted!\n");
			return -EINTR;
		}
	}

	if (mutex_lock_interruptible(&capture->mutex))
		return -ERESTARTSYS;

	/* try to extract a frame ... */
	pool_slot = list_first_entry(&capture->zero.wdqbuf_frames,
				      struct stm_v4l2_zc_pool, list);

	/* too paranoic (better to _REMOVE_) ? */
	if (!pool_slot) {
		printk(KERN_ERR "%s: Internal: DQBUF list of frames corrupted!\n", __func__);
		mutex_unlock(&capture->mutex);
		return -EIO;
	}
	/* extract the frame  from node (a ref. to decoder structure) */
/*	BUG_ON(pool_slot->src_object == NULL); */
	frame = (stm_se_capture_buffer_t *)pool_slot->src_object;

	/* check if this is an End Of Stream (EOS) frame */
	if (EOS_FRAME(frame)) {
		vb2_set_plane_payload(vb, 0, 0);
                /* this could be a prb. for user, an EOS should be
                 * signaled as an empty frame also by player
                 */
		buf->index = vb->v4l2_buf.index;
	}
	else
	{
		buf->index      = pool_slot->buf_index;
		vb2_set_plane_payload(vb, 0, frame->payload_length);
	}

	/* fill metadata info in v4l2_buf */
	video_stream_zc_set_metadata_in_v4l2_buf(frame, buf);

	pool_slot->flags = STM_ZC_FRAME_TOUSER;

	/* Remove the node from DQBUF list ... and from memory */
	list_del(&pool_slot->list);

        _STRACE("VIDIOC_DQBUF obj %p (frame @ 0x%lx kva 0x%lx len 0x%x t/n %llu/%llu)\n",
                frame, (unsigned long)frame->physical_address, pool_slot->ker_addr, buf->bytesused,
                frame->u.uncompressed.system_time, frame->u.uncompressed.native_time);

	mutex_unlock(&capture->mutex);

	return 0;
}

/* End of "zero copy" code
 * ======================================================================
 */

/* VIDIOC_STREAMON helpers */
static int attach_memsink_to_play_stream(struct file *file,
					stm_se_play_stream_h handle,
					stm_memsink_h * memsink,
					stm_se_play_stream_output_port_t type,
					stm_data_mem_type_t mode)
{
	int ret;
	char memsink_name[STM_REGISTRY_MAX_TAG_SIZE];

	/* Here we create the memsink and connect it to the playstream */
	if (type == STM_SE_PLAY_STREAM_OUTPUT_PORT_ANCILLIARY)
		snprintf(memsink_name, STM_REGISTRY_MAX_TAG_SIZE,
			 "SE_ANC_TO_SINK-%p", (void *)handle);
	else
		snprintf(memsink_name, STM_REGISTRY_MAX_TAG_SIZE,
			 "SE_DEF_TO_SINK-%p", (void *)handle);

	ret = stm_memsink_new(memsink_name,
			((file->f_flags & O_NONBLOCK) == O_NONBLOCK) ?
			STM_IOMODE_NON_BLOCKING_IO : STM_IOMODE_BLOCKING_IO,
			mode, memsink);
	if (ret) {
		DVB_ERROR("%s: failed create memory sink (%d)\n", __func__,
			  ret);
		return -EINVAL;
	}

	ret = stm_se_play_stream_attach(handle, *memsink, type);
	if (ret) {
		DVB_ERROR
		    ("%s: failed to attach the play_stream to the memory sink (%d)\n",
		     __func__, ret);
		if (stm_memsink_delete(*memsink))
			printk(KERN_ERR "%s: failed to delete memsink\n", __func__);
		*memsink = NULL;
		return -EINVAL;
	}

	return 0;
}

static int detach_memsink_from_play_stream(stm_se_play_stream_h handle,
					   stm_memsink_h memsink)
{
	int ret;

	if (handle) {
		ret = stm_se_play_stream_detach(handle, memsink);
		if (ret) {
			printk(KERN_ERR
			       "%s: failed to detach the play_stream from the memory sink (%d)\n",
			       __func__, ret);
			return -EINVAL;
		}
	}

	ret = stm_memsink_delete(memsink);
	if (ret) {
		printk(KERN_ERR "%s: failed to delete the memory sink (%d)\n",
		       __func__, ret);
		return -EINVAL;
	}

	return 0;
}

#define MEMORY64MB_SIZE		(1<<26)
#define _ALIGN_DOWN(addr,size)	((addr)&(~((size)-1)))
static int allocate_intermediate_buffer(unsigned long *physicalAddr,
					void **virtAddr, unsigned long size)
{
	static const char *partname = BPA2_PARTITION;
	struct bpa2_part *part;
	unsigned long nPages;
	unsigned long phys;
	void *virt;

	part = bpa2_find_part(partname);
	if (!part) {
		printk(KERN_ERR "%s: failed to find the BPA2 area %s\n",
		       __func__, partname);
		return -EIO;
	}

	nPages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	phys = bpa2_alloc_pages(part, nPages, 1, GFP_KERNEL);
	if (!phys) {
		printk(KERN_ERR
		       "%s: failed to allocate %ld bytes (%ld pages) from %s\n",
		       __func__, size, nPages, partname);
		return -ENOMEM;
	}

	if (_ALIGN_DOWN (phys, MEMORY64MB_SIZE) != _ALIGN_DOWN (phys+size, MEMORY64MB_SIZE)){
		bpa2_free_pages(part, phys);
		phys = bpa2_alloc_pages(part, nPages,
					MEMORY64MB_SIZE/PAGE_SIZE, GFP_KERNEL);
		if (!phys) {
			printk(KERN_ERR
				"%s: failed to allocate %ld bytes (%ld pages) from %s with realignment of 64Mbytes\n",
				__func__, size, nPages, partname);
			return -ENOMEM;
		}
	}

	if (bpa2_low_part(part))
		virt = phys_to_virt(phys);
	else {
		virt = ioremap_nocache(phys, size);
                if (!virt) {
			printk(KERN_ERR
                               "bpa2: couldn't ioremap() region at 0x%08lx\n",
                               phys);
			bpa2_free_pages(part, phys);
			return -ENOMEM;
		}
	}

	*physicalAddr = phys;
	*virtAddr = virt;

	return 0;
}

static void free_intermediate_buffer(unsigned long physicalAddr, void *virt)
{
	static const char *partname = BPA2_PARTITION;
	struct bpa2_part *part;

	part = bpa2_find_part(partname);
	if (!part) {
		printk(KERN_ERR "%s: failed to find the BPA2 area %s\n",
		       __func__, partname);
		return;
	}

	if (!bpa2_low_part(part))
		iounmap(virt);

	bpa2_free_pages(part, physicalAddr);
}

static int stm_v4l2_capture_aud_streamon(struct stm_v4l2_input_description *input)
{
	struct stm_v4l2_audio_type *audio = input->priv;
	int ret;

	if (mutex_lock_interruptible(&(input->audio_dec->audops_mutex)))
		return -ERESTARTSYS;

	if (input->audio_dec->AudioStream == NULL) {
		mutex_unlock(&(input->audio_dec->audops_mutex));
		return -ENODEV;
	}

	/* Allocate the intermediate buffer for retrieving data out of the memsink */
	ret =
	    allocate_intermediate_buffer(&audio->capture->
					 audio_buf.intermediate_buffer_Phys,
					 &audio->capture->
					 audio_buf.intermediate_buffer_Virt,
					 AUDIO_MAX_BUFFER_SIZE);
	if (ret) {
		printk(KERN_ERR
		       "%s: failed to allocate the intermediate buffer\n",
		       __func__);
		mutex_unlock(&(input->audio_dec->audops_mutex));
		return ret;
	}

	ret =
	    attach_memsink_to_play_stream(input->aud_file,
				input->audio_dec->AudioStream->Handle,
				&audio->capture->memsink_interface,
				STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT,
				KERNEL);
	if (ret) {
		DVB_ERROR
		    ("%s: failed to attach the play_stream to the memory sink (%d)\n",
		     __func__, ret);
		ret = -EIO;
		goto error;
	}
	input->audio_dec->AudioStream->grab_Sink =
	    audio->capture->memsink_interface;

	mutex_unlock(&(input->audio_dec->audops_mutex));

	return 0;

error:
	free_intermediate_buffer(audio->capture->
			audio_buf.intermediate_buffer_Phys,
				 audio->capture->
			audio_buf.intermediate_buffer_Virt);
	mutex_unlock(&(input->audio_dec->audops_mutex));
	return ret;
}

static int video_streamon_capture(struct stm_v4l2_input_description *input)
{
	struct stm_v4l2_video_type *video = input->priv;
	int ret;

	if (!video->capture->destination.format)
		/* FIXME: can this ever happen?? */
		return -EINVAL;

	if (mutex_lock_interruptible(&(input->video_dec->vidops_mutex)))
		return -ERESTARTSYS;

	if (input->video_dec->VideoStream == NULL) {
		mutex_unlock(&(input->video_dec->vidops_mutex));
		return -ENODEV;
	}

	/* Allocate the intermediate buffer for retrieving data out of the memsink */
	ret =
	    allocate_intermediate_buffer(&video->capture->video_buf.
					 intermediate_buffer_Phys,
					 &video->capture->video_buf.
					 intermediate_buffer_Virt,
					 VIDEO_MAX_BUFFER_SIZE);
	if (ret) {
		printk(KERN_ERR
		       "%s: failed to allocate the intermediate buffer\n",
		       __func__);
		mutex_unlock(&(input->video_dec->vidops_mutex));
		return ret;
	}

	ret =
	    attach_memsink_to_play_stream(input->vid_file,
				input->video_dec->VideoStream->Handle,
				&video->capture->memsink_interface,
				STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT,
				KERNEL);
	if (ret) {
		DVB_ERROR
		    ("%s: failed to attach the play_stream to the memory sink (%d)\n",
		     __func__, ret);
		ret = -EIO;
		goto error;
	}

	input->video_dec->VideoStream->grab_Sink =
	    video->capture->memsink_interface;

	mutex_unlock(&(input->video_dec->vidops_mutex));

	return 0;

error:
	free_intermediate_buffer(video->capture->
			video_buf.intermediate_buffer_Phys,
				 video->capture->
			video_buf.intermediate_buffer_Virt);
	mutex_unlock(&(input->video_dec->vidops_mutex));
	return ret;
}

static int stm_v4l2_capture_usr_streamon(struct stm_v4l2_input_description *input)
{
	struct stm_v4l2_video_type *video = input->priv;
	int ret;

	if (mutex_lock_interruptible(&(input->video_dec->vidops_mutex)))
		return -ERESTARTSYS;

	if (input->video_dec->VideoStream == NULL) {
		mutex_unlock(&(input->video_dec->vidops_mutex));
		return -ENODEV;
	}

	ret =
	    attach_memsink_to_play_stream(input->usr_file,
				input->video_dec->VideoStream->Handle,
				&video->ud->memsink_interface,
				STM_SE_PLAY_STREAM_OUTPUT_PORT_ANCILLIARY,
				KERNEL);
	if (ret) {
		DVB_ERROR
		    ("%s: failed to attach the play_stream to the memory sink (%d)\n",
		     __func__, ret);
		mutex_unlock(&(input->video_dec->vidops_mutex));
		return -EINVAL;
	}

	input->video_dec->VideoStream->user_data_Sink =
	    video->ud->memsink_interface;

	mutex_unlock(&(input->video_dec->vidops_mutex));

	return 0;
}

/* VIDIOC_STREAMOFF helpers */
static int stm_v4l2_capture_aud_streamoff(struct stm_v4l2_input_description *input)
{
	struct stm_v4l2_audio_type *audio = input->priv;
	stm_se_play_stream_h play_stream = NULL;
	int ret;

	if (mutex_lock_interruptible(&(input->audio_dec->audops_mutex)))
		return -ERESTARTSYS;
	if (input->audio_dec->AudioStream) {
		play_stream = input->audio_dec->AudioStream->Handle;
		input->audio_dec->AudioStream->grab_Sink = NULL;
	}

	ret =
	    detach_memsink_from_play_stream(play_stream,
					    audio->capture->memsink_interface);
	if (ret) {
		printk(KERN_ERR "%s: failed to detach & release the memsink\n",
		       __func__);
		mutex_unlock(&(input->audio_dec->audops_mutex));
		return -EIO;
	}
	free_intermediate_buffer(audio->capture->
			audio_buf.intermediate_buffer_Phys,
				 audio->capture->
			audio_buf.intermediate_buffer_Virt);

	mutex_unlock(&(input->audio_dec->audops_mutex));

	return 0;
}

static int video_streamoff_capture(struct stm_v4l2_input_description *input)
{
	struct stm_v4l2_video_type *video = input->priv;
	stm_se_play_stream_h play_stream;
	int ret;

	if (!video->capture->destination.format)
		/* FIXME: can this ever happen?? */
		return -EINVAL;

	if (mutex_lock_interruptible(&(input->video_dec->vidops_mutex)))
		return -ERESTARTSYS;

	if (input->video_dec->VideoStream == NULL) {
		play_stream = NULL;
	} else {
		play_stream = input->video_dec->VideoStream->Handle;
		input->video_dec->VideoStream->grab_Sink = NULL;
	}

	ret =
	    detach_memsink_from_play_stream(play_stream,
					    video->capture->memsink_interface);
	if (ret) {
		printk(KERN_ERR "%s: failed to detach & release the memsink\n",
		       __func__);
		mutex_unlock(&(input->video_dec->vidops_mutex));
		return -EIO;
	}

	free_intermediate_buffer(video->capture->
			video_buf.intermediate_buffer_Phys,
				 video->capture->
			video_buf.intermediate_buffer_Virt);

	mutex_unlock(&(input->video_dec->vidops_mutex));

	return 0;
}

static int stm_v4l2_capture_usr_streamoff(struct stm_v4l2_input_description *input)
{
	struct stm_v4l2_video_type *video = input->priv;
	stm_se_play_stream_h play_stream;
	int ret;

	if (mutex_lock_interruptible(&(input->video_dec->vidops_mutex)))
		return -ERESTARTSYS;

	if (input->video_dec->VideoStream == NULL) {
		play_stream = NULL;
	} else {
		play_stream = input->video_dec->VideoStream->Handle;
		input->video_dec->VideoStream->user_data_Sink = NULL;
	}

	ret =
	    detach_memsink_from_play_stream(play_stream,
					    video->ud->memsink_interface);
	if (ret) {
		printk(KERN_ERR "%s: failed to detach & release the memsink\n",
		       __func__);
		mutex_unlock(&(input->video_dec->vidops_mutex));
		return -EIO;
	}

	mutex_unlock(&(input->video_dec->vidops_mutex));

	return 0;
}

/**
 * stm_v4l2_capture_s_fmt_vid_overlay
 * VIDIOC_S_FMT callback for V4L2_BUF_TYPE_VIDEO_OVERLAY
 */
static int stm_v4l2_capture_s_fmt_vid_overlay(struct file *file,
					void *fh, struct v4l2_format *fmt)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	int ret = 0;

	CHECK_INPUT(input, VIDEO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = VideoSetOutputWindowValue(input->video_dec,
			fmt->fmt.win.w.left, fmt->fmt.win.w.top,
			fmt->fmt.win.w.width, fmt->fmt.win.w.height);
	if (ret)
		DVB_ERROR("Unable to set format of overlay window %d\n", ret);

	mutex_unlock(&input->lock);

	return ret;
}

static struct vb2_ops stm_v4l2_capture_vb2_ops;

static int stm_v4l2_capture_s_fmt_aud_cap(struct file *file,
				void *fh, struct v4l2_format *fmt)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_audio_type *audio;
	struct v4l2_audio_format *audio_fmt;
	struct vb2_queue *q;
	int ret = 0;

	CHECK_INPUT(input, AUDIO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	audio = input->priv;

	if (!audio->capture) {
		audio->capture =
			kzalloc(sizeof(struct stm_v4l2_audio_capture), GFP_KERNEL);
		if (audio->capture == NULL) {
			ret = -ENOMEM;
			goto s_fmt_aud_cap_done;
		}
	}

	audio_fmt = (struct v4l2_audio_format *)&fmt->fmt.raw_data;
	memcpy(&audio->capture->user_surface.format, audio_fmt,
			sizeof(audio->capture->user_surface.format));

	if (input->aud_queue)
		goto s_fmt_aud_cap_done;

	/*
	 * Intitialize the vb2_queue for audio capture
	 */
	q = (struct vb2_queue *)kzalloc(sizeof(struct vb2_queue), GFP_KERNEL);
	if (!q) {
		DVB_ERROR("Failed to allocate memory for video queue\n");
		ret = -ENOMEM;
		goto s_fmt_aud_cap_q_alloc_failed;
	}

	/*
	 * Setup the vb2 queue now
	 */
	q->type = V4L2_BUF_TYPE_AUDIO_CAPTURE;
	q->io_modes = VB2_USERPTR | VB2_MMAP;
	q->ops = &stm_v4l2_capture_vb2_ops;
	q->mem_ops = &vb2_vmalloc_memops;
	q->drv_priv = input;
	q->buf_struct_size = 0; /* FIXME */

	ret = vb2_queue_init(q);
	if (ret) {
		DVB_ERROR("Video capture queue init failed\n");
		goto s_fmt_aud_cap_q_init_failed;
	}
	input->aud_queue = q;
	input->aud_file = file;

	mutex_unlock(&input->lock);

	return 0;

s_fmt_aud_cap_q_init_failed:
	kfree(q);
s_fmt_aud_cap_q_alloc_failed:
	kfree(audio->capture);
	audio->capture = NULL;
s_fmt_aud_cap_done:
	mutex_unlock(&input->lock);
	return ret;
}

/**
 * stm_capture_vid_s_fmt
 * Sets the format for V4L2_BUF_TYPE_VIDEO_CAPTURE
 */
static int stm_capture_vid_s_fmt(struct v4l2_format *fmt,
			struct stm_v4l2_input_description *input)
{
	const format_info *info;
	int n, mapping_index, ret = 0;
	stm_blitter_t *blitter;
	struct stm_v4l2_video_type *video = input->priv;
	struct stm_v4l2_video_capture *capture = video->capture;

	/*
	 * Allocate capture descriptor structure first, and let assume
	 * a "blit" transaction is always used to transfer to user
	 */
	if (!capture) {
		capture = kzalloc(sizeof(struct stm_v4l2_video_capture),
							GFP_KERNEL);
		if (!capture) {
			DVB_ERROR("Out of memory for video capture context\n");
			ret = -ENOMEM;
			goto s_fmt_alloc_failed;
		}
		video->capture = capture;

		mutex_init(&capture->mutex);

		/*
		 * Get a handle to the blitter
		 */
		blitter = stm_blitter_get(0);
		if (IS_ERR(blitter)) {
			DVB_ERROR("Cannot get blitter device\n");
			ret = -ENODEV;
			goto s_fmt_get_blitter_failed;
		}
		video->capture->blitter = blitter;
	}

	info = NULL;
	for (n = 0; n < ARRAY_SIZE(stm_blitter_v4l2_mapping_info); ++n)
		if (stm_blitter_v4l2_mapping_info[n].fmt.pixelformat ==
						fmt->fmt.pix.pixelformat) {
			mapping_index = n;
			info = &stm_blitter_v4l2_mapping_info[n];
			break;
		}

	if (!info || info->blt_format == STM_BLITTER_SF_INVALID) {
		DVB_ERROR("Invalid format request received\n");
		ret = -EINVAL;
		goto s_fmt_invalid_format;
	}

	if (!fmt->fmt.pix.bytesperline)
		fmt->fmt.pix.bytesperline =
				(info->depth * fmt->fmt.pix.width / 8);

	capture->mapping_index = mapping_index;
	capture->destination.format = info->blt_format;
	switch (fmt->fmt.pix.colorspace) {
	case V4L2_COLORSPACE_SMPTE170M:
		capture->destination.cspace =
			((fmt->fmt.pix.priv & V4L2_BUF_FLAG_FULLRANGE)
			 ? STM_BLITTER_SCS_BT601_FULLRANGE :
			   STM_BLITTER_SCS_BT601);
		break;

	case V4L2_COLORSPACE_REC709:
		capture->destination.cspace =
			((fmt->fmt.pix.priv & V4L2_BUF_FLAG_FULLRANGE)
			 ? STM_BLITTER_SCS_BT709_FULLRANGE :
			   STM_BLITTER_SCS_BT709);
		break;

	default:
		fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
		capture->destination.cspace =
			((fmt->fmt.pix.priv & V4L2_BUF_FLAG_FULLRANGE)
			 ? STM_BLITTER_SCS_RGB :
			   STM_BLITTER_SCS_RGB_VIDEORANGE);
		break;
	}

	/*
	 * This block to support "capture in zero_copy mode". "zero_copy" is
	 * accepted if both condition are true:
	 * a. user want native pixelformat
	 * b. decode buffers must be "sharable"
	 * Note: VIDIOC_S_FMT has to follow VIDIOC_S_INPUT so that
	 * 	 VideoEncodingFlags is valid
	 */
	if ((info->fmt.pixelformat == V4L2_PIX_FMT_NV12) &&
				(input->video_dec->VideoEncodingFlags &
				 VIDEO_ENCODING_USER_ALLOCATED_FRAMES)) {

		ret = video_zc_init(capture);
		if (ret) {
			DVB_ERROR("Zero-copy init failed\n");
			goto s_fmt_invalid_format;
		}
		capture->zero.stm_vb2 = &input->vb2_vid.stm_vb2_vid;
	}

	capture->destination.buffer_dimension.w = fmt->fmt.pix.width;
	capture->destination.buffer_dimension.h = fmt->fmt.pix.height;
	capture->destination.pitch = fmt->fmt.pix.bytesperline;

	capture->destination.rect.position.x = 0;
	capture->destination.rect.position.y = 0;
	capture->destination.rect.size.w = fmt->fmt.pix.width;
	capture->destination.rect.size.h = fmt->fmt.pix.height;


	return 0;

s_fmt_invalid_format:
	stm_blitter_put(video->capture->blitter);
	video->capture->blitter = NULL;
s_fmt_get_blitter_failed:
	kfree(capture);
	video->capture = NULL;
s_fmt_alloc_failed:
	return ret;
}

static int stm_v4l2_capture_s_fmt_usr_cap(struct file *file,
				void *fh, struct v4l2_format *fmt)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_video_type *video;
	struct vb2_queue *q;
	int ret = 0;

	CHECK_INPUT(input, USER);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	video = input->priv;

	if (!video->ud) {
		video->ud = kzalloc(sizeof(struct stm_v4l2_ud), GFP_KERNEL);
		if (!video->ud) {
			DVB_ERROR("Out of memory for user-data context\n");
			ret = -ENOMEM;
			goto s_fmt_usr_cap_done;
		}
	}

	if (input->vq.usr_queue)
		goto s_fmt_usr_cap_done;

	/*
	 * Intitialize the vb2_queue for user-data capture
	 */
	q = (struct vb2_queue *)kzalloc(sizeof(struct vb2_queue), GFP_KERNEL);
	if (!q) {
		DVB_ERROR("Failed to allocate memory for user-data queue\n");
		ret = -ENOMEM;
		goto s_fmt_usr_cap_queue_alloc_failed;
	}

	/*
	 * Setup the vb2 queue now
	 */
	q->type = V4L2_BUF_TYPE_USER_DATA_CAPTURE;
	q->io_modes = VB2_USERPTR | VB2_MMAP;
	q->ops = &stm_v4l2_capture_vb2_ops;
	q->mem_ops = &vb2_vmalloc_memops;
	q->drv_priv = input;
	q->buf_struct_size = 0; /* FIXME */

	ret = vb2_queue_init(q);
	if (ret) {
		DVB_ERROR("User capture queue init failed\n");
		goto s_fmt_usr_cap_queue_init_failed;
	}
	input->vq.usr_queue = q;
	input->usr_file = file;

	mutex_unlock(&input->lock);

	return 0;

s_fmt_usr_cap_queue_init_failed:
	kfree(q);
s_fmt_usr_cap_queue_alloc_failed:
	kfree(video->ud);
	video->ud = NULL;
s_fmt_usr_cap_done:
	mutex_unlock(&input->lock);
	return ret;
}

/**
 * stm_v4l2_capture_g_fmt_vid_overlay
 * VIDIOC_G_FMT for V4L2_BUF_TYPE_VIDEO_OVERLAY
 */
static int stm_v4l2_capture_g_fmt_vid_overlay(struct file *file,
					void *fh, struct v4l2_format *fmt)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	int ret = 0;

	CHECK_INPUT(input, VIDEO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = VideoGetOutputWindowValue(input->video_dec,
			&fmt->fmt.win.w.left, &fmt->fmt.win.w.top,
			&fmt->fmt.win.w.width, &fmt->fmt.win.w.height);

	if (ret)
		DVB_ERROR("Cannot get output window format %d\n", ret);

	mutex_unlock(&input->lock);

	return ret;
}

/**
 * stm_v4l2_capture_g_fmt_aud_cap
 * Get the audio format
 */
static int stm_v4l2_capture_g_fmt_aud_cap(struct file *file,
				void *fh, struct v4l2_format *fmt)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_audio_type *audio;
	struct v4l2_audio_format *audio_fmt;

	CHECK_INPUT(input, AUDIO);

	audio = input->priv;

	CHECK_AUDIO_FMT_LOCKED(audio, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	audio_fmt = (struct v4l2_audio_format *)&fmt->fmt.raw_data;

	memcpy(audio_fmt, &audio->capture->user_surface.format,
				       sizeof(struct v4l2_audio_format));

	mutex_unlock(&input->lock);

	return 0;
}

/* VIDIOC_TRY_FMT helpers */
static int video_try_fmt_capture(struct stm_v4l2_input_description *input,
				 struct v4l2_format *fmt)
{
	int n, surface = -1;

	for (n = 0; n < sizeof(stm_blitter_v4l2_mapping_info) /
	     sizeof(format_info); n++)
		if (stm_blitter_v4l2_mapping_info[n].fmt.pixelformat ==
		    fmt->fmt.pix.pixelformat)
			surface = n;

	if (surface == -1)
		return -EINVAL;

	return 0;
}

/**
 * video_s_crop_overlay
 * VIDIOC_S_CROP callback. Only valid for V4L2_BUF_TYPE_VIDEO_OVERLAY
 */
static int video_s_crop_overlay(struct stm_v4l2_input_description *input,
						const struct v4l2_crop *crop)
{
	int ret = 0;

	ret = VideoSetInputWindowValue(input->video_dec,
				crop->c.left, crop->c.top,
				crop->c.width, crop->c.height);

	if (ret)
		DVB_ERROR("Cannot crop input window %d\n", ret);

	return ret;
}

/**
 * video_g_crop_overlay
 * VIDIOC_G_CROP callback. Only valid for V4L2_BUF_TYPE_VIDEO_OVERLAY
 */
static int video_g_crop_overlay(struct stm_v4l2_input_description *input,
				struct v4l2_crop *crop)
{
	int ret;

	ret = VideoGetInputWindowValue(input->video_dec,
				&(crop->c.left), &(crop->c.top),
				&(crop->c.width), &(crop->c.height));
	if (ret)
		DVB_ERROR("Cannot get the input window dimensions %d\n", ret);

	return ret;
}

/* VIDIOC_G_PARM helpers */
static int video_g_parm_capture(struct stm_v4l2_input_description *input,
					 struct v4l2_streamparm *gparm)
{
	unsigned int frame_rate = 0;

	memset(&(gparm->parm.capture), 0, sizeof(gparm->parm.capture));
	VideoIoctlGetFrameRate(input->video_dec, &frame_rate);
	gparm->parm.capture.timeperframe.numerator = 1000;
	gparm->parm.capture.timeperframe.denominator = frame_rate;
	return 0;
}

/* VIDIOC_CROPCAP helpers */
static int video_cropcap_overlay(struct stm_v4l2_input_description *input,
				 struct v4l2_cropcap *cropcap)
{
	video_size_t video_size;

	VideoIoctlGetSize(input->video_dec, &video_size);

	cropcap->bounds.left = 0;
	cropcap->bounds.top = 0;
	cropcap->bounds.width = video_size.w;
	cropcap->bounds.height = video_size.h;

	cropcap->defrect = cropcap->bounds;

	VideoGetPixelAspectRatio(input->video_dec,
				 &cropcap->pixelaspect.numerator,
				 &cropcap->pixelaspect.denominator);

	return 0;
}

/* VIDIOC_REQBUFS helpers */
static int stm_v4l2_capture_aud_reqbufs(struct file *file,
				void *fh, struct v4l2_requestbuffers *buf)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_audio_type *audio;
	int ret;

	CHECK_INPUT(input, AUDIO);

	audio =  input->priv;

	CHECK_AUDIO_FMT_LOCKED(audio, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = vb2_reqbufs(input->aud_queue, buf);
	if (ret) {
		pr_err("%s: Unable to do vb_reqbuf:%d\n", __func__, ret);
		goto reqbufs_done;
	}

	DELETE_VBLIST(&input->stm_vb2_audio.list);

reqbufs_done:
	mutex_unlock(&input->lock);

	return ret;
}

static int video_reqbufs_capture(struct stm_v4l2_input_description *input,
				 struct v4l2_requestbuffers *buf)
{
	struct stm_v4l2_video_type *video = input->priv;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	CHECK_VIDEO_FMT_LOCKED(video, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	/* for time being, zero copy not yet supported in MMAP model.
	 * Seems all OK, now checks for enough user buffers
	 */
	if (zero_copy_enabled(input)) {
		/* checks if user has provided enough buffers */
		ctrl = v4l2_ctrl_find(&(input->video_dec->ctrl_handler),
					V4L2_CID_MIN_BUFFERS_FOR_CAPTURE);
		if(!ctrl) {
			ret = -EINVAL;
			goto reqbufs_done;
		}

		video->capture->zero.buf_required = v4l2_ctrl_g_ctrl(ctrl);
		if (video->capture->zero.buf_required > buf->count) {
			ret = -EINVAL;
			goto reqbufs_done;
		}

		video->capture->zero.buf_provided = buf->count;

		/* we fill the buf size also here*/
		ctrl = v4l2_ctrl_find(&(input->video_dec->ctrl_handler),
					V4L2_CID_MPEG_STM_FRAME_SIZE);
		if(!ctrl) {
			ret = -EINVAL;
			goto reqbufs_done;
		}

		video->capture->zero.buf_size = v4l2_ctrl_g_ctrl(ctrl);
	}

	ret = vb2_reqbufs(input->vq.vid_queue, buf);
	if (ret) {
		pr_err("%s: Unable to do vb_reqbuf:%d\n", __func__, ret);
		goto reqbufs_done;
	}

	DELETE_VBLIST(&input->vb2_vid.stm_vb2_vid.list);

reqbufs_done:
	mutex_unlock(&input->lock);
	return ret;
}

static int stm_v4l2_capture_usr_reqbufs(struct file *file,
				void *fh, struct v4l2_requestbuffers *buf)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_video_type *video = input->priv;
	int ret;

	CHECK_INPUT(input, USER);

	video = input->priv;

	CHECK_USER_FMT_LOCKED(video, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = vb2_reqbufs(input->vq.usr_queue, buf);
	if (ret) {
		pr_err("%s: Unable to do vb_reqbuf:%d\n", __func__, ret);
		goto reqbufs_done;
	}

	DELETE_VBLIST(&input->vb2_vid.stm_vb2_usr.list);

reqbufs_done:
	mutex_unlock(&input->lock);

	return ret;
}

/* VIDIOC_QBUF helpers */
static int stm_v4l2_capture_aud_qbuf(struct file *file,
				void *fh, struct v4l2_buffer *buf)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_audio_type *audio = input->priv;
	int ret;

	CHECK_INPUT(input, AUDIO);

	audio = input->priv;

	CHECK_AUDIO_FMT_LOCKED(audio, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = vb2_qbuf(input->aud_queue, buf);

	mutex_unlock(&input->lock);

	return ret;
}

static int video_qbuf_capture(struct stm_v4l2_input_description *input,
			      struct vb2_buffer *vb)
{
	struct stm_v4l2_video_type *video = input->priv;
	struct stm_v4l2_video_capture *capture;
	stm_blitter_surface_address_t address;
	void *plane_cookie;

	capture = video->capture;

	/* Currently we support capture of only one buffer */
	/* at a time, anything else doesn't make sense. */
	if (capture->destination.surface)
		return -EIO;

	plane_cookie = vb2_plane_cookie(vb, 0);
	if ((!plane_cookie) || (plane_cookie && !((unsigned long *)plane_cookie)))
		return -EIO;
	address.base = *(unsigned long *)plane_cookie;

	if (capture->destination.format & STM_BLITTER_SF_PLANAR) {
		switch (capture->destination.format) {
			/* FIXME: need Cb/Cr buffer(s) address(es) here */
		case STM_BLITTER_SF_I420:
		case STM_BLITTER_SF_YV12:
		case STM_BLITTER_SF_YV16:
		case STM_BLITTER_SF_YV61:
		case STM_BLITTER_SF_YCBCR444P:
			address.cb_offset = 0;	/* FIXME */
			address.cr_offset = 0;	/* FIXME */
			break;

		case STM_BLITTER_SF_NV12:
		case STM_BLITTER_SF_NV16:
		case STM_BLITTER_SF_NV21:
		case STM_BLITTER_SF_NV61:
			address.cbcr_offset = 0;	/* FIXME */
			break;

		default:
			/* nothing to do */
			break;
		}
	}

	if (vb->v4l2_buf.flags & V4L2_BUF_FLAG_FULLRANGE) {
		if (printk_ratelimit())
			printk(KERN_INFO
			       "You must switch to using"
			       " V4L2_BUF_FLAG_FULLRANGE during"
			       " VIDIOC_S_FMT\n");

		switch (capture->destination.cspace) {
		case STM_BLITTER_SCS_BT601:
			capture->destination.cspace =
			    STM_BLITTER_SCS_BT601_FULLRANGE;
			break;
		case STM_BLITTER_SCS_BT709:
			capture->destination.cspace =
			    STM_BLITTER_SCS_BT709_FULLRANGE;
			break;
		case STM_BLITTER_SCS_RGB_VIDEORANGE:
			capture->destination.cspace = STM_BLITTER_SCS_RGB;
			break;
		default:
			BUG();
			return -EINVAL;
		}
	}

	capture->destination.surface =
	    stm_blitter_surface_new_preallocated(capture->destination.format,
						 capture->destination.cspace,
						 &address,
						 vb2_plane_size(vb, 0),
						 &capture->destination.buffer_dimension,
						 capture->destination.pitch);
	if (IS_ERR(capture->destination.surface)) {
		printk(KERN_WARNING
		       "%s: couldn't create destination surface: %ld\n",
		       __FUNCTION__, PTR_ERR(capture->destination.surface));
		capture->destination.surface = NULL;
		return -EINVAL;
	}

	return 0;
}

static int stm_v4l2_capture_usr_qbuf(struct file *file,
				void *fh, struct v4l2_buffer *buf)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_video_type *video;
	int ret;

	CHECK_INPUT(input, USER);

	video = input->priv;

	CHECK_USER_FMT_LOCKED(video, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = vb2_qbuf(input->vq.usr_queue, buf);

	mutex_unlock(&input->lock);

	return ret;
}

/* VIDIOC_DQBUF helpers */
static int read_data_from_memsink(stm_memsink_h memsink, bool nonblocking,
				  void *ptr, unsigned int max_size,
				  unsigned int *size)
{
	int ret;

	ret = stm_memsink_pull_data(memsink, ptr, max_size, size);
	if (ret) {
		DVB_DEBUG("%s: Failed to pull_data\n", __func__);
		return -EIO;
	}

	/*
	 * Since, memsink is by default aligned to file->f_flags
	 * at the moment of creation, so, this can only happen,
	 * if memsink is non-blocking.
	 */
	if (*size == 0)
		return -EAGAIN;

	return 0;
}

static int audio_stream_set_metadata_in_v4l2_buf(
		struct stm_v4l2_audio_capture *capture,
		struct v4l2_buffer *buf)
{
	struct v4l2_audio_uncompressed_metadata v4l2_metadata;
	stm_se_uncompressed_frame_metadata_audio_t *se_metadata =
		&capture->audio_buf.uncompressed_metadata;
	void*  out_metadata_p;
	unsigned long long temporary_time;
	int sample_format;
	int ret = 0;
	int i;

	/* Convert PTS into sec/usec.
	 * do_div() modifies its first argument (it is a macro, not
	 * a function), therefore using a temporary is good practice.
	 */
	temporary_time = capture->audio_buf.presentation_time;
	buf->timestamp.tv_usec = do_div(temporary_time, USEC_PER_SEC);
	buf->timestamp.tv_sec = temporary_time;

	v4l2_metadata.sample_rate    = se_metadata->core_format.sample_rate;
	v4l2_metadata.channel_count  = se_metadata->core_format.channel_placement.channel_count;
	for (i=0; i<v4l2_metadata.channel_count; i++) {
		v4l2_metadata.channel[i]  =
			se_metadata->core_format.channel_placement.chan[i];
	}

	sample_format = stm_v4l2_convert_SE_v4l2_define( se_metadata->sample_format,
		stm_v4l2_audio_pcm_format_table,
	        sizeof(stm_v4l2_audio_pcm_format_table));

	if(sample_format == -1) {
		DVB_ERROR("%s: Not able to map sample_format=%d\n", __func__,
				sample_format);
		/* TBD(vgi): need to verify if we consider this as an error */
		sample_format = V4L2_MPEG_AUDIO_STM_PCM_FMT_32_S32LE;
	}
	v4l2_metadata.sample_format = sample_format;
	v4l2_metadata.program_level = se_metadata->program_level;
	v4l2_metadata.emphasis = se_metadata->emphasis;

	out_metadata_p = (void*)buf->reserved;
	if (out_metadata_p != NULL) {
		ret = copy_to_user(out_metadata_p, &v4l2_metadata,
				sizeof(struct v4l2_audio_uncompressed_metadata));
		if (ret) {
			DVB_ERROR("%s: can't copy to user\n", __func__);
		}
	}
	return ret;
}

static int stm_v4l2_capture_aud_dqbuf(struct vb2_buffer *vb,
				struct stm_v4l2_audio_capture *capture,
				bool nonblocking)
{
	struct v4l2_buffer *buf = &vb->v4l2_buf;
	unsigned int length;
	int ret;
	void *virtual_address;

	ret = audio_stream_read_memsink(capture, nonblocking);
	if (ret) {
		/* There were an error, give back the queued buffer */
		return ret;
	}

	/* set metadata in v4l2_buffer */
	ret = audio_stream_set_metadata_in_v4l2_buf(capture, buf);
	if (ret)
		return ret;

	length = min(capture->audio_buf.payload_length,
			vb2_plane_size(vb, 0));

	/* TODO
	 * Or shall we return an error if
	 * user_surface.length < audio_buf.payload_length ...*/
	virtual_address = vb2_plane_vaddr(vb, 0);
	if (virtual_address != NULL) {
		memcpy(virtual_address, capture->audio_buf.virtual_address, length);
		vb2_set_plane_payload(vb, 0, length);
	}

	return ret;
}

static int video_stream_set_metadata_in_v4l2_buf(
		struct stm_v4l2_video_capture *capture,
		struct v4l2_buffer *buf)
{
	struct v4l2_video_uncompressed_metadata v4l2_metadata;
	stm_se_uncompressed_frame_metadata_video_t *se_metadata =
		&capture->video_buf.uncompressed_metadata;
	void*  out_metadata_p;
	unsigned long long temporary_time;
	int ret = 0;

	/* Convert PTS into sec/usec.
	 * do_div() modifies its first argument (it is a macro, not
	 * a function), therefore using a temporary is good practice.
	 */
	temporary_time = capture->video_buf.presentation_time;
	buf->timestamp.tv_usec = do_div(temporary_time, USEC_PER_SEC);
	buf->timestamp.tv_sec = temporary_time;

	v4l2_metadata.framerate_num    = se_metadata->frame_rate.framerate_num;
	v4l2_metadata.framerate_den    = se_metadata->frame_rate.framerate_den;

	/* Set the field information */
	if (se_metadata->video_parameters.scan_type ==
			STM_SE_SCAN_TYPE_PROGRESSIVE)
		buf->field = V4L2_FIELD_NONE;
	else {
		if (se_metadata->top_field_first)
			buf->field = V4L2_FIELD_INTERLACED_TB;
		else
			buf->field = V4L2_FIELD_INTERLACED_BT;
	}

	out_metadata_p = (void*)buf->reserved;
	if (out_metadata_p != NULL) {
		ret = copy_to_user(out_metadata_p, &v4l2_metadata,
				sizeof(struct v4l2_video_uncompressed_metadata));
		if (ret) {
			DVB_ERROR("can't copy to user\n");
		}
	}
	return ret;
}

static int video_dqbuf_capture(struct stm_v4l2_input_description *input,
			       bool nonblocking, struct vb2_buffer *vb)
{
	struct stm_v4l2_video_type *video = input->priv;
	struct stm_v4l2_video_capture *capture;
	int ret;
	unsigned long payload_length;
	struct v4l2_buffer *buf = &vb->v4l2_buf;
	stm_se_capture_buffer_t * capture_buffer;

	capture = video->capture;

	if (!capture->destination.surface)
		return -EIO;

	ret = video_stream_read_memsink(capture, nonblocking);
	if (ret == -EAGAIN) {
		/* That isn't really an error */
		return ret;
	}

	if (ret)
		goto function_error;

	capture_buffer =
		(stm_se_capture_buffer_t *)capture->video_buf.intermediate_buffer_Virt;

	if (capture_buffer->payload_length != 0) {
		ret = video_stream_prepare_to_blit(capture);
		if (ret)
			goto function_error;

		ret = video_stream_execute_blit(capture);
		if (ret)
			goto function_error;

		/* set metadata in v4l2_buffer */
		ret = video_stream_set_metadata_in_v4l2_buf(capture, buf);
		if (ret)
			goto function_error;

		payload_length = capture->destination.buffer_dimension.h *
					capture->destination.pitch;
	} else
		payload_length = 0;

	/*
	 * Set the payload for the user application
	 */
	vb2_set_plane_payload(vb, 0, payload_length);

	/* Increase the grab_buffer_count */
	input->video_dec->stats.grab_buffer_count ++;

	return 0;

function_error:
/* There were an error, give back the queued buffer */
	stm_blitter_surface_put(capture->destination.surface);
	capture->destination.surface = NULL;
	return ret;
}

static int stm_v4l2_capture_usr_dqbuf(struct vb2_buffer *vb,
			struct stm_v4l2_ud *ud, bool nonblocking)
{
	unsigned int read_size = 0;
	int ret;

	/* We got a free buffer - copy into it */
	ret =
	    read_data_from_memsink(ud->memsink_interface, nonblocking,
				   vb2_plane_vaddr(vb, 0),
				   vb2_plane_size(vb, 0), &read_size);
	if (ret) {
		DVB_DEBUG("couldn't read properly (%d)\n",ret);
		return ret;
	}

	vb2_set_plane_payload(vb, 0, read_size);

	return ret;
}

/**
 * stm_v4l2_capture_queue_setup
 * Queue setup for all buffer types
 */
static int stm_v4l2_capture_queue_setup(struct vb2_queue *q,
		const struct v4l2_format *fmt, unsigned int *num_buffers,
		unsigned int *num_planes, unsigned int sizes[],
		void *alloc_ctxs[])
{
	struct stm_v4l2_input_description *input = vb2_get_drv_priv(q);
	int ret = 0;

	/*
	 * Set the number of planes and buffer
	 */
	switch((int)(q->type)) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	{
		struct stm_v4l2_video_type *video = input->priv;

		sizes[0] = video->capture->destination.buffer_dimension.h *
				video->capture->destination.pitch;
		*num_planes = 1;
		alloc_ctxs[0] = stm_v4l2_capture_dev.mem_bpa2_ctx;
		break;
	}

	/*
	 * No need for allocation context for audio/user-data as
	 * vb2-vmalloc ops doesn't require it
	 */
	case V4L2_BUF_TYPE_AUDIO_CAPTURE:
	{
		struct stm_v4l2_audio_type *audio = input->priv;
		struct v4l2_audio_format *fmt =
					&audio->capture->user_surface.format;

		sizes[0] = fmt->BitsPerSample *
				fmt->Channelcount * fmt->SampleRateHz / 8;
		*num_planes = 1;
		break;
	}

	case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
	{
		sizes[0] = USER_DATA_MAX_BUFFER_SIZE;
		*num_planes = 1;
		break;
	}

	default:
		ret = -EINVAL;
	}

	return ret;
}

/**
 * stm_v4l2_capture_wait_prepare() - wait for the buffer to arrive
 */
static void stm_v4l2_capture_wait_prepare(struct vb2_queue *q)
{
	struct stm_v4l2_input_description *input = vb2_get_drv_priv(q);

	/*
	 * Zero copy grab is a push model data capture, meaning that
	 * the decoder pushes the new buffer. So, we release the driver
	 * lock and wait in the vb2 core for the buffer to be pushed.
	 * NOTE: This lock is released only in case of zero-copy because
	 * DQBUF and buffer push are independent which is not the case
	 * with the other mechanism, in which case we pull data from
	 * decoder. Also, it is not expected that application will be
	 * using USERPTR and MMAP in the same capture and then push mmap
	 * and QBUF, thus resulting in a deadlock.
	 */
	if (zero_copy_enabled(input))
		mutex_unlock(&input->lock);
}

/**
 * stm_v4l2_capture_wait_finish() - finish wait as the buffer arrived
 */
static void stm_v4l2_capture_wait_finish(struct vb2_queue *q)
{
	struct stm_v4l2_input_description *input = vb2_get_drv_priv(q);

	/*
	 * Re-acquire the driver lock as the wait is now over.
	 */
	if (zero_copy_enabled(input))
		mutex_unlock(&input->lock);
}

/**
 * stm_v4l2_capture_buf_prepare
 * Prepare buffer for all buffer types
 */
static int stm_v4l2_capture_buf_prepare(struct vb2_buffer *vb)
{
	int ret = 0;
	struct vb2_queue *q = vb->vb2_queue;
	struct stm_v4l2_input_description *input = vb2_get_drv_priv(q);
	struct stm_v4l2_video_type *video = input->priv;
	struct stm_v4l2_video_capture *capture = video->capture;
	struct stm_v4l2_vb2_buffer *stm_vb2;

	/*
	 * Prepare buffer only if link is enabled
	 */
	if (!isInputLinkEnabled(input))
		return -EINVAL;

	switch((int)q->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (zero_copy_enabled(input)) {
			if (capture->zero.pool_idx < capture->zero.buf_provided) {
				stm_vb2 = kzalloc(sizeof(struct stm_v4l2_vb2_buffer), GFP_KERNEL);
				if (!stm_vb2) {
					DVB_ERROR("No memory for stm vb2 buffers\n");
					return -ENOMEM;
				}
				stm_vb2->vb2_buf = vb;
				list_add_tail(&stm_vb2->list, &input->vb2_vid.stm_vb2_vid.list);
			}
		} else {
			/*
			 * In non-zero copy video capture, audio/user-data capture,
			 * only a single buffer enqueue is supported, one is enqueued
			 * here and is deleted in streamoff.
			 */
			if (list_empty(&input->vb2_vid.stm_vb2_vid.list)) {
				stm_vb2 = kzalloc(sizeof(struct stm_v4l2_vb2_buffer),
						GFP_KERNEL);
				if (!stm_vb2) {
					DVB_ERROR("No memory for stm vb2 buffer\n");
					return -ENOMEM;
				}
				stm_vb2->vb2_buf = vb;
				list_add_tail(&stm_vb2->list, &input->vb2_vid.stm_vb2_vid.list);
			}
			ret = video_qbuf_capture(input, vb);
		}
		break;

	case V4L2_BUF_TYPE_AUDIO_CAPTURE:
	case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
	{
		struct list_head *list;

		switch((int)q->type) {
		case V4L2_BUF_TYPE_AUDIO_CAPTURE:
			list = &input->stm_vb2_audio.list;
			break;

		case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
			list = &input->vb2_vid.stm_vb2_usr.list;
			break;
		}

		if (list_empty(list)) {
			stm_vb2 = kzalloc(sizeof(struct stm_v4l2_vb2_buffer),
					GFP_KERNEL);
			if (!stm_vb2) {
				DVB_ERROR("No memory for stm vb2 buffer\n");
				return -ENOMEM;
			}
			stm_vb2->vb2_buf = vb;
			list_add_tail(&stm_vb2->list, list);
		}
		break;
	}

	default:
		ret = -EINVAL;
	}

	return ret;
}

/**
 * stm_v4l2_capture_buf_finish
 * Dequeueing the buffer using the callback will prohibit another call when
 * DQBUF is done in non-blocking mode and there's no data present
 */
static int stm_v4l2_capture_buf_finish(struct vb2_buffer *vb)
{
	struct vb2_queue *q = vb->vb2_queue;
	struct stm_v4l2_input_description *input = vb2_get_drv_priv(q);
	int ret = 0;

	if ((q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) &&
					zero_copy_enabled(input))
		ret = video_dqbuf_zc_capture(input,
			((input->vid_file->f_flags & O_NONBLOCK) == O_NONBLOCK),
			vb);

	return ret;
}

/**
 * stm_v4l2_capture_start_streaming
 * Start streaming for all buffer types
 */
static int stm_v4l2_capture_start_streaming(struct vb2_queue *q,
							unsigned int count)
{
	int ret = 0;
	struct stm_v4l2_input_description *input = vb2_get_drv_priv(q);

	/*
	 * Start streaming if link is enabled
	 */
	if (!isInputLinkEnabled(input))
		return -EINVAL;

	switch((int)(q->type)) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (zero_copy_enabled(input))
			ret = video_streamon_zc_capture(input);
		else
			ret = video_streamon_capture(input);
		break;

	case V4L2_BUF_TYPE_AUDIO_CAPTURE:
		ret = stm_v4l2_capture_aud_streamon(input);
		break;

	case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
		ret = stm_v4l2_capture_usr_streamon(input);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int stm_v4l2_capture_stop_streaming(struct vb2_queue *q)
{
	int ret = 0;
	struct stm_v4l2_input_description *input = vb2_get_drv_priv(q);

	/*
	 * Stream off if link is enabled
	 */
	if (!isInputLinkEnabled(input))
		return -EINVAL;

	switch((int)(q->type)) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (zero_copy_enabled(input))
			ret = video_streamoff_zc_capture(input);
		else
			ret = video_streamoff_capture(input);
		break;

	case V4L2_BUF_TYPE_AUDIO_CAPTURE:
		ret = stm_v4l2_capture_aud_streamoff(input);
		break;

	case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
		ret = stm_v4l2_capture_usr_streamoff(input);
		break;

	default:
		ret = -EINVAL;

	}

	return ret;
}

/**
 * stm_v4l2_capture_vid_buf_queue
 * At the moment, all the buffer types queueing can result in an error, so,
 * using this callback to indicate dqbuf that buffer has been processed.
 */
static void stm_v4l2_capture_vid_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *q = vb->vb2_queue;
	struct stm_v4l2_input_description *input = vb2_get_drv_priv(q);
	int ret = 0;

	if ((q->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) ||
					!zero_copy_enabled(input))
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
	else {
		ret = video_qbuf_zc_capture(input, vb);
		if (ret)
			printk(KERN_ERR "Queue buffer failed\n");
	}
}

/*
 * These are vb2 queue and the associated buffer operations
 */
static struct vb2_ops stm_v4l2_capture_vb2_ops = {
	.queue_setup = stm_v4l2_capture_queue_setup,
	.wait_prepare = stm_v4l2_capture_wait_prepare,
	.wait_finish = stm_v4l2_capture_wait_finish,
	.buf_prepare = stm_v4l2_capture_buf_prepare,
	.buf_finish = stm_v4l2_capture_buf_finish,
	.start_streaming = stm_v4l2_capture_start_streaming,
	.stop_streaming = stm_v4l2_capture_stop_streaming,
	.buf_queue = stm_v4l2_capture_vid_buf_queue,
};

/**
 * stm_v4l2_capture_querycap
 * VIDIOC_QUERYCAP
 */
static int stm_v4l2_capture_querycap(struct file *file, void *fh,
			   struct v4l2_capability *cap)
{
	strlcpy(cap->driver, "AV Decoder", sizeof(cap->driver));
	strlcpy(cap->card, "STMicroelectronics", sizeof(cap->card));
	strlcpy(cap->bus_info, "", sizeof(cap->bus_info));

	cap->version = LINUX_VERSION_CODE;
	cap->capabilities = (0
			| V4L2_CAP_VIDEO_OVERLAY
			| V4L2_CAP_VIDEO_CAPTURE
			| V4L2_CAP_STREAMING);

	return 0;
}

static int stm_v4l2_capture_enum_input(struct file *file, void *fh,
			     struct v4l2_input *inp)
{
	int index = inp->index;
	struct media_pad *local_pad;
	struct media_pad *remote_pad;
	struct v4l2_subdev *remote_sd;
	int id = 0;

	/* check consistency of index */
	if (index >= audio_decoder_offset)
		return -EINVAL;

	/* we need to set at least the std field to 0 since we don't have standard */
	memset(inp, 0, sizeof(struct v4l2_input));
	inp->index = index;

	/* Try to get remote sub-device */
	local_pad =
	    &stm_v4l2_capture_dev.pads[index +
					  video_decoder_offset];
	remote_pad =
	    stm_media_find_remote_pad(local_pad, 0, &id);
	if (remote_pad) {
		remote_sd =
		    media_entity_to_v4l2_subdev(remote_pad->
						entity);
		strlcpy(inp->name, remote_sd->name, sizeof(inp->name));
	}

	return 0;
}

static int stm_v4l2_capture_enumaudio(struct file *file, void *fh,
			     struct v4l2_audio *audio)
{
	unsigned int index = audio->index;
	struct media_pad *local_pad;
	struct media_pad *remote_pad;
	struct v4l2_subdev *remote_sd;
	int id = 0;

	/* check consistency of index */
	if (index >= nb_dec_audio)
		return -EINVAL;

	/* we need to set at least the std field to 0 since we don't have standard */
	memset(audio, 0, sizeof(struct v4l2_audio));
	audio->index = index;

	/* Try to get remote sub-device */
	local_pad =
	    &stm_v4l2_capture_dev.pads[index +
					  audio_decoder_offset];
	remote_pad =
	    stm_media_find_remote_pad(local_pad, 0, &id);
	if (remote_pad) {
		remote_sd =
		    media_entity_to_v4l2_subdev(remote_pad->
						entity);
		strlcpy(audio->name, remote_sd->name, sizeof(audio->name));
	}

	return 0;
}

static int stm_v4l2_capture_s_input(struct file *file, void *fh, unsigned int id)
{
	struct media_pad *local_pad, *remote_pad;
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input;

	/* Note: an invalid value is not ERROR unless the Registration */
	/* Driver Interface tells so. */
	if (id >= audio_decoder_offset)
		return -EINVAL;
	input =
	    &g_stm_v4l2_input_device[video_decoder_offset + id];

	if (mutex_lock_interruptible(&input->lock))
		return -EINTR;

	input->users++;
	if (input->users == 1) {
		/* first user - allocate handle for driver
		   registration */
		input->flags = STM_V4L2_VIDEO_CONTROL;
		input->priv =
		    (void *)
		    kzalloc(sizeof(struct stm_v4l2_video_type),
			    GFP_KERNEL);
		if (!input->priv) {
			DVB_ERROR("kmalloc failed\n");
			input->users--;
			mutex_unlock(&input->lock);
			return -ENODEV;
		}
		if(id < nb_dec_video) {
			input->input_source = STM_V4L2_DVB_VIDEO;
			input->video_dec =
			v4l2_input_to_VideoDvbContext(input);
			if (!input->video_dec) {
				mutex_unlock(&input->lock);
				return -ENODEV;
			}
		}
		else {
			/*handle the HDMI case here*/
			input->input_source = STM_V4L2_VIDEXTIN_VIDEO;
			local_pad = &stm_v4l2_capture_dev.pads[id];
			remote_pad = stm_media_find_remote_pad(local_pad, 0, &id);
			if (remote_pad){
				input->vidextin_handle = media_entity_to_v4l2_subdev(remote_pad->entity);
				mutex_unlock(&input->lock);
			}
		}
	}

	dvb_fh->input = input;
	((struct v4l2_fh *)fh)->ctrl_handler = &(input->video_dec->ctrl_handler);
	mutex_unlock(&input->lock);

	return 0;
}

static int STM_V4L2_FUNC(stm_v4l2_capture_s_audio,
		 struct file *file, void *fh, struct v4l2_audio *audio)
{
	unsigned int id = audio->index;
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input;

	/* Note: an invalid value is not ERROR unless the Registration */
	/* Driver Interface tells so. */
	if (id >= nb_dec_audio)
		return -EINVAL;
	input =
	    &g_stm_v4l2_input_device[audio_decoder_offset + id];

	if (mutex_lock_interruptible(&input->lock))
		return -EINTR;

	input->users++;
	if (input->users == 1) {
		/* first user - allocate handle for driver
		   registration */
		input->flags = STM_V4L2_AUDIO_CONTROL;
		input->priv =
		    (void *)
		    kzalloc(sizeof(struct stm_v4l2_audio_type),
			    GFP_KERNEL);
		if (!input->priv) {
			DVB_ERROR("kmalloc failed\n");
			input->users--;
			mutex_unlock(&input->lock);
			return -ENODEV;
		}

		input->audio_dec =
		    v4l2_input_to_AudioDvbContext(input);
		if (!input->audio_dec) {
			mutex_unlock(&input->lock);
			return -ENODEV;
		}

	}
	dvb_fh->input = input;
	((struct v4l2_fh *)fh)->ctrl_handler = &(input->audio_dec->ctrl_handler);
	mutex_unlock(&input->lock);

	return 0;
}

static int stm_v4l2_capture_g_audio(struct file *file, void *fh, struct v4l2_audio *audio)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;

	CHECK_INPUT(input, AUDIO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	memset(audio, 0, sizeof(struct v4l2_audio));
	audio->index = input->pad_id - audio_decoder_offset;

	mutex_unlock(&input->lock);

	return 0;
}

static int STM_V4L2_FUNC(stm_v4l2_capture_s_crop, struct file *file,
					void *fh, struct v4l2_crop *c)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	int ret;

	CHECK_INPUT(input, VIDEO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	switch (c->type) {
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		ret = video_s_crop_overlay(input, (const struct v4l2_crop *)c);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&input->lock);

	return ret;
}

static int STM_V4L2_FUNC1(stm_v4l2_capture_subscribe_events,
		 struct v4l2_fh *fh, struct v4l2_event_subscription *sub)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	int ret = 0;

	if (!input)
		return -EINVAL;

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	if(input->input_source == STM_V4L2_VIDEXTIN_VIDEO)
		ret = v4l2_subdev_call(input->vidextin_handle,core,subscribe_event  ,(struct v4l2_fh *)&dvb_fh->fh ,(struct v4l2_event_subscription *)sub);

	mutex_unlock(&input->lock);

	return ret;
}

static int STM_V4L2_FUNC1(stm_v4l2_capture_unsubscribe_events,
		 struct v4l2_fh *fh, struct v4l2_event_subscription *sub)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	int ret = 0;

	if (!input)
		return -EINVAL;

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	if(input->input_source == STM_V4L2_VIDEXTIN_VIDEO)
		ret = v4l2_subdev_call(input->vidextin_handle,core,unsubscribe_event  , (struct v4l2_fh *)&dvb_fh->fh ,(struct v4l2_event_subscription *)sub);

	mutex_unlock(&input->lock);

	return ret;
}

static int stm_v4l2_capture_g_crop(struct file *file, void *fh, struct v4l2_crop *c)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	int ret;

	CHECK_INPUT(input, VIDEO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	switch (c->type) {
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		ret = video_g_crop_overlay(input, c);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&input->lock);

	return ret;
}

static int stm_v4l2_capture_cropcap(struct file *file, void *fh, struct v4l2_cropcap *c)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	int ret;

	CHECK_INPUT(input, VIDEO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	switch (c->type) {
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		ret = video_cropcap_overlay(input, c);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&input->lock);

	return ret;
}

static int stm_v4l2_capture_vid_streamon(struct file *file,
						void *fh, unsigned int type)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_video_type *video;
	int ret;

	CHECK_INPUT(input, VIDEO);

	video = input->priv;

	CHECK_VIDEO_FMT_LOCKED(video, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = vb2_streamon(input->vq.vid_queue, type);

	mutex_unlock(&input->lock);

	return ret;
}

static int stm_v4l2_capture_streamoff(struct file *file, void *fh, unsigned int type)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_video_type *video;
	int ret;

	CHECK_INPUT(input, VIDEO);

	video = input->priv;

	CHECK_VIDEO_FMT_LOCKED(video, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = vb2_streamoff(input->vq.vid_queue, type);

	mutex_unlock(&input->lock);

	return ret;
}

/**
 * stm_v4l2_capture_enum_fmt_vid_cap
 * VIDIOC_ENUM_FMT for V4L2_BUF_TYPE_VIDEO_CAPTURE
 */
static int stm_v4l2_capture_enum_fmt_vid_cap(struct file *file,
					void *fh, struct v4l2_fmtdesc *fmt)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	int index;

	CHECK_INPUT(input, VIDEO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	if (fmt->index > SURF_END) {
		mutex_unlock(&input->lock);
		return -EINVAL;
	}

	/*
	 * Retain the index as requested by application
	 */
	index = fmt->index;
	memcpy(fmt, &(stm_blitter_v4l2_mapping_info[fmt->index].fmt),
					sizeof(struct v4l2_fmtdesc));
	fmt->index = index;
	fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	mutex_unlock(&input->lock);

	return 0;
}

/**
 * stm_v4l2_capture_try_fmt_vid_cap
 * VIDIOC_TRY_FMT callback for V4L2_BUF_TYPE_VIDEO_CAPTURE
 */
static int stm_v4l2_capture_try_fmt_vid_cap(struct file *file, void *fh,
				  struct v4l2_format *fmt)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	int ret;

	CHECK_INPUT(input, VIDEO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = video_try_fmt_capture(input, fmt);

	mutex_unlock(&input->lock);

	return ret;
}

/**
 * stm_v4l2_capture_s_fmt_vid_cap
 * VIDIOC_S_FMT for V4L2_BUF_TYPE_VIDEO_CAPTURE
 */
static int stm_v4l2_capture_s_fmt_vid_cap(struct file *file,
				void *fh, struct v4l2_format *fmt)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct vb2_queue *q;
	int ret = 0;

	CHECK_INPUT(input, VIDEO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = stm_capture_vid_s_fmt(fmt, input);
	if (ret) {
		DVB_ERROR("Cannot set format for video capture\n");
		goto s_fmt_vid_cap_done;
	}

	/*
	 * If the vb2 queue is already created then do nothing
	 */
	if (input->vq.vid_queue)
		goto s_fmt_vid_cap_done;

	/*
	 * Intitialize the vb2_queue for video-frame capture
	 */
	q = (struct vb2_queue *)kzalloc(sizeof(struct vb2_queue), GFP_KERNEL);
	if (!q) {
		DVB_ERROR("Failed to allocate memory for video queue\n");
		ret = -ENOMEM;
		goto s_fmt_vid_cap_done;
	}

	/*
	 * Setup the vb2 queue now
	 */
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_USERPTR | VB2_MMAP;
	q->ops = &stm_v4l2_capture_vb2_ops;
	q->mem_ops = &vb2_bpa2_contig_memops;
	q->drv_priv = input;
	q->buf_struct_size = 0; /* FIXME */

	ret = vb2_queue_init(q);
	if (ret) {
		DVB_ERROR("Video capture queue init failed\n");
		goto s_fmt_vid_cap_queue_init_failed;
	}
	input->vq.vid_queue = q;
	input->vid_file = file;

	mutex_unlock(&input->lock);

	return 0;

s_fmt_vid_cap_queue_init_failed:
	kfree(q);
s_fmt_vid_cap_done:
	mutex_unlock(&input->lock);
	return ret;
}

/**
 * stm_v4l2_capture_g_fmt_vid_cap
 * VIDIOC_G_FMT for V4L2_BUF_TYPE_VIDEO_CAPTURE
 */
static int stm_v4l2_capture_g_fmt_vid_cap(struct file *file,
				void *fh, struct v4l2_format *fmt)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_video_type *video;
	int ret = 0;

	CHECK_INPUT(input, VIDEO);

	video = input->priv;

	CHECK_VIDEO_FMT_LOCKED(video, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	/*
	 * Fill in the v4l2_format structure
	 */
	fmt->fmt.pix.width = video->capture->destination.rect.size.w;
	fmt->fmt.pix.height = video->capture->destination.rect.size.h;
	fmt->fmt.pix.pixelformat = stm_blitter_v4l2_mapping_info
				[video->capture->mapping_index].fmt.pixelformat;
	fmt->fmt.pix.bytesperline = video->capture->destination.pitch;
	fmt->fmt.pix.sizeimage = fmt->fmt.pix.bytesperline *
						fmt->fmt.pix.height;
	fmt->fmt.pix.field = 0;

	switch (video->capture->destination.cspace) {
		case STM_BLITTER_SCS_BT601:
			fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
			fmt->fmt.pix.priv = 0;
			break;

		case STM_BLITTER_SCS_BT601_FULLRANGE:
			fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
			fmt->fmt.pix.priv = V4L2_BUF_FLAG_FULLRANGE;
			break;

		case STM_BLITTER_SCS_BT709:
			fmt->fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
			fmt->fmt.pix.priv = 0;
			break;

		case STM_BLITTER_SCS_BT709_FULLRANGE:
			fmt->fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
			fmt->fmt.pix.priv = V4L2_BUF_FLAG_FULLRANGE;
			break;

		case STM_BLITTER_SCS_RGB:
			fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
			fmt->fmt.pix.priv = V4L2_BUF_FLAG_FULLRANGE;
			break;

		case STM_BLITTER_SCS_RGB_VIDEORANGE:
			fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
			fmt->fmt.pix.priv = 0;
			break;

		default:
			DVB_ERROR("Destination color space not set properly\n");
			ret = -EINVAL;
	}

	mutex_unlock(&input->lock);
	return ret;
}

static int stm_v4l2_capture_g_parm(struct file *file, void *fh,
				 struct v4l2_streamparm *gparm)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	int ret;

	CHECK_INPUT(input, VIDEO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	switch ((unsigned int)(gparm->type)) {

	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		ret = video_g_parm_capture(input, gparm);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&input->lock);

	return ret;
}
static int stm_v4l2_capture_vid_reqbufs(struct file *file,
					void *fh, struct v4l2_requestbuffers *p)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	int ret;

	CHECK_INPUT(input, VIDEO);

	/* Only valid if the link is enabled */
	if (!isInputLinkEnabled(input))
		return -EINVAL;

	ret = video_reqbufs_capture(input, p);

	return ret;
}

/**
 * stm_v4l2_capture_querybuf
 * Query the buffer info
 */
static int stm_v4l2_capture_querybuf(struct file *file,
					void *fh, struct v4l2_buffer *buf)
{
	struct stm_v4l2_dvb_fh *dvb_fh = fh;
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_video_type *video;
	int ret;

	CHECK_INPUT(input, VIDEO);

	/* Only valid if the link is enabled */
	if (!isInputLinkEnabled(input))
		return -EINVAL;

	video = input->priv;

	CHECK_VIDEO_FMT_LOCKED(video, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = vb2_querybuf(input->vq.vid_queue, buf);

	mutex_unlock(&input->lock);

	return ret;
}

static int stm_v4l2_capture_vid_qbuf(struct file *file,
					void *fh, struct v4l2_buffer *buf)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_video_type *video;
	int ret;

	CHECK_INPUT(input, VIDEO);

	video = input->priv;

	CHECK_VIDEO_FMT_LOCKED(video, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	ret = vb2_qbuf(input->vq.vid_queue, buf);

	mutex_unlock(&input->lock);

	return ret;
}


static int stm_v4l2_capture_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct stm_v4l2_video_type *video;
	int ret = 0;

	CHECK_INPUT(input, VIDEO);

	/*
	 * DQBUF valid if link is enabled
	 */
	if (!isInputLinkEnabled(input))
		return -EINVAL;

	video = input->priv;

	CHECK_VIDEO_FMT_LOCKED(video, &input->lock);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	if (zero_copy_enabled(input)) {
		ret = vb2_dqbuf(input->vq.vid_queue, buf,
				((file->f_flags & O_NONBLOCK) == O_NONBLOCK));
	} else {
		struct vb2_buffer *vb;
		vb = list_first_entry(&input->vb2_vid.stm_vb2_vid.list,
				struct stm_v4l2_vb2_buffer, list)->vb2_buf;
		if (!vb || !vb2_is_streaming(vb->vb2_queue)) {
			ret = -EINVAL;
			goto dqbuf_done;
		}
		vb->v4l2_buf.reserved = buf->reserved;
		ret = video_dqbuf_capture(input,
				((file->f_flags & O_NONBLOCK) == O_NONBLOCK),
				vb);
		if (!ret)
			ret = vb2_dqbuf(input->vq.vid_queue, buf,
				((file->f_flags & O_NONBLOCK) == O_NONBLOCK));
	}

dqbuf_done:
	mutex_unlock(&input->lock);
	return ret;
}

/**
 * stm_v4l2_capture_g_input (VIDIOC_G_INPUT callback)
 */
static int stm_v4l2_capture_g_input(struct file *file,
					void *fh, unsigned int *i)
{
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;

	CHECK_INPUT(input, VIDEO);

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	*i = input->pad_id - video_decoder_offset;

	mutex_unlock(&input->lock);

	return 0;
}

int stm_v4l2_capture_s_dv_timings(struct file *file, void *fh,
			    struct v4l2_dv_timings *timings)
{
	struct stm_v4l2_dvb_fh *dvb_fh = fh;
	struct stm_v4l2_input_description *input = dvb_fh->input;

	if (!input) {
		DVB_ERROR("Input not set, call VIDIOC_S_INPUT\n");
		return -EINVAL;
	}

	if(input->input_source != STM_V4L2_VIDEXTIN_VIDEO) {
		DVB_ERROR("s_dv_timings defined only for vid-extinputs\n");
		return -EINVAL;
	}

	return v4l2_subdev_call(input->vidextin_handle, video, s_dv_timings, timings);
}

int stm_v4l2_capture_g_dv_timings(struct file *file, void *fh,
			    struct v4l2_dv_timings *timings)
{
	struct stm_v4l2_dvb_fh *dvb_fh = fh;
	struct stm_v4l2_input_description *input = dvb_fh->input;

	if (!input) {
		DVB_ERROR("Input not set, call VIDIOC_S_INPUT\n");
		return -EINVAL;
	}

	if(input->input_source != STM_V4L2_VIDEXTIN_VIDEO) {
		DVB_ERROR("g_dv_timings defined only for vid-extinputs\n");
		return -EINVAL;
	}

	return v4l2_subdev_call(input->vidextin_handle, video, g_dv_timings, timings);
}

int stm_v4l2_capture_g_fbuf_vid_overlay(struct file *file, void *fh,
				struct v4l2_framebuffer *a)
{
	struct stm_v4l2_dvb_fh *dvb_fh = fh;
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct v4l2_mbus_framefmt fmt = {0};
	int ret = 0;
	if (!input) {
		DVB_ERROR("Input not set, call VIDIOC_S_INPUT\n");
		return -EINVAL;
	}

	if(input->input_source != STM_V4L2_VIDEXTIN_VIDEO) {
		DVB_ERROR("G_FBUF defined only for vid-extinputs\n");
		return -EINVAL;
	}

	a->capability = V4L2_FBUF_CAP_EXTERNOVERLAY;
	a->flags = 0;
	a->base = 0; /* For non destructive fb, this is 0*/
	a->fmt.bytesperline = 0;
	a->fmt.sizeimage = 0;
	ret = v4l2_subdev_call(input->vidextin_handle, video, g_mbus_fmt, &fmt);
	if(ret)
		return ret;

	a->fmt.width = fmt.width;
	a->fmt.height = fmt.height;
	a->fmt.colorspace = fmt.colorspace;
	a->fmt.field = fmt.field;

	switch(fmt.code) {
	case V4L2_MBUS_FMT_STM_RGB8_3x8:
		a->fmt.pixelformat = V4L2_PIX_FMT_RGB24;
		break;
	case V4L2_MBUS_FMT_YUYV8_2X8:
		a->fmt.pixelformat = V4L2_PIX_FMT_YUYV;
		break;
	case V4L2_MBUS_FMT_STM_YUV8_3X8:
		a->fmt.pixelformat = V4L2_PIX_FMT_YUV444;
		break;
	default:
		DVB_ERROR("undefined pix code\n");
		return -EINVAL;
	}
	return 0;
}

static int STM_V4L2_FUNC(stm_v4l2_capture_s_fbuf_vid_overlay,
		 struct file *file, void *fh, struct v4l2_framebuffer *a)
{
	struct stm_v4l2_dvb_fh *dvb_fh = fh;
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct v4l2_mbus_framefmt fmt = {0};

	if (!input) {
		DVB_ERROR("Input not set, call VIDIOC_S_INPUT\n");
		return -EINVAL;
	}

	if(input->input_source != STM_V4L2_VIDEXTIN_VIDEO) {
		DVB_ERROR("S_FBUF defined only for vid-extinputs\n");
		return -EINVAL;
	}

	if(a->flags || a->base || a->fmt.bytesperline || a->fmt.sizeimage){
		DVB_ERROR("not supported in S_FBUF\n");
		return -ENOTSUP;
	}

	fmt.width = a->fmt.width ;
	fmt.height = a->fmt.height;
	fmt.colorspace = a->fmt.colorspace;
	fmt.field = a->fmt.field;

	switch(a->fmt.pixelformat) {
	case V4L2_PIX_FMT_RGB24: /* fall through */
	case V4L2_PIX_FMT_BGR24:
		fmt.code = V4L2_MBUS_FMT_STM_RGB8_3x8;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0))
		a->fmt.pixelformat = V4L2_PIX_FMT_RGB24;
#endif
		break;
	case V4L2_PIX_FMT_YUYV:/* fall through */
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		fmt.code = V4L2_MBUS_FMT_YUYV8_2X8;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0))
		a->fmt.pixelformat = V4L2_PIX_FMT_YUYV;
#endif
		break;
	case V4L2_PIX_FMT_YUV444:
		fmt.code = V4L2_MBUS_FMT_STM_YUV8_3X8;
		break;
	default:
		DVB_ERROR("undefined pixel format\n");
		return -EINVAL;
	}
	return v4l2_subdev_call(input->vidextin_handle, video, s_mbus_fmt, &fmt);
}

/*
 * These are V4L2 AV decoder capture ioctls
 */
struct v4l2_ioctl_ops stm_v4l2_capture_ioctl_ops = {
	.vidioc_querycap = stm_v4l2_capture_querycap,
	.vidioc_enum_fmt_vid_cap = stm_v4l2_capture_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = stm_v4l2_capture_g_fmt_vid_cap,
	.vidioc_g_fmt_vid_overlay = stm_v4l2_capture_g_fmt_vid_overlay,
	.vidioc_s_fmt_vid_cap = stm_v4l2_capture_s_fmt_vid_cap,
	.vidioc_s_fmt_vid_overlay = stm_v4l2_capture_s_fmt_vid_overlay,
	.vidioc_try_fmt_vid_cap = stm_v4l2_capture_try_fmt_vid_cap,
	.vidioc_reqbufs = stm_v4l2_capture_vid_reqbufs,
	.vidioc_querybuf = stm_v4l2_capture_querybuf,
	.vidioc_qbuf = stm_v4l2_capture_vid_qbuf,
	.vidioc_dqbuf = stm_v4l2_capture_dqbuf,
	.vidioc_streamon = stm_v4l2_capture_vid_streamon,
	.vidioc_streamoff = stm_v4l2_capture_streamoff,
	.vidioc_enum_input = stm_v4l2_capture_enum_input,
	.vidioc_g_input = stm_v4l2_capture_g_input,
	.vidioc_s_input = stm_v4l2_capture_s_input,
	.vidioc_enumaudio = stm_v4l2_capture_enumaudio,
	.vidioc_g_audio = stm_v4l2_capture_g_audio,
	.vidioc_s_audio = stm_v4l2_capture_s_audio,
	.vidioc_g_crop = stm_v4l2_capture_g_crop,
	.vidioc_s_crop = stm_v4l2_capture_s_crop,
	.vidioc_cropcap = stm_v4l2_capture_cropcap,
	.vidioc_g_parm = stm_v4l2_capture_g_parm,
	.vidioc_s_dv_timings = stm_v4l2_capture_s_dv_timings,
	.vidioc_g_dv_timings = stm_v4l2_capture_g_dv_timings,
	.vidioc_g_fbuf = stm_v4l2_capture_g_fbuf_vid_overlay,
	.vidioc_s_fbuf = stm_v4l2_capture_s_fbuf_vid_overlay,
	.vidioc_subscribe_event = stm_v4l2_capture_subscribe_events,
	.vidioc_unsubscribe_event = stm_v4l2_capture_unsubscribe_events,
};

/**
 * stm_v4l2_capture_private
 * This is a callback from video_usercopy to handle non-standard buffer type
 */
static long stm_v4l2_capture_private(struct file *file,
					unsigned int cmd, void *arg)
{
	struct stm_v4l2_dvb_fh *dvb_fh = file->private_data;
	struct stm_v4l2_input_description *input = dvb_fh->input;
	void *fh = file->private_data;
	int type = 0;
	long ret = 0;

	switch(cmd) {
	case VIDIOC_G_FMT:
	{
		struct v4l2_format *fmt = arg;
		type = fmt->type;

		switch(type) {
		case V4L2_BUF_TYPE_AUDIO_CAPTURE:
			ret = stm_v4l2_capture_g_fmt_aud_cap(file, fh, fmt);
			break;

		case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
			/*
			 * There's nothing to do here, so, return no error
			 */
			break;

		default:
			break;
		}

		break;
	}

	case VIDIOC_S_FMT:
	{
		struct v4l2_format *fmt = arg;
		type = fmt->type;

		switch(type) {
		case V4L2_BUF_TYPE_AUDIO_CAPTURE:
			ret = stm_v4l2_capture_s_fmt_aud_cap(file, fh, fmt);
			break;

		case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
			ret = stm_v4l2_capture_s_fmt_usr_cap(file, fh, fmt);
			break;

		default:
			break;
		}

		break;
	}

	case VIDIOC_TRY_FMT:
	{
		struct v4l2_format *fmt = arg;
		type = fmt->type;

		switch(type) {
		case V4L2_BUF_TYPE_AUDIO_CAPTURE:
			ret = -EINVAL;
			break;

		case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
			break;

		default:
			break;
		}

		break;
	}

	case VIDIOC_REQBUFS:
	{
		struct v4l2_requestbuffers *buf = arg;
		type = buf->type;

		switch(type) {
		case V4L2_BUF_TYPE_AUDIO_CAPTURE:
			ret = stm_v4l2_capture_aud_reqbufs(file, fh, buf);
			break;

		case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
			ret = stm_v4l2_capture_usr_reqbufs(file, fh, buf);
			break;

		default:
			break;
		}

		break;
	}

	case VIDIOC_QUERYBUF:
	{
		struct v4l2_buffer *buf = arg;
		type = buf->type;

		switch(type) {
		case V4L2_BUF_TYPE_AUDIO_CAPTURE:
		{
			struct stm_v4l2_audio_type *audio;

			CHECK_INPUT(input, AUDIO);

			audio = input->priv;

			CHECK_AUDIO_FMT_LOCKED(audio, &input->lock);

			if (mutex_lock_interruptible(&input->lock))
				return -ERESTARTSYS;

			ret = vb2_querybuf(input->aud_queue, buf);

			mutex_unlock(&input->lock);

			break;
		}

		case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
		{
			struct stm_v4l2_video_type *video;

			CHECK_INPUT(input, USER);

			video = input->priv;

			CHECK_USER_FMT_LOCKED(video, &input->lock);

			if (mutex_lock_interruptible(&input->lock))
				return -ERESTARTSYS;

			ret = vb2_querybuf(input->vq.usr_queue, buf);

			mutex_unlock(&input->lock);

			break;
		}

		default:
			break;
		}

		break;
	}

	case VIDIOC_QBUF:
	{
		struct v4l2_buffer *buf = arg;
		type = buf->type;

		switch(type) {
		case V4L2_BUF_TYPE_AUDIO_CAPTURE:
			ret = stm_v4l2_capture_aud_qbuf(file, fh, buf);
			break;

		case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
			ret = stm_v4l2_capture_usr_qbuf(file, fh, buf);
			break;

		default:
			break;
		}

		break;
	}

	case VIDIOC_DQBUF:
	{
		struct v4l2_buffer *buf = arg;
		struct vb2_buffer *vb;

		type = buf->type;

		/*
		 * DQBUF valid if link is enabled
		 */
		if (!isInputLinkEnabled(input))
			return -EINVAL;

		switch(type) {
		case V4L2_BUF_TYPE_AUDIO_CAPTURE:
		{
			struct stm_v4l2_audio_type *audio;

			CHECK_INPUT(input, AUDIO);

			audio = input->priv;

			CHECK_AUDIO_FMT_LOCKED(audio, &input->lock);

			if (mutex_lock_interruptible(&input->lock))
				return -ERESTARTSYS;

			vb = list_first_entry(&input->stm_vb2_audio.list,
				struct stm_v4l2_vb2_buffer, list)->vb2_buf;

			if (!vb || !vb2_is_streaming(vb->vb2_queue)) {
				mutex_unlock(&input->lock);
				return -EINVAL;
			}
			vb->v4l2_buf.reserved = buf->reserved;

			ret = stm_v4l2_capture_aud_dqbuf(vb, audio->capture,
					((file->f_flags & O_NONBLOCK) == O_NONBLOCK));
			if (!ret) {
				ret = vb2_dqbuf(input->aud_queue, buf,
					((file->f_flags & O_NONBLOCK) == O_NONBLOCK));

				/*
				 * Increase the grabbed buffer count
				 */
				if (!ret)
					input->audio_dec->stats.grab_buffer_count++;
			}

			mutex_unlock(&input->lock);

			break;
		}

		case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
		{
			struct stm_v4l2_video_type *video;

			CHECK_INPUT(input, USER);

			video = input->priv;

			CHECK_USER_FMT_LOCKED(video, &input->lock);

			if (mutex_lock_interruptible(&input->lock))
				return -ERESTARTSYS;

			vb = list_first_entry(&input->vb2_vid.stm_vb2_usr.list,
				struct stm_v4l2_vb2_buffer, list)->vb2_buf;

			if (!vb || !vb2_is_streaming(vb->vb2_queue)) {
				mutex_unlock(&input->lock);
				return -EINVAL;
			}

			ret = stm_v4l2_capture_usr_dqbuf(vb, video->ud,
					((file->f_flags & O_NONBLOCK) == O_NONBLOCK));
			if (!ret)
				ret = vb2_dqbuf(input->vq.usr_queue, buf,
					((file->f_flags & O_NONBLOCK) == O_NONBLOCK));

			mutex_unlock(&input->lock);

			break;
		}

		default:
			break;
		}

		break;
	}

	case VIDIOC_STREAMON:
	{
		type = *(unsigned int *)arg;

		switch(type) {
		case V4L2_BUF_TYPE_AUDIO_CAPTURE:
		{
			struct stm_v4l2_audio_type *audio;

			CHECK_INPUT(input, AUDIO);

			audio = input->priv;

			CHECK_AUDIO_FMT_LOCKED(audio, &input->lock);

			if (mutex_lock_interruptible(&input->lock))
				return -ERESTARTSYS;

			ret = vb2_streamon(input->aud_queue, type);

			mutex_unlock(&input->lock);

			break;
		}

		case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
		{
			struct stm_v4l2_video_type *video;

			CHECK_INPUT(input, USER);

			video = input->priv;

			CHECK_USER_FMT_LOCKED(video, &input->lock);

			if (mutex_lock_interruptible(&input->lock))
				return -ERESTARTSYS;

			ret = vb2_streamon(input->vq.usr_queue, type);

			mutex_unlock(&input->lock);

			break;
		}

		default:
			break;
		}

		break;
	}

	case VIDIOC_STREAMOFF:
	{
		type = *(unsigned int *)arg;
		switch(type) {
		case V4L2_BUF_TYPE_AUDIO_CAPTURE:
		{
			struct stm_v4l2_audio_type *audio;

			CHECK_INPUT(input, AUDIO);

			audio = input->priv;

			CHECK_AUDIO_FMT_LOCKED(audio, &input->lock);

			if (mutex_lock_interruptible(&input->lock))
				return -ERESTARTSYS;

			ret = vb2_streamoff(input->aud_queue, V4L2_BUF_TYPE_AUDIO_CAPTURE);

			mutex_unlock(&input->lock);

			break;
		}

		case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
		{
			struct stm_v4l2_video_type *video;

			CHECK_INPUT(input, USER);

			video = input->priv;

			CHECK_USER_FMT_LOCKED(video, &input->lock);

			if (mutex_lock_interruptible(&input->lock))
				return -ERESTARTSYS;

			ret = vb2_streamoff(input->vq.usr_queue, V4L2_BUF_TYPE_USER_DATA_CAPTURE);

			mutex_unlock(&input->lock);

			break;
		}

		default:
			break;
		}

		break;
	}

	default:
		DVB_ERROR("Invalid ioctl passed for processing\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * stm_v4l2_capture_ioctl
 * This is the first ioctl to happen on user-action. At the moment, video_ioctl2
 * cannot be used for all buffer types, so, this function is going to parse the
 * buffer type and call appropriate ioctl
 */
static long stm_v4l2_capture_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	enum v4l2_buf_type type = 0;

	/*
	 * Either we can write the whole part of video_usercopy here
	 * or just check the buffer type and decide which to call
	 * video_usercopy or video_ioctl2. Latter is used.
	 */

	switch(cmd) {
	case VIDIOC_G_FMT:
	case VIDIOC_S_FMT:
	case VIDIOC_TRY_FMT:
	{
		struct v4l2_format fmt;

		if (copy_from_user(&fmt, (void *)arg, sizeof(struct v4l2_format))) {
			DVB_ERROR("Failed to copy data from user\n");
			ret = -EINVAL;
			goto capture_ioctl_done;
		}

		type = fmt.type;
		break;
	}

	case VIDIOC_REQBUFS:
	{
		struct v4l2_requestbuffers buf;

		if (copy_from_user(&buf, (void *)arg,
				sizeof(struct v4l2_requestbuffers))) {
			DVB_ERROR("Failed to copy data from user\n");
			ret = -EINVAL;
			goto capture_ioctl_done;
		}

		type = buf.type;
		break;
	}

	case VIDIOC_QBUF:
	case VIDIOC_DQBUF:
	case VIDIOC_QUERYBUF:
	{
		struct v4l2_buffer buf;

		if (copy_from_user(&buf, (void *)arg,
				sizeof(struct v4l2_buffer))) {
			DVB_ERROR("Failed to copy data from user\n");
			ret = -EINVAL;
			goto capture_ioctl_done;
		}

		type = buf.type;
		break;
	}

	case VIDIOC_STREAMON:
	case VIDIOC_STREAMOFF:
	{
		if (copy_from_user(&type, (void *)arg,
				sizeof(enum v4l2_buf_type))) {
			DVB_ERROR("Failed to copy data from user\n");
			ret = -EINVAL;
			goto capture_ioctl_done;
		}

		break;
	}

	/*
	 * Where the buffer type determination is not required, simply
	 * pass control over to video_ioctl2
	 */
	default:
		ret = video_ioctl2(file, cmd, arg);
		goto capture_ioctl_done;
	}

	/*
	 * In the above switch case, we are going to determine the buffer type
	 * and decide what to do.
	 */
	switch((int)type) {
	case V4L2_BUF_TYPE_AUDIO_CAPTURE:
	case V4L2_BUF_TYPE_USER_DATA_CAPTURE:
		ret = video_usercopy(file, cmd, arg, stm_v4l2_capture_private);
		break;

	default:
		ret = video_ioctl2(file, cmd, arg);
		break;
	}

capture_ioctl_done:
	return ret;
}

/**
 * stm_v4l2_capture_video_close
 */
static void stm_v4l2_capture_video_close(void *context)
{
	struct stm_v4l2_input_description *input = context;
	struct stm_v4l2_video_type *video = input->priv;
	int ret = 0;
	struct list_head *plist;

	/*
	 * If the device is only opened, let's say for QUERYCAP,
	 * there's no video context, so, return
	 */
	if (!video)
		return;


	/*
	* Internal vb2 list is maintained for buffer maintaince; if the list
	* is not empty, then clean it up
	*/
	plist = &input->vb2_vid.stm_vb2_vid.list;

	DELETE_VBLIST(plist);

	/*
	 * Clean up all the video capture
	 */
	if (video->capture) {


		if (video->capture->source.surface)
			stm_blitter_surface_put(video->capture->source.surface);

		if (video->capture->destination.surface)
			stm_blitter_surface_put(video->capture->
						destination.surface);

		stm_blitter_put(video->capture->blitter);

		if (zero_copy_enabled(input)) {

			if (vb2_is_streaming(input->vq.vid_queue))
				ret = vb2_streamoff(input->vq.vid_queue,
						V4L2_BUF_TYPE_VIDEO_CAPTURE);


		} else {

			if (vb2_is_streaming(input->vq.vid_queue))
				ret = vb2_streamoff(input->vq.vid_queue,
						V4L2_BUF_TYPE_VIDEO_CAPTURE);

		}

		vb2_queue_release(input->vq.vid_queue);
		kfree(input->vq.vid_queue);
		input->vq.vid_queue = NULL;

		if (ret)
			DVB_ERROR("Video capture streamoff failed(%d)\n", ret);

		kfree(video->capture);
		video->capture = NULL;

	}

}

/**
 * stm_v4l2_capture_userdata_close
 */
static void stm_v4l2_capture_userdata_close(void *context)
{
	struct stm_v4l2_input_description *input = context;
	struct stm_v4l2_video_type *video = input->priv;
	int ret = 0;
	struct list_head *plist;

	/*
	 * If the device is only opened, let's say for QUERYCAP,
	 * there's no video context, so, return
	 */
	if (!video)
		return;

	/*
	* Internal vb2 list is maintained for buffer maintaince; if the list
	* is not empty, then clean it up
	*/
	plist = &input->vb2_vid.stm_vb2_usr.list;

	DELETE_VBLIST(plist);

	/*
	 * Clean up all the userdata
	 */
	if (video->ud) {

		if (vb2_is_streaming(input->vq.usr_queue))
			ret = vb2_streamoff(input->vq.usr_queue,
					V4L2_BUF_TYPE_USER_DATA_CAPTURE);
		if (ret)
			DVB_ERROR("User-data capture streamoff failed: %d\n", ret);

		vb2_queue_release(input->vq.usr_queue);
		kfree(input->vq.usr_queue);
		input->vq.usr_queue = NULL;

		kfree(video->ud);
		video->ud = NULL;
	}

}

/**
 * stm_v4l2_capture_audio_close
 * Cleanup the audio context
 */
static void stm_v4l2_capture_audio_close(void *context)
{
	struct stm_v4l2_input_description *input = context;
	struct stm_v4l2_audio_type *audio = input->priv;
	int ret = 0;
	struct list_head *plist;

	/*
	 * If there's no audio, then, driver is not configured for any
	 * audio operation, nothing to do
	 */
	if (!audio)
		return;

	/*
	* Internal vb2 list is maintained for buffer maintaince; if the list
	* is not empty, then clean it up
	*/
	plist = &input->stm_vb2_audio.list;

	DELETE_VBLIST(plist);
	/*
	 * Clean up the audio capture context
	 */
	if (audio->capture) {


		if (vb2_is_streaming(input->aud_queue))
			ret = vb2_streamoff(input->aud_queue,
					V4L2_BUF_TYPE_AUDIO_CAPTURE);
		if (ret)
			DVB_ERROR("Audio capture streamoff failed: %d\n", ret);

		vb2_queue_release(input->aud_queue);
		kfree(input->aud_queue);
		input->aud_queue = NULL;

		kfree(audio->capture);
		audio->capture = NULL;
	}

}

/**
 * stm_v4l2_capture_mmap
 * mmap the buffer for application
 */
static int stm_v4l2_capture_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct stm_v4l2_dvb_fh *dvb_fh = file->private_data;
	struct stm_v4l2_input_description *input = dvb_fh->input;
	struct vb2_queue *q;
	int ret;

	if (!input)
		return -EINVAL;

	if (mutex_lock_interruptible(&input->lock))
		return -ERESTARTSYS;

	if (input->aud_file == file)
		q = input->aud_queue;
	else if (input->vid_file == file)
		q = input->vq.vid_queue;
	else if (input->usr_file ==file)
		q = input->vq.usr_queue;
	else {
		ret = -EINVAL;
		goto mmap_done;
	}

	if (!q) {
		ret = -EINVAL;
		goto mmap_done;
	}

	ret = vb2_mmap(q, vma);

mmap_done:
	mutex_unlock(&input->lock);
	return ret;
}

/**
 * stm_v4l2_capture_close
 * Capture device v4l2 close operation. This stops the associated
 * audio/video/user streaming and performs the cleanup
 */
static int stm_v4l2_capture_close(struct file *file)
{
	struct v4l2_fh *fh = file->private_data;
	struct stm_v4l2_dvb_fh *dvb_fh = container_of(fh, struct stm_v4l2_dvb_fh, fh);
	struct stm_v4l2_input_description *input = dvb_fh->input;

	/*
	 * If a simple, open and close operation is done on this
	 * device, then there won't be any input, so, return
	 */
	if (!input) {
		v4l2_fh_del(fh);
		v4l2_fh_exit(fh);
		kfree(dvb_fh);
		return 0;
	}

	mutex_lock(&input->lock);

	if (input->aud_file == file)
		stm_v4l2_capture_audio_close((void *)input);
	if(input->vid_file == file)
		stm_v4l2_capture_video_close((void *)input);
	else if(input->usr_file == file)
		stm_v4l2_capture_userdata_close((void *)input);

	input->users--;
	if (input->users == 0) {
		kfree(input->priv);
		input->priv = NULL;
	}
	/*
	 * FIXME: There is going to be a race in multiple threads
	 * because the serialization mutex from video device init
	 * is missing
	 */

	/*
	 * Remove V4L2 file handlers
	 */
	v4l2_fh_del(fh);
	v4l2_fh_exit(fh);
	kfree(dvb_fh);
	file->private_data = NULL;
	mutex_unlock(&input->lock);
	return 0;
}

static int stm_v4l2_capture_open(struct file *file)
{
	struct video_device *viddev = &stm_v4l2_capture_dev.viddev;
	struct stm_v4l2_dvb_fh *dvb_fh;

	/* Allocate memory */
	dvb_fh = kzalloc(sizeof(struct stm_v4l2_dvb_fh), GFP_KERNEL);
	if (NULL == dvb_fh) {
		printk("%s: nomem on v4l2 open\n", __FUNCTION__);
		return -ENOMEM;
	}
	v4l2_fh_init(&dvb_fh->fh, viddev);
	file->private_data = &dvb_fh->fh;
	dvb_fh->input = NULL;
	v4l2_fh_add(&dvb_fh->fh);
	return 0;
}

static unsigned int
	stm_v4l2_capture_poll(struct file *file, struct poll_table_struct *wait)
{
	unsigned int res = 0;
	struct v4l2_fh *fh = file->private_data;

	if (v4l2_event_pending(fh))
		res |= POLLPRI;
	else
		poll_wait(file, &fh->wait, wait);
	return res;
}

static struct v4l2_file_operations stm_v4l2_capture_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = stm_v4l2_capture_ioctl,
	.mmap = stm_v4l2_capture_mmap,
	.open = stm_v4l2_capture_open,
	.release = stm_v4l2_capture_close,
	.poll = stm_v4l2_capture_poll,
};

static void stm_v4l2_capture_vdev_release(struct video_device *vdev)
{
	/* Nothing to do, but need by V4L2 stack */
}

/*
 * Media Operations
 */
static int stm_v4l2_capture_video_link_setup(struct media_entity *entity,
					     const struct media_pad *local,
					     const struct media_pad *remote,
					     u32 flags)
{
	return 0;
}

static const struct media_entity_operations stm_v4l2_capture_media_ops = {
	.link_setup = stm_v4l2_capture_video_link_setup,
};

int stm_v4l2_capture_init(void)
{
	struct video_device *viddev = &stm_v4l2_capture_dev.viddev;
	struct media_pad *pads;
	int ret, i, nb_extip_dev = 0;
	struct media_entity *entity;
	struct media_entity *sink;
	unsigned int flags = MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED;
	struct bpa2_part *bpa2_part;

	/* Count the number of VIDEO_DECODER entities and AUDIO_DECODER entities */
	video_decoder_offset = 0;
	entity = stm_media_find_entity_with_type_first
				(MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER);
	while (entity) {
		nb_dec_video++;
		entity = stm_media_find_entity_with_type_next(entity,
					      MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER);
	}

	/* Count the number of hdmirx entities and vidextin entities */
	entity =
	    stm_media_find_entity_with_type_first
	    (MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX);
	while (entity) {
		nb_extip_dev++;
		entity = stm_media_find_entity_with_type_next(entity,
							      MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX);
	}
	entity =
	    stm_media_find_entity_with_type_first
	    (MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_HDMIRX);
	while (entity) {
		nb_extip_dev++;
		entity = stm_media_find_entity_with_type_next(entity,
							      MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_HDMIRX);
	}

	/* Count the number of vxi entities and vidextin vxi entities */
	entity =
	    stm_media_find_entity_with_type_first
	    (MEDIA_ENT_T_V4L2_SUBDEV_VXI);
	while (entity) {
		nb_extip_dev++;
		entity = stm_media_find_entity_with_type_next(entity,
							      MEDIA_ENT_T_V4L2_SUBDEV_VXI);
	}
	entity =
	    stm_media_find_entity_with_type_first
	    (MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_VXI);
	while (entity) {
		nb_extip_dev++;
		entity = stm_media_find_entity_with_type_next(entity,
							      MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_VXI);
	}

	audio_decoder_offset = nb_dec_video + nb_extip_dev;
	entity =
	    stm_media_find_entity_with_type_first
	    (MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER);
	while (entity) {
		nb_dec_audio++;
		entity = stm_media_find_entity_with_type_next(entity,
					      MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER);
	}

	if (nb_dec_video + nb_extip_dev + nb_dec_audio == 0) {
		ret = -ENODEV;
		printk(KERN_ERR "%s: failed to find entities\n", __func__);
		goto failed_input_table_alloc;
	}

	/* Initialize the stm_v4l2_input_description table */
	g_stm_v4l2_input_device =
	    kzalloc(sizeof(struct stm_v4l2_input_description) *
		    (nb_dec_video + nb_extip_dev + nb_dec_audio), GFP_KERNEL);
	if (!g_stm_v4l2_input_device) {
		printk(KERN_ERR
		       "%s: failed to allocate memory for stm_v4l2_input_description\n",
		       __func__);
		ret = -ENOMEM;
		goto failed_input_table_alloc;
	}

	/* Allocate the pad table */
	stm_v4l2_capture_dev.pads =
	    kzalloc(sizeof(struct media_pad) * (nb_dec_video + nb_extip_dev + nb_dec_audio),
		    GFP_KERNEL);
	if (!stm_v4l2_capture_dev.pads) {
		printk(KERN_ERR "%s: failed to allocate memory for pads\n",
		       __func__);
		ret = -ENOMEM;
		goto failed_pads_table_alloc;
	}
	pads = stm_v4l2_capture_dev.pads;

	for (i = 0; i < (nb_dec_audio + nb_extip_dev + nb_dec_video); i++) {
		mutex_init(&g_stm_v4l2_input_device[i].lock);
		g_stm_v4l2_input_device[i].pad_id = i;
		INIT_LIST_HEAD(&g_stm_v4l2_input_device[i].vb2_vid.stm_vb2_vid.list);
		INIT_LIST_HEAD(&g_stm_v4l2_input_device[i].vb2_vid.stm_vb2_usr.list);
	}

	/* Initialize the capture video device */
	strncpy(viddev->name, "STM Grab Overlay", sizeof(viddev->name));
	viddev->fops = &stm_v4l2_capture_fops;
	viddev->minor = -1;
	viddev->release = stm_v4l2_capture_vdev_release;
	viddev->ioctl_ops = &stm_v4l2_capture_ioctl_ops;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	viddev->vfl_dir = VFL_DIR_RX;
#endif

	bpa2_part = bpa2_find_part(BPA2_PARTITION);
	if (!bpa2_part) {
		DVB_ERROR("No %s bpa2 part exist\n", BPA2_PARTITION);
		return -EINVAL;
	}

	stm_v4l2_capture_dev.mem_bpa2_ctx = vb2_bpa2_contig_init_ctx(&viddev->dev, bpa2_part);

	/* Initialize capture video device pads */
	for (i = 0; i < nb_dec_video + nb_extip_dev; i++)
		pads[video_decoder_offset + i].flags = MEDIA_PAD_FL_SINK;
	for (i = 0; i < nb_dec_audio; i++)
		pads[audio_decoder_offset + i].flags = MEDIA_PAD_FL_SINK;

	viddev->entity.ops = &stm_v4l2_capture_media_ops;
	ret =
	    media_entity_init(&viddev->entity, nb_dec_video + nb_extip_dev + nb_dec_audio,
			      pads, 0);
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to initialize the entity (%d)\n",
		       __func__, ret);
		BUG();
	}

	/* In our device model, Grab/Overlay is video0 */
	ret = stm_media_register_v4l2_video_device(viddev, VFL_TYPE_GRABBER, 0);
	if (ret < 0) {
		printk(KERN_ERR
		       "%s: failed to register the overlay driver (%d)\n",
		       __func__, ret);
		return ret;
	}

	/* Create disabled links between capture video device and video decoders */
	entity =
	    stm_media_find_entity_with_type_first
	    (MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER);
	i = 0;

	while (entity) {

		/* Video decoder -> video device link */
		sink = &viddev->entity;
		ret = media_entity_create_link(entity, 0, sink,
				     i + video_decoder_offset, flags);
		if (ret < 0) {
			/* TODO - error handling */
			printk(KERN_ERR "%s: failed to create link (%d)\n",
		       __func__, ret);
			BUG();
		}

		/* Video decoder -> plane link */
		sink =
		    stm_media_find_entity_with_type_first
		    (MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO);

		while (sink) {
			ret = media_entity_create_link(entity, 0, sink, 0, 0);
			if (ret < 0) {
				/* TODO - error handling */
				printk(KERN_ERR
				       "%s: failed to create link (%d)\n",
				       __func__, ret);
				BUG();
			}

			sink = stm_media_find_entity_with_type_next(sink,
								    MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO);
		}

		entity = stm_media_find_entity_with_type_next(entity,
						      MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER);
		i++;
	}

	/* Create disabled links between capture  device and hdmixrx */
	entity =
		    stm_media_find_entity_with_type_first
		    (MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX);
		i = 0;
		while (entity) {
			sink = &viddev->entity;
			ret =
			    media_entity_create_link(entity, 0, sink,
				i + (video_decoder_offset + nb_dec_video), flags);
			if (ret < 0) {
				/* TODO - error handling */
				printk(KERN_ERR "%s: failed to create link (%d)\n",
				       __func__, ret);
				BUG();
			}
			entity = stm_media_find_entity_with_type_next(entity,
								      MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX);
			i++;
		}

	/* Create disabled links between capture  device and vidextin */
	entity =
		    stm_media_find_entity_with_type_first
		    (MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_HDMIRX);

		while (entity) {
			sink = &viddev->entity;
			ret =
			    media_entity_create_link(entity, 0, sink,
				i + (video_decoder_offset + nb_dec_video), flags);
			if (ret < 0) {
				/* TODO - error handling */
				printk(KERN_ERR "%s: failed to create link (%d)\n",
				       __func__, ret);
				BUG();
			}
			entity = stm_media_find_entity_with_type_next(entity,
								      MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_HDMIRX);
			i++;
		}
	/* Create disabled links between capture  device and vxi */
	entity =
		    stm_media_find_entity_with_type_first
		    (MEDIA_ENT_T_V4L2_SUBDEV_VXI);

		while (entity) {
			sink = &viddev->entity;
			ret =
			    media_entity_create_link(entity, 0, sink,
				i + (video_decoder_offset + nb_dec_video), flags);
			if (ret < 0) {
				/* TODO - error handling */
				printk(KERN_ERR "%s: failed to create link (%d)\n",
				       __func__, ret);
				BUG();
			}
			entity = stm_media_find_entity_with_type_next(entity,
								      MEDIA_ENT_T_V4L2_SUBDEV_VXI);
			i++;
		}

	/* Create disabled links between capture  device and vidextin */
	entity =
		    stm_media_find_entity_with_type_first
		    (MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_VXI);

		while (entity) {
			sink = &viddev->entity;
			ret =
			    media_entity_create_link(entity, 0, sink,
				i + (video_decoder_offset + nb_dec_video), flags);
			if (ret < 0) {
				/* TODO - error handling */
				printk(KERN_ERR "%s: failed to create link (%d)\n",
				       __func__, ret);
				BUG();
			}
			entity = stm_media_find_entity_with_type_next(entity,
								      MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_VXI);
			i++;
		}

	/* Create disabled links between capture video device and audio decoders */
	entity =
	    stm_media_find_entity_with_type_first
	    (MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER);
	i = 0;
	while (entity) {

		/* Audio decoder -> video device link */
		sink = &viddev->entity;
		ret = media_entity_create_link(entity, 0, sink,
					i + audio_decoder_offset, flags);
		if (ret < 0) {
			/* TODO - error handling */
			printk(KERN_ERR "%s: failed to create link (%d)\n",
					__func__, ret);
			BUG();
		}

		/* Audio decoder -> plane link */
		sink = stm_media_find_entity_with_type_first
					(MEDIA_ENT_T_ALSA_SUBDEV_MIXER);

		while (sink) {
			ret = media_entity_create_link(entity, 0, sink,
					STM_SND_MIXER_PAD_PRIMARY, 0);
			if (ret < 0) {
				/* TODO - error handling */
				printk(KERN_ERR "%s: failed to create link (%d)\n",
						__func__, ret);
				BUG();
			}

			ret = media_entity_create_link(entity, 0, sink,
					STM_SND_MIXER_PAD_SECONDARY, 0);
			if (ret < 0) {
				/* TODO - error handling */
				printk(KERN_ERR
						"%s: failed to create link (%d)\n",
						__func__, ret);
				BUG();
			}

			ret = media_entity_create_link(entity, 0, sink,
						STM_SND_MIXER_PAD_AUX2, 0);
			if (ret < 0) {
				/* TODO - error handling */
				printk(KERN_ERR
						"%s: failed to create link (%d)\n",
						__func__, ret);
				BUG();
			}

			ret = media_entity_create_link(entity, 0, sink,
						STM_SND_MIXER_PAD_AUX3, 0);
			if (ret < 0) {
				/* TODO - error handling */
				printk(KERN_ERR
						"%s: failed to create link (%d)\n",
						__func__, ret);
				BUG();
			}

			sink = stm_media_find_entity_with_type_next(sink,
					MEDIA_ENT_T_ALSA_SUBDEV_MIXER);
		}

		/* Audio decoder -> audio-player link */
		sink =
			stm_media_find_entity_with_type_first
			(MEDIA_ENT_T_ALSA_SUBDEV_PLAYER);

		while (sink) {
			ret =
				media_entity_create_link(entity, 0, sink,
						0, 0);
			if (ret < 0) {
				/* TODO - error handling */
				printk(KERN_ERR
						"%s: failed to create link (%d)\n",
						__func__, ret);
				BUG();
			}

			sink = stm_media_find_entity_with_type_next(sink,
					MEDIA_ENT_T_ALSA_SUBDEV_PLAYER);
		}


		i++;
		entity = stm_media_find_entity_with_type_next(entity,
				MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER);
	}
	/* all init seems ok!
         * now register the capture zero-copy interface also ...
         * Assume not enough memory if registry fails!
         */
	if ( video_zc_init(NULL) )
		goto failed_zc_interface_registration;

	return 0;

failed_zc_interface_registration:
	kfree(stm_v4l2_capture_dev.pads);
failed_pads_table_alloc:
	kfree(g_stm_v4l2_input_device);
failed_input_table_alloc:
	return -ENOMEM;
}

/**
 * stm_v4l2_capture_exit() - Audio/Video/User-data capture driver exit
 * This is the driver exit routine. Here, we unregister this video
 * device and cleanup the associated media entities. Cleanup bpa2
 * context used for allocation and translation of user pointer.
 * Exit zero copy video capture and free all the allocated contexts.
 */
void stm_v4l2_capture_exit(void)
{
	stm_media_unregister_v4l2_video_device(&stm_v4l2_capture_dev.viddev);

	vb2_bpa2_contig_cleanup_ctx(stm_v4l2_capture_dev.mem_bpa2_ctx);

	media_entity_cleanup(&stm_v4l2_capture_dev.viddev.entity);

	video_zc_exit();

	kfree(stm_v4l2_capture_dev.pads);

	kfree(g_stm_v4l2_input_device);
}
