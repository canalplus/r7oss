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

Source file name : codec_mme_audio_silence.h
Author :           Daniel

Definition of the stream specific codec implementation for mpeg audio in player 2


Date        Modification                                    Name
----        ------------                                    --------
26-Apr-07   Created (from codec_mme_video_mpeg2.h)          Daniel

************************************************************************/

#ifndef H_CODEC_MME_AUDIO_SILENCE
#define H_CODEC_MME_AUDIO_SILENCE

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "codec_mme_audio.h"
#include "codec_mme_audio_dtshd.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Codec_MmeAudioSilence_c : public Codec_MmeAudio_c
{
protected:

    // Data
    unsigned int             CurrentTranscodeBufferIndex;
    CodecBufferState_t       TranscodedBuffers[DTSHD_TRANSCODE_BUFFER_COUNT];
    Buffer_c*                CurrentTranscodeBuffer;
    bool                     TranscodeEnable;
    
    allocator_device_t       TranscodedFrameMemoryDevice;
    BufferPool_t             TranscodedFramePool;
    void                    *TranscodedFrameMemory[3];

    BufferDataDescriptor_t  *TranscodedFrameBufferDescriptor;
    BufferType_t             TranscodedFrameBufferType;
    // Functions

public:

    //
    // Constructor/Destructor methods
    //

    Codec_MmeAudioSilence_c(		void );
    ~Codec_MmeAudioSilence_c(		void );

    //
    // Extension to base functions
    //
    
    CodecStatus_t   HandleCapabilities(         void );

protected:

    CodecStatus_t   FillOutTransformerInitializationParameters( void );
    CodecStatus_t   FillOutSetStreamParametersCommand( 		void );
    CodecStatus_t   FillOutDecodeCommand(       		void );
    CodecStatus_t   ValidateDecodeContext( CodecBaseDecodeContext_t *Context );
    CodecStatus_t   DumpSetStreamParameters( 			void	*Parameters );
    void            SetCommandIO(void);
    CodecStatus_t   GetTranscodedFrameBufferPool( BufferPool_t * Tfp );
    CodecStatus_t   GetTranscodeBuffer( void );
    void            AttachCodedFrameBuffer( void );
    CodecStatus_t   DumpDecodeParameters( 			void	*Parameters );
    CodecStatus_t   Reset( void );
};
#endif /* H_CODEC_MME_AUDIO_SILENCE */
