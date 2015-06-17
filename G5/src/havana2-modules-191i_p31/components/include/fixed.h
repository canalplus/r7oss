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

Source file name : Fixed.h
Author :           Julian

Definition of some common defines and structures used in more than one class


Date        Modification                                    Name
----        ------------                                    --------
19-Sep-06   Created                                         Julian

************************************************************************/

#ifndef H_FIXED
#define H_FIXED

#ifdef __cplusplus
extern "C" {
#endif

/*
Fixed point numbers for frame rates aspect ratios etc
We are using a Q16.16 format
*/

typedef unsigned int                    Fixed_t;

#define FIX(a)                          (a << 16)
#define INTEGER_PART(a)                 (a >> 16)
#define TIMES(a, b)                     (Fixed_t)((((unsigned long long)a) * ((unsigned long long)b)) >> 16)
#define DIVIDE(a, b)                    (Fixed_t)((((unsigned long long)a) << 16) / b)
#define FRACTIONAL_PART(a)              (a & 0xffff)
#define FRACTIONAL_DIVISOR              0x10c6f687              /* 0x800000000000 / 500000.5 */
#define FRACTIONAL_DECIMAL(a)           (unsigned int)((((unsigned long long)FRACTIONAL_PART(a)) << 32ULL) / FRACTIONAL_DIVISOR)

#ifdef __cplusplus
}
#endif

#endif

