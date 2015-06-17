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

Source file name : allocatorio.h
Author :           Nick

Contains the ioctl interface between the allocator user level components
(allocinline.h) and the allocator device level components.

Date        Modification                                    Name
----        ------------                                    --------
14-Jan-03   Created                                         Nick
12-Nov-07   Changed to use partition names for BPA2         Nick

************************************************************************/

#ifndef H_ALLOCATORIO
#define H_ALLOCATORIO

#define ALLOCATOR_MAX_PARTITION_NAME_SIZE	64

#define ALLOCATOR_IOCTL_ALLOCATE_DATA           1

#define ALLOCATOR_DEVICE                        "/dev/allocator"

//

typedef struct allocator_ioctl_allocate_s
{
    unsigned int         RequiredSize;
    char		 PartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
    unsigned char	*CachedAddress;
    unsigned char	*UnCachedAddress;
    unsigned char       *PhysicalAddress;
} allocator_ioctl_allocate_t;

//

#endif
