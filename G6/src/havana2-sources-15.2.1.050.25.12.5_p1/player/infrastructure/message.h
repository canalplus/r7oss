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

#ifndef H_MESSAGE
#define H_MESSAGE

#include "player.h"

typedef enum
{
    MessageNoError            = 0,
    MessageError,
    MessageNoMemory
} MessageStatus_t;

#define POLL_MESSAGE_QUEUE_SIZE 8

// Class for message subscription management
class MessageSubscription_c
{
public:
    MessageSubscription_c(void)
        : Next(NULL)
        , Prev(NULL)
        , Message_Mask(0)
        , MessageCount(0)
        , QueueDepth(0)
        , WritePos(0)
        , ReadPos(0)
        , MessageQueue(NULL)
    {}
    ~MessageSubscription_c(void)
    {
        OS_Free(MessageQueue);
    }

    MessageStatus_t Init(uint32_t     messageMask,
                         uint32_t     queueDepth);
    MessageStatus_t WriteMessage(stm_se_play_stream_msg_t *msg);
    MessageStatus_t ReadMessage(stm_se_play_stream_msg_t *msg);

    friend class Message_c;
private:
    class MessageSubscription_c    *Next;
    class MessageSubscription_c    *Prev;
    uint32_t                        Message_Mask;
    uint32_t                        MessageCount;
    uint32_t                        QueueDepth;
    uint32_t                        WritePos;
    uint32_t                        ReadPos;
    stm_se_play_stream_msg_t (*MessageQueue)[];

    DISALLOW_COPY_AND_ASSIGN(MessageSubscription_c);
};

/// Player wrapper component to manage play_stream message subscription.
class Message_c
{
public:
    Message_c(void);
    ~Message_c(void);

    MessageStatus_t              Init(void);
    MessageStatus_t              CreateSubscription(uint32_t                                messageMask,
                                                    uint32_t                                queueDepth,
                                                    stm_se_play_stream_subscription_h      *subscription);
    MessageStatus_t              DeleteSubscription(stm_se_play_stream_subscription_h       subscription);
    MessageStatus_t              NewSignaledMessage(stm_se_play_stream_msg_t                *message);
    MessageStatus_t              GetPollMessage(stm_se_play_stream_msg_id_t              msg_id,
                                                stm_se_play_stream_msg_t                *message);
    MessageStatus_t              GetMessage(stm_se_play_stream_subscription_h       subscription,
                                            stm_se_play_stream_msg_t                *message);

private:
    OS_Mutex_t                  MessageMutex;
    bool                        isIntialized;
    stm_se_play_stream_msg_t    PollMessage[POLL_MESSAGE_QUEUE_SIZE *sizeof(stm_se_play_stream_msg_id_t)];
    MessageSubscription_c       *SubscriptionList;

    MessageStatus_t              IsValidSubscription(stm_se_play_stream_subscription_h   subscription);

    DISALLOW_COPY_AND_ASSIGN(Message_c);
};

#endif
