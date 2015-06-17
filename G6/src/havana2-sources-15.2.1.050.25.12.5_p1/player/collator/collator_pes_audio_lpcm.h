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

#ifndef H_COLLATOR_PES_AUDIO_LPCM
#define H_COLLATOR_PES_AUDIO_LPCM

#include "collator_pes_audio.h"
#include "lpcm.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator_PesAudioLpcm_c"

class Collator_PesAudioLpcm_c : public Collator_PesAudio_c
{
public:
    explicit Collator_PesAudioLpcm_c(LpcmStreamType_t StreamType);

    LpcmStreamType_t GetStreamType(void) const { return StreamType; }

private:
    LpcmStreamType_t             StreamType;

    LpcmAudioParsedFrameHeader_t ParsedFrameHeader;
    LpcmAudioParsedFrameHeader_t NextParsedFrameHeader;
    unsigned int                 GuessedNextFirstAccessUnit; ///< In debug mode, try to guess the next first access unit pointer
    bool                         IsPesPrivateDataAreaNew;    ///< Indicates if the private dara area of the pes has some key parameters different from the previous one
    bool                         AccumulatePrivateDataArea;  ///< Indicates if it there is a ned to store the pda at the beginning of the frame
    bool                         IsPesPrivateDataAreaValid;  ///< Validity of Private Data Area
    bool                         IsFirstPacket;

    unsigned char                NewPesPrivateDataArea[LPCM_MAX_PRIVATE_HEADER_LENGTH];
    int                          AccumulatedFrameNumber;
    unsigned char                StreamId;
    unsigned int                 RemainingDataLength;
    unsigned int                 PesPrivateToSkip;
    int                          GlobbedFramesOfNewPacket;

    CollatorStatus_t FindNextSyncWord(int *CodeOffset);

    CollatorStatus_t DecideCollatorNextStateAndGetLength(unsigned int *FrameLength);
    void             SetPesPrivateDataLength(unsigned char SpecificCode);
    CollatorStatus_t HandlePesPrivateData(unsigned char *PesPrivateData);
    void             ResetCollatorStateAfterForcedFrameFlush();
};

#endif // H_COLLATOR_PES_AUDIO_LPCM
