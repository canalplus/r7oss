/***********************************************************************
 * File: linux/kernel/drivers/video/stmvout_driver.h
 * Copyright (c) 2005,2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 ***********************************************************************/

#ifndef __STMVOUT_DRIVER_H
#define __STMVOUT_DRIVER_H

#include <media/v4l2-dev.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include "stmedia.h"

#include <stm_pixel_capture.h>

#ifdef DEBUG
#define PKMOD "stmvout: "
#define debug_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#define err_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#define info_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#define debug_mc(fmt,arg...) printk(PKMOD fmt,##arg)
#define debug_ioctl(fmt,arg...) printk(PKMOD fmt,##arg)
#define debug_link(fmt,arg...) printk(PKMOD fmt,##arg)
#else
#define debug_msg(fmt,arg...)
#define err_msg(fmt,arg...)
#define info_msg(fmt,arg...)
#define debug_mc(fmt,arg...)
#define debug_ioctl(fmt,arg...)
#define debug_link(fmt,arg...)
#endif

#define MAX_OPENS           32
#define MAX_USER_BUFFERS    15
#define MAX_PIPELINES       3
#define MAX_PLANES          8

struct _stvout_device;

struct stm_v4l2_queue {
	rwlock_t lock;
	struct list_head list;	/* of type struct _streaming_buffer */
};

typedef struct _streaming_buffer {
	struct list_head node;
	struct v4l2_buffer vidBuf;
	unsigned long physicalAddr;
	struct bpa2_part *bpa2_part;
	char *cpuAddr;
	int mapCount;
	struct _stvout_device *pDev;
	stm_display_buffer_t bufferHeader;

	unsigned long clut_phys;
	char *clut_virt;

	/* only reflects state for userbuffers, i.e. unused in case of system
	   buffers */
	int isAllocated;
} streaming_buffer_t;

/*
 * This structure keep track of all plane users: internal from LinuxDVB
 * APIs and V4L2 via /dev/video1
 */
typedef struct _open_data {
	int isOpen;
	int isFirstUser;
	int padId;
	struct list_head open_list;
	struct _stvout_device *pDev;
} open_data_t;

#define STMEDIA_MAX_PLANES         8	/* Max #. of planes we will probe */
#define STMEDIA_MAX_OUTPUTS        8	/* Max #. of outputs we will probe */

#define STMEDIA_PLANE_PADS_NUM     2
#define STMEDIA_OUTPUT_PADS_NUM    2
/*
 * an entity has a unique array of PADs and by convention the first PAD
 * is assumed to be the input (SINK) and the second is the output (SOURCE)
 */
#define SINK_PAD_INDEX             0
#define SOURCE_PAD_INDEX           1

#define STMEDIA_PLANE_TYPE_VIDEO       (1<<0)
#define STMEDIA_PLANE_TYPE_GRAPHICS    (1<<1)
#define STMEDIA_PLANE_TYPE_VBI         (1<<2)

/*
 * ouput plug descriptor the stvout_device (below) is connected to, via
 * Media Controller.
 */
struct output_plug {
	/* struct list_head list; */
	char name[32];		/* dups Display object name   */
	unsigned int id;	/* as in Display              */
	unsigned int type;	/* output type                */
	struct mutex lock;	/* TODO: not yet used         */

	struct v4l2_subdev sdev;
	stm_display_output_h hOutput;
	stm_display_device_h hDisplay;	/* belongs to Display ...     */

	/* not sure are necessary */
	int input_pads;		/* sink                      */
	int output_pads;	/* source                     */

	/* Pixel Capture information - valid in case of Main Compo */
	stm_pixel_capture_h pixel_capture;
/* Number of buffers used for Dual Display tunneled */
#define DD_BPA2_PARTITION	"v4l2-compo"
#define DD_GFX_BUFFERS_NB	4		/* additional buffer for FRC */
#define DD_VID_BUFFERS_NB	6
	int nb_buffers;

	stm_pixel_capture_buffer_descr_t pixel_capture_buffer[DD_VID_BUFFERS_NB];
	stm_pixel_capture_buffer_format_t pixel_capture_format;

	struct media_pad pads[STMEDIA_OUTPUT_PADS_NUM];
};

/*!This represents the display parameters. These parameters will be filled
 * while opening the devices and the same information is passed for closing.
 */
struct stm_source_info{
	stm_display_source_h source;
	/* we need to differentiate the type */
	stm_display_source_interfaces_t iface_type;

	/* interface handle - type indepandant - typecast before use */
	void *iface_handle;

	/* needed for pixel stream sources*/
	u32 pixel_inst;
	u32 pixeltype;

	/* Need this to buffer the pixel params since this is filled up over 2 calls */
	stm_display_source_pixelstream_params_t pixel_param;
};

typedef struct _stvout_device {
	char name[32];
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_format bufferFormat;
	struct v4l2_rect actualPicSize;
	struct v4l2_crop srcCrop;
	struct v4l2_crop outputCrop;

	struct stm_v4l2_queue pendingStreamQ;
	struct stm_v4l2_queue completeStreamQ;

	int currentOutputNum;
	int isStreaming;
	int isRegistered;

	stm_display_plane_h hPlane;	/* Display Plane Handle */
	stm_display_source_h hSource;	/* Display Source connected to */
	stm_display_source_queue_h hQueueInterface;
	stm_display_source_interface_params_t SourceParams;

	struct stm_source_info *source_info;

	/* added for MC support only (see stmvout_mc source) */
	unsigned int id;	/* plane id, as in Display    */
	unsigned int type;	/* plane type                 */
	stm_display_device_h hDisplay;	/* Display handle             */
	struct stmedia_v4l2_subdev stmedia_sdev;

	int input_pads;		/* number of SINK pads        */
	int output_pads;	/* number of SOURCE pads      */

	struct media_pad pads[STMEDIA_PLANE_PADS_NUM];
	/* End of MC specific info */

	int queues_inited;
	wait_queue_head_t wqBufDQueue;
	wait_queue_head_t wqStreamingState;
	struct semaphore devLock;

	int open_count;
	open_data_t openData;	/* TODO likely no longer
				   necessary here */
	streaming_buffer_t *streamBuffers;
	int n_streamBuffers;

	streaming_buffer_t userBuffers[MAX_USER_BUFFERS];
	unsigned long userClut_phys;
	char *userClut_virt;

	stm_pixel_format_t planeFormat;
	unsigned int globalAlpha;
	int alpha_control;

	struct _stvout_device_state {
		stm_color_key_config_t old_ckey, new_ckey;

		stm_fmd_params_t old_fmd, new_fmd;    
	    stm_iqi_peaking_conf_t old_peaking, new_peaking;	
	    stm_iqi_le_conf_t old_le, new_le;	
	    stm_iqi_cti_conf_t old_cti, new_cti;
	    int old_peaking_preset, new_peaking_preset;
	    int old_le_preset, new_le_preset;
	    int old_cti_preset, new_cti_preset;	       			    	    

	} state;
} stvout_device_t;

enum STM_VOUT_OUTPUT_ID {
	SVOI_MAIN,
	SVOI_AUX
};

/*
 * Media Controller helpers
 */
#define plane_entity_to_stvout_device(_e) \
	container_of(entity_to_stmedia_v4l2_subdev(_e), \
			struct _stvout_device, stmedia_sdev)

#define output_entity_to_output_plug(_e) \
	container_of(media_entity_to_v4l2_subdev(_e), \
			struct output_plug, sdev)

/**
 * stm_media_entity_to_display_plane
 * @me:		pointer to the media entity
 * This macro returns the pointer to the plane connected
 * to this media entity
 */
#define stm_media_entity_to_display_plane(_e)			\
	plane_entity_to_stvout_device(_e)->hPlane

/**
 * stm_media_entity_to_display_device
 * @me:		pointer to the media entity
 * This macro returns the pointer to the display device connected
 * to this media entity
 */
#define stm_media_entity_to_display_device(_e)			\
	plane_entity_to_stvout_device(_e)->hDisplay

stm_display_output_h stmvout_get_output_display_handle(stvout_device_t * pDev);

/*
 * stmvout_buffers.c
 */
void stmvout_init_buffer_queues(stvout_device_t * const device);

static inline int stmvout_has_queued_buffers(stvout_device_t * pDev)
{
	return !list_empty(&pDev->pendingStreamQ.list);
}

static inline int stmvout_has_completed_buffers(stvout_device_t * pDev)
{
	return !list_empty(&pDev->completeStreamQ.list);
}

int stmvout_allocate_clut(struct _stvout_device *device);
void stmvout_deallocate_clut(struct _stvout_device *device);

int stmvout_queue_buffer(stvout_device_t * pDev, struct v4l2_buffer *pVidBuf);
int stmvout_dequeue_buffer(stvout_device_t * pDev, struct v4l2_buffer *pVidBuf);
void stmvout_send_next_buffer_to_display(stvout_device_t * pDev);
int stmvout_streamon(stvout_device_t * pDev);
int stmvout_streamoff(stvout_device_t * pDev);

int stmvout_set_buffer_format(stvout_device_t * pDev, struct v4l2_format *fmt,
			      int updateConfig);
int stmvout_enum_buffer_format(stvout_device_t * pDev, struct v4l2_fmtdesc *f);

int stmvout_set_output_crop(stvout_device_t * pDev,
				const struct v4l2_crop *pCrop);
int stmvout_set_buffer_crop(stvout_device_t * pDev,
				const struct v4l2_crop *pCrop);

int stmvout_allocate_mmap_buffers(stvout_device_t * pDev,
				  struct v4l2_requestbuffers *req);
int stmvout_deallocate_mmap_buffers(stvout_device_t * pDev);

streaming_buffer_t *stmvout_get_buffer_from_mmap_offset(stvout_device_t * pDev,
							unsigned long offset);

void stmvout_delete_buffers_from_list(const struct list_head *const list);

/*
 * stmvout_display.c
 */
int stmvout_get_supported_standards(stvout_device_t * pDev, v4l2_std_id * ids);
int stmvout_get_current_standard(stvout_device_t * pDev, v4l2_std_id * stdid);
int stmvout_set_current_standard(stvout_device_t * pDev, v4l2_std_id stdid);
int stmvout_get_display_standard(stvout_device_t * pDev,
				 struct v4l2_standard *std);
int stmvout_get_display_size(stvout_device_t * pDev, struct v4l2_cropcap *cap);

/*
 * stmvout_ctrl.c
 */
int stmvout_get_queue_iface_display_source( stvout_device_t *pDev );

int stvmout_init_ctrl_handler(struct _stvout_device *const pDev);


#endif /* __STMVOUT_DRIVER_H */
