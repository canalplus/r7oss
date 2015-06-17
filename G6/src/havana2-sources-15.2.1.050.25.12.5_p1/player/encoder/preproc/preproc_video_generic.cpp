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

#include "preproc_video_generic.h"

#undef TRACE_TAG
#define TRACE_TAG "Preproc_Video_Generic_c"

Preproc_Video_Generic_c::Preproc_Video_Generic_c(void)
    : Preproc_Video_c()
    , PreprocVideoDelegate(NULL)
    , ControlLock()
{
    OS_InitializeMutex(&ControlLock);
}

Preproc_Video_Generic_c::~Preproc_Video_Generic_c(void)
{
    Halt();

    OS_TerminateMutex(&ControlLock);

    if (PreprocVideoDelegate != NULL)
    {
        PreprocVideoDelegate->DeRegisterPreprocVideo(this);
        delete PreprocVideoDelegate;
    }
}

PreprocStatus_t Preproc_Video_Generic_c::Halt()
{
    // Halt early to prevent new input;
    PreprocStatus_t Status = Preproc_Video_c::Halt();

    // Try our delegate class to Halt();
    if (PreprocVideoDelegate != NULL)
    {
        Status = PreprocVideoDelegate->Halt();
        if (Status != PreprocNoError)
        {
            SE_ERROR("Failed to halt delegate class\n");
            // continue
        }
    }

    return Status;
}

PreprocStatus_t Preproc_Video_Generic_c::Input(Buffer_t   Buffer)
{
    PreprocStatus_t Status;
    SE_DEBUG(group_encoder_video_preproc, "\n");

    if (PreprocVideoDelegate == NULL)
    {
        OS_LockMutex(&ControlLock);

        // Find PreprocVideoDelegate
        PreprocVideoDelegate = FindPreprocVideo(Buffer);
        if (PreprocVideoDelegate == NULL)
        {
            SE_ERROR("Failed to find suitable preproc\n");
            OS_UnLockMutex(&ControlLock);
            return PreprocError;
        }

        PreprocVideoDelegate->RegisterEncoder(Encoder.Encoder,
                                              Encoder.Encode,
                                              Encoder.EncodeStream,
                                              PreprocVideoDelegate,
                                              Encoder.Coder,
                                              Encoder.Transporter,
                                              Encoder.EncodeCoordinator);

        // Register PreprocVideoDelegate
        Status = PreprocVideoDelegate->RegisterPreprocVideo(this);
        if (Status != PreprocNoError)
        {
            SE_ERROR("Failed to register delegate preproc, Status = %u\n", Status);
            OS_UnLockMutex(&ControlLock);

            PreprocVideoDelegate->DeRegisterPreprocVideo(this);

            delete PreprocVideoDelegate;
            // reset delegate to default
            PreprocVideoDelegate = NULL;

            return PreprocError;
        }

        OS_UnLockMutex(&ControlLock);
    }

    Status = PreprocVideoDelegate->Input(Buffer);
    if (Status != PreprocNoError)
    {
        // Any error in the delegate must be propagated to the caller
        SE_ERROR("Preproc video delegate input failed\n");
        return Status;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Generic_c::GetControl(stm_se_ctrl_t  Control,
                                                    void          *Data)
{
    PreprocStatus_t Status;
    OS_LockMutex(&ControlLock);

    if (PreprocVideoDelegate != NULL)
    {
        Status = PreprocVideoDelegate->GetControl(Control, Data);
        // Any error in the delegate must be propagated to the caller
        OS_UnLockMutex(&ControlLock);
        return Status;
    }

    Status = Preproc_Video_c::GetControl(Control, Data);
    OS_UnLockMutex(&ControlLock);
    return Status;
}

PreprocStatus_t Preproc_Video_Generic_c::GetCompoundControl(stm_se_ctrl_t  Control,
                                                            void          *Data)
{
    PreprocStatus_t Status;
    OS_LockMutex(&ControlLock);

    if (PreprocVideoDelegate != NULL)
    {
        Status = PreprocVideoDelegate->GetCompoundControl(Control, Data);
        // Any error in the delegate must be propagated to the caller
        OS_UnLockMutex(&ControlLock);
        return Status;
    }

    Status = Preproc_Video_c::GetCompoundControl(Control, Data);
    OS_UnLockMutex(&ControlLock);
    return Status;
}

PreprocStatus_t Preproc_Video_Generic_c::SetControl(stm_se_ctrl_t  Control,
                                                    const void    *Data)
{
    PreprocStatus_t Status;
    OS_LockMutex(&ControlLock);

    if (PreprocVideoDelegate != NULL)
    {
        Status = PreprocVideoDelegate->SetControl(Control, Data);
        // Any error in the delegate must be propagated to the caller
        OS_UnLockMutex(&ControlLock);
        return Status;
    }

    Status = Preproc_Video_c::SetControl(Control, Data);
    OS_UnLockMutex(&ControlLock);
    return Status;
}

PreprocStatus_t Preproc_Video_Generic_c::SetCompoundControl(stm_se_ctrl_t  Control,
                                                            const void    *Data)
{
    PreprocStatus_t Status;
    OS_LockMutex(&ControlLock);

    if (PreprocVideoDelegate != NULL)
    {
        Status = PreprocVideoDelegate->SetCompoundControl(Control, Data);
        // Any error in the delegate must be propagated to the caller
        OS_UnLockMutex(&ControlLock);
        return Status;
    }

    Status = Preproc_Video_c::SetCompoundControl(Control, Data);
    OS_UnLockMutex(&ControlLock);
    return Status;
}

PreprocStatus_t Preproc_Video_Generic_c::InjectDiscontinuity(stm_se_discontinuity_t  Discontinuity)
{
    PreprocStatus_t Status;

    // First check that discontinuity is applicable to video
    if ((Discontinuity & STM_SE_DISCONTINUITY_MUTE)
        || (Discontinuity & STM_SE_DISCONTINUITY_FADEOUT)
        || (Discontinuity & STM_SE_DISCONTINUITY_FADEIN))
    {
        SE_ERROR("Discontinuity MUTE/FADEOUT/FADEIN request not applicable to video\n");
        return EncoderNotSupported;
    }

    if (PreprocVideoDelegate != NULL)
    {
        Status = PreprocVideoDelegate->InjectDiscontinuity(Discontinuity);
        if (Status != PreprocNoError)
        {
            // Any error in the delegate must be propagated to the caller
            SE_ERROR("Preproc video delegate failed to inject discontinuity\n");
            return Status;
        }
    }
    else
    {
        // Preproc delegate not created yet, generate discontinuity anyway
        Status = Preproc_Base_c::InjectDiscontinuity(Discontinuity);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Unable to inject discontinuity\n");
            return PREPROC_STATUS_RUNNING(Status);
        }

        if (Discontinuity)
        {
            // Generate discontinuity Buffer
            Status = GenerateBufferDiscontinuity(Discontinuity);
            if (Status != PreprocNoError)
            {
                PREPROC_ERROR_RUNNING("Unable to insert discontinuity\n");
                return PREPROC_STATUS_RUNNING(Status);
            }
        }
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Generic_c::RegisterOutputBufferRing(Ring_t  Ring)
{
    OutputRing = Ring;
    return PreprocNoError;
}

Preproc_Video_t Preproc_Video_Generic_c::FindPreprocVideo(Buffer_t Buffer)
{
    PreprocVideoCaps_t Caps;
    Preproc_Video_t    PreprocVideo = NULL;

    // Retrieve input metadata
    Buffer->ObtainMetaDataReference(InputMetaDataBufferType, (void **)(&InputMetaDataDescriptor));
    SE_ASSERT(InputMetaDataDescriptor != NULL);

    // Get capability of video preproc and see which one can support
    // First match down the priority list wins
    // Try Video Scaler
    Caps = Preproc_Video_Scaler_c::GetCapabilities();

    if (AreCapabilitiesMatching(InputMetaDataDescriptor, &Caps))
    {
        PreprocVideo = new class Preproc_Video_Scaler_c;
        if (!PreprocVideo)
        {
            SE_ERROR("Preproc_Video_Scaler_c object creation failed\n");
            return PreprocVideo;
        }

        if (PreprocVideo->InitializationStatus != PreprocNoError)
        {
            SE_ERROR("Preproc_Video_Scaler_c failed to initialise\n");
            delete PreprocVideo;
            PreprocVideo = NULL;
        }

        return PreprocVideo;
    }

    // Try Video Blitter
    Caps = Preproc_Video_Blitter_c::GetCapabilities();

    if (AreCapabilitiesMatching(InputMetaDataDescriptor, &Caps))
    {
        PreprocVideo = new class Preproc_Video_Blitter_c;
        if (!PreprocVideo)
        {
            SE_ERROR("Preproc_Video_Blitter_c object creation failed\n");
            return PreprocVideo;
        }

        if (PreprocVideo->InitializationStatus != PreprocNoError)
        {
            SE_ERROR("Preproc_Video_Blitter_c failed to initialise\n");
            delete PreprocVideo;
            PreprocVideo = NULL;
        }

        return PreprocVideo;
    }

    SE_ERROR("Neither scaler nor blitter match requested capabilities\n");
    return PreprocVideo;
}

bool Preproc_Video_Generic_c::AreCapabilitiesMatching(stm_se_uncompressed_frame_metadata_t *MetaDataDescriptor,
                                                      PreprocVideoCaps_t *Caps)
{
    // Check capabilties on latched control only
    if (!Caps->DEISupport && (PreprocCtrl.EnableDeInterlacer != STM_SE_CTRL_VALUE_DISAPPLY))
    {
        return false;
    }

    if (!Caps->TNRSupport && (PreprocCtrl.EnableNoiseFilter != STM_SE_CTRL_VALUE_DISAPPLY))
    {
        return false;
    }

    // Check capabilties on metadata
    // If no surface format is specified, true is returned
    if (!(Caps->VideoFormatCaps & (1 << MetaDataDescriptor->video.surface_format)) && MetaDataDescriptor->video.surface_format)
    {
        return false;
    }

    return true;
}
