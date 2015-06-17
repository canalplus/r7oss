/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

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

Source file name : to_string.cpp
Author :           Daniel

Massively overloaded to_string() method to decode various types.
This is mostly for debugging/logging purposes.

Date        Modification                                    Name
----        ------------                                    --------
7-Jan-09    Created                                         Daniel

************************************************************************/

#include "player_types.h"

#define CASE(x) case x: return #x
#define DEFAULT default: return "UNKNOWN"

const char *ToString(PlayerStreamType_t StreamType)
{
    switch (StreamType) {
	CASE(StreamTypeNone);
	CASE(StreamTypeAudio);
	CASE(StreamTypeVideo);
	CASE(StreamTypeOther);
	DEFAULT;
    }
}
