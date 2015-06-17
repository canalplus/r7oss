/***********************************************************************

This file is part of stgfx2 driver

COPYRIGHT (C) 2003, 2004, 2005 STMicroelectronics - All Rights Reserved

License type: LGPLv2.1

stgfx2 is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License version 2.1 as published by
the Free Software Foundation.

stgfx2 is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

The stgfx2 Library may alternatively be licensed under a proprietary
license from ST.

This file was created by STMicroelectronics on 2008-09-12
and modified by STMicroelectronics on 2014-05-28

************************************************************************/

#ifndef __STGFX2_FEATURES_H__
#define __STGFX2_FEATURES_H__


/************************** implementation details **************************/
/* the following are basically implementation details of this driver and you
   should not need to touch them. */

/* we used to accelerate DRAWLINE just for being able to draw horizontal
   and vertical lines in hardware, which is what happens in a
   cairo/gtk/webkit environment. But nowadays DirectFB is intelligent enough
   to draw rectangles instead in this case, so that code was removed. */
#define STGFX2_VALID_DRAWINGFUNCTIONS (0 \
                                       | DFXL_FILLRECTANGLE \
                                       | DFXL_DRAWRECTANGLE \
                                      )

#define STGFX2_VALID_BLITTINGFUNCTIONS (DFXL_BLIT \
                                        | DFXL_STRETCHBLIT \
                                        | DFXL_BLIT2)

/* Do we want clipping to be done in hardware? This is needed for better
   looking stretch blit support (at the edges) but it can significantly
   increase CPU and blitter usage because operations are still executed,
   just the result is not written.
   On current SoCs this might also lead to the BDisp accessing memory beyond
   a 64MB bank boundary, thus this should never be enabled! */
#undef STGFX2_SUPPORT_HW_CLIPPING


#endif /* __STGFX2_FEATURES_H__ */
