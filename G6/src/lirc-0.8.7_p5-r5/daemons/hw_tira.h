/*   $Id: hw_tira.h,v 5.3 2010/03/20 16:18:30 lirc Exp $  */
/*****************************************************************************
 ** hw_tira.h ****************************************************************
 *****************************************************************************
 * Routines for the HomeElectronics TIRA-2 USB dongle.
 *
 * Serial protocol described at: 
 *    http://www.home-electro.com/Download/Protocol2.pdf
 *
 * Copyright (C) 2003 Gregory McLean <gregm@gxsnmp.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef HW_TIRA_H
#define HW_TIRA_H

#include "drivers/lirc.h"

int     tira_decode                  (struct ir_remote   *remote,
				      ir_code            *prep,
				      ir_code            *codep,
				      ir_code            *postp,
				      int                *repeat_flagp,
				      lirc_t             *min_remaining_gapp,
				      lirc_t             *max_remaining_gapp);
int     tira_init                    (void);
int     tira_deinit                  (void);
char    *tira_rec                    (struct ir_remote   *remotes);
char    *tira_rec_mode2              (struct ir_remote   *remotes);
static int tira_send(struct ir_remote *remote, struct ir_ncode *code);
lirc_t	tira_readdata(lirc_t timeout);

#endif


