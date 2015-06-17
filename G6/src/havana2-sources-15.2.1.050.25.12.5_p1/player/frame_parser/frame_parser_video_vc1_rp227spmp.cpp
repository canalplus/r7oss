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

#include "frame_parser_video_vc1_rp227spmp.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoVc1_Rp227SpMp_c"

#define REMOVE_ANTI_EMULATION_BYTES
#define Assert(L)               if( !(L) )                                                                      \
                                {                                                                               \
                                    SE_WARNING("Check failed at line %d\n", __LINE__ );\
                                    Stream->MarkUnPlayable();                                     \
                                    return FrameParserError;                                                    \
                                }
#define AssertAntiEmulationOk()                                                                             \
                                {                                                                               \
                                    FrameParserStatus_t     Status;                                                 \
                                    Status  = TestAntiEmulationBuffer();                                        \
                                    if( Status != FrameParserNoError )                                          \
                                    {                                                                           \
                                        SE_WARNING("Anti Emulation Test fail\n");    \
                                        Stream->MarkUnPlayable();                                 \
                                        return FrameParserError;                                                \
                                    }                                                                           \
                                }
FrameParser_VideoVc1_Rp227SpMp_c::FrameParser_VideoVc1_Rp227SpMp_c(void)
    : FrameParser_VideoWmv_c()
{
    Configuration.FrameParserName               = "VideoVc1Rp227SpMp";

#if defined (DUMP_HEADERS)
    PictureNo   = 0;
#endif
}

FrameParserStatus_t  FrameParser_VideoVc1_Rp227SpMp_c::ReadSpMpPesPacketPayloadFormatHeader(void)
{
    Vc1VideoSequence_t         *SequenceHeader;
    Vc1VideoEntryPoint_t       *EntryPointHeader;

    FrameParserStatus_t Status = GetNewStreamParameters((void **)&StreamParameters);
    if (Status != FrameParserNoError)
    {
        return Status;
    }

    StreamParameters->UpdatedSinceLastFrame = true;

    SequenceHeader      = &StreamParameters->SequenceHeader;
    memset(SequenceHeader, 0, sizeof(Vc1VideoSequence_t));
    EntryPointHeader    = &StreamParameters->EntryPointHeader;
    memset(EntryPointHeader, 0, sizeof(Vc1VideoEntryPoint_t));

    StreamParameters->StreamType = Vc1StreamTypeVc1Rp227SpMp;

    // frame_width Table 8: VC-1_SPMP_PESpacket_PayloadFormatHeader() structure
    SequenceHeader->max_coded_width                    = Bits.Get(8);          // frame_width
    SequenceHeader->max_coded_width                   |= (Bits.Get(8) << 8);
    // frame_height Table 8: VC-1_SPMP_PESpacket_PayloadFormatHeader() structure
    SequenceHeader->max_coded_height                     = Bits.Get(8);          // frame_height
    SequenceHeader->max_coded_height                    |= (Bits.Get(8) << 8);
    // Sequence Header Struct C - see table 263, 264 in Annex J

    if (ReadSequenceHeaderStruct_C() != FrameParserNoError)
    {
        return FrameParserError;
    }

    StreamParameters->SequenceHeaderPresent             = true;
    StreamParameters->EntryPointHeaderPresent           = true;
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "    max_coded_width   : %6d\n", SequenceHeader->max_coded_width);
    SE_INFO(group_frameparser_video,  "    max_coded_height  : %6d\n", SequenceHeader->max_coded_height);
#endif

#if defined (REMOVE_ANTI_EMULATION_BYTES)
    AssertAntiEmulationOk();
#endif
    return FrameParserNoError;
}

//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_Rp227SpMp_c::ReadHeaders(void)
{
    FrameParserStatus_t         Status  = FrameParserNoError;
    Bits.SetPointer(BufferData);
    unsigned int                i;
    unsigned int                Code;
    bool                        FrameReadyForDecode     = false;

    for (i = 0; i < StartCodeList->NumberOfStartCodes; i++)
    {
        Code    = StartCodeList->StartCodes[i];
#if defined (REMOVE_ANTI_EMULATION_BYTES)
        LoadAntiEmulationBuffer(BufferData + ExtractStartCodeOffset(Code));
        CheckAntiEmulationBuffer(METADATA_ANTI_EMULATION_REQUEST);
#else
        Bits.SetPointer(BufferData + ExtractStartCodeOffset(Code));
#endif

        Bits.FlushUnseen(32);
        Status  = FrameParserNoError;

        if (FrameReadyForDecode)
        {
            Status              = CommitFrameForDecode();
            FrameReadyForDecode = false;
        }

        switch (ExtractStartCodeCode(Code))
        {
        case  VC1_SEQUENCE_HEADER_CODE:
            Status  = ReadSpMpPesPacketPayloadFormatHeader();
            break;

        case   VC1_FRAME_START_CODE:
            ParsedFrameParameters->DataOffset       = ExtractStartCodeOffset(Code);
            Status = ReadPictureHeaderSimpleMainProfile();
            if (Status == FrameParserNoError)
            {
                FrameReadyForDecode = true;
            }

#if defined (REMOVE_ANTI_EMULATION_BYTES)
            AssertAntiEmulationOk();
#endif
            break;

        default:
            SE_ERROR("Unknown start code %x  This is @ StartCode number %d\n", ExtractStartCodeCode(Code), i);
            Status  = FrameParserUnhandledHeader;
            break;
        }

        if (Status != FrameParserNoError)
        {
            IncrementErrorStatistics(Status);
        }

        if ((Status != FrameParserNoError) && (Status != FrameParserUnhandledHeader))
        {
            return Status;
        }

#if defined (REMOVE_ANTI_EMULATION_BYTES)
        // Check that we didn't lose track and overun the anti-emulation buffer
        AssertAntiEmulationOk();
#endif
    }

    // Finished processing all the start codes, send the frame to be decoded.
    if (FrameReadyForDecode)
    {
        Status              = CommitFrameForDecode();
        FrameReadyForDecode = false;
    }

    return Status;
}
