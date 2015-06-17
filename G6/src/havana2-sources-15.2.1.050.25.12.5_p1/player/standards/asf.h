/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
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
