/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_core_export.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_CORE_EXPORT_H__
#define __HDMIRX_CORE_EXPORT_H__

/*Includes------------------------------------------------------------------------------*/
#include <hdmirx_drv.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* Exported Types ---------------------------------------------------------- --------------*/
  typedef enum {
    MEAS_VERTICAL_TIMING_UNSTABLE,
    MEAS_HORIZONTAL_TIMING_UNSTABLE,
    MEAS_LINK_CLOCK_LOST,
    MEAS_HV_TIMINGS_STABLE
  }
  sthdmirx_HV_timing_meas_error_t;
  /* Private Constants --------------------------------------------------------------------- */

  /* Private variables (static) --------------------------------------------------------------- */

  /* Global Variables ----------------------------------------------------------------------- */

  /* Private Macros ------------------------------------------------------------------------ */

  /* Exported Macros--------------------------------------------------------- --------------*/

  /* Exported Functions ----------------------------------------------------- ---------------*/
  /*Packet processing Functions*/
  BOOL sthdmirx_CORE_SW_clear_packet_data_memory(const hdmirx_handle_t
      Handle);
  BOOL sthdmirx_CORE_clear_packet_memory(const hdmirx_handle_t Handle);
  BOOL sthdmirx_CORE_clear_packet(const hdmirx_handle_t Handle,
                                  sthdmirx_packet_id_t stPacketId);
  BOOL sthdmirx_CORE_ISR_SDP_client(hdmirx_route_handle_t *const pInpHdl);

  /* Measurement blk*/
  void sthdmirx_MEAS_reset_measurement(sthdmirx_signal_meas_ctrl_t *pMeasCtrl);
  void sthdmirx_MEAS_HW_init(const hdmirx_handle_t Handle, U32 RefClkFreq);
  void sthdmirx_MEAS_linkclk_meas(const hdmirx_handle_t Handle);
  void sthdmirx_MEAS_horizontal_timing_meas(const hdmirx_handle_t Handle);
  void sthdmirx_MEAS_vertical_timing_meas(const hdmirx_handle_t Handle);
  void sthdmirx_MEAS_check_double_clk_mode(
    sthdmirx_signal_meas_ctrl_t *const pMeasCtrl);
  void sthdmirx_MEAS_initialize_stabality_check_mode(
    sthdmirx_signal_meas_ctrl_t *const pMeasCtrl);
  void sthdmirx_MEAS_monitor_input_signal_stability(
    sthdmirx_signal_meas_ctrl_t *const pMeasCtrl);
  U16 sthdmirx_MEAS_get_PHY_decoder_error(const hdmirx_handle_t Handle,
                                          U8 uDurationMsec, U16 ErrMaskLevel);
  BOOL sthdmirx_MEAS_is_linkclk_present(const hdmirx_handle_t Handle);
  U8 sthdmirx_MEAS_input_wait_Vsync(const hdmirx_handle_t Handle,
                                    U8 B_NumSync);
  void sthdmirx_MEAS_print_mode(signal_timing_info_t const *vMode_p);
  sthdmirx_HV_timing_meas_error_t
  sthdmirx_MEAS_monitor_HVtiming_statbility(const hdmirx_handle_t
      Handle);

  /* core.c files function prototype*/
  void sthdmirx_CORE_reset(const hdmirx_handle_t pHandle,
                           sthdmirx_core_reset_ctrl_t uReset);
  void sthdmirx_CORE_enable_reset(const hdmirx_handle_t pHandle,
                                  BOOL uReset);
  void sthdmirx_CORE_enable_auto_reset_on_no_clk(const hdmirx_handle_t
      pHandle, BOOL uReset);
  void sthdmirx_CORE_enable_auto_reset_on_port_change(const
      hdmirx_handle_t pHandle, BOOL uReset);
  void sthdmirx_CORE_select_PHY_source(const hdmirx_handle_t pHandle, U8 Phy, BOOL bIsPolInv);
  BOOL sthdmirx_CORE_is_HDMI_signal(const hdmirx_handle_t pHandle);
  BOOL sthdmirx_CORE_is_HDMI1V3_signal(const hdmirx_handle_t pHandle);
  BOOL sthdmirx_CORE_HDMI_signal_process_handler(const hdmirx_handle_t
      pHandle);
  BOOL sthdmirx_CORE_DVI_signal_process_handler(hdmirx_handle_t const
      pHandle);
  BOOL sthdmirx_CORE_select_pixel_clk_out_domain(const hdmirx_handle_t
      pHandle);
  BOOL sthdmirx_CORE_config_clk(const hdmirx_handle_t pHandle,
                                sthdmirx_clk_selection_t eClkSel,
                                sthdmirx_clk_type_t eClktype, BOOL bIsPhaseInv);

  /* video functions*/
  void sthdmirx_video_pipe_init(const hdmirx_handle_t Handle);
  void sthdmirx_video_subsampler_setup(const hdmirx_handle_t Handle,
                                       sthdmirx_sub_sampler_mode_t HdmiSubsamplerMode);
  U8 sthdmirx_video_HW_color_depth_get(const hdmirx_handle_t Handle);
  BOOL sthdmirx_video_HW_AVmute_status_get(const hdmirx_handle_t Handle);
  stm_hdmirx_pixel_repeat_factor_t
  sthdmirx_video_HW_pixel_repeatfactor_get(const hdmirx_handle_t Handle);
  stm_hdmirx_color_format_t sthdmirx_video_HW_color_space_get(const
      hdmirx_handle_t Handle);
  void sthdmirx_video_enable_raw_sync(const hdmirx_route_handle_t *pInpHandle);
  void sthdmirx_video_enable_regen_sync(const hdmirx_route_handle_t *pInpHandle);

  /* HDCP Functions*/
  void sthdmirx_CORE_HDCP_init(const hdmirx_handle_t Handle);
  void sthdmirx_CORE_HDCP_enable(const hdmirx_handle_t Handle,
                                 BOOL B_Enable);
  void sthdmirx_CORE_set_HDCP_TWSaddrs(const hdmirx_handle_t Handle,
                                       U8 Addrs);

  BOOL sthdmirx_CORE_HDCP_is_encryption_enabled(const hdmirx_handle_t Handle);
  stm_hdmirx_route_hdcp_status_t sthdmirx_CORE_HDCP_status_get(const
      hdmirx_route_handle_t *pInpHandle);
  BOOL sthdmirx_CORE_HDCP_Authentication_Detection(hdmirx_route_handle_t *const pInpHandle);
  BOOL sthdmirx_CORE_HDCP_is_repeater_enabled(const hdmirx_handle_t Handle);
  BOOL sthdmirx_CORE_HDCP_is_repeater_ready(const hdmirx_handle_t Handle);
  void sthdmirx_CORE_HDCP_set_repeater_mode_enable(const hdmirx_handle_t Handle, BOOL B_Enable);
  void sthdmirx_CORE_HDCP_set_repeater_status_ready(const hdmirx_handle_t Handle, BOOL B_Ready);
  void sthdmirx_CORE_HDCP_set_repeater_downstream_status(const hdmirx_handle_t Handle,
                                                          stm_hdmirx_hdcp_downstream_status_t *status);
  void sthdmirx_CORE_HDCP_set_downstream_ksv_list(const hdmirx_handle_t Handle,
                                                  U8 *ksv_list,
                                                  U32 size);


  /*Audio Functions*/
  void sthdmirx_reset_audio_config_prop(sthdmirx_audio_Mngr_ctrl_t *AudMngr);
  void sthdmirx_audio_reset(hdmirx_handle_t const Handle);
  void sthdmirx_audio_initialize(const hdmirx_handle_t Handle);
  void sthdmirx_CORE_ISR_SDP_audio_client(
    hdmirx_route_handle_t *const pInpHdl);
  BOOL sthdmirx_CORE_initialize_clks(const hdmirx_handle_t pHandle);
  void sthdmirx_audio_soft_mute(const hdmirx_handle_t Handle,
                                BOOL B_Mute);
  BOOL sthdmirx_audio_channel_status_get(const hdmirx_handle_t Handle,
                                         U8 *pBuffer);
  sthdmirx_audio_mngr_state_t sthdmirx_audio_manager(hdmirx_handle_t Handle);

  /*I2S Tx Functions*/
#ifdef STHDMIRX_I2S_TX_IP
  void sthdmirx_I2S_init(const hdmirx_handle_t Handle,
                         sthdmirx_I2S_init_params_t *pI2SInit);
#endif

#ifdef __cplusplus
}
#endif
#endif				/*end of __HDMIRX_CORE_EXPORT_H__ */
