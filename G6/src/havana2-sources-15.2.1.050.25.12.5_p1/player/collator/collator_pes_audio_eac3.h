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

#ifndef H_COLLATOR_PES_AUDIO_EAC3
#define H_COLLATOR_PES_AUDIO_EAC3

#include "collator_pes_audio_dvd.h"
#include "eac3_audio.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator_PesAudioEAc3_c"

class Collator_PesAudioEAc3_c : public Collator_PesAudioDvd_c
{
public:
    Collator_PesAudioEAc3_c(void);

private:
    bool             EightChannelsRequired; ///< Holds the output channel required configuration
    int              ProgrammeId;           ///< Identifier if the eac3 programme to be decoded
    int              NbAccumulatedSamples;  ///< The collator has to accumulate 1536 samples so that the eac3->ac3 transcoding is possible (for spdif output)
    ///< The collator has to accumulate 1536 samples of the selected substream so that the eac3->ac3 transcoding is possible (for spdif output) if needed.
    int              SelectedSubStreamNbAccumulatedSamples;

    bool             FirstBlockConvSync;    ///< ConvSync found on first block
    bool             InvalidBSID;
    unsigned int     DeltaLength;  // Delat of two probale farme sizes in case of invalid BSID

    CollatorStatus_t FindNextSyncWord(int *CodeOffset);

    CollatorStatus_t DecideCollatorNextStateAndGetLength(unsigned int *FrameLength);
    void             GetStreamType(unsigned char *Header, EAc3AudioParsedFrameHeader_t *ParsedFrameHeader);
    void             SetPesPrivateDataLength(unsigned char SpecificCode);
    CollatorStatus_t HandlePesPrivateData(unsigned char *PesPrivateData);
    CollatorStatus_t HandleProfileMSxx(EAc3AudioParsedFrameHeader_t *ParsedFrameHeader);
    CollatorStatus_t HandleProfileNonMSxx(EAc3AudioParsedFrameHeader_t *ParsedFrameHeader);
    void             ReadSubframe(EAc3AudioParsedFrameHeader_t *ParsedFrameHeader);
    void             UpdateAccumulatedSamplesAndChangeState(CollatorState_t NextCollatorState, EAc3AudioParsedFrameHeader_t *ParsedFrameHeader)
    {
        CollatorState        = NextCollatorState;
        NbAccumulatedSamples = ParsedFrameHeader->NumberOfSamples;
        FirstBlockConvSync   = ParsedFrameHeader->FirstBlockForTranscoding;

        if (CollatorState == GotCompleteFrame)
        {
            SelectedSubStreamNbAccumulatedSamples = 0;
            ResetDvdSyncWordHeuristics();
        }
    }
};

#endif // H_COLLATOR_PES_AUDIO_EAC3
