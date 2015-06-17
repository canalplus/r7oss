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

#ifndef H_COLLATOR_PES_AUDIO_MPEG
#define H_COLLATOR_PES_AUDIO_MPEG

#include "collator_pes_audio.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator_PesAudioMpeg_c"

class Collator_PesAudioMpeg_c : public Collator_PesAudio_c
{
public:
    Collator_PesAudioMpeg_c(void);

private:
    void             SetPesPrivateDataLength(unsigned char SpecificCode);

    CollatorStatus_t FindNextSyncWord(int *CodeOffset);
    CollatorStatus_t DecideCollatorNextStateAndGetLength(unsigned int *FrameLength);
};

#endif // H_COLLATOR_PES_AUDIO_MPEG
