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

#ifndef H_MANIFESTOR_VIDEO
#define H_MANIFESTOR_VIDEO

#include "osinline.h"
#include <stm_display.h>

#include "allocinline.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "manifestor_base.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_Video_c"

#define PAIRED_MACROBLOCK(v)            (((v) + 0x1f) & 0xffffffe0)
#define INPUT_WINDOW_SCALE_FACTOR       16

#define MAX_SCALING_FACTOR              2

#define SOURCE_INDEX 0

// This value must be less than the period at which the kernel will think
// the task has hung. This is a constant of 120 seconds
// This is configured by the Kernel Option CONFIG_DETECT_HUNG_TASK
#define MAX_THREAD_BLOCK_MS             (60 * 1000)

typedef enum
{
    BufferStateAvailable,
    BufferStateQueued,
    BufferStateNotQueued,
} BufferState_t;

struct StreamBuffer_s
{
    unsigned int                BufferIndex;
    unsigned int                QueueCount;

    BufferState_t               BufferState;

    class Manifestor_Video_c   *Manifestor;
    class Buffer_c             *BufferClass;
    struct ManifestationOutputTiming_s *OutputTiming[MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION];
    unsigned int                NumberOfTimings;

    unsigned int                DisplayFrameIndex;

    unsigned long long          NativePlaybackTime;
    PlayerTimeFormat_t          NativeTimeFormat;
    unsigned int                NbOutput;

    stm_buffer_presentation_stats_t Data;

    PlayerSequenceNumber_t     *SequenceNumberStructure;
};

/* Generic window structure - in pixels */
struct Window_s
{
    unsigned int     X;         /* X coordinate of top left corner */
    unsigned int     Y;         /* Y coordinate of top left corner */
    unsigned int     Width;     /* Width of window in pixels */
    unsigned int     Height;    /* Height of window in pixels */
};

/// Framework for implementing video manifestors.
class Manifestor_Video_c : public Manifestor_Base_c
{
public:
    /* relayfs to differentiate the manifestor instance */
    unsigned int  RelayfsIndex;

    /* Constructor / Destructor */
    Manifestor_Video_c(void);
    ~Manifestor_Video_c(void);

    /* Overrides for component base class functions */
    ManifestorStatus_t   Halt(void);
    ManifestorStatus_t   Reset(void);

    /* Manifestor class functions */
    ManifestorStatus_t   Connect(Port_c *Port);
    ManifestorStatus_t   GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces);
    ManifestorStatus_t   GetNextQueuedManifestationTime(unsigned long long    *Time, unsigned int *NumTimes);
    ManifestorStatus_t   ReleaseQueuedDecodeBuffers(void);
    ManifestorStatus_t   QueueDecodeBuffer(class Buffer_c        *Buffer,  ManifestationOutputTiming_t  **TimingPointerArray, unsigned int *NumTimes);
    ManifestorStatus_t   GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long     *Pts);

    void                 ResetOnStreamSwitch() { mFirstFrameOnDisplay = true; }

    unsigned int         GetBufferId(void);

    /* these virtual functions are implemented by the device specific part of the video manifestor */
    virtual ManifestorStatus_t  OpenOutputSurface(void *DisplayDevice) = 0;
    virtual ManifestorStatus_t  CloseOutputSurface(void) = 0;
    virtual ManifestorStatus_t  Enable(void) = 0;
    virtual ManifestorStatus_t  Disable(void) = 0;
    virtual bool                GetEnable(void) = 0;
    virtual long long           GetPipelineLatency();

    void                DisplaySignalThread(void);
    void                CheckFirstFrameOnDisplay(void);

    ManifestorStatus_t  CalcSWCRC(BufferFormat_t BufferFormat, unsigned int Width, unsigned int Height,
                                  PictureStructure_t PictureStructure, unsigned char *LumaAddress,
                                  unsigned char *CbAddress, unsigned char *CrAddress ,
                                  unsigned int *LumaCRC, unsigned  int *ChromaCRC);

    unsigned int        IsSoftwareCRCEnabled(void) {return EnableSoftwareCRC;}

protected:
    bool                                Visible;
    bool                                DisplayUpdatePending;

    /* Display Information */
    OutputSurfaceDescriptor_t           SurfaceDescriptor[MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION];
    struct VideoDisplayParameters_s     StreamDisplayParameters;

    /* Buffer information */
    unsigned int                        MaxBufferCount;
    struct StreamBuffer_s               StreamBuffer[MAX_DECODE_BUFFERS];
    unsigned int                        BufferOnDisplay;
    unsigned long long                  PtsOnDisplay;
    unsigned int                        LastQueuedBuffer;
    OS_Mutex_t                          BufferLock;
    unsigned int                        NumberOfTimings;

    /* Data shared with display signal process */
    OS_Event_t                          DisplaySignalThreadTerminated;
    bool                                DisplaySignalThreadRunning;
    OS_Semaphore_t                      BufferDisplayed;
    bool                                mFirstFrameOnDisplay;

    bool                                IsHalting;

    virtual ManifestorStatus_t  QueueBuffer(unsigned int                    BufferIndex,
                                            struct ParsedFrameParameters_s *FrameParameters,
                                            struct ParsedVideoParameters_s *VideoParameters,
                                            struct ManifestationOutputTiming_s **VideoOutputTimingArray,
                                            Buffer_t                        Buffer) = 0;

    virtual ManifestorStatus_t  UpdateOutputSurfaceDescriptor(void) = 0;

private:
    unsigned long long       mNextQueuedManifestationTime;

    /*SW CRC section*/
    static unsigned int volatile        EnableSoftwareCRC;  /* global class member to be RegisterTuneable (debugfs) */
    unsigned int                        CRC32Table[256];        /* CRC coefficients table */

    ManifestorStatus_t  _QueueDecodeBuffer(class Buffer_c                         *Buffer,
                                           ManifestationOutputTiming_t  **TimingArray, unsigned int *NumTimesg);
    ManifestorStatus_t  ObtainBufferMetaData(class Buffer_c                         *Buffer,
                                             struct ParsedFrameParameters_s        **FrameParameters,
                                             struct ParsedVideoParameters_s        **VideoParameters);
    void                InitCRC32Table(void);

    void UpdateNextQueuedManifestationTime(ManifestationOutputTiming_t  **VideoOutputTimingArray);

    DISALLOW_COPY_AND_ASSIGN(Manifestor_Video_c);
};

#endif
