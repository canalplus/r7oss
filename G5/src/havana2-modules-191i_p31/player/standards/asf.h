/************************************************************************
Copyright (C) 2002, 2005 STMicroelectronics. All Rights Reserved.

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

Source file name : demux_asf.h
Author :           Daniel

Definition of the asf demux implementation of the demux pure virtual class
module for havana.


Date        Modification                                    Name
----        ------------                                    --------
17-Nov-05   Cloned from demux_avi.h                         Daniel

************************************************************************/

#ifndef H_ASF
#define H_ASF

#include "asf_guids.h"

// generic
#define ASF_OBJECT_ID_OFFSET 0
#define ASF_OBJECT_SIZE_OFFSET 16

// file properties object
#define ASF_FILE_PROPERTIES_FLAGS_OFFSET 88
#define ASF_FILE_PROPERTIES_MINIMUM_DATA_PACKET_SIZE_OFFSET 92
#define ASF_FILE_PROPERTIES_MAXIMUM_DATA_PACKET_SIZE_OFFSET 96

// stream properties object
#define ASF_STREAM_PROPERTIES_STREAM_TYPE_OFFSET 24
#define ASF_STREAM_PROPERTIES_FLAGS_OFFSET 72

// clock object
#define ASF_CLOCK_CLOCK_TYPE_OFFSET 24
#define ASF_CLOCK_CLOCK_SIZE_OFFSET 40

// data object
#define ASF_DATA_TOTAL_DATA_PACKETS_OFFSET 40

#endif // H_ASF
