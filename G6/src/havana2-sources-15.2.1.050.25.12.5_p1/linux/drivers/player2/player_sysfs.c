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

#include <linux/device.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/fs.h>

#include "stm_se.h"
#include "player_sysfs.h"

// sysfs statistics are considered as debug info:
// not compiled if CONFIG_DEBUG_FS not set..
#if defined(CONFIG_DEBUG_FS) || defined(SDK2_ENABLE_STATISTICS)
#define SE_ENABLE_STATISTICS
#endif

#ifdef SE_ENABLE_STATISTICS

static struct mutex SysfsWriteLock;
static struct mutex SysfsPlaybackLock;
static struct mutex SysfsEncodeLock;

enum attribute_id_e {
	ATTRIBUTE_ID_input_format,
	ATTRIBUTE_ID_decode_errors,
	ATTRIBUTE_ID_number_channels,
	ATTRIBUTE_ID_sample_frequency,
	ATTRIBUTE_ID_number_of_samples_processed,
	ATTRIBUTE_ID_supported_input_format,
	ATTRIBUTE_ID_MAX
};

struct StreamData_s {
	unsigned int                    Id;
	stm_se_media_t                  Media;
	stm_se_play_stream_h            Stream;
	struct device                   StreamClassDevice;
	dev_t                           StreamDev_t;
	bool                            NotifyOnDestroy; //!< True, if the stream has attributes
	bool                            UpdateStatistics;
	struct statistics_s             Statistics;
	bool                            StreamClassDeviceRegistered;
};

struct PlaybackData_s {
	unsigned int                        Id;
	stm_se_playback_h                   Playback;
	struct device                       PlaybackClassDevice;
	dev_t                               PlaybackDev_t;
	bool                                UpdateStatistics;
	struct __stm_se_playback_statistics_s Statistics;
	bool                                PlaybackClassDeviceRegistered;

	struct StreamData_s                 StreamData[STREAM_MAX_NUMBER];
};

struct EncodeStreamData_s {
	unsigned int                        Id;
	stm_se_encode_stream_media_t        Media;
	stm_se_encode_stream_h              Stream;
	struct device                       StreamClassDevice;
	dev_t                               StreamDev_t;
	bool                                NotifyOnDestroy; //!< True, if the stream has attributes
	bool                                UpdateStatistics;
	encode_stream_statistics_t          Statistics;
	bool                                StreamClassDeviceRegistered;
};

struct EncodeData_s {
	unsigned int                        Id;
	stm_se_encode_h                     Encode;
	struct device                       *EncodeClassDevice;
	dev_t                               EncodeDev_t;
	//no need identified yet for encode statistics
	struct EncodeStreamData_s           StreamData[ENCODE_STREAM_MAX_NUMBER];
};

static struct PlaybackData_s            PlaybackData[PLAYBACK_MAX_NUMBER];
static struct EncodeData_s              EncodeData[ENCODE_MAX_NUMBER];

/*{{{  class_attributes*/

static unsigned long NotifyCount = 0;

static ssize_t ShowNotifyCount(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", NotifyCount);
}

static struct class_attribute StreamingEngineClassAttributes[] = {
	__ATTR(notify, 0400, ShowNotifyCount, NULL),
	__ATTR_NULL,
};

static struct class StreamingEngineClass = {
		.name = "streaming_engine",
		.class_attrs = StreamingEngineClassAttributes,
	};

struct device *PlayerClassDevice;

/******************************
 * SYSFS TOOLS
 ******************************/

static int ResetStreamStatisticsForSysfs(struct StreamData_s *StreamData);
static int GetStreamStatisticsForSysfs(struct StreamData_s *StreamData);
struct StreamData_s *SysfsGetStreamData(struct device *StreamDev);
void SysfsStreamNewAttributeNotification(struct device *StreamDev);

static int ResetPlaybackStatisticsForSysfs(struct PlaybackData_s *PlaybackData);
static int GetPlaybackStatisticsForSysfs(struct PlaybackData_s *PlaybackData);
struct PlaybackData_s *SysfsGetPlaybackData(struct device *PlaybackDev);

//encode part
static int ResetEncodeStreamStatisticsForSysfs(struct EncodeStreamData_s *EncodeStreamData);
static int GetEncodeStreamStatisticsForSysfs(struct EncodeStreamData_s *EncodeStreamData);
struct EncodeStreamData_s *SysfsGetEncodeStreamData(struct device *StreamDev);
void SysfsEncodeStreamNewAttributeNotification(struct device *StreamDev);

static struct statistics_s *SysfsGetStreamStatistics(struct device *StreamDev)
{
	struct StreamData_s *StreamData = SysfsGetStreamData(StreamDev);

	if ((StreamData->Stream) && (StreamData->UpdateStatistics)) {
		GetStreamStatisticsForSysfs(StreamData);
	}

	return &(StreamData->Statistics);
}

static encode_stream_statistics_t *SysfsGetEncodeStreamStatistics(struct device *StreamDev)
{
	struct EncodeStreamData_s *EncodeStreamData = SysfsGetEncodeStreamData(StreamDev);

	if ((EncodeStreamData->Stream) && (EncodeStreamData->UpdateStatistics)) {
		GetEncodeStreamStatisticsForSysfs(EncodeStreamData);
	}

	return &(EncodeStreamData->Statistics);
}

// --------UpdateStatistics: This flag corresponds on how to get the statistics in DvbVideoSysfsShow... at real time or not.
//-----------------------------------------------------------------------------------------------------------------------------
static ssize_t SysfsShowUpdateStreamStatistics(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	if (SysfsGetStreamData(class_dev)->UpdateStatistics) {
		return scnprintf(buf, PAGE_SIZE, "Y\n");
	} else {
		return scnprintf(buf, PAGE_SIZE, "N\n");
	}
}

static ssize_t SysfsStoreUpdateStreamStatistics(struct device *class_dev, struct device_attribute *attr,
                                                const char *buf,
                                                size_t count)
{
	switch (buf[0]) {
	case 'u':
	case 'U':
		/* Update the statistics now - Useful for updates when UpdateStatistics == false */
		GetStreamStatisticsForSysfs(SysfsGetStreamData(class_dev));
		break;
	case 'y':
	case 'Y':
	case '1':
		SysfsGetStreamData(class_dev)->UpdateStatistics = true;
		break;
	case 'n':
	case 'N':
	case '0':
		SysfsGetStreamData(class_dev)->UpdateStatistics = false;
		break;
	default:
		pr_err("Error: %s Invalid input\n", __func__);
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(update_statistics, S_IRUGO | S_IWUSR , SysfsShowUpdateStreamStatistics,
                   SysfsStoreUpdateStreamStatistics);

static ssize_t SysfsShowUpdateEncodeStreamStatistics(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	if (SysfsGetEncodeStreamData(class_dev)->UpdateStatistics) {
		return scnprintf(buf, PAGE_SIZE, "Y\n");
	} else {
		return scnprintf(buf, PAGE_SIZE, "N\n");
	}
}

static ssize_t SysfsStoreUpdateEncodeStreamStatistics(struct device *class_dev, struct device_attribute *attr,
                                                      const char *buf,
                                                      size_t count)
{
	switch (buf[0]) {
	case 'u':
	case 'U':
		/* Update the statistics now - Useful for updates when UpdateStatistics == false */
		GetEncodeStreamStatisticsForSysfs(SysfsGetEncodeStreamData(class_dev));
		break;
	case 'y':
	case 'Y':
	case '1':
		SysfsGetEncodeStreamData(class_dev)->UpdateStatistics = true;
		break;
	case 'n':
	case 'N':
	case '0':
		SysfsGetEncodeStreamData(class_dev)->UpdateStatistics = false;
		break;
	default:
		pr_err("Error: %s Invalid input\n", __func__);
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(update_encode_stream_statistics, S_IRUGO | S_IWUSR , SysfsShowUpdateEncodeStreamStatistics,
                   SysfsStoreUpdateEncodeStreamStatistics);

// --------ResetStatistics: This flag reset all statistics.
//-----------------------------------------------------------------------------------------------------------------------------
static ssize_t SysfsShowResetStreamStatistics(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t SysfsStoreResetStreamStatistics(struct device *class_dev, struct device_attribute *attr, const char *buf,
                                               size_t count)
{
	unsigned int Val = 0;
	sscanf(buf, "%u\n", &Val);
	if (Val != 0) {
		ResetStreamStatisticsForSysfs(SysfsGetStreamData(class_dev));
	}

	return count;
}

static DEVICE_ATTR(reset_statistics, S_IRUGO | S_IWUSR , SysfsShowResetStreamStatistics,
                   SysfsStoreResetStreamStatistics);

//encode_stream part
static ssize_t SysfsShowResetEncodeStreamStatistics(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t SysfsStoreResetEncodeStreamStatistics(struct device *class_dev, struct device_attribute *attr,
                                                     const char *buf,
                                                     size_t count)
{
	unsigned int Val = 0;
	sscanf(buf, "%u\n", &Val);
	if (Val != 0) {
		ResetEncodeStreamStatisticsForSysfs(SysfsGetEncodeStreamData(class_dev));
	}

	return count;
}

static DEVICE_ATTR(reset_encode_stream_statistics, S_IRUGO | S_IWUSR , SysfsShowResetEncodeStreamStatistics,
                   SysfsStoreResetEncodeStreamStatistics);

// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE( LinuxName )                                                              \
static ssize_t SysfsShow_##LinuxName( struct device *class_dev, struct device_attribute *attr, char *buf ) \
{                                                                                               \
    int v = SysfsGetStreamStatistics(class_dev)->LinuxName;                                     \
    return scnprintf(buf, PAGE_SIZE, "%d\n", v);                                                \
}                                                                                               \
                                                                                                \
static DEVICE_ATTR(LinuxName, S_IRUGO, SysfsShow_##LinuxName, NULL)

#define SYSFS_DECLARE_ARRAY( LinuxName, Number )				\
static ssize_t SysfsShow_##LinuxName##_##Number( struct device *class_dev, struct device_attribute *attr, char *buf ) \
{                                                                                               \
    int v = SysfsGetStreamStatistics(class_dev)->LinuxName[Number];                                     \
    return scnprintf(buf, PAGE_SIZE, "%d\n", v);                                                \
}                                                                                               \
                                                                                                \
static DEVICE_ATTR(LinuxName##_##Number, S_IRUGO, SysfsShow_##LinuxName##_##Number, NULL)

//encode part
#define SYSFS_ENCODE_STREAM_DECLARE( LinuxName )                                                              \
static ssize_t SysfsShow_##LinuxName( struct device *class_dev, struct device_attribute *attr, char *buf ) \
{                                                                                               \
    int v = SysfsGetEncodeStreamStatistics(class_dev)->LinuxName;                                     \
    return scnprintf(buf, PAGE_SIZE, "%d\n", v);                                                \
}                                                                                               \
                                                                                                \
static DEVICE_ATTR(LinuxName, S_IRUGO, SysfsShow_##LinuxName, NULL)

#define SYSFS_ENCODE_STREAM_DECLARE_ARRAY( LinuxName, Number )				\
static ssize_t SysfsShow_##LinuxName##_##Number( struct device *class_dev, struct device_attribute *attr, char *buf ) \
{                                                                                               \
    int v = SysfsGetEncodeStreamStatistics(class_dev)->LinuxName[Number];                                     \
    return scnprintf(buf, PAGE_SIZE, "%d\n", v);                                                \
}                                                                                               \
                                                                                                \
static DEVICE_ATTR(LinuxName##_##Number, S_IRUGO, SysfsShow_##LinuxName##_##Number, NULL)
// ------------------------------------------------------------------------------------------------

static ssize_t SysfsShowVideoFrameRate(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	struct statistics_s *statistics = SysfsGetStreamStatistics(class_dev);
	return scnprintf(buf, PAGE_SIZE, "%d.%03d\n",
	                 statistics->video_framerate_integerpart,
	                 statistics->video_framerate_remainderdecimal);
}

static DEVICE_ATTR(video_framerate, S_IRUGO, SysfsShowVideoFrameRate, NULL);


// Buffer Level Reporting
// ------------------------------------------------------------------------------------------------

#define SYSFS_DECLARE_BUFFERLEVEL( Pool, Member )                                               \
static ssize_t SysfsShow_##Pool##_##Member( struct device *class_dev, struct device_attribute *attr, char *buf ) \
{                                                                                               \
    buffer_pool_level * level = &(SysfsGetStreamStatistics(class_dev)->Pool);                   \
    return sprintf(buf, "%d\n", level->Member);                                                 \
}                                                                                               \
                                                                                                \
static DEVICE_ATTR( Pool##_##Member, S_IRUGO, SysfsShow_##Pool##_##Member, NULL)


// Gradient (ppm) reporting
// ------------------------------------------------------------------------------------------------

#define SYSFS_DECLARE_GRADIENT( LinuxName, Number )				\
static ssize_t SysfsShow_##LinuxName##_##Number( struct device *class_dev, struct device_attribute *attr, char *buf ) \
{                                                                                               \
    int gradient_in_ppm_deviance_from_unity = SysfsGetStreamStatistics(class_dev)->LinuxName[Number];   \
    return sprintf(buf, "%dppm\n", gradient_in_ppm_deviance_from_unity);                        \
}                                                                                               \
                                                                                                \
static DEVICE_ATTR(LinuxName##_##Number, S_IRUGO, SysfsShow_##LinuxName##_##Number, NULL)

// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE_SIGNED_LONG_LONG( LinuxName )                                                               \
static ssize_t SysfsShow_##LinuxName( struct device *class_dev, struct device_attribute *attr, char *buf ) \
{                                                                                                          \
    long long v = SysfsGetStreamStatistics(class_dev)->LinuxName;                                 \
    return sprintf(buf, "%lld\n", v);                                                                      \
}                                                                                                          \
                                                                                                           \
static DEVICE_ATTR(LinuxName, S_IRUGO, SysfsShow_##LinuxName, NULL)

// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE_LONG_LONG( LinuxName )                                                               \
static ssize_t SysfsShow_##LinuxName( struct device *class_dev, struct device_attribute *attr, char *buf ) \
{                                                                                                          \
    unsigned long long v = SysfsGetStreamStatistics(class_dev)->LinuxName;                                 \
    return sprintf(buf, "%llu\n", v);                                                                      \
}                                                                                                          \
                                                                                                           \
static DEVICE_ATTR(LinuxName, S_IRUGO, SysfsShow_##LinuxName, NULL)

// ------------------------------------------------------------------------------------------------
SYSFS_DECLARE(frame_count_launched_decode);

SYSFS_DECLARE(vid_frame_count_launched_decode_I);
SYSFS_DECLARE(vid_frame_count_launched_decode_P);
SYSFS_DECLARE(vid_frame_count_launched_decode_B);

SYSFS_DECLARE(frame_count_decoded);

SYSFS_DECLARE(frame_count_manifested);
SYSFS_DECLARE(frame_count_to_manifestor);
SYSFS_DECLARE(frame_count_from_manifestor);

SYSFS_DECLARE(max_decode_video_hw_time);
SYSFS_DECLARE(min_decode_video_hw_time);
SYSFS_DECLARE(avg_decode_video_hw_time);

SYSFS_DECLARE(frame_count_parser_error);
SYSFS_DECLARE(frame_count_parser_nostreamparameters_error);
SYSFS_DECLARE(frame_count_parser_partialframeparameters_error);
SYSFS_DECLARE(frame_count_parser_unhandledheader_error);
SYSFS_DECLARE(frame_count_parser_headersyntax_error);
SYSFS_DECLARE(frame_count_parser_headerunplayable_error);
SYSFS_DECLARE(frame_count_parser_streamsyntax_error);
SYSFS_DECLARE(frame_count_parser_failedtocreatereverseplaystacks_error);
SYSFS_DECLARE(frame_count_parser_failedtoallocatebuffer_error);
SYSFS_DECLARE(frame_count_parser_referencelistconstructiondeferred_error);
SYSFS_DECLARE(frame_count_parser_insufficientreferenceframes_error);
SYSFS_DECLARE(frame_count_parser_streamunplayable_error);

SYSFS_DECLARE(frame_count_decode_error);
SYSFS_DECLARE(frame_count_decode_mb_overflow_error);
SYSFS_DECLARE(frame_count_decode_recovered_error);
SYSFS_DECLARE(frame_count_decode_not_recovered_error);
SYSFS_DECLARE(frame_count_decode_task_timeout_error);

SYSFS_DECLARE(frame_count_codec_error);
SYSFS_DECLARE(frame_count_codec_unknownframe_error);

SYSFS_DECLARE(dropped_before_decode_window_singlegroupplayback);
SYSFS_DECLARE(dropped_before_decode_window_keyframesonly);
SYSFS_DECLARE(dropped_before_decode_window_outsidepresentationinterval);
SYSFS_DECLARE(dropped_before_decode_window_trickmodenotsupported);
SYSFS_DECLARE(dropped_before_decode_window_trickmode);
SYSFS_DECLARE(dropped_before_decode_window_others);
SYSFS_DECLARE(dropped_before_decode_window_total);

SYSFS_DECLARE(dropped_before_decode_singlegroupplayback);
SYSFS_DECLARE(dropped_before_decode_keyframesonly);
SYSFS_DECLARE(dropped_before_decode_outsidepresentationinterval);
SYSFS_DECLARE(dropped_before_decode_trickmodenotsupported);
SYSFS_DECLARE(dropped_before_decode_trickmode);
SYSFS_DECLARE(dropped_before_decode_others);
SYSFS_DECLARE(dropped_before_decode_total);

SYSFS_DECLARE(dropped_before_output_timing_outsidepresentationinterval);
SYSFS_DECLARE(dropped_before_output_timing_others);
SYSFS_DECLARE(dropped_before_output_timing_total);

SYSFS_DECLARE(dropped_before_manifestation_singlegroupplayback);
SYSFS_DECLARE(dropped_before_manifestation_toolateformanifestation);
SYSFS_DECLARE(dropped_before_manifestation_trickmodenotsupported);
SYSFS_DECLARE(dropped_before_manifestation_others);
SYSFS_DECLARE(dropped_before_manifestation_total);

SYSFS_DECLARE(buffer_count_from_collator);
SYSFS_DECLARE(buffer_count_to_frame_parser);

SYSFS_DECLARE(collator_audio_elementry_sync_lost_count);
SYSFS_DECLARE(collator_audio_pes_sync_lost_count);

SYSFS_DECLARE(frame_count_from_frame_parser);
SYSFS_DECLARE(frame_parser_audio_sample_rate);
SYSFS_DECLARE(frame_parser_audio_frame_size);

SYSFS_DECLARE(frame_count_to_codec);
SYSFS_DECLARE(frame_count_from_codec);
SYSFS_DECLARE(codec_audio_coding_mode);
SYSFS_DECLARE(codec_audio_sampling_frequency);
SYSFS_DECLARE(codec_audio_num_of_output_samples);
SYSFS_DECLARE(manifestor_audio_mixer_starved);

SYSFS_DECLARE(dolbypulse_id_count);
SYSFS_DECLARE(dolbypulse_sbr_present);
SYSFS_DECLARE(dolbypulse_ps_present);
SYSFS_DECLARE(codec_frame_length);
SYSFS_DECLARE(codec_num_of_output_channels);

SYSFS_DECLARE_BUFFERLEVEL(coded_frame_buffer_pool, buffers_in_pool);
SYSFS_DECLARE_BUFFERLEVEL(coded_frame_buffer_pool, buffers_with_non_zero_reference_count);
SYSFS_DECLARE_BUFFERLEVEL(coded_frame_buffer_pool, largest_free_memory_block);
SYSFS_DECLARE_BUFFERLEVEL(coded_frame_buffer_pool, memory_allocated);
SYSFS_DECLARE_BUFFERLEVEL(coded_frame_buffer_pool, memory_in_pool);
SYSFS_DECLARE_BUFFERLEVEL(coded_frame_buffer_pool, memory_in_use);

SYSFS_DECLARE_BUFFERLEVEL(decode_buffer_pool, buffers_in_pool);
SYSFS_DECLARE_BUFFERLEVEL(decode_buffer_pool, buffers_with_non_zero_reference_count);
SYSFS_DECLARE_BUFFERLEVEL(decode_buffer_pool, largest_free_memory_block);
SYSFS_DECLARE_BUFFERLEVEL(decode_buffer_pool, memory_allocated);
SYSFS_DECLARE_BUFFERLEVEL(decode_buffer_pool, memory_in_pool);
SYSFS_DECLARE_BUFFERLEVEL(decode_buffer_pool, memory_in_use);

SYSFS_DECLARE(h264_preproc_error_sc_detected);
SYSFS_DECLARE(h264_preproc_error_bit_inserted);
SYSFS_DECLARE(h264_preproc_intbuffer_overflow);
SYSFS_DECLARE(h264_preproc_bitbuffer_underflow);
SYSFS_DECLARE(h264_preproc_bitbuffer_overflow);
SYSFS_DECLARE(h264_preproc_read_errors);
SYSFS_DECLARE(h264_preproc_write_errors);

SYSFS_DECLARE(hevc_preproc_error_sc_detected);
SYSFS_DECLARE(hevc_preproc_error_eos);
SYSFS_DECLARE(hevc_preproc_error_end_of_dma);
SYSFS_DECLARE(hevc_preproc_error_range);
SYSFS_DECLARE(hevc_preproc_error_entropy_decode);

SYSFS_DECLARE_GRADIENT(output_rate_gradient, 0);
SYSFS_DECLARE_GRADIENT(output_rate_gradient, 1);
SYSFS_DECLARE_GRADIENT(output_rate_gradient, 2);
SYSFS_DECLARE_GRADIENT(output_rate_gradient, 3);
SYSFS_DECLARE_GRADIENT(output_rate_gradient, 4);
SYSFS_DECLARE_GRADIENT(output_rate_gradient, 5);
SYSFS_DECLARE_GRADIENT(output_rate_gradient, 6);

SYSFS_DECLARE_ARRAY(output_rate_frames_to_integrate_over, 0);
SYSFS_DECLARE_ARRAY(output_rate_frames_to_integrate_over, 1);
SYSFS_DECLARE_ARRAY(output_rate_frames_to_integrate_over, 2);
SYSFS_DECLARE_ARRAY(output_rate_frames_to_integrate_over, 3);
SYSFS_DECLARE_ARRAY(output_rate_frames_to_integrate_over, 4);
SYSFS_DECLARE_ARRAY(output_rate_frames_to_integrate_over, 5);
SYSFS_DECLARE_ARRAY(output_rate_frames_to_integrate_over, 6);

SYSFS_DECLARE_ARRAY(output_rate_integration_count, 0);
SYSFS_DECLARE_ARRAY(output_rate_integration_count, 1);
SYSFS_DECLARE_ARRAY(output_rate_integration_count, 2);
SYSFS_DECLARE_ARRAY(output_rate_integration_count, 3);
SYSFS_DECLARE_ARRAY(output_rate_integration_count, 4);
SYSFS_DECLARE_ARRAY(output_rate_integration_count, 5);
SYSFS_DECLARE_ARRAY(output_rate_integration_count, 6);

SYSFS_DECLARE_GRADIENT(output_rate_clock_adjustment, 0);
SYSFS_DECLARE_GRADIENT(output_rate_clock_adjustment, 1);
SYSFS_DECLARE_GRADIENT(output_rate_clock_adjustment, 2);
SYSFS_DECLARE_GRADIENT(output_rate_clock_adjustment, 3);
SYSFS_DECLARE_GRADIENT(output_rate_clock_adjustment, 4);
SYSFS_DECLARE_GRADIENT(output_rate_clock_adjustment, 5);
SYSFS_DECLARE_GRADIENT(output_rate_clock_adjustment, 6);

SYSFS_DECLARE_LONG_LONG(pts);
SYSFS_DECLARE_LONG_LONG(presentation_time);
SYSFS_DECLARE_LONG_LONG(system_time);
SYSFS_DECLARE_SIGNED_LONG_LONG(sync_error);

SYSFS_DECLARE_SIGNED_LONG_LONG(output_sync_error_0);
SYSFS_DECLARE_SIGNED_LONG_LONG(output_sync_error_1);

//encode preproc counters
SYSFS_ENCODE_STREAM_DECLARE(buffer_count_to_preproc);
SYSFS_ENCODE_STREAM_DECLARE(frame_count_to_preproc);
SYSFS_ENCODE_STREAM_DECLARE(discontinuity_count_to_preproc);
SYSFS_ENCODE_STREAM_DECLARE(buffer_count_from_preproc);
SYSFS_ENCODE_STREAM_DECLARE(discontinuity_count_from_preproc);

//encode coder counters
SYSFS_ENCODE_STREAM_DECLARE(buffer_count_to_coder);
SYSFS_ENCODE_STREAM_DECLARE(frame_count_from_coder);
SYSFS_ENCODE_STREAM_DECLARE(eos_count_from_coder);
SYSFS_ENCODE_STREAM_DECLARE(video_skipped_count_from_coder);

//encode transporter counters
SYSFS_ENCODE_STREAM_DECLARE(buffer_count_to_transporter);
SYSFS_ENCODE_STREAM_DECLARE(buffer_count_from_transporter);
SYSFS_ENCODE_STREAM_DECLARE(null_size_buffer_count_from_transporter);
SYSFS_ENCODE_STREAM_DECLARE(release_buffer_count_from_tsmux_transporter);

//encode error counters
SYSFS_ENCODE_STREAM_DECLARE(tsmux_queue_error);
SYSFS_ENCODE_STREAM_DECLARE(tsmux_transporter_buffer_address_error);
SYSFS_ENCODE_STREAM_DECLARE(tsmux_transporter_unexpected_released_buffer_error);
SYSFS_ENCODE_STREAM_DECLARE(tsmux_transporter_ring_extract_error);
//
// playback statistics handling
//

static struct __stm_se_playback_statistics_s *SysfsGetPlaybackStatistics(struct device *PlaybackDev)
{
	struct PlaybackData_s *PlaybackData = SysfsGetPlaybackData(PlaybackDev);


	if ((PlaybackData->Playback) && (PlaybackData->UpdateStatistics)) {
		GetPlaybackStatisticsForSysfs(PlaybackData);
	}

	return &(PlaybackData->Statistics);
}

// --------UpdateStatistics: This flag corresponds on how to get the statistics in DvbVideoSysfsShow... at real time or not.
//-----------------------------------------------------------------------------------------------------------------------------

static ssize_t SysfsShowUpdatePlaybackStatistics(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	if (SysfsGetPlaybackData(class_dev)->UpdateStatistics) {
		return sprintf(buf, "Y\n");
	} else {
		return sprintf(buf, "N\n");
	}
}

static ssize_t SysfsStoreUpdatePlaybackStatistics(struct device *class_dev, struct device_attribute *attr,
                                                  const char *buf, size_t count)
{
	switch (buf[0]) {
	case 'u':
	case 'U':
		/* Update the statistics now - Useful for updates when UpdateStatistics == false */
		GetPlaybackStatisticsForSysfs(SysfsGetPlaybackData(class_dev));
		break;
	case 'y':
	case 'Y':
	case '1':
		SysfsGetPlaybackData(class_dev)->UpdateStatistics = true;
		break;
	case 'n':
	case 'N':
	case '0':
		SysfsGetPlaybackData(class_dev)->UpdateStatistics = false;
		break;
	default:
		pr_err("Error: %s Invalid input\n", __func__);
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(update_playback_statistics, S_IRUGO | S_IWUSR , SysfsShowUpdatePlaybackStatistics,
                   SysfsStoreUpdatePlaybackStatistics);


// --------ResetStatistics: This flag reset all statistics.
//-----------------------------------------------------------------------------------------------------------------------------
static ssize_t SysfsShowResetPlaybackStatistics(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
}

static ssize_t SysfsStoreResetPlaybackStatistics(struct device *class_dev, struct device_attribute *attr,
                                                 const char *buf, size_t count)
{
	unsigned int Val = 0;
	sscanf(buf, "%u\n", &Val);
	if (Val != 0) {
		ResetPlaybackStatisticsForSysfs(SysfsGetPlaybackData(class_dev));
	}

	return count;
}

static DEVICE_ATTR(reset_playback_statistics, S_IRUGO | S_IWUSR , SysfsShowResetPlaybackStatistics,
                   SysfsStoreResetPlaybackStatistics);

// Integer attributes (for playbacks)
// ------------------------------------------------------------------------------------------------

#define PLAYBACK_DECLARE_INTEGER_ATTRIBUTE( LinuxName )                                                              \
static ssize_t SysfsShow_##LinuxName( struct device *class_dev, struct device_attribute *attr, char *buf ) \
{                                                                                               \
    int v = SysfsGetPlaybackStatistics(class_dev)->LinuxName;                                   \
    return sprintf(buf, "%d\n", v);                                                             \
}                                                                                               \
                                                                                                \
static DEVICE_ATTR(LinuxName, S_IRUGO, SysfsShow_##LinuxName, NULL)

// Gradient attributes (for playbacks)
// ------------------------------------------------------------------------------------------------

#define PLAYBACK_DECLARE_GRADIENT_ATTRIBUTE( LinuxName )                                                  \
static ssize_t SysfsShow_##LinuxName( struct device *class_dev, struct device_attribute *attr, char *buf ) \
{                                                                                               \
    int gradient_in_ppm_deviance_from_unity = SysfsGetPlaybackStatistics(class_dev)->LinuxName;   \
    return sprintf(buf, "%dppm\n", gradient_in_ppm_deviance_from_unity);                        \
}                                                                                               \
                                                                                                \
static DEVICE_ATTR(LinuxName, S_IRUGO, SysfsShow_##LinuxName, NULL)

// Declare all the attributes for playbacks
// ------------------------------------------------------------------------------------------------

PLAYBACK_DECLARE_INTEGER_ATTRIBUTE(clock_recovery_accumulated_points);
PLAYBACK_DECLARE_GRADIENT_ATTRIBUTE(clock_recovery_clock_adjustment);
PLAYBACK_DECLARE_INTEGER_ATTRIBUTE(clock_recovery_cummulative_discarded_points);
PLAYBACK_DECLARE_INTEGER_ATTRIBUTE(clock_recovery_cummulative_discard_resets);
PLAYBACK_DECLARE_GRADIENT_ATTRIBUTE(clock_recovery_actual_gradient);
PLAYBACK_DECLARE_GRADIENT_ATTRIBUTE(clock_recovery_established_gradient);
PLAYBACK_DECLARE_INTEGER_ATTRIBUTE(clock_recovery_integration_time_window);
PLAYBACK_DECLARE_INTEGER_ATTRIBUTE(clock_recovery_integration_time_elapsed);


#define SYSFS_CREATE_ATTRIBUTE( LinuxName )                                                     \
{                                                                                               \
int Result;                                                                                     \
                                                                                                \
    Result  = device_create_file(&StreamData->StreamClassDevice,                                \
                      &dev_attr_##LinuxName);                                                   \
    if (Result) {                                                                               \
        pr_err("Error: %s device_create_file failed (%d)\n", __func__, Result);                                \
        return -1;                                                                              \
    }                                                                                           \
    SysfsStreamNewAttributeNotification(&StreamData->StreamClassDevice);                              \
}

/*{{{  show_generic_attribute*/
ssize_t show_generic_attribute(struct device *class_dev, char *buf, enum attribute_id_e attr_id, const char *attr_name)
{
	int Result = 0;
	struct StreamData_s        *Stream         = container_of(class_dev, struct StreamData_s, StreamClassDevice);
	struct attributes_s         AttributesData;

	if ((Stream == NULL) || (buf == NULL)) {
		return -EINVAL;
	}

	Result = __stm_se_play_stream_get_attributes(Stream->Stream, &AttributesData);
	if (Result < 0) {
		pr_err("Error: %s stream get attributes failed\n", __func__);
		return sprintf(buf, "%s not available\n", attr_name);
	}

	switch (attr_id) {
	case ATTRIBUTE_ID_input_format:
		if (AttributesData.input_format.Id == 0) { return (sprintf(buf, "n/a\n")); }
		return sprintf(buf, "%s\n", AttributesData.input_format.u.ConstCharPointer);
		break;
	case ATTRIBUTE_ID_number_channels:
		return sprintf(buf, "%s\n", AttributesData.number_channels.u.ConstCharPointer);
		break;
	case ATTRIBUTE_ID_decode_errors:
		if (AttributesData.decode_errors.Id == 0) { return (sprintf(buf, "n/a\n")); }
		return sprintf(buf, "%d\n", AttributesData.decode_errors.u.Int);
		break;
	case ATTRIBUTE_ID_sample_frequency:
		return sprintf(buf, "%d\n", AttributesData.sample_frequency.u.Int);
		break;
	case ATTRIBUTE_ID_number_of_samples_processed:
		if (AttributesData.number_of_samples_processed.Id == 0) { return (sprintf(buf, "n/a\n")); }
		return sprintf(buf, "%lld\n", AttributesData.number_of_samples_processed.u.UnsignedLongLongInt);
		break;
	case ATTRIBUTE_ID_supported_input_format:
		if (AttributesData.supported_input_format.Id == 0) { return (sprintf(buf, "n/a\n")); }
		return sprintf(buf, "%d\n", AttributesData.supported_input_format.u.Bool);
		break;
	default:
		return sprintf(buf, "%s not available\n", attr_name);
		break;
	}
}
/*}}}*/
/*{{{  store_generic_attribute*/
ssize_t store_generic_attribute(struct device *class_dev, const char *buf, enum attribute_id_e attr_id,
                                const char *attr_name, size_t count)
{
	int Result = 0;

	struct StreamData_s        *Stream         = container_of(class_dev, struct StreamData_s, StreamClassDevice);

	switch (attr_id) {
	case ATTRIBUTE_ID_decode_errors:
		break;
	default:
		return -EIO;
		break;
	}

	Result = __stm_se_play_stream_reset_attributes(Stream->Stream);
	if (Result < 0) {
		pr_err("Error: %s stream reset attributes failed\n", __func__);
		return -EIO;
	}

	return count;
}
/*}}}*/

#define SHOW(x)                                                                 \
    ssize_t show_ ## x (struct device *class_dev, struct device_attribute *attr, char *buf)              \
    {                                                                           \
        return show_generic_attribute(class_dev, buf, ATTRIBUTE_ID_ ## x, #x);  \
    }

#define STORE(x)                                                                \
    ssize_t store_ ## x (struct device *class_dev, struct device_attribute *attr, const char *buf, size_t count)                 \
    {                                                                           \
        return store_generic_attribute(class_dev, buf, ATTRIBUTE_ID_ ## x, #x, count);\
    }

/* creation of show_xx methods */

SHOW(input_format);
SHOW(decode_errors);
SHOW(number_channels);
SHOW(sample_frequency);
SHOW(supported_input_format);
SHOW(number_of_samples_processed);

/* creation of store_xx methods */

STORE(decode_errors);

/* creation of device_attr_xx attributes */

DEVICE_ATTR(input_format, S_IRUGO, show_input_format, NULL);
DEVICE_ATTR(supported_input_format, S_IRUGO, show_supported_input_format, NULL);
DEVICE_ATTR(decode_errors, S_IWUSR | S_IRUGO, show_decode_errors, store_decode_errors);
DEVICE_ATTR(sample_frequency, S_IRUGO, show_sample_frequency, NULL);
DEVICE_ATTR(number_channels, S_IRUGO, show_number_channels, NULL);
DEVICE_ATTR(number_of_samples_processed, S_IRUGO, show_number_of_samples_processed, NULL);



static void DoNotify(void)
{
	NotifyCount++;
	sysfs_notify(StreamingEngineClass.dev_kobj, NULL, "notify");
}

/*{{{  GetPlaybackDataSysfs*/
struct PlaybackData_s *GetPlaybackDataSysfs(stm_se_playback_h  Playback)
{
	int p;

	for (p = 0; p < PLAYBACK_MAX_NUMBER; p++) {
		if (PlaybackData[p].Playback == Playback) {
			break;
		}
	}
	if (p == PLAYBACK_MAX_NUMBER) {
		return NULL;
	}

	return &PlaybackData[p];
}
/*}}}*/
/*{{{  GetStreamDataSysfs*/
struct StreamData_s *GetStreamDataSysfs(struct PlaybackData_s *PlaybackData, stm_se_play_stream_h Stream)
{
	int s;

	for (s = 0; s < STREAM_MAX_NUMBER; s++) {
		if (PlaybackData->StreamData[s].Stream == Stream) {
			break;
		}
	}
	if (s == STREAM_MAX_NUMBER) {
		return NULL;
	}

	return &PlaybackData->StreamData[s];
}
/*}}}*/


/*{{{  SysfsGetStreamData*/
struct StreamData_s *SysfsGetStreamData(struct device *StreamDev)
{
	struct StreamData_s *StreamData = NULL;

	StreamData = container_of(StreamDev, struct StreamData_s, StreamClassDevice);

	return StreamData;
}
/*}}}*/
/*{{{  SysfsGetEncodeStreamData*/
struct EncodeStreamData_s *SysfsGetEncodeStreamData(struct device *StreamDev)
{
	struct EncodeStreamData_s *EncodeStreamData = NULL;

	EncodeStreamData = container_of(StreamDev, struct EncodeStreamData_s, StreamClassDevice);

	return EncodeStreamData;
}
/*}}}*/
/*{{{  SysfsGetPlaybackData*/
struct PlaybackData_s *SysfsGetPlaybackData(struct device *PlaybackDev)
{
	struct PlaybackData_s *PlaybackData = NULL;

	PlaybackData = container_of(PlaybackDev, struct PlaybackData_s, PlaybackClassDevice);

	return PlaybackData;
}
/*}}}*/

/*{{{  player_sysfs_new_attribute_notification*/
void SysfsStreamNewAttributeNotification(struct device *StreamDev)
{
	if (StreamDev) {
		struct StreamData_s *StreamData;

		/* the compiler cannot make this function type-safe (we don't know
		 * that StreamDev points to a stream device rather than some
		 * other device). fortunately all valid StreamDev pointers live
		 * within a known block of static memory so we can search for API
		 * abuse at runtime.
		 */
		BUG_ON(((char *) StreamDev < ((char *) PlaybackData)) ||
		       ((char *) StreamDev >= ((char *) PlaybackData) + sizeof(PlaybackData)));

		StreamData = container_of(StreamDev, struct StreamData_s, StreamClassDevice);
		StreamData->NotifyOnDestroy = true;
	}

	DoNotify();
}
/*}}}*/
/*{{{  SysfsEncodeStreamNewAttributeNotification*/
void SysfsEncodeStreamNewAttributeNotification(struct device *StreamDev)
{
	if (StreamDev) {
		struct EncodeStreamData_s *StreamData;

		/* the compiler cannot make this function type-safe (we don't know
		 * that StreamDev points to a stream device rather than some
		 * other device). fortunately all valid StreamDev pointers live
		 * within a known block of static memory so we can search for API
		 * abuse at runtime.
		 */
		BUG_ON(((char *) StreamDev < ((char *) EncodeData)) ||
		       ((char *) StreamDev >= ((char *) EncodeData) + sizeof(EncodeData)));

		StreamData = container_of(StreamDev, struct EncodeStreamData_s, StreamClassDevice);
		StreamData->NotifyOnDestroy = true;
	}

	DoNotify();
}
/*}}}*/

static void DevRelease(struct device *Dev)
{
}

/*{{{  CleanUpStreamSysfs*/
void CleanUpStreamSysfs(struct PlaybackData_s *PlaybackData)
{
	int s;

	for (s = 0; s < STREAM_MAX_NUMBER; s++) {
		if (PlaybackData->StreamData[s].Stream == NULL && PlaybackData->StreamData[s].StreamClassDeviceRegistered) {
			device_unregister(&PlaybackData->StreamData[s].StreamClassDevice);
			PlaybackData->StreamData[s].StreamClassDeviceRegistered = false;

			if (PlaybackData->StreamData[s].NotifyOnDestroy) {
				DoNotify();
			}

			BUG_ON(PlaybackData->StreamData[s].Stream);
		}
	}
}
/*}}}*/
/*{{{  CleanUpPlaybackSysfs*/
void CleanUpPlaybackSysfs(void)
{
	int p;

	for (p = 0; p < PLAYBACK_MAX_NUMBER; p++) {
		if (PlaybackData[p].Playback == NULL && PlaybackData[p].PlaybackClassDeviceRegistered) {
			CleanUpStreamSysfs(&PlaybackData[p]);

			device_unregister(&PlaybackData[p].PlaybackClassDevice);
			PlaybackData[p].PlaybackClassDeviceRegistered = false;

			BUG_ON(PlaybackData[p].Playback);
		}
	}
}
/*}}}*/
/*{{{  PlaybackCreateSysfs*/
int PlaybackCreateSysfs(const char *Name, stm_se_playback_h  Playback)
{
	int Result = 0;
	struct PlaybackData_s *PlaybackData;

	mutex_lock(&SysfsPlaybackLock);

	CleanUpPlaybackSysfs();

	if (!Playback) {
		pr_err("Error: %s null playback\n", __func__);
		mutex_unlock(&SysfsPlaybackLock);
		return -ENOMEM;
	}

	/* Check that this playback does not already exist. In that case, do nothing */
	PlaybackData = GetPlaybackDataSysfs(Playback);
	if (PlaybackData) {
		pr_info("%s playback already exists\n", __func__);
		mutex_unlock(&SysfsPlaybackLock);
		return 0;
	}

	/* Search for free slot */
	PlaybackData = GetPlaybackDataSysfs(NULL);
	if (!PlaybackData) {
		pr_err("Error: %s failed to get a slot\n", __func__);
		mutex_unlock(&SysfsPlaybackLock);
		return -ENOMEM;
	}

	/* Create a class device and register it with sysfs ( = sysfs/class/player2/playback0,1,2,3....) */
	PlaybackData->PlaybackDev_t            = MKDEV(0, 0);

	memset(&PlaybackData->PlaybackClassDevice, 0, sizeof(struct device));
	PlaybackData->PlaybackClassDevice.devt        = PlaybackData->PlaybackDev_t;
	PlaybackData->PlaybackClassDevice.class       = &StreamingEngineClass;
	PlaybackData->PlaybackClassDevice.release     = DevRelease;
	PlaybackData->PlaybackClassDevice.init_name   = Name;

	Result = device_register(&PlaybackData->PlaybackClassDevice);
	if (Result) {
		pr_err("Error: %s failed device registered %d (%p)\n", __func__, PlaybackData->Id, &PlaybackData->PlaybackClassDevice);
		mutex_unlock(&SysfsPlaybackLock);
		return -ENODEV;
	}

	/* Update structure */
	PlaybackData->Playback = Playback;
	PlaybackData->UpdateStatistics = true;
	PlaybackData->PlaybackClassDeviceRegistered = true;

	/* clear out any old statistics */
	memset(&PlaybackData->Statistics, 0, sizeof(PlaybackData->Statistics));

	/* create all the device files */
	Result = 0;
#define C(x) if (0 == Result) Result = device_create_file(&PlaybackData->PlaybackClassDevice, &dev_attr_##x)

	C(update_playback_statistics);
	C(reset_playback_statistics);

	C(clock_recovery_accumulated_points);
	C(clock_recovery_clock_adjustment);
	C(clock_recovery_cummulative_discarded_points);
	C(clock_recovery_cummulative_discard_resets);
	C(clock_recovery_actual_gradient);
	C(clock_recovery_established_gradient);
	C(clock_recovery_integration_time_window);
	C(clock_recovery_integration_time_elapsed);

#undef C

	mutex_unlock(&SysfsPlaybackLock);

	if (0 != Result) {
		pr_err("Error: %s Cannot create sysfs device file (%d)\n", __func__, Result);
		return -1;
	}

	return 0;
}
/*}}}*/
/*{{{  PlaybackTerminateSysfs*/
int PlaybackTerminateSysfs(stm_se_playback_h  Playback)
{
	struct PlaybackData_s *PlaybackData;

	mutex_lock(&SysfsPlaybackLock);

	if (!Playback) {
		pr_err("Error: %s null playback\n", __func__);
		mutex_unlock(&SysfsPlaybackLock);
		return -ENOMEM;
	}

	/* Find specific playback */
	PlaybackData = GetPlaybackDataSysfs(Playback);
	if (!PlaybackData) {
		pr_err("Error: %s failed to get playback\n", __func__);
		mutex_unlock(&SysfsPlaybackLock);
		return -ENOMEM;
	}

	/* after this call completes we are no longer allowed to deference
	 * the stream pointer.
	 */
	PlaybackData->Playback = NULL;

	mutex_unlock(&SysfsPlaybackLock);
	return 0;
}
/*}}}*/
/*{{{  StreamCreateSysfs*/
int StreamCreateSysfs(const char *Name,
                      stm_se_playback_h     Playback,
                      stm_se_media_t        Media,
                      stm_se_play_stream_h  Stream)
{
	int Result = 0;
	struct PlaybackData_s     *PlaybackData;
	struct StreamData_s       *StreamData;

	mutex_lock(&SysfsPlaybackLock);

	if ((!Playback) || (!Stream)) {
		pr_err("Error: %s null parameter %p-%p\n", __func__, Playback, Stream);
		mutex_unlock(&SysfsPlaybackLock);
		return -ENOMEM;
	}

	if ((Media != STM_SE_MEDIA_AUDIO) && (Media != STM_SE_MEDIA_VIDEO)) {
		pr_err("Error: %s Not a video or audio stream\n", __func__);
		mutex_unlock(&SysfsPlaybackLock);
		return 0;
	}

	/* Find specific playback */
	PlaybackData = GetPlaybackDataSysfs(Playback);
	if (!PlaybackData) {
		pr_err("Error: %s failed to get playback\n", __func__);
		mutex_unlock(&SysfsPlaybackLock);
		return -ENOMEM;
	} else {
		// Clean up any stream sysfs entry for a given playback, this can happen in this case :
		// When doing stop, Playback remains alive, streams are deleted and sysfs not removed.
		CleanUpStreamSysfs(PlaybackData);
	}

	/* Check that this stream does not already exist. In that case, do nothing */
	StreamData              = GetStreamDataSysfs(PlaybackData, Stream);
	if (StreamData) {
		mutex_unlock(&SysfsPlaybackLock);
		return 0;
	}

	/* Search for free slot */
	StreamData = GetStreamDataSysfs(PlaybackData, NULL);
	if (!StreamData) {
		pr_err("Error: %s failed to get free slot for %d\n", __func__, PlaybackData->Id);
		mutex_unlock(&SysfsPlaybackLock);
		return -ENOMEM;
	}


	/* Populate structure */
	StreamData->Stream              = Stream;
	StreamData->Media               = Media;
	StreamData->UpdateStatistics    = true;
	StreamData->StreamClassDeviceRegistered = false;

	/*
	 Create a device and register it with sysfs ( = sys/class/streaming_engine/playbackx/video1,2,3... or audio1,2,3... )
	 Populate the class structure in similar way to class_device_create().
	 We don't use device_create so we can use container of to access the parent structure.
	*/
	StreamData->StreamDev_t        = MKDEV(0, 0);

	memset(&StreamData->StreamClassDevice, 0, sizeof(struct device));
	StreamData->StreamClassDevice.devt        = StreamData->StreamDev_t;
	StreamData->StreamClassDevice.class       = &StreamingEngineClass;
	StreamData->StreamClassDevice.parent      = &PlaybackData->PlaybackClassDevice;
	StreamData->StreamClassDevice.release     = DevRelease;
	StreamData->StreamClassDevice.init_name   = Name;

	Result = device_register(&StreamData->StreamClassDevice);
	if (Result || IS_ERR(&StreamData->StreamClassDevice)) {
		pr_err("Error: %s failed StreamClassDevice %d (%p)\n", __func__, StreamData->Id, &StreamData->StreamClassDevice);
		StreamData->Stream          = NULL;
		mutex_unlock(&SysfsPlaybackLock);
		return -ENODEV;
	}
	StreamData->StreamClassDeviceRegistered = true;

	/* clear out any old statistics */
	memset(&StreamData->Statistics, 0, sizeof(StreamData->Statistics));

	/* create all the device files */
	Result = 0;
#define C(x) if (0 == Result) Result = device_create_file(&StreamData->StreamClassDevice, &dev_attr_##x)
#define C2(x, y) if (0 == Result) Result = device_create_file(&StreamData->StreamClassDevice, &dev_attr_##x##_##y)

	C(frame_count_launched_decode);

	if (Media == STM_SE_MEDIA_VIDEO) {
		C(vid_frame_count_launched_decode_I);
		C(vid_frame_count_launched_decode_P);
		C(vid_frame_count_launched_decode_B);

		C(h264_preproc_error_sc_detected);
		C(h264_preproc_error_bit_inserted);
		C(h264_preproc_intbuffer_overflow);
		C(h264_preproc_bitbuffer_underflow);
		C(h264_preproc_bitbuffer_overflow);
		C(h264_preproc_read_errors);
		C(h264_preproc_write_errors);

		C(hevc_preproc_error_sc_detected);
		C(hevc_preproc_error_eos);
		C(hevc_preproc_error_end_of_dma);
		C(hevc_preproc_error_range);
		C(hevc_preproc_error_entropy_decode);

		C(frame_count_decode_mb_overflow_error);
		C(frame_count_decode_recovered_error);
		C(frame_count_decode_not_recovered_error);
		C(frame_count_decode_task_timeout_error);
	}

	C(frame_count_decoded);

	C(frame_count_manifested);
	C(frame_count_to_manifestor);
	C(frame_count_from_manifestor);

	C(max_decode_video_hw_time);
	C(min_decode_video_hw_time);
	C(avg_decode_video_hw_time);

	C(frame_count_parser_error);
	C(frame_count_parser_nostreamparameters_error);
	C(frame_count_parser_partialframeparameters_error);
	C(frame_count_parser_unhandledheader_error);
	C(frame_count_parser_headersyntax_error);
	C(frame_count_parser_headerunplayable_error);
	C(frame_count_parser_streamsyntax_error);
	C(frame_count_parser_failedtocreatereverseplaystacks_error);
	C(frame_count_parser_failedtoallocatebuffer_error);
	C(frame_count_parser_referencelistconstructiondeferred_error);
	C(frame_count_parser_insufficientreferenceframes_error);
	C(frame_count_parser_streamunplayable_error);

	C(frame_count_decode_error);

	C(frame_count_codec_error);
	C(frame_count_codec_unknownframe_error);

	C(dropped_before_decode_window_singlegroupplayback);
	C(dropped_before_decode_window_keyframesonly);
	C(dropped_before_decode_window_outsidepresentationinterval);
	C(dropped_before_decode_window_trickmodenotsupported);
	C(dropped_before_decode_window_trickmode);
	C(dropped_before_decode_window_others);
	C(dropped_before_decode_window_total);

	C(dropped_before_decode_singlegroupplayback);
	C(dropped_before_decode_keyframesonly);
	C(dropped_before_decode_outsidepresentationinterval);
	C(dropped_before_decode_trickmodenotsupported);
	C(dropped_before_decode_trickmode);
	C(dropped_before_decode_others);
	C(dropped_before_decode_total);

	C(dropped_before_output_timing_outsidepresentationinterval);
	C(dropped_before_output_timing_others);
	C(dropped_before_output_timing_total);

	C(dropped_before_manifestation_singlegroupplayback);
	C(dropped_before_manifestation_toolateformanifestation);
	C(dropped_before_manifestation_trickmodenotsupported);
	C(dropped_before_manifestation_others);
	C(dropped_before_manifestation_total);

	C(frame_count_from_frame_parser);
	C(frame_count_to_codec);
	C(frame_count_from_codec);

	if (Media == STM_SE_MEDIA_VIDEO) {
		C(video_framerate);
	}

	if (Media == STM_SE_MEDIA_AUDIO) {
		C(collator_audio_elementry_sync_lost_count);
		C(collator_audio_pes_sync_lost_count);
		C(frame_parser_audio_sample_rate);
		C(frame_parser_audio_frame_size);
		C(codec_audio_coding_mode);
		C(codec_audio_sampling_frequency);
		C(codec_audio_num_of_output_samples);
		C(manifestor_audio_mixer_starved);
		C(dolbypulse_id_count);
		C(dolbypulse_sbr_present);
		C(dolbypulse_ps_present);
		C(codec_frame_length);
		C(codec_num_of_output_channels);
	}

	C(buffer_count_from_collator);
	C(buffer_count_to_frame_parser);

	C(pts);
	C(presentation_time);
	C(system_time);
	C(sync_error);
	C(output_sync_error_0);
	C(output_sync_error_1);

#define CBL(pool, member) if (0 == Result) Result = device_create_file(&StreamData->StreamClassDevice, &dev_attr_##pool##_##member)
	CBL(coded_frame_buffer_pool, buffers_in_pool);
	CBL(coded_frame_buffer_pool, buffers_with_non_zero_reference_count);
	CBL(coded_frame_buffer_pool, largest_free_memory_block);
	CBL(coded_frame_buffer_pool, memory_allocated);
	CBL(coded_frame_buffer_pool, memory_in_pool);
	CBL(coded_frame_buffer_pool, memory_in_use);

	CBL(decode_buffer_pool, buffers_in_pool);
	CBL(decode_buffer_pool, buffers_with_non_zero_reference_count);
	CBL(decode_buffer_pool, largest_free_memory_block);
	CBL(decode_buffer_pool, memory_allocated);
	CBL(decode_buffer_pool, memory_in_pool);
	CBL(decode_buffer_pool, memory_in_use);
#undef CBL

	C(update_statistics);
	C(reset_statistics);

	C2(output_rate_gradient, 0);
	C2(output_rate_gradient, 1);
	C2(output_rate_gradient, 2);
	C2(output_rate_gradient, 3);
	C2(output_rate_gradient, 4);
	C2(output_rate_gradient, 5);
	C2(output_rate_gradient, 6);
	C2(output_rate_frames_to_integrate_over, 0);
	C2(output_rate_frames_to_integrate_over, 1);
	C2(output_rate_frames_to_integrate_over, 2);
	C2(output_rate_frames_to_integrate_over, 3);
	C2(output_rate_frames_to_integrate_over, 4);
	C2(output_rate_frames_to_integrate_over, 5);
	C2(output_rate_frames_to_integrate_over, 6);
	C2(output_rate_integration_count, 0);
	C2(output_rate_integration_count, 1);
	C2(output_rate_integration_count, 2);
	C2(output_rate_integration_count, 3);
	C2(output_rate_integration_count, 4);
	C2(output_rate_integration_count, 5);
	C2(output_rate_integration_count, 6);
	C2(output_rate_clock_adjustment, 0);
	C2(output_rate_clock_adjustment, 1);
	C2(output_rate_clock_adjustment, 2);
	C2(output_rate_clock_adjustment, 3);
	C2(output_rate_clock_adjustment, 4);
	C2(output_rate_clock_adjustment, 5);
	C2(output_rate_clock_adjustment, 6);
#undef C
#undef C2

	if (0 != Result) {
		pr_err("Error: %s Cannot create sysfs device file (%d)\n", __func__, Result);
		mutex_unlock(&SysfsPlaybackLock);
		return -1;
	}

	SysfsStreamNewAttributeNotification(&StreamData->StreamClassDevice);

	/*
	Create Stream attributes
	*/
	if (Media == STM_SE_MEDIA_AUDIO) {
		SYSFS_CREATE_ATTRIBUTE(input_format);
		SYSFS_CREATE_ATTRIBUTE(supported_input_format);
		SYSFS_CREATE_ATTRIBUTE(decode_errors);
		SYSFS_CREATE_ATTRIBUTE(sample_frequency);
		SYSFS_CREATE_ATTRIBUTE(number_of_samples_processed);
		SYSFS_CREATE_ATTRIBUTE(number_channels);
	}

	mutex_unlock(&SysfsPlaybackLock);
	return 0;
}
/*}}}*/
/*{{{  StreamTerminateSysfs*/
int StreamTerminateSysfs(stm_se_playback_h    Playback,
                         stm_se_play_stream_h Stream)
{
	struct PlaybackData_s     *PlaybackData;
	struct StreamData_s       *StreamData;

	mutex_lock(&SysfsPlaybackLock);

	if ((!Playback) || (!Stream)) {
		pr_err("Error: %s null parameter %p-%p\n", __func__, Playback, Stream);
		mutex_unlock(&SysfsPlaybackLock);
		return -ENOMEM;
	}

	/* Find specific playback */
	PlaybackData = GetPlaybackDataSysfs(Playback);
	if (!PlaybackData) {
		pr_err("Error: %s failed to get playback\n", __func__);
		mutex_unlock(&SysfsPlaybackLock);
		return -ENOMEM;
	}

	/* Find specific stream */
	StreamData = GetStreamDataSysfs(PlaybackData, Stream);
	if (!StreamData) {
		pr_err("Error: %s failed to get stream\n", __func__);
		mutex_unlock(&SysfsPlaybackLock);
		return -ENOMEM;
	}

	/* after this call completes we are no longer allowed to deference
	 * the stream pointer.
	 */
	StreamData->Stream              = NULL;

	mutex_unlock(&SysfsPlaybackLock);
	return 0;
}
/*}}}*/

// Encoder
/*{{{  GetEncodeDataSysfs*/
struct EncodeData_s *GetEncodeDataSysfs(stm_se_encode_h  Encode)
{
	int e;

	for (e = 0; e < ENCODE_MAX_NUMBER; e++) {
		if (EncodeData[e].Encode == Encode) {
			break;
		}
	}
	if (e == ENCODE_MAX_NUMBER) {
		return NULL;
	}

	return &EncodeData[e];
}
/*}}}*/
/*{{{  GetEncodeStreamDataSysfs*/
struct EncodeStreamData_s *GetEncodeStreamDataSysfs(struct EncodeData_s *EncodeData, stm_se_encode_stream_h Stream)
{
	int s;

	for (s = 0; s < ENCODE_STREAM_MAX_NUMBER; s++) {
		if (EncodeData->StreamData[s].Stream == Stream) {
			break;
		}
	}
	if (s == ENCODE_STREAM_MAX_NUMBER) {
		return NULL;
	}

	return &EncodeData->StreamData[s];
}
/*}}}*/
/*{{{  CleanUpEncodeStreamSysfs*/
void CleanUpEncodeStreamSysfs(struct EncodeData_s *EncodeData)
{
	int s;

	for (s = 0; s < ENCODE_STREAM_MAX_NUMBER; s++) {
		if (EncodeData->StreamData[s].Stream == NULL && EncodeData->StreamData[s].StreamClassDeviceRegistered) {
			device_unregister(&EncodeData->StreamData[s].StreamClassDevice);
			EncodeData->StreamData[s].StreamClassDeviceRegistered = false;

			if (EncodeData->StreamData[s].NotifyOnDestroy) {
				DoNotify();
			}

			BUG_ON(EncodeData->StreamData[s].Stream);
		}
	}
}
/*}}}*/
/*{{{  CleanUpEncodeSysfs*/
void CleanUpEncodeSysfs(void)
{
	int p;
	for (p = 0; p < ENCODE_MAX_NUMBER; p++) {
		if (EncodeData[p].Encode == NULL && EncodeData[p].EncodeClassDevice != NULL) {
			CleanUpEncodeStreamSysfs(&EncodeData[p]);
			device_unregister(EncodeData[p].EncodeClassDevice);
			// Zero Encode entry in table
			EncodeData[p].EncodeClassDevice = NULL;
			BUG_ON(EncodeData[p].Encode);
		}
	}
}
/*}}}*/


/*{{{  EncodeCreateSysfs*/
int EncodeCreateSysfs(const char *Name, stm_se_encode_h  Encode)
{
	struct EncodeData_s *EncodeData;

	if (!Encode) {
		pr_err("Error: %s null encode\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&SysfsEncodeLock);

	CleanUpEncodeSysfs();

	/* Check that this encode does not already exist. In that case, do nothing */
	EncodeData    = GetEncodeDataSysfs(Encode);
	if (EncodeData) {
		mutex_unlock(&SysfsEncodeLock);
		return 0;
	}

	/* Search for free slot */
	EncodeData = GetEncodeDataSysfs(NULL);
	if (!EncodeData) {
		pr_err("Error: %s failed to get free slot\n", __func__);
		mutex_unlock(&SysfsEncodeLock);
		return -ENOMEM;
	}

	/* Populate structure */
	EncodeData->Encode                    = Encode;

	/* Create a class device and register it with sysfs ( = sysfs/class/streaming_engine/encode0,1,2,3....) */
	EncodeData->EncodeDev_t               = MKDEV(0, 0);
	EncodeData->EncodeClassDevice         = device_create(
	                                                &StreamingEngineClass,            // pointer to the struct class that this device should be registered to
	                                                NULL,                                 // pointer to the parent struct class_device of this new device, if any
	                                                EncodeData->EncodeDev_t,              // the dev_t for the (char) device to be added
	                                                NULL,                                 // void *drvdata (device private data in linux 2.4?
	                                                Name);      // string for the class device's name
	if (IS_ERR(EncodeData->EncodeClassDevice)) {
		pr_err("Error: %s failed to create Encode class device (%d)\n", __func__, (int)EncodeData->EncodeClassDevice);
		EncodeData->EncodeClassDevice     = NULL;
		EncodeData->Encode                = NULL;
		mutex_unlock(&SysfsEncodeLock);
		return -ENODEV;
	}

	mutex_unlock(&SysfsEncodeLock);
	return 0;
}
/*}}}*/
/*{{{  PlaybackTerminateSysfs*/
int EncodeTerminateSysfs(stm_se_encode_h  Encode)
{
	struct EncodeData_s *EncodeData;

	if (!Encode) {
		pr_err("Error: %s null encode\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&SysfsEncodeLock);
	/* Find specific Encode */
	EncodeData = GetEncodeDataSysfs(Encode);
	if (!EncodeData) {
		pr_err("Error: %s failed to get encode\n", __func__);
		mutex_unlock(&SysfsEncodeLock);
		return -ENOMEM;
	}

	/* after this call completes we are no longer allowed to deference
	 * the stream pointer.
	 */
	EncodeData->Encode = NULL;

	mutex_unlock(&SysfsEncodeLock);
	return 0;
}
/*}}}*/
/*{{{  EncodeStreamCreateSysfs*/
int EncodeStreamCreateSysfs(const char               *Name,
                            stm_se_encode_h              Encode,
                            stm_se_encode_stream_media_t Media,
                            stm_se_encode_stream_h       Stream)
{
	int Result = 0;
	struct EncodeData_s           *EncodeData;
	struct EncodeStreamData_s     *StreamData;

	if ((!Encode) || (!Stream)) {
		pr_err("Error: %s null parameter %p-%p\n", __func__, Encode, Stream);
		return -ENOMEM;
	}

	if ((Media != STM_SE_ENCODE_STREAM_MEDIA_AUDIO) && (Media != STM_SE_ENCODE_STREAM_MEDIA_VIDEO)) {
		pr_err("Error: %s Not a video or audio stream\n", __func__);
		return 0;
	}

	mutex_lock(&SysfsEncodeLock);

	/* Find specific encode */
	EncodeData            = GetEncodeDataSysfs(Encode);
	if (!EncodeData) {
		mutex_unlock(&SysfsEncodeLock);
		pr_err("Error: %s failed to get encode\n", __func__);
		return -ENOMEM;
	} else {
		// Clean up any stream sysfs entry for a given encode, this can happen in this case :
		// When doing stop, encode remains alive, streams are deleted and sysfs not removed.
		// if we do play, we remove all stream sysfs for this encode
		CleanUpEncodeStreamSysfs(EncodeData);
	}

	/* Check that this stream does not already exist. In that case, do nothing */
	StreamData = GetEncodeStreamDataSysfs(EncodeData, Stream);
	if (StreamData) {
		mutex_unlock(&SysfsEncodeLock);
		return 0;
	}

	/* Search for free slot */
	StreamData = GetEncodeStreamDataSysfs(EncodeData, NULL);
	if (!StreamData) {
		mutex_unlock(&SysfsEncodeLock);
		pr_err("Error: %s failed to get free slot for Encode%d\n", __func__, EncodeData->Id);
		return -ENOMEM;
	}


	/* Populate structure */
	StreamData->Stream              = Stream;
	StreamData->Media               = Media;
	StreamData->UpdateStatistics    = true;
	StreamData->StreamClassDeviceRegistered = false;

	/*
	 Create a device and register it with sysfs ( = sys/class/streaming_engine/encodex/video1,2,3... or audio1,2,3... )
	 Populate the class structure in similar way to class_device_create().
	 We don't use device_create so we can use container of to access the parent structure.
	*/
	StreamData->StreamDev_t        = MKDEV(0, 0);

	memset(&StreamData->StreamClassDevice, 0, sizeof(struct device));
	StreamData->StreamClassDevice.devt        = StreamData->StreamDev_t;
	StreamData->StreamClassDevice.class       = &StreamingEngineClass;
	StreamData->StreamClassDevice.parent      = EncodeData->EncodeClassDevice;
	StreamData->StreamClassDevice.release     = DevRelease;
	StreamData->StreamClassDevice.init_name   = Name;

	Result = device_register(&StreamData->StreamClassDevice);

	if (Result || IS_ERR(&StreamData->StreamClassDevice)) {
		pr_err("Error: %s Unable to create StreamClassDevice %d (%d)\n", __func__, StreamData->Id,
		       (int) &StreamData->StreamClassDevice);
		StreamData->Stream          = NULL;
		mutex_unlock(&SysfsEncodeLock);
		return -ENODEV;
	}
	StreamData->StreamClassDeviceRegistered = true;

#define C(x) if (0 == Result) Result = device_create_file(&StreamData->StreamClassDevice, &dev_attr_##x)

	if (Media == STM_SE_ENCODE_STREAM_MEDIA_VIDEO) {
		// Video Specific Statistics
		C(video_skipped_count_from_coder);
	}

	if (Media == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) {
		// Audio Specific Statistics
	}

	C(buffer_count_to_preproc);
	C(frame_count_to_preproc);
	C(discontinuity_count_to_preproc);
	C(buffer_count_from_preproc);
	C(discontinuity_count_from_preproc);

	C(buffer_count_to_coder);
	C(frame_count_from_coder);
	C(eos_count_from_coder);

	C(buffer_count_to_transporter);
	C(buffer_count_from_transporter);
	C(null_size_buffer_count_from_transporter);
	C(release_buffer_count_from_tsmux_transporter);

	C(tsmux_queue_error);
	C(tsmux_transporter_buffer_address_error);
	C(tsmux_transporter_unexpected_released_buffer_error);
	C(tsmux_transporter_ring_extract_error);

	C(update_encode_stream_statistics);
	C(reset_encode_stream_statistics);

#undef C

	SysfsEncodeStreamNewAttributeNotification(&StreamData->StreamClassDevice);

	mutex_unlock(&SysfsEncodeLock);
	return 0;
}
/*}}}*/
/*{{{  EncodeStreamTerminateSysfs*/
int EncodeStreamTerminateSysfs(stm_se_encode_h        Encode,
                               stm_se_encode_stream_h Stream)
{
	struct EncodeData_s       *EncodeData;
	struct EncodeStreamData_s *StreamData;

	if ((!Encode) || (!Stream)) {
		pr_err("Error: %s null parameter %p-%p\n", __func__, Encode, Stream);
		return -ENOMEM;
	}

	mutex_lock(&SysfsEncodeLock);

	/* Find specific Encode */
	EncodeData            = GetEncodeDataSysfs(Encode);
	if (!EncodeData) {
		mutex_unlock(&SysfsEncodeLock);
		pr_err("Error: %s failed to get encode\n", __func__);
		return -ENOMEM;
	}

	/* Find specific stream */
	StreamData              = GetEncodeStreamDataSysfs(EncodeData, Stream);
	if (!StreamData) {
		mutex_unlock(&SysfsEncodeLock);
		pr_err("Error: %s failed to get stream\n", __func__);
		return -ENOMEM;
	}

	/* after this call completes we are no longer allowed to deference
	 * the stream pointer.
	 */
	StreamData->Stream              = NULL;

	mutex_unlock(&SysfsEncodeLock);
	return 0;
}
/*}}}*/

/*{{{  SysfsInit*/
int SysfsInit(void)
{
	int res;
	int p;
	int s;

	res = class_register(&StreamingEngineClass);
	if (0 != res) {
		pr_err("Error: %s to register class\n", __func__);
		return -EPERM;
	}

	for (p = 0; p < PLAYBACK_MAX_NUMBER; p++) {
		memset(&PlaybackData[p], 0, sizeof(struct PlaybackData_s));
		PlaybackData[p].Id     = p;
		for (s = 0; s < STREAM_MAX_NUMBER; s++) {
			PlaybackData[p].StreamData[s].Id = s;
		}
	}

	for (p = 0; p < ENCODE_MAX_NUMBER; p++) {
		memset(&EncodeData[p], 0, sizeof(struct EncodeData_s));
		EncodeData[p].Id     = p;
		for (s = 0; s < ENCODE_STREAM_MAX_NUMBER; s++) {
			EncodeData[p].StreamData[s].Id = s;
		}
	}

	mutex_init(&SysfsWriteLock);
	mutex_init(&SysfsPlaybackLock);
	mutex_init(&SysfsEncodeLock);

	return 0;
}
/*}}}*/
/*{{{  SysfsDelete*/
void SysfsDelete(void)
{
	mutex_destroy(&SysfsWriteLock);

	// Clean up all sysfs entries
	CleanUpEncodeSysfs();
	CleanUpPlaybackSysfs();
	class_unregister(&StreamingEngineClass);

	mutex_destroy(&SysfsEncodeLock);
	mutex_destroy(&SysfsPlaybackLock);
}
/*}}}*/

/*{{{  ResetStreamStatisticsForSysfs*/
static int ResetStreamStatisticsForSysfs(struct StreamData_s *StreamData)
{
	__stm_se_play_stream_reset_statistics(StreamData->Stream);
	memset(&StreamData->Statistics, 0, sizeof(StreamData->Statistics));

	return 0;
}
/*}}}*/
/*{{{  GetStreamStatisticsForSysfs*/
static int GetStreamStatisticsForSysfs(struct StreamData_s *StreamData)
{
	struct statistics_s  Statistics;

	if (StreamData->Stream == NULL) {
		return -EINVAL;
	}

	__stm_se_play_stream_get_statistics(StreamData->Stream, &Statistics);

	/* Update statistics  */
	memcpy(&StreamData->Statistics, &Statistics, sizeof(struct statistics_s));

	return 0;
}
/*}}}*/

/*{{{  ResetPlaybackStatisticsForSysfs*/
static int ResetPlaybackStatisticsForSysfs(struct PlaybackData_s *PlaybackData)
{
	__stm_se_playback_reset_statistics(PlaybackData->Playback);
	memset(&PlaybackData->Statistics, 0, sizeof(PlaybackData->Statistics));

	return 0;
}
/*}}}*/
/*{{{  GetPlaybackStatisticsForSysfs*/
static int GetPlaybackStatisticsForSysfs(struct PlaybackData_s *PlaybackData)
{
	if (PlaybackData->Playback == NULL) {
		return -EINVAL;
	}

	__stm_se_playback_get_statistics(PlaybackData->Playback, &PlaybackData->Statistics);

	return 0;
}
/*}}}*/


/*{{{  WrapperGetPlaybackStatisticsForSysfs*/
int WrapperGetPlaybackStatisticsForSysfs(stm_se_playback_h  Playback, stm_se_play_stream_h Stream)
{
	struct PlaybackData_s     *PlaybackData;
	struct StreamData_s       *StreamData;

	mutex_lock(&SysfsWriteLock);

	if (Playback == NULL) {
		pr_err("Error: %s null playback\n", __func__);
		goto err0;
	}

	mutex_lock(&SysfsPlaybackLock);

	/* Find specific playback */
	PlaybackData = GetPlaybackDataSysfs(Playback);
	if (!PlaybackData) {
		pr_err("Error: %s failed to get playback\n", __func__);
		goto err1;
	} else if (Stream == NULL) {
		pr_err("Error: %s This stream does not exist\n", __func__);
		goto err1;
	}

	/* Find specific stream */
	StreamData = GetStreamDataSysfs(PlaybackData, Stream);
	if (!StreamData) {
		pr_err("Error: %s failed to get stream\n", __func__);
		goto err1;
	}

	mutex_unlock(&SysfsPlaybackLock);
	mutex_unlock(&SysfsWriteLock);

	GetStreamStatisticsForSysfs(StreamData);

	return 0;

err1:
	mutex_unlock(&SysfsPlaybackLock);
err0:
	mutex_unlock(&SysfsWriteLock);
	return -ENOMEM;
}
/*}}}*/

/*{{{  ResetEncodeStreamStatisticsForSysfs*/
static int ResetEncodeStreamStatisticsForSysfs(struct EncodeStreamData_s *EncodeStreamData)
{
	__stm_se_encode_stream_reset_statistics(EncodeStreamData->Stream);
	memset(&EncodeStreamData->Statistics, 0, sizeof(EncodeStreamData->Statistics));

	return 0;
}
/*}}}*/
/*{{{  GetEncodeStreamStatisticsForSysfs*/
static int GetEncodeStreamStatisticsForSysfs(struct EncodeStreamData_s *EncodeStreamData)
{
	struct encode_stream_statistics_s  Statistics;

	if (EncodeStreamData->Stream == NULL) {
		return -EINVAL;
	}

	__stm_se_encode_stream_get_statistics(EncodeStreamData->Stream, &Statistics);

	/* Update statistics  */
	memcpy(&EncodeStreamData->Statistics, &Statistics, sizeof(struct encode_stream_statistics_s));

	return 0;
}
/*}}}*/

// no ResetEncodeStatisticsForSysfs & GetEncodeStatisticsForSysfs
// as no relevant statistics yet

/*{{{  WrapperGetEncodeStatisticsForSysfs*/
int WrapperGetEncodeStatisticsForSysfs(stm_se_encode_h Encode, stm_se_encode_stream_h EncodeStream)
{
	struct EncodeData_s         *EncodeData;
	struct EncodeStreamData_s   *EncodeStreamData;

	mutex_lock(&SysfsWriteLock);

	if (Encode == NULL) {
		pr_err("Error: %s null encode\n", __func__);
		goto err0;
	}

	if (EncodeStream == NULL) {
		pr_err("Error: %s This encode stream does not exist\n", __func__);
		goto err0;
	}

	mutex_lock(&SysfsEncodeLock);

	/* Find specific encode */
	EncodeData = GetEncodeDataSysfs(Encode);
	if (!EncodeData) {
		pr_err("Error: %s failed to get encode\n", __func__);
		goto err1;
	}

	/* Find specific encode stream */
	EncodeStreamData = GetEncodeStreamDataSysfs(EncodeData, EncodeStream);
	if (!EncodeStreamData) {
		pr_err("Error: %s failed to get encode stream\n", __func__);
		goto err1;
	}

	mutex_unlock(&SysfsEncodeLock);
	mutex_unlock(&SysfsWriteLock);

	GetEncodeStreamStatisticsForSysfs(EncodeStreamData);

	return 0;

err1:
	mutex_unlock(&SysfsEncodeLock);
err0:
	mutex_unlock(&SysfsWriteLock);
	return -ENOMEM;
}

#else

int SysfsInit(void) { return 0; }
void SysfsDelete(void) { }

int PlaybackCreateSysfs(const char *Name, stm_se_playback_h Playback) { return 0; }
int PlaybackTerminateSysfs(stm_se_playback_h                Playback) { return 0; }

int StreamCreateSysfs(const char          *Name,
                      stm_se_playback_h    Playback,
                      stm_se_media_t       Media,
                      stm_se_play_stream_h Stream) { return 0; }
int StreamTerminateSysfs(stm_se_playback_h     Playback,
                         stm_se_play_stream_h  Stream) { return 0; }

int WrapperGetPlaybackStatisticsForSysfs(stm_se_playback_h    Playback,
                                         stm_se_play_stream_h Stream) { return 0; }

int EncodeCreateSysfs(const char *Name, stm_se_encode_h Encode) { return 0; }
int EncodeTerminateSysfs(stm_se_encode_h                Encode) { return 0; }

int EncodeStreamCreateSysfs(const char                   *Name,
                            stm_se_encode_h               Encode,
                            stm_se_encode_stream_media_t  Media,
                            stm_se_encode_stream_h        Stream) { return 0; }
int EncodeStreamTerminateSysfs(stm_se_encode_h            Encode,
                               stm_se_encode_stream_h     Stream) { return 0; }

int WrapperGetEncodeStatisticsForSysfs(stm_se_encode_h Encode,
                                       stm_se_encode_stream_h EncodeStream) { return 0; }

#endif  // SE_ENABLE_STATISTICS

/*}}}*/
