/***********************************************************************
 *
 * File: include/stmmetadata.h
 * Copyright (c) 2008 STMicroelectronics Limited.
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

typedef enum
{
  STM_WSS_OFF = 0,
  STM_WSS_4_3,
  STM_WSS_16_9,
  STM_WSS_14_9,
  STM_WSS_GT_16_9
} stm_wss_t;


typedef enum
{
  STM_LETTERBOX_NONE = 0,
  STM_LETTERBOX_CENTER,
  STM_LETTERBOX_TOP,
  STM_SHOOT_AND_PROTECT_14_9,
  STM_SHOOT_AND_PROTECT_4_3
} stm_letterbox_t;


/*
 * Note this order must match the HDMI AVI frame scaling values
 */
typedef enum
{
  STM_RESCALE_NONE = 0,
  STM_RESCALE_HORIZONTAL,
  STM_RESCALE_VERTICAL,
  STM_RESCALE_BOTH
} stm_picture_rescale_t;


#define STM_PIC_INFO_PICTURE_ASPECT  (1L<<0)
#define STM_PIC_INFO_VIDEO_ASPECT    (1L<<1)
#define STM_PIC_INFO_LETTERBOX       (1L<<2)
#define STM_PIC_INFO_RESCALE         (1L<<3)
#define STM_PIC_INFO_BAR_DATA        (1L<<4)
#define STM_PIC_INFO_BAR_DATA_ENABLE (1L<<5)


typedef struct
{
  ULONG                 flags;
  stm_wss_t             pictureAspect;
  stm_wss_t             videoAspect;
  stm_letterbox_t       letterboxStyle;
  stm_picture_rescale_t pictureRescale;
  USHORT                barEnable;
  USHORT                topEndLine;
  USHORT                bottomStartLine;
  USHORT                leftEndPixel;
  USHORT                rightStartPixel;
} stm_picture_format_info_t;


/*
 * A teletext line, including clock run in and framing/start code and padded
 * to be 4byte aligned.
 */
typedef struct
{
  UCHAR lineData[48];
} stm_ebu_teletext_line_t;


typedef struct
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
   * size of the lineData buffer _must_ be 48*18 bytes.
   */
  ULONG ulValidLineMask;
  stm_ebu_teletext_line_t *lines;
} stm_ebu_teletext_t;


typedef enum
{
  STM_METADATA_TYPE_RESERVED = 0,
  STM_METADATA_TYPE_PICTURE_INFO,       /* stm_picture_format_info_t */
  STM_METADATA_TYPE_ISRC_DATA,          /* stm_hdmi_isrc_data_t      */
  STM_METADATA_TYPE_ACP_DATA,           /* stm_hdmi_info_frame_t     */
  STM_METADATA_TYPE_COLOR_GAMUT_DATA,   /* stm_hdmi_info_frame_t     */
  STM_METADATA_TYPE_AUDIO_IFRAME,       /* stm_hdmi_info_frame_t     */
  STM_METADATA_TYPE_VENDOR_IFRAME,      /* stm_hdmi_info_frame_t     */
  STM_METADATA_TYPE_NTSC_IFRAME,        /* stm_hdmi_info_frame_t     */
  STM_METADATA_TYPE_SPD_IFRAME,         /* stm_hdmi_info_frame_t     */
  STM_METADATA_TYPE_EBU_TELETEXT,       /* stm_ebu_teletext_t        */
} stm_meta_data_type_t;


/*
 * The following structure is the type used to queue metadata changes on display
 * outputs, which can timed to coincide with the presentation of a specific
 * video plane or set of audio samples.
 *
 * The user should allocate memory sufficient for the structure below plus the
 * actual data payload, which should begin at the address of the data field.
 *
 * The display output processing will call the optionally provided release()
 * function when it is finished with the structure. This make take place in
 * an interrupt context, so if your OS memory allocator free function cannot
 * be used here (e.g. OS21) then you will have to implement some form of
 * "garbage collection" mechanism to do the real release at some time later
 * from a thread context.
 *
 * The display output implementation uses the refCount field to determine when
 * to call the release() function, its value is reset when the structure is
 * passed to stm_display_output_queue_metadata(). Once queued the caller no
 * longer has ownership of the structure and it is an error for the caller to
 * use it again before the release() function has been called.
 *
 * The metadata structures of a particular type are processed in the order they
 * were queued, taking into account the presentationTime field.
 *
 * The presentationTime field may be zero, which indicates the metadata should
 * be processed as quickly as possible. It is an error to request a time in the
 * past, or less than or equal to the latest metadata presentationTime already
 * queued of this metadata type. It is however allowed to queue multiple
 * metadata structures with a presentationTime set to zero, these will be
 * processed one after the other.
 *
 */
typedef struct stm_meta_data_s
{
  stm_meta_data_type_t type;
  ULONG                size;            /* Total size of structure + payload */
  TIME64               presentationTime;
  ULONG                refCount;
  void                *privateData;     /* For driver use on call to release */
  void                (*release)(struct stm_meta_data_s *);

  /* Start of Payload */
  ULONG                data[1];
} stm_meta_data_t;


typedef enum
{
  STM_METADATA_RES_OK,
  STM_METADATA_RES_UNSUPPORTED_TYPE,
  STM_METADATA_RES_TIMESTAMP_IN_PAST,
  STM_METADATA_RES_INVALID_DATA,
  STM_METADATA_RES_QUEUE_BUSY,
  STM_METADATA_RES_QUEUE_UNAVAILABLE
} stm_meta_data_result_t;


static inline void stm_meta_data_addref(stm_meta_data_t *m)
{
  m->refCount++;
}


static inline void stm_meta_data_release(stm_meta_data_t *m)
{
  --m->refCount;
  if((m->refCount == 0) && m->release)
    m->release(m);
}


#if defined(__cplusplus)
}
#endif


#endif /* _STM_META_DATA_H */
