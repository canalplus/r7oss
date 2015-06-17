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

#include "frame_parser_video_divx.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoDivx_c"
//#define DUMP_HEADERS 1

static BufferDataDescriptor_t  DivxStreamParametersBuffer = BUFFER_MPEG4_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t  DivxFrameParametersBuffer = BUFFER_MPEG4_FRAME_PARAMETERS_TYPE;

#ifdef DUMP_HEADERS
static int parsedCount = 0;
static int inputCount = 0;
static int vopCount = 0;
#endif

static QuantiserMatrix_t ZigZagScan =
{
    0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

static QuantiserMatrix_t DefaultIntraQuantizationMatrix =
{
    8, 17, 18, 19, 21, 23, 25, 27,
    17, 18, 19, 21, 23, 25, 27, 28,
    20, 21, 22, 23, 24, 26, 28, 30,
    21, 22, 23, 24, 26, 28, 30, 32,
    22, 23, 24, 26, 28, 30, 32, 35,
    23, 24, 26, 28, 30, 32, 35, 38,
    25, 26, 28, 30, 32, 35, 38, 41,
    27, 28, 30, 32, 35, 38, 41, 45
};

static QuantiserMatrix_t DefaultNonIntraQuantizationMatrix =
{
    16, 17, 18, 19, 20, 21, 22, 23,
    17, 18, 19, 20, 21, 22, 23, 24,
    18, 19, 20, 21, 22, 23, 24, 25,
    19, 20, 21, 22, 23, 24, 26, 27,
    20, 21, 22, 23, 25, 26, 27, 28,
    21, 22, 23, 24, 26, 27, 28, 30,
    22, 23, 24, 26, 27, 28, 30, 31,
    23, 24, 25, 27, 28, 30, 31, 33
};

#define PAR_SQUARE                      0x01                    /* 1:1 */
#define PAR_4_3_PAL                     0x02                    /* 12:11 */
#define PAR_4_3_NTSC                    0x03                    /* 10:11 */
#define PAR_16_9_PAL                    0x04                    /* 16:11 */
#define PAR_16_9_NTSC                   0x05                    /* 40:33 */

// BEWARE !!!! you cannot declare static initializers of a constructed type such as Rational_t
//             the compiler will silently ignore them..........
static unsigned int     DivxAspectRatioValues[][2]     =
{
    {1, 1},
    {1, 1},
    {12, 11},
    {10, 11 },
    {16, 11 },
    {40, 33 }
};

#define DivxAspectRatios(N) Rational_t(DivxAspectRatioValues[N][0],DivxAspectRatioValues[N][1])

static SliceType_t SliceTypeTranslation[]  = { SliceTypeI, SliceTypeP, SliceTypeB };

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//
FrameParser_VideoDivx_c::FrameParser_VideoDivx_c(void)
    : FrameParser_Video_c()
    , DivXVersion(100)
    , FrameParameters(NULL)
    , StreamParameters(NULL)
    , DroppedFrame(false)
    , StreamParametersSet(false)
    , SentStreamParameter(false)
    , TimeIncrementBits(0)
    , QuantPrecision(0)
    , Interlaced(0)
    , CurrentMicroSecondsPerFrame(0)
    , LastPredictionType(0)
    , LastTimeIncrement(0)
    , bit_skip_no(0)
    , old_time_base(0)
    , prev_time_base(0)
    , TimeIncrementResolution(1)
    , LastVop()
{
    Configuration.FrameParserName               = "VideoDivx";
    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &DivxStreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &DivxFrameParametersBuffer;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

FrameParser_VideoDivx_c::~FrameParser_VideoDivx_c(void)
{
#ifdef DUMP_HEADERS
    SE_ERROR("FP got %d Headers\n", inputCount);
    SE_ERROR("Parsed %d Frames\n", parsedCount);
    SE_ERROR("Parsed %d VOP Headers\n", vopCount);
#endif
    Halt();

    delete StreamParameters;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Method to connect to neighbor
//

FrameParserStatus_t   FrameParser_VideoDivx_c::Connect(Port_c *Port)
{
    // Clear our parameter pointers
    //
    StreamParameters                    = NULL;
    FrameParameters                     = NULL;
    DeferredParsedFrameParameters       = NULL;
    DeferredParsedVideoParameters       = NULL;
    FrameParserStatus_t Status = FrameParser_Video_c::Connect(Port);
    return Status;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Deal with decode of a single frame in forward play
//

FrameParserStatus_t   FrameParser_VideoDivx_c::ForPlayProcessFrame(void)
{
    return FrameParser_Video_c::ForPlayProcessFrame();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Deal with a single frame in reverse play
//
/*
FrameParserStatus_t   FrameParser_VideoDivx_c::RevPlayProcessFrame(      void )
{
    return FrameParser_Video_c::RevPlayProcessFrame();
}
*/

// /////////////////////////////////////////////////////////////////////////
//
//      Addition to the base queue a buffer for decode increments
//      the field index.
//

FrameParserStatus_t   FrameParser_VideoDivx_c::ForPlayQueueFrameForDecode(void)
{
    return  FrameParser_Video_c::ForPlayQueueFrameForDecode();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Specific reverse play implementation of queue for decode
//      the field index.
//
/*
FrameParserStatus_t   FrameParser_VideoDivx_c::RevPlayQueueFrameForDecode( void )
{
    return FrameParser_Video_c::RevPlayQueueFrameForDecode();
}
*/

// /////////////////////////////////////////////////////////////////////////
//
//      This function is responsible for walking the stacks to handle
//      reverse decode.
//
/*
FrameParserStatus_t   FrameParser_VideoDivx_c::RevPlayProcessDecodeStacks(              void )
{
    ReverseQueuedPostDecodeSettingsRing->Flush();
    return FrameParser_Video_c::RevPlayProcessDecodeStacks();
}
*/

// /////////////////////////////////////////////////////////////////////////
//
//      This function is responsible for walking the stacks to discard
//      everything on them when we abandon reverse decode.
//
/*
FrameParserStatus_t   FrameParser_VideoDivx_c::RevPlayPurgeDecodeStacks(                void )
{
    return FrameParser_Video_c::RevPlayPurgeDecodeStacks();
}
*/
FrameParserStatus_t   FrameParser_VideoDivx_c::ReadHeaders(void)
{
    unsigned int  Code;
    ParsedFrameParameters->NewStreamParameters = false;
    ParsedFrameParameters->NewFrameParameters = false;
    ParsedFrameParameters->DataOffset = 0xafff0000;
    /*
        SE_INFO(group_frameparser_video,  "Start Code List ");
        for (unsigned int j = 0 ; j < StartCodeList->NumberOfStartCodes; ++j)
            SE_INFO(group_frameparser_video, "%x ",ExtractStartCodeCode(StartCodeList->StartCodes[j]));
        SE_INFO(group_frameparser_video, "\n");
    */
#ifdef DUMP_HEADERS
    ++inputCount;
#endif

    //      SE_ERROR( "%d start codes found\n",StartCodeList->NumberOfStartCodes);
    for (unsigned int i = 0; i < StartCodeList->NumberOfStartCodes; ++i)
    {
        Code = ExtractStartCodeCode(StartCodeList->StartCodes[i]);
        //SE_INFO(group_frameparser_video,  "Processing code: %x (%d of %d)\n",Code,i+1, StartCodeList->NumberOfStartCodes);
        Bits.SetPointer(BufferData + ExtractStartCodeOffset(StartCodeList->StartCodes[i]) + 4);

        if (Code == 0x31)
        {
            // Magic to pass version number around
            DivXVersion = Bits.Get(8);

            switch (DivXVersion)
            {
            case 3:
                DivXVersion = 311;
                break;

            case 4:
                DivXVersion = 412;
                break;

            case 5:
                DivXVersion = 500;
                break;

            default:
                DivXVersion = 100;
            }

            //SE_INFO(group_frameparser_video,  "DivX Version Number %d\n",DivXVersion);
        }
        else if (Code == DROPPED_FRAME_CODE)
        {
            // magic to work around avi's with skipped 0 byte frames inside them
            DroppedFrame = true;
            SE_INFO(group_frameparser_video, "Seen AVI Dropped Frame\n");
        }
        else if ((Code & VOL_START_CODE_MASK) == VOL_START_CODE)
        {
            FrameParserStatus_t     Status = FrameParserNoError;

            // SE_ERROR("%x VOL_START_CODE\n",VOL_START_CODE);
            if (StreamParameters == NULL)
            {
                StreamParameters = new Mpeg4VideoStreamParameters_t;
                ParsedFrameParameters->StreamParameterStructure = StreamParameters;
                ParsedFrameParameters->SizeofStreamParameterStructure = sizeof(Mpeg4VideoStreamParameters_t);
                ParsedFrameParameters->NewStreamParameters = true;
            }

            Status  = ReadVolHeader(&StreamParameters->VolHeader);

            if (Status != FrameParserNoError)
            {
                ParsedFrameParameters->NewStreamParameters = false;
                StreamParametersSet = false;
                return Status;
            }

            // take some state which affects the decoding of VOP headers
            QuantPrecision = StreamParameters->VolHeader.quant_precision;
            Interlaced = StreamParameters->VolHeader.interlaced;
            StreamParameters->MicroSecondsPerFrame = CurrentMicroSecondsPerFrame;
            StreamParametersSet = true;
            //SE_ERROR("Stream is %s\n",Interlaced?"Interlaced":"Progressive");

            if (DivXVersion != 311)
            {
                ParsedFrameParameters->DataOffset =  ExtractStartCodeOffset(StartCodeList->StartCodes[i]);
            }
        }
        else if ((Code & VOP_START_CODE_MASK) == VOP_START_CODE)
        {
            // SE_ERROR("%x VOP_START_CODE\n",VOP_START_CODE);
            // NOTE the vop reading depends on a valid Vol having been acquired
            if (StreamParametersSet)
            {
                Mpeg4VopHeader_t Vop;
                FrameParserStatus_t     Status = FrameParserNoError;

                //                              SE_ERROR("VOP Addr %08llx\n",(BufferData + ExtractStartCodeOffset(StartCodeList->StartCodes[i]) +4));
                //
                if (DivXVersion != 311 && (ParsedFrameParameters->DataOffset == 0xafff0000))
                {
                    ParsedFrameParameters->DataOffset =  ExtractStartCodeOffset(StartCodeList->StartCodes[i]);
                }

                Status  = ReadVopHeader(&Vop);

                if (Status != FrameParserNoError)
                {
                    return Status;
                }

                if (FrameParameters == NULL)
                {
                    FrameParserStatus_t status  = GetNewFrameParameters((void **)&FrameParameters);

                    if (status != FrameParserNoError)
                    {
                        SE_ERROR("Failed to get new FrameParameters\n");
                        return status;
                    }
                }

                FrameParameters->VopHeader = Vop;
                FrameParameters->bit_skip_no = bit_skip_no;
                ParsedFrameParameters->FrameParameterStructure = FrameParameters;
                ParsedFrameParameters->SizeofFrameParameterStructure = sizeof(Mpeg4VideoFrameParameters_t);
                ParsedFrameParameters->NewFrameParameters = true;
                Buffer->AttachBuffer(FrameParametersBuffer);
#ifdef DUMP_HEADERS
                ++vopCount;
#endif

                if (DivXVersion == 311)
                {
                    unsigned char *ptr;
                    unsigned int bits;
                    Bits.GetPosition(&ptr, &bits);
                    unsigned int position = ptr - BufferData;

                    if (bits) { position++; }

                    ParsedFrameParameters->DataOffset = position;
                }

                CommitFrameForDecode();
            }
            else
            {
                SE_ERROR("Frame without Stream Parameters\n");
                //                              Code = INVALID_START_CODE;
            }
        }
        else if (Code == VSOS_START_CODE)
        {
            // SE_ERROR("%x VSOS_START_CODE\n",VSOS_START_CODE);
            //
            // NOTE this could be my AVI header added to send the MS per frame from the
            // AVI file
            // some avi files have real ones which upset things.... if it returns back 0
            // which is from an unknown profile then use the last one, otherwise pick 25fps
            unsigned int cnpf =  ReadVosHeader();

            if (cnpf != 0)
            {
                CurrentMicroSecondsPerFrame = cnpf;
            }

            // sanitize the frame rate (4..120)
            if ((CurrentMicroSecondsPerFrame == 0) || !inrange(Rational_t(1000000, CurrentMicroSecondsPerFrame), 4, 120))
            {
                SE_WARNING("Current ms per frame not valid (%d) defaulting to 25fps\n",
                           CurrentMicroSecondsPerFrame);
                CurrentMicroSecondsPerFrame = 40000;
            }

            //SE_ERROR("CurrentMicroSecondsPerFrame %d\n",CurrentMicroSecondsPerFrame);
        }
        else if ((Code & VO_START_CODE_MASK) == VO_START_CODE)
        {
            SE_INFO(group_frameparser_video, "%x VO_START_CODE\n", VO_START_CODE);
            ReadVoHeader();
        }

        /*
                        else if( (Code & USER_DATA_START_CODE_MASK) == USER_DATA_START_CODE )
                        {
                                SE_INFO(group_frameparser_video, "%x USER_DATA_START_CODE\n",USER_DATA_START_CODE);
                        } // No action
                        else if( Code == VSOS_END_CODE )
                        {
                              SE_INFO(group_frameparser_video, "%x VSOS_END_CODE\n",VSOS_END_CODE);
                        } // No action
                        else if( Code == VSO_START_CODE )
                        {
                                SE_INFO(group_frameparser_video, "%x VSO_START_CODE\n",VSO_START_CODE);
                        } // No action
                        else if( Code == GOV_START_CODE )
                        {
                                SE_INFO(group_frameparser_video, "%x GOV_START_CODE\n",GOV_START_CODE);
                        } // No action
                        else if( Code == END_OF_START_CODE_LIST )
                        {
                                SE_ERROR("%x END_OF_START_CODE_LIST\n",END_OF_START_CODE_LIST);
                        } // No action
                        else
                        {
                                SE_ERROR( "ReadHeaders - Unknown/Unsupported header 0x%02x\n", Code );
                        }
        */
    }

    return FrameParserNoError;
}

FrameParserStatus_t   FrameParser_VideoDivx_c::CommitFrameForDecode(void)
{
    FrameParserStatus_t Status = FrameParserNoError;
    SliceType_t      SliceType;
    Mpeg4VideoFrameParameters_t *VFP = (Mpeg4VideoFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure);
    Mpeg4VopHeader_t  *Vop = &(VFP->VopHeader);

    if (Buffer == NULL)
    {
        // Basic check: before attach stream/frame param to Buffer
        SE_ERROR("No current buffer to commit to decode\n");
        return FrameParserError;
    }

#ifdef DUMP_HEADERS
    ++parsedCount;
#endif

    if (Vop->divx_nvop || (StreamParameters == NULL))
    {
        if (Vop->vop_coded)
        {
            SE_ERROR("can't be divx_nvop and vop_coded! (%p)\n", StreamParameters);
        }

        if (DivXVersion == 311)
        {
            SE_ERROR("can't be divx_nvop in a faked DivX3.11 frame?\n");
            Stream->MarkUnPlayable();
        }

        return FrameParserError;
    }

    SliceType       = SliceTypeTranslation[Vop->prediction_type];
    ParsedFrameParameters->SizeofStreamParameterStructure = sizeof(Mpeg4VideoStreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure = StreamParameters;

    if (StreamParameters->VolHeader.aspect_ratio_info <= PAR_16_9_NTSC)
        ParsedVideoParameters->Content.PixelAspectRatio =
            DivxAspectRatios(StreamParameters->VolHeader.aspect_ratio_info);
    else if (StreamParameters->VolHeader.aspect_ratio_info == PAR_EXTENDED)
    {
        if ((0 == StreamParameters->VolHeader.par_width) || (0 == StreamParameters->VolHeader.par_height))
        {
            SE_INFO(group_frameparser_video, "invalid w.h %d:%d; forcing 1:1\n",
                    StreamParameters->VolHeader.par_width, StreamParameters->VolHeader.par_height);
            StreamParameters->VolHeader.par_width  = 1;
            StreamParameters->VolHeader.par_height = 1;
        }

        ParsedVideoParameters->Content.PixelAspectRatio =
            Rational_t(StreamParameters->VolHeader.par_width, StreamParameters->VolHeader.par_height);
    }
    else
        ParsedVideoParameters->Content.PixelAspectRatio =
            DivxAspectRatios(PAR_SQUARE);

    ParsedVideoParameters->Content.Width = StreamParameters->VolHeader.width;
    ParsedVideoParameters->Content.Height = StreamParameters->VolHeader.height;
    ParsedVideoParameters->Content.DisplayWidth = StreamParameters->VolHeader.width;
    ParsedVideoParameters->Content.DisplayHeight = StreamParameters->VolHeader.height;
    Status = CheckForResolutionConstraints(ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);

    if (Status != FrameParserNoError)
    {
        SE_ERROR("Unsupported resolution %d x %d\n", ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);
        return Status;
    }

    ParsedVideoParameters->Content.Progressive = !Interlaced;

    if (0 == StreamParameters->MicroSecondsPerFrame)
    {
        StreamEncodedFrameRate = INVALID_FRAMERATE;
    }
    else
    {
        StreamEncodedFrameRate = Rational_t(1000000, StreamParameters->MicroSecondsPerFrame);
    }

    ParsedVideoParameters->Content.FrameRate = ResolveFrameRate();
    ParsedVideoParameters->Content.OverscanAppropriate = 0;
    ParsedVideoParameters->InterlacedFrame = Interlaced;
    ParsedFrameParameters->FirstParsedParametersForOutputFrame = true;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump = ContinuousReverseJump;
    ParsedFrameParameters->KeyFrame     = SliceType == SliceTypeI;
    ParsedFrameParameters->ReferenceFrame   = SliceType != SliceTypeB;
    ParsedFrameParameters->IndependentFrame = ParsedFrameParameters->KeyFrame;
    ParsedFrameParameters->NumberOfReferenceFrameLists = 1;
    ParsedVideoParameters->DisplayCount[0] = 1 + (DroppedFrame * 1);

    if (Interlaced)
    {
        ParsedVideoParameters->DisplayCount[1] = 1 + (DroppedFrame * 1);
    }
    else
    {
        ParsedVideoParameters->DisplayCount[1] = 0;
    }

    ParsedVideoParameters->SliceType = SliceType;
    ParsedVideoParameters->TopFieldFirst = Vop->top_field_first;
    ParsedVideoParameters->PictureStructure = StructureFrame;
    ParsedVideoParameters->PanScanCount                 = 1;
    ParsedVideoParameters->PanScan[0].DisplayCount      = ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1];
    ParsedVideoParameters->PanScan[0].Width             = ParsedVideoParameters->Content.DisplayWidth;
    ParsedVideoParameters->PanScan[0].Height            = ParsedVideoParameters->Content.DisplayHeight;
    ParsedVideoParameters->PanScan[0].HorizontalOffset  = 0;
    ParsedVideoParameters->PanScan[0].VerticalOffset    = 0;
    FirstDecodeOfFrame = true;
    FrameToDecode = true;
    FrameParameters = NULL;
    DroppedFrame = false;
    return FrameParserNoError;
}

FrameParserStatus_t  FrameParser_VideoDivx_c::ReadVoHeader(void)
{
    unsigned int IsVisualObjectIdentifier;
    unsigned int VisualObjectType;
    unsigned int VideoSignalType;
    unsigned int ColourPrimaries;
    unsigned int TransferCharacteristics;
    unsigned int MatrixCoefficients;
    IsVisualObjectIdentifier = Bits.Get(1);

    if (IsVisualObjectIdentifier)
    {
        Bits.Get(4); // VisualObjectVerId
        Bits.Get(3); // VisualObjectPriority
    }
    else
    {
        // VisualObjectVerId = 1;
    }

    VisualObjectType = Bits.Get(4);

    if (VisualObjectType == 1) // Video ID
    {
        VideoSignalType = Bits.Get(1);

        if (VideoSignalType)
        {
            Bits.Get(3); // VideoFormat
            Bits.Get(1); // VideoRange

            unsigned char ColourDescription = Bits.Get(1);
            if (ColourDescription)
            {
                ColourPrimaries = Bits.Get(8);
                TransferCharacteristics = Bits.Get(8);
                MatrixCoefficients = Bits.Get(8);
            }
            else
            {
                ColourPrimaries = 1;   /*COLOUR_PRIMARIES_ITU_R_BT_709 */
                TransferCharacteristics = 1;   /*TRANSFER_ITU_R_BT_709 */
                MatrixCoefficients = 1;        /*MATRIX_COEFFICIENTS_ITU_R_BT_709 */
            }

            switch (MatrixCoefficients)
            {
            case MPEG4P2_MATRIX_COEFFICIENTS_ITU_R_BT_709 :
                MatrixCoefficients = MatrixCoefficients_ITU_R_BT709; break;

            case MPEG4P2_MATRIX_COEFFICIENTS_FCC :
                MatrixCoefficients = MatrixCoefficients_FCC; break;

            case MPEG4P2_MATRIX_COEFFICIENTS_ITU_R_BT_470_2_BG :
                MatrixCoefficients = MatrixCoefficients_ITU_R_BT470_2_BG; break;

            case MPEG4P2_MATRIX_COEFFICIENTS_SMPTE_170M :
                MatrixCoefficients = MatrixCoefficients_SMPTE_170M; break;

            case MPEG4P2_MATRIX_COEFFICIENTS_SMPTE_240M :
                MatrixCoefficients = MatrixCoefficients_SMPTE_240M; break;

            default:
            case MPEG4P2_MATRIX_COEFFICIENTS_UNSPECIFIED :
                MatrixCoefficients = MatrixCoefficients_Undefined; break;
            }

            ParsedVideoParameters->Content.ColourMatrixCoefficients = MatrixCoefficients;
            ParsedVideoParameters->Content.VideoFullRange = false; // not used by DivX
            SE_INFO(group_frameparser_video,  "Colour Primaries %d\n", ColourPrimaries);
            SE_INFO(group_frameparser_video,  "Transfer Characteristics %d\n", TransferCharacteristics);
            SE_INFO(group_frameparser_video,  "Matrix Coefficients %d\n", MatrixCoefficients);
        }
    }

    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in the vol header.
//

FrameParserStatus_t  FrameParser_VideoDivx_c::ReadVolHeader(Mpeg4VolHeader_t       *Vol)
{
    int i;
    memset(Vol, 0, sizeof(Mpeg4VolHeader_t));
    //
    Vol->random_accessible_vol                  = Bits.Get(1);
    Vol->type_indication                        = Bits.Get(8);
    Vol->is_object_layer_identifier             = Bits.Get(1);

    if (Vol->is_object_layer_identifier)
    {
        Vol->visual_object_layer_verid          = Bits.Get(4);
        Vol->visual_object_layer_priority       = Bits.Get(3);
    }
    else
    {
        Vol->visual_object_layer_verid          = 1;
        Vol->visual_object_layer_priority       = 1;
    }

    Vol->aspect_ratio_info                      = Bits.Get(4);

    if (Vol->aspect_ratio_info == PAR_EXTENDED)
    {
        Vol->par_width                          = Bits.Get(8);
        Vol->par_height                         = Bits.Get(8);
    }

    Vol->vol_control_parameters                 = Bits.Get(1);

    if (Vol->vol_control_parameters)
    {
        Vol->chroma_format                      = Bits.Get(2);
        Vol->low_delay                          = Bits.Get(1);
        Vol->vbv_parameters                     = Bits.Get(1);

        if (Vol->vbv_parameters)
        {
            Vol->first_half_bit_rate            = Bits.Get(15);
            Bits.Get(1);
            Vol->latter_half_bit_rate           = Bits.Get(15);
            Bits.Get(1);
            Vol->first_half_vbv_buffer_size     = Bits.Get(15);
            Bits.Get(1);
            Vol->latter_half_vbv_buffer_size    = Bits.Get(3);
            Vol->first_half_vbv_occupancy       = Bits.Get(11);
            Bits.Get(1);
            Vol->latter_half_vbv_occupancy      = Bits.Get(15);
            Bits.Get(1);
        }
    }

    Vol->shape                                  = Bits.Get(2);

    if (Vol->shape != SHAPE_RECTANGULAR)
    {
        SE_ERROR("VolHeader shape other than RECTANGULAR not supported\n");
        Stream->MarkUnPlayable();
        return FrameParserError;
    }

    // The following code is adjusted to only cater for shape == RECTANGULAR
    //
    Bits.Get(1);
    Vol->time_increment_resolution              = Bits.Get(16);
    TimeIncrementBits                           = INCREMENT_BITS(Vol->time_increment_resolution);
    TimeIncrementResolution                     = Vol->time_increment_resolution;
    Bits.Get(1);
    Vol->fixed_vop_rate                         = Bits.Get(1);

    if (Vol->fixed_vop_rate)
    {
        Vol->fixed_vop_time_increment           = Bits.Get(TimeIncrementBits);
    }

    Bits.Get(1);
    Vol->width                                  = Bits.Get(13);
    Bits.Get(1);
    Vol->height                                 = Bits.Get(13);
    Bits.Get(1);
    Vol->version = DivXVersion;
    Vol->interlaced                             = Bits.Get(1);
    Vol->obmc_disable                           = Bits.Get(1);

    if (Vol->visual_object_layer_verid == 1)
    {
        Vol->sprite_usage                       = Bits.Get(1);
    }
    else
    {
        Vol->sprite_usage                       = Bits.Get(2);
    }

    if (Vol->sprite_usage != SPRITE_NOT_USED)
    {
        SE_ERROR("VolHeader sprite_usage other than NOT_USED not supported\n");
        Stream->MarkUnPlayable();
        return FrameParserError;
        /* TODO: cleanup following comments
                        // code to support GMC assuming firmware is compiled with GMC support.
                        //
                        if(Vol->sprite_usage != GMC_SPRITE)
                        {

                                Bits.Get(13 + 1 + 13 + 1);
                                // sprite_width (13 bits)
                                // marker (1 bit)
                                // sprite_height (13 bits)
                                // marker (1 bit)
                                Bits.Get(13 + 1 + 13 + 1);
                                // sprite_left_coordinate (13 bits)
                                // marker (1 bit)
                                // sprite_top_coordinate (13 bits)
                                // marker (1 bit)
                        }

                        Vol->no_of_sprite_warping_points = Bits.Get(6);
                        Vol->sprite_warping_accuracy = Bits.Get(2);
                        // 00: halfpel
                        // 01: 1/4 pel
                        // 10: 1/8 pel
                        // 11: 1/16 pel
                        //
                        Vol->sprite_brightness_change=Bits.Get(1);
                        //       assert(!Vol->sprite_brightness_change);
                        if(Vol->sprite_usage != GMC_SPRITE)
                        {
                                //           int low_latency_sprite_enable=
                                Bits.Get(1);
                        }
        */
    }

    // The following code is adjusted to only cater for sprite_usage == SPRITE_NOT_USED
    //
    Vol->not_8_bit                              = Bits.Get(1);

    if (Vol->not_8_bit)
    {
        Vol->quant_precision                    = Bits.Get(4);
        Vol->bits_per_pixel                     = Bits.Get(4);
    }
    else
    {
        Vol->quant_precision                    = 5;
        Vol->bits_per_pixel                     = 8;
    }

    Vol->quant_type                             = Bits.Get(1);

    if (Vol->quant_type)
    {
        Vol->load_intra_quant_matrix            = Bits.Get(1);

        if (Vol->load_intra_quant_matrix)
        {
            for (i = 0; i < QUANTISER_MATRIX_SIZE; i++)
            {
                Vol->intra_quant_matrix[ZigZagScan[i]] = Bits.Get(8);

                if (Vol->intra_quant_matrix[ZigZagScan[i]] == 0)
                {
                    break;
                }
            }

            for (; i < QUANTISER_MATRIX_SIZE; i++)
            {
                Vol->intra_quant_matrix[ZigZagScan[i]] = Vol->intra_quant_matrix[ZigZagScan[i - 1]];
            }
        }
        else
        {
            memcpy(Vol->intra_quant_matrix, DefaultIntraQuantizationMatrix, QUANTISER_MATRIX_SIZE);
        }

        Vol->load_non_intra_quant_matrix        = Bits.Get(1);

        if (Vol->load_non_intra_quant_matrix)
        {
            for (i = 0; i < QUANTISER_MATRIX_SIZE; i++)
            {
                Vol->non_intra_quant_matrix[ZigZagScan[i]] = Bits.Get(8);

                if (Vol->non_intra_quant_matrix[ZigZagScan[i]] == 0)
                {
                    break;
                }
            }

            for (; i < QUANTISER_MATRIX_SIZE; i++)
            {
                Vol->non_intra_quant_matrix[ZigZagScan[i]] = Vol->non_intra_quant_matrix[ZigZagScan[i - 1]];
            }
        }
        else
        {
            memcpy(Vol->non_intra_quant_matrix, DefaultNonIntraQuantizationMatrix, QUANTISER_MATRIX_SIZE);
        }
    }

    if (Vol->visual_object_layer_verid != 1)
    {
        Vol->quarter_pixel                      = Bits.Get(1);
    }
    else
    {
        Vol->quarter_pixel                      = 0;
    }

    Vol->complexity_estimation_disable          = Bits.Get(1);
    Vol->resync_marker_disable                  = Bits.Get(1);
    Vol->data_partitioning                      = Bits.Get(1);

    if (Vol->data_partitioning)
    {
        Vol->reversible_vlc                     = Bits.Get(1);
    }

    if (Vol->visual_object_layer_verid != 1)
    {
        Vol->intra_acdc_pred_disable            = Bits.Get(1);

        if (Vol->intra_acdc_pred_disable)
        {
            Vol->request_upstream_message_type  = Bits.Get(2);
            Vol->newpred_segment_type           = Bits.Get(1);
        }

        Vol->reduced_resolution_vop_enable      = Bits.Get(1);
    }

    Vol->scalability                            = Bits.Get(1);

    if (Vol->scalability)
    {
        SE_ERROR("VolHeader scalability not supported\n");
        Stream->MarkUnPlayable();
        return FrameParserError;
    }

    //
    if (!Vol->complexity_estimation_disable)
    {
        SE_ERROR("VolHeader complexity_estimation_disable not set\n");
        Stream->MarkUnPlayable();
        return FrameParserError;
    }

    if (Vol->data_partitioning)
    {
        SE_ERROR("VolHeader data_partitioning not supported\n");
        Stream->MarkUnPlayable();
        return FrameParserError;
    }

    //
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Vol header :-\n");
    SE_INFO(group_frameparser_video,  "        random_accessible_vol             %6d\n", Vol->random_accessible_vol);
    SE_INFO(group_frameparser_video,  "        type_indication                   %6d\n", Vol->type_indication);
    SE_INFO(group_frameparser_video,  "        is_object_layer_identifier        %6d\n", Vol->is_object_layer_identifier);
    SE_INFO(group_frameparser_video,  "        visual_object_layer_verid         %6d\n", Vol->visual_object_layer_verid);
    SE_INFO(group_frameparser_video,  "        visual_object_layer_priority      %6d\n", Vol->visual_object_layer_priority);
    SE_INFO(group_frameparser_video,  "        aspect_ratio_info                 %6d\n", Vol->aspect_ratio_info);
    SE_INFO(group_frameparser_video,  "        par_width                         %6d\n", Vol->par_width);
    SE_INFO(group_frameparser_video,  "        par_height                        %6d\n", Vol->par_height);
    SE_INFO(group_frameparser_video,  "        vol_control_parameters            %6d\n", Vol->vol_control_parameters);
    SE_INFO(group_frameparser_video,  "        chroma_format                     %6d\n", Vol->chroma_format);
    SE_INFO(group_frameparser_video,  "        low_delay                         %6d\n", Vol->low_delay);
    SE_INFO(group_frameparser_video,  "        vbv_parameters                    %6d\n", Vol->vbv_parameters);
    SE_INFO(group_frameparser_video,  "        first_half_bit_rate               %6d\n", Vol->first_half_bit_rate);
    SE_INFO(group_frameparser_video,  "        latter_half_bit_rate              %6d\n", Vol->latter_half_bit_rate);
    SE_INFO(group_frameparser_video,  "        first_half_vbv_buffer_size        %6d\n", Vol->first_half_vbv_buffer_size);
    SE_INFO(group_frameparser_video,  "        latter_half_vbv_buffer_size       %6d\n", Vol->latter_half_vbv_buffer_size);
    SE_INFO(group_frameparser_video,  "        first_half_vbv_occupancy          %6d\n", Vol->first_half_vbv_occupancy);
    SE_INFO(group_frameparser_video,  "        latter_half_vbv_occupancy         %6d\n", Vol->latter_half_vbv_occupancy);
    SE_INFO(group_frameparser_video,  "        shape                             %6d\n", Vol->shape);
    SE_INFO(group_frameparser_video,  "        time_increment_resolution         %6d\n", Vol->time_increment_resolution);
    SE_INFO(group_frameparser_video,  "        fixed_vop_rate                    %6d\n", Vol->fixed_vop_rate);
    SE_INFO(group_frameparser_video,  "        fixed_vop_time_increment          %6d\n", Vol->fixed_vop_time_increment);
    SE_INFO(group_frameparser_video,  "        width                             %6d\n", Vol->width);
    SE_INFO(group_frameparser_video,  "        height                            %6d\n", Vol->height);
    SE_INFO(group_frameparser_video,  "        interlaced                        %6d\n", Vol->interlaced);
    SE_INFO(group_frameparser_video,  "        obmc_disable                      %6d\n", Vol->obmc_disable);
    SE_INFO(group_frameparser_video,  "        sprite_usage                      %6d\n", Vol->sprite_usage);
    SE_INFO(group_frameparser_video,  "        not_8_bit                         %6d\n", Vol->not_8_bit);
    SE_INFO(group_frameparser_video,  "        quant_precision                   %6d\n", Vol->quant_precision);
    SE_INFO(group_frameparser_video,  "        bits_per_pixel                    %6d\n", Vol->bits_per_pixel);
    SE_INFO(group_frameparser_video,  "        quant_type                        %6d\n", Vol->quant_type);
    SE_INFO(group_frameparser_video,  "        load_intra_quant_matrix           %6d\n", Vol->load_intra_quant_matrix);
    SE_INFO(group_frameparser_video,  "        load_non_intra_quant_matrix       %6d\n", Vol->load_non_intra_quant_matrix);
    SE_INFO(group_frameparser_video,  "        quarter_pixel                     %6d\n", Vol->quarter_pixel);
    SE_INFO(group_frameparser_video,  "        complexity_estimation_disable     %6d\n", Vol->complexity_estimation_disable);
    SE_INFO(group_frameparser_video,  "        data_partitioning                 %6d\n", Vol->data_partitioning);
    SE_INFO(group_frameparser_video,  "        intra_acdc_pred_disable           %6d\n", Vol->intra_acdc_pred_disable);
    SE_INFO(group_frameparser_video,  "        request_upstream_message_type     %6d\n", Vol->request_upstream_message_type);
    SE_INFO(group_frameparser_video,  "        newpred_segment_type              %6d\n", Vol->newpred_segment_type);
    SE_INFO(group_frameparser_video,  "        reduced_resolution_vop_enable     %6d\n", Vol->reduced_resolution_vop_enable);
    SE_INFO(group_frameparser_video,  "        scalability                       %6d\n", Vol->scalability);
#endif
    return FrameParserNoError;
}

static unsigned int divround1(int v1, int v2)
{
    return (v1 + v2 / 2) / v2;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a Vop header
//

FrameParserStatus_t  FrameParser_VideoDivx_c::ReadVopHeader(Mpeg4VopHeader_t         *Vop)
{
    unsigned int    display_time = 0;
    //
    memset(Vop, 0, sizeof(Mpeg4VopHeader_t));
    // Not sure this is true now!
    // NOTE this code assumes that VolHeader        shape           == RECTANGULAR
    //                                              sprite_usage    == SPRITE_NOT_USED
    //
    Vop->prediction_type                        = Bits.Get(2);

    if (Vop->prediction_type != PREDICTION_TYPE_B)
    {
        prev_time_base = old_time_base;
        display_time = old_time_base;
    }
    else
    {
        display_time = prev_time_base;
    }

    while (Bits.Get(1) == 1)
    {
        if (Vop->prediction_type != PREDICTION_TYPE_B)
        {
            Vop->time_base++;
        }

        display_time++;
    }

    old_time_base += Vop->time_base;
    Bits.Get(1);

    /* we enter this case when VOL header is not present, code to estimate no. of bits for time increment resolution */
    if (TimeIncrementBits == 0)
    {
        for (TimeIncrementBits = 1 ; TimeIncrementBits < 16; TimeIncrementBits++)
        {
            if (Vop->prediction_type == PREDICTION_TYPE_P)
            {
                if ((Bits.Show(TimeIncrementBits + 6) & 0x37) == 0x30) { break; }
            }
            else
            {
                if ((Bits.Show(TimeIncrementBits + 5) & 0x1F) == 0x18) { break; }
            }
        }
        SE_DEBUG(group_frameparser_video, "No of bits for time increment resolution : %d\n", TimeIncrementBits);
    }

    Vop->time_inc                               = Bits.Get(TimeIncrementBits);
    Bits.Get(1);
    // trb/trd - display_time calculation
    //
    display_time = display_time * TimeIncrementResolution + Vop->time_inc;

    if (Vop->prediction_type != PREDICTION_TYPE_B)
    {
        LastVop.display_time_prev       = LastVop.display_time_next;
        LastVop.display_time_next       = display_time;

        if (display_time != LastVop.display_time_prev)
        {
            Vop->trd        = LastVop.display_time_next - LastVop.display_time_prev;
            LastVop.trd     = Vop->trd;
        }

        /* to make the code match the firmware test env */
        Vop->trb = LastVop.trb;
        Vop->trb_trd = LastVop.trb_trd;
        Vop->trb_trd_trd = LastVop.trb_trd_trd;
    }
    else
    {
        Vop->trd        = LastVop.trd;
        Vop->trb        = display_time - LastVop.display_time_prev;

        if (Interlaced)
        {
            if (Vop->tframe <= 0)
            {
                Vop->tframe = LastVop.display_time_next - display_time;
            }

            Vop->trbi =
                2 * (divround1(display_time, Vop->tframe) - divround1(LastVop.display_time_prev, Vop->tframe));
            Vop->trdi =
                2 * (divround1(LastVop.display_time_next, Vop->tframe) - divround1(LastVop.display_time_prev, Vop->tframe));
        }

        /* set up the frame values for trb_trd and trb_trd_trd */

        if (LastVop.trd != 0)
        {
            Vop->trb_trd = ((Vop->trb << 16) / LastVop.trd) + 1;
            Vop->trb_trd_trd = (((Vop->trb - LastVop.trd) << 16) / LastVop.trd) - 1;
        }
    }

    LastVop.trb = Vop->trb;
    LastVop.trd = Vop->trd;
    LastVop.trb_trd = Vop->trb_trd;
    LastVop.trb_trd_trd = Vop->trb_trd_trd;
    Vop->vop_coded                              = Bits.Get(1);

    if (Vop->vop_coded)
    {
        if (Vop->prediction_type == PREDICTION_TYPE_P)
        {
            Vop->rounding_type                  = Bits.Get(1);
        }
        else
        {
            Vop->rounding_type                  = 0;
        }

        Vop->intra_dc_vlc_thr                   = Bits.Get(3);

        if (Interlaced)
        {
            Vop->top_field_first                = Bits.Get(1);
            Vop->alternate_vertical_scan_flag   = Bits.Get(1);
#if 0
            SE_INFO(group_frameparser_video, "interlaced info : tff %d, avsf %d\n",
                    Vop->top_field_first,
                    Vop->alternate_vertical_scan_flag);
#endif

            // 25fps is a frame rate of 40ms per frame
            if ((CurrentMicroSecondsPerFrame == 40000) && (Vop->top_field_first == 0))
            {
#if 0
                SE_INFO(group_frameparser_video, "PAL is normally top_field_first.. inverting\n");
#endif
                Vop->top_field_first = 1;
            }
        }
        else
        {
            Vop->top_field_first                = 0;
            Vop->alternate_vertical_scan_flag   = 0;
        }

        Vop->quantizer                          = Bits.Get(QuantPrecision);

        if (Vop->prediction_type != PREDICTION_TYPE_I)
        {
            Vop->fcode_forward                  = Bits.Get(3);
        }

        Vop->fcode_backward = LastVop.fcode_backward; // seems to match what the firmware test app does

        if (Vop->prediction_type == PREDICTION_TYPE_B)
        {
            Vop->fcode_backward                 = Bits.Get(3);
        }

        LastVop.fcode_backward = Vop->fcode_backward; // seems to match what the firmware test app does
    }
    else
    {
        // try to determine a fake DivX NVOP or a real NVOP
        if ((Vop->prediction_type == PREDICTION_TYPE_P) &&
            (LastPredictionType != PREDICTION_TYPE_B) &&
            (LastTimeIncrement == Vop->time_inc))
        {
#if 0
            SE_INFO(group_frameparser_video,  "DivX NVOP detected %d %d %d %d\n",
                    Vop->prediction_type,
                    LastPredictionType,
                    LastTimeIncrement,
                    Vop->time_inc);
#endif
            Vop->divx_nvop = 1;
        }

#if 0
        else
        {
            SE_INFO(group_frameparser_video,  "real NVOP detected %d %d %d %d\n",
                    Vop->prediction_type,
                    LastPredictionType,
                    LastTimeIncrement,
                    Vop->time_inc);
        }

#endif
    }

    if (Vop->prediction_type != PREDICTION_TYPE_B)
    {
        LastPredictionType = Vop->prediction_type;
        LastTimeIncrement = Vop->time_inc;
    }

    //
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Vop header :-\n");
    SE_INFO(group_frameparser_video,  "        prediction_type                   %6d\n", Vop->prediction_type);
    SE_INFO(group_frameparser_video,  "        quantizer                         %6d\n", Vop->quantizer);
    SE_INFO(group_frameparser_video,  "        rounding_type                     %6d\n", Vop->rounding_type);
    SE_INFO(group_frameparser_video,  "        fcode_for                         %6d\n", Vop->fcode_forward);
    SE_INFO(group_frameparser_video,  "        vop_coded                         %6d\n", Vop->vop_coded);
    SE_INFO(group_frameparser_video,  "        use_intra_dc_vlc                  %6d\n", Vop->intra_dc_vlc_thr ? 0 : 1);
    SE_INFO(group_frameparser_video,  "        trbi                              %6d\n", Vop->trbi);
    SE_INFO(group_frameparser_video,  "        trdi                              %6d\n", Vop->trdi);
    SE_INFO(group_frameparser_video,  "        trb_trd                           %6d\n", Vop->trb_trd);
    SE_INFO(group_frameparser_video,  "        trb_trd_trd                       %6d\n", Vop->trb_trd_trd);
    SE_INFO(group_frameparser_video,  "        trd                               %6d\n", Vop->trd);
    SE_INFO(group_frameparser_video,  "        trb                               %6d\n", Vop->trb);
    SE_INFO(group_frameparser_video,  "        intra_dc_vlc_thr                  %6d\n", Vop->intra_dc_vlc_thr);
    SE_INFO(group_frameparser_video,  "        time_inc                          %6d\n", Vop->time_inc);
    SE_INFO(group_frameparser_video,  "        fcode_back                        %6d\n", Vop->fcode_backward);
    SE_INFO(group_frameparser_video,  "        shape_coding_type                 %6d\n", 0);
    SE_INFO(group_frameparser_video,  "        bit_skip_no                       %6d\n", bit_skip_no);
    SE_INFO(group_frameparser_video,  "        time_base                         %6d\n", Vop->time_base);
    SE_INFO(group_frameparser_video,  "        rounding_type                     %6d\n", Vop->rounding_type);
#endif
    /*
        SE_INFO(group_frameparser_video,  "        prediction_type is %d\n", Vop->prediction_type );
        SE_INFO(group_frameparser_video,  "        quantizer is %d\n", Vop->quantizer );
        SE_INFO(group_frameparser_video,  "        rounding_type is %d\n", Vop->rounding_type );
        SE_INFO(group_frameparser_video,  "        fcode_for is %d\n", Vop->fcode_forward );
        SE_INFO(group_frameparser_video,  "        vop_coded is %d\n", Vop->vop_coded );
        SE_INFO(group_frameparser_video,  "        use_intra_dc_vlc is %d\n", Vop->intra_dc_vlc_thr?0:1 );
        SE_INFO(group_frameparser_video,  "        trbi is %d\n", Vop->trbi );
        SE_INFO(group_frameparser_video,  "        trdi is %d\n", Vop->trdi );
        SE_INFO(group_frameparser_video,  "        trb_trd is %d\n", Vop->trb_trd );
        SE_INFO(group_frameparser_video,  "        trb_trd_trd is %d\n", Vop->trb_trd_trd );
        SE_INFO(group_frameparser_video,  "        trd is %d\n", Vop->trd );
        SE_INFO(group_frameparser_video,  "        trb is %d\n", Vop->trb );
        SE_INFO(group_frameparser_video,  "        intra_dc_vlc_thr is %d\n", Vop->intra_dc_vlc_thr );
        SE_INFO(group_frameparser_video,  "        time_inc is %d\n", Vop->time_inc );
        SE_INFO(group_frameparser_video,  "        fcode_back is %d\n", Vop->fcode_backward );
        SE_INFO(group_frameparser_video,  "        shape_coding_type is %d\n", 0);
        SE_INFO(group_frameparser_video,  "        bit_skip_no is %d\n", bit_skip_no );
    */
    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in the vos header.
//
unsigned int FrameParser_VideoDivx_c::ReadVosHeader()
{
    unsigned int profile = Bits.Get(8);

    if (profile != 0)
    {
        //SE_ERROR( "bad Vos header - profile = %d\n", profile);
        return 0;         /* STTC has a profile set to 0 which is Reserved in the mpeg4 spec */
    }

    if (Bits.Get(32) == 0x1b2)
    {
        if (Bits.Get(32) == 0x53545443)
        {
            return Bits.Get(32);
        }
    }

    //SE_ERROR( "bad Vos header\n");
    return 0;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to prepare a reference frame list
//

FrameParserStatus_t   FrameParser_VideoDivx_c::PrepareReferenceFrameList(void)
{
    unsigned int    i;
    unsigned int    ReferenceFramesNeeded;
    //
    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequence header or group of pictures
    // header which belong to the next frame.
    //
    ReferenceFramesNeeded = ((Mpeg4VideoFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->VopHeader.prediction_type;

    if (ReferenceFrameList.EntryCount < ReferenceFramesNeeded)
    {
        SE_ERROR("Insufficient Ref Frames %d vs %d\n", ReferenceFrameList.EntryCount,  ReferenceFramesNeeded);
        return FrameParserInsufficientReferenceFrames;
    }

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;
    ParsedFrameParameters->ReferenceFrameList[0].EntryCount             = ReferenceFramesNeeded;

    for (i = 0; i < ReferenceFramesNeeded; i++)
    {
        ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   =
            ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount - ReferenceFramesNeeded + i];
    }

    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to manage a reference frame list in forward play
//      we only record a reference frame as such on the last field, in order to
//      ensure the correct management of reference frames in the codec, we immediately
//      inform the codec of a release on the first field of a field picture.
//

FrameParserStatus_t   FrameParser_VideoDivx_c::ForPlayUpdateReferenceFrameList(void)
{
    if (ParsedFrameParameters->ReferenceFrame)
    {
        if (ReferenceFrameList.EntryCount >= MAX_REFERENCE_FRAMES_FORWARD_PLAY)
        {
            Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0]);
            ReferenceFrameList.EntryCount--;

            for (unsigned int i = 0; i < ReferenceFrameList.EntryCount; i++)
            {
                ReferenceFrameList.EntryIndicies[i] = ReferenceFrameList.EntryIndicies[i + 1];
            }
        }

        ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount++] = ParsedFrameParameters->DecodeFrameIndex;
    }

    return FrameParserNoError;
}
