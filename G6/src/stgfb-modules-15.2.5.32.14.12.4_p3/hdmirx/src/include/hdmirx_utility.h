/***********************************************************************
 *
 * File: hdmirx/src/include/hdmirx_utility.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_UTILITY_H__
#define __HDMIRX_UTILITY_H__

/*Includes------------------------------------------------------------------------------*/

/* Support dual Interface C & C++*/
#ifdef __cplusplus
extern "C" {
#endif

  /* Private Types ---------------------------------------------------------- --------------*/


  /* Private Constants -------------------------------------------------------------------- */

  /* Private variables (static) ----------------------------------------------------------- */

  /* Global Variables --------------------------------------------------------------------- */

  /* Private Macros ----------------------------------------------------------------------- */

#define     MAKE_WORD(LSB, MSB)             ((LSB) | ((U16)(MSB) << 8))
#define     MAKE_DWORD(LSB, B1, B2, MSB)    ((U32)(MAKE_WORD(LSB, B1)) | \
                                        ((U32)(MAKE_WORD(B2, MSB)) << 16))

#define     SET_BIT(a, bit)         (a) |= (bit)
#define     CLEAR_BIT(a, bit)       (a) &= ~(bit)
#define     CHECK_BIT(a, bit)       (((a) & (bit)) == (bit))

#define     HDMI_MIN(a, b)          (((a) < (b)) ? (a) : (b))
#define     HDMI_MAX(a, b)          (((a) > (b)) ? (a) : (b))
  /* Exported Macros--------------------------------------------------------- --------------*/

  /* Exported Functions ----------------------------------------------------- ---------------*/

  /* ------------------------------- End of file ---------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif

#endif /*end of __HDMIRX_ANALOGPHY_H__*/
