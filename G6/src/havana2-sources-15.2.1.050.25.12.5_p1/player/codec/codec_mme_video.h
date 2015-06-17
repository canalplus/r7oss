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

#ifndef H_CODEC_MME_VIDEO
#define H_CODEC_MME_VIDEO

#include <VideoCompanion.h>
#include "osinline.h"
#include "codec_mme_base.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideo_c"

class Codec_MmeVideo_c : public Codec_MmeBase_c
{
public:
    Codec_MmeVideo_c(void);
    virtual ~Codec_MmeVideo_c(void) {}

    CodecStatus_t   Halt(void);

    //
    // Codec class functions
    //

    CodecStatus_t   Input(Buffer_t          CodedBuffer);

    //
    // Extension to base functions
    //

    CodecStatus_t   InitializeDataTypes(void);

    //
    // Implementation of fill out function for generic video,
    // may be overridden if necessary.
    //

    virtual CodecStatus_t   FillOutDecodeBufferRequest(DecodeBufferRequest_t     *Request);

protected:
    ParsedVideoParameters_t              *ParsedVideoParameters;
    bool                                  KnownLastSliceInFieldFrame;

#ifdef DELTATOP_MME_VERSION
    DeltaTop_TransformerCapability_t      DeltaTopTransformCapability;
#endif

    CodecStatus_t   VerifyMMECapabilities(unsigned int ActualTransformer);
    CodecStatus_t   VerifyMMECapabilities() { return VerifyMMECapabilities(SelectedTransformer); }

private:
    DISALLOW_COPY_AND_ASSIGN(Codec_MmeVideo_c);
};
#endif
