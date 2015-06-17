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

#include "h263.h"
#include "frame_parser_video_flv1.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoFlv1_c"

struct DisplaySize_s
{
    unsigned int        Width;
    unsigned int        Height;
};
static struct DisplaySize_s Flv1DisplaySize[] =
{
    {    0,    0 },                     //
    {    0,    0 },                     //
    {  352,  288 },                     // H263_FORMAT_CIF,
    {  176,  144 },                     // H263_FORMAT_QCIF,
    {  128,   96 },                     // H263_FORMAT_SUB_QCIF,
    {  320,  240 },                     // H263_FORMAT_QVGA,
    {  160,  120 },                     // H263_FORMAT_QQVGA,
    {    0,    0 },                     //
};


#if defined (DUMP_HEADERS)
static unsigned int PictureNo;
#endif


// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoFlv1_c::FrameParser_VideoFlv1_c(void)
{
    Configuration.FrameParserName               = "VideoFlv1";

    mMinFrameWidth  = FLV1_MIN_FRAME_WIDTH;
    mMaxFrameWidth  = FLV1_MAX_FRAME_WIDTH;
    mMinFrameHeight = FLV1_MIN_FRAME_HEIGHT;
    mMaxFrameHeight = FLV1_MAX_FRAME_HEIGHT;

#if defined (DUMP_HEADERS)
    PictureNo                                   = 0;
#endif

}

//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoFlv1_c::ReadHeaders(void)
{
    FrameParserStatus_t         Status  = FrameParserNoError;
    unsigned int                StartCode;
    Bits.SetPointer(BufferData);
    StartCode                   = Bits.Get(17);

    if (StartCode == H263_PICTURE_START_CODE)
    {
        Status                  = FlvReadPictureHeader();
    }
    else
    {
        SE_ERROR("Not at the start of a picture - lost sync (%x)\n", StartCode);
        return FrameParserError;
    }

    if (Status == FrameParserNoError)
    {
        Status      = CommitFrameForDecode();
    }

    return Status;
}
//}}}
//{{{  FlvReadPictureHeader
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a picture header
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoFlv1_c::FlvReadPictureHeader(void)
{
    FrameParserStatus_t         Status;
    H263VideoPicture_t         *Header;
    H263VideoSequence_t        *SequenceHeader;
    unsigned int                Width;
    unsigned int                Height;
    unsigned char              *EndPointer;
    unsigned int                EndBitsInByte;

    if (FrameParameters == NULL)
    {
        Status                                  = GetNewFrameParameters((void **)&FrameParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }
    }

    Header                                      = &FrameParameters->PictureHeader;
    memset(Header, 0, sizeof(H263VideoPicture_t));
    Header->version                             = Bits.Get(5);

    if ((Header->version != 0x00) && (Header->version != 0x01))
    {
        SE_ERROR("Version %02x incorrect - should be 0x00 or 0x01\n", Header->version);
        return FrameParserError;
    }

    Header->tref                                = Bits.Get(8);                          // temporal reference 5.1.2
    Header->format                              = Bits.Get(3);                          // source format

    switch (Header->format)
    {
    case 0:
        Width                               = Bits.Get(8);
        Height                              = Bits.Get(8);
        break;

    case 1:
        Width                               = Bits.Get(16);
        Height                              = Bits.Get(16);
        break;

    default:
        Width                               = Flv1DisplaySize[Header->format].Width;
        Height                              = Flv1DisplaySize[Header->format].Height;
        break;
    }

    Header->ptype                               = Bits.Get(2);

    // For H263 there is no sequence header so we create one whenever the picture size changes
    if ((StreamParameters == NULL) || (!StreamParameters->SequenceHeaderPresent) ||
        (Header->format != StreamParameters->SequenceHeader.format))
    {
        Status                                  = GetNewStreamParameters((void **)&StreamParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }

        StreamParameters->UpdatedSinceLastFrame = true;
        SequenceHeader                          = &StreamParameters->SequenceHeader;
        memset(SequenceHeader, 0, sizeof(H263VideoSequence_t));
        StreamParameters->SequenceHeaderPresent = true;
        SequenceHeader->format                  = Header->format;
        SequenceHeader->width                   = Width;
        SequenceHeader->height                  = Height;
        /*In case of the stream starting with P, we don't change ptype to I in the sequence header*/
    }
    else
    {
        SequenceHeader                          = &StreamParameters->SequenceHeader;
    }

    Header->dflag                               = Bits.Get(1);                          // deblocking flag
    Header->pquant                              = Bits.Get(5);                          // quantizer 5.1.4

    while (Bits.Get(1))                                                                 // extra insertion info (pei) 5.1.9
    {
        Bits.Get(8);    // spare info (pspare) 5.1.10
    }

    Bits.GetPosition(&EndPointer, &EndBitsInByte);
    Header->bit_offset                          = (((unsigned int)EndPointer - (unsigned int)BufferData) * 8) + (8 - EndBitsInByte);
    FrameParameters->PictureHeaderPresent       = true;
#ifdef DUMP_HEADERS
    {
        SE_INFO(group_frameparser_video,  "Picture header (%d)\n", PictureNo++);
        SE_INFO(group_frameparser_video,  "    tref          : %6d\n", Header->tref);
        SE_INFO(group_frameparser_video,  "    format        : %6d\n", Header->format);
        SE_INFO(group_frameparser_video,  "    width         : %6d\n", SequenceHeader->width);
        SE_INFO(group_frameparser_video,  "    height        : %6d\n", SequenceHeader->height);
        SE_INFO(group_frameparser_video,  "    ptype         : %6d\n", Header->ptype);
        SE_INFO(group_frameparser_video,  "    pquant        : %6d\n", Header->pquant);
        SE_INFO(group_frameparser_video,  "    dflag         : %6d\n", Header->cpm);
    }
#endif
    return FrameParserNoError;
}
//}}}
