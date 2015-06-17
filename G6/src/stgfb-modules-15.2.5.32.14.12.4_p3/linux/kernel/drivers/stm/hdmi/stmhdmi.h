/***********************************************************************
This file is part of HDMI module

COPYRIGHT (C) 2007-2012 STMicroelectronics - All Rights Reserved

License type: Proprietary

ST makes no warranty express or implied including but not limited to, any
warranty of
    (i)  merchantability or fitness for a particular purpose and/or
    (ii) requirements, for a particular purpose in relation to the LICENSED
         MATERIALS, which is provided “AS IS”, WITH ALL FAULTS. ST does not
         represent or warrant that the LICENSED MATERIALS provided here
         under is free of infringement of any third party patents,
         copyrights, trade secrets or other intellectual property rights.
         ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE
         EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW

This file was created by STMicroelectronics on 2007
and modified by adel.chaouch@st.com on 2013
***************************************************************************/

#ifndef _STMHDMI_H
#define _STMHDMI_H

#if !defined(__KERNEL__)
#include <sys/time.h>
#endif

/*
 * Standard EDID defines
 */
#define STM_EDID_MANUFACTURER        0x08
#define STM_EDID_PROD_ID             0x0A
#define STM_EDID_SERIAL_NR           0x0C
#define STM_EDID_PRODUCTION_WK       0x10
#define STM_EDID_PRODUCTION_YR       0x11
#define STM_EDID_VERSION             0x12
#define STM_EDID_REVISION            0x13
#define STM_EDID_MAX_HSIZE           0x15
#define STM_EDID_MAX_VSIZE           0x16
#define STM_EDID_FEATURES            0x18
#define STM_EDID_ESTABLISHED_TIMING1 0x23
#define STM_EDID_ESTABLISHED_TIMING2 0x24
#define STM_EDID_MANUFACTURER_TIMING 0x25
#define STM_EDID_STD_TIMING_START    0x26
#define STM_EDID_DEFAULT_TIMING      0x36
#define STM_EDID_INFO_1              0x48
#define STM_EDID_INFO_2              0x5A
#define STM_EDID_INFO_3              0x6C

#define STM_EDID_MD_RANGES           0xFD
#define STM_EDID_MD_TYPE_NAME        0xFC

#define STM_EDID_EXTENSION           0x7E

#define STM_EDID_VGA_TIMING_CODE     0x20

#define STM_EDID_FEATURES_GTF        (1<<0)
#define STM_EDID_FEATURES_PT         (1<<1)
#define STM_EDID_FEATURES_SRGB       (1<<2)
#define STM_EDID_FEATURES_RGB        (1<<3)

#define STM_EDID_DTD_SIZE            18

/*
 * CEA Extension defines
 */
#define STM_EXT_TYPE              0
#define STM_EXT_TYPE_CEA          0x2
#define STM_EXT_TYPE_EDID_20      0x20
#define STM_EXT_TYPE_COLOR_INFO_0 0x30
#define STM_EXT_TYPE_DI           0x40
#define STM_EXT_TYPE_LS           0x50
#define STM_EXT_TYPE_MI           0x60
#define STM_EXT_TYPE_BLOCK_MAP    0xF0
#define STM_EXT_TYPE_MANUFACTURER 0xFF
#define STM_EXT_VERSION           1
#define STM_EXT_TIMING_OFFSET     2
#define STM_EXT_FORMATS           3
#define STM_EXT_DATA_BLOCK        4

#define STM_EXT_FORMATS_YCBCR_422 (1<<4)
#define STM_EXT_FORMATS_YCBCR_444 (1<<5)
#define STM_EXT_FORMATS_AUDIO     (1<<6)
#define STM_EXT_FORMATS_UNDERSCAN (1<<7)

#define STM_CEA_BLOCK_TAG_SHIFT   5
#define STM_CEA_BLOCK_TAG_MASK    (0x7<<STM_CEA_BLOCK_TAG_SHIFT)
#define STM_CEA_BLOCK_LEN_MASK    0x1F

#define STM_CEA_BLOCK_TAG_AUDIO   1
#define STM_CEA_BLOCK_TAG_VIDEO   2
#define STM_CEA_BLOCK_TAG_VENDOR  3
#define STM_CEA_BLOCK_TAG_SPEAKER 4
#define STM_CEA_BLOCK_TAG_EXT     7

#define STM_CEA_BLOCK_AUDIO_FORMAT_MASK   0x78
#define STM_CEA_BLOCK_AUDIO_FORMAT_SHIFT  3
#define STM_CEA_BLOCK_AUDIO_CHANNELS_MASK 0x7

#define STM_CEA_BLOCK_EXT_TAG_VCDB            0x0
#define STM_CEA_BLOCK_EXT_TAG_COLORIMETRY     0x5
#define STM_CEA_BLOCK_EXT_TAG_YCBCR_420_VDB   0xE
#define STM_CEA_BLOCK_EXT_TAG_YCBCR_420_CMDB  0xF /* Capability Map Data Block */

/* VSDB byte 8 block content flags */
#define STM_HDMI_VSDB_VIDEO_PRESENT      (1L<<5)
#define STM_HDMI_VSDB_I_LATENCY_PRESENT  (1L<<6)
#define STM_HDMI_VSDB_LATENCY_PRESENT    (1L<<7)

/* VSDB video features flags */
#define STM_HDMI_VSDB_MULTI_STRUCTURE_ALL      (1L<<5)
#define STM_HDMI_VSDB_MULTI_STRUCTURE_AND_MASK (2L<<5)
#define STM_HDMI_VSDB_MULTI_MASK               (3L<<5)
#define STM_HDMI_VSDB_3D_PRESENT               (1L<<7)
#define STM_HDMI_VSDB_3D_FLAGS_MASK            (7L<<5)

#define STM_3D_DETAIL_ALL           (0x0)
#define STM_3D_DETAIL_H_SUBSAMPLING (0x1)
#define STM_3D_DETAIL_All_QUINCUNX  (0x6)

#define HDMI_EVENT_DISPLAY_PRIVATE_START (0x08000000)
#define HDMI_EVENT_DISPLAY_CONNECTED     (HDMI_EVENT_DISPLAY_PRIVATE_START + 0x1)
#define HDMI_EVENT_DISPLAY_DISCONNECTED  (HDMI_EVENT_DISPLAY_PRIVATE_START + 0x2)

/*
 * The following structure contents passed using STMHDMIIO_SET_SPD_DATA will
 * change the SPD InfoFrame contents, which are transmitted automatically every
 * half second when the HDMI output is active.
 *
 * Setting the identifier to the reserved value 0xFF will disable the
 * transmission of SPD InfoFrames.
 */
struct stmhdmiio_spd
{
  unsigned char vendor_name[8];
  unsigned char product_name[16];
  unsigned char identifier;  /* As specified in CEA861 SPD InfoFrame byte 25 */
};


/*
 * The following structure contents passed using STMHDMIIO_SET_AUDIO_DATA will
 * be placed directly into the next audio InfoFrame when audio is sent over the
 * HDMI interface.
 *
 * Those bits mandated to be zero in the HDMI specification will be masked out.
 *
 * The default is to send all zeros, to match either 2ch L-PCM or
 * IEC60958 encoded data streams. If other audio sources are transmitted then
 * this information should be set correctly.
 *
 */
struct stmhdmiio_audio
{
  unsigned char channel_count;    /* CEA861 Audio InfoFrame byte 1 (bits 0-2) */
  unsigned char sample_frequency; /* CEA861 Audio InfoFrame byte 2 (bits 2-4) */
  unsigned char speaker_mapping;  /* CEA861 Audio InfoFrame byte 4            */
  unsigned char downmix_info;     /* CEA861 Audio InfoFrame byte 5 (bits 3-7) */
};


/*
 *  The following allows HDMI data island packets of various types to be
 *  sent using the STMHDMIIO_SEND_DATA_PACKET interface. The possible types
 *  supported by this are:
 *
 *  - ACP data packets
 *  - Colour gamut data packets (Profile 0 only)
 *  - Vendor specific info frames
 *  - NTSC VBI info frames
 *
 *  If the timestamp field is {0,0} then the packet will be sent as soon as
 *  possible. Otherwise timestamp must be an absolute time in the
 *  timebase of CLOCK_MONOTONIC, i.e. offset from the time returned by
 *  clock_gettime(CLOCK_MONOTONIC,...), so that the packet will not be sent
 *  until that time has passed. Packets of the same type are queued and
 *  processed in strict order, in the same way as V4L2 buffer queues. At most
 *  one packet is processed each VSync while the HDMI output is active.
 *
 *  Data packets may be queued when the HDMI output is disabled, in anticipation
 *  of it being restarted.
 *
 *  The data packet queues are NOT flushed for hotplug, display mode changes
 *  or explicit HDMI disable/enable sequences. While the HDMI output is disabled
 *  the queue will not be processed, possibly resulting in data eventually being
 *  presented late when the output is eventually re-started. A queue may be
 *  explicitly flushed using STMHDMIIO_FLUSH_DATA_PACKET_QUEUE to prevent this.
 *
 *
 *  ACP packets are only accepted for transmission if the connected TV's EDID
 *  has the supports_AI bit set. If the ACP type is set to 0 (Generic) then
 *  ACP packet transmission will be stopped. Otherwise the ACP packet data
 *  will be sent periodically to meet the HDMI specification of repeats at
 *  least every 300ms. The current ACP packet data is persistent across hotplug,
 *  display mode changes or explicit HDMI disable/enable sequences. Flushing
 *  the ACP data queue will also invalidate the current data.
 *
 *  Profile 0 colour gamut data packets are repeated once per video field until
 *  a new packet is set. The current color gamut packet data is persistent
 *  across hotplug, display mode changes or explicit HDMI disable/enable
 *  sequences. After setting an initial valid colour gamut data packet, to
 *  return to a standard colourspace a new data packet with the No_Crnt_GBD bit
 *  set (HB2 bit 7) should be sent. See the HDMI specification for details.
 *  Note however that flushing the color gamut data queue will also invalidate
 *  the color gamut data and stop the transmission of these packets.
 *
 *  Vendor and NTSC VBI info frames are sent once, based on the given timestamp.
 *
 *  The IOCTL can produce the following error conditions:
 *
 *  ETIME      - an attempt to queue with a timestamp in the past or
 *               earlier than the last packet still pending transmission with
 *               a non-zero timestamp.
 *  EPERM      - ACP data was being set and the connected TV does not indicate
 *               supports_AI in its EDID.
 *  EAGAIN     - the data packet system was temporarily unavailable, try again.
 *  EBUSY      - no more data packets of the given type can be queued yet.
 *  EOPNOTSUPP - the packet type is not supported on this implementation.
 */
struct stmhdmiio_data_packet
{
  struct timeval timestamp;
  unsigned char  type;
  unsigned char  version; /* Also ACP type or Gamut HB1 */
  unsigned char  length;  /* Also Gamut HB2             */
  unsigned char  data[28];
};


/*
 *  The following configures HDMI ISRC data packets to be sent using the
 *  STMHDMIIO_SET_ISRC_DATA interface.
 *
 *  If the timestamp field is {0,0} then the packet will be sent as soon as
 *  possible. Otherwise timestamp should be an absolute time in the
 *  timebase of CLOCK_MONOTONIC, i.e. offset from the time returned by
 *  clock_gettime(CLOCK_MONOTONIC,...), for which the packet should not be sent
 *  until that time has passed. Packets are queued and processed in strict
 *  order, in the same way as V4L2 buffer queues.
 *
 *  Once enabled, packets containing the provided data are sent continuously
 *  until replaced by new data or transmission is stopped. Setting the ISRC
 *  status to ISRC_STATUS_DISABLE will cause ISRC transmission to stop at the
 *  time indicated by the provided timestamp. The queued and current ISRC data
 *  is persistent across hotplug, display mode changes or explicit
 *  HDMI disable/enable sequences. While the HDMI output is disabled the
 *  queue will not be processed, possibly resulting in data eventually being
 *  presented late when the output is re-started. The queue may be explicitly
 *  flushed using STMHDMIIO_FLUSH_DATA_PACKET_QUEUE, which will also invalidate
 *  the current data and stop the transmission of ISRC packets.
 *
 *  ISRC data is only accepted for transmission if the connected TV's EDID
 *  has the supports_AI bit set.
 *
 *  The IOCTL can produce the following error conditions:
 *
 *  ETIME      - an attempt to queue with a timestamp in the past or earlier
 *               than the last packet still pending transmission with a non-zero
 *               timestamp.
 *  EPERM      - the connected TV does not indicate supports_AI.
 *  EAGAIN     - the data packet system was temporarily unavailable, try again.
 *  EBUSY      - no more data packets can be queued (queue is full).
 *  EOPNOTSUPP - ISRC packets are not supported on this implementation.
 */
typedef enum
{
  ISRC_STATUS_DISABLE      = 0,
  ISRC_STATUS_STARTING     = 1,
  ISRC_STATUS_INTERMEDIATE = 2,
  ISRC_STATUS_ENDING       = 4
} stmhdmiio_isrc_status_t;


struct stmhdmiio_isrc_data
{
  struct timeval          timestamp;
  stmhdmiio_isrc_status_t status;
  unsigned char           upc_ean_isrc[32];
};


/*
 * Or'd flags to STMHDMIIO_FLUSH_DATA_PACKET_QUEUE
 */
#define STMHDMIIO_FLUSH_ACP_QUEUE    0x00000001
#define STMHDMIIO_FLUSH_ISRC_QUEUE   0x00000002
#define STMHDMIIO_FLUSH_GAMUT_QUEUE  0x00000004
#define STMHDMIIO_FLUSH_VENDOR_QUEUE 0x00000008
#define STMHDMIIO_FLUSH_NTSC_QUEUE   0x00000010
#define STMHDMIIO_FLUSH_ALL          0x0000001f


typedef enum {
  STMHDMIIO_SCAN_UNKNOWN,
  STMHDMIIO_SCAN_OVERSCANNED,
  STMHDMIIO_SCAN_UNDERSCANNED
} stmhdmiio_overscan_mode_t;


typedef enum {
  STMHDMIIO_IT_GRAPHICS,
  STMHDMIIO_IT_PHOTO,
  STMHDMIIO_IT_CINEMA,
  STMHDMIIO_IT_GAME,
  STMHDMIIO_CE_CONTENT,
} stmhdmiio_content_type_t;


typedef enum {
  STMHDMIIO_EDID_STRICT_MODE_HANDLING,     /* Do not enable HDMI if display mode        */
                                           /* not indicated in EDID (default)           */
  STMHDMIIO_EDID_NON_STRICT_MODE_HANDLING, /* Always output display mode, ignoring EDID */
} stmhdmiio_edid_mode_t;


/*
 * The driver supports three hotplug behaviours.
 *
 * (1) STMHDMIIO_HPD_ALWAYS_STOP_AND_RESTART - the default, a hotplug de-assert
 * will always cause the HDMI output to stop. The next hotplug assert will
 * attempt to restart the output (after reading the display's EDID) if it
 * has not been explicitly disabled. This mode is appropriate for a basic
 * STLinux system showing display demos based on standard Linux applications.
 *
 * (2) STMHDMIIO_HPD_DISABLE_OUTPUT - a hotplug de-assert will always cause the
 * HDMI output to stop and will additionally force the output into a "disabled"
 * state. The HDMI output must be explicitly re-enabled by the application
 * when it determines it is correct to do so. This is appropriate for a product
 * which wants to prevent any unsupported signals (video or audio) from reaching
 * the connected display; it allows the application to co-ordinate changes to
 * the rest of the system (such as video playback) before re-enabling the
 * output.
 *
 * (3) STMHDMIIO_HPD_STOP_IF_NECESSARY - a hotplug de-assert will not
 * immediately cause the HDMI output to stop. Instead the HDMI output will
 * continue until either a hotplug assert happens, or 1.5 seconds has
 * elapsed. If the time elapses the HDMI output will be stopped and disabled
 * as in (2). If hotplug is asserted again inside the time limit then the
 * display's new EDID is read and if the current display mode is no longer
 * supported the HDMI output is then stopped and disabled, otherwise the output
 * continues. Note that it is out of scope for the display driver to check any
 * changes to the audio capabilities of the system, the application needs to do
 * this and change the audio configuration if necessary. This is appropriate to
 * a media centre type product that wants to avoid disruption to audio only
 * playback when connected to an AVReceiver and HDMI repeater when a downstream
 * display device connected to the repeater is switched on/off.
 */
typedef enum {
  STMHDMIIO_HPD_STOP_AND_RESTART = 0,
  STMHDMIIO_HPD_DISABLE_OUTPUT,
  STMHDMIIO_HPD_STOP_IF_NECESSARY,
} stmhdmiio_hpd_mode_t;


/*
 * The STMHDMIIO_SET_CEA_MODE_SELECTION ioctl changes which CEA mode number
 * is sent in the AVI InfoFrame for display modes that have both 4:3 and 16:9
 * variants. Note that this has no effect on the mode number sent for 16:9 only
 * HD modes.
 *
 * The default behaviour is to use the aspect ratio determined by reading the
 * display's EDID. If the EDID indicates no aspect ratio then 16:9 is used.
 *
 * The other options are to use the picture aspect ratio sent in the PICAR
 * field in the AVI InfoFrame, or to explicitly force 4:3 or 16:9. If
 * using the picture aspect ratio and the current picture (WSS) configuration
 * indicates that this is unknown then 16:9 is used.
 */
typedef enum {
  STMHDMIIO_CEA_MODE_FOLLOW_PICTURE_ASPECT_RATIO,
  STMHDMIIO_CEA_MODE_4_3,
  STMHDMIIO_CEA_MODE_16_9,
  STMHDMIIO_CEA_MODE_64_27,
  STMHDMIIO_CEA_MODE_FROM_EDID_ASPECT_RATIO
} stmhdmiio_cea_mode_selection_t;


/*
 * The STMHDMIIO_SET_QUANTIZATION_MODE ioctl changes the RGB or YCC quantization
 * bits set in the AVI infoframe when the sink device reports support for them.
 *
 * The auto mode will pick the most appropriate value based on the configuration
 * of the HDMI output.
 *
 */
typedef enum {
  STMHDMIIO_QUANTIZATION_AUTO,
  STMHDMIIO_QUANTIZATION_DEFAULT,
  STMHDMIIO_QUANTIZATION_LIMITED,
  STMHDMIIO_QUANTIZATION_FULL
} stmhdmiio_quantization_mode_t;

struct hdmi_event_subscription {
  unsigned int        type;        /* one of HDMI_EVENT_DISPLAY_CONNECTED or HDMI_EVENT_DISPLAY_DISCONNECTED */
  unsigned int        id;          /* not used */
  unsigned int        flags;       /* not used  */
  unsigned int        reserved[5]; /* not used */
};

struct hdmi_event {
  unsigned int        type; /* one of HDMI_EVENT_DISPLAY_CONNECTED or HDMI_EVENT_DISPLAY_DISCONNECTED */
  union {
  unsigned char        data[64]; /* one of stmhdmi_connection_mode_t */
  } u;
  unsigned int        pending;
  unsigned int        sequence;
  struct timespec     timestamp;
  unsigned int        id; /*not used */
  unsigned int        reserved[8]; /* kept to ensure binary compatibilty with V4L2 */
};

typedef enum {
  HDMI_SINK_IS_DISCONNECTED = 0,
  HDMI_SINK_IS_CONNECTED    = 1
} stmhdmi_connection_state_t;

typedef enum {
  HDMI_SINK_IS_HDMI          = (0<<1),
  HDMI_SINK_IS_DVI           = (1<<1),
  HDMI_SINK_IS_SAFEMODE_HDMI = (1<<2),
  HDMI_SINK_IS_SAFEMODE_DVI  = (1<<3)
} stmhdmi_connection_mode_t;

#define STMHDMIIO_AUDIO_SOURCE_2CH_I2S (0)
#define STMHDMIIO_AUDIO_SOURCE_PCM     STMHDMIIO_AUDIO_SOURCE_2CH_I2S
#define STMHDMIIO_AUDIO_SOURCE_SPDIF   (1)
#define STMHDMIIO_AUDIO_SOURCE_8CH_I2S (2)
#define STMHDMIIO_AUDIO_SOURCE_6CH_I2S (3)
#define STMHDMIIO_AUDIO_SOURCE_4CH_I2S (4)
#define STMHDMIIO_AUDIO_SOURCE_NONE    (0xffffffff)

#define STMHDMIIO_AUDIO_TYPE_NORMAL     (0) /* LPCM or IEC61937 compressed audio */
#define STMHDMIIO_AUDIO_TYPE_ONEBIT     (1) /* 1-bit audio (SACD)                */
#define STMHDMIIO_AUDIO_TYPE_DST        (2) /* Compressed DSD audio streams      */
#define STMHDMIIO_AUDIO_TYPE_DST_DOUBLE (3) /* Double Rate DSD audio streams     */
#define STMHDMIIO_AUDIO_TYPE_HBR        (4) /* High bit rate compressed audio    */

#define STMHDMIIO_SAFE_MODE_DVI         (0)
#define STMHDMIIO_SAFE_MODE_HDMI        (1)

#define STMHDMIIO_SET_SPD_DATA                 _IOW  ('H', 0x1, struct stmhdmiio_spd)
#define STMHDMIIO_SET_AUDIO_DATA               _IOW  ('H', 0x2, struct stmhdmiio_audio)
#define STMHDMIIO_SEND_DATA_PACKET             _IOW  ('H', 0x3, struct stmhdmiio_data_packet)
#define STMHDMIIO_SET_ISRC_DATA                _IOW  ('H', 0x4, struct stmhdmiio_isrc_data)
#define STMHDMIIO_SET_AVMUTE                   _IO   ('H', 0x5)
#define STMHDMIIO_SET_AUDIO_SOURCE             _IO   ('H', 0x6)
#define STMHDMIIO_SET_AUDIO_TYPE               _IO   ('H', 0x8)
#define STMHDMIIO_SET_OVERSCAN_MODE            _IO   ('H', 0x9)
#define STMHDMIIO_SET_CONTENT_TYPE             _IO   ('H', 0xa)
#define STMHDMIIO_SET_EDID_MODE_HANDLING       _IO   ('H', 0xb)
#define STMHDMIIO_SET_HOTPLUG_MODE             _IO   ('H', 0xc)
#define STMHDMIIO_SET_CEA_MODE_SELECTION       _IO   ('H', 0xd)
#define STMHDMIIO_FLUSH_DATA_PACKET_QUEUE      _IO   ('H', 0xe)
#define STMHDMIIO_SET_SAFE_MODE_PROTOCOL       _IO   ('H', 0xf)
#define STMHDMIIO_SET_QUANTIZATION_MODE        _IO   ('H', 0x10)
#define STMHDMIIO_GET_OUTPUT_ID                _IOR  ('H', 0x11, __u32)
#define STMHDMIIO_SET_DISABLED                 _IO   ('H', 0x12)
#define STMHDMIIO_GET_DISABLED                 _IOR  ('H', 0x12, __u32)
#define STMHDMIIO_FORCE_RESTART                _IO   ('H', 0x13)
#define STMHDMIIO_SET_VIDEO_FORMAT             _IO   ('H', 0x14)
#define STMHDMIIO_SET_FORCE_OUTPUT             _IO   ('H', 0x15)
#define STMHDMIIO_SET_FORCED_RGB_VALUE         _IO   ('H', 0x16)
#define STMHDMIIO_SET_PIXEL_REPETITION         _IO   ('H', 0x17)
#define STMHDMIIO_SET_EXTENDED_COLOR           _IO   ('H', 0x18)
#define STMHDMIIO_DQEVENT                      _IOWR ('H', 0x19, struct hdmi_event)
#define STMHDMIIO_SUBSCRIBE_EVENT              _IOW  ('H', 0x1a, struct hdmi_event_subscription)
#define STMHDMIIO_UNSUBSCRIBE_EVENT            _IOW  ('H', 0x1b, struct hdmi_event_subscription)
#define STMHDMIIO_GET_DISPLAY_CONNECTION_STATE _IOR  ('H', 0x1c, unsigned long)

#endif /* _STMHDMI_H */
