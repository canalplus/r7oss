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

#ifndef STM_SE_TYPES_H_
#define STM_SE_TYPES_H_

#ifndef ENTRY
#define ENTRY(enum) case enum: return #enum
#endif

/*! \addtogroup streaming_engine
 *  @{
 */

/*!
 * Maximum of streaming engine playback object
 */
#define PLAYBACK_MAX_NUMBER     16
#define STREAM_MAX_NUMBER       16

/*!
 * Maximum of streaming engine encoder object
 * Mme transformer is limited to 32 instances by multicom
 * Encode stream is limited to 6 instances per encoder for HLS use case
 * As encode * encode stream = mme instances, encoder is limited to 5 instances
 */
#define ENCODE_MAX_NUMBER        5
#define ENCODE_STREAM_MAX_NUMBER 6

/*!
 * A special case value for time (uint64_t) that may be used to indicate a time interval without boundaries.
 */
#define STM_SE_PLAY_TIME_NOT_BOUNDED            0xfedcba9876543210ull

/*!
 * Defines the maximum number of elements in ::stm_se_audio_channel_placement_t
 */
#define STM_SE_MAX_NUMBER_OF_AUDIO_CHAN 32

/*!
 * Defines the maximum number of input audio channels for an audio encode stream
 */
#define STM_SE_MAX_AUDIO_CHAN  8

/*!
 * Defines the minimum latency in the HDMIRx audio chain
 */
#define STM_SE_HDMIRX_MIN_LATENCY_MS           40

/*!
 * Defines the maximum number of samples per injection into the audio encode stream
 */
#define STM_SE_AUDIO_ENCODE_MAX_SAMPLES_PER_FRAME    4096

/*!
 * Defines media types
 */
typedef enum
{
    STM_SE_MEDIA_AUDIO,
    STM_SE_MEDIA_VIDEO,
    STM_SE_MEDIA_ANY,
} stm_se_media_t;

/*!
 * This type describes the aspect ratio of a video frame.
 */
typedef enum stm_se_aspect_ratio_e
{
    STM_SE_ASPECT_RATIO_4_BY_3,
    STM_SE_ASPECT_RATIO_16_BY_9,
    STM_SE_ASPECT_RATIO_221_1,
    STM_SE_ASPECT_RATIO_UNSPECIFIED = 0x10000
} stm_se_aspect_ratio_t;

static inline const char *StringifyAspectRatio(stm_se_aspect_ratio_t aAspectRatio)
{
    switch (aAspectRatio)
    {
        ENTRY(STM_SE_ASPECT_RATIO_4_BY_3);
        ENTRY(STM_SE_ASPECT_RATIO_16_BY_9);
        ENTRY(STM_SE_ASPECT_RATIO_221_1);
        ENTRY(STM_SE_ASPECT_RATIO_UNSPECIFIED);
    default: return "<unknown aspect ratio>";
    }
}

/*!
 * Describes audio channels.
 *
 * All values above STM_SE_AUDIO_CHAN_END are reserved
 */
typedef enum stm_se_audio_channel_id_t
{
    STM_SE_AUDIO_CHAN_L,        //!< Left
    STM_SE_AUDIO_CHAN_R,        //!< Right
    STM_SE_AUDIO_CHAN_LFE,      //!< Low Frequency Effect
    STM_SE_AUDIO_CHAN_C,        //!< Centre
    STM_SE_AUDIO_CHAN_LS,       //!< Left Surround
    STM_SE_AUDIO_CHAN_RS,       //!< Right Surround

    STM_SE_AUDIO_CHAN_LT,       //!< Surround Matrixed Left
    STM_SE_AUDIO_CHAN_RT,       //!< Surround Matrixed Right
    STM_SE_AUDIO_CHAN_LPLII,        //!< DPLII Matrixed Left
    STM_SE_AUDIO_CHAN_RPLII,        //!< DPLII Matrixed Right

    STM_SE_AUDIO_CHAN_CREAR,        //!< Rear Centre

    STM_SE_AUDIO_CHAN_CL,       //!< Centre Left
    STM_SE_AUDIO_CHAN_CR,       //!< Centre Right

    STM_SE_AUDIO_CHAN_LFEB,     //!< Second LFE

    STM_SE_AUDIO_CHAN_L_DUALMONO,   //!< Dual-Mono Left
    STM_SE_AUDIO_CHAN_R_DUALMONO,   //!< Dual-Mono Right

    STM_SE_AUDIO_CHAN_LWIDE,        //!< Wide Left
    STM_SE_AUDIO_CHAN_RWIDE,        //!< Wide Right

    STM_SE_AUDIO_CHAN_LDIRS,        //!< Directional Surround left
    STM_SE_AUDIO_CHAN_RDIRS,        //!< Directional Surround Right

    STM_SE_AUDIO_CHAN_LSIDES,       //!< Side Surround Left
    STM_SE_AUDIO_CHAN_RSIDES,       //!< Side Surround Right

    STM_SE_AUDIO_CHAN_LREARS,       //!< Rear Surround Left
    STM_SE_AUDIO_CHAN_RREARS,       //!< Rear Surround Right

    STM_SE_AUDIO_CHAN_CHIGH,        //!< High Centre
    STM_SE_AUDIO_CHAN_LHIGH,        //!< High Left
    STM_SE_AUDIO_CHAN_RHIGH,        //!< High Right
    STM_SE_AUDIO_CHAN_LHIGHSIDE,    //!< High Side Left
    STM_SE_AUDIO_CHAN_RHIGHSIDE,    //!< High Side Right
    STM_SE_AUDIO_CHAN_CHIGHREAR,    //!< High Rear Centre
    STM_SE_AUDIO_CHAN_LHIGHREAR,    //!< High Rear Left
    STM_SE_AUDIO_CHAN_RHIGHREAR,    //!< High Rear Right

    STM_SE_AUDIO_CHAN_CLOWFRONT,    //!< Low Front Centre
    STM_SE_AUDIO_CHAN_TOPSUR,       //!< Surround Top

    STM_SE_AUDIO_CHAN_DYNSTEREO_LS, //!< Dynamic Stereo Left Surround
    STM_SE_AUDIO_CHAN_DYNSTEREO_RS, //!< Dynamic Stereo Right Surround

    //! Last channel id with a defined positioning
    STM_SE_AUDIO_CHAN_LAST_NAMED = STM_SE_AUDIO_CHAN_DYNSTEREO_RS,

    //! Id for a channel with valid content but no defined positioning
    STM_SE_AUDIO_CHAN_UNKNOWN,

    //! Id for the last channel id with valid content
    STM_SE_AUDIO_CHAN_LAST_VALID = STM_SE_AUDIO_CHAN_UNKNOWN,

    /*!
     * This channel id is to indicate that although it is allocated in the buffer it is not valid content.
     *
     * There is no guarantee that the content of a STUFFING channel be set to 0.
     *
     * Channels identified as STM_SE_AUDIO_CHAN_STUFFING at the input of the process should be ignored.
     *
     * If a process intends to use a STUFFING channel (e.g. to send a buffer "as is" to a FDMA playback) it should mute them.
     *
     * For example if the buffer is allocated for 5.1 channels but the content is stereo, there will be 4 STM_SE_AUDIO_CHAN_STUFFING ids.
     **/
    STM_SE_AUDIO_CHAN_STUFFING = 254,

    /*!
     * All values above STM_SE_AUDIO_CHAN_STUFFING are reserved, because we will
     * case to uint8_t
     */
    STM_SE_AUDIO_CHAN_RESERVED = 255
} stm_se_audio_channel_id_t;

/*!
 * This type describes the format of an audio sample.
 *
 * This includes its quantization as well as the number of bits it is
 * stored in and aligned on.
 */
typedef enum stm_se_audio_pcm_format_e
{
    STM_SE_AUDIO_PCM_FMT_S32LE,  //!< 32bit aligned and store, signed 32bit Q31linear little endian
    STM_SE_AUDIO_PCM_FMT_S32BE,  //!< 32bit aligned and store, signed 32bit Q31linear big endian
    STM_SE_AUDIO_PCM_FMT_S24LE,  //!< 24bit aligned and store, signed 24bit Q23linear little endian
    STM_SE_AUDIO_PCM_FMT_S24BE,  //!< 24bit aligned and store, signed 24bit Q23linear big endian
    STM_SE_AUDIO_PCM_FMT_S16LE,  //!< 16bit aligned and store, signed 16bit Q15linear little endian
    STM_SE_AUDIO_PCM_FMT_S16BE,  //!< 16bit aligned and store, signed 16bit Q15linear big endian
    STM_SE_AUDIO_PCM_FMT_U16BE,  //!< 16bit aligned and store, unsigned 16bit Q16linear big endian
    STM_SE_AUDIO_PCM_FMT_U16LE,  //!< 16bit aligned and store, unsigned 16bit Q16linear little endianQ8linear  */
    STM_SE_AUDIO_PCM_FMT_U8,     //!< 8bit aligned and store, unsigned 8bit Q8linear*/
    STM_SE_AUDIO_PCM_FMT_S8,     //!< 8bit aligned and store, signed 8bit   Q7linear
    STM_SE_AUDIO_PCM_FMT_ALAW_8, //!< 8bit aligned and store, alaw 8bit
    STM_SE_AUDIO_PCM_FMT_ULAW_8, //!< 8bit aligned and store, ulaw 8bit

    STM_SE_AUDIO_PCM_FMT_LAST = STM_SE_AUDIO_PCM_FMT_ULAW_8, //!< Last supported format.
    STM_SE_AUDIO_LPCM_FORMAT_RESERVED, //!< Reserved.
} stm_se_audio_pcm_format_t;

static inline const char *StringifyPcmFormat(stm_se_audio_pcm_format_t aPcmFormat)
{
    switch (aPcmFormat)
    {
        ENTRY(STM_SE_AUDIO_PCM_FMT_S32LE);
        ENTRY(STM_SE_AUDIO_PCM_FMT_S32BE);
        ENTRY(STM_SE_AUDIO_PCM_FMT_S24LE);
        ENTRY(STM_SE_AUDIO_PCM_FMT_S24BE);
        ENTRY(STM_SE_AUDIO_PCM_FMT_S16LE);
        ENTRY(STM_SE_AUDIO_PCM_FMT_S16BE);
        ENTRY(STM_SE_AUDIO_PCM_FMT_U16BE);
        ENTRY(STM_SE_AUDIO_PCM_FMT_U16LE);
        ENTRY(STM_SE_AUDIO_PCM_FMT_U8);
        ENTRY(STM_SE_AUDIO_PCM_FMT_S8);
        ENTRY(STM_SE_AUDIO_PCM_FMT_ALAW_8);
        ENTRY(STM_SE_AUDIO_PCM_FMT_ULAW_8);
        ENTRY(STM_SE_AUDIO_LPCM_FORMAT_RESERVED);
    default: return "<unknown pcm format>";
    }
}

/*!
 * This type describes the Serial Copy Management System
 */
typedef enum stm_se_audio_scms_e
{
    STM_SE_AUDIO_NO_COPYRIGHT,                  //!< Any further copy is authorised.
    STM_SE_AUDIO_ONE_MORE_COPY_AUTHORISED,      //!< One more generation copy is authorised.
    STM_SE_AUDIO_NO_FUTHER_COPY_AUTHORISED      //!< No more copy authorized.
} stm_se_audio_scms_t;

/*!
 * This type defines colorspace of a frame.
 */
typedef enum stm_se_colorspace_e
{
    STM_SE_COLORSPACE_UNSPECIFIED,
    STM_SE_COLORSPACE_SMPTE170M,
    STM_SE_COLORSPACE_SMPTE240M,
    STM_SE_COLORSPACE_BT709,
    STM_SE_COLORSPACE_BT470_SYSTEM_M,
    STM_SE_COLORSPACE_BT470_SYSTEM_BG,
    STM_SE_COLORSPACE_SRGB
} stm_se_colorspace_t;

static inline const char *StringifyColorspace(stm_se_colorspace_t aColorspace)
{
    switch (aColorspace)
    {
        ENTRY(STM_SE_COLORSPACE_UNSPECIFIED);
        ENTRY(STM_SE_COLORSPACE_SMPTE170M);
        ENTRY(STM_SE_COLORSPACE_SMPTE240M);
        ENTRY(STM_SE_COLORSPACE_BT709);
        ENTRY(STM_SE_COLORSPACE_BT470_SYSTEM_M);
        ENTRY(STM_SE_COLORSPACE_BT470_SYSTEM_BG);
        ENTRY(STM_SE_COLORSPACE_SRGB);
    default: return "<unknown colorspace>";
    }
}

/*!
 * This type defines a possible discontinuity in the stream. A particular case is the 'end of stream'.
 *
 * \warning Stability: Unstable
 */
typedef enum stm_se_discontinuity_e
{
    STM_SE_DISCONTINUITY_CONTINUOUS                          = (0),
    STM_SE_DISCONTINUITY_DISCONTINUOUS                       = (1 << 0),
    STM_SE_DISCONTINUITY_EOS                                 = (1 << 1),
    STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST                  = (1 << 2),
    STM_SE_DISCONTINUITY_OPEN_GOP_REQUEST                    = (1 << 3),
    STM_SE_DISCONTINUITY_FRAME_SKIPPED                       = (1 << 4),
    STM_SE_DISCONTINUITY_MUTE                                = (1 << 5),
    STM_SE_DISCONTINUITY_FADEOUT                             = (1 << 6),
    STM_SE_DISCONTINUITY_FADEIN                              = (1 << 7)
} stm_se_discontinuity_t;

static inline const char *StringifyDiscontinuity(stm_se_discontinuity_t aDiscontinuity)
{
    switch (aDiscontinuity)
    {
        ENTRY(STM_SE_DISCONTINUITY_CONTINUOUS);
        ENTRY(STM_SE_DISCONTINUITY_DISCONTINUOUS);
        ENTRY(STM_SE_DISCONTINUITY_EOS);
        ENTRY(STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST);
        ENTRY(STM_SE_DISCONTINUITY_OPEN_GOP_REQUEST);
        ENTRY(STM_SE_DISCONTINUITY_FRAME_SKIPPED);
        ENTRY(STM_SE_DISCONTINUITY_MUTE);
        ENTRY(STM_SE_DISCONTINUITY_FADEOUT);
        ENTRY(STM_SE_DISCONTINUITY_FADEIN);
    default: return "<unknown discontinuity>";
    }
}

/*!
 * This type describes the level of emphasis present in an audio stream.
 *
 * In addition to its role in describing data this type is also used
 * used by \ref STM_SE_CTRL_AUDIO_INPUT_EMPHASIS as a control value.
 */
typedef enum stm_se_emphasis_type_e
{
    STM_SE_NO_EMPHASIS,
    STM_SE_EMPH_50_15us,
    STM_SE_EMPH_CCITT_J_17
} stm_se_emphasis_type_t;

static inline const char *StringifyEmphasisType(stm_se_emphasis_type_t aEmphasisType)
{
    switch (aEmphasisType)
    {
        ENTRY(STM_SE_NO_EMPHASIS);
        ENTRY(STM_SE_EMPH_50_15us);
        ENTRY(STM_SE_EMPH_CCITT_J_17);
    default: return "<unknown emphasis type>";
    }
}

typedef enum stm_se_encode_stream_encoding_e
{
    // Audio Encoding Types

    STM_SE_ENCODE_STREAM_ENCODING_AUDIO_FIRST,
    /// The Null Encoder instantiates a bypass class to send raw data directly to the output.
    STM_SE_ENCODE_STREAM_ENCODING_AUDIO_NULL = STM_SE_ENCODE_STREAM_ENCODING_AUDIO_FIRST,

    STM_SE_ENCODE_STREAM_ENCODING_AUDIO_MP3,
    STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AC3,
    STM_SE_ENCODE_STREAM_ENCODING_AUDIO_DTS,
    STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AAC,

    STM_SE_ENCODE_STREAM_ENCODING_AUDIO_LAST = STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AAC,

    // Video Encoding Types

    STM_SE_ENCODE_STREAM_ENCODING_VIDEO_FIRST,
    /// The Null Encoder instantiates a bypass class to send raw data directly to the output
    /// It is not recommended to use this type on Video, except for debug/development
    STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL = STM_SE_ENCODE_STREAM_ENCODING_VIDEO_FIRST,

    STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264,
    STM_SE_ENCODE_STREAM_ENCODING_VIDEO_LAST = STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264

} stm_se_encode_stream_encoding_t;

typedef enum
{
    STM_SE_ENCODE_STREAM_MEDIA_AUDIO,
    STM_SE_ENCODE_STREAM_MEDIA_VIDEO,
    STM_SE_ENCODE_STREAM_MEDIA_ANY,
} stm_se_encode_stream_media_t;

static inline const char *StringifyMedia(stm_se_encode_stream_media_t aMedia)
{
    switch (aMedia)
    {
        ENTRY(STM_SE_ENCODE_STREAM_MEDIA_AUDIO);
        ENTRY(STM_SE_ENCODE_STREAM_MEDIA_VIDEO);
        ENTRY(STM_SE_ENCODE_STREAM_MEDIA_ANY);
    default: return "<unknown media>";
    }
}

/*!
 * This type describes how 3D information is encoded in a video frame (or sequence of video frames).
 */
typedef enum stm_se_format_3d_e
{
    STM_SE_FORMAT_3D_NONE,
    STM_SE_FORMAT_3D_FRAME_SEQUENTIAL,
    STM_SE_FORMAT_3D_STACKED_HALF,
    STM_SE_FORMAT_3D_SIDE_BY_SIDE_HALF
} stm_se_format_3d_t;

static inline const char *StringifyFormat3d(stm_se_format_3d_t aFormat3d)
{
    switch (aFormat3d)
    {
        ENTRY(STM_SE_FORMAT_3D_NONE);
        ENTRY(STM_SE_FORMAT_3D_FRAME_SEQUENTIAL);
        ENTRY(STM_SE_FORMAT_3D_STACKED_HALF);
        ENTRY(STM_SE_FORMAT_3D_SIDE_BY_SIDE_HALF);
    default: return "<unknown 3d format>";
    }
}

/*!
 * This type enumerates the encodings supported by a play stream or an encode stream
 *
 * Some of the encodings above have HD variants that can automatically be detected a streaming engine
 * play_stream and therefore are not enumerated as distinct encodings.
 *
 * - To playback Dolby Digital Plus (E-AC3) use ::STM_SE_STREAM_ENCODING_AUDIO_AC3.
 * - To playback DTS-HD use ::STM_SE_STREAM_ENCODING_AUDIO_DTS.
 *
 * Some media formats are, in fact, composed of a multiplex of data with different encodings. These
 * multiplexed formats are also not enumerate as distinct encodings:
 *
 * - To playback the lossless component of a Dolby TrueHD stream use ::STM_SE_STREAM_ENCODING_AUDIO_MLP
 * - To playback the lossy component of a Dolby TrueHD stream use ::STM_SE_STREAM_ENCODING_AUDIO_AC3
 */
typedef enum
{
    STM_SE_STREAM_ENCODING_AUDIO_FIRST,
    STM_SE_STREAM_ENCODING_AUDIO_AUTO = STM_SE_STREAM_ENCODING_AUDIO_FIRST,
    STM_SE_STREAM_ENCODING_AUDIO_NONE,
    STM_SE_STREAM_ENCODING_AUDIO_PCM,
    STM_SE_STREAM_ENCODING_AUDIO_LPCM, //!< DVD-Video LPCM
    STM_SE_STREAM_ENCODING_AUDIO_MPEG1,
    STM_SE_STREAM_ENCODING_AUDIO_MPEG2,
    STM_SE_STREAM_ENCODING_AUDIO_MP3,
    STM_SE_STREAM_ENCODING_AUDIO_AC3,
    STM_SE_STREAM_ENCODING_AUDIO_DTS,
    STM_SE_STREAM_ENCODING_AUDIO_AAC,
    STM_SE_STREAM_ENCODING_AUDIO_WMA,
    STM_SE_STREAM_ENCODING_AUDIO_LPCMA, //!< DVD-Audio LPCM
    STM_SE_STREAM_ENCODING_AUDIO_LPCMH, //!< DVD-HD LPCM
    STM_SE_STREAM_ENCODING_AUDIO_LPCMB, //!< BD-ROM LPCM
    STM_SE_STREAM_ENCODING_AUDIO_SPDIFIN, //!< Raw IEC61937 data
    STM_SE_STREAM_ENCODING_AUDIO_DTS_LBR,
    STM_SE_STREAM_ENCODING_AUDIO_MLP,
    STM_SE_STREAM_ENCODING_AUDIO_RMA,
    STM_SE_STREAM_ENCODING_AUDIO_AVS,
    STM_SE_STREAM_ENCODING_AUDIO_VORBIS,
    STM_SE_STREAM_ENCODING_AUDIO_FLAC,
    STM_SE_STREAM_ENCODING_AUDIO_DRA,
    STM_SE_STREAM_ENCODING_AUDIO_MS_ADPCM,
    STM_SE_STREAM_ENCODING_AUDIO_IMA_ADPCM,
    STM_SE_STREAM_ENCODING_AUDIO_AMRWB,
    STM_SE_STREAM_ENCODING_AUDIO_AMRNB,
    STM_SE_STREAM_ENCODING_AUDIO_G711,
    STM_SE_STREAM_ENCODING_AUDIO_G726,
    STM_SE_STREAM_ENCODING_AUDIO_G729AB,

    STM_SE_STREAM_ENCODING_AUDIO_MIXER0,
    STM_SE_STREAM_ENCODING_AUDIO_MIXER1,

    STM_SE_STREAM_ENCODING_AUDIO_LAST = STM_SE_STREAM_ENCODING_AUDIO_MIXER1,

    STM_SE_STREAM_ENCODING_VIDEO_FIRST,
    STM_SE_STREAM_ENCODING_VIDEO_AUTO = STM_SE_STREAM_ENCODING_VIDEO_FIRST,
    STM_SE_STREAM_ENCODING_VIDEO_NONE,
    STM_SE_STREAM_ENCODING_VIDEO_MPEG1,
    STM_SE_STREAM_ENCODING_VIDEO_MPEG2,
    STM_SE_STREAM_ENCODING_VIDEO_AVS,
    STM_SE_STREAM_ENCODING_VIDEO_MJPEG,
    STM_SE_STREAM_ENCODING_VIDEO_DIVX3,
    STM_SE_STREAM_ENCODING_VIDEO_DIVX4,
    STM_SE_STREAM_ENCODING_VIDEO_DIVX5,
    STM_SE_STREAM_ENCODING_VIDEO_DIVXHD,
    STM_SE_STREAM_ENCODING_VIDEO_MPEG4P2,
    STM_SE_STREAM_ENCODING_VIDEO_H264,
    STM_SE_STREAM_ENCODING_VIDEO_WMV,
    STM_SE_STREAM_ENCODING_VIDEO_VC1,
    STM_SE_STREAM_ENCODING_VIDEO_VC1_RP227SPMP,
    STM_SE_STREAM_ENCODING_VIDEO_H263,
    STM_SE_STREAM_ENCODING_VIDEO_FLV1,
    STM_SE_STREAM_ENCODING_VIDEO_VP6,
    STM_SE_STREAM_ENCODING_VIDEO_RMV,
    STM_SE_STREAM_ENCODING_VIDEO_DVP,
    STM_SE_STREAM_ENCODING_VIDEO_VP3,
    STM_SE_STREAM_ENCODING_VIDEO_THEORA,
    STM_SE_STREAM_ENCODING_VIDEO_VP8,
    STM_SE_STREAM_ENCODING_VIDEO_MVC,
    STM_SE_STREAM_ENCODING_VIDEO_HEVC,
    STM_SE_STREAM_ENCODING_VIDEO_CAP,
    STM_SE_STREAM_ENCODING_VIDEO_RAW,
    STM_SE_STREAM_ENCODING_VIDEO_UNCOMPRESSED,

    STM_SE_STREAM_ENCODING_VIDEO_LAST = STM_SE_STREAM_ENCODING_VIDEO_UNCOMPRESSED,
} stm_se_stream_encoding_t;

/*!
 * This type defines a frame encoding type.
 *
 * \warning Stability: Unstable
 */
typedef enum stm_se_picture_type_e
{
    STM_SE_PICTURE_TYPE_UNKNOWN,
    STM_SE_PICTURE_TYPE_I,
    STM_SE_PICTURE_TYPE_P,
    STM_SE_PICTURE_TYPE_B,
} stm_se_picture_type_t;

static inline const char *StringifyPictureType(stm_se_picture_type_t aPictureType)
{
    switch (aPictureType)
    {
        ENTRY(STM_SE_PICTURE_TYPE_UNKNOWN);
        ENTRY(STM_SE_PICTURE_TYPE_I);
        ENTRY(STM_SE_PICTURE_TYPE_P);
        ENTRY(STM_SE_PICTURE_TYPE_B);
    default: return "<unknown picture type>";
    }
}

/*!
 * This type describes the scan type of a video frame.
 */
typedef enum stm_se_scan_type_e
{
    STM_SE_SCAN_TYPE_PROGRESSIVE,
    STM_SE_SCAN_TYPE_INTERLACED
} stm_se_scan_type_t;

static inline const char *StringifyScanType(stm_se_scan_type_t aScanType)
{
    switch (aScanType)
    {
        ENTRY(STM_SE_SCAN_TYPE_PROGRESSIVE);
        ENTRY(STM_SE_SCAN_TYPE_INTERLACED);
    default: return "<unknown scan type>";
    }
}

/*!
 * This type enumerates the tick rate used in a time format.
 */
typedef enum stm_se_time_format_e
{
    TIME_FORMAT_US,     //!< Time base is in microseconds (1,000,000 ticks per second).
    TIME_FORMAT_PTS,    //!< PES time base (90,000 ticks per second).
    TIME_FORMAT_27MHz,  //!< Extended precision PCR time base (27,000,000 ticks per second).
} stm_se_time_format_t;

static inline const char *StringifyTimeFormat(stm_se_time_format_t aTimeFormat)
{
    switch (aTimeFormat)
    {
        ENTRY(TIME_FORMAT_US);
        ENTRY(TIME_FORMAT_PTS);
        ENTRY(TIME_FORMAT_27MHz);
    default: return "<unknown time format>";
    }
}

typedef enum
{
    SURFACE_FORMAT_UNKNOWN,
    SURFACE_FORMAT_MARKER_FRAME,
    SURFACE_FORMAT_AUDIO,
    SURFACE_FORMAT_VIDEO_420_MACROBLOCK,
    SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK,
    SURFACE_FORMAT_VIDEO_422_RASTER,
    SURFACE_FORMAT_VIDEO_420_PLANAR,
    SURFACE_FORMAT_VIDEO_420_PLANAR_ALIGNED,
    SURFACE_FORMAT_VIDEO_422_PLANAR,
    SURFACE_FORMAT_VIDEO_8888_ARGB,
    SURFACE_FORMAT_VIDEO_888_RGB,
    SURFACE_FORMAT_VIDEO_565_RGB,
    SURFACE_FORMAT_VIDEO_422_YUYV,
    SURFACE_FORMAT_VIDEO_420_RASTER2B,
} surface_format_t;

static inline const char *StringifySurfaceFormat(surface_format_t aSurfaceFormat)
{
    switch (aSurfaceFormat)
    {
        ENTRY(SURFACE_FORMAT_UNKNOWN);
        ENTRY(SURFACE_FORMAT_MARKER_FRAME);
        ENTRY(SURFACE_FORMAT_AUDIO);
        ENTRY(SURFACE_FORMAT_VIDEO_420_MACROBLOCK);
        ENTRY(SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK);
        ENTRY(SURFACE_FORMAT_VIDEO_422_RASTER);
        ENTRY(SURFACE_FORMAT_VIDEO_420_PLANAR);
        ENTRY(SURFACE_FORMAT_VIDEO_420_PLANAR_ALIGNED);
        ENTRY(SURFACE_FORMAT_VIDEO_422_PLANAR);
        ENTRY(SURFACE_FORMAT_VIDEO_8888_ARGB);
        ENTRY(SURFACE_FORMAT_VIDEO_888_RGB);
        ENTRY(SURFACE_FORMAT_VIDEO_565_RGB);
        ENTRY(SURFACE_FORMAT_VIDEO_422_YUYV);
        ENTRY(SURFACE_FORMAT_VIDEO_420_RASTER2B);
    default: return "<unknown surface format>";
    }
}

/*!
 * This type defines the fixed-point representation of a linear gain factor within the range [0.0, 8.0.[This
 * corresponds to a dB gain within the range]-inf, +18 dB]
 *
 * The computation of a linear gain into the Q3.13 fixed-point notation is achieved with the following
 * equation:
 *
 * \code
 * stm_se_q3dot13_t convert_linear_gain_to_q3_13(float f_gain) {
 *  return (stm_se_q3dot13_t) (f_gain * (1 << 13));
 * }
 * \endcode
 *
 * Examples :
 *
 * \code
 * f_gain = 0.0  (-inf dB) ==> i_gain = 0x0000
 * f_gain = 0.5  (- 6  dB) ==> i_gain = 0x1000
 * f_gain = 1.0  (  0  dB) ==> i_gain = 0x2000
 * f_gain = 1.4  ( +3  dB) ==> i_gain = 0x2CCD
 * f_gain = 7.999(+18  dB) ==> i_gain = 0xFFFF
 * \endcode
 */
typedef unsigned short stm_se_q3dot13_t;

/*!
 * Describes the audio bit-rate allocation types
 *
 * A CBR bit-allocation process guarantees that on each considered period of time the number of bit used
 * will be exactly bit-rate * time.
 *
 * - In the case of ac-3 the period of time is covered by 2*1536 samples at 44.1kHz frequencies,
 *   1536 samples otherwise.
 * - In the case of mpeg1/2LIII the period is 3*2*576 for mono streams, 3*2*576 samples other wise.
 *
 * An ABR bit-allocation guarantees only over very long period of times will the number of bits used be
 * bit-rate * time
 *
 * A VBR bit-allocation process is where in order to reach a certain set quality up to the maximum number
 * of bits per frame allowed in the standard are used.
 */
typedef struct stm_se_audio_bitrate_control_s
{
    /*!
     * If (true and the codec supports it), a VBR allocation is used, in which case,
     * vbr_quality_factor is used.
     *
     * Otherwise depending of the codec either a CBR or ABR allocation is used in which case bitrate
     * is used.
     */
    bool        is_vbr;

    /*!
     * The target bitrate in bps for non VBR modes.
     *
     * Reserved if is_vbr is true.
     */
    uint32_t    bitrate;

    /*!
     * Quality factor for VBR modes.
     *
     * For MP3 and AAC the quality factor is expressed from 0 (lowest quality) to
     * 100 (highest quality).
     *
     * Reserved if is_vbr is false.
     */
    uint32_t    vbr_quality_factor;

    /*!
     * For VBR and ABR mode only: it set then this field indicated the instant bitrate in bps that should not be
     * exceeded.
     *
     * If set to 0 if there is no cap (pure VBR or ABR).
     */
    uint32_t    bitrate_cap;
} stm_se_audio_bitrate_control_t;

/*!
 * Describes individual channel placement in a buffer or a stream.
 */
typedef struct stm_se_audio_channel_placement_s
{
    /*!
     * The number of channels in the buffer (the buffer width) is defined by channel_count.
     * Parsing the chan[] array will be from chan[0] to chan[channel_count]
     */
    uint8_t channel_count;

    /*!
     * An array of (uint8_t)::stm_se_audio_channel_id_t, indicating each channel's semantic.
     */
    uint8_t chan[STM_SE_MAX_NUMBER_OF_AUDIO_CHAN];
} stm_se_audio_channel_placement_t;

/*!
 * Describes the core parameters of an audio stream or buffer, independently of any codec, quantization.
 *
 * In addition to its role in describing data this type is also used
 * used by \ref STM_SE_CTRL_AUDIO_GENERATOR_BUFFER as a control value.
 */
typedef struct stm_se_audio_core_format_s
{
    /*!
     * Sample rate in Hz
     */
    uint32_t sample_rate;

    /*!
     * Description of the audio channel content
     *
     * For an uncompressed buffer (including an
     * \ref STM_SE_CTRL_AUDIO_INPUT_FORMAT "audio generator input buffer" it
     * describes both the channels semantics and their positions
     *
     * For a compressed buffer is describes only their semantics, since position
     * is defined by the codec standard.
     */
    stm_se_audio_channel_placement_t channel_placement;
} stm_se_audio_core_format_t;

/*!
 * The frame rate, expressed as a rational fraction.
 */
typedef struct stm_se_framerate_s
{
    uint32_t                framerate_num; //!< Numerator
    uint32_t                framerate_den; //!< Denominator
} stm_se_framerate_t;

/*!
 * Describes a rectangle within a picture.
 *
 * This is used to describe rectangles within frames such as the window
 * of interest (see \ref stm_se_uncompressed_frame_metadata_video_s).
 */
typedef struct stm_se_picture_rectangle_s
{
    unsigned int                x;
    unsigned int                y;
    unsigned int                width;
    unsigned int                height;
} stm_se_picture_rectangle_t;

/*!
 * Describes the size of a picture.
 */
typedef struct stm_se_picture_resolution_s
{
    unsigned int                width;
    unsigned int                height;
} stm_se_picture_resolution_t;


/*!
 * See ::stm_se_play_stream_msg_s.
 *
 * 3d_frame_packing is an enumeration that describes the stereoscopic encoded format and can be set to the
 * following values :
 *
 * * ::STM_SE_FORMAT_3D_NONE
 *
 * The decoded video stream is not stereoscopic.
 *
 * * ::STM_SE_FORMAT_3D_FRAME_SEQUENTIAL,
 *
 * Each frame is individually representative of either the left or of the right view.
 *
 * * ::STM_SE_FORMAT_3D_STACKED_HALF,
 *
 * The frames are split into 2 areas. The top side of the frames is representative of either the left or
 * right view, the bottom side is representative of the other view.
 *
 * * ::STM_SE_FORMAT_3D_SIDE_BY_SIDE_HALF
 *
 * The frames are split into 2 areas. The left side of the frames is representative of the either the left
 * or the right view, the right side is representative of the other view.
 *
 * left_right_format is a Boolean that informs
 *
 * which side of the frame is representative of the left view. If set to true the either left side or top
 * side of the frames is representative of the left view.
 *
 * which layer is representative of the left view. In case of MVC video decoding, the base layer is
 * representative of the left view if left_right_format is set to true.
 */
typedef struct stm_se_play_stream_video_parameters_s
{
    unsigned int                      width;
    unsigned int                      height;
    stm_se_aspect_ratio_t             aspect_ratio;
    stm_se_scan_type_t                scan_type;
    unsigned int                      pixel_aspect_ratio_numerator;
    unsigned int                      pixel_aspect_ratio_denominator;
    stm_se_format_3d_t                format_3d;
    bool                              left_right_format;
    stm_se_colorspace_t               colorspace;
    unsigned int                      frame_rate;
} stm_se_play_stream_video_parameters_t;

/*!
 * This type describes pairs of speakers that can take part in speaker topologies.
 *
 * Forms part of the ::stm_se_audio_channel_assignment_t structure.
 */
typedef enum stm_se_audio_channel_pair                               /* pair0   pair1   pair2   pair3   pair4   */
{
    STM_SE_AUDIO_CHANNEL_PAIR_DEFAULT,                   /* 0 Y       Y       Y       Y       Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_L_R = 0,                   /*   Y                               Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CNTR_LFE1 = 0,             /*           Y                             */
    STM_SE_AUDIO_CHANNEL_PAIR_LSUR_RSUR = 0,             /*                   Y                     */
    STM_SE_AUDIO_CHANNEL_PAIR_LSURREAR_RSURREAR = 0,     /*                           Y             */

    STM_SE_AUDIO_CHANNEL_PAIR_LT_RT,                     /* 1 Y                               Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_LPLII_RPLII,               /*   Y                               Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CNTRL_CNTRR,               /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_LHIGH_RHIGH,               /* 4                                 Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_LWIDE_RWIDE,               /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_LRDUALMONO,                /*   Y                                     */
    STM_SE_AUDIO_CHANNEL_PAIR_RESERVED1,                 /*                                         */

    STM_SE_AUDIO_CHANNEL_PAIR_CNTR_0,                    /* 8         Y                             */
    STM_SE_AUDIO_CHANNEL_PAIR_0_LFE1,                    /*           Y                             */
    STM_SE_AUDIO_CHANNEL_PAIR_0_LFE2,                    /*           Y                             */
    STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_0,                   /*           Y                             */
    STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_0,               /*12         Y                             */

    STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CSURR,                /*           Y                             */
    STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CHIGH,                /*           Y                             */
    STM_SE_AUDIO_CHANNEL_PAIR_CNTR_TOPSUR,               /*           Y                             */
    STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CHIGHREAR,            /*16         Y                             */
    STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CLOWFRONT,            /*           Y                             */

    STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_TOPSUR,              /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_CHIGHREAR,           /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_CLOWFRONT,           /*20                                 Y     */

    STM_SE_AUDIO_CHANNEL_PAIR_CNTR_LFE2,                 /*           Y                             */
    STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_LFE1,                /*           Y                       Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_LFE2,                /*           Y                       Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_LFE1,            /*24         Y                             */
    STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_LFE2,            /*           Y                             */

    STM_SE_AUDIO_CHANNEL_PAIR_LSIDESURR_RSIDESURR,       /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_LHIGHSIDE_RHIGHSIDE,       /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_LDIRSUR_RDIRSUR,           /*28                                 Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_LHIGHREAR_RHIGHREAR,       /*                                   Y     */

    STM_SE_AUDIO_CHANNEL_PAIR_CSURR_0,                   /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_TOPSUR_0,                  /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CSURR_TOPSUR,              /*32                                 Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CHIGH,               /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CHIGHREAR,           /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CLOWFRONT,           /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CSURR_LFE1,                /*36                                 Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CSURR_LFE2,                /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_CHIGHREAR_0,               /*                                   Y     */
    STM_SE_AUDIO_CHANNEL_PAIR_DSTEREO_LsRs,              /*                           Y             */

    STM_SE_AUDIO_CHANNEL_PAIR_PAIR0,                     /*40 Y                                     */
    STM_SE_AUDIO_CHANNEL_PAIR_PAIR1,                     /*   Y                                     */
    STM_SE_AUDIO_CHANNEL_PAIR_PAIR2,                     /*   Y                                     */
    STM_SE_AUDIO_CHANNEL_PAIR_PAIR3,                     /*   Y                                     */

    STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED,             /*44 Y       Y       Y       Y       Y     */
} stm_se_audio_channel_pair_t;

/*!
 * This type defines the mapping between a channel number and the audio held within it.
 */
typedef struct stm_se_audio_channel_assignment
{
    unsigned int pair0: 6; //!< ::stm_se_audio_channel_pair_t for channels 0 and 1
    unsigned int pair1: 6; //!< ::stm_se_audio_channel_pair_t for channels 2 and 3
    unsigned int pair2: 6; //!< ::stm_se_audio_channel_pair_t for channels 4 and 5
    unsigned int pair3: 6; //!< ::stm_se_audio_channel_pair_t for channels 6 and 7
    unsigned int pair4: 6; //!< ::stm_se_audio_channel_pair_t for channels 8 and 9

    unsigned int reserved0: 1; //!< Shall be set to 0

    /*!
     * Used to describe speaker topologies that can dynamically adjust to the input provided (e.g.
     * multichannel HDMI receivers)
     */
    unsigned int malleable: 1;
} stm_se_audio_channel_assignment_t;

/*! @} */ /* streaming_engine */

/*
 * These types require declaring early to make the headers parseable
 * (they are used by some of the internal interfaces) but are documented
 * using \typedef in the corresponding header file since they do not
 * belong in the streaming_engine group.
 */
typedef void           *stm_se_playback_h;
typedef void           *stm_se_play_stream_h;
typedef void           *stm_se_encode_h;
typedef void           *stm_se_encode_stream_h;

#endif /* STM_SE_TYPES_H_ */
