/**
 * \brief Peripheral Mailbox register map
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

#ifndef _P2012_PERIPH_MAILBOX_REGISTERMAP_H_
#define _P2012_PERIPH_MAILBOX_REGISTERMAP_H_


/*----------------------------------------------------------------------------
 * Registers Mapping
 *----------------------------------------------------------------------------*/

#ifndef ADDR_DOUBLE_WORD_ALIGNED
#define ADDR_DOUBLE_WORD_ALIGNED         0x003
#endif

//------------------------------------------------------------
//              Generic Peripheral Mailbox
//------------------------------------------------------------

#define MAILBOX_SEM_BASE_ADDRESS         0x000

#define MAILBOX_QUEUE_BASE_ADDRESS       0x004


#ifdef __cplusplus
struct periph_mailbox{
    unsigned int semaphore;
    std::list<int> queue;
};
#endif



#endif /* _P2012_PERIPH_MAILBOX_REGISTERMAP_H_ */
