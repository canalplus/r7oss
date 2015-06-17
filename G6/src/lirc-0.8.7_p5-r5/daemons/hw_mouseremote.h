/*      $Id: hw_mouseremote.h,v 5.3 2007/07/29 18:20:08 lirc Exp $      */

/****************************************************************************
 ** hw_mouseremote.h ********************************************************
 ****************************************************************************
 *
 * routines for X10 MP3 Mouse Remote
 * 
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *	modified for logitech receiver by Isaac Lauer <inl101@alumni.psu.edu>
 *      modified for X10 receiver by Shawn Nycz <dscordia@eden.rutgers.edu>
 *      modified for X10 MouseRemote by Brian Craft <bcboy@thecraftstudio.com>
 *
 */

#ifndef HW_MOUSEREMOTE_H
#define HW_MOUSEREMOTE_H

#include "drivers/lirc.h"

int mouseremote_decode(struct ir_remote *remote,
		       ir_code *prep,ir_code *codep,ir_code *postp,
		       int *repeat_flagp,
		       lirc_t *min_remaining_gapp,
		       lirc_t *max_remaining_gapp);
int mouseremote_init(void);
int mouseremote_ps2_init(void);
int mouseremote_deinit(void);
char *mouseremote_rec(struct ir_remote *remotes);

#endif
