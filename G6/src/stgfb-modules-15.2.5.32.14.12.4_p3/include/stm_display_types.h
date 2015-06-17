/***********************************************************************
 *
 * File: include/stm_display_types.h
 * Copyright (c) 2000, 2004, 2005, 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STM_DISPLAY_TYPES_H
#define STM_DISPLAY_TYPES_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_display_types.h
 *  \brief Types and macros used throughout coredisplay
 */


/*
 * Common public API types
 */
__extension__ typedef long long stm_time64_t;


/*! \struct stm_rational_s
 *  \brief A simple representation of a rational number, used for aspect ratios.
 */
typedef struct stm_rational_s
{
  int32_t numerator;
  int32_t denominator;
} stm_rational_t;


/*! \struct stm_rect_s
 *  \brief A rectangle, position and size
 */
typedef struct stm_rect_s
{
  int32_t  x;
  int32_t  y;
  uint32_t width;
  uint32_t height;
} stm_rect_t;


/*! \enum    stm_display_flip_ctrl_e
 *  \brief   Specify if an image should be flipped around the horizontal
 *           and/or vertical axis.
 *  \ctrlarg OUTPUT_CTRL_PANEL_FLIP, PLANE_CTRL_FLIP
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control(),
 *           ::stm_display_plane_set_control(), ::stm_display_plane_get_control()
 */
typedef enum stm_display_flip_ctrl_e
{
  STM_H_FLIP = (1L<<0), /*!< Flip image horizontally */
  STM_V_FLIP = (1L<<1)  /*!< Flip image vertically   */
} stm_display_flip_ctrl_t;


typedef enum stm_display_3d_format_e
{
  FORMAT_3D_NONE,
  FORMAT_3D_FRAME_SEQ,
  FORMAT_3D_STACKED_HALF,
  FORMAT_3D_STACKED_FRAME,
  FORMAT_3D_SBS_HALF,
  FORMAT_3D_SBS_FULL,
  FORMAT_3D_FIELD_ALTERNATE,
  FORMAT_3D_PICTURE_INTERLEAVE,
  FORMAT_3D_L_D,
  FORMAT_3D_L_D_G_GMINUSD,
} stm_display_3d_format_t;


typedef enum stm_display_3d_frame_seq_format_e
{
  FORMAT_3D_FRAME_SEQ_LEFT_FRAME,
  FORMAT_3D_FRAME_SEQ_RIGHT_FRAME,
  FORMAT_3D_FRAME_SEQ_FLAG_HW,
} stm_display_3d_frame_seq_format_t;


typedef struct stm_display_3d_stacked_frame_format_s
{
  uint32_t                                      vactive_space1;
  uint32_t                                      vactive_space2;
  bool                                          is_left_right_format;
} stm_display_3d_stacked_frame_format_t;


typedef enum stm_display_3d_sbs_sampling_mode_e
{
  FORMAT_3D_SBS_NO_SUB_SAMPLING,
  FORMAT_3D_SBS_HALF_SAMPLING_HORZ_OLOR,
  FORMAT_3D_SBS_HALF_SAMPLING_HORZ_OLER,
  FORMAT_3D_SBS_HALF_SAMPLING_HORZ_ELOR,
  FORMAT_3D_SBS_HALF_SAMPLING_HORZ_ELER,
  FORMAT_3D_SBS_HALF_SAMPLING_QUIN_OLOR,
  FORMAT_3D_SBS_HALF_SAMPLING_QUIN_OLER,
  FORMAT_3D_SBS_HALF_SAMPLING_QUIN_ELOR,
  FORMAT_3D_SBS_HALF_SAMPLING_QUIN_ELER,
} stm_display_3d_sbs_sampling_mode_t;


typedef struct stm_display_3d_sbs_format_s
{
  stm_display_3d_sbs_sampling_mode_t            sbs_sampling_mode;
  bool                                          is_left_right_format;
} stm_display_3d_sbs_format_t;


typedef struct stm_display_3d_field_alt_format_s
{
  uint32_t                                      vactive;
  uint32_t                                      vblank;
  bool                                          is_left_right_format;
} stm_display_3d_field_alt_format_t;


typedef enum stm_display_3d_picture_interleave_format_e
{
  FORMAT_3D_LINE_INTERLEAVE_LEFT_FIRST_POS,
  FORMAT_3D_LINE_INTERLEAVE_RIGHT_FIRST_POS,
  FORMAT_3D_CHECKERBOX_LEFT_FIRST_POS,
  FORMAT_3D_CHECKERBOX_RIGHT_FIRST_POS,
  FORMAT_3D_COLUMN_INTERLEAVE_LEFT_FIRST_POS,
  FORMAT_3D_COLUMN_INTERLEAVE_LEFT_RIGHT_POS,
} stm_display_3d_picture_interleave_format_t;


typedef struct stm_display_3d_L_D_format_s
{
  uint32_t                                      vact_video;
  uint32_t                                      vact_space;
} stm_display_3d_L_D_format_t;


typedef struct stm_display_3d_L_D_G_G_format_s
{
  uint32_t                                      vact_video;
  uint32_t                                      vact_space;
} stm_display_3d_L_D_G_G_format_t;


typedef union stm_display_3d_video_format_extra_param_s
{
  int32_t                                       not_applicable;
  stm_display_3d_frame_seq_format_t             frame_seq;
  stm_display_3d_stacked_frame_format_t         frame_packed;
  stm_display_3d_sbs_format_t                   sbs;
  stm_display_3d_field_alt_format_t             field_alt;
  stm_display_3d_picture_interleave_format_t    picture_interleave;
  stm_display_3d_L_D_format_t                   l_d;
  stm_display_3d_L_D_G_G_format_t               l_d__g_g;
} stm_display_3d_video_format_extra_param_t;


typedef struct stm_display_3d_video_s
{
    stm_display_3d_format_t                     formats;
    stm_display_3d_video_format_extra_param_t   parameters;
} stm_display_3d_video_t;


/*! \enum  stm_display_output_capabilities_e
 *  \brief Output capability flags.
 *  \apis  ::stm_display_output_get_capabilities()
 */
typedef enum stm_display_output_capabilities_e
{
  OUTPUT_CAPS_DISPLAY_TIMING_MASTER   = (1L<<0),  /*!< Output can set and change the display timing, it owns a timing generator           */
  OUTPUT_CAPS_EXTERNAL_SYNC_SIGNALS   = (1L<<1),  /*!< Output can generate Hsync and Vsync signals which can be
                                                       routed to external pins on the SoC                                                 */
  OUTPUT_CAPS_PLANE_MIXER             = (1L<<2),  /*!< Output can have planes connected and mix them to produce video signal data         */
  OUTPUT_CAPS_MIXER_BACKGROUND        = (1L<<3),  /*!< Output has a controllable mixer background color,
                                                       visible when no planes cover a part of the video active area.                      */
  OUTPUT_CAPS_FORCED_COLOR            = (1L<<4),  /*!< Output can be forced to produce a single configurable
                                                       color value instead of the normal content                                          */
  OUTPUT_CAPS_RGB_QUANTIZATION_CHANGE = (1L<<5),  /*!< Output can change the quantization of RGB video data
                                                       between CEA and VESA value ranges.                                                 */
  OUTPUT_CAPS_HW_TELETEXT             = (1L<<6),  /*!< Output supports hardware based Teletext insertion, with
                                                       Teletext data provided via the metadata interface.                                 */

  OUTPUT_CAPS_SD_ANALOG               = (1L<<7),  /*!< Output supports SD analog video output standards                                   */
  OUTPUT_CAPS_ED_ANALOG               = (1L<<8),  /*!< Output supports ED analog video output standards                                   */
  OUTPUT_CAPS_HD_ANALOG               = (1L<<9),  /*!< Output supports HD analog video output standards                                   */

  OUTPUT_CAPS_HDMI                    = (1L<<10), /*!< Output supports HDMI (and DVI) digital video output                                */
  OUTPUT_CAPS_HDMI_DEEPCOLOR          = (1L<<11), /*!< Output supports HDMI deepcolor packing                                             */

  OUTPUT_CAPS_LVDS                    = (1L<<12), /*!< Output supports LVDS digital video output                                          */
  OUTPUT_CAPS_DISPLAYPORT             = (1L<<13), /*!< Output supports DisplayPort digital video output                                   */

  OUTPUT_CAPS_DVO_656                 = (1L<<14), /*!< Output supports ITU-R BT.656 8bit SD and ED digital video output                   */
  OUTPUT_CAPS_DVO_16BIT               = (1L<<15), /*!< Output supports 16bit digital video output                                         */
  OUTPUT_CAPS_DVO_24BIT               = (1L<<16), /*!< Output supports 24bit digital video output                                         */

  OUTPUT_CAPS_422_CHROMA_FILTER       = (1L<<17), /*!< Output has a selectable YUV 4:4:4 to 4:2:2 chroma decimation filter                */

  OUTPUT_CAPS_CVBS_YC_EXCLUSIVE       = (1L<<18), /*!< Output supports CVBS+Y/C with component DACs disabled (or they do not exist)       */
  OUTPUT_CAPS_YPbPr_EXCLUSIVE         = (1L<<19), /*!< Output supports ED/HD component YPbPr, or SD component with CVBS & Y/C DACs
                                                       disabled (or they do not exist)                                                    */
  OUTPUT_CAPS_RGB_EXCLUSIVE           = (1L<<20), /*!< Output supports VGA/D-Sub, RGB on component output with no embedded syncs          */
  OUTPUT_CAPS_SD_RGB_CVBS_YC          = (1L<<21), /*!< Output supports SCART, RGB+CVBS with Y/C optionally disabled                       */
  OUTPUT_CAPS_SD_YPbPr_CVBS_YC        = (1L<<22), /*!< Output supports Component YPbPr and composite outputs simultaneously               */

  OUTPUT_CAPS_LEDBLU                  = (1L<<23), /*!< Output supports LED backlight control for TV panels                                */

  OUTPUT_CAPS_HW_CC                   = (1L<<24), /*!< Output supports hardware based Closed caption insertion, with
                                                       Closed caption data provided via the metadata interface.                           */

  OUTPUT_CAPS_UHD_DIGITAL             = (1L<<25), /*!< Output supports Ultra High Definition HDMI digital video output                    */
} stm_display_output_capabilities_t;


/*! \enum stm_plane_capabilities_t
 *
 *  \brief This type is a group of symbolic names for display output
 *         capabilities, which may be OR'd together in a 32 bit wide variable
 *         to indicate a set of supported capabilities returned from
 *         stm_display_output_get_capabilities().
 */
typedef enum stm_plane_capabilities_e
{
  /* A plane capability that indicates if the  queried plane is a video plane. */
  PLANE_CAPS_VIDEO              =(1L<<0),
  /* A plane capability that indicates if the queried plane is a graphics plane. */
  PLANE_CAPS_GRAPHICS           =(1L<<1),
  /* A plane capability that indicates if the queried plane is a vbi plane. */
  PLANE_CAPS_VBI                =(1L<<2),
  /* A plane capability that indicates if the queried plane has the best video
     quality compared to the other planes. */
  PLANE_CAPS_VIDEO_BEST_QUALITY =(1L<<3),
  /* A plane capability that indicates if the queried plane has the best graphics
     quality compared to the other planes. */
  PLANE_CAPS_GRAPHICS_BEST_QUALITY = PLANE_CAPS_VIDEO_BEST_QUALITY,
  /* A plane capability that indicates if the queried plane is the primary or
     secondary plane to a given display. */
  PLANE_CAPS_PRIMARY_PLANE      =(1L<<4),
  /* A plane capability that indicates if the queried plane would connect to a
     primary output. */
  PLANE_CAPS_PRIMARY_OUTPUT     =(1L<<5),
  /* A plane capability that indicates if the queried plane would connect to a
     secondary output. */
  PLANE_CAPS_SECONDARY_OUTPUT   =(1L<<6),

} stm_plane_capabilities_t;


 /*! \enum stm_display_status_t
 *
 *  \brief Global status of display objects
 */
typedef enum stm_display_status_e
{
  /* Source status */
  STM_STATUS_SOURCE_CONNECTED             =(1L<<0),  /*!< Source connected to at least one plane                */
  /* Queue status */
  STM_STATUS_QUEUE_LOCKED                 =(1L<<1),  /*!< Queue is locked for exclusive use                     */
  STM_STATUS_QUEUE_FULL                   =(1L<<2),  /*!< Queue is full                                         */
  /* Plane status */
  STM_STATUS_PLANE_HAS_BUF_TO_DISPLAY     =(1L<<3),  /*!< Plane actively being processed, buffers enqueued      */
  STM_STATUS_PLANE_VISIBLE                =(1L<<4),  /*!< Plane visible                                         */
  STM_STATUS_PLANE_PAUSED                 =(1L<<5),  /*!< Plane paused                                          */
  STM_STATUS_PLANE_CONNECTED_TO_SOURCE    =(1L<<6),  /*!< Plane connected to a source                           */
  STM_STATUS_PLANE_CONNECTED_TO_OUTPUT    =(1L<<7),  /*!< Plane connected to an output                          */

  /* Buffer specific status, returned only in a buffer completed callback */
  STM_STATUS_BUF_HW_ERROR                 =(1L<<29), /*!< A hardware error occured while displaying the picture */
  STM_STATUS_BUF_DISPLAYED                =(1L<<30), /*!< The picture has been displayed                        */
} stm_display_status_t;


#if defined(__cplusplus)
}
#endif

#endif /* STM_DISPLAY_TYPES_H */
