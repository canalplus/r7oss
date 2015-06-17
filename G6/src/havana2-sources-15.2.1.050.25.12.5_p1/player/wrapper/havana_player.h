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

#ifndef H_HAVANA_PLAYER
#define H_HAVANA_PLAYER

#include "osinline.h"
#include "player.h"
#include "buffer_generic.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG   "HavanaPlayer_c"

#define MAX_FACTORIES                   64
#define MAX_PLAYBACKS                   16
#define MAX_STREAMS_PER_PLAYBACK        32
#define MAX_SINGLE_STAGE_MIXERS         4 // as many as AUDIO_PLAYERS
#define MAX_DUAL_STAGE_MIXERS           1 // only one MS12 mixer

#define STM_SE_MIXER_BYPASS  (STM_SE_MIXER_MAX_TYPE + 1)
#define MAX_BYPASS_MIXERS               2 // one for HDMI , one for SPDIF

#define MAX_MIXERS                      ( MAX_SINGLE_STAGE_MIXERS + MAX_DUAL_STAGE_MIXERS + MAX_BYPASS_MIXERS )
#define MAX_AUDIO_PLAYERS               4
#define MAX_FACTORY_LISTS               16
#define MAX_AUDIO_READERS               4
#define MAX_AUDIO_GENERATORS            8
#define MAX_INTERACTIVE_AUDIO           8

#define INVALID_FACTORY_ID              0xffffffff

// MAX_DISPLAYS = 16 is not enough for mosaic usecase
#define MAX_DISPLAYS                    20

typedef enum HavanaStatus_e
{
    HavanaNoError,
    HavanaNotOpen,
    HavanaNoDevice,
    HavanaNoMemory,
    HavanaPlaybackInvalid,
    HavanaTooManyPlaybacks,
    HavanaStreamInvalid,
    HavanaTooManyStreams,
    HavanaTooManyDisplays,
    HavanaDisplayInvalid,
    HavanaMixerAlreadyExists,
    HavanaTooManyMixers,
    HavanaGeneratorAlreadyExists,
    HavanaTooManyGenerators,
    HavanaNoFactory,
    HavanaComponentInvalid,
    HavanaBusy,
    HavanaError
} HavanaStatus_t;

struct FactoryList_s
{
    unsigned int                FactoryId;
    class HavanaFactory_c      *FactoryList;

};

class HavanaPlayback_c;
class HavanaStream_c;
class HavanaFactory_c;
class HavanaPlayback_c;
class HavanaDisplay_c;
class HavanaStream_c;
class Mixer_Mme_c;
class Audio_Player_c;
class BufferManager_c;
class Player_c;
class Audio_Reader_c;
class Audio_Generator_c;

/// Player wrapper class responsible for managing the player.
class HavanaPlayer_c
{
public:
    HavanaPlayer_c(void);
    ~HavanaPlayer_c(void);

    HavanaStatus_t              Init(void);

    HavanaStatus_t              CallFactory(PlayerComponent_t       Component,
                                            PlayerStreamType_t      StreamType,
                                            stm_se_stream_encoding_t Encoding,
                                            void                  **Class);
    HavanaStatus_t              RegisterFactory(PlayerComponent_t       Component,
                                                PlayerStreamType_t      StreamType,
                                                stm_se_stream_encoding_t Encoding,
                                                unsigned int            Version,
                                                bool                    Force,
                                                void *(*NewFactory)(void));

    HavanaStatus_t              GetManifestor(class HavanaStream_c   *Stream,
                                              unsigned int            Capability,
                                              stm_object_h            DisplayHandle,
                                              class Manifestor_c    **Manifestor,
                                              stm_se_sink_input_port_t input_port = STM_SE_SINK_INPUT_PORT_PRIMARY);

    HavanaStatus_t              DeleteManifestor(class HavanaStream_c   *Stream,
                                                 stm_object_h            DisplayHandle);

    HavanaStatus_t              GetBufferManager(class BufferManager_c **BufferManager);

    HavanaStatus_t              CreatePlayback(HavanaPlayback_c      **HavanaPlayback);
    HavanaStatus_t              DeletePlayback(HavanaPlayback_c       *HavanaPlayback);

    HavanaStatus_t              CreateMixer(const char                *name,
                                            stm_se_mixer_spec_t        topology,
                                            Mixer_Mme_c              **audio_mixer);

    HavanaStatus_t              DeleteMixer(Mixer_Mme_c            *audio_mixer);

    HavanaStatus_t              CreateAudioGenerator(const char             *name,
                                                     Audio_Generator_c     **audio_generator);
    HavanaStatus_t              DeleteAudioGenerator(Audio_Generator_c      *audio_generator);
    HavanaStatus_t              AttachGeneratorToMixer(Audio_Generator_c    *audio_generator,
                                                       Mixer_Mme_c          *audio_mixer);
    HavanaStatus_t              DetachGeneratorFromMixer(Audio_Generator_c  *audio_generator,
                                                         Mixer_Mme_c        *audio_mixer);
    HavanaStatus_t              CreateAudioPlayer(const char           *name,
                                                  const char           *hw_name,
                                                  Audio_Player_c        **audio_player);
    HavanaStatus_t              DeleteAudioPlayer(Audio_Player_c         *audio_player);
    HavanaStatus_t              AttachPlayerToMixer(Mixer_Mme_c          *audio_mixer,
                                                    Audio_Player_c       *audio_player);
    HavanaStatus_t              DetachPlayerFromMixer(Mixer_Mme_c        *audio_mixer,
                                                      Audio_Player_c     *audio_player);

    HavanaStatus_t              UpdateMixerTransformerId(unsigned int     mixerId,
                                                         const char *transformerName);

    HavanaStatus_t              CreateAudioReader(const char             *name,
                                                  const char             *hw_name,
                                                  Audio_Reader_c        **audio_reader);
    HavanaStatus_t              DeleteAudioReader(Audio_Reader_c         *audio_reader);
    HavanaStatus_t              AttachAudioReader(Audio_Reader_c         *audio_reader,
                                                  HavanaStream_c         *play_stream);
    HavanaStatus_t              DetachAudioReader(Audio_Reader_c         *audio_reader,
                                                  HavanaStream_c         *play_stream);

    HavanaStatus_t              SetOption(PlayerPolicy_t          Option,
                                          int                     Value);
    HavanaStatus_t              GetOption(PlayerPolicy_t          Option,
                                          int                    *Value);
    HavanaStatus_t              CreateDisplay(HavanaDisplay_c       **HavanaDisplay);
    HavanaStatus_t              DeleteDisplay(class HavanaStream_c   *Stream,
                                              stm_object_h            DisplayHandle);

    HavanaStatus_t              AttachStream(class HavanaStream_c   *Stream,
                                             stm_se_media_t          Media,
                                             int                    *StreamCount);
    HavanaStatus_t              DetachStream(class HavanaStream_c   *Stream,
                                             stm_se_media_t          Media);

    HavanaStatus_t              LowPowerEnter(__stm_se_low_power_mode_t low_power_mode);
    HavanaStatus_t              LowPowerExit(void);

    friend class HavanaPlayback_c;
private:
    struct FactoryList_s        FactoryLists[MAX_FACTORY_LISTS];
    unsigned int                NoFactoryLists;
    OS_Mutex_t                  LockFactoryList;

    class HavanaPlayback_c     *Playback[MAX_PLAYBACKS];
    class HavanaDisplay_c      *Display[MAX_DISPLAYS];
    /* Attached streams to current player.
     * Usefull to know in which memory partition the A/V buffers should be allocated.*/
    class HavanaStream_c       *AttachedStreams[MAX_PLAYBACKS *MAX_STREAMS_PER_PLAYBACK][STM_SE_MEDIA_ANY + 1];

    union
    {
        struct
        {
            class Mixer_Mme_c  *SingleStageMixer[MAX_SINGLE_STAGE_MIXERS];
            class Mixer_Mme_c  *DualStageMixer[MAX_DUAL_STAGE_MIXERS];
            class Mixer_Mme_c  *BypassMixer[MAX_BYPASS_MIXERS];
        }                               Typed;
        class Mixer_Mme_c              *Any[MAX_MIXERS];
    }                           mMixer;
    class Audio_Player_c       *Audio_Player[MAX_AUDIO_PLAYERS];
    class Audio_Reader_c       *AudioReader[MAX_AUDIO_READERS];
    class Audio_Generator_c    *AudioGenerator[MAX_AUDIO_GENERATORS + MAX_INTERACTIVE_AUDIO];

    class BufferManager_c      *BufferManager;
    class Player_c             *Player;


    OS_Mutex_t                  Lock;

    DISALLOW_COPY_AND_ASSIGN(HavanaPlayer_c);


};

/*{{{  doxynote*/
/*! \class      HavanaPlayer_c
    \brief      Overall management of outside world access to the player.

The HavanaPlayer_c class is the outside layer of the player wrapper.  It is responsible for managing
the various component factories, creating, initialising and deleting playbacks

Playbacks are kept in a table and created on demand.  Playbacks are deleted when explicitly requested
or when the HavanaPlayer_c class is destroyed.  A playback will be reused if it contains no streams
when a new one is requested.

*/

/*! \fn HavanaStatus_t HavanaPlayer_c::RegisterFactory (PlayerComponent_t       Component,
                                                        PlayerStreamType_t      StreamType,
                                                        stm_se_stream_encoding_t Encoding,
                                                        unsigned int            Version,
                                                        bool                    Force,
                                                        void*                  (*NewFactory)     (void));

\brief Register the factory which can make a player component identified by Component, StreamType and Encoding.

\param Component        Which player component will be manufactured by this factory
\param StreamType       Audio or video
\param Encoding         Audio or video encoding type this factory supports
\param Version          Which version of the factory this is.  A higher number indicates a newer
                        version which will automatically supercede the older version.
\param Force            If Force is true the factory will alwayse replace any previous similar
                        factory.
\param NewFactory       Function which will acually manufacture the comonent.

\return Havana status code, HavanaNoError indicates success.
*/

/*! \fn HavanaStatus_t HavanaPlayer_c::DeRegisterFactory (PlayerComponent_t     Component,
                                                          PlayerStreamType_t    StreamType,
                                                          stm_se_stream_encoding_t Encoding,
                                                          unsigned int          Version);

\brief Remove the factory registered to make the player component identified by Id, StreamType and Component.

\param Component        Which player component will be manufactured by this factory
\param StreamType       Audio or Video
\param Encoding         Audio or video encoding type this factory supports
\param Version          Which version of the factory to remove.

\return Havana status code, HavanaNoError indicates success.
*/

/*! \fn HavanaStatus_t HavanaPlayer_c::CallFactory (PlayerComponent_t       Component,
                                                    PlayerStreamType_t      StreamType,
                                                    stm_se_stream_encoding_t Encoding,
                                                    void**                  Class);

\brief Manufacture the player component identified by Id, StreamType and Component.

\param Component        Which player component to be manufactured.
\param StreamType       Audio or Video
\param Encoding         Audio or video encoding type this factory supports
\param Class            Pointer to location of component to build.

\return Havana status code, HavanaNoError indicates success.
*/

/*! \fn HavanaStatus_t HavanaPlayer_c::CreatePlayback (HavanaPlayback_c**       HavanaPlayback);

\return Havana status code, HavanaNoError indicates playback succesfully created.
*/
/*! \fn HavanaStatus_t HavanaPlayer_c::DeletePlayback (HavanaPlayback_c*        HavanaPlayback);

\return Havana status code, HavanaNoError indicates success.
*/
/*! \fn HavanaStatus_t HavanaPlayer_c::GetManifestor (stm_se_media_t            Media,
                                                      stm_se_stream_encoding_t  Encoding,
                                                      unsigned int              SurfaceId,
                                                      class Manifestor_c**      Manifestor);

\brief Access an appropriate manifestor for this stream and content.
\Param Media                    Audio or Video
\Param Encoding                 Audio or Video content encoding
\Param SurfaceId                Which surface manifestor is to be associated with - in Linux DVB this
                                corresponds with the device number.
\param Manifestor               Pointer to location to store returned manifestor.

\return Havana status code, HavanaNoError indicates success.
*/
/*}}}*/

#endif

