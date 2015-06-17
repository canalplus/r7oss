/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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

Source file name : dvb.h
Author :           Nick

Definition of the constants/macros that define useful things associated with 
DVB transport streams.


Date        Modification                                    Name
----        ------------                                    --------
19-Aug-05   Created                                         Nick

************************************************************************/

#ifndef H_DVB
#define H_DVB

#include "mpeg2.h"

#define DVB_MAX_PIDS				8192

#define DVB_HEADER_SIZE				4
#define DVB_PACKET_SIZE				188

#define DVB_SYNC_BYTE				0x47
#define DVB_SYNC_BYTE_MASK			0x000000ff

#define DVB_PACKET_ERROR_MASK			0x00008000
#define DVB_PID_HIGH_MASK			0x00001f00
#define DVB_PID_LOW_MASK			0x00ff0000
#define DVB_PAYLOAD_UNIT_START_MASK		0x00004000
#define DVB_PRIORITY_MASK			0x00002000
#define DVB_SCRAMBLED_MASK			0x80000000
#define DVB_SCRAMBLING_POLARITY_MASK		0x40000000
#define DVB_ADAPTATION_FIELD_MASK		0x20000000
#define DVB_PAYLOAD_PRESENT_MASK		0x10000000
#define DVB_CONTINUITY_COUNT_MASK		0x0f000000

#define DVB_DISCONTINUITY_INDICATOR_MASK	0x80

#define DVB_VALID_PACKET( H )			(((H & DVB_SYNC_BYTE_MASK) == DVB_SYNC_BYTE) && ((H & DVB_PACKET_ERROR_MASK) == 0))
#define DVB_PID( H )				((H & DVB_PID_HIGH_MASK) | ((H & DVB_PID_LOW_MASK) >> 16))
#define DVB_PRIORITY( H )			((H & DVB_PRIORITY_MASK) != 0)
#define DVB_PUSI( H )				((H & DVB_PAYLOAD_UNIT_START_MASK) != 0)
#define DVB_SCRAMBLED( H )  	         	((H & DVB_SCRAMBLED_MASK) != 0)
#define DVB_SCRAMBLING_POLARITY( H )		((H & DVB_SCRAMBLING_POLARITY_MASK) != 0)
#define DVB_ADAPTATION_FIELD( H )		((H & DVB_ADAPTATION_FIELD_MASK) != 0)
#define DVB_PAYLOAD_PRESENT( H )		((H & DVB_PAYLOAD_PRESENT_MASK) != 0)
#define DVB_CONTINUITY_COUNT( H )		((H & DVB_CONTINUITY_COUNT_MASK) >> 24)

#define DVB_PCR_FLAG				0x10
#define DVB_PCR_PRESENT( H, P )			(DVB_ADAPTATION_FIELD(H) && ((P)[4] > 7) && (((P)[5] & DVB_PCR_FLAG) != 0))
#define DVB_GET_PCR_BIT_32( P )			((P)[6] >> 7)
#define DVB_GET_PCR_BITS_0_TO_31( P )		(((P)[6] << 25) | ((P)[7] << 17) | ((P)[8] << 9) | ((P)[9] << 1) | ((P)[10] >> 7))
#define DVB_GET_PCR_EXTENSION( P )		((((P)[10] & 0x1) << 8) | (P)[11])

#define DVB_MAX_START_CODES_PER_PACKET		16

#endif
