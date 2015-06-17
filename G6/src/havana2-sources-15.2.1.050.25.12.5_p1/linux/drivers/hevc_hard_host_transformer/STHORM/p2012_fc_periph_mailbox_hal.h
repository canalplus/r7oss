/**
 * \brief Basic API to access the Fabric Controller Peripheral Mailbox component
 *
 * Copyright (C) 2012 STMicroelectronics
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Iker DE POY, STMicroelectronics (iker.de-poy-alonso@st.com)
 *
 */

#ifndef _P2012_FC_PERIPH_MAILBOX_HAL_H_
#define _P2012_FC_PERIPH_MAILBOX_HAL_H_

#include <p2012_memory_map.h>
#include <p2012_periph_mailbox_hal.h>


/**
 * \brief used with doxygen for the documentation (doxygen can't support key words static inline)
 */
#ifndef STATIC_INLINE
#define STATIC_INLINE static inline
#endif


/*Mailbox Macros*/

/**
 * \brief read a message from the Fabric Controller Peripheral Mailbox
 */
#define FC_READ_MAILBOX_QUEUE(QUEUE_ID)              fc_mailbox_read_message(QUEUE_ID)


/**
 * \brief send a message to the Fabric Controller Peripheral Mailbox queue
 */
#define FC_SEND_MESSAGE_MAILBOX(QUEUE_ID,VAL)        fc_mailbox_send_message(QUEUE_ID,VAL)


/**
 * \brief Read the Fabric Controller Peripheral Mailbox semaphore
 */
#define FC_READ_MAILBOX_SEMAPHORE(QUEUE_ID)          fc_mailbox_read_semaphore(QUEUE_ID)



/*****************************************************************************/
/*                      FC_PERIPH_MAILBOX HAL                                */
/*****************************************************************************/


/** \brief Read the Fabric Controller Peripheral Mailbox semaphore
 *
 * \param[in] queue_id    the queue id. Available values: 0 or 1.
 *
 * \return returns 1 if there are free slots in the queue. If not returns 0.
 */
STATIC_INLINE
unsigned int
fc_mailbox_read_semaphore( unsigned int queue_id )
{
    return mailbox_read_semaphore(P2012_CONF_FC_MBX_BASE + (queue_id << ADDR_DOUBLE_WORD_ALIGNED));
}


/** \brief Send a message to the Fabric Controller Peripheral Mailbox queue without having previously granted the token (Semaphore).
 *
 * \param[in] queue_id    the queue id. Available values: 0 or 1.
 * \param[in] msg         the message to be sent
 *
 * \return returns void
 */
STATIC_INLINE
void
fc_mailbox_send_message_wo_sem( unsigned int queue_id, unsigned int msg )
{
    mailbox_send_message_wo_sem(P2012_CONF_FC_MBX_BASE + (queue_id << ADDR_DOUBLE_WORD_ALIGNED), msg);
}


/** \brief Send a message to the Fabric Controller Peripheral Mailbox queue.
 *
 * \param[in] queue_id    the queue id. Available values: 0 or 1.
 * \param[in] msg         the message to be sent
 *
 * \return returns void
 */
STATIC_INLINE
void
fc_mailbox_send_message( unsigned int queue_id, unsigned int msg )
{
    mailbox_send_message(P2012_CONF_FC_MBX_BASE + (queue_id << ADDR_DOUBLE_WORD_ALIGNED), msg);
}


/** \brief Read a message from the Fabric Controller Peripheral Mailbox queue
 *
 * \param[in] queue_id    the queue id. Available values: 0 or 1.
 *
 * \return returns the message of the queue.
 */
STATIC_INLINE
unsigned int
fc_mailbox_read_message( unsigned int queue_id )
{
    return mailbox_read_message(P2012_CONF_FC_MBX_BASE + (queue_id << ADDR_DOUBLE_WORD_ALIGNED));
}

#endif // _P2012_FC_PERIPH_MAILBOX_HAL_H_
