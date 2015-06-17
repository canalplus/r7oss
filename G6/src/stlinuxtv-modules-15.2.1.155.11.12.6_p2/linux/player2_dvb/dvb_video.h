/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : dvb_video.h - video device definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-03   Created                                         Julian

************************************************************************/

#ifndef H_DVB_VIDEO
#define H_DVB_VIDEO

#include <dvbdev.h>

int VideoInitSysfsAttributes(struct VideoDeviceContext_s *Context);
void VideoRemoveSysfsAttributes(struct VideoDeviceContext_s *Context);
struct dvb_device *VideoInit(struct VideoDeviceContext_s *Context);
int VideoInitCtrlHandler(struct VideoDeviceContext_s *Context);
int VideoInitSubdev(struct VideoDeviceContext_s *Context);
void VideoReleaseSubdev(struct VideoDeviceContext_s *Context);
int VideoIoctlPlay(struct VideoDeviceContext_s *Context);
int VideoIoctlStop(struct VideoDeviceContext_s *Context, unsigned int Mode);
int VideoIoctlSetId(struct VideoDeviceContext_s *Context, int Id);
int VideoIoctlClearBuffer(struct VideoDeviceContext_s *Context);
int VideoIoctlGetSize(struct VideoDeviceContext_s *Context, video_size_t * Size);
int VideoIoctlSetSpeed(struct VideoDeviceContext_s *Context, int Speed,
		       int PlaySpeedUpdate);
int VideoIoctlSetPlayInterval(struct VideoDeviceContext_s *Context,
			      video_play_interval_t * PlayInterval);
int PlaybackInit(struct VideoDeviceContext_s *Context);
int VideoSetInputWindowMode(struct VideoDeviceContext_s *Context,
                            unsigned int Mode);
int VideoGetInputWindowMode(struct VideoDeviceContext_s *Context,
                            unsigned int *Mode);
int VideoSetOutputWindowMode(struct VideoDeviceContext_s *Context,
                             unsigned int Mode);
int VideoGetOutputWindowMode(struct VideoDeviceContext_s *Context,
                             unsigned int *Mode);
int VideoSetOutputWindowValue(struct VideoDeviceContext_s *Context,
			 unsigned int Left,
			 unsigned int Top,
			 unsigned int Width, unsigned int Height);
int VideoGetOutputWindowValue(struct VideoDeviceContext_s *Context,
			 unsigned int *Left,
			 unsigned int *Top,
			 unsigned int *Width, unsigned int *Height);
int VideoSetInputWindowValue(struct VideoDeviceContext_s *Context,
			unsigned int Left,
			unsigned int Top,
			unsigned int Width, unsigned int Height);
int VideoGetInputWindowValue(struct VideoDeviceContext_s *Context,
			unsigned int *Left,
			unsigned int *Top,
			unsigned int *Width, unsigned int *Height);
int VideoGetPixelAspectRatio(struct VideoDeviceContext_s *Context,
			     unsigned int *Numerator,
			     unsigned int *Denominator);
int VideoIoctlGetFrameRate(struct VideoDeviceContext_s *Context,
				  int *FrameRate);
#endif
