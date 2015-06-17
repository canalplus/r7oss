/***********************************************************************
 *
 * File: include/stm_display_buffer.h
 * Copyright (c) 2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DISPLAY_BUFFER_H
#define _STM_DISPLAY_BUFFER_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_display_buffer.h
 *  \brief Definitions and structures describing image buffers (graphics and video)
 */

/*! \enum stm_pixel_format_t
 *  \brief Buffer pixel formats
 */
typedef enum stm_pixel_format_e
{
  SURF_NULL_PAD,   /*!< */
  SURF_RGB565 ,    /*!< */
  SURF_RGB888 ,    /*!< */
  SURF_ARGB8565,   /*!< */
  SURF_ARGB8888,   /*!< */
  SURF_ARGB1555,   /*!< */
  SURF_ARGB4444,   /*!< */
  SURF_CRYCB888,   /*!< */
  SURF_YCBCR422R,  /*!< */
  SURF_YCBCR422MB, /*!< */
  SURF_YCBCR420MB, /*!< */
  SURF_ACRYCB8888, /*!< */
  SURF_CLUT1,      /*!< */
  SURF_CLUT2,      /*!< */
  SURF_CLUT4,      /*!< */
  SURF_CLUT8,      /*!< */
  SURF_ACLUT44,    /*!< */
  SURF_ACLUT88,    /*!< */
  SURF_A1,         /*!< */
  SURF_A8,         /*!< */
  SURF_BGRA8888,   /*!< Bigendian ARGB                                     */
  SURF_YUYV,       /*!< 422R with luma and chroma byteswapped              */
  SURF_YUV420,     /*!< Planar YUV with 1/2 horizontal and vertical chroma */
                   /*   in three separate buffers Y,Cb then Cr             */
  SURF_YVU420,     /*!< Planar YUV with 1/2 horizontal and vertical chroma */
                   /*   in three separate buffers Y,Cr then Cb             */
  SURF_YUV422P,    /*!< Planar YUV with 1/2 horizontal chroma              */
                   /*   in three separate buffers Y,Cb then Cr             */
  SURF_RLD_BD,     /*!< RLE Decoding controlled by setting source format   */
  SURF_RLD_H2,     /*!< */
  SURF_RLD_H8,     /*!< */
  SURF_CLUT8_ARGB4444_ENTRIES, /*!< For cursor plane support             */
  SURF_YCbCr420R2B, /* fourCC: NV12: 12 bit YCbCr (8 bit Y plane followed by
                                                   one 16 bit quarter size
                                                   Cb|Cr [7:0|7:0] plane) */
  SURF_YCbCr422R2B, /* fourCC: NV16: 16 bit YCbCr (8 bit Y plane followed by
                                                   one 16 bit half width
                                                   Cb|Cr [7:0|7:0] plane) */
  SURF_CRYCB101010,   /* YCbCr 444 RSB 10bpp */
  SURF_YCbCr422R2B10, /* YCbCr 422 RDB 10bpp */
  SURF_END,
  SURF_COUNT = SURF_END /*!< Must remain last */
} stm_pixel_format_t;


/*!
 * \brief ::stm_color_key_config_s flags field values
 */
enum StmColorKeyConfigFlags {
  SCKCF_NONE   = 0x00000000, /*!< */

  SCKCF_ENABLE = 0x00000001, /*!< */
  SCKCF_FORMAT = 0x00000002, /*!< */
  SCKCF_R_INFO = 0x00000004, /*!< */
  SCKCF_G_INFO = 0x00000008, /*!< */
  SCKCF_B_INFO = 0x00000010, /*!< */
  SCKCF_MINVAL = 0x00000020, /*!< */
  SCKCF_MAXVAL = 0x00000040, /*!< */

  SCKCF_ALL    = 0x0000007f  /*!< */
};


/*!
 * \brief ::stm_color_key_config_s key colour formats
 */
enum StmColorKeyConfigValueFormat {
  SCKCVF_RGB   = 0x00,  /*!< */
  SCKCVF_CrYCb = 0x01   /*!< */
};

/*!
 * \brief ::stm_color_key_config_s component colour match modes
 */
enum StmColorKeyColorComponentMode {
  SCKCCM_DISABLED = 0x00,                    /*!< Do not match component                      */
  SCKCCM_ENABLED  = 0x01,                    /*!< Inclusive match component against key range */
  SCKCCM_INVERSE  = (0x02 | SCKCCM_ENABLED)  /*!< Exclusive match component against key range */
};


/*! \struct stm_color_key_config_s
 *  \brief  Describe a source colour key match configuration
 *  \apis   ::stm_display_source_queue_buffer(),
 *          ::stm_display_plane_set_control(),
 *          ::stm_display_plane_get_control()
 *
 *  \ctrlarg PLANE_CTRL_COLOR_KEY
 */
typedef struct stm_color_key_config_s {
  enum StmColorKeyConfigFlags flags; /*!< Or'd values that specify which of the following
                                          fields are present */

  uint8_t enable;                            /*!< */
  enum StmColorKeyConfigValueFormat format;  /*!< Key value format, RGB or CrYCb */

  enum StmColorKeyColorComponentMode r_info; /*!< Red/Cr channel match mode   */
  enum StmColorKeyColorComponentMode g_info; /*!< Green/Y channel match mode  */
  enum StmColorKeyColorComponentMode b_info; /*!< Blue/Cb channel match mode  */

  uint32_t minval; /*!< Low key values  */
  uint32_t maxval; /*!< High key values */
} stm_color_key_config_t;

typedef enum stm_display_source_flags_e
{
    STM_BUFFER_SRC_INTERLACED                   = (1L<<0),      /*!< SRC content is interleaved interlaced fields.             */
    STM_BUFFER_SRC_BOTTOM_FIELD_FIRST           = (1L<<1),      /*!< Display the bottom field of interlaced content            */
                                                                /* before the top field,the default is top field first.        */
    STM_BUFFER_SRC_REPEAT_FIRST_FIELD           = (1L<<2),      /*!< Repeat the first field on interlaced content, i.e. the    */
                                                                /* buffer is on display for at least 3 fields.                 */
    STM_BUFFER_SRC_TOP_FIELD_ONLY               = (1L<<3),      /*!< Display the top field only.                               */
    STM_BUFFER_SRC_BOTTOM_FIELD_ONLY            = (1L<<4),      /*!< Display the bottom field only.                            */
    STM_BUFFER_SRC_INTERPOLATE_FIELDS           = (1L<<5),      /*!< Interpolate fields of interlaced content for trick mode   */
                                                                /* display of a buffer for multiple display frames.            */
    STM_BUFFER_SRC_COLORSPACE_709               = (1L<<6),      /*!< Src content is in ITU-R BT.709 colorspace,                */
                                                                /* the default is ITU-R BT.601. This is only relevent to YUV   */
                                                                /* SRC pixel formats.                                          */
    STM_BUFFER_SRC_PREMULTIPLIED_ALPHA          = (1L<<7),      /*!< The source colour has been premultiplied by the per pixel */
                                                                /* alpha channel, only relevent with ARGB formats.             */
    STM_BUFFER_SRC_LIMITED_RANGE_ALPHA          = (1L<<8),      /*!< The alpha channel is in the range 0-128 not 0-255         */
    STM_BUFFER_SRC_PAN_AND_SCAN                 = (1L<<9),      /*!< Pan&Scan info are available in PanAndScanRect.            */
    STM_BUFFER_SRC_CONST_ALPHA                  = (1L<<10),     /*!< Enable constant alpha.                                    */
    STM_BUFFER_SRC_REPEATED_PICTURE             = (1L<<11),     /*!< The picture queued is the same as the one previously queued.*/
    STM_BUFFER_SRC_TEMPORAL_DISCONTINUITY       = (1L<<12),     /*!< One or more pictures has been skipped during decode.        */
    STM_BUFFER_SRC_VC1_POSTPROCESS_LUMA         = (1L<<13),     /*!< Enable VC1 luma post processing, type set in ulPostProcessLumaType */
    STM_BUFFER_SRC_VC1_POSTPROCESS_CHROMA       = (1L<<14),     /*!< Enable VC1 chroma post processing, type set in ulPostProcessChromaType */
    STM_BUFFER_SRC_CHARACTERISTIC_DISCONTINUITY = (1L<<15),     /*!< A source characteristic has changed (Size, FrameRate, Aspect Ratio, etc.) */
    STM_BUFFER_SRC_FORCE_DISPLAY                = (1L<<16),     /*!< Force this picture to be displayed ASAP */
    STM_BUFFER_SRC_RESPECT_PICTURE_POLARITY     = (1L<<17),     /*!< When set, check that the picture polarity matches the output VSync polarity */

}stm_display_source_flags_t;

/*! \struct stm_buffer_src_picture_desc_t
 *  \brief  source image memory storage specific part of a buffer descriptor
 */
typedef struct stm_buffer_src_picture_desc_s
{
 uint32_t               width;                      /*!< The maximum number of horizontal pixels  */
                                                    /*   in this buffer.                          */
 uint32_t               height;                     /*!< The maximum number of video lines in     */
                                                    /*   this buffer, height*pitch is             */
                                                    /*   the number of bytes in the buffer for    */
                                                    /*   raster formats and the size of the luma  */
                                                    /*   part of the buffer for MB and Planar     */
                                                    /*   formats.                                 */
 uint32_t               pitch;                      /*!< Length of a raster or luma line in bytes */
 uint32_t               video_buffer_addr ;         /*!< Hardware bus address of video buffer     */
 uint32_t               chroma_buffer_offset ;      /*!< Start of chroma in non interleaved YUV   */
 uint32_t               video_buffer_size ;         /*!< Size of video buffer                     */
 uint32_t               pixel_depth ;               /*!< Bits per pixel                           */
 stm_pixel_format_t     color_fmt ;                 /*!< Pixel format                             */
} stm_buffer_src_picture_desc_t;


/*! \struct stm_decimation_factor_t
 *  \brief  list of possible decimation factors
 */
typedef enum stm_decimation_factor_e
{
  STM_NO_DECIMATION,
  STM_DECIMATION_BY_TWO,
  STM_DECIMATION_BY_FOUR,
  STM_DECIMATION_BY_EIGHT
} stm_decimation_factor_t;


/*! \struct stm_buffer_src_s
 *  \brief  source image specific part of a buffer descriptor
 */
typedef struct stm_buffer_src_s
{
  stm_buffer_src_picture_desc_t     primary_picture;            /*!< Main reconstruction of a decoded picture       */
  stm_buffer_src_picture_desc_t     secondary_picture;          /*!< Secondary reconstruction of a decoded picture  */

  stm_rect_t                        visible_area;               /*!< Area to display                                */
  stm_rect_t                        pan_and_scan_rect;          /*!< Rectangle containing Pan&Scan cropping         */
                                                                /*   information : NOT USED YET                     */
  stm_rect_t                        Rect;                       /*!< Source rectangle to display : WILL BE REMOVED  */
                                                                /*   when IO Windows will be managed by CoreDisplay */

  stm_rational_t                    pixel_aspect_ratio;         /*!< Source pixel aspect ratio                      */
  uint32_t                          linear_center_percentage;   /*!< Region to preserve source aspect ratio         */
                                                                /*   when converting to 16:9 display output.        */
  stm_rational_t                    src_frame_rate;             /*!< Source Frame Rate                              */

  stm_decimation_factor_t           horizontal_decimation_factor; /*!< Horizontal decimation factor                 */
  stm_decimation_factor_t           vertical_decimation_factor;   /*!< Vertical decimation factor                   */

  uint32_t                          clut_bus_address;           /*!< The hardware bus address of the CLUT           */
                                                                /*   associated with this image. This should        */
                                                                /*   contain 256 entries in packed 32bit            */
                                                                /*   ARGB colour format, but where the alpha        */
                                                                /*   range is limited to 0-128 (note not 127!)      */

  uint32_t                          post_process_luma_type;     /*!< VC1 range mapping R, or range reduction define */
  uint32_t                          post_process_chroma_type;   /*!< VC1 range mapping R, or range reduction define */

#define STM_BUFFER_SRC_VC1_RANGE_MAP_MIN   0x0
#define STM_BUFFER_SRC_VC1_RANGE_MAP_MAX   0x7
#define STM_BUFFER_SRC_VC1_RANGE_REDUCTION 0x7
  uint32_t                          flags;                      /*!< Or'd STM_BUFFER_SRC_* values */

  stm_display_3d_video_t            config_3D;                   /*!< Description of the 3D video parameters         */

  /* Those fields will be moved... */
  uint32_t                          ulConstAlpha;               /*!< Range 0-255                                    */
  stm_color_key_config_t            ColorKey;                   /*!< color key configuration                        */
} stm_buffer_src_t;


/*
 * WARNING: The destination structure is likely to be removed from the
 *          buffer description in the future.
 */
#define STM_BUFFER_DST_COLOR_KEY               (1L<<0) /*!< Enable dest color keying                            */
#define STM_BUFFER_DST_COLOR_KEY_INV           (1L<<1) /*!< Dst colour key matches if outside of specifed range */
#define STM_BUFFER_DST_RESCALE_TO_VIDEO_RANGE  (1L<<4) /*!< Rescale the full digital RGB range to the typical   */
                                                       /*   video output range 16-235 (8bit values)             */
#define STM_BUFFER_DST_OVERWITE                (1L<<5) /*!< Do not blend the src with the destination           */
                                                       /*   overwrite the destination instead. This is only,    */
                                                       /*   an option on outputs employing a blitter composition*/
                                                       /*   based mixer.                                        */
#define STM_BUFFER_DST_CONVERT_TO_16_9_DISPLAY (1L<<7) /*!< Use a non-linear zoom to convert from source pixel  */
                                                       /*   aspect ratio to the destination pixel aspect ratio  */
                                                       /*   assuming it is a 16:9 display.                      */
/*! \struct stm_buffer_dst_s
 *  \brief  destination specific part of a buffer descriptor
 */
typedef struct stm_buffer_dst_s
{
  uint32_t   ulColorKeyMin;   /*!< RGB format                      */
  uint32_t   ulColorKeyMax;   /*!< RGB format                      */
  uint32_t   ulFlags;         /*!< Or'd STM_BUFFER_DST_* values  . */
} stm_buffer_dst_t;


/*! \struct stm_buffer_presentation_stats_s
 *  \brief  information about a buffer descirptor's presentation passed
 *          in the completed callback.
 */
typedef struct stm_buffer_presentation_stats_s
{
  stm_time64_t vsyncTime;  /*!< Timestamp when the buffer was first read by the hardware        */
  uint32_t     status;     /*!< Status flags at this timestamp: Or'd STM_STATUS_* values.       */
} stm_buffer_presentation_stats_t;

typedef enum stm_buffer_presentation_flags_e
{
    STM_BUFFER_PRESENTATION_PERSISTENT         = (1L<<0), /*!< Keep re-reading the buffer if there are no more in the queue              */
    STM_BUFFER_PRESENTATION_GRAPHICS           = (1L<<1), /*!< Use historic graphics plane semantics when using interlaced timings.      */
                                                          /*   The buffer will start being read for the first time on the next top field */
                                                          /*   closest to the required presentation time.                                */
    STM_BUFFER_PRESENTATION_DIRECT_BUFFER_ADDR = (1L<<2)  /*!< Use a direct base address for the buffer data, ignoring ulVideoBufferAddr */
}stm_buffer_presentation_flags_t;

typedef enum stm_buffer_output_change_flags_e
{
    OUTPUT_DISPLAY_MODE_CHANGE                 = (1L<<0), /*!< The Display mode has changed for at least one output                      */
    OUTPUT_CONNECTION_CHANGE                   = (1L<<1)  /*!< Some connection between sources-planes-outputs have changed               */
}stm_buffer_output_change_flags_t;

// Invalid ID used in case of problem when filling the structure stm_display_latency_params_t or statistics
#define STM_INVALID_OUTPUT_ID      (0xFFFF)
#define STM_INVALID_PLANE_ID      (0xFFFF)
#define STM_INVALID_SOURCE_ID      (0xFFFF)

typedef struct stm_display_latency_params_s
{
  uint32_t output_id;               /*!< Id of the output connected to the display plane */
  uint32_t plane_id;                /*!< Id of the plane connected to the display source */
  uint32_t output_latency_in_us;    /*!< Latency of presenting frame to the output (i.e. from the time of pixel stream conversion to real output) */
  bool     output_change;           /*!< Boolean indicating a display mode change for this output */
} stm_display_latency_params_t;

/*! \struct stm_buffer_presentation_s
 *  \brief  Presentation specific part of the buffer descriptor
 */
typedef struct stm_buffer_presentation_s
{
  stm_time64_t presentation_time; /*!< Required presentation time, relative to the CPU wall clock     */
  stm_time64_t PTS;               /*!< Required presentation time, relative to stream itself          */
  int32_t      nfields;           /*!< The number of fields/frames the buffer will be read for        */
  uint32_t     ulFlags;           /*!< Or'd STM_BUFFER_PRESENTATION_* values.                         */

  void       (*display_callback)  (void*                         user_data,
                                   stm_time64_t                  vsync_time,
                                   uint16_t                      output_change,             /* Bit field indicating any change of the output
                                                                                               (Or'd stm_buffer_output_change_flags_t values) */
                                   uint16_t                      nb_planes,                 /* Number of plane(s) the buffer will be sent to (This is also the
                                                                                               number of elements in the "display_latency_params" array) */
                                   stm_display_latency_params_t *display_latency_params);   /* Array of latency params for each plane */
                                  /*!< Callback on first field/frame buffer is displayed              */

  void       (*completed_callback)(void*, const stm_buffer_presentation_stats_t *);
                                 /*!< Callback when buffer is removed from display                   */

  void        *puser_data;        /*!< User defined value passed into both callback routines          */
  stm_buffer_presentation_stats_t stats;
                                 /*!< For private app/implementation use                             */
} stm_buffer_presentation_t;


/*! \struct stm_display_buffer_s
 *  \brief  Buffer description
 *  \apis   ::stm_display_source_queue_buffer()
 */
typedef struct stm_display_buffer_s
{
  stm_buffer_src_t          src;  /*!< */
  stm_buffer_dst_t          dst;  /*!< */
  stm_buffer_presentation_t info; /*!< */
} stm_display_buffer_t;

#if defined(__cplusplus)
}
#endif

#endif /* _STM_DISPLAY_BUFFER_H */
