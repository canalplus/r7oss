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

Source file name : codec_mme_video_h263.h
Author :           Mark C

Definition of the stream specific codec implementation for H263 video in player 2


Date        Modification                                    Name
----        ------------                                    --------
20-May-08   Created                                         Julian

************************************************************************/

#ifndef H_CODEC_MME_VIDEO_FLV1
#define H_CODEC_MME_VIDEO_FLV1

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video.h"
#include "FLV1_VideoTransformerTypes.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define FLV1_NUM_MME_INPUT_BUFFERS              2
#define FLV1_NUM_MME_OUTPUT_BUFFERS             3
#define FLV1_NUM_MME_BUFFERS                    (FLV1_NUM_MME_INPUT_BUFFERS+FLV1_NUM_MME_OUTPUT_BUFFERS)

#define FLV1_MME_CODED_DATA_BUFFER              0
#define FLV1_MME_CURRENT_FRAME_BUFFER           1
#define FLV1_MME_REFERENCE_FRAME_BUFFER         2
#define FLV1_MME_CURRENT_FRAME_BUFFER_LUMA      3
#define FLV1_MME_CURRENT_FRAME_BUFFER_CHROMA    4

#define FLV1_DECODE_PICTURE_TYPE_I              1
#define FLV1_DECODE_PICTURE_TYPE_P              2


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/// The FLV1 video codec proxy.
class Codec_MmeVideoFlv1_c : public Codec_MmeVideo_c
{
private:

    // Data

    FLV1_InitTransformerParam_t         Flv1InitializationParameters;

    allocator_device_t                  TranscodedFrameMemoryDevice;
    void*                               TranscodedFrameMemory[3];
    unsigned char*                      TranscodedFrameLumaBuffer;
    unsigned char*                      TranscodedFrameChromaBuffer;

    bool                                RestartTransformer;
    unsigned int                        DecodingWidth;
    unsigned int                        DecodingHeight;
    unsigned int                        MaxBytesPerFrame;

    // Functions

public:

    //
    // Constructor/Destructor methods
    //

    Codec_MmeVideoFlv1_c(                void );
    ~Codec_MmeVideoFlv1_c(               void );

    //
    // Stream specific functions
    //

protected:

    CodecStatus_t   Reset(                                      void );
    CodecStatus_t   HandleCapabilities(                         void );
    CodecStatus_t   RegisterOutputBufferRing (                  Ring_t                          Ring);
    CodecStatus_t   InitializeMMETransformer(                   void );

    CodecStatus_t   FillOutTransformerInitializationParameters( void );
    CodecStatus_t   FillOutSetStreamParametersCommand(          void );
    CodecStatus_t   FillOutDecodeCommand(                       void );
    CodecStatus_t   FillOutDecodeBufferRequest(                 BufferStructure_t              *Request );

    CodecStatus_t   ValidateDecodeContext(                      CodecBaseDecodeContext_t       *Context );
    CodecStatus_t   DumpSetStreamParameters(                    void                           *Parameters );
    CodecStatus_t   DumpDecodeParameters(                       void                           *Parameters );

    CodecStatus_t   SendMMEStreamParameters(                    void );

};
#endif
