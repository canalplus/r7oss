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

Source file name : h264ppio.h
Author :           Nick

Contains the ioctl interface between the h264 pre-processor user level components
(h264ppinline.h) and the h264 pre-processor device level components.

Date        Modification                                    Name
----        ------------                                    --------
02-Dec-05   Created                                         Nick

************************************************************************/

#ifndef H_H264PPIO
#define H_H264PPIO

#define H264_PP_IOCTL_QUEUE_BUFFER              1
#define H264_PP_IOCTL_GET_PREPROCESSED_BUFFER   2

#define H264_PP_DEVICE                          "/dev/h264pp"

//

typedef struct h264pp_ioctl_queue_s
{
    unsigned int	  QueueIdentifier;
    void		 *InputBufferCachedAddress;
    void		 *InputBufferPhysicalAddress;
    void		 *OutputBufferCachedAddress;
    void		 *OutputBufferPhysicalAddress;
    unsigned int	  Field;
    unsigned int          InputSize;
    unsigned int          SliceCount;
    unsigned int          CodeLength;
    unsigned int          PicWidth;
    unsigned int          Cfg;
} h264pp_ioctl_queue_t;

//

typedef struct h264pp_ioctl_dequeue_s
{
    unsigned int	  QueueIdentifier;
    unsigned int	  OutputSize;
    unsigned int          ErrorMask;
} h264pp_ioctl_dequeue_t;

//

#endif
