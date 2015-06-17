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

////////////////////////////////////////////////////////////////////////////
// \class Coder_Capabilities_c
//
// This class allows client to access audio & video encoder capabilities.
// This class collects capabilities of each coder to expose it to client.

#ifndef H_CODER_CAPABILITIES
#define H_CODER_CAPABILITIES

////////////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "osinline.h"
#include "stm_se.h"

class Coder_Capabilities_c
{
public:
    static int GetAudioEncodeCapabilities(stm_se_audio_enc_capability_t *decCap);
    static int GetVideoEncodeCapabilities(stm_se_video_enc_capability_t *decCap);
};

#endif //H_CODER_CAPABILITIES
