/*
   ST Microelectronics BDispII driver - some types we use

   (c) Copyright 2008       STMicroelectronics Ltd.

   All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __STM_TYPES_H__
#define __STM_TYPES_H__



#define likely(x)       __builtin_expect(!!(x),1)
#define unlikely(x)     __builtin_expect(!!(x),0)

/* general driver stuff */
typedef struct _STGFX2DeviceData STGFX2DeviceData;
typedef struct _STGFX2DriverData STGFX2DriverData;




/* BDisp stuff */
/* general config */
typedef struct _BltNodeGroup00 BltNodeGroup00;
/* target config */
typedef struct _BltNodeGroup01 BltNodeGroup01;
/* color fill */
typedef struct _BltNodeGroup02 BltNodeGroup02;
/* source 1 config */
typedef struct _BltNodeGroup03 BltNodeGroup03;
/* source 2 config */
typedef struct _BltNodeGroup04 BltNodeGroup04;
/* source 3 config */
typedef struct _BltNodeGroup05 BltNodeGroup05;
/* clip config */
typedef struct _BltNodeGroup06 BltNodeGroup06;
/* clut & color space conversion */
typedef struct _BltNodeGroup07 BltNodeGroup07;
/* filters control and plane mask */
typedef struct _BltNodeGroup08 BltNodeGroup08;
/* 2D (chroma) filter control */
typedef struct _BltNodeGroup09 BltNodeGroup09;
/* 2D (lumna) filter control */
typedef struct _BltNodeGroup10 BltNodeGroup10;
/* flicker filter */
typedef struct _BltNodeGroup11 BltNodeGroup11;
/* color key 1 and 2 */
typedef struct _BltNodeGroup12 BltNodeGroup12;
/* static addressing and user */
typedef struct _BltNodeGroup14 BltNodeGroup14;
/* input versatile matrix 0, 1, 2, 3 */
typedef struct _BltNodeGroup15 BltNodeGroup15;
/* output versatile matrix 0, 1, 2, 3 */
typedef struct _BltNodeGroup16 BltNodeGroup16;
/* VC1 range engine */
typedef struct _BltNodeGroup18 BltNodeGroup18;


#endif /* __STM_TYPES_H__ */
