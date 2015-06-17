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

#ifndef H_COLLATOR_PES_AUDIO_MLP
#define H_COLLATOR_PES_AUDIO_MLP

#include "collator_pes_audio_dvd.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator_PesAudioMlp_c"

class Collator_PesAudioMlp_c : public Collator_PesAudioDvd_c
{
public:
    Collator_PesAudioMlp_c(void);

private:
    int AccumulatedFrameNumber;   ///< Holds the number of audio frames accumulated
    int NbFramesToGlob;           ///< Number of audio frames to glob (depends on sampling frequency)
    int StuffingBytesLength;      ///< Number of stuffing bytes in the pda (concerns MLP in DVD-Audio)

    CollatorStatus_t FindNextSyncWord(int *CodeOffset);
    CollatorStatus_t DecideCollatorNextStateAndGetLength(unsigned int *FrameLength);
    void             SetPesPrivateDataLength(unsigned char SpecificCode);
    CollatorStatus_t HandlePesPrivateData(unsigned char *PesPrivateData);
};

#endif // H_COLLATOR_PES_AUDIO_MLP
