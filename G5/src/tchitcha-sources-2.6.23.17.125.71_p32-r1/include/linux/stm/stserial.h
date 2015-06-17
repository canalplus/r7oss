/*
 *  --------------------------------------------------------------------------
 *
 *  stserial.h
 *  define and struct for STMicroelectronics ASC device
 *
 *  --------------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *  --------------------------------------------------------------------------
 */

#ifndef STM_SERIAL_H
#define STM_SERIAL_H 1

#include <linux/ioctl.h>


/* Using the same magic 'T' but take the number from 200 to 225 */
#define TIOSETGUARD	_IOW('T', 200, unsigned int)
#define TIOSETRETRY	_IOW('T', 201, unsigned int)
#define TIOSETNACK	_IOW('T', 202, unsigned int)
#define TIOSETSCEN	_IOW('T', 203, unsigned int)
#define TIOSETCLKSRC	_IOW('T', 204, unsigned int)
#define TIOSETCLK		_IOW('T', 205, unsigned int)
#define TIOCLKEN		_IOW('T', 206, unsigned int)
#define TIORST			_IOW('T', 207, unsigned int)
#define TIOVCC			_IOW('T', 208, unsigned int)
#define TIOWAITDATA		_IOW('T', 209, unsigned int)
#endif				/* STM_SERIAL_H */
