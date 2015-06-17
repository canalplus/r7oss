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

Source file name : codec_mme_video_mpeg2.h
Author :           Nick

Definition of the stream specific codec implementation for mpeg2 video in player 2


Date        Modification                                    Name
----        ------------                                    --------
25-Jan-07   Created                                         Nick

************************************************************************/

#ifndef H_CODEC_MME_VIDEO_MPEG2
#define H_CODEC_MME_VIDEO_MPEG2

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video.h"

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

/// The MPEG video codec proxy.
class Codec_MmeVideoMpeg2_c : public Codec_MmeVideo_c
{
protected:

    // Data

    MPEG2_TransformerCapability_t         Mpeg2TransformCapability;
    MPEG2_InitTransformerParam_t          Mpeg2InitializationParameters;

    // Functions

public:

    //
    // Constructor/Destructor methods
    //

    Codec_MmeVideoMpeg2_c(              void );
    ~Codec_MmeVideoMpeg2_c(             void );

    //
    // Stream specific functions
    //

protected:

    CodecStatus_t   HandleCapabilities( void );

    CodecStatus_t   FillOutTransformerInitializationParameters( void );
    CodecStatus_t   FillOutSetStreamParametersCommand(          void );
    CodecStatus_t   FillOutDecodeCommand(                       void );
    CodecStatus_t   ValidateDecodeContext( CodecBaseDecodeContext_t *Context );
    CodecStatus_t   DumpSetStreamParameters(                    void    *Parameters );
    CodecStatus_t   DumpDecodeParameters(                       void    *Parameters );
    CodecStatus_t   CheckCodecReturnParameters( CodecBaseDecodeContext_t *Context );
};
#endif
