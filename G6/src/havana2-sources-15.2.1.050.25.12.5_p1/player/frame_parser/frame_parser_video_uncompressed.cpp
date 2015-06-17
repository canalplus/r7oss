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

#include "mpeg4.h"
#include "frame_parser_video_uncompressed.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoUncompressed_c"
//#define DUMP_HEADERS 1

static BufferDataDescriptor_t     UncompressedStreamParametersBuffer = BUFFER_MPEG4_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     UncompressedFrameParametersBuffer = BUFFER_MPEG4_FRAME_PARAMETERS_TYPE;

////////////////////////////////////////////////////////////////////////////
//
//  Constructor
//
FrameParser_VideoUncompressed_c::FrameParser_VideoUncompressed_c(void)
    : FrameParser_Video_c()
{
    Configuration.FrameParserName               = "VideoUncompressed";
    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &UncompressedStreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &UncompressedFrameParametersBuffer;
    Configuration.SupportSmoothReversePlay      = false;        // Cannot reverse captured data
    Configuration.InitializeStartCodeList       = false;        // We take a simple structure - no start codes
}

////////////////////////////////////////////////////////////////////////////
//
//  Destructor
//

FrameParser_VideoUncompressed_c::~FrameParser_VideoUncompressed_c(void)
{
    Halt();
}

///////////////////////////////////////////////////////////////////////////
//
//      Method to connect to neighbor
//

FrameParserStatus_t   FrameParser_VideoUncompressed_c::Connect(Port_c *Port)
{
    FrameParserStatus_t Status = FrameParserNoError;

    //
    // Clear our parameter pointers
    //
    DeferredParsedFrameParameters       = NULL;
    DeferredParsedVideoParameters       = NULL;

    Status = FrameParser_Video_c::Connect(Port);

    return Status;
}


////////////////////////////////////////////////////////////////////////////
//
//      The ReadHeaders stream specific function
//

FrameParserStatus_t   FrameParser_VideoUncompressed_c::ReadHeaders(void)
{
    FrameParserStatus_t         Status = FrameParserNoError;
    UncompressedBufferDesc_t    *BufferDesc;
    Buffer_t                    DecodeBuffer;

    //
    // Find the uncompressed buffer descriptor, and extract the buffer
    //
    BufferDesc   = (UncompressedBufferDesc_t *)BufferData;
    DecodeBuffer = (Buffer_t)BufferDesc->BufferClass;

    SE_DEBUG(group_frameparser_video, "BufferDesc %p DecodeBuffer %p\n", BufferDesc, DecodeBuffer);

    //
    // Switch the ownership of the buffer
    //
    DecodeBuffer->TransferOwnership(IdentifierFrameParser);

    //
    // Fill the appropriate frame and video parameters
    //

    if ((0 == BufferDesc->Content.PixelAspectRatio.GetNumerator()) ||
        (0 == BufferDesc->Content.PixelAspectRatio.GetDenominator()))
    {
        SE_INFO(group_frameparser_video, "Invalid pixel aspect ratio (%d:%d); forcing 1:1\n",
                (int)BufferDesc->Content.PixelAspectRatio.GetNumerator(),
                (int)BufferDesc->Content.PixelAspectRatio.GetDenominator());

        BufferDesc->Content.PixelAspectRatio = Rational_t(1, 1);
    }

    if (0 == BufferDesc->Content.FrameRate.GetDenominator())
    {
        SE_INFO(group_frameparser_video, "Invalid framerate (denominator 0); forcing 1\n");
        BufferDesc->Content.FrameRate = Rational_t(BufferDesc->Content.FrameRate.GetNumerator(), 1);
    }

//
    ParsedFrameParameters->FirstParsedParametersForOutputFrame  = true;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump  = false;
    ParsedFrameParameters->SurplusDataInjected                  = false;
    ParsedFrameParameters->ContinuousReverseJump                = false;
    ParsedFrameParameters->KeyFrame                             = true;
    ParsedFrameParameters->IndependentFrame                     = true;
    ParsedFrameParameters->ReferenceFrame                       = true;     // Turn off autogeneration of DTS
    ParsedFrameParameters->NumberOfReferenceFrameLists          = 0;
    ParsedFrameParameters->NewStreamParameters                  = false;
    ParsedFrameParameters->SizeofStreamParameterStructure       = sizeof(UncompressedBufferDesc_t);
    ParsedFrameParameters->StreamParameterStructure             = &BufferDesc;
    ParsedFrameParameters->NewFrameParameters                   = true;
    ParsedFrameParameters->SizeofFrameParameterStructure        = 0;//?
    ParsedFrameParameters->FrameParameterStructure              = NULL;//?
//
    ParsedVideoParameters->Content.Width                    = BufferDesc->Content.Width;
    ParsedVideoParameters->Content.Height                   = BufferDesc->Content.Height;
    ParsedVideoParameters->Content.DisplayWidth             = BufferDesc->Content.Width;
    ParsedVideoParameters->Content.DisplayHeight            = BufferDesc->Content.Height;
    ParsedVideoParameters->Content.Progressive              = !(BufferDesc->Content.Interlaced);
    ParsedVideoParameters->Content.OverscanAppropriate      = 0;
    ParsedVideoParameters->Content.VideoFullRange           = BufferDesc->Content.VideoFullRange;
    ParsedVideoParameters->Content.PixelAspectRatio         = BufferDesc->Content.PixelAspectRatio;
    ParsedVideoParameters->Content.FrameRate                = BufferDesc->Content.FrameRate;
    ParsedVideoParameters->Content.ColourMatrixCoefficients = ((BufferDesc->Content.ColourMode == UNCOMPRESSED_COLOUR_MODE_601) ? MatrixCoefficients_ITU_R_BT601 :
                                                               ((BufferDesc->Content.ColourMode == UNCOMPRESSED_COLOUR_MODE_709) ? MatrixCoefficients_ITU_R_BT709 :
                                                                MatrixCoefficients_Undefined));

    ParsedVideoParameters->InterlacedFrame    = BufferDesc->Content.Interlaced;
    ParsedVideoParameters->DisplayCount[0]    = 1;
    ParsedVideoParameters->DisplayCount[1]    = (ParsedVideoParameters->InterlacedFrame ? 1 : 0);
    ParsedVideoParameters->SliceType          = SliceTypeI;
    ParsedVideoParameters->FirstSlice         = true;
    ParsedVideoParameters->TopFieldFirst      = BufferDesc->Content.TopFieldFirst;
    ParsedVideoParameters->PictureStructure   = StructureFrame;
    ParsedVideoParameters->PanScanCount       = 0;
    FirstDecodeOfFrame                        = true;
    FrameToDecode                             = true;
//
    // TODO Needed?
    Status = CheckForResolutionConstraints(ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);

    return Status;
}


////////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to prepare a reference frame list
//

FrameParserStatus_t   FrameParser_VideoUncompressed_c::PrepareReferenceFrameList(void)
{
    ParsedFrameParameters->NumberOfReferenceFrameLists    = 0;

    return FrameParserNoError;
}
