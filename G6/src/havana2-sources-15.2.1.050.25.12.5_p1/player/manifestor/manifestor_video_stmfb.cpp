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

#include "osinline.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "manifestor_video_stmfb.h"
#include "post_manifest_edge.h"
#include "timestamps.h"
#define ENOTSUP EOPNOTSUPP

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_VideoStmfb_c"

#define HW_DELAY    -10500

#define PIXEL_DEPTH                     16

extern "C" int snprintf(char *buf, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4))) ;

struct QueueRecord_s
{
    unsigned int        Flags;
    unsigned int        Count;
};

static void display_callback(void           *Buffer,
                             stm_time64_t    VsyncTime,
                             uint16_t        output_change,
                             uint16_t        nb_output,
                             stm_display_latency_params_t *display_latency_params);

static void done_callback(void           *Buffer,
                          const stm_buffer_presentation_stats_t *Data);


Manifestor_VideoStmfb_c::Manifestor_VideoStmfb_c(void)
    : Manifestor_Video_c()
    , DisplayDevice(NULL)
    , Source(NULL)
    , QueueInterface(NULL)
    , DisplayPlaneMutex()
    , NumPlanes(0)
    , DisplayPlanes()
    , DisplayCallbackSpinLock()
    , TopologyChanged(true)
    , DisplayModeChanged(true)
    , PlaneSelectedForAVsync(-1)
    , LastSourceFrameRate(0)
    , DisplayBuffer()
    , DisplayLatencyTarget()
    , ClockRateAdjustment()
{
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p\n", Stream, this);

    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Stream 0x%p this 0x%p Initialization status not valid - aborting init\n", Stream, this);
        return;
    }

    SetGroupTrace(group_manifestor_video_stmfb);

    Configuration.Capabilities  = MANIFESTOR_CAPABILITY_DISPLAY;

    for (int i = 0; i < MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; i++)
    {
        SurfaceDescriptor[i].StreamType                   = StreamTypeVideo;
        SurfaceDescriptor[i].ClockPullingAvailable        = true;
        SurfaceDescriptor[i].MasterCapable                = true;
        SurfaceDescriptor[i].InheritRateAndTypeFromSource = false;
        SurfaceDescriptor[i].PercussiveCapable            = true;
        SurfaceDescriptor[i].OutputType                   = SURFACE_OUTPUT_TYPE_VIDEO_NO_OUTPUT;
        SurfaceDescriptor[i].IsSlavedSurface              = false;
        SurfaceDescriptor[i].MasterSurface                = NULL;
        SurfaceDescriptor[i].FrameRate                    = 0;
    }

    OS_InitializeSpinLock(&DisplayCallbackSpinLock);
    OS_InitializeMutex(&DisplayPlaneMutex);

    NumberOfTimings = 0;  // TODO(S.L.D) fixme can this be removed: belong to base class (init with 1)
}

Manifestor_VideoStmfb_c::~Manifestor_VideoStmfb_c(void)
{
    SE_DEBUG(group_manifestor_video_stmfb, ">Stream 0x%p this 0x%p\n", Stream, this);

    if (TestComponentState(ComponentRunning))
    {
        Halt();
    }

    if (Source != NULL)
    {
        CloseOutputSurface();
    }

    OS_TerminateSpinLock(&DisplayCallbackSpinLock);
    OS_TerminateMutex(&DisplayPlaneMutex);

    SE_DEBUG(group_manifestor_video_stmfb, "<Stream 0x%p this 0x%p\n", Stream, this);
}

void Manifestor_VideoStmfb_c::CheckAllBuffersReturned()
{
    for (int i = 0; i < MAX_DECODE_BUFFERS; i++)
    {
        if (StreamBuffer[i].BufferState != BufferStateAvailable)
        {
            SE_ERROR("Stream 0x%p this 0x%p Buffer #%d (state %d, QueueCount %d) not returned by Vibe!\n",
                     Stream, this, i, StreamBuffer[i].BufferState, StreamBuffer[i].QueueCount);
        }
    }
}

//  Halt
//  doxynote
/// \brief              Shutdown, stop presenting and retrieving frames
///                     don't return until complete
//
ManifestorStatus_t      Manifestor_VideoStmfb_c::Halt(void)
{
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p\n", Stream, this);

    if (QueueInterface != NULL)
    {
        SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p >stm_display_source_queue_flush\n", Stream, this);
        if (stm_display_source_queue_flush(QueueInterface, true) < 0)
        {
            SE_ERROR("Stream 0x%p this 0x%p Not able to flush display source queue\n", Stream, this);
        }

        SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p <stm_display_source_queue_flush\n", Stream, this);

        CheckAllBuffersReturned();

        PtsOnDisplay    = INVALID_TIME;
    }

    return Manifestor_Video_c::Halt();
}

//  Reset
/// \brief              Reset all state to initial values
ManifestorStatus_t Manifestor_VideoStmfb_c::Reset(void)
{
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p\n", Stream, this);

    if (TestComponentState(ComponentRunning))
    {
        Halt();
    }

    for (int i = 0; i < MAX_DECODE_BUFFERS; i++)
    {
        DisplayBuffer[i].info.display_callback          = NULL;
        DisplayBuffer[i].info.completed_callback        = NULL;
        DisplayLatencyTarget[i]                         = 0;
    }

    return Manifestor_Video_c::Reset();
}

Rational_c Manifestor_VideoStmfb_c::ConvertDisplayFramerateToRational(uint32_t displayVerticalRefreshRate)
{
    switch (displayVerticalRefreshRate)
    {
    case 59940:
        return Rational_c(60000, 1001);

    case 23976:
        return Rational_c(24000, 1001);

    case 29970:
        return Rational_c(30000, 1001);

    default:
        return Rational_c(displayVerticalRefreshRate, 1000);
    }
}

ManifestorStatus_t Manifestor_VideoStmfb_c::UpdateDisplayMode(OutputSurfaceDescriptor_t *SurfaceDescriptorPtr, unsigned int PlaneIndex)
{
    int Result = stm_display_output_get_current_display_mode(DisplayPlanes[PlaneIndex].OutputHandle, &DisplayPlanes[PlaneIndex].CurrentMode);
    if (Result != 0)
    {
        SE_ERROR("Stream 0x%p this 0x%p Failed to get display mode (%d)\n", Stream, this, Result);
        return ManifestorError;
    }

    SurfaceDescriptorPtr->DisplayWidth = DisplayPlanes[PlaneIndex].CurrentMode.mode_params.active_area_width;

    // The active area for 480p is actually 483 lines, but by default
    // we don't want to scale up 480 content to that odd number.
    if (DisplayPlanes[PlaneIndex].CurrentMode.mode_params.active_area_height == 483)
    {
        SurfaceDescriptorPtr->DisplayHeight       = 480;
    }
    else
    {
        SurfaceDescriptorPtr->DisplayHeight       = DisplayPlanes[PlaneIndex].CurrentMode.mode_params.active_area_height;
    }

    if (!SurfaceDescriptorPtr->InheritRateAndTypeFromSource || SurfaceDescriptorPtr->FrameRate == 0)
    {
        SurfaceDescriptorPtr->FrameRate     = ConvertDisplayFramerateToRational(DisplayPlanes[PlaneIndex].CurrentMode.mode_params.vertical_refresh_rate);
        SurfaceDescriptorPtr->Progressive   = (DisplayPlanes[PlaneIndex].CurrentMode.mode_params.scan_type == STM_PROGRESSIVE_SCAN);
    }

    return ManifestorNoError;
}

bool Manifestor_VideoStmfb_c::isMainPlane(int PlaneIndex)
{
    stm_plane_capabilities_t    caps;
    int Result = stm_display_plane_get_capabilities(DisplayPlanes[PlaneIndex].PlaneHandle, &caps);
    SE_ASSERT(Result == 0);
    return ((caps & kMainPlaneCapabilities) == kMainPlaneCapabilities);
}

bool Manifestor_VideoStmfb_c::isPipPlane(int PlaneIndex)
{
    stm_plane_capabilities_t    caps;
    int Result = stm_display_plane_get_capabilities(DisplayPlanes[PlaneIndex].PlaneHandle, &caps);
    SE_ASSERT(Result == 0);
    return (caps == kPipPlaneCapabilities);
}

bool Manifestor_VideoStmfb_c::isSharedPipPlane(int PlaneIndex)
{
    stm_plane_capabilities_t    caps;
    int Result = stm_display_plane_get_capabilities(DisplayPlanes[PlaneIndex].PlaneHandle, &caps);
    SE_ASSERT(Result == 0);
    return (caps == kSharedPipPlaneCapabilities);
}

bool Manifestor_VideoStmfb_c::isAuxPlane(int PlaneIndex)
{
    stm_plane_capabilities_t    caps;
    int Result = stm_display_plane_get_capabilities(DisplayPlanes[PlaneIndex].PlaneHandle, &caps);
    SE_ASSERT(Result == 0);
    return ((caps & kAuxPlaneCapabilities) == kAuxPlaneCapabilities);
}

long long Manifestor_VideoStmfb_c::GetDisplayLatencyTarget()
{
    // Our target will be the middle value between the min and max latency of the output
    // selected for avsync. It is what we will converge towards when controlling both clocks
    long long MinLatency, MaxLatency;
    SE_ASSERT(PlaneSelectedForAVsync >= 0);
    GetPlaneLatency(PlaneSelectedForAVsync, &MinLatency, &MaxLatency);
    long long latencyTarget = (MinLatency + MaxLatency) / 2;
    SE_EXTRAVERB(group_avsync, "Stream 0x%p this 0x%p lateny target %lld\n", Stream, this, latencyTarget);
    return latencyTarget;
}

void Manifestor_VideoStmfb_c::PrintSurfaceDescriptors()
{
    for (unsigned int i = 0; i < NumberOfTimings; i++)
    {
        SE_INFO(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p SurfaceDescriptor #%d: Width %d, height %d, FrameRate %d.%06d, Progressive %d"
                " OutputType=%d PercussiveCapable=%d InheritFrameRate=%d ClockPulling=%d\n",
                Stream, this, i, SurfaceDescriptor[i].DisplayWidth, SurfaceDescriptor[i].DisplayHeight,
                SurfaceDescriptor[i].FrameRate.IntegerPart(), SurfaceDescriptor[i].FrameRate.RemainderDecimal(),
                SurfaceDescriptor[i].Progressive, SurfaceDescriptor[i].OutputType, SurfaceDescriptor[i].PercussiveCapable,
                SurfaceDescriptor[i].InheritRateAndTypeFromSource, SurfaceDescriptor[i].ClockPullingAvailable);
    }
}

bool Manifestor_VideoStmfb_c::CheckPicturePolarityRespect(struct ManifestationOutputTiming_s **VideoOutputTimingArray, Buffer_t Buffer)
{

    const uint32_t NumRefPlane = 1;
    // Nothing to do no plane is connected to the source
    if (NumPlanes < 1)
    {
        SE_VERBOSE(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p At least 1 plane required\n", Stream, this);
        return false;
    }

    // In case more than 1 plane is connected to the source, they will have the same VTG
    // so we can take the first one (NumRefPlane) as reference

    // Stream and Output must be interlaced
    if ((VideoOutputTimingArray[SOURCE_INDEX]->Interlaced == false) ||
        (SurfaceDescriptor [NumRefPlane].Progressive == true))
    {
        SE_EXTRAVERB(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p Interlaced Stream and Output required\n", Stream, this);
        return false;
    }

    // Must be in IPBB display mode
    if ((Player->PolicyValue(Playback, Stream, PolicyStreamOnlyKeyFrames) == PolicyValueApply) ||
        (Player->PolicyValue(Playback, Stream, PolicyStreamDiscardFrames) != PolicyValueNoDiscard))
    {
        SE_EXTRAVERB(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p IPBB display mode required\n", Stream, this);
        return false;
    }

    // Must be in normal speed
    if (((PlayerPlayback_s *)Playback)->mSpeed != 1)
    {
        SE_EXTRAVERB(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p Speed 1 required\n", Stream, this);
        return false;
    }

    // Make sure there is no FRC
    struct ParsedVideoParameters_s      *VideoParameters;

    Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)&VideoParameters);
    SE_ASSERT(VideoParameters != NULL);

    // Must multiple Stream rate by 2 (i.e. Output 50 interlaced  => stream at 25 fps intelaced)
    if (SurfaceDescriptor [NumRefPlane].FrameRate != (VideoParameters->Content.FrameRate * 2))
    {
        SE_EXTRAVERB(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p FRC needed Stream rate %lld/%lld Output rate %lld/%lld\n", Stream, this,
                     VideoParameters->Content.FrameRate.GetNumerator(),
                     VideoParameters->Content.FrameRate.GetDenominator(),
                     SurfaceDescriptor [NumRefPlane].FrameRate.GetNumerator(),
                     SurfaceDescriptor [NumRefPlane].FrameRate.GetDenominator()
                    );
        return false;
    }

    SE_EXTRAVERB(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p Picture Polarity needed\n", Stream, this);
    return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
//                          Cannes Case: Single clock
//--------------------------------------------------------------------------------------------------------------------------------

ManifestorStatus_t Manifestor_VideoStmfb_c::UpdatePlaneSurfaceDescriptor(int PlaneIndex)
{
    ManifestorStatus_t Status = UpdatePlaneLatency(PlaneIndex);
    if (Status != ManifestorNoError) { return Status; }

    Status = UpdateDisplayMode(&SurfaceDescriptor[PlaneIndex + 1], PlaneIndex);
    if (Status != ManifestorNoError) { return Status; }

    OutputSurfaceDescriptor_t *PlaneSurface         = &SurfaceDescriptor[PlaneIndex + 1];

    PlaneSurface->ClockPullingAvailable             = true;
    PlaneSurface->InheritRateAndTypeFromSource      = false;
    PlaneSurface->IsSlavedSurface                   = true;
    PlaneSurface->MasterSurface                     = &SurfaceDescriptor[SOURCE_INDEX];
    PlaneSurface->PercussiveCapable                 = true;

    if (isMainPlane(PlaneIndex) == true)
    {
        PlaneSurface->OutputType             = SURFACE_OUTPUT_TYPE_VIDEO_MAIN;
    }
    else if (isPipPlane(PlaneIndex) == true)
    {
        PlaneSurface->OutputType             = SURFACE_OUTPUT_TYPE_VIDEO_PIP;
    }
    else if (isAuxPlane(PlaneIndex) == true)
    {
        PlaneSurface->OutputType             = SURFACE_OUTPUT_TYPE_VIDEO_AUX;
    }
    else
    {
        PlaneSurface->OutputType             = SURFACE_OUTPUT_TYPE_VIDEO_NO_OUTPUT;
    }
    SE_INFO(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p PlaneSurface %p Discovered surface with OutputType %d\n",
            Stream, this, PlaneSurface, PlaneSurface->OutputType);

    return ManifestorNoError;
}

void Manifestor_VideoStmfb_c::UpdateDisplaySourceSurfaceDescriptor()
{
    SurfaceDescriptor[SOURCE_INDEX].ClockPullingAvailable           = false;
    SurfaceDescriptor[SOURCE_INDEX].InheritRateAndTypeFromSource    = false;
    SurfaceDescriptor[SOURCE_INDEX].IsSlavedSurface                 = false;
    SurfaceDescriptor[SOURCE_INDEX].PercussiveCapable               = false;
    SurfaceDescriptor[SOURCE_INDEX].OutputType                      = SURFACE_OUTPUT_TYPE_VIDEO_NO_OUTPUT;
}

ManifestorStatus_t Manifestor_VideoStmfb_c::FillDisplayPlaneSurfaceDescriptors()
{
    for (int i = 0; i < NumPlanes; i++)
    {
        UpdatePlaneSurfaceDescriptor(i);
    }

    // Only one plane for Cannes, so select index 0
    PlaneSelectedForAVsync = 0;

    NumberOfTimings = NumPlanes + 1;
    SE_ASSERT(NumberOfTimings <= MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION);

    return ManifestorNoError;
}

ManifestorStatus_t Manifestor_VideoStmfb_c::FillDisplaySourceSurfaceDescriptor()
{
    UpdateDisplaySourceSurfaceDescriptor();

    ManifestorStatus_t Status = UpdateDisplayMode(&SurfaceDescriptor[SOURCE_INDEX], PlaneSelectedForAVsync);
    if (Status != ManifestorNoError) { return Status; }

    return ManifestorNoError;
}

ManifestorStatus_t Manifestor_VideoStmfb_c::FillDisplaySurfaceDescriptors()
{
    ManifestorStatus_t Status = FillDisplayPlaneSurfaceDescriptors();
    if (Status != ManifestorNoError) { return Status; }

    Status = FillDisplaySourceSurfaceDescriptor();
    if (Status != ManifestorNoError) { return Status; }

    return ManifestorNoError;
}

//--------------------------------------------------------------------------------------------------------------------------------


//
//  UpdateOutputSurfaceDescriptor
// This method is called before queuing each buffer.
// When it is called, the timing recors have already been filled by the output_timer
//  doxynote
/// \brief      Find out information about display plane and output
/// \return     ManifestorNoError if done successfully
//
ManifestorStatus_t Manifestor_VideoStmfb_c::UpdateOutputSurfaceDescriptor(void)
{
    bool topologyChanged, displayModeChanged, sourceFrameRateChanged;
    OS_LockSpinLockIRQSave(&DisplayCallbackSpinLock);
    topologyChanged     = TopologyChanged;
    displayModeChanged  = DisplayModeChanged;
    TopologyChanged     = false;
    DisplayModeChanged  = false;
    OS_UnLockSpinLockIRQRestore(&DisplayCallbackSpinLock);
    sourceFrameRateChanged  = SurfaceDescriptor[SOURCE_INDEX].FrameRate != LastSourceFrameRate;
    LastSourceFrameRate     = SurfaceDescriptor[SOURCE_INDEX].FrameRate;

    if (topologyChanged)
    {
        SE_INFO(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p Display Topology changed!\n", Stream, this);
    }

    if (sourceFrameRateChanged)
    {
        SE_INFO(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p Source Frame Rate changed!\n", Stream, this);
    }

    if (topologyChanged)
    {
        OS_LockMutex(&DisplayPlaneMutex);

        CloseDisplayPlaneHandles();

        ManifestorStatus_t Status = OpenDisplayPlaneHandles();
        if (Status != ManifestorNoError)
        {
            OS_UnLockMutex(&DisplayPlaneMutex);
            return Status;
        }

        OS_UnLockMutex(&DisplayPlaneMutex);
    }

    if (topologyChanged || displayModeChanged || sourceFrameRateChanged)
    {
        FillDisplaySurfaceDescriptors();

        if (displayModeChanged)
        {
            char rate[16];
            char rate_decimalpart[4];

            snprintf(rate_decimalpart, sizeof(rate_decimalpart), ".%02d", SurfaceDescriptor[SOURCE_INDEX].FrameRate.RemainderDecimal() * 10000);
            snprintf(rate, sizeof(rate), "%d%s"
                     , SurfaceDescriptor[SOURCE_INDEX].FrameRate.IntegerPart()
                     , SurfaceDescriptor[SOURCE_INDEX].FrameRate.RemainderDecimal() != 0 ? rate_decimalpart : "");

            // SE-PIPELINE trace
            SE_INFO2(group_manifestor_video_stmfb, group_frc, "Stream 0x%p this 0x%p Display Mode changed : %dx%d-%s%s\n"
                     , Stream
                     , this
                     , SurfaceDescriptor[SOURCE_INDEX].DisplayWidth
                     , SurfaceDescriptor[SOURCE_INDEX].DisplayHeight
                     , rate
                     , SurfaceDescriptor[SOURCE_INDEX].Progressive ? "" : "i");
        }

        PrintSurfaceDescriptors();
    }

    return ManifestorNoError;
}


ManifestorStatus_t Manifestor_VideoStmfb_c::OpenDisplaySourceDevice(stm_object_h    DisplayHandle)
{
    int Result;
    stm_display_source_h                    source;
    stm_display_source_interface_params_t   InterfaceParams;
    unsigned int                            DeviceId;
    SE_DEBUG(group_manifestor_video_stmfb, "\n");
    source = (stm_display_source_h)DisplayHandle;
    InterfaceParams.interface_type  = STM_SOURCE_QUEUE_IFACE;

    Result = stm_display_source_get_interface(source, InterfaceParams, (void **)&QueueInterface);
    if (Result != 0)
    {
        SE_ERROR("Stream 0x%p this 0x%p Failed to get queue interface from source 0x%p (%d)\n", Stream, this, source, Result);
        return ManifestorError;
    }

    Result = stm_display_source_get_device_id(source, &DeviceId);
    if (Result != 0)
    {
        SE_ERROR("Stream 0x%p this 0x%p Failed to get device id (%d)\n", Stream, this, Result);
        return ManifestorError;
    }

    Result = stm_display_open_device(DeviceId, &DisplayDevice);
    if (Result != 0)
    {
        SE_ERROR("Stream 0x%p this 0x%p Failed to open device (%d)\n", Stream, this, Result);
        return ManifestorError;
    }

    // Lock this queue for our exclusive usage
    Result = stm_display_source_queue_lock(QueueInterface, true);
    if (Result != 0)
    {
        SE_ERROR("Stream 0x%p this 0x%p Queue interface could not be locked (%d)\n", Stream, this, Result);
        return ManifestorError;
    }

    Source = source;
    return ManifestorNoError;
}

void Manifestor_VideoStmfb_c::CloseDisplaySourceDevice()
{
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p Source 0x%p, QueueInterface 0x%p\n", Stream, this, Source, QueueInterface);

    if (QueueInterface != NULL)
    {
        int Result = stm_display_source_queue_unlock(QueueInterface);
        if (Result != 0)
        {
            SE_ERROR("Stream 0x%p this 0x%p Cannot unlock queue interface from source (%d)\n", Stream, this, Result);
        }

        Result = stm_display_source_queue_release(QueueInterface);
        if (Result != 0)
        {
            SE_ERROR("Stream 0x%p this 0x%p Source is not connected to queue interface (%d)\n", Stream, this, Result);
        }

        QueueInterface = NULL;
    }

    if (DisplayDevice != NULL)
    {
        stm_display_device_close(DisplayDevice);
        DisplayDevice = NULL;
    }

    Source = NULL;
}


//
//  OpenOutputSurface
//  doxynote
/// \brief      Use opaque handle to access queue interface
/// \param      DisplayHandle Handle of the stmfb source object.
/// \return     ManifestorNoError if queue interface available
//
ManifestorStatus_t Manifestor_VideoStmfb_c::OpenOutputSurface(stm_object_h    DisplayHandle)
{
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p\n", Stream, this);
    ManifestorStatus_t Status = OpenDisplaySourceDevice(DisplayHandle);

    if (Status != ManifestorNoError) { return Status; }

    Visible             = false;

    UpdateOutputSurfaceDescriptor();

    // reset variables so that the display planes
    // are re-opened on first buffer
    TopologyChanged = true;
    DisplayModeChanged = true;

    return ManifestorNoError;
}

//
//  CloseOutputSurface
//  doxynote
/// \brief              Release all frame buffer resources
//
ManifestorStatus_t Manifestor_VideoStmfb_c::CloseOutputSurface(void)
{
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p\n", Stream, this);
    CloseDisplayPlaneHandles();
    CloseDisplaySourceDevice();
    return ManifestorNoError;
}

ManifestorStatus_t Manifestor_VideoStmfb_c::OpenDisplayPlaneHandles(void)
{
    int Count, Result;
    unsigned int PlaneIds[PLANE_MAX];

    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p\n", Stream, this);

    // we assume a 1:1 mapping of DisplayPlanes[].PlaneHandle to DisplayPlanes[].OutputHandle which is currently true
    NumPlanes = stm_display_source_get_connected_plane_id(Source, PlaneIds, PLANE_MAX);
    if (NumPlanes < 0)
    {
        SE_ERROR("Stream 0x%p this 0x%p Failed to get connected plane id (%d)\n", Stream, this, NumPlanes);
        return ManifestorError;
    }

    SE_ASSERT(NumPlanes <= PLANE_MAX);
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p %d Planes connected to source 0x%p\n", Stream, this, NumPlanes, Source);

    for (int i = 0; i < NumPlanes; i++)
    {
        DisplayPlanes[i].PlaneId = PlaneIds[i];
        Result = stm_display_device_open_plane(DisplayDevice, DisplayPlanes[i].PlaneId, &DisplayPlanes[i].PlaneHandle);
        if (Result != 0)
        {
            SE_ERROR("Stream 0x%p this 0x%p Failed to get plane %x (%d)\n", Stream, this, DisplayPlanes[i].PlaneId, Result);
            goto open_display_planes_error;
        }

        Count = stm_display_plane_get_connected_output_id(DisplayPlanes[i].PlaneHandle, &DisplayPlanes[i].OutputId, 1);
        if (Count != 1)
        {
            SE_ERROR("Stream 0x%p this 0x%p Failed to get connected output or too many outputs on plane (%d)\n", Stream, this, Count);
            goto open_display_planes_error;
        }

        Result = stm_display_device_open_output(DisplayDevice, DisplayPlanes[i].OutputId, &DisplayPlanes[i].OutputHandle);
        if (Result != 0)
        {
            SE_ERROR("Stream 0x%p this 0x%p Failed to open OutputId[%d] (%d)\n", Stream, this, i, Result);
            goto open_display_planes_error;
        }

        SE_INFO(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p Source 0x%p DisplayDevice 0x%p PlaneId %d Plane 0x%p OutputId %d Output 0x%p\n",
                Stream, this, Source, DisplayDevice, DisplayPlanes[i].PlaneId, DisplayPlanes[i].PlaneHandle, DisplayPlanes[i].OutputId, DisplayPlanes[i].OutputHandle);
    }

    return ManifestorNoError;
open_display_planes_error:
    CloseDisplayPlaneHandles();
    return ManifestorError;
}


void Manifestor_VideoStmfb_c::CloseDisplayPlaneHandles(void)
{
    for (int i = 0; i < NumPlanes; i++)
    {
        if (DisplayPlanes[i].PlaneHandle != NULL)
        {
            stm_display_plane_close(DisplayPlanes[i].PlaneHandle);
            DisplayPlanes[i].PlaneHandle = NULL;
        }

        if (DisplayPlanes[i].OutputHandle != NULL)
        {
            stm_display_output_close(DisplayPlanes[i].OutputHandle);
            DisplayPlanes[i].OutputHandle = NULL;
        }
    }

    NumPlanes = 0;
}

//  Some shared functions used for queuing buffers
void Manifestor_VideoStmfb_c::SelectDisplaySource(Buffer_t                    Buffer,
                                                  stm_display_buffer_t       *DisplayBuff)
{
    /////////// Temporary code to retrieve the full picture width /////////////
    struct ParsedVideoParameters_s *VideoParameters;
    Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)&VideoParameters);
    SE_ASSERT(VideoParameters != NULL);

    DisplayBuff->src.visible_area.x = VideoParameters->Content.DisplayX;
    DisplayBuff->src.visible_area.y = VideoParameters->Content.DisplayY;
    DisplayBuff->src.visible_area.width = VideoParameters->Content.DisplayWidth;
    DisplayBuff->src.visible_area.height = VideoParameters->Content.DisplayHeight;

    // Need for DisplayBuff->src.pan_and_scan_rect calculation based on Width/DisplayWidth & Height/DisplayHeight to set
    // area of interest for aspect ratio conversion performed by STMFB
    //////////////////////////////////////////////////////////////////////////
    DisplayBuff->src.primary_picture.width = Stream->GetDecodeBufferManager()->ComponentDimension(Buffer, PrimaryManifestationComponent, 0);
    DisplayBuff->src.primary_picture.height = Stream->GetDecodeBufferManager()->ComponentDimension(Buffer, PrimaryManifestationComponent, 1);

    switch (Stream->GetDecodeBufferManager()->ComponentDataType(PrimaryManifestationComponent))
    {
    case FormatVideo420_PairedMacroBlock:
    case FormatVideo420_MacroBlock:
        DisplayBuff->src.primary_picture.color_fmt         = SURF_YCBCR420MB;
        DisplayBuff->src.primary_picture.pixel_depth       = 8;
        break;

    case FormatVideo420_Raster2B:
        DisplayBuff->src.primary_picture.color_fmt         = SURF_YCbCr420R2B;
        DisplayBuff->src.primary_picture.pixel_depth       = 8;
        break;

    case FormatVideo8888_ARGB:
        DisplayBuff->src.primary_picture.color_fmt         = SURF_ARGB8888;
        DisplayBuff->src.primary_picture.pixel_depth       = 32;
        DisplayBuff->src.flags           |= STM_BUFFER_SRC_LIMITED_RANGE_ALPHA;
        break;

    case FormatVideo888_RGB:
        DisplayBuff->src.primary_picture.color_fmt         = SURF_RGB888;
        DisplayBuff->src.primary_picture.pixel_depth       = 24;
        break;

    case FormatVideo565_RGB:
        DisplayBuff->src.primary_picture.color_fmt         = SURF_RGB565;
        DisplayBuff->src.primary_picture.pixel_depth       = 16;
        break;

    case FormatVideo422_Raster:
        DisplayBuff->src.primary_picture.color_fmt         = SURF_YCBCR422R;
        DisplayBuff->src.primary_picture.pixel_depth       = 16;
        break;

    case FormatVideo420_PlanarAligned:
    case FormatVideo420_Planar:
        DisplayBuff->src.primary_picture.color_fmt         = SURF_YUV420;
        DisplayBuff->src.primary_picture.pixel_depth       = 8;
        break;

    case FormatVideo422_Planar:
        DisplayBuff->src.primary_picture.color_fmt         = SURF_YUV422P;
        DisplayBuff->src.primary_picture.pixel_depth       = 8;
        break;

    case FormatVideo422_YUYV:
        DisplayBuff->src.primary_picture.color_fmt         = SURF_YUYV;
        DisplayBuff->src.primary_picture.pixel_depth       = 16;
        break;

    case FormatVideo422_Raster2B:
        DisplayBuff->src.primary_picture.color_fmt         = SURF_YCbCr422R2B;
        DisplayBuff->src.primary_picture.pixel_depth       = 8;
        break;

    case FormatVideo444_Raster:
        DisplayBuff->src.primary_picture.color_fmt         = SURF_CRYCB888;
        DisplayBuff->src.primary_picture.pixel_depth       = 24;
        break;

    default:
        SE_ERROR("Stream 0x%p this 0x%p Unsupported display format (%d)\n",
                 Stream, this, Stream->GetDecodeBufferManager()->ComponentDataType(PrimaryManifestationComponent));
        break;
    }

    if (Stream->GetDecodeBufferManager()->ComponentPresent(Buffer, DecimatedManifestationComponent))
    {
        DisplayBuff->src.secondary_picture.width = Stream->GetDecodeBufferManager()->ComponentDimension(Buffer, DecimatedManifestationComponent, 0);
        DisplayBuff->src.secondary_picture.height = Stream->GetDecodeBufferManager()->ComponentDimension(Buffer, DecimatedManifestationComponent, 1);
        // Here we assume FormatVideo returned by Stream->GetDecodeBufferManager()->ComponentDataType(DecimatedManifestationComponent) shall be
        // then same than Stream->GetDecodeBufferManager()->ComponentDataType(PrimaryManifestationComponent)
        DisplayBuff->src.secondary_picture.color_fmt         = DisplayBuff->src.primary_picture.color_fmt;
        DisplayBuff->src.secondary_picture.pixel_depth       = DisplayBuff->src.primary_picture.pixel_depth;
    }
}

static int SelectColourMatrixCoefficients(struct ParsedVideoParameters_s *VideoParameters)
{
    unsigned int Flags = 0;

    switch (VideoParameters->Content.ColourMatrixCoefficients)
    {
    case MatrixCoefficients_ITU_R_BT601:
    case MatrixCoefficients_ITU_R_BT470_2_M:
    case MatrixCoefficients_ITU_R_BT470_2_BG:
    case MatrixCoefficients_SMPTE_170M:
    case MatrixCoefficients_FCC:
        // Do nothing, use 601 coefficients
        break;

    case MatrixCoefficients_ITU_R_BT709:
    case MatrixCoefficients_SMPTE_240M:
        // Use 709 coefficients
        Flags                          |= STM_BUFFER_SRC_COLORSPACE_709;
        break;

    case MatrixCoefficients_Undefined:
    default:

        // Base coefficients on display size SD=601, HD = 709
        if (VideoParameters->Content.Width > 720)
        {
            Flags                      |= STM_BUFFER_SRC_COLORSPACE_709;
        }

        break;
    }

    return Flags;
}

void Manifestor_VideoStmfb_c::SelectDisplayBufferPointers(Buffer_t   Buffer,
                                                          struct StreamBuffer_s  *StreamBuff,
                                                          stm_display_buffer_t   *DisplayBuff)
{
    // Fill in src fields depends on decimation
    DisplayBuff->src.primary_picture.video_buffer_addr      = (uint32_t)Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, PrimaryManifestationComponent, PhysicalAddress);
    DisplayBuff->src.primary_picture.video_buffer_size      = Stream->GetDecodeBufferManager()->ComponentSize(Buffer, PrimaryManifestationComponent);
    DisplayBuff->src.primary_picture.chroma_buffer_offset   = Stream->GetDecodeBufferManager()->Chroma(Buffer, PrimaryManifestationComponent) -
                                                              Stream->GetDecodeBufferManager()->Luma(Buffer, PrimaryManifestationComponent);
    DisplayBuff->src.primary_picture.pitch                  = Stream->GetDecodeBufferManager()->ComponentStride(Buffer, PrimaryManifestationComponent, 0, 0);
    DisplayBuff->src.horizontal_decimation_factor           = STM_NO_DECIMATION;
    DisplayBuff->src.vertical_decimation_factor             = STM_NO_DECIMATION;

    if (Stream->GetDecodeBufferManager()->ComponentPresent(Buffer, DecimatedManifestationComponent))
    {
        DisplayBuff->src.secondary_picture.video_buffer_addr      = (uint32_t)Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, DecimatedManifestationComponent, PhysicalAddress);
        DisplayBuff->src.secondary_picture.video_buffer_size      = Stream->GetDecodeBufferManager()->ComponentSize(Buffer, DecimatedManifestationComponent);
        DisplayBuff->src.secondary_picture.chroma_buffer_offset   = Stream->GetDecodeBufferManager()->Chroma(Buffer, DecimatedManifestationComponent) -
                                                                    Stream->GetDecodeBufferManager()->Luma(Buffer, DecimatedManifestationComponent);
        DisplayBuff->src.secondary_picture.pitch                  = Stream->GetDecodeBufferManager()->ComponentStride(Buffer, DecimatedManifestationComponent, 0, 0);
        // SE API exposes only H4V2, H2V2 or H1V1 (no decim) decimation
        DisplayBuff->src.horizontal_decimation_factor             = (Stream->GetDecodeBufferManager()->DecimationFactor(Buffer, 0) == 2) ? STM_DECIMATION_BY_TWO : STM_DECIMATION_BY_FOUR;
        DisplayBuff->src.vertical_decimation_factor               = STM_DECIMATION_BY_TWO;
    }
}

void Manifestor_VideoStmfb_c::ApplyPixelAspectRatioCorrection(stm_display_buffer_t       *DisplayBuff,
                                                              struct ParsedVideoParameters_s *VideoParameters)
{
    Rational_t                  RestrictedPixelAspectRatio;
    // The Rational class assignment operator tries to restrict the
    // numerator and denominator to 32bit values.
    RestrictedPixelAspectRatio                      = VideoParameters->Content.PixelAspectRatio;
    DisplayBuff->src.pixel_aspect_ratio.numerator   = (long)RestrictedPixelAspectRatio.GetNumerator();
    DisplayBuff->src.pixel_aspect_ratio.denominator = (long)RestrictedPixelAspectRatio.GetDenominator();
    DisplayBuff->src.linear_center_percentage       = 0;
}

ManifestorStatus_t Manifestor_VideoStmfb_c::SetOutputRateAdjustment(unsigned int TimingIndex, struct StreamBuffer_s *StreamBuff)
{
    int Adjustment, Status = 0;
    SE_PRECONDITION(StreamBuff != NULL);

    if (!SurfaceDescriptor[TimingIndex].ClockPullingAvailable) { return ManifestorNoError; }

    ManifestorStatus_t Result = DerivePPMValueFromOutputRateAdjustment(
                                    StreamBuff->OutputTiming[TimingIndex]->OutputRateAdjustment, &Adjustment, TimingIndex);

    if (Result != ManifestorNoError) { return Result; }

    Adjustment -= 1000000;

    if (Adjustment != ClockRateAdjustment[TimingIndex])
    {
        if (TimingIndex == SOURCE_INDEX)
        {
            SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p Setting DisplaySource clock adjustment to %d parts per million\n", Stream, this, Adjustment);
            Status = stm_display_source_set_control(Source, SOURCE_CTRL_CLOCK_ADJUSTMENT, Adjustment);
        }
        else
        {
            SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p Setting DisplayOutput #%d clock adjustment to %d parts per million\n", Stream, this, TimingIndex - 1, Adjustment);
            Status = stm_display_output_set_control(DisplayPlanes[TimingIndex - 1].OutputHandle, OUTPUT_CTRL_CLOCK_ADJUSTMENT, Adjustment);
        }

        if (Status == 0)
        {
            ClockRateAdjustment[TimingIndex] = Adjustment;
        }
        else
        {
            SE_ERROR("Stream 0x%p this 0x%p Cannot set output rate adjustment (%d)\n", Stream, this, Status);
            SE_ERROR("Stream 0x%p this 0x%p Disabling ClockPulling for surface #%d\n", Stream, this, TimingIndex);
            SurfaceDescriptor[TimingIndex].ClockPullingAvailable = false;
        }
    }

    return ManifestorNoError;
}

void Manifestor_VideoStmfb_c::FillInDisplayBufferDstFields(
    stm_display_buffer_t *DisplayBuff)
{
    DisplayBuff->dst.ulFlags                    = 0;
}

void Manifestor_VideoStmfb_c::FillInDisplayBufferSrcFields(
    Buffer_t Buffer,
    struct StreamBuffer_s *StreamBuff,
    struct ParsedVideoParameters_s *VideoParameters,
    stm_display_buffer_t *DisplayBuff,
    struct ManifestationOutputTiming_s **VideoOutputTimingArray)
{
    SelectDisplayBufferPointers(Buffer, StreamBuff, DisplayBuff);

    ApplyPixelAspectRatioCorrection(DisplayBuff, VideoParameters);

    SelectDisplaySource(Buffer, DisplayBuff);

    DisplayBuff->src.src_frame_rate.numerator   = VideoParameters->Content.FrameRate.GetNumerator();
    DisplayBuff->src.src_frame_rate.denominator = VideoParameters->Content.FrameRate.GetDenominator();

    DisplayBuff->src.ulConstAlpha               = 0xff;

    switch (VideoParameters->Content.Output3DVideoProperty.Stream3DFormat)
    {
    case Stream3DFormatFrameSeq :
        DisplayBuff->src.config_3D.formats = FORMAT_3D_FRAME_SEQ;
        DisplayBuff->src.config_3D.parameters.frame_seq = (VideoParameters->Content.Output3DVideoProperty.Frame0IsLeft) ?
                                                          FORMAT_3D_FRAME_SEQ_LEFT_FRAME : FORMAT_3D_FRAME_SEQ_RIGHT_FRAME;
        break;

    case Stream3DFormatStackedHalf :
        DisplayBuff->src.config_3D.formats = FORMAT_3D_STACKED_HALF;
        DisplayBuff->src.config_3D.parameters.frame_packed.is_left_right_format = VideoParameters->Content.Output3DVideoProperty.Frame0IsLeft;
        break;

    case Stream3DFormatSidebysideHalf :
        DisplayBuff->src.config_3D.formats = FORMAT_3D_SBS_HALF;
        DisplayBuff->src.config_3D.parameters.sbs.is_left_right_format = VideoParameters->Content.Output3DVideoProperty.Frame0IsLeft;
        break;

    default :
        DisplayBuff->src.config_3D.formats = FORMAT_3D_NONE;
        break;
    }

    DisplayBuff->src.flags  = STM_BUFFER_SRC_CONST_ALPHA;
    DisplayBuff->src.flags |= SelectColourMatrixCoefficients(VideoParameters);

    if (VideoParameters->InterlacedFrame)
    {
        DisplayBuff->src.flags |= STM_BUFFER_SRC_INTERLACED;
    }

    Rational_t          Speed;
    PlayDirection_t     Direction;
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

    if (Speed < 1)
    {
        DisplayBuff->src.flags |= STM_BUFFER_SRC_INTERPOLATE_FIELDS;
    }

    // Check if the we must repect PICTURE polarity
    if (CheckPicturePolarityRespect(VideoOutputTimingArray, Buffer) == true)
    {
        DisplayBuff->src.flags |= STM_BUFFER_SRC_RESPECT_PICTURE_POLARITY;
    }

#ifndef UNITTESTS
    // DisplayDiscontinuity flag is set to true on

    // Display Discontinuity is progated using Markers in multiple cases:
    // a new stream creation, on a stream switch (channel change use case)
    // on inject discontinuity (seek use case) and drain with discard data.
    // PostManifestEdge->mDiscardingUntilMarkerFrame is set to true on drain (DisplayDiscontinuity is also set to true on drain)
    // we need to wait for the drain to be finish to set discontinuity flag.
    if ((Stream->DisplayDiscontinuity) && (!Stream->PostManifestEdge->isDiscardingUntilMarkerFrame()))
    {
        DisplayBuff->src.flags |= STM_BUFFER_SRC_TEMPORAL_DISCONTINUITY;
        Stream->DisplayDiscontinuity = false;
    }

    // Inform vibe that there is a display discontinuity when we are in I only decode mode.
    // This is a fix for bug 38953, specific to Orly.
    if ((Player->PolicyValue(Playback, Stream, PolicyStreamDiscardFrames) == PolicyValueKeyFramesOnly)
        && (!Stream->PostManifestEdge->isDiscardingUntilMarkerFrame()))
    {
        DisplayBuff->src.flags |= STM_BUFFER_SRC_TEMPORAL_DISCONTINUITY;
        DisplayBuff->src.flags |= STM_BUFFER_SRC_FORCE_DISPLAY;
    }
#endif
}

void Manifestor_VideoStmfb_c::FillInDisplayBufferInfoFields(
    struct StreamBuffer_s *StreamBuff,
    struct ManifestationOutputTiming_s **VideoOutputTimingArray,
    stm_display_buffer_t *DisplayBuff)
{
    DisplayLatencyTarget[StreamBuff->BufferIndex] = GetDisplayLatencyTarget();
    OS_Smp_Mb(); // TODO review all memory barriers in manifestor_stmfb

    DisplayBuff->info.ulFlags                   = STM_BUFFER_PRESENTATION_PERSISTENT;
    DisplayBuff->info.presentation_time          = ValidTime(VideoOutputTimingArray[SOURCE_INDEX]->SystemPlaybackTime) ?
                                                   VideoOutputTimingArray[SOURCE_INDEX]->SystemPlaybackTime - DisplayLatencyTarget[StreamBuff->BufferIndex] : 0;
    DisplayBuff->info.puser_data                = StreamBuff;
    DisplayBuff->info.PTS                       = StreamBuff->NativePlaybackTime;
    DisplayBuff->info.display_callback          = &display_callback;
    DisplayBuff->info.completed_callback        = &done_callback;
}

void Manifestor_VideoStmfb_c::FillInDisplayBuffer(
    Buffer_t Buffer,
    struct StreamBuffer_s *StreamBuff,
    struct ParsedVideoParameters_s *VideoParameters,
    struct ManifestationOutputTiming_s **VideoOutputTimingArray,
    stm_display_buffer_t *DisplayBuff)
{
    // Clear the DisplayBuff record
    memset((void *)DisplayBuff, 0, sizeof(stm_display_buffer_t));

    FillInDisplayBufferSrcFields(Buffer, StreamBuff, VideoParameters, DisplayBuff, VideoOutputTimingArray);
    FillInDisplayBufferDstFields(DisplayBuff);
    FillInDisplayBufferInfoFields(StreamBuff, VideoOutputTimingArray, DisplayBuff);
}

void Manifestor_VideoStmfb_c::ReportQueueBufferFailedEvent(int BufferIndex)
{
    struct PlayerEventRecord_s DisplayEvent;

    DisplayEvent.Code                           = EventFailedToQueueBufferToDisplay;
    DisplayEvent.Playback                       = Playback;
    DisplayEvent.Stream                         = Stream;
    DisplayEvent.PlaybackTime                   = OS_GetTimeInMicroSeconds();

    PlayerStatus_t Status = Stream->SignalEvent(&DisplayEvent);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p this 0x%p Failed to signal event\n", Stream, this);
    }
}


//  QueueBuffer
//  doxynote
/// \brief                      Actually put buffer on display
/// \param BufferIndex          Index into array of stream buffers
/// \param FrameParameters      Frame parameters generated by frame parser
/// \param VideoParameters      Display positioning information generated by frame parser
/// \param VideoOutputTiming    Display timing information generated by output timer
/// \return                     Success or fail
//
ManifestorStatus_t Manifestor_VideoStmfb_c::QueueBuffer(unsigned int                    BufferIndex,
                                                        struct ParsedFrameParameters_s *FrameParameters,
                                                        struct ParsedVideoParameters_s *VideoParameters,
                                                        struct ManifestationOutputTiming_s **VideoOutputTimingArray,
                                                        Buffer_t                        Buffer)
{
    struct StreamBuffer_s      *StreamBuff          = &StreamBuffer[BufferIndex];
    stm_display_buffer_t       *DisplayBuff         = &DisplayBuffer[BufferIndex];
    unsigned int                FieldFlags[2]       = {0, 0};
    ParsedFrameParameters_t    *ParsedFrameParameters;

    Rational_t          Speed;
    PlayDirection_t     Direction;
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

    if (Speed > 1
        && (VideoOutputTimingArray[SOURCE_INDEX]->DisplayCount[0] == 0)
        && (VideoOutputTimingArray[SOURCE_INDEX]->DisplayCount[1] == 0))
    {
        return ManifestorNoError;
    }

    for (unsigned int i = 0; i < NumberOfTimings; i++)
    {
        ManifestorStatus_t Result = SetOutputRateAdjustment(i, StreamBuff);
        if (Result != ManifestorNoError) { return Result; }
    }

    StreamBuff->Manifestor = this;
    FillInDisplayBuffer(Buffer, StreamBuff, VideoParameters, VideoOutputTimingArray, DisplayBuff);

    if (VideoOutputTimingArray[SOURCE_INDEX]->Interlaced)
    {
        if (VideoOutputTimingArray[SOURCE_INDEX]->TopFieldFirst)
        {
            FieldFlags[0]    |= STM_BUFFER_SRC_TOP_FIELD_ONLY;
            FieldFlags[1]    |= STM_BUFFER_SRC_BOTTOM_FIELD_ONLY;
        }
        else
        {
            FieldFlags[0]    |= STM_BUFFER_SRC_BOTTOM_FIELD_ONLY;
            FieldFlags[1]    |= STM_BUFFER_SRC_TOP_FIELD_ONLY;
        }
    }

    if (SE_IS_VERBOSE_ON(group_manifestor_video_stmfb) || SE_IS_VERBOSE_ON(group_frc))
    {
        FRCPatternTrace(FrameParameters->DisplayFrameIndex
                        , VideoOutputTimingArray[SOURCE_INDEX]->DisplayCount[0]
                        , VideoOutputTimingArray[SOURCE_INDEX]->DisplayCount[1]
                        , VideoOutputTimingArray[SOURCE_INDEX]->TopFieldFirst
                       );
    }

    long long Now = OS_GetTimeInMicroSeconds();

    if (DisplayBuff->info.presentation_time && DisplayBuff->info.presentation_time <= Now)
    {
        SE_WARNING("Stream 0x%p this 0x%p Queueing late buffer to Display #%d (DecodeFrameIndex %d DisplayFrameIndex %d PTS 0x%llx presentationTime %lld)\n",
                   Stream, this, BufferIndex, FrameParameters->DecodeFrameIndex, FrameParameters->DisplayFrameIndex,
                   StreamBuff->NativePlaybackTime, DisplayBuff->info.presentation_time);
    }

    unsigned int CommonFlags        = DisplayBuff->src.flags;
    unsigned int LocalQueueCount    = StreamBuff->QueueCount;

    for (int i = 0; i < 2; i++)
    {
        // For speeds <= 1, skipped fields are sent to the display so that
        // it can use them for the DEI
        if ((VideoOutputTimingArray[SOURCE_INDEX]->DisplayCount[i])
            || (Speed <= 1 && VideoOutputTimingArray[SOURCE_INDEX]->Interlaced))
        {
            DisplayBuff->src.flags      = CommonFlags | FieldFlags[i];
            DisplayBuff->info.nfields   = VideoOutputTimingArray[SOURCE_INDEX]->DisplayCount[i];

            ManifestorStatus_t ManifestorStatus = UpdateManifestedState(StreamBuff, QueuedState);
            if (ManifestorStatus != ManifestorNoError) { return ManifestorStatus; }

            // Save the Display Frame Index to get it in the display callback
            StreamBuff->BufferClass->ObtainMetaDataReference(
                Stream->GetPlayer()->MetaDataParsedFrameParametersReferenceType,
                (void **)(&ParsedFrameParameters));
            SE_ASSERT(ParsedFrameParameters != NULL);

            StreamBuff->DisplayFrameIndex = ParsedFrameParameters->DisplayFrameIndex;

            SE_VERBOSE2(group_manifestor_video_stmfb, group_avsync,
                        "Stream 0x%p this 0x%p Queueing buffer #%d to Vibe (DecodeFrameIndex %d DisplayFrameIndex %d flags 0x%x nfields %d PTS 0x%llx presentationTime %lld deltaNow %lld)\n",
                        Stream, this, BufferIndex, FrameParameters->DecodeFrameIndex, FrameParameters->DisplayFrameIndex, DisplayBuff->src.flags, DisplayBuff->info.nfields,
                        StreamBuff->NativePlaybackTime, DisplayBuff->info.presentation_time,
                        DisplayBuff->info.presentation_time ? DisplayBuff->info.presentation_time - Now : 0);

            StreamBuff->QueueCount++;
            StreamBuff->BufferState = BufferStateQueued;
            int Status = stm_display_source_queue_buffer(QueueInterface, DisplayBuff);

            if (Status != 0)
            {
                SE_ERROR("Stream 0x%p this 0x%p Failed to queue buffer %d to display\n", Stream, this, BufferIndex);
                StreamBuff->QueueCount = LocalQueueCount;
                ReportQueueBufferFailedEvent(BufferIndex);
                UpdateManifestedState(StreamBuff, InitialState);
                return ManifestorUnplayable;
            }
        }

        if (DisplayBuff->info.nfields != 0)
        {
            DisplayBuff->info.presentation_time     = 0;
            DisplayBuff->info.display_callback      = NULL;
        }
    }

    return ManifestorNoError;
}

//
//      QueueNullManifestation :
//      Action  : Insert null frame into display sequence
//      Input   :
//      Output  :
//      Results :
//
ManifestorStatus_t      Manifestor_VideoStmfb_c::QueueNullManifestation(void)
{
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p\n", Stream, this);
    return ManifestorNoError;
}

//  FlushDisplayQueue
//  doxynote
/// \brief      Flushes the display queue so buffers not yet manifested are returned
//
ManifestorStatus_t      Manifestor_VideoStmfb_c::FlushDisplayQueue(void)
{
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p\n", Stream, this);
    //
    // Reset the reserved memory in Decode Buffer manager because the following stm_display_source_queue_flush
    // will release the last frame already pushed to display but not yet rendered.
    // This frame should not be considered as reserved as its associated memory can
    // be reallocated immediatly by the decode buffer manager.
    //
    Stream->GetDecodeBufferManager()->ReserveMemory(NULL, 0);

    if (Player->PolicyValue(Playback, Stream, PolicyVideoLastFrameBehaviour) == PolicyValueLeaveLastFrameOnScreen)
    {
        SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p >stm_display_source_queue_flush PolicyValueLeaveLastFrameOnScreen\n", Stream, this);
        if (stm_display_source_queue_flush(QueueInterface, false) < 0)
        {
            SE_ERROR("Stream 0x%p this 0x%p Not able to flush display source queue PolicyValueLeaveLastFrameOnScreen\n", Stream, this);
        }
        SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p <stm_display_source_queue_flush\n", Stream, this);
    }
    else
    {
        SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p >stm_display_source_queue_flush\n", Stream, this);
        if (stm_display_source_queue_flush(QueueInterface, true) < 0)
        {
            SE_ERROR("Stream 0x%p this 0x%p Not able to flush display source queue\n", Stream, this);
        }
        SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p <stm_display_source_queue_flush\n", Stream, this);
    }

    return ManifestorNoError;
}

//  SynchronizeOutput
//
//      SynchronizeOutput:
//      Action  : Force the stmfb driver to synchronize vsync
//      Input   : void
//      Output  :
//      Results :
//

ManifestorStatus_t Manifestor_VideoStmfb_c::SynchronizeOutput(void)
{
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p NumPlanes = %d\n", Stream, this, NumPlanes);

    for (int i = 0; i < NumPlanes; i++)
    {
        stm_display_output_soft_reset(DisplayPlanes[i].OutputHandle);
    }

    return ManifestorNoError;
}

ManifestorStatus_t Manifestor_VideoStmfb_c::Enable(void)
{
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p NumPlanes = %d\n", Stream, this, NumPlanes);

    Visible = true;

    OS_LockMutex(&DisplayPlaneMutex);

    for (int i = 0; i < NumPlanes; i++)
    {
        SE_DEBUG(group_manifestor_video_stmfb, "Enabling plane %p\n", DisplayPlanes[i].PlaneHandle);
        stm_display_plane_show(DisplayPlanes[i].PlaneHandle);
    }

    OS_UnLockMutex(&DisplayPlaneMutex);

    return ManifestorNoError;
}

ManifestorStatus_t Manifestor_VideoStmfb_c::Disable(void)
{
    SE_DEBUG(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p NumPlanes = %d\n", Stream, this, NumPlanes);

    Visible = false;

    OS_LockMutex(&DisplayPlaneMutex);

    for (int i = 0; i < NumPlanes; i++)
    {
        SE_DEBUG(group_manifestor_video_stmfb, "Disabling plane %p\n", DisplayPlanes[i].PlaneHandle);
        stm_display_plane_hide(DisplayPlanes[i].PlaneHandle);
    }

    OS_UnLockMutex(&DisplayPlaneMutex);

    return ManifestorNoError;
}

bool Manifestor_VideoStmfb_c::GetEnable(void)
{
    uint32_t Status;
    SE_EXTRAVERB(group_manifestor_video_stmfb, "\n");
    //TODO revisit for all Planes
    if (stm_display_plane_get_status(DisplayPlanes[0].PlaneHandle, &Status) < 0)
    {
        SE_ERROR("Stream 0x%p this 0x%p Not able to get status on Plane 0x%p\n", Stream, this, DisplayPlanes[0].PlaneHandle);
    }

    if (Status & STM_STATUS_PLANE_VISIBLE)
    {
        return true;
    }
    else
    {
        return false;
    }
}

long long Manifestor_VideoStmfb_c::GetPipelineLatency()
{
    long long DisplayLatency  = GetDisplayLatencyTarget();
    if (DisplayLatency == 0)
    {
        Rational_t   PeriodOutput, OutputFrameRate;
        OutputFrameRate = ConvertDisplayFramerateToRational(DisplayPlanes[PlaneSelectedForAVsync].CurrentMode.mode_params.vertical_refresh_rate);
        PeriodOutput    = 1000000 / OutputFrameRate;
        DisplayLatency  = PeriodOutput.RoundedLongLongIntegerPart() * 3 / 2;
    }
    return DisplayLatency;
}

ManifestorStatus_t Manifestor_VideoStmfb_c::UpdatePlaneLatency(int PlaneIndex)
{
    stm_compound_control_latency_t MinDisplayLatency, MaxDisplayLatency;
    SE_ASSERT(PlaneIndex >= 0 && PlaneIndex < NumPlanes);

    int Result = stm_display_plane_get_compound_control(DisplayPlanes[PlaneIndex].PlaneHandle, PLANE_CTRL_MIN_VIDEO_LATENCY, &MinDisplayLatency);
    if (Result < 0) { return ManifestorError; }

    Result = stm_display_plane_get_compound_control(DisplayPlanes[PlaneIndex].PlaneHandle, PLANE_CTRL_MAX_VIDEO_LATENCY, &MaxDisplayLatency);
    if (Result < 0) { return ManifestorError; }

    OS_LockSpinLockIRQSave(&DisplayCallbackSpinLock);
    DisplayPlanes[PlaneIndex].MinLatency      = MinDisplayLatency;
    DisplayPlanes[PlaneIndex].MaxLatency      = MaxDisplayLatency;
    OS_UnLockSpinLockIRQRestore(&DisplayCallbackSpinLock);

    SE_INFO(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p Output #%d: MinLatency=(NbInputPeriods=%d, NbOutputPeriods=%d) MaxLatency=(NbInputPeriods=%d, NbOutputPeriods=%d)\n",
            Stream, this, PlaneIndex, MinDisplayLatency.nb_input_periods, MinDisplayLatency.nb_output_periods,
            MaxDisplayLatency.nb_input_periods, MaxDisplayLatency.nb_output_periods);

    return ManifestorNoError;
}

void Manifestor_VideoStmfb_c::GetPlaneLatency(int PlaneIndex, long long *MinLatency, long long *MaxLatency)
{
    stm_compound_control_latency_t MinDisplayLatency, MaxDisplayLatency;
    Rational_t   PeriodInput, PeriodOutput, OutputFrameRate;

    // TODO uncomment this assert when report can be called from interrupt context
    //SE_ASSERT(PlaneIndex >= 0 && PlaneIndex < NumPlanes);

    OS_LockSpinLockIRQSave(&DisplayCallbackSpinLock);
    MinDisplayLatency = DisplayPlanes[PlaneIndex].MinLatency;
    MaxDisplayLatency = DisplayPlanes[PlaneIndex].MaxLatency;
    OS_UnLockSpinLockIRQRestore(&DisplayCallbackSpinLock);

    // Calculate the FVDP pipeline latency based on the forumla that is given to us from VIBE
    PeriodInput     = 1000000 / SurfaceDescriptor[SOURCE_INDEX].FrameRate;
    OutputFrameRate = ConvertDisplayFramerateToRational(DisplayPlanes[PlaneIndex].CurrentMode.mode_params.vertical_refresh_rate);
    PeriodOutput    = 1000000 / OutputFrameRate;
    *MinLatency = MinDisplayLatency.nb_input_periods * PeriodInput.RoundedLongLongIntegerPart()
                  +  MinDisplayLatency.nb_output_periods * PeriodOutput.RoundedLongLongIntegerPart();
    *MaxLatency = MaxDisplayLatency.nb_input_periods * PeriodInput.RoundedLongLongIntegerPart()
                  +  MaxDisplayLatency.nb_output_periods * PeriodOutput.RoundedLongLongIntegerPart();
}

int Manifestor_VideoStmfb_c::GetPlaneIndex(unsigned int PlaneId)
{
    for (int i = 0; i < NumPlanes; i++)
    {
        if (DisplayPlanes[i].PlaneId == PlaneId) { return i; }
    }

    return -1;
}

void Manifestor_VideoStmfb_c::SanityCheckVibeLatencies(
    uint16_t    NbOutput,
    stm_display_latency_params_t *DisplayLatency)
{
    SE_ASSERT(NbOutput <= MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION);

    for (int i = 0; i < NbOutput; i++)
    {
        int PlaneIndex = GetPlaneIndex(DisplayLatency[i].plane_id);

        // TODO: use instead assert when it can be called from interrupt
        if (PlaneIndex < 0 || PlaneIndex > NumPlanes)
        {
            // no trace when topology changed (because it is taken into account after by SE in another thread)
            if (TopologyChanged == false)
            {
                SE_ERROR("Stream 0x%p this 0x%p could not find plane index of plane id %d!\n", Stream, this, DisplayLatency[i].plane_id);
            }
            return;
        }

        long long MinLatency, MaxLatency;
        GetPlaneLatency(PlaneIndex, &MinLatency, &MaxLatency);

        if (DisplayLatency[i].output_latency_in_us > MaxLatency)
        {
            SE_VERBOSE(group_avsync, "Stream 0x%p this 0x%p Unusually high latency DisplayLatency[%d].output_latency_in_us = %d (MinLatency = %lld MaxLatency = %lld)\n",
                       Stream, this, i, DisplayLatency[i].output_latency_in_us, MinLatency, MaxLatency);
            DisplayLatency[i].output_latency_in_us = MaxLatency;
        }

        if (DisplayLatency[i].output_latency_in_us < MinLatency)
        {
            SE_VERBOSE(group_avsync, "Stream 0x%p this 0x%p Unusually low latency DisplayLatency[%d].output_latency_in_us = %d (MinLatency = %lld MaxLatency=%lld)\n",
                       Stream, this, i, DisplayLatency[i].output_latency_in_us, MinLatency, MaxLatency);
            DisplayLatency[i].output_latency_in_us = MinLatency;
        }
    }
}

void Manifestor_VideoStmfb_c::DisplayCallback(struct StreamBuffer_s  *Buffer,
                                              stm_time64_t            VsyncTime,
                                              uint16_t                OutputChangeFlags,
                                              uint16_t                NbOutput,
                                              stm_display_latency_params_t *DisplayLatency)
{
    //If the manifestor has been halted and/or reset then various state has been destroyed
    if (! TestComponentState(ComponentRunning) || (IsHalting == true))
    {
        return;
    }

    OS_LockSpinLockIRQSave(&DisplayCallbackSpinLock);
    TopologyChanged     = (OutputChangeFlags & OUTPUT_CONNECTION_CHANGE) ? true : false;
    DisplayModeChanged  = (OutputChangeFlags & OUTPUT_DISPLAY_MODE_CHANGE) ? true : false;
    OS_UnLockSpinLockIRQRestore(&DisplayCallbackSpinLock);

    Buffer->OutputTiming[SOURCE_INDEX]->ActualSystemPlaybackTime = VsyncTime + DisplayLatencyTarget[Buffer->BufferIndex];

    SanityCheckVibeLatencies(NbOutput, DisplayLatency);

    for (int i = 0; i < (int) NbOutput; i++)
    {
        int PlaneIndex = GetPlaneIndex(DisplayLatency[i].plane_id);

        if (PlaneIndex < 0 || PlaneIndex + 1 >= (int) Buffer->NumberOfTimings) { continue; }

        Buffer->OutputTiming[PlaneIndex + 1]->ActualSystemPlaybackTime = VsyncTime + DisplayLatency[i].output_latency_in_us + HW_DELAY;
    }

    Buffer->NbOutput             = NbOutput;
    BufferOnDisplay              = Buffer->BufferIndex;
    PtsOnDisplay                 = Buffer->NativePlaybackTime;
    FrameCountManifested++;

    SE_VERBOSE2(group_manifestor_video_stmfb, group_avsync, "Stream 0x%p this 0x%p idx=%d PTS=%llu FrameCountManifested=%llu\n",
                Stream, this, BufferOnDisplay, PtsOnDisplay, FrameCountManifested);

    // update playback info statistics
    Stream->Statistics().FrameCountManifested++;
    Stream->Statistics().Pts              = Buffer->NativePlaybackTime;
    Stream->Statistics().PresentationTime = VsyncTime + DisplayLatency[0].output_latency_in_us + HW_DELAY;
    Stream->Statistics().SystemTime       = OS_GetTimeInMicroSeconds();

    if (ValidTime(Buffer->OutputTiming[PlaneSelectedForAVsync + 1]->SystemPlaybackTime))
    {
        Stream->Statistics().SyncError        = Buffer->OutputTiming[PlaneSelectedForAVsync + 1]->SystemPlaybackTime
                                                - Buffer->OutputTiming[PlaneSelectedForAVsync + 1]->ActualSystemPlaybackTime;
    }

    // Frame is coming back from display, mark it as rendered.
    // No check on function returned value as it cannot fail. Function just updates Buffer->SequenceNumberStructure->ManifestedState field.
    UpdateManifestedState(Buffer, RenderedState);

    Stream->GetDecodeBufferManager()->ReserveMemory((void *)(DisplayBuffer[Buffer->BufferIndex].src.primary_picture.video_buffer_addr),
                                                    DisplayBuffer[Buffer->BufferIndex].src.primary_picture.video_buffer_size);
    OS_SemaphoreSignal(&BufferDisplayed);
}

static void display_callback(void           *Buffer,
                             stm_time64_t    VsyncTime,
                             uint16_t        output_change_flags,
                             uint16_t        nb_planes,
                             stm_display_latency_params_t *display_latency_params)
{
    struct StreamBuffer_s              *StreamBuffer    = (struct StreamBuffer_s *)Buffer;
    SE_ASSERT((StreamBuffer != NULL) && (display_latency_params != NULL));
    class Manifestor_VideoStmfb_c      *Manifestor      = (Manifestor_VideoStmfb_c *)StreamBuffer->Manifestor;

    if (StreamBuffer->BufferState != BufferStateQueued)
    {
        SE_ERROR("this 0x%p Wrong BufferState %d for Buffer #%d\n", Manifestor, StreamBuffer->BufferState, StreamBuffer->BufferIndex);
        return;
    }

    SE_ASSERT(Manifestor != NULL);
    Manifestor->DisplayCallback(StreamBuffer, VsyncTime, output_change_flags, nb_planes, display_latency_params);
}

void Manifestor_VideoStmfb_c::DoneCallback(struct StreamBuffer_s  *Buffer,
                                           stm_time64_t            VsyncTime,
                                           unsigned int            Status)
{
    // If the manifestor has been halted and/or reset then various state has been destroyed
    if (! TestComponentState(ComponentRunning) || (IsHalting == true))
    {
        return;
    }

    if ((Status & STM_STATUS_BUF_HW_ERROR) != 0)
    {
        SE_ERROR("Stream 0x%p this 0x%p FatalHardwareError\n", Stream, this);
    }

    Buffer->QueueCount--;

    if (Buffer->QueueCount == 0)
    {
        Buffer->BufferState   = BufferStateAvailable;

        SE_VERBOSE(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p Release Buffer #%d\n", Stream, this, Buffer->BufferIndex);
        mOutputPort->InsertFromIRQ((uintptr_t) Buffer->BufferClass);
    }
}


static void done_callback(void   *Buffer, const stm_buffer_presentation_stats_t *Data)
{
    struct StreamBuffer_s              *StreamBuffer    = (struct StreamBuffer_s *)Buffer;
    SE_ASSERT(StreamBuffer != NULL);
    class Manifestor_VideoStmfb_c      *Manifestor      = (Manifestor_VideoStmfb_c *)StreamBuffer->Manifestor;

    SE_VERBOSE(group_manifestor_video_stmfb, "this 0x%p #%d\n", Manifestor, StreamBuffer->BufferIndex);

    if (StreamBuffer->BufferState != BufferStateQueued)
    {
        SE_ERROR("this 0x%p Wrong BufferState %d for Buffer #%d\n", Manifestor, StreamBuffer->BufferState, StreamBuffer->BufferIndex);
        return;
    }

    SE_ASSERT(Manifestor != NULL);
    Manifestor->DoneCallback(StreamBuffer, Data ? Data->vsyncTime : 0, Data ? (unsigned int)Data->status : 0);
}

ManifestorStatus_t Manifestor_VideoStmfb_c::GetNumberOfTimings(unsigned int *NumTimes)
{
    SE_VERBOSE(group_manifestor_video_stmfb, "Stream 0x%p this 0x%p NumTimes = %d\n", Stream, this, NumberOfTimings);
    *NumTimes = NumberOfTimings;
    return ManifestorNoError;
}

ManifestorStatus_t Manifestor_VideoStmfb_c::UpdateManifestedState(struct StreamBuffer_s *StreamBuff, ManifestedState_t ManifestedState)
{
    class Buffer_c          *ManifestedBuffer;
    Buffer_t                 OriginalCodedFrameBuffer;
    PlayerSequenceNumber_t  *SequenceNumberStructure;

    // Treatment on QueuedState is different from the other ones because meta data are read and
    // SequenceNumberStructure is attach to the buffer. For the other states, we only need
    // to update SequenceNumberStructure, no longer read the meta data.
    if (ManifestedState == QueuedState)
    {
        ManifestedBuffer = StreamBuff->BufferClass;
        ManifestedBuffer->ObtainAttachedBufferReference(Stream->GetCodedFrameBufferType(), &OriginalCodedFrameBuffer);
        SE_ASSERT(OriginalCodedFrameBuffer != NULL);

        OriginalCodedFrameBuffer->ObtainMetaDataReference(Stream->GetPlayer()->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
        SE_ASSERT(SequenceNumberStructure != NULL);

        // Frame is queued to display
        // WA for bug 36367 consists in attaching SequenceNumberStructure to StreamBuff; this way in DisplayCallback,
        // SequenceNumberStructure of the buffer is updated to ManifestedState and there is no use to obtain the
        // meta data again. ObtainAttachedBufferReference() needs to take buffer's lock mutex, and DisplayCallback
        // is called under VIBE IT: this was causing the hang as buffer's lock mutex could be already taken in another
        // place in SE.
        StreamBuff->SequenceNumberStructure      = SequenceNumberStructure;
        SequenceNumberStructure->ManifestedState = QueuedState;
    }
    else
    {
        StreamBuff->SequenceNumberStructure->ManifestedState = ManifestedState;
    }

    return ManifestorNoError;
}

void Manifestor_VideoStmfb_c::FRCPatternTrace(unsigned int DisplayFrameIndex
                                              , unsigned int DisplayCount_0
                                              , unsigned int DisplayCount_1
                                              , bool TopFieldFirst)
{
    char first[16], second[16];
    int i;

    for (i = 0; i < min(16, DisplayCount_0); i++)
    {
        if (TopFieldFirst)
        {
            first[i] = 'T';
        }
        else
        {
            first[i] = 'B';
        }
    }
    first[i] = '\0';

    for (i = 0; i < min(16, DisplayCount_1); i++)
    {
        if (TopFieldFirst)
        {
            second[i] = 'B';
        }
        else
        {
            second[i] = 'T';
        }
    }
    second[i] = '\0';

    // SE-PIPELINE trace
    SE_VERBOSE2(group_manifestor_video_stmfb, group_frc, "Frame %d  Top first : %d DisplayCounts:%d,%d FRC pattern : %s%s\n"
                , DisplayFrameIndex
                , TopFieldFirst
                , DisplayCount_0
                , DisplayCount_1
                , first
                , second
               );
}
