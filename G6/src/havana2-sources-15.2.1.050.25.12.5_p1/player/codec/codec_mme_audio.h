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

#ifndef H_CODEC_MME_AUDIO
#define H_CODEC_MME_AUDIO

#include "codec_mme_base.h"
#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudio_c"

//      Include any firmware headers
#include <ACC_Transformers/acc_mmedefines.h>
#include <ACC_Transformers/Audio_DecoderTypes.h>
#include <ACC_Transformers/DolbyDigital_DecoderTypes.h>
#include <ACC_Transformers/DTS_DecoderTypes.h>
#include <ACC_Transformers/DolbyDigitalPlus_DecoderTypes.h>
#include <ACC_Transformers/LPCM_DecoderTypes.h>
#include <ACC_Transformers/MP2a_DecoderTypes.h>
#if (DRV_MME_MP2a_DECODER_VERSION >= 0x140417)
#include <ACC_Transformers/AAC_DecoderTypes.h>
#endif
#include <ACC_Transformers/WMA_DecoderTypes.h>
#include <ACC_Transformers/WMAProLsl_DecoderTypes.h>
#include <ACC_Transformers/Pcm_PostProcessingTypes.h>
#include <ACC_Transformers/Spdifin_DecoderTypes.h>
#include <ACC_Transformers/TrueHD_DecoderTypes.h>
#include <ACC_Transformers/RealAudio_DecoderTypes.h>
#include <ACC_Transformers/OV_DecoderTypes.h>
#include <ACC_Transformers/DRA_DecoderTypes.h>
#include "se_mixer_transformer_types.h"

#include "audio_conversions.h"


// AudioDecoder transformer names for each running CPU
#define AUDIO_CODEC_MAX_TRANSFORMERS 2
#define AUDIO_DECODER_TRANSFORMER_NAME0 "AUDIO_DECODER_a0"
#define AUDIO_DECODER_TRANSFORMER_NAME1 "AUDIO_DECODER_a1"

#define AUDIO_DECODER_TRANSCODE_BUFFER_COUNT 64
#define AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_COUNT 64
#define AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_SCATTER_PAGES 2 // For Ms1x 2 scatter pages are required for Compressed Frame Buffer
#define AUDIO_DECODER_AUX_FRAME_BUFFER_COUNT 64
#define AUDIO_DECODER_MAX_AUX_CHANNEL_COUNT 2

extern const int ACC_AcMode2ChannelCountLUT[];
#define ACC_AcMode2ChannelCount(AcMode) (ACC_AcMode2ChannelCountLUT[(AcMode&0xf)] + ((AcMode >= 0x80)?1:0))


//{{{  MME_LxPcmProcessingGlobalParams_Subset_t
typedef struct
{
    U16                        StructSize;
    U8                         DigSplit;
    U8                         AuxSplit;
    MME_CMCGlobalParams_t      CMC;
    MME_DMixGlobalParams_t     DMix;
    MME_Resamplex2GlobalParams_t Resamplex2;
    MME_TempoGlobalParams_t    Tempo;
    MME_DeEmphGlobalParams_t   DeEmphasis;
} MME_LxPcmProcessingGlobalParams_Subset_t;

#define ACC_PCM_PROCESSING_GLOBAL_PARAMS_SIZE sizeof(MME_LxPcmProcessingGlobalParams_Subset_t)
//#define ACC_PCM_PROCESSING_NODMIX_PARAMS_SIZE offsetof(MME_LxPcmProcessingGlobalParams_Subset_t, DMix)
//}}}

//{{{  MME_PcmProcessingFrameExtStatus_Concrete_t
typedef struct
{
    U32                                       BytesUsed;  // Amount of this structure already filled
    MME_PcmProcessingOutputChainStatus_t      Main;
    MME_PcmProcessingOutputChainStatus_t      Aux;
    MME_PcmProcessingOutputChainStatus_t      Spdif;
    MME_PcmProcessingOutputChainStatus_t      HDMI;
    MME_CrcStatus_t                           Crc; // If CRC computation is enabled , then CRC for the 4 outputs is reported in a single struct.
    MME_LxSAStatus_t                          SupplementaryAudio; // SupplementaryAudio status
} MME_PcmProcessingFrameExtCommonStatus_t;

typedef struct
{
    MME_PcmProcessingFrameExtStatus_t PcmStatus;
    char Padding[256 - sizeof(MME_PcmProcessingFrameExtStatus_t)]; // additional padding...
} MME_PcmProcessingFrameExtStatus_Concrete_t;
//}}}

//{{{  AdditionalBufferState_t
typedef struct
{
    Buffer_t                     Buffer;
    uint32_t                     BufferLength;
    void                        *BufferPointer;
} AdditionalBufferState_t;
//}}}

class Codec_MmeAudio_c : public Codec_MmeBase_c
{
public:
    Codec_MmeAudio_c(void);
    virtual  ~Codec_MmeAudio_c(void);

    CodecStatus_t   Halt(void);

    CodecStatus_t   SetModuleParameters(unsigned int      ParameterBlockSize,
                                        void             *ParameterBlock);
    CodecStatus_t   GetAttribute(const char                     *Attribute,
                                 PlayerAttributeDescriptor_t    *Value);

    //
    // Codec class functions
    //

    CodecStatus_t   Connect(Port_c *Port);
    CodecStatus_t   Input(Buffer_t          CodedBuffer);

    //
    // Extension to base functions
    //

    CodecStatus_t   InitializeDataTypes(void);
    virtual CodecStatus_t   HandleCapabilities(unsigned int ActualTransformer) { return CodecNoError; };
    virtual CodecStatus_t   HandleCapabilities(void)                   { return CodecNoError; };
    virtual CodecStatus_t   ParseCapabilities(unsigned int ActualTransformer);

    void            UpdateConfig(unsigned int Update);
    //
    // Implementation of fill out function for generic video,
    // may be overridden if necessary.
    //

    virtual CodecStatus_t   FillOutDecodeBufferRequest(DecodeBufferRequest_t   *Request);

    //
    // Functions to support transcoder buffer mgt.
    //

    CodecStatus_t   GetTranscodedFrameBufferPool(BufferPool_t *Tfp);
    CodecStatus_t   GetTranscodeBuffer(void);

    //
    // Functions to support compressed frame buffer mgt.
    //

    CodecStatus_t   GetCompressedFrameBufferPool(BufferPool_t *Cfp);
    CodecStatus_t   GetCompressedFrameBuffer(int32_t NoOfCompressedBufferToGet);

    //
    // Functions to support aux buffer mgt.
    //

    CodecStatus_t   GetAuxFrameBufferPool(BufferPool_t *Afp);
    CodecStatus_t   GetAuxBuffer(void);

    // utility functions

    static CodecStatus_t   ConvertFreqToAccCode(int aFrequency, enum eAccFsCode &aAccFreqCode);
    static unsigned char GetNumberOfChannelsFromAudioConfiguration(enum eAccAcMode Mode);
    void   DisableTranscodingBasedOnProfile(void);
    void   EnableAuxBufferBasedOnProfile(void);

protected:
    OutputSurfaceDescriptor_t              DefaultAudioSurfaceDescriptor;
    OutputSurfaceDescriptor_t             *AudioOutputSurface;
    ParsedAudioParameters_t               *ParsedAudioParameters;

    stm_se_play_stream_audio_parameters_t  AudioParametersEvents;
    enum eAccAcMode                        actualDecAudioMode;

    PlayerChannelSelect_t                  SelectedChannel;
    DRCParams_t                            DRC;
    eAccAcMode                             OutmodeMain;
    eAccAcMode                             OutmodeAux;
    bool                                   StreamDrivenDownmix;
    unsigned int                           RawAudioSamplingFrequency; // Sampling frequency of RAW audio format (PCM , AAC-RAW ,BSAC-RAW)
    unsigned int                           RawAudioNbChannels; // No. of channels in RAW audio format (PCM , AAC-RAW ,BSAC-RAW)
    unsigned int                           RelayfsIndex;      //stores id from relayfs to differentiate the aux buffer.

    unsigned int             CurrentTranscodeBufferIndex;
    AdditionalBufferState_t  TranscodedBuffers[AUDIO_DECODER_TRANSCODE_BUFFER_COUNT];
    Buffer_c                *CurrentTranscodeBuffer;
    bool                     TranscodeEnable;
    bool                     TranscodeNeeded;
    bool                     CompressedFrameNeeded;

    allocator_device_t       TranscodedFrameMemoryDevice;
    BufferPool_t             TranscodedFramePool;
    void                    *TranscodedFrameMemory[3];

    BufferDataDescriptor_t  *TranscodedFrameBufferDescriptor;
    BufferType_t             TranscodedFrameBufferType;

    // Handling corresponding to the Aux buffer management
    unsigned int             CurrentAuxBufferIndex;
    AdditionalBufferState_t  AuxBuffers[AUDIO_DECODER_AUX_FRAME_BUFFER_COUNT];
    Buffer_c                *CurrentAuxBuffer;
    bool                     AuxOutputEnable;

    allocator_device_t       AuxFrameMemoryDevice;
    BufferPool_t             AuxFramePool;
    void                    *AuxFrameMemory[3];

    BufferDataDescriptor_t  *AuxFrameBufferDescriptor;
    BufferType_t             AuxFrameBufferType;

    // Buffer to write the Compressed Frame (by the FW) mainly in the StreamBased decoding
    // Up to AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_SCATTER_PAGES scatter pages can be attached for the Compressed Frame
    uint32_t                              CurrentCompressedFrameBufferIndex[AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_SCATTER_PAGES];
    AdditionalBufferState_t               CompressedFrameBuffers[AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_SCATTER_PAGES][AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_COUNT];

    Buffer_c                             *CurrentCompressedFrameBuffer[AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_SCATTER_PAGES];
    uint32_t                              NoOfCompressedFrameBuffers; // To store How many CompressedFrame buffers attached to coded buffer
    bool                                  CompressedFrameEnable;

    allocator_device_t                    CompressedFrameMemoryDevice;
    BufferPool_t                          CompressedFramePool;
    void                                  *CompressedFrameMemory[3];

    BufferDataDescriptor_t                *CompressedFrameBufferDescriptor;
    BufferType_t                          CompressedFrameBufferType;


    /// If true the TransformName is immutable.
    bool                                  ProtectTransformName;
    char                                  TransformName[CODEC_MAX_TRANSFORMERS][MME_MAX_TRANSFORMER_NAME];

    /// Data structure providing information about the configuration of the AUDIO_DECODER.
    MME_LxAudioDecoderInfo_t              AudioDecoderTransformCapability;
    MME_LxAudioDecoderInfo_t              AudioDecoderTransformCapabilityMask;

    /// Data structure used to initialized the AUDIO_DECODER.
    MME_LxAudioDecoderInitParams_t        AudioDecoderInitializationParameters;

    void                    PresetIOBuffers(void);
    virtual void            SetCommandIO(void);
    virtual CodecStatus_t   FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams);
    virtual CodecStatus_t   FillOutTransformerInitializationParameters(void);
    virtual void            GlobalParamsCommandCompleted(CodecBaseStreamParameterContext_t *StreamParameterContext);
    virtual CodecStatus_t   ValidatePcmProcessingExtendedStatus(CodecBaseDecodeContext_t *Context,
                                                                MME_PcmProcessingFrameExtStatus_t *PcmStatus);
    virtual void            HandleMixingMetadata(CodecBaseDecodeContext_t *Context,
                                                 MME_PcmProcessingStatusTemplate_t *PcmStatus);

    void                    HandleDownmixTable(CodecBaseDecodeContext_t *Context,
                                               MME_PcmProcessingStatusTemplate_t *PcmStatus, int DmixTableNo);

    virtual void           ReportDecodedCRC(MME_PcmProcessingStatusTemplate_t *PcmStatus);

//! TODO - add virtual function for DecodeContext that WMA codec can over-ride for different buffers and MME command
    virtual CodecStatus_t   FillOutDecodeContext(void);
    virtual void            FinishedDecode(void);
    virtual void            AttachCodedFrameBuffer(void);
    virtual void            AttachAuxFrameBuffer(void);
    void  SetAudioCodecDecStatistics();
//!
    void  SetAudioCodecDecAttributes();

    virtual void            CheckAudioParameterEvent(MME_LxAudioDecoderFrameStatus_t *DecFrameStatus, MME_PcmProcessingFrameExtStatus_t *PcmExtStatus,
                                                     stm_se_play_stream_audio_parameters_t *newAudioParametersValues, PlayerEventRecord_t  *myEvent);
    void                    GetAudioDecodedStreamInfo(MME_PcmProcessingFrameExtStatus_t *PcmExtStatus, stm_se_play_stream_audio_parameters_t *audioparams);
    struct stm_se_audio_channel_assignment TranslateAudioModeToChannelAssignment(enum eAccAcMode AudioMode);

    // Externally useful information
    MME_LxAudioDecoderFrameStatus_t AudioDecoderStatus;

    // Store the last ratio param set to tempo PCM processing
    int mTempoRequestedRatio;
    int mTempoSetRatio;

private:
    DISALLOW_COPY_AND_ASSIGN(Codec_MmeAudio_c);

    void                     EvaluateImplicitDownmixPromotion(void);
    void                     EvaluateTranscodeCompressedNeeded(void);
    void                     EvaluateMixerDRCParameters(void);
};

#endif
