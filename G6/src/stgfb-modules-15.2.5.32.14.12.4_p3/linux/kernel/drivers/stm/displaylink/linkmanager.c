/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/displaylink/linkmanager.c
 * Copyright (c) 2012 STMicroelectronics Limited.
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
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include <linux/ktime.h>

#include <stm_common.h>
#include <stm_event.h>
#include <event_subscriber.h>

#include <asm/uaccess.h>
#include <linux/semaphore.h>

#include <stm_display.h>
#include <stm_display_link.h>

#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorelink.h>
#include <linkproxy_client.h>
/**
 *      GetSystemTime - Helper to get the system time
 *
 *      RETURNS:
 *      The system time in us
 */
stm_time64_t GetSystemTime(void)
{
  struct timespec ts;

  getrawmonotonic(&ts);

  return ktime_to_us(timespec_to_ktime(ts));
}

/**
 *      usleep - Helper to sleep for a while
 *      @us: time to sleep in us
 *
 */
void usleep(unsigned long us) {
  set_current_state(TASK_UNINTERRUPTIBLE);
  schedule_timeout((us * HZ) / 1000000);
}

/**
 *      send_event - Helper to send event notification
 *      @link: Pointer to the link struct
 *      @type: type of event to send
 *
 *      RETURNS:
 *      0 if no error
 */
int send_event (struct stmlink* link, unsigned int type) {
  stm_event_t event;
  int ret = 0;
  LINKDBG(3,"Event Sent %d",type);
  event.object = (stm_object_h)link;
  event.event_id = type;
  ret = stm_event_signal(&event);
  return ret;
}

/**
 *      stmlink_interrupt - HDMI Interrupt handler
 *      @irq: IRQ trigged
 *      @data: Pointer to the link structure used by the HDMI device
 *
 *      RETURNS:
 *      status
 */
irqreturn_t stmlink_interrupt(int irq, void* data)
{
  stm_display_output_connection_status_t hdmi_status;
  stm_display_link_hpd_state_t hpd_current;
  stm_display_link_rxsense_state_t rxsense_current;
  struct stmlink *link = (struct stmlink *)data;


  hpd_current = link->hpd_new = link->hpd_state ;
  rxsense_current = link->rxsense_new = link->rxsense_state;

  stm_display_output_handle_interrupts(link->hdmi_output);
  stm_display_output_get_connection_status(link->hdmi_output, &hdmi_status);

  LINKDBG(8,"HDMI interrupt event intercepted");
  switch(hdmi_status) {
  case STM_DISPLAY_DISCONNECTED:
    link->hpd_new = STM_DISPLAY_LINK_HPD_STATE_LOW;
    link->rxsense_new = STM_DISPLAY_LINK_RXSENSE_STATE_INACTIVE;
    break;
  case STM_DISPLAY_INACTIVE:
    link->hpd_new = STM_DISPLAY_LINK_HPD_STATE_HIGH;
    link->rxsense_new = STM_DISPLAY_LINK_RXSENSE_STATE_INACTIVE;
    break;
  case STM_DISPLAY_CONNECTED:
    // Rx sense state remains unchanged keeping the last state reached
    link->hpd_new = STM_DISPLAY_LINK_HPD_STATE_HIGH;
    break;
  case STM_DISPLAY_NEEDS_RESTART:
    link->hpd_new = STM_DISPLAY_LINK_HPD_STATE_HIGH;
    link->rxsense_new = STM_DISPLAY_LINK_RXSENSE_STATE_ACTIVE;
    break;
  default :
    break;
  }
  if ((link->hdcp_start_needed)&& (link->display_mode!=0))
  {
     link->hdcp_start_needed=false;
     link->hdcp_start = true;
     wake_up(&link->wait_queue);
   }
  if ((hpd_current != link->hpd_new) ||
     ((link->rxsense) && (rxsense_current != link->rxsense_new))||
     (link->hpd_missed))
  {
     link->hpd_check_needed =true;
     wake_up(&link->wait_queue);
  }

  return IRQ_HANDLED;
}

/**
 *      hpd_manager - HPD filter to avoid glitchs
 *      @data: Pointer to the link structure
 *
 *      RETURNS:
 *      0 if no error
 */
int hpd_manager(void *data) {
  struct stmlink *link = (struct stmlink *)data;
  int retVal = 0;
  int timeout_ms=20;

  LINKDBG(1,"Starting HPD manager");
  set_freezable();

  while(1)
    {
      wait_event_freezable_timeout(link->wait_queue, (link->hpd_check_needed || link->hdcp_start|| kthread_should_stop()), msecs_to_jiffies(timeout_ms));

      LINKDBG(3,"%d : HPD thread %d wakes up", (unsigned int)GetSystemTime(), link->device_id);

      mutex_lock(&(link->lock));
      if(kthread_should_stop())
      {
        mutex_unlock(&(link->lock));
        break;
      }
      if ((link->hpd_state != link->hpd_new) || (link->hpd_missed)){
         if (!link->hpd_missed){
            link->hpd_state = link->hpd_new;
         } else {
            link->hpd_missed = false;
         }
         /* If HDCP daemon is here, send the updated HPD state.
          * This will allow the daemon to take care of this new state.
          */
         if (stm_display_link_hdcp_proxy_client_send_hpd_state(link,link->hpd_new) < 0){
            LINKDBG(8,"Problem while sending new HPD state to the daemon");
         }

         if ((retVal = send_event(link, STM_DISPLAY_LINK_HPD_STATE_CHANGE_EVT))!= 0) {
            LINKDBG(8,"An error occured in HPD state change event");
         }
      }
      if (link->rxsense_state != link->rxsense_new){
         link->rxsense_state = link->rxsense_new;
          link->rxsense_missed=true;
      } else {
        if (!link->hpd_check_needed && link->rxsense_missed){
          link->rxsense_missed=false;
          /* If HDCP daemon is here, send the updated Rx sense state.
          * This will allow the daemon to take care of this new state.
          */

          if (stm_display_link_hdcp_proxy_client_send_rxsense_state(link,link->rxsense_new) < 0){
            LINKDBG(8,"Problem while sending new HPD state to the daemon");
          }
          if ((retVal = send_event(link, STM_DISPLAY_LINK_RXSENSE_STATE_CHANGE_EVT))!= 0) {
            LINKDBG(8,"An error occured in Rx sense state change event");
          }
        }
      }

      link->hpd_check_needed=false;

      if (link->hdcp_start)
      {
        mutex_unlock(&(link->lock));
        if (stm_display_link_hdcp_proxy_client_set_ctrl(link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE, link->display_mode)<0)
          LINKDBG(8,"An error occured in HDCP enabling");
        mutex_lock (&(link->lock));

        link->hdcp_start=false;
      }
      mutex_unlock(&(link->lock));

   }

    return 0;
}
