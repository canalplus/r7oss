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

#include "message.h"

//{{{  ilog2
//{{{  doxynote
/// \brief Integer binary logarithm function.
///
/// This is a simple implementation based on the number of leading zeros.
/// If x is 0 then it will return -1.
///
/// \return Floor of the binary logarithm of @x
//}}}
static inline int ilog2(unsigned int x)
{
    return 31 - __lzcntw(x);
}

//}}}
//{{{  Init
//{{{  doxynote
/// \brief Initialize the subscription mask and allocate the message queue (circular buffer)
/// \brief Note that current management of circular buffer require one extra entry. This is
/// \brief why queueDepth is incremented by one
/// \param messageMask          bitmask of message type to subscribe
/// \param queueDepth           Max. entries in the queue
/// \return                     status code, MessageNoError indicates success.
MessageStatus_t MessageSubscription_c::Init(uint32_t     messageMask,
                                            uint32_t     queueDepth)
{
    if (queueDepth < 1)
    {
        // invalid queue size
        return MessageError;
    }

    // WritePos = Next message position to write
    // ReadPos  = Next message position to read
    WritePos          = 0;
    ReadPos           = 0;
    Message_Mask      = messageMask;
    QueueDepth        = queueDepth + 1;
    MessageQueue      = (stm_se_play_stream_msg_t (*)[])OS_Malloc(sizeof(stm_se_play_stream_msg_t) * QueueDepth);

    if (MessageQueue == NULL)
    {
        return MessageNoMemory;
    }

    return MessageNoError;
}
//}}}
//{{{  WriteMessage
//{{{  doxynote
/// \brief Write a message in the queue (TAIL) if its message type matches queue bitMask.
/// \brief If there is no more entry, the last written message is changed to "LOST MESSAGE"
/// \brief or its lost message counter is incremented if it has already "LOST MESSAGE" type.
/// \param msgToWrite           Message to copy and queue
/// \return                     status code, MessageNoError indicates success.
//}}}
MessageStatus_t MessageSubscription_c::WriteMessage(stm_se_play_stream_msg_t *msgToWrite)
{
    uint32_t                 nextWritePos;
    stm_se_play_stream_msg_t *TailMsg = NULL;

    if (Message_Mask & msgToWrite->msg_id)
    {
        // go next write position
        nextWritePos = ((WritePos + 1) >= QueueDepth ? 0 : WritePos + 1);

        // check if overflow
        if (nextWritePos == ReadPos)
        {
            uint32_t  lastWritePos;
            // Cannot write next message,
            // If last written message is already MESSAGE LOST
            // increase its lost counter
            lastWritePos = (WritePos == 0 ? QueueDepth - 1 : WritePos - 1);
            TailMsg = &(*MessageQueue)[lastWritePos];

            if (TailMsg->msg_id == STM_SE_PLAY_STREAM_MSG_MESSAGE_LOST)
            {
                TailMsg->u.lost_count += 1;
            }
            else
            {
                // Force message tyoe to STM_SE_PLAY_STREAM_MSG_MESSAGE_LOST
                TailMsg->msg_id = STM_SE_PLAY_STREAM_MSG_MESSAGE_LOST;
                TailMsg->u.lost_count = 0;
            }

            return MessageError;
        }

        // copy message into circular buffer (Tail)
        TailMsg = &(*MessageQueue)[WritePos];
        memcpy(TailMsg,  msgToWrite, sizeof(stm_se_play_stream_msg_t));
        // Set the next entry to write
        WritePos = nextWritePos;
        //increase message count in the circular buffer
        MessageCount++;
    }

    return MessageNoError;
}
//}}}
//{{{  ReadMessage
//{{{  doxynote
/// \brief Read if exist, the older message from the subscription queue (HEAD).
/// \brief The read message is removed from the queue.
/// \param readMessage          Message to copy from the queue
/// \return                     status code, MessageNoError indicates success.
//}}}
MessageStatus_t MessageSubscription_c::ReadMessage(stm_se_play_stream_msg_t *readMessage)
{
    uint32_t                 nextReadPos;
    stm_se_play_stream_msg_t *readMsg = NULL;

    // something to read ?
    if (ReadPos == WritePos)
    {
        readMessage->msg_id = STM_SE_PLAY_STREAM_MSG_INVALID;
        return MessageNoError;
    }

    // copy message from circular buffer (head)
    readMsg = &(*MessageQueue)[ReadPos];
    memcpy(readMessage,  readMsg, sizeof(stm_se_play_stream_msg_t));
    // go next read position
    nextReadPos = ((ReadPos + 1) == QueueDepth ? 0 : ReadPos + 1);
    // Set the next entry to read
    ReadPos = nextReadPos;
    //decrease message count in the circular buffer
    MessageCount--;
    return MessageNoError;
}
//}}}
//{{{  Message_c
Message_c::Message_c(void)
    : MessageMutex()
    , isIntialized(false)
    , PollMessage()
    , SubscriptionList(NULL)
{
    OS_InitializeMutex(&MessageMutex);

    for (int pollMsgIdx = 0;
         pollMsgIdx < POLL_MESSAGE_QUEUE_SIZE;
         pollMsgIdx++)
    {
        PollMessage[pollMsgIdx].msg_id  = STM_SE_PLAY_STREAM_MSG_INVALID;
    }
}
//}}}
//{{{  ~Message_c
Message_c::~Message_c(void)
{
    MessageSubscription_c *subscriptionElt;
    // release subscription list
    subscriptionElt = SubscriptionList;

    while (subscriptionElt)
    {
        MessageSubscription_c *next;
        next = subscriptionElt->Next;
        delete(subscriptionElt);
        subscriptionElt = next;
    }

    OS_TerminateMutex(&MessageMutex);
}
//}}}
//{{{  Init
//{{{  doxynote
/// \brief Initialize message subscription
/// \return                     status code, MessageNoError indicates success.
//}}}
MessageStatus_t Message_c::Init(void)
{
    // Initialization successful
    isIntialized = true;
    return MessageNoError;
}
//}}}
//{{{  IsValidSubscription
//{{{  doxynote
/// \brief Check if subscription handle is valid, that is correspond to a
/// \brief MessageSubscription_c object in SubscriptionList
/// \param subscription         subscription handle to check
/// \return                     status code, MessageNoError indicates success.
//}}}
MessageStatus_t Message_c::IsValidSubscription(stm_se_play_stream_subscription_h subscription)
{
    MessageSubscription_c *subscriptionElt;
    subscriptionElt = SubscriptionList;

    while (subscriptionElt)
    {
        if ((MessageSubscription_c *)subscription == subscriptionElt)
        {
            return MessageNoError;
        }

        subscriptionElt = subscriptionElt->Next;
    }

    return MessageError;
}
//}}}
//{{{  CreateSubscription
//{{{  doxynote
/// \brief Create a new subscription object and insert it in subscription list
/// \brief MessageSubscription_c object of SubscriptionList
/// \param messageMask          Bitmask of message type wanted for this subscription
/// \param queueDepth           Maxinum number of message in associated message queue
/// \param subscription         returned created subscription handle
/// \return                     status code, MessageNoError indicates success.
//}}}
MessageStatus_t Message_c::CreateSubscription(uint32_t                            messageMask,
                                              uint32_t                            queueDepth,
                                              stm_se_play_stream_subscription_h  *subscription)
{
    MessageSubscription_c   *newSubscription;

    // access precondition
    if (isIntialized == false)
    {
        return MessageError;
    }

    newSubscription = new MessageSubscription_c();

    if (newSubscription == NULL)
    {
        return MessageError;
    }

    if (newSubscription->Init(messageMask, queueDepth) != MessageNoError)
    {
        // release allocated subscription
        delete newSubscription;
        newSubscription = NULL;
        return MessageError;
    }

    // Insert this new subscription in SubscriptionList
    OS_LockMutex(&MessageMutex);

    // add subscription in head position
    if (SubscriptionList)
    {
        SubscriptionList->Prev = newSubscription;
    }

    newSubscription->Next  = SubscriptionList;
    newSubscription->Prev  = NULL;
    SubscriptionList       = newSubscription;
    // return subscription
    *subscription = (stm_se_play_stream_subscription_h)newSubscription;
    OS_UnLockMutex(&MessageMutex);
    return MessageNoError;
}
//}}}
//{{{  DeleteSubscription
//{{{  doxynote
/// \brief Try to retrieve and release a subscription object thanks to its handle
/// \brief MessageSubscription_c object of SubscriptionList
/// \param subscription         Handle of the subscription to delete
/// \return                     status code, MessageNoError indicates success.
//}}}
MessageStatus_t Message_c::DeleteSubscription(stm_se_play_stream_subscription_h subscription)
{
    MessageStatus_t           result = MessageNoError;
    MessageSubscription_c   *removedSubscription;

    // access precondition
    if (isIntialized == false)
    {
        return MessageError;
    }

    // protect message queue access
    OS_LockMutex(&MessageMutex);
    // Subscription must exist in the list
    result = IsValidSubscription(subscription);

    if (result == MessageNoError)
    {
        // Remove  this subscription from SubscriptionList
        removedSubscription = (MessageSubscription_c *) subscription;

        if (removedSubscription->Prev)
        {
            removedSubscription->Prev->Next = removedSubscription->Next;
        }
        else
        {
            SubscriptionList = removedSubscription->Next;
        }

        if (removedSubscription->Next)
        {
            removedSubscription->Next->Prev = removedSubscription->Prev;
        }

        // release the subscription object
        delete removedSubscription;
    }

    // release protection
    OS_UnLockMutex(&MessageMutex);
    return result;
}
//}}}
//{{{  NewSignaledMessage
//{{{  doxynote
/// \brief A new message is generated. Write a copy into each matching subscription queue
/// \brief and in the corresponding _MESSAGE_POLL copy
/// \param message              New generated message
/// \return                     status code, MessageNoError indicates success.
//}}}
MessageStatus_t Message_c::NewSignaledMessage(stm_se_play_stream_msg_t *message)
{
    MessageSubscription_c *subscriptionElt;
    MessageStatus_t         result = MessageNoError;
    uint32_t               pollMsg_idx;

    // access precondition
    if (isIntialized == false)
    {
        return MessageError;
    }

    // lookup the index to the PollMessage array
    pollMsg_idx = ilog2((unsigned int) message->msg_id);
    // protect message queue access
    OS_LockMutex(&MessageMutex);
    // save message for _POLL_MESSAGE access
    memcpy(&PollMessage[pollMsg_idx], message, sizeof(stm_se_play_stream_msg_t));
    // Try to push new message in queue for each subscription
    subscriptionElt = SubscriptionList;

    while (subscriptionElt)
    {
        subscriptionElt->WriteMessage(message);
        subscriptionElt = subscriptionElt->Next;
    }

    // release protection
    OS_UnLockMutex(&MessageMutex);
    return result;
}
//}}}
//{{{  GetPollMessage
//{{{  doxynote
/// \brief Try to get last generated message with wanted message type.
/// \param message              Message container
/// \return                     status code, MessageNoError indicates success.
//}}}
MessageStatus_t Message_c::GetPollMessage(stm_se_play_stream_msg_id_t    msg_id,
                                          stm_se_play_stream_msg_t      *message)
{
    MessageStatus_t     result = MessageNoError;
    uint32_t           pollMsg_idx;

    // access precondition
    if (isIntialized == false)
    {
        return MessageError;
    }

    // message container must be provided
    if (message == NULL)
    {
        return MessageError;
    }

    // lookup the index to the PollMessage array
    pollMsg_idx = ilog2((unsigned int) msg_id);
    // protect message queue access
    OS_LockMutex(&MessageMutex);

    // By testing equality to msg_id (rather then inequality to
    // STM_SE_PLAY_STREAM_MSG_INVALID) we correctly handle the case where
    // msg_id has more than one bit set by failing to match (and reporting
    // an error)
    if (PollMessage[pollMsg_idx].msg_id == msg_id)
    {
        memcpy(message, &PollMessage[pollMsg_idx], sizeof(stm_se_play_stream_msg_t));
    }
    else
    {
        result = MessageError;
    }

    // release protection
    OS_UnLockMutex(&MessageMutex);
    return result;
}
//}}}
//{{{  GetMessage
//{{{  doxynote
/// \brief Read the older message from queue of wanted subscription
/// \param subscription         Handle of the subscription
/// \param message              Message container
/// \return                     status code, MessageNoError indicates success.
//}}}
MessageStatus_t Message_c::GetMessage(stm_se_play_stream_subscription_h       subscription,
                                      stm_se_play_stream_msg_t                *message)
{
    MessageStatus_t           result = MessageNoError;
    MessageSubscription_c   *targetedSubscription;

    // access precondition
    if (isIntialized == false)
    {
        return MessageError;
    }

    // message container must be provided
    if (message == NULL)
    {
        return MessageError;
    }

    // protect message queue access
    OS_LockMutex(&MessageMutex);
    // Subscription must exist in the list
    result = IsValidSubscription(subscription);

    if (result == MessageNoError)
    {
        // Get subscription object
        targetedSubscription = (MessageSubscription_c *) subscription;
        // Get message if any
        result = targetedSubscription->ReadMessage(message);
    }

    // release protection
    OS_UnLockMutex(&MessageMutex);
    return result;
}
//}}}
