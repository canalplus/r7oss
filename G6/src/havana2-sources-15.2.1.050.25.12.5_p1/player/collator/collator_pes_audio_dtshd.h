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

#ifndef H_COLLATOR_PES_AUDIO_DTSHD
#define H_COLLATOR_PES_AUDIO_DTSHD

#include "collator_pes_audio_dvd.h"
#include "dtshd.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator_PesAudioDtshd_c"

class Collator_PesAudioDtshd_c : public Collator_PesAudioDvd_c
{
public:
    Collator_PesAudioDtshd_c(void);

private:
    unsigned int     CoreFrameSize;
    bool             GotCoreFrameSize;

    bool             EightChannelsRequired; ///< Holds the output channel required configuration

    DtshdAudioSyncFrameHeader_t SyncFrameHeader;

    BitStreamClass_c Bits;                  ///< will be used to parse frame header

    CollatorStatus_t FindNextSyncWord(int *CodeOffset);
    CollatorStatus_t FindAnyNextSyncWord(int *CodeOffset, DtshdStreamType_t *Type);

    CollatorStatus_t DecideCollatorNextStateAndGetLength(unsigned int *FrameLength);
    void             GetStreamType(unsigned char *Header, DtshdAudioParsedFrameHeader_t *ParsedFrameHeader);
    void             SetPesPrivateDataLength(unsigned char SpecificCode);
    CollatorStatus_t HandlePesPrivateData(unsigned char *PesPrivateData);
    CollatorStatus_t GetSpecificFrameLength(unsigned int *FrameLength);
};

#endif // H_COLLATOR_PES_AUDIO_DTSHD
