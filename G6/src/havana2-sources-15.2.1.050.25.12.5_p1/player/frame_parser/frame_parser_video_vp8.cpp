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

//#define DUMP_HEADERS

#include "vp8.h"
#include "ring_generic.h"
#include "frame_parser_video_vp8.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoVp8_c"

static BufferDataDescriptor_t     Vp8StreamParametersBuffer     = BUFFER_VP8_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     Vp8FrameParametersBuffer      = BUFFER_VP8_FRAME_PARAMETERS_TYPE;

const int Vp8MbFeatureDataBits[VP8_MB_LVL_MAX]          = {7, 6};

//#if defined (DUMP_HEADERS)
static unsigned int PictureNo;
//#endif

FrameParser_VideoVp8_c::FrameParser_VideoVp8_c(void)
    : FrameParser_Video_c()
    , BoolDecoder()
    , StreamParameters(NULL)
    , FrameParameters(NULL)
    , CopyOfStreamParameters()
    , FrameRate()
    , MetaData()
    , StreamMetadataValid(false)
{
    Configuration.FrameParserName               = "VideoVp8";
    Configuration.StreamParametersCount         = 4;
    Configuration.StreamParametersDescriptor    = &Vp8StreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &Vp8FrameParametersBuffer;

    memset(&ReferenceFrameList, 0, sizeof(ReferenceFrameList_t)*VP8_NUM_REF_FRAME_LISTS);  // TODO(pht) fixme

    DefaultFrameRate                            = Rational_t (24000, 1001);

//#if defined (DUMP_HEADERS)
    PictureNo                                   = 0;
//#endif
}

FrameParser_VideoVp8_c::~FrameParser_VideoVp8_c(void)
{
    Halt();
}

//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp8_c::ReadHeaders(void)
{
    FrameParserStatus_t         Status  = FrameParserNoError;

    if (!StreamMetadataValid)
    {
        Status          = ReadStreamMetadata();
    }
    else
    {
        Status          = ReadPictureHeader();

        if (Status == FrameParserNoError)
        {
            Status      = CommitFrameForDecode();
        }
    }

    return Status;
}
//}}}
//{{{  ReadStreamMetadata
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in an abc metadata structure
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp8_c::ReadStreamMetadata(void)
{
    if ((StreamParameters != NULL) && (StreamParameters->SequenceHeaderPresent))
    {
        SE_ERROR("Received Sequence Layer MetaData after previous sequence data\n");
        return FrameParserNoError;
    }

    memcpy(&MetaData, BufferData, sizeof(MetaData));

    if (MetaData.FrameRate == 0)
    {
        FrameRate                               = Rational_t (30000, 1000);
    }
    else if (MetaData.FrameRate < 23950000)
    {
        FrameRate                               = Rational_t (MetaData.FrameRate, 1000000);
    }
    else if (MetaData.FrameRate < 23990000)
    {
        FrameRate                               = Rational_t (24000, 1001);
    }
    else if (MetaData.FrameRate < 24050000)
    {
        FrameRate                               = Rational_t (24000, 1000);
    }
    else if (MetaData.FrameRate < 24950000)
    {
        FrameRate                               = Rational_t (MetaData.FrameRate, 1000000);
    }
    else if (MetaData.FrameRate < 25050000)
    {
        FrameRate                               = Rational_t (25000, 1000);
    }
    else if (MetaData.FrameRate < 29950000)
    {
        FrameRate                               = Rational_t (MetaData.FrameRate, 1000000);
    }
    else if (MetaData.FrameRate < 29990000)
    {
        FrameRate                               = Rational_t (30000, 1001);
    }
    else if (MetaData.FrameRate < 30050000)
    {
        FrameRate                               = Rational_t (30000, 1000);
    }
    else
    {
        FrameRate                               = Rational_t (MetaData.FrameRate, 1000000);
    }

    StreamMetadataValid                         = true;//(MetaData.Codec == CODEC_ID_VP8)
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video, "StreamMetadata :-\n");
    SE_INFO(group_frameparser_video, "    Codec             : %6d\n", MetaData.Codec);
    SE_INFO(group_frameparser_video, "    Width             : %6d\n", MetaData.Width);
    SE_INFO(group_frameparser_video, "    Height            : %6d\n", MetaData.Height);
    SE_INFO(group_frameparser_video, "    Duration          : %6d\n", MetaData.Duration);
    SE_INFO(group_frameparser_video, "    FrameRate         : %6d\n", MetaData.FrameRate);
#endif
    return FrameParserNoError;
}
//}}}
//{{{  ReadPictureHeader
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a picture header
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp8_c::ReadPictureHeader(void)
{
    FrameParserStatus_t         Status;
    Vp8VideoPicture_t          *Header;
    Vp8VideoSequence_t         *SequenceHeader;
    unsigned char              *HeaderData      = BufferData;
    unsigned int                FrameTag;
    unsigned int                StartCode;
    unsigned int                SizeCode;
    unsigned int                KeyFrame;
    unsigned int                Version;
    unsigned int                EncodedWidth;
    unsigned int                HorizontalScale;
    unsigned int                EncodedHeight;
    unsigned int                VerticalScale;

    if (FrameParameters == NULL)
    {
        Status                                  = GetNewFrameParameters((void **)&FrameParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }
    }

    Header                                      = &FrameParameters->PictureHeader;
    memset(Header, 0, sizeof(Vp8VideoPicture_t));
    Header->refresh_golden_frame                = VP8_REFRESH_GOLDEN_FRAME;
    Header->refresh_alternate_frame             = VP8_REFRESH_ALTERNATE_FRAME;
    Header->refresh_last_frame                  = VP8_REFRESH_LAST_FRAME;

    FrameTag                                    = HeaderData[0] + (HeaderData[1] << 8) + (HeaderData[2] << 16);
    KeyFrame                                    = (FrameTag & 0x1) == 0;
    Version                                     = (FrameTag >> 1) & 0x7;
    Header->show_frame                          = (FrameTag >> 4) & 0x1;
    Header->first_part_size                     = (FrameTag >> 5) & 0x7FFFF;
    HeaderData                                 += 3;

    if (KeyFrame)
    {
        Header->ptype                           = VP8_PICTURE_CODING_TYPE_I;
        StartCode                               = HeaderData[2] + (HeaderData[1] << 8) + (HeaderData[0] << 16);

        if (StartCode != VP8_FRAME_START_CODE)
        {
            SE_ERROR("Invalid frame start code %06x\n", StartCode);
            return FrameParserError;
        }

        SizeCode                                = HeaderData[3] + (HeaderData[4] << 8);
        EncodedWidth                            = SizeCode & 0x3FFF;
        HorizontalScale                         = SizeCode >> 14;
        SizeCode                                = HeaderData[5] + (HeaderData[6] << 8);
        EncodedHeight                           = SizeCode & 0x3FFF;
        VerticalScale                           = SizeCode >> 14;
        HeaderData                             += 7;

        // For VP8 there is no sequence header so we create one whenever the picture size changes
        if ((StreamParameters == NULL) || (!StreamParameters->SequenceHeaderPresent) ||
            (EncodedWidth  != StreamParameters->SequenceHeader.encoded_width)  ||
            (EncodedHeight != StreamParameters->SequenceHeader.encoded_height) ||
            (HorizontalScale != StreamParameters->SequenceHeader.horizontal_scale)  ||
            (VerticalScale != StreamParameters->SequenceHeader.vertical_scale))
        {
            Status                                      = GetNewStreamParameters((void **)&StreamParameters);

            if (Status != FrameParserNoError)
            {
                return Status;
            }

            StreamParameters->UpdatedSinceLastFrame     = true;
            SequenceHeader                              = &StreamParameters->SequenceHeader;
            memset(SequenceHeader, 0, sizeof(Vp8VideoSequence_t));
            StreamParameters->SequenceHeaderPresent     = true;
            SequenceHeader->version                     = Version;
            SequenceHeader->encoded_width               = EncodedWidth;
            SequenceHeader->encoded_height              = EncodedHeight;
            SequenceHeader->decode_width                = (EncodedWidth + 0x0f) & 0xfff0;
            SequenceHeader->decode_height               = (EncodedHeight + 0x0f) & 0xfff0;
            SequenceHeader->horizontal_scale            = HorizontalScale;
            SequenceHeader->vertical_scale              = VerticalScale;
        }
        else
        {
            SequenceHeader                              = &StreamParameters->SequenceHeader;
        }
    }
    else if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Sequence header not found\n");
        return FrameParserNoStreamParameters;
    }
    else
    {
        Header->ptype                           = VP8_PICTURE_CODING_TYPE_P;
        SequenceHeader                          = &StreamParameters->SequenceHeader;
    }

    BoolDecoder.Init(HeaderData);

    if (KeyFrame)
    {
        SequenceHeader->colour_space            = BoolDecoder.GetBit();
        SequenceHeader->clamping                = BoolDecoder.GetBit();
    }

    if (BoolDecoder.GetBit())                   // segmentation_enabled
        //{{{  read new segmentation map
    {
        // Is the segmentation map being explicitly updated this frame.
        unsigned int            UpdateMbSegmentationMap         = BoolDecoder.GetBit();
        unsigned int            UpdateMbSegmentationData        = BoolDecoder.GetBit();

        if (UpdateMbSegmentationData)
        {
            BoolDecoder.GetBit();

            // For each segmentation feature (Quant and loop filter level)
            for (int i = 0; i < VP8_MB_LVL_MAX; i++)
            {
                for (int j = 0; j < VP8_MAX_MB_SEGMENTS; j++)
                {
                    // Frame level data
                    if (BoolDecoder.GetBit())
                    {
                        BoolDecoder.GetBits(Vp8MbFeatureDataBits[i]);
                        BoolDecoder.GetBit();
                    }
                }
            }
        }

        if (UpdateMbSegmentationMap)
        {
            // Skip the probs used to decode the segment id for each macro block.
            for (int i = 0; i < VP8_MB_FEATURE_TREE_PROBS; i++)
            {
                if (BoolDecoder.GetBit())
                {
                    BoolDecoder.GetBits(8);
                }
            }
        }
    }

    //}}}
    Header->filter_type                         = BoolDecoder.GetBit();
    Header->loop_filter_level                   = BoolDecoder.GetBits(6);
    Header->sharpness_level                     = BoolDecoder.GetBits(3);

    // Read in loop filter deltas applied at the MB level based on mode or ref frame.
    if (BoolDecoder.GetBit())
    {
        // Do the deltas need to be updated
        if (BoolDecoder.GetBit())
        {
            // Send update
            for (int i = 0; i < VP8_MAX_REF_LF_DELTAS; i++)
                if (BoolDecoder.GetBit())
                {
                    BoolDecoder.GetBits(6);
                    BoolDecoder.GetBit();       // sign
                }

            for (int i = 0; i < VP8_MAX_MODE_LF_DELTAS; i++)
                if (BoolDecoder.GetBit())
                {
                    BoolDecoder.GetBits(6);
                    BoolDecoder.GetBit();       // sign
                }
        }
    }

    BoolDecoder.GetBits(2);                     // number of partitions
    // Skip the default quantizers.
    BoolDecoder.GetBits(7);

    for (int i = 0; i < 5; i++)
        if (BoolDecoder.GetBit())
        {
            BoolDecoder.GetBits(4);
            BoolDecoder.GetBit();               // sign
        }

    if (!KeyFrame)
    {
        // Golden frame and alternate reference frame handling
        Header->refresh_golden_frame            = BoolDecoder.GetBit();
        Header->refresh_alternate_frame         = BoolDecoder.GetBit();

        if (Header->refresh_golden_frame != VP8_REFRESH_GOLDEN_FRAME)
        {
            Header->copy_buffer_to_golden       = BoolDecoder.GetBits(2);
        }

        if (Header->refresh_alternate_frame != VP8_REFRESH_ALTERNATE_FRAME)
        {
            Header->copy_buffer_to_alternate    = BoolDecoder.GetBits(2);
        }

        BoolDecoder.GetBit();                   // golden sign bias
        BoolDecoder.GetBit();                   // alternate sign bias
        BoolDecoder.GetBit();                   // refresh entropy probs
        Header->refresh_last_frame              = BoolDecoder.GetBit();
    }

    FrameParameters->PictureHeaderPresent       = true;
#ifdef DUMP_HEADERS

    if (Header->ptype == VP8_PICTURE_CODING_TYPE_I)
    {
        SE_INFO(group_frameparser_video, "Sequence header (%d)\n", PictureNo);
        SE_INFO(group_frameparser_video, "    version             : %6d\n", SequenceHeader->version);
        SE_INFO(group_frameparser_video, "    encoded_width       : %6d\n", SequenceHeader->encoded_width);
        SE_INFO(group_frameparser_video, "    encoded_height      : %6d\n", SequenceHeader->encoded_height);
        SE_INFO(group_frameparser_video, "    decode_width        : %6d\n", SequenceHeader->decode_width);
        SE_INFO(group_frameparser_video, "    decode_height       : %6d\n", SequenceHeader->decode_height);
        SE_INFO(group_frameparser_video, "    horizontal_scale    : %6d\n", SequenceHeader->horizontal_scale);
        SE_INFO(group_frameparser_video, "    vertical_scale      : %6d\n", SequenceHeader->vertical_scale);
        SE_INFO(group_frameparser_video, "    colour_space        : %6d\n", SequenceHeader->colour_space);
    }

    SE_INFO(group_frameparser_video, "Picture header (%d)\n", PictureNo++);
    SE_INFO(group_frameparser_video, "    ptype                     : %6d\n", Header->ptype);
    SE_INFO(group_frameparser_video, "    show_frame                : %6d\n", Header->show_frame);
    SE_INFO(group_frameparser_video, "    first_part_size           : %6d\n", Header->first_part_size);
    SE_INFO(group_frameparser_video, "    filter_type               : %6d\n", Header->filter_type);
    SE_INFO(group_frameparser_video, "    loop_filter_level         : %6d\n", Header->loop_filter_level);
    SE_INFO(group_frameparser_video, "    sharpness_level           : %6d\n", Header->sharpness_level);
    SE_INFO(group_frameparser_video, "    refresh_golden_frame      : %6d\n", Header->refresh_golden_frame);
    SE_INFO(group_frameparser_video, "    refresh_alternate_frame   : %6d\n", Header->refresh_alternate_frame);
    SE_INFO(group_frameparser_video, "    copy_buffer_to_golden     : %6d\n", Header->copy_buffer_to_golden);
    SE_INFO(group_frameparser_video, "    copy_buffer_to_alternate  : %6d\n", Header->copy_buffer_to_alternate);
    SE_INFO(group_frameparser_video, "    refresh_last_frame        : %6d\n", Header->refresh_last_frame);
#else
    SE_DEBUG(group_frameparser_video, "Picture header (%d)\n", PictureNo++);
#endif
    return FrameParserNoError;
}
//}}}

//{{{  Connect
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Connect the output Port
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp8_c::Connect(Port_c *Port)
{
    //
    // Clear our parameter pointers
    //
    StreamParameters                    = NULL;
    FrameParameters                     = NULL;
    DeferredParsedFrameParameters       = NULL;
    DeferredParsedVideoParameters       = NULL;
    //
    // Pass the call on down (we need the frame parameters count obtained by the lower level function).
    //
    return FrameParser_Video_c::Connect(Port);
}
//}}}
//{{{  PrepareReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Stream specific function to prepare a reference frame list
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp8_c::PrepareReferenceFrameList(void)
{
    unsigned int                ReferenceFramesNeeded;
    unsigned int                PictureCodingType;
    Vp8VideoPicture_t          *PictureHeader;
    //
    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequence header or group of pictures
    // header which belong to the next frame.
    //
    // For VP8, every frame is a reference frame so we always fill in the reference frame list
    // even though I frames do not actually need them.
    // Element 0 is for the previous frame, element 1 is for the golden frame and element 2 is
    // for the alternate reference frame.
    PictureHeader               = &(((Vp8FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader);
    PictureCodingType           = PictureHeader->ptype;
    ReferenceFramesNeeded       = (PictureCodingType == VP8_PICTURE_CODING_TYPE_P) ? 1 : 0;

    if (ReferenceFrameList[0].EntryCount < ReferenceFramesNeeded)
    {
        return FrameParserInsufficientReferenceFrames;
    }

    ParsedFrameParameters->NumberOfReferenceFrameLists                          = VP8_NUM_REF_FRAME_LISTS;

    for (int i = 0; i < VP8_NUM_REF_FRAME_LISTS; i++)
    {
        if ((ReferenceFrameList[i].EntryCount == 0) && (PictureCodingType == VP8_PICTURE_CODING_TYPE_I))       // First frame reference self
        {
            ParsedFrameParameters->ReferenceFrameList[i].EntryCount             = 1;
            ParsedFrameParameters->ReferenceFrameList[i].EntryIndicies[0]       = NextDecodeFrameIndex;
        }
        else
        {
            ParsedFrameParameters->ReferenceFrameList[i].EntryCount             = ReferenceFrameList[i].EntryCount;
            ParsedFrameParameters->ReferenceFrameList[i].EntryIndicies[0]       = ReferenceFrameList[i].EntryIndicies[0];
        }
    }

    //SE_INFO(group_frameparser_video,  "Prepare Ref list %d %d - %d %d - %d %d %d\n", ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[1],
    //        ReferenceFrameList.EntryIndicies[0], ReferenceFrameList.EntryIndicies[1],
    //        ReferenceFramesNeeded, ReferenceFrameList.EntryCount, ReferenceFrameList.EntryCount - ReferenceFramesNeeded );
    return FrameParserNoError;
}
//}}}
//{{{  ForPlayUpdateReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Stream specific function to prepare a reference frame list
///             A reference frame is only recorded as such on the last field, in order to
///             ensure the correct management of reference frames in the codec, the
///             codec is informed immediately of a release on the first field of a field picture.
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp8_c::ForPlayUpdateReferenceFrameList(void)
{
    Vp8FrameParameters_t       *FrameParameters = (Vp8FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    Vp8VideoPicture_t          *PictureHeader   = &(FrameParameters->PictureHeader);
    unsigned int                EntryCount[VP8_NUM_REF_FRAME_LISTS + 1];
    unsigned int                EntryIndicies[VP8_NUM_REF_FRAME_LISTS + 1];

    // For VP8 every frame is a reference frame so we always free the current reference frame if it isn't a
    // golden frame.  The reference frame is kept in list 0 slot 0. If the current frame is also a golden frame we
    // free and replace the golden frame in list 1 slot 0 as well.  Alternate reference frames are located in list 2
    // slot 0 and are treated like golden frames.

    for (int i = 0; i < VP8_NUM_REF_FRAME_LISTS; i++)
    {
        EntryCount[i]           = ReferenceFrameList[i].EntryCount;
        EntryIndicies[i]        = ReferenceFrameList[i].EntryIndicies[0];
    }

    EntryCount[VP8_NUM_REF_FRAME_LISTS]         = 1;
    EntryIndicies[VP8_NUM_REF_FRAME_LISTS]      = ParsedFrameParameters->DecodeFrameIndex;

    if ((PictureHeader->refresh_last_frame) || (PictureHeader->ptype == VP8_PICTURE_CODING_TYPE_I))
    {
        ReferenceFrameList[0].EntryIndicies[0]                  = ParsedFrameParameters->DecodeFrameIndex;      // insert into list 0 slot 0 as 1 as a reference frame
        ReferenceFrameList[0].EntryCount                        = 1;
    }

    if (PictureHeader->ptype == VP8_PICTURE_CODING_TYPE_I)
    {
        ReferenceFrameList[1].EntryCount                        = 1;
        ReferenceFrameList[2].EntryCount                        = 1;
        ReferenceFrameList[1].EntryIndicies[0]                  = ParsedFrameParameters->DecodeFrameIndex;      // insert into list 1 slot 0 as a golden frame
        ReferenceFrameList[2].EntryIndicies[0]                  = ParsedFrameParameters->DecodeFrameIndex;      // insert into list 2 slot 0 as an alternate reference frame
    }
    else
    {
        if (PictureHeader->refresh_golden_frame == VP8_REFRESH_GOLDEN_FRAME)
        {
            ReferenceFrameList[1].EntryIndicies[0]              = ParsedFrameParameters->DecodeFrameIndex;      // insert into list 1 slot 0 as a golden frame
            ReferenceFrameList[1].EntryCount                    = 1;
        }
        else
        {
            switch (PictureHeader->copy_buffer_to_golden)
            {
            case VP8_LAST_FRAME_TO_GOLDEN:
                ReferenceFrameList[1].EntryCount            = 1;
                ReferenceFrameList[1].EntryIndicies[0]      = EntryIndicies[0];
                break;

            case VP8_ALTERNATE_FRAME_TO_GOLDEN:
                ReferenceFrameList[1].EntryCount            = 1;
                ReferenceFrameList[1].EntryIndicies[0]      = EntryIndicies[2];
                break;
            }
        }

        if (PictureHeader->refresh_alternate_frame == VP8_REFRESH_ALTERNATE_FRAME)
        {
            ReferenceFrameList[2].EntryIndicies[0]              = ParsedFrameParameters->DecodeFrameIndex;      // insert into list 1 slot 0 as a golden frame
            ReferenceFrameList[2].EntryCount                    = 1;
        }
        else
        {
            switch (PictureHeader->copy_buffer_to_alternate)
            {
            case VP8_LAST_FRAME_TO_ALTERNATE:
                ReferenceFrameList[2].EntryCount            = 1;
                ReferenceFrameList[2].EntryIndicies[0]      = EntryIndicies[0];
                break;

            case VP8_GOLDEN_FRAME_TO_ALTERNATE:
                ReferenceFrameList[2].EntryCount            = 1;
                ReferenceFrameList[2].EntryIndicies[0]      = EntryIndicies[1];
                break;
            }
        }
    }

    for (int i = 0; i < VP8_NUM_REF_FRAME_LISTS + 1; i++)
    {
        if (EntryCount[i] != 0)
        {
            bool        InUse   = false;

            for (int j = 0; j < VP8_NUM_REF_FRAME_LISTS; j++)
            {
                if ((ReferenceFrameList[j].EntryCount != 0) && (EntryIndicies[i] == ReferenceFrameList[j].EntryIndicies[0]))
                {
                    InUse   = true;
                    break;
                }
            }

            if (!InUse)
            {
                SE_DEBUG(group_frameparser_video, "Releasing %d\n", EntryIndicies[i]);
                Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE,
                                                          CodecFnReleaseReferenceFrame, EntryIndicies[i]);
            }
        }
    }

    return FrameParserNoError;
}
//}}}

//{{{  RevPlayProcessDecodeStacks
// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific override function for processing decode stacks, this
//      initializes the post decode ring before passing itno the real
//      implementation of this function.
//

FrameParserStatus_t   FrameParser_VideoVp8_c::RevPlayProcessDecodeStacks(void)
{
    ReverseQueuedPostDecodeSettingsRing->Flush();
    return FrameParser_Video_c::RevPlayProcessDecodeStacks();
}
//}}}

//{{{  CommitFrameForDecode
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Send frame for decode
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp8_c::CommitFrameForDecode(void)
{
    Vp8VideoPicture_t                  *PictureHeader;
    Vp8VideoSequence_t                 *SequenceHeader;
    FrameParserStatus_t                 Status = FrameParserNoError;

    //
    // Check we have the headers we need
    //
    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Stream parameters unavailable for decode\n");
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        SE_ERROR("Frame parameters unavailable for decode (%p)\n", FrameParameters);
        return FrameParserPartialFrameParameters;
    }

    if (Buffer == NULL)
    {
        // Basic check: before attach stream/frame param to Buffer
        SE_ERROR("No current buffer to commit to decode\n");
        return FrameParserError;
    }

    SequenceHeader                      = &StreamParameters->SequenceHeader;
    PictureHeader                       = &FrameParameters->PictureHeader;
    // Record the stream and frame parameters into the appropriate structure
    // Parsed frame parameters
    ParsedFrameParameters->FirstParsedParametersForOutputFrame  = true;  //FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump  = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                  = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                = ContinuousReverseJump;
    ParsedFrameParameters->KeyFrame                             = PictureHeader->ptype == VP8_PICTURE_CODING_TYPE_I;
    ParsedFrameParameters->ReferenceFrame                       = true;
    ParsedFrameParameters->IndependentFrame                     = ParsedFrameParameters->KeyFrame;
    ParsedFrameParameters->NumberOfReferenceFrameLists          = 1;
    ParsedFrameParameters->NewStreamParameters                  = NewStreamParametersCheck();
    ParsedFrameParameters->SizeofStreamParameterStructure       = sizeof(Vp8StreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure             = StreamParameters;
    ParsedFrameParameters->NewFrameParameters                   = true;
    ParsedFrameParameters->SizeofFrameParameterStructure        = sizeof(Vp8FrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure              = FrameParameters;
    // Parsed video parameters
    ParsedVideoParameters->Content.Width                        = SequenceHeader->encoded_width;
    ParsedVideoParameters->Content.Height                       = SequenceHeader->encoded_height;
    ParsedVideoParameters->Content.DisplayWidth                 = SequenceHeader->encoded_width;
    ParsedVideoParameters->Content.DisplayHeight                = SequenceHeader->encoded_height;
    Status = CheckForResolutionConstraints(ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);

    if (Status != FrameParserNoError)
    {
        SE_ERROR("Unsupported resolution %d x %d\n", ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);
        return Status;
    }

    ParsedVideoParameters->Content.Progressive                  = true;
    ParsedVideoParameters->Content.OverscanAppropriate          = false;
    ParsedVideoParameters->Content.VideoFullRange               = false;                                // VP8 conforms to ITU_R_BT601 for source
    ParsedVideoParameters->Content.ColourMatrixCoefficients     = MatrixCoefficients_ITU_R_BT601;
    // Frame rate defaults to 23.976 if metadata not valid
    StreamEncodedFrameRate                  = FrameRate;
    ParsedVideoParameters->Content.FrameRate                    = ResolveFrameRate();
    ParsedVideoParameters->DisplayCount[0]                      = 1;
    ParsedVideoParameters->DisplayCount[1]                      = 0;
    ParsedVideoParameters->Content.PixelAspectRatio             = 1;
    ParsedVideoParameters->PictureStructure                     = StructureFrame;
    ParsedVideoParameters->InterlacedFrame                      = false;
    ParsedVideoParameters->TopFieldFirst                        = true;
    ParsedVideoParameters->PanScanCount                         = 0;
    // Record our claim on both the frame and stream parameters
    Buffer->AttachBuffer(StreamParametersBuffer);
    Buffer->AttachBuffer(FrameParametersBuffer);
    // We clear the FrameParameters pointer, a new one will be obtained
    // before/if we read in headers pertaining to the next frame. This
    // will generate an error should I accidentally write code that
    // accesses this data when it should not.
    FrameParameters                                             = NULL;
    // Finally set the appropriate flag and return
    FrameToDecode                                               = true;
    return FrameParserNoError;
}
//}}}
//{{{  NewStreamParametersCheck
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Boolean function to evaluate whether or not the stream
///             parameters are new.
///
/// /////////////////////////////////////////////////////////////////////////
bool   FrameParser_VideoVp8_c::NewStreamParametersCheck(void)
{
    bool            Different;

    //
    // The parameters cannot be new if they have been used before.
    //

    if (!StreamParameters->UpdatedSinceLastFrame)
    {
        return false;
    }

    StreamParameters->UpdatedSinceLastFrame     = false;
    //
    // Check for difference using a straightforward comparison to see if the
    // stream parameters have changed. (since we zero on allocation simple
    // memcmp should be sufficient).
    //
    Different   = memcmp(&CopyOfStreamParameters, StreamParameters, sizeof(Vp8StreamParameters_t)) != 0;

    if (Different)
    {
        memcpy(&CopyOfStreamParameters, StreamParameters, sizeof(Vp8StreamParameters_t));
        return true;
    }

//
    return false;
}
//}}}
void   FrameParser_VideoVp8_c::CalculateFrameIndexAndPts(
    ParsedFrameParameters_t         *ParsedFrame,
    ParsedVideoParameters_t         *ParsedVideo)
{
    Vp8FrameParameters_t  *ParsedPicture  = (Vp8FrameParameters_t *) ParsedFrame->FrameParameterStructure;
    FrameParser_Video_c::CalculateFrameIndexAndPts(ParsedFrame, ParsedVideo);

    if (!ParsedPicture->PictureHeader.show_frame) /*Alternate frames are used for inter-prediction only, we don't show them*/
    {
        //SE_ERROR("Frame %d should be discarded\n",ParsedFrame->DisplayFrameIndex);
        ParsedFrame->Discard_picture = true;
    }
    else
    {
        ParsedFrame->Discard_picture = false;
    }
}

