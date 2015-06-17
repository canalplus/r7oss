/************************************************************************
Copyright (C) 2002 STMicroelectronics. All Rights Reserved.

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

Source file name : remote.h
Author :           Nick

Contains the protoypes for the remote_control package

************************************************************************/

#ifndef __REMOTE_H
#define __REMOTE_H

typedef enum
{
    NoKeyPressed                = 0xffffffff,
    Key0                        = 0x30,
    Key1                        = 0x31,
    Key2                        = 0x32,
    Key3                        = 0x33,
    Key4                        = 0x34,
    Key5                        = 0x35,
    Key6                        = 0x36,
    Key7                        = 0x37,
    Key8                        = 0x38,
    Key9                        = 0x39,
    KeyMute                     = 128,
    KeyStandby                  = 129,
    KeyExit                     = 130,
    KeyFavourite                = 131,
    KeyTeletext                 = 132,
    KeyEpg                      = 134,
    KeyBack                     = 135,
    KeyMenu                     = 136,
    KeySelect                   = 0x40000004,
    KeyUpArrow                  = 137,
    KeyDownArrow                = 138,
    KeyLeftArrow                = 139,
    KeyRightArrow               = 140,
    KeyPause                    = 141,
    KeyInfo                     = 142,
    KeyButtonRed                = 143,
    KeyButtonGreen              = 144,
    KeyButtonYellow             = 145,
    KeyButtonBlue               = 146,
    KeyPageUp                   = 147,
    KeyRewind                   = 148,
    KeyPlay                     = 149,
    KeyFastFroward              = 150,
    KeyPageDown                 = 151,
    KeyRecord                   = 152,
    KeyStop                     = 153,
    KeyAudio                    = 154,
    KeyHdd                      = 155,
    KeyDvd                      = 156,
    KeyA                        = 157,
    KeyB                        = 158,
    KeyUnknown
} remote_control_key_t;

remote_control_key_t HavanaReadKey (void);

#endif  /* #ifndef __REMOTE_H */

/* End of remote.h */


