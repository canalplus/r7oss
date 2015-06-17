/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef STM_SE_CONTROLS_H_
#define STM_SE_CONTROLS_H_

#ifndef ENTRY
#define ENTRY(enum) case enum: return #enum
#endif

/*!
 * \defgroup controls Controls
 *
 * Controls can be categorized by several orthogonal means.
 *
 * The most trivial categorization separates controls based their interface complexity. Specifically
 * controls may be either simple integers, compound structures or banked compound structures.
 *
 * Controls can also be categorized into those whose values are cascaded from container objects to their
 * children and those that are not.
 *
 * Finally controls can be categorized by their role and the objects to which they are applied.
 * Notably control that relates to policy tuning can be distinguished from other controls.
 *
 * \section simple_controls Simple controls
 *
 * A simple control is a key/value pair where the value is a signed 32-bit integer. Simple controls are used
 * to pass for Boolean, enumerated and integer values. Simple controls have getter/setter functions with the
 * post-fix get_control and set_control respectively.
 *
 * Simple controls are used to configure boolean, enumerated or numeric properties of STKPI objects.
 *
 * \section compound_controls Compound controls
 *
 * A compound control is also a key/value pair. The value consists of structured data and is described by
 * special purpose types that dictate the structure. Compound controls have getter/setter functions with the
 * post-fix get_compound_control and set_compound_control, respectively.
 *
 * Compound controls are used to collect properties of STKPI objects where is makes sense to set all
 * properties atomically. For example, the control to enable audio virtualization is atomically linked to
 * other virtualization parameters such as the desired virtualization algorithm.
 *
 * \section banked_compound_controls Banked compound controls
 *
 * A banked compound control has a single key but a banked collection of values. Like normal compound
 * controls they are represented as structured data, use special purpose types, and have getter/setter
 * functions with the post-fix get_compound_control and set_compound_control, respectively. The
 * distinguished feature of banked compound controls is the presence of bank selectors within the structure
 * that must be provided to both getter and setter functions (for a normal compound control the getter
 * function fills out the entire of the structure).
 *
 * Banked controls permit objects with N:1, 1:M or N:M links to other STKPI objects to be appropriately
 * configured.
 *
 * For example a sink object connected to two source objects can be configured to treat incoming data
 * differently, an audio_mixer provides a concrete example because different inputs must be configured with
 * different input gain levels. Similar a source object connected to multiple sink objects can be configured
 * to issue different data formats to each sink object.
 *
 * \code
 * typedef struct stm_se_example_banked_control_s {
 *     stm_object_h sink_object;  // bank selector
 *     int integer_property;
 *     bool boolean_property;
 * } stm_se_example_banked_control_t;
 * int res;
 * stm_object_h src_obj = ..., sink_obj = ...;
 * stm_se_example_banked_control_t banked_control;
 *
 *
 * banked_control.sink_object = sink_obj;
 * res = stm_se_example_get_control(src_obj,
 *         STM_SE_EXAMPLE_BANKED_CONTROL, &banked_control);
 * // check return value
 * banked_control.integer_property++;
 * res = stm_se_example_set_control(src_obj,
 *         STM_SE_EXAMPLE_BANKED_CONTROL, &banked_control);
 * // check return value
 * \endcode
 *
 * Banked compound controls are closely tied to the object graph. If the bank selector is not connected to
 * the controlled object when the control getter/setter functions are call then these functions will report
 * an error. Likewise when an object is detached from the controlled object then state any banked compound
 * controls related to that detached object will be discarded.
 *
 * \section cascaded_controls Cascaded controls
 *
 * A cascaded control allows a default value to be supplied to a container object such that the default
 * shall be applied to all contained objects.
 *
 * All streaming engine objects are implicitly contained within the streaming engine itself, thus all
 * cascaded controls can be applied system-wide using either ::stm_se_set_control.
 *
 * These system-wide values flow into both the playback and encode container objects before finally flowing
 * into the appropriate stream objects.
 *
 * Flow of cascaded controls into leaf objects:
 *
 * \image html cascaded_controls.svg
 * \image latex cascaded_controls.pdf
 *
 * \section cascaded_policy_controls Cascaded policy controls
 *
 * Cascaded policy controls alter the behavior of playbacks or of the play streams contained within them.
 *
 * @{
 */

/*
 * Enumeration macros for control values (in the same order control appear in stm_se_ctrl_t)
 */

/*! \name Generic control values */
//!\{
#define STM_SE_CTRL_VALUE_DISAPPLY              0  //!< Do not apply the control
#define STM_SE_CTRL_VALUE_APPLY                 1  //!< Apply the control
#define STM_SE_CTRL_VALUE_AUTO                  2  //!< Automatically apply the control if required for best operation
//!\}

static inline const char *StringifyGenericValue(uint32_t aGenericValue)
{
    switch (aGenericValue)
    {
        ENTRY(STM_SE_CTRL_VALUE_DISAPPLY);
        ENTRY(STM_SE_CTRL_VALUE_APPLY);
        ENTRY(STM_SE_CTRL_VALUE_AUTO);
    default: return "<unknown generic value>";
    }
}

/*! \name playout control values for
     \ref STM_SE_CTRL_PLAYOUT_ON_TERMINATE
     \ref STM_SE_CTRL_PLAYOUT_ON_SWITCH
     \ref STM_SE_CTRL_PLAYOUT_ON_DRAIN */
//!\{
#define STM_SE_CTRL_VALUE_PLAYOUT               0
#define STM_SE_CTRL_VALUE_DISCARD               1
//!\}

/*! \name HDMIRx mode control values */
//!\{
#define STM_SE_CTRL_VALUE_HDMIRX_DISABLED                     0  //!< See \ref STM_SE_CTRL_HDMI_RX_MODE
#define STM_SE_CTRL_VALUE_HDMIRX_REPEATER                     1  //!< See \ref STM_SE_CTRL_HDMI_RX_MODE
#define STM_SE_CTRL_VALUE_HDMIRX_NON_REPEATER                 2  //!< See \ref STM_SE_CTRL_HDMI_RX_MODE
//!\}


/*! \name MPEG2 application type control values */
//!\{
#define STM_SE_CTRL_VALUE_MPEG2_APPLICATION_MPEG2 0  //!< See \ref STM_SE_CTRL_MPEG2_APPLICATION_TYPE
#define STM_SE_CTRL_VALUE_MPEG2_APPLICATION_ATSC  1  //!< See \ref STM_SE_CTRL_MPEG2_APPLICATION_TYPE
#define STM_SE_CTRL_VALUE_MPEG2_APPLICATION_DVB   2  //!< See \ref STM_SE_CTRL_MPEG2_APPLICATION_TYPE
//!\}
/*! \name Video memory profiles */
//!\{
#define STM_SE_CTRL_VALUE_VIDEO_DECODE_HD_PROFILE                0 //!< see STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_DECODE_4K2K_PROFILE              1 //!< see STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_DECODE_SD_PROFILE                2 //!< see STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_DECODE_720P_PROFILE              3 //!< see STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_DECODE_UHD_PROFILE               4 //!< see STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_DECODE_HD_LEGACY_PROFILE         5 //!< see STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_HDMIRX_REP_HD_PROFILE            6 //!< see STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_HDMIRX_REP_UHD_PROFILE           7 //!< see STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_HDMIRX_NON_REP_HD_PROFILE        8 //!< see STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_HDMIRX_NON_REP_UHD_PROFILE       9 //!< see STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
//!\}

/*! \name Frame rate calculation precedence control values
 */
//!\{
#define STM_SE_CTRL_VALUE_PRECEDENCE_STREAM_PTS_CONTAINER_DEFAULT  0 //<! See \ref STM_SE_CTRL_FRAME_RATE_CALCULUATION_PRECEDENCE
#define STM_SE_CTRL_VALUE_PRECEDENCE_STREAM_CONTAINER_PTS_DEFAULT  1 //<! See \ref STM_SE_CTRL_FRAME_RATE_CALCULUATION_PRECEDENCE
#define STM_SE_CTRL_VALUE_PRECEDENCE_PTS_STREAM_CONTAINER_DEFAULT  2 //<! See \ref STM_SE_CTRL_FRAME_RATE_CALCULUATION_PRECEDENCE
#define STM_SE_CTRL_VALUE_PRECEDENCE_PTS_CONTAINER_STREAM_DEFAULT  3 //<! See \ref STM_SE_CTRL_FRAME_RATE_CALCULUATION_PRECEDENCE
#define STM_SE_CTRL_VALUE_PRECEDENCE_CONTAINER_PTS_STREAM_DEFAULT  4 //<! See \ref STM_SE_CTRL_FRAME_RATE_CALCULUATION_PRECEDENCE
#define STM_SE_CTRL_VALUE_PRECEDENCE_CONTAINER_STREAM_PTS_DEFAULT  5 //<! See \ref STM_SE_CTRL_FRAME_RATE_CALCULUATION_PRECEDENCE
//!\}

/*! \name Error decoding level control values
 */
//!\{
#define STM_SE_CTRL_VALUE_ERROR_DECODING_LEVEL_NORMAL  0  //<! See \ref STM_SE_CTRL_ERROR_DECODING_LEVEL
#define STM_SE_CTRL_VALUE_ERROR_DECODING_LEVEL_MAXIMUM 1  //<! See \ref STM_SE_CTRL_ERROR_DECODING_LEVEL
//!\}

/*! \name Audio application type control values */
//!\{
#define STM_SE_CTRL_VALUE_AUDIO_APPLICATION_ISO               0  //!< See \ref STM_SE_CTRL_AUDIO_APPLICATION_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVD               1  //!< See \ref STM_SE_CTRL_AUDIO_APPLICATION_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVB               2  //!< See \ref STM_SE_CTRL_AUDIO_APPLICATION_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS10              3  //!< See \ref STM_SE_CTRL_AUDIO_APPLICATION_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS11              4  //!< See \ref STM_SE_CTRL_AUDIO_APPLICATION_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS12              5  //!< See \ref STM_SE_CTRL_AUDIO_APPLICATION_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_APPLICATION_HDMIRX_GAME_MODE  6 //!< See \ref STM_SE_CTRL_AUDIO_APPLICATION_TYPE
#define STM_SE_CTRL_VALUE_LAST_AUDIO_APPLICATION_TYPE STM_SE_CTRL_VALUE_AUDIO_APPLICATION_HDMIRX_GAME_MODE
//!\}

/*! \name Audio service type control values */
//!\{
#define STM_SE_CTRL_VALUE_AUDIO_SERVICE_PRIMARY                     0  //!< STM_SE_CTRL_AUDIO_SERVICE_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_SERVICE_SECONDARY                   1  //!< STM_SE_CTRL_AUDIO_SERVICE_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_SERVICE_MAIN                        2  //!< STM_SE_CTRL_AUDIO_SERVICE_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_SERVICE_AUDIO_DESCRIPTION           3  //!< STM_SE_CTRL_AUDIO_SERVICE_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_SERVICE_MAIN_AND_AUDIO_DESCRIPTION  4  //!< STM_SE_CTRL_AUDIO_SERVICE_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_SERVICE_CLEAN_AUDIO                 5  //!< STM_SE_CTRL_AUDIO_SERVICE_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_SERVICE_AUDIO_GENERATOR             6  //!< STM_SE_CTRL_AUDIO_SERVICE_TYPE
#define STM_SE_CTRL_VALUE_AUDIO_SERVICE_INTERACTIVE_AUDIO           7  //!< STM_SE_CTRL_AUDIO_SERVICE_TYPE
#define STM_SE_CTRL_VALUE_LAST_AUDIO_SERVICE_TYPE STM_SE_CTRL_VALUE_AUDIO_SERVICE_INTERACTIVE_AUDIO
//!\}

/*! \name Region control values */
//!\{
#define STM_SE_CTRL_VALUE_REGION_UNDEFINED 0  //!< See \ref STM_SE_CTRL_REGION
#define STM_SE_CTRL_VALUE_REGION_ATSC      1  //!< See \ref STM_SE_CTRL_REGION
#define STM_SE_CTRL_VALUE_REGION_DVB       2  //!< See \ref STM_SE_CTRL_REGION
#define STM_SE_CTRL_VALUE_REGION_NORDIG    3  //!< See \ref STM_SE_CTRL_REGION
#define STM_SE_CTRL_VALUE_REGION_DTG       4  //!< See \ref STM_SE_CTRL_REGION
#define STM_SE_CTRL_VALUE_REGION_ARIB      5  //!< See \ref STM_SE_CTRL_REGION
#define STM_SE_CTRL_VALUE_REGION_DTMB      6  //!< See \ref STM_SE_CTRL_REGION
//!\}

/*! \name Decimate decoder output control values */
//!\{
#define STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_DISABLED  0  //<! See \ref STM_SE_CTRL_DECIMATE_DECODER_OUTPUT
#define STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_HALF      1  //<! See \ref STM_SE_CTRL_DECIMATE_DECODER_OUTPUT
#define STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_QUARTER   2  //<! See \ref STM_SE_CTRL_DECIMATE_DECODER_OUTPUT
//!\}

/*! \name Video last frame behaviour control values */
//!\{
#define STM_SE_CTRL_VALUE_LEAVE_LAST_FRAME_ON_SCREEN 0  //!< See \ref STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR
#define STM_SE_CTRL_VALUE_BLANK_SCREEN               1  //!< See \ref STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR
//!\}

/*! \name Master clock control values */
//!\{
#define STM_SE_CTRL_VALUE_VIDEO_CLOCK_MASTER    0  //!< See \ref STM_SE_CTRL_MASTER_CLOCK
#define STM_SE_CTRL_VALUE_AUDIO_CLOCK_MASTER    1  //!< See \ref STM_SE_CTRL_MASTER_CLOCK
#define STM_SE_CTRL_VALUE_SYSTEM_CLOCK_MASTER   2  //!< See \ref STM_SE_CTRL_MASTER_CLOCK
//!\}

/*! \name Video discard frame control values */
//!\{
#define STM_SE_CTRL_VALUE_NO_DISCARD            0 //!< See \ref STM_SE_CTRL_VIDEO_DISCARD_FRAMES
#define STM_SE_CTRL_VALUE_REFERENCE_FRAMES_ONLY 1 //!< See \ref STM_SE_CTRL_VIDEO_DISCARD_FRAMES
#define STM_SE_CTRL_VALUE_KEY_FRAMES_ONLY       2 //!< See \ref STM_SE_CTRL_VIDEO_DISCARD_FRAMES
//!\}

/*! \name Video discard late frame control values */
//!\{
#define STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_NEVER             0  //!< See \ref STM_SE_CTRL_DISCARD_LATE_FRAMES
#define STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_ALWAYS            1  //!< See \ref STM_SE_CTRL_DISCARD_LATE_FRAMES
#define STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_AFTER_SYNCHRONIZE 2  //!< See \ref STM_SE_CTRL_DISCARD_LATE_FRAMES
//!\}

/*! \name Trick mode domain control values
 *
 * \note These values are selected so that (with the exception of _AUTO) numerically
 *       higher values represent a greater degree of discard/degrade.
 */
//!\{
#define STM_SE_CTRL_VALUE_TRICK_MODE_AUTO                                           0  //!< See \ref STM_SE_CTRL_TRICK_MODE_DOMAIN
#define STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL                                     1  //!< See \ref STM_SE_CTRL_TRICK_MODE_DOMAIN
#define STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES        2  //!< See \ref STM_SE_CTRL_TRICK_MODE_DOMAIN
#define STM_SE_CTRL_VALUE_TRICK_MODE_DISCARD_NON_REFERENCE_FRAMES                   3  //!< See \ref STM_SE_CTRL_TRICK_MODE_DOMAIN
#define STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES 4  //!< See \ref STM_SE_CTRL_TRICK_MODE_DOMAIN
#define STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_KEY_FRAMES                              5  //!< See \ref STM_SE_CTRL_TRICK_MODE_DOMAIN
#define STM_SE_CTRL_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES                       6  //!< See \ref STM_SE_CTRL_TRICK_MODE_DOMAIN
//!\}

/*! \name Trick mode audio
 */
//!\{
#define STM_SE_CTRL_VALUE_TRICK_AUDIO_MUTE                                          0  //!< See \ref STM_SE_CTRL_TRICK_MODE_AUDIO
#define STM_SE_CTRL_VALUE_TRICK_AUDIO_PITCH_CORRECTED                               1  //!< See \ref STM_SE_CTRL_TRICK_MODE_AUDIO
//!\}

/*! \name Audio dual-mono meta-data override control values */
//!\{
#define STM_SE_CTRL_VALUE_USER_ENFORCED_DUALMONO    0  //<! See \ref STM_SE_CTRL_STREAM_DRIVEN_DUALMONO
#define STM_SE_CTRL_VALUE_STREAM_DRIVEN_DUALMONO    1  //<! See \ref STM_SE_CTRL_STREAM_DRIVEN_DUALMONO
//!\}

/*! \name Audio dual-mono mode control values; see \ref STM_SE_CTRL_DUALMONO */
typedef enum stm_se_dual_mode_e
{
    STM_SE_STEREO_OUT,      //!< L and R channels are rendered as is
    STM_SE_DUAL_LEFT_OUT,   //!< L channel is replicated on R channel
    STM_SE_DUAL_RIGHT_OUT,  //!< R channel is replicated on L channel
    STM_SE_MONO_OUT,        //!< L+R mix is rendered on both L and R channels
} stm_se_dual_mode_t;

/*! \name Encode stream bitrate mode control values */
//!\{
#define STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_CBR  0  //!< See \ref STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE
#define STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR  1  //!< See \ref STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE
#define STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_OFF  2  //!< See \ref STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE
//!\}

static inline const char *StringifyBitrateMode(uint32_t aBitrateMode)
{
    switch (aBitrateMode)
    {
        ENTRY(STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_CBR);
        ENTRY(STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR);
        ENTRY(STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_OFF);
    default: return "<unknown bitrate mode>";
    }
}

/*! \name Video encode memory profiles control values */
//!\{
#define STM_SE_CTRL_VALUE_ENCODE_CIF_PROFILE           0  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_ENCODE_SD_PROFILE            1  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_ENCODE_720p_PROFILE          2  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE
#define STM_SE_CTRL_VALUE_ENCODE_HD_PROFILE            3  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE
//!\}

static inline const char *StringifyMemoryProfile(uint32_t aMemoryProfile)
{
    switch (aMemoryProfile)
    {
        ENTRY(STM_SE_CTRL_VALUE_ENCODE_CIF_PROFILE);
        ENTRY(STM_SE_CTRL_VALUE_ENCODE_SD_PROFILE);
        ENTRY(STM_SE_CTRL_VALUE_ENCODE_720p_PROFILE);
        ENTRY(STM_SE_CTRL_VALUE_ENCODE_HD_PROFILE);
    default: return "<unknown memory profile>";
    }
}

/*! \name Video encode stream H.264 level control values */
//!\{
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_0_9    9  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_0   10  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_B  101  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_1   11  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_2   12  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_3   13  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_0   20  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_1   21  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_2   22  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_0   30  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_1   31  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_2   32  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_0   40  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_1   41  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_2   42  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_0   50  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_1   51  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
#define STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_2   52  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
//!\}

static inline const char *StringifyLevel(uint32_t aLevel)
{
    switch (aLevel)
    {
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_0_9);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_0);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_B);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_1);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_2);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_3);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_0);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_1);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_2);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_0);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_1);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_2);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_0);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_1);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_2);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_0);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_1);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_2);
    default: return "<unknown level>";
    }
}

/*! \name Video encode stream H.264 profile control values */
//!\{
#define STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_BASELINE  66  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_MAIN      77  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_EXTENDED  88  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH     100  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_10  110  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_422 122  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE
#define STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_444 144  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE
//!\}

static inline const char *StringifyProfile(uint32_t aProfile)
{
    switch (aProfile)
    {
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_BASELINE);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_MAIN);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_EXTENDED);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_10);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_422);
        ENTRY(STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_444);
    default: return "<unknown profile>";
    }
}

/*! \name Encode display aspect ratio control values */
//!\{
#define STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE   0  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO
#define STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_4_BY_3   1  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO
#define STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_16_BY_9  2  //!< See \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO
//!\}

static inline const char *StringifyDisplayAspectRatio(uint32_t aDisplayAspectRatio)
{
    switch (aDisplayAspectRatio)
    {
        ENTRY(STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE);
        ENTRY(STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_4_BY_3);
        ENTRY(STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_16_BY_9);
    default: return "<unknown display aspect ratio>";
    }
}

/*!
 * Compound control value used by \ref STM_SE_CTRL_PLAY_STREAM_ELEMENTARY_BUFFER_LEVEL.
 */
typedef struct stm_se_ctrl_play_stream_elementary_buffer_level_s
{
    uint32_t actual_size;
    uint32_t effective_size;
    uint32_t bytes_used;
    uint32_t maximum_nonblock_write;
} stm_se_ctrl_play_stream_elementary_buffer_level_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_MPEG2_TIME_CODE.
 */
typedef struct stm_se_ctrl_mpeg2_time_code_s
{
    uint8_t time_code_hours;
    uint8_t time_code_minutes;
    uint8_t time_code_seconds;
    uint8_t time_code_pictures;
} stm_se_ctrl_mpeg2_time_code_t;

/*!
 * Compound control value used, indirectly, by \ref STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS
 * and \ref STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS.
 */
typedef struct  stm_se_play_stream_decoded_frame_buffer_reference_s
{
    void *virtual_address;   //<! kernel virtual address (mandatory).
    void *physical_address;  //<! Corresponding physical address (can be NULL).
    uint32_t buffer_size;    //<! Size in bytes of the buffer.
} stm_se_play_stream_decoded_frame_buffer_reference_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS
 * and \ref STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS.
 */
typedef struct  stm_se_play_stream_decoded_frame_buffer_info_s
{
    /*!
     * Number of buffers.
     *
     * For \ref STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS this is the minimum number
     * of buffers required; for \ref STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS this is
     * the actual number of buffer provided.
     */
    uint32_t number_of_buffers;

    /*!
     * Size, in bytes, of one buffer.
     *
     * For \ref STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS this is the minimum size
     * a buffer must be; for \ref STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS this is
     * the actual size of buffers provided.
     */
    uint32_t buffer_size;

    /*!
     * Used by \ref STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS to expose what boundary
     * buffers must be aligned to.
     *
     * \pre Must be a power-of-two or zero (no alignment contraint).
     */
    uint32_t buffer_alignement;

    /*!
     * Used by \ref STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS to expose any hardware
     * constraints related to memory type.
     *
     * 0 means no attributed.
     */
    uint32_t mem_attribute;

    /*!
     * Used by \ref STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS to provide a list of decode buffers.
     */
    stm_se_play_stream_decoded_frame_buffer_reference_t *buf_list;
} stm_se_play_stream_decoded_frame_buffer_info_t;

/*!
 * Compound control value used, indirectly, by \ref STM_SE_CTRL_AAC_DECODER_CONFIG.
 */
typedef  enum stm_se_aac_profile_e
{
    STM_SE_AAC_LC_TS_PROFILE,   //!< Auto detect
    STM_SE_AAC_LC_ADTS_PROFILE, //!< ADTS force
    STM_SE_AAC_LC_LOAS_PROFILE, //!< LOAS force
    STM_SE_AAC_LC_RAW_PROFILE,  //!< RAW  force
    STM_SE_AAC_BSAC_PROFILE,    //!< BSAC force
} stm_se_aac_profile_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_AAC_DECODER_CONFIG.
 */
typedef struct stm_se_mpeg4aac_s
{
    /*!
     * Specify the type of AAC to be processed by the play-stream
     */
    stm_se_aac_profile_t         aac_profile;

    /*!
     * Enable SuBandReplication decoding
     */
    bool                         sbr_enable;

    /*!
     * Enable processing of SBR when AAC-LC is at 48 kHz or more.
     */
    bool                         sbr_96k_enable;

    /*!
     * Enable ParametricStereo decoding.
     */
    bool                         ps_enable;
} stm_se_mpeg4aac_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_AUDIO_GENERATOR_BUFFER.
 */
typedef struct stm_se_audio_generator_buffer_s
{
    /*!
     * Pointer to the buffer containing the samples.
     */
    void                           *audio_buffer;

    /*!
     * Size in bytes of the buffer.
     */
    uint32_t                        audio_buffer_size;

    /*!
     * WS8, WS16, WS32 ...
     */
    stm_se_audio_pcm_format_t       format;
} stm_se_audio_generator_buffer_t;


/*!
 * Compound control value used, indirectly, by \ref STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION.
 */

/*!
 * Compound control value used, indirectly, by \ref STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION.
 */
typedef enum stm_se_compression_mode_e
{
    STM_SE_NO_COMPRESSION,  //!< Compression is disabled
    STM_SE_COMP_CUSTOM_A,   //!< Dialog norm disabled, DRC enabled
    STM_SE_COMP_CUSTOM_B,   //!< Alternate test mode (as per DolbyDigital definition)
    STM_SE_COMP_LINE_OUT,   //!< Wide dynamic range output (PROGRAM LEVEL at -31 dBFS)
    STM_SE_COMP_RF_MODE,    //!< Reduced dynamic range output (PROGRAM_LEVEL at -20 dBFS)
} stm_se_compression_mode_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION.
 */

typedef struct stm_se_compression_s
{
    /*!
     * Dynamic range compression to be applied.
     */
    stm_se_compression_mode_t  mode;

    /*!
     * Modulate the attenuation applied to loud audio signals.
     *
     * Must be within the range 0 (no attenuation) to 255 (100% of encoded attenuation).
     */
    uint8_t                    cut;

    /*!
     * Modules the gain applied to quiet audio signals.
     *
     * Must be within the range 0 (no applification) to 255 (100% of encoded amplification).
     */
    uint8_t                    boost;
} stm_se_drc_t;

/*!
 * Compound control value used, indirectly, by \ref STM_SE_CTRL_BASSMGT.
 */
typedef enum stm_se_bassmgt_type_e
{
    STM_SE_BASSMGT_SPEAKER_BALANCE,
    STM_SE_BASSMGT_DOLBY_1,
    STM_SE_BASSMGT_DOLBY_2,
    STM_SE_BASSMGT_DOLBY_3
} stm_se_bassmgt_type_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_BASSMGT.
 */
typedef struct stm_se_bassmgt_s
{
    /*!
     * Bass management enable/disable
     */
    uint32_t                  apply;

    /*!
     * Bass management topology.
     */
    stm_se_bassmgt_type_t     type;

    /*!
     * Set to TRUE when 12 dB boost is applied in analog domain, Center, LeftSurround and
     * RightSurround channels will be attenuated by 12 dB in digital domain.
     */
    bool                      boost_out;

    /*!
     * Output wordsize: Defines the Rounding to apply to every 32 bit samples in order to match the
     * requested output wordsize.
     */
    uint16_t                  ws_out;

    /*!
     * Cut-off frequency in Hz of the high and low pass filters.
     */
    uint8_t                   cut_off_frequency;

    /*!
     * Order of the high and low pass filter (choices are 1 and 2).
     */
    uint8_t                   filter_order;

    /*!
     * Signed attenuation in mB to apply to individual channels. Must be in the
     * range -9600..0.
     */
    int16_t                   gain[8];

    /*!
     * Enable/disable the delay module of this pcm-processing.
     */
    uint32_t                  delay_enable;

    /*!
     * Delay in ms to apply to individual channels. Must be in the range 0..30.
     */
    uint8_t                   delay[8];
} stm_se_bassmgt_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_LIMITER.
 */
typedef struct stm_se_limiter_s
{
    /*!
     * limiter enable/disable
     */
    uint32_t                  apply;

    /*!
     * Set to TRUE if one wants to force the given gain at once (without smoothing).
     */
    bool                     hard_gain;

    /*!
     * Set to TRUE to activate the look-ahead module of this pcm-processing.
     */
    uint32_t                 lookahead_enable;

    /*!
     * Look-ahead to apply in ms (if activated).
     */
    uint32_t                 lookahead;
} stm_se_limiter_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_BTSC.
 */
typedef struct stm_se_btsc_s
{
    bool dual_signal;    //<! Set to TRUE to enable dual signal
    int32_t tx_gain;     //<! tx_gain applied to BTSC output
    int32_t input_gain;  //<! Input gain for BTSC output
} stm_se_btsc_t;

/*!
 * Max number of bands supported by the MDRC process.
 */
#define STM_SE_NB_MDRC_BANDS 5

/*!
 * Compound control value used by \ref STM_SE_CTRL_ST_MDRC.
 */
typedef struct stm_se_st_mdrc_s
{
    uint32_t num_bands;                 //!< Number of bands (see \ref STM_SE_NB_MDRC_BANDS)
    struct stm_se_mdrc_band_s
    {
        uint32_t cut_off_frequency;     //!< Cut-off frequency of the filter in Hz
        uint32_t post_gain;             //!< Makeup gain within [0, 2000] mB
        int32_t  compression_threshold; //!< Compression Threshold within [-4000, -1000] mB
        uint32_t compression_slope;     //!< Compression Slope within [0, 100] %
        int32_t  noise_gate_threshold;  //!< Noise gate threshold within [-9000, -4000] mB
        uint32_t noise_gate_slope;      //!< Noise gate slope within [100, 800] %
        uint32_t attack_time;           //!< Compressor attack time within [1,1000] ms
        uint32_t release_time;          //!< Compressor release time within [10, 5000] ms
    } band[STM_SE_NB_MDRC_BANDS];
} stm_se_st_mdrc_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENT.
 */
typedef struct stm_se_q3dot13_input_gain_s
{
    /*!
     * Handle of the input port on which this coefficient control applies.
     */
    stm_object_h     input;

    /*!
     * Handle of the mixer-object or audio-player object to which this mixing applies or
     * NULL.
     *
     * If the output handler is set to NULL this instructs the mixer to apply the same set of
     * coefficients on the named input for all outputs.
     *
     * \pre Reserved. Set to NULL.
     */
    stm_object_h     output;

    /*!
     *
     */
    stm_object_h     objecthandle;

    /*!
     * For multichannel input: Array of Gain coefficient.
     *
     * Values are within [0.0, 8.0] in Q3.13 fixed point notation.
     */
    stm_se_q3dot13_t gain[8];

    /*!
     * For mono input: Array of Panning coefficient from input to Channel[n].
     *
     * Values are within [0.0, 8.0] in Q3.13 fixed point notation.
     */
    stm_se_q3dot13_t panning[8];
} stm_se_q3dot13_input_gain_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN.
 */
typedef struct stm_se_q3dot13_output_gain_s
{
    /*!
     * Bank selector.
     */
    stm_object_h     output;

    /*!
     * Array of Gain coefficient to apply to the output.
     *
     * Values are within [0.0, 8.0] in Q3.13 fixed point notation.
     */
    stm_se_q3dot13_t    gain[8];
} stm_se_q3dot13_output_gain_t;

/*!
 * Compound control value used, indirectly, by \ref STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE.
 */
typedef enum stm_se_player_type_e
{
    STM_SE_PLAYER_I2S,
    STM_SE_PLAYER_SPDIF,
    STM_SE_PLAYER_HDMI,
    STM_SE_PLAYER_SPDIF_HDMI,
} stm_se_player_type_t;

/*!
 * Compound control value used, indirectly, by \ref STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE.
 */
typedef enum stm_se_player_sink_e
{
    STM_SE_PLAYER_SINK_AUTO,
    STM_SE_PLAYER_SINK_TV,
    STM_SE_PLAYER_SINK_AVR,
    STM_SE_PLAYER_SINK_HEADPHONE,
} stm_se_player_sink_t;

/*!
 * Compound control value used, indirectly, by \ref STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE.
 */
typedef enum stm_se_player_mode_e
{
    /*!
     * Issue PCM.
     */
    STM_SE_PLAY_PCM_OUT,

    /*!
     * Bypass the compressed stream of the play_stream tagged as MAIN that is
     * connected to this player.
     *
     * Bypass is simplified or suppressed if the player doesn't support the specific type of
     * stream (for example, DD+ for SPDIF would be replaced by AC3, DTSHD by DTS, WMA by PCM ...).
     */
    STM_SE_PLAY_MAIN_COMPRESSED_OUT,

    /*!
     * Bypass the compressed stream of the play_stream tagged as MAIN that is
     * connected to this player.
     *
     * The COMPRESSED_SD mode is similar to the COMPRESSED mode
     * except for the following HDMI cases:
     *
     * input stream -> HDMI output presented stream
     * ============================================
     * DD+ (E-AC3)  -> DD AC3
     * DTS-HD       -> DTS
     * DTS-HD MA    -> DTS
     *
     * This mode provides compatibility with older A/V Receivers that doesn't support these formats.
     */
    STM_SE_PLAY_MAIN_COMPRESSED_SD_OUT,

    /*!
     * Re-encode the audio to play through this player into AC3.
     */
    STM_SE_PLAY_AC3_OUT,

    /*!
     * Re-encode the audio to play through this player into DTS.
     */
    STM_SE_PLAY_DTS_OUT,

    /*!
     * Re-encode the audio to play through this player into BTSC
     */
    STM_SE_PLAY_BTSC_OUT,
} stm_se_player_mode_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE.
 */
typedef struct stm_se_ctrl_audio_player_hardware_mode_s
{
    /*!
     * Hint the player about to which physical type of output it is connected to (I2S, SPDIF or HDMI).
     */
    stm_se_player_type_t   player_type;

    /*!
     * Hint the player about to which kind of device the output is connected to (AVR, TV, Headphone)
     *
     * If Auto, streaming engine will consider following default according to player_type:
     *   - I2S   ==> TV
     *   - SPDIF ==> AVR
     *   - HDMI  ==> TV (including SPDIF_HDMI)
     */
    stm_se_player_sink_t   sink_type;

    /*!
     * Specify the type of data that the player should render.
     */
    stm_se_player_mode_t   playback_mode;

    /*!
     * The following values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY
     *             Condition the playback_mode according to player capability.
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY
     *             Force the playback mode without restriction (for example, DD+ could be played on
     *             SPDIF at 192 kHz)
     * \arg \c ::STM_SE_CTRL_VALUE_AUTO
     *             Ccondition the playback_mode according to player capability and EDID if available
     *             (HDMI case)
     */
    uint32_t               playback_control;

    /*!
     * Number of channels to issue.
     */
    int32_t                num_channels;
} stm_se_ctrl_audio_player_hardware_mode_t;

/*!
 * Compound control value used, indirectly, by \ref STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY.
 */
typedef enum  stm_se_output_freq_e
{
    STM_SE_MAX_OUTPUT_FREQUENCY,
    STM_SE_FIXED_OUTPUT_FREQUENCY
} stm_se_output_freq_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY.
 */
typedef struct stm_se_output_frequency_s
{
    /*!
     * Describes whether the given frequency is the absolute target frequency or the max frequency
     * allowed for this object.
     */
    enum stm_se_output_freq_e control;

    /*!
     * Sampling frequency in KHz at which the audio should be played out of the object.
     *
     * If '0' , the audio is played at the frequency of the MASTER client. Must be an ISO
     * frequency within the range 32..192.
     */
    uint32_t                  frequency;
} stm_se_output_frequency_t;

/*!
 * Compound control value used, indirectly, by \ref STM_SE_CTRL_AUDIO_READER_SOURCE_INFO.
 * Aligned with enum stm_hdmirx_audio_stream_type_e
 */
typedef enum stm_se_audio_stream_type_e
{
    STM_SE_AUDIO_STREAM_TYPE_UNKNOWN = 0x00,
    STM_SE_AUDIO_STREAM_TYPE_IEC     = 0x01,
    STM_SE_AUDIO_STREAM_TYPE_DSD     = 0x02,
    STM_SE_AUDIO_STREAM_TYPE_DST     = 0x04,
    STM_SE_AUDIO_STREAM_TYPE_HBR     = 0x08
} stm_se_audio_stream_type_t;

/*!
 * Compound control value used, indirectly, by \ref STM_SE_CTRL_AUDIO_READER_SOURCE_INFO.
 */
typedef enum stm_se_audio_layout_e
{
    STM_SE_AUDIO_LAYOUT_0,
    STM_SE_AUDIO_LAYOUT_1
} stm_se_audio_layout_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_AUDIO_READER_SOURCE_INFO.
 */
typedef struct stm_se_ctrl_audio_reader_source_info_s
{
    /*!
     * The source has muted the audio samples (perhaps because of some change in audio parameters).
     */
    bool                        is_muted;

    /*!
     * Number of audio channels from the source.
     */
    uint8_t                     channel_count;

    /*!
     * Channel allocation details on the (applicable for multichannel audio only). Refer to CIA
     * 861-D 6.6.2 table 20.
     */
    uint8_t                     channel_alloc;

    /*!
     * Down mix attenuation that is applied uniformly by the source on all channels, when the source
     * number of channels is not matching the sink number of channels.
     *
     * Refer to CIA 861-D 6.6.2 table 21.
     *
     * Units in dB, with min = 0dB and Max is 15 dB.
     */
    uint8_t                     level_shift_value;

    /*!
     * Describes what value is used for LFE playback level as compared to the other channel (units
     * in dB. Valid values of 0 and 10 dB only).
     */
    uint8_t                     lfe_playback_level;

    /*!
     * Sampling Frequency in Hz
     */
    uint32_t                    sampling_frequency;

    /*!
     * Describes whether audio output is permitted to be down-mixed or not
     */
    bool                        down_mix_inhibit;

    /*!
     * Audio stream type.
     */
    stm_se_audio_stream_type_t  stream_type;

    /*!
     * Audio layout (stereo or multi-channel)
     */
    stm_se_audio_layout_t       audio_layout;
} stm_se_ctrl_audio_reader_source_info_t;

/*!
 * Selects the multi-slice mode.
 *
 * See ::stm_se_encode_process_metadata_video_t .
 */
typedef enum  stm_se_video_multi_slice_mode_e
{
    STM_SE_VIDEO_MULTI_SLICE_MODE_SINGLE,   //<! 1 only slice per encoded frame
    STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_MB,   //<! A slice is closed as soon as the slice_max_mb_size limit is reached
    STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_BYTES //<! A slice is closed as soon as the slice_max_byte_size limit is reached
} stm_se_video_multi_slice_mode_t;

static inline const char *StringifySliceMode(stm_se_video_multi_slice_mode_t aSliceMode)
{
    switch (aSliceMode)
    {
        ENTRY(STM_SE_VIDEO_MULTI_SLICE_MODE_SINGLE);
        ENTRY(STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_MB);
        ENTRY(STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_BYTES);
    default: return "<unknown slice mode>";
    }
}

/*!
 * Compound control value used by \ref STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE.
 */
typedef struct stm_se_video_multi_slice_s
{
    stm_se_video_multi_slice_mode_t control;
    uint32_t                        slice_max_mb_size;   //<! Used with ::STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_MB mode
    uint32_t                        slice_max_byte_size; //<! Used with ::STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_BYTES mode
} stm_se_video_multi_slice_t;

/*!
 * Compound control value used, indirectly, by STM_SE_CTRL_VIRTUALIZER.
 */
typedef enum
{
    /*!
     * Control virtualizer as per audio-fw capability with its
     * own default controls.
     */
    STM_SE_VIRTUALIZER_AUTO,
    STM_SE_VIRTUALIZER_ST_OMNISURROUND,
    STM_SE_VIRTUALIZER_SRS_TRUSURROUND_HD,
    STM_SE_VIRTUALIZER_DOLBY_VIRTUAL_SURROUND,
} stm_se_virtualizer_algorithm_t;

/*!
 * Compound control value used by STM_SE_CTRL_VIRTUALIZER.
 */
typedef struct
{
    /*!
     * Type of controlled virtualizer. Defaults to STM_SE_VIRTUALIZER_AUTO.
     */
    stm_se_virtualizer_algorithm_t type;

    union
    {
        struct stm_se_virtualizer_auto_s
        {
            /*!
             * Virtualizer enable/disable.
             */
            uint32_t                 apply;
        } virtualizer_auto;

        struct stm_se_st_omnisurround_s
        {
            /*!
             * Virtualizer enable/disable.
             */
            uint32_t                 apply;

            /*!
             * If TRUE, then the processing is optimized for Headphone output.
             */
            bool                     headphone;

            /*!
             * If TRUE, then the processing produces a dedicated center channel (equivalent MODE30 versus
                 * MODE20).
                 */
            bool                     center_out;
        } st_omnisurround;

        struct stm_se_srs_tru_surround_hd_s
        {
            /*!
             * Virtualizer enable/disable.
             */
            uint32_t                 apply;

            /*!
             * Switch On/Off the virtualizer (TruSurround or Wow).
             *
             * When OFF, a basic down mix is performed.
             */
            bool                     trusurround_hd_enable;

            /*!
             * Gain to apply to the input samples [-9600, 0] mB
             */
            uint32_t                 input_gain;

            /*!
             * Gain to apply to the output samples [-9600, +0] mB
             */
            uint32_t                 output_gain;

            /*!
             * Control of the mix of the centre channel.
             */
            int32_t                  center_control;

            /*!
             * Control of the spatialization effect.
             */
            int32_t                  space_control;

            /*!
             * Control of the mix of the surround channels.
             */
            int32_t                  surround_control;

            /*!
             * Switch On/Off 3D spatialization when applying WowHD.
             */
            bool                     srs_3d_enable;

            /*!
             * Switch On/Off the limiter.
             */
            bool                     limiter_enable;

            /*!
             * Define the limiter strength.
             */
            int32_t                  limiter_control;

            /*!
             * If TRUE, then the processing is optimized for output via headphones.
             */
            bool                     headphone_enable;

            /*!
             * Switch On/Off the HighDefinition processing.
             */
            bool                     definition_enable;

            /*!
             * Switch On/Off the HighDefinition processing on the Center.
             */
            bool                     center_definition_enable;

            /*!
             * Define the strength of the Centre definition.
             */
            int32_t                  center_definition_control;

            /*!
             * Switch On/Off the TruBass module.
             */
            bool                     t_bass_enable;

            /*!
             * If TRUE then bypass the compressor submodule of TruBass.
             */
            bool                     t_bass_compress_bypass;

            /*!
             * Defines the cut-off frequency of the SubWoofer (in Hz).
             */
            uint32_t                 t_bass_speaker_size;

            /*!
             * Defines the strength of the TruBass processing.
             */
            uint32_t                 t_bass_level;

            /*!
             * Enable the SubWoofer output of the TruBass module.
             */
            bool                     t_bass_subwoofer_enable;

            /*!
             * x.
             */
            int32_t                  t_bass_subwoofer_control;

            /*!
             * Enable the Focus module of TSHD.
             */
            bool                     focus_enable;

            /*!
             * Defines the amount of Focus to apply.
             */
            int32_t                  focus_elevation;

            /*!
             * Defines the amount of high frequency Focus to apply.
             */
            int32_t                  focus_tweeter_elevation;

            /*!
             * Defines the strength of the Dialog Clarity enhancer.
             */
            int32_t                  dialog_clarity_control;
        } srs_tru_surround_hd;
    } config;
} stm_se_virtualizer_t;

/*!
 * Compound control value used, indirectly, by STM_SE_CTRL_VOLUME_MANAGER.
 */
typedef enum
{
    /*!
     * Applied leveler as per capability of the audio-fw with its own default params
     */
    STM_SE_VOLUME_LEVELER,
    STM_SE_VOLUME_ST_IVC,
    STM_SE_VOLUME_SRS_TRUVOLUME,
    STM_SE_VOLUME_DOLBY_VOLUME,
} stm_se_volume_algorithm_t;

/*!
 * Compound control value used, indirectly, by STM_SE_CTRL_VOLUME_MANAGER.
 */
typedef enum stm_se_srs_truvolume_mode_e
{
    STM_SE_SRS_TRUVOLUME_OFF,
    STM_SE_SRS_TRUVOLUME_BYPASS,
    STM_SE_SRS_TRUVOLUME_LIGHTm,
    STM_SE_SRS_TRUVOLUME_NORMAL,
    STM_SE_SRS_TRUVOLUME_HEAVY,
} stm_se_srs_truvolume_mode_t;

/*!
 * Compound control value used by STM_SE_CTRL_VOLUME_MANAGER.
 */
typedef struct
{
    /*!
     * Type of leveler. Defaults to STM_SE_VOLUME_LEVELER.
     */
    stm_se_volume_algorithm_t    type;

    union
    {
        struct stm_se_volume_leveler
        {
            /*!
             * Volume leveler enable/disable.
             */
            uint32_t                 apply;

            /*!
             * Amount of leveling applied by the volume leveler within [0,10] default 0
             */
            int32_t                  leveling_amount;

            /*!
             * x.
             */
            int32_t                  output_reference_level;
        } volume_leveler;

        struct stm_se_st_ivc_s
        {
            /*!
             * Volume leveler enable/disable.
             */
            uint32_t                 apply;

            /*!
             * Enable the MDRC module of the IVC process (reuses the MDRC description from ::STM_SE_CTRL_ST_MDRC).
             */
            bool                     mdrc_enable;

            /*!
             * Enable the Loudness module of the IVC process.
             */
            bool                     loudness_enable;

            /*!
             * Controls the speed for loudness normalization within [500, 5000] ms
             */
            uint32_t                 smoothing_time;

            /*!
             * Reference level for normalizing the loudness within [-300, 2400] mB
             */
            int32_t                  reference_level;

            /*!
             * Maximum Gain STIVC can apply to the signal [600, 3000] mB
             */
            uint32_t                 max_gain;

            /*!
             * Enable volume limiter.
             */
            bool                     limiter_enable;

            /*!
             * If TRUE, then measure the RMS instead of peak amplitude to control the limiter.
             */
            bool                     measure_rms;

            /*!
             * Limiter threshold within [-2000, 0] mB
             */
            int32_t                  threshold;
        } st_ivc;

        struct stm_se_dolby_volume_s
        {
            /*!
             * Volume leveler enable/disable.
             */
            uint32_t                 apply;

            /*!
             * Switch on/off the volume leveler.
             */
            bool                     leveler_enable;

            /*!
             * Switch on/off the modeler.
             */
            bool                     modeler_enable;

            /*!
             * Enable/disable Half Mode Volume Modeler (default should be disabled)
             */
            bool                     half_mode_modeler_enable;

            /*!
             *  Enable/Disable midside processing for stereo signals [default : true]
             */
            bool                     msp_enable;

            /*!
             * Amount of leveling applied by the Volume Leveler within [0,10] [default 0]
             */
            int32_t                  leveling_amount;

            /*!
             * The measured reference level of the system set by the system manufacturer.
             *
             * Range : [0, 130] dB SPL, default 0
             */
            int32_t                  output_reference_level;

            /*!
             * Digital volume gain from the system volume control
             *
             * Range : [-9600, +3000] mB , default 0
             */
            int32_t                  digital_reference_level;

            /*!
             * Analog volume gain from the system volume control
                 *
                 * Range : [-9600, +3000] mB , default 0
             */
            int32_t                  analog_reference_level;

            /*!
             * Control enabling end-user to compensate manufacture output_reference_level settings
             *
             * Range : [-9600, +3000] mB , Default 0
             */
            int32_t                  calibration_offset;
        } dolby_volume;

        struct stm_se_srs_truvolume_s
        {
            /*!
             * Volume leveler enable/disable.
             */
            uint32_t                     apply;

            /*!
             * Strength of Leveling
             */
            stm_se_srs_truvolume_mode_t  mode;

            /*!
             * x.
             */
            bool                         noise_manager_enable;

            /*!
             * Gain to apply to the input samples
             *
             * Range : [-9600, +3000] mBGain to apply to the input samples [-9600, +3000] mB
             */
            int32_t                      input_gain;

            /*!
             * Gain to apply to the output samples
             *
             * Range : [-9600, +3000] mB
             */
            int32_t                      output_gain;
        } srs_truvolume;
    } config;
} stm_se_volume_manager_t;

/*!
 * Compound control value used, indirectly, by STM_SE_CTRL_UPMIXER.
 */
typedef enum
{
    STM_SE_UPMIXER_AUTO,
    STM_SE_UPMIXER_ST_CHANNEL_SYNTHESIS,
    STM_SE_UPMIXER_SRS_CIRCLE_SURROUND,
    STM_SE_UPMIXER_DOLBY_PROLOIC,
    STM_SE_UPMIXER_DTS_NEURAL,
    STM_SE_UPMIXER_DTS_NEO,
} stm_se_upmixer_algorithm_t;

/*!
 * Compound control value used by STM_SE_CTRL_UPMIXER.
 */
typedef struct
{
    stm_se_upmixer_algorithm_t      type;

    union
    {
        struct stm_se_upsampler_auto_s
        {
            uint32_t                 apply;
        } upsampler_auto ;

        struct stm_se_srs_circle_surround_s
        {
            uint32_t                 apply;
            bool                     phantom_center;
            bool                     full_bandwidth_center;
            uint32_t                 output_gain;
            int32_t                  center_control;
            int32_t                  space_control;
            int32_t                  surround_control;
            bool                     srs_3d_enable;
            bool                     limiter_enable;
            int32_t                  limiter_control;
            bool                     headphone_enable;
            bool                     definition_enable;
            bool                     center_definition_enable;
            int32_t                  center_definition_control;
            bool                     t_bass_enable;
            bool                     t_bass_compress_bypass;
            uint32_t                 t_bass_speaker_size;
            uint32_t                 t_bass_level;
            bool                     t_bass_subwoofer_enable;
            int32_t                  t_bass_subwoofer_control;
            bool                     focus_enable;
            int32_t                  focus_elevation;
            int32_t                  focus_tweeter_elevation;
            int32_t                  dialog_clarity_control;
        } srs_circle_surround ;

        struct stm_se_dolby_prologic_s
        {
            uint32_t                 apply;
            // TBD
        } dolby_prologic;

        struct stm_se_dts_neural_s
        {
            uint32_t                 apply;
            // TBD
        } dts_neural;
    } config;
} stm_se_ctrl_upmixer_t;

typedef struct
{
    bool capable;
} audio_dec_common_capabilities_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE.
 */
struct stm_se_audio_dec_capability_s
{
    struct audio_dec_pcm_capability_s
    {
        audio_dec_common_capabilities_t common;
        bool                            lpcm_DVDvideo;
        bool                            lpcm_DVDaudio;
        bool                            lpcm_CDDA;
        bool                            lpcm_8bits;
        bool                            ALaw;
        bool                            MuLaw;
    } pcm;

    struct audio_dec_ac3_capability_s
    {
        audio_dec_common_capabilities_t common;
        bool                            DolyDigitalEx;
        bool                            DolyDigitalPlus;
        bool                            DolyDigitalPlus_7_1;
    } ac3;

    struct audio_dec_mp2a_capability_s
    {
        audio_dec_common_capabilities_t common;
        bool                            mp2_stereo_L1;
        bool                            mp2_stereo_L2;
        bool                            mp2_51_L1;
        bool                            mp2_51_L2;
        bool                            mp2_71_L2;
        bool                            mp2_DAB;
    } mp2a;

    struct audio_dec_mp3_capability_s
    {
        audio_dec_common_capabilities_t common;
        bool                            mp3_PRO;
        bool                            mp3_SURROUND;
        bool                            mp3_BINAURAL;
        bool                            mp3_HD;
    } mp3;

    struct audio_dec_dts_capability_s
    {
        audio_dec_common_capabilities_t common;
        bool                            dts_ES;
        bool                            dts_96K;
        bool                            dts_HD;
        bool                            dts_XLL;
    } dts;

    struct audio_dec_wma_capability_s
    {
        audio_dec_common_capabilities_t common;
        bool                            wma_STD;
        bool                            wma_PRO;
        bool                            wma_LOSSLESS;
        bool                            wma_VOICE;
    } wma;

    struct audio_dec_vorbis_capability_s
    {
        audio_dec_common_capabilities_t common;
    } vorbis;

    struct audio_dec_aac_capability_s
    {
        audio_dec_common_capabilities_t common;
        bool                            aac_BSAC;
        bool                            aac_DolbyPulse;
        bool                            aac_SBR;
        bool                            aac_PS;
    } aac;

    struct audio_dec_realaudio_capability_s
    {
        audio_dec_common_capabilities_t common;
        bool                            ra_COOK;
        bool                            ra_LSD;
        bool                            ra_AAC;
        bool                            ra_DEPACK;
    } realaudio;

    struct audio_dec_amr_capability_t
    {
        audio_dec_common_capabilities_t common;
    } amr;

    struct audio_dec_iec61937_capability_s
    {
        audio_dec_common_capabilities_t common;
        bool                            iec61937_DD;
        bool                            iec61937_DTS;
        bool                            iec61937_MPG;
        bool                            iec61937_AAC;
    } iec61937; //spdif_in//hdmi RX

    struct audio_dec_dolby_truehd_capability_s
    {
        audio_dec_common_capabilities_t common;
        bool                            dthd_DVD_Audio;
        bool                            dthd_BD;
        bool                            dthd_MAT;
    } dolby_truehd;

    struct audio_dec_flac_capability_s
    {
        audio_dec_common_capabilities_t common;
    } flac;

    struct audio_dec_sbc_capability_s
    {
        audio_dec_common_capabilities_t common;
    } sbc;

    struct audio_dec_dra_capability_s
    {
        audio_dec_common_capabilities_t common;
    } dra;

    struct audio_dec_g711_capability_s
    {
        audio_dec_common_capabilities_t common;
    } g711;

    struct audio_dec_g729ab_capability_s
    {
        audio_dec_common_capabilities_t common;
    } g729ab;

    struct audio_dec_g726_capability_s
    {
        audio_dec_common_capabilities_t common;
    } g726;

    struct audio_dec_alac_capability_s
    {
        audio_dec_common_capabilities_t common;
    } alac;
};

typedef struct stm_se_audio_dec_capability_s stm_se_audio_dec_capability_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_GET_CAPABILITY_AUDIO_ENCODE.
 */
typedef struct
{
} stm_se_audio_enc_capability_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_GET_CAPABILITY_VIDEO_DECODE.
 */
typedef struct
{
} stm_se_video_dec_capability_t;

/*!
 * Compound control value used by \ref STM_SE_CTRL_GET_CAPABILITY_VIDEO_ENCODE.
 */
typedef struct
{
} stm_se_video_enc_capability_t;

// Control enum partitions that guarantee binary compatibility
// They can be extended but they must not be changed
#define STM_SE_CTRL_PLAYER_MIN        (0<<12)
#define STM_SE_CTRL_ENCODE_BASE       (1<<12)
#define STM_SE_CTRL_VIDEO_ENCODE_BASE (2<<12)
#define STM_SE_CTRL_AUDIO_ENCODE_BASE (3<<12)
#define STM_SE_CTRL_PLAYER_MAX        (16<<12)

/*!
 * This type enumerates the keys that may be used to define controls.
 *
 * controls are sorted by
 * -policy type controls
 *  #sorted by application type (global, player, collator, frameparser, codecs, manifestor, outputs)
 * -simple/compound data type controls
 *  #sorted by domain type
 * -read-only data type controls
 */
typedef enum stm_se_ctrl_e
{
    /*!
     * This policy control allows to optimize the behavior for playback from live sources. In
     * particular it influences that techniques deployed to recover from starvation.
     * (applies to collators, mixer, output timer, output coordinator)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Optimize for media playback
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Optimize for live playback
     */
    STM_SE_CTRL_LIVE_PLAY = STM_SE_CTRL_PLAYER_MIN,

    /*!
     * This policy control allows the caller enable/disable stream synchronization.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY Disable synchronization.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY    Enable synchronization.
     */
    STM_SE_CTRL_ENABLE_SYNC,

    /*!
     * This policy control causes the system to automatically clamp the lower or upper bounds of the play interval on
     * direction change. This causes a request to enter trick mode to be honored more quickly but will freeze
     * the display for a few frames as the pipeline refills.
     * (applies to player)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  .
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     .
     */
    STM_SE_CTRL_CLAMP_PLAY_INTERVAL_ON_DIRECTION_CHANGE,

    /*!
     * This policy control allows the caller to request how buffered data is handled during stream termination.
     * (applies to player)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISCARD   Discard buffered frames.
     * \arg \c ::STM_SE_CTRL_VALUE_PLAYOUT   Play out all buffered frames.
     */
    STM_SE_CTRL_PLAYOUT_ON_TERMINATE,

    /*!
     * This policy control allows the caller to request how buffered data is handled during stream switch.
     * (applies to player)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISCARD   Discard buffered frames.
     * \arg \c ::STM_SE_CTRL_VALUE_PLAYOUT   Play out all buffered frames.
     */
    STM_SE_CTRL_PLAYOUT_ON_SWITCH,

    /*!
     * This policy control allows the caller to request how buffered data is handled during stream drain.
     * (applies to player)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISCARD   Discard buffered frames.
     * \arg \c ::STM_SE_CTRL_VALUE_PLAYOUT   Play out all buffered frames.
     */
    STM_SE_CTRL_PLAYOUT_ON_DRAIN,

    /*!
     * This policy control sets whether the player
     * continues attempts to decode an unplayable play stream.
     * (applies to player)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Stop attempting to decode unplayable streams.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Continue attempts to decode unplayable streams.
     */
    STM_SE_CTRL_IGNORE_STREAM_UNPLAYABLE_CALLS,

    /*!
     * This policy control causes playback-streams linked to specific playbacks to be created
     * *without* allocating decode buffers.
     * The decode buffers of this next play stream will then be provided by user thanks to the
     * following compound controls (::STM_SE_CTRL_DECODE_BUFFERS_USER_ALLOCATED and ::STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS).
     * (applies to playback-streams)
     *
     * \pre This control influences play_stream creation therefore it cannot be meaningfully applied to a play_stream
     * (which must be created before the control can be applied to it). Instead the control must be cascade from
     * the playback that owns the play_stream.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Automatically allocate decode buffers.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Avoid automatic allocation of decode buffers.
     */
    STM_SE_CTRL_DECODE_BUFFERS_USER_ALLOCATED,

    /*!
     * This policy control allows a limiting mechanism for injecting data ahead of time.
     *
     * By limiting the quantity of data in the later parts of the decode pipeline, we decrease the latency need
     * for some actions to be performed. Adopting new PID (for example, switching languages) is an example of
     * this.
     *
     * This is normally desirable (and enabled by default). Operating without throttling allows the decode
     * buffers to comprise part of the buffering for very bursty systems operating with very low memory systems.
     * (applies to collators)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Unlimited injection.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Limit inject ahead.
     */
    STM_SE_CTRL_LIMIT_INJECT_AHEAD,

    /*!
     * This policy control is used to enable detection of information transmitted through data interface.
     *
     * \warning internal
     * \warning Stability: Unstable
     * (applies to collator2)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  .
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     .
     */
    STM_SE_CTRL_ENABLE_CONTROL_DATA_DETECTION,

    /*!
     * This policy control allows the caller to reduce the amount of collated data to the
     * minimum for a correct decoding. Thereby, the play out of all buffered frames will be faster.
     * (applies to collator2)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Use normal amount of collated data.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Use the smallest amount of collated data.
     */
    STM_SE_CTRL_REDUCE_COLLATED_DATA,

    /*!
     * This policy control allows reversible mode in collator2
     * (applies to collator2)
     *
     * \warning internal
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY .
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY    .
     */
    STM_SE_CTRL_OPERATE_COLLATOR2_IN_REVERSIBLE_MODE,

    /*!
     * This policy control allows the caller to request the decoder to ignore the progressive frame flag in an MPEG2
     * picture coding extension header.
     *
     * Some broadcasters are unable to reliably set this flag, causing interlaced frames to be incorrectly
     * treated as progressive. Applying this control allows streams from these broadcasters to render correctly.
     *
     * This control is harmful to streams that depend on progressive frame flag being honored. This includes
     * many DVD and VCD streams and any stream deploying 3:2 pulldown.
     * (applies to frameparser mpeg2)
     *
     * \pre Underlying MPEG2 must be known to have an unreliable progressive frame flag.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Standards conformant behavior.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Workaround exceptional streams.
     */
    STM_SE_CTRL_MPEG2_IGNORE_PROGRESSIVE_FRAME_FLAG,

    /*!
     * This policy control specifies the application for MPEG2 decoding purposes. This affects the default values for
     * color matrices.
     * (applies to frameparser mpeg2)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_MPEG2_APPLICATION_MPEG2  Use MPEG2 color matrices.
     * \arg \c ::STM_SE_CTRL_VALUE_MPEG2_APPLICATION_ATSC   Use ATSC color matrices.
     * \arg \c ::STM_SE_CTRL_VALUE_MPEG2_APPLICATION_DVB    Use DVB color matrices.
     */
    STM_SE_CTRL_MPEG2_APPLICATION_TYPE,

    /*!
     * Read-only playback-streams control to get the last reported time_code information
     * from the GOP header of MPEG2 streams.
     *
     * The read-only \ref compound_controls "compound" value uses \ref stm_se_ctrl_mpeg2_time_code_t.
     */
    STM_SE_CTRL_MPEG2_TIME_CODE,

    /*!
     * This policy control requests a workaround for special H.264 streams.
     * (applies to frameparsers h264/hevc)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Disable workaround logic.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Enable workaround logic.
     */
    STM_SE_CTRL_H264_TREAT_DUPLICATE_DPB_VALUES_AS_NON_REF_FRAME_FIRST,

    /*!
     * This policy control requests workaround for a special H.264 streams.
     * (applies to frameparser h264)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Disable workaround logic.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Enable workaround logic.
     */
    STM_SE_CTRL_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING,

    /*!
     * This policy control requests a workaround for special H.264 streams.
     * (applies to frameparsers h264/hevc)
     *
     * \note For transport streams, this is preferable to
     *       ::STM_SE_CTRL_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Disable workaround logic.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Enable workaround logic.
     */
    STM_SE_CTRL_H264_VALIDATE_DPB_VALUES_AGAINST_PTS_VALUES,

    /*!
     * This policy control requests a workaround for special H.264 streams.
     * (applies to frameparser h264)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Disable workaround logic.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Enable workaround logic.
     */
    STM_SE_CTRL_H264_TREAT_TOP_BOTTOM_PICTURE_AS_INTERLACED,

    /*!
     * This policy control allows the caller to enable support for certain unusual H.264 streams.
     * (applies to frameparser h264)
     *
     * The H.264 standard requires streams to contain IDR frames as re-synchronization points and jumps will not
     * resynchronize until IDR to guarantee reference frame integrity.
     *
     * Unfortunately, some broadcast streams do not incorporate IDRs. This control allows such streams to be
     * reproduced by using I frames to indicate re-synchronization points.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Resynchronize only on IDR frames.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Resynchronize on I frames.
     */
    STM_SE_CTRL_H264_ALLOW_NON_IDR_RESYNCHRONIZATION,

    /*!
     * This policy control allows to choose which information source is
     * most trusted to determine the frame rate of the stream.
     * STM_SE_CTRL_VALUE_PRECEDENCE_CONTAINER_PTS_STREAM_DEFAULT shall bet set for any media container playback (.avi, .mkv).
     * STM_SE_CTRL_VALUE_PRECEDENCE_STREAM_PTS_CONTAINER_DEFAULT shall be set far any live broadcast feeding.
     * (applies to frameparser video)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_PRECEDENCE_STREAM_PTS_CONTAINER_DEFAULT. Extraction from elementary stream, then from PTS, then from container.
     * \arg \c ::STM_SE_CTRL_VALUE_PRECEDENCE_STREAM_CONTAINER_PTS_DEFAULT. Extraction from elementary stream, then from container, then from PTS.
     * \arg \c ::STM_SE_CTRL_VALUE_PRECEDENCE_PTS_STREAM_CONTAINER_DEFAULT. Extraction from PTS, then from elementary stream, then from container.
     * \arg \c ::STM_SE_CTRL_VALUE_PRECEDENCE_PTS_CONTAINER_STREAM_DEFAULT. Extraction from PTS, then from container, then from elementary stream.
     * \arg \c ::STM_SE_CTRL_VALUE_PRECEDENCE_CONTAINER_PTS_STREAM_DEFAULT. Extraction from container, then from PTS, then from elementary stream.
     * \arg \c ::STM_SE_CTRL_VALUE_PRECEDENCE_CONTAINER_STREAM_PTS_DEFAULT. Extraction from container, then from elementary stream, then from PTS.
     *
     * \todo fix typo on name
     */
    STM_SE_CTRL_FRAME_RATE_CALCULUATION_PRECEDENCE,

    /*!
     * This policy control allows to set container framerate of the stream.
     *
     * By default, the stream is played at native framerate.
     *
     * The stream will be played with container framerate according to
     * framerate calculation precedence.
     *
     * The following control values are permitted:
     *
     * integer 'value' sets the framerate in fps and must be in
     * the range 7..120.
     *
     */
    STM_SE_CTRL_CONTAINER_FRAMERATE,

    /*!
     * This policy control allows the caller to request additional effort be applied to decode frames that are known to be partly
     * damaged.
     * (applies to H264 video codec only)
     *
     * \warning Stability: Temporary.
     * \note This control may be replaced with an alternative that is not codec specific.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Discard badly pre-processed frames.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Decode badly pre-processed frames.
     */
    STM_SE_CTRL_H264_ALLOW_BAD_PREPROCESSED_FRAMES,

    /*!
     * This policy control allows the caller to request additional effort be applied to decode
     * frames that are known to be partly damaged.
     * (applies to HEVC video codec only)
     *
     * \warning Stability: Temporary.
     * \note This control may be replaced with an alternative that is not codec specific.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Discard badly pre-processed frames.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Decode badly pre-processed frames.
     */
    STM_SE_CTRL_HEVC_ALLOW_BAD_PREPROCESSED_FRAMES,

    /*!
     * This policy control allows to set the maximum usable memory size for
     * video decoders (applies to codecs video h264/hevc)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_DECODE_HD_PROFILE
     *             Optimized buffer allocation for HD streams (1920x1088)
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_DECODE_HD_LEGACY_PROFILE
     *             Max buffer allocation for HD streams (1920x1088)
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_DECODE_4K2K_PROFILE
     *             Max buffer allocation for 4K2K streams (4096x2400)
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_DECODE_UHD_PROFILE
     *             Max buffer allocation for UHD streams (3840x2160)
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_DECODE_SD_PROFILE
     *             Max buffer allocation for SD streams (720x576)
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_DECODE_720P_PROFILE
     *             Max buffer allocation for 720p streams (1280x720)
     */
    STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE,

    /*!
     * This policy control sets the slice based video error concealment algorithm
     * This algorithm either emulates an entire slice or emulates the missing ending of the slice from the discovered corruption point
     * (applies to codec video h264/mpeg2/hevc)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_ERROR_DECODING_LEVEL_NORMAL  Emulates the entire slice
     * \arg \c ::STM_SE_CTRL_VALUE_ERROR_DECODING_LEVEL_MAXIMUM Emulates the missing ending of the slice
     */
    STM_SE_CTRL_ERROR_DECODING_LEVEL,

    /*!
     * This policy control specifies the application for audio decoding and rendering  purposes.
     * also used an audio generator control.
     *
     * This affects the default values of other controls. This control is of particular benefit to application
     * authors seeking system certification for specific audio applications.
     *
     * The control can only be overridden by setting the "APPLICATION TYPE" of the object to fine-tune to
     * AUDIO_APPLICATION_ISO and then specifying the individual controls accordingly.
     * (applies to codecs audio)
     *
     * The controls affected include:
     *  - Compression mode
     *  - DRC
     *  - Dialog normalization
     *  - Preferred down mix equations
     *  - Analog output policies
     *  - Digital output policies
     *  - Mixing metadata
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_APPLICATION_ISO
     *             Decode files according to ISO standard (don't look after DVB or Dolby specific metadata).
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVD
     *             Decode files according to DVD playback profile.
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVB
     *             Decode files according to DVB guidelines.
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS10
     *             Decode files according to Dolby MS10 certification requirements (stereo analog out, transcode
     *             of primary)
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS11
     *             Decode files according to Dolby MS11 certification requirements (5.1 analog out , transcode
     *             of mixer output)
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS12
     *             Decode files according to Dolby MS12 certification requirements (7.1 analog out ...)
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_APPLICATION_HDMIRX_GAME_MODE
     *             To achieve the low-latency tune the audio-playstream according HDMIRx usecase.
     */
    STM_SE_CTRL_AUDIO_APPLICATION_TYPE,

    /*!
     * This policy control qualifies the type of service that is parsed and decoded by the play-stream. If the stream
     * carries two substreams (DD+ case) and if the application wants the second substream (audio-description
     * service) to be decoded, then the application sets the service as
     * ::STM_SE_CTRL_VALUE_AUDIO_SERVICE_MAIN_AND_AUDIO_DESCRIPTION.
     *
     * Both primary (BD-ROM) and main (DVB) service types are treated by the sync logic as candidates to be the
     * sync master. If the mixer topology results in ambiguity (i.e. there is more than one candidate to be sync
     * be master) then ::STM_SE_CTRL_AUDIO_MIXER_ROLE can be used to resolve this.
     *
     * For streams where the service type is unknown, the value ::STM_SE_CTRL_VALUE_AUDIO_SERVICE_PRIMARY should be
     * used.
     * (applies to codecs audio)
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_SERVICE_PRIMARY
     *             This audio stream is the primary stream of a bluray disc
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_SERVICE_SECONDARY
     *             This audio stream is the secondary stream of a bluray disc
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_SERVICE_MAIN
     *             This audio stream is the main audio of a DVB program
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_SERVICE_AUDIO_DESCRIPTION
     *             This audio stream is an audio-description service of a DVB program
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_SERVICE_MAIN_AND_AUDIO_DESCRIPTION
     *             This audio stream of a DVB program carries two substreams: a main and its associated
     *             audio-description; both are decoded.
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_SERVICE_CLEAN_AUDIO
     *             This primary audio stream of a DVB program carries clean-audio information
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_SERVICE_AUDIO_GENERATOR
     *             The linearizer input of the mixer carries Audio Generator input.
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_SERVICE_INTERACTIVE_AUDIO
     *             The linearizer input carries interactive audio which in itself can carry 8 independent mono
     *             streams of same sampling frequency
     */
    STM_SE_CTRL_AUDIO_SERVICE_TYPE,

    /*!
     * This policy control hints to the streaming engine objects about region specific defaults to apply.
     * (applies to codec audio aac)
     *
     * Examples:
     *   - Audio-Program-Reference-Level in case of NORDIG versus DVB
     *   - Stereo-Downmix specific equations in case of ARIB
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_REGION_UNDEFINED
     * \arg \c ::STM_SE_CTRL_VALUE_REGION_ATSC
     * \arg \c ::STM_SE_CTRL_VALUE_REGION_DVB
     * \arg \c ::STM_SE_CTRL_VALUE_REGION_NORDIG
     * \arg \c ::STM_SE_CTRL_VALUE_REGION_DTG
     * \arg \c ::STM_SE_CTRL_VALUE_REGION_ARIB
     * \arg \c ::STM_SE_CTRL_VALUE_REGION_DTMB
     */
    STM_SE_CTRL_REGION,

    /*!
     * This policy control sets the reference program level for audio playback-streams
     * (applies to codecs audio)
     *
     * The integer control value sets the program level in millibels and must be in
     * the range -3100..0.
     * Zero is treated as "use region specific default", typically -3100.
     */
    STM_SE_CTRL_AUDIO_PROGRAM_REFERENCE_LEVEL,

    /*!
     * This policy control allows to specify audio substream id
     *
     * When the audio stream carries multiple independent subtreams (e.g. DD+ stream is main and
     * audio-description or multilingual program), if the application wants a substream to be decoded, then the
     * application specifies the ID of this independent substream.
     *
     * \warning Stability: Unstable
     *
     * The integer control value sets the substream index and must be in the range 0..31.
     */
    STM_SE_CTRL_AUDIO_SUBSTREAM_ID,

    /*!
     * This policy control allows the substitution of a corrupted reference picture within the reference picture list
     * by an existing and valid reference picture which is temporally close from it.
     * (applies to codecs video)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY Forbids corrupted reference picture substitution.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY    Allows corrupted reference picture substitution.
     */
    STM_SE_CTRL_ALLOW_REFERENCE_FRAME_SUBSTITUTION,

    /*!
     * This policy control sets a threshold value preventing a badly decoded reference picture from being used as reference for the current picture's decoding.
     * Any picture which has a quality index under this threshold value is dropped from the reference picture list applied to the current picture's decoding.
     * Substitution control, if applied, prevents from having some pictures missing from the reference picture list.
     * If reference pictures are missing from the reference picture list, any picture depending on them are not decoded.
     * (applies to codecs video mpeg2/h264/hevc)
     *
     * This integer control value sets between 0 and 255.
     * \arg \c ::0   All pictures are kept if the reference picture list
     * \arg \c ::255 Any picture not having a full quality reliability is dropped from the reference list, and may be replaced by a picture having a full quality index reliability.
     *
     */
    STM_SE_CTRL_DISCARD_FOR_REFERENCE_QUALITY_THRESHOLD,

    /*!
     * This policy control sets a threshold value preventing a badly decoded picture from being manifested.
     * Any picture which has a quality index under this threshold value is not manifested.
     * (applies to codecs video mpeg2/h264/hevc)
     *
     * This integer control value sets between 0 and 255.
     * \arg \c ::0   Any picture is manifested, whatever its quality.
     * \arg \c ::64  provides a good fluency but many corrupted frame are displayed
     * \arg \c ::192 prevent most of the corrupted pictures from being displayed and lead to a jerky display
     * \arg \c ::255 Any picture not having a full quality reliability is not manifested.
     *
     */
    STM_SE_CTRL_DISCARD_FOR_MANIFESTATION_QUALITY_THRESHOLD,

    /*!
     * This policy control requests that the decoder issue decimated buffers in addition to full frame buffers.
     *
     * Decimated buffers are used in PiP deployments to reduce memory bandwidth required by the display
     * subsystem.
     * (applies to codecs and manifestors video)
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_DISABLED
     *             No decimation.
     * \arg \c ::STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_HALF
     *             Decimate to half original size (quarter of original bandwidth).
     * \arg \c ::STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_QUARTER
     *             Decimate to a quarter of the original size (eight fold reduction in bandwidth, 1/4 in width and 1/2 in height).
     */
    STM_SE_CTRL_DECIMATE_DECODER_OUTPUT,

    /*!
     * This policy control allows the caller to request a still frame be put on display as soon as it is decoded to
     * offer a preview while waiting for normal decoding to start.
     * (applies to manifestors)
     *
     * This is most appropriate for very highly buffered (high latency) systems, such as an IP set-top box
     * connected to a network with substantial jitter.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY Show the first frame at its normal display time.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY    Show the first frame as soon as it is decoded.
     */
    STM_SE_CTRL_DISPLAY_FIRST_FRAME_EARLY,

    /*!
     * This policy control allows the caller to request special termination characteristics for video play streams.
     * (applies to manifestor video stmfb)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_LEAVE_LAST_FRAME_ON_SCREEN Keep final frame on display during stream shutdown.
     * \arg \c ::STM_SE_CTRL_VALUE_BLANK_SCREEN Blank display during stream shutdown.
     */
    STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR,

    /*!
     * This policy control selects the clock to be used to master mappings between system time and playback time.
     *
     * The system clock may be either a free running operating system clock or a recovered clock, depending upon
     * the media transmission system.
     * (applies to output timer and coordinator)
     *
     * \note Each playback has a unique system clock recovered from the PCR values delivered by the demux
     *       attached to it. Where the demux is not directly attached to the playback the PCR values must
     *       be injected using stm_se_playback_inject_clock_data_point(), otherwise it the playback's clock
     *       will free run.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_CLOCK_MASTER  Use (primary) video stream as master.
     * \arg \c ::STM_SE_CTRL_VALUE_AUDIO_CLOCK_MASTER  Use (primary) audio stream as master.
     * \arg \c ::STM_SE_CTRL_VALUE_SYSTEM_CLOCK_MASTER Use a system clock as master.
     */
    STM_SE_CTRL_MASTER_CLOCK,

    /*!
     * This policy control allows the caller to impose an externally specified mapping between system time and playback
     * time. This is used in real-time capture and playback systems to ensure fixed input to output latencies.
     * (applies to output timer and coordinator)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY Use normal heuristics to select the time mapping.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY    Use an externally imposed time mapping.
     */
    STM_SE_CTRL_EXTERNAL_TIME_MAPPING,

    /*!
     * This policy control allows the caller to request the use of highly aggressive synchronization actions to achieve
     * the requested external time mapping.
     * (applies to output timer and coordinator)
     *
     * In particular, when this control is applied, the system may re-time the external vertical sync to create
     * a fixed relationship between incoming and outgoing vertical syncs. This will cause the connected display
     * device to glitch during initial synchronization and is therefore normally disabled.
     *
     * It is appropriate for use in very low latency modes of operation, such as a mode optimized for
     * interactive games where local control signals are sent to a server that replies with a compressed stream.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY Use standard synchronization mechanisms.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY    Permit the use of aggressive synchronization mechanisms.
     */
    STM_SE_CTRL_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED,

    /*!
     * This policy control allows the caller to force the startup synchronization to work on the basis of starting at
     * the first video frame and letting audio join in appropriately.
     *
     * This has particular effect in some transport streams where there may be a lead in time of up to one
     * second of audio.
     * (applies to output timer and coordinator)
     *
     * \pre ::STM_SE_CTRL_DISCARD_LATE_FRAMES must be configured to discard late frames.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Never start video early.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Start the video stream(s) as soon as frames are available.
     */
    STM_SE_CTRL_VIDEO_START_IMMEDIATE,

    /*!
     * This policy control sets some workarounds for odd behaviours, especially with
     * regard to time, that are typical of packet injectors.
     * (applies to output timer and coordinator)
     *
     * \warning internal This control should never be used in production
     *                   environments and will be replaced by automated heuristics.
     */
    STM_SE_CTRL_PACKET_INJECTOR,

    /*!
     * This policy control causes only key frames to be decoded.
     * (applies to output timer)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY
     *
     * \deprecated Replaced by ::STM_SE_CTRL_VIDEO_DISCARD_FRAMES with value ::STM_SE_CTRL_VALUE_KEY_FRAMES_ONLY
     */
    STM_SE_CTRL_VIDEO_KEY_FRAMES_ONLY,

    /*!
     * This policy control allows the caller to restrict the decode to reference or key frames only. This causes
     * identical behavior to some of the ::STM_SE_CTRL_TRICK_MODE_DOMAIN control values, but can be applied even
     * when the playback speed is x1.
     * (applies to output timer)
     *
     * This is typically used for key or reference frames based trick modes under application control
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_NO_DISCARD            All the frames are decoded
     * \arg \c ::STM_SE_CTRL_VALUE_REFERENCE_FRAMES_ONLY The non reference frames are not decoded
     * \arg \c ::STM_SE_CTRL_VALUE_KEY_FRAMES_ONLY       Only the key frames are decoded.
     */
    STM_SE_CTRL_VIDEO_DISCARD_FRAMES,

    /*!
     * This policy control allows the caller to restrict the decode to a single group of pictures (GOP).
     *
     * This is typically used for trick modes under application control when the container is not indexed (or
     * indexed badly). At allows the application to over-inject data without the additional data causing
     * additional unwanted pictures to be displayed. Usually, this control is combined with
     * ::STM_SE_CTRL_VIDEO_DISCARD_FRAMES and ::STM_SE_CTRL_VALUE_KEY_FRAMES_ONLY.
     * (applies to output timer)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Normal playback.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Playback a single GOP and stall until the next discontinuity.
     *
     * \warning Stability: Unstable
     */
    STM_SE_CTRL_VIDEO_SINGLE_GOP_UNTIL_NEXT_DISCONTINUITY,

    /*!
     * This policy control selects how the player should cope with late decodes.
     * (applies to output timer)
     *
     * When performing media playback from locally controlled data sources (such as the hard disc), discarding
     * frames may be harmful; it is better to keep them and require the player to reestablish the time mapping.
     *
     * For broadcast sources, discarding frames makes it possible to quickly realign with the broadcaster.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_NEVER
     *             Never discard late frames.
     * \arg \c ::STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_ALWAYS
     *             Discard all late frames.
     * \arg \c ::STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_AFTER_SYNCHRONIZE
     *             Discard late frames only after all synchronization actions have been performed (no early discard).
     */
    STM_SE_CTRL_DISCARD_LATE_FRAMES,

    /*!
     * This policy control allows the caller to enable restablishment of the time mapping when data is delivered late.
     *
     * Enabling this control provides a good user experience during recovery from hardware error conditions,
     * such as scratched optical media.
     * (applies to output timer)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Avoid recalculating the time mapping.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Recalculate the time mapping when data is delivered late.
     */
    STM_SE_CTRL_REBASE_ON_DATA_DELIVERY_LATE,

    /*!
     * This policy control allows the caller to enable restablishment of the time mapping when a frame decode occurs
     * after the last opportunity to queue it for display.
     * (applies to output timer)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Avoid recalculating the time mapping.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Recalculate the time mapping when decodes complete late.
     */
    STM_SE_CTRL_ALLOW_REBASE_AFTER_LATE_DECODE,

    /*!
     * This policy control selects the techniques applied to reduce the decode bandwidth during trick mode playback.
     * (applies to output timer)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_TRICK_MODE_AUTO
     *             Automatic discard and degrade management.
     * \arg \c ::STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL
     *             Decode everything at full quality.
     * \arg \c ::STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES
     *             Allow degraded decode of non-reference frames (reducing decoder bandwidth).
     * \arg \c ::STM_SE_CTRL_VALUE_TRICK_MODE_DISCARD_NON_REFERENCE_FRAMES
     *             Discard non-reference frames.
     * \arg \c ::STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES
     *             Allow degraded decode.
     * \arg \c ::STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_KEY_FRAMES
     *             Decode only key frames.
     * \arg \c ::STM_SE_CTRL_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES
     *             Decode key frames with large jumps between the frames displayed.
     */
    STM_SE_CTRL_TRICK_MODE_DOMAIN,

    /*!
     * This policy control allows the selective discard of B frames during normal playback if the decoder cannot
     * maintain real-time performance.
     * (applies to output timer)
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Forbid B frame discard.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Permit B frame discard.
     */
    STM_SE_CTRL_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED,

    /*!
     * This policy control allows the caller to specify the maximum rate at which the clock rates for the rendering
     * hardware are adjusted.
     *
     * This control is used to constrain the clock pulling software to particular appropriate limits.
     * (applies to output coordinator)
     *
     * \note This control is intended to facilitate debugging when the clock recovery system is failing. It can
     *       be used to prevent the output hardware from losing sync on the signal issued by the system. There should
     *       be no other need to manipulate this control.
     *
     * The following control values are permitted:
     *
     * integer 'value' sets the limit to 2^value (or 1 << value) parts per million.
     */
    STM_SE_CTRL_CLOCK_RATE_ADJUSTMENT_LIMIT,

    /*!
     * This policy control allows that short backward jumps be handled the same way as short forward jumps.
     *
     * When a jump is detected, the system may perform some automatic activity. In particular, when operating as
     * a media player then the relationship between the system time and playback time may be reestablished.
     *
     * Normally, any backwards step in playback time results in a jump being detected and jump activity
     * performed. This control can be used to suppress this for short backwards jumps.
     * (applies to output coordinator)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Asymmetric jump detection.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Symmetric jump detection.
     */
    STM_SE_CTRL_SYMMETRIC_JUMP_DETECTION,

    /*!
     * This policy control sets the forward jump threshold for the PTS jump detector.
     * (applies to output coordinator)
     *
     * When a jump is detected, the system may perform some automatic activity. In particular, when operating as
     * a media player then the relationship between the system time and playback time may be reestablished.
     *
     * For example, if the playback time jumps forwards by one second then the system may either:
     *
     * 1. Frame repeat (or mute for audio) for one second until the time for the
     *    next frame is reached
     * 2. Rebase time mapping, effectively jumping the playback time forwards by
     *    one second
     *
     * The control sets the threshold at which a discontinuity in playback times will be treated as a jump
     * (rather than as stream corruption).
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * integer 'value' sets the limit to 2^value (or 1 << value) seconds.
     */
    STM_SE_CTRL_SYMMETRIC_PTS_FORWARD_JUMP_DETECTION_THRESHOLD,

    /*!
     * This policy control allows to increase the level of buffering for all streams within a playback allowing the
     * playback to tolerate periods of starvation. In particular this allows the playback to tolerate network
     * jitter in IP set-top-box applications.
     * (applies to output coordinator)
     *
     * For an IP network with a maximum specified jitter of 300ms then this control should be set to 300,
     *
     * The following control values are permitted:
     *
     * integer 'value' sets the additional latency to be added to the presentation time before
     * choosing a render time (in ms).
     */
    STM_SE_CTRL_PLAYBACK_LATENCY,

    /*!
     * This policy control allows to set mode of HDMIRx.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_HDMIRX_DISABLED
     *             HDMIRx disabled in this case.
     * \arg \c ::STM_SE_CTRL_VALUE_HDMIRX_REPEATER
     *             HDMIRx in repeater mode in this case.
     * \arg \c ::STM_SE_CTRL_VALUE_HDMIRX_NON_REPEATER
     *             HDMIRx in non repeater mode in this case.
    */
    STM_SE_CTRL_HDMI_RX_MODE,

    /*!
     * This policy control allows to signal devlog is running
     *
     * \warning internal
     * (applies to collators, output coordinator)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  .
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     .
     */
    STM_SE_CTRL_RUNNING_DEVLOG,

    /*!
     * This policy control allows to tune trick mode for audio
     *
     * \warning This control is not implemented
     * \warning internal
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_TRICK_AUDIO_MUTE             .
     * \arg \c ::STM_SE_CTRL_VALUE_TRICK_AUDIO_PITCH_CORRECTED  .
     */
    STM_SE_CTRL_TRICK_MODE_AUDIO,

    /*!
     * This policy control allows to play 24fps video at 25fps
     *
     * \warning This control is not implemented
     * \warning internal
     */
    STM_SE_CTRL_PLAY_24FPS_VIDEO_AT_25FPS,

    /*!
     * This policy control allows the correct handling of streams with incorrectly described frame rates.
     *
     * \warning This control is not implemented
     * \warning internal
     */
    STM_SE_CTRL_USE_PTS_DEDUCED_DEFAULT_FRAME_RATES,

    /*!
     * This playback-streams compound control provides a means for the caller to provide the play stream
     * with a list of decode buffers.
     *
     * In the current implementation this control can be set only once for a given play_stream, meaning the
     * whole collection of buffers which form the decode pool must be sent in one operation.
     *
     * Note also, that any attempt to get this control before it has been set will fail; it cannot be used to
     * gain access to buffers allocated by the streaming engine itself.
     *
     * \pre The policy control ::STM_SE_CTRL_DECODE_BUFFERS_USER_ALLOCATED must have been applied during play_stream
     *      creation.
     *
     * \warning Stability: Unstable
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_play_stream_decoded_frame_buffer_info_t.
     */
    STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS,

    /*!
     * This playback-streams compound control is a read only compound control to get the minimal number of
     * decode buffers required to correctly decode the play_stream encoding type.
     *
     * \pre The policy control ::STM_SE_CTRL_DECODE_BUFFERS_USER_ALLOCATED must be been applied during play_stream
     *      creation.
     *
     * \warning Stability: Unstable
     *
     * The read-only \ref compound_controls "compound" value uses \ref stm_se_play_stream_decoded_frame_buffer_info_t.
     */
    STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS,

    /*!
     * Read-only control to get the video decode capabilities of streaming engine.
     * \warning This control is not implemented
     *
     * The read-only \ref compound_controls "compound" value uses \ref stm_se_video_dec_capability_t.
     * This control only as an input of stm_se_get_compound_control STKPI.
     */
    STM_SE_CTRL_GET_CAPABILITY_VIDEO_DECODE,

    /*!
     * This playback-streams audio codec compound control defines what type of MPEG4 AAC streams
     * the decoder should handle and how to handle it.
     *
     * \warning Stability: Unstable
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_mpeg4aac_t.
     */
    STM_SE_CTRL_AAC_DECODER_CONFIG,

    /*!
     * This playback-streams audio codec control defines the number of channels carried in the raw audio stream.
     *
     * \warning Stability: Temporary. The following controls may be merged:
     *          ::STM_SE_CTRL_PLAY_STREAM_NBCHANNELS, and
     *          ::STM_SE_CTRL_PLAY_STREAM_SAMPLING_FREQUENCY.
     */
    STM_SE_CTRL_PLAY_STREAM_NBCHANNELS,

    /*!
     * This playback-streams audio codec control defines the sampling frequency of the raw audio stream.
     *
     * \warning Stability: Temporary. The following controls may be merged:
     *          ::STM_SE_CTRL_PLAY_STREAM_NBCHANNELS, and
     *          ::STM_SE_CTRL_PLAY_STREAM_SAMPLING_FREQUENCY.
     */
    STM_SE_CTRL_PLAY_STREAM_SAMPLING_FREQUENCY,

    /*!
     * Read-Only playback-streams compound control For getting elementary buffer level
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_ctrl_play_stream_elementary_buffer_level_t.
     */
    STM_SE_CTRL_PLAY_STREAM_ELEMENTARY_BUFFER_LEVEL,

    /*!
     * This playback-streams audio codec control defines the mute state of the raw audio stream.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY
     */
    STM_SE_CTRL_PLAY_STREAM_MUTE,

    /*!
     * This audio generator compound control describes a circular buffer shared
     * between the application (write) and the streaming engine (read and flow control).
     *
     * This function is guaranteed to support little endian 16-bit, mono, or stereo streams at 32, 44.1 or 48
     * kHz. Support for other formats is implementation defined. Lack of support is indicated by -ERANGE when
     * setting the control.
     *
     * The circular buffer must be at least 3072 samples long if it is to work with all possible mixer
     * configurations. If the circular buffer is too small for a specific mixer to operate correctly, the audio
     * generator's attach function will report an error on connection to the mixer.
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_audio_generator_buffer_t .
     */
    STM_SE_CTRL_AUDIO_GENERATOR_BUFFER,

    /*!
     * This audio generator compound control describes the actual audio-content carried in the defined buffer,
     * namely its sampling frequency and channel-placement (from which the number of interleaved channels is derived).
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_audio_core_format_t .
     */
    STM_SE_CTRL_AUDIO_INPUT_FORMAT,

    /*!
     * This audio generator compound control describes the emphasis if any applied to the audio-content
     * carried in the defined buffer.
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_emphasis_type_t and
     * defaults to \ref STM_SE_NO_EMPHASIS.
     */
    STM_SE_CTRL_AUDIO_INPUT_EMPHASIS,

    /*!
     * This audio player, mixer, encoder preproc control defines whether to restrict the application of dual-mono user-setting
     * to only dual-mono;
     * also used as a policy control
     * (applies to codecs audio)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_USER_ENFORCED_DUALMONO ::STM_SE_CTRL_DUALMONO is applied to any stream
     * \arg \c ::STM_SE_CTRL_VALUE_STREAM_DRIVEN_DUALMONO ::STM_SE_CTRL_DUALMONO is applied only to dual mono stream (MODE1p1)
     */
    STM_SE_CTRL_STREAM_DRIVEN_DUALMONO,

    /*!
     * This audio player, mixer, encoder preproc control specifies what channel to render on L and R when stream is a dual mono (MODE1p1).
     * It would also apply to stereo (downmixed) streams if ::STM_SE_CTRL_STREAM_DRIVEN_DUALMONO is not applied;
     * also used as a policy control
     * (applies to codecs audio)
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_STEREO_OUT
     * \arg \c ::STM_SE_DUAL_LEFT_OUT
     * \arg \c ::STM_SE_DUAL_RIGHT_OUT
     * \arg \c ::STM_SE_MONO_OUT
     */
    STM_SE_CTRL_DUALMONO,

    /*!
     * This playback-streams, audio codec, mixer compound control tunes the Dynamic Range Control and
     * DialogNormalisation algorithm to use.
     * This control is effective only with codecs supporting DRC and Dialog Normalisation metadata and
     * through pcm-processing managing DRC like DolbyDigital ReEncoder (DDRE) module.
     *
     * If set to ::STM_SE_NO_COMPRESSION, then DRC and DialogNormalisation is disabled.
     *
     * Because DD and DD+ are optimized to provide non-compromised audio for two different output sets, the port
     * is used to specify the DRC control of both outputs. The same applies to DDRE as a post-processing of
     * audio-mixer object.
     *
     * For audio-player objects, these would generally control a single port (MAIN) unless the audio-player is
     * multiplexing 5.1 or 7.1 multichannel and stereo outputs on a respectively 8 or 10 channel I2S output.
     *
     * \pre Applies only to a play_stream object or a mixer object.
     *
     * \warning Stability: Unstable
     *
     * The \ref banked_compound_controls "banked compound" value uses \ref stm_se_drc_t.
     */
    STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION,

    /*!
     * This playback-streams and audio mixer control defines whether to apply the stereo-downmix recommended
     * in the decoded stream.
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Stereo downmix recommended in decoded stream is not applied
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Stereo downmix recommended in decoded stream is applied
     */
    STM_SE_CTRL_STREAM_DRIVEN_STEREO,

    /*!
     * This audio mixer control directs the mixer to operate with a defined grain.
     * This impacts the latency of the mixer.
     *
     * By default, the grain is set to 768 samples (16 ms @ 48 kHz).
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * integer 'value' sets the grain size and must be in the range 512..1536 and quantized
     * to 256 sample boundaries.
     */
    STM_SE_CTRL_AUDIO_MIXER_GRAIN,

    /*!
     * This audio mixer coumpound control tunes the gains or panning applied to each input channel of the master mixer
     * (when output is the handle of the mixer object) or of the postprocessing mixer when the output is the handle of an
     * audio-player object.
     *
     * Output handler is set to NULL instructing the mixer to apply the same set of coefficients on the named
     * input for all outputs.
     *
     * \warning Stability: Unstable
     *
     * The \ref banked_compound_controls "banked compound" value uses \ref stm_se_q3dot13_input_gain_t.
     */
    STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENT,

    /*!
     * This audio mixer coumpound control tunes the gains to be applied to each output channel of the mixer.
     * When the given output is the mixer object, this gain is applied to the output of the master mixer.
     * When the given output is the handle of an audio-player, the gain is applied to the output of the
     * postprocessing mixer that drives this audio-player.
     *
     * \pre If the given output is the handle of an audio-player object, the mixer object will be attached to this
     * player.
     *
     * \warning Stability: Unstable
     *
     * The \ref banked_compound_controls "banked compound" value uses \ref stm_se_q3dot13_output_gain_t.
     */
    STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN,

    /*!
     * This playback-streams compound control allows deemphasis
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  .
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     .
     * \arg \c ::STM_SE_CTRL_VALUE_AUTO      .
     */
    STM_SE_CTRL_DEEMPHASIS,

    /*!
     * This playback-streams & audio player & audio mixer coumpound control specifies the speakers that shall be rendered by the object
     * (it drives the downmix).
     *
     * \pre Applies to play_stream, audio_mixer and audio_player.
     * \pre Specified speaker-config shall not exceed the number of channel setup for the given audio-player.
     *
     * \warning Stability: Unstable
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_audio_channel_assignment_t.
     */
    STM_SE_CTRL_SPEAKER_CONFIG,

    /*!
     * This audio player & audio mixer coumpound control defines the sampling frequency at which the audio is rendered
     * (If '0', the sampling frequency of the master audio is used).
     *
     * The user has the possibility to force the output sampling frequency to the state sampling frequency or to
     * limit the output sampling frequency to the stated max sampling frequency.
     *
     * \warning Stability: Unstable
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_output_frequency_t.
     */
    STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY,

    /*!
     * This audio player coumpound control selects what hardware features should be deployed for this audio player.
     *
     * \warning Stability: Unstable
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_ctrl_audio_player_hardware_mode_t.
     */
    STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE,

    /*!
     * This audio player control provides the "program level" AKA "dialog level" at which the played stream should be
     * decoded/rendered. For example European Broadcast Union requires rendering TV / analog outputs at a
     * standard level of -2300 mB and -3100 mB for digital output connected to an AVR.
     *
     * Default value is 0 mB in which case the target playback level is driven by the AUDIO_APPLICATION_TYPE.
     *
     * If set to another value than 0 mB for a given object, this control will override the APPLICATION_TYPE
     * driven setting for this object.
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * integer 'value' sets the program level (in mB) and must be in the range
     * -3100..0 (and will be quanitzed to the nearest 100mB)
     */
    STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL,

    /*!
     * This audio player control specifies the application of the DC-remove filter on a given object.
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Disable DC-Remove filter.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Enable DC-Remove filter.
     */
    STM_SE_CTRL_DCREMOVE,

    /*!
     * This audio player compound control describes the topology and params of the bass management to apply.
     *
     * It provides a mechanism to attenuate and delay each audio channel independently.
     *
     * \warning Stability: Unstable
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_bassmgt_t and is
     * not applied by default.
     */
    STM_SE_CTRL_BASSMGT,

    /*!
     * This audio player compound control sets the overall volume of an audio path. When enabling the
     * look-ahead facility, this module basically introduce an equivalent delay to all-channels.
     *
     * \warning Stability: Unstable
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_limiter_t and is
     * not applied by default.
     */
    STM_SE_CTRL_LIMITER,

    /*!
     * This audio player compound control sets the analog output to be encoded in BTSC encoder
     *
     * \pre To enable BTSC hardware_mode for a particular player should be changed to BTSC
     *
     * \warning Stability: Unstable
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_btsc_t and is
     * not applied by default.
     */
    STM_SE_CTRL_BTSC,

    /*!
     * This audio player compound control defines the delay to apply to the processed audio output
     *
     * \pre Applies to a play-stream, an audio-player or an audio-mixer object.
     *
     * \warning Stability: Unstable
     *
     * The integer control value sets the delay in milliseconds and must be in the range 0..200.
     */
    STM_SE_CTRL_AUDIO_DELAY,

    /*!
     * This audio player & mixer control defines the gain (attenuation or amplification)
     * to apply to the processed audio output
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * integer 'value' sets the gain in millibels and must be in the range -9600..3200.
     */
    STM_SE_CTRL_AUDIO_GAIN,

    /*!
     * This audio player control enables to query a softmute of the processed audio output.
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY
     */
    STM_SE_CTRL_AUDIO_SOFTMUTE,

    /*!
     * This audio player compound control specifies the proprietary virtualizer's behaviors (<n> channels to 2 or 3 channels).
     *
     * \warning This control is not implemented
     * \warning internal
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_virtualizer_t and is
     * not applied by default.
     */
    STM_SE_CTRL_VIRTUALIZER,

    /*!
     * This audio player compound control specifies the type of volume manager processing.
     *
     * With ST-IVC, if MDRC is enabled, it will inherit the MDRC parameters specified through STM_SE_CTRL_MDRC.
     *
     * \warning This control is not implemented
     * \warning internal
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_volume_manager_t and is
     * not applied by default.
     */
    STM_SE_CTRL_VOLUME_MANAGER,

    /*!
     * This audio player compound control specifies the type of upmix processing.
     *
     * \warning This control is not implemented
     * \warning internal
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_ctrl_upmixer_t and is
     * not applied by default.
     */
    STM_SE_CTRL_UPMIXER,

    /*!
     * This audio player compound control specifies the type of dialog enhancer processing.
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_ctrl_dialog_enhancer_t and is
     * not applied by default.
     */
    STM_SE_CTRL_DIALOG_ENHANCER,

    /*!
     * This audio player and audio mixer simple control specifies the amount of virtualizer processing
     * that has been set with STM_SE_CTRL_VIRTUALIZER control.
     *
     * The acceptable range is [0-100] and is interpreted as a percentage.
     */
    STM_SE_CTRL_VIRTUALIZER_AMOUNT,

    /*!
     * This audio player and audio mixer simple control specifies the amount of volume Manager processing
     * that has been set with STM_SE_CTRL_VOLUME_MANAGER control.
     *
     * The acceptable range is [0-100] and is interpreted as a percentage.
     */
    STM_SE_CTRL_VOLUME_MANAGER_AMOUNT,

    /*!
     * This audio player and audio mixer simple control specifies the amount of upmixer processing
     * that has been set with STM_SE_CTRL_UPMIXER control.
     *
     * The acceptable range is [0-100] and is interpreted as a percentage.
     */
    STM_SE_CTRL_UPMIXER_AMOUNT,

    /*!
     * This audio player and audio mixer simple control specifies the amount of Dialog Enhancer processing
     * that has been set with STM_SE_CTRL_DIALOG_ENHANCER control.
     *
     * The acceptable range is [0-100] and is interpreted as a percentage.
     */
    STM_SE_CTRL_DIALOG_ENHANCER_AMOUNT,

    /*!
     * This compound control defines the number of filters and the spec of each filter for the MultiBand
     * DynamicRangeController.
     *
     * \warning This control is not implemented
     * \warning internal
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_st_mdrc_t.
     */
    STM_SE_CTRL_ST_MDRC,

    /*!
     * This audio reader compound control sets and gets the information from an audio source feeding into the audio readers. This
     * information is usually set by the audio sources such as HDMIRX and DPRX components but may be of interest
     * to applications.
     *
     * \note This control is implemented as a read/write control but when data is tunneled within STKPI is not
     *       expected that applications or middlewares update this control. Conversely if the capture unit is off-chip
     *       and there is no tunneling then the capture unit's driver should use this control to ensure correct
     *       handling of data by the streaming engine.
     *
     * \pre This control is unique to the audio reader and cannot be applied to any other objects.
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_ctrl_audio_reader_source_info_t.
     */
    STM_SE_CTRL_AUDIO_READER_SOURCE_INFO,

    /*!
     * This audio reader control sets capture grain.
     *
     * \pre This control is unique to the audio reader and cannot be applied to any other objects.
     *
     */
    STM_SE_CTRL_AUDIO_READER_GRAIN,


    /*!
     * Read-only control to get the audio decode capabilities of streaming engine.
     *
     * The read-only \ref compound_controls "compound" value uses \ref stm_se_audio_dec_capability_t.
     * This control can only be used with stm_se_get_compound_control STKPI.
     */
    STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE,

    /*!
     * This encoder control allows to setup a tunneled transcode use case in "non real-time" mode.
     *
     * In NRT mode, SE performs internal synchronization of all the audio/video encode_streams in
     * order to have ordered encoded frames, in regards of their input presentation time stamp.
     * In case one of the streams is missing frames, it may take some corrective action to generate
     * some stuffing frames at the encoder output: MUTE for audio streams, REPEAT for video streams.
     * In case all play_streams are not feeding the encoder fast enough, it may just stop encoding
     * frames, and then restart when all streams are synchronized again.
     *
     * The internal synchonization process supplies a continuous presentation time stamp information
     * for all transcoded streams, carried by the encoded_time field in the compressed metatada
     * supplied with each encoded frame. The original input presentation time stamp information is
     * carried by the native_time field in the compressed metatada.
     *
     * Currently this control is not supported for non-tunneled transcode use case. Thus any call
     * to stm_se_encode_stream_inject_frame() API will return an error if NRT mode has been set.
     *
     * It applies on an encode object: next created encode_stream(s) will be impacted by this control.
     * In order to have a correct setup for NRT transcode, it is needed that this control is set before
     * any play_stream is attached to an encode_stream for the corresponding encode. Thus any try to
     * enable NRT mode when an encode_stream is already attached to a play_stream will fail.
     *
     * By default, NRT mode is disabled.
     *
     * \warning Stability: Unstable
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY
     */
    STM_SE_CTRL_ENCODE_NRT_MODE = STM_SE_CTRL_ENCODE_BASE,

    /*!
     * This encoder video control sets the bitrate control mode to encode video: variable bitrate (VBR), constant bitrate (CBR) or bitrate control off (OFF).
     * When bitrate control is disable instead of targetting a certain bitrate, we target a certain quantization (qp) that is to say an output quality.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR
     */
    STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE,

    /*!
     * This encoder video control specifies the targeted video encode bitrate.
     *
     * As all video encode controls, it must be static: called once before starting frame injection.
     *
     * The following control values are permitted:
     *
     * \arg \c integer 'value' sets the bitrate in bits/s and must be positive.
     */
    STM_SE_CTRL_ENCODE_STREAM_BITRATE,

    /*!
     * This encoder video control sets the maximum usable memory size for a video encoder.
     * It applies on an encode object: next created encode_stream will be impacted/dimensioned by this control.
     *
     * By default, profile is set to HD.
     *
     * \warning Stability: Temporary
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_ENCODE_HD_PROFILE            Internal buffers are dimensionned for 1920x1088
     * \arg \c ::STM_SE_CTRL_VALUE_ENCODE_720p_PROFILE          Internal buffers are dimensionned for 1280x720
     * \arg \c ::STM_SE_CTRL_VALUE_ENCODE_SD_PROFILE            Internal buffers are dimensionned for 720x576
     * \arg \c ::STM_SE_CTRL_VALUE_ENCODE_CIF_PROFILE           Internal buffers are dimensionned for 352x288
     */
    STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE = STM_SE_CTRL_VIDEO_ENCODE_BASE,

    /*!
     * This encoder video control sets the input color format injected to a video encoder.
     * It applies on an encode object: next created encode_stream will be impacted by this control.
     *
     * Some memory optimization can be handled depending on input color format.
     * In particular handling 422_YUYV color format at encode input implies use of an extra preproc buffer
     * sized with input resolution.
     *
     * By default, SURFACE_FORMAT_VIDEO_422_YUYV is not supported (except if explicitly requested through this control).
     *
     * \warning Stability: Temporary
     *
     * \arg \c The value uses \ref surface_format_t
     *
     */
    STM_SE_CTRL_VIDEO_ENCODE_INPUT_COLOR_FORMAT,

    /*!
     * This encoder video compound control provides the resolution of the encoded frame.
     *
     * This control is mandatory so as to guaranty a static encode configuration,
     * ie no dependency on injected frame resolution.
     *
     * As all video encode controls, it must be static: called once before starting frame injection.
     *
     * If encode resolution (provided through this control) is different from the
     * resolution of the injected frame, scaling operation would be transparently managed.
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_picture_resolution_t.
     */
    STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION,

    /*!
     * This encoder video control enables / disables deinterlacing before encode stage.
     *
     * By default, deinterlacing is enabled to ensure progressive content to be encoded
     * This is required till we rely on a frame only encoder (not a field encoder).
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Frames to be encoded are deinterlaced before being encoded.
     */
    STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING,

    /*!
     * This encoder video control enables / disables noise filtering before encode stage.
     *
     * By default, noise filtering is disabled.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Noise filtering algorithm is not enabled.
     */
    STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING,

    /*!
     * This encoder video compound control provides the framerate of the encoded sequence.
     *
     * This control is mandatory so as to guaranty a static encode configuration,
     * ie no dependency on injected frame framerate.
     *
     * As all video encode controls, it must be static: called once before starting frame injection.
     *
     * Warning : only INTEGER framerate (denominator=1) are allowed.
     *
     * If integer encode framerate (provided through this control) is different from the framerate
     * of the injected frame, framerate conversion would be transparently managed to get
     * requested encode framerate.
     *
     * The following control values are permitted:
     *
     * \arg \c integer 'value' only
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_framerate_t.
     */
    STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE,

    /*!
     * This encoder video control provides the intra frame period.
     *
     * By default, intra period is set to '15'.
     *
     * The following control values are permitted:
     *
     * \arg \c “1 upto (2^32)-1", knowing that ‘1’ would involve all frames to be encoded as ‘I’ frames and ’0’ would involve all frames to be encoded as ‘P’ frames except first one
     */
    STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE,

    /*!
     * This encoder video compound control provides flexibility on encode slicing
     * (Mono or Multi slice per encoded frame).
     *
     * Multi-slice is configured through a maximum macroblock size criteria or a maximum bytes
     * criteria.
     *
     * The \ref compound_controls "compound" value would use \ref stm_se_video_multi_slice_t.
     */
    STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE,

    /*!
     * This encoder video control provides the H264 level of the encoded stream as mentioned in H264 standard.
     *
     * By default, H264 level is set to '4.2'
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_0_9  Special level to indicate 1B for PROFILE_HIGH variants output
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_0
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_B  Special level to indicate 1B input
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_1
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_2
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_3
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_0
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_1
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_2
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_0
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_1
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_2
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_0
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_1
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_2
     */
    STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL,

    /*!
     * This encoder video control provides the H264 profile of the encoded stream as mentioned in H264 standard
     *
     * By default, H264 encode profile is set to 'BASELINE'
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_BASELINE
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_MAIN
     * \arg \c ::STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH
     */
    STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE,

    /*!
     * This encoder video control provides the Coded Picture Buffer (CPB) size.
     *
     * By default, CPB size is set to '2 * bitrate'.
     *
     * The CPB refers to H.264 hypothetical reference decoder (HRD)  buffer model.
     * Applicable to the H.264 encoder only.
     *
     * The integer control value sets the CPB size in bits & must be positive.
     */
    STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE,

    /*!
     * This encoder video control sets the display aspect ratio management.
     *
     * By default, the encode display aspect ratio is set to 'IGNORE',
     * meaning that only encode resolution is considered and no aspect ratio conversion is performed.
     *
     * If set to '4/3' or '16/9', relevant black bars will be added in the encoded frame
     * to respect wished display aspect ratio (letterbox or pillarbox modes).
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE
     * \arg \c ::STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_4_BY_3
     * \arg \c ::STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_16_BY_9
     */
    STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO,

    /*!
     * This encoder video control enables / disables cropping of the "window of interest" before encode stage.
     *
     * This control may be used in case of transcode to allow / disallow automatic cropping of a broadcasted
     * window of interest rectangle, that would be embedded in the original coded stream (e.g. pan-scan data).
     *
     * By default, cropping of "window of interest" is disabled.
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  Frames to be encoded are not cropped before being encoded,
     *                                       thus the window_of_interest field in the metadata of each
     *                                       injected frame is ignored.
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     Frames to be encoded are cropped before being scaled then encoded, as specified
     *                                       by the window_of_interest field in the metadata of each injected frame.
     */
    STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING,

    /*!
     * Read-only control to get the video encode capabilities of streaming engine.
     * \warning This control is not implemented
     *
     * The read-only \ref compound_controls "compound" value uses \ref stm_se_video_enc_capability_t.
     * This control only as an input of stm_se_get_compound_control STKPI.
     */
    STM_SE_CTRL_GET_CAPABILITY_VIDEO_ENCODE,

    /*!
     * This encoder audio compound control provides bit-rate target parameters.
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_audio_bitrate_control_t.
     */
    STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL = STM_SE_CTRL_AUDIO_ENCODE_BASE,

    /*!
     * This encoder audio compound control provides SCMS information to be written in the stream.
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_audio_scms_t.
     */
    STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS,

    /*!
     * This encoder audio compound control provides the encoded stream's
     * sampling frequency and channel assignment
     *
     * When control stm_se_audio_core_format_t::sample_rate is 0 then the
     * output sample rate is the same as the input sample rate.
     *
     * When control stm_se_audio_core_format_t::channel_placement.channel_count is 0
     * then the output channel placement is the same as the input channel placement.
     *
     * The \ref compound_controls "compound" value uses \ref stm_se_audio_core_format_t.
     */
    STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT,

    /*!
     * This encoder audio control defines whether MPEG derived encoders append CRC values to frames.
     *
     * If true and supported by codec the CRC will be computed and added to stream, according
     * to the standard (if not enabled but crc is mandated by codec, crc will still be
     * computed and added to stream).
     *
     * The following control values are permitted:
     *
     * \arg \c ::STM_SE_CTRL_VALUE_DISAPPLY  CRC will be omitted (unless mandated by encoding)
     * \arg \c ::STM_SE_CTRL_VALUE_APPLY     CRC will be computed and added to stream (if support by encoding)
     */
    STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE,

    /*!
     * This encoder audio control sets the program level for audio encoding.
     * \todo This control is not implemented
     *
     * The following control values are permitted:
     *
     * integer 'value' sets the program level in millibels and must be in
     * the range -3100..0. Zero is treated as "use region specific default", typically
     * -3100.
     */
    STM_SE_CTRL_AUDIO_ENCODE_STREAM_PROGRAM_LEVEL,

    /*!
     * Read-only control to get the audio encode capabilities of streaming engine.
     * \warning This control is not implemented
     *
     * The read-only \ref compound_controls "compound" value uses \ref stm_se_audio_enc_capability_t.
     * This control can only be used with stm_se_get_compound_control STKPI.
     */
    STM_SE_CTRL_GET_CAPABILITY_AUDIO_ENCODE,

    STM_SE_CTRL_MAX_PLAYER_OPTION = STM_SE_CTRL_PLAYER_MAX,

    STM_SE_CTRL_UNKNOWN_OPTION
} stm_se_ctrl_t;

static inline const char *StringifyControl(stm_se_ctrl_t aControl)
{
    switch (aControl)
    {
        // STM_SE_CTRL_PLAYER_MIN
        ENTRY(STM_SE_CTRL_LIVE_PLAY);
        ENTRY(STM_SE_CTRL_ENABLE_SYNC);
        ENTRY(STM_SE_CTRL_CLAMP_PLAY_INTERVAL_ON_DIRECTION_CHANGE);
        ENTRY(STM_SE_CTRL_PLAYOUT_ON_TERMINATE);
        ENTRY(STM_SE_CTRL_PLAYOUT_ON_SWITCH);
        ENTRY(STM_SE_CTRL_PLAYOUT_ON_DRAIN);
        ENTRY(STM_SE_CTRL_IGNORE_STREAM_UNPLAYABLE_CALLS);
        ENTRY(STM_SE_CTRL_DECODE_BUFFERS_USER_ALLOCATED);
        ENTRY(STM_SE_CTRL_LIMIT_INJECT_AHEAD);
        ENTRY(STM_SE_CTRL_ENABLE_CONTROL_DATA_DETECTION);
        ENTRY(STM_SE_CTRL_REDUCE_COLLATED_DATA);
        ENTRY(STM_SE_CTRL_OPERATE_COLLATOR2_IN_REVERSIBLE_MODE);
        ENTRY(STM_SE_CTRL_MPEG2_IGNORE_PROGRESSIVE_FRAME_FLAG);
        ENTRY(STM_SE_CTRL_MPEG2_APPLICATION_TYPE);
        ENTRY(STM_SE_CTRL_MPEG2_TIME_CODE);
        ENTRY(STM_SE_CTRL_H264_TREAT_DUPLICATE_DPB_VALUES_AS_NON_REF_FRAME_FIRST);
        ENTRY(STM_SE_CTRL_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING);
        ENTRY(STM_SE_CTRL_H264_VALIDATE_DPB_VALUES_AGAINST_PTS_VALUES);
        ENTRY(STM_SE_CTRL_H264_TREAT_TOP_BOTTOM_PICTURE_AS_INTERLACED);
        ENTRY(STM_SE_CTRL_H264_ALLOW_NON_IDR_RESYNCHRONIZATION);
        ENTRY(STM_SE_CTRL_FRAME_RATE_CALCULUATION_PRECEDENCE);
        ENTRY(STM_SE_CTRL_CONTAINER_FRAMERATE);
        ENTRY(STM_SE_CTRL_HEVC_ALLOW_BAD_PREPROCESSED_FRAMES);
        ENTRY(STM_SE_CTRL_H264_ALLOW_BAD_PREPROCESSED_FRAMES);
        ENTRY(STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE);
        ENTRY(STM_SE_CTRL_ERROR_DECODING_LEVEL);
        ENTRY(STM_SE_CTRL_AUDIO_APPLICATION_TYPE);
        ENTRY(STM_SE_CTRL_AUDIO_SERVICE_TYPE);
        ENTRY(STM_SE_CTRL_REGION);
        ENTRY(STM_SE_CTRL_AUDIO_PROGRAM_REFERENCE_LEVEL);
        ENTRY(STM_SE_CTRL_AUDIO_SUBSTREAM_ID);
        ENTRY(STM_SE_CTRL_ALLOW_REFERENCE_FRAME_SUBSTITUTION);
        ENTRY(STM_SE_CTRL_DISCARD_FOR_REFERENCE_QUALITY_THRESHOLD);
        ENTRY(STM_SE_CTRL_DISCARD_FOR_MANIFESTATION_QUALITY_THRESHOLD);
        ENTRY(STM_SE_CTRL_DECIMATE_DECODER_OUTPUT);
        ENTRY(STM_SE_CTRL_DISPLAY_FIRST_FRAME_EARLY);
        ENTRY(STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR);
        ENTRY(STM_SE_CTRL_MASTER_CLOCK);
        ENTRY(STM_SE_CTRL_EXTERNAL_TIME_MAPPING);
        ENTRY(STM_SE_CTRL_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED);
        ENTRY(STM_SE_CTRL_VIDEO_START_IMMEDIATE);
        ENTRY(STM_SE_CTRL_PACKET_INJECTOR);
        ENTRY(STM_SE_CTRL_VIDEO_KEY_FRAMES_ONLY);
        ENTRY(STM_SE_CTRL_VIDEO_DISCARD_FRAMES);
        ENTRY(STM_SE_CTRL_VIDEO_SINGLE_GOP_UNTIL_NEXT_DISCONTINUITY);
        ENTRY(STM_SE_CTRL_DISCARD_LATE_FRAMES);
        ENTRY(STM_SE_CTRL_REBASE_ON_DATA_DELIVERY_LATE);
        ENTRY(STM_SE_CTRL_ALLOW_REBASE_AFTER_LATE_DECODE);
        ENTRY(STM_SE_CTRL_TRICK_MODE_DOMAIN);
        ENTRY(STM_SE_CTRL_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED);
        ENTRY(STM_SE_CTRL_CLOCK_RATE_ADJUSTMENT_LIMIT);
        ENTRY(STM_SE_CTRL_SYMMETRIC_JUMP_DETECTION);
        ENTRY(STM_SE_CTRL_SYMMETRIC_PTS_FORWARD_JUMP_DETECTION_THRESHOLD);
        ENTRY(STM_SE_CTRL_PLAYBACK_LATENCY);
        ENTRY(STM_SE_CTRL_HDMI_RX_MODE);
        ENTRY(STM_SE_CTRL_RUNNING_DEVLOG);
        ENTRY(STM_SE_CTRL_TRICK_MODE_AUDIO);
        ENTRY(STM_SE_CTRL_PLAY_24FPS_VIDEO_AT_25FPS);
        ENTRY(STM_SE_CTRL_USE_PTS_DEDUCED_DEFAULT_FRAME_RATES);
        ENTRY(STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS);
        ENTRY(STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS);
        ENTRY(STM_SE_CTRL_GET_CAPABILITY_VIDEO_DECODE);
        ENTRY(STM_SE_CTRL_AAC_DECODER_CONFIG);
        ENTRY(STM_SE_CTRL_PLAY_STREAM_NBCHANNELS);
        ENTRY(STM_SE_CTRL_PLAY_STREAM_SAMPLING_FREQUENCY);
        ENTRY(STM_SE_CTRL_PLAY_STREAM_ELEMENTARY_BUFFER_LEVEL);
        ENTRY(STM_SE_CTRL_AUDIO_GENERATOR_BUFFER);
        ENTRY(STM_SE_CTRL_AUDIO_INPUT_FORMAT);
        ENTRY(STM_SE_CTRL_AUDIO_INPUT_EMPHASIS);
        ENTRY(STM_SE_CTRL_STREAM_DRIVEN_DUALMONO);
        ENTRY(STM_SE_CTRL_DUALMONO);
        ENTRY(STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION);
        ENTRY(STM_SE_CTRL_STREAM_DRIVEN_STEREO);
        ENTRY(STM_SE_CTRL_AUDIO_MIXER_GRAIN);
        ENTRY(STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENT);
        ENTRY(STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN);
        ENTRY(STM_SE_CTRL_DEEMPHASIS);
        ENTRY(STM_SE_CTRL_SPEAKER_CONFIG);
        ENTRY(STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY);
        ENTRY(STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE);
        ENTRY(STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL);
        ENTRY(STM_SE_CTRL_DCREMOVE);
        ENTRY(STM_SE_CTRL_BASSMGT);
        ENTRY(STM_SE_CTRL_LIMITER);
        ENTRY(STM_SE_CTRL_BTSC);
        ENTRY(STM_SE_CTRL_AUDIO_DELAY);
        ENTRY(STM_SE_CTRL_AUDIO_GAIN);
        ENTRY(STM_SE_CTRL_AUDIO_SOFTMUTE);
        ENTRY(STM_SE_CTRL_VIRTUALIZER);
        ENTRY(STM_SE_CTRL_VOLUME_MANAGER);
        ENTRY(STM_SE_CTRL_UPMIXER);
        ENTRY(STM_SE_CTRL_DIALOG_ENHANCER);
        ENTRY(STM_SE_CTRL_ST_MDRC);
        ENTRY(STM_SE_CTRL_AUDIO_READER_SOURCE_INFO);
        ENTRY(STM_SE_CTRL_AUDIO_READER_GRAIN);
        ENTRY(STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE);

        // STM_SE_CTRL_ENCODE_BASE
        ENTRY(STM_SE_CTRL_ENCODE_NRT_MODE);
        ENTRY(STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE);
        ENTRY(STM_SE_CTRL_ENCODE_STREAM_BITRATE);

        // STM_SE_CTRL_VIDEO_ENCODE_BASE
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_INPUT_COLOR_FORMAT);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO);
        ENTRY(STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING);
        ENTRY(STM_SE_CTRL_GET_CAPABILITY_VIDEO_ENCODE);

        // STM_SE_CTRL_AUDIO_ENCODE_BASE
        ENTRY(STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL);
        ENTRY(STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS);
        ENTRY(STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT);
        ENTRY(STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE);
        ENTRY(STM_SE_CTRL_AUDIO_ENCODE_STREAM_PROGRAM_LEVEL);
        ENTRY(STM_SE_CTRL_GET_CAPABILITY_AUDIO_ENCODE);

        // STM_SE_CTRL_PLAYER_MAX
        ENTRY(STM_SE_CTRL_MAX_PLAYER_OPTION);
        ENTRY(STM_SE_CTRL_UNKNOWN_OPTION);
    default: return "<unknown control>";
    }
}

/*! @} */ /* controls */

#endif /* STM_SE_CONTROLS_H_ */
