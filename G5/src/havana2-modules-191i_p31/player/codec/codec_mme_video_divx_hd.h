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

Source file name : codec_mme_video_divx.h
Author :           Chris

Definition of the stream specific codec implementation for Divx video in player 2


Date        Modification                                    Name
----        ------------                                    --------
11-Jun-07   Created                                         Chris

************************************************************************/

#ifndef H_CODEC_MME_VIDEO_DIVX_HD
#define H_CODEC_MME_VIDEO_DIVX_HD

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video.h"
#include "mpeg4.h"
#include "mme.h"

#include "VideoCompanion.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define Divx_TransformerCapability_t MME_DivXVideoDecodeCapabilityParams_t
#define Divx_InitTransformerParam_t  MME_DivXVideoDecodeInitParams_t
// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//
// /////////////////////////////////////////////////////////////////////////

class Codec_MmeVideoDivxHd_c : public Codec_MmeVideo_c
{
protected:

	// Data
	MME_DivXHD_Capability_t             DivxTransformCapability;
	MME_DivXHDVideoDecodeInitParams_t   DivxInitializationParameters;
	
	MME_DivXHDSetGlobalParamSequence_t  DivXGlobalParamSequence;	

	BufferDataDescriptor_t*             DivxMbStructDescriptor;
	BufferType_t                        DivxMbStructType;
	BufferPool_t                        DivxMbStructPool;

	MME_DivXHDVideoDecodeReturnParams_t   ReturnParams;
	MME_DivXHDVideoDecodeParams_t       DecodeParams;     

	unsigned int StreamWidth;
	unsigned int StreamHeight;
	
	bool Supports311;
public:

	// Constructor/Destructor methods

	 Codec_MmeVideoDivxHd_c( void );
	~Codec_MmeVideoDivxHd_c( void );

protected:

	// Functions
	CodecStatus_t   Reset( void );
	CodecStatus_t   HandleCapabilities( void );

	CodecStatus_t   RegisterOutputBufferRing(   Ring_t                    Ring );
	CodecStatus_t   FillOutTransformerInitializationParameters( void );
	CodecStatus_t   FillOutSetStreamParametersCommand( void );
	CodecStatus_t   FillOutDecodeCommand( void );
    
	CodecStatus_t   ValidateDecodeContext( CodecBaseDecodeContext_t *Context );
    
	CodecStatus_t   DumpSetStreamParameters( void *Parameters );
	CodecStatus_t   DumpDecodeParameters( void *Parameters );

	CodecStatus_t   InitializeMMETransformer( void );
};
#endif
