/***********************************************************************
 *
 * File: linux/kernel/include/linux/stm/stmcorelink.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STMCORELINK_H
#define STMCORELINK_H

#include <linux/i2c.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/connector.h>

#include <stm_display.h>
#include <stm_display_link.h>
#include <linux/stm/stmcoredisplay.h>

#define DEBUG_DISPLAY_LINK_LEVEL 1
#if DEBUG_DISPLAY_LINK_LEVEL
#define LINKDBG(level, fmt, ...) \
    if (level <= DEBUG_DISPLAY_LINK_LEVEL) \
    printk("stmdisplaylink: %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)
#else
#define LINKDBG(...)
#endif


#define HDCP_BCAPS_SIZE   1
#define HDCP_BSTATUS_SIZE 2
#define HDCP_BKSV_SIZE    5

#define SINK_STATUS(x) (((unsigned int)(x)[0]) | (((unsigned int)(x)[1]) << 8))

typedef struct KSV_s {
  uint8_t key[5];
} KSV_t;

typedef enum hdcp_mode_e {
  HDCP_LEGACY_MODE,
  HDCP_CRYPTO_MODE,
  HDCP_NVS_MODE
} hdcp_mode_t;

typedef enum display_mode_e {
  DISPLAY_MODE_INVALID = 0,
  DISPLAY_MODE_HDMI,
  DISPLAY_MODE_DVI
} display_mode_t;




struct stmlink {
  stm_display_link_type_t               type;
  stm_display_link_hpd_state_t          hpd_state;
  stm_display_link_hpd_state_t          hpd_new;
  stm_display_link_rxsense_state_t      rxsense_state;
  stm_display_link_rxsense_state_t      rxsense_new;
  stm_display_link_hdcp_status_t        status;
  stm_display_link_capability_t         capability;
  stm_display_link_hdcp_transaction_t   last_transaction;
  bool                                  rxsense;
  bool                                  hdcp;
  bool                                  frame_encryption;
  bool                                  hpd_missed;
  bool                                  rxsense_missed;
  bool                                  enhanced_link_check;
  bool                                  advanced_cipher_mode;
  bool                                  link_check_by_frame;
  bool                                  restart_authentication;
  bool                                  hdcp_check_needed;
  bool                                  hpd_check_needed;
  bool                                  key_read;
  bool                                  hdcp_start_needed;
  bool                                  hdcp_start;
  uint16_t                              hdcp_delay_ms;
  uint8_t                               retry_interval;
  uint8_t                               sink_caps;
  uint16_t                              sink_status;
  uint32_t                              hdcp_frame_counter;
  uint32_t                              revoked_ksv;
  uint8_t                               ksv_list[128*HDCP_BKSV_SIZE];
  uint8_t                               bcaps[HDCP_BCAPS_SIZE];
  uint8_t                               bstatus[HDCP_BSTATUS_SIZE];
  uint8_t                               error_daemon;
  KSV_t                                *revocation_list;
  hdcp_mode_t                           hdcp_mode;
  display_mode_t                        display_mode;
  struct task_struct                   *thread;
  struct task_struct                   *hpd_thread;

  struct i2c_adapter                   *i2c_adapter;
  struct i2c_client                    *edid_client;
  struct i2c_client                    *eddc_segment_reg_client;
  int                                   i2c_retry;

  uint32_t                              pipeline_id;
  uint32_t                              device_id;
  int                                   display_device_id;
  stm_display_device_h                  device;
  stm_display_output_h                  main_output;
  stm_display_output_h                  hdmi_output;
  stm_display_link_h                    link;
  void                                * hdmi_offset;
  void                                * hdcp_offset;

  int                                   irq;
  struct stmcore_vsync_cb               vsync_cb_info;
  uint32_t                              vsync_wait_count;
  wait_queue_head_t                     wait_queue;
  struct cdev                           cdev;
  struct device                       * class_device;
  wait_queue_head_t                     wait_q;
  struct mutex                          lock;
  struct cb_id                          link_id;
  struct cn_msg                       * link_msg;
  void                                * semlock;
  struct mutex                          proxy_lock;
  struct platform_device               *pdev;
  int                                   use_count;
};

/*
 * This is the declaration of the entrypoints for LINK management
 */
#if defined(SDK2_ENABLE_HDMI_TX_ATTRIBUTES)
int stmlink_create_class_device_files(struct stmlink *linkp, struct device *display_class_device);
#endif
#endif /* STMCORELINK_H */
