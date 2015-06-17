/***********************************************************************
 *
 * File: linkproxy_client.c
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <stm_common.h>
#include <stm_event.h>
#include <event_subscriber.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/connector.h>

#include <vibe_os.h>
#include <linkproxy_client.h>
#include <stm_display_link.h>
#include <linux/stm/stmcorelink.h>
extern void usleep(unsigned long us);
extern int send_event (struct stmlink* link, unsigned int type);

/**
 *      stm_display_link_hdcp_proxy_send_cmd - send comamnd through proxy client (netlink)
 *      @link: Pointer to the link structure
 *      @cmd_data: command data
 *
 *      RETURNS:
 *      0 if no error
 */
int stm_display_link_hdcp_proxy_send_cmd(struct stmlink* linkp,
    const stm_display_link_proxy_data_t * const cmd_data)
{
  struct cn_msg *m;
  m = kzalloc(sizeof(*m) + 6 + cmd_data->len, GFP_KERNEL);
  if (m) {
    memcpy(&m->id, &linkp->link_id, sizeof(m->id));
    m->seq = 0;
    m->len = cmd_data->len+6;
    m->ack =1;
    m->data[0]=cmd_data->cmd;
    m->data[1]=BYTE_VALUE_1(cmd_data->linkp);
    m->data[2]=BYTE_VALUE_2(cmd_data->linkp);
    m->data[3]=BYTE_VALUE_3(cmd_data->linkp);
    m->data[4]=BYTE_VALUE_4(cmd_data->linkp);
    m->data[5]=0;
    if (cmd_data->data!= NULL)
      memcpy(&(m->data[6]), cmd_data->data, cmd_data->len);
    LINKDBG(3,"proxy command = %d ; value = %d", m->data[0], m->data[6]);

    if (cn_netlink_send(m, 0, GFP_KERNEL) !=0){
     kfree(m);
     return -1;
    }

    kfree(m);
  }
  return 0;
}

/**
 *      stm_display_link_hdcp_proxy_client_set_ctrl - send comamnd controls through proxy client (netlink)
 *      @link: Pointer to the link structure
 *      @ctrl: command control
 *      @value: value of command control
 *
 *      RETURNS:
 *      0 if no error
 */
int stm_display_link_hdcp_proxy_client_set_ctrl ( struct stmlink* linkp,
    stm_display_link_ctrl_t ctrl,
    uint32_t value ) {

  stm_display_link_proxy_data_t link_proxy_data;
  int ret = 0;

  RESET_CMD_DATA(link_proxy_data);

  LINKDBG(3,"link =%p, Ctrl = %d ; value = %d", linkp, ctrl, value);

  if (linkp == NULL)
    return -EINVAL;

  switch (ctrl) {
  case STM_DISPLAY_LINK_CTRL_HDCP_ENABLE :
    mutex_lock(&(linkp->lock));
    if (value) {
      if ((linkp->hpd_state != STM_DISPLAY_LINK_HPD_STATE_HIGH) ||
         (linkp->rxsense && (linkp->rxsense_state != STM_DISPLAY_LINK_RXSENSE_STATE_ACTIVE))){
        mutex_unlock(&(linkp->lock));
        LINKDBG(1,"Link is not active");
        return -EPERM;
      }
    }
    mutex_unlock(&(linkp->lock));

    link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_ENABLE;
    link_proxy_data.linkp    = (unsigned int)linkp;
    link_proxy_data.len     = 1;
    link_proxy_data.data = kzalloc((sizeof(unsigned char)*link_proxy_data.len), GFP_KERNEL);
    if (link_proxy_data.data ==NULL)
      return -EINVAL;
    link_proxy_data.data[0] = value;
    LINKDBG(2,"proxy command = %d ; value = %d", link_proxy_data.cmd, link_proxy_data.data[0]);

    break;
  case STM_DISPLAY_LINK_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE :
    link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE;
    link_proxy_data.linkp    = (unsigned int)linkp;
    link_proxy_data.len     = 1;
    link_proxy_data.data = kzalloc((sizeof(unsigned char)*link_proxy_data.len), GFP_KERNEL);
    if (link_proxy_data.data ==NULL)
      return -EINVAL;
    link_proxy_data.data[0] = value;

    break;
  case STM_DISPLAY_LINK_CTRL_HDCP_ENHANCED_LINK_VERIFICATION_ENABLE :
    link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_ENHANCED_LINK_VERIF_ENABLE;
    link_proxy_data.linkp    = (unsigned int)linkp;
    link_proxy_data.len     = 1;
    link_proxy_data.data = kzalloc((sizeof(unsigned char)*link_proxy_data.len), GFP_KERNEL);
    if (link_proxy_data.data ==NULL)
      return -EINVAL;
    link_proxy_data.data[0] = value;
    break;
  case STM_DISPLAY_LINK_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL:
    link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL;
    link_proxy_data.linkp    = (unsigned int)linkp;
    link_proxy_data.len     = 1;
    link_proxy_data.data = kzalloc((sizeof(unsigned char)*link_proxy_data.len), GFP_KERNEL);
    if (link_proxy_data.data ==NULL)
      return -EINVAL;
    link_proxy_data.data[0] = value;

    break;
  case STM_DISPLAY_LINK_CTRL_HDCP_ENCRYPTION_START_DELAY :
    mutex_lock(&(linkp->lock));
    if (value > 500) {
      mutex_unlock(&(linkp->lock));
      LINKDBG(1, "Unable to set frame encryption delay : delay (%d) is too high",value);
      return -EPERM;
    }
    mutex_unlock(&(linkp->lock));
    LINKDBG(3, "Set frame encryption delay : delay (%d)",value);
    link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_ENCRYPTION_START_DELAY;
    link_proxy_data.linkp    = (unsigned int)linkp;
    link_proxy_data.len     = 2;
    link_proxy_data.data = kzalloc((sizeof(unsigned char)*link_proxy_data.len), GFP_KERNEL);
    if (link_proxy_data.data ==NULL)
      return -EINVAL;
    link_proxy_data.data[0] = BYTE_VALUE_1(value);
    link_proxy_data.data[1] = BYTE_VALUE_2(value);
    break;

  default:
    break;
  }

  mutex_lock(&(linkp->proxy_lock));
  if ( stm_display_link_hdcp_proxy_send_cmd(linkp, &link_proxy_data)!=0) {
    LINKDBG(1," %d  :Command not successfully transmitted", link_proxy_data.cmd);
    mutex_unlock(&(linkp->proxy_lock));
    kfree (link_proxy_data.data);
    return -EPERM;
  }
  /* wait for command completion*/
  if(vibe_os_down_semaphore(linkp->semlock) != 0) {
    mutex_unlock(&(linkp->proxy_lock));
    kfree (link_proxy_data.data);
    return -EINTR;
  }
  /* check daemon returned error*/
  ret=linkp->error_daemon;
  mutex_unlock(&(linkp->proxy_lock));


  kfree (link_proxy_data.data);
  return ret;
}

/**
 *      stm_display_link_hdcp_proxy_client_get_ctrl - Get comamnd controls through proxy client (netlink)
 *      @link: Pointer to the link structure
 *      @ctrl: command control
 *
 *      RETURNS:
 *      0 if no error
 */
int stm_display_link_hdcp_proxy_client_get_ctrl (struct stmlink* linkp,
    stm_display_link_ctrl_t ctrl) {

  stm_display_link_proxy_data_t link_proxy_data;
  int ret = 0;
  RESET_CMD_DATA(link_proxy_data);

  LINKDBG(3,"Ctrl = %d", ctrl);
  switch (ctrl) {
  case STM_DISPLAY_LINK_CTRL_HDCP_ENABLE :
    link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_ENABLE;
    break;

  case STM_DISPLAY_LINK_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE :
    link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE;
    break;

  case STM_DISPLAY_LINK_CTRL_HDCP_ENHANCED_LINK_VERIFICATION_ENABLE :
    link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_ENHANCED_LINK_VERIF_ENABLE;
    break;

  case STM_DISPLAY_LINK_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL :
    link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL;
    break;

  case STM_DISPLAY_LINK_CTRL_HDCP_ENCRYPTION_START_DELAY :
    link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_ENCRYPTION_START_DELAY;
    break;
  default:
    break;
  }
  link_proxy_data.linkp    = (unsigned int)linkp;
  mutex_lock(&(linkp->proxy_lock));
  if ( stm_display_link_hdcp_proxy_send_cmd(linkp, &link_proxy_data)!=0) {
    LINKDBG(1," %d  :Command not successfully transmitted", link_proxy_data.cmd);
    mutex_unlock(&(linkp->proxy_lock));
    return -EPERM;
  }
  /* wait for command completion*/
  if(vibe_os_down_semaphore(linkp->semlock) != 0) {
    mutex_unlock(&(linkp->proxy_lock));
    return -EINTR;
  }
  /* check daemon returned error*/
  ret=linkp->error_daemon;
  mutex_unlock(&(linkp->proxy_lock));

  return ret;
}

/**
 *      stm_display_link_hdcp_proxy_client_get_last_transaction - Get last HDCP transaction through proxy client (netlink)
 *      @link: Pointer to the link structure
 *
 *      RETURNS:
 *      0 if no error
 */
int stm_display_link_hdcp_proxy_client_get_last_transaction (struct stmlink* linkp) {

  stm_display_link_proxy_data_t link_proxy_data;
  int ret = 0;
  RESET_CMD_DATA(link_proxy_data);

  link_proxy_data.cmd = STM_DISPLAY_LINK_PROXY_CMD_GET_LAST_TRANSACTION;
  link_proxy_data.linkp    = (unsigned int)linkp;

  mutex_lock(&(linkp->proxy_lock));
  if ( stm_display_link_hdcp_proxy_send_cmd(linkp, &link_proxy_data)!=0) {
    LINKDBG(1," %d  :Command not successfully transmitted", link_proxy_data.cmd);
    mutex_unlock(&(linkp->proxy_lock));
    return -EPERM;
  }
  /* wait for command completion*/
  if(vibe_os_down_semaphore(linkp->semlock) != 0) {
    mutex_unlock(&(linkp->proxy_lock));
    return -EINTR;
  }
  /* check daemon returned error*/
  ret=linkp->error_daemon;
  mutex_unlock(&(linkp->proxy_lock));

  return ret;
}

/**
 *      stm_display_link_hdcp_proxy_client_get_status - Get HDCP transaction through proxy client (netlink)
 *      @link: Pointer to the link structure
 *
 *      RETURNS:
 *      0 if no error
 */
int stm_display_link_hdcp_proxy_client_get_status ( struct stmlink* linkp){

  stm_display_link_proxy_data_t link_proxy_data;
  int ret = 0;
  RESET_CMD_DATA(link_proxy_data);

  link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_GET_STATUS;
  link_proxy_data.linkp    = (unsigned int)linkp;
  mutex_lock(&(linkp->proxy_lock));
  if ( stm_display_link_hdcp_proxy_send_cmd(linkp, &link_proxy_data)!=0) {
    mutex_unlock(&(linkp->proxy_lock));
    LINKDBG(1," %d  :Command not successfully transmitted", link_proxy_data.cmd);
    return -EPERM;
  }

  /* wait for command completition*/
  if(vibe_os_down_semaphore(linkp->semlock) != 0) {
    mutex_unlock(&(linkp->proxy_lock));
    return -EINTR;
  }
  /* check daemon returned error*/
  ret=linkp->error_daemon;
  mutex_unlock(&(linkp->proxy_lock));


  return ret;
}

/**
 *      stm_display_link_hdcp_proxy_client_set_srm - Set SRM list through proxy client (netlink)
 *      @link: Pointer to the link structure
 *      @size: Pointer to the size list
 *      @srm:  Pointer to the srm list
 *
 *      RETURNS:
 *      0 if no error
 */
int stm_display_link_hdcp_proxy_client_set_srm ( struct stmlink* linkp,
    uint8_t size,
    uint8_t *srm ) {

  stm_display_link_proxy_data_t link_proxy_data;
  int ret = 0;
  RESET_CMD_DATA(link_proxy_data);
  link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_SET_SRM;
  link_proxy_data.linkp    = (unsigned int)linkp;
  link_proxy_data.len     = size*5;
  link_proxy_data.data = kzalloc((sizeof(unsigned char)*link_proxy_data.len), GFP_KERNEL);

  if (link_proxy_data.data ==NULL)
    return -EINVAL;

  memcpy(link_proxy_data.data, srm, link_proxy_data.len);

  mutex_lock(&(linkp->proxy_lock));
  if ( stm_display_link_hdcp_proxy_send_cmd(linkp, &link_proxy_data)!=0) {
    LINKDBG(1," %d  :Command not successfully transmitted", link_proxy_data.cmd);
    mutex_unlock(&(linkp->proxy_lock));
    kfree(link_proxy_data.data);
    return -EPERM;
  }
  /* wait for command completition*/
  if(vibe_os_down_semaphore(linkp->semlock) != 0) {
    mutex_unlock(&(linkp->proxy_lock));
    kfree (link_proxy_data.data);
    return -EINTR;
  }
  /* check daemon returned error*/
  ret=linkp->error_daemon;
  mutex_unlock(&(linkp->proxy_lock));


  kfree (link_proxy_data.data);
  return ret;
}

/**
 *      stm_display_link_hdcp_proxy_client_read_sink_bcaps - Get HDCP Sink bcaps through proxy client (netlink)
 *      @link: Pointer to the link structure
 *
 *      RETURNS:
 *      0 if no error
 */
int stm_display_link_hdcp_proxy_client_read_sink_bcaps ( struct stmlink* linkp){

  stm_display_link_proxy_data_t link_proxy_data;
  int ret = 0;
  RESET_CMD_DATA(link_proxy_data);

  link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_GET_BCAPS;
  link_proxy_data.linkp    = (unsigned int)linkp;
  mutex_lock(&(linkp->proxy_lock));
  if ( stm_display_link_hdcp_proxy_send_cmd(linkp, &link_proxy_data)!=0) {
    LINKDBG(1," %d  :Command not successfully transmitted", link_proxy_data.cmd);
    mutex_unlock(&(linkp->proxy_lock));
    return -EPERM;
  }

  /* wait for command completition*/
  if(vibe_os_down_semaphore(linkp->semlock) != 0) {
    mutex_unlock(&(linkp->proxy_lock));
    return -EINTR;
  }
  /* check daemon returned error*/
  ret=linkp->error_daemon;
  mutex_unlock(&(linkp->proxy_lock));

  if ( ret != 0 )
    return -ETIMEDOUT;

  return 0;
}

/**
 *      stm_display_link_hdcp_proxy_client_read_sink_bstatus - Get HDCP Sink bstatus through proxy client (netlink)
 *      @link: Pointer to the link structure
 *
 *      RETURNS:
 *      0 if no error
 */
int stm_display_link_hdcp_proxy_client_read_sink_bstatus ( struct stmlink* linkp){

  stm_display_link_proxy_data_t link_proxy_data;
  int ret = 0;
  RESET_CMD_DATA(link_proxy_data);

  link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_GET_BSTATUS;
  link_proxy_data.linkp    = (unsigned int)linkp;
  mutex_lock(&(linkp->proxy_lock));
  if ( stm_display_link_hdcp_proxy_send_cmd(linkp, &link_proxy_data)!=0) {
    LINKDBG(1," %d  :Command not successfully transmitted", link_proxy_data.cmd);
    mutex_unlock(&(linkp->proxy_lock));
    return -EPERM;
  }

  /* wait for command completition*/
  if(vibe_os_down_semaphore(linkp->semlock) != 0) {
    mutex_unlock(&(linkp->proxy_lock));
    return -EINTR;
  }
  /* check daemon returned error*/
  ret=linkp->error_daemon;
  mutex_unlock(&(linkp->proxy_lock));

  if ( ret != 0 )
    return -ETIMEDOUT;

  return 0;
}

/**
 *      stm_display_link_hdcp_proxy_client_get_downstream_ksv_list - Get downstream KSV list through proxy client (netlink)
 *      @link: Pointer to the link structure
 *      @srm:  Pointer to the KSV list
 *      @size: Pointer to the size list
 *
 *      RETURNS:
 *      0 if no error
 */
int stm_display_link_hdcp_proxy_client_get_downstream_ksv_list(struct stmlink* linkp,
    uint8_t *ksv_list,
    uint32_t size) {

  stm_display_link_proxy_data_t link_proxy_data;
  int ret = 0;
  RESET_CMD_DATA(link_proxy_data);
  link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_GET_DOWNSTREAM_KSV_LIST;
  link_proxy_data.linkp    = (unsigned int)linkp;

  mutex_lock(&(linkp->proxy_lock));
  if ( stm_display_link_hdcp_proxy_send_cmd(linkp, &link_proxy_data)!=0) {
    LINKDBG(1," %d  :Command not successfully transmitted", link_proxy_data.cmd);
    mutex_unlock(&(linkp->proxy_lock));
    return -EPERM;
  }
  /* wait for command completition*/
  if(vibe_os_down_semaphore(linkp->semlock) != 0) {
    mutex_unlock(&(linkp->proxy_lock));
    return -EINTR;
  }
  /* check daemon returned error*/
  ret=linkp->error_daemon;
  mutex_unlock(&(linkp->proxy_lock));

  if(ret != 0)
    return ret;

  memcpy(&(ksv_list[0]), &(linkp->ksv_list[0]), (size>=127)?(127*HDCP_BKSV_SIZE):(size*HDCP_BKSV_SIZE));

  return 0;
}
/**
 *      stm_display_link_hdcp_proxy_client_send_hpd_state - Set the updated HPD state through proxy client (netlink)
 *      @link: Pointer to the link structure
 *      @hpd_state:  Current hpd state.
 *
 *      RETURNS:
 *      0 if no error
 */
int stm_display_link_hdcp_proxy_client_send_hpd_state(struct stmlink* linkp,
    stm_display_link_hpd_state_t hpd_state){

  stm_display_link_proxy_data_t link_proxy_data;
  int ret = 0;
  RESET_CMD_DATA(link_proxy_data);
  link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_HPD_STATE_CHANGE_SIGNAL;
  link_proxy_data.linkp    = (unsigned int)linkp;
  link_proxy_data.len     = 1;
  link_proxy_data.data = kzalloc((sizeof(unsigned char)*link_proxy_data.len), GFP_KERNEL);
  if (link_proxy_data.data ==NULL)
      return -EINVAL;

  link_proxy_data.data[0] = hpd_state;

  mutex_lock(&(linkp->proxy_lock));
  if ( stm_display_link_hdcp_proxy_send_cmd(linkp, &link_proxy_data)!=0) {
        LINKDBG(1," %d  :Command not successfully transmitted", link_proxy_data.cmd);
        mutex_unlock(&(linkp->proxy_lock));
        kfree(link_proxy_data.data);
        return -EPERM;
  }
  /* wait for command completition*/
  if(vibe_os_down_semaphore(linkp->semlock) != 0){
    mutex_unlock(&(linkp->proxy_lock));
    kfree (link_proxy_data.data);
    return -EINTR;
  }
  /* check daemon returned error*/
  ret=linkp->error_daemon;
  mutex_unlock(&(linkp->proxy_lock));

  kfree (link_proxy_data.data);
  return 0;
}

/**
 *      stm_display_link_hdcp_proxy_client_send_hpd_state - Set the updated HPD state through proxy client (netlink)
 *      @link: Pointer to the link structure
 *      @hpd_state:  Current hpd state.
 *
 *      RETURNS:
 *      0 if no error
 */
int stm_display_link_hdcp_proxy_client_send_rxsense_state(struct stmlink* linkp,
     stm_display_link_rxsense_state_t rxsense_state){

  stm_display_link_proxy_data_t link_proxy_data;
  int ret = 0;
  RESET_CMD_DATA(link_proxy_data);
  link_proxy_data.cmd     = STM_DISPLAY_LINK_PROXY_CMD_RXSENSE_STATE_CHANGE_SIGNAL;
  link_proxy_data.linkp    = (unsigned int)linkp;

  link_proxy_data.len     = 1;
  link_proxy_data.data = kzalloc((sizeof(unsigned char)*link_proxy_data.len), GFP_KERNEL);
  if (link_proxy_data.data ==NULL)
      return -EINVAL;

  link_proxy_data.data[0] = rxsense_state;

  mutex_lock(&(linkp->proxy_lock));
  if ( stm_display_link_hdcp_proxy_send_cmd(linkp, &link_proxy_data)!=0) {
        LINKDBG(1," %d  :Command not successfully transmitted", link_proxy_data.cmd);
        mutex_unlock(&(linkp->proxy_lock));
        kfree(link_proxy_data.data);
        return -EPERM;
  }
  /* wait for command completition*/
  if(vibe_os_down_semaphore(linkp->semlock) != 0){
    mutex_unlock(&(linkp->proxy_lock));
    kfree (link_proxy_data.data);
    return -EINTR;
  }

  ret=linkp->error_daemon;
  mutex_unlock(&(linkp->proxy_lock));

  kfree (link_proxy_data.data);

  return ret;
}
/**
 *      cn_link_callback - Netlink connector callback
 *      @cn_msg: netlink connector message
 *      @nsp:  netlink callback params
 *
 *      RETURNS:
 *      0 if no error
 */
void cn_link_callback(struct cn_msg *msg, struct netlink_skb_parms *nsp)
{
  stm_display_link_hdcp_proxy_cmd_t   cmd;
  struct stmlink* linkp;

  cmd = msg->data[0];
  linkp =(struct stmlink*)((msg->data[1])|(msg->data[2]<<8)|(msg->data[3]<<16 )|(msg->data[4]<<24));

  switch (cmd){
  case STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_ENABLE:
  case STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE:
  case STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_ENHANCED_LINK_VERIF_ENABLE:
  case STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL:
  case STM_DISPLAY_LINK_PROXY_CMD_SET_CTRL_HDCP_ENCRYPTION_START_DELAY:
  case STM_DISPLAY_LINK_PROXY_CMD_SET_SRM:
  case STM_DISPLAY_LINK_PROXY_CMD_HPD_STATE_CHANGE_SIGNAL:
  case STM_DISPLAY_LINK_PROXY_CMD_RXSENSE_STATE_CHANGE_SIGNAL:
  {
    linkp->error_daemon = msg->data[5];
    vibe_os_up_semaphore (linkp->semlock);
    break;
  }
  case STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_ENABLE:
  {
    linkp->error_daemon = msg->data[5];
    mutex_lock(&(linkp->lock));
    linkp->hdcp = msg->data[6];
    mutex_unlock(&(linkp->lock));
    vibe_os_up_semaphore (linkp->semlock);
    break;
  }
  case STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE:
  {
    linkp->error_daemon = msg->data[5];
    mutex_lock(&(linkp->lock));
    linkp->frame_encryption = msg->data[6];
    mutex_unlock(&(linkp->lock));
    vibe_os_up_semaphore (linkp->semlock);
    break;
  }
  case STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_ENHANCED_LINK_VERIF_ENABLE:
  {
    linkp->error_daemon = msg->data[5];
    mutex_lock(&(linkp->lock));
    linkp->enhanced_link_check = msg->data[6];
    mutex_unlock(&(linkp->lock));
    vibe_os_up_semaphore (linkp->semlock);
    break;
  }
  case STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL:
  {
    linkp->error_daemon = msg->data[5];
    mutex_lock(&(linkp->lock));
    linkp->retry_interval = msg->data[6];
    mutex_unlock(&(linkp->lock));
    vibe_os_up_semaphore (linkp->semlock);
    break;
  }
  case STM_DISPLAY_LINK_PROXY_CMD_GET_CTRL_HDCP_ENCRYPTION_START_DELAY:
  {
    linkp->error_daemon = msg->data[5];
    mutex_lock(&(linkp->lock));
    linkp->hdcp_delay_ms = (msg->data[6]|(msg->data[7]<<8));
    mutex_unlock(&(linkp->lock));
    vibe_os_up_semaphore (linkp->semlock);
    break;
  }
  case STM_DISPLAY_LINK_PROXY_CMD_GET_LAST_TRANSACTION:
  {
    linkp->error_daemon = msg->data[5];
    mutex_lock(&(linkp->lock));
    linkp->last_transaction = msg->data[6];
    mutex_unlock(&(linkp->lock));
    vibe_os_up_semaphore (linkp->semlock);
    break;
  }
  case STM_DISPLAY_LINK_PROXY_CMD_GET_STATUS:
  {
    linkp->error_daemon = msg->data[5];
    linkp->status = msg->data[6];
    vibe_os_up_semaphore (linkp->semlock);
    break;
  }
  case STM_DISPLAY_LINK_PROXY_CMD_GET_BCAPS:
  {
    memcpy(&(linkp->bcaps[0]), &(msg->data[6]), HDCP_BCAPS_SIZE);
    linkp->error_daemon = msg->data[5];
    vibe_os_up_semaphore (linkp->semlock);
    break;
  }
  case STM_DISPLAY_LINK_PROXY_CMD_GET_BSTATUS:
  {
    memcpy(&(linkp->bstatus[0]), &(msg->data[6]), HDCP_BSTATUS_SIZE);
    linkp->error_daemon = msg->data[5];
    vibe_os_up_semaphore (linkp->semlock);
    break;
  }
  case STM_DISPLAY_LINK_PROXY_CMD_GET_DOWNSTREAM_KSV_LIST:
  {
    memcpy(&(linkp->ksv_list[0]), &(msg->data[6]), 128*HDCP_BKSV_SIZE);
    linkp->error_daemon = msg->data[5];
    vibe_os_up_semaphore (linkp->semlock);
    break;
  }
  case STM_DISPLAY_LINK_PROXY_CMD_HDCP_STATUS_CHANGE_SIGNAL:
  {
    send_event(linkp, STM_DISPLAY_LINK_HDCP_STATUS_CHANGE_EVT);
    break;
  }
  default :
    break;
 }
}
