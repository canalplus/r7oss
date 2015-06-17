/***********************************************************************
 *
 * File: include/stm_hdmi_output.h
 * Copyright (c) 2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_HDMI_OUTPUT_H
#define _STM_HDMI_OUTPUT_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_hdmi_output.h
 *  \brief type definitions for HDMI output specific controls and info frames
 */

/*! \enum    stm_avi_vic_selection_e
 *  \brief   Control argument to configure the HDMI AVI Infoframe CEA mode (VIC)
 *           selection
 *  \ctrlarg OUTPUT_CTRL_AVI_VIC_SELECT
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_avi_vic_selection_e
{
  STM_AVI_VIC_FOLLOW_PICTURE_ASPECT_RATIO, /*!< Choose the CEA mode based on the configured picture aspect ratio */
  STM_AVI_VIC_4_3,  /*!< Force the 4:3 mode value  */
  STM_AVI_VIC_16_9, /*!< Force the 16:9 mode value */
  STM_AVI_VIC_64_27 /*!< Force the 64:27 mode value */
} stm_avi_vic_selection_t;


/*! \enum    stm_hdmi_audio_output_type_e
 *  \brief   Control argument to configure the HDMI audio data island type
 *  \ctrlarg OUTPUT_CTRL_HDMI_AUDIO_OUT_SELECT
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_hdmi_audio_output_type_e
{
  STM_HDMI_AUDIO_TYPE_NORMAL = 0, /*!< LPCM or IEC61937 compressed audio */
  STM_HDMI_AUDIO_TYPE_ONEBIT,     /*!< 1-bit audio (SACD)                */
  STM_HDMI_AUDIO_TYPE_DST,        /*!< Compressed DSD audio streams      */
  STM_HDMI_AUDIO_TYPE_DST_DOUBLE, /*!< Double Rate DSD audio streams     */
  STM_HDMI_AUDIO_TYPE_HBR,        /*!< High bit rate compressed audio    */
} stm_hdmi_audio_output_type_t;


/*! \enum    stm_vcdb_quantization_e
 *  \brief   What quantization support is reported in a HDMI Sink's EDID
 *  \ctrlarg OUTPUT_CTRL_VCDB_QUANTIZATION_SUPPORT
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_vcdb_quantization_e
{
  STM_VCDB_QUANTIZATION_UNSUPPORTED = 0, /*!< */
  STM_VCDB_QUANTIZATION_RGB  = 1,        /*!< */
  STM_VCDB_QUANTIZATION_YCC  = 2,        /*!< */
  STM_VCDB_QUANTIZATION_BOTH = 3,        /*!< */
} stm_vcdb_quantization_t;


/*! \enum    stm_avi_quantization_mode_e
 *  \brief   Control argument to configure the setting of the HDMI AVI Infoframe
 *           RGB and YUV video quantization fields
 *  \ctrlarg OUTPUT_CTRL_AVI_QUANTIZATION_MODE
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_avi_quantization_mode_e
{
  STM_AVI_QUANTIZATION_AUTO,
  STM_AVI_QUANTIZATION_DEFAULT,
  STM_AVI_QUANTIZATION_LIMITED,
  STM_AVI_QUANTIZATION_FULL
} stm_avi_quantization_mode_t;


/*! \enum    stm_avi_scan_info_e
 *  \brief   Control argument to configure the the HDMI AVI Infoframe
 *           scan bits (byte 1 S0,S1)
 *  \ctrlarg OUTPUT_CTRL_AVI_SCAN_INFO
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_avi_scan_info_e
{
  STM_AVI_NO_SCAN_DATA,
  STM_AVI_OVERSCAN,
  STM_AVI_UNDERSCAN
} stm_avi_scan_info_t;


/*! \enum    stm_avi_content_type_e
 *  \brief   Control argument to configure the the HDMI AVI Infoframe
 *           ITC and CN bits
 *  \ctrlarg OUTPUT_CTRL_AVI_CONTENT_TYPE
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_avi_content_type_e
{
  STM_AVI_IT_GRAPHICS,
  STM_AVI_IT_PHOTO,
  STM_AVI_IT_CINEMA,
  STM_AVI_IT_GAME,
  STM_AVI_IT_UNSPECIFIED
} stm_avi_content_type_t;


/*! \struct  stm_display_hdmi_phy_config_s
 *  \brief   Generic HDMI Phy board specific configuration table entry
 *  \ctrlarg OUTPUT_CTRL_HDMI_PHY_CONF_TABLE
 *  \caps    OUTPUT_CAPS_HDMI
 *
 * A pointer to an array of these structures is passed to a TMDS (HDMI) output
 * via the control interface to provide board and SoC specific
 * configurations of the HDMI PHY. Each entry in the array specifies a hardware
 * specific configuration for a given TMDS clock frequency range. The array
 * should be terminated with an entry that has all fields set to zero.
 */
typedef struct stm_display_hdmi_phy_config_s
{
  uint32_t min_tmds_freq; /*!< Lower bound of TMDS clock frequency this entry applies to */
  uint32_t max_tmds_freq; /*!< Upper bound of TMDS clock frequency this entry applies to */
  uint32_t config[4];     /*!< SoC specific register configuration                       */
} stm_display_hdmi_phy_config_t;


/*! \struct stm_hdmi_info_frame_s
    \brief The layout of a HDMI infoframe packet
*/
typedef struct stm_hdmi_info_frame_s
{
  uint8_t type;
  uint8_t version;
  uint8_t length;
  uint8_t data[28];
} stm_hdmi_info_frame_t;


/*! \struct stm_hdmi_isrc_data_s
    \brief An ISRC data packet, which can span across two HDMI infoframes
*/
typedef struct stm_hdmi_isrc_data_s
{
  stm_hdmi_info_frame_t isrc1;
  stm_hdmi_info_frame_t isrc2;
} stm_hdmi_isrc_data_t;


#if defined(__cplusplus)
}
#endif

#endif /* _STM_HDMI_OUTPUT_H */
