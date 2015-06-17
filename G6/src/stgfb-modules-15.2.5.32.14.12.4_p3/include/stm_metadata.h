/***********************************************************************
 *
 * File: include/stm_metadata.h
 * Copyright (c) 2008-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_META_DATA_H
#define _STM_META_DATA_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_metadata.h
 *  \brief Output metadata definitions
 *
 * This header file contains the basic definitions for the metadata interface
 * available on output objects ::stm_display_output_queue_metadata(). It also
 * defines most of the metadata payload types, which the exception of those
 * associated specifically with HDMI, which are defined in stmhdmiinfo.h
 *
 */

/*! \enum stm_wss_aspect_ratio_e
 *  \brief Widescreen signaling aspect ratios
 */
typedef enum stm_wss_aspect_ratio_e
{
  STM_WSS_ASPECT_RATIO_UNKNOWN = 0,  /*!< */
  STM_WSS_ASPECT_RATIO_4_3,          /*!< */
  STM_WSS_ASPECT_RATIO_16_9,         /*!< */
  STM_WSS_ASPECT_RATIO_14_9,         /*!< */
  STM_WSS_ASPECT_RATIO_GT_16_9       /*!< */
} stm_wss_aspect_ratio_t;


/*! \enum stm_letterbox_style_e
 *  \brief Widescreen signaling letterbox styles
 */
typedef enum stm_letterbox_style_e
{
  STM_LETTERBOX_NONE = 0,      /*!< */
  STM_LETTERBOX_CENTER,        /*!< */
  STM_LETTERBOX_TOP,           /*!< */
  STM_SHOOT_AND_PROTECT_14_9,  /*!< */
  STM_SHOOT_AND_PROTECT_4_3    /*!< */
} stm_letterbox_style_t;


/*! \enum stm_picture_rescale_e
 *  \brief Picture scaling indication
 *
 * This enum order must match the HDMI AVI frame scaling values
 */
typedef enum stm_picture_rescale_e
{
  STM_RESCALE_NONE = 0,    /*!< */
  STM_RESCALE_HORIZONTAL,  /*!< */
  STM_RESCALE_VERTICAL,    /*!< */
  STM_RESCALE_BOTH         /*!< */
} stm_picture_rescale_t;


/*! \enum    stm_teletext_subtitles_e
 *  \brief   ETS 300 294 Line 23 VBI Teletext subtitle definition.
 */
typedef enum stm_teletext_subtitles_e
{
  STM_TELETEXT_SUBTITLES_NONE,    /*!< */
  STM_TELETEXT_SUBTITLES_PRESENT, /*!< */
} stm_teletext_subtitles_t;


/*! \enum    stm_open_subtitles_e
 *  \brief   ETS 300 294 Line 23 VBI open subtitle definition.
 */
typedef enum stm_open_subtitles_e
{
  STM_OPEN_SUBTITLES_NONE = 0,              /*!< */
  STM_OPEN_SUBTITLES_INSIDE_ACTIVE_VIDEO,   /*!< */
  STM_OPEN_SUBTITLES_OUTSIDE_ACTIVE_VIDEO   /*!< */
} stm_open_subtitles_t;


/*! \enum    stm_bar_data_present_e
 *  \brief   CEA-861 AVI Infoframe Data Byte 1 bits B1-B0.
 */
typedef enum stm_bar_data_present_e
{
  STM_BARDATA_NONE,      /*!< */
  STM_BARDATA_VERTICAL,  /*!< */
  STM_BARDATA_HORIZONTAL,/*!< */
  STM_BARDATA_BOTH       /*!< */
} stm_bar_data_present_t;


/*! \enum    stm_picture_format_info_flags_e
 *  \brief   Field update flags used in ::stm_picture_format_info_s
 *  \note    The enumeration order matches the STKPI display document, but
 *           the enumeration values are arranged to maintain binary
 *           compatibility with the existing framebuffer picture configuration
 *           IOCTL interface.
 */
typedef enum stm_picture_format_info_flags_e
{
  STM_PIC_INFO_PICTURE_ASPECT     = (1L<<0), /*!< */
  STM_PIC_INFO_VIDEO_ASPECT       = (1L<<1), /*!< */
  STM_PIC_INFO_LETTERBOX_STYLE    = (1L<<2), /*!< */
  STM_PIC_INFO_TELETEXT_SUBTITLES = (1L<<6), /*!< */
  STM_PIC_INFO_OPEN_SUBTITLES     = (1L<<7), /*!< */
  STM_PIC_INFO_RESCALE            = (1L<<3), /*!< */
  STM_PIC_INFO_BAR_DATA_PRESENT   = (1L<<4), /*!< */
  STM_PIC_INFO_BAR_DATA           = (1L<<5), /*!< */
  STM_PIC_INFO_ALL                = (0xff)   /*!< */
} stm_picture_format_info_flags_t;


/*! \struct stm_picture_format_info_s
 *  \brief Picture format metadata payload
 *
 *  This structure contains information about the video being output that
 *  can be transmitted possibly simultaneously in analogue VBI and the
 *  HDMI AVI infoframe. The \a flags field indicates which values should
 *  be changed by this payload.
 */
typedef struct stm_picture_format_info_s
{
  uint32_t                 flags;                /*!< Or of stm_picture_format_info_flags_t values indicating
                                                      which fields in the structure should be used            */

  stm_wss_aspect_ratio_t   picture_aspect_ratio; /*!< Valid when STM_PIC_INFO_PICTURE_ASPECT set in flags     */
  stm_wss_aspect_ratio_t   video_aspect_ratio;   /*!< Valid when STM_PIC_INFO_VIDEO_ASPECT set in flags       */
  stm_letterbox_style_t    letterbox_style;      /*!< Valid when STM_PIC_INFO_LETTERBOX_STYLE set in flags    */
  stm_teletext_subtitles_t teletext_subtitles;   /*!< Valid when STM_PIC_INFO_TELETEXT_SUBTITLES set in flags */
  stm_open_subtitles_t     open_subtitles;       /*!< Valid when STM_PIC_INFO_OPEN_SUBTITLES set in flags     */
  stm_picture_rescale_t    picture_rescale;      /*!< Valid when STM_PIC_INFO_RESCALE set in flags            */
  stm_bar_data_present_t   bar_data_present;     /*!< Valid when STM_PIC_INFO_BAR_DATA_PRESENT set in flags   */
  uint16_t                 bar_top_end_line;     /*!< Valid when STM_PIC_INFO_BAR_DATA set in flags           */
  uint16_t                 bar_bottom_start_line;/*!< Valid when STM_PIC_INFO_BAR_DATA set in flags           */
  uint16_t                 bar_left_end_pixel;   /*!< Valid when STM_PIC_INFO_BAR_DATA set in flags           */
  uint16_t                 bar_right_start_pixel;/*!< Valid when STM_PIC_INFO_BAR_DATA set in flags           */
} stm_picture_format_info_t;


/*! \struct stm_teletext_line_s
 *  \brief A single Teletext data line
 *
 * A teletext line, including clock run in and framing/start code and padded
 * to be 4byte aligned.
 */
typedef struct stm_teletext_line_s
{
  uint8_t line_data[48]; /*!< */
} stm_teletext_line_t;


/*! \struct stm_teletext_s
 *  \brief Teletext metadata payload
 *
 * A complete set of teletext lines for one field to be queued for output
 * through the metadata interface.
 */
typedef struct stm_teletext_s
{
  /*
   * Bit mask indicating which lines in CCIR 625 line numbering (+312 for second
   * field)
   *
   * Only lines 6-23 are available, bit zero indicates the field parity for
   * this set of data lines. Teletext data on line 23 will override any WSS/CGMS
   * data normally output on this line in 625 line standards.
   *
   * The line data should be a sparse buffer, containing data starting at
   * line 6/318. The data content for inactive lines is ignored; hence the
   * size of the line_data buffer _must_ be 48*18 bytes.
   */
  uint32_t             valid_line_mask; /*!< */
  stm_teletext_line_t *lines;           /*!< */
} stm_teletext_t;


/*! \struct stm_closed_caption_s
 *  \brief Closed caption metadata payload
 */
typedef struct stm_closed_caption_s
{
  uint8_t     field;
  uint16_t    lines_field;
  uint8_t     data[2];
} stm_closed_caption_t;


/*! \enum stm_display_metadata_type_e
 *  \brief Supported metadata payload types
 *
 *  \apis ::stm_display_output_queue_metadata(),
 *        ::stm_display_output_flush_metadata()
 */
typedef enum stm_display_metadata_type_e
{
  STM_METADATA_TYPE_RESERVED = 0,       /*!< */
  STM_METADATA_TYPE_PICTURE_INFO,       /*!< Payload type ::stm_picture_format_info_s */
  STM_METADATA_TYPE_TELETEXT,           /*!< Payload type ::stm_teletext_s            */
  STM_METADATA_TYPE_CLOSED_CAPTION,     /*!< Payload type ::stm_closed_caption_s      */
  STM_METADATA_TYPE_ISRC_DATA,          /*!< Payload type ::stm_hdmi_isrc_data_s      */
  STM_METADATA_TYPE_ACP_DATA,           /*!< Payload type ::stm_hdmi_info_frame_s     */
  STM_METADATA_TYPE_COLOR_GAMUT_DATA,   /*!< Payload type ::stm_hdmi_info_frame_s     */
  STM_METADATA_TYPE_AUDIO_IFRAME,       /*!< Payload type ::stm_hdmi_info_frame_s     */
  STM_METADATA_TYPE_VENDOR_IFRAME,      /*!< Payload type ::stm_hdmi_info_frame_s     */
  STM_METADATA_TYPE_NTSC_IFRAME,        /*!< Payload type ::stm_hdmi_info_frame_s     */
  STM_METADATA_TYPE_SPD_IFRAME,         /*!< Payload type ::stm_hdmi_info_frame_s     */
  STM_METADATA_TYPE_HDMI_VSIF_3D_EXT,   /*!< Payload type ::stm_hdmi_info_frame_s     */
} stm_display_metadata_type_t;


/*! \struct stm_display_metadata_s
 *  \brief Metadata packet transmission descriptor
 *
 *  \apis ::stm_display_output_queue_metadata()
 *
 * This structure is the type used to queue metadata changes on display
 * outputs, which can timed to coincide with the presentation of a specific
 * video frame or set of audio samples.
 *
 * The user should allocate memory sufficient for the structure below plus the
 * actual data payload, which should begin at the address of the data field.
 *
 * The display output processing will call the optionally provided release()
 * function when it is finished with the structure. This may take place in
 * an interrupt context, so if your OS memory allocator free function cannot
 * be used here (e.g. OS21) then you will have to implement some form of
 * "garbage collection" mechanism to do the real release at some time later
 * from a thread context.
 *
 * The display output implementation uses the ref_count field to determine when
 * to call the release() function, its value is reset when the structure is
 * passed to ::stm_display_output_queue_metadata(). Once queued the caller no
 * longer has ownership of the structure and it is an error for the caller to
 * use it again before the release() function has been called.
 *
 * The metadata structures of a particular type are processed in the order they
 * were queued, taking into account the presentation_time field.
 *
 * The presentation_time field may be zero, which indicates the metadata should
 * be processed as quickly as possible. It is an error to request a time in the
 * past, or less than or equal to the latest metadata presentation_time already
 * queued of this metadata type. It is however allowed to queue multiple
 * metadata structures with a presentation_time set to zero, these will be
 * processed one after the other.
 *
 */
typedef struct stm_display_metadata_s
{
  stm_display_metadata_type_t type;             /*!< */
  uint32_t                    size;             /*!< Total size of structure + payload */
  stm_time64_t                presentation_time;/*!< */
  uint32_t                    ref_count;        /*!< */
  void                       *private_data;     /*!< For driver use on call to release function */
  void                      (*release)(struct stm_display_metadata_s *); /*!< Optional release function */

  uint32_t             data[1]; /*!< Start of Payload */
} stm_display_metadata_t;


/*!
    \brief Increment the reference count on a metadata structure
    \param m The metadata structure to add a reference to
 */

static inline void stm_meta_data_addref(stm_display_metadata_t *m)
{
  m->ref_count++;
}


/*!
    \brief Release a reference count to the metadata structure
    \param m The metadata structure to release
*/

static inline void stm_meta_data_release(stm_display_metadata_t *m)
{
  --m->ref_count;
  if((m->ref_count == 0) && m->release)
    m->release(m);
}


#if defined(__cplusplus)
}
#endif


#endif /* _STM_META_DATA_H */
