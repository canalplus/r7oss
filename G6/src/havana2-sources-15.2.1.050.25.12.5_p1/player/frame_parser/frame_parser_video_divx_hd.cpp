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

#include "frame_parser_video_divx_hd.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoDivxHd_c"

FrameParser_VideoDivxHd_c::FrameParser_VideoDivxHd_c(void)
    : FrameParser_VideoDivx_c()
    , mMetaData()
    , mIsMetaDataConfigurationSet(false)
{
}

FrameParserStatus_t  FrameParser_VideoDivxHd_c::ReadVopHeader(Mpeg4VopHeader_t         *Vop)
{
    unsigned char *ptr;
    unsigned int bits;
    FrameParserStatus_t Status;
    Status = FrameParser_VideoDivx_c::ReadVopHeader(Vop);

    if (Status == FrameParserNoError)
    {
        Bits.GetPosition(&ptr, &bits);

        if (!bits)
        {
            SE_ERROR("A point in the code that should never be reached - Implementation error\n");
            ParsedFrameParameters->DataOffset = ptr - BufferData;
            bit_skip_no = 0;
        }
        else
        {
            ParsedFrameParameters->DataOffset = ptr - BufferData;
            bit_skip_no = 8 - bits;
        }
    }

    return Status;
}

void   FrameParser_VideoDivxHd_c::ReadStreamMetadata(void)
{
    memcpy(&mMetaData, BufferData, sizeof(mMetaData));
    SE_DEBUG(group_frameparser_video, "----- StreamMetadata :-\n");
    SE_DEBUG(group_frameparser_video, "-----     Width             : %6d\n", mMetaData.Width);
    SE_DEBUG(group_frameparser_video, "-----     Height            : %6d\n", mMetaData.Height);

    mIsMetaDataConfigurationSet = true;
}

FrameParserStatus_t   FrameParser_VideoDivxHd_c::ReadHeaders(void)
{
    FrameParserStatus_t         Status  = FrameParserNoError;
    if (CodedFrameParameters->IsMpeg4p2MetaDataPresent)
    {
        SE_VERBOSE(group_frameparser_video, "About to call FrameParser_VideoDivxHd_c ReadStreamMetadata\n");
        ReadStreamMetadata();
        CodedFrameParameters->IsMpeg4p2MetaDataPresent = 0;
    }

    Status = ReadPictureHeader();
    return Status;
}

FrameParserStatus_t   FrameParser_VideoDivxHd_c::AllocateStreamParameterSet(void)
{
    StreamParameters = new Mpeg4VideoStreamParameters_t;
    if (StreamParameters == NULL)
    {
        SE_ERROR("Failed to allocate memory for StreamParameters\n");
        return FrameParserError;
    }
    ParsedFrameParameters->StreamParameterStructure = StreamParameters;
    ParsedFrameParameters->SizeofStreamParameterStructure = sizeof(Mpeg4VideoStreamParameters_t);
    ParsedFrameParameters->NewStreamParameters = true;
    memset(&StreamParameters->VolHeader, 0, sizeof(Mpeg4VolHeader_t));
    return FrameParserNoError;
}

FrameParserStatus_t   FrameParser_VideoDivxHd_c::ConstructDefaultVolHeader(void)
{
    FrameParserStatus_t Status;
    SE_INFO(group_frameparser_video, "Stream without VOL header, creating StreamParameters struct here\n");

    if (StreamParameters != NULL)
    {
        SE_ERROR("StreamParameters already set\n");
        return FrameParserError;
    }

    Status = AllocateStreamParameterSet();
    if (Status != FrameParserNoError)
    {
        SE_ERROR("Error allocating Stream Parameter Set\n");
        return Status;
    }

    Mpeg4VideoStreamParameters_t  *Parsed  = (Mpeg4VideoStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    /* default values for VOL header parameters */
    Parsed->VolHeader.version  = 100;
    Parsed->VolHeader.visual_object_layer_priority = 1;
    Parsed->VolHeader.visual_object_layer_verid = 1;
    Parsed->VolHeader.shape = SHAPE_RECTANGULAR;
    if (mIsMetaDataConfigurationSet)
    {
        Parsed->VolHeader.width = mMetaData.Width;
        Parsed->VolHeader.height = mMetaData.Height;
    }
    else
    {
        SE_ERROR("Inside VOL header creation but metaData not received\n");
        delete StreamParameters;
        StreamParameters = NULL;
        ParsedFrameParameters->NewStreamParameters = false;
        return FrameParserError;
    }
    Parsed->VolHeader.sprite_usage = SPRITE_NOT_USED;
    Parsed->VolHeader.quant_precision = 5;
    QuantPrecision = Parsed->VolHeader.quant_precision;
    Parsed->VolHeader.bits_per_pixel = 8;
    Parsed->VolHeader.resync_marker_disable = 1;
    Parsed->VolHeader.complexity_estimation_disable = 1;
    Parsed->VolHeader.obmc_disable  = 1;
    Parsed->VolHeader.par_height = mMetaData.Height;
    Parsed->VolHeader.par_width = mMetaData.Width;
    Parsed->VolHeader.aspect_ratio_info = PAR_SQUARE;
    memset(Parsed->VolHeader.intra_quant_matrix, 0, (sizeof(unsigned char) * QUANT_MATRIX_SIZE));
    memset(Parsed->VolHeader.non_intra_quant_matrix, 0, (sizeof(unsigned char) * QUANT_MATRIX_SIZE));
    Parsed->VolHeader.type_indication = 1;
    QuantPrecision = StreamParameters->VolHeader.quant_precision;
    Interlaced = StreamParameters->VolHeader.interlaced;
    StreamParameters->MicroSecondsPerFrame = CurrentMicroSecondsPerFrame;
    //TimeIncrementResolution = Parsed->VolHeader.time_increment_resolution;
    StreamParametersSet = true;
    return Status;
}

FrameParserStatus_t   FrameParser_VideoDivxHd_c::ReadPictureHeader(void)
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
                Status  = AllocateStreamParameterSet();
                if (Status != FrameParserNoError)
                {
                    SE_ERROR("Error allocating Stream Parameter Set\n");
                    return Status;
                }
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
            if (!StreamParametersSet)
            {
                ConstructDefaultVolHeader();
            }

            // SE_ERROR("%x VOP_START_CODE\n",VOP_START_CODE);
            // NOTE the vop reading depends on a valid Vol having been acquired
            if (StreamParametersSet)
            {
                Mpeg4VopHeader_t Vop;
                FrameParserStatus_t     Status = FrameParserNoError;

                if (DivXVersion == 311)
                {
                    unsigned char *ptr;
                    memset(&Vop, 0, sizeof(Vop)) ;
                    Vop.prediction_type = Bits.Get(2);
                    Vop.quantizer       = Bits.Get(5);
                    Bits.GetPosition(&ptr, &bit_skip_no);
                    bit_skip_no = 8 - bit_skip_no;
                    ParsedFrameParameters->DataOffset = (unsigned int)(ptr - BufferData);
                }
                else
                {
                    if (DivXVersion != 311 && (ParsedFrameParameters->DataOffset == 0xafff0000))
                    {
                        ParsedFrameParameters->DataOffset =  ExtractStartCodeOffset(StartCodeList->StartCodes[i]);
                    }

                    Status  = ReadVopHeader(&Vop);

                    if (Status != FrameParserNoError)
                    {
                        return Status;
                    }
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

                if (!Vop.divx_nvop)
                {
                    Status = CommitFrameForDecode();

                    if (Status != FrameParserNoError)
                    {
                        IncrementErrorStatistics(Status);
                    }
                }
            }
            else
            {
                SE_ERROR("Have Frame without Stream Parameters\n");
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
                SE_WARNING("Current ms per frame not valid (%u) defaulting to 25fps\n",
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
