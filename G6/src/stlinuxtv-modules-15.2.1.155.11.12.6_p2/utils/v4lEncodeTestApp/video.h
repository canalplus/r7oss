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

Source file name : video.h

Video encoder related functions

************************************************************************/

#ifndef VIDEO_H_
#define VIDEO_H_

#include <linux/videodev2.h>
#include "v4lEncodeTestApp.h"

int parse_video_ini(drv_context_t *ContextPt, char *filename);
int encode_video(drv_context_t *ContextPt);

#endif /* VIDEO_H_ */
