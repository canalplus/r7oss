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

#ifndef H_HAVANA_FACTORY
#define H_HAVANA_FACTORY

#include "player.h"
#include "havana_player.h"

class HavanaFactory_c
{
public:
    HavanaFactory_c(void);
    ~HavanaFactory_c(void);

    HavanaStatus_t              Init(class HavanaFactory_c  *FactoryList,
                                     PlayerComponent_t       Component,
                                     PlayerStreamType_t      StreamType,
                                     stm_se_stream_encoding_t Encoding,
                                     unsigned int            Version,
                                     void *(*NewFactory)(void));

    HavanaStatus_t              ReLink(class HavanaFactory_c  *FactoryList);

    bool                        CanBuild(PlayerComponent_t       Component,
                                         PlayerStreamType_t      StreamType,
                                         stm_se_stream_encoding_t Encoding);
    HavanaStatus_t              Build(void                  **Class);

    class HavanaFactory_c      *Next(void);
    unsigned int                Version(void);

private:
    class HavanaFactory_c      *NextFactory;
    PlayerComponent_t           PlayerComponent;
    PlayerStreamType_t          PlayerStreamType;
    stm_se_stream_encoding_t    Encoding;
    unsigned int                FactoryVersion;
    void                      *(*Factory)(void);

    DISALLOW_COPY_AND_ASSIGN(HavanaFactory_c);
};

#endif
