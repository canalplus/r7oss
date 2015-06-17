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

Source file name : frame_parser_video_flv1.cpp
Author :           Julian

Implementation of the flv1 video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
28-May-08   Created                                         Julian

************************************************************************/

//#define DUMP_HEADERS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "h263.h"
#include "frame_parser_video_flv1.h"


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

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

FrameParser_VideoFlv1_c::FrameParser_VideoFlv1_c (void)
{

    // Our constructor is called after our subclass so the only change is to rename the frame parser
    Configuration.FrameParserName               = "VideoFlv1";

#if defined (DUMP_HEADERS)
    PictureNo                                   = 0;
#endif

    Reset();
}

//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoFlv1_c::ReadHeaders (void)
{
    FrameParserStatus_t         Status  = FrameParserNoError;
    unsigned int                StartCode;

#if 0
    unsigned int                i;
    report (severity_info, "First 32 bytes of %d :", BufferLength);
    for (i=0; i<32; i++)
        report (severity_info, " %02x", BufferData[i]);
    report (severity_info, "\n");
#endif

    Bits.SetPointer (BufferData);

#if 0
    {
        unsigned char* Buff;
        unsigned int   Bib;

        Bits.GetPosition(&Buff, &Bib);
        Buff -= 8;
        report (severity_info, "::::%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n    %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                Buff[0], Buff[1], Buff[2], Buff[3], Buff[4], Buff[5], Buff[6], Buff[7],
                Buff[8], Buff[9], Buff[10], Buff[11], Buff[12], Buff[13], Buff[14], Buff[15],
                Buff[16], Buff[17], Buff[18], Buff[19], Buff[20], Buff[21], Buff[22], Buff[23],
                Buff[24], Buff[25], Buff[26], Buff[27], Buff[28], Buff[29], Buff[30], Buff[31]);
    }
#endif

    StartCode                   = Bits.Get(17);
    if (StartCode == H263_PICTURE_START_CODE)
        Status                  = FlvReadPictureHeader ();
    else
    {
        FRAME_ERROR("Not at the start of a picture - lost sync (%x).\n", StartCode);
        return FrameParserError;
    }

    if (Status == FrameParserNoError)
        Status      = CommitFrameForDecode();

    return Status;
}
//}}}
//{{{  FlvReadPictureHeader
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a picture header
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoFlv1_c::FlvReadPictureHeader (void)
{
    FrameParserStatus_t         Status;
    H263VideoPicture_t*         Header;
    H263VideoSequence_t*        SequenceHeader;
    unsigned int                Width;
    unsigned int                Height;
    unsigned char*              EndPointer;
    unsigned int                EndBitsInByte;

    if (FrameParameters == NULL)
    {
        Status                                  = GetNewFrameParameters ((void **)&FrameParameters);
        if (Status != FrameParserNoError)
        return Status;
    }

    Header                                      = &FrameParameters->PictureHeader;
    memset (Header, 0x00, sizeof(H263VideoPicture_t));

    Header->version                             = Bits.Get(5);
    if ((Header->version != 0x00) && (Header->version != 0x01))
    {
        FRAME_ERROR("Version %02x incorrect - should be 0x00 or 0x01.\n", Header->version);
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
        Status                                  = GetNewStreamParameters ((void **)&StreamParameters);
        if (Status != FrameParserNoError)
            return Status;
        StreamParameters->UpdatedSinceLastFrame = true;

        SequenceHeader                          = &StreamParameters->SequenceHeader;
        memset (SequenceHeader, 0x00, sizeof(H263VideoSequence_t));
        StreamParameters->SequenceHeaderPresent = true;

        SequenceHeader->format                  = Header->format;
        SequenceHeader->width                   = Width;
        SequenceHeader->height                  = Height;

        Header->ptype                           = H263_PICTURE_CODING_TYPE_I;           // New sequence - set picture type to I (5.1.3)
    }
    else
        SequenceHeader                          = &StreamParameters->SequenceHeader;

    Header->dflag                               = Bits.Get(1);                          // deblocking flag
    Header->pquant                              = Bits.Get(5);                          // quantizer 5.1.4
    while (Bits.Get(1))                                                                 // extra insertion info (pei) 5.1.9
        Bits.Get(8);                                                                    // spare info (pspare) 5.1.10

    Bits.GetPosition (&EndPointer, &EndBitsInByte);
    Header->bit_offset                          = (((unsigned int)EndPointer - (unsigned int)BufferData) * 8) + (8 - EndBitsInByte);

    FrameParameters->PictureHeaderPresent       = true;

#ifdef DUMP_HEADERS
    {
        report (severity_note, "Picture header (%d)\n", PictureNo++);
        report (severity_info, "    tref          : %6d\n", Header->tref);
        report (severity_info, "    format        : %6d\n", Header->format);
        report (severity_info, "    width         : %6d\n", SequenceHeader->width);
        report (severity_info, "    height        : %6d\n", SequenceHeader->height);
        report (severity_info, "    ptype         : %6d\n", Header->ptype);
        report (severity_info, "    pquant        : %6d\n", Header->pquant);
        report (severity_info, "    dflag         : %6d\n", Header->cpm);
    }
#endif

     return FrameParserNoError;
}
//}}}
