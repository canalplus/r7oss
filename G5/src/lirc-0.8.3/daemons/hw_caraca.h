/*      $Id: hw_caraca.h,v 1.2 2007/07/29 18:20:06 lirc Exp $      */

/****************************************************************************
 ** hw_caraca.h **********************************************************
 ****************************************************************************
 *
 * routines for caraca receiver 
 * 
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *	modified for caraca RC5 receiver by Konrad Riedel <k.riedel@gmx.de>
 */

#ifndef HW_CARACA_H
#define HW_CARACA_H

#include "drivers/lirc.h"

int caraca_decode(struct ir_remote *remote,
		  ir_code *prep,ir_code *codep,ir_code *postp,
		  int *repeat_flagp,
		  lirc_t *min_remaining_gapp,
		  lirc_t *max_remaining_gapp);
int caraca_init(void);
int caraca_deinit(void);
char *caraca_rec(struct ir_remote *remotes);

#endif
