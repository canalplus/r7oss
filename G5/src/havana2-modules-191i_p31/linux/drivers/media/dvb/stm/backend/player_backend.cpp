/************************************************************************
COPYRIGHT (C) STMicroelectronics 2003

Source file name : player_backend.cpp - backend engine for driving player2
Author :           Julian


Date        Modification                                    Name
----        ------------                                    --------
09-Feb-07   Created                                         Julian

************************************************************************/

#include <asm/errno.h>

#include "backend_ops.h"
#include "alsa_backend_ops.h"
#include "player_interface_ops.h"
#include "fixed.h"
#include "player_module.h"
#include "player_backend.h"
#include "havana_player.h"
#include "player_factory.h"
#include "havana_playback.h"
#include "havana_demux.h"
#include "havana_stream.h"
#include "mixer_mme.h"

static class HavanaPlayer_c*    HavanaPlayer;

#ifdef __cplusplus
extern "C" {
#endif
//{{{  BackendInit
int BackendInit (void)
{
    HavanaStatus_t              Status;

    PLAYER_DEBUG("\n");
    HavanaPlayer        = new HavanaPlayer_c;
    if (HavanaPlayer == NULL)
    {
        PLAYER_ERROR("Unable to create player\n");
        return -ENOMEM;
    }
    Status      = HavanaPlayer->Init ();
    if (Status != HavanaNoError)
    {
        delete HavanaPlayer;
        HavanaPlayer    = NULL;
        return -ENOMEM;
    }

    Status      = RegisterBuiltInFactories (HavanaPlayer);

    return Status;
}
//}}}
//{{{  BackendDelete
int BackendDelete (void)
{
    if (HavanaPlayer != NULL)
        delete HavanaPlayer;
    HavanaPlayer        = NULL;

    return 0;
}
//}}}

//{{{  DemuxInjectData
int DemuxInjectData    (demux_handle_t          Demux,
                        const unsigned char*    Data,
                        unsigned int            DataLength)
{
    class HavanaDemux_c*        HavanaDemux     = (class HavanaDemux_c*)Demux;

    HavanaDemux->InjectData (Data, DataLength);

    return DataLength;
}
//}}}

//{{{  PlaybackCreate
int PlaybackCreate     (playback_handle_t*      Playback)
{
    HavanaStatus_t              Status;
    class HavanaPlayback_c*     HavanaPlayback = NULL;

    PLAYER_DEBUG("\n");
    Status      = HavanaPlayer->CreatePlayback (&HavanaPlayback);
    if (Status != HavanaNoError)
        return -ENOMEM;
    *Playback   = (playback_handle_t)HavanaPlayback;
    return 0;
}
//}}}
//{{{  PlaybackDelete
int PlaybackDelete     (playback_handle_t       Playback)
{
    class HavanaPlayback_c*     HavanaPlayback  = (class HavanaPlayback_c*)Playback;

    PLAYER_DEBUG("\n");
    if (HavanaPlayback == NULL)
        return -EINVAL;

    HavanaPlayer->DeletePlayback (HavanaPlayback);

    return 0;
}
//}}}
//{{{  PlaybackAddDemux
int PlaybackAddDemux           (playback_handle_t       Playback,
                                int                     DemuxId,
                                demux_handle_t*         Demux)
{
    class HavanaPlayback_c*     HavanaPlayback  = (class HavanaPlayback_c*)Playback;
    class HavanaDemux_c*        HavanaDemux     = NULL;
    HavanaStatus_t              Status;

    PLAYER_DEBUG("\n");
    if (HavanaPlayback == NULL)
        return -EINVAL;

    Status      = HavanaPlayback->AddDemux (DemuxId, &HavanaDemux);
    if (Status != HavanaNoError)
        return -ENOMEM;
    *Demux      = (demux_handle_t)HavanaDemux;
    return 0;
}
//}}}
//{{{  PlaybackRemoveDemux
int PlaybackRemoveDemux        (playback_handle_t       Playback,
                                demux_handle_t          Demux)
{
    class HavanaPlayback_c*     HavanaPlayback  = (class HavanaPlayback_c*)Playback;
    class HavanaDemux_c*        HavanaDemux     = (class HavanaDemux_c*)Demux;

    PLAYER_DEBUG("\n");
    if (HavanaPlayback == NULL)
        return -EINVAL;

    if (HavanaPlayback->RemoveDemux (HavanaDemux) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  PlaybackAddStream
int PlaybackAddStream           (playback_handle_t      Playback,
                                 char*                  Media,
                                 char*                  Format,
                                 char*                  Encoding,
                                 unsigned int           SurfaceId,
                                 stream_handle_t*       Stream)
{
    class HavanaPlayback_c*     HavanaPlayback  = (class HavanaPlayback_c*)Playback;
    class HavanaStream_c*       HavanaStream    = NULL;
    HavanaStatus_t              Status          = HavanaNoError;

    PLAYER_DEBUG("%s\n", Media);
    if (HavanaPlayback == NULL)
        return -EINVAL;

    Status      = HavanaPlayback->AddStream (Media, Format, Encoding, SurfaceId, &HavanaStream);
    if (Status == HavanaNoMemory)
        return -ENOMEM;
    else if (Status != HavanaNoError)
        return -ENODEV;

    *Stream     = (stream_handle_t)HavanaStream;
    return 0;
}
//}}}
//{{{  PlaybackRemoveStream
int PlaybackRemoveStream       (playback_handle_t       Playback,
                                stream_handle_t         Stream)
{
    class HavanaPlayback_c*     HavanaPlayback  = (class HavanaPlayback_c*)Playback;
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaPlayback == NULL)
        return -EINVAL;

    if (HavanaPlayback->RemoveStream (HavanaStream) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  PlaybackSetSpeed
int PlaybackSetSpeed            (playback_handle_t       Playback,
                                 int                     Speed)
{
    class HavanaPlayback_c*     HavanaPlayback  = (class HavanaPlayback_c*)Playback;

    PLAYER_DEBUG("\n");
    if (HavanaPlayback == NULL)
        return -EINVAL;

    if (HavanaPlayback->SetSpeed (Speed) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  PlaybackGetSpeed
int PlaybackGetSpeed            (playback_handle_t       Playback,
                                 int*                    PlaySpeed)
{
    class HavanaPlayback_c*     HavanaPlayback  = (class HavanaPlayback_c*)Playback;

    PLAYER_DEBUG("\n");
    if (HavanaPlayback == NULL)
        return -EINVAL;

    if (HavanaPlayback->GetSpeed (PlaySpeed) != HavanaNoError)
        return -EINVAL;

    PLAYER_DEBUG("Speed = %d\n", *PlaySpeed);
    return 0;
}
//}}}
//{{{  PlaybackSetNativePlaybackTime
int PlaybackSetNativePlaybackTime (playback_handle_t    Playback,
                                   unsigned long long   NativeTime,
                                   unsigned long long   SystemTime)
{
    class HavanaPlayback_c*     HavanaPlayback  = (class HavanaPlayback_c*)Playback;

    PLAYER_DEBUG("\n");
    if (HavanaPlayback == NULL)
        return -EINVAL;

    if (HavanaPlayback->SetNativePlaybackTime (NativeTime, SystemTime) != HavanaNoError)
    {
        PLAYER_ERROR("SetNativePlaybackTime failed\n");
        return -ENOMEM;
    }

    return 0;
}
//}}}
//{{{  PlaybackSetOption
int PlaybackSetOption          (playback_handle_t       Playback,
                                play_option_t           Option,
                                unsigned int            Value)
{
    class HavanaPlayback_c*     HavanaPlayback  = (class HavanaPlayback_c*)Playback;

    PLAYER_DEBUG("\n");
    if (HavanaPlayback == NULL)
        return -EINVAL;

    if (HavanaPlayback->SetOption (Option, Value) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  PlaybackGetPlayerEnvironment
int PlaybackGetPlayerEnvironment (playback_handle_t               Playback,
                                  playback_handle_t*              playerplayback)
{
    PlayerPlayback_t            player_playback = NULL;
    class HavanaPlayback_c*     HavanaPlayback  = (class HavanaPlayback_c*)Playback;

    PLAYER_DEBUG("\n");
    if (HavanaPlayback == NULL)
        return -EINVAL;

    if (HavanaPlayback->GetPlayerEnvironment (&player_playback) != HavanaNoError)
        return -EINVAL;

    *playerplayback     = (void *)player_playback;

    return 0;
}
//}}}
//{{{  PlaybackSetClockdataPoint
int PlaybackSetClockDataPoint     (playback_handle_t    Playback,
                                   clock_data_point_t*  DataPoint)
{
    class HavanaPlayback_c*     HavanaPlayback  = (class HavanaPlayback_c*)Playback;

    PLAYER_DEBUG("\n");
    if (HavanaPlayback == NULL)
        return -EINVAL;

    if (HavanaPlayback->SetClockDataPoint (DataPoint) != HavanaNoError)
    {
        PLAYER_ERROR("SetClockDataPoint failed\n");
        return -ENOMEM;
    }

    return 0;
}
//}}}

//{{{  StreamInjectData
int StreamInjectData            (stream_handle_t         Stream,
                                 const unsigned char*    Data,
                                 unsigned int            DataLength)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    HavanaStream->InjectData (Data, DataLength);

    return DataLength;
}
//}}}
//{{{  StreamInjectDataPacket
int StreamInjectDataPacket      (stream_handle_t         Stream,
                                 const unsigned char*    Data,
                                 unsigned int            DataLength,
                                 bool                    PresentationTimeValid,
                                 unsigned long long      PresentationTime )
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    HavanaStream->InjectDataPacket (Data, DataLength, PresentationTimeValid, PresentationTime);

    return DataLength;
}
//}}}
//{{{  StreamDiscontinuity
int StreamDiscontinuity         (stream_handle_t         Stream,
                                 discontinuity_t         Discontinuity)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    //PLAYER_DEBUG("%x %x %x\n", Discontinuity, Discontinuity & DISCONTINUITY_CONTINUOUS_REVERSE, Discontinuity & DISCONTINUITY_SURPLUS_DATA);
    if (HavanaStream->Discontinuity     ((Discontinuity & DISCONTINUITY_CONTINUOUS_REVERSE) != 0,
                                         (Discontinuity & DISCONTINUITY_SURPLUS_DATA) != 0) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamDrain
int StreamDrain                 (stream_handle_t         Stream,
                                 unsigned int            Discard)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    HavanaStream->Drain (Discard);

    return 0;
}
//}}}
//{{{  StreamEnable
int StreamEnable                (stream_handle_t         Stream,
                                 unsigned int            Enable)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->Enable (Enable) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamSetId
int StreamSetId                 (stream_handle_t         Stream,
                                 unsigned int            DemuxId,
                                 unsigned int            Id)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->SetId (DemuxId, Id) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamChannelSelect
int StreamChannelSelect         (stream_handle_t         Stream,
                                 channel_select_t        Channel)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    //PLAYER_DEBUG("%x %x %x\n", Discontinuity, Discontinuity & DISCONTINUITY_CONTINUOUS_REVERSE, Discontinuity & DISCONTINUITY_SURPLUS_DATA);
    if (HavanaStream->ChannelSelect     (Channel) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamSetOption
int StreamSetOption            (stream_handle_t         Stream,
                                play_option_t           Option,
                                unsigned int            Value)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->SetOption (Option, Value) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamGetOption
int StreamGetOption            (stream_handle_t         Stream,
                                play_option_t           Option,
                                unsigned int*           Value)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->GetOption (Option, Value) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamStep
int StreamStep                  (stream_handle_t         Stream)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->Step () != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamSwitch
int StreamSwitch                (stream_handle_t         Stream,
                                 char*                   Format,
                                 char*                   Encoding)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->Switch (Format, Encoding) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamSetAlarm
int StreamSetAlarm              (stream_handle_t         Stream,
                                 unsigned long long      Pts)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamGetPlayInfo
int StreamGetPlayInfo           (stream_handle_t         Stream,
                                 struct play_info_s*     PlayInfo)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->GetPlayInfo (PlayInfo) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamGetDecodeBuffer
int StreamGetDecodeBuffer      (stream_handle_t         Stream,
                                buffer_handle_t*        Buffer,
                                unsigned char**         Data,
                                unsigned int            Format,
                                unsigned int            DimensionCount,
                                unsigned int            Dimensions[],
                                unsigned int*           Index,
                                unsigned int*           Stride)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if ( HavanaStream->GetDecodeBuffer (Buffer, Data, Format, DimensionCount, Dimensions, Index, Stride) != HavanaNoError)
        return -EINVAL;

    return 0;

}
//}}}
//{{{  StreamReturnDecodeBuffer
int StreamReturnDecodeBuffer      (stream_handle_t         Stream,
                                buffer_handle_t*        Buffer)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->ReturnDecodeBuffer (Buffer) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamGetDecodeBufferPoolStatus
int StreamGetDecodeBufferPoolStatus      (stream_handle_t         Stream,
                                          unsigned int*           BuffersInPool,
                                          unsigned int*           BuffersWithNonZeroReferenceCount)

{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->GetDecodeBufferPoolStatus (BuffersInPool,BuffersWithNonZeroReferenceCount) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamSetOutputWindow
int StreamSetOutputWindow      (stream_handle_t         Stream,
                                unsigned int            X,
                                unsigned int            Y,
                                unsigned int            Width,
                                unsigned int            Height)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->SetOutputWindow (X, Y, Width, Height) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamRegisterEventSignalCallback
stream_event_signal_callback StreamRegisterEventSignalCallback (stream_handle_t                 Stream,
                                                                context_handle_t                Context,
                                                                stream_event_signal_callback    Callback)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return NULL;

    return HavanaStream->RegisterEventSignalCallback (Context, Callback);
}
//}}}
//{{{  StreamGetOutputWindow
int StreamGetOutputWindow      (stream_handle_t         Stream,
                                unsigned int*           X,
                                unsigned int*           Y,
                                unsigned int*           Width,
                                unsigned int*           Height)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->GetOutputWindow (X, Y, Width, Height) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamSetInputWindow
int StreamSetInputWindow       (stream_handle_t         Stream,
                                unsigned int            X,
                                unsigned int            Y,
                                unsigned int            Width,
                                unsigned int            Height)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->SetInputWindow (X, Y, Width, Height) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamSetPlayInterval
int StreamSetPlayInterval      (stream_handle_t         Stream,
                                play_interval_t*        PlayInterval)
{
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->SetPlayInterval (PlayInterval) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  StreamGetPlayerEnvironment
int StreamGetPlayerEnvironment (stream_handle_t                 Stream,
                                playback_handle_t*              playerplayback,
                                stream_handle_t*                playerstream)
{
    PlayerPlayback_t            player_playback = NULL;
    PlayerStream_t              player_stream   = NULL;
    class HavanaStream_c*       HavanaStream    = (class HavanaStream_c*)Stream;

    PLAYER_DEBUG("\n");
    if (HavanaStream == NULL)
        return -EINVAL;

    if (HavanaStream->GetPlayerEnvironment (&player_playback, &player_stream) != HavanaNoError)
        return -EINVAL;

    *playerplayback     = (void *)player_playback;
    *playerstream       = (void *)player_stream;

    return 0;
}
//}}}

//{{{  MixerGetInstance
int MixerGetInstance (int StreamId, component_handle_t* Classoid)
{
    const char *BackendId;
    HavanaStatus_t Status;

    //PLAYER_DEBUG("\n");

    switch (StreamId)
    {
    case 0:  BackendId = BACKEND_MIXER0_ID; break;
    case 1:  BackendId = BACKEND_MIXER1_ID; break;
    default: return -EINVAL;
    }

    Status      = HavanaPlayer->CallFactory (BACKEND_AUDIO_ID, BackendId,
                                         StreamTypeAudio, ComponentExternal, Classoid);
    if (Status == HavanaNoMemory)
        return -ENOMEM;
    else if (Status != HavanaNoError)
        return -ENODEV;

    return 0;
}
//}}}
//{{{  ComponentSetModuleParameters
int ComponentSetModuleParameters (component_handle_t Classoid, void *Data, unsigned int Size)
{
    BaseComponentClass_c *Component = (BaseComponentClass_c *) Classoid;
    PlayerStatus_t Status;

    //PLAYER_DEBUG("\n");

    Status = Component->SetModuleParameters(Size, Data);
    if (Status != PlayerNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  MixerAllocSubStream
int MixerAllocSubStream   (component_handle_t Component, int *SubStreamId)
{
    Mixer_Mme_c *Mixer = (Mixer_Mme_c *) Component;
    PlayerStatus_t Status;

    Status = Mixer->AllocInteractiveInput(SubStreamId);
    if (Status != PlayerNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  MixerFreeSubStream
int MixerFreeSubStream    (component_handle_t Component, int SubStreamId)
{
    Mixer_Mme_c *Mixer = (Mixer_Mme_c *) Component;
    PlayerStatus_t Status;

    Status = Mixer->FreeInteractiveInput(SubStreamId);
    if (Status != PlayerNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  MixerSetupSubStream
int MixerSetupSubStream   (component_handle_t Component, int SubStreamId,
                           struct alsa_substream_descriptor *Descriptor)
{
    Mixer_Mme_c *Mixer = (Mixer_Mme_c *) Component;
    PlayerStatus_t Status;

    Status = Mixer->SetupInteractiveInput(SubStreamId, Descriptor);
    if (Status != PlayerNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  MixerPrepareSubStream
int MixerPrepareSubStream (component_handle_t Component, int SubStreamId)
{
    Mixer_Mme_c *Mixer = (Mixer_Mme_c *) Component;
    PlayerStatus_t Status;

    Status = Mixer->PrepareInteractiveInput(SubStreamId);
    if (Status != PlayerNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  MixerStartSubStream
int MixerStartSubStream   (component_handle_t Component, int SubStreamId)
{
    Mixer_Mme_c *Mixer = (Mixer_Mme_c *) Component;
    PlayerStatus_t Status;

    Status = Mixer->EnableInteractiveInput(SubStreamId);
    if (Status != PlayerNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  MixerStopSubstream
int MixerStopSubStream    (component_handle_t Component, int SubStreamId)
{
    Mixer_Mme_c *Mixer = (Mixer_Mme_c *) Component;
    PlayerStatus_t Status;

    Status = Mixer->DisableInteractiveInput(SubStreamId);
    if (Status != PlayerNoError)
        return -EINVAL;

    return 0;
}
//}}}

//{{{  DisplayCreate
int DisplayCreate      (char*           Media,
                        unsigned int    SurfaceId)
{
    class HavanaDisplay_c*      Display;

    PLAYER_DEBUG("SurfaceId  = %d\n", SurfaceId);

    if (HavanaPlayer->CreateDisplay (Media, SurfaceId, &Display) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  DisplayDelete
int DisplayDelete      (char*           Media,
                        unsigned int    SurfaceId)
{

    PLAYER_DEBUG("SurfaceId  = %d\n", SurfaceId);

    if (HavanaPlayer->DeleteDisplay (Media, SurfaceId) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}

//{{{  DisplaySynchronize
int DisplaySynchronize (char*           Media,
                        unsigned int    SurfaceId)
{
    if (HavanaPlayer->SynchronizeDisplay (Media, SurfaceId) != HavanaNoError)
        return -EINVAL;

    return 0;
}
//}}}

//{{{  ComponentGetAttribute
int ComponentGetAttribute      (player_component_handle_t       Component,
                                const char*                     Attribute,
                                union attribute_descriptor_u*   Value)
{
    BaseComponentClass_c*               PlayerComponent = (BaseComponentClass_c*)Component;
    PlayerStatus_t                      Status;
    PlayerAttributeDescriptor_t         AttributeDescriptor;

    //PLAYER_DEBUG("\n");

    Status      = PlayerComponent->GetAttribute (Attribute, &AttributeDescriptor);
    if (Status != PlayerNoError)
        return -EINVAL;

    switch (AttributeDescriptor.Id)
    {
        case SYSFS_ATTRIBUTE_ID_INTEGER:
            Value->Int                      = (int)AttributeDescriptor.u.Int;
            break;
        case SYSFS_ATTRIBUTE_ID_BOOL:
            Value->Bool                     = (int)AttributeDescriptor.u.Bool;
            break;
        case SYSFS_ATTRIBUTE_ID_UNSIGNEDLONGLONGINT:
            Value->UnsignedLongLongInt      = (unsigned long long int)AttributeDescriptor.u.UnsignedLongLongInt;
            break;
        case SYSFS_ATTRIBUTE_ID_CONSTCHARPOINTER:
            Value->ConstCharPointer         = (char*)AttributeDescriptor.u.ConstCharPointer;
            break;
        default:
            PLAYER_ERROR("This attribute does not exist.\n");
            return -EINVAL;
    }

    return 0;
}
//}}}
//{{{  ComponentSetAttribute
int ComponentSetAttribute      (player_component_handle_t       Component,
                                const char*                     Attribute,
                                union attribute_descriptor_u*   Value)
{
    BaseComponentClass_c*       PlayerComponent = (BaseComponentClass_c*)Component;
    PlayerStatus_t              Status;

    //PLAYER_DEBUG("\n");

    Status      = PlayerComponent->SetAttribute (Attribute, (PlayerAttributeDescriptor_t*)Value);
    if (Status != PlayerNoError)
        return -EINVAL;

    return 0;
}
//}}}
//{{{  COMMENT ComponentSetModuleParameters
#if 0
int ComponentSetModuleParameters (player_component_handle_t     Component,
                                  void*                         Data,
                                  unsigned int                  Size)
{
    BaseComponentClass_c*       Component       = (BaseComponentClass_c*)Component;
    PlayerStatus_t              Status;

    PLAYER_DEBUG("\n");
    Status      = Component->SetModuleParameters (Size, Data);
    if (Status != PlayerNoError)
        return -EINVAL;

    return 0;
}
#endif
//}}}
//{{{  PlayerRegisterEventSignalCallback
player_event_signal_callback PlayerRegisterEventSignalCallback (player_event_signal_callback Callback)
{
    PLAYER_DEBUG("\n");

    return HavanaPlayer->RegisterEventSignalCallback (Callback);
}
//}}}

#ifdef __cplusplus
}
#endif
//{{{  C++ operators
#if defined (__KERNEL__)
#include "osinline.h"

////////////////////////////////////////////////////////////////////////////
// operator new
////////////////////////////////////////////////////////////////////////////

void* operator new (unsigned int size)
{
#if 0
    void* Addr = __builtin_new (size);
    HAVANA_DEBUG ("Newing %d (%x) at %p\n", size, size, Addr);
    return Addr;
#else
    return __builtin_new (size);
#endif
}

////////////////////////////////////////////////////////////////////////////
// operator delete
////////////////////////////////////////////////////////////////////////////

void operator delete (void *mem)
{
#if 0
    HAVANA_DEBUG ("Deleting (%p)\n", mem);
#else
    __builtin_delete (mem);
#endif
}

////////////////////////////////////////////////////////////////////////////
// operator new
////////////////////////////////////////////////////////////////////////////

void* operator new[] (unsigned int size)
{
#if 0
    void* Addr = __builtin_vec_new (size);
    HAVANA_DEBUG ("Vec newing %d (%x) at %p\n", size, size, Addr);
    return Addr;
#else
    return __builtin_vec_new (size);
#endif
}

////////////////////////////////////////////////////////////////////////////
//   operator delete
////////////////////////////////////////////////////////////////////////////

void operator delete[] (void *mem)
{
#if 0
    HAVANA_DEBUG ("Vec deleting (%p)\n", mem);
#else
    __builtin_vec_delete (mem);
#endif
}
#endif
//}}}
