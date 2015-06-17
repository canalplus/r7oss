/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
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

struct dvb_device*  AudioInit                          (struct DeviceContext_s*        Context);
int                 AudioIoctlPlay                     (struct DeviceContext_s*        Context);
int                 AudioIoctlStop                     (struct DeviceContext_s*        Context);
int                 AudioIoctlSetId                    (struct DeviceContext_s*        Context,
                                                        int                            Id);
int                 AudioIoctlSetPlayInterval          (struct DeviceContext_s*        Context,
                                                        audio_play_interval_t*         PlayInterval);

#endif
