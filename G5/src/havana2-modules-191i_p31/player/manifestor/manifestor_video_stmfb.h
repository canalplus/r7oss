/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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

Source file name : surface.h
Author :           Julian

Definition of the surface class for havana.


Date        Modification                                    Name
----        ------------                                    --------
19-Feb-07   Created                                         Julian

************************************************************************/

#ifndef MANIFESTOR_VIDEO_STMFB_H
#define MANIFESTOR_VIDEO_STMFB_H

#include "osinline.h"
#include <include/stmdisplay.h>
#include "allocinline.h"
#include "manifestor_video.h"

#undef  MANIFESTOR_TAG
#define MANIFESTOR_TAG          "ManifestorVideoStmfb_c::"

#define STMFB_BUFFER_HEADROOM   12              /* Number of buffers to wait after queue is full before we restart queueing buffers */

/// Video manifestor based on the stgfb core driver API.
class Manifestor_VideoStmfb_c : public Manifestor_Video_c
{
private:
    stm_display_device_t*       DisplayDevice;
    stm_display_plane_t*        Plane;
    stm_display_output_t*       Output;

    stm_plane_crop_t            srcRect;
    stm_plane_crop_t            croppedRect;
    stm_plane_crop_t            dstRect;

    unsigned int                VideoBufferBase;

    stm_display_buffer_t        DisplayBuffer[MAXIMUM_NUMBER_OF_DECODE_BUFFERS];

#if defined (QUEUE_BUFFER_CAN_FAIL)
    OS_Semaphore_t              DisplayAvailable;
    bool                        DisplayAvailableValid;
    unsigned int                DisplayHeadroom;
    bool                        DisplayFlush;
    bool                        WaitingForHeadroom;
#endif

    unsigned char*              DisplayAddress;
    unsigned int                DisplaySize;

    int                         ClockRateAdjustment;

public:

    /* Constructor / Destructor */
    Manifestor_VideoStmfb_c                            (void);
    ~Manifestor_VideoStmfb_c                           (void);

    /* Overrides for component base class functions */
    ManifestorStatus_t   Halt                          (void);
    ManifestorStatus_t   Reset                         (void);

    /* Manifestor video class functions */
    ManifestorStatus_t  OpenOutputSurface              (DeviceHandle_t          DisplayDevice,
                                                        unsigned int            PlaneId,
                                                        unsigned int            OutputId);
    ManifestorStatus_t  CloseOutputSurface             (void);
    ManifestorStatus_t  UpdateOutputSurfaceDescriptor  (void);

    ManifestorStatus_t  CreateDecodeBuffers            (unsigned int            Count,
                                                        unsigned int            Width,
                                                        unsigned int            Height);
    ManifestorStatus_t  DestroyDecodeBuffers           (void);

    ManifestorStatus_t  QueueBuffer                    (unsigned int                    BufferIndex,
                                                        struct ParsedFrameParameters_s* FrameParameters,
                                                        struct ParsedVideoParameters_s* VideoParameters,
                                                        struct VideoOutputTiming_s*     VideoOutputTiming,
                                                        struct BufferStructure_s*       BufferStructure );
    ManifestorStatus_t  QueueInitialFrame              (unsigned int                    BufferIndex,
                                                        struct ParsedVideoParameters_s* VideoParameters,
                                                        struct BufferStructure_s*       BufferStructure);
    ManifestorStatus_t  QueueNullManifestation         (void);
    ManifestorStatus_t  FlushDisplayQueue              (void);

    ManifestorStatus_t  Enable                         (void);
    ManifestorStatus_t  Disable                        (void);
    ManifestorStatus_t  UpdateDisplayWindows           (void);
    ManifestorStatus_t  CheckInputDimensions           (unsigned int                    Width,
                                                        unsigned int                    Height);

    ManifestorStatus_t   SynchronizeOutput(             void );

    bool                 BufferAvailable               (unsigned char*                  Address,
                                                        unsigned int                    Size);

    /* The following functions are public because they are accessed via C stub functions */
    void                DisplayCallback                (struct StreamBuffer_s*  Buffer,
                                                        TIME64                  VsyncTime);
    void                InitialFrameDisplayCallback    (struct StreamBuffer_s*  Buffer,
                                                        TIME64                  VsyncTime);
    void                DoneCallback                   (struct StreamBuffer_s*  Buffer,
                                                        TIME64                  VsyncTime,
                                                        unsigned int            Status);
 protected:
    void                SelectDisplayBufferPointers    (struct BufferStructure_s*   BufferStructure,
                                                        struct StreamBuffer_s*       StreamBuff,
                                                        stm_display_buffer_t*       DisplayBuff);

    void                ApplyPixelAspectRatioCorrection( stm_display_buffer_t*       DisplayBuff,
                                                         struct ParsedVideoParameters_s* VideoParameters);

};

#endif
