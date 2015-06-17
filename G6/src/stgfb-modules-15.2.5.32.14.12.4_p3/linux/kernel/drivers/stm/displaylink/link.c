/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/displaylink/link.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/i2c.h>

#include <linkproxy_client.h>

#include <stm_display.h>
#include <linux/stm/stmcorelink.h>

#include <stm_display_link.h>

extern int send_event (struct stmlink* link, unsigned int event);


/******************************************************************************
 * Functions to check the EDID for a valid mode and enable the HDMI output
 */

/**
 *      stm_display_link_edid_read - Displaylink API, reads EDID block number
 *      @link: Link handle
 *      @number: Block number to read
 *      @edid_block: Reference to the table to store the reading data
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 *              -EFAULT  Invalid pointer address
 *              -EIO     I2C error
 *              -EPERM   Operation not permitted
 */
int stm_display_link_edid_read ( stm_display_link_h link,
    uint8_t block_number,
    stm_display_link_edid_block_t edid_block ) {

  struct stmlink* linkp;
  int retry, res= 0;
  unsigned char segment;
  unsigned char startreg;
  struct i2c_msg msgs[2],msgs1[3];

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if(!linkp->edid_client)
    {
      LINKDBG(1,"No EDID client found");
      return -EPERM;
    }

  mutex_lock(&(linkp->lock));
  if (linkp->hpd_state != STM_DISPLAY_LINK_HPD_STATE_HIGH){
      mutex_unlock(&(linkp->lock));
      LINKDBG(1,"Link is not connected");
      return -EPERM;
  }
  mutex_unlock(&(linkp->lock));

  LINKDBG(4,"Read Edid block number %d",block_number);
  if (( block_number==0)|| ( block_number==1))
    {
      msgs[0].addr  = linkp->edid_client->addr;
      msgs[0].flags = linkp->edid_client->flags & I2C_M_TEN;
      msgs[0].len   = 1;
      msgs[0].buf   = &startreg;
      msgs[1].addr  = linkp->edid_client->addr ;
      msgs[1].flags = (linkp->edid_client->flags & I2C_M_TEN) | I2C_M_RD;
      msgs[1].len   = sizeof(stm_display_link_edid_block_t) ;
      msgs[1].buf   = (void *)edid_block;
      startreg = sizeof(stm_display_link_edid_block_t) * block_number;

      for(retry=0;retry<linkp->i2c_retry;retry++)
        {
          res = 0;

          /*
           * If we are retrying, wait and relax a while to give the TV a chance to
           * recover from any error.
           */
          if(retry>0)
            {
              set_current_state(TASK_UNINTERRUPTIBLE);
              schedule_timeout(HZ/10);
            }
          if(i2c_transfer(linkp->edid_client->adapter, &msgs[0], 2) != 2)
            {
              LINKDBG(4,"I2C transfer fails res = %d",res);
              res = -EIO;
              continue;
            }

          /*
           * Success, break out of the retry loop.
           */
          break;
        }
    }
  else
    {
      msgs1[0].addr  = linkp->eddc_segment_reg_client->addr;
      msgs1[0].flags = linkp->eddc_segment_reg_client->flags & I2C_M_TEN;
      msgs1[0].len   = 1;
      msgs1[0].buf   = &segment;
      msgs1[1].addr  = linkp->edid_client->addr;
      msgs1[1].flags = linkp->edid_client->flags & I2C_M_TEN;
      msgs1[1].len   = 1;
      msgs1[1].buf   = &startreg;
      msgs1[2].addr  = linkp->edid_client->addr;
      msgs1[2].flags = (linkp->edid_client->flags & I2C_M_TEN) | I2C_M_RD;
      msgs1[2].len   = sizeof(stm_display_link_edid_block_t) ;

      /*
       * Move to the appropriate block from the corresponding page
       */
      startreg = sizeof(stm_display_link_edid_block_t) * (block_number % 2);
      segment = block_number/2;

      msgs1[2].buf =  (void *)edid_block;
      for(retry=0;retry<linkp->i2c_retry;retry++)
        {
          res = 0;

          /*
           * If we are retrying, wait and relax a while to give the TV a chance to
           * recover from any error.
           */
          if(retry>0)
            {
              set_current_state(TASK_UNINTERRUPTIBLE);
              schedule_timeout(HZ/10);
            }

          if(i2c_transfer(linkp->edid_client->adapter, msgs1, 3) != 3)
            {
              LINKDBG(4,"I2C Transaction Error res=%d",res);
              res = -EIO;
              continue;
            }
          /*
           * Success, break out of the retry loop.
           */
          break;
        }
    }

  if(res<0)
    return res;

  return 0;
}

/**
 *      stm_display_link_get_capability - Displaylink API, Get the capability of the link
 *      @link: Link handle
 *      @capability: Pointer to the stm_display_link_capability_t structure to store the result
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 *              -EFAULT  Invalid pointer address
 */
int stm_display_link_get_capability ( stm_display_link_h link,
    stm_display_link_capability_t *capability ) {
  struct stmlink* linkp;

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if (capability == NULL)
    return -EFAULT;

  mutex_lock(&(linkp->lock));
  capability->rxsense = linkp->capability.rxsense ;
  mutex_unlock(&(linkp->lock));

  return 0;
}

/**
 *      stm_display_link_get_type - Displaylink API, returns the link type of the handle
 *      @link: Link handle
 *      @type: Pointer to the type struct to store the result
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 *              -EFAULT  Invalid pointer address
 */
int stm_display_link_get_type ( stm_display_link_h link,
    stm_display_link_type_t *type ) {
  struct stmlink* linkp;

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if (type == NULL)
    return -EFAULT;

  mutex_lock(&(linkp->lock));
  *type = linkp->type ;
  mutex_unlock(&(linkp->lock));

  return 0;
}

/**
 *      stm_display_link_set_ctrl - Displaylink API, set the specified control
 *      @link: Link handle
 *      @ctrl: Control type to be set
 *      @value: Control value
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 *              -EPERM   Operation not permitted
 */
int stm_display_link_set_ctrl ( stm_display_link_h link,
    stm_display_link_ctrl_t ctrl,
    uint32_t value ) {
  struct stmlink* linkp;
  int retVal=0;

  LINKDBG(3,"Ctrl = %d ; value = %d", ctrl, value);

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  switch (ctrl) {
  case STM_DISPLAY_LINK_CTRL_RXSENSE_ENABLE :
    mutex_lock(&(linkp->lock));
    linkp->rxsense = (value!=0);
    mutex_unlock(&(linkp->lock));
    retVal = send_event(linkp, STM_DISPLAY_LINK_RXSENSE_STATE_CHANGE_EVT);
    return retVal;
  case STM_DISPLAY_LINK_CTRL_DISPLAY_MODE :
    mutex_lock(&(linkp->lock));
    linkp->display_mode = value;
    mutex_unlock(&(linkp->lock));
    break;
  case STM_DISPLAY_LINK_CTRL_HDCP_START :
    mutex_lock(&(linkp->lock));
    linkp->hdcp_start_needed = value;
    mutex_unlock(&(linkp->lock));
    break;
  case STM_DISPLAY_LINK_CTRL_HDCP_ENABLE :
  case STM_DISPLAY_LINK_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE :
  case STM_DISPLAY_LINK_CTRL_HDCP_ENHANCED_LINK_VERIFICATION_ENABLE :
  case STM_DISPLAY_LINK_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL:
  case STM_DISPLAY_LINK_CTRL_HDCP_ENCRYPTION_START_DELAY :
    retVal = stm_display_link_hdcp_proxy_client_set_ctrl(linkp, ctrl, value);
    return retVal;
  default:
    break;
  }
  return 0;
}

/**
 *      stm_display_link_get_ctrl - Displaylink API, get the specified control value
 *      @link: Link handle
 *      @ctrl: Control type to be set
 *      @value: Pointer to store the control value
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 *              -EPERM   Operation not permitted
 */
int stm_display_link_get_ctrl ( stm_display_link_h link,
    stm_display_link_ctrl_t ctrl,
    uint32_t *value ) {
  struct stmlink* linkp;

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if (value == NULL)
    return -EFAULT;

  switch (ctrl) {
  case STM_DISPLAY_LINK_CTRL_HDCP_ENABLE :
  case STM_DISPLAY_LINK_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE:
  case STM_DISPLAY_LINK_CTRL_HDCP_ENHANCED_LINK_VERIFICATION_ENABLE :
  case STM_DISPLAY_LINK_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL :
  case STM_DISPLAY_LINK_CTRL_HDCP_ENCRYPTION_START_DELAY :
    if (stm_display_link_hdcp_proxy_client_get_ctrl(linkp, ctrl) <0)
      return -EPERM;
  default:
    break;
  }

  mutex_lock(&(linkp->lock));
  switch (ctrl) {
  case STM_DISPLAY_LINK_CTRL_RXSENSE_ENABLE :
    *value = linkp->rxsense;
    break;

  case STM_DISPLAY_LINK_CTRL_DISPLAY_MODE :
    *value = linkp->display_mode;
    break;

  case STM_DISPLAY_LINK_CTRL_HDCP_START:
    *value = linkp->hdcp_start_needed;
    break;

  case STM_DISPLAY_LINK_CTRL_HDCP_ENABLE :
    *value = (linkp->hdcp)?1:0;
    break;

  case STM_DISPLAY_LINK_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE :
    *value = linkp->frame_encryption;
    break;

  case STM_DISPLAY_LINK_CTRL_HDCP_ENHANCED_LINK_VERIFICATION_ENABLE :
    *value = linkp->enhanced_link_check;
    break;

  case STM_DISPLAY_LINK_CTRL_HDCP_AUTHENTICATION_RETRY_INTERVAL :
      *value = linkp->retry_interval;
      break;

  case STM_DISPLAY_LINK_CTRL_HDCP_ENCRYPTION_START_DELAY :
    *value = linkp->hdcp_delay_ms;
    break;

  default:
    break;
  }
  mutex_unlock(&(linkp->lock));
  return 0;
}

/**
 *      stm_display_link_hdcp_get_last_transaction - Displaylink API, get last executed I2C transaction
 *      @link: Link handle
 *      @transaction: Pointer to store the last transaction
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 *              -EFAULT  Invalid pointer address
 *              -EPERM   Operation not permitted
 */
int stm_display_link_hdcp_get_last_transaction ( stm_display_link_h link,
    stm_display_link_hdcp_transaction_t    *transaction) {
  struct stmlink* linkp;

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if (transaction == NULL)
    return -EFAULT;

  if (stm_display_link_hdcp_proxy_client_get_last_transaction(linkp)<0)
    return -EPERM;

  mutex_lock(&(linkp->lock));
  *transaction = linkp->last_transaction;
  mutex_unlock(&(linkp->lock));

  return 0;

}

/**
 *      stm_display_link_hpd_get_state - Displaylink API, get HPD state
 *      @link: Link handle
 *      @state: Pointer to store the HPD state
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 *              -EFAULT  Invalid pointer address
 */
int stm_display_link_hpd_get_state ( stm_display_link_h link,
    stm_display_link_hpd_state_t *state ) {
  struct stmlink* linkp;

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if (state == NULL)
    return -EFAULT;

  mutex_lock(&(linkp->lock));
  *state = linkp->hpd_state;
  mutex_unlock(&(linkp->lock));

  return 0;
}

/**
 *      stm_display_link_rxsense_get_state - Displaylink API, get RxSense state
 *      @link: Link handle
 *      @state: Pointer to store the RxSense state
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 *              -EFAULT  Invalid pointer address
 */
int stm_display_link_rxsense_get_state ( stm_display_link_h link,
    stm_display_link_rxsense_state_t *state ) {
  struct stmlink* linkp;

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if (state == NULL)
    return -EFAULT;

  mutex_lock(&(linkp->lock));
  *state = linkp->rxsense_state;
  mutex_unlock(&(linkp->lock));

  return 0;
}

/**
 *      stm_display_link_hdcp_set_srm - Displaylink API, send revoked KSV list contains in SRM data
 *      @link: Link handle
 *      @size: Number (in byte) of the data
 *      @srm: Pointer to the KSV list to store in the revoked list
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 *              -EFAULT  Invalid pointer address
 *              -EPERM   Operation not permitted
 */
int stm_display_link_hdcp_set_srm ( stm_display_link_h link,
    uint8_t size,
    uint8_t *srm ) {
  struct stmlink* linkp;

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if (srm == NULL)
    return -EFAULT;

  if (stm_display_link_hdcp_proxy_client_set_srm(linkp,size, srm)<0)
    return -EPERM;

  return 0;
}

/**
 *      stm_display_link_hdcp_get_status - Displaylink API, get HDCP status
 *      @link: Link handle
 *      @status: Pointer to store the status
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 *              -EFAULT  Invalid pointer address
 *              -EPERM   Operation not permitted
 */
int stm_display_link_hdcp_get_status ( stm_display_link_h link,
    stm_display_link_hdcp_status_t *status ) {
  struct stmlink* linkp;

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if (status == NULL)
    return -EFAULT;

  if (stm_display_link_hdcp_proxy_client_get_status(linkp)<0)
    return -EPERM;

  *status = linkp->status;

  return 0;
}

/**
 *      stm_display_link_hdcp_get_sink_caps_info - Displaylink API, get sink HDCP caps register
 *      @link: Link handle
 *      @caps: Pointer to store the result
 *      @timeout: Timeout (in ms) to perform the reading operation
 *
 *      RETURNS:
 *              0:         No error
 *              -EINVAL    Invalid link handle
 *              -EFAULT    Invalid pointer address
 *              -ETIMEDOUT Timeout of the operation
 *              -EPERM     Operation not permitted
 */
int stm_display_link_hdcp_get_sink_caps_info ( stm_display_link_h link,
    uint8_t *caps,
    uint32_t timeout) {
  struct stmlink* linkp;
  int save_timeout, ret = 0 ;

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if (caps == NULL)
    return -EFAULT;

  mutex_lock(&(linkp->lock));
  if (linkp->hpd_state != STM_DISPLAY_LINK_HPD_STATE_HIGH){
      mutex_unlock(&(linkp->lock));
      LINKDBG(1,"Link is not connected");
      return -EPERM;
  }
  mutex_unlock(&(linkp->lock));

  if (timeout != 0) {
      mutex_lock(&(linkp->lock));
      save_timeout = linkp->i2c_adapter->timeout;
      linkp->i2c_adapter->timeout = timeout;
      mutex_unlock(&(linkp->lock));
  }

  if ((ret=stm_display_link_hdcp_proxy_client_read_sink_bcaps(linkp))==0) {
    *caps = linkp->bcaps[0];
  }

  if (timeout != 0) {
      mutex_lock(&(linkp->lock));
      linkp->i2c_adapter->timeout = save_timeout;
      mutex_unlock(&(linkp->lock));
  }

  return ret;
}

/**
 *      stm_display_link_hdcp_get_sink_status_info - Displaylink API, get sink HDCP status register
 *      @link: Link handle
 *      @status: Pointer to store the result
 *      @timeout: Timeout (in ms) to perform the reading operation
 *
 *      RETURNS:
 *              0:         No error
 *              -EINVAL    Invalid link handle
 *              -EFAULT    Invalid pointer address
 *              -ETIMEDOUT Timeout of the operation
 *              -EPERM     Operation not permitted
 */
int stm_display_link_hdcp_get_sink_status_info (stm_display_link_h link,
    uint16_t *status,
    uint32_t timeout) {
  struct stmlink* linkp;
  int save_timeout, ret = 0 ;

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if (status == NULL)
    return -EFAULT;

  mutex_lock(&(linkp->lock));
  if (linkp->hpd_state != STM_DISPLAY_LINK_HPD_STATE_HIGH){
      mutex_unlock(&(linkp->lock));
      LINKDBG(1,"Link is not connected");
      return -EPERM;
  }
  mutex_unlock(&(linkp->lock));

  if (timeout != 0) {
      mutex_lock(&(linkp->lock));
      save_timeout = linkp->i2c_adapter->timeout;
      linkp->i2c_adapter->timeout = timeout;
      mutex_unlock(&(linkp->lock));
  }

  if ((ret= stm_display_link_hdcp_proxy_client_read_sink_bstatus(linkp))==0) {
    *status = SINK_STATUS(linkp->bstatus);
  }

  if (timeout != 0) {
      mutex_lock(&(linkp->lock));
      linkp->i2c_adapter->timeout = save_timeout;
      mutex_unlock(&(linkp->lock));
  }

  return ret;
}

/**
 *      stm_display_link_hdcp_get_downstream_ksv_list - Displaylink API, reads the KSV FIFO list
 *      @link: Link handle
 *      @ksv_list: Pointer to the memory to store the reading data
 *      @size: Number of tKSV to read
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 *              -EFAULT  Invalid pointer address
 *              -EPERM   Operation not permitted
 */
int stm_display_link_hdcp_get_downstream_ksv_list ( stm_display_link_h link,
    uint8_t *ksv_list,
    uint32_t size) {

  struct stmlink* linkp;

  LINKDBG(3,"");

  if (link == NULL)
    return -EINVAL;

  linkp = (struct stmlink *)link ;

  if (ksv_list == NULL)
    return -EFAULT;

  if (size < 1)
    return -EFAULT;

  if ((linkp->status != STM_DISPLAY_LINK_HDCP_STATUS_SUCCESS_ENCRYPTING) &&
      (linkp->status != STM_DISPLAY_LINK_HDCP_STATUS_SUCCESS_NOT_ENCRYPTING)){
      LINKDBG(1,"Unable to get Downstream KSV list till the link is not authenticated");
      return -EPERM;
  }

  if (stm_display_link_hdcp_proxy_client_get_downstream_ksv_list(linkp, ksv_list, size)<0)
    return -EPERM;

  return 0;
}

EXPORT_SYMBOL(stm_display_link_edid_read);
EXPORT_SYMBOL(stm_display_link_get_capability);
EXPORT_SYMBOL(stm_display_link_get_type);
EXPORT_SYMBOL(stm_display_link_set_ctrl);
EXPORT_SYMBOL(stm_display_link_get_ctrl);
EXPORT_SYMBOL(stm_display_link_hpd_get_state);
EXPORT_SYMBOL(stm_display_link_rxsense_get_state);
EXPORT_SYMBOL(stm_display_link_hdcp_set_srm);
EXPORT_SYMBOL(stm_display_link_hdcp_get_status);
EXPORT_SYMBOL(stm_display_link_hdcp_get_last_transaction);
EXPORT_SYMBOL(stm_display_link_hdcp_get_sink_caps_info);
EXPORT_SYMBOL(stm_display_link_hdcp_get_sink_status_info);
EXPORT_SYMBOL(stm_display_link_hdcp_get_downstream_ksv_list);
