/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

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

Source file name : codec_mme_audio.h
Author :           Daniel

Definition of the codec audio class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
26-Apr-07   Created (from codec_mme_video.h)                Daniel
11-Sep-07   Reorganised to allow override by WMA/OGG to
	    make frame->stream possible                     Adam

************************************************************************/

#ifndef H_CODEC_MME_AUDIO
#define H_CODEC_MME_AUDIO

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_base.h"

// /////////////////////////////////////////////////////////////////////
//
//      Include any firmware headers (typically included in stlinux##-sh4-ACC_firmware-dev)
//

#include <ACC_Transformers/acc_mmedefines.h>
#include <ACC_Transformers/Audio_DecoderTypes.h>
#include <ACC_Transformers/DolbyDigital_DecoderTypes.h>
#include <ACC_Transformers/DTS_DecoderTypes.h>
#include <ACC_Transformers/DolbyDigitalPlus_DecoderTypes.h>
#include <ACC_Transformers/LPCM_DecoderTypes.h>
#include <ACC_Transformers/MP2a_DecoderTypes.h>
#include <ACC_Transformers/WMA_DecoderTypes.h>
#include <ACC_Transformers/WMAProLsl_DecoderTypes.h>
#include <ACC_Transformers/Mixer_ProcessorTypes.h>
#include <ACC_Transformers/Pcm_PostProcessingTypes.h>
#include <ACC_Transformers/Spdifin_DecoderTypes.h>
#include <ACC_Transformers/TrueHD_DecoderTypes.h>
#include <ACC_Transformers/RealAudio_DecoderTypes.h>
#include <ACC_Transformers/OV_DecoderTypes.h>

////////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define AUDIO_DECODER_TRANSFORMER_NAME "AUDIO_DECODER"
extern const int ACC_AcMode2ChannelCountLUT[];
extern const int ACC_SamplingFreqLUT[];
#define ACC_AcMode2ChannelCount(AcMode) (ACC_AcMode2ChannelCountLUT[(AcMode&0xf)] + ((AcMode >= 0x80)?1:0))

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

//{{{  MME_LxPcmProcessingGlobalParams_Subset_t
typedef struct {
    U16                        StructSize;
    U8                         DigSplit;
    U8                         AuxSplit;
    MME_CMCGlobalParams_t      CMC;
    MME_DMixGlobalParams_t     DMix;
    MME_Resamplex2GlobalParams_t Resamplex2;
} MME_LxPcmProcessingGlobalParams_Subset_t;

#define ACC_PCM_PROCESSING_GLOBAL_PARAMS_SIZE sizeof(MME_LxPcmProcessingGlobalParams_Subset_t)
//#define ACC_PCM_PROCESSING_NODMIX_PARAMS_SIZE offsetof(MME_LxPcmProcessingGlobalParams_Subset_t, DMix)
//}}}  

//{{{  MME_PcmProcessingFrameExtStatus_Concrete_t
typedef struct {
    MME_PcmProcessingFrameExtStatus_t PcmStatus;
    char Padding[256 - sizeof(MME_PcmProcessingFrameExtStatus_t)]; // additional padding...
} MME_PcmProcessingFrameExtStatus_Concrete_t;
//}}}  

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Codec_MmeAudio_c : public Codec_MmeBase_c
{
protected:

    // Data

    AudioOutputSurfaceDescriptor_t       *AudioOutputSurface;
    ParsedAudioParameters_t              *ParsedAudioParameters;

    PlayerChannelSelect_t                 SelectedChannel;
    DRCParams_t                           DRC;
    eAccAcMode                            OutmodeMain;

    /// If true the TransformName is immutable.
    bool                                  ProtectTransformName;
    char                                  TransformName[CODEC_MAX_TRANSFORMERS][MME_MAX_TRANSFORMER_NAME];

    /// Data structure providing information about the configuration of the AUDIO_DECODER.
    MME_LxAudioDecoderInfo_t              AudioDecoderTransformCapability;
    MME_LxAudioDecoderInfo_t              AudioDecoderTransformCapabilityMask;

    /// Data structure used to initialized the AUDIO_DECODER.
    MME_LxAudioDecoderInitParams_t        AudioDecoderInitializationParameters;

    // Functions
    void                    PresetIOBuffers(void);
    virtual void            SetCommandIO(void);
    virtual CodecStatus_t   FillOutTransformerGlobalParameters        ( MME_LxAudioDecoderGlobalParams_t *GlobalParams );
    virtual CodecStatus_t   FillOutTransformerInitializationParameters( void );
    virtual CodecStatus_t   ValidatePcmProcessingExtendedStatus( CodecBaseDecodeContext_t *Context,
								 MME_PcmProcessingFrameExtStatus_t *PcmStatus );
    virtual void            HandleMixingMetadata( CodecBaseDecodeContext_t *Context,
						  MME_PcmProcessingStatusTemplate_t *PcmStatus );
//! AWTODO - add virtual function for DecodeContext that WMA codec can over-ride for different buffers and MME command
    virtual CodecStatus_t   FillOutDecodeContext( void );
    virtual void            FinishedDecode( void );
    virtual void            AttachCodedFrameBuffer( void );
//!

    // Externally useful information
    MME_LxAudioDecoderFrameStatus_t AudioDecoderStatus;

public:

    //
    // Constructor/Destructor methods
    //

    Codec_MmeAudio_c(           void );

    //
    // Overrides for component base class functions
    //

    CodecStatus_t   Halt(       void );
    CodecStatus_t   Reset(      void );
    CodecStatus_t   SetModuleParameters(        unsigned int      ParameterBlockSize,
						void             *ParameterBlock );
    CodecStatus_t   CreateAttributeEvents(                      void );
    CodecStatus_t   GetAttribute(                               const char                     *Attribute,
								PlayerAttributeDescriptor_t    *Value );

    //
    // Codec class functions
    //

    CodecStatus_t   RegisterOutputBufferRing(   Ring_t            Ring );
    CodecStatus_t   Input(                      Buffer_t          CodedBuffer );

    //
    // Extension to base functions
    //

    CodecStatus_t   InitializeDataTypes(        void );
    CodecStatus_t   HandleCapabilities(         void );

    //
    // Implementation of fill out function for generic video,
    // may be overidden if necessary.
    //

    virtual CodecStatus_t   FillOutDecodeBufferRequest( BufferStructure_t        *Request );

    // utility functions
    static int      ConvertCodecSamplingFreq(enum eAccFsCode Code);
    static unsigned char GetNumberOfChannelsFromAudioConfiguration(enum eAccAcMode Mode);

};
#endif
