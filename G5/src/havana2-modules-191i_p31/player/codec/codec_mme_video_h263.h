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
Author :           Julian

Definition of the stream specific codec implementation for H263 video in player 2


Date        Modification                                    Name
----        ------------                                    --------
20-May-08   Created                                         Julian

************************************************************************/

#ifndef H_CODEC_MME_VIDEO_H263
#define H_CODEC_MME_VIDEO_H263

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video.h"
#include "H263Dec_TransformerTypes.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
#define H263_MME_TRANSFORMER_NAME               "H263_TRANSFORMER"

#define H263_NUM_MME_INPUT_BUFFERS              1
#define H263_NUM_MME_OUTPUT_BUFFERS             2
#define H263_NUM_MME_BUFFERS                    (H263_NUM_MME_INPUT_BUFFERS+H263_NUM_MME_OUTPUT_BUFFERS)

#define H263_MME_CURRENT_FRAME_BUFFER           0
#define H263_MME_REFERENCE_FRAME_BUFFER         1
#define H263_MME_CODED_DATA_BUFFER              2


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/// The H263 video codec proxy.
class Codec_MmeVideoH263_c : public Codec_MmeVideo_c
{
private:

    // Data

    H263D_Capability_t                  H263TransformerCapability;
    H263D_GlobalParams_t                H263InitializationParameters;

    // Functions

public:

    //
    // Constructor/Destructor methods
    //

    Codec_MmeVideoH263_c(                void );
    ~Codec_MmeVideoH263_c(               void );

    //
    // Stream specific functions
    //

protected:

    CodecStatus_t   Reset( void );
    CodecStatus_t   HandleCapabilities(                         void );

    CodecStatus_t   FillOutTransformerInitializationParameters( void );
    CodecStatus_t   FillOutSetStreamParametersCommand(          void );
    CodecStatus_t   FillOutDecodeCommand(                       void );

    CodecStatus_t   ValidateDecodeContext( CodecBaseDecodeContext_t *Context );
    CodecStatus_t   DumpSetStreamParameters(                    void    *Parameters );
    CodecStatus_t   DumpDecodeParameters(                       void    *Parameters );
};
#endif
