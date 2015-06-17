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

Source file name : frame_parser_video_wmv.cpp
Author :           Julian

Implementation of the VC-1 video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
26-Nov-07   Created                                         Julian

************************************************************************/

//#define DUMP_HEADERS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_video_wmv.h"


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

#if defined (DUMP_HEADERS)
static unsigned int PictureNo;
#endif


FrameParser_VideoWmv_c::FrameParser_VideoWmv_c (void)
{

    // Our constructor is called after our subclass so the only change is to rename the frame parser
    Configuration.FrameParserName               = "VideoWmv";
#if defined (DUMP_HEADERS)
    PictureNo   = 0;
#endif
    RangeReduction                              = 0;

    Reset();
}

//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoWmv_c::ReadHeaders (void)
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
    if (!SequenceLayerMetaDataValid)
        Status          = ReadSequenceLayerMetadata();
    else
    {
        Status          = ReadPictureHeaderSimpleMainProfile();
        if (Status == FrameParserNoError)
            Status      = CommitFrameForDecode();
    }

    return Status;
}
//}}}  
//{{{  ReadPictureHeaderSimpleMainProfile
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a picture header
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoWmv_c::ReadPictureHeaderSimpleMainProfile (void)
{
    FrameParserStatus_t         Status;
    Vc1VideoPicture_t          *Header;
    Vc1VideoSequence_t         *SequenceHeader;
    Vc1VideoEntryPoint_t*       EntryPointHeader;

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent || !StreamParameters->EntryPointHeaderPresent)
    {
        report (severity_error, "FrameParser_VideoWmv_c::ReadPictureHeader - Sequence header not found.\n");
        return FrameParserNoStreamParameters;
    }

    if (FrameParameters == NULL)
    {
        Status  = GetNewFrameParameters ((void **)&FrameParameters);
        if (Status != FrameParserNoError)
        return Status;
    }

    Header                              = &FrameParameters->PictureHeader;
    memset (Header, 0x00, sizeof(Vc1VideoPicture_t));

    SequenceHeader                      = &StreamParameters->SequenceHeader;
    EntryPointHeader                    = &StreamParameters->EntryPointHeader;

    Header->first_field                         = 1;

    if (SequenceHeader->finterpflag == 1)
        Header->interpfrm                       = Bits.Get(1);

    Header->frmcnt                              = Bits.Get(2);

    if (SequenceHeader->rangered == 1)
        Header->rangeredfrm                     = Bits.Get(1);

    if (SequenceHeader->maxbframes == 0)
    {
        if (Bits.Get(1) == 0x01)
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_P;
        else
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_I;
    }
    else
    {
        if (Bits.Get(1) == 0x01)
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_P;
        else if (Bits.Get(1) == 0x01)
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_I;
        else
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_B;
    }

    if (Header->ptype == VC1_PICTURE_CODING_TYPE_B)
    {
        unsigned int    BFraction               = Bits.Get(3);                          // see 7.1.1.14
        if (BFraction == 0x07)                                                          // 111b
            BFraction                          += Bits.Get(4);
        //if (BFraction == 0x7f)                                                          // 1111111b
        if (BFraction == 22)                                                            // = 111b + 1111b
        {
            report (severity_info, "%s: BI frame\n", __FUNCTION__);
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_BI;
        }
        else
        {
            Header->bfraction_numerator         = BFractionNumerator[BFraction];
            Header->bfraction_denominator       = BFractionDenominator[BFraction];
        }
    }

    if ((Header->ptype == VC1_PICTURE_CODING_TYPE_B) || (Header->ptype == VC1_PICTURE_CODING_TYPE_BI))
        Header->rangeredfrm                     = RangeReduction;                       // B frame is same as previous P frame

    RangeReduction                              = Header->rangeredfrm;                  // Preserve for future B frames

    if ((Header->ptype == VC1_PICTURE_CODING_TYPE_I) || (Header->ptype == VC1_PICTURE_CODING_TYPE_BI))
        Header->bf                              = Bits.Get(7);

    Header->pqindex                             = Bits.Get(5);
    if (Header->pqindex <= 8)
        Header->halfqp                          = Bits.Get(1);

    if (EntryPointHeader->quantizer == 0x00)
    {
        Header->pquant                          = Pquant[Header->pqindex];
        Header->pquantizer                      = (Header->pqindex <= 8) ? 1 : 0;
    }
    else
    {
        Header->pquant                          = Header->pqindex;
        if (EntryPointHeader->quantizer == 0x01)                                    // 7.1.1.8
            Header->pquantizer                  = Bits.Get(1);
    }

    if (EntryPointHeader->extended_mv == 0x01)
    {
        Header->mvrange                         = BitsDotGetVc1VLC(3, VC1_VLC_LEAF_ZERO);
        Header->mvrange                         = VC1_VLC_RESULT(3, Header->mvrange);
    }

    if ((SequenceHeader->multires == 0x01) &&
        ((Header->ptype == VC1_PICTURE_CODING_TYPE_I) || (Header->ptype == VC1_PICTURE_CODING_TYPE_P)))
        Header->respic                         = Bits.Get(2);

    if (Header->ptype == VC1_PICTURE_CODING_TYPE_P)
    {
        for (Header->mvmode = 0; Header->mvmode < 4; Header->mvmode++)
            if (Bits.Get(1) == 0x01)                                                            // MVMODE
                break;
        Header->mvmode                          = (Header->pquant > 12) ? MvModeLowRate[Header->mvmode] : MvModeHighRate[Header->mvmode];

        if (Header->mvmode == VC1_MV_MODE_INTENSITY_COMP)
        {
            unsigned int LumaScale;
            unsigned int LumaShift;

            for (Header->mvmode2 = 0; Header->mvmode2 < 3; Header->mvmode2++)
                if (Bits.Get(1) == 0x01)                                                        // MVMODE2
                    break;
            Header->mvmode2                     = (Header->pquant > 12) ? MvMode2LowRate[Header->mvmode2] : MvMode2HighRate[Header->mvmode2];

            LumaScale                           = Bits.Get(6);
            LumaShift                           = Bits.Get(6);
            Header->intensity_comp_top          = (LumaScale << VC1_LUMASCALE_SHIFT) | (LumaShift << VC1_LUMASHIFT_SHIFT);
            Header->intensity_comp_bottom       = (LumaScale << VC1_LUMASCALE_SHIFT) | (LumaShift << VC1_LUMASHIFT_SHIFT);
            Header->intensity_comp_field        = VC1_INTENSITY_COMP_BOTH;
        }
        if ((Header->mvmode == VC1_MV_MODE_MV_MIXED) ||
            ((Header->mvmode == VC1_MV_MODE_INTENSITY_COMP) && (Header->mvmode2 == VC1_MV_MODE_MV_MIXED)))
        {
                // bitplane decoding aargh
        }
    }
    else if (Header->ptype == VC1_PICTURE_CODING_TYPE_B)
        Header->mvmode                          = Bits.Get(1);                                  // MVMODE

    // set rndctrl - (see 8.3.7)
    if (Header->ptype == VC1_PICTURE_CODING_TYPE_P)
    {
        RoundingControl                         = 1 - RoundingControl;
        Header->rndctrl                         = RoundingControl;
    }
    else if (Header->ptype != VC1_PICTURE_CODING_TYPE_B)
    {
        RoundingControl                         = 1;
        Header->rndctrl                         = RoundingControl;
    }

    FrameParameters->PictureHeaderPresent   = true;


#ifdef DUMP_HEADERS
    {
        report (severity_note, "Picture header (%d):- \n", PictureNo++);
        report (severity_info, "    interpfrm     : %6d\n", Header->interpfrm);
        report (severity_info, "    frmcnt        : %6d\n", Header->frmcnt);
        report (severity_info, "    rangeredfrm   : %6d\n", Header->rangeredfrm);
        report (severity_info, "    ptype         : %6d\n", Header->ptype);
        report (severity_info, "    bfraction_num : %6d\n", Header->bfraction_numerator);
        report (severity_info, "    bfraction_den : %6d\n", Header->bfraction_denominator);
        report (severity_info, "    rndctrl       : %6d\n", Header->rndctrl);
        report (severity_info, "    bf            : %6d\n", Header->bf);
        report (severity_info, "    pqindex       : %6d\n", Header->pqindex);
        report (severity_info, "    halfqp        : %6d\n", Header->halfqp);
        report (severity_info, "    pquant        : %6d\n", Header->pquant);
        report (severity_info, "    pquantizer    : %6d\n", Header->pquantizer);
        report (severity_info, "    mvrange       : %6d\n", Header->mvrange);
        report (severity_info, "    respic        : %6d\n", Header->respic);
        report (severity_info, "    mvmode        : %6d\n", Header->mvmode);
        report (severity_info, "    mvmode2       : %6d\n", Header->mvmode2);
        report (severity_info, "    IntComp.Top   : %6d\n", Header->intensity_comp_top);
        report (severity_info, "    IntComp.Bottom: %6d\n", Header->intensity_comp_bottom);
    }
#endif

     return FrameParserNoError;
}
//}}}  
