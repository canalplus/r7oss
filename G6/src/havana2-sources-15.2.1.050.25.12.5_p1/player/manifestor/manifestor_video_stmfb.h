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

#ifndef MANIFESTOR_VIDEO_STMFB_H
#define MANIFESTOR_VIDEO_STMFB_H

#include <stm_registry.h>
#include "osinline.h"
#include <stm_display.h>

#include "allocinline.h"
#include "manifestor_video.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_VideoStmfb_c"

#define STMFB_BUFFER_HEADROOM   12              /* Number of buffers to wait after queue is full before we restart queuing buffers */

#define PLANE_MAX           MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION - 1

struct DisplayPlaneInfo
{
    stm_compound_control_latency_t      MinLatency;
    stm_compound_control_latency_t      MaxLatency;
    long long                           TargetLatency;
    stm_display_mode_t                  CurrentMode;
    stm_display_plane_h                 PlaneHandle;
    unsigned int                        PlaneId;
    stm_display_output_h                OutputHandle;
    unsigned int                        OutputId;
};

/// Video manifestor based on the stgfb core driver API.
class Manifestor_VideoStmfb_c : public Manifestor_Video_c
{
public:
    /* Constructor / Destructor */
    Manifestor_VideoStmfb_c(void);
    ~Manifestor_VideoStmfb_c(void);

    /* Overrides for component base class functions */
    ManifestorStatus_t   Halt(void);
    ManifestorStatus_t   Reset(void);

    /* Manifestor video class functions */
    ManifestorStatus_t  OpenOutputSurface(stm_object_h DisplayHandle);
    ManifestorStatus_t  CloseOutputSurface(void);
    ManifestorStatus_t  UpdateOutputSurfaceDescriptor(void);

    ManifestorStatus_t  GetNumberOfTimings(unsigned int *NumTimes);

    ManifestorStatus_t  QueueBuffer(unsigned int                         BufferIndex,
                                    struct ParsedFrameParameters_s      *FrameParameters,
                                    struct ParsedVideoParameters_s      *VideoParameters,
                                    struct ManifestationOutputTiming_s **VideoOutputTimingArray,
                                    Buffer_t                             Buffer);
    ManifestorStatus_t  QueueNullManifestation(void);
    ManifestorStatus_t  FlushDisplayQueue(void);

    void FRCPatternTrace(unsigned int DisplayFrameIndex
                         , unsigned int DisplayCount_0
                         , unsigned int DisplayCount_1
                         , bool TopFieldFirst);

    ManifestorStatus_t  Enable(void);
    ManifestorStatus_t  Disable(void);
    bool                GetEnable(void);

    ManifestorStatus_t  SynchronizeOutput(void);
    long long           GetPipelineLatency();

    /* The following functions are public because they are accessed via C stub functions */
    void                DisplayCallback(struct StreamBuffer_s  *Buffer,
                                        stm_time64_t            VsyncTime,
                                        uint16_t                output_change_flags,
                                        uint16_t                nb_output,
                                        stm_display_latency_params_t *display_latency_params);

    void                DoneCallback(struct StreamBuffer_s  *Buffer,
                                     stm_time64_t            VsyncTime,
                                     unsigned int            Status);

private:
    stm_display_device_h        DisplayDevice;
    stm_display_source_h        Source;
    stm_display_source_queue_h  QueueInterface;

    OS_Mutex_t                  DisplayPlaneMutex;
    int                         NumPlanes;
    DisplayPlaneInfo            DisplayPlanes[PLANE_MAX];

    OS_SpinLock_t               DisplayCallbackSpinLock;

    bool                        TopologyChanged;
    bool                        DisplayModeChanged;

    int                         PlaneSelectedForAVsync;
    Rational_t                  LastSourceFrameRate;

    stm_display_buffer_t        DisplayBuffer[MAX_DECODE_BUFFERS];
    long long                   DisplayLatencyTarget[MAX_DECODE_BUFFERS];

    int                         ClockRateAdjustment[MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION];

    DISALLOW_COPY_AND_ASSIGN(Manifestor_VideoStmfb_c);

    void                FillInDisplayBufferDstFields(stm_display_buffer_t       *DisplayBuff);

    void                FillInDisplayBufferSrcFields(Buffer_t                    Buffer,
                                                     struct StreamBuffer_s      *StreamBuff,
                                                     struct ParsedVideoParameters_s *VideoParameters,
                                                     stm_display_buffer_t       *DisplayBuff,
                                                     struct ManifestationOutputTiming_s **VideoOutputTimingArray);
    void                FillInDisplayBufferInfoFields(struct StreamBuffer_s      *StreamBuff,
                                                      struct ManifestationOutputTiming_s **VideoOutputTimingArray,
                                                      stm_display_buffer_t       *DisplayBuff);

    void                FillInDisplayBuffer(Buffer_t                    Buffer,
                                            struct StreamBuffer_s      *StreamBuff,
                                            struct ParsedVideoParameters_s *VideoParameters,
                                            struct ManifestationOutputTiming_s **VideoOutputTimingArray,
                                            stm_display_buffer_t       *DisplayBuff);

    void                SelectDisplaySource(Buffer_t                    Buffer,
                                            stm_display_buffer_t       *DisplayBuff);

    void                SelectDisplayBufferPointers(Buffer_t                    Buffer,
                                                    struct StreamBuffer_s      *StreamBuff,
                                                    stm_display_buffer_t       *DisplayBuff);

    void                ApplyPixelAspectRatioCorrection(stm_display_buffer_t       *DisplayBuff,
                                                        struct ParsedVideoParameters_s *VideoParameters);

    ManifestorStatus_t  SetOutputRateAdjustment(unsigned int TimingIndex,
                                                struct StreamBuffer_s      *StreamBuff);

    ManifestorStatus_t  UpdatePlaneLatency(int OutputIndex);
    void                GetPlaneLatency(int OutputIndex,
                                        long long *MinLatency,
                                        long long *MaxLatency);

    ManifestorStatus_t  OpenDisplayPlaneHandles(void);
    void                CloseDisplayPlaneHandles(void);

    ManifestorStatus_t  OpenDisplaySourceDevice(stm_object_h    DisplayHandle);
    void                CloseDisplaySourceDevice(void);

    ManifestorStatus_t  UpdateDisplayMode(OutputSurfaceDescriptor_t *SurfaceDesciptor, unsigned int OutputIndex);
    Rational_c          ConvertDisplayFramerateToRational(uint32_t displayVerticalRefreshRate);

    void                SanityCheckVibeLatencies(uint16_t                nb_output,
                                                 stm_display_latency_params_t *display_latency_params);
    int                 GetPlaneIndex(unsigned int OutputId);
    long long           GetDisplayLatencyTarget();

    void                PrintSurfaceDescriptors();
    ManifestorStatus_t  UpdateManifestedState(struct StreamBuffer_s *StreamBuff, ManifestedState_t ManifestedState);

    ManifestorStatus_t  UpdatePlaneSurfaceDescriptor(int i);
    void                UpdateDisplaySourceSurfaceDescriptor();
    ManifestorStatus_t  FillDisplayPlaneSurfaceDescriptors();
    ManifestorStatus_t  FillDisplaySourceSurfaceDescriptor();
    ManifestorStatus_t  FillDisplaySurfaceDescriptors();

    bool                isMainPlane(int PlaneIndex);
    bool                isAuxPlane(int PlaneIndex);
    bool                isPipPlane(int PlaneIndex);
    bool                isSharedPipPlane(int PlaneIndex);
    bool                CheckPicturePolarityRespect(struct ManifestationOutputTiming_s **VideoOutputTimingArray, Buffer_t Buffer);

    void                ReportQueueBufferFailedEvent(int BufferIndex);

    static const int    kMainPlaneCapabilities  = PLANE_CAPS_VIDEO | PLANE_CAPS_PRIMARY_OUTPUT | PLANE_CAPS_VIDEO_BEST_QUALITY | PLANE_CAPS_PRIMARY_PLANE;
    static const int    kAuxPlaneCapabilities   = PLANE_CAPS_VIDEO | PLANE_CAPS_SECONDARY_OUTPUT;
    static const int    kPipPlaneCapabilities   = PLANE_CAPS_VIDEO | PLANE_CAPS_PRIMARY_OUTPUT;
    static const int    kSharedPipPlaneCapabilities = PLANE_CAPS_VIDEO | PLANE_CAPS_PRIMARY_OUTPUT | PLANE_CAPS_SECONDARY_OUTPUT;

    void CheckAllBuffersReturned();
};


#endif
