/***********************************************************************
 * File: linux/drivers/video/stmvout_driver.h
 * Copyright (c) 2005,2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 ***********************************************************************/

#ifndef __STMVOUT_DRIVER_H
#define __STMVOUT_DRIVER_H

#if defined DEBUG
#  define PKMOD "stmvout: "
#  define debug_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#  define err_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#  define info_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#else
#  define debug_msg(fmt,arg...)
#  define err_msg(fmt,arg...)
#  define info_msg(fmt,arg...)
#endif

#define MAX_OPENS	    32
#define MAX_USER_BUFFERS    15
#define MAX_PIPELINES       3
#define MAX_PLANES          8

#include "STMCommon/stmiqi.h"

struct _stvout_device;


struct stm_v4l2_queue
{
  	rwlock_t         lock;
	struct list_head list; // of type struct _streaming_buffer
};

typedef struct _streaming_buffer
{
  struct list_head       node;
  struct v4l2_buffer     vidBuf;
  unsigned long          physicalAddr;
  const char            *partname;
  char                  *cpuAddr;
  int			 mapCount;
  struct _stvout_device *pDev;
  stm_display_buffer_t   bufferHeader;

  unsigned long          clut_phys;
  char                  *clut_virt;

  /* only reflects state for userbuffers, i.e. unused in case of system
     buffers */
  int                    isAllocated;
} streaming_buffer_t;


typedef struct _open_data
{
  int                    isOpen;
  int                    isFirstUser;
  struct _stvout_device *pDev;
} open_data_t;


typedef struct _stvout_device
{
  char                    name[32];
  struct v4l2_format      bufferFormat;
  struct v4l2_crop        srcCrop;
  struct v4l2_crop        outputCrop;

  struct stm_v4l2_queue pendingStreamQ;
  struct stm_v4l2_queue completeStreamQ;

  struct stmcore_display_pipeline_data displayPipe;

  int                     currentOutputNum;
  int                     isStreaming;
  int                     isRegistered;

  struct stmcore_plane_data *planeConfig;
  stm_display_plane_t*       pPlane;
  stm_display_output_t*      pOutput;

  int                     queues_inited;
  wait_queue_head_t       wqBufDQueue;
  wait_queue_head_t       wqStreamingState;
  struct semaphore        devLock;

  open_data_t             openData[MAX_OPENS];
  streaming_buffer_t     *streamBuffers;
  int                     n_streamBuffers;

  streaming_buffer_t      userBuffers[MAX_USER_BUFFERS];
  unsigned long           userClut_phys;
  char                   *userClut_virt;

  SURF_FMT                planeFormat;
  unsigned int            globalAlpha;

  struct _stvout_device_state {
    struct StmColorKeyConfig ckey;

    struct fmd_init_data {
      u32 ulH_block_sz;
      u32 ulV_block_sz;

      u32 count_miss;
      u32 count_still;
      u32 t_noise;
      u32 k_cfd1;
      u32 k_cfd2;
      u32 k_cfd3;

      u32 t_mov;
      u32 t_num_mov_pix;
      u32 t_repeat;
      u32 t_scene;

      u32 k_scene;
      u8  d_scene;
    } old_fmd, new_fmd;

    struct iqi_init_data {
      u32 enables;
      struct _IQIPeakingConf peaking;
      struct _IQILtiConf lti;
      struct _IQICtiConf cti;
      struct _IQILEConf le;
      /* CE not supported */
    } old_iqi, new_iqi;
  } state;
} stvout_device_t;

enum STM_VOUT_OUTPUT_ID {
  SVOI_MAIN,
  SVOI_AUX
};

/*
 * stmvout_buffers.c
 */
void stmvout_init_buffer_queues (stvout_device_t * const device);

static inline int stmvout_has_queued_buffers(stvout_device_t *pDev)
{
  return !list_empty(&pDev->pendingStreamQ.list);
}


static inline int stmvout_has_completed_buffers(stvout_device_t *pDev)
{
  return !list_empty(&pDev->completeStreamQ.list);
}

int   stmvout_allocate_clut   (struct _stvout_device *device);
void  stmvout_deallocate_clut (struct _stvout_device *device);

int   stmvout_queue_buffer (stvout_device_t *pDev, struct v4l2_buffer* pVidBuf);
int   stmvout_dequeue_buffer (stvout_device_t *pDev, struct v4l2_buffer* pVidBuf);
void  stmvout_send_next_buffer_to_display (stvout_device_t *pDev);
int   stmvout_streamoff (stvout_device_t *pDev);

int   stmvout_set_buffer_format  (stvout_device_t *pDev, struct v4l2_format *fmt, int updateConfig);
int   stmvout_enum_buffer_format (stvout_device_t *pDev, struct v4l2_fmtdesc *f);

int   stmvout_set_output_crop (stvout_device_t *pDev, struct v4l2_crop *pCrop);
int   stmvout_set_buffer_crop (stvout_device_t *pDev, struct v4l2_crop *pCrop);


int   stmvout_allocate_mmap_buffers   (stvout_device_t *pDev, struct v4l2_requestbuffers* req);
int   stmvout_deallocate_mmap_buffers (stvout_device_t *pDev);

streaming_buffer_t *stmvout_get_buffer_from_mmap_offset (stvout_device_t *pDev, unsigned long offset);

/*
 * stmvout_display.c
 */
int stmvout_set_video_output(stvout_device_t *pDev, enum STM_VOUT_OUTPUT_ID id);
int stmvout_get_supported_standards (stvout_device_t *pDev, v4l2_std_id *ids);
int stmvout_get_current_standard(stvout_device_t *pDev, v4l2_std_id *stdid);
int stmvout_set_current_standard(stvout_device_t *pDev, v4l2_std_id stdid);
int stmvout_get_display_standard(stvout_device_t *pDev, struct v4l2_standard *std);
int stmvout_get_display_size (stvout_device_t *pDev, struct v4l2_cropcap *cap);

/*
 * stmvout_ctrl.c
 */
extern const u32 *stmvout_ctrl_classes[];

void stmvout_get_controls_from_core (struct _stvout_device * const device);
int stmvout_queryctrl (struct _stvout_device * const pDev,
                       struct _stvout_device  pipeline_devs[],
                       struct v4l2_queryctrl * const pqc);
int stmvout_querymenu (struct _stvout_device * const device,
                       struct _stvout_device   pipeline_devs[],
                       struct v4l2_querymenu * const qmenu);

int stmvout_s_ctrl (struct _stvout_device * const pDev,
                    struct _stvout_device   pipeline_devs[],
                    struct v4l2_control    * const pctrl);
int stmvout_g_ctrl (struct _stvout_device * const pDev,
                    struct _stvout_device  pipeline_devs[],
                    struct v4l2_control   * const pctrl);

int stmvout_g_ext_ctrls   (struct _stvout_device    * const device,
                           struct v4l2_ext_controls *ctrls);
int stmvout_s_ext_ctrls   (struct _stvout_device    * const device,
                           struct v4l2_ext_controls *ctrls);
int stmvout_try_ext_ctrls (struct _stvout_device    * const device,
                           struct v4l2_ext_controls *ctrls);


#endif /* __STMVOUT_DRIVER_H */
