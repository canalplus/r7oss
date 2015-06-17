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
#ifndef H_MIXER_PCMPROCTUNING
#define H_MIXER_PCMPROCTUNING

#include <mme.h>
#include <ACC_Transformers/Pcm_PostProcessingTypes.h>
#include <ACC_Transformers/Pcm_TuningProcessingTypes.h>
#include "player_types.h"
#include "acc_mme.h"

struct PcmProcManagerParams_s
{
    struct MME_PcmProcParamsHeader_s   *ProfileAddr;
    int                                 ResetProcId;
    int                                 ResetProcSize;
};

void SetMixerTuningProfile(struct MME_PcmProcParamsHeader_s *NewPcmProcParam,
                           struct PcmProcManagerParams_s *StoredPcmProcParam);

void PrepareMixerPcmProcTuningCommand(struct PcmProcManagerParams_s *PcmProcParam,
                                      int amount,
                                      enum eAccMixOutput chain,
                                      uint8_t *ptr, unsigned int *size);
#endif //H_MIXER_PCMPROCTUNING
