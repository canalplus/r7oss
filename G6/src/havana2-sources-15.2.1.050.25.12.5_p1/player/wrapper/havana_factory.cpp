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
#include "havana_player.h"
#include "havana_factory.h"

HavanaFactory_c::HavanaFactory_c(void)
    : NextFactory(NULL)
    , PlayerComponent(ComponentPlayer)
    , PlayerStreamType(StreamTypeNone)
    , Encoding(STM_SE_STREAM_ENCODING_AUDIO_NONE)
    , FactoryVersion(0)
    , Factory(NULL)
{
    SE_VERBOSE(group_havana, "\n");
}

HavanaFactory_c::~HavanaFactory_c(void)
{
    SE_VERBOSE(group_havana, "\n");
}

HavanaStatus_t HavanaFactory_c::Init(class HavanaFactory_c  *FactoryList,
                                     PlayerComponent_t       Component,
                                     PlayerStreamType_t      StreamType,
                                     stm_se_stream_encoding_t Encoding,
                                     unsigned int            Version,
                                     void *(*NewFactory)(void))
{
    SE_VERBOSE(group_havana, "\n");
    NextFactory         = FactoryList;
    PlayerComponent     = Component;
    PlayerStreamType    = StreamType;
    this->Encoding      = Encoding;
    FactoryVersion      = Version;
    Factory             = NewFactory;
    return HavanaNoError;
}

HavanaStatus_t HavanaFactory_c::ReLink(class HavanaFactory_c  *Next)
{
    SE_VERBOSE(group_havana, "\n");
    NextFactory         = Next;
    return HavanaNoError;
}

//{{{  doxynote
/// \brief Determine whether we are able to build a component of the required type
/// \param Id           Identifier use to distinguish component from other similar components
///                     e.g. mpeg2 for a frame parser
/// \param SubId        Extra identifier use to distinguish component from other similar components
///                     e.g. mpeg2 for an audio pes collator from an lpcm audio pes collator
/// \param StreamType   Player type to indicate audio or video
/// \param Component    Player class to be constructed
/// \return             true if can build the component, false if not
//}}}
bool HavanaFactory_c::CanBuild(PlayerComponent_t       Component,
                               PlayerStreamType_t      StreamType,
                               stm_se_stream_encoding_t Encoding)
{
    SE_VERBOSE(group_havana, "Looking for %x, %x, %x Am %x, %x, %x\n", Component, StreamType, Encoding,
               PlayerComponent, PlayerStreamType, this->Encoding);

    if ((Encoding == this->Encoding) && (StreamType == PlayerStreamType) && (Component == PlayerComponent))
    {
        return true;
    }
    else
    {
        return false;
    }
}

HavanaStatus_t HavanaFactory_c::Build(void  **Class)
{
    SE_VERBOSE(group_havana, "\n");
    *Class = Factory();
    if (*Class == NULL)
    {
        return HavanaNoMemory;
    }

    return HavanaNoError;
}

class HavanaFactory_c *HavanaFactory_c::Next(void)
{
    return NextFactory;
}

unsigned int HavanaFactory_c::Version(void)
{
    return FactoryVersion;
}
