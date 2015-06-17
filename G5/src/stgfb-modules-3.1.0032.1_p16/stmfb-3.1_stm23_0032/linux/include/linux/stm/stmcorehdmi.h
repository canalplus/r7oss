/***********************************************************************
 *
 * File: linux/video/stmcorehdmi.h
 * Copyright (c) 2007-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMCOREHDMI_H
#define _STMCOREHDMI_H

#include <linux/kobject.h> /* kobject.h is needed by cdev.h until we switch to 2.6.23 */
#include <linux/cdev.h>

#include <linux/stm/stmcoredisplay.h>

#define STM_MAX_EDID_BLOCKS 256
typedef unsigned char edid_block_t[128];

#define STM_MAX_CEA_MODES   128

typedef enum
{
  STM_DISPLAY_INVALID = -1,
  STM_DISPLAY_DVI     = 0,
  STM_DISPLAY_HDMI    = 1
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

#define STM_HDMI_VSDB_DC_Y444        (1L<<3)
#define STM_HDMI_VSDB_DC_30BIT       (1L<<4)
#define STM_HDMI_VSDB_DC_36BIT       (1L<<5)
#define STM_HDMI_VSDB_DC_48BIT       (1L<<6)
#define STM_HDMI_VSDB_DC_MASK        (0x70)
#define STM_HDMI_VSDB_SUPPORTS_AI    (1L<<7)


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
  stm_wss_t      tv_aspect;

  unsigned char  cea_version;
  unsigned char  num_modes;
  unsigned char  cea_vcdb_flags;
  unsigned int   cea_coldb_flags;

  const stm_mode_line_t *display_modes[STM_MAX_CEA_MODES];
  stm_cea_video_code_descriptor_t cea_codes[STM_MAX_CEA_MODES];

  struct stm_cea_audio_descriptor audio_modes[STM_CEA_AUDIO_LAST+1];

  unsigned int   speaker_allocation;

  /*
   * HDMI-LLC Vendor specific data
   */
  unsigned char  cec_address[4];
  unsigned int   hdmi_vsdb_flags;
  unsigned int   max_tmds_clock;
  unsigned short progressive_video_latency;
  unsigned short progressive_audio_latency;
  unsigned short interlaced_video_latency;
  unsigned short interlaced_audio_latency;

  edid_block_t   raw[STM_MAX_EDID_BLOCKS];
};


#define STMHDMI_HPD_STOP_AND_RESTART   0
#define STMHDMI_HPD_DISABLE_OUTPUT     1
#define STMHDMI_HPD_STOP_IF_NECESSARY  2

#define _STM_HDMI_DEF 1

struct stm_hdmi {
  /* Hotplug HDMI/DVI display management and EDID information */
  int                           irq;
  struct mutex                  lock;
  struct cdev                   cdev;
  spinlock_t                    spinlock;
  wait_queue_head_t             auth_wait_queue;
  wait_queue_head_t             status_wait_queue;

  struct stmcore_display       *display_runtime;
  struct stmcore_vsync_cb       vsync_cb_info;

  struct task_struct           *thread;
  struct class_device          *class_device;

  stm_display_device_t         *device;
  stm_display_output_t         *main_output;
  stm_display_output_t         *hdmi_output;

  ULONG                         capabilities;

  volatile stm_display_status_t status;         /* HDMI Hotplug connection status                */
  volatile int                  status_changed; /* HDMI Hotplug connection status wait condition */
  volatile int                  auth;
  volatile int                  disable;
  volatile int                  deferred_disconnect;

  ULONG                         video_type;     /* RGB or YUV video for HDMI output              */

  /*
   * The I2C adapter that we expect hotplugged HDMI TVs to appear on
   * for E-DID information.
   */
  struct i2c_adapter           *i2c_adapter;
  struct i2c_client            *edid_client;
  struct i2c_client            *eddc_segment_reg_client;

  struct stm_edid               edid_info;
  int                           non_strict_edid_semantics;
  int                           hotplug_mode;
  int                           hdmi_safe_mode;
  int                           cea_mode_from_edid;

   /*
   * Source Product Descriptor info frame
   */
  stm_meta_data_t               *spd_metadata;
  stm_hdmi_info_frame_t         *spd_frame;
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
int stmhdmi_init_class_device(struct stm_hdmi *hdmi, struct stmcore_display_pipeline_data *platform_data);


#endif /* _STMFBINFO_H */
