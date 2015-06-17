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

Source file name : havana_player.h derived from havana_player_factory.h
Author :           Nick

Definition of the implementation of havana player module for havana.


Date        Modification                                    Name
----        ------------                                    --------
04-Apr-05   Created                                         Julian

************************************************************************/

#ifndef H_HAVANA_PLAYER
#define H_HAVANA_PLAYER

#include "backend_ops.h"
#include "player_interface_ops.h"
#include "player.h"
#include "buffer_generic.h"
#include "player_generic.h"

#define FACTORY_ANY_ID                  "*"

#define MAX_FACTORIES                   64
#define MAX_PLAYBACKS                   4
#define MAX_DEMUX_CONTEXTS              4

#define MAX_DISPLAYS                    16

/*      Debug printing macros   */
#ifndef ENABLE_HAVANA_DEBUG
#define ENABLE_HAVANA_DEBUG             0
#endif

#define HAVANA_DEBUG(fmt, args...)      ((void) (ENABLE_HAVANA_DEBUG && \
                                            (report(severity_note, "HavanaPlayer_c::%s: " fmt, __FUNCTION__, ##args), 0)))

/* Output trace information off the critical path */
#define HAVANA_TRACE(fmt, args...)      (report(severity_note, "HavanaPlayer_c::%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define HAVANA_ERROR(fmt, args...)      (report(severity_error, "HavanaPlayer_c::%s: " fmt, __FUNCTION__, ##args))


typedef enum HavanaStatus_e
{
    HavanaNoError,
    HavanaNotOpen,
    HavanaNoMemory,
    HavanaPlaybackInvalid,
    HavanaPlaybackAlreadyExists,
    HavanaPlaybackActive,
    HavanaTooManyPlaybacks,
    HavanaDemuxInvalid,
    HavanaStreamInvalid,
    HavanaStreamAlreadyExists,
    HavanaTooManyStreams,
    HavanaNoFactory,
    HavanaError
} HavanaStatus_t;

class HavanaPlayback_c;
class HavanaStream_c;
class HavanaFactory_c;

/// Player wrapper class responsible for managing the player.
class HavanaPlayer_c
{
private:
    DeviceHandle_t              DisplayDevice;

    class HavanaFactory_c*      FactoryList;
    class HavanaPlayback_c*     Playback[MAX_PLAYBACKS];
    class HavanaDisplay_c*      AudioDisplays[MAX_DISPLAYS];
    class HavanaDisplay_c*      VideoDisplays[MAX_DISPLAYS];
    class HavanaDisplay_c*      OtherDisplays[MAX_DISPLAYS];

    DemultiplexorContext_t      DemuxContext[MAX_DEMUX_CONTEXTS];

    class BufferManager_c*      BufferManager;
    class Demultiplexor_c*      Demultiplexor;
    class Player_c*             Player;

    OS_Mutex_t                  Lock;

    /* Data shared with event signal process */
    OS_Event_t                  EventSignal;
    OS_Event_t                  EventSignalThreadTerminated;
    OS_Thread_t                 EventSignalThreadId;
    bool                        EventSignalThreadRunning;
    player_event_signal_callback        EventSignalCallback;

public:

                                HavanaPlayer_c                 (void);
                               ~HavanaPlayer_c                 (void);
    HavanaStatus_t              Init                           (void);

    HavanaStatus_t              CallFactory                    (const char*             Id,
                                                                const char*             SubId,
                                                                PlayerStreamType_t      StreamType,
                                                                PlayerComponent_t       Component,
                                                                void**                  Class);
    HavanaStatus_t              RegisterFactory                (const char*             Id,
                                                                const char*             SubId,
                                                                PlayerStreamType_t      StreamType,
                                                                PlayerComponent_t       Component,
                                                                unsigned int            Version,
                                                                bool                    Force,
                                                                void*                  (*NewFactory)     (void));
    HavanaStatus_t              DeRegisterFactory              (const char*             Id,
                                                                const char*             SubId,
                                                                PlayerStreamType_t      StreamType,
                                                                PlayerComponent_t       Component,
                                                                unsigned int            Version);

    HavanaStatus_t              GetManifestor                  (char*                   Media,
                                                                char*                   Encoding,
                                                                unsigned int            SurfaceId,
                                                                class Manifestor_c**    Manifestor);
    HavanaStatus_t              GetDemuxContext                (unsigned int            DemuxId,
                                                                class Demultiplexor_c** Demultiplexor,
                                                                DemultiplexorContext_t* DemultiplexorContext);
    HavanaStatus_t              CreatePlayback                 (HavanaPlayback_c**      HavanaPlayback);
    HavanaStatus_t              DeletePlayback                 (HavanaPlayback_c*       HavanaPlayback);
    HavanaStatus_t              CreateDisplay                  (char*                   Media,
                                                                unsigned int            SurfaceId,
                                                                HavanaDisplay_c**       HavanaDisplay);
    HavanaStatus_t              DeleteDisplay                  (char*                   Media,
                                                                unsigned int            SurfaceId);
    HavanaStatus_t              SynchronizeDisplay             (char*                   Media,
                                                                unsigned int            SurfaceId);

    player_event_signal_callback        RegisterEventSignalCallback    (player_event_signal_callback   EventSignalCallback);
    void                        EventSignalThread              (void);
};

/*{{{  doxynote*/
/*! \class      HavanaPlayer_c
    \brief      Overall management of outside world access to the player.

The HavanaPlayer_c class is the outside layer of the player wrapper.  It is responsible for managing
the various component factories, creating, initialising and deleting playbacks and creating
demultiplexor contexts.

Playbacks are kept in a table and created on demand.  Playbacks are deleted when explicitly requested
or when the HavanaPlayer_c class is destroyed.  A playback will be reused if it contains no streams
when a new one is requested.

Demultiplexor contexts are identified by an integer id.  For linux dvb this will be the id of the
demux device associated with this stream i.e /dev/dvb/adapter0/demux2 will use demultiplexor context 2.

Factories are about to change because a single Id string is not specific enough to distinguish between
pes video mpeg2 and pes video vc1 for example.

*/

/*! \fn HavanaStatus_t HavanaPlayer_c::RegisterFactory (char*                   Id,
                                                        char*                   SubId,
                                                        PlayerStreamType_t      StreamType,
                                                        PlayerComponent_t       Component,
                                                        unsigned int            Version,
                                                        bool                    Force,
                                                        void*                  (*NewFactory)     (void));

\brief Register the factory which can make a player component identified by Id, StreamType and Component.

\param Id               String which uniquely identifies this factory.
\param SubId            TODO
\param StreamType       Audio or video
\param Component        Which player component will be manufactured by this factory
\param Version          Which version of the factory this is.  A higher number indicates a newer
                        version which will automatically supercede the older version.
\param Force            If Force is true the factory will alwayse replace any previous similar
                        factory.
\param NewFactory       Function which will acually manufacture the comonent.

\return Havana status code, HavanaNoError indicates success.
*/

/*! \fn HavanaStatus_t HavanaPlayer_c::DeRegisterFactory (char*                   Id,
                                                          char*                   SubId,
                                                          PlayerStreamType_t      StreamType,
                                                          PlayerComponent_t       Component,
                                                          unsigned int            Version)

\brief Remove the factory registered to make the player component identified by Id, StreamType and Component.

\param Id               String which uniquely identifies this factory.
\param StreamType       Audio or Video
\param SubId            TODO
\param Component        Which player component will be manufactured by this factory
\param Version          Which version of the factory to remove.

\return Havana status code, HavanaNoError indicates success.
*/

/*! \fn HavanaStatus_t HavanaPlayer_c::CallFactory (char*                   Id,
                                                    char*                   SubId,
                                                    PlayerStreamType_t      StreamType,
                                                    PlayerComponent_t       Component,
                                                    void**                  Class);

\brief Manufacture the player component identified by Id, StreamType and Component.

\param Id               String which uniquely identifies this factory.
\param SubId            TODO
\param StreamType       Audio or Video
\param Component        Which player component to be manufactured.
\param Class            Pointer to location of ccomponent to build.

\return Havana status code, HavanaNoError indicates success.
*/

/*! \fn HavanaStatus_t HavanaPlayer_c::GetDemuxContext (unsigned int            DemuxId,
                                                      class Demultiplexor_c** Demultiplexor,
                                                      DemultiplexorContext_t* DemultiplexorContext);

\brief Identify or create the DemultiplexorContext associated with this DemuxId

\param DemuxId                  Number of desired demultiplexor context.
\param Demultiplexor            Pointer to location to store the demultiplexor class using the contexts.
\param demultiplexorContext     Pointer to location to store the context

\return Havana status code, HavanaNoError indicates success.
*/
/*! \fn HavanaStatus_t HavanaPlayer_c::CreatePlayback (HavanaPlayback_c**      HavanaPlayback);

\return Havana status code, HavanaNoError indicates playback succesfully created.
*/
/*! \fn HavanaStatus_t HavanaPlayer_c::DeletePlayback (HavanaPlayback_c*       HavanaPlayback);

\return Havana status code, HavanaNoError indicates success.
*/
/*}}}  */

#endif

