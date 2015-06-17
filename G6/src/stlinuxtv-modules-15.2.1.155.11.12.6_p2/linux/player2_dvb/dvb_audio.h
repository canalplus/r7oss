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

Source file name : dvb_audio.h - audio device definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-03   Created                                         Julian

************************************************************************/

#ifndef H_DVB_AUDIO
#define H_DVB_AUDIO

#include "dvbdev.h"

int AudioInitSysfsAttributes(struct AudioDeviceContext_s *Context);
void AudioRemoveSysfsAttributes(struct AudioDeviceContext_s *Context);
struct dvb_device *AudioInit(struct AudioDeviceContext_s *Context);
int AudioInitCtrlHandler(struct AudioDeviceContext_s *Context);
int AudioInitSubdev(struct AudioDeviceContext_s *Context);
void AudioReleaseSubdev(struct AudioDeviceContext_s *Context);
int AudioIoctlPlay(struct AudioDeviceContext_s *Context);
int AudioIoctlStop(struct AudioDeviceContext_s *Context);
int AudioIoctlClearBuffer(struct AudioDeviceContext_s *Context);
int AudioIoctlSetPlayInterval(struct AudioDeviceContext_s *Context,
			      audio_play_interval_t * PlayInterval);
int AudioIoctlSetSpeed(struct AudioDeviceContext_s *Context, int Speed,
			      int PlaySpeedUpdate);
int AudioSetDemuxId(struct AudioDeviceContext_s *Context, int Id);

#endif
