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

#ifndef H_MIXER_CLASS
#define H_MIXER_CLASS

#include "player_types.h"

class Mixer_c : public BaseComponentClass_c
{
public:
    enum InputState_t
    {
        /// There is no manifestor connected to this input.
        DISCONNECTED,

        /// Neither the input nor the output side are running.
        STOPPED,

        /// Manifestor has been enabled but we have yet to receive parameters from it.
        UNCONFIGURED,

        /// Manifestor has been configured and we are waiting for the mixer loop to start using it.
        STARTING,

        /// The output side is primed and ready but might not be sending (muted)
        /// samples to the speakers yet.
        STARTED,

        /// Manifestor has been disabled but we have not yet stopped mixing from it.
        STOPPING,
    };

    static inline const char *LookupInputState(InputState_t state)
    {
        switch (state)
        {
#define E(x) case x: return #x
            E(DISCONNECTED);
            E(STOPPED);
            E(UNCONFIGURED);
            E(STARTING);
            E(STARTED);
            E(STOPPING);
#undef E

        default:
            return "INVALID";
        }
    }
};

#endif // H_MIXER_CLASS
