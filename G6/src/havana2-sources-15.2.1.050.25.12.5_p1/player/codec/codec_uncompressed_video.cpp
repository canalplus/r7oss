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

#include "codec_uncompressed_video.h"
#include "uncompressed.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_UncompressedVideo_c"

// /////////////////////////////////////////////////////////////////////////
//
// Class Constructor
//

Codec_UncompressedVideo_c::Codec_UncompressedVideo_c()
    : BufferManager(NULL)
    , DataTypesInitialized(false)
    , UncompressedTrickModeParameters()
    , DecodeBufferPool(NULL)
    , mOutputPort(NULL)
{
    SetGroupTrace(group_decoder_video);

    // Setup the trick mode parameters
    UncompressedTrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration   = 120;
    UncompressedTrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration    = 120;
    UncompressedTrickModeParameters.DecodeFrameRateShortIntegrationForIOnly       = 120;
    UncompressedTrickModeParameters.DecodeFrameRateLongIntegrationForIOnly        = 120;
    UncompressedTrickModeParameters.SubstandardDecodeSupported        = false;
    UncompressedTrickModeParameters.SubstandardDecodeRateIncrease     = 1;
    UncompressedTrickModeParameters.DefaultGroupSize                  = 1;
    UncompressedTrickModeParameters.DefaultGroupReferenceFrameCount   = 1;
}

// /////////////////////////////////////////////////////////////////////////
//
// Class Destructor
//

Codec_UncompressedVideo_c::~Codec_UncompressedVideo_c()
{
    Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
// Connect output port
//

CodecStatus_t   Codec_UncompressedVideo_c::Connect(Port_c *Port)
{
    if (Port == NULL)
    {
        SE_ERROR("Incorrect input param\n");
        return CodecError;
    }
    if (mOutputPort != NULL)
    {
        SE_WARNING("Port already connected\n");
    }
    mOutputPort = Port;

    //
    // Obtain the buffer manager
    //
    Player->GetBufferManager(&BufferManager);
    //
    // Obtain the decode buffer pool
    //
    Player->GetDecodeBufferPool(Stream, &DecodeBufferPool);
    if (DecodeBufferPool == NULL)
    {
        SE_ERROR("This implementation does not support no-output decoding\n");
        return CodecError;
    }

    //
    // Attach the stream specific (audio|video|data)
    // parsed frame parameters to the decode buffer pool.
    //
    BufferStatus_t Status = DecodeBufferPool->AttachMetaData(Player->MetaDataParsedVideoParametersType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to attach stream specific parsed parameters to all decode buffers\n");
        return CodecError;
    }

    //
    // Go live
    //
    SetComponentState(ComponentRunning);
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// Release a decode buffer - just do a decrement reference count
//

CodecStatus_t   Codec_UncompressedVideo_c::ReleaseDecodeBuffer(Buffer_t Buffer)
{
    Stream->GetDecodeBufferManager()->ReleaseBuffer(Buffer, false);
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// Input a buffer - just pass it on
//

CodecStatus_t   Codec_UncompressedVideo_c::Input(Buffer_t CodedBuffer)
{
    CodecStatus_t               Status;
    unsigned int                CodedDataLength;
    UncompressedBufferDesc_t    *BufferDesc;
    ParsedFrameParameters_t     *ParsedFrameParameters;
    ParsedVideoParameters_t     *ParsedVideoParameters;

    //
    // Extract the uncompressed buffer descriptor
    //
    BufferStatus_t BufStatus = CodedBuffer->ObtainDataReference(NULL, &CodedDataLength, (void **)(&BufferDesc), CachedAddress);
    if ((BufStatus != BufferNoError) && (BufStatus != BufferNoDataAttached))
    {
        SE_ERROR("Unable to obtain data reference\n");
        return CodecError;
    }

    CodedBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    CodedBuffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)&ParsedVideoParameters);
    SE_ASSERT(ParsedVideoParameters != NULL);

    //
    // Handle the special case of a marker frame
    //

    if ((CodedDataLength == 0) && !ParsedFrameParameters->NewStreamParameters && !ParsedFrameParameters->NewFrameParameters)
    {
        DecodeBufferRequest_t   BufferRequest;
        Buffer_t                MarkerBuffer;

        //
        // Get a marker buffer
        //
        memset(&BufferRequest, 0, sizeof(DecodeBufferRequest_t));
        BufferRequest.MarkerFrame  = true;
        BufStatus = Stream->GetDecodeBufferManager()->GetDecodeBuffer(&BufferRequest, &MarkerBuffer);
        if (BufStatus != BufferNoError)
        {
            SE_ERROR("Failed to get marker decode buffer from decode buffer manager\n");
            return CodecError;
        }

        MarkerBuffer->TransferOwnership(IdentifierCodec);

        BufStatus = MarkerBuffer->AttachMetaData(Player->MetaDataParsedFrameParametersReferenceType,
                                                 UNSPECIFIED_SIZE,
                                                 (void *)ParsedFrameParameters);
        if (BufStatus != BufferNoError)
        {
            SE_ERROR("Unable to attach a reference to \"ParsedFrameParameters\" to the marker buffer\n");
            return CodecError;
        }

        MarkerBuffer->AttachBuffer(CodedBuffer);
        //
        // Queue/pass on the buffer
        //
        mOutputPort->Insert((uintptr_t)MarkerBuffer);
        return CodecNoError;
    }

    //
    // Attach the coded data fields to the decode/captured buffer
    //
    Buffer_t                    CapturedBuffer;
    ParsedVideoParameters_t     *CapturedParsedVideoParameters;

    CapturedBuffer = (Buffer_t)BufferDesc->BufferClass;

    if (CapturedBuffer == NULL)
    {
        SE_FATAL("NULL Buffer\n");
        return CodecError;
    }

    CapturedBuffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)&CapturedParsedVideoParameters);
    SE_ASSERT(CapturedParsedVideoParameters != NULL);

    memcpy(CapturedParsedVideoParameters, ParsedVideoParameters, sizeof(ParsedVideoParameters_t));

    BufStatus = CapturedBuffer->AttachMetaData(Player->MetaDataParsedFrameParametersReferenceType,
                                               UNSPECIFIED_SIZE,
                                               (void *)ParsedFrameParameters);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to attach Frame Parameters\n");
        return CodecError;
    }

    //
    // Switch the ownership hierarchy, and allow the captured buffer to exist on it's own.
    //
    CodedBuffer->DetachBuffer(CapturedBuffer);

    Status      = CapturedBuffer->AttachBuffer(CodedBuffer);

    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to attach captured buffer to CodedFrameBuffer\n");
        return Status;
    }

    //
    // Pass the captured buffer on
    //
    mOutputPort->Insert((uintptr_t)CapturedBuffer);
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
// Return the trick mode parameters
//

CodecStatus_t   Codec_UncompressedVideo_c::GetTrickModeParameters(CodecTrickModeParameters_t    *TrickModeParameters)
{
    *TrickModeParameters    = UncompressedTrickModeParameters;
    return CodecNoError;
}

