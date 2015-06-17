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

Source file name : dvb_video.h - video device definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-03   Created                                         Julian

************************************************************************/

#ifndef H_DVB_VIDEO
#define H_DVB_VIDEO

#include <dvbdev.h>

#define MAX_VIDEO_EVENT         8
struct VideoEvent_s
{
    struct video_event          Event[MAX_VIDEO_EVENT];         /*! Linux dvb event structure */
    unsigned int                Write;                          /*! Pointer to next event location to write by decoder*/
    unsigned int                Read;                           /*! Pointer to next event location to read by user */
    unsigned int                Overflow;                       /*! Flag to indicate events have been missed */
    wait_queue_head_t           WaitQueue;                      /*! Queue to wake up any waiters */
    struct mutex                Lock;                           /*! Protection for access to Read and Write pointers */
};

#define DVB_OPTION_VALUE_INVALID                0xffffffff

struct dvb_device*  VideoInit                  (struct DeviceContext_s*        Context);
int                 VideoIoctlStop             (struct DeviceContext_s*        Context,
                                                unsigned int                   Mode);
int                 VideoIoctlPlay             (struct DeviceContext_s*        Context);
int                 VideoIoctlSetId            (struct DeviceContext_s*        Context,
                                                int                            Id);
int                 VideoIoctlGetSize          (struct DeviceContext_s*        Context,
                                                video_size_t*                  Size);
int                 VideoIoctlSetSpeed         (struct DeviceContext_s*        Context,
                                                int                            Speed);
int                 VideoIoctlSetPlayInterval  (struct DeviceContext_s*        Context,
                                                video_play_interval_t*         PlayInterval);
int                 PlaybackInit               (struct DeviceContext_s*        Context);
int                 VideoSetOutputWindow       (struct DeviceContext_s*        Context,
                                                unsigned int                   Left,
                                                unsigned int                   Top,
                                                unsigned int                   Width,
                                                unsigned int                   Height);
int                 VideoSetInputWindow        (struct DeviceContext_s*        Context,
                                                unsigned int                   Left,
                                                unsigned int                   Top,
                                                unsigned int                   Width,
                                                unsigned int                   Height);
int                 VideoGetPixelAspectRatio   (struct DeviceContext_s*        Context,
                                                unsigned int*                  Numerator,
                                                unsigned int*                  Denominator);
#endif
