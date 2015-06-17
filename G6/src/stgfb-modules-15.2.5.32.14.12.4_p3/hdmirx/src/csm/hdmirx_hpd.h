/***********************************************************************
 *
 * File: hdmirx/src/csm/hdmirx_hpd.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_HPD_H__
#define __HDMIRX_HPD_H__

/*Includes------------------------------------------------------------------------------*/
#include "stddefs.h"

#ifdef __cplusplus
extern "C" {
#endif

  /* Private Types ---------------------------------------------------------- --------------*/
#define     STHDMIRX_CABLE_CONNECT_DEBOUNCE_TIME            10000	/*100 msec */

  /* Private Constants --------------------------------------------------------------------- */
  typedef enum {
    ACTIVE_HPD_HW_IN_CSM,
    ACTIVE_HPD_HW_IN_CORE
  }
  sthdmirx_HPD_hardware_select_t;

  /* ------------------------------- End of file --------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif				/* __HDMIRX_HPD_H__ */
