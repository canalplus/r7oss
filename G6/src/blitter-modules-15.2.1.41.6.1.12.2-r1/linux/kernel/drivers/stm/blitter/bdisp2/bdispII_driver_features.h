/*
   ST Microelectronics BDispII driver - features of the driver

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

#ifndef __BDISPII_DRIVER_FEATURES_H__
#define __BDISPII_DRIVER_FEATURES_H__


/***************************** some debug/stats *****************************/
/* should we printf() the time we had to wait for an entry in the nodelist
   to be available? */
#undef BDISP2_PRINT_NODE_WAIT_TIME

/* Do we want clipping to be done in hardware? This is needed for better
   looking stretch blit support (at the edges) but it can significantly
   increase CPU and blitter usage because operations are still executed,
   just the result is not written.
   On current SoCs this might also lead to the BDisp accessing memory beyond
   a 64MB bank boundary, thus this should never be enabled! */
#undef BDISP2_SUPPORT_HW_CLIPPING

/* If this is defined, the CLUT will be uploaded only once into the BDisp
   and will be assumed not to change anytime after outside our control.
   So if multiple applications using different CLUTs on different BDisp AQs
   are active (or if somebody is using the kernel interface in parallel) the
   CLUT feature will not work correctly. This feature is therefore disabled
   by default. In earlier versions of this driver we gained a little
   performance increase, but this is not the case anymore, so there's no
   point in enabling this option... */
#undef BDISP2_CLUT_UNSAFE_MULTISESSION


#endif /* __STGFX2_FEATURES_H__ */
