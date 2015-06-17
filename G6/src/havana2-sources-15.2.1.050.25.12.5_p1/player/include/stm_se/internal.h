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

#ifndef STM_SE_INTERNAL_H_
#define STM_SE_INTERNAL_H_

#include <stm_registry.h>
#include <stm_se/types.h>

/*!
 * \defgroup internal_interfaces Internal interfaces
 * \warning internal
 *
 * These interfaces are not for application (or middleware use) and should
 * only be used by other components within the STKPI implementation.
 *
 * @{
 */

#define STM_SE_PLAY_FRAME_RATE_MULTIPLIER       1000

typedef struct statistic_fields_s
{
    unsigned int             Count;
    unsigned long long           Total;
    unsigned long long           Longest;
    unsigned long long           Shortest;
} statistic_fields_t;

typedef struct buffer_pool_level_s
{
    unsigned int              buffers_in_pool;
    unsigned int              buffers_with_non_zero_reference_count;
    unsigned int              memory_in_pool;
    unsigned int              memory_allocated;
    unsigned int              memory_in_use;
    unsigned int              largest_free_memory_block;
} buffer_pool_level;

typedef struct AttributeDescriptor_s
{
    int                                 Id;

    union
    {
        const char                     *ConstCharPointer;
        int                             Int;
        unsigned long long int          UnsignedLongLongInt;
        bool                            Bool;
    } u;

} AttributeDescriptor_t;

typedef struct attributes_s
{
    AttributeDescriptor_t input_format;
    AttributeDescriptor_t decode_errors;
    AttributeDescriptor_t number_channels;
    AttributeDescriptor_t sample_frequency;
    AttributeDescriptor_t number_of_samples_processed;
    AttributeDescriptor_t supported_input_format;
} attributes_t;

struct __stm_se_playback_statistics_s
{
    unsigned int              clock_recovery_accumulated_points;
    int                       clock_recovery_clock_adjustment;
    unsigned int              clock_recovery_cummulative_discarded_points;
    unsigned int              clock_recovery_cummulative_discard_resets;
    int                       clock_recovery_actual_gradient;
    int                       clock_recovery_established_gradient;
    unsigned int              clock_recovery_integration_time_window;
    unsigned int              clock_recovery_integration_time_elapsed;
};

// copy of MAXIMUM_MANIFESTATION_TIMING_COUNT defined in player_types.h
#define STM_SE_MAXIMUM_MANIFESTATION_TIMING_COUNT 24

struct statistics_s
{
    unsigned int                count;
    statistic_fields_t          delta_entry_into_process_0;
    statistic_fields_t          delta_entry_into_process_1;
    statistic_fields_t          delta_entry_into_process_2;
    statistic_fields_t          delta_entry_into_process_3;

    statistic_fields_t          traverse_0_to_1;
    statistic_fields_t          traverse_1_to_2;
    statistic_fields_t          traverse_2_to_3;

    statistic_fields_t          frame_time_in_process_1;
    statistic_fields_t          frame_time_in_process_2;
    statistic_fields_t          total_traversal_time;

    unsigned int              frame_count_launched_decode;
    unsigned int              vid_frame_count_launched_decode_I;
    unsigned int              vid_frame_count_launched_decode_P;
    unsigned int              vid_frame_count_launched_decode_B;

    unsigned int              frame_count_decoded;

    unsigned int              frame_count_manifested;
    unsigned int              frame_count_to_manifestor;
    unsigned int              frame_count_from_manifestor;

    unsigned int              frame_count_parser_error;
    unsigned int              frame_count_parser_nostreamparameters_error;
    unsigned int              frame_count_parser_partialframeparameters_error;
    unsigned int              frame_count_parser_unhandledheader_error;
    unsigned int              frame_count_parser_headersyntax_error;
    unsigned int              frame_count_parser_headerunplayable_error;
    unsigned int              frame_count_parser_streamsyntax_error;
    unsigned int              frame_count_parser_failedtocreatereverseplaystacks_error;
    unsigned int              frame_count_parser_failedtoallocatebuffer_error;
    unsigned int              frame_count_parser_referencelistconstructiondeferred_error;
    unsigned int              frame_count_parser_insufficientreferenceframes_error;
    unsigned int              frame_count_parser_streamunplayable_error;

    unsigned int              frame_count_decode_error;
    unsigned int              frame_count_decode_mb_overflow_error;
    unsigned int              frame_count_decode_recovered_error;
    unsigned int              frame_count_decode_not_recovered_error;
    unsigned int              frame_count_decode_task_timeout_error;

    unsigned int              frame_count_codec_error;
    unsigned int              frame_count_codec_unknownframe_error;

    unsigned int              dropped_before_decode_window_singlegroupplayback;
    unsigned int              dropped_before_decode_window_keyframesonly;
    unsigned int              dropped_before_decode_window_outsidepresentationinterval;
    unsigned int              dropped_before_decode_window_trickmodenotsupported;
    unsigned int              dropped_before_decode_window_trickmode;
    unsigned int              dropped_before_decode_window_others;
    unsigned int              dropped_before_decode_window_total;

    unsigned int              dropped_before_decode_singlegroupplayback;
    unsigned int              dropped_before_decode_keyframesonly;
    unsigned int              dropped_before_decode_outsidepresentationinterval;
    unsigned int              dropped_before_decode_trickmodenotsupported;
    unsigned int              dropped_before_decode_trickmode;
    unsigned int              dropped_before_decode_others;
    unsigned int              dropped_before_decode_total;

    unsigned int              dropped_before_output_timing_outsidepresentationinterval;
    unsigned int              dropped_before_output_timing_others;
    unsigned int              dropped_before_output_timing_total;

    unsigned int              dropped_before_manifestation_singlegroupplayback;
    unsigned int              dropped_before_manifestation_toolateformanifestation;
    unsigned int              dropped_before_manifestation_trickmodenotsupported;
    unsigned int              dropped_before_manifestation_others;
    unsigned int              dropped_before_manifestation_total;

    int                       video_framerate_integerpart;
    int                       video_framerate_remainderdecimal;

    unsigned int              buffer_count_from_collator;
    unsigned int              buffer_count_to_frame_parser;

    unsigned int              collator_audio_elementry_sync_lost_count;
    unsigned int              collator_audio_pes_sync_lost_count;

    unsigned int              frame_count_from_frame_parser;
    unsigned int              frame_parser_audio_sample_rate;
    unsigned int              frame_parser_audio_frame_size;

    unsigned int              frame_count_to_codec;
    unsigned int              frame_count_from_codec;
    unsigned int              codec_audio_coding_mode;
    unsigned int              codec_audio_sampling_frequency;
    unsigned int              codec_audio_num_of_output_samples;
    unsigned int              manifestor_audio_mixer_starved;

    unsigned long             max_decode_video_hw_time;
    unsigned long             min_decode_video_hw_time;
    unsigned long             avg_decode_video_hw_time;

    buffer_pool_level         coded_frame_buffer_pool;
    buffer_pool_level         decode_buffer_pool;

    unsigned int              h264_preproc_error_sc_detected;
    unsigned int              h264_preproc_error_bit_inserted;
    unsigned int              h264_preproc_intbuffer_overflow;
    unsigned int              h264_preproc_bitbuffer_underflow;
    unsigned int              h264_preproc_bitbuffer_overflow;
    unsigned int              h264_preproc_read_errors;
    unsigned int              h264_preproc_write_errors;

    unsigned int              hevc_preproc_error_sc_detected;
    unsigned int              hevc_preproc_error_eos;
    unsigned int              hevc_preproc_error_end_of_dma;
    unsigned int              hevc_preproc_error_range;
    unsigned int              hevc_preproc_error_entropy_decode;

    int                       output_rate_gradient[STM_SE_MAXIMUM_MANIFESTATION_TIMING_COUNT];
    unsigned int              output_rate_frames_to_integrate_over[STM_SE_MAXIMUM_MANIFESTATION_TIMING_COUNT];
    unsigned int              output_rate_integration_count[STM_SE_MAXIMUM_MANIFESTATION_TIMING_COUNT];
    int                       output_rate_clock_adjustment[STM_SE_MAXIMUM_MANIFESTATION_TIMING_COUNT];

    unsigned long long        system_time;
    unsigned long long        presentation_time;
    unsigned long long        pts;
    unsigned long long        sync_error;
    unsigned long long        output_sync_error_0;
    unsigned long long        output_sync_error_1;

    unsigned int           codec_frame_length;
    unsigned int           codec_num_of_output_channels;
    unsigned int           dolbypulse_id_count;
    unsigned int           dolbypulse_sbr_present;
    unsigned int              dolbypulse_ps_present;
};

typedef struct encode_stream_statistics_s
{
    //Buffer count injected at preproc input (at early stage with discontinuity)
    unsigned int        buffer_count_to_preproc;
    //Frame buffer count injected at preproc input (pure discontinuity excluded)
    unsigned int        frame_count_to_preproc;
    //Discontinuity buffer count injected at preproc input (inject_discontinuity API)
    unsigned int        discontinuity_count_to_preproc;
    //Buffer count output from preproc (including discontinuity)
    unsigned int        buffer_count_from_preproc;
    //Discontinuity buffer count output from preproc (EOS, video closed gop request)
    unsigned int        discontinuity_count_from_preproc;

    //Buffer count injected at coder input (should be equal to frame_count_from_preproc + discontinuity_count_from_preproc)
    unsigned int        buffer_count_to_coder;
    //Frame buffer count output from coder (eos discontinuity excluded)
    unsigned int        frame_count_from_coder;
    //Eos buffer count from coder
    unsigned int        eos_count_from_coder;
    //Video skipped frame count from video coder
    unsigned int        video_skipped_count_from_coder;

    //Buffer count injected at transporter input (with eos)
    unsigned int        buffer_count_to_transporter;
    //Buffer count output from transporter (with eos)
    unsigned int        buffer_count_from_transporter;
    //EOS Buffer count output from transporter
    unsigned int        null_size_buffer_count_from_transporter;
    //Release callback received from TsMux
    unsigned int        release_buffer_count_from_tsmux_transporter;

    //errors counters
    unsigned int        tsmux_queue_error;
    unsigned int        tsmux_transporter_buffer_address_error;
    unsigned int        tsmux_transporter_unexpected_released_buffer_error;
    unsigned int        tsmux_transporter_ring_extract_error;
} encode_stream_statistics_t;

/* The statistics functions are prefixed with underscores to indicate they
 * should not be exported beyond this module.
 *
 * They are called by the wrapper's sysfs logic which is written in C. That
 * code must therefore use the same mechanisms to cross from C to C++ as
 * everyone else (i.e. make wrapper function calls) but these symbols will
 * not be exported to anyone else.
 */
int             __stm_se_playback_reset_statistics(stm_se_playback_h    Playback);
int             __stm_se_playback_get_statistics(stm_se_playback_h    Playback,
                                                 struct __stm_se_playback_statistics_s     *Statistics);
int             __stm_se_play_stream_reset_statistics(stm_se_play_stream_h    Stream);
int             __stm_se_play_stream_get_statistics(stm_se_play_stream_h    Stream,
                                                    struct statistics_s     *Statistics);
int             __stm_se_play_stream_reset_attributes(stm_se_play_stream_h    Stream);
int             __stm_se_play_stream_get_attributes(stm_se_play_stream_h    Stream,
                                                    struct attributes_s     *Attributes);

int             __stm_se_encode_stream_reset_statistics(stm_se_encode_stream_h    EncodeStream);
int             __stm_se_encode_stream_get_statistics(stm_se_encode_stream_h encode_stream,
                                                      struct encode_stream_statistics_s *Statistics);

/*
 * This function is used to specify MME transformerName that should be used
 * for a given mixer specified by mixerId parameter. If the requested transformer
 * is not available/valid, a fallback transformer is selected.
 *
 * \param[in]  mixerId            Id of the Mixer, must be inferior to MAX_MIXERS
 * \param[in]  transformerName    Requested transformer name for that Mixer
 *
 * \retval 0       No error
 * \retval -EINVAL invalid Mixer ID
 * \retval -ENODEV Mixer ID has not been created
 */
int             __stm_se_audio_mixer_update_transformer_id(unsigned int mixerId,
                                                           const char *transformerName);

//

typedef enum __stm_se_low_power_mode_e
{
    STM_SE_LOW_POWER_MODE_HPS,
    STM_SE_LOW_POWER_MODE_CPS
} __stm_se_low_power_mode_t;

/* The power management functions are prefixed with underscores to indicate they
 * should not be exported beyond this module.
 *
 * They are called by the player module logic which is written in C. That
 * code must therefore use the same mechanisms to cross from C to C++ as
 * everyone else (i.e. make wrapper function calls) but these symbols will
 * not be exported to anyone else.
 */
int __stm_se_pm_low_power_enter(__stm_se_low_power_mode_t low_power_mode);
int __stm_se_pm_low_power_exit(void);

//

/*!
 * This type describes the audio specific meta-data linked to an uncompressed frame.
 *
 * StreamingEngine only supports buffers of interlaced audio channels, without padding between samples
 * except where explicitly marked by stm_se_audio_chan_t::STM_SE_AUDIO_CHAN_STUFFING in the core_format channel_assignment.
 */
typedef struct stm_se_uncompressed_frame_metadata_audio_s
{
    /*!
     * Core audio format description
     */
    stm_se_audio_core_format_t        core_format;

    /*!
     * PCM sample format.
     *
     * Indicates the quantization as well as the number of bits used for storage.
     */
    stm_se_audio_pcm_format_t         sample_format;
    /*!
     * Describes the average loudness level of the stream
     *
     * Expressed in millibels, describes the average loudness of the data the metadata applies to.
     * Typical values are -3100 (-31dB), -2000 (-21dB), -1800 (-18dB).
     */
    int16_t                           program_level;

    /*!
     * Describes whether any type of pre-emphasis has been applied on this buffer.
     */
    stm_se_emphasis_type_t            emphasis;
} stm_se_uncompressed_frame_metadata_audio_t;

/*!
 * This type describes the video specific meta-data linked to an uncompressed frame
 *
 * \warning Stability: Unstable
 */
typedef struct stm_se_uncompressed_frame_metadata_video_s
{
    /*!
     * Provides video information like resolution / scan_type / aspect ratio...
     */
    stm_se_play_stream_video_parameters_t   video_parameters;

    /*!
     * Enable to define a crop window for an injected frame
     *
     * In the context of injecting a frame to an encode stream, this information is ignored
     * if the STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING control has not been set.
     */
    stm_se_picture_rectangle_t              window_of_interest;

    /*!
     * Provides frame rate (in frames per second) for the stream being processed.
     */
    stm_se_framerate_t                      frame_rate;

    /*!
     * Provides buffer description related to line size (in bytes)
     */
    unsigned int                            pitch;

    /*!
     * Provides buffer description related to the chroma component offset in a 2 plans video buffer.
     * Vertical alignment describes the alignment in height to be respected to evaluate the start
     * address of the chroma component.
     */
    unsigned int                            vertical_alignment;

    /*!
     * Provides an indication on the original picture encoding type, in case the picture was processed
     * by an optional previous decoder.
     * May be taken into account by the encoder.
     */
    stm_se_picture_type_t                   picture_type;

    /*!
     * Provides information on surface format
     */
    surface_format_t                        surface_format;

    /*!
     * For interlaced frames only.
     * Provides information on which field should be rendered at first: top or bottom.
     */
    bool                                    top_field_first;
} stm_se_uncompressed_frame_metadata_video_t;

/*!
 * This type describes an uncompressed frame in sufficient detail to allow it to be processed further for
 * example for injection into an encode stream.
 *
 * \warning Stability: Unstable
 */
typedef struct stm_se_uncompressed_frame_metadata_s
{
    /*!
     * The presentation time stamp for this frame.
     * Value received in monotonic time units
     */
    uint64_t                      system_time;

    /*!
     * Describes the unit in which native_time is expressed
     */
    stm_se_time_format_t          native_time_format;

    /*!
     * The corresponding source presentation time stamp for this frame.
     */
    uint64_t                      native_time;

    /*!
     * The in bytes of the data pointed by user_data_buffer_address
     */
    unsigned int                  user_data_size;
    void                         *user_data_buffer_address;

    /*!
     * Provides information on a possible discontinuity for the frame.
     *
     * STM_SE_DISCONTINUITY_EOS indicates this is the last frame of the sequence.
     * STM_SE_DISCONTINUITY_DISCONTINUOUS indicates a time discontinuity, so a timing break versus previous frames.
     */
    stm_se_discontinuity_t        discontinuity;

    stm_se_encode_stream_media_t  media;

    union
    {
        /*!
        * If an audio frame, the audio metadata attached to this uncompressed frame
         */
        stm_se_uncompressed_frame_metadata_audio_t audio;

        /*!
        * If a video frame, the video metadata attached to this uncompressed frame
         */
        stm_se_uncompressed_frame_metadata_video_t video;
    };
} stm_se_uncompressed_frame_metadata_t;


/*!
 * This type describes the audio specific metadata required to use a compressed audio frame.
 */
typedef struct stm_se_compressed_frame_metadata_audio_s
{
    stm_se_audio_core_format_t        core_format; //!< Core audio format description
    uint16_t                          drc_factor; //!< Reserved.
} stm_se_compressed_frame_metadata_audio_t;


/*!
 * This type describes the video specific metadata required to use a compressed audio frame.
 */
typedef struct stm_se_compressed_frame_metadata_video_s
{
    bool                          new_gop; //!< Set to true if encoded frame matches a gop start.
    bool                          closed_gop; //!< Provides information on the type of gop: can be open or closed gop
    stm_se_picture_type_t         picture_type; //!< Provides information on the type of encoded frame (I/P/B)
} stm_se_compressed_frame_metadata_video_t;

/*!
 * This structure contains all the data required to use a compressed frame, for a multiplex, or to further
 * process it.
 *
 * \warning Stability: Unstable
 */
typedef struct stm_se_compressed_frame_metadata_s
{
    /*!
     * Reserved.
     */
    uint64_t                        system_time;

    /*!
     * Describes the unit in which native_time is expressed
     */
    stm_se_time_format_t            native_time_format;

    /*!
     * The corresponding source presentation time stamp for this frame.
     *
     * It takes into account:
     * - encoder input time stamp
     * - encoder temporal filtering if any (currently concerns audio encode path only)
     */
    uint64_t                        native_time;

    /*!
     * Describes the unit in which encoded_time is expressed
     */
    stm_se_time_format_t            encoded_time_format;

    /*!
     * The presentation time stamp of the given encoded output frame.
     *
     * It takes into account:
     * - encoder input time stamp
     * - encoder temporal filtering if any (currently concerns audio encode path only)
     * - encoder synchronization if any (NRT transcode case)
     */
    uint64_t                        encoded_time;

    /*!
     * Provides information on a possible discontinuity for the frame.
     *
     * STM_SE_DISCONTINUITY_EOS indicates this is the last frame of the sequence.
     * STM_SE_DISCONTINUITY_DISCONTINUOUS indicates a time discontinuity, so a timing break versus previous frames.
     * STM_SE_DISCONTINUITY_FRAME_SKIPPED indicates this frame has been skipped by the encoder, involving a compressed buffer
     * with a null payload and suitable discontinuity information.
     */
    stm_se_discontinuity_t          discontinuity;

    /*!
     * The type of media contained.
     */
    stm_se_encode_stream_encoding_t encoding;

    union
    {
        /*!
         * If an audio frame, the audio metadata attached to this compressed frame
         */
        stm_se_compressed_frame_metadata_audio_t audio;

        /*!
         * If a video frame, the video metadata attached to this compressed frame
         */
        stm_se_compressed_frame_metadata_video_t video;
    };
} stm_se_compressed_frame_metadata_t;


typedef struct stm_se_capture_buffer_s
{
    // input : payload data
    void         *virtual_address;  // kernel address
    void         *physical_address; // may be zero
    unsigned int  buffer_length;

    // output : returned length
    unsigned int  payload_length;

    // Metadata description
    union
    {
        stm_se_compressed_frame_metadata_t    compressed;
        stm_se_uncompressed_frame_metadata_t  uncompressed;
    } u;
} stm_se_capture_buffer_t;

/* Interface registered by playback object to implement the
 * set-clock-data-point entry point. The source will use these functions to connect,
 * disconnect and supply clock data point.  */

#define STM_SE_CLOCK_DATA_POINT_INTERFACE_PUSH "stm-se-clock-data-point-push"

typedef struct stm_se_clock_data_point_interface_push_sink
{
    int (*connect)(stm_object_h src_object,
                   stm_object_h sink_object);
    int (*disconnect)(stm_object_h src_object,
                      stm_object_h sink_object);
    int (*set_clock_data_point_data)(stm_object_h sink_object,
                                     stm_se_time_format_t    time_format,
                                     bool                    new_sequence,
                                     unsigned long long      source_time,
                                     unsigned long long      system_time);

} stm_se_clock_data_point_interface_push_sink_t;

/*! @} */ /* internal_interfaces */

#endif /* STM_SE_INTERNAL_H_ */
