/****************************************************************************
 ** hw_mplay.h **************************************************************
 ****************************************************************************
 *
 * LIRC driver for Vlsys mplay usb ftdi serial port remote control.
 * 
 * Author:	Benoit Laurent <ben905@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef HW_MPLAY_H
#define HW_MPLAY_H

#include "drivers/lirc.h"

extern int mplay_decode(struct ir_remote *remote,
                        ir_code *prep,
                        ir_code *codep,
                        ir_code *postp,
                        int *repeat_flagp,
                        lirc_t *min_remaining_gapp,
                        lirc_t *max_remaining_gapp);

extern int mplay_init(void);
extern int mplay_deinit(void);
extern char *mplay_rec(struct ir_remote *remotes);

#endif
