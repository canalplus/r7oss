/***********************************************************************
 *
 * File: stm_display_link.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STM_DISPLAY_LINK_H
#define STM_DISPLAY_LINK_H

#if defined(__cplusplus)
extern "C" {
#endif

  typedef struct stm_display_link_s * stm_display_link_h ;

#define STM_DISPLAY_LINK_HPD_STATE_CHANGE_EVT (1 << 0)
#define STM_DISPLAY_LINK_RXSENSE_STATE_CHANGE_EVT (1 << 2)
#define STM_DISPLAY_LINK_HDCP_STATUS_CHANGE_EVT (1 << 3)

  typedef enum stm_display_link_type_e {
    STM_DISPLAY_LINK_TYPE_HDMI,         // HDMI link
    STM_DISPLAY_LINK_TYPE_DP,           // Display port link
  } stm_display_link_type_t;

  typedef enum stm_display_link_ctrl_e {
    STM_DISPLAY_LINK_CTRL_RXSENSE_ENABLE,                               // Enable/Disable RxSense
    STM_DISPLAY_LINK_CTRL_DISPLAY_MODE,                                 // Set Display and fow data mode transmitted on the link
    STM_DISPLAY_LINK_CTRL_HDCP_START,                                   // Start HDCP even if HDMI/DVI are not transmitted, authentication will be started once TMDS data are running.
    STM_DISPLAY_LINK_CTRL_HDCP_ENABLE,                                  // Enable/Disable HDCP
    STM_DISPLAY_LINK_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE,                 // Enable/Disable Frame Encryption
    STM_DISPLAY_LINK_CTRL_HDCP_ENHANCED_LINK_VERIFICATION_ENABLE,       // Enable/Disable Enhanced link encryption (if supported by the sink)
    STM_DISPLAY_LINK_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL,           // Set the retry interval to wait before retry another authentication
    STM_DISPLAY_LINK_CTRL_HDCP_ENCRYPTION_START_DELAY                   // Set the delay before to set encryption
  } stm_display_link_ctrl_t;

  typedef enum stm_display_link_hpd_state_e {
    STM_DISPLAY_LINK_HPD_STATE_LOW,   // HPD signal low, nothing connected
    STM_DISPLAY_LINK_HPD_STATE_HIGH   // HPD signal high, sink connected
  } stm_display_link_hpd_state_t;

  typedef enum stm_display_link_rxsense_state_e {
    STM_DISPLAY_LINK_RXSENSE_STATE_INACTIVE,   // Receiver/sink inactive
    STM_DISPLAY_LINK_RXSENSE_STATE_ACTIVE      // Receiver/sink active
  } stm_display_link_rxsense_state_t;

  typedef enum stm_display_link_hdcp_status_e {
    STM_DISPLAY_LINK_HDCP_STATUS_DISABLED,                      // Authentication disable
    STM_DISPLAY_LINK_HDCP_STATUS_IN_PROGRESS,                   // Authentication in progress
    STM_DISPLAY_LINK_HDCP_STATUS_SUCCESS_ENCRYPTING,            // Authenticated and encrypting
    STM_DISPLAY_LINK_HDCP_STATUS_SUCCESS_NOT_ENCRYPTING,        // Authenticated and encryption disable
    STM_DISPLAY_LINK_HDCP_STATUS_FAIL_NO_ACK,                   // HDCP I2C access failure (no ack)
    STM_DISPLAY_LINK_HDCP_STATUS_FAIL_BKSV_INVALID,             // Invalid BKSV
    STM_DISPLAY_LINK_HDCP_STATUS_FAIL_KSV_REVOKED,              // Revoked KSV
    STM_DISPLAY_LINK_HDCP_STATUS_FAIL_MAX_CASCADE_EXCEEDED,     // Max cascade exceeded
    STM_DISPLAY_LINK_HDCP_STATUS_FAIL_MAX_DEVICE_COUNT_EXCEEDED,// Max device count exceeded
    STM_DISPLAY_LINK_HDCP_STATUS_FAIL_V_CHECK,                  // V' check failed
    STM_DISPLAY_LINK_HDCP_STATUS_FAIL_R0_CHECK,                 // R0 Check failed
    STM_DISPLAY_LINK_HDCP_STATUS_FAIL_FIFO_NOT_READY,           // Downstream Repeater fifo is not ready
    STM_DISPLAY_LINK_HDCP_STATUS_FAIL_WDT_TIMEOUT,              // Downstream wait exceeds 5 seconds
    STM_DISPLAY_LINK_HDCP_STATUS_FAIL_LINK_INTEGRITY,           // Link integrity Check (Ri') failed
    STM_DISPLAY_LINK_HDCP_STATUS_FAIL_ENHANCED_LINK_VERIFICATION// Enhanced link verification (Pj') failed
  } stm_display_link_hdcp_status_t;

  typedef enum stm_display_link_hdcp_transaction_e
  {
    STM_DISPLAY_LINK_HDCP_TRANSACTION_NONE,             // No transaction
    STM_DISPLAY_LINK_HDCP_TRANSACTION_WR_AKSV,          // Write AKSV
    STM_DISPLAY_LINK_HDCP_TRANSACTION_WR_AN,            // Write An
    STM_DISPLAY_LINK_HDCP_TRANSACTION_WR_AINFO,         // Write Ainfo
    STM_DISPLAY_LINK_HDCP_TRANSACTION_RD_BCAPS,         // Read BCaps
    STM_DISPLAY_LINK_HDCP_TRANSACTION_RD_BKSV,          // Read BKSV
    STM_DISPLAY_LINK_HDCP_TRANSACTION_RD_R0,            // Read R0
    STM_DISPLAY_LINK_HDCP_TRANSACTION_RD_BSTATUS,       // Read Bstatus
    STM_DISPLAY_LINK_HDCP_TRANSACTION_RD_KSV_LIST,      // Read KSV list
    STM_DISPLAY_LINK_HDCP_TRANSACTION_RD_V,             // Read V
    STM_DISPLAY_LINK_HDCP_TRANSACTION_RD_Ri,            // Read Ri
    STM_DISPLAY_LINK_HDCP_TRANSACTION_RD_Pj             // Read Pj
  } stm_display_link_hdcp_transaction_t;


  typedef struct stm_display_link_capability_s
  {
    bool rxsense;
  } stm_display_link_capability_t;

  typedef uint8_t stm_display_link_edid_block_t[128];

  int
  stm_display_link_open  (
      uint32_t                  id,
      stm_display_link_h       *link
  );

  int
  stm_display_link_close (
      stm_display_link_h        link
  );

  int
  stm_display_link_get_capability (
      stm_display_link_h                link,
      stm_display_link_capability_t    *capability
  );

  int
  stm_display_link_get_type (
      stm_display_link_h        link,
      stm_display_link_type_t  *type
  );


  int
  stm_display_link_set_ctrl (
      stm_display_link_h        link,
      stm_display_link_ctrl_t   ctrl,
      uint32_t                  value
  );


  int
  stm_display_link_get_ctrl (
      stm_display_link_h        link,
      stm_display_link_ctrl_t   ctrl,
      uint32_t                 *value
  );

  int
  stm_display_link_hpd_get_state (
      stm_display_link_h                link,
      stm_display_link_hpd_state_t     *state
  );

  int
  stm_display_link_rxsense_get_state (
      stm_display_link_h                link,
      stm_display_link_rxsense_state_t *state
  );

  int
  stm_display_link_edid_read (
      stm_display_link_h                link,
      uint8_t                           block_number,
      stm_display_link_edid_block_t     edid_block
  );

  int
  stm_display_link_hdcp_set_srm (
      stm_display_link_h        link,
      uint8_t                   size,
      uint8_t                  *srm
  );

  int
  stm_display_link_hdcp_get_last_transaction (
      stm_display_link_h                        link,
      stm_display_link_hdcp_transaction_t      *transaction
  );

  int
  stm_display_link_hdcp_get_status (
      stm_display_link_h                link,
      stm_display_link_hdcp_status_t   *status
  );

  int
  stm_display_link_hdcp_get_sink_caps_info (
      stm_display_link_h        link,
      uint8_t                   *caps,
      uint32_t                  timeout
  );

  int
  stm_display_link_hdcp_get_sink_status_info (
      stm_display_link_h        link,
      uint16_t                 *status,
      uint32_t                  timeout
  );

  int
  stm_display_link_hdcp_get_downstream_ksv_list (
      stm_display_link_h        link,
      uint8_t                  *ksv_list,
      uint32_t                  size
  );

#if defined(__cplusplus)
}
#endif

#endif /* STM_DISPLAY_LINK_H */
