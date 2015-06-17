/***********************************************************************
 *
 * File: linkproxy_client.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

/* Define to prevent recursive inclusion */
#ifndef __LINKPROXY_CLIENT_H
#define __LINKPROXY_CLIENT_H

/* Includes --------------------------------------------------------------- */
#include <linux/stm/stmcorelink.h>

/* Exported Constants ----------------------------------------------------- */

/* more than 1 needs a thread waiting for connections */
#define MAX_CLIENTS                 1

/* Max User data lenght */
#define STM_DISPLAY_LINK_MAX_USER_DATA               31

static const unsigned int STM_DISPLAY_LINK_SOCKET_CMD_VALID = 0xA55AA55A;

#define BYTE_VALUE_1(x)  (x&0xFF)
#define BYTE_VALUE_2(x)  (x&0xFF00)>>8
#define BYTE_VALUE_3(x)  (x&0xFF0000)>>16
#define BYTE_VALUE_4(x)  (x&0xFF000000)>>24

typedef enum stm_display_link_hdcp_proxy_cmd_s
{
  STM_DISPLAY_LINK_PROXY_CMD_INVALID,
  STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_ENABLE,
  STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE,
  STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_ENHANCED_LINK_VERIF_ENABLE,
  STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL,
  STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_ENCRYPTION_START_DELAY,

  STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_ENABLE,
  STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE,
  STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_ENHANCED_LINK_VERIF_ENABLE,
  STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL,
  STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_ENCRYPTION_START_DELAY,

  STM_DISPLAY_LINK_PROXY_CMD_GET_LAST_TRANSACTION,
  STM_DISPLAY_LINK_PROXY_CMD_GET_STATUS,
  STM_DISPLAY_LINK_PROXY_CMD_SET_SRM,
  STM_DISPLAY_LINK_PROXY_CMD_GET_BCAPS,
  STM_DISPLAY_LINK_PROXY_CMD_GET_BSTATUS,
  STM_DISPLAY_LINK_PROXY_CMD_GET_DOWNSTREAM_KSV_LIST,
  STM_DISPLAY_LINK_PROXY_CMD_HPD_STATE_CHANGE_SIGNAL,
  STM_DISPLAY_LINK_PROXY_CMD_RXSENSE_STATE_CHANGE_SIGNAL,
  STM_DISPLAY_LINK_PROXY_CMD_HDCP_STATUS_CHANGE_SIGNAL,
  STM_DISPLAY_LINK_PROXY_CMD_UNKNOWN
} stm_display_link_hdcp_proxy_cmd_t;


/* Exported Macros -------------------------------------------------------- */

#ifdef DEBUG_SOCKET

#define REPORT_SOCKET(level, arg) REPORT(level, arg)
#define DERROR_SOCKET(id, err, msg) DERROR(id, err, msg)
#define PERROR(arg) perror(arg)

#else /* DEBUG_SOCKET */

#define REPORT_SOCKET(level, arg) { }
#define DERROR_SOCKET(id, err, msg) err
#define PERROR(arg) { }

#endif /* DEBUG_SOCKET */

#if 0
#define CHECK_CMD_DATA_VALIDITY(cmd_data)                       \
({                                                              \
  int error = 0;                                                \
  if( cmd_data.flags != STMCP_SOCKET_CMD_VALID )                \
  {                                                             \
    REPORT(1, ( "%s: Invalid command data!!\n"                  \
            , __FUNCTION__));                                   \
    error = -EBADMSG;                                           \
  }                                                             \
  error;                                                        \
})

#define SET_CMD_DATA_VALIDITY(cmd_data)                         \
  cmd_data.flags = STM_DISPLAY_LINK_SOCKET_CMD_VALID;           \

#endif

#define RESET_CMD_DATA(cmd_data)                                \
  memset(&cmd_data, 0, sizeof(stm_display_link_proxy_data_t));            \

/* Exported Variables ----------------------------------------------------- */

typedef struct stm_display_link_proxy_data_s
{
  stm_display_link_hdcp_proxy_cmd_t   cmd;
  //unsigned int                   lock;
  unsigned int                   linkp;
  int                            len;
  unsigned char                  *data;
} stm_display_link_proxy_data_t;

int stm_display_link_hdcp_proxy_client_set_ctrl ( struct stmlink* linkp,
    stm_display_link_ctrl_t ctrl,
    uint32_t value );

int stm_display_link_hdcp_proxy_client_get_ctrl (struct stmlink* linkp,
    stm_display_link_ctrl_t ctrl);

int stm_display_link_hdcp_proxy_client_get_last_transaction (struct stmlink* linkp);

int stm_display_link_hdcp_proxy_client_get_status ( struct stmlink* linkp);

int stm_display_link_hdcp_proxy_client_set_srm ( struct stmlink* linkp,
    uint8_t size,
    uint8_t *srm );

int stm_display_link_hdcp_proxy_client_read_sink_bcaps ( struct stmlink* linkp);

int stm_display_link_hdcp_proxy_client_read_sink_bstatus ( struct stmlink* linkp);

int stm_display_link_hdcp_proxy_client_get_downstream_ksv_list(struct stmlink* linkp,
    uint8_t *ksv_list,
    uint32_t size);
int stm_display_link_hdcp_proxy_client_send_hpd_state( struct stmlink* linkp,
    stm_display_link_hpd_state_t hpd_state);

int stm_display_link_hdcp_proxy_client_send_rxsense_state( struct stmlink* linkp,
    stm_display_link_rxsense_state_t rxsense_state);
#endif /* LINKPROXY_CLIENT_H */

