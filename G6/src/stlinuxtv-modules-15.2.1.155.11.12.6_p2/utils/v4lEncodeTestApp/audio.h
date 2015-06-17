/************************************************************************
 * Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV utilities

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV utilities may alternatively be licensed under a proprietary
license from ST.

Source file name : audio.h

Audio encoder related functions

************************************************************************/

#ifndef AUDIO_H_
#define AUDIO_H_

#include <linux/videodev2.h>
#include "v4lEncodeTestApp.h"

/* Print out parsed settings */
#define AUDIO_PARAMS_VERBOSE

/* V4L2 driver parameters - end*/
int parse_audio_ini(drv_context_t *ContextPt, char *filename);
int encode_audio(drv_context_t *ContextPt);

#endif /* AUDIO_H_ */
