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
#ifndef _H_STM_SE_DEPRECATED_
#define _H_STM_SE_DEPRECATED_

/*!
 * \defgroup deprecated_interfaces Deprecated interfaces
 *
 * These interfaces should not be used by any newly written code but may appear
 * in historic middleware implementations.
 *
 * @{
 */

/// \deprecated replaced by stm_se_dual_mode_t
typedef stm_se_dual_mode_t channel_select_t;   // TODO(pht) remove in stlinuxtv

/*! \name buffer capture callback related; used with manifestor_video_grab */
// seems still needed/used by OMXSE (not used by STlinuxTV)
//!\{

/// \deprecated
typedef enum
{
    CAPTURE_XY_IN_16THS         = 0x00000001,
    CAPTURE_XY_IN_32NDS         = 0x00000002,
    CAPTURE_COLOURSPACE_709     = 0x00000004, // only one currently used..in manifestor_video_grab
} capture_buffer_flags_t; // type not used directly

/// \deprecated
struct picture_rectangle_s
{
    unsigned int                x;
    unsigned int                y;
    unsigned int                width;
    unsigned int                height;
};

/// \deprecated
typedef void           *stm_se_event_context_h;
/// \deprecated
typedef void (*releaseCaptureBuffer)(stm_se_event_context_h          context,
                                     unsigned int                user_private);

/// \deprecated
struct capture_buffer_s
{
    unsigned long               physical_address;
    unsigned long long          presentation_time;
    unsigned long long          native_presentation_time;
    struct picture_rectangle_s  rectangle;
    unsigned int                flags;
    unsigned int                size;
    unsigned int                total_lines;
    unsigned int                stride;
    surface_format_t            buffer_format;
    unsigned int                chroma_buffer_offset;
    unsigned int                pixel_depth;
    unsigned int                user_private;
    void                       *virtual_address;
    unsigned int                bits_per_sample;
    unsigned int                channel_count;
    unsigned int                sample_rate;

    releaseCaptureBuffer        releaseBuffer;
    stm_se_event_context_h      releaseBufferContext;
};

/// \deprecated stream_buffer_capture_callback
typedef int (*stream_buffer_capture_callback)(stm_se_event_context_h          context,
                                              struct capture_buffer_s        *capture_buffer);

/// \deprecated This function may be replaced with alternative functionality in future releases
stream_buffer_capture_callback  stm_se_play_stream_register_buffer_capture_callback(stm_se_play_stream_h            play_stream,
                                                                                    stm_se_event_context_h          context,
                                                                                    stream_buffer_capture_callback  callback);
//!\}

/*! @} */ /* deprecated_interfaces */

#endif /* _H_STM_SE_DEPRECATED_ */

