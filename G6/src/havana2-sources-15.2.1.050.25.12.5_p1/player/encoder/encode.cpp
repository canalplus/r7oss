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

#include "encode.h"

#undef TRACE_TAG
#define TRACE_TAG "Encode_c"

unsigned int Encode_c::sEncodeIdGene = 0;

Encode_c::Encode_c(void)
    : Lock()
    , VideoEncodeMemoryProfile()
    , VideoEncodeInputColorFormatForecasted()
    , IsNRT(false) // By default, NRT mode is disabled
    , ListOfStreams(NULL)
    , Next(NULL)
    , BufferManager(NULL)
    , ControlStructurePool(NULL)
    , EncodeID(0)
{
    SE_VERBOSE(group_encoder_stream, "0x%p\n", this);
    OS_InitializeMutex(&Lock);
    GenerateID();

    //Set the VideoEncodeMemoryProfile = EncodeProfileHD to ensure full encode capability (max buffer size in encode_stream)
    SetVideoEncodeMemoryProfile(EncodeProfileHD);

    //Set forecasted input color format to the format provided by SE decoder, enabling memory optimization at preprocessor side
    //SURFACE_FORMAT_VIDEO_422_YUYV at encode input would require allocation of an extra buffer (input size) at video preprocessor output
    SetVideoInputColorFormatForecasted(SURFACE_FORMAT_VIDEO_420_RASTER2B);
}

Encode_c::~Encode_c(void)
{
    SE_VERBOSE(group_encoder_stream, "0x%p\n", this);
    OS_TerminateMutex(&Lock);
}

EncoderStatus_t   Encode_c::RegisterBufferManager(BufferManager_t   BufferManager)
{
    SE_VERBOSE(group_encoder_stream, "Encode 0x%p\n", this);

    if (this->BufferManager != NULL)
    {
        SE_ERROR("Buffer manager already registered for Encode 0x%p\n", this);
        return EncoderError;
    }

    this->BufferManager = BufferManager;
    return EncoderNoError;
}

EncoderStatus_t   Encode_c::AddStream(EncodeStream_t    Stream,
                                      Preproc_t   Preproc,
                                      Coder_t     Coder,
                                      Transporter_t   Transporter)
{
    SE_INFO(group_encoder_stream, ">Encode 0x%p Stream 0x%p Preproc 0x%p Coder 0x%p Transporter 0x%p\n", this, Stream, Preproc, Coder, Transporter);

    if (Stream      == NULL ||
        Preproc     == NULL ||
        Coder       == NULL ||
        Transporter == NULL)
    {
        // Passing NULL pointers in here means someone failed to create something important
        SE_ERROR("Encode 0x%p: Components were not created correctly\n", this);
        return EncoderNoMemory;
    }

    // The next check we must make is that all the Components have constructed correctly
    if (Stream     ->InitializationStatus != EncoderNoError ||
        Preproc    ->InitializationStatus != EncoderNoError ||
        Coder      ->InitializationStatus != EncoderNoError ||
        Transporter->InitializationStatus != EncoderNoError)
    {
        SE_ERROR("Encode 0x%p: Components were not initialized correctly\n", this);
        return EncoderNoMemory;
    }

    /* check that Stream is not already added */
    OS_LockMutex(&Lock);
    for (EncodeStream_t TempStream = ListOfStreams;
         TempStream != NULL;
         TempStream = TempStream->GetNext())
    {
        if (Stream == TempStream)
        {
            OS_UnLockMutex(&Lock);
            SE_ERROR("Encode 0x%p: Stream 0x%p already added\n", this, Stream);
            return EncoderError;
        }
    }
    OS_UnLockMutex(&Lock);


    // Perform early registration of all the components
    RegisterEncoder(Encoder.Encoder, this, Stream, Preproc, Coder, Transporter, Encoder.EncodeCoordinator);
    Stream     ->RegisterEncoder(Encoder.Encoder, this, Stream, Preproc, Coder, Transporter, Encoder.EncodeCoordinator);
    Preproc    ->RegisterEncoder(Encoder.Encoder, this, Stream, Preproc, Coder, Transporter);
    Coder      ->RegisterEncoder(Encoder.Encoder, this, Stream, Preproc, Coder, Transporter);
    Transporter->RegisterEncoder(Encoder.Encoder, this, Stream, Preproc, Coder, Transporter);

    // Manage memory profile
    EncoderStatus_t Status = Stream->ManageMemoryProfile();
    if (Status != EncoderNoError)
    {
        SE_ERROR("Encode 0x%p: Stream 0x%p failed to manage memory profile\n", this, Stream);
        return Status;
    }

    // This call registers the buffer manager with the stream (and worker classes)
    // and provides the opportunity to create any buffer pools that are required
    // (For example the PreprocessorBufferPool and the CodedFrameBufferPool)
    Status = Stream->RegisterBufferManager(BufferManager);
    if (Status != EncoderNoError)
    {
        SE_ERROR("Encode 0x%p: Stream 0x%p failed to register the buffer manager\n", this, Stream);
        return Status;
    }

    // Initialise and create threads
    Status = Stream->StartStream();
    if (Status != EncoderNoError)
    {
        SE_ERROR("Encode 0x%p: Stream 0x%p failed to start\n", this, Stream);
        return Status;
    }

    // Add new encode stream instance to linked list
    OS_LockMutex(&Lock);
    Stream->SetNext(ListOfStreams);
    ListOfStreams = Stream;
    OS_UnLockMutex(&Lock);

    SE_DEBUG(group_encoder_stream, "<Encode 0x%p Stream 0x%p\n", this, Stream);
    return Status;
}

EncoderStatus_t   Encode_c::RemoveStream(EncodeStream_t Stream)
{
    EncoderStatus_t     Status;
    EncodeStream_t      TempStream;

    SE_INFO(group_encoder_stream, ">Encode 0x%p Stream 0x%p\n", this, Stream);

    // Remove encode stream instance from linked list
    OS_LockMutex(&Lock);
    if (ListOfStreams == Stream)
    {
        ListOfStreams = Stream->GetNext();
    }
    else if (ListOfStreams != NULL)
    {
        TempStream = ListOfStreams;

        while (TempStream->GetNext() != NULL)
        {
            if (TempStream->GetNext() == Stream)
            {
                TempStream->SetNext(Stream->GetNext());
                break;
            }

            TempStream = TempStream->GetNext();
        }
    }
    OS_UnLockMutex(&Lock);

    // We started the stream in AddStream, so we are responsible for Stopping the stream
    Status = Stream->StopStream();

    SE_DEBUG(group_encoder_stream, "<Encode 0x%p Stream 0x%p\n", this, Stream);
    return Status;
}

EncoderStatus_t   Encode_c::GenerateID(void)
{
    OS_LockMutex(&Lock);
    EncodeID = sEncodeIdGene++;
    OS_UnLockMutex(&Lock);

    SE_DEBUG(group_encoder_stream, "Encode 0x%p: Id %d\n", this, EncodeID);
    return EncoderNoError;
}

EncoderStatus_t   Encode_c::GetControl(stm_se_ctrl_t     Control,
                                       void             *Data)
{
    switch (Control)
    {
    case STM_SE_CTRL_ENCODE_NRT_MODE:
        *(uint32_t *)Data = IsNRT ? STM_SE_CTRL_VALUE_APPLY : STM_SE_CTRL_VALUE_DISAPPLY;
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE:
        *(VideoEncodeMemoryProfile_t *)Data = GetVideoEncodeMemoryProfile();
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_INPUT_COLOR_FORMAT:
        *(surface_format_t *)Data = GetVideoInputColorFormatForecasted();
        break;

    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_stream, "Encode 0x%p: Not match encode control %u\n", this, Control);
        return EncoderControlNotMatch;
    }

    return EncoderNoError;
}

EncoderStatus_t   Encode_c::GetCompoundControl(stm_se_ctrl_t     Control,
                                               void             *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_stream, "Encode 0x%p: Not match encode control %u\n", this, Control);
        return EncoderControlNotMatch;
    }

    return EncoderNoError;
}

EncoderStatus_t   Encode_c::SetControl(stm_se_ctrl_t     Control,
                                       const void       *Data)
{
    EncoderStatus_t Status = EncoderNoError;

    switch (Control)
    {
    case STM_SE_CTRL_ENCODE_NRT_MODE:
        Status = SetNrtMode(*(uint32_t *)Data);
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE:
        Status = SetMemoryProfile(*(VideoEncodeMemoryProfile_t *)Data);
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_INPUT_COLOR_FORMAT:
        Status = SetInputColorFormat(*(surface_format_t *)Data);
        break;

    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_stream, "Encode 0x%p: Not match encode control %u\n", this, Control);
        return EncoderControlNotMatch;
    }

    return Status;
}

EncoderStatus_t   Encode_c::SetCompoundControl(stm_se_ctrl_t     Control,
                                               const void       *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_stream, "Encode 0x%p: Not match encode control %u\n", this, Control);
        return EncoderControlNotMatch;
    }

    return EncoderNoError;
}

EncoderStatus_t   Encode_c::SetMemoryProfile(VideoEncodeMemoryProfile_t MemoryProfile)
{
    switch (MemoryProfile)
    {
    case STM_SE_CTRL_VALUE_ENCODE_CIF_PROFILE:
    case STM_SE_CTRL_VALUE_ENCODE_SD_PROFILE:
    case STM_SE_CTRL_VALUE_ENCODE_720p_PROFILE:
    case STM_SE_CTRL_VALUE_ENCODE_HD_PROFILE:
        SetVideoEncodeMemoryProfile((VideoEncodeMemoryProfile_t)MemoryProfile);
        break;
    default:
        SE_ERROR("Encode 0x%p: Invalid memory profile %u\n", this, MemoryProfile);
        return EncoderUnsupportedControl;
    }

    SE_INFO(group_encoder_video_coder, "Encode 0x%p: Memory profile set to %u (%s)\n", this, MemoryProfile, StringifyMemoryProfile(MemoryProfile));
    return EncoderNoError;
}

EncoderStatus_t   Encode_c::SetInputColorFormat(surface_format_t InputColorFormat)
{
    switch (InputColorFormat)
    {
    case SURFACE_FORMAT_UNKNOWN:
    case SURFACE_FORMAT_MARKER_FRAME:
    case SURFACE_FORMAT_AUDIO:
    case SURFACE_FORMAT_VIDEO_420_MACROBLOCK:
    case SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK:
        goto unsupported_color_format;
    case SURFACE_FORMAT_VIDEO_422_RASTER:
        SetVideoInputColorFormatForecasted((surface_format_t)InputColorFormat);
        break;
    case SURFACE_FORMAT_VIDEO_420_PLANAR:
    case SURFACE_FORMAT_VIDEO_420_PLANAR_ALIGNED:
    case SURFACE_FORMAT_VIDEO_422_PLANAR:
    case SURFACE_FORMAT_VIDEO_8888_ARGB:
    case SURFACE_FORMAT_VIDEO_888_RGB:
    case SURFACE_FORMAT_VIDEO_565_RGB:
        goto unsupported_color_format;
    case SURFACE_FORMAT_VIDEO_422_YUYV:
    case SURFACE_FORMAT_VIDEO_420_RASTER2B:
        SetVideoInputColorFormatForecasted((surface_format_t)InputColorFormat);
        break;
    default:
        SE_ERROR("Encode 0x%p: Invalid input color format %u\n", this, InputColorFormat);
        return EncoderUnsupportedControl;
    }

    SE_INFO(group_encoder_video_coder, "Encode 0x%p: Input color format set to %u (%s)\n", this, InputColorFormat, StringifySurfaceFormat(InputColorFormat));
    return EncoderNoError;

unsupported_color_format:
    SE_ERROR("Encode 0x%p: Unsupported input color format %u (%s)\n", this, InputColorFormat, StringifySurfaceFormat(InputColorFormat));
    return EncoderUnsupportedControl;
}

EncoderStatus_t   Encode_c::SetNrtMode(uint32_t NrtMode)
{
    EncoderStatus_t Status = EncoderNoError;

    if (NrtMode == STM_SE_CTRL_VALUE_APPLY)
    {
        // First check there is no encode stream already connected
        // On-the-fly change of NRT mode is not supported
        EncodeStream_t    Stream;
        OS_LockMutex(&Lock);
        for (Stream = ListOfStreams; Stream != NULL; Stream = Stream->GetNext())
        {
            if (Stream->IsConnected())
            {
                // At least one encode stream is already attached to a play stream
                SE_ERROR("Encode 0x%p: Cannot enable NRT mode because Stream 0x%p is already attached\n", this, Stream);
                Status = EncoderBusy;
                break;
            }
        }
        OS_UnLockMutex(&Lock);
        if (Status == EncoderNoError)
        {
            SE_INFO(group_encoder_stream, "Encode 0x%p: NRT mode enabled\n", this);
            IsNRT = true;
        }
    }
    else if (NrtMode == STM_SE_CTRL_VALUE_DISAPPLY)
    {
        SE_INFO(group_encoder_stream, "Encode 0x%p: NRT mode disabled\n", this);
        IsNRT = false;
    }
    else
    {
        SE_ERROR("Encode 0x%p: Invalid NRT mode %d\n", this, NrtMode);
        Status = EncoderUnsupportedControl;
    }

    return Status;
}

VideoEncodeMemoryProfile_t  Encode_c::GetVideoEncodeMemoryProfile(void)
{
    SE_DEBUG(group_encoder_stream, "Encode 0x%p: %u\n", this, VideoEncodeMemoryProfile);
    return VideoEncodeMemoryProfile;
}

void                Encode_c::SetVideoEncodeMemoryProfile(VideoEncodeMemoryProfile_t MemoryProfile)
{
    SE_DEBUG(group_encoder_stream, "Encode 0x%p: %u\n", this, MemoryProfile);
    VideoEncodeMemoryProfile = MemoryProfile;
}

surface_format_t        Encode_c::GetVideoInputColorFormatForecasted(void)
{
    SE_DEBUG(group_encoder_stream, "Encode 0x%p: %u\n", this, VideoEncodeInputColorFormatForecasted);
    return VideoEncodeInputColorFormatForecasted;
}

void                Encode_c::SetVideoInputColorFormatForecasted(surface_format_t ForecastedFormat)
{
    SE_DEBUG(group_encoder_stream, "Encode 0x%p: %u\n", this, ForecastedFormat);
    VideoEncodeInputColorFormatForecasted = ForecastedFormat;
}

Encode_t    Encode_c::GetNext(void)
{
    return Next;
}

void    Encode_c::SetNext(Encode_t Encode)
{
    Next = Encode;
}

EncoderStatus_t   Encode_c::LowPowerEnter(void)
{
    SE_DEBUG(group_encoder_stream, ">Encode 0x%p\n", this);

    // Call Encode streams method
    OS_LockMutex(&Lock);
    for (EncodeStream_t Stream = ListOfStreams; Stream != NULL; Stream = Stream->GetNext())
    {
        // TBD: call Stream->Drain() before entering low power state (when method will be available)
        Stream->LowPowerEnter();
    }
    OS_UnLockMutex(&Lock);

    SE_DEBUG(group_encoder_stream, "<Encode 0x%p\n", this);
    return EncoderNoError;
}

EncoderStatus_t   Encode_c::LowPowerExit(void)
{
    SE_DEBUG(group_encoder_stream, ">Encode 0x%p\n", this);

    // Call Encode streams method
    OS_LockMutex(&Lock);
    for (EncodeStream_t Stream = ListOfStreams; Stream != NULL; Stream = Stream->GetNext())
    {
        Stream->LowPowerExit();
    }
    OS_UnLockMutex(&Lock);

    SE_DEBUG(group_encoder_stream, "<Encode 0x%p\n", this);
    return EncoderNoError;
}
