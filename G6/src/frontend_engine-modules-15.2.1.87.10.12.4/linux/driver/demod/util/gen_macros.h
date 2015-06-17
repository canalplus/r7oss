/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : gen_macros.h
Author :           LLA

LLA generic file

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created

************************************************************************/
#ifndef _GEN_MACROS_H
#define _GEN_MACROS_H

	/* MACRO definitions */
#define ABS(X) ((X) < 0 ? (-1 * (X)) : (X))
#define MAX(X, Y) ((X) >= (Y) ? (X) : (Y))
#define MIN(X, Y) ((X) <= (Y) ? (X) : (Y))
#define INRANGE(X, Y, Z) ((((X) <= (Y)) && ((Y) <= (Z))) || \
			(((Z) <= (Y)) && ((Y) <= (X))) ? 1 : 0)

#ifndef MAKEWORD
#define MAKEWORD(X, Y) (((X) << 8) + (Y))
#endif

#define LSB(X) (((X) & 0xFF))
#define MSB(Y) (((Y) >> 8) & 0xFF)
#define MMSB(Y) (((Y) >> 16) & 0xFF)

#endif /* _GEN_MACROS_H */
