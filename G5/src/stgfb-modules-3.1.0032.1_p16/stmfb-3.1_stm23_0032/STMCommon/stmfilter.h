/***********************************************************************
 *
 * File: STMCommon/stmfilter.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMFILTER_H
#define _STMFILTER_H

/*
 * TODO: This file is almost obsolete now, maybe we should look to just fold
 *       this into the video plane classes.
 */
static inline void
Get5TapFilterSetup (int &pixelpos,
                    int &size,
                    int &repeat,
                    int  increment,
                    int  fixedpointONE)
{
  /*
   * 5 tap filters have a slight complication in that the initial phase of the
   * filter is offset by half. This allows us to have a negative
   * starting sub-pixel position up to -1/2, which is useful for interpolating
   * a top field from bottom field data.
   *
   * Vertically we need 2 pixels in the pipeline before the first real pixel.
   * The vertical filter can only repeat the pixel up to 2 times.
   */
  pixelpos += fixedpointONE / 2; /* +0.5 */

  if(pixelpos < 0)
  {
    DEBUGF2(1,("Get5TapFilterSetup - start position out of range\n"));
    DEBUGF2(1,("Get5TapFilterSetup - pixelpos = 0x%08x increment = 0x%08x\n",
               pixelpos,increment));
    /*
     * Reset to first pixel rather than failing, a slightly incorrect
     * image is better than not queueing the image for display at all.
     */
    pixelpos = 0;
  }

  repeat = 2;
}

#endif /* _STMFILTER_H */
