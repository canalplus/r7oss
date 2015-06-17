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
#ifndef PREPROC_VIDEO_H
#define PREPROC_VIDEO_H

#include "preproc_base.h"

#define PREPROC_WIDTH_MIN      32
#define PREPROC_WIDTH_MAX      1920
#define PREPROC_HEIGHT_MIN     32
#define PREPROC_HEIGHT_MAX     1088
#define ENCODE_WIDTH_MIN       120  // fvdp-enc limit
#define ENCODE_WIDTH_MAX       1920
#define ENCODE_HEIGHT_MIN      36   // fvdp-enc limit
#define ENCODE_HEIGHT_MAX      1088
#define ENCODE_FRAMERATE_MIN   (1*STM_SE_PLAY_FRAME_RATE_MULTIPLIER)
#define ENCODE_FRAMERATE_MAX   (60*STM_SE_PLAY_FRAME_RATE_MULTIPLIER)

typedef struct PreprocVideoCtrl_s
{
    stm_se_picture_resolution_t Resolution;
    bool                        ResolutionSet; // To detect if resolution has been set by control. If not, set to input buffer resolution
    uint32_t                    EnableDeInterlacer;
    uint32_t                    EnableNoiseFilter;
    stm_se_framerate_t          Framerate;
    bool                        FramerateSet; // To detect if framerate has been set by control. If not, set to input frame rate
    uint32_t                    DisplayAspectRatio;
    uint32_t                    EnableCropping; // Enable use of window_of_interest metadata for cropping before encode
} PreprocVideoCtrl_t;

// This structure is used for per input buffer
typedef struct FRCCtrl_s
{
    bool        DiscardInput;       // If input field is discarded
    int32_t     FrameRepeat;        // If output frame corresponding to input field is repeated
    bool        DiscardOutput;      // If output frame is discarded
    uint64_t    NativeTimeOutput;   // PTS for the first frame output in stm_se_time_format_t
    uint64_t    EncodedTimeOutput;  // Remap PTS (used in NRT mode) for the first frame output, in same format as NativeTimeOutput
    uint32_t    TimePerOutputFrame; // Delta PTS for the subsequent frame output in stm_se_time_format_t
    bool        SecondField;        // Time instance of the input field whether first or second
} FRCCtrl_t;

typedef struct PreprocVideoCaps_s
{
    stm_se_picture_resolution_t MaxResolution;
    stm_se_framerate_t          MaxFramerate;
    bool                        DEISupport;
    bool                        TNRSupport;
    uint32_t                    VideoFormatCaps;
} PreprocVideoCaps_t;

typedef struct Ratio_s
{
    uint32_t num;
    uint32_t den;
} Ratio_t;

typedef struct ARCCtrl_s
{
    stm_se_picture_resolution_t     SrcSize;                 // input picture size
    stm_se_picture_rectangle_t      SrcRect;                 // input window of interest
    stm_se_aspect_ratio_t           SrcDisplayAspectRatio;   // input display aspect ratio
    Ratio_t                         SrcPixelAspectRatio;     // input pixel aspect ratio
    stm_se_picture_resolution_t     DstSize;                 // encoded picture size
    stm_se_picture_rectangle_t      DstRect;                 // encoded window of interest
    stm_se_aspect_ratio_t           DstDisplayAspectRatio;   // encoded aspect ratio
    Ratio_t                         DstPixelAspectRatio;     // input pixel aspect ratio
} ARCCtrl_t;

class Preproc_Video_c: public Preproc_Base_c
{
public:
    Preproc_Video_c(void);
    ~Preproc_Video_c(void);

    PreprocStatus_t   Halt(void);

    PreprocStatus_t   ManageMemoryProfile(void);

    PreprocStatus_t   Input(Buffer_t     Buffer);

    PreprocStatus_t   GetControl(stm_se_ctrl_t   Control,
                                 void           *Data);

    PreprocStatus_t   SetControl(stm_se_ctrl_t   Control,
                                 const void     *Data);

    PreprocStatus_t   GetCompoundControl(stm_se_ctrl_t   Control,
                                         void           *Data);

    PreprocStatus_t   SetCompoundControl(stm_se_ctrl_t   Control,
                                         const void     *Data);

    PreprocStatus_t   RegisterPreprocVideo(void *PreprocHandle);
    void              DeRegisterPreprocVideo(void *PreprocHandle);

protected:
    PreprocVideoCtrl_t PreprocCtrl;        // PreprocCtrl changes with GetControl and SetControl

    static const struct stm_se_picture_resolution_s ProfileSize[];

    // FRC variables
    uint32_t           TimePerInputField;
    uint32_t           TimePerOutputFrame;
    uint64_t           InputTimeLine;
    uint64_t           OutputTimeLine;
    stm_se_framerate_t InputFramerate;
    stm_se_framerate_t OutputFramerate;
    stm_se_scan_type_t InputScanType;

    void                    InitControl(void);
    void                    InitFRC(void);
    PreprocStatus_t         UpdateInputFrameRate(void);
    PreprocStatus_t         UpdateOutputFrameRate(void);
    PreprocStatus_t         CheckBufferAlignment(Buffer_t   Buffer);
    PreprocStatus_t         CheckFrameRateSupport(void);
    PreprocStatus_t         CheckAspectRatioSupport(void);
    PreprocStatus_t         ConvertFrameRate(FRCCtrl_t  *FRCCtrl);
    Ratio_t                 GetDisplayAspectRatio(stm_se_aspect_ratio_t AspectRatio);
    stm_se_aspect_ratio_t   MapAspectRatio(uint32_t CtrlAspectRatio);
    void                    SimplifyRatio(Ratio_t *Ratio);
    bool                    TestMultOverflow(uint32_t int1, uint32_t int2, uint32_t int3, uint32_t *mul);
    PreprocStatus_t         ConvertAspectRatio(ARCCtrl_t    *ARCCtrl);
    void                    EvaluatePreprocFrameSize(uint32_t *PreProcFrameSize);
    void                    EvaluatePreprocBufferNeeds(uint32_t *PreProcFrameSize, uint32_t *PreProcBufferNumber);
    PreprocStatus_t         CheckBufferColorFormat(void);
    bool                    IsRequestedResolutionCompatibleWithProfile(uint32_t Width, uint32_t Height);
    PreprocStatus_t         CheckMedia(stm_se_encode_stream_media_t Media);
    PreprocStatus_t         CheckResolution(uint32_t Width, uint32_t Height);
    PreprocStatus_t         CheckAspectRatio(stm_se_aspect_ratio_t AspectRatio, uint32_t PixelAspectRatioNum, uint32_t PixelAspectRatioDen, uint32_t Width, uint32_t Height);
    PreprocStatus_t         CheckScanType(stm_se_scan_type_t ScanType);
    PreprocStatus_t         CheckColorspace(stm_se_colorspace_t Colorspace);
    PreprocStatus_t         CheckSurfaceFormat(surface_format_t SurfaceFormat, uint32_t Width, uint32_t Height, uint32_t InputBufferSize);
    int32_t                 GetBitPerPixel(surface_format_t SurfaceFormat);
    int32_t                 GetMinPitch(uint32_t Width, surface_format_t SurfaceFormat);
    PreprocStatus_t         CheckPitch(uint32_t Pitch, uint32_t Width, surface_format_t SurfaceFormat);
    PreprocStatus_t         CheckVerticalAlignment(uint32_t VerticalAlignment, uint32_t Height, surface_format_t SurfaceFormat, uint32_t BufferSize);
    PreprocStatus_t         CheckPictureType(stm_se_picture_type_t PictureType);
    PreprocStatus_t         CheckFrameRate(stm_se_framerate_t Framerate);
    void                    EntryTrace(stm_se_uncompressed_frame_metadata_t *metadata, PreprocVideoCtrl_t *control);
    void                    ExitTrace(stm_se_uncompressed_frame_metadata_t *metadata, PreprocVideoCtrl_t *control);

private:
    bool            FirstFrame;
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Preproc_Video_c
\brief Base class implementation for the Video Preprocessor classes.

We need to consider that our input rate may not match our output rate.<br>
When performing frame-rate conversion, we may scale up as well as down.<br>

<pre>
For example:
  Input: PAL (25 FPS) -> Output: NTSC (30 FPS)
  Input: NTSC (30 FPS) -> Output: PAL (25 FPS)
</pre>

When managing frame rate conversions we must also ensure that we correctly re-time frames to the output frame-rate.

We can use the buffer manager to intelligently duplicate and repeat frames.

<pre>
  Original Frame:
    [BufA] <- Has Memory and Metadata

  Repeated Frame:
    [BufA] <- Has Memory
      ^^
    [BufB] <- Has Metadata.
</pre>

The Preprocessor may also have its own thread to manage timings and the return
of buffers from external sources such as the blitter or scaler interfaces

If de-interlacing is required, it will be performed in these classes.

*/

#endif /* PREPROC_VIDEO_H */
