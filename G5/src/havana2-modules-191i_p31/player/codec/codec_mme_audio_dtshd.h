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

Source file name : codec_mme_audio_dtshd.h
Author :           Sylvain Barge

Definition of the stream specific codec implementation for mpeg audio in player 2


Date        Modification                                    Name
----        ------------                                    --------
11-Jun-07   Ported to Player2 and added dtshd support       Sylvain Barge

************************************************************************/

#ifndef H_CODEC_MME_AUDIO_DTSHD
#define H_CODEC_MME_AUDIO_DTSHD

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "codec_mme_audio.h"
#include "dtshd.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define DTSHD_FRAME_MAX_SIZE                     16384
#define DTSHD_TRANSCODE_BUFFER_COUNT             64

#define DTSHD_TRANSCODE_SCATTER_PAGE_INDEX       2

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//
typedef struct
{
	U32                             Id;                           //< Id of this processing structure: ACC_MIX_METADATA_ID
	U32                             StructSize;                   //< Size of this structure
	U16                             PrimaryAudioGain[ACC_MAIN_CSURR + 1];       //< unsigned Q3.13 gain to be applied to each channel of primary stream
	U16                             PostMixGain;                  //< unsigned Q3.13 gain to be applied to output of mixed primary and secondary
	U16                             NbOutMixConfig;               //< Number of mix output configurations
	MME_MixingOutputConfiguration_t MixOutConfig[MAX_MIXING_OUTPUT_CONFIGURATION]; //< This array is extensible according to NbOutMixConfig
} MME_LxAudioDecoderDtsMixingMetadata_t;

/// Calculate the apparent size of the structure when NbOutMixConfig is zero.
#define DTSHD_MIN_MIXING_METADATA_SIZE       (sizeof(MME_LxAudioDecoderDtsMixingMetadata_t) - (MAX_MIXING_OUTPUT_CONFIGURATION * sizeof(MME_MixingOutputConfiguration_t)))
#define DTSHD_MIN_MIXING_METADATA_FIXED_SIZE (DTSHD_MIN_MIXING_METADATA_SIZE - (2 * sizeof(U32))) // same as above minus Id and StructSize


typedef struct
{
	U32                                    BytesUsed;  // Amount of this structure already filled
	MME_LxAudioDecoderDtsMixingMetadata_t  MixingMetadata;
} MME_MixMetadataDtsFrameStatus_t;

typedef struct
{
	MME_LxAudioDecoderFrameStatus_t  DecStatus;  
	MME_MixMetadataDtsFrameStatus_t  PcmStatus;
} MME_LxAudioDecoderDtsFrameMixMetadataStatus_t;


typedef struct DtshdAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_LxAudioDecoderFrameParams_t                DecodeParameters;
    MME_LxAudioDecoderDtsFrameMixMetadataStatus_t  DecodeStatus;
    unsigned int                                   TranscodeBufferIndex;
    DtshdAudioFrameParameters_t                    ContextFrameParameters;
} DtshdAudioCodecDecodeContext_t;

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Codec_MmeAudioDtshd_c : public Codec_MmeAudio_c
{
protected:

    // Data
    
    eAccDecoderId            DecoderId;
    unsigned int             CurrentTranscodeBufferIndex;
    CodecBufferState_t       TranscodedBuffers[DTSHD_TRANSCODE_BUFFER_COUNT];
    Buffer_c*                CurrentTranscodeBuffer;
    bool                     TranscodeEnable;
    
    allocator_device_t       TranscodedFrameMemoryDevice;
    BufferPool_t             TranscodedFramePool;
    void                    *TranscodedFrameMemory[3];

    BufferDataDescriptor_t  *TranscodedFrameBufferDescriptor;
    BufferType_t             TranscodedFrameBufferType;
    bool                     IsLbrStream;

    // Functions

public:

    //
    // Constructor/Destructor methods
    //

    Codec_MmeAudioDtshd_c(		bool IsLbrStream);
    ~Codec_MmeAudioDtshd_c(		void );

    //
    // Stream specific functions
    //
    static void     FillStreamMetadata(ParsedAudioParameters_t * AudioParameters, MME_LxAudioDecoderFrameStatus_t * Status);
    static void     TranscodeDtshdToDts(CodecBaseDecodeContext_t    * BaseContext,
                                        unsigned int                  TranscodeBufferIndex,
                                        DtshdAudioFrameParameters_t * FrameParameters,
                                        CodecBufferState_t *          TranscodedBuffers );

protected:

    CodecStatus_t   FillOutTransformerGlobalParameters        ( MME_LxAudioDecoderGlobalParams_t *GlobalParams );
    CodecStatus_t   FillOutTransformerInitializationParameters( void );
    CodecStatus_t   FillOutSetStreamParametersCommand( 		void );
    CodecStatus_t   FillOutDecodeCommand(       		void );
    CodecStatus_t   ValidateDecodeContext( CodecBaseDecodeContext_t *Context );
    void            HandleMixingMetadata( CodecBaseDecodeContext_t *Context,
	                                  MME_PcmProcessingStatusTemplate_t *PcmStatus );
    CodecStatus_t   DumpSetStreamParameters( 			void	*Parameters );
    void            SetCommandIO(void);
    CodecStatus_t   GetTranscodedFrameBufferPool( BufferPool_t * Tfp );
    CodecStatus_t   GetTranscodeBuffer( void );
    void            AttachCodedFrameBuffer( void );
    CodecStatus_t   DumpDecodeParameters( 			void	*Parameters );
    CodecStatus_t   Reset( void );
};
#endif //H_CODEC_MME_AUDIO_DTSHD
