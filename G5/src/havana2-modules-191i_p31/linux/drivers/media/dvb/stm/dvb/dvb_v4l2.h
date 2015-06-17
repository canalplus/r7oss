/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

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

Source file name : dvb_v4l2.h
Author :           Pete

Describes internal v4l interfaces

Date        Modification                                    Name
----        ------------                                    --------
09-Dec-07   Created                                         Pete

************************************************************************/

#ifndef __DVB_V4L2
#define __DVB_V4L2

void* stm_v4l2_findbuffer(unsigned long userptr, unsigned int size,int device);

#endif
