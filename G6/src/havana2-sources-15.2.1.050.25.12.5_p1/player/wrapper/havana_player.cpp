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

#include "havana_factory.h"
#include "havana_playback.h"
#include "havana_stream.h"
#include "havana_display.h"
#include "mixer_mme.h"
#include "mixer_transformer.h"

#include "audio_reader.h"
#include "audio_generator.h"
#include "havana_player.h"

#undef TRACE_TAG
#define TRACE_TAG   "HavanaPlayer_c"

//{{{  HavanaPlayer_c
HavanaPlayer_c::HavanaPlayer_c(void)
    : FactoryLists()
    , NoFactoryLists(0)
    , LockFactoryList()
    , Playback()
    , Display()
    , AttachedStreams()
    , mMixer()
    , Audio_Player()
    , AudioReader()
    , AudioGenerator()
    , BufferManager(NULL)
    , Player(NULL)
    , Lock()
{
    SE_VERBOSE(group_havana, "\n");

    OS_InitializeMutex(&Lock);
    OS_InitializeMutex(&LockFactoryList);

    for (int i = 0; i < MAX_FACTORY_LISTS; i++)
    {
        FactoryLists[i].FactoryId = INVALID_FACTORY_ID;
    }
}
//}}}
//{{{  ~HavanaPlayer_c
HavanaPlayer_c::~HavanaPlayer_c(void)
{
    int i;
    int j;
    SE_VERBOSE(group_havana, "\n");

    for (i = 0; i < (MAX_PLAYBACKS * MAX_STREAMS_PER_PLAYBACK); i++)
    {
        if (AttachedStreams[i][STM_SE_MEDIA_AUDIO] != NULL)
        {
            OS_LockMutex(&Lock);
            for (j = 0; j < MAX_AUDIO_READERS; j++)
            {
                if (AudioReader[j])
                {
                    SE_ERROR("Audio Stream %d still attached; forcing audio reader %d detach\n", i, j);
                    AudioReader[j]->Detach(AttachedStreams[i][STM_SE_MEDIA_AUDIO]);
                }
            }
            OS_UnLockMutex(&Lock);
        }
        if (AttachedStreams[i][STM_SE_MEDIA_VIDEO] != NULL)
        {
            SE_ERROR("Video Stream %d still connected;\n", i);
        }
    }

    for (i = 0; i < MAX_PLAYBACKS; i++)
    {
        delete Playback[i];
    }

    for (i = 0; i < MAX_DISPLAYS; i++)
    {
        delete Display[i];
    }

    for (i = 0; i < MAX_MIXERS; i++)
    {
        if (mMixer.Any[i] && (mMixer.Any[i]->IsDisconnected() != PlayerNoError))
        {
            OS_LockMutex(&Lock);
            for (j = 0; j < MAX_AUDIO_PLAYERS; j++)
            {
                if (Audio_Player[j])
                {
                    SE_ERROR("Mixer %d still connected; forcing audio player %d detach\n", i, j);
                    mMixer.Any[i]->SendDetachPlayer(Audio_Player[j]);
                }
            }
            for (j = 0; j < (MAX_AUDIO_GENERATORS + MAX_INTERACTIVE_AUDIO); j++)
            {
                if (AudioGenerator[j])
                {
                    SE_ERROR("Mixer %d still connected; forcing audio generator %d detach\n", i, j);
                    AudioGenerator[j]->DetachMixer(mMixer.Any[i]);
                }
            }
            OS_UnLockMutex(&Lock);
        }
        delete mMixer.Any[i];
    }

    for (i = 0; i < MAX_AUDIO_PLAYERS; i++)
    {
        delete Audio_Player[i];
    }

    for (i = 0; i < MAX_AUDIO_READERS; i++)
    {
        delete AudioReader[i];
    }

    for (i = 0; i < (MAX_AUDIO_GENERATORS + MAX_INTERACTIVE_AUDIO); i++)
    {
        delete AudioGenerator[i];
    }

    delete BufferManager;
    delete Player;

    for (i = 0; i < MAX_FACTORY_LISTS; i++)
    {
        class HavanaFactory_c  *FactoryList     = FactoryLists[i].FactoryList;

        while (FactoryList != NULL)
        {
            class  HavanaFactory_c *Factory = FactoryList;
            FactoryList     = Factory->Next();
            delete Factory;
        }

        FactoryLists[i].FactoryId       = INVALID_FACTORY_ID;
        FactoryLists[i].FactoryList     = NULL;
    }

    OS_TerminateMutex(&LockFactoryList);
    OS_TerminateMutex(&Lock);
}
//}}}
//{{{  Init
HavanaStatus_t HavanaPlayer_c::Init(void)
{
    HavanaStatus_t Status = HavanaNoError;
    SE_VERBOSE(group_havana, "\n");

    if (Player == NULL)
    {
        Player = new Player_Generic_c();
    }

    if (Player == NULL)
    {
        SE_ERROR("Unable to create player - insufficient memory\n");
        Status = HavanaNoMemory;
        goto failp;
    }

    if (BufferManager == NULL)
    {
        BufferManager   = new BufferManager_Generic_c();
    }

    if (BufferManager == NULL)
    {
        SE_ERROR("Unable to create buffer manager - insufficient memory\n");
        Status = HavanaNoMemory;
        goto failb;
    }

    Player->RegisterBufferManager(BufferManager);

    //Player->SpecifySignalledEvents (PlayerAllPlaybacks, PlayerAllStreams, EventAllEvents); // Must be done by every stream
    return Status;

failb:
    delete BufferManager;
    BufferManager = NULL;
failp:
    delete Player;
    Player = NULL;
    return Status;
}
//}}}

//{{{  CallFactory
HavanaStatus_t HavanaPlayer_c::CallFactory(PlayerComponent_t       Component,
                                           PlayerStreamType_t      StreamType,
                                           stm_se_stream_encoding_t Encoding,
                                           void                  **Class)
{
    const char                 *Name;
    unsigned int                FactoryId       = (unsigned int)Component + (unsigned int)StreamType;
    SE_VERBOSE(group_havana, "\n");

    for (unsigned int List = 0; List < MAX_FACTORY_LISTS; List++)
    {
        if (FactoryLists[List].FactoryId == FactoryId)
        {
            class HavanaFactory_c      *Factory;
            class HavanaFactory_c      *Previous        = FactoryLists[List].FactoryList;
            // Protect the access to the FactoryList against concurrent playback acting upon it
            OS_LockMutex(&LockFactoryList);

            for (Factory = FactoryLists[List].FactoryList; Factory != NULL; Factory = Factory->Next())
            {
                if (Factory->CanBuild(Component, StreamType, Encoding))
                {
                    if (Factory != FactoryLists[List].FactoryList)
                    {
                        Previous->ReLink(Factory->Next());                      // Move this factory to the head of the list
                        Factory->ReLink(FactoryLists[List].FactoryList);
                        FactoryLists[List].FactoryList  = Factory;
                    }

                    break;
                }

                Previous        = Factory;
            }

            OS_UnLockMutex(&LockFactoryList);

            if (Factory != NULL)
            {
                return Factory->Build(Class);
            }
        }
        else if (FactoryLists[List].FactoryId == INVALID_FACTORY_ID)
        {
            break;
        }
    }

    switch (Component)
    {
    case ComponentCollator:                 Name = "Collator"; break;
    case ComponentFrameParser:              Name = "Frame parser"; break;
    case ComponentCodec:                    Name = "Codec"; break;
    case ComponentManifestor:               Name = "Manifestor"; break;
    case ComponentOutputTimer:              Name = "Output timer"; break;
    case ComponentDecodeBufferManager:      Name = "Decode buffer manager"; break;
    case ComponentManifestationCoordinator: Name = "Manifestation Coordinator"; break;
    default:                                Name = "Unknown component"; break;
    }

    SE_ERROR("No factory found for %s, %x\n", Name, Encoding);
    return HavanaNoFactory;
}
//}}}
//{{{  RegisterFactory
HavanaStatus_t HavanaPlayer_c::RegisterFactory(PlayerComponent_t       Component,
                                               PlayerStreamType_t      StreamType,
                                               stm_se_stream_encoding_t Encoding,
                                               unsigned int            Version,
                                               bool                    Force,
                                               void *(*NewFactory)(void))
{
    HavanaStatus_t              HavanaStatus;
    class  HavanaFactory_c     *Factory;
    unsigned int                List;
    unsigned int                FactoryId       = (unsigned int)Component + (unsigned int)StreamType;

    for (List = 0; List < MAX_FACTORY_LISTS; List++)
    {
        if (FactoryLists[List].FactoryId == FactoryId)
        {
            for (Factory = FactoryLists[List].FactoryList; Factory != NULL; Factory = Factory->Next())
            {
                if (Factory->CanBuild(Component, StreamType, Encoding))
                {
                    if ((Version > Factory->Version()) || Force)
                    {
                        SE_INFO(group_havana, "New factory version %d supercedes previously registered for %x version %d\n",
                                Version, Component, Factory->Version());
                        break;
                    }
                    else
                    {
                        SE_ERROR("New factory version %d does not supercede previously registered for %x version %d\n",
                                 Version, Component, Factory->Version());
                        return HavanaError;
                    }
                }
            }

            break;
        }
        else if (FactoryLists[List].FactoryId == INVALID_FACTORY_ID)
        {
            break;
        }
    }

    if (List == MAX_FACTORY_LISTS)
    {
        SE_ERROR("Unable to create new factory list - too many components\n");
        return HavanaError;
    }

    FactoryLists[List].FactoryId        = FactoryId;
    Factory                             = new HavanaFactory_c();

    if (Factory == NULL)
    {
        SE_ERROR("Unable to create new factory - insufficient memory\n");
        return HavanaNoMemory;
    }

    HavanaStatus        = Factory->Init(FactoryLists[List].FactoryList, Component, StreamType, Encoding, Version, NewFactory);

    if (HavanaStatus != HavanaNoError)
    {
        SE_ERROR("Unable to create Factory  for component %x Encoding %x (%x)\n", Component, Encoding, HavanaStatus);
        delete Factory;
        Factory = NULL;
        return HavanaStatus;
    }

    FactoryLists[List].FactoryList      = Factory;
    return HavanaNoError;
}
//}}}

//{{{  GetManifestor
//{{{  doxynote
/// \brief  access a manifestor - or create a new one if it doesn't exist
/// \return Havana status code, HavanaNoError indicates success.
//}}}
HavanaStatus_t HavanaPlayer_c::GetManifestor(class HavanaStream_c           *Stream,
                                             unsigned int                    Capability,
                                             stm_object_h                    DisplayHandle,
                                             class Manifestor_c            **Manifestor,
                                             stm_se_sink_input_port_t        input_port)
{
    HavanaStatus_t      Status          = HavanaNoError;
    HavanaDisplay_c    *HavanaDisplay   = NULL;
    unsigned int        i;
    PlayerStreamType_t  StreamType      = Stream->StreamType();

    SE_VERBOSE(group_havana, "StreamType %x Capability %x DisplayHandle %p\n",
               StreamType, Capability, DisplayHandle);

    for (i = 0; i < MAX_DISPLAYS; i++)
    {
        if ((Display[i] != NULL) && Display[i]->Owns(DisplayHandle, Stream))
        {
            HavanaDisplay   = Display[i];
            break;
        }
    }

    if (HavanaDisplay == NULL)
    {
        //
        // No - Create a new one
        //
        Status      = CreateDisplay(&HavanaDisplay);

        if (Status != HavanaNoError)
        {
            return Status;
        }

        SE_INFO(group_havana, "No display so create one StreamType %x Capability %x HavanaDisplay %p\n"
                , StreamType, Capability, HavanaDisplay);
    }
    else
    {
        SE_INFO(group_havana, "Reusing a display because one already exists StreamType %x Capability %x HavanaDisplay %p\n"
                , StreamType, Capability, HavanaDisplay);
    }

    Status      = HavanaDisplay->GetManifestor(this, Stream, Capability, DisplayHandle, Manifestor, input_port);

    if (Status != HavanaNoError)
    {
        DeleteDisplay(Stream, DisplayHandle);
    }

    SE_DEBUG(group_havana, "Manifestor = %p\n", *Manifestor);
    return Status;
}

HavanaStatus_t HavanaPlayer_c::DeleteManifestor(class HavanaStream_c *Stream, stm_object_h DisplayHandle)
{
    HavanaStatus_t      Status          = HavanaNoError;
    HavanaDisplay_c    *HavanaDisplay   = NULL;
    unsigned int        i;
    PlayerStreamType_t  StreamType      = Stream->StreamType();

    SE_VERBOSE(group_havana, "Stream %p: StreamType %x DisplayHandle %p\n",
               Stream, StreamType, DisplayHandle);

    for (i = 0; i < MAX_DISPLAYS; i++)
    {
        if ((Display[i] != NULL) && Display[i]->Owns(DisplayHandle, Stream))
        {
            HavanaDisplay   = Display[i];
            break;
        }
    }

    if (HavanaDisplay == NULL)
    {
        SE_ERROR("No Display found\n");
        return HavanaError;
    }

    // GetManifestor() creates a Display, therefore DeleteManifestor() deletes it
    DeleteDisplay(Stream, DisplayHandle);
    return Status;
}
//}}}

//{{{  GetBufferManager
//{{{  doxynote
/// \brief  Obtain a pointer to the BufferManager created and used by the Player Object
/// \return Havana status code, HavanaNoError indicates success.
//}}}
HavanaStatus_t HavanaPlayer_c::GetBufferManager(class BufferManager_c **BufferManager)
{
    if (BufferManager != NULL)
    {
        *BufferManager = this->BufferManager;
    }

    return HavanaNoError;
}
//}}}


//{{{  CreatePlayback
HavanaStatus_t HavanaPlayer_c::CreatePlayback(HavanaPlayback_c      **HavanaPlayback)
{
    HavanaStatus_t      HavanaStatus;
    int                 i;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_PLAYBACKS; i++)
    {
        if (Playback[i] == NULL)
        {
            break;
        }
    }

    if (i == MAX_PLAYBACKS)
    {
        SE_ERROR("Unable to create playback context - Too many playbacks\n");
        OS_UnLockMutex(&Lock);
        return HavanaTooManyPlaybacks;
    }

    Playback[i] = new HavanaPlayback_c();

    if (Playback[i] == NULL)
    {
        SE_ERROR("Unable to create playback context - insufficient memory\n");
        OS_UnLockMutex(&Lock);
        return HavanaNoMemory;
    }

    HavanaStatus        = Playback[i]->Init(this, Player, BufferManager);

    if (HavanaStatus != HavanaNoError)
    {
        SE_ERROR("Unable to create playback context %x\n", HavanaStatus);
        delete Playback[i];
        Playback[i]     = NULL;
        OS_UnLockMutex(&Lock);
        return HavanaStatus;
    }

    *HavanaPlayback     = Playback[i];
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  DeletePlayback
HavanaStatus_t HavanaPlayer_c::DeletePlayback(HavanaPlayback_c       *HavanaPlayback)
{
    int     i;

    if (HavanaPlayback == NULL)
    {
        return HavanaPlaybackInvalid;
    }

    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_PLAYBACKS; i++)
    {
        if (Playback[i] == HavanaPlayback)
        {
            break;
        }
    }

    if (i == MAX_PLAYBACKS)
    {
        SE_ERROR("Unable to locate playback for delete\n");
        OS_UnLockMutex(&Lock);
        return HavanaPlaybackInvalid;
    }

    delete Playback[i];
    Playback[i]   = NULL;
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  CreateMixer
HavanaStatus_t HavanaPlayer_c::CreateMixer(const char             *name,
                                           stm_se_mixer_spec_t     topology,
                                           Mixer_Mme_c           **audio_mixer)
{
    int     i;
    SE_VERBOSE(group_havana, "\n");

    int                  max_mixers = 0;
    class Mixer_Mme_c **mixer;

    switch (topology.type)
    {
    // External SE mixer types
    case STM_SE_MIXER_SINGLE_STAGE_MIXING:
        mixer       = (Mixer_Mme_c **) &mMixer.Typed.SingleStageMixer;
        max_mixers  = MAX_SINGLE_STAGE_MIXERS;
        break;

    case STM_SE_MIXER_DUAL_STAGE_MIXING:
        mixer       = (Mixer_Mme_c **) &mMixer.Typed.DualStageMixer;
        max_mixers  = MAX_DUAL_STAGE_MIXERS;
        break;

    // Internal SE mixer type
    case STM_SE_MIXER_BYPASS:
        mixer       = (Mixer_Mme_c **) &mMixer.Typed.BypassMixer;
        max_mixers  = MAX_BYPASS_MIXERS;
        // reset the topology to the definition of bypass :
        topology.nb_max_decoded_audio     = 1;
        topology.nb_max_application_audio = 0;
        topology.nb_max_interactive_audio = 0;
        break;

    default:
        SE_ERROR("No mixer of type %d can be created\n", topology.type);
        return HavanaNoDevice;
    };

    OS_LockMutex(&Lock);

    for (i = 0; i < max_mixers; i++)
    {
        if (mixer[i] != NULL)
        {
            if (strcmp(name, mixer[i]->Name) == 0)
            {
                SE_ERROR("Mixer %s already exists\n", name);
                OS_UnLockMutex(&Lock);
                return HavanaMixerAlreadyExists;
            }
        }
        else
        {
            break;
        }
    }

    if (i == max_mixers)
    {
        SE_ERROR("Too many mixers of type %d already created (max %d)\n", topology.type, i);
        OS_UnLockMutex(&Lock);
        return HavanaTooManyMixers;
    }

    mixer[i] = new Mixer_Mme_c(name, mixerTransformerArray[i].transformerName,
                               sizeof(mixerTransformerArray[i].transformerName),
                               topology);

    if (mixer[i] == NULL)
    {
        SE_ERROR("Unable to create mixer context - insufficient memory\n");
        OS_UnLockMutex(&Lock);
        return HavanaNoMemory;
    }

    *audio_mixer = mixer[i];
    SE_DEBUG(group_havana, "Created mixer %d: %p\n", i, mixer[i]);
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  DeleteMixer
HavanaStatus_t HavanaPlayer_c::DeleteMixer(Mixer_Mme_c          *audio_mixer)
{
    int      i;
    OS_LockMutex(&Lock);

    for (i = 0 ; i < MAX_MIXERS; i++)
    {
        if (mMixer.Any[i] == audio_mixer)
        {
            break;
        }
    }

    if (i == MAX_MIXERS)
    {
        SE_ERROR("Mixer %p does not exist\n", audio_mixer);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    // Mixer exists but need to check if it is busy
    if (mMixer.Any[i]->IsDisconnected() != PlayerNoError)
    {
        SE_ERROR("Mixer %d still connected - cannot be deleted\n", i);
        OS_UnLockMutex(&Lock);
        return HavanaError;
    }

    SE_DEBUG(group_havana, "Delete mixer %d: %p\n", i, mMixer.Any[i]);
    delete mMixer.Any[i];
    mMixer.Any[i] = NULL;
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}

//{{{ Create audio generator
HavanaStatus_t HavanaPlayer_c::CreateAudioGenerator(const char             *name,
                                                    Audio_Generator_c     **audio_generator)
{
    int AudioGenIdx;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (AudioGenIdx = 0; AudioGenIdx < (MAX_AUDIO_GENERATORS + MAX_INTERACTIVE_AUDIO); AudioGenIdx++)
    {
        if (AudioGenerator[AudioGenIdx] != NULL)
        {
            if (strncmp(name, AudioGenerator[AudioGenIdx]->Name, sizeof(AudioGenerator[AudioGenIdx]->Name)) == 0)
            {
                SE_ERROR("Audio generator %s already exists\n", name);
                OS_UnLockMutex(&Lock);
                return HavanaGeneratorAlreadyExists;
            }
        }
        else
        {
            break;
        }
    }

    if (AudioGenIdx == (MAX_AUDIO_GENERATORS + MAX_INTERACTIVE_AUDIO))
    {
        SE_ERROR("Too many audio generators already created (max %d)\n", AudioGenIdx);
        OS_UnLockMutex(&Lock);
        return HavanaTooManyGenerators;
    }

    AudioGenerator[AudioGenIdx] = new Audio_Generator_c(name);

    if (AudioGenerator[AudioGenIdx] == NULL)
    {
        SE_ERROR("Unable to create audio generator context - insufficient memory\n");
        OS_UnLockMutex(&Lock);
        return HavanaNoMemory;
    }

    *audio_generator = AudioGenerator[AudioGenIdx];
    SE_DEBUG(group_havana, "Created audio generator %d: %p\n", AudioGenIdx, AudioGenerator[AudioGenIdx]);
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  DeleteGenerator
HavanaStatus_t HavanaPlayer_c::DeleteAudioGenerator(Audio_Generator_c          *audio_generator)
{
    int AudioGenIdx;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (AudioGenIdx = 0; AudioGenIdx < (MAX_AUDIO_GENERATORS + MAX_INTERACTIVE_AUDIO); AudioGenIdx++)
    {
        if (AudioGenerator[AudioGenIdx] == audio_generator)
        {
            break;
        }
    }

    if (AudioGenIdx == (MAX_AUDIO_GENERATORS + MAX_INTERACTIVE_AUDIO))
    {
        SE_ERROR("Audio generator %p does not exist\n", audio_generator);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    SE_DEBUG(group_havana, "Delete audio generator %d: %p\n", AudioGenIdx, AudioGenerator[AudioGenIdx]);
    delete AudioGenerator[AudioGenIdx];
    AudioGenerator[AudioGenIdx] = NULL;
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  AttachGeneratorToMixer
HavanaStatus_t HavanaPlayer_c::AttachGeneratorToMixer(Audio_Generator_c     *audio_generator,
                                                      Mixer_Mme_c           *audio_mixer)
{
    int AudioGenIdx, AudioMixerIdx;
    PlayerStatus_t      Status;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (AudioGenIdx = 0; AudioGenIdx < (MAX_AUDIO_GENERATORS + MAX_INTERACTIVE_AUDIO); AudioGenIdx++)
    {
        if (AudioGenerator[AudioGenIdx] == audio_generator)
        {
            break;
        }
    }

    if (AudioGenIdx == (MAX_AUDIO_GENERATORS + MAX_INTERACTIVE_AUDIO))
    {
        SE_ERROR("Audio generator %p does not exist\n", audio_generator);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    for (AudioMixerIdx = 0; AudioMixerIdx < MAX_MIXERS; AudioMixerIdx++)
    {
        if (mMixer.Any[AudioMixerIdx] == audio_mixer)
        {
            break;
        }
    }

    if (AudioMixerIdx == MAX_MIXERS)
    {
        SE_ERROR("Mixer %p does not exist\n", audio_mixer);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    Status = audio_generator->AttachMixer(audio_mixer);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Error attaching mixer %d to audio generator %d\n", AudioMixerIdx, AudioGenIdx);
        OS_UnLockMutex(&Lock);
        return HavanaError;
    }

    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  DetachGeneratorFromMixer
HavanaStatus_t HavanaPlayer_c::DetachGeneratorFromMixer(Audio_Generator_c     *audio_generator,
                                                        Mixer_Mme_c           *audio_mixer)
{
    int AudioGenIdx, AudioMixerIdx;
    PlayerStatus_t      Status;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (AudioGenIdx = 0; AudioGenIdx < (MAX_AUDIO_GENERATORS + MAX_INTERACTIVE_AUDIO); AudioGenIdx++)
    {
        if (AudioGenerator[AudioGenIdx] == audio_generator)
        {
            break;
        }
    }

    if (AudioGenIdx == (MAX_AUDIO_GENERATORS + MAX_INTERACTIVE_AUDIO))
    {
        SE_ERROR("Audio generator %p does not exist\n", audio_generator);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    for (AudioMixerIdx = 0; AudioMixerIdx < MAX_MIXERS; AudioMixerIdx++)
    {
        if (mMixer.Any[AudioMixerIdx] == audio_mixer)
        {
            break;
        }
    }

    if (AudioMixerIdx == MAX_MIXERS)
    {
        SE_ERROR("Mixer %p does not exist\n", audio_mixer);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    Status = audio_generator->DetachMixer(audio_mixer);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Error detaching mixer %d from audio generator %d\n", AudioMixerIdx, AudioGenIdx);
        OS_UnLockMutex(&Lock);
        return HavanaError;
    }

    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  UpdateMixerTransformerId
HavanaStatus_t HavanaPlayer_c::UpdateMixerTransformerId(unsigned int     mixerId,
                                                        const char *transformerName)
{
    SE_VERBOSE(group_havana, "\n");
    if (mMixer.Any[mixerId] != NULL)
    {
        mMixer.Any[mixerId]->UpdateTransformerId(transformerName);
        return HavanaNoError;
    }
    else
    {
        return HavanaNotOpen;
    }
}
//}}}
//{{{  CreateAudioPlayer
HavanaStatus_t HavanaPlayer_c::CreateAudioPlayer(const char             *name,
                                                 const char             *hw_name,
                                                 Audio_Player_c        **audio_player)
{
    int     i;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_AUDIO_PLAYERS; i++)
    {
        if (Audio_Player[i] != NULL)
        {
            if (strcmp(hw_name, Audio_Player[i]->Name) == 0)
            {
                SE_ERROR("Audio player %s already exists\n", hw_name);
                OS_UnLockMutex(&Lock);
                return HavanaComponentInvalid;
            }
        }
        else
        {
            break;
        }
    }

    if (i == MAX_AUDIO_PLAYERS)
    {
        SE_ERROR("Too many audio players already created (max %d)\n", i);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    Audio_Player[i] = new Audio_Player_c(name, hw_name);

    if (Audio_Player[i] == NULL)
    {
        SE_ERROR("Unable to create audio player context - insufficient memory\n");
        OS_UnLockMutex(&Lock);
        return HavanaNoMemory;
    }

    *audio_player = Audio_Player[i];
    SE_DEBUG(group_havana, "Created audio player %d: %p\n", i, Audio_Player[i]);
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  DeleteAudioPlayer
HavanaStatus_t HavanaPlayer_c::DeleteAudioPlayer(Audio_Player_c          *audio_player)
{
    int     i;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (i = 0 ; i < MAX_AUDIO_PLAYERS; i++)
    {
        if (Audio_Player[i] == audio_player)
        {
            break;
        }
    }

    if (i == MAX_AUDIO_PLAYERS)
    {
        SE_ERROR("Audio player %p does not exist\n", audio_player);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    SE_DEBUG(group_havana, "Delete audio player %d: %p\n", i, Audio_Player[i]);
    delete Audio_Player[i];
    Audio_Player[i] = NULL;
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  AttachPlayerToMixer
HavanaStatus_t HavanaPlayer_c::AttachPlayerToMixer(Mixer_Mme_c          *audio_mixer,
                                                   Audio_Player_c       *audio_player)
{
    int i, j;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_MIXERS; i++)
    {
        if (mMixer.Any[i] == audio_mixer)
        {
            break;
        }
    }

    if (i == MAX_MIXERS)
    {
        SE_ERROR("Mixer %p does not exist\n", audio_mixer);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    for (j = 0; j < MAX_AUDIO_PLAYERS; j++)
    {
        if (Audio_Player[j] == audio_player)
        {
            break;
        }
    }

    if (j == MAX_AUDIO_PLAYERS)
    {
        SE_ERROR("Audio player %p does not exist\n", audio_player);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    if (PlayerNoError != mMixer.Any[i]->SendAttachPlayer(Audio_Player[j]))
    {
        SE_ERROR("Error attaching mixer %d to player %d\n", i, j);
        OS_UnLockMutex(&Lock);
        return HavanaError;
    }

    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  DetachPlayerFromMixer
HavanaStatus_t HavanaPlayer_c::DetachPlayerFromMixer(Mixer_Mme_c          *audio_mixer,
                                                     Audio_Player_c       *audio_player)
{
    int i, j;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_MIXERS; i++)
    {
        if (mMixer.Any[i] == audio_mixer)
        {
            break;
        }
    }

    if (i == MAX_MIXERS)
    {
        SE_ERROR("Mixer %p does not exist\n", audio_mixer);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    for (j = 0; j < MAX_AUDIO_PLAYERS; j++)
    {
        if (Audio_Player[j] == audio_player)
        {
            break;
        }
    }

    if (j == MAX_AUDIO_PLAYERS)
    {
        SE_ERROR("Audio player %p does not exist\n", audio_player);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    if (PlayerNoError != mMixer.Any[i]->SendDetachPlayer(Audio_Player[j]))
    {
        SE_ERROR("Error detaching player %d (%s) from mixer %d\n", j, Audio_Player[j]->card.alsaname, i);
        OS_UnLockMutex(&Lock);
        return HavanaError;
    }

    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  CreateAudioReader
HavanaStatus_t HavanaPlayer_c::CreateAudioReader(const char           *name,
                                                 const char           *hw_name,
                                                 Audio_Reader_c      **audio_reader)
{
    int      i;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_AUDIO_READERS; i++)
    {
        if (AudioReader[i] != NULL)
        {
            if (strcmp(hw_name, AudioReader[i]->Alsaname) == 0)
            {
                SE_ERROR("Audio reader %s already exists\n", hw_name);
                OS_UnLockMutex(&Lock);
                return HavanaComponentInvalid;
            }
        }
        else
        {
            break;
        }
    }

    if (i == MAX_AUDIO_READERS)
    {
        SE_ERROR("Too many audio readers already created (max %d)\n", i);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    AudioReader[i] = new Audio_Reader_c(name, hw_name);

    if (AudioReader[i] == NULL)
    {
        SE_ERROR("Unable to create audio reader context - insufficient memory\n");
        OS_UnLockMutex(&Lock);
        return HavanaNoMemory;
    }

    *audio_reader = AudioReader[i];
    SE_DEBUG(group_havana, "Created audio reader %d: %p\n", i, AudioReader[i]);
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  DeleteAudioReader
HavanaStatus_t HavanaPlayer_c::DeleteAudioReader(Audio_Reader_c          *audio_reader)
{
    int      i;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (i = 0 ; i < MAX_AUDIO_READERS; i++)
    {
        if (AudioReader[i] == audio_reader)
        {
            break;
        }
    }

    if (i == MAX_AUDIO_READERS)
    {
        SE_ERROR("Audio reader %p does not exist\n", audio_reader);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    SE_DEBUG(group_havana, "Delete audio reader %d: %p\n", i, AudioReader[i]);
    delete AudioReader[i];
    AudioReader[i] = NULL;
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  AttachAudioReader
HavanaStatus_t HavanaPlayer_c::AttachAudioReader(Audio_Reader_c          *audio_reader,
                                                 HavanaStream_c          *play_stream)
{
    int i;
    PlayerStreamType_t      StreamType;
    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_AUDIO_READERS; i++)
    {
        if (AudioReader[i] == audio_reader)
        {
            break;
        }
    }

    if (i == MAX_AUDIO_READERS)
    {
        SE_ERROR("Audio reader %p does not exist\n", audio_reader);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    StreamType = play_stream->StreamType();

    if (StreamType != StreamTypeAudio)
    {
        SE_ERROR("Audio reader can only be attached with audio play stream\n");
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    if (PlayerNoError != AudioReader[i]->Attach(play_stream))
    {
        SE_ERROR("Audio reader to play stream attach failed\n");
        OS_UnLockMutex(&Lock);
        return HavanaError;
    }

    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}

//{{{  DetachAudioReader
HavanaStatus_t HavanaPlayer_c::DetachAudioReader(Audio_Reader_c          *audio_reader,
                                                 HavanaStream_c          *play_stream)
{
    int i;
    PlayerStreamType_t      StreamType;
    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_AUDIO_READERS; i++)
    {
        if (AudioReader[i] == audio_reader)
        {
            break;
        }
    }

    if (i == MAX_AUDIO_READERS)
    {
        SE_ERROR("Audio reader %p does not exist\n", audio_reader);
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    StreamType = play_stream->StreamType();

    if (StreamType != StreamTypeAudio)
    {
        SE_ERROR("Audio reader can only be detached from audio play stream\n");
        OS_UnLockMutex(&Lock);
        return HavanaComponentInvalid;
    }

    if (PlayerNoError != AudioReader[i]->Detach(play_stream))
    {
        SE_ERROR("Audio reader to play stream detach failed\n");
        OS_UnLockMutex(&Lock);
        return HavanaError;
    }

    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}

//{{{  SetOption
HavanaStatus_t HavanaPlayer_c::SetOption(PlayerPolicy_t          Option,
                                         int                     Value)
{
    PlayerStatus_t      Status;
    SE_VERBOSE(group_havana, "%d, %d\n", Option, Value);
    // Value is "known" to be in range because it comes from a lookup table in the wrapper
    Status  = Player->SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, Option, Value);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Unable to set player option %x, %x\n", Option, Value);
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  GetOption
HavanaStatus_t HavanaPlayer_c::GetOption(PlayerPolicy_t          Option,
                                         int                    *Value)
{
    SE_VERBOSE(group_havana, "%x, %p\n", Option, Value);
    // Value is "known" to be in range because it comes from a lookup table in the wrapper
    *Value = Player->PolicyValue(PlayerAllPlaybacks, PlayerAllStreams, Option);
    return HavanaNoError;
}
//}}}
//{{{  CreateDisplay
HavanaStatus_t HavanaPlayer_c::CreateDisplay(HavanaDisplay_c       **HavanaDisplay)
{
    HavanaStatus_t      Status  = HavanaNoError;
    int                 i;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_DISPLAYS; i++)
    {
        if (Display[i] == NULL)
        {
            break;
        }
    }

    if (i == MAX_DISPLAYS)
    {
        SE_ERROR("Unable to create display context - too many displays\n");
        OS_UnLockMutex(&Lock);
        return HavanaTooManyDisplays;
    }

    Display[i]          = new HavanaDisplay_c();

    if (Display[i] == NULL)
    {
        SE_ERROR("Unable to create display context - insufficient memory\n");
        Status          = HavanaNoMemory;
    }

    *HavanaDisplay      = Display[i];
    OS_UnLockMutex(&Lock);
    return Status;
}
//}}}
//{{{  DeleteDisplay
HavanaStatus_t HavanaPlayer_c::DeleteDisplay(class HavanaStream_c *Stream, stm_object_h    DisplayHandle)
{
    int                 i;
    HavanaDisplay_c    *HavanaDisplay  = NULL;
    class Manifestor_c *Manifestor     = NULL;

    SE_VERBOSE(group_havana, "Stream %p DisplayHandle %p\n", Stream, DisplayHandle);

    if (DisplayHandle == NULL || Stream == NULL)
    {
        return HavanaDisplayInvalid;
    }

    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_DISPLAYS; i++)
    {
        if (Display[i] != NULL && Display[i]->Owns(DisplayHandle, Stream))
        {
            HavanaDisplay = Display[i];
            break;
        }
    }

    //remove reference from manifestation co-ordinator before deleting it
    if (HavanaDisplay)
    {
        Manifestor = HavanaDisplay->ReferenceManifestor();

        if (Manifestor)
        {
            Stream->RemoveManifestor(Manifestor);
        }
    }
    else
    {
        SE_ERROR("Unable to locate display for deletion\n");
        OS_UnLockMutex(&Lock);
        return HavanaDisplayInvalid;
    }

    delete Display[i];
    Display[i]   = NULL;

    OS_UnLockMutex(&Lock);

    return HavanaNoError;
}
//}}}

//{{{  AttachStream
//{{{  doxynote
/// \brief Add stream to the current player stream list and retrieve its ID
/// \param Media        Textual description of media (audio or video).
/// \param Stream       Reference to the stream.
/// \param StreamCount  Reference to the ID of the current stream.
/// \return Havana status code, HavanaNoError indicates success.
//}}}
HavanaStatus_t HavanaPlayer_c::AttachStream(class HavanaStream_c   *Stream, stm_se_media_t Media, int     *StreamCount)
{
    int     i;
    // WARNING: we should not lock the HavanaPlayer_c mutex here, as it is already locked by caller HavanaPlayback_c::AddStream()
    OS_AssertMutexHeld(&Lock);
    *StreamCount = 0;

    for (i = 0; i < (MAX_PLAYBACKS * MAX_STREAMS_PER_PLAYBACK); i++)
    {
        if (AttachedStreams[i][Media] == NULL)
        {
            AttachedStreams[i][Media] = Stream;
            break;
        }

        *StreamCount += 1;
    }

    if (i == MAX_PLAYBACKS * MAX_STREAMS_PER_PLAYBACK)
    {
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}

//}}}
//{{{  DetachStream
//{{{  doxynote
/// \brief Remove a stream from the current player stream list
/// \param Media        Textual description of media (audio or video)
/// \param Stream       Reference to the stream.
/// \return Havana status code, HavanaNoError indicates success.
//}}}
HavanaStatus_t HavanaPlayer_c::DetachStream(class HavanaStream_c   *Stream, stm_se_media_t Media)
{
    int     i;
    // WARNING: we should not lock the HavanaPlayer_c mutex here, as it is already locked by caller HavanaPlayback_c::RemoveStream()
    OS_AssertMutexHeld(&Lock);

    for (i = 0; i < (MAX_PLAYBACKS * MAX_STREAMS_PER_PLAYBACK); i++)
    {
        if (AttachedStreams[i][Media] == Stream)
        {
            AttachedStreams[i][Media] = NULL;
            break;
        }
    }

    if (i == MAX_PLAYBACKS * MAX_STREAMS_PER_PLAYBACK)
    {
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}

//{{{  LowPowerEnter
HavanaStatus_t  HavanaPlayer_c::LowPowerEnter(__stm_se_low_power_mode_t low_power_mode)
{
    int i;
    SE_VERBOSE(group_havana, "\n");
    // Lock access to Player objects
    OS_LockMutex(&Lock);
    // Call Player method
    Player->LowPowerEnter(low_power_mode);

    // Call Playbacks method
    for (i = 0; i < MAX_PLAYBACKS; i++)
    {
        if (Playback[i] != NULL)
        {
            Playback[i]->LowPowerEnter();
        }
    }

    // Call Mixers method
    for (i = 0; i < MAX_MIXERS; i++)
    {
        if (mMixer.Any[i] != NULL)
        {
            mMixer.Any[i]->LowPowerEnter(low_power_mode);
        }
    }

    // Unlock access to Player objects
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}

//{{{  LowPowerExit
HavanaStatus_t  HavanaPlayer_c::LowPowerExit(void)
{
    int i;
    SE_VERBOSE(group_havana, "\n");
    // Lock access to Player
    OS_LockMutex(&Lock);

    // Call Playbacks method
    for (i = 0; i < MAX_PLAYBACKS; i++)
    {
        if (Playback[i] != NULL)
        {
            Playback[i]->LowPowerExit();
        }
    }

    // Call Mixers method
    for (i = 0; i < MAX_MIXERS; i++)
    {
        if (mMixer.Any[i] != NULL)
        {
            mMixer.Any[i]->LowPowerExit();
        }
    }

    // Call Player method
    Player->LowPowerExit();
    // Unlock access to Player
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
