/*******************************************************************************
 *
 * File: display/ip/vdp/STRefVDPFilters.h
 * Copyright (c) 2005-2007,2008 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Description : Contains filter coeffs for Horizontal & vertical format
 *               (HAL video display displaypipe family) taken from the
 *               ST Reference drivers.
 *
 ******************************************************************************/

#ifndef _STREF_VDP_FILTERS_H
#define _STREF_VDP_FILTERS_H

#define HFC_NB_COEFS (35 * 4)
#define VFC_NB_COEFS (22 * 4)

#include "VdpFilter_legacy.h"
#include "VdpFilter_medium.h"
#include "VdpFilter_sharp.h"
#include "VdpFilter_smooth.h"

/***********************************************************/
/*                                                         */
/*                 HORIZONTAL COEFFICIENTS                 */
/*         ----------------------------------------        */
/*         1      < Zoom < 10     -> use coeffs 'A'        */
/*                  Zoom = 1      -> use coeffs 'B'        */
/*         0.80   < Zoom < 1      -> use coeffs 'C'        */
/*         0.60   < Zoom < 0.80   -> use coeffs 'D'        */
/*         0.50   < Zoom < 0.60   -> use coeffs 'E'        */
/*         0.25   < Zoom < 0.50   -> use coeffs 'F'        */
/*                                                         */
/***********************************************************/

/* Filter coeffs available for Horizontal zoom 1 (no zoom) */
/*---------------------------------------------------------*/
static const signed char HorizontalCoefficientsB[HFC_NB_COEFS] =
  {
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0, -128,    0
  };


/***********************************************************/
/*                                                         */
/*                 VERTICAL COEFFICIENTS                   */
/*         ----------------------------------------        */
/*         1      < Zoom < 10     -> use coeffs 'A'        */
/*                  Zoom = 1      -> use coeffs 'B'        */
/*         0.80   < Zoom < 1      -> use coeffs 'C'        */
/*         0.60   < Zoom < 0.80   -> use coeffs 'D'        */
/*         0.50   < Zoom < 0.60   -> use coeffs 'E'        */
/*         0.25   < Zoom < 0.50   -> use coeffs 'F'        */
/*                                                         */
/***********************************************************/

/* Filter coeffs available for Vertical zoom 1 (no zoom) */
/*-------------------------------------------------------*/
static const signed char VerticalCoefficientsB[VFC_NB_COEFS] =
  {
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,    0,    0,   64
  };

#endif // _STREF_VDP_FILTER_H
