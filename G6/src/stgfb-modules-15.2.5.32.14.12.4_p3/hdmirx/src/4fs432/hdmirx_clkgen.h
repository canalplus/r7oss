/***********************************************************************
 *
 * File: hdmirx/src/clkgen/hdmirx_clkgen.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_CLOCKGEN_H__
#define __HDMIRX_CLOCKGEN_H__

/*Includes------------------------------------------------------------------------------*/
#include <stddefs.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* Private Types ---------------------------------------------------------- --------------*/

  /* Private Constants --------------------------------------------------------------------- */
  typedef enum {
    HDMIRX_PIX_AVDDS1 = 0,
    HDMIRX_AUD_AVDDS2,
    HDMIRX_PIX_AVDDS3,
    HDMIRX_AUD_AVDDS4,
  }
  sthdmirx_AVDDStypes_t;
  typedef enum
  {
    SEL_INPUT_CLOCK_FROM_DDS,
    SEL_INPUT_CLOCK_FROM_TMDS_LINK
  } sthdmirx_input_clkSource_t;

  /* Private variables (static) --------------------------------------------------------------- */

  /* Global Variables ----------------------------------------------------------------------- */

  /* Private Macros ------------------------------------------------------------------------ */

  /* Exported Macros--------------------------------------------------------- --------------*/

  /* Exported Functions ----------------------------------------------------- ---------------*/
  void sthdmirx_clkgen_powerdownDDS(sthdmirx_AVDDStypes_t estAVdds,
                                    U32 ulBaseAddrs);
  void sthdmirx_clkgen_powerupDDS(sthdmirx_AVDDStypes_t estAVdds,
                                  U32 ulBaseAddrs);
  void sthdmirx_clkgen_init(U32 ulBaseAddrs, const void *Handle);
  void sthdmirx_clkgen_term(void);
  U32 sthdmirx_clkgen_DDS_current_freq_get(sthdmirx_AVDDStypes_t estAVdds,
      U32 ulBaseAddrs);
  U16 sthdmirx_clkgen_DDS_tracking_error_get(sthdmirx_AVDDStypes_t estAVdds,
      U32 ulBaseAddrs);
  void sthdmirx_clkgen_DDS_openloop_force(
    sthdmirx_AVDDStypes_t estAVdds,U32 ulDdsInitFreq,U32 ulBaseAddrs);
  BOOL sthdmirx_clkgen_DDS_closeloop_force(sthdmirx_AVDDStypes_t estAVdds,
      U32 ulDdsInitFreq,U32 ulBaseAddrs);
  /* ------------------------------- End of file --------------------------------------------- */
#ifdef __cplusplus
}
#endif
#endif				/*end of __HDMIRX_CLOCKGEN_H__ */
