/************************************************************************
Copyright (C) 2010, 2011,2012 STMicroelectronics. All Rights Reserved.

This file is part of the hdmirx Library.

hdmirx is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

hdmirx is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with hdmirx; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The hdmirx Library may alternatively be licensed under a proprietary
license from ST.

Source file name : hdmirx_system.h
Author :            Pravin Kumar

Date        Modification                                    Name
----        ------------                                    --------
6th July,2009   Created                                         Pravin Kumar

************************************************************************/
#ifndef __HDMIRX_SYSTEM_H__
#define __HDMIRX_SYSTEM_H__

/*Includes------------------------------------------------------------------------------*/

#include "hdmirx_drv.h"

/* Support dual Interface C & C++*/
#ifdef __cplusplus
extern "C" {
#endif

  /* Private Types ---------------------------------------------------------- --------------*/

  /* Private Constants --------------------------------------------------------------------- */

  /* Private variables (static) --------------------------------------------------------------- */

  /* Global Variables ----------------------------------------------------------------------- */

  /* Private Macros ------------------------------------------------------------------------ */

  /* Exported Macros--------------------------------------------------------- --------------*/

  /* Exported Functions ----------------------------------------------------- ---------------*/
  void sthdmirx_reset_signal_process_handler(
    hdmirx_route_handle_t *const pInpHandle);
  void sthdmirx_main_signal_process_handler(
    hdmirx_route_handle_t *const pInpHandle);

  stm_error_code_t STHDMIRX_EvaluateInputHandleEvents(
    hdmirx_route_handle_t *const pInpHandle);
  stm_error_code_t sthdmirx_evaluate_packet_events(
    hdmirx_route_handle_t *const pInpHandle);

  /* Equalizer functions*/
  void sthdmirx_PHY_adaptive_signal_equalizer_handler(
    const hdmirx_handle_t Handle);

  /* Source Plug detection prototypes*/
  BOOL sthdmirx_source_plug_detect_task(const hdmirx_handle_t Handle);
  stm_error_code_t sthdmirx_CSM_init(const hdmirx_handle_t Handle, sthdmirx_CSM_config_t csm_config);

#ifdef STHDMIRX_HDCP_KEY_LOADING_THRU_ST40
  BOOL sthdmirx_load_HDCP_key_data(const hdmirx_handle_t Handle);
#endif

  void sthdmirx_ACP_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                         stm_hdmirx_acp_info_t *AcpInfo);
  void sthdmirx_ISRC_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                          stm_hdmirx_isrc_info_t *ISRCInfo);
  void sthdmirx_MPEG_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                          stm_hdmirx_mpeg_source_info_t *MpegInfo);
  void sthdmirx_SPD_info_frame_data_fill(
    const hdmirx_route_handle_t *pInpHandle,stm_hdmirx_spd_info_t *SpdInfo);
  void sthdmirx_VS_info_data_fill(
    const hdmirx_route_handle_t *pInpHandle,stm_hdmirx_vs_info_t *VsInfo);
  /* ------------------------------- End of file ---------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif
#endif				/*end of __HDMIRX_SYSTEM_H__ */
