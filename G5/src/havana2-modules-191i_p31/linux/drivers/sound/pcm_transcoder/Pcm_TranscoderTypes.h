/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : codec_mme_audio_pcm.cpp
Author :           Julian

Implementation of the pcm audio codec mme interface


Date            Modification            Name
----            ------------            --------
12-Aug-09       Created                 Julian

************************************************************************/

#ifndef _PCMAUDIO_DECODERTYPES_H_
#define _PCMAUDIO_DECODERTYPES_H_

#include "mme.h"
#include "ACC_Transformers/acc_mmedefines.h"
#include "ACC_Transformers/Audio_DecoderTypes.h"

/* Defines the Maximum Scatter Pages expected in Real Audio in one Databuffer of a Transform Command */
#define PCM_NB_SCATTERPAGES                                            (4) // 4 * 256 may be used

#define PCM_MME_TRANSFORMER_NAME        "PCM_TRANSCODER"

#define PCM_LITTLE_ENDIAN               0
#define PCM_BIG_ENDIAN                  1

typedef struct 
{
        enum eAccDecoderId   DecoderId;
        unsigned int         StructSize;
        unsigned int         ChannelCount;
        unsigned int         SampleRate;
        unsigned int         BytesPerSecond;
        unsigned int         BlockAlign;
        unsigned int         BitsPerSample;
        unsigned int         DataEndianness;
}MME_LxPcmAudioConfig_t;

typedef struct
{
        unsigned int                       StructSize; //!< Size of this structure
        MME_LxPcmAudioConfig_t             PcmAudioConfig;  //!< Private Config of Pcm transcoder
} MME_LxPcmAudioTransformerGlobalParams_t;


#endif /* _PCMAUDIO_DECODERTYPES_H_ */
