/************************************************************************
Copyright (C) 2008 STMicroelectronics. All Rights Reserved.

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

Source file name : monitor_types.h - external interface definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Jul-08   Created                                         Julian

************************************************************************/

#ifndef H_MONITOR_TYPES
#define H_MONITOR_TYPES

#define MONITOR_PARAMETER_COUNT                 4
#define MONITOR_EVENT_REPORT_ON_REQUEST         0x00000100

typedef enum monitor_subsystem_e
{
    MONITOR_DISABLE_ALL                         = 0x00000000,
    MONITOR_ENABLE_ALL                          = 0x000f0000,
    MONITOR_AUDIO_FORMAT_MONITORING             = 0x00010000,
    MONITOR_AUDIO_DECODER_PERFORMANCE           = 0x00020000,
    MONITOR_VIDEO_SIGNAL_ACQUISITION            = 0x00040000,
    MONITOR_VIDEO_PROCESSING                    = 0x00080000,
    MONITOR_SYSTEM_GENERIC                      = 0x00100000
} monitor_subsystem_t;

typedef enum monitor_event_code_e
{
    MONITOR_EVENT_AUDIO_SIGNAL_ACQUIRED         = MONITOR_AUDIO_FORMAT_MONITORING       | 0x00000000,
    MONITOR_EVENT_AUDIO_SIGNAL_LOST             = MONITOR_AUDIO_FORMAT_MONITORING       | 0x00000001,
    MONITOR_EVENT_AUDIO_FORMAT_DETECTED         = MONITOR_AUDIO_FORMAT_MONITORING       | 0x00000002,
    MONITOR_EVENT_AUDIO_FORMAT_UNKNOWN          = MONITOR_AUDIO_FORMAT_MONITORING       | 0x00000003,

    MONITOR_EVENT_AUDIO_INITIALIZATION_START    = MONITOR_AUDIO_DECODER_PERFORMANCE     | 0x00000004,
    MONITOR_EVENT_AUDIO_SHUTDOWN_COMPLETE       = MONITOR_AUDIO_DECODER_PERFORMANCE     | 0x00000005,
    MONITOR_EVENT_AUDIO_SAMPLING_RATE_CHANGE    = MONITOR_AUDIO_DECODER_PERFORMANCE     | 0x00000006,
    MONITOR_EVENT_AUDIO_SAMPLES_PROCESSED       = MONITOR_AUDIO_DECODER_PERFORMANCE     | MONITOR_EVENT_REPORT_ON_REQUEST   | 0x00000007,
    MONITOR_EVENT_AUDIO_POTENTIAL_ARTIFACT      = MONITOR_AUDIO_DECODER_PERFORMANCE     | 0x00000008,

    MONITOR_EVENT_VIDEO_FIRST_FIELD_ACQUIRED    = MONITOR_VIDEO_SIGNAL_ACQUISITION      | 0x00000009,
    MONITOR_EVENT_VIDEO_ACQUISITION_STOPPED     = MONITOR_VIDEO_SIGNAL_ACQUISITION      | 0x0000000a,
    MONITOR_EVENT_VIDEO_SIGNAL_ACQUIRED         = MONITOR_VIDEO_SIGNAL_ACQUISITION      | 0x0000000b,
    MONITOR_EVENT_VIDEO_SIGNAL_LOST             = MONITOR_VIDEO_SIGNAL_ACQUISITION      | 0x0000000c,
    MONITOR_EVENT_VIDEO_FIELDS_EXPECTED         = MONITOR_VIDEO_SIGNAL_ACQUISITION      | MONITOR_EVENT_REPORT_ON_REQUEST   | 0x0000000d,
    MONITOR_EVENT_VIDEO_FIELDS_DROPPED          = MONITOR_VIDEO_SIGNAL_ACQUISITION      | MONITOR_EVENT_REPORT_ON_REQUEST   | 0x0000000e,
    MONITOR_EVENT_VIDEO_TNR_SETTING             = MONITOR_VIDEO_SIGNAL_ACQUISITION      | MONITOR_EVENT_REPORT_ON_REQUEST   | 0x0000000f,
    MONITOR_EVENT_VIDEO_DEINTERLACER_SETTING    = MONITOR_VIDEO_SIGNAL_ACQUISITION      | MONITOR_EVENT_REPORT_ON_REQUEST   | 0x00000010,

    MONITOR_EVENT_INFORMATION                   = MONITOR_SYSTEM_GENERIC                | 0x00000020
} monitor_event_code_t;

#endif
