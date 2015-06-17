/************************************************************************
Copyright (C) 2007, 2009 STMicroelectronics. All Rights Reserved.

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
 * V4L2 dvp output device driver for ST SoC display subsystems header file
************************************************************************/

#ifndef DVB_DVP_AUDIO_MME_H_
#define DVB_DVP_AUDIO_MME_H_

#include "allocinline.h"

#include "acc_mme.h"

#include "ACC_Transformers/acc_mmedefines.h"
#include "ACC_Transformers/DTS_DecoderTypes.h"
#include "ACC_Transformers/Pcm_PostProcessingTypes.h"
#include "ACC_Transformers/PcmProcessing_DecoderTypes.h"
#include "ACC_Transformers/Spdifin_DecoderTypes.h"
#include "ACC_Transformers/Lowlatency_TransformerTypes.h"
#include "ACC_Transformers/Perf_TransformerTypes.h"

/* Extract the four extended capability bits for a given codec */
#define ACC_DECODER_CAPABILITY_EXT_FLAGS(hdinfo, decoder) \
	(((hdinfo)->DecoderCapabilityExtFlags[((int) (decoder)) >> 3] >> (4 * ((decoder) & 7))) & 0xf)

/* Keep working against BL025_10 until we can retire it */
#ifndef DRV_MULTICOM_PERFLOG_VERSION
#define DRV_MULTICOM_PERFLOG_VERSION 0
typedef enum
{
       POSTMORTEM_RUNNING,
       POSTMORTEM_CRASH,
       POSTMORTEM_LIVELOCK,
       POSTMORTEM_DEADLOCK,

       POSTMORTEM_TRAPPED, //!< Transient state to ensure we catch exceptions within the exception handler.
} MME_TimeLogPostMortemStatus_t;
typedef struct { MME_TimeLogPostMortemStatus_t Status; } MME_TimeLogPostMortem_t;
#endif

#define AVR_LOW_LATENCY_MAX_OUTPUT_CARDS 4
#define AVR_LOW_LATENCY_MAX_INPUT_CARDS 2

#define AVR_LOW_LATENCY_MAX_SIMULTANEOUS_BUFFERS 3

// defines the maximum number of buffers used to log the acquired data from the pcm reader (by the low latency transformer)
#define AVR_LOW_LATENCY_MAX_LOG_INPUT_SEND_BUFFERS (AVR_LOW_LATENCY_MAX_INPUT_CARDS * AVR_LOW_LATENCY_MAX_SIMULTANEOUS_BUFFERS)
// defines the maximum number of buffers used to log the data that will be emitted to the pcm players (by the low latency transformer)
#define AVR_LOW_LATENCY_MAX_LOG_OUTPUT_SEND_BUFFERS (AVR_LOW_LATENCY_MAX_OUTPUT_CARDS * AVR_LOW_LATENCY_MAX_SIMULTANEOUS_BUFFERS)

#define AVR_LOW_LATENCY_MAX_TOTAL_SEND_BUFFERS (AVR_LOW_LATENCY_MAX_LOG_INPUT_SEND_BUFFERS + AVR_LOW_LATENCY_MAX_LOG_OUTPUT_SEND_BUFFERS)

#define AVR_LOW_LATENCY_BUFFER_INPUT_SIZE  (64 * 1024)
#define AVR_LOW_LATENCY_BUFFER_OUTPUT_SIZE (64 * 1024)

typedef struct 
{
    void       * Ptr; // Pointer to the buffer start
    unsigned int Size;      // This entry size
    unsigned int BytesUsed; // Amount of data present in this entry 
    bool         IsFree;    // Is this buffer free for being sent
} BufferEntry_t;

typedef struct 
{
    void       * CachedPtr;// Buffer pool address
    void       * PhysPtr;  // Buffer pool address
    unsigned int Size;    // Total pool size
    unsigned int EltCount;// Number of buffers inside this pool
    unsigned int EltSize; // Size of a buffer inside this pool
} BufferPool_t;

typedef struct
{
    U32                         Id;                //!< Id of the PostProcessing structure.
    U16                         StructSize;        //!< Size of this structure
    U8                          NbPcmProcessings;  //!< NbPcmProcessings on main[0..3] and aux[4..7]
    U8                          AuxSplit;          //!< Point of split between Main output and Aux output
} MME_LLPcmProcessingGlobalParams_t;

typedef struct
{
    MME_BassMgtGlobalParams_t   BassMgt;
    MME_DCRemoveGlobalParams_t  DCRemove;
    MME_Resamplex2GlobalParams_t Resamplex2;
    MME_CMCGlobalParams_t       CMC;
    MME_DMixGlobalParams_t      Dmix;
    MME_FatpipeGlobalParams_t	FatPipeOrSpdifOut;
    MME_LimiterGlobalParams_t	Limiter;
} MME_LLChainPcmProcessingGlobalParams_t;

typedef struct
{
    MME_UINT            StructSize;
    MME_SpdifinConfig_t SPDIFin;                               //!< Optional : SPDIFin configuration
    MME_LowLatencyIO_t  IOCfg[AVR_LOW_LATENCY_MAX_INPUT_CARDS + AVR_LOW_LATENCY_MAX_OUTPUT_CARDS];     //!< Optional : IO connections
    MME_LLPcmProcessingGlobalParams_t      PcmParams;
    MME_LLChainPcmProcessingGlobalParams_t PcmChainParams[AVR_LOW_LATENCY_MAX_OUTPUT_CARDS];
} MME_LowlatencySpecializedGlobalParams_t;

typedef struct
{
    MME_UINT                          StructSize;   //!< Sizeof this struct
    tLowlatencyInitP                  InitParams;
    MME_LowlatencySpecializedGlobalParams_t    GlobalInitParams;
} MME_LowlatencySpecializedInitParams_t;

typedef struct 
{
    MME_LxAudioDecoderFrameStatus_t         DecoderFrameStatus;
    U32                                     BytesUsed;  // Amount of this structure already filled
    MME_PcmProcessingOutputChainStatus_t    OutputChainStatus[AVR_LOW_LATENCY_MAX_OUTPUT_CARDS];
    MME_LimiterStatus_t                     LimiterStatus[AVR_LOW_LATENCY_MAX_OUTPUT_CARDS];
} MME_LowlatencySpecializedTransformStatus_t;


#if DRV_MULTICOM_AUDIO_DECODER_VERSION < 0x090128
typedef struct
{
	U32                    State        : 2; //!< LIMITER_MUTE = 0 / LIMITER_PLAY = 2
	U32                    Chains       : 4; //!< applies to chain[bit_n]
	U32                    Override     : 1; //!< Forces the State independently of the Id
	U32                    AutoUnmute   : 1; //!< If set then let the FW UnMute while restarting the IOs
	U32                    MuteId       :24; //!< reference ID to match to disable Emergency Mute.
} tEmergencyMute;
#endif

typedef struct 
{
    tMMESpdifinStatus CurrentSpdifStatus;
    enum eAccFsCode   CurrentDecoderFrequency;
    enum eAccAcMode   CurrentDecoderAudioMode;  
    tEmergencyMute    CurrentMuteStatus;        
    int               DecodeErrorCount;
    long long int     NumberOfProcessedSamples;
} LLDecoderStatus_t;

#endif // DVB_DVP_AUDIO_MME_H_

