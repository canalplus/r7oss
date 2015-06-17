/***********************************************************************
 *
 * File: display/ip/stmmisrviewport.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_MISR_VIEWPORT_H
#define _STM_MISR_VIEWPORT_H

/*
 * The following routines are generic helpers for ST video devices.
 * They calculate a misr viewport line number or pixel number, suitable
 * for programming the hardware; taking into account the video mode's
 * VBI region and horizontal front porch. This is needed by a number
 * of different hardware blocks spanning various devices, so we want
 * the calculation in one place so everything is consistent and the
 * code is more readable.
 *
 * The values are clamped to sensible maximum values based on the video mode.
 *
 * It also means if we have got this wrong we only have to change it
 * again in one place!
 */

static inline uint32_t STCalculateMisrViewportLine(const stm_display_mode_t *pModeLine, int y)
{
    /*
     * Video frame line numbers start at 1, y starts at 0 as in a standard
     * graphics coordinate system. In interlaced modes the start line is the
     * field line number of the odd field, but y is still defined as a
     * progressive frame.
     *
     * Note that y can be negative to place a viewport before
     * the active video area (i.e. in the VBI).
     *
     * But MISR calculation is base on field line in interlace modes, so y has to be divided by 2 here.
     */
    int adjust  = (pModeLine->mode_params.scan_type == STM_INTERLACED_SCAN)?2:1;
    int line    = (pModeLine->mode_params.active_area_start_line) + y/adjust;
    int maxline = (pModeLine->mode_params.active_area_start_line) + (pModeLine->mode_params.active_area_height - 1);

    if(line < 1)       line = 1;
    if(line > maxline) line = maxline;

    return line;
}

static inline uint32_t STCalculateMisrViewportPixel(const stm_display_mode_t *pModeLine, int x)
{
    /*
     * x starts at 0 as in a standard graphics coordinate system.
     * MISR is not stable for the first 8 pixels of the first frame, so the minimum pixel is set to 8.
     */
    int pixel    = (pModeLine->mode_params.active_area_start_pixel + x);
    int maxpixel = (pModeLine->mode_params.active_area_start_pixel + pModeLine->mode_params.active_area_width - 1);

    if(pixel < 8)   pixel = 8;
    if(pixel > maxpixel) pixel = maxpixel;

    return pixel;
}
#endif //_STM_MISR_VIEWPORT_H
