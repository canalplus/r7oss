/************************************************************************
COPYRIGHT (C) STMicroelectronics 2005

Source file name : havana_demux.cpp - derived from havana_player.cpp
Author :           Julian

Implementation of the demux module for havana.


Date        Modification                                    Name
----        ------------                                    --------
14-Feb-07   Created                                         Julian

************************************************************************/

#include "osdev_user.h"
#include "mpeg2.h"
#include "backend_ops.h"
#include "player.h"
#include "havana_player.h"
#include "havana_playback.h"
#include "havana_demux.h"
#include "display.h"

//{{{  HavanaDemux_c
HavanaDemux_c::HavanaDemux_c (void)
{
    DEMUX_DEBUG("\n");

    Player              = NULL;
    PlayerPlayback      = NULL;
    DemuxContext        = NULL;
}
//}}}  
//{{{  ~HavanaDemux_c
HavanaDemux_c::~HavanaDemux_c (void)
{
    DEMUX_DEBUG("\n");

    Player              = NULL;
    PlayerPlayback      = NULL;
    DemuxContext        = NULL;

    OS_TerminateMutex (&InputLock);
}
//}}}  
//{{{  Init
//{{{  doxynote
/// \brief Create and initialise all of the necessary player components for a new player stream
/// \param Player               The player
/// \param PlayerPlayback       The player playback to which the stream will be added
/// \param DemultiplexorContext Context variable used to manipulate the demultiplexor (TS).
/// \return                     Havana status code, HavanaNoError indicates success.
//}}}  
HavanaStatus_t HavanaDemux_c::Init     (class Player_c*         Player,
                                        PlayerPlayback_t        PlayerPlayback,
                                        DemultiplexorContext_t  DemultiplexorContext)
{
    DEMUX_DEBUG("\n");

    this->Player                = Player;
    this->PlayerPlayback        = PlayerPlayback;
    this->DemuxContext          = DemultiplexorContext;

    if (OS_InitializeMutex (&InputLock) != OS_NO_ERROR)
    {
        DEMUX_ERROR ("Failed to initialize InputLock mutex\n");
        return HavanaNoMemory;
    }

    return HavanaNoError;
}
//}}}  
//{{{  InjectData
HavanaStatus_t HavanaDemux_c::InjectData       (const unsigned char*            Data,
                                                unsigned int                    DataLength)
{
    Buffer_t                    Buffer;
    PlayerInputDescriptor_t*    InputDescriptor;

    //OS_LockMutex (&InputLock);
    Player->GetInjectBuffer (&Buffer);
    Buffer->ObtainMetaDataReference (Player->MetaDataInputDescriptorType, (void**)&InputDescriptor);

    InputDescriptor->MuxType                    = MuxTypeTransportStream;
    InputDescriptor->DemultiplexorContext       = DemuxContext;

    InputDescriptor->PlaybackTimeValid          = false;
    InputDescriptor->DecodeTimeValid            = false;
    InputDescriptor->DataSpecificFlags          = 0;

    Buffer->RegisterDataReference (DataLength, (void*)Data);
    Buffer->SetUsedDataSize (DataLength);

    Player->InjectData (PlayerPlayback, Buffer);
    //OS_UnLockMutex (&InputLock);

    return HavanaNoError;
}
//}}}  

