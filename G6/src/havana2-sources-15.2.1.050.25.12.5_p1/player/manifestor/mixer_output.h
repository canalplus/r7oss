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

#ifndef H_MIXER_OUTPUT_CLASS
#define H_MIXER_OUTPUT_CLASS

#include "acc_mme.h"
#include <ACC_Transformers/acc_mmedefines.h>

#include "report.h"
#include "pcmplayer.h"

#undef TRACE_TAG
#define TRACE_TAG   "Mixer_Output_c"

/// Helper for mixer_player. This aggregates PCM Player configuration.
class Mixer_Output_c
{
public:
    PcmPlayerSurfaceParameters_t SurfaceParameters;
    void *MappedSamples;
    enum eAccAcMode LastInputMode; ///< Used to suppress duplicative messages
    enum eAccAcMode LastOutputMode; ///< Used to suppress duplicative messages

    inline Mixer_Output_c(void)
        : SurfaceParameters()
        , MappedSamples(NULL)
        , LastInputMode(ACC_MODE_ID)
        , LastOutputMode(ACC_MODE_ID)
    {
        SE_DEBUG(group_mixer, "<: %p\n", this);
    }

    inline ~Mixer_Output_c(void)
    {
        SE_DEBUG(group_mixer, "<: %p\n", this);
    }

    inline void Reset()
    {
        memset(&SurfaceParameters, 0, sizeof(SurfaceParameters));
        MappedSamples = NULL;
        LastInputMode = ACC_MODE_ID;
        LastOutputMode = ACC_MODE_ID;
        SE_DEBUG(group_mixer, "<: %p\n", this);
    }

private:
    DISALLOW_COPY_AND_ASSIGN(Mixer_Output_c);
};

#endif // H_MIXER_OUTPUT_CLASS
