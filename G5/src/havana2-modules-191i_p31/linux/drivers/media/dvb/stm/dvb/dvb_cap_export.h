/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.
 * Header file for the V4L2 dvp capture device driver containing the defines
 * to be exported to the user level.
************************************************************************/

#ifndef DVB_CAP_EXPORT_H_
#define DVB_CAP_EXPORT_H_

/*
 * CAP video modes
 */

typedef enum
{
    CAP_720_480_I60000,			/* NTSC I60 						*/
    CAP_720_480_P60000,			/* NTSC P60 						*/
    CAP_720_480_I59940,			/* NTSC 						*/
    CAP_720_480_P59940,			/* NTSC P59.94 (525p ?) 				*/

    CAP_640_480_I59940,			/* NTSC square, PAL M square 				*/
    CAP_720_576_I50000,			/* PAL B,D,G,H,I,N, SECAM 				*/
    CAP_720_576_P50000,			/* PAL P50 (625p ?)					*/
    CAP_768_576_I50000,			/* PAL B,D,G,H,I,N, SECAM square 			*/

    CAP_1920_1080_P60000,		/* SMPTE 274M #1  P60 					*/
    CAP_1920_1080_P59940,		/* SMPTE 274M #2  P60 /1.001 				*/
    CAP_1920_1080_P50000,		/* SMPTE 274M #3  P50 					*/
    CAP_1920_1080_P30000,		/* SMPTE 274M #7  P30 					*/

    CAP_1920_1080_P29970,		/* SMPTE 274M #8  P30 /1.001 				*/
    CAP_1920_1080_P25000,		/* SMPTE 274M #9  P25 					*/
    CAP_1920_1080_P24000,		/* SMPTE 274M #10 P24 					*/
    CAP_1920_1080_P23976,		/* SMPTE 274M #11 P24 /1.001 				*/

    CAP_1920_1080_I60000,		/* EIA770.3 #3 I60 = SMPTE274M #4 I60 			*/
    CAP_1920_1080_I59940,		/* EIA770.3 #4 I60 /1.001 = SMPTE274M #5 I60 /1.001 	*/
    CAP_1920_1080_I50000_274M,		/* SMPTE 274M Styled 1920x1080 I50 CEA/HDMI Code 20 	*/
    CAP_1920_1080_I50000_AS4933,	/* AS 4933.1-2005 1920x1080 I50 CEA/HDMI Code 39 	*/

    CAP_1280_720_P60000,		/* EIA770.3 #1 P60 = SMPTE 296M #1 P60 			*/
    CAP_1280_720_P59940,		/* EIA770.3 #2 P60 /1.001= SMPTE 296M #2 P60 /1.001 	*/
    CAP_1280_720_P50000,		/* SMPTE 296M Styled 1280x720 50P CEA/HDMI Code 19 	*/
    CAP_1280_1152_I50000,		/* AS 4933.1 1280x1152 I50 				*/

    CAP_640_480_P59940,			/* 640x480p @ 59.94Hz                    		*/
    CAP_640_480_P60000,			/* 640x480p @ 60Hz					*/
    CAP_720_240_P60000,			/* 720x240p @ 60Hz       - vcr trick modes              */
    CAP_720_240_P59940,			/* 720x240p @ 59.94 Hz   - vcr trick modes              */
    CAP_720_288_P50000,			/* 720x288p @ 50 Hz      - vcr trick modes              */

    CAP_VIDEO_MODE_LAST                 /* must stay last */
} cap_v4l2_video_mode_t;

/*
 * CAP picture aspect ratio modes
 */

typedef enum 
{
    CAP_PICTURE_ASPECT_RATIO_4_3,     /* 4:3 format  */
    CAP_PICTURE_ASPECT_RATIO_16_9,    /* 16:9 format */
    
    CAP_PICTURE_ASPECT_RATIO_LAST    
} cap_v4l2_picture_aspect_ratio_t;

typedef enum
{
    CAP_PT_VID1,        /* Internal VID1 output (ARGB8888 format, with A 0 to 128 range) */
    CAP_PT_VID2,        /* Internal VID2 output (ARGB8888 format, with A 0 to 128 range) */
    CAP_PT_EXT_YCBCR888,/* External AYCbCr port (AYCbCr8888 format, with A 0 to 255 range) */
    CAP_PT_MIX2_YCBCR,  /* MIX2 YCbCr output */
    CAP_PT_MIX2_RGB,    /* MIX2 RGB output */
    CAP_PT_MIX1_YCBCR,  /* MIX1 YCbCr output */
    CAP_PT_MIX1_RGB,    /* MIX1 RGB output */
    CAP_VIDEO_SOURCE_LAST                 /* must stay last */
} cap_v4l2_capture_source_t;

/*
 * A/V Receiver
 */

#define CAP_USE_DEFAULT						0x80000000

/*
 * A/V receiver controls to control the "D1-CAP0" input
 *
 */

/*
 * This list should not really have an offset again private base since this
 * prevents proper V4L2 enumeration of controls. Nevertheless for as long as 
 * it *does* exist we must remain ABI compatible. This means you must *not*
 * the order or numeric value of the controls.
 *
 * Specifically:
 *   - controls should not be removed from the list (but can be marked as
 *     deprecated)
 *   - controls must be added at the end of the list (just before VIDEO_LAST)
 *   - list must remain in current order (i.e. no, it is *not* helpful to
 *     alphabetize this list)
 */
enum 
{
    V4L2_CID_STM_CAP_VIDEO_FIRST		= (V4L2_CID_PRIVATE_BASE+400),

    V4L2_CID_STM_CAPIF_RESTORE_DEFAULT,
    V4L2_CID_STM_CAPIF_MIXER_INPUT,
    V4L2_CID_STM_CAPIF_VIDEO_INPUT,
    V4L2_CID_STM_CAPIF_BIG_ENDIAN,
    V4L2_CID_STM_CAPIF_FULL_RANGE,
    V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_HORIZONTAL_OFFSET,
    V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_VERTICAL_OFFSET,
    V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_WIDTH,
    V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_HEIGHT,
    V4L2_CID_STM_CAPIF_COLOUR_MODE,
    V4L2_CID_STM_CAPIF_VIDEO_LATENCY,
    V4L2_CID_STM_CAPIF_BLANK,
    V4L2_CID_STM_CAPIF_HORIZONTAL_RESIZE_ENABLE,
    V4L2_CID_STM_CAPIF_VERTICAL_RESIZE_ENABLE,
    V4L2_CID_STM_CAPIF_TOP_FIELD_FIRST,

    V4L2_CID_STM_AVR_LATENCY,					//!< Global AVR latency target (20ms to 150ms)

    // cjt do we need these?  
     
    V4L2_CID_STM_CAPIF_INCOMPLETE_FIRST_PIXEL,
    V4L2_CID_STM_CAPIF_ODD_PIXEL_COUNT,
    V4L2_CID_STM_CAPIF_VSYNC_BOTTOM_HALF_LINE_ENABLE,
    V4L2_CID_STM_CAPIF_HREF_POLARITY_POSITIVE,
     
     
    //
    // The enums and defines for the output crop transition modes
    //

#define CAP_TRANSITION_MODE_SINGLE_STEP		0
#define CAP_TRANSITION_MODE_STEP_OVER_N_VSYNCS	1

    V4L2_CID_STM_CAPIF_OUTPUT_CROP_TRANSITION_MODE,
    V4L2_CID_STM_CAPIF_OUTPUT_CROP_TRANSITION_MODE_PARAMETER_0,

//
    
    V4L2_CID_STM_CAPIF_PIXEL_ASPECT_RATIO_CORRECTION,

    V4L2_CID_STM_CAP_VIDEO_LAST
};

/*
 * A/V receiver controls to control the "I2S0" audio input
 */

enum 
{
  V4L2_HDMI_AUDIO_LAYOUT0 = 0,
  V4L2_HDMI_AUDIO_LAYOUT1 = 1,
  V4L2_HDMI_AUDIO_HBR     = 2,
};

enum v4l2_cap_audio_channel_select
{
    V4L2_CAP_AUDIO_CHANNEL_SELECT_STEREO = 0,
    V4L2_CAP_AUDIO_CHANNEL_SELECT_MONO_LEFT = 1,
    V4L2_CAP_AUDIO_CHANNEL_SELECT_MONO_RIGHT = 2
};

#if 0
/*
 * This list should not really have an offset again private base since this
 * prevents proper V4L2 enumeration of controls. Nevertheless for as long as 
 * it *does* exist we must remain ABI compatible. This means you must *not*
 * the order or numeric value of the controls.
 *
 * Specifically:
 *   - controls should not be removed from the list (but can be marked as
 *     deprecated)
 *   - controls must be added at the end of the list (for the AUDIO_I_LAST value)
 *   - list must remain in current order (i.e. no, it is *not* helpful to
 *     alphabetize this list)
 */
enum 
{
    V4L2_CID_STM_CAP_AUDIO_I_FIRST	= (V4L2_CID_PRIVATE_BASE+400),

    V4L2_CID_STM_CAP_AUDIO_MUTE, V4L2_CID_STM_AUDIO_MUTE = V4L2_CID_STM_CAP_AUDIO_MUTE,
    V4L2_CID_STM_CAP_AUDIO_FORMAT_RECOGNIZER, V4L2_CID_STM_AUDIO_FORMAT_RECOGNIZER = V4L2_CID_STM_CAP_AUDIO_FORMAT_RECOGNIZER,

    V4L2_CID_STM_CAP_AUDIO_AAC_DECODE, V4L2_CID_STM_AUDIO_AAC_DECODE = V4L2_CID_STM_CAP_AUDIO_AAC_DECODE,

    /* Emphasis = 
       0 --> No Emphasis
       1 --> SPDIF/HDMI Input Channel Status report Emphasis is ON.
    */
    V4L2_CID_STM_CAP_AUDIO_EMPHASIS, 

    /* HDMI_LAYOUT = 
       0 --> Layout 0
       1 --> Layout 1
       2 --> HighBitRate Audio
    */
    V4L2_CID_STM_CAP_AUDIO_HDMI_LAYOUT, 

    /* forward of Audio InfoFrame DataByte 4 is expected in case of HDMI_LAYOUT1 */
    V4L2_CID_STM_CAP_AUDIO_HDMI_AUDIOINFOFRAME_DB4, 

    /* Channel select for dual-mono streams. */
    V4L2_CID_STM_CAP_AUDIO_CHANNEL_SELECT,

    V4L2_CID_STM_CAP_AUDIO_I_LAST,
};
#endif


#endif /*DVB_CAP_EXPORT_H_*/
