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

Source file name : frame_parser_video_theora.cpp
Author :           Julian

Implementation of the theora video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
10-Mar-09   Created                                         Julian

************************************************************************/

//#define DUMP_HEADERS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "theora.h"
#include "frame_parser_video_theora.h"
#include "ring_generic.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//


static BufferDataDescriptor_t     TheoraStreamParametersBuffer  = BUFFER_THEORA_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     TheoraFrameParametersBuffer   = BUFFER_THEORA_FRAME_PARAMETERS_TYPE;

#define Assert(x, fmt, args...) {                                                               \
        if(!(x))                                                                                \
        {                                                                                       \
            report (severity_error, "FrameParser_VideoTheora_c::%s: " fmt,  __FUNCTION__, ##args); \
            Player->MarkStreamUnPlayable (Stream);                                              \
            return FrameParserError;                                                            \
        }                       }

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

#if defined (DUMP_HEADERS)
static unsigned int PictureNo;
#endif

//{{{  av_log2
#if defined (PARSE_THEORA_HEADERS)
const unsigned char ff_log2_tab[256]    =
{
        0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};
static inline unsigned int av_log2(unsigned int v)
{
    int n = 0;
    if (v & 0xffff0000) {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00) {
        v >>= 8;
        n += 8;
    }
    n += ff_log2_tab[v];

    return n;
}
#error "AArgh"
#endif
//}}}

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoTheora_c::FrameParser_VideoTheora_c (void)
{

    // Our constructor is called after our subclass so the only change is to rename the frame parser
    Configuration.FrameParserName               = "VideoTheora";


    Configuration.StreamParametersCount         = 4;
    Configuration.StreamParametersDescriptor    = &TheoraStreamParametersBuffer;

    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &TheoraFrameParametersBuffer;

    memset (&ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t));

    FrameRate                                   = Rational_t (24000, 1001);
    StreamHeadersRead                           = TheoraNoHeaders;

#if defined (DUMP_HEADERS)
    PictureNo                                   = 0;
#endif

    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoTheora_c::~FrameParser_VideoTheora_c (void)
{
    Halt();
    Reset();
}
//}}}
//{{{  Reset
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      The Reset function release any resources, and reset all variable
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoTheora_c::Reset (void)
{
    memset (&ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t));

    StreamParameters                            = NULL;
    FrameParameters                             = NULL;

    FirstDecodeOfFrame                          = true;
    FrameRate                                   = Rational_t (24000, 1001);
    StreamHeadersRead                           = TheoraNoHeaders;

    return FrameParser_Video_c::Reset();
}
//}}}


//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoTheora_c::ReadHeaders (void)
{
    FrameParserStatus_t         Status  = FrameParserNoError;

#if 0
    unsigned int                i;
    report (severity_info, "First 32 bytes of %d :", BufferLength);
    for (i=0; i<32; i++)
        report (severity_info, " %02x", BufferData[i]);
    report (severity_info, "\n");
#endif

    Bits.SetPointer (BufferData);

    if (StreamHeadersRead != TheoraAllHeaders)
        Status          = ReadStreamHeaders ();
    else
    {
        Status          = ReadPictureHeader ();
        if (Status == FrameParserNoError)
            Status      = CommitFrameForDecode();
    }

    return Status;
}
//}}}
//{{{  ReadStreamHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in an Theora stream headers
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoTheora_c::ReadStreamHeaders (void)
{
    unsigned int                HeaderType;
    char                        HeaderName[8];
    TheoraVideoSequence_t*      SequenceHeader;
    FrameParserStatus_t         Status;
    int                         i;

#if 0
    {
        for (i=0; i<40; i+=4)
            report (severity_info, "%02x %02x %02x %02x\n", BufferData[i], BufferData[i+1], BufferData[i+2], BufferData[i+3]);
    }
#endif

    if ((StreamParameters != NULL) && (StreamParameters->SequenceHeaderPresent))
    {
        FRAME_ERROR ("%s: Received Stream headers after previous sequence data\n", __FUNCTION__);
        return FrameParserNoError;
    }

    if (StreamParameters == NULL)
    {
        Status                                          = GetNewStreamParameters ((void **)&StreamParameters);
        if (Status != FrameParserNoError)
            return Status;

        memset (&StreamParameters->SequenceHeader, 0x00, sizeof(TheoraVideoSequence_t));
    }

    SequenceHeader                                      = &StreamParameters->SequenceHeader;

    HeaderType                                          = Bits.Get(8);
    memset (HeaderName, 0, sizeof(HeaderName));
    for (i=0; i<6; i++)
        HeaderName[i]                                   = Bits.Get(8);
    if (strcmp (HeaderName, "theora") != 0)
    {
        FRAME_ERROR ("Stream is not a valid Theora stream\n");
        Player->MarkStreamUnPlayable (Stream);
        return FrameParserError;
    }
    //SequenceHeader->StreamType                          = THEORA_STREAM_TYPE_THEORA;

    if (HeaderType == THEORA_IDENTIFICATION_HEADER)
    {
        unsigned int            PictureXOffset;
        unsigned int            PictureYOffset;

        SequenceHeader->Version                         = Bits.Get(24);
        //if (SequenceHeader->Version < 0x30200)
        //{
        //    FRAME_ERROR ("Invalid version numbers should be 0x302xx, %x\n", SequenceHeader->Version);
        //    Player->MarkStreamUnPlayable (Stream);
        //    return FrameParserError;
        //}
        SequenceHeader->MacroblockWidth                 = Bits.Get(16);
        SequenceHeader->MacroblockHeight                = Bits.Get(16);
        SequenceHeader->DecodedWidth                    = Bits.Get(24);
        SequenceHeader->DecodedHeight                   = Bits.Get(24);
        PictureXOffset                                  = Bits.Get(8);
        PictureYOffset                                  = Bits.Get(8);
        SequenceHeader->DisplayWidth                    = SequenceHeader->DecodedWidth - PictureXOffset - PictureXOffset;
        SequenceHeader->DisplayHeight                   = SequenceHeader->DecodedHeight - PictureYOffset - PictureYOffset;
        SequenceHeader->FrameRateNumerator              = Bits.Get(32);
        SequenceHeader->FrameRateDenominator            = Bits.Get(32);
        FrameRate                                       = Rational_t (SequenceHeader->FrameRateNumerator, SequenceHeader->FrameRateDenominator);
        SequenceHeader->PixelAspectRatioNumerator       = Bits.Get(24);
        SequenceHeader->PixelAspectRatioDenominator     = Bits.Get(24);
        if ((SequenceHeader->PixelAspectRatioNumerator == 0) || (SequenceHeader->PixelAspectRatioDenominator == 0))
            PixelAspectRatio                            = Rational_t (1, 1);
        else
            PixelAspectRatio                            = Rational_t (SequenceHeader->PixelAspectRatioNumerator, SequenceHeader->PixelAspectRatioDenominator);

        if (SequenceHeader->Version < 0x30200)
            Bits.Get(5);                                // keyframe frequency force

        SequenceHeader->ColourSpace                     = Bits.Get(8);
        Bits.Get(24);                                   // NOMBR
        Bits.Get(6);                                    // QUAL
        Bits.Get(5);                                    // KGSHIFT
        SequenceHeader->PixelFormat                     = Bits.Get(2);

    #if !defined (PARSE_THEORA_HEADERS)
        memcpy (SequenceHeader->InfoHeaderBuffer, BufferData, BufferLength);
        SequenceHeader->InfoHeaderSize                 = BufferLength;
    #endif

        StreamHeadersRead                              |= TheoraIdentificationHeader;
    }
    else if (HeaderType == THEORA_COMMENT_HEADER)
    {

    #if !defined (PARSE_THEORA_HEADERS)
        memcpy (SequenceHeader->CommentHeaderBuffer, BufferData, BufferLength);
        SequenceHeader->CommentHeaderSize               = BufferLength;
    #endif

        StreamHeadersRead                              |= TheoraCommentHeader;
    }
    else if (HeaderType == THEORA_SETUP_HEADER)
    {

    #if !defined (PARSE_THEORA_HEADERS)
        memcpy (SequenceHeader->SetupHeaderBuffer, BufferData, BufferLength);
        SequenceHeader->SetupHeaderSize                 = BufferLength;
    #else
        //{{{  Parse the setup header
        unsigned int    NBits;
        unsigned int    BmIndexes;
        unsigned int    qi, bmi, ci, qti, pli;

        if (SequenceHeader->Version >= 0x030200)
        {
            NBits                                       = Bits.Get(3);
            for (qi=0; qi<64; qi++)
                SequenceHeader->LoopFilterLimit[qi]     = Bits.Get(NBits);
        }

        if (SequenceHeader->Version >= 0x030200)
            NBits                                       = Bits.Get(4)+1;
        else
            NBits                                       = 16;
        for (qi=0; qi<64; qi++)
            SequenceHeader->AcScale[qi]                 = Bits.Get(NBits);

        if (SequenceHeader->Version >= 0x030200)
            NBits                                       = Bits.Get(4)+1;
        else
            NBits                                       = 16;
        for (qi=0; qi<64; qi++)
            SequenceHeader->DcScale[qi]                 = Bits.Get(NBits);

        if (SequenceHeader->Version >= 0x030200)
            BmIndexes                                   = Bits.Get(9)+1;
        else
            BmIndexes                                   = 3;
        if (BmIndexes >= 384)
        {
            FRAME_ERROR ("Too many base matrices (%d)\n", BmIndexes);
            Player->MarkStreamUnPlayable (Stream);
            return FrameParserError;
        }
        for (bmi=0; bmi<BmIndexes; bmi++)
        {
            for (ci=0; ci<64; ci++)
                SequenceHeader->BaseMatrix[bmi][ci]      = Bits.Get(8);
        }

        for (qti=0; qti<=1; qti++)
        {
            for (pli=0; pli<=2; pli++)
            {
                int     NewQR   = 1;

                if (qti || (pli > 0))
                    NewQR                               = Bits.Get(1);
                if (NewQR == 0)
                //{{{  Copy a previously defined set of quant matrices
                {
                    unsigned int    qtj, plj;

                    if (qti && Bits.Get(1))
                    {
                        qtj                             = 0;
                        plj                             = pli;
                    }
                    else
                    {
                        qtj                             = (3 * qti + pli - 1) / 3;
                        plj                             = (pli + 2) % 3;
                    }
                    SequenceHeader->QRCount[qti][pli]   = SequenceHeader->QRCount[qtj][plj];
                    memcpy (SequenceHeader->QRSize[qti][pli], SequenceHeader->QRSize[qtj][plj], sizeof(SequenceHeader->QRSize[0][0]));
                    memcpy (SequenceHeader->QRBase[qti][pli], SequenceHeader->QRBase[qtj][plj], sizeof(SequenceHeader->QRBase[0][0]));
                }
                //}}}
                else
                //{{{  define new set
                {
                    unsigned int    qri = 0;

                    qi                  = 0;
                    while (1)
                    {
                        unsigned int     BmIndex;
                        BmIndex                                 = Bits.Get (av_log2 (BmIndexes-1)+1);

                        if (BmIndex >= BmIndexes)
                        {
                            FRAME_ERROR ("Invalid base matrix index %d (%d)\n", BmIndex, BmIndexes);
                            Player->MarkStreamUnPlayable (Stream);
                            return FrameParserError;
                        }
                        SequenceHeader->QRBase[qti][pli][qri]   = BmIndex;

                        if (qi >= 63)
                                break;

                        BmIndex                                 = Bits.Get (av_log2 (63-qi)+1) + 1;
                        SequenceHeader->QRSize[qti][pli][qri]   = BmIndex;
                        qi                                     += BmIndex;
                        qri++;
                    }
                    if (qi > 63)
                    {
                        FRAME_ERROR ("Invalid qi %d\n", qi);
                        Player->MarkStreamUnPlayable (Stream);
                        return FrameParserError;
                    }
                    SequenceHeader->QRCount[qti][pli]           = qri;
                }
                //}}}
            }

        }


        // Read the Huffman tables
        for (SequenceHeader->hti=0; SequenceHeader->hti<80; SequenceHeader->hti++)
        {
            SequenceHeader->Entries                     = 0;
            SequenceHeader->HuffmanCodeSize             = 1;

            if (Bits.Get(1) == 0)
            {
                SequenceHeader->HBits                   = 0;
                if (ReadHuffmanTree(SequenceHeader) != 0)
                    return FrameParserError;

                SequenceHeader->HBits                   = 1;
                if (ReadHuffmanTree(SequenceHeader) != 0)
                    return FrameParserError;
            }
        }

        //{{{  Trace
        #if 0
        report (severity_info, "Loop Filter Limits:\n");
        for (qi=0; qi<64; qi++)
        {
            report (severity_info, "%02x ", SequenceHeader->LoopFilterLimit[qi]);
            if (((qi+1)&0x1f)== 0)
                report (severity_info, "\n");
        }
        report (severity_info, "\nAC Scale:\n");
        for (qi=0; qi<64; qi++)
        {
            report (severity_info, "%08x ", SequenceHeader->AcScale[qi]);
            if (((qi+1)&0x07)== 0)
                report (severity_info, "\n");
        }
        report (severity_info, "\nDC Scale:\n");
        for (qi=0; qi<64; qi++)
        {
            report (severity_info, "%04x ", SequenceHeader->DcScale[qi]);
            if (((qi+1)&0x0f)== 0)
                report (severity_info, "\n");
        }
        report (severity_info, "\nBm Indexes %d\n", BmIndexes);
        for (bmi=0; bmi<BmIndexes; bmi++)
        {
            report (severity_info, "%d:\n", bmi);
            for (ci=0; ci<64; ci++)
            {
                report (severity_info, "%02x ", SequenceHeader->BaseMatrix[bmi][ci]);
                if (((ci+1)&0x1f)== 0)
                    report (severity_info, "\n");
            }
        }
        report (severity_info, "\nQR Counts\n");
        for (qti=0; qti<=1; qti++)
        {
            report (severity_info, "%d:\n", qti);
            for (pli=0; pli<=2; pli++)
                report (severity_info, "%02x ", SequenceHeader->QRCount[qti][pli]);
        }
                if (((ci+1)&0x1f)== 0)
                    report (severity_info, "\n");
            {
            }
        report (severity_info, "\nQR Size\n");
        for (qti=0; qti<=1; qti++)
        {
            for (pli=0; pli<=2; pli++)
            {
                report (severity_info, "%d:%d:\n", qti, pli);
                for (qi=0; qi<64; qi++)
                {
                    report (severity_info, "%02x ", SequenceHeader->QRSize[qti][pli][qi]);
                    if (((qi+1)&0x1f)== 0)
                        report (severity_info, "\n");
                }
            }
        }
        report (severity_info, "\nQR Base\n");
        for (qti=0; qti<=1; qti++)
        {
            for (pli=0; pli<=2; pli++)
            {
                report (severity_info, "%d:%d:\n", qti, pli);
                for (qi=0; qi<64; qi++)
                {
                    report (severity_info, "%02x ", SequenceHeader->QRBase[qti][pli][qi]);
                    if (((qi+1)&0x1f)== 0)
                        report (severity_info, "\n");
                }
            }
        }
        report (severity_info, "\nHuffman table hti %d, HBits %d, Entries %d, HuffmanCodeSize %d\n", SequenceHeader->hti, SequenceHeader->HBits, SequenceHeader->Entries, SequenceHeader->HuffmanCodeSize);
        for (qti=0; qti<80; qti++)
        {
            report (severity_info, "%d:\n", qti);
            for (pli=0; pli<32; pli++)
            {
                report (severity_info, "(%04x %04x)", SequenceHeader->HuffmanTable[qti][pli][0], SequenceHeader->HuffmanTable[qti][pli][1]);
                if (((pli+1)&0x07)== 0)
                    report (severity_info, "\n");
            }
            report (severity_info, "\n");
        }
        #endif
        //}}}

        //}}}
    #endif

        StreamHeadersRead                              |= TheoraSetupHeader;
    }
    else
    {
        FRAME_ERROR ("Unrecognised header (Not Identification, Comment or Setup) (%x)\n", HeaderType);
        Player->MarkStreamUnPlayable (Stream);
        return FrameParserError;
    }

    StreamParameters->SequenceHeaderPresent             = StreamHeadersRead == TheoraAllHeaders;
    StreamParameters->UpdatedSinceLastFrame             = StreamParameters->SequenceHeaderPresent;

#if 1 //def DUMP_HEADERS
    if (StreamHeadersRead == TheoraAllHeaders)
    {
        report (severity_info, "SequenceHeader :- \n");
        report (severity_info, "    Version           : %6x\n", SequenceHeader->Version);
        report (severity_info, "    DecodedWidth      : %6d\n", SequenceHeader->DecodedWidth);
        report (severity_info, "    DecodedHeight     : %6d\n", SequenceHeader->DecodedHeight);
        report (severity_info, "    DisplayWidth      : %6d\n", SequenceHeader->DisplayWidth);
        report (severity_info, "    DisplayHeight     : %6d\n", SequenceHeader->DisplayHeight);
        report (severity_info, "    FrameRate         : %d.%d\n", FrameRate.IntegerPart(), FrameRate.RemainderDecimal());
        report (severity_info, "    FrameRate         : %d,%d\n", SequenceHeader->FrameRateNumerator, SequenceHeader->FrameRateDenominator);
        report (severity_info, "    PixelAspectRatio  : %d.%d\n", PixelAspectRatio.IntegerPart(), PixelAspectRatio.RemainderDecimal());
        report (severity_info, "    PixelAspectRatio  : %d,%d\n", SequenceHeader->PixelAspectRatioNumerator, SequenceHeader->PixelAspectRatioDenominator);
        report (severity_info, "    ColourSpace       : %6d\n", SequenceHeader->ColourSpace);
        report (severity_info, "    PixelFormat       : %6d\n", SequenceHeader->PixelFormat);
    }
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
FrameParserStatus_t   FrameParser_VideoTheora_c::ReadPictureHeader (void)
{
    FrameParserStatus_t         Status;
    TheoraVideoPicture_t*       Header;

    if (FrameParameters == NULL)
    {
        Status                                  = GetNewFrameParameters ((void **)&FrameParameters);
        if (Status != FrameParserNoError)
        return Status;
    }

    if (Bits.Get(1) != 0)
    {
        report (severity_error, "%s: Not frame data - first bit is not zero\n", __FUNCTION__);
        return FrameParserError;
    }

    Header                                      = &FrameParameters->PictureHeader;
    memset (Header, 0x00, sizeof(TheoraVideoPicture_t));

    if (Bits.Get(1) == 0)
        Header->PictureType                     = THEORA_PICTURE_CODING_TYPE_I;
    else
        Header->PictureType                     = THEORA_PICTURE_CODING_TYPE_P;

    FrameParameters->PictureHeaderPresent       = true;

#ifdef DUMP_HEADERS
    report (severity_note, "Picture header (%d)\n", PictureNo++);
    report (severity_info, "    PictureType             : %6d\n", Header->PictureType);
#endif

     return FrameParserNoError;
}
//}}}
//{{{  ReadHuffmanTree
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read a huffman tree
///
/// /////////////////////////////////////////////////////////////////////////
int FrameParser_VideoTheora_c::ReadHuffmanTree (TheoraVideoSequence_t*        SequenceHeader)
{
#if defined (PARSE_THEORA_HEADERS)

    if (Bits.Get (1))
    {
        int     Token;
        if (SequenceHeader->Entries >= 32)
        {
            FRAME_ERROR ("Huffman tree entries too great (A)\n");
            Player->MarkStreamUnPlayable (Stream);
            return FrameParserError;
        }
        Token                                                           = Bits.Get(5);
        SequenceHeader->HuffmanTable[SequenceHeader->hti][Token][0]     = SequenceHeader->HBits;
        SequenceHeader->HuffmanTable[SequenceHeader->hti][Token][1]     = SequenceHeader->HuffmanCodeSize;
        SequenceHeader->Entries++;
    }
    else
    {
        if (SequenceHeader->HuffmanCodeSize >= 32)
        {
            FRAME_ERROR ("Huffman tree entries too great (B)\n");
            Player->MarkStreamUnPlayable (Stream);
            return FrameParserError;
        }
        SequenceHeader->HuffmanCodeSize++;
        SequenceHeader->HBits         <<= 1;
        if (ReadHuffmanTree(SequenceHeader) != 0)
            return -1;

        SequenceHeader->HBits          |= 1;
        if (ReadHuffmanTree(SequenceHeader) != 0)
            return -1;
        SequenceHeader->HBits         >>= 1;

        SequenceHeader->HuffmanCodeSize--;
    }
#endif
    return 0;
}
//}}}

//{{{  RegisterOutputBufferRing
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Register the output ring
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoTheora_c::RegisterOutputBufferRing (Ring_t          Ring)
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

    return FrameParser_Video_c::RegisterOutputBufferRing (Ring);
}
//}}}
//{{{  PrepareReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Stream specific function to prepare a reference frame list
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoTheora_c::PrepareReferenceFrameList (void)
{
    unsigned int               i;
    unsigned int               ReferenceFramesNeeded;
    unsigned int               PictureCodingType;
    TheoraVideoPicture_t*      PictureHeader;

    //
    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequenece header or group of pictures
    // header which belong to the next frame.
    //

    // For Theora, every frame is a reference frame so we always fill in the reference frame list
    // even though I frames do not actually need them.
    // Element 0 is for the reference frame and element 1 is for the golden frame.

    PictureHeader               = &(((TheoraFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader);
    PictureCodingType           = PictureHeader->PictureType;
    ReferenceFramesNeeded       = (PictureCodingType == THEORA_PICTURE_CODING_TYPE_P) ? 1 : 0;

    if (ReferenceFrameList.EntryCount < ReferenceFramesNeeded)
        return FrameParserInsufficientReferenceFrames;

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;

    if ((ReferenceFrameList.EntryCount == 0) && (PictureCodingType == THEORA_PICTURE_CODING_TYPE_I))       // First frame reference self
    {
        ParsedFrameParameters->ReferenceFrameList[0].EntryCount         = 2;
        ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0]   = NextDecodeFrameIndex;
        ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[1]   = NextDecodeFrameIndex;
    }
    else
    {
        ParsedFrameParameters->ReferenceFrameList[0].EntryCount         = ReferenceFrameList.EntryCount;
        for (i=0; i<ReferenceFrameList.EntryCount; i++)
            ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[i];
    }
    //report (severity_info, "Prepare Ref list %d %d - %d %d - %d %d %d\n", ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[1],
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
FrameParserStatus_t   FrameParser_VideoTheora_c::ForPlayUpdateReferenceFrameList (void)
{
    TheoraFrameParameters_t*    FrameParameters = (TheoraFrameParameters_t*)ParsedFrameParameters->FrameParameterStructure;

    // For Theora every frame is a reference frame so we always free the current reference frame if it isn't a
    // golden frame.  The reference frame is kept in slot 0. If the current frame is also a golden frame we
    // free and replace the golden frame in slot 1 as well.

    if (ReferenceFrameList.EntryCount == 0)
        ReferenceFrameList.EntryCount           = 1;
    else if (ReferenceFrameList.EntryIndicies[0] != ReferenceFrameList.EntryIndicies[1])
        Player->CallInSequence (Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0]);

    ReferenceFrameList.EntryIndicies[0]         = ParsedFrameParameters->DecodeFrameIndex;      // put us into slot 0 as a reference frame

    if (FrameParameters->PictureHeader.PictureType == THEORA_PICTURE_CODING_TYPE_I)
    {
        if (ReferenceFrameList.EntryCount == 1)
            ReferenceFrameList.EntryCount       = 2;
        else
            Player->CallInSequence (Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[1]);
        ReferenceFrameList.EntryIndicies[1]     = ParsedFrameParameters->DecodeFrameIndex;      // insert into slot 1 as a golden frame
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

FrameParserStatus_t   FrameParser_VideoTheora_c::RevPlayProcessDecodeStacks (void)
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
FrameParserStatus_t   FrameParser_VideoTheora_c::CommitFrameForDecode (void)
{
    TheoraVideoPicture_t*               PictureHeader;
    TheoraVideoSequence_t*              SequenceHeader;

    //
    // Check we have the headers we need
    //
    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        FRAME_ERROR ("Stream parameters unavailable for decode.\n");
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        FRAME_ERROR ("Frame parameters unavailable for decode (%p).\n", FrameParameters);
        return FrameParserPartialFrameParameters;
    }

    SequenceHeader                      = &StreamParameters->SequenceHeader;
    PictureHeader                       = &FrameParameters->PictureHeader;

    // Record the stream and frame parameters into the appropriate structure

    // Parsed frame parameters
    ParsedFrameParameters->FirstParsedParametersForOutputFrame  = true;  //FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump  = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                  = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                = ContinuousReverseJump;

    ParsedFrameParameters->KeyFrame                             = PictureHeader->PictureType == THEORA_PICTURE_CODING_TYPE_I;
    ParsedFrameParameters->ReferenceFrame                       = true;

    ParsedFrameParameters->IndependentFrame                     = ParsedFrameParameters->KeyFrame;
    ParsedFrameParameters->NumberOfReferenceFrameLists          = 1;

    ParsedFrameParameters->NewStreamParameters                  = NewStreamParametersCheck();
    ParsedFrameParameters->SizeofStreamParameterStructure       = sizeof(TheoraStreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure             = StreamParameters;

    ParsedFrameParameters->NewFrameParameters                   = true;
    ParsedFrameParameters->SizeofFrameParameterStructure        = sizeof(TheoraFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure              = FrameParameters;

    // Parsed video parameters
    ParsedVideoParameters->Content.Width                        = SequenceHeader->DecodedWidth;
    ParsedVideoParameters->Content.Height                       = SequenceHeader->DecodedHeight;
    ParsedVideoParameters->Content.DisplayWidth                 = SequenceHeader->DisplayWidth;
    ParsedVideoParameters->Content.DisplayHeight                = SequenceHeader->DisplayHeight;

    ParsedVideoParameters->Content.Progressive                  = true;
    ParsedVideoParameters->Content.OverscanAppropriate          = false;

    ParsedVideoParameters->Content.VideoFullRange               = false;                                // Theora conforms to ITU_R_BT601 for source
    ParsedVideoParameters->Content.ColourMatrixCoefficients     = MatrixCoefficients_ITU_R_BT601;

    ParsedVideoParameters->Content.FrameRate                    = FrameRate;
    ParsedVideoParameters->DisplayCount[0]                      = 1;
    ParsedVideoParameters->DisplayCount[1]                      = 0;

    ParsedVideoParameters->Content.PixelAspectRatio             = PixelAspectRatio;

    ParsedVideoParameters->PictureStructure                     = StructureFrame;
    ParsedVideoParameters->InterlacedFrame                      = false;
    ParsedVideoParameters->TopFieldFirst                        = true;

    ParsedVideoParameters->PanScan.Count                        = 0;

    // Record our claim on both the frame and stream parameters
    Buffer->AttachBuffer (StreamParametersBuffer);
    Buffer->AttachBuffer (FrameParametersBuffer);

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
bool   FrameParser_VideoTheora_c::NewStreamParametersCheck (void)
{
    //
    // The parameters cannot be new if they have been used before.
    //

    if (!StreamParameters->UpdatedSinceLastFrame)
        return false;

    StreamParameters->UpdatedSinceLastFrame     = false;

    return true;
}
//}}}
