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

Source file name : manifestor_video_stmfb.cpp
Author :           Julian

Implementation of the stm frame buffer manifestor


Date        Modification                                    Name
----        ------------                                    --------
19-Feb-07   Created                                         Julian

************************************************************************/

#include "osinline.h"
#include "osdev_user.h"
#include "monitor_inline.h"
#include "manifestor_video_stmfb.h"

extern "C" {
  extern int sprintf(char * buf, const char * fmt, ...);

  /// A hack until Nick gives us a nice interface, that we can request a decoded buffer
  /// from the buffer pool, sorry Julian, I have tried to make it as tidy looking as possible.
  extern volatile stm_display_buffer_t *ManifestorLastDisplayedBuffer;
  typedef struct __wait_queue_head wait_queue_head_t;
#define TASK_INTERRUPTIBLE      1
extern void __wake_up(wait_queue_head_t *q, unsigned int mode,
                      int nr_exclusive, void *key);
#define wake_up_interruptible(x)        __wake_up(x, TASK_INTERRUPTIBLE, 1, NULL)
  extern wait_queue_head_t g_ManifestorLastWaitQueue;
};

#include "st_relay.h"

#define PIXEL_DEPTH                     16

struct QueueRecord_s
{
    unsigned int        Flags;
    unsigned int        Count;
    int                 PSIndex;
};

// ///////////////////////////////////////////////////////////////////////////////////////
//    Static constant data
//
static void initial_frame_display_callback     (void*           Buffer,
                                                TIME64          VsyncTime);
static void display_callback                   (void*           Buffer,
                                                TIME64          VsyncTime);
static void done_callback                      (void*           Buffer,
                                const stm_buffer_presentation_stats_t *Data);

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Constructor :
//      Action  : Initialise state
//      Input   :
//      Output  :
//      Result  :
//

Manifestor_VideoStmfb_c::Manifestor_VideoStmfb_c  (void)
{
    //MANIFESTOR_DEBUG ("\n");
    if (InitializationStatus != ManifestorNoError)
    {
        MANIFESTOR_ERROR ("Initialization status not valid - aborting init\n");
        return;
    }

    DisplayDevice               = NULL;
    Plane                       = NULL;
    Output                      = NULL;
    Visible                     = false;
    ClockRateAdjustment         = 0;

    DisplayAddress              = 0;
    DisplaySize                 = 0;

#if defined (QUEUE_BUFFER_CAN_FAIL)
    DisplayAvailableValid       = false;
    DisplayHeadroom             = 0;
    DisplayFlush                = false;

    if (OS_SemaphoreInitialize (&DisplayAvailable, 0) != OS_NO_ERROR)
    {
        MANIFESTOR_ERROR ("Failed to initialize DisplayAvailable semaphore\n");
        InitializationStatus    = ManifestorError;
        return;
    }
    else
        DisplayAvailableValid   = true;
    WaitingForHeadroom          = false;
#endif

    Reset ();

}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor :
//      Action  : Give up switch off the lights and go home
//      Input   :
//      Output  :
//      Result  :
//

Manifestor_VideoStmfb_c::~Manifestor_VideoStmfb_c   (void)
{
    //MANIFESTOR_DEBUG ("\n");

#if defined (QUEUE_BUFFER_CAN_FAIL)
    if (DisplayAvailableValid)
    {
        OS_SemaphoreSignal    (&DisplayAvailable);

        DisplayAvailableValid   = false;
        while (WaitingForHeadroom)
            OS_SleepMilliSeconds (2);

        OS_SemaphoreTerminate (&DisplayAvailable);
    }
#endif

    CloseOutputSurface ();

    // Again Julian, a hack...
    ManifestorLastDisplayedBuffer = NULL;
    wake_up_interruptible (&g_ManifestorLastWaitQueue);
}
//}}}
//{{{  Halt
//{{{  doxynote
/// \brief              Shutdown, stop presenting and retrieving frames
///                     don't return until complete
//}}}
ManifestorStatus_t      Manifestor_VideoStmfb_c::Halt (void)
{
    MANIFESTOR_DEBUG ("\n");

    if (Player->PolicyValue (Playback, Stream, PolicyVideoBlankOnShutdown) == PolicyValueLeaveLastFrameOnScreen)
        stm_display_plane_pause (Plane, 1);
    else
    {
        stm_display_plane_flush (Plane);
        PtsOnDisplay    = INVALID_TIME;
    }

    // Again Julian, a hack...
    ManifestorLastDisplayedBuffer = NULL;
    wake_up_interruptible (&g_ManifestorLastWaitQueue);

    return Manifestor_Video_c::Halt ();
}
//}}}
//{{{  Reset
/// \brief              Reset all state to intial values
ManifestorStatus_t Manifestor_VideoStmfb_c::Reset (void)
{
    unsigned int                i;

    //MANIFESTOR_DEBUG ("Stmfb\n");
    if (TestComponentState (ComponentRunning))
        Halt ();

    for (i = 0; i < MAXIMUM_NUMBER_OF_DECODE_BUFFERS; i++)
    {
        DisplayBuffer[i].info.pDisplayCallback          = NULL;
        DisplayBuffer[i].info.pCompletedCallback        = NULL;
    }

    // Again Julian, a hack...
    ManifestorLastDisplayedBuffer = NULL;
    wake_up_interruptible (&g_ManifestorLastWaitQueue);

    return Manifestor_Video_c::Reset ();
}
//}}}
//{{{  UpdateOutputSurfaceDescriptor
//{{{  doxynote
/// \brief      Find out information about display plane and output
/// \return     ManifestorNoError if done sucessfully
//}}}
ManifestorStatus_t Manifestor_VideoStmfb_c::UpdateOutputSurfaceDescriptor    ( void )
{
    const stm_mode_line_t*                      CurrentMode;

    // Fill in surface descriptor details
    // The Frame rate from the driver is an approximate value multiplied by 1000.  This means that standard
    // NTSC rates (60/1.001) give a value of 59940.  The bodge below reconverts this to the more accurate
    // value of 60000/1001.

    CurrentMode = stm_display_output_get_current_display_mode (Output);
    if (CurrentMode != NULL)
    {
        SurfaceDescriptor.DisplayWidth      = CurrentMode->ModeParams.ActiveAreaWidth;

        // The active area for 480p is actually 483 lines, but by default
        // we don't want to scale up 480 content to that odd number.
        if (CurrentMode->ModeParams.ActiveAreaHeight == 483)
          SurfaceDescriptor.DisplayHeight       = 480;
        else
          SurfaceDescriptor.DisplayHeight       = CurrentMode->ModeParams.ActiveAreaHeight;

        if (CurrentMode->ModeParams.FrameRate == 59940)
            SurfaceDescriptor.FrameRate         = Rational_t(60000, 1001);
        else
            SurfaceDescriptor.FrameRate         = Rational_t(CurrentMode->ModeParams.FrameRate, 1000);
        SurfaceDescriptor.Progressive           = (CurrentMode->ModeParams.ScanType == SCAN_P);
    }
    else
    {
        MANIFESTOR_ERROR("Failed to get display mode\n");
        return ManifestorError;
    }

        return ManifestorNoError;
}
//}}}
//{{{  OpenOutputSurface
//{{{  doxynote
/// \brief      Find out information about display plane and output and use it to
///             open handles to the desired display plane
/// \param      Device Handle of the display device.
/// \param      PlaneId Plane identifier (intepreted in the same way as stgfb)
/// \return     ManifestorNoError if plane opened successfully
//}}}
ManifestorStatus_t Manifestor_VideoStmfb_c::OpenOutputSurface    (DeviceHandle_t          Device,
                                                                  unsigned int            PlaneId,
                                                                  unsigned int            OutputId)
{
    ManifestorStatus_t    Status;

    MANIFESTOR_DEBUG ("\n");

    if (DisplayDevice == NULL)
        DisplayDevice       = (stm_display_device_t*)Device;
    if (DisplayDevice == NULL)
    {
        MANIFESTOR_ERROR("Invalid display device\n");
        return ManifestorError;
    }
    MANIFESTOR_DEBUG("DisplayDevice %p, PlaneId %x\n", DisplayDevice, PlaneId);
    if (Plane == NULL)
        Plane           = stm_display_get_plane (DisplayDevice, PlaneId);
    if (Plane == NULL)
    {
        MANIFESTOR_ERROR("Failed to get plane id %x\n", PlaneId);
        return ManifestorError;
    }

    if (Output == NULL)
        Output          = stm_display_get_output (DisplayDevice, OutputId);
    if (Output == NULL)
    {
        stm_display_plane_release (Plane);
        Plane           = NULL;
        MANIFESTOR_ERROR("Failed to get output\n");
        return ManifestorError;
    }

    stm_display_plane_connect_to_output (Plane, Output);

//
// Nicks replacement code to update the surface descriptor.
//

    Status = UpdateOutputSurfaceDescriptor();
    if( Status != ManifestorNoError )
        return Status;

    MANIFESTOR_DEBUG("Width %d, height %d, FrameRate %d.%06d, Progressive %d\n",
            SurfaceDescriptor.DisplayWidth, SurfaceDescriptor.DisplayHeight,
            SurfaceDescriptor.FrameRate.IntegerPart(), SurfaceDescriptor.FrameRate.RemainderDecimal(),
            SurfaceDescriptor.Progressive);

//

    if (stm_display_plane_lock (Plane) < 0)
    {
        MANIFESTOR_ERROR("Failed to lock plane\n");
        return ManifestorError;
    }

    Visible             = false;

    return ManifestorNoError;
}
//}}}
//{{{  CloseOutputSurface
//{{{  doxynote
/// \brief              Release all frame buffer resources
//}}}
ManifestorStatus_t Manifestor_VideoStmfb_c::CloseOutputSurface (void)
{
    MANIFESTOR_DEBUG ("\n");
    if ((Output != NULL) && (Plane != NULL))
        stm_display_plane_disconnect_from_output (Plane, Output);
    if (Plane != NULL)
        stm_display_plane_release (Plane);
    if (Output != NULL)
        stm_display_output_release (Output);

    DisplayDevice       = NULL;
    Plane               = NULL;
    Output              = NULL;

    // Again Julian, a hack...
    ManifestorLastDisplayedBuffer = NULL;
    wake_up_interruptible (&g_ManifestorLastWaitQueue);

    return ManifestorNoError;
}
//}}}

//{{{  Some shared functions used for queuing buffers
static void SelectDisplaySource (struct BufferStructure_s*   BufferStructure,
                                 stm_display_buffer_t*       DisplayBuff)
{

    switch (BufferStructure->Format)
    {
      case FormatVideo8888_ARGB:
          DisplayBuff->src.ulColorFmt           = SURF_ARGB8888;
          DisplayBuff->src.ulPixelDepth         = 32;
          DisplayBuff->src.ulTotalLines         = BufferStructure->Dimension[1];
          DisplayBuff->src.ulFlags             |= STM_PLANE_SRC_LIMITED_RANGE_ALPHA;
          break;

      case FormatVideo888_RGB:
          DisplayBuff->src.ulColorFmt           = SURF_RGB888;
          DisplayBuff->src.ulPixelDepth         = 24;
          DisplayBuff->src.ulTotalLines         = BufferStructure->Dimension[1];
          break;       

      case FormatVideo565_RGB:
          DisplayBuff->src.ulColorFmt           = SURF_RGB565;
          DisplayBuff->src.ulPixelDepth         = 16;
          DisplayBuff->src.ulTotalLines         = BufferStructure->Dimension[1];
          break;       

      case FormatVideo420_PairedMacroBlock:
      case FormatVideo420_MacroBlock:
          DisplayBuff->src.ulColorFmt           = SURF_YCBCR420MB;
          DisplayBuff->src.ulPixelDepth         = 8;
          break;

      case FormatVideo422_Raster:
          DisplayBuff->src.ulColorFmt           = SURF_YCBCR422R;
          DisplayBuff->src.ulPixelDepth         = 16;
          DisplayBuff->src.ulTotalLines         = BufferStructure->Dimension[1];
          break;

      case FormatVideo420_Planar:
          DisplayBuff->src.ulColorFmt           = SURF_YUV420;
          DisplayBuff->src.ulPixelDepth         = 8;
          DisplayBuff->src.ulTotalLines         = BufferStructure->Dimension[1];

          DisplayBuff->src.Rect.x              += BufferStructure->ComponentBorder[0]*16;
          DisplayBuff->src.Rect.y              += BufferStructure->ComponentBorder[1]*16;
          break;

      case FormatVideo422_Planar:
          DisplayBuff->src.ulColorFmt           = SURF_YUV422P;
          DisplayBuff->src.ulPixelDepth         = 8;
          DisplayBuff->src.ulTotalLines         = BufferStructure->Dimension[1];

          DisplayBuff->src.Rect.x              += BufferStructure->ComponentBorder[0]*16;
          DisplayBuff->src.Rect.y              += BufferStructure->ComponentBorder[1]*16;
          break;

      case FormatVideo422_YUYV:
          DisplayBuff->src.ulColorFmt           = SURF_YUYV;
          DisplayBuff->src.ulPixelDepth         = 16;
          DisplayBuff->src.ulTotalLines         = BufferStructure->Dimension[1];
          break;

      default:
          MANIFESTOR_ERROR("Unsupported display format (%d).\n", BufferStructure->Format);
          break;
    }   

}

static int SelectColourMatrixCoefficients(struct ParsedVideoParameters_s* VideoParameters)
{  
   unsigned int Flags = 0;

    switch( VideoParameters->Content.ColourMatrixCoefficients )
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
                Flags                          |= STM_PLANE_SRC_COLORSPACE_709;
                break;

        case MatrixCoefficients_Undefined:
        default:
                // Base coefficients on display size SD=601, HD = 709 
                if (VideoParameters->Content.Width > 720)
                    Flags                      |= STM_PLANE_SRC_COLORSPACE_709;
                break;
    }

    return Flags;
}

void Manifestor_VideoStmfb_c::SelectDisplayBufferPointers(struct BufferStructure_s*   BufferStructure,
                                        struct StreamBuffer_s*       StreamBuff,
                                        stm_display_buffer_t*       DisplayBuff)
{  
    // Fill in src fields depends on decimation
    if ((!StreamBuff->DecimateIfAvailable) || (BufferStructure->DecimatedSize == 0))
    {
        DisplayBuff->src.ulVideoBufferAddr      = StreamBuff->Data + BufferStructure->ComponentOffset[0];
        DisplayBuff->src.ulVideoBufferSize      = BufferStructure->Size;
        DisplayBuff->src.chromaBufferOffset     = BufferStructure->ComponentOffset[1] - BufferStructure->ComponentOffset[0];
        DisplayBuff->src.ulStride               = BufferStructure->Strides[0][0];
#if defined (CROP_INPUT_WHEN_DECIMATION_NEEDED_BUT_NOT_AVAILABLE)
        if (StreamBuff->DecimateIfAvailable)
            DisplayBuff->src.Rect               = croppedRect;
        else
#endif
            DisplayBuff->src.Rect               = srcRect;
    }
    else
    {
        unsigned int    HDecimationFactor       = 0;
        unsigned int    VDecimationFactor       = 0;

        DisplayBuff->src.ulVideoBufferAddr      = StreamBuff->Data + BufferStructure->ComponentOffset[2];
        DisplayBuff->src.ulVideoBufferSize      = BufferStructure->DecimatedSize;
        DisplayBuff->src.chromaBufferOffset     = BufferStructure->ComponentOffset[3] - BufferStructure->ComponentOffset[2];
        DisplayBuff->src.ulStride               = BufferStructure->Strides[0][2];

        if (Player->PolicyValue (Playback, Stream, PolicyDecimateDecoderOutput) == PolicyValueDecimateDecoderOutputQuarter)
        {
            HDecimationFactor                   = 2;
            VDecimationFactor                   = 1;
        }
        else
        {
            HDecimationFactor                   = 1;
            VDecimationFactor                   = 1;
        }
        DisplayBuff->src.Rect.x                 = srcRect.x      >> HDecimationFactor;
        DisplayBuff->src.Rect.y                 = srcRect.y      >> VDecimationFactor;
        DisplayBuff->src.Rect.width             = srcRect.width  >> HDecimationFactor;
        DisplayBuff->src.Rect.height            = srcRect.height >> VDecimationFactor;
    }
    //MANIFESTOR_DEBUG("src.Rect = %dx%d at %d,%d\n", DisplayBuff->src.Rect.width, DisplayBuff->src.Rect.height, DisplayBuff->src.Rect.x, DisplayBuff->src.Rect.y);


}

void Manifestor_VideoStmfb_c::ApplyPixelAspectRatioCorrection ( stm_display_buffer_t*       DisplayBuff,
                                                                struct ParsedVideoParameters_s* VideoParameters)
{
    Rational_t                  RestrictedPixelAspectRatio;

    if(PixelAspectRatioCorrectionPolicyValue < PolicyValuePixelAspectRatioCorrectionDisabled)
    {
        DisplayBuff->dst.ulFlags                 |= STM_PLANE_DST_CONVERT_TO_16_9_DISPLAY;
    }

    // The Rational class assignment operator tries to restrict the
    // numerator and denominator to 32bit values.
    RestrictedPixelAspectRatio                    = VideoParameters->Content.PixelAspectRatio;

    DisplayBuff->src.PixelAspectRatio.numerator   = (long)RestrictedPixelAspectRatio.GetNumerator();
    DisplayBuff->src.PixelAspectRatio.denominator = (long)RestrictedPixelAspectRatio.GetDenominator();
    DisplayBuff->src.ulLinearCenterPercentage     = PixelAspectRatioCorrectionPolicyValue;   

}
//}}}

//{{{  QueueBuffer
//{{{  doxynote
/// \brief                      Actually put buffer on display
/// \param BufferIndex          Index into array of stream buffers
/// \param FrameParameters      Frame parameters generated by frame parser
/// \param VideoParameters      Display positioning information generated by frame parser
/// \param VideoOutputTiming    Display timing information generated by output timer
/// \return                     Success or fail
//}}}
ManifestorStatus_t Manifestor_VideoStmfb_c::QueueBuffer        (unsigned int                    BufferIndex,
                                                                struct ParsedFrameParameters_s* FrameParameters,
                                                                struct ParsedVideoParameters_s* VideoParameters,
                                                                struct VideoOutputTiming_s*     VideoOutputTiming,
                                                                struct BufferStructure_s*       BufferStructure)
{
    struct StreamBuffer_s*      StreamBuff      = &StreamBuffer[BufferIndex];
    stm_display_buffer_t*       DisplayBuff     = &DisplayBuffer[BufferIndex];
    int                         Status          = 0;
    BufferStatus_t              BufferStatus    = BufferNoError;
    unsigned int                i               = 0;
    unsigned int                FirstFieldFlags = 0;
    unsigned int                SecondFieldFlags;
    unsigned int                FieldFlags;
    unsigned int                Count;
    unsigned int                DCount;
    unsigned int                PCount;
    unsigned int                PSIndex;
    bool                        UsePanScan      = false;
    struct QueueRecord_s        QueueRecord[MAX_PAN_SCAN_VALUES*2];
    Rational_t                  Speed;
    PlayDirection_t             Direction;
    Buffer_t                    PostProcessBuffer;
    int                         Adjustment;

    //
    // Clear the DisplayBuff record
    //

    memset( (void*)DisplayBuff, 0, sizeof(stm_display_buffer_t));

//

#if defined (QUEUE_BUFFER_CAN_FAIL)
    DisplayFlush                = false;
#endif
    // Convert the rational (where 1 means no adjustment) into parts per million

    DerivePPMValueFromOutputRateAdjustment( StreamBuff->OutputTiming->OutputRateAdjustment, &Adjustment );
    Adjustment                  -= 1000000;

    if (Adjustment != ClockRateAdjustment)
    {
//        MANIFESTOR_DEBUG("Setting output clock adjustment to %d parts per million\n", Adjustment);
        Status                  = stm_display_output_set_control (Output, STM_CTRL_CLOCK_ADJUSTMENT, Adjustment);
        if (Status == 0)
            ClockRateAdjustment = Adjustment;
        else
            MANIFESTOR_ERROR ("Cannot set output rate adjustment (%d).\n", Status);
    }

    //StreamBuff->QueueCount                      = 0;
    StreamBuff->Manifestor                      = this;
    StreamBuff->TimeOnDisplay                   = INVALID_TIME;

    // Fill in dst fields
    DisplayBuff->dst.ulFlags                    = 0;
    DisplayBuff->dst.Rect                       = dstRect;


    SelectDisplayBufferPointers(BufferStructure,StreamBuff,DisplayBuff);

    ApplyPixelAspectRatioCorrection(DisplayBuff,VideoParameters);   

    SelectDisplaySource (BufferStructure,DisplayBuff);

#if 0
    DisplayBuff->src.ulPostProcessLumaType      = 0;
    DisplayBuff->src.ulPostProcessChromaType    = 0;
#endif

    DisplayBuff->src.ulConstAlpha               = 0xff;
    FirstFieldFlags                             = STM_PLANE_SRC_CONST_ALPHA | STM_PLANE_SRC_XY_IN_16THS;

    FirstFieldFlags |= SelectColourMatrixCoefficients(VideoParameters);

    if (VideoOutputTiming->Interlaced)
        FirstFieldFlags                        |= STM_PLANE_SRC_INTERLACED;

    Player->GetPlaybackSpeed (Playback, &Speed, &Direction);
    if (Speed < 1)
        FirstFieldFlags                        |= STM_PLANE_SRC_INTERPOLATE_FIELDS;

    // Check if any post processing buffers are attached and inform the video driver accordingly
    BufferStatus        = StreamBuff->BufferClass->ObtainAttachedBufferReference (Player->BufferVideoPostProcessingControlType, &PostProcessBuffer);
    if (BufferStatus == BufferNoError)
    {
        VideoPostProcessingControl_t*       PPControl;

        PostProcessBuffer->ObtainDataReference (NULL, NULL, (void**)&PPControl);
#if 0
        if (PPControl->RangeMapLumaPresent)
        {
            DisplayBuff->src.ulPostProcessLumaType          = PPControl->RangeMapLuma;
            FirstFieldFlags                                |= STM_PLANE_SRC_VC1_POSTPROCESS_LUMA;
        }
        if (PPControl->RangeMapChromaPresent)
        {
            DisplayBuff->src.ulPostProcessChromaType        = PPControl->RangeMapChroma;
            FirstFieldFlags                                |= STM_PLANE_SRC_VC1_POSTPROCESS_CHROMA;
        }
        MANIFESTOR_DEBUG ("Range Map Settings: Luma %d, Chroma %d\n", PPControl->RangeMapLuma, PPControl->RangeMapChroma);
#endif
    }

    SecondFieldFlags                            = FirstFieldFlags;

    //if (!VideoOutputTiming->TopFieldFirst)
    //    FirstFieldFlags                        |= STM_PLANE_SRC_BOTTOM_FIELD_FIRST;

    // Fill in info fields
    DisplayBuff->info.ulFlags                   = STM_PLANE_PRESENTATION_PERSISTENT;
    DisplayBuff->info.presentationTime          = (VideoOutputTiming->SystemPlaybackTime != UNSPECIFIED_TIME) ?
                                                        VideoOutputTiming->SystemPlaybackTime : 0;
    DisplayBuff->info.pUserData                 = StreamBuff;

    //{{{  COMMENT
    #if 0
    The output timing module will generate field/frame counts for the manifestor as follows :-

        Content         Display         Generated counts
        Progressive     Progressive     one value, the display frame count.
        Progressive     Interlaced      one value, the display field count.
        Interlaced      Progressive     two values, first field display frame count + second field display frame count.
        Interlaced      Interlaced      two values, first field display field count + second field display field count.

    The display driver will be fed entities (field or frame) in the following manner (where the counts above are N and M):-
        Content         Display         Display driver input
        Progressive     Progressive     Single queue of N frame periods of the frame.
        Progressive     Interlaced      N separate queues each of one frame period, alternating appropriate first field, second, first ...
        Interlaced      Progressive     Two queues of N frame periods of the first field + M periods of the second.
        Interlaced      Interlaced      Two queues of N frame periods of the first field + M periods of the second.

    This is further modified by the constraint that should pan and scan values be used, the may not vary during a single queueing. IE if you have anything queued for two periods with PS values A for the first and B for the second, this must be split into two separate queueings.

    The data to be supplied on each queueing of a display entity is at least :-
        Frame pointer
        structure information of the buffer (Top field, Bottom field, Interlaced Frame, Progressive Frame)
        what to display (Top field, Bottom field, Frame)
        Time at which to display it
    #endif
    //}}}

    if ((DisplayFormatPolicyValue == PolicyValuePanScan) && (DisplayAspectRatioPolicyValue == PolicyValue4x3) && (VideoOutputTiming->PanScan.Count > 0))
    //{{{  check pan/scan info
    {
        PCount      = 0;
        for (i = 0; i < VideoOutputTiming->PanScan.Count; i++)
            PCount += VideoOutputTiming->PanScan.DisplayCount[i];
        if (PCount != (VideoOutputTiming->DisplayCount[0] + VideoOutputTiming->DisplayCount[1]))
            MANIFESTOR_DEBUG  ("Warning display counts do not match: Display count %d, %d, Pan scan count (%d) %d %d %d\n",
                               VideoOutputTiming->DisplayCount[0], VideoOutputTiming->DisplayCount[1], VideoOutputTiming->PanScan.Count,
                               VideoOutputTiming->PanScan.DisplayCount[0], VideoOutputTiming->PanScan.DisplayCount[1], VideoOutputTiming->PanScan.DisplayCount[2]);

        //for (i = 0; i < VideoOutputTiming->PanScan.Count; i++)
        //{
        //    MANIFESTOR_DEBUG ("Pan scan (%d, %d), Horiz %d, vert %d\n", i, VideoOutputTiming->PanScan.DisplayCount[i],
        //                   VideoOutputTiming->PanScan.HorizontalOffset[i], VideoOutputTiming->PanScan.VerticalOffset[i]);
        //}

        if (VideoOutputTiming->PanScan.Count == 1)
        {
            DisplayBuff->src.Rect.x    -= VideoOutputTiming->PanScan.HorizontalOffset[0];
            DisplayBuff->src.Rect.y    -= VideoOutputTiming->PanScan.VerticalOffset[0];
            UsePanScan                  = false;
        }
        else
            UsePanScan                  = true;
    }
    //}}}

    // Special case when an interlaced frame needs to be repeated on an interlaced display.
    // For example 25Hz content on 60Hz display where every 5th frame is repeated.
    // If this is presented as separate fields, the driver inserts field delays to prevent presentation
    // on the wrong field.  This is avoided by presenting the frame as a single entity.
    if ((VideoOutputTiming->DisplayCount[0] > 1) && (Speed == 1) && (VideoOutputTiming->Interlaced) && (!SurfaceDescriptor.Progressive))
    {
        if (!VideoOutputTiming->TopFieldFirst)
            FirstFieldFlags            |= STM_PLANE_SRC_BOTTOM_FIELD_FIRST;
        QueueRecord[0].Flags            = FirstFieldFlags;
        QueueRecord[0].Count            = VideoOutputTiming->DisplayCount[0] + VideoOutputTiming->DisplayCount[1];
        QueueRecord[i].PSIndex          = 0;
        Count                           = 1;
        StreamBuff->QueueCount         += 1;
    }
    else
    {
        if (VideoOutputTiming->DisplayCount[1] > 0)
        {
            if (VideoOutputTiming->TopFieldFirst)
            {
                FirstFieldFlags    |= STM_PLANE_SRC_TOP_FIELD_ONLY;
                SecondFieldFlags   |= STM_PLANE_SRC_BOTTOM_FIELD_ONLY;
            }
            else
            {
                FirstFieldFlags    |= STM_PLANE_SRC_BOTTOM_FIELD_ONLY;
                SecondFieldFlags   |= STM_PLANE_SRC_TOP_FIELD_ONLY;
            }
        }

        DCount              = VideoOutputTiming->DisplayCount[0];
        PCount              = VideoOutputTiming->PanScan.DisplayCount[0];
        Count               = 0;
        PSIndex             = 0;
        FieldFlags          = FirstFieldFlags;
        for (i = 0; Count < (VideoOutputTiming->DisplayCount[0] + VideoOutputTiming->DisplayCount[1]); i++)
        {
            unsigned int FieldCount = DCount - Count;

            //if ((UsePanScan) && (FieldCount > VideoOutputTiming->PanScan.DisplayCount[PSIndex]))
            //    FieldCount          = VideoOutputTiming->PanScan.DisplayCount[PSIndex];

            if (UsePanScan)
            {
                if (FieldCount > VideoOutputTiming->PanScan.DisplayCount[PSIndex])
                    FieldCount  = VideoOutputTiming->PanScan.DisplayCount[PSIndex];
                QueueRecord[i].PSIndex  = PSIndex;
            }
            else
                QueueRecord[i].PSIndex  = -1;

            QueueRecord[i].Count    = FieldCount;
            QueueRecord[i].Flags    = FieldFlags;
            Count                  += FieldCount;

            if (Count == DCount)
            {
                DCount             += VideoOutputTiming->DisplayCount[1];
                FieldFlags          = SecondFieldFlags;
            }
            if ((UsePanScan) && (Count == PCount))
            {
                if (PSIndex < (VideoOutputTiming->PanScan.Count - 1))
                    PSIndex++;
                PCount             += VideoOutputTiming->PanScan.DisplayCount[PSIndex];
            }
        }
        Count                       = i;
        StreamBuff->QueueCount     += i;
        //MANIFESTOR_DEBUG ("Queuing buffer %d, Count %d (%d, %d)\n", StreamBuff->BufferIndex, Count,
        //                   VideoOutputTiming->DisplayCount[0], VideoOutputTiming->DisplayCount[1]);
    }

    //{{{  Register events associated with this buffer
    if (Count > 0)
    {
        if ((DisplayEventRequested & EventSourceSizeChangeManifest) != 0)
        {
            DisplayEvent.Code           = EventSourceSizeChangeManifest;
            DisplayEvent.Playback       = Playback;
            DisplayEvent.Stream         = Stream;
            DisplayEvent.PlaybackTime   = FrameParameters->NativePlaybackTime;
            StreamBuff->EventPending    = true;
            QueueEventSignal (&DisplayEvent);
        }
        if ((DisplayEventRequested & EventSourceFrameRateChangeManifest) != 0)
        {
            DisplayEvent.Code           = EventSourceFrameRateChangeManifest;
            DisplayEvent.Playback       = Playback;
            DisplayEvent.Stream         = Stream;
            DisplayEvent.PlaybackTime   = FrameParameters->NativePlaybackTime;
            DisplayEvent.Rational       = VideoParameters->Content.FrameRate;
            StreamBuff->EventPending    = true;
            QueueEventSignal (&DisplayEvent);
        }
        if ((DisplayEventRequested & EventOutputSizeChangeManifest) != 0)
        {
            DisplayEvent.Code           = EventOutputSizeChangeManifest;
            DisplayEvent.Playback       = Playback;
            DisplayEvent.Stream         = Stream;
            DisplayEvent.PlaybackTime   = FrameParameters->NativePlaybackTime;
            DisplayEvent.Value[0].UnsignedInt   = OutputWindow.X;
            DisplayEvent.Value[1].UnsignedInt   = OutputWindow.Y;
            DisplayEvent.Value[2].UnsignedInt   = OutputWindow.Width;
            DisplayEvent.Value[3].UnsignedInt   = OutputWindow.Height;

            StreamBuff->EventPending    = true;
            QueueEventSignal (&DisplayEvent);
        }
        DisplayEventRequested           = 0;
    }
    //}}}
    for (i = 0, Status = 0; (i < Count) && (Status == 0); i++)
    {
        DisplayBuff->src.ulFlags               = QueueRecord[i].Flags;

        if (DisplayBuff->src.ulColorFmt == SURF_ARGB8888)
            DisplayBuff->src.ulFlags             |= STM_PLANE_SRC_LIMITED_RANGE_ALPHA;

        DisplayBuff->info.nfields               = QueueRecord[i].Count;

        if ((QueueRecord[i].Count != 1) && (DisplayBuff->src.ulColorFmt == SURF_RGB565))
                    MANIFESTOR_ERROR("QueueRecord[i].Count %d\n",QueueRecord[i].Count);

        if (QueueRecord[i].PSIndex >= 0)
        {
            DisplayBuff->src.Rect.x             = srcRect.x - VideoOutputTiming->PanScan.HorizontalOffset[QueueRecord[i].PSIndex];
            DisplayBuff->src.Rect.y             = srcRect.y - VideoOutputTiming->PanScan.VerticalOffset[QueueRecord[i].PSIndex];
        }
        // register a going on display call back for the first field and a coming off for the last field
        // so we get time on/off display correct and we put buffer back on ring once only.
        if (i == 0)
            DisplayBuff->info.pDisplayCallback          = &display_callback;
        else
            DisplayBuff->info.pDisplayCallback          = NULL;

        //{{{  COMMENT removed because we have switched back to reference counting to solve half-queuing problem on flush
        #if 0
        if (i == (Count - 1))
            DisplayBuff->info.pCompletedCallback        = &done_callback;
        else
            DisplayBuff->info.pCompletedCallback        = NULL;
        #endif
        //}}}

        DisplayBuff->info.pCompletedCallback            = &done_callback;
        ValidatePhysicalDecodeBufferAddress( DisplayBuff->src.ulVideoBufferAddr );
        Status                                          = stm_display_plane_queue_buffer (Plane, DisplayBuff);
        if (Status != 0)
        {
#if !defined (QUEUE_BUFFER_CAN_FAIL)
            // At this point something has gone seriously wrong.  Firstly tell the outside world and
            // then crash and burn.
            unsigned int Parameters[MONITOR_PARAMETER_COUNT];

            memset (Parameters, 0, sizeof (Parameters));
            Parameters[0]                               = BufferIndex;
            MonitorSignalEvent (MONITOR_EVENT_INFORMATION, Parameters, "Failed to queue buffer to display");

            DisplayEvent.Code                           = EventFailedToQueueBufferToDisplay;
            DisplayEvent.Playback                       = Playback;
            DisplayEvent.Stream                         = Stream;
            DisplayEvent.PlaybackTime                   = OS_GetTimeInMicroSeconds ();
            Player->SignalEvent (&DisplayEvent);

            Player->MarkStreamUnPlayable (Stream);
            MANIFESTOR_ERROR ("Failed to queue buffer %d to display\n", BufferIndex);
            return ManifestorUnplayable;
#else
            WaitingForHeadroom                          = true;
            if (DisplayAvailableValid)
            {
                DisplayHeadroom                         = ((Count - i) > STMFB_BUFFER_HEADROOM) ? STMFB_BUFFER_HEADROOM : Count - i;

                OS_SemaphoreWait (&DisplayAvailable);

                if (!DisplayFlush)
                {
                    ValidatePhysicalDecodeBufferAddress( DisplayBuff->src.ulVideoBufferAddr );
                    Status                              = stm_display_plane_queue_buffer (Plane, DisplayBuff);
                }
            }
            WaitingForHeadroom                          = false;
#endif
        }
        DisplayBuff->info.presentationTime              = 0;

    }

    if (Status == 0)
        return ManifestorNoError;
    else
        return ManifestorError;
}
//}}}
//{{{  QueueInitialFrame
//{{{  doxynote
/// \brief                      Actually put first field of initial buffer on display
/// \param BufferIndex          Index into array of stream buffers
/// \param VideoParameters      Display positioning information generated by frame parser
/// \return                     Success or fail
//}}}
ManifestorStatus_t Manifestor_VideoStmfb_c::QueueInitialFrame  (unsigned int                    BufferIndex,
                                                                struct ParsedVideoParameters_s* VideoParameters,
                                                                struct BufferStructure_s*       BufferStructure)
{

    struct StreamBuffer_s*      StreamBuff      = &StreamBuffer[BufferIndex];
    stm_display_buffer_t*       DisplayBuff     = &DisplayBuffer[BufferIndex];
    unsigned int                Flags           = 0;

    //
    // Clear the DisplayBuff record
    //

    memset( (void*)DisplayBuff, 0, sizeof(stm_display_buffer_t));

//

    //MANIFESTOR_DEBUG("\n");
    StreamBuff->QueueCount                      = 0;
    StreamBuff->Manifestor                      = this;
    StreamBuff->TimeOnDisplay                   = INVALID_TIME;

    DisplayBuff->dst.ulFlags                    = 0;
    DisplayBuff->dst.Rect                       = dstRect;

    if(PixelAspectRatioCorrectionPolicyValue < PolicyValuePixelAspectRatioCorrectionDisabled)
    {
        DisplayBuff->dst.ulFlags               |= STM_PLANE_DST_CONVERT_TO_16_9_DISPLAY;
    }

    SelectDisplayBufferPointers(BufferStructure,StreamBuff,DisplayBuff);

    ApplyPixelAspectRatioCorrection(DisplayBuff,VideoParameters);   

    SelectDisplaySource (BufferStructure,DisplayBuff);

#if 0
    DisplayBuff->src.ulPostProcessLumaType      = 0;
    DisplayBuff->src.ulPostProcessChromaType    = 0;
#endif

    DisplayBuff->info.presentationTime          = 0;

    if ((DisplayFormatPolicyValue == PolicyValuePanScan) && (DisplayAspectRatioPolicyValue == PolicyValue4x3) && (VideoParameters->PanScan.Count > 0))
    {
        DisplayBuff->src.Rect.x                -= VideoParameters->PanScan.HorizontalOffset[0];
        DisplayBuff->src.Rect.y                -= VideoParameters->PanScan.VerticalOffset[0];
    }

    DisplayBuff->src.ulConstAlpha               = 0xff;
    Flags                                       = STM_PLANE_SRC_CONST_ALPHA | STM_PLANE_SRC_XY_IN_16THS;

    Flags |= SelectColourMatrixCoefficients(VideoParameters);

    if (VideoParameters->InterlacedFrame)
        Flags                                  |= STM_PLANE_SRC_INTERLACED;

    if (VideoParameters->DisplayCount[1] > 0)
    {
        if (VideoParameters->TopFieldFirst)
            Flags                              |= STM_PLANE_SRC_TOP_FIELD_ONLY;
        else
            Flags                              |= STM_PLANE_SRC_BOTTOM_FIELD_ONLY;
    }
    else if (!VideoParameters->TopFieldFirst)
        Flags                                  |= STM_PLANE_SRC_BOTTOM_FIELD_FIRST;
    DisplayBuff->src.ulFlags                    = Flags;

    if (DisplayBuff->src.ulColorFmt == SURF_ARGB8888)
        DisplayBuff->src.ulFlags               |= STM_PLANE_SRC_LIMITED_RANGE_ALPHA;

    DisplayBuff->info.ulFlags                   = STM_PLANE_PRESENTATION_PERSISTENT;
    DisplayBuff->info.nfields                   = 1;
    DisplayBuff->info.pDisplayCallback          = &initial_frame_display_callback;
    DisplayBuff->info.pCompletedCallback        = NULL;
    DisplayBuff->info.pUserData                 = StreamBuff;

    MANIFESTOR_DEBUG("Queueing initial frame %d\n", BufferIndex);
    InitialFrameState                           = InitialFrameQueued;

    ValidatePhysicalDecodeBufferAddress( DisplayBuff->src.ulVideoBufferAddr );
    int QueueBufferResult = stm_display_plane_queue_buffer (Plane, DisplayBuff);
    if (QueueBufferResult == 0)
        return ManifestorNoError;

    InitialFrameState                           = InitialFrameNotQueued;
    Player->MarkStreamUnPlayable (Stream);
    MANIFESTOR_ERROR ("Failed to queue initial frame %d to display - error %d\n", BufferIndex,QueueBufferResult);
    return ManifestorUnplayable;
}
//}}}
//{{{  QueueNullManifestation
// /////////////////////////////////////////////////////////////////////////
//
//      QueueNullManifestation :
//      Action  : Insert null frame into display sequence
//      Input   :
//      Output  :
//      Results :
//

ManifestorStatus_t      Manifestor_VideoStmfb_c::QueueNullManifestation (void)
{
    MANIFESTOR_DEBUG("\n");

    return ManifestorNoError;
}
//}}}
//{{{  FlushDisplayQueue
//{{{  doxynote
/// \brief      Flushes the display queue so buffers not yet manifested are returned
//}}}
ManifestorStatus_t      Manifestor_VideoStmfb_c::FlushDisplayQueue (void)
{
    MANIFESTOR_DEBUG("\n");

#if defined (QUEUE_BUFFER_CAN_FAIL)
    DisplayFlush        = true;
#endif

    stm_display_plane_pause (Plane, 1);

    if (BufferOnDisplay == INVALID_BUFFER_ID)
    {
        OS_SemaphoreSignal (&InitialFrameDisplayed);
           InitialFrameState       = InitialFrameNotQueued;
    }

#if defined (QUEUE_BUFFER_CAN_FAIL)
    if (DisplayHeadroom != 0)
    {
        DisplayHeadroom = 0;
        OS_SemaphoreSignal (&DisplayAvailable);
    }
#endif

    // Again Julian, a hack...
    ManifestorLastDisplayedBuffer = NULL;
    wake_up_interruptible (&g_ManifestorLastWaitQueue);

    return ManifestorNoError;
}
//}}}

//{{{  UpdateDisplayWindows
// ///////////////////////////////////////////////////////////////////////////////////////
//
//      UpdateDisplayWindows:
//      Action  : Update display parameters - set source and destination windows
//      Input   : void
//      Output  :
//      Results :
//

ManifestorStatus_t Manifestor_VideoStmfb_c::UpdateDisplayWindows (void)
{
    stm_plane_caps_t            PlaneCaps;
    const stm_mode_line_t*      CurrentMode;

    MANIFESTOR_DEBUG ("\n");
    // If this isn't on screen then just mark the update as pending
    //if (!Visible)
    //{
    //    DisplayUpdatePending     = true;
    //    return ManifestorNoError;
    //}

    if (!Stepping)
        MANIFESTOR_DEBUG("%d,%d, %dx%d, Dest %d,%d, %dx%d\n",
                      InputWindow.X,  InputWindow.Y,  InputWindow.Width,  InputWindow.Height,
                      OutputWindow.X, OutputWindow.Y, OutputWindow.Width, OutputWindow.Height);

    // Check output window fits in active area
    CurrentMode = stm_display_output_get_current_display_mode (Output);
    if (CurrentMode == NULL)
    {
        MANIFESTOR_ERROR ("Cannot access current display mode\n");
        return ManifestorError;
    }

    if (((OutputWindow.X + OutputWindow.Width) > CurrentMode->ModeParams.ActiveAreaWidth) ||
        ((OutputWindow.Y + OutputWindow.Height) > CurrentMode->ModeParams.ActiveAreaHeight))
    {
        MANIFESTOR_ERROR ("Output window too large for display\n");
        return ManifestorUnplayable;
    }

    // Check can resize if requested
    stm_display_plane_get_capabilities (Plane, &PlaneCaps);
    if ((PlaneCaps.ulCaps & PLANE_CAPS_RESIZE) != PLANE_CAPS_RESIZE)
    {
        if ((OutputWindow.Width != InputWindow.Width) || (OutputWindow.Height != InputWindow.Height))
        {
            MANIFESTOR_ERROR ("Cannot resize content to fit display\n");
            return ManifestorUnplayable;
        }
    }

    // Check surface supports content size
    if ((InputWindow.Width  < PlaneCaps.ulMinWidth) ||
        (InputWindow.Width  > PlaneCaps.ulMaxWidth) ||
        (InputWindow.Height < PlaneCaps.ulMinHeight) ||
        (InputWindow.Height > PlaneCaps.ulMaxHeight))
    {
        MANIFESTOR_ERROR ("Content %dx%d not supported by display (min: %dx%d, max: %dx%d)\n", InputWindow.Width, InputWindow.Height,
                           PlaneCaps.ulMinWidth, PlaneCaps.ulMinHeight, PlaneCaps.ulMaxWidth, PlaneCaps.ulMaxHeight);
        return ManifestorUnplayable;
    }

    srcRect.x                   = InputWindow.X;
    srcRect.y                   = InputWindow.Y;
    srcRect.width               = InputWindow.Width;
    srcRect.height              = InputWindow.Height;

    croppedRect.x               = CroppedWindow.X;
    croppedRect.y               = CroppedWindow.Y;
    croppedRect.width           = CroppedWindow.Width;
    croppedRect.height          = CroppedWindow.Height;

    dstRect.x                   = OutputWindow.X;
    dstRect.y                   = OutputWindow.Y;
    dstRect.width               = OutputWindow.Width;
    dstRect.height              = OutputWindow.Height;

    return ManifestorNoError;
}
//}}}
//{{{  CheckInputDimensions
// ///////////////////////////////////////////////////////////////////////////////////////
//
//      UpdateDisplayWindows:
//      Action  : Update display parameters - set source and destination windows
//      Input   : void
//      Output  :
//      Results :
//

ManifestorStatus_t Manifestor_VideoStmfb_c::CheckInputDimensions       (unsigned int    Width,
                                                                        unsigned int    Height)
{
    stm_plane_caps_t            PlaneCaps;

    MANIFESTOR_DEBUG ("\n");

    // Check surface supports content size

    stm_display_plane_get_capabilities (Plane, &PlaneCaps);

    if ((Width  < PlaneCaps.ulMinWidth) ||
        (Width  > PlaneCaps.ulMaxWidth) ||
        (Height < PlaneCaps.ulMinHeight) ||
        (Height > PlaneCaps.ulMaxHeight))
    {
        MANIFESTOR_ERROR ("Content %dx%d not supported by display (min: %dx%d, max: %dx%d)\n", Width, Height,
                           PlaneCaps.ulMinWidth, PlaneCaps.ulMinHeight, PlaneCaps.ulMaxWidth, PlaneCaps.ulMaxHeight);
        return ManifestorError;
    }

    return ManifestorNoError;
}
//}}}
//{{{  SynchronizeOutput
// ///////////////////////////////////////////////////////////////////////////////////////
//
//      SynchronizeOutput:
//      Action  : Force the stmfb driver to synchronize vsync
//      Input   : void
//      Output  :
//      Results :
//

ManifestorStatus_t Manifestor_VideoStmfb_c::SynchronizeOutput( void )
{
    MANIFESTOR_DEBUG ("\n");

    stm_display_output_soft_reset( Output );

    return ManifestorNoError;
}
//}}}
//{{{  Enable
// ///////////////////////////////////////////////////////////////////////////////////////
//    Enable
//

ManifestorStatus_t Manifestor_VideoStmfb_c::Enable (void)
{
    //MANIFESTOR_DEBUG ("\n");
    Visible     = true;
    stm_display_plane_show (Plane);

    return ManifestorNoError;
}
//}}}
//{{{  Disable
// ///////////////////////////////////////////////////////////////////////////////////////
//    Disable
//

ManifestorStatus_t Manifestor_VideoStmfb_c::Disable (void)
{
    //MANIFESTOR_DEBUG ("\n");
    Visible             = false;
    stm_display_plane_hide (Plane);

    return ManifestorNoError;
}
//}}}

//{{{  BufferAvailable
// ///////////////////////////////////////////////////////////////////////////////////////
//
//      BufferAvailable
//      Action  : Checks to see if the buffer is currently visible
//      Input   : void
//      Output  :
//      Results : true if visible false otherwise
//

bool Manifestor_VideoStmfb_c::BufferAvailable     (unsigned char*  Address,
                                                        unsigned int    Size)
{
    unsigned char*      End;
    unsigned char*      DisplayEnd;

    MANIFESTOR_DEBUG("\n");

    if ((InitialFrameState != InitialFramePossible) && (InitialFrameState != InitialFrameQueued))
        return true;

    if ((!Visible) || (DisplaySize == 0))
        return true;

    End                 = Address + Size;
    DisplayEnd          = DisplayAddress + DisplaySize;

    if (((Address >= DisplayAddress) && (Address < DisplayEnd)) || ((End >= DisplayAddress) && (End < DisplayEnd)))
    {
        MANIFESTOR_DEBUG ("Overlap at Address %p, End %p (%d), DisplayAddress %p, DisplayEnd %p (%d)\n", Address, End, Size, DisplayAddress, DisplayEnd, DisplaySize);
        return false;
    }

    return true;
}
//}}}

//{{{  DisplayCallback
void Manifestor_VideoStmfb_c::DisplayCallback  (struct StreamBuffer_s*  Buffer,
                                                TIME64                  VsyncTime)
{
    Buffer->TimeOnDisplay       = VsyncTime;
    BufferOnDisplay             = Buffer->BufferIndex;

    TimeSlotOnDisplay           = Buffer->TimeSlot;
    PtsOnDisplay                = Buffer->NativePlaybackTime;
    FrameCount++;

    // Again Julian, sorry for this hack.
    ManifestorLastDisplayedBuffer = &DisplayBuffer[BufferOnDisplay];
    wake_up_interruptible (&g_ManifestorLastWaitQueue);

    DisplayAddress              = (unsigned char*)(DisplayBuffer[BufferOnDisplay].src.ulVideoBufferAddr);
    DisplaySize                 = DisplayBuffer[BufferOnDisplay].src.ulVideoBufferSize;

#if !defined (COUNT_FRAMES)
    if (Buffer->EventPending)
#endif
        OS_SemaphoreSignal (&BufferDisplayed);

#if defined (QUEUE_BUFFER_CAN_FAIL)
    if ((DisplayHeadroom != 0) && !DisplayFlush)
    {
        DisplayHeadroom--;
        if (DisplayHeadroom == 0)
            OS_SemaphoreSignal (&DisplayAvailable);
    }
#endif

#if 0
    {
        unsigned int                Parameters[MONITOR_PARAMETER_COUNT];
        memset (Parameters, 0, sizeof (Parameters));
        memcpy (&Parameters[0], (unsigned int*)&VsyncTime, sizeof(VsyncTime));
        memcpy (&Parameters[2], (unsigned int*)&PtsOnDisplay, sizeof(PtsOnDisplay));
        MonitorSignalEvent (MONITOR_EVENT_INFORMATION, Parameters, "Frame on display");
    }
#endif
}
static void display_callback   (void*           Buffer,
                                TIME64          VsyncTime)
{
    struct StreamBuffer_s*              StreamBuffer    = (struct StreamBuffer_s*)Buffer;
    class Manifestor_VideoStmfb_c*      Manifestor      = (Manifestor_VideoStmfb_c*)StreamBuffer->Manifestor;

    Manifestor->DisplayCallback (StreamBuffer, VsyncTime);
}
//}}}
//{{{  InitialFrameDisplayCallback
void Manifestor_VideoStmfb_c::InitialFrameDisplayCallback      (struct StreamBuffer_s*  Buffer,
                                                                TIME64                  VsyncTime)
{
    unsigned int                Parameters[MONITOR_PARAMETER_COUNT];

    Buffer->TimeOnDisplay       = VsyncTime;
    BufferOnDisplay             = Buffer->BufferIndex;
    PtsOnDisplay                = INVALID_TIME;
    InitialFrameState           = InitialFrameOnDisplay;
    OS_SemaphoreSignal (&InitialFrameDisplayed);

    // Again Julian, sorry for this hack.
    ManifestorLastDisplayedBuffer = &DisplayBuffer[BufferOnDisplay];
    wake_up_interruptible (&g_ManifestorLastWaitQueue);

    DisplayAddress              = (unsigned char*)(DisplayBuffer[Buffer->BufferIndex].src.ulVideoBufferAddr);
    DisplaySize                 = DisplayBuffer[Buffer->BufferIndex].src.ulVideoBufferSize;

    memset (Parameters, 0, sizeof (Parameters));
    memcpy (Parameters, (unsigned int*)&VsyncTime, sizeof(VsyncTime));
    MonitorSignalEvent (MONITOR_EVENT_INFORMATION, Parameters, "First field on display");

}
static void initial_frame_display_callback     (void*           Buffer,
                                                TIME64          VsyncTime)
{
    struct StreamBuffer_s*              StreamBuffer    = (struct StreamBuffer_s*)Buffer;
    class Manifestor_VideoStmfb_c*      Manifestor      = (Manifestor_VideoStmfb_c*)StreamBuffer->Manifestor;

    Manifestor->InitialFrameDisplayCallback (StreamBuffer, VsyncTime);
}
//}}}
//{{{  DoneCallback
void Manifestor_VideoStmfb_c::DoneCallback     (struct StreamBuffer_s*  Buffer,
                                                TIME64                  VsyncTime,
                                                unsigned int            Status )
{
    //if ((Buffer->BufferState != BufferStateQueued) || (--(Buffer->QueueCount) != 0))
    if ((Buffer->BufferState != BufferStateQueued) && (Buffer->BufferState != BufferStateMultiQueue))
        return;

    if (Buffer->QueueCount == 1)                                // Fill in fields if about to be released
    {
        if (TestComponentState (ComponentRunning))
            Buffer->OutputTiming->ActualSystemPlaybackTime      = Buffer->TimeOnDisplay;

        if ((Buffer->TimeSlot > TimeSlotOnDisplay) && (Buffer->TimeSlot < NextTimeSlot))
            NextTimeSlot                                        = Buffer->TimeSlot;
            //NextTimeSlot    = (NextTimeSlot < TimeSlotOnDisplay) ? NextTimeSlot : TimeSlotOnDisplay;
    }

    if( (Status & STM_PLANE_STATUS_BUF_HW_ERROR) != 0 )
        FatalHardwareError              = true;

    DequeuedStreamBuffers[DequeueIn]   = Buffer;
    DequeueIn++;
    if (DequeueIn == MAX_DEQUEUE_BUFFERS)
        DequeueIn       = 0;
}


static void done_callback      (void*   Buffer,
                                const stm_buffer_presentation_stats_t *Data)
{
    struct StreamBuffer_s*              StreamBuffer    = (struct StreamBuffer_s*)Buffer;
    class Manifestor_VideoStmfb_c*      Manifestor      = (Manifestor_VideoStmfb_c*)StreamBuffer->Manifestor;

#ifdef CONFIG_RELAY
#define RELAY_CRC
    // All this relay stuff needs to be placed somehow in the abstraction layer, an extended report effectivley
    char buffer[96];

    if (Data)
    {
        sprintf (buffer,"%llx %08x %x %x %x %x\n",
                 StreamBuffer->NativePlaybackTime,
                 (unsigned int)Data->ulStatus,
                 (unsigned int)Data->ulYCRC[0],
                 (unsigned int)Data->ulUVCRC[0],
                 (unsigned int)Data->ulYCRC[1],
                 (unsigned int)Data->ulUVCRC[1]);

        st_relayfs_write (ST_RELAY_TYPE_CRC,
                          ST_RELAY_SOURCE_VIDEO_MANIFESTOR + Manifestor->RelayfsIndex,
                          (unsigned char*)buffer,
                          strlen(buffer),
                          NULL);
    }
#endif

    Manifestor->DoneCallback (StreamBuffer, Data ? Data->vsyncTime : 0, Data ? (unsigned int)Data->ulStatus : 0);

}
//}}}
