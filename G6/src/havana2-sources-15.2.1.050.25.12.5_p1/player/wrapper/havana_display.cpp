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

#include "player.h"
#include "buffer_generic.h"
#include "manifestor_audio.h"
#include "manifestor_video.h"
#include "manifestor_video_grab.h"
#include "manifestor_video_crc.h"
#include "manifestor_encode.h"
#include "manifestor_encode_audio.h"
#include "manifestor_encode_video.h"
#include "manifestor_video_sourceGrab.h"
#include "manifestor_audio_sourceGrab.h"
#include "manifestor_video_push_release_grab.h"

#include "havana_player.h"
#include "havana_stream.h"
#include "havana_factory.h"
#include "havana_display.h"

//{{{  HavanaDisplay_c
HavanaDisplay_c::HavanaDisplay_c(void)
    : Manifestor(NULL)
    , Stream(NULL)
    , PlayerStreamType(StreamTypeOther)
    , DisplayHandle(NULL)

{
    SE_VERBOSE(group_havana, " this = %p\n", this);
    PlayerStreamType    = StreamTypeOther;
}
//}}}
//{{{  ~HavanaDisplay_c
HavanaDisplay_c::~HavanaDisplay_c(void)
{
    SE_VERBOSE(group_havana, "this = %p Manifestor %p\n", this , Manifestor);
    delete Manifestor;
}
//}}}

//{{{  GetManifestor
//{{{  doxynote
/// \brief  access a manifestor - or create a new one if it doesn't exist
/// \return Havana status code, HavanaNoError indicates success.
//}}}
HavanaStatus_t HavanaDisplay_c::GetManifestor(class HavanaPlayer_c           *HavanaPlayer,
                                              class HavanaStream_c           *Stream,
                                              unsigned int                    Capability,
                                              stm_object_h                    DisplayHandle,
                                              class Manifestor_c            **Manifestor,
                                              stm_se_sink_input_port_t        input_port)
{
    HavanaStatus_t              Status          = HavanaNoError;
    bool                        NewManifestor   = false;
    PlayerStreamType_t          StreamType      = Stream->StreamType();

    SE_VERBOSE(group_havana, " Stream %x %p\n", StreamType, this->Manifestor);

    PlayerStreamType            = StreamType;
    this->Stream                = Stream;
    this->DisplayHandle         = DisplayHandle;

    if (this->Manifestor == NULL)
    {
        if ((StreamType == StreamTypeVideo) && (Capability == MANIFESTOR_CAPABILITY_ENCODE))
        {
            this->Manifestor    = new Manifestor_EncodeVideo_c();
        }
        else if ((StreamType == StreamTypeAudio) && (Capability == MANIFESTOR_CAPABILITY_ENCODE))
        {
            this->Manifestor    = new Manifestor_EncodeAudio_c();
        }
        else if ((StreamType == StreamTypeVideo) && (Capability == MANIFESTOR_CAPABILITY_GRAB))
        {
            this->Manifestor    = new Manifestor_VideoGrab_c();
        }
        else if ((StreamType == StreamTypeVideo) && (Capability == MANIFESTOR_CAPABILITY_PUSH_RELEASE))
        {
            this->Manifestor    = new Manifestor_VideoPushRelease_c();
        }
        else if ((StreamType == StreamTypeVideo) && (Capability == MANIFESTOR_CAPABILITY_SOURCE))
        {
            this->Manifestor    = new Manifestor_VideoSrc_c();
        }
        else if ((StreamType == StreamTypeAudio) && (Capability == MANIFESTOR_CAPABILITY_SOURCE))
        {
            this->Manifestor    = new Manifestor_AudioSrc_c();
        }
        //if ((StreamType == StreamTypeVideo) && (Capability == MANIFESTOR_CAPABILITY_CRC))
        //    this->Manifestor    = new Manifestor_VideoCRC_c ();
        else
            Status              = HavanaPlayer->CallFactory(ComponentManifestor,
                                                            PlayerStreamType,
                                                            PlayerStreamType == StreamTypeAudio ? STM_SE_STREAM_ENCODING_AUDIO_NONE : STM_SE_STREAM_ENCODING_VIDEO_NONE,
                                                            (void **)&this->Manifestor);

        if (Status != HavanaNoError)
        {
            SE_ERROR("Failed to create Manifestor\n");
            return HavanaNoMemory;
        }
        else
        {
            NewManifestor       = true;
        }
    }

    if (Capability == MANIFESTOR_CAPABILITY_SOURCE)
    {
        //
        // try to connect to memorySink object
        //
        class Manifestor_Source_c      *SourceManifestor = (class Manifestor_Source_c *)this->Manifestor;
        Status = (SourceManifestor->Connect(Stream, DisplayHandle) == ManifestorNoError ? HavanaNoError : HavanaError);
    }
    else if (Capability == MANIFESTOR_CAPABILITY_ENCODE)
    {
        class Manifestor_Encoder_c    *EncodeManifestor = (class Manifestor_Encoder_c *)this->Manifestor;
        Status = (EncodeManifestor->Connect(Stream, DisplayHandle) == ManifestorNoError ? HavanaNoError : HavanaError);
    }
    else if (Capability == MANIFESTOR_CAPABILITY_PUSH_RELEASE)
    {
        //
        // try to connect to pushSink object (aka zero copy grab)
        //
        class Manifestor_VideoPushRelease_c      *VideoPushReleaseManifestor = (class Manifestor_VideoPushRelease_c *)this->Manifestor;
        Status = (VideoPushReleaseManifestor->Connect(Stream, DisplayHandle) == ManifestorNoError ? HavanaNoError : HavanaError);
    }
    else  if (PlayerStreamType == StreamTypeAudio)
        //{{{  Init audio manifestor
    {
        class Manifestor_Audio_c               *AudioManifestor = (class Manifestor_Audio_c *) this->Manifestor;
        ManifestorStatus_t                      ManifestorStatus;
        ManifestorAudioParameterBlock_t         ManifestorParameters;

        // No need of mixer when grabbing
        if (Capability != MANIFESTOR_CAPABILITY_GRAB)
        {
            ManifestorParameters.ParameterType                      = ManifestorAudioMixerConfiguration;
            ManifestorParameters.Mixer                              = DisplayHandle; // This is the Mixer
            ManifestorStatus = AudioManifestor->SetModuleParameters(sizeof(ManifestorParameters), &ManifestorParameters);
            if (ManifestorStatus != ManifestorNoError)
            {
                SE_ERROR("Failed to set manifestor mixer parameters (%08x)\n", ManifestorStatus);
                delete this->Manifestor;
                this->Manifestor = NULL;
                return HavanaNoMemory;
            }
        }

        // provide the rest of the manifestors configuration
        ManifestorStatus      = AudioManifestor->OpenOutputSurface(Stream, input_port);
        if (ManifestorStatus != ManifestorNoError)
        {
            SE_ERROR("Failed to create audio output surface\n");
            delete this->Manifestor;
            this->Manifestor = NULL;
            return HavanaNoMemory;
        }
    }
    //}}}
    else if (PlayerStreamType == StreamTypeVideo)
        //{{{  Init video manifestor
    {
        class Manifestor_Video_c                *VideoManifestor;
        ManifestorStatus_t                       ManifestorStatus;
        SE_DEBUG(group_havana, " Stream %x %p\n", StreamType, this->Manifestor);
        VideoManifestor                 = (class Manifestor_Video_c *)this->Manifestor;

        if (NewManifestor)
        {
            ManifestorStatus      = VideoManifestor->OpenOutputSurface(DisplayHandle);
            if (ManifestorStatus != ManifestorNoError)
            {
                SE_ERROR("Failed to create video output surface\n");
                delete this->Manifestor;
                this->Manifestor = NULL;
                return HavanaNoMemory;
            }
        }

        //}}}
    }

    //}}}
    *Manifestor = this->Manifestor;
    return Status;
}
//}}}
//{{{  Owns
bool HavanaDisplay_c::Owns(stm_object_h DisplayHandle, class HavanaStream_c   *Stream)
{
    bool own = (this->DisplayHandle == DisplayHandle);
    SE_VERBOSE(group_havana, "\n");

    // keep provision in case the stream info is not relevant (used not to be, until bug18011) ...but just the DisplayHandle
    if (Stream)
    {
        own = own && (this->Stream == Stream);
    }

    return own;
}
//}}}
