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

#include "mixer_pcmproctuning.h"

#undef TRACE_TAG
#define TRACE_TAG   "mixer_pcmproctuning"

void SetMixerTuningProfile(struct MME_PcmProcParamsHeader_s *NewPcmProcParam,
                           struct PcmProcManagerParams_s *StoredPcmProcParam)
{
    /* TODO(JGI)
     * Query the audio FW to retrieve the expected size for this PCM processing
     * to check if StructSize declared in this profile is consistent */

    /* If the new requested processing type is different from previous one
     * We need to disable the old one before applying the new */
    if (StoredPcmProcParam->ProfileAddr != NULL)
    {
        if (ACC_PCMPROC_ID(StoredPcmProcParam->ProfileAddr->Id) != ACC_PCMPROC_ID(NewPcmProcParam->Id))
        {
            StoredPcmProcParam->ResetProcId   = ACC_PCMPROC_ID(StoredPcmProcParam->ProfileAddr->Id);
            StoredPcmProcParam->ResetProcSize = StoredPcmProcParam->ProfileAddr->StructSize;
            SE_INFO(group_mixer, "Request Reset PCM PROC Id:%2d [chain:%2d size:%4d]\n",
                    ACC_PCMPROC_ID(StoredPcmProcParam->ResetProcId),
                    ACC_PCMPROC_SUBID(StoredPcmProcParam->ResetProcId),
                    StoredPcmProcParam->ResetProcSize);
        }
    }
    StoredPcmProcParam->ProfileAddr = NewPcmProcParam;
}

void PrepareMixerPcmProcTuningCommand(struct PcmProcManagerParams_s *PcmProcParam,
                                      int amount,
                                      enum eAccMixOutput chain,
                                      uint8_t *ptr, unsigned int *size)
{
    if (PcmProcParam->ResetProcId)
    {
        SE_INFO(group_mixer, "Reset PCM PROC Id:%2d [chain:%2d size:%4d]\n",
                ACC_PCMPROC_ID(PcmProcParam->ResetProcId), chain, PcmProcParam->ResetProcSize);

        struct MME_PcmProcParamsHeader_s param;
        param.Id = PCMPROCESS_SET_ID(PcmProcParam->ResetProcId, chain);
        param.StructSize = PcmProcParam->ResetProcSize;
        param.Apply      = ACC_MME_DISABLED;

        memset(ptr + *size, 0, PcmProcParam->ResetProcSize);
        memcpy(ptr + *size, &param, sizeof(struct MME_PcmProcParamsHeader_s));
        *size += PcmProcParam->ResetProcSize;

        PcmProcParam->ResetProcId = 0;
    }

    if (PcmProcParam->ProfileAddr)
    {
        SE_INFO(group_mixer, "Set PCM PROC Id:%2d [chain:%2d size:%4d amount:%d[0-100]]\n",
                ACC_PCMPROC_ID(PcmProcParam->ProfileAddr->Id),
                ACC_PCMPROC_SUBID(PcmProcParam->ProfileAddr->Id),
                PcmProcParam->ProfileAddr->StructSize,
                amount);

        int processingId = ACC_PCMPROC_ID(PcmProcParam->ProfileAddr->Id);
        PcmProcParam->ProfileAddr->Id = PCMPROCESS_SET_ID(processingId, chain);

        memcpy(ptr + *size, PcmProcParam->ProfileAddr, PcmProcParam->ProfileAddr->StructSize);
        *size += PcmProcParam->ProfileAddr->StructSize;

        /* Set amount for this processing*/
        MME_TuningGlobalParams_t tuningGlobalParams;
        tuningGlobalParams.Id         = PCMPROCESS_SET_ID(processingId, chain);
        tuningGlobalParams.StructSize = sizeof(MME_TuningGlobalParams_t);
        tuningGlobalParams.Apply      = ACC_MME_ENABLED;
        tuningGlobalParams.Version    = SET_GLOBAL_TUNING_VERSION;
        tuningGlobalParams.Amount     = amount;

        memcpy(ptr + *size, &tuningGlobalParams, tuningGlobalParams.StructSize);
        *size += tuningGlobalParams.StructSize;
    }
}
