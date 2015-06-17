/************************************************************************
COPYRIGHT (C) STMicroelectronics 2005

Source file name : havana_player.cpp
Author :           Julian

Implementation of the player module for havana.


Date        Modification                                    Name
----        ------------                                    --------
14-Feb-07   Created                                         Julian

************************************************************************/

#include "player.h"
#include "buffer_generic.h"
#include "demultiplexor_ts.h"
#include "manifestor_audio.h"
#include "manifestor_video.h"

#include "backend_ops.h"
#include "havana_player.h"
#include "havana_factory.h"
#include "havana_playback.h"
#include "havana_stream.h"
#include "havana_display.h"
#include "display.h"

//{{{  Thread entry stub
static OS_TaskEntry (EventSignalThreadStub)
{
    HavanaPlayer_c*     HavanaPlayer    = (HavanaPlayer_c*)Parameter;

    HavanaPlayer->EventSignalThread ();
    OS_TerminateThread ();
    return NULL;
}
//}}}  

//{{{  HavanaPlayer_c
HavanaPlayer_c::HavanaPlayer_c (void)
{
    int i;

    HAVANA_DEBUG("\n");
    Player                      = NULL;
    Demultiplexor               = NULL;
    BufferManager               = NULL;
    FactoryList                 = NULL;

    for (i = 0; i < MAX_PLAYBACKS; i++)
        Playback[i]             = NULL;

    for (i = 0; i < MAX_DEMUX_CONTEXTS; i++)
        DemuxContext[i]         = NULL;

    for (i = 0; i < MAX_DISPLAYS; i++)
    {
        AudioDisplays[i]        = NULL;
        VideoDisplays[i]        = NULL;
        OtherDisplays[i]        = NULL;
    }

    DisplayDevice               = NULL;

    EventSignalThreadId         = OS_INVALID_THREAD;
    EventSignalThreadRunning    = false;
    EventSignalCallback         = NULL;

}
//}}}  
//{{{  ~HavanaPlayer_c
HavanaPlayer_c::~HavanaPlayer_c (void)
{
    int                         i;
    class  HavanaFactory_c*     Factory;

    HAVANA_DEBUG("\n");


    // remove event signal
    Player->SetEventSignal (PlayerAllPlaybacks, PlayerAllStreams, EventAllEvents, NULL);

    // Shut down event signal thread first
    if (EventSignalThreadId != OS_INVALID_THREAD)
    {
        EventSignalThreadRunning        = false;
        EventSignalCallback             = NULL;
        OS_SetEvent (&EventSignal);
        OS_WaitForEvent (&EventSignalThreadTerminated, OS_INFINITE);       // Wait for display signal to exit
        EventSignalThreadId             = OS_INVALID_THREAD;
        OS_TerminateEvent (&EventSignalThreadTerminated);
        OS_TerminateEvent (&EventSignal);
        OS_TerminateMutex (&Lock);
    }

    for (i = 0; i < MAX_PLAYBACKS; i++)
    {
        if (Playback[i] != NULL)
            delete Playback[i];
    }
    for (i = 0; i < MAX_DEMUX_CONTEXTS; i++)
    {
        if (DemuxContext[i] != NULL)
            delete DemuxContext[i];
    }
    for (i = 0; i < MAX_DISPLAYS; i++)
    {
        if (AudioDisplays[i] != NULL)
            delete AudioDisplays[i];
        if (VideoDisplays[i] != NULL)
            delete VideoDisplays[i];
        if (OtherDisplays[i] != NULL)
            delete OtherDisplays[i];
    }

    if (BufferManager != NULL)
        delete BufferManager;
    if (Demultiplexor != NULL)
        delete Demultiplexor;

    while (FactoryList != NULL)
    {
        Factory         = FactoryList;
        FactoryList     = Factory->Next ();
        delete Factory;
    }

    DisplayDevice       = NULL;

    EventSignalCallback = NULL;

}
//}}}  
//{{{  Init
HavanaStatus_t HavanaPlayer_c::Init (void)
{
    HAVANA_DEBUG("\n");

    if (Player == NULL)
        Player          = new Player_Generic_c();
    if (Player == NULL)
    {
        HAVANA_ERROR("Unable to create player - insufficient memory\n");
        return HavanaNoMemory;
    }

    //{{{  Event management
    // Create event signal thread and input mutexes
    if (EventSignalThreadId == OS_INVALID_THREAD)
    {
        if (OS_InitializeMutex (&Lock) != OS_NO_ERROR)
        {
            HAVANA_ERROR ("Failed to initialize Lock mutex\n");
            return HavanaNoMemory;
        }

        if (OS_InitializeEvent (&EventSignal) != OS_NO_ERROR)
        {
            HAVANA_ERROR ("Failed to initialize EventSignal\n");
            OS_TerminateMutex (&Lock);
            return HavanaError;
        }
        if (OS_InitializeEvent (&EventSignalThreadTerminated) != OS_NO_ERROR)
        {
            HAVANA_ERROR ("Failed to initialize EventSignalThreadTerminated event\n");
            OS_TerminateEvent (&EventSignal);
            OS_TerminateMutex (&Lock);
            return HavanaError;
        }
        EventSignalThreadRunning        = true;
        if (OS_CreateThread (&EventSignalThreadId, EventSignalThreadStub, this, "Havana player Event Signal Thread", OS_MID_PRIORITY) != OS_NO_ERROR)
        {
            HAVANA_ERROR("Unable to create Display Signal thread\n");
            EventSignalThreadRunning    = false;
            EventSignalThreadId         = OS_INVALID_THREAD;
            OS_TerminateEvent (&EventSignalThreadTerminated);
            OS_TerminateEvent (&EventSignal);
            OS_TerminateMutex (&Lock);
            return HavanaError;
        }
    }
    //}}}  

    if (BufferManager == NULL)
        BufferManager   = new BufferManager_Generic_c();
    if (BufferManager == NULL)
    {
        HAVANA_ERROR("Unable to create buffer manager - insufficient memory\n");
        return HavanaNoMemory;
    }
    Player->RegisterBufferManager (BufferManager);

    if (Demultiplexor == NULL)
        Demultiplexor   = new Demultiplexor_Ts_c ();
    if (Demultiplexor == NULL)
    {
        HAVANA_ERROR("Unable to create transport stream demultiplexor\n");
        return HavanaNoMemory;
    }
    Player->RegisterDemultiplexor (Demultiplexor);

    DisplayDevice       = NULL;

    if (Player->SetEventSignal (PlayerAllPlaybacks, PlayerAllStreams, EventAllEvents, &EventSignal) != PlayerNoError)
    {
        HAVANA_ERROR("Failed to set up player event signal\n");
        return HavanaError;
    }
    //Player->SpecifySignalledEvents (PlayerAllPlaybacks, PlayerAllStreams, EventAllEvents); // Must be done by every stream


    return HavanaNoError;
}
//}}}  

//{{{  CallFactory
HavanaStatus_t HavanaPlayer_c::CallFactory     (const char*             Id,
                                                const char*             SubId,
                                                PlayerStreamType_t      StreamType,
                                                PlayerComponent_t       Component,
                                                void**                  Class)
{
    class HavanaFactory_c*      Factory;
    const char*                 Name;

    //HAVANA_DEBUG("\n");

    for (Factory = FactoryList; Factory != NULL; Factory = Factory->Next())
    {
        if (Factory->CanBuild (Id, SubId, StreamType, Component))
        {
            return Factory->Build (Class);
        }
    }
    switch (Component)
    {
        case ComponentCollator:
            Name        = "Collator";
            break;
        case ComponentFrameParser:
            Name        = "Frame parser";
            break;
        case ComponentCodec:
            Name        = "Codec";
            break;
        case ComponentManifestor:
            Name        = "Manifestor";
            break;
        case ComponentOutputTimer:
            Name        = "Output timer";
            break;
        case ComponentExternal:
            Name        = "External component";
            break;
        default:
            Name        = "Unknown component";
    }
    HAVANA_ERROR("No factory found for %s %s %s\n", SubId, Id, Name);

    return HavanaNoFactory;
}
//}}}  
//{{{  RegisterFactory
HavanaStatus_t HavanaPlayer_c::RegisterFactory (const char*             Id,
                                                const char*             SubId,
                                                PlayerStreamType_t      StreamType,
                                                PlayerComponent_t       Component,
                                                unsigned int            Version,
                                                bool                    Force,
                                                void*                  (*NewFactory)   (void))
{
    HavanaStatus_t              HavanaStatus;
    class  HavanaFactory_c*     Factory;

    //HAVANA_DEBUG(": Id %s, SubId %s, StreamType %d, Component %x Version %d\n", Id, SubId, StreamType, Component, Version);
    for (Factory = FactoryList; Factory != NULL; Factory = Factory->Next())
    {
        if (Factory->CanBuild (Id, SubId, StreamType, Component))
        {
            if ((Version > Factory->Version ()) || Force)
            {
                HAVANA_TRACE("New factory version %d supercedes previously registered for %s %s %x version %d. \n",
                             Version, Id, Component, Factory->Version());
                break;
            }
            else
            {
                HAVANA_TRACE("New factory version %d does not supercede previously registered for %s %x version %d. \n",
                             Version, Id, SubId, Component, Factory->Version());
                return HavanaError;
            }
        }
    }

    Factory     = new HavanaFactory_c ();
    if (Factory == NULL)
    {
        HAVANA_ERROR("Unable to create new factory - insufficient memory\n");
        return HavanaNoMemory;
    }
    HavanaStatus        = Factory->Init (FactoryList, Id, SubId, StreamType, Component, Version, NewFactory);
    if (HavanaStatus != HavanaNoError)
    {
        HAVANA_ERROR("Unable to create Factory  for %s %s %x\n", Id, SubId, HavanaStatus);
        delete Factory;
        return HavanaStatus;
    }
    FactoryList = Factory;

    return HavanaNoError;
}
//}}}  
//{{{  DeRegisterFactory
HavanaStatus_t HavanaPlayer_c::DeRegisterFactory       (const char*             Id,
                                                        const char*             SubId,
                                                        PlayerStreamType_t      StreamType,
                                                        PlayerComponent_t       Component,
                                                        unsigned int            Version)
{
    class  HavanaFactory_c*     Factory;
    class  HavanaFactory_c*     Previous;

    HAVANA_DEBUG("\n");
    Previous    = FactoryList;
    for (Factory = FactoryList; Factory != NULL; Factory = Factory->Next())
    {
        if (Factory->CanBuild (Id, SubId, StreamType, Component))
        {
            if (Version == Factory->Version ())
                break;
        }
        Previous        = Factory;
    }
    if (Factory == NULL)
    {
        HAVANA_ERROR("Unable to find factory version %d for %s %s %x.\n",
                      Version, Id, SubId, Component);
        return HavanaError;
    }

    if (Factory == FactoryList)
        FactoryList     = Factory->Next ();
    else
        Previous->ReLink (Factory->Next ());

    delete Factory;

    HAVANA_TRACE("Factory version %d for %s %s %x removed.\n", Version, Id, SubId, Component);

    return HavanaNoError;
}
//}}}  

//{{{  GetManifestor
//{{{  doxynote
/// \brief  access a manifestor - or create a new one if it doesn't exist
/// \return Havana status code, HavanaNoError indicates success.
//}}}  
HavanaStatus_t HavanaPlayer_c::GetManifestor   (char*                           Media,
                                                char*                           Encoding,
                                                unsigned int                    SurfaceId,
                                                class Manifestor_c**            Manifestor)
{
    HavanaStatus_t      Status  = HavanaNoError;
    HavanaDisplay_c*    Display = NULL;

    HAVANA_DEBUG("%s: %d\n", Media, SurfaceId);

    Status      = CreateDisplay (Media, SurfaceId, &Display);

    if (Status != HavanaNoError)
        return Status;

    Status      = Display->GetManifestor       (this,
                                                Media,
                                                Encoding,
                                                SurfaceId,
                                                Manifestor);
    if (Status != HavanaNoError)
        DeleteDisplay (Media, SurfaceId);

    return Status;
}
//}}}  
//{{{  GetDemuxContext
//{{{  doxynote
/// \brief  Create a new demux context
/// \return Havana status code, HavanaNoError indicates success.
//}}}  
HavanaStatus_t HavanaPlayer_c::GetDemuxContext (unsigned int                    DemuxId,
                                                class Demultiplexor_c**         Demultiplexor,
                                                DemultiplexorContext_t*         DemultiplexorContext)
{
    // The string Multiplex will be of the form tsn where the number n indicates which demux
    // context is required

    *DemultiplexorContext   = NULL;

    if (DemuxId >= MAX_DEMUX_CONTEXTS)
        return HavanaError;

    if (DemuxContext[DemuxId] == NULL)
    {
        this->Demultiplexor->CreateContext (&DemuxContext[DemuxId]);
        if (DemuxContext[DemuxId] == NULL)
            return HavanaError;
    }

    *Demultiplexor           = this->Demultiplexor;
    *DemultiplexorContext    = DemuxContext[DemuxId];

    return HavanaNoError;
}
//}}}  
//{{{  CreatePlayback
HavanaStatus_t HavanaPlayer_c::CreatePlayback    (HavanaPlayback_c**      HavanaPlayback)
{
    HavanaStatus_t      HavanaStatus;
    int                 i;

    HAVANA_DEBUG("\n");

    OS_LockMutex (&Lock);
    for (i = 0; i < MAX_PLAYBACKS; i++)
    {
        if (Playback[i] != NULL)
        {
            if (Playback[i]->Active () == HavanaPlaybackActive)
                continue;
        }
        break;
    }

    if (i == MAX_PLAYBACKS)
    {
        HAVANA_ERROR("Unable to create playback context - Too many playbacks\n");
        OS_UnLockMutex (&Lock);
        return HavanaTooManyPlaybacks;
    }

    if (Playback[i] == NULL)
        Playback[i] = new HavanaPlayback_c ();
    if (Playback[i] == NULL)
    {
        HAVANA_ERROR("Unable to create playback context - insufficient memory\n");
        OS_UnLockMutex (&Lock);
        return HavanaNoMemory;
    }

    HavanaStatus        = Playback[i]->Init (this, Player, BufferManager);
    if (HavanaStatus != HavanaNoError)
    {
        HAVANA_ERROR("Unable to create playback context %x\n", HavanaStatus);
        delete Playback[i];
        Playback[i]     = NULL;
        OS_UnLockMutex (&Lock);
        return HavanaStatus;
    }

    *HavanaPlayback     = Playback[i];

    OS_UnLockMutex (&Lock);

    return HavanaNoError;
}
//}}}  
//{{{  DeletePlayback
HavanaStatus_t HavanaPlayer_c::DeletePlayback    (HavanaPlayback_c*       HavanaPlayback)
{
    int                 i;

    if (HavanaPlayback == NULL)
        return HavanaPlaybackInvalid;

    OS_LockMutex (&Lock);
    for (i = 0; i < MAX_PLAYBACKS; i++)
    {
        if (Playback[i] == HavanaPlayback)
            break;
    }
    if (i == MAX_PLAYBACKS)
    {
        HAVANA_ERROR("Unable to locate playback for delete\n");
        OS_UnLockMutex (&Lock);
        return HavanaPlaybackInvalid;
    }

    delete HavanaPlayback;
    Playback[i]   = NULL;

    OS_UnLockMutex (&Lock);
    return HavanaNoError;
}
//}}}  
//{{{  CreateDisplay
HavanaStatus_t HavanaPlayer_c::CreateDisplay   (char*                           Media,
                                                unsigned int                    SurfaceId,
                                                HavanaDisplay_c**               HavanaDisplay)
{
    HavanaStatus_t      Status  = HavanaNoError;
    HavanaDisplay_c**   Display = NULL;

    HAVANA_DEBUG("%s: %d\n", Media, SurfaceId);

    if (SurfaceId > MAX_DISPLAYS)
        return HavanaError;

    if (strcmp (Media, BACKEND_AUDIO_ID) == 0)
        Display                 = (HavanaDisplay_c**)AudioDisplays;
    else if (strcmp (Media, BACKEND_VIDEO_ID) == 0)
        Display                 = (HavanaDisplay_c**)VideoDisplays;
    else
        Display                 = (HavanaDisplay_c**)OtherDisplays;

    if (Display[SurfaceId] == NULL)
    {
        Display[SurfaceId]      = new HavanaDisplay_c();
        if (Display[SurfaceId] == NULL)
        {
            HAVANA_ERROR("Unable to create display context - insufficient memory\n");
            Status              = HavanaNoMemory;
        }
    }

    *HavanaDisplay              = Display[SurfaceId];

    return HavanaNoError;
}
//}}}  
//{{{  DeleteDisplay
HavanaStatus_t HavanaPlayer_c::DeleteDisplay   (char*           Media,
                                                unsigned int    SurfaceId)
{
    HavanaDisplay_c**   Display = NULL;

    HAVANA_DEBUG("%s: %d\n", Media, SurfaceId);

    if (SurfaceId > MAX_DISPLAYS)
        return HavanaError;

    if (strcmp (Media, BACKEND_AUDIO_ID) == 0)
        Display         = (HavanaDisplay_c**)AudioDisplays;
    else if (strcmp (Media, BACKEND_VIDEO_ID) == 0)
        Display         = (HavanaDisplay_c**)VideoDisplays;
    else
        Display         = (HavanaDisplay_c**)OtherDisplays;

    if (Display[SurfaceId] == NULL)
        return HavanaError;

    delete  Display[SurfaceId];
    Display[SurfaceId]  = NULL;

    return HavanaNoError;
}
//}}}  

//{{{  SynchronizeDisplay
HavanaStatus_t HavanaPlayer_c::SynchronizeDisplay   (char*           Media,
                                                     unsigned int    SurfaceId)
{
    PlayerStatus_t      Status;
    HavanaDisplay_c**   Display = NULL;

    HAVANA_DEBUG("%s: %d\n", Media, SurfaceId);

    if (SurfaceId > MAX_DISPLAYS)
        return HavanaError;

    if (strcmp (Media, BACKEND_AUDIO_ID) == 0)
        Display         = (HavanaDisplay_c**)AudioDisplays;
    else if (strcmp (Media, BACKEND_VIDEO_ID) == 0)
        Display         = (HavanaDisplay_c**)VideoDisplays;
    else
        Display         = (HavanaDisplay_c**)OtherDisplays;

    if (Display[SurfaceId] == NULL)
        return HavanaError;

//

    Status      = Display[SurfaceId]->ReferenceManifestor()->SynchronizeOutput();

    return (Status==PlayerNoError) ? HavanaNoError : HavanaError;
}
//}}}  

//{{{  RegisterEventSignalCallback
player_event_signal_callback HavanaPlayer_c::RegisterEventSignalCallback       (player_event_signal_callback   Callback)
{
    player_event_signal_callback        PreviousCallback        = this->EventSignalCallback;

    this->EventSignalCallback   = Callback;
    return PreviousCallback;
}
//}}}  
//{{{  EventSignalThread
void  HavanaPlayer_c::EventSignalThread (void)
{
    PlayerStatus_t              PlayerStatus;
    struct PlayerEventRecord_s  PlayerEvent;
    HavanaStatus_t              HavanaStatus;
    int                         i;
    struct player_event_s       Event;

    HAVANA_DEBUG("Starting\n");

    while (EventSignalThreadRunning)
    {
        OS_WaitForEvent (&EventSignal, OS_INFINITE);
        OS_ResetEvent   (&EventSignal);

        while (true)
        {
            PlayerStatus        = Player->GetEventRecord (PlayerAllPlaybacks, PlayerAllStreams, EventAllEvents, &PlayerEvent, true);
            if (PlayerStatus != PlayerNoError)
                break;

            HAVANA_DEBUG("Got Event 0x%x\n", PlayerEvent.Code);

            OS_LockMutex (&Lock);                       // Make certain we cannot delete playback while checking the event
            for (i = 0; i < MAX_PLAYBACKS; i++)         // Check to see if any streams are interested in this event
            {
                if (Playback[i] != NULL)
                {
                    HavanaStatus    = Playback[i]->CheckEvent (&PlayerEvent);
                    if (HavanaStatus == HavanaNoError)  // A stream has claimed event don't try any more.
                        break;
                }
            }
            OS_UnLockMutex (&Lock);

            if ((EventSignalThreadRunning) && (EventSignalCallback != NULL))
            //{{{  translate from a Player2 event record to the external Player event record.
            {
                Event.timestamp                 = PlayerEvent.PlaybackTime;
                Event.component                 = PlayerEvent.Value[0].Pointer;
                Event.playback                  = PlayerEvent.Playback;
                Event.stream                    = PlayerEvent.Stream;
                switch (PlayerEvent.Code)
                {
                    case EventPlaybackCreated:
                        Event.code              = PLAYER_EVENT_PLAYBACK_CREATED;
                        break;

                    case EventPlaybackTerminated:
                        Event.code              = PLAYER_EVENT_PLAYBACK_TERMINATED;
                        break;

                    case EventStreamCreated:
                        Event.code              = PLAYER_EVENT_STREAM_CREATED;
                        break;

                    case EventStreamTerminated:
                        Event.code              = PLAYER_EVENT_STREAM_TERMINATED;
                        break;

                    case EventStreamSwitched:
                        Event.code              = PLAYER_EVENT_STREAM_SWITCHED;
                        break;

                    case EventStreamDrained:
                        Event.code              = PLAYER_EVENT_STREAM_DRAINED;
                        break;

                    case EventStreamUnPlayable:
                        Event.code              = PLAYER_EVENT_STREAM_UNPLAYABLE;
                        break;

                    case EventFirstFrameManifested:
                        Event.code              = PLAYER_EVENT_FIRST_FRAME_ON_DISPLAY;
                        break;

                    case EventTimeNotification:
                        Event.code              = PLAYER_EVENT_TIME_NOTIFICATION;
                        break;

                    case EventDecodeBufferAvailable:
                        Event.code              = PLAYER_EVENT_DECODE_BUFFER_AVAILABLE;
                        break;

                    case EventInputFormatCreated:
                        Event.code              = PLAYER_EVENT_INPUT_FORMAT_CREATED;
                        break;

                    case EventSupportedInputFormatCreated:
                        Event.code              = PLAYER_EVENT_SUPPORTED_INPUT_FORMAT_CREATED;
                        break;

                    case EventDecodeErrorsCreated:
                        Event.code              = PLAYER_EVENT_DECODE_ERRORS_CREATED;
                        break;

                    case EventSampleFrequencyCreated:
                        Event.code              = PLAYER_EVENT_SAMPLE_FREQUENCY_CREATED;
                        break;

                    case EventNumberChannelsCreated:
                        Event.code              = PLAYER_EVENT_NUMBER_CHANNELS_CREATED;
                        break;

                    case EventNumberOfSamplesProcessedCreated:
                        Event.code              = PLAYER_EVENT_NUMBER_OF_SAMPLES_PROCESSED;
                        break;

                    case EventInputFormatChanged:
                        Event.code              = PLAYER_EVENT_INPUT_FORMAT_CHANGED;
                        break;

                    case EventSourceSizeChangeManifest:
                        Event.code              = PLAYER_EVENT_SIZE_CHANGED;
                        break;

                    case EventSourceFrameRateChangeManifest:
                        Event.code              = PLAYER_EVENT_FRAME_RATE_CHANGED;
                        break;

                    case EventFailedToDecodeInTime:
                        Event.code              = PLAYER_EVENT_FRAME_DECODED_LATE;
                        break;

                    case EventFailedToDeliverDataInTime:
                        Event.code              = PLAYER_EVENT_DATA_DELIVERED_LATE;
                        break;

                    case EventBufferRelease:
                        Event.code              = PLAYER_EVENT_BUFFER_RELEASE;
                        break;

                    case EventTrickModeDomainChange:
                        Event.code              = PLAYER_EVENT_TRICK_MODE_CHANGE;
                        break;

                    case EventTimeMappingEstablished:
                        Event.code              = PLAYER_EVENT_TIME_MAPPING_ESTABLISHED;
                        break;

                    case EventTimeMappingReset:
                        Event.code              = PLAYER_EVENT_TIME_MAPPING_RESET;
                        break;

                    case EventFailureToPlaySmoothReverse:
                        Event.code              = PLAYER_EVENT_REVERSE_FAILURE;
                        break;

                    case EventFatalHardwareFailure:
                        Event.code              = PLAYER_EVENT_FATAL_HARDWARE_FAILURE;
                        break;

                    default:
                        //HAVANA_TRACE("Unexpected event %x\n", PlayerEvent.Code);
                        Event.code              = PLAYER_EVENT_INVALID;
                        //memset (Event, 0, sizeof (struct player_event_s));
                        //return HavanaError;
                }
                //HAVANA_TRACE("Code %x, at 0x%llx\n", Event.code, Event.timestamp);

                if (Event.code != PLAYER_EVENT_INVALID)
                    EventSignalCallback (&Event);
            }
            //}}}  
        }
    }

    OS_SetEvent (&EventSignalThreadTerminated);
    HAVANA_DEBUG ("Terminating\n");
}
//}}}  

