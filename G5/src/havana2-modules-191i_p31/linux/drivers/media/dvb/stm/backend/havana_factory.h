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

Source file name : havana_factory.h
Author :           Julian

Definition of the implementation of havana player module for havana.


Date        Modification                                    Name
----        ------------                                    --------
16-Feb-07   Created                                         Julian

************************************************************************/

#ifndef H_HAVANA_FACTORY
#define H_HAVANA_FACTORY

#include "player.h"
#include "havana_player.h"

/// Player wrapper class responsible for system instanciation.
class HavanaFactory_c
{
private:

    class HavanaFactory_c*      NextFactory;
    const char*                 Id;
    const char*                 SubId;
    PlayerStreamType_t          PlayerStreamType;
    PlayerComponent_t           PlayerComponent;
    unsigned int                FactoryVersion;
    void*                      (*Factory)                      (void);

public:

                                HavanaFactory_c                (void);
                               ~HavanaFactory_c                (void);
    HavanaStatus_t              Init                           (class HavanaFactory_c*  FactoryList,
                                                                const char*             Id,
                                                                const char*             SubId,
                                                                PlayerStreamType_t      StreamType,
                                                                PlayerComponent_t       Component,
                                                                unsigned int            Version,
                                                                void*                  (*NewFactory)   (void));
    HavanaStatus_t              ReLink                         (class HavanaFactory_c*  FactoryList);
    bool                        CanBuild                       (const char*             Id,
                                                                const char*             SubId,
                                                                PlayerStreamType_t      StreamType,
                                                                PlayerComponent_t       Component);
    HavanaStatus_t              Build                          (void**                  Class);
    class HavanaFactory_c*      Next                           (void);
    unsigned int                Version                        (void);

};
#endif

