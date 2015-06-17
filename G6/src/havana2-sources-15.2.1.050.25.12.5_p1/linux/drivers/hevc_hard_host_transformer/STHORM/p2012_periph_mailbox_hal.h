/**
 * \brief Basic API to access the Peripheral Mailbox component
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
 *          Raphael David, CEA LIST (raphael.david@cea.fr)
 *          Maroun Ojail, CEA LIST (maroun.ojail@cea.fr)
 *          Yves Lhuillier, CEA LIST (yves.lhuillier@cea.fr)
 *
 */

#ifndef _P2012_PERIPH_MAILBOX_HAL_H_
#define _P2012_PERIPH_MAILBOX_HAL_H_

#include <p2012_periph_mailbox_regmap.h>


/**
 * \brief used with doxygen for the documentation (doxygen can't support key words static inline)
 */
#ifndef STATIC_INLINE
#define STATIC_INLINE static inline
#endif



/*****************************************************************************/
/*                        PERIPH_MAILBOX HAL                                 */
/*****************************************************************************/



/** \brief Read the Peripheral Mailbox semaphore
 *
 * \param[in] address     the memory address of the Peripheral Mailbox
 *
 * \return returns 1 if there are free slots in the queue. If not returns 0.
 */
STATIC_INLINE
unsigned int
mailbox_read_semaphore( unsigned int address )
{
    return p2012_read(address + MAILBOX_SEM_BASE_ADDRESS);
}


/** \brief Send a message to the Peripheral Mailbox queue without having previously granted the token (Semaphore).
 *
 * \param[in] address     the memory address of the Peripheral Mailbox
 * \param[in] msg         the message to be sent
 *
 * \return returns void
 */
STATIC_INLINE
void
mailbox_send_message_wo_sem (   unsigned int address, unsigned int msg  )
{
    p2012_write((address + MAILBOX_SEM_BASE_ADDRESS),msg);
}


/** \brief Send a message to the Peripheral Mailbox queue
 *
 * \param[in] address     the memory address of the Peripheral Mailbox
 * \param[in] msg         the message to be sent
 *
 * \return returns void
 */
STATIC_INLINE
void
mailbox_send_message( unsigned int address, unsigned int msg )
{
    p2012_write((address + MAILBOX_QUEUE_BASE_ADDRESS),msg);
}


/** \brief Read a message from the Peripheral Mailbox queue
 *
 * \param[in] address     the memory address of the Peripheral Mailbox
 *
 * \return returns the message of the queue.
 */
STATIC_INLINE
unsigned int
mailbox_read_message( unsigned int address )
{
    return p2012_read(address + MAILBOX_QUEUE_BASE_ADDRESS);
}

#endif // _P2012_PERIPH_MAILBOX_HAL_H_
