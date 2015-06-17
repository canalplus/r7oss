/************************************************************************
Copyright (C) 2009-2010 STMicroelectronics. All Rights Reserved.

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
 * Header file for the V4L2 interface driver containing the defines
 * to be exported to the user level.
************************************************************************/


#ifndef DVB_V4L2_EXPORT_H_
#define DVB_V4L2_EXPORT_H_

/*
 * Video buffers captured from a LINUXDVB_* input will normally be in the
 * Y'CbCr _video_ range (either conforming to BT.709 or BT.601). They will
 * have been converted into R'G'B' colorspace by the hardware if the
 * application requested the driver to do so. That means the application will
 * get _video_ range buffers by default.
 * In addition, the hardware is able to scale the data to _full_ range
 * (identical to computer graphics). Therefore, applications which want the
 * data in _full_ (computer graphics) range will have to add this flag in the
 * VIDIOC_QBUF ioctl.
 */
#define V4L2_BUF_FLAG_FULLRANGE 0x10000

/*
 * V4L2 Implemented Controls 
 *
 */
enum 
{
    V4L2_CID_STM_DVBV4L2_FIRST		= (V4L2_CID_PRIVATE_BASE+300),
    
    V4L2_CID_STM_BLANK,	

    V4L2_CID_STM_AUDIO_O_FIRST,
    V4L2_CID_STM_AUDIO_O_LAST,
    
    V4L2_CID_STM_DVBV4L2_LAST
};


#endif /*DVB_V4L2_EXPORT_H_*/
