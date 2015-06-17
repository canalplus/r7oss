/*
 * pcm_transcoder_transformer.c
 *
 * Copyright (C) STMicroelectronics Limited 2009. All rights reserved.
 *
 * Pcm transcoding transformer.
 * Input:               Either endian, n channel
 * Output:              Little endian, 8 channel
 */

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mempolicy.h>
#include <asm/cacheflush.h>
#else
#include <string.h>
#endif

#include <mme.h>
#include "pcm_transcoder.h"
#include "Pcm_TranscoderTypes.h"

#define PCM_INPUT_BUFFER                0
#define PCM_OUTPUT_BUFFER               1
#define PCM_OUTPUT_BITS_PER_SAMPLE      32
#define PCM_OUTPUT_FRONT_LEFT           0
#define PCM_OUTPUT_FRONT_RIGHT          1
#define PCM_OUTPUT_FRONT_CENTRE         3
#define PCM_OUTPUT_SAMPLE_SIZE          (PCM_OUTPUT_BITS_PER_SAMPLE/8)
#define PCM_OUTPUT_MONO_OFFSET          (PCM_OUTPUT_FRONT_CENTRE*PCM_OUTPUT_SAMPLE_SIZE)        /* Mono goes out through front centre */
#define PCM_OUTPUT_CHANNEL_COUNT        8
#define PCM_OUTPUT_BYTES_PER_SAMPLE     (PCM_OUTPUT_SAMPLE_SIZE * PCM_OUTPUT_CHANNEL_COUNT)

/*{{{  typedefs and structures*/
struct PcmContext_s
{
    unsigned int                ChannelCount;
    unsigned int                SampleRate;
    unsigned int                BytesPerSecond;
    unsigned int                BlockAlign;
    unsigned int                BitsPerSample;
    unsigned int                DataEndianness;
    enum eAccAcMode             AudioMode;
    enum eAccFsCode             SamplingFrequency;
};
/*}}}*/

/*{{{  InitializeContext*/
static MME_ERROR InitializeContext (struct PcmContext_s* PcmContext, MME_LxPcmAudioConfig_t* Config)
{
    PCM_TRACE("Config.SampleRate          %d (%x)\n", Config->SampleRate, Config->SampleRate);
    PCM_TRACE("Config.BytesPerSecond      %d (%x)\n", Config->BytesPerSecond, Config->BytesPerSecond);
    PCM_TRACE("Config.BlockAlign          %d (%x)\n", Config->BlockAlign, Config->BlockAlign);
    PCM_TRACE("Config.BitsPerSample       %d (%x)\n", Config->BitsPerSample, Config->BitsPerSample);
    PCM_TRACE("Config.DataEndianness      %d (%x)\n", Config->DataEndianness, Config->DataEndianness);

    PcmContext->ChannelCount            = Config->ChannelCount;
    PcmContext->SampleRate              = Config->SampleRate;
    PcmContext->BytesPerSecond          = Config->BytesPerSecond;
    PcmContext->BlockAlign              = Config->BlockAlign;
    PcmContext->BitsPerSample           = Config->BitsPerSample;
    PcmContext->DataEndianness          = Config->DataEndianness;

    switch (PcmContext->ChannelCount)
    {
        case 1:         PcmContext->AudioMode   = ACC_MODE10;           break;          /* Mono */
        case 2:         PcmContext->AudioMode   = ACC_MODE20;           break;          /* Stereo */
        case 6:         PcmContext->AudioMode   = ACC_MODE32_LFE;       break;          /* 5,1 */
        default:
            PCM_ERROR("%d Channels not supported - treating as stereo.\n", PcmContext->ChannelCount);
            PcmContext->AudioMode               = ACC_MODE20;           break;          /* Stereo */
    }
    PCM_TRACE("PcmContext->AudioMode        %d (%x)\n", PcmContext->AudioMode, PcmContext->AudioMode);

    switch (PcmContext->SampleRate)
    {
        case  8000:     PcmContext->SamplingFrequency   = ACC_FS8k;        break;
        case 11025:     PcmContext->SamplingFrequency   = ACC_FS11k;       break;
        case 12000:     PcmContext->SamplingFrequency   = ACC_FS12k;       break;
        case 16000:     PcmContext->SamplingFrequency   = ACC_FS16k;       break;
        case 22050:     PcmContext->SamplingFrequency   = ACC_FS22k;       break;
        case 24000:     PcmContext->SamplingFrequency   = ACC_FS24k;       break;
        case 32000:     PcmContext->SamplingFrequency   = ACC_FS32k;       break;
        case 44100:     PcmContext->SamplingFrequency   = ACC_FS44k;       break;
        case 48000:     PcmContext->SamplingFrequency   = ACC_FS48k;       break;
        case 88200:     PcmContext->SamplingFrequency   = ACC_FS88k;       break;
        case 96000:     PcmContext->SamplingFrequency   = ACC_FS96k;       break;
        case 192000:    PcmContext->SamplingFrequency   = ACC_FS192k;      break;
        default:
            PCM_ERROR("Frequency %d not supported - treating as 44k.\n", PcmContext->SampleRate);
            PcmContext->SamplingFrequency               = ACC_FS44k;       break;
    }
    PCM_TRACE("PcmContext->SamplingFrequency   %d (%x)\n", PcmContext->SamplingFrequency, PcmContext->SamplingFrequency);

    return MME_SUCCESS;
}
/*}}}*/
/*{{{  PcmTranscoder_AbortCommand*/
static MME_ERROR PcmTranscoder_AbortCommand (void* Context, MME_CommandId_t CommandId)
{
    /* This transformer supports neither aborting the currently executing 
     * transform nor does it utilize deferred transforms. For this reason
     * there is nothing useful it can do here.
     */

    return MME_INVALID_ARGUMENT;
}
/*}}}*/
/*{{{  PcmTranscoder_GetTransformerCapability*/
static MME_ERROR PcmTranscoder_GetTransformerCapability (MME_TransformerCapability_t* Capability)
{
    Capability->Version = 0x010000;
    return MME_SUCCESS;
}
/*}}}*/
/*{{{  PcmTranscoder_InitTransformer*/
static MME_ERROR PcmTranscoder_InitTransformer (MME_UINT ParamsSize, MME_GenericParams_t Params, void** ContextPointer)
{
    struct PcmContext_s*                        PcmContext;
    MME_LxAudioDecoderInitParams_t*             InitParams      = (MME_LxAudioDecoderInitParams_t*)Params;
    MME_LxAudioDecoderGlobalParams_t*           GlobalParams    = &InitParams->GlobalParams;
    MME_LxPcmAudioConfig_t*                     Config          = (MME_LxPcmAudioConfig_t*)GlobalParams->DecConfig;
    MME_ERROR                                   Status;

    *ContextPointer             = NULL;
    PcmContext                  = kmalloc (sizeof (struct PcmContext_s), GFP_KERNEL);
    if (PcmContext == NULL)
    {
        PCM_ERROR("Unable to allocate memory for context\n");
        return MME_NOMEM;
    }

    Status                      = InitializeContext (PcmContext, Config);
    if (Status == MME_SUCCESS)
        *ContextPointer         = PcmContext;
    else
    {
        kfree (PcmContext);
        PCM_ERROR("Failed to initialize decode context\n");
    }

    return Status;
}
/*}}}*/
/*{{{  PcmTranscoder_Transform*/
static MME_ERROR PcmTranscoder_Transform (void* Context, MME_Command_t* Command)
{
    unsigned int                        i;
    unsigned int                        Sample;
    unsigned int                        Channel;
    struct PcmContext_s*                PcmContext              = (struct PcmContext_s*)Context;
    MME_CommandStatus_t*                CommandStatus           = &Command->CmdStatus;
    MME_LxAudioDecoderFrameStatus_t*    FrameStatus             = (MME_LxAudioDecoderFrameStatus_t*)CommandStatus->AdditionalInfo_p;

    MME_DataBuffer_t*                   InputDataBuffer         = Command->DataBuffers_p[PCM_INPUT_BUFFER];
    MME_DataBuffer_t*                   OutputDataBuffer        = Command->DataBuffers_p[PCM_OUTPUT_BUFFER];
    MME_ScatterPage_t*                  InputScatterPage        = &InputDataBuffer->ScatterPages_p[0];
    MME_ScatterPage_t*                  OutputScatterPage       = &OutputDataBuffer->ScatterPages_p[0];
    unsigned char*                      Input                   = InputScatterPage->Page_p;
    unsigned int                        SampleSize              = PcmContext->BitsPerSample / 8;
    unsigned int                        SampleCount             = InputScatterPage->Size/PcmContext->BlockAlign;
    unsigned int                        OutputOffset            = 0;

    PCM_DEBUG("Samples %d SampleSize %d\n",SampleCount, SampleSize);
    if (PcmContext->ChannelCount == 1)
        OutputOffset                    = PCM_OUTPUT_MONO_OFFSET;

    memset ((unsigned char*)OutputScatterPage->Page_p, 0, SampleCount*PCM_OUTPUT_BYTES_PER_SAMPLE);
    if (PcmContext->DataEndianness == PCM_LITTLE_ENDIAN)
    {
        for (Sample=0;Sample<(InputScatterPage->Size/PcmContext->BlockAlign); Sample++)
        {
            unsigned char*      Output          = ((unsigned char*)OutputScatterPage->Page_p) + (Sample * PCM_OUTPUT_BYTES_PER_SAMPLE) + OutputOffset;

            for (Channel=0; Channel<PcmContext->ChannelCount; Channel++)
            {
                memcpy (Output+4-SampleSize, Input, SampleSize);      /* Put values into most significant bytes in a little endian way */
                Output                 += PCM_OUTPUT_SAMPLE_SIZE;
                Input                  += SampleSize;
            }
        }
    }
    else
    {
        for (Sample=0;Sample<(InputScatterPage->Size/PcmContext->BlockAlign); Sample++)
        {
            unsigned char*      Output          = ((unsigned char*)OutputScatterPage->Page_p) + (Sample * PCM_OUTPUT_BYTES_PER_SAMPLE) + OutputOffset;

            for (Channel=0; Channel<PcmContext->ChannelCount; Channel++)
            {
                for (i=0; i<SampleSize; i++)
                    Output[i+4-SampleSize]      = Input[SampleSize-i-1];        /* Put values into most significant bytes in a little endian way */
                Output                         += PCM_OUTPUT_SAMPLE_SIZE;
                Input                          += SampleSize;
            }
        }
    }
    /*{{{  trace*/
    #if 0
    {
        unsigned char*  P       = (unsigned char*)InputScatterPage->Page_p;
        printk("\n%08x:  ",  0);
        for (i=0; i<512; i++)
        {
            printk("%02x ",  P[i]);
            if (((i+1)&0x1f)==0)
                printk("\n%08x:  ",  i);
        }
    }
    {
        unsigned char*  P       = (unsigned char*)OutputScatterPage->Page_p;
        printk("\n%08x:  ",  0);
        for (i=0; i<512; i++)
        {
            printk("%02x ",  P[i]);
            if (((i+1)&0x1f)==0)
                printk("\n%08x:  ",  i);
        }
    }
    #endif
    /*}}}*/

    OutputScatterPage->Size             = Sample * PCM_OUTPUT_BYTES_PER_SAMPLE;
    //OutputScatterPage->Flags            = PAGE_VALID;

    PCM_DEBUG("Output %d bytes\n",OutputScatterPage->Size);

#ifdef __KERNEL__
    // TODO: most of the Linux cache flush functions "don't do what you think they do". We take a
    //       conservative approach here.
    flush_cache_all();
#endif

    CommandStatus->Error                = MME_SUCCESS;
    CommandStatus->State                = MME_COMMAND_COMPLETED;

    FrameStatus->AudioMode              = PcmContext->AudioMode;
    FrameStatus->SamplingFreq           = PcmContext->SamplingFrequency;
    FrameStatus->NbOutSamples           = SampleCount;

    PCM_DEBUG("Output %d, %d, %d \n", FrameStatus->AudioMode, FrameStatus->SamplingFreq, FrameStatus->NbOutSamples);
    return MME_SUCCESS;
}
/*}}}*/
/*{{{  PcmTranscoder_SetGlobalParams*/
static MME_ERROR PcmTranscoder_SetGlobalParams(void* Context, MME_Command_t* Command)
{
    struct PcmContext_s*                PcmContext      = (struct PcmContext_s*)Context;
    MME_LxAudioDecoderGlobalParams_t*   GlobalParams    = Command->Param_p;
    MME_CommandStatus_t*                CommandStatus   = &Command->CmdStatus;
    MME_LxPcmAudioConfig_t*             Config          = (MME_LxPcmAudioConfig_t*)GlobalParams->DecConfig;

    CommandStatus->Error        = InitializeContext (PcmContext, Config);
    CommandStatus->State        = MME_COMMAND_COMPLETED;

    return CommandStatus->Error;
}
/*}}}*/

static MME_ERROR PcmTranscoder_ProcessCommand (void* Context, MME_Command_t* Command)
{
    PCM_DEBUG("\n");
    switch (Command->CmdCode)
    {
        case MME_TRANSFORM:
            return PcmTranscoder_Transform (Context, Command);

        case MME_SET_GLOBAL_TRANSFORM_PARAMS:
            return PcmTranscoder_SetGlobalParams (Context, Command);
            break;

        case MME_SEND_BUFFERS: /* not supported by this transformer */
        default:
            break;
    }

    return MME_INVALID_ARGUMENT;
}

static MME_ERROR PcmTranscoder_TermTransformer (void* Context)
{
    kfree (Context);
    return MME_SUCCESS;
}


MME_ERROR PcmTranscoder_RegisterTransformer (const char* Name)
{
    return MME_RegisterTransformer     (Name,
                                        PcmTranscoder_AbortCommand,
                                        PcmTranscoder_GetTransformerCapability,
                                        PcmTranscoder_InitTransformer,
                                        PcmTranscoder_ProcessCommand,
                                        PcmTranscoder_TermTransformer);
}
