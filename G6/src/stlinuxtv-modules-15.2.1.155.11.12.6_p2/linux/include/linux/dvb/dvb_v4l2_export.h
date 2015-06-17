/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : dvb_v4l2_export.h

Header file for the V4L2 interface driver containing the defines
to be exported to the user level.

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef DVB_V4L2_EXPORT_H_
#define DVB_V4L2_EXPORT_H_

#include <linux/version.h>

/*
 * Video buffers captured from a LINUXDVB_* input will normally be in the
 * Y'CbCr _video_ range (either conforming to BT.709 or BT.601). They will
 * have been converted into R'G'B' colorspace by the hardware if the
 * application requested the driver to do so. That means the application will
 * get _video_ range buffers by default.
 * In addition, the hardware is able to scale the data to _full_ range
 * (identical to computer graphics). Therefore, applications which want the
 * data in _full_ (computer graphics) range will have to add this flag in the
 * VIDIOC_QBUF ioctl.
 */
#define V4L2_BUF_FLAG_FULLRANGE 0x10000

/* Plane Aspect Ratio Conversion Mode settings */
enum _V4L2_CID_STM_PLANE_ASPECT_RATIO_CONV_MODE {
	VCSASPECT_RATIO_CONV_LETTER_BOX,
	VCSASPECT_RATIO_CONV_PAN_AND_SCAN,
	VCSASPECT_RATIO_CONV_COMBINED,
	VCSASPECT_RATIO_CONV_IGNORE
};

/* Output Aspect Ratio settings */
enum _V4L2_CID_STM_OUTPUT_DISPLAY_ASPECT_RATIO {
	VCSOUTPUT_DISPLAY_ASPECT_RATIO_16_9,
	VCSOUTPUT_DISPLAY_ASPECT_RATIO_4_3,
	VCSOUTPUT_DISPLAY_ASPECT_RATIO_14_9
};

/* Plane mode settings */
enum _V4L2_CID_STM_PLANE_MODE {
	VCSPLANE_MANUAL_MODE,
	VCSPLANE_AUTO_MODE
};

/* Pixel Timing stability setting */
enum _V4L2_CID_STM_PIXEL_TIMING_STABILITY {
	VCSPIXEL_TIMING_UNSTABLE,
	VCSPIXEL_TIMING_STABLE
};

enum _V4L2_CID_STM_ENCODER_CAPTURE_ASPECT_RATIO{
	V4L2_STM_ENCODER_ASPECT_RATIO_IGNORE,
	V4L2_STM_ENCODER_ASPECT_RATIO_4_3,
	V4L2_STM_ENCODER_ASPECT_RATIO_16_9
};

/*
 * V4L2 Implemented Controls 
 *
 */
enum {
	V4L2_CID_STM_DVBV4L2_FIRST = (V4L2_CID_PRIVATE_BASE + 300),

	V4L2_deprecated_CID_STM_BLANK,
	V4L2_deprecated_CID_STM_PLANE_ASPECT_RATIO_CONV_MODE,	/* enum _V4L2_CID_STM_PLANE_ASPECT_RATIO_CONV_MODE */
	V4L2_deprecated_CID_STM_OUTPUT_DISPLAY_ASPECT_RATIO,	/* enum _V4L2_CID_STM_OUTPUT_DISPLAY_ASPECT_RATIO  */
	V4L2_deprecated_CID_STM_PLANE_INPUT_WINDOW_MODE,		/* enum _V4L2_CID_STM_PLANE_MODE  */
	V4L2_deprecated_CID_STM_PLANE_OUTPUT_WINDOW_MODE,		/* enum _V4L2_CID_STM_PLANE_MODE  */
	V4L2_deprecated_CID_STM_PLANE_PIXEL_TIMING_STABILITY,		/* enum _V4L2_CID_STM_PIXEL_TIMING_STABILITY */

	V4L2_CID_STM_AUDIO_O_FIRST,
	V4L2_CID_STM_AUDIO_O_LAST,

	V4L2_deprecated_CID_STM_ENCODE_DEI_SET_MODE,
	V4L2_deprecated_CID_STM_ENCODE_NOISE_FILTERING,

	V4L2_deprecated_CID_STM_DECODER_FRAME_SIZE,

	V4L2_deprecated_CID_STM_ENCODER_ASPECT_RATIO,

	V4L2_CID_STM_DVBV4L2_LAST
};


/* add new mbus formats for the pixelstream interfaces */
#define V4L2_MBUS_FMT_STM_RGB8_3x8		0x6001
#define V4L2_MBUS_FMT_STM_RGB10_3x10		0x6002
#define V4L2_MBUS_FMT_STM_RGB12_3x12		0x6003

#define V4L2_MBUS_FMT_STM_YUV8_3X8		0x6004
#define V4L2_MBUS_FMT_STM_YUV10_3X10		0x6005
#define V4L2_MBUS_FMT_STM_YUV12_3X12		0x6006

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
#define V4L2_MBUS_FMT_RGB888_1X24		0x7001
#endif
#define V4L2_MBUS_FMT_RGB101010_1X30		0x7002
#define V4L2_MBUS_FMT_YUV8_1X24			0x7003
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
#define V4L2_MBUS_FMT_YUV10_1X30		0x7004
#endif

/* add new format dor the newly added buffer type.
 * use it to cas the rw_data field from the union */
struct v4l2_audio_format {
	unsigned int BitsPerSample;
	unsigned int Channelcount;
	unsigned int SampleRateHz;
};

#define V4L2_CID_MPEG_STM_BASE	(V4L2_CTRL_CLASS_MPEG | 0x2000)
#define V4L2_CID_DV_STM_BASE	(V4L2_CTRL_CLASS_DV | 0x2000)
#define V4L2_CID_IMAGE_PROC_STM_BASE	(V4L2_CTRL_CLASS_IMAGE_PROC | 0x2000)


#define V4L2_CID_MPEG_STM_VIDEO_DECODER_BLANK    	V4L2_CID_MPEG_STM_BASE+0
#define V4L2_CID_MPEG_STM_VIDEO_H264_DEI_SET_MODE	V4L2_CID_MPEG_STM_BASE+1
#define V4L2_CID_MPEG_STM_VIDEO_H264_NOISE_FILTERING	V4L2_CID_MPEG_STM_BASE+2
#define V4L2_CID_MPEG_STM_FRAME_SIZE			V4L2_CID_MPEG_STM_BASE+3

#define V4L2_CID_MPEG_STM_VIDEO_ELEMENTARY_BUFFER_SIZE  V4L2_CID_MPEG_STM_BASE+4
/*
 * This control will give coded frame buffer pool maximum size for video
 * How to use these control:
 * struct v4l2_control ctrl;
 * memset (&ctrl, '\0', sizeof (ctrl));
 *  ctrl.id  = V4L2_CID_MPEG_STM_VIDEO_ELEMENTARY_BUFFER_SIZE;
 *  ioctl (v4l2fd, VIDIOC_G_CTRL, &(ctrl));
 *  the ctrl.value will contain the value of buffer size
 *  The same is applicable to all newly added control
 */

#define V4L2_CID_MPEG_STM_VIDEO_ELEMENTARY_BUFFER_USED  V4L2_CID_MPEG_STM_BASE+5
/*
 * This control will give memory used in the pool for video
 */

/* controls for video encoder */
#define V4L2_CID_MPEG_STM_VIDEO_H264_SPS_WIDTH		V4L2_CID_MPEG_STM_BASE+6
#define V4L2_CID_MPEG_STM_VIDEO_H264_SPS_HEIGHT		V4L2_CID_MPEG_STM_BASE+7
#define V4L2_CID_MPEG_STM_VIDEO_H264_ASPECT_RATIO	V4L2_CID_MPEG_STM_BASE+8


/* controls for audio encoder */
#define V4L2_CID_MPEG_STM_AUDIO_VBR_QUALITY_FACTOR 	V4L2_CID_MPEG_STM_BASE+9
#define V4L2_CID_MPEG_STM_AUDIO_PROGRAM_LEVEL 		V4L2_CID_MPEG_STM_BASE+10
#define V4L2_CID_MPEG_STM_AUDIO_SERIAL_CONTROL 		V4L2_CID_MPEG_STM_BASE+11
#define V4L2_CID_MPEG_STM_AUDIO_BITRATE_CAP 		V4L2_CID_MPEG_STM_BASE+12
#define V4L2_CID_MPEG_STM_AUDIO_BITRATE_CONTROL 	V4L2_CID_MPEG_STM_BASE+13
#define V4L2_CID_MPEG_STM_AUDIO_CHANNEL_COUNT 		V4L2_CID_MPEG_STM_BASE+14
#define V4L2_CID_MPEG_STM_AUDIO_CHANNEL_MAP 		V4L2_CID_MPEG_STM_BASE+15
#define V4L2_CID_MPEG_STM_AUDIO_CORE_FORMAT 		V4L2_CID_MPEG_STM_BASE+16
#define V4L2_CID_MPEG_STM_AUDIO_BITRATE_MODE 		V4L2_CID_MPEG_STM_BASE+17

/*
 * Common control for audio/video encoder, but needs to be applied separately to both
 */
#define V4L2_CID_MPEG_STM_ENCODE_NRT_MODE		(V4L2_CID_MPEG_STM_BASE + 18)

/*
 * Controls for Audio decoder
 */
#define V4L2_CID_MPEG_STM_AUDIO_ELEMENTARY_BUFFER_SIZE  V4L2_CID_MPEG_STM_BASE+19
/*
 * Above control will give coded frame buffer pool maximum size for audio
 */

#define V4L2_CID_MPEG_STM_AUDIO_ELEMENTARY_BUFFER_USED  V4L2_CID_MPEG_STM_BASE+20
/*
 * Above control will give memory used in the pool for audio
 */

/* This will have to be removed once the subdev in stmvout devices are exposed */
#define V4L2_CID_STM_ASPECT_RATIO_CONV_MODE 	V4L2_CID_IMAGE_PROC_STM_BASE+0
#define V4L2_CID_STM_INPUT_WINDOW_MODE		V4L2_CID_IMAGE_PROC_STM_BASE+1
#define V4L2_CID_STM_OUTPUT_WINDOW_MODE		V4L2_CID_IMAGE_PROC_STM_BASE+2

#define V4L2_CID_DV_STM_TX_ASPECT_RATIO			V4L2_CID_DV_STM_BASE+0
#define V4L2_CID_DV_STM_RX_PIXEL_TIMING_STABILITY	V4L2_CID_DV_STM_BASE+1
/* maintaining till then */




/*
 * Discontinuity flags for encoder inside struct v4l2_buffer
 * V4L2_BUF_FLAG_STM_ENCODE_TIME_DISCONTINUITY (QBUF/DQBUF)
 *	Application set this flag along with QBUF to indicate encoder that the
 *	current buffer pushed in has time discontinuity. Application while
 *	reading the buffer may get the same flag from encoder device to indicate
 *	the same.
 *
 * V4L2_BUF_FLAG_STM_ENCODE_GOP_START (DQBUF)
 * 	Application will retrieve this flag from encoder when DQBUF'ed buffer
 * 	is the new GOP
 *
 * V4L2_BUF_FLAG_STM_ENCODE_CLOSED_GOP_REQUEST (QBUF)
 *	Application will QBUF with this flag to request a closed GOP
 *
 * V4L2_BUF_FLAG_STM_ENCODE_CLOSED_GOP (DQBUF)
 * 	Application will get this flag set, when DQBUF'ed buffer is a closed GOP
 *
 * There is one more discontinuity which can fed to encoder with bytesused field
 * of v4l2_buffer set to zero (0). This is End Of Stream (EOS). This mechanism
 * remains same for QBUF/DQBUF
 */
#define V4L2_BUF_FLAG_STM_ENCODE_TIME_DISCONTINUITY	0x00010000
#define V4L2_BUF_FLAG_STM_ENCODE_GOP_START		0x00020000
#define V4L2_BUF_FLAG_STM_ENCODE_CLOSED_GOP_REQUEST	0x00040000
#define V4L2_BUF_FLAG_STM_ENCODE_CLOSED_GOP		0x00080000

/*
 * At TSMUX level there is there is no way to differentiate between the NULL
 * ES packet and EOS packet. This flag is introduced to differentiate whether
 * the empty packet is EOS or user wants to inject NULL packet
 */
#define V4L2_BUF_FLAG_STM_TSMUX_NULL_PACKET		0x00800000

#define V4L2_COLORSPACE_UNSPECIFIED 0

/* audio codec */
enum stm_v4l2_mpeg_audio_encoding {
	V4L2_MPEG_AUDIO_STM_ENCODING_PCM     = 0,
	V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_1 = 1,
	V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_2 = 2,
	V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_3 = 3,
	V4L2_MPEG_AUDIO_STM_ENCODING_AC3     = 4,
	V4L2_MPEG_AUDIO_STM_ENCODING_AAC     = 5,
	V4L2_MPEG_AUDIO_STM_ENCODING_HEAAC   = 6,
};

enum stm_v4l2_mpeg_audio_pcm_format {

	V4L2_MPEG_AUDIO_STM_PCM_FMT_32_S32LE,
	V4L2_MPEG_AUDIO_STM_PCM_FMT_32_S32BE,

	V4L2_MPEG_AUDIO_STM_PCM_FMT_24_S24LE,
	V4L2_MPEG_AUDIO_STM_PCM_FMT_24_S24BE,

	V4L2_MPEG_AUDIO_STM_PCM_FMT_16_S16LE,
	V4L2_MPEG_AUDIO_STM_PCM_FMT_16_S16BE,
	V4L2_MPEG_AUDIO_STM_PCM_FMT_16_U16BE,
	V4L2_MPEG_AUDIO_STM_PCM_FMT_16_U16LE,

	V4L2_MPEG_AUDIO_STM_PCM_FMT_8_U8,
	V4L2_MPEG_AUDIO_STM_PCM_FMT_8_S8,

	V4L2_MPEG_AUDIO_STM_PCM_FMT_8_ALAW_8,
	V4L2_MPEG_AUDIO_STM_PCM_FMT_8_ULAW_8,

};

enum stm_v4l2_encode_audio_channel_id {

    STM_V4L2_AUDIO_CHAN_L,		/* Left */
    STM_V4L2_AUDIO_CHAN_R,		/* Right */
    STM_V4L2_AUDIO_CHAN_LFE,		/* Low Frequency Effect */
    STM_V4L2_AUDIO_CHAN_C,		/* Centre */
    STM_V4L2_AUDIO_CHAN_LS,		/* Left Surround */
    STM_V4L2_AUDIO_CHAN_RS,		/* Right Surround */

    STM_V4L2_AUDIO_CHAN_LT,		/* Surround Matrixed Left */
    STM_V4L2_AUDIO_CHAN_RT,		/* Surround Matrixed Right */
    STM_V4L2_AUDIO_CHAN_LPLII,		/* DPLII Matrixed Left */
    STM_V4L2_AUDIO_CHAN_RPLII,		/* DPLII Matrixed Right */

    STM_V4L2_AUDIO_CHAN_CREAR,		/* Rear Centre */

    STM_V4L2_AUDIO_CHAN_CL,		/* Centre Left */
    STM_V4L2_AUDIO_CHAN_CR,		/* Centre Right */

    STM_V4L2_AUDIO_CHAN_LFEB,		/* Second LFE */

    STM_V4L2_AUDIO_CHAN_L_DUALMONO,	/* Dual-Mono Left */
    STM_V4L2_AUDIO_CHAN_R_DUALMONO,	/* Dual-Mono Right */

    STM_V4L2_AUDIO_CHAN_LWIDE,		/* Wide Left */
    STM_V4L2_AUDIO_CHAN_RWIDE,		/* Wide Right */

    STM_V4L2_AUDIO_CHAN_LDIRS,		/* Directional Surround left */
    STM_V4L2_AUDIO_CHAN_RDIRS,		/* Directional Surround Right */

    STM_V4L2_AUDIO_CHAN_LSIDES,		/* Side Surround Left */
    STM_V4L2_AUDIO_CHAN_RSIDES,		/* Side Surround Right */

    STM_V4L2_AUDIO_CHAN_LREARS,		/* Rear Surround Left */
    STM_V4L2_AUDIO_CHAN_RREARS,		/* Rear Surround Right */

    STM_V4L2_AUDIO_CHAN_CHIGH,		/* High Centre */
    STM_V4L2_AUDIO_CHAN_LHIGH,		/* High Left */
    STM_V4L2_AUDIO_CHAN_RHIGH,		/* High Right */
    STM_V4L2_AUDIO_CHAN_LHIGHSIDE,	/* High Side Left */
    STM_V4L2_AUDIO_CHAN_RHIGHSIDE,	/* High Side Right */
    STM_V4L2_AUDIO_CHAN_CHIGHREAR,	/* High Rear Centre */
    STM_V4L2_AUDIO_CHAN_LHIGHREAR,	/* High Rear Left */
    STM_V4L2_AUDIO_CHAN_RHIGHREAR,	/* High Rear Right */

    STM_V4L2_AUDIO_CHAN_CLOWFRONT,	/* Low Front Centre */
    STM_V4L2_AUDIO_CHAN_TOPSUR,		/* Surround Top */

    STM_V4L2_AUDIO_CHAN_DYNSTEREO_LS,	/* Dynamic Stereo Left Surround */
    STM_V4L2_AUDIO_CHAN_DYNSTEREO_RS,	/* Dynamic Stereo Right Surround */


    /* Control values  */
    /* Last channel id with a defined positioning */
    STM_V4L2_AUDIO_CHAN_LAST_NAMED = STM_V4L2_AUDIO_CHAN_DYNSTEREO_RS,

    /* Id for a channel with valid content but no defined positioning */
    STM_V4L2_AUDIO_CHAN_UNKNOWN,
    /* Last id for channel with valid content */
    STM_V4L2_AUDIO_CHAN_LAST_VALID = STM_V4L2_AUDIO_CHAN_UNKNOWN,

    STM_V4L2_AUDIO_CHAN_STUFFING = 254,

    /* All values above STM_SE_AUDIO_CHAN_STUFFING are reserved, because we will
       case to uint8_t */
    STM_V4L2_AUDIO_CHAN_RESERVED = 255
};

enum stm_v4l2_encode_emphasis_type
{
	V4L2_MPEG_AUDIO_NO_EMPHASIS,
	V4L2_MPEG_AUDIO_EMPH_50_15us,
	V4L2_MPEG_AUDIO_EMPH_CCITT_J_17
};

struct v4l2_audenc_format {
	int codec;
	int max_channels;
	int reserved;
};

#define V4L2_STM_AUDIO_MAX_CHANNELS 32
#define V4L2_STM_AUDENC_MAX_CHANNELS V4L2_STM_AUDIO_MAX_CHANNELS

struct v4l2_audio_uncompressed_metadata {
	unsigned int  sample_rate;
	int           channel_count;
	unsigned char channel[V4L2_STM_AUDIO_MAX_CHANNELS];
	unsigned int  sample_format;
	int           program_level;
	unsigned int  emphasis;
};

/* TBD(vgi): remove alias introduced to avoid API break */
#define v4l2_audenc_src_metadata v4l2_audio_uncompressed_metadata

struct v4l2_audenc_dst_metadata {
	unsigned int sample_rate;
	int           channel_count;
	unsigned char channel[V4L2_STM_AUDENC_MAX_CHANNELS];
	int drc_factor;
};

enum v4l2_audio_encode_bitrate_mode {
	V4L2_MPEG_AUDIO_BITRATE_MODE_CBR = 0,
	V4L2_MPEG_AUDIO_BITRATE_MODE_VBR = 1,
};

enum v4l2_audio_encode_serial_control {
	V4L2_MPEG_AUDIO_NO_COPYRIGHT,                /* Any further copy is authorised */
	V4L2_MPEG_AUDIO_ONE_MORE_COPY_AUTHORISED,    /* One more generation copy is authorised */
	V4L2_MPEG_AUDIO_NO_FUTHER_COPY_AUTHORISED    /* No more copy authorized */
};

enum v4l2_audio_encode_pcm_sampling_freq {
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_4000 = 0,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_5000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_6000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_8000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_11025 = 4,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_12000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_16000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_22050,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_24000 = 8,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_32000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_44100,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_48000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_64000 = 12,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_88200,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_96000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_128000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_176400 = 16,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_192000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_256000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_352800,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_384000 = 20,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_512000,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_705600,
	V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_768000,
};

/* Video related structures and controls */

/* video uncompressed metadata */
struct v4l2_video_uncompressed_metadata {
	unsigned int framerate_num;
	unsigned int framerate_den;
	unsigned int reserved[8];

	/* unsigned int width;
	   unsigned int height;
	   unsigned int pitch;
	   unsigned int vertical_alignment;
	   unsigned int noise_filter;
	   unsigned int colorspace;
	   unsigned int pixelformat */
};

/*tsmux related structures and controls */
struct v4l2_tsmux_src_es_metadata {
	unsigned long long DTS;
	int dit_transition;
};

enum stm_tsmux_stream_type {
	V4L2_STM_TSMUX_STREAM_TYPE_RESERVED			= 0x00,
	V4L2_STM_TSMUX_STREAM_TYPE_VIDEO_MPEG1		= 0x01,
	V4L2_STM_TSMUX_STREAM_TYPE_VIDEO_MPEG2		= 0x02,
	V4L2_STM_TSMUX_STREAM_TYPE_AUDIO_MPEG1		= 0x03,
	V4L2_STM_TSMUX_STREAM_TYPE_AUDIO_MPEG2		= 0x04,
	V4L2_STM_TSMUX_STREAM_TYPE_PRIVATE_SECTIONS	= 0x05,
	V4L2_STM_TSMUX_STREAM_TYPE_PRIVATE_DATA		= 0x06,
	V4L2_STM_TSMUX_STREAM_TYPE_MHEG		= 0x07,
	V4L2_STM_TSMUX_STREAM_TYPE_DSMCC		= 0x08,
	V4L2_STM_TSMUX_STREAM_TYPE_H222_1		= 0x09,

	V4L2_STM_TSMUX_STREAM_TYPE_AUDIO_AAC		= 0x0f,
	V4L2_STM_TSMUX_STREAM_TYPE_VIDEO_MPEG4	= 0x10,
	V4L2_STM_TSMUX_STREAM_TYPE_VIDEO_H264		= 0x1b,

	V4L2_STM_TSMUX_STREAM_TYPE_PS_AUDIO_AC3		= 0x81,
	V4L2_STM_TSMUX_STREAM_TYPE_PS_AUDIO_DTS		= 0x8a,
	V4L2_STM_TSMUX_STREAM_TYPE_PS_AUDIO_LPCM		= 0x8b,
	V4L2_STM_TSMUX_STREAM_TYPE_PS_DVD_SUBPICTURE	= 0xff,

	V4L2_STM_TSMUX_STREAM_TYPE_VIDEO_DIRAC		= 0xD1,
};

#define V4L2_STM_TSMUX_MAX_STRING 16

/*  STM control IDs */
#define V4L2_CID_STM_TSMUX_BASE		(V4L2_CTRL_CLASS_TSMUX | 0x2000)

/* Transport stream global controls */
#define V4L2_CID_STM_TSMUX_PCR_PERIOD		(V4L2_CID_STM_TSMUX_BASE+1)
#define V4L2_CID_STM_TSMUX_GEN_PCR_STREAM	(V4L2_CID_STM_TSMUX_BASE+2)
#define V4L2_CID_STM_TSMUX_PCR_PID		(V4L2_CID_STM_TSMUX_BASE+3)
#define V4L2_CID_STM_TSMUX_TABLE_GEN		(V4L2_CID_STM_TSMUX_BASE+4)
enum stm_tsmux_table_gen_flags {
	V4L2_STM_TSMUX_TABLE_GEN_NONE = 0x00,
	V4L2_STM_TSMUX_TABLE_GEN_PAT_PMT = 0x01,
	V4L2_STM_TSMUX_TABLE_GEN_SDT = 0x04
};
#define V4L2_CID_STM_TSMUX_TABLE_PERIOD		(V4L2_CID_STM_TSMUX_BASE+5)
#define V4L2_CID_STM_TSMUX_TS_ID		(V4L2_CID_STM_TSMUX_BASE+6)
#define V4L2_CID_STM_TSMUX_BITRATE_IS_CONSTANT	(V4L2_CID_STM_TSMUX_BASE+7)
#define V4L2_CID_STM_TSMUX_BITRATE		(V4L2_CID_STM_TSMUX_BASE+8)
/* Program controls */
#define V4L2_CID_STM_TSMUX_PROGRAM_NUMBER	(V4L2_CID_STM_TSMUX_BASE+9)
#define V4L2_CID_STM_TSMUX_PMT_PID		(V4L2_CID_STM_TSMUX_BASE+10)
#define V4L2_CID_STM_TSMUX_SDT_PROV_NAME	(V4L2_CID_STM_TSMUX_BASE+11)
#define V4L2_CID_STM_TSMUX_SDT_SERV_NAME	(V4L2_CID_STM_TSMUX_BASE+12)
#define V4L2_CID_STM_TSMUX_PMT_DESCRIPTOR	(V4L2_CID_STM_TSMUX_BASE+13)
/* Stream controls */
#define V4L2_CID_STM_TSMUX_INCLUDE_PCR		(V4L2_CID_STM_TSMUX_BASE+14)
#define V4L2_CID_STM_TSMUX_STREAM_PID		(V4L2_CID_STM_TSMUX_BASE+15)
#define V4L2_CID_STM_TSMUX_PES_STREAM_ID		(V4L2_CID_STM_TSMUX_BASE+16)
#define V4L2_CID_STM_TSMUX_STREAM_DESCRIPTOR	(V4L2_CID_STM_TSMUX_BASE+17)
#define V4L2_CID_STM_TSMUX_BIT_BUFFER_SIZE		(V4L2_CID_STM_TSMUX_BASE+18)
#define V4L2_CID_STM_TSMUX_INDEXING		(V4L2_CID_STM_TSMUX_BASE+19)
#define V4L2_CID_STM_TSMUX_SECTION_PERIOD		(V4L2_CID_STM_TSMUX_BASE+20)
#define V4L2_CID_STM_TSMUX_INDEXING_MASK		(V4L2_CID_STM_TSMUX_BASE+21)
#define V4L2_CID_STM_TSMUX_IGNORE_AUTO_PAUSE	(V4L2_CID_STM_TSMUX_BASE+22)

enum stm_tsmux_index_definition{
	V4L2_STM_TSMUX_INDEX_NONE = 0x0000,
	V4L2_STM_TSMUX_INDEX_PUSI = 0x0001,
	V4L2_STM_TSMUX_INDEX_PTS = 0x0002,
	V4L2_STM_TSMUX_INDEX_I_FRAME = 0x0004,
	V4L2_STM_TSMUX_INDEX_B_FRAME = 0x0008,
	V4L2_STM_TSMUX_INDEX_P_FRAME = 0x0010,
	V4L2_STM_TSMUX_INDEX_DIT = 0x0020,
	V4L2_STM_TSMUX_INDEX_RAP = 0x0040,
};

#define V4L2_ENC_IDX_FRAME_PUSI    (0x10)
#define V4L2_ENC_IDX_FRAME_PTS    (0x11)
#define V4L2_ENC_IDX_FRAME_DIT    (0x12)
#define V4L2_ENC_IDX_FRAME_RAP    (0x13)

#define STM_TSMUX_INDEX_RESERVED_PID  0
static inline unsigned int stm_tsmux_get_index_pid(struct v4l2_enc_idx_entry
						   *index)
{
	return index->reserved[STM_TSMUX_INDEX_RESERVED_PID];
}

#endif /*DVB_V4L2_EXPORT_H_ */
