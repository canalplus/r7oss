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

#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Player_Generic_c"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Get a buffer to inject input data
//

PlayerStatus_t   Player_Generic_c::GetInjectBuffer(Buffer_t             *Buffer)
{
    PlayerStatus_t Status = InputBufferPool->GetBuffer(Buffer, IdentifierGetInjectBuffer);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to get an input buffer\n");
        return PlayerError;
    }

    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Inject input data to the player
//

PlayerStatus_t   Player_Generic_c::InjectData(PlayerPlayback_t          Playback,
                                              Buffer_t                  Buffer)
{
    unsigned int              Length;
    void                     *Data;
    PlayerInputDescriptor_t  *Descriptor;

    Buffer->ObtainMetaDataReference(MetaDataInputDescriptorType, (void **)(&Descriptor));
    SE_ASSERT(Descriptor != NULL);

    //
    // call the appropriate collator
    //
    Buffer->ObtainDataReference(NULL, &Length, &Data);
    if (Data == NULL)
    {
        SE_ERROR("Unable to obtain data reference\n");
        Buffer->DecrementReferenceCount(IdentifierGetInjectBuffer);
        return PlayerError;
    }

    PlayerStatus_t Status = Descriptor->UnMuxedStream->GetCollator()->Input(Descriptor, Length, Data);

    //
    // Release the buffer
    //
    Buffer->DecrementReferenceCount(IdentifierGetInjectBuffer);
    return Status;
}
