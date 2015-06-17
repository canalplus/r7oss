/*      $Id: hw_pixelview.h,v 5.4 2007/07/29 18:20:09 lirc Exp $      */

/****************************************************************************
 ** hw_pixelview.h **********************************************************
 ****************************************************************************
 *
 * routines for PixelView Play TV receiver
 * 
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

#ifndef _HW_PIXELVIEW_H
#define _HW_PIXELVIEW_H

#include "drivers/lirc.h"

int pixelview_decode(struct ir_remote *remote,
		     ir_code *prep,ir_code *codep,ir_code *postp,
		     int *repeat_flagp,
		     lirc_t *min_remaining_gapp,
		     lirc_t *max_remaining_gapp);
int pixelview_init(void);
int pixelview_deinit(void);
char *pixelview_rec(struct ir_remote *remotes);

#endif
