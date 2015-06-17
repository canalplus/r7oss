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

Source file name : codec_mme_video_rmv.h
Author :           Mark C

Definition of the stream specific codec implementation for Real Media video in player 2


Date        Modification                                    Name
----        ------------                                    --------
24-Oct-2008 Created                                         Julian

************************************************************************/

#ifndef H_CODEC_MME_VIDEO_RMV
#define H_CODEC_MME_VIDEO_RMV

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define RMV_DEFAULT_PICTURE_WIDTH               320
#define RMV_DEFAULT_PICTURE_HEIGHT              240

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/// The RMV video codec proxy.
class Codec_MmeVideoRmv_c : public Codec_MmeVideo_c
{
private:

    // Data

    RV89Dec_InitTransformerParam_t      InitializationParameters;

    bool                                RestartTransformer;

    BufferDataDescriptor_t*             SegmentListDescriptor;
    BufferType_t                        SegmentListType;
    BufferPool_t                        SegmentListPool;

    // Functions


public:

    //
    // Constructor/Destructor methods
    //

    Codec_MmeVideoRmv_c(                void );
    ~Codec_MmeVideoRmv_c(               void );

    //
    // Stream specific functions
    //

protected:

    CodecStatus_t   Reset(                                      void );
    CodecStatus_t   RegisterOutputBufferRing(                   Ring_t                          Ring );
    CodecStatus_t   HandleCapabilities(                         void );
    CodecStatus_t   FillOutTransformerInitializationParameters( void );
    CodecStatus_t   FillOutSetStreamParametersCommand(          void );
    CodecStatus_t   FillOutDecodeCommand(                       void );
    CodecStatus_t   FillOutDecodeBufferRequest(                 BufferStructure_t              *Request );
    CodecStatus_t   ValidateDecodeContext(                      CodecBaseDecodeContext_t       *Context );
    CodecStatus_t   DumpSetStreamParameters(                    void                           *Parameters );
    CodecStatus_t   DumpDecodeParameters(                       void                           *Parameters );
   
    CodecStatus_t   SendMMEStreamParameters(                    void );
    CodecStatus_t   CheckCodecReturnParameters( CodecBaseDecodeContext_t *Context );
    
};
#endif
