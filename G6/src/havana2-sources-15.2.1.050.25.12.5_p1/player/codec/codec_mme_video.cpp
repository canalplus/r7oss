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

#include "codec_mme_video.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideo_c"

#define DECODE_RATE_LOWER_LIMIT             Rational_t(3,4)     // Used to be 1, changed to trap failure to decode of divxhd
#define DECODE_RATE_UPPER_LIMIT             6

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor function, fills in the codec specific parameter values
//

Codec_MmeVideo_c::Codec_MmeVideo_c(void)
    : Codec_MmeBase_c()
    , ParsedVideoParameters(NULL)
    , KnownLastSliceInFieldFrame(false)
    , DeltaTopTransformCapability()
{
    SetGroupTrace(group_decoder_video);

    Configuration.CodecName = "unknown video";

    strncpy(Configuration.TranscodedMemoryPartitionName, "vid-transcoded",
            sizeof(Configuration.TranscodedMemoryPartitionName));
    Configuration.TranscodedMemoryPartitionName[sizeof(Configuration.TranscodedMemoryPartitionName) - 1] = '\0';
    strncpy(Configuration.AncillaryMemoryPartitionName, "vid-codec-data",
            sizeof(Configuration.AncillaryMemoryPartitionName));
    Configuration.AncillaryMemoryPartitionName[sizeof(Configuration.AncillaryMemoryPartitionName) - 1] = '\0';
}

// /////////////////////////////////////////////////////////////////////////
//
//  The Halt function, give up access to any registered resources
//

CodecStatus_t   Codec_MmeVideo_c::Halt(void)
{
    ParsedVideoParameters   = NULL;
    KnownLastSliceInFieldFrame  = false;
    return Codec_MmeBase_c::Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//  The get coded frame buffer pool fn
//

CodecStatus_t   Codec_MmeVideo_c::Input(Buffer_t      CodedBuffer)
{
    unsigned int          i;
    CodecStatus_t         Status;
    ParsedVideoParameters_t  *PreviousFieldParameters;
    bool              LastSlice;
    bool              SeenBothFields;
    bool              LastDecodeIntoThisBuffer;
    CodecBufferState_t   *State;
    //
    // Are we allowed in here
    //
    AssertComponentState(ComponentRunning);
    //
    // First perform base operations
    //
    Status      = Codec_MmeBase_c::Input(CodedBuffer);

    if (Status != CodecNoError)
    {
        return Status;
    }

    //
    // Do we need to issue a new set of stream parameters
    //

    if (ParsedFrameParameters->NewStreamParameters)
    {
        Status      = FillOutSetStreamParametersCommand();

        if (Status == CodecNoError)
        {
            Status    = SendMMEStreamParameters();
        }

        if (Status != CodecNoError)
        {
            SE_ERROR("(%s) - Failed to fill out, and send, a set stream parameters command\n", Configuration.CodecName);
            ForceStreamParameterReload      = true;
            StreamParameterContextBuffer->DecrementReferenceCount();
            StreamParameterContextBuffer    = NULL;
            StreamParameterContext      = NULL;
            return Status;
        }

        StreamParameterContextBuffer    = NULL;
        StreamParameterContext      = NULL;
    }

    //
    // If there is no frame to decode then exit now.
    //

    if (!ParsedFrameParameters->NewFrameParameters)
    {
        return CodecNoError;
    }

    //
    // Test if this frame indicates completion of any previous decodes (for slice decoding)
    //
    OS_LockMutex(&Lock);

    if (Configuration.SliceDecodePermitted &&
        ParsedVideoParameters->FirstSlice &&
        (CurrentDecodeBufferIndex != INVALID_INDEX) &&
        (BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters->PictureStructure == StructureFrame))
    {
        // Operation cannot fail
        SetOutputOnDecodesComplete(CurrentDecodeBufferIndex, true);
        CurrentDecodeBufferIndex    = INVALID_INDEX;
    }

    //
    // Check for a half buffer
    //

    if ((CurrentDecodeBufferIndex != INVALID_INDEX) && ParsedFrameParameters->FirstParsedParametersForOutputFrame)
    {
        SE_ERROR("(%s) - New frame starts when we have one field in decode buffer\n", Configuration.CodecName);
        // FIXME: This is really bad
        OS_UnLockMutex(&Lock);
        Codec_MmeVideo_c::OutputPartialDecodeBuffers();
        OS_LockMutex(&Lock);
    }

    //
    // Obtain a new buffer if needed
    //

    if (CurrentDecodeBufferIndex == INVALID_INDEX)
    {
        Status  = GetDecodeBuffer();

        if (Status != CodecNoError)
        {
            SE_ERROR("(%s) - Failed to get decode buffer\n", Configuration.CodecName);
            ReleaseDecodeContext(DecodeContext);
            OS_UnLockMutex(&Lock);
            return Status;
        }

        //
        // reset the buffer content to contain no data
        //
        State   = &BufferState[CurrentDecodeBufferIndex];
        State->ParsedVideoParameters->PictureStructure  = StructureEmpty;
        State->FieldDecode              = ParsedVideoParameters->PictureStructure != StructureFrame;
    }
    //
    // If we are re-using the buffer, and this is the first slice
    // (hence of a second field) we first check the decode components
    // for a reference frame (to support first field non-reference,
    // second field reference). Then we update the field counts in the
    // buffer record.
    //
    else if (ParsedVideoParameters->FirstSlice)
    {
        if (ParsedFrameParameters->ReferenceFrame)
        {
            Status  = Stream->GetDecodeBufferManager()->EnsureReferenceComponentsPresent(BufferState[CurrentDecodeBufferIndex].Buffer);

            if (Status != PlayerNoError)
            {
                SE_ERROR("(%s) - Failed to ensure reference components present\n", Configuration.CodecName);
                OS_UnLockMutex(&Lock);
                Codec_MmeVideo_c::OutputPartialDecodeBuffers();
                return Status;
            }
        }

//
        Status  = MapBufferToDecodeIndex(ParsedFrameParameters->DecodeFrameIndex, CurrentDecodeBufferIndex);

        if (Status != CodecNoError)
        {
            SE_ERROR("(%s) - Failed to map second field index to decode buffer - Implementation error\n", Configuration.CodecName);
            OS_UnLockMutex(&Lock);
            Codec_MmeVideo_c::OutputPartialDecodeBuffers();
            return PlayerImplementationError;
        }

//
        PreviousFieldParameters             = BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters;

        if (PreviousFieldParameters == NULL)
        {
            SE_ERROR("(%s) - PreviousFieldParameters (ParsedVideoParameters, idx %d) are NULL\n", Configuration.CodecName, CurrentDecodeBufferIndex);
            OS_UnLockMutex(&Lock);
            return CodecError;
        }

        if (PreviousFieldParameters->DisplayCount[1] != 0)
        {
            SE_ERROR("(%s) - DisplayCount for second field non-zero after decoding only first field - Implementation error\n", Configuration.CodecName);
            OS_UnLockMutex(&Lock);
            Codec_MmeVideo_c::OutputPartialDecodeBuffers();
            return PlayerImplementationError;
        }

        PreviousFieldParameters->DisplayCount[1]    = ParsedVideoParameters->DisplayCount[0];

//

        if ((PreviousFieldParameters->PanScanCount + ParsedVideoParameters->PanScanCount) >
            MAX_PAN_SCAN_VALUES)
        {
            SE_ERROR("(%s) - Cumulative PanScanCount in two fields too great (%d + %d) - Implementation error\n", Configuration.CodecName,
                     PreviousFieldParameters->PanScanCount, ParsedVideoParameters->PanScanCount);
            OS_UnLockMutex(&Lock);
            Codec_MmeVideo_c::OutputPartialDecodeBuffers();
            return PlayerImplementationError;
        }

        for (i = 0; i < ParsedVideoParameters->PanScanCount; i++)
        {
            memcpy(&PreviousFieldParameters->PanScan[i + PreviousFieldParameters->PanScanCount], &ParsedVideoParameters->PanScan[i], sizeof(PanScan_t));
        }
        PreviousFieldParameters->PanScanCount += ParsedVideoParameters->PanScanCount;
    }

    //
    // Record the buffer being used in the decode context
    //
    DecodeContext->BufferIndex  = CurrentDecodeBufferIndex;
    //
    // Translate reference lists, and Update the reference frame access counts
    //
    Status  = TranslateReferenceFrameLists(ParsedVideoParameters->FirstSlice);

    if (Status != CodecNoError)
    {
        SE_ERROR("(%s) - Failed to find all reference frames - skipping frame\n", Configuration.CodecName);
        ReleaseDecodeContext(DecodeContext);

        if (BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters->PictureStructure != StructureEmpty)
        {
            OS_UnLockMutex(&Lock);
            Codec_MmeVideo_c::OutputPartialDecodeBuffers();
        }
        else
        {
            ReleaseDecodeBufferLocked(BufferState[CurrentDecodeBufferIndex].Buffer);
            CurrentDecodeBufferIndex    = INVALID_INDEX;
            OS_UnLockMutex(&Lock);
        }

        if (ParsedFrameParameters->ReferenceFrame)
        {
            Codec_MmeVideo_c::ReleaseReferenceFrame(ParsedFrameParameters->DecodeFrameIndex);
        }

        return Status;
    }

    //
    // Provide default arguments for the input and output buffers. Default to no buffers not because there
    // are no buffers but because the video firmware interface uses a backdoor to gain access to the buffers.
    // Yes, this does violate the spec. but does nevertheless work on the current crop of systems.
    //
    DecodeContext->MMECommand.NumberInputBuffers        = 0;
    DecodeContext->MMECommand.NumberOutputBuffers       = 0;
    DecodeContext->MMECommand.DataBuffers_p         = NULL;
    //
    // Load the parameters into MME command
    //
    OS_UnLockMutex(&Lock);   // FIXME
    Status  = FillOutDecodeCommand();
    OS_LockMutex(&Lock);   // FIXME

    if (Status != CodecNoError)
    {
        SE_ERROR("(%s) - Failed to fill out a decode command\n", Configuration.CodecName);
        ReleaseDecodeContext(DecodeContext);

        if (BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters->PictureStructure != StructureEmpty)
        {
            OS_UnLockMutex(&Lock);
            Codec_MmeVideo_c::OutputPartialDecodeBuffers();
        }
        else
        {
            ReleaseDecodeBufferLocked(BufferState[CurrentDecodeBufferIndex].Buffer);
            CurrentDecodeBufferIndex    = INVALID_INDEX;
            OS_UnLockMutex(&Lock);
        }

        if (ParsedFrameParameters->ReferenceFrame)
        {
            Codec_MmeVideo_c::ReleaseReferenceFrame(ParsedFrameParameters->DecodeFrameIndex);
        }

        return Status;
    }

    //
    // Update ongoing decode count, and completion flags
    //
    BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters->PictureStructure   |= ParsedVideoParameters->PictureStructure;
    LastSlice           = Configuration.SliceDecodePermitted ? KnownLastSliceInFieldFrame : true;
    SeenBothFields      = BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters->PictureStructure == StructureFrame;
    LastDecodeIntoThisBuffer    = SeenBothFields && LastSlice;
    DecodeContext->DecodeInProgress = true;
    BufferState[CurrentDecodeBufferIndex].DecodesInProgress++;
    BufferState[CurrentDecodeBufferIndex].OutputOnDecodesComplete   = LastDecodeIntoThisBuffer;
    OS_UnLockMutex(&Lock);
    //
    // Ensure that the coded frame will be available throughout the
    // life of the decode by attaching the coded frame to the decode
    // context prior to launching the decode.
    //
    DecodeContextBuffer->AttachBuffer(CodedFrameBuffer);
    //! set up MME_TRANSFORM - SendMMEDecodeCommand no longer does this as we need to do
    //! MME_SEND_BUFFERS instead for certain codecs, WMA being one, OGG Vorbis another
    DecodeContext->MMECommand.CmdCode = MME_TRANSFORM;
    Status  = SendMMEDecodeCommand();

    if (Status != CodecNoError)
    {
        SE_ERROR("(%s) - Failed to send a decode command\n", Configuration.CodecName);
        // FIXME: ?? include SendMMEDecodeCommand() call in lock ??
        OS_LockMutex(&Lock);
        ReleaseDecodeContext(DecodeContext);
        OS_UnLockMutex(&Lock);
        return Status;
    }

    //
    // have we finished decoding into this buffer
    //

    if (LastDecodeIntoThisBuffer)
    {
        CurrentDecodeBufferIndex = INVALID_INDEX;
        CurrentDecodeIndex       = INVALID_INDEX;
    }
    else
    {
        CurrentDecodeIndex      = ParsedFrameParameters->DecodeFrameIndex;
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  The intercept to the initialise data types function, that
//  ensures the video specific type is recorded in the configuration
//  record.
//

CodecStatus_t   Codec_MmeVideo_c::InitializeDataTypes(void)
{
    //
    // Add video specific types, and the address of
    // parsed video parameters to the configuration
    // record. Then pass on down to the base class
    //
    Configuration.AudioVideoDataParsedParametersType    = Player->MetaDataParsedVideoParametersType;
    Configuration.AudioVideoDataParsedParametersPointer = (void **)&ParsedVideoParameters;
    Configuration.SizeOfAudioVideoDataParsedParameters  = sizeof(ParsedVideoParameters_t);
    return Codec_MmeBase_c::InitializeDataTypes();
}


// /////////////////////////////////////////////////////////////////////////
//
//  The generic video function used to fill out a buffer structure
//  request.
//

CodecStatus_t   Codec_MmeVideo_c::FillOutDecodeBufferRequest(DecodeBufferRequest_t   *Request)
{
    bool        DecimatedComponentNeeded;
    unsigned char   Decimate;

    memset(Request, 0, sizeof(DecodeBufferRequest_t));

    if (ParsedVideoParameters == NULL)
    {
        SE_ERROR("(%s) - ParsedVideoParameters NULL\n", Configuration.CodecName);
        goto bail_err;
    }

    Decimate = Player->PolicyValue(Playback, Stream, PolicyDecimateDecoderOutput);
    DecimatedComponentNeeded = Configuration.DecimatedDecodePermitted &&
                               (Decimate != PolicyValueDecimateDecoderOutputDisabled);
    Stream->GetDecodeBufferManager()->ComponentEnable(DecimatedManifestationComponent, DecimatedComponentNeeded);
    Request->DimensionCount     = 2;
    Request->Dimension[0]       = ParsedVideoParameters->Content.DecodeWidth;
    Request->Dimension[1]       = ParsedVideoParameters->Content.DecodeHeight;

    if (Decimate == PolicyValueDecimateDecoderOutputHalf)
    {
        Request->DecimationFactors[0] = 2;
        Request->DecimationFactors[1] = 2;
    }
    else if (Decimate == PolicyValueDecimateDecoderOutputQuarter)
    {
        Request->DecimationFactors[0] = 4;
        Request->DecimationFactors[1] = 2;    // Only support a factor of 2 vertically
    }

    if (ParsedFrameParameters == NULL)
    {
        SE_ERROR("(%s) -  ParsedFrameParameters NULL\n", Configuration.CodecName);
        goto bail_err;
    }
    Request->ReferenceFrame = ParsedFrameParameters->ReferenceFrame;
    return CodecNoError;

bail_err:
    //Default values to avoid div by 0 later ...
    if ((Request->DecimationFactors[0] == 0) || (Request->DecimationFactors[1] == 0))
    {
        Request->DecimationFactors[0] = 1;
        Request->DecimationFactors[1] = 1;
    }
    return CodecError;
}

// /////////////////////////////////////////////////////////////////////////
//
// Verify that the transformer is capable of correct operation.
//
// This method is always called before a transformer is initialised.
// This method overrides the base class so that we may query the
// DeltaTop Capabilities
//
CodecStatus_t Codec_MmeVideo_c::VerifyMMECapabilities(unsigned int ActualTransformer)
{
#ifdef DELTATOP_MME_VERSION
    MME_ERROR                        MMEStatus;
    MME_TransformerCapability_t      Capability;
    //
    // Query the capabilities of the DeltaTop
    //
    Configuration.DeltaTopCapabilityStructurePointer = (void *)(&DeltaTopTransformCapability);
    memset(&Capability, 0, sizeof(MME_TransformerCapability_t));
    memset(Configuration.DeltaTopCapabilityStructurePointer, 0, sizeof(DeltaTop_TransformerCapability_t));
    Capability.StructSize           = sizeof(MME_TransformerCapability_t);
    Capability.TransformerInfoSize  = sizeof(DeltaTop_TransformerCapability_t);
    Capability.TransformerInfo_p    = Configuration.DeltaTopCapabilityStructurePointer;
    MMEStatus                       = MME_GetTransformerCapability(DELTATOP_MME_TRANSFORMER_NAME "0", &Capability);

    if (MMEStatus != MME_SUCCESS)
    {
        SE_ERROR("(%s:%s) - Unable to read capabilities (%08x)\n", Configuration.CodecName, DELTATOP_MME_TRANSFORMER_NAME "0", MMEStatus);
        //
        // Failure to identify DELTATOP capabilities is not a fatal error. We may be on an old firmware
        //
    }

#if defined (CONFIG_STM_VIRTUAL_PLATFORM) /* VSOC WORKAROUND : Current native video delta firmware
                                             get capability function is stubbed, update manually... */
    DeltaTopTransformCapability.DecoderCapabilityFlags  = FLV1DEC_CAPABILITY  | VP6DEC_CAPABILITY | MPEG4P2DEC_CAPABILITY | AVSDECSD_CAPABILITY;
    DeltaTopTransformCapability.DecoderCapabilityFlags |= MPEG2DEC_CAPABILITY | VC1DEC_CAPABILITY | H264DEC_CAPABILITY;
    DeltaTopTransformCapability.DisplayBufferFormat     = DELTA_OUTPUT_RASTER;
#endif
#endif
    return Codec_MmeBase_c::VerifyMMECapabilities(ActualTransformer);
}
