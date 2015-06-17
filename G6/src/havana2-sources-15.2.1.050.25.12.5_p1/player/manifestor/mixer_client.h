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

#ifndef H_MIXER_CLIENT_CLASS
#define H_MIXER_CLIENT_CLASS

#include "output_timer_audio.h"
#include "audio_player.h"
#include "player_types.h"
#include "manifestor_audio_ksound.h"
#include "mixer.h"
#include "dtshd.h"
#include "aac_audio.h"
#include "havana_stream.h"

#undef TRACE_TAG
#define TRACE_TAG   "Mixer_Client_c"

class Mixer_Client_c : public Mixer_c
{
public:
    /// Control Parameters exchanged with Input Clients
    struct MixerClientParameters
    {
        OutputTimerParameterBlock_t ParameterBlockOT;
        CodecParameterBlock_t       ParameterBlockDownmix;
    };

    inline Mixer_Client_c(void)
        : Mixer_c()
        , State(DISCONNECTED)
        , Manifestor(NULL)
        , Stream(NULL)
        , Parameters()
        , Muted(false)
        , Lock()
    {
        OS_InitializeMutex(&Lock);
        SE_DEBUG(group_mixer, "<: This:%p Manifestor:%p Stream:%p\n", this, Manifestor, Stream);
    }

    inline ~Mixer_Client_c(void)
    {
        // Expected to be in DISCONNECTED state
        SE_ASSERT(State == DISCONNECTED);
        OS_TerminateMutex(&Lock);
        SE_DEBUG(group_mixer, "<: This:%p Manifestor:%p Stream:%p\n", this, Manifestor, Stream);
    }

    inline void LockTake()
    {
        SE_EXTRAVERB(group_mixer, ">: This:%p Manifestor:%p Stream:%p\n", this, Manifestor, Stream);
        OS_LockMutex(&Lock);
        SE_EXTRAVERB(group_mixer, "<: %p\n", this);
    }

    inline void LockRelease()
    {
        SE_EXTRAVERB(group_mixer, ">: This:%p Manifestor:%p Stream:%p\n", this, Manifestor, Stream);
        OS_UnLockMutex(&Lock);
        SE_EXTRAVERB(group_mixer, "<:\n");
    }

    inline bool IsMyManifestor(const Manifestor_AudioKsound_c *Manifestor) const
    {
        bool Result;

        if (NULL == Mixer_Client_c::Manifestor)
        {
            Result = false;
        }
        else
        {
            Result = (Mixer_Client_c::Manifestor == Manifestor);
        }

        SE_VERBOSE(group_mixer, "Returning %s\n", Result ? "true" : "false");
        return Result;
    }

    inline bool IsMyStream(const HavanaStream_c *Stream) const
    {
        bool Result;

        if (NULL == Mixer_Client_c::Stream)
        {
            Result = false;
        }
        else
        {
            Result = (Mixer_Client_c::Stream == Stream);
        }

        SE_VERBOSE(group_mixer, "Returning %s\n", Result ? "true" : "false");
        return Result;
    }

    inline void SetState(Mixer_c::InputState_t State)
    {
        SE_DEBUG(group_mixer, "This:%p Manifestor:%p Stream:%p (%s) -> %s\n",
                 this, Manifestor, Stream, Mixer_c::LookupInputState(Mixer_Client_c::State), Mixer_c::LookupInputState(State));
        Mixer_Client_c::State = State;
    }

    inline Mixer_c::InputState_t GetState() const
    {
        //Synchronize a possible change ongoing.
        SE_EXTRAVERB(group_mixer, "This:%p Manifestor:%p Stream:%p %s\n",
                     this, Manifestor, Stream, Mixer_c::LookupInputState(State));
        return State;
    }

    inline void RegisterManifestor(Manifestor_AudioKsound_c *Manifestor, HavanaStream_c *Stream)
    {
        SE_DEBUG(group_mixer, ">: This:%p Manifestor:%p Stream:%p\n", this, Manifestor, Stream);
        // Expected to be in DISCONNECTED state
        SE_ASSERT(State == DISCONNECTED);
        SetState(STOPPED);
        Mixer_Client_c::Manifestor = Manifestor;
        Mixer_Client_c::Stream = Stream;
        memset(&Parameters, 0, sizeof(Parameters));
        Muted = false;
        SE_DEBUG(group_mixer, "<: %s\n", Mixer_c::LookupInputState(State));
    }

    inline Manifestor_AudioKsound_c *GetManifestor() const
    {
        SE_ASSERT(Manifestor);
        SE_EXTRAVERB(group_mixer, "returning: %p\n", Manifestor);
        return Manifestor;
    }

    inline void DeRegisterManifestor()
    {
        SE_DEBUG(group_mixer, "This:%p Manifestor:%p Stream:%p\n", this, Manifestor, Stream);
        // Expected to be in STOPPED state
        SetState(DISCONNECTED);
        Manifestor = NULL;
        Stream = NULL;
        memset(&Parameters, 0, sizeof(Parameters));
        Muted = false;
    }

    inline PlayerStatus_t UpdateManifestorModuleParameters(const MixerClientParameters &params)
    {
        SE_DEBUG(group_mixer, ">: This:%p Manifestor:%p Stream:%p\n", this, Manifestor, Stream);
        // Expected NOT to be in DISCONNECTED state
        SE_ASSERT(!(State == DISCONNECTED));
        ManifestorStatus_t         Status;
        Manifestor_AudioKsound_c *Manifestor = GetManifestor();
        // This client is correctly stated, and its manifestor is well known.
        Status = Manifestor->SetModuleParameters(sizeof(params.ParameterBlockOT), (void *) &params.ParameterBlockOT);

        if (ManifestorNoError != Status)
        {
            return PlayerError;
        }

        Status = Manifestor->SetModuleParameters(sizeof(params.ParameterBlockDownmix), (void *) &params.ParameterBlockDownmix);

        if (ManifestorNoError != Status)
        {
            return PlayerError;
        }

        return PlayerNoError;
    }

    inline bool ManagedByThread() const
    {
        SE_EXTRAVERB(group_mixer, ">: This:%p Manifestor:%p Stream:%p %s\n",
                     this, Manifestor, Stream, Mixer_c::LookupInputState(State));

        switch (State)
        {
        case DISCONNECTED:
            return false;

        case STOPPED:
            return false;

        case UNCONFIGURED:
            return false;

        // In the following states this client internal members are relevant and allow operation mixer thread side.
        case STARTING:
            return true;

        case STARTED:
            return true;

        //case STOPPING:     return true;  TODO:  Should check this
        case STOPPING:
            return false;

        default:
            SE_ERROR("<: Unknown input state %d. Implementation error\n", State);
            return false;
        }
    }

    inline void UpdateParameters(const ParsedAudioParameters_t *ParsedAudioParameters)
    {
        SE_DEBUG(group_mixer, "This:%p Manifestor:%p Stream:%p\n", this, Manifestor, Stream);
        Parameters = *ParsedAudioParameters;
    }

    // Caller must check verify that state is not DISCONNECTED
    inline ParsedAudioParameters_t GetParameters() const
    {
        SE_EXTRAVERB(group_mixer, ">: This:%p Manifestor:%p Stream:%p\n", this, Manifestor, Stream);
        // Expected NOT to be in DISCONNECTED state
        SE_ASSERT(!(State == DISCONNECTED));
        SE_VERBOSE(group_mixer, "@: BitsPerSample:%u Channels:%u Rate:%uHz SampleCount:%u\n",
                   Parameters.Source.BitsPerSample,
                   Parameters.Source.ChannelCount,
                   Parameters.Source.SampleRateHz,
                   Parameters.SampleCount);
        return Parameters;
    }

    inline void GetParameters(ParsedAudioParameters_t *Parameters)
    {
        SE_EXTRAVERB(group_mixer, ">: This:%p Manifestor:%p Stream:%p\n", this, Manifestor, Stream);
        // Expected NOT to be in DISCONNECTED state
        SE_ASSERT(!(State == DISCONNECTED));
        *Parameters = Mixer_Client_c::Parameters;
    }

    inline HavanaStream_c *GetStream() const
    {
        SE_EXTRAVERB(group_mixer, ">: This:%p Manifestor:%p Stream:%p\n", this, Manifestor, Stream);
        // CAUTION: Caller is expected to check that Stream ! NULL
        // Expected NOT to be in DISCONNECTED state
        // SE_ASSERT(State != DISCONNECTED);
        return Stream;
    }

    inline void SetMute(bool Muted)
    {
        SE_DEBUG(group_mixer, "This:%p Manifestor:%p Stream:%p %smuted\n",
                 this, Manifestor, Stream, Muted ? "" : "not ");
        SE_INFO(group_mixer, "@: %p Set mute state to %s (was %s)\n",
                this,
                (Muted ? "true" : "false"),
                (Mixer_Client_c::Muted ? "true" : "false"));
        Mixer_Client_c::Muted = Muted;
    }

    inline bool IsMute() const
    {
        SE_VERBOSE(group_mixer, "<: %p %p %p Returning %s\n",
                   this, Manifestor, Stream, Muted ? "true" : "false");
        return Muted;
    }

private:
    InputState_t State; // The state of this client of audio mixer.
    Manifestor_AudioKsound_c *Manifestor; // Manifestor object as a pointer.
    HavanaStream_c *Stream; // Stream object as a pointer.
    ParsedAudioParameters_t Parameters; // The audio parameters for this client.
    bool Muted; // The mute state for this client.
    OS_Mutex_t Lock; // Internal mutex to arbitrate possible concurrent accesses for this client.

    DISALLOW_COPY_AND_ASSIGN(Mixer_Client_c);
};

#endif // H_MIXER_CLIENT_CLASS
