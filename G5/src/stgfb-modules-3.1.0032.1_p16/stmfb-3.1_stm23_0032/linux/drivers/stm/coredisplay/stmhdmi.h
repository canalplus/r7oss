/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/stmhdmi.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMHDMI_H
#define _STMHDMI_H

#if !defined(__KERNEL__)
#include <sys/time.h>
#endif

/*
 * The following structure contents passed using STMHDMIIO_SET_SPD_DATA will
 * change the SPD InfoFrame contents, which are transmitted automatically every
 * half second when the HDMI output is active.
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
 *  NOTE: Currently this driver does not support xvYcc or the transmission of
 *  gamut metadata on current SoCs. Full support for HDMI1.3a deepcolor and
 *  xvYcc extended colour spaces will be supported on future devices.
 *
 *  Vendor and NTSC VBI info frames are sent once, based on the given timestamp.
 *
 *  The IOCTL can produce the following error conditions:
 *
 *  EINVAL     - the operation failed due to invalid data such as an unknown
 *               type or an attempt to queue with a timestamp in the past or
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
 *  EINVAL     - the operation failed due to invalid data or an attempt to
 *               queue with a timestamp in the past or earlier than the last
 *               packet still pending transmission with a non-zero timestamp.
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
  STMHDMIIO_CEA_MODE_FROM_EDID_ASPECT_RATIO
} stmhdmiio_cea_mode_selection_t;


#define STMHDMIIO_AUDIO_SOURCE_2CH_I2S (0)
#define STMHDMIIO_AUDIO_SOURCE_PCM     STMHDMIIO_AUDIO_SOURCE_2CH_I2S
#define STMHDMIIO_AUDIO_SOURCE_SPDIF   (1)
#define STMHDMIIO_AUDIO_SOURCE_8CH_I2S (2)
#define STMHDMIIO_AUDIO_SOURCE_NONE    (0xffffffff)

#define STMHDMIIO_AUDIO_TYPE_NORMAL     (0) /* LPCM or IEC61937 compressed audio */
#define STMHDMIIO_AUDIO_TYPE_ONEBIT     (1) /* 1-bit audio (SACD)                */
#define STMHDMIIO_AUDIO_TYPE_DST        (2) /* Compressed DSD audio streams      */
#define STMHDMIIO_AUDIO_TYPE_DST_DOUBLE (3) /* Double Rate DSD audio streams     */
#define STMHDMIIO_AUDIO_TYPE_HBR        (4) /* High bit rate compressed audio    */

#define STMHDMIIO_SAFE_MODE_DVI         (0)
#define STMHDMIIO_SAFE_MODE_HDMI        (1)

#define STMHDMIIO_SET_SPD_DATA            _IOW ('H', 0x1, struct stmhdmiio_spd)
#define STMHDMIIO_SET_AUDIO_DATA          _IOW ('H', 0x2, struct stmhdmiio_audio)
#define STMHDMIIO_SEND_DATA_PACKET        _IOW ('H', 0x3, struct stmhdmiio_data_packet)
#define STMHDMIIO_SET_ISRC_DATA           _IOW ('H', 0x4, struct stmhdmiio_isrc_data)
#define STMHDMIIO_SET_AVMUTE              _IO  ('H', 0x5)
#define STMHDMIIO_SET_AUDIO_SOURCE        _IO  ('H', 0x6)
#define STMHDMIIO_SET_AUDIO_TYPE          _IO  ('H', 0x8)
#define STMHDMIIO_SET_OVERSCAN_MODE       _IO  ('H', 0x9)
#define STMHDMIIO_SET_CONTENT_TYPE        _IO  ('H', 0xa)
#define STMHDMIIO_SET_EDID_MODE_HANDLING  _IO  ('H', 0xb)
#define STMHDMIIO_SET_HOTPLUG_MODE        _IO  ('H', 0xc)
#define STMHDMIIO_SET_CEA_MODE_SELECTION  _IO  ('H', 0xd)
#define STMHDMIIO_FLUSH_DATA_PACKET_QUEUE _IO  ('H', 0xe)
#define STMHDMIIO_SET_SAFE_MODE_PROTOCOL  _IO  ('H', 0xf)

#endif /* _STMHDMI_H */
