/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

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

Source file name : player_interface_ops.h - player device definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-03   Created                                         Julian

************************************************************************/

#ifndef H_PLAYER_INTERFACE_OPS
#define H_PLAYER_INTERFACE_OPS

typedef void           *player_component_handle_t;
typedef void           *player_playback_handle_t;
typedef void           *player_stream_handle_t;

#define PLAYER_EVENT_PLAYBACK_CREATED                   0
#define PLAYER_EVENT_PLAYBACK_TERMINATED                (PLAYER_EVENT_PLAYBACK_CREATED + 1)

#define PLAYER_EVENT_STREAM_CREATED                     (PLAYER_EVENT_PLAYBACK_TERMINATED + 1)
#define PLAYER_EVENT_STREAM_TERMINATED                  (PLAYER_EVENT_STREAM_CREATED + 1)
#define PLAYER_EVENT_STREAM_SWITCHED                    (PLAYER_EVENT_STREAM_TERMINATED + 1)
#define PLAYER_EVENT_STREAM_DRAINED                     (PLAYER_EVENT_STREAM_SWITCHED + 1)

#define PLAYER_EVENT_STREAM_UNPLAYABLE                  (PLAYER_EVENT_STREAM_DRAINED + 1)
#define PLAYER_EVENT_FIRST_FRAME_ON_DISPLAY             (PLAYER_EVENT_STREAM_UNPLAYABLE + 1)
#define PLAYER_EVENT_TIME_NOTIFICATION                  (PLAYER_EVENT_FIRST_FRAME_ON_DISPLAY + 1)
#define PLAYER_EVENT_DECODE_BUFFER_AVAILABLE            (PLAYER_EVENT_TIME_NOTIFICATION + 1)
#define PLAYER_EVENT_INPUT_FORMAT_CREATED               (PLAYER_EVENT_DECODE_BUFFER_AVAILABLE + 1)
#define PLAYER_EVENT_SUPPORTED_INPUT_FORMAT_CREATED     (PLAYER_EVENT_INPUT_FORMAT_CREATED + 1)
#define PLAYER_EVENT_DECODE_ERRORS_CREATED              (PLAYER_EVENT_SUPPORTED_INPUT_FORMAT_CREATED + 1)
#define PLAYER_EVENT_SAMPLE_FREQUENCY_CREATED           (PLAYER_EVENT_DECODE_ERRORS_CREATED + 1)        
#define PLAYER_EVENT_NUMBER_CHANNELS_CREATED            (PLAYER_EVENT_SAMPLE_FREQUENCY_CREATED + 1)
#define PLAYER_EVENT_NUMBER_OF_SAMPLES_PROCESSED        (PLAYER_EVENT_NUMBER_CHANNELS_CREATED + 1)

#define PLAYER_EVENT_SIZE_CHANGED                       0x10000
#define PLAYER_EVENT_FRAME_RATE_CHANGED                 (PLAYER_EVENT_SIZE_CHANGED + 1)
#define PLAYER_EVENT_FRAME_DECODED_LATE                 (PLAYER_EVENT_FRAME_RATE_CHANGED + 1)
#define PLAYER_EVENT_DATA_DELIVERED_LATE                (PLAYER_EVENT_FRAME_DECODED_LATE + 1)
#define PLAYER_EVENT_BUFFER_RELEASE                     (PLAYER_EVENT_DATA_DELIVERED_LATE + 1)
#define PLAYER_EVENT_TRICK_MODE_CHANGE                  (PLAYER_EVENT_BUFFER_RELEASE + 1)
#define PLAYER_EVENT_INPUT_FORMAT_CHANGED               (PLAYER_EVENT_TRICK_MODE_CHANGE + 1)
#define PLAYER_EVENT_TIME_MAPPING_ESTABLISHED           (PLAYER_EVENT_INPUT_FORMAT_CHANGED + 1)
#define PLAYER_EVENT_TIME_MAPPING_RESET                 (PLAYER_EVENT_TIME_MAPPING_ESTABLISHED + 1)
#define PLAYER_EVENT_REVERSE_FAILURE                    (PLAYER_EVENT_TIME_MAPPING_RESET + 1)

#define PLAYER_EVENT_FATAL_ERROR                        (PLAYER_EVENT_REVERSE_FAILURE + 1)

#define PLAYER_EVENT_FATAL_HARDWARE_FAILURE             (PLAYER_EVENT_FATAL_ERROR + 1)

#define PLAYER_EVENT_INVALID                            0xffffffff

struct player_event_s
{
    unsigned int                code;
    unsigned long long          timestamp;
    player_component_handle_t   component;
    player_playback_handle_t    playback;
    player_stream_handle_t      stream;
};

union attribute_descriptor_u
{
    const char*                 ConstCharPointer;
    int                         Int;
    unsigned long long int      UnsignedLongLongInt;
    int                         Bool;
};

typedef int (*player_event_signal_callback)             (struct player_event_s* event);

struct player_interface_operations
{
    struct module                      *owner;

    int (*component_get_attribute)    (player_component_handle_t       Component,
                                       const char*                     Attribute,
                                       union attribute_descriptor_u*   Value);
    int (*component_set_attribute)    (player_component_handle_t       Component,
                                       const char*                     Attribute,
                                       union attribute_descriptor_u*   Value);

    player_event_signal_callback (*player_register_event_signal_callback)      (player_event_signal_callback    Callback);

};

int register_player_interface  (char                                   *name,
                                struct player_interface_operations     *player_ops);
#endif
