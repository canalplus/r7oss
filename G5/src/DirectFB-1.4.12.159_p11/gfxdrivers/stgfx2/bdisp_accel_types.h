/*
   ST Microelectronics BDispII driver - hardware acceleration

   (c) Copyright 2007/2008  STMicroelectronics Ltd.

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

#ifndef __BDISP_ACCEL_TYPES_H__
#define __BDISP_ACCEL_TYPES_H__


#include <dfb_types.h>




/* general config */
struct _BltNodeGroup00 {
  u32 BLT_NIP;
  u32 BLT_CIC;
  u32 BLT_INS;
  u32 BLT_ACK;
};
/* target config */
struct _BltNodeGroup01 {
  u32 BLT_TBA;
  u32 BLT_TTY;
  u32 BLT_TXY;
  u32 BLT_TSZ_S1SZ;
};
/* color fill */
struct _BltNodeGroup02 {
  u32 BLT_S1CF;
  u32 BLT_S2CF;
};
/* source 1 config */
struct _BltNodeGroup03 {
  u32 BLT_S1BA;
  u32 BLT_S1TY;
  u32 BLT_S1XY;
  u32 dummy;
};
/* source 2 config */
struct _BltNodeGroup04 {
  u32 BLT_S2BA;
  u32 BLT_S2TY;
  u32 BLT_S2XY;
  u32 BLT_S2SZ;
};
/* source 3 config */
struct _BltNodeGroup05 {
  u32 BLT_S3BA;
  u32 BLT_S3TY;
  u32 BLT_S3XY;
  u32 BLT_S3SZ;
};
/* clip config */
struct _BltNodeGroup06 {
  u32 BLT_CWO;
  u32 BLT_CWS;
};
/* clut & color space conversion */
struct _BltNodeGroup07 {
  u32 BLT_CCO;
  u32 BLT_CML;
};
/* filters control and plane mask */
struct _BltNodeGroup08 {
  u32 BLT_FCTL_RZC;
  u32 BLT_PMK;
};
/* 2D (chroma) filter control */
struct _BltNodeGroup09 {
  u32 BLT_RSF;
  u32 BLT_RZI;
  u32 BLT_HFP;
  u32 BLT_VFP;
};
/* 2D (lumna) filter control */
struct _BltNodeGroup10 {
  u32 BLT_Y_RSF;
  u32 BLT_Y_RZI;
  u32 BLT_Y_HFP;
  u32 BLT_Y_VFP;
};
/* flicker filter */
struct _BltNodeGroup11 {
  u32 BLT_FF0;
  u32 BLT_FF1;
  u32 BLT_FF2;
  u32 BLT_FF3;
};
/* color key 1 and 2 */
struct _BltNodeGroup12 {
  u32 BLT_KEY1;
  u32 BLT_KEY2;
};
/* XYL */
struct _BltNodeGroup13 {
  u32 BLT_XYL;
  u32 BLT_XYP;
};
/* static addressing and user */
struct _BltNodeGroup14 {
  u32 BLT_SAR;
  u32 BLT_USR;
};
/* input versatile matrix 0, 1, 2, 3 */
struct _BltNodeGroup15 {
  u32 BLT_IVMX0;
  u32 BLT_IVMX1;
  u32 BLT_IVMX2;
  u32 BLT_IVMX3;
};
/* output versatile matrix 0, 1, 2, 3 */
struct _BltNodeGroup16 {
  u32 BLT_OVMX0;
  u32 BLT_OVMX1;
  u32 BLT_OVMX2;
  u32 BLT_OVMX3;
};
/* Pace Dot */
struct _BltNodeGroup17 {
  u32 BLT_PACE;
  u32 BLT_DOT;
};
/* VC1 range engine */
struct _BltNodeGroup18 {
  u32 BLT_VC1R;
  u32 dummy;
};
/* Gradient fill */
struct _BltNodeGroup19 {
  u32 BLT_HGF;
  u32 BLT_VGF;
};



#endif /* __BDISP_ACCEL_TYPES_H__ */
