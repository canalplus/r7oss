/***********************************************************************
 *
 * File: linux/kernel/include/linux/stm/stmcorehdmi.h
 * Copyright (c) 2007-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMCOREHDMI_H
#define _STMCOREHDMI_H

#include <linux/cdev.h>

#include <stm_display_link.h>
#include <stm_hdmirx.h>

#include <linux/stm/stmcoredisplay.h>

#include "stm_event.h"

#define STM_MAX_EDID_BLOCKS 256
typedef unsigned char edid_block_t[128];

#define STM_MAX_CEA_MODES_HDMI_14B   64
#define STM_MAX_CEA_MODES           255

typedef enum
{
  STM_DISPLAY_INVALID = 0,
  STM_DISPLAY_DVI     = 1,
  STM_DISPLAY_HDMI    = 2
} stm_display_t;


typedef enum
{
  STM_CEA_AUDIO_RESERVED = 0,
  STM_CEA_AUDIO_LPCM,
  STM_CEA_AUDIO_AC3,
  STM_CEA_AUDIO_MPEG1,
  STM_CEA_AUDIO_MP3,
  STM_CEA_AUDIO_MPEG2,
  STM_CEA_AUDIO_AAC,
  STM_CEA_AUDIO_DTS,
  STM_CEA_AUDIO_ATRAC,
  STM_CEA_AUDIO_ONE_BIT_AUDIO,
  STM_CEA_AUDIO_DOLBY_DIGITAL_PLUS,
  STM_CEA_AUDIO_DTS_HD,
  STM_CEA_AUDIO_MLP,
  STM_CEA_AUDIO_DST,
  STM_CEA_AUDIO_WMA_PRO,
  STM_CEA_AUDIO_EXTENSION_FORMAT,
  STM_CEA_AUDIO_LAST = STM_CEA_AUDIO_EXTENSION_FORMAT
} stm_cea_audio_formats_t;


typedef enum
{
  STM_CEA_AUDIO_EX_RESERVED = 0,
  STM_CEA_AUDIO_EX_HE_AAC,
  STM_CEA_AUDIO_EX_HE_AAC_V2,
  STM_CEA_AUDIO_EX_MPEG_SURROUND,
  STM_CEA_AUDIO_EX_LAST = STM_CEA_AUDIO_EX_MPEG_SURROUND,
} stm_cea_audio_ex_formats_t;


#define STM_CEA_AUDIO_RATE_32_KHZ    (1L<<0)
#define STM_CEA_AUDIO_RATE_44_1_KHZ  (1L<<1)
#define STM_CEA_AUDIO_RATE_48_KHZ    (1L<<2)
#define STM_CEA_AUDIO_RATE_88_2_KHZ  (1L<<3)
#define STM_CEA_AUDIO_RATE_96_KHZ    (1L<<4)
#define STM_CEA_AUDIO_RATE_176_4_KHZ (1L<<5)
#define STM_CEA_AUDIO_RATE_192_KHZ   (1L<<6)

#define STM_CEA_SPEAKER_FLFR         (1L<<0)
#define STM_CEA_SPEAKER_LFE          (1L<<1)
#define STM_CEA_SPEAKER_FC           (1L<<2)
#define STM_CEA_SPEAKER_RLRR         (1L<<3)
#define STM_CEA_SPEAKER_RC           (1L<<4)
#define STM_CEA_SPEAKER_FLCFRC       (1L<<5)
#define STM_CEA_SPEAKER_RLCRRC       (1L<<6)
#define STM_CEA_SPEAKER_FLWFRW       (1L<<7)
#define STM_CEA_SPEAKER_FLHFRH       (1L<<8)
#define STM_CEA_SPEAKER_TC           (1L<<9)
#define STM_CEA_SPEAKER_FCH          (1L<<10)

typedef enum
{
  STM_CEA_VIDEO_CODE_UNSUPPORTED = 0,
  STM_CEA_VIDEO_CODE_NATIVE,
  STM_CEA_VIDEO_CODE_NON_NATIVE
} stm_cea_video_code_descriptor_t;


struct stm_cea_video_code
{
  stm_cea_video_code_descriptor_t cea_code_descriptor;
  unsigned int                    edid_entry;
  unsigned int                    supported_3d_flags;
  unsigned int                    sbs_half_detail;
};


struct stm_cea_audio_descriptor
{
  stm_cea_audio_formats_t format;
  unsigned                max_channels;
  unsigned                sample_rates;
  union
  {
    unsigned              lpcm_bit_depths;
    unsigned              max_bit_rate;
    unsigned              format_data;
    unsigned              profile;
    stm_cea_audio_ex_formats_t ex_format;
  };
};

#define STM_CEA_CAPS_SRGB            (1<<0)
#define STM_CEA_CAPS_YUV             (1<<1)
#define STM_CEA_CAPS_422             (1<<2)
#define STM_CEA_CAPS_UNDERSCAN       (1<<3)
#define STM_CEA_CAPS_GTF             (1<<4)
#define STM_CEA_CAPS_BASIC_AUDIO     (1<<5)
#define STM_CEA_CAPS_RGB             (1<<6)
#define STM_CEA_CAPS_420             (1<<7)

#define STM_CEA_VCDB_CE_UNSUPPORTED  (0)
#define STM_CEA_VCDB_CE_OVERSCANNED  (1)
#define STM_CEA_VCDB_CE_UNDERSCANNED (2)
#define STM_CEA_VCDB_CE_SCAN_BOTH    (3)
#define STM_CEA_VCDB_IT_UNSUPPORTED  (0)
#define STM_CEA_VCDB_IT_OVERSCANNED  (1<<2)
#define STM_CEA_VCDB_IT_UNDERSCANNED (2<<2)
#define STM_CEA_VCDB_IT_SCAN_BOTH    (3<<2)
#define STM_CEA_VCDB_PT_UNDEFINED    (0)
#define STM_CEA_VCDB_PT_OVERSCANNED  (1<<4)
#define STM_CEA_VCDB_PT_UNDERSCANNED (2<<4)
#define STM_CEA_VCDB_PT_SCAN_BOTH    (3<<4)
#define STM_CEA_VCDB_QS_RGB_QUANT    (1<<6)
#define STM_CEA_VCDB_QY_YCC_QUANT    (1<<7)


#define STM_CEA_COLDB_XVYCC601    (1)
#define STM_CEA_COLDB_XVYCC709    (1<<1)
#define STM_CEA_COLDB_SYCC601     (1<<2)
#define STM_CEA_COLDB_ADOBEYCC601 (1<<3)
#define STM_CEA_COLDB_ADOBERGB    (1<<4)
#define STM_CEA_COLDB_MD_PROFILE0 (1<<8)
#define STM_CEA_COLDB_MD_PROFILE1 (1<<9)
#define STM_CEA_COLDB_MD_PROFILE2 (1<<10)
#define STM_CEA_COLDB_MD_PROFILE3 (1<<11)

/* VSDB Byte 6 defintions */
#define STM_HDMI_VSDB_DC_Y444        (1L<<3)
#define STM_HDMI_VSDB_DC_30BIT       (1L<<4)
#define STM_HDMI_VSDB_DC_36BIT       (1L<<5)
#define STM_HDMI_VSDB_DC_48BIT       (1L<<6)
#define STM_HDMI_VSDB_DC_MASK        (0x70)
#define STM_HDMI_VSDB_SUPPORTS_AI    (1L<<7)

/* VSDB Byte 8 definitions of CNC flags */
#define STM_HDMI_VSDB_CNC_GRAPHICS       (1L<<0)
#define STM_HDMI_VSDB_CNC_PHOTO          (1L<<1)
#define STM_HDMI_VSDB_CNC_CINEMA         (1L<<2)
#define STM_HDMI_VSDB_CNC_GAME           (1L<<3)
#define STM_HDMI_VSDB_CNC_MASK           (0xf)

/* HF-VSDB Byte 6 defintions */
#define STM_HDMI_HF_VSDB_3D_OSD_DISPARITY      (1)
#define STM_HDMI_HF_VSDB_DUAL_VIEW             (1<<1)
#define STM_HDMI_HF_VSDB_INDEPENDENT_VIEW      (1<<2)
#define STM_HDMI_HF_VSDB_LTE_340MCSC_SCRAMBLE  (1<<3)
#define STM_HDMI_HF_VSDB_RR_CAPABLE            (1<<6)
#define STM_HDMI_HF_VSDB_SCDC_PRESENT          (1<<7)

/* HF-VSDB Byte 7 defintions */
#define STM_HDMI_HF_VSDB_DC_30BIT_420          (1)
#define STM_HDMI_HF_VSDB_DC_36BIT_420          (1<<2)
#define STM_HDMI_HF_VSDB_DC_48BIT_420          (1<<3)
#define STM_HDMI_HF_VSDB_DC_MASK               (0x7)

struct stm_edid {
  stm_display_t  display_type;

  int            edid_version;
  int            edid_revision;

  unsigned int   manufacturer_id;
  unsigned int   product_id;
  unsigned int   serial_nr;
  short          production_week;
  short          production_year;

  char           monitor_name[14];

  unsigned int   cea_capabilities;
  unsigned int   max_clock;
  unsigned int   min_vfreq;
  unsigned int   max_vfreq;
  unsigned int   min_hfreq;
  unsigned int   max_hfreq;

  unsigned int   max_hsize;
  unsigned int   max_vsize;

  stm_wss_aspect_ratio_t tv_aspect;

  unsigned char  cea_version;
  unsigned char  num_modes;
  unsigned char  cea_vcdb_flags;
  unsigned int   cea_coldb_flags;

  stm_display_mode_t        display_modes[STM_MAX_CEA_MODES];
  struct stm_cea_video_code cea_codes[STM_MAX_CEA_MODES];
  int                       next_cea_video_code_index;

  struct stm_cea_audio_descriptor audio_modes[STM_CEA_AUDIO_LAST+1];

  unsigned int   speaker_allocation;

  /*
   * HDMI-LLC Vendor specific data
   */
  unsigned char  cec_address[4];
  unsigned int   hdmi_vsdb_flags;
  unsigned int   max_tmds_clock;
  unsigned int   hdmi_cnc_flags;
  unsigned short progressive_video_latency;
  unsigned short progressive_audio_latency;
  unsigned short interlaced_video_latency;
  unsigned short interlaced_audio_latency;

  edid_block_t   base_raw;
  edid_block_t   *extension_raw;

  /*
   * HDMI-HF Vendor specific data
   */
   unsigned char hf_version;
   unsigned int  max_tmds_character_rate;
   unsigned char hdmi_hf_vsdb_flags;
   unsigned char hdmi_hf_dc_flags;        /* deep color 4:2:0 */
   unsigned char svd[STM_MAX_CEA_MODES];  /* Short Video Descriptor */
   unsigned char cmdb[STM_MAX_CEA_MODES]; /* Capability Map Data Block */

};


#define STMHDMI_HPD_STOP_AND_RESTART   0
#define STMHDMI_HPD_DISABLE_OUTPUT     1
#define STMHDMI_HPD_STOP_IF_NECESSARY  2

#define _STM_HDMI_DEF 1

#define STMHDMI_PENDING_STATUS_DISABLE    (1L << 0) /* Disable status from user */
#define STMHDMI_PENDING_STATUS_ENABLE     (1L << 1) /* Enable status from user */
#define STMHDMI_PENDING_STATUS_RESTART    (1L << 2) /* Restart status from hdmi module */
#define STMHDMI_PENDING_STATUS_UPDATE     (1L << 3) /* Update status from coredisplay module */

struct stm_hdmi {
  /* Hotplug HDMI/DVI display management and EDID information */
  int                           irq;
  struct mutex                  lock;
  struct mutex                  hdmilock;
  struct cdev                   cdev;
  spinlock_t                    spinlock;
  wait_queue_head_t             status_wait_queue;

  struct stm_pad_state         *hotplug_pad;
  uint32_t                      hotplug_gpio;

  struct stmcore_vsync_cb       vsync_cb_info;

  struct task_struct           *thread;
  struct device                *class_device;

  uint32_t                      pipeline_id;
  uint32_t                      hdmi_output_id;
  int                           display_device_id;
  int                           device_use_count;
  bool                          device_is_opened;
  stm_display_device_h          device;
  stm_display_output_h          main_output;
  stm_display_output_h          hdmi_output;

  uint32_t                      capabilities;

  volatile stm_display_output_connection_status_t status; /* HDMI Hotplug connection status      */
  volatile int                  status_changed; /* HDMI Hotplug connection status wait condition */
  volatile int                  disable;
  volatile int                  hdcp_enable;
  volatile int                  current_hdcp_status;
  volatile int                  enc_enable;
  volatile int                  stop_output;
  volatile int                  deferred_disconnect;

  uint32_t                      video_type;     /* RGB or YUV video for HDMI output              */

  struct stm_edid               edid_info;
  int                           non_strict_edid_semantics;
  int                           hotplug_mode;
  int                           hdmi_safe_mode;
  int                           cea_mode_from_edid;
  int                           max_pixel_repeat;
  bool                          edid_updated;
  bool                          hdcp_stopped;
  /*
   * HDMI Rx
   */
  stm_hdmirx_device_h            hdmirx_device;
  stm_hdmirx_device_capability_t dev_capability;
  stm_hdmirx_port_h              port;
  stm_hdmirx_port_capability_t   port_capability;
  stm_hdmirx_route_h             route;
  stm_hdmirx_route_capability_t  route_capability;
  stm_event_subscription_h       subscription;
  stm_hdmirx_route_signal_status_t     signal_status;
  stm_hdmirx_route_hdcp_status_t  hdmirx_hdcp_status;
   /*
   * Source Product Descriptor info frame
   */
  stm_display_metadata_t       *spd_metadata;
  stm_hdmi_info_frame_t        *spd_frame;

  stm_display_link_h            link;
  stm_display_link_capability_t link_capability;
  stm_display_link_hpd_state_t  hpd_state;
  stm_display_link_rxsense_state_t rxsense_state;
  stm_display_link_hdcp_status_t hdcp_status;

};

/*
 * This is the declaration of the entrypoints for HDMI management
 */
struct stmcore_display_pipeline_data;
int stmhdmi_create(int id, dev_t firstdevice, struct stmcore_display_pipeline_data *);
int stmhdmi_destroy(struct stm_hdmi *);
int stmhdmi_read_edid(struct stm_hdmi *hdmi);
int stmhdmi_safe_edid(struct stm_hdmi *hdmi);
void stmhdmi_invalidate_edid_info(struct stm_hdmi *hdmi);
void stmhdmi_dump_edid(struct stm_hdmi *);
#if defined(SDK2_ENABLE_HDMI_TX_ATTRIBUTES)
int stmhdmi_create_class_device_files(struct stm_hdmi *hdmi, struct device *display_class_device);
#endif

#endif /* _STMFBINFO_H */
