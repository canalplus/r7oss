/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

/************************************************************************
 * Description:
 * 1. Subscribe to STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY
 * 2. Set a callback on STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY
 *    and output values transmitted on STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS to
 *    ControlDataTest to be read from strelaytool
 ************************************************************************/
#include <linux/version.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/string.h>

#include <asm/uaccess.h>
#include <asm/irq.h>

#include "KTM_ControlData.h"
#include "KTM_ControlData_event.h"

#include "st_relayfs_se.h"

extern struct ControlDataTestCtx Context;
char bufferRelay[MAX_RELAYFS_STRING_LENGTH] = {0, };

uint8_t SubscribeEvent(void)
{
    uint8_t res = 0;
    uint32_t EventMask =
        STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY;
    uint32_t MessageMask =
        STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS;
    stm_event_subscription_entry_t EventEntryH264 =
    {
        .object = (stm_object_h) Context.StreamH264Handle,
        .event_mask = EventMask,
    };
    stm_event_subscription_entry_t EventEntryMpeg2 =
    {
        .object = (stm_object_h) Context.StreamMpeg2Handle,
        .event_mask = EventMask,
    };
    /* Subscribe to events (for notification) */
    res = stm_event_subscription_create(&EventEntryH264, 1,
                                        &Context.EventSubscriptionH264);

    if (0 != res)
    {
        pr_err("Unable to subscribe event for H264\n");
        return res;
    }

    res = stm_event_subscription_create(&EventEntryMpeg2, 1,
                                        &Context.EventSubscriptionMpeg2);

    if (0 != res)
    {
        pr_err("Unable to subscribe event for Mpeg2\n");
        return res;
    }

    /* subscribe to messages from the play stream */
    res = stm_se_play_stream_subscribe(Context.StreamH264Handle,
                                       MessageMask,
                                       MESSAGE_DEPTH,
                                       &Context.MessageSubscriptionH264);

    if (0 != res)
    {
        pr_err("Unable to subscribe to streaming engine message for H264 stream\n");
        //(void) stm_event_subscription_delete(Context.EventSubscription);
        return res;
    }

    res = stm_se_play_stream_subscribe(Context.StreamMpeg2Handle,
                                       MessageMask,
                                       MESSAGE_DEPTH,
                                       &Context.MessageSubscriptionMpeg2);

    if (0 != res)
    {
        pr_err("Unable to subscribe to streaming engine message for mpeg2 stream\n");
        //(void) stm_event_subscription_delete(Context.EventSubscription);
        return res;
    }

    res = stm_event_set_handler(Context.EventSubscriptionH264, HandleEventH264);

    if (0 != res)
    {
        pr_err("Unable to setup callback on event\n");
        return res;
    }

    res = stm_event_set_handler(Context.EventSubscriptionMpeg2, HandleEventMpeg2);

    if (0 != res)
    {
        pr_err("Unable to setup callback on event\n");
        return res;
    }

    return res;
}

void UnsubscribeEvent(void)
{
    uint8_t res1, res2;
    stm_se_play_stream_unsubscribe(Context.StreamH264Handle,
                                   Context.MessageSubscriptionH264);
    stm_se_play_stream_unsubscribe(Context.StreamMpeg2Handle,
                                   Context.MessageSubscriptionMpeg2);
    res1 = stm_event_subscription_delete(Context.EventSubscriptionH264);
    res2 = stm_event_subscription_delete(Context.EventSubscriptionMpeg2);
    if (res1 || res2)
    {
        pr_err("stm_event_subscription_delete failed\n");
    }
}

/* This is the callback on STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY */
void HandleEventH264(unsigned int number_of_events, stm_event_info_t *eventsInfo)
{
    uint8_t res = 0;
    uint32_t BufferRelayLength = 0;
    stm_se_play_stream_msg_t message;

    if (eventsInfo->event.event_id == STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY)
    {
        /* The loop is required to handle cases where several messages
         * are sent on the same event. */
        while (1)
        {
            res = stm_se_play_stream_get_message(Context.StreamH264Handle,
                                                 Context.MessageSubscriptionH264,
                                                 &message);

            if (0 != res)
            {
                break;
            }

            if (message.msg_id == STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS)
            {
                pr_info("PTS is %d, size is %d \n ", (int)message.u.alarm_parsed_pts.pts,
                        (int)message.u.alarm_parsed_pts.size);
                pr_info("ControlUserData[1]= %02x%02x%02x%02x , ControlUserData[2]=%02x%02x%02x%02x\n",
                        message.u.alarm_parsed_pts.data[0],
                        (message.u.alarm_parsed_pts.data[1]),
                        (message.u.alarm_parsed_pts.data[2]),
                        (message.u.alarm_parsed_pts.data[3]),
                        (message.u.alarm_parsed_pts.data[4]),
                        (message.u.alarm_parsed_pts.data[5]),
                        (message.u.alarm_parsed_pts.data[6]),
                        (message.u.alarm_parsed_pts.data[7]));
                /* Write values associated to STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS
                 * in bufferRelay to be retrieved in user space through strelaytool. */
                BufferRelayLength = snprintf(bufferRelay, MAX_RELAYFS_STRING_LENGTH, "%d:", (int)message.u.alarm_parsed_pts.pts);
                bufferRelay[MAX_RELAYFS_STRING_LENGTH - 1] = '\0';
                st_relayfs_write_se(ST_RELAY_TYPE_CONTROL_REQ_TIME, ST_RELAY_SOURCE_CONTROL_TEST + Context.FSIndex,
                                    bufferRelay, BufferRelayLength, 0);

                BufferRelayLength = snprintf(bufferRelay, MAX_RELAYFS_STRING_LENGTH, "%d:", (int)message.u.alarm_parsed_pts.size);
                bufferRelay[MAX_RELAYFS_STRING_LENGTH - 1] = '\0';
                st_relayfs_write_se(ST_RELAY_TYPE_CONTROL_REQ_TIME, ST_RELAY_SOURCE_CONTROL_TEST + Context.FSIndex,
                                    bufferRelay, BufferRelayLength, 0);

                BufferRelayLength = snprintf(bufferRelay, MAX_RELAYFS_STRING_LENGTH, "%02x%02x%02x%02x:%02x%02x%02x%02x\n",
                                             message.u.alarm_parsed_pts.data[0],
                                             (message.u.alarm_parsed_pts.data[1]),
                                             (message.u.alarm_parsed_pts.data[2]),
                                             (message.u.alarm_parsed_pts.data[3]),
                                             (message.u.alarm_parsed_pts.data[4]),
                                             (message.u.alarm_parsed_pts.data[5]),
                                             (message.u.alarm_parsed_pts.data[6]),
                                             (message.u.alarm_parsed_pts.data[7]));
                bufferRelay[MAX_RELAYFS_STRING_LENGTH - 1] = '\0';
                st_relayfs_write_se(ST_RELAY_TYPE_CONTROL_REQ_TIME, ST_RELAY_SOURCE_CONTROL_TEST + Context.FSIndex, bufferRelay, BufferRelayLength, 0);
            }
        }
    }
}

/* This is the callback on STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY */
void HandleEventMpeg2(unsigned int number_of_events, stm_event_info_t *eventsInfo)
{
    uint8_t res = 0;
    uint32_t BufferRelayLength = 0;
    stm_se_play_stream_msg_t message;

    if (eventsInfo->event.event_id == STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY)
    {
        /* The loop is required to handle cases where several messages
         * are sent on the same event. */
        while (1)
        {
            res = stm_se_play_stream_get_message(Context.StreamMpeg2Handle,
                                                 Context.MessageSubscriptionMpeg2,
                                                 &message);

            if (0 != res)
            {
                break;
            }

            if (message.msg_id == STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS)
            {
                pr_info("PTS is %d, size is %d \n ", (int)message.u.alarm_parsed_pts.pts,
                        (int)message.u.alarm_parsed_pts.size);
                pr_info("ControlUserData[1]= %02x%02x%02x%02x , ControlUserData[2]=%02x%02x%02x%02x\n",
                        message.u.alarm_parsed_pts.data[0],
                        (message.u.alarm_parsed_pts.data[1]),
                        (message.u.alarm_parsed_pts.data[2]),
                        (message.u.alarm_parsed_pts.data[3]),
                        (message.u.alarm_parsed_pts.data[4]),
                        (message.u.alarm_parsed_pts.data[5]),
                        (message.u.alarm_parsed_pts.data[6]),
                        (message.u.alarm_parsed_pts.data[7]));
                /* Write values associated to STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS
                 * in bufferRelay to be retrieved in user space through strelaytool. */
                BufferRelayLength = snprintf(bufferRelay, MAX_RELAYFS_STRING_LENGTH, "%d:", (int)message.u.alarm_parsed_pts.pts);
                bufferRelay[MAX_RELAYFS_STRING_LENGTH - 1] = '\0';
                st_relayfs_write_se(ST_RELAY_TYPE_CONTROL_REQ_TIME, ST_RELAY_SOURCE_CONTROL_TEST + Context.FSIndex, bufferRelay, BufferRelayLength, 0);

                BufferRelayLength = snprintf(bufferRelay, MAX_RELAYFS_STRING_LENGTH, "%d:", (int)message.u.alarm_parsed_pts.size);
                bufferRelay[MAX_RELAYFS_STRING_LENGTH - 1] = '\0';
                st_relayfs_write_se(ST_RELAY_TYPE_CONTROL_REQ_TIME, ST_RELAY_SOURCE_CONTROL_TEST + Context.FSIndex, bufferRelay, BufferRelayLength, 0);

                BufferRelayLength = snprintf(bufferRelay, MAX_RELAYFS_STRING_LENGTH, "%02x%02x%02x%02x:%02x%02x%02x%02x\n",
                                             message.u.alarm_parsed_pts.data[0],
                                             (message.u.alarm_parsed_pts.data[1]),
                                             (message.u.alarm_parsed_pts.data[2]),
                                             (message.u.alarm_parsed_pts.data[3]),
                                             (message.u.alarm_parsed_pts.data[4]),
                                             (message.u.alarm_parsed_pts.data[5]),
                                             (message.u.alarm_parsed_pts.data[6]),
                                             (message.u.alarm_parsed_pts.data[7]));
                bufferRelay[MAX_RELAYFS_STRING_LENGTH - 1] = '\0';
                st_relayfs_write_se(ST_RELAY_TYPE_CONTROL_REQ_TIME, ST_RELAY_SOURCE_CONTROL_TEST + Context.FSIndex,
                                    bufferRelay, BufferRelayLength, 0);
            }
        }
    }
}
