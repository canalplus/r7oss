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

#include "frame_parser_video_wmv.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoWmv_c"

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

#if defined (DUMP_HEADERS)
static unsigned int PictureNo;
#endif


FrameParser_VideoWmv_c::FrameParser_VideoWmv_c(void)
    : FrameParser_VideoVc1_c()
    , RoundingControl(0)
    , RangeReduction(0)
{
    Configuration.FrameParserName  = "VideoWmv";

#if defined (DUMP_HEADERS)
    PictureNo   = 0;
#endif
}

//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoWmv_c::ReadHeaders(void)
{
    FrameParserStatus_t         Status  = FrameParserNoError;
    Bits.SetPointer(BufferData);

    if (!SequenceLayerMetaDataValid)
    {
        Status          = ReadSequenceLayerMetadata();

        if (StreamParameters != NULL)
        {
            StreamParameters->StreamType = Vc1StreamTypeWMV;
        }
    }
    else
    {
        Status          = ReadPictureHeaderSimpleMainProfile();

        if (Status == FrameParserNoError)
        {
            Status      = CommitFrameForDecode();
        }
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
FrameParserStatus_t   FrameParser_VideoWmv_c::ReadPictureHeaderSimpleMainProfile(void)
{
    FrameParserStatus_t         Status;
    Vc1VideoPicture_t          *Header;
    Vc1VideoSequence_t         *SequenceHeader;
    Vc1VideoEntryPoint_t       *EntryPointHeader;

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent || !StreamParameters->EntryPointHeaderPresent)
    {
        SE_ERROR("Sequence header not found\n");
        return FrameParserNoStreamParameters;
    }

    if (FrameParameters == NULL)
    {
        Status  = GetNewFrameParameters((void **)&FrameParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }
    }

    Header                              = &FrameParameters->PictureHeader;
    memset(Header, 0, sizeof(Vc1VideoPicture_t));
    Header->bfraction_denominator = 1;

    SequenceHeader                      = &StreamParameters->SequenceHeader;
    EntryPointHeader                    = &StreamParameters->EntryPointHeader;
    Header->first_field                         = 1;

    if (SequenceHeader->finterpflag == 1)
    {
        Header->interpfrm                       = Bits.Get(1);
    }

    Header->frmcnt                              = Bits.Get(2);

    if (SequenceHeader->rangered == 1)
    {
        Header->rangeredfrm                     = Bits.Get(1);
    }

    if (SequenceHeader->maxbframes == 0)
    {
        if (Bits.Get(1) == 0x01)
        {
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_P;
        }
        else
        {
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_I;
        }
    }
    else
    {
        if (Bits.Get(1) == 0x01)
        {
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_P;
        }
        else if (Bits.Get(1) == 0x01)
        {
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_I;
        }
        else
        {
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_B;
        }
    }

    if (Header->ptype == VC1_PICTURE_CODING_TYPE_B)
    {
        unsigned int    BFraction               = Bits.Get(3);                          // see 7.1.1.14

        if (BFraction == 0x07)                                                          // 111b
        {
            BFraction                          += Bits.Get(4);
        }

        //if (BFraction == 0x7f)                                                          // 1111111b
        if (BFraction == 22)                                                            // = 111b + 1111b
        {
            SE_INFO(group_frameparser_video,  "BI frame\n");
            Header->ptype                       = VC1_PICTURE_CODING_TYPE_BI;
        }
        else
        {
            Header->bfraction_numerator         = BFractionNumerator[BFraction];
            Header->bfraction_denominator       = BFractionDenominator[BFraction];

            // BFractionDenominator has last indexes set to 0
            if (Header->bfraction_denominator == 0)
            {
                SE_INFO(group_frameparser_video, "bfraction_den 0; forcing 8\n");
                Header->bfraction_denominator = 8; // last value of BFractionDenominator
            }
        }
    }

    if ((Header->ptype == VC1_PICTURE_CODING_TYPE_B) || (Header->ptype == VC1_PICTURE_CODING_TYPE_BI))
    {
        Header->rangeredfrm                     = RangeReduction;    // B frame is same as previous P frame
    }

    RangeReduction                              = Header->rangeredfrm;                  // Preserve for future B frames

    if ((Header->ptype == VC1_PICTURE_CODING_TYPE_I) || (Header->ptype == VC1_PICTURE_CODING_TYPE_BI))
    {
        Header->bf                              = Bits.Get(7);
    }

    Header->pqindex                             = Bits.Get(5);

    if (Header->pqindex <= 8)
    {
        Header->halfqp                          = Bits.Get(1);
    }

    if (EntryPointHeader->quantizer == 0x00)
    {
        Header->pquant                          = Pquant[Header->pqindex];
        Header->pquantizer                      = (Header->pqindex <= 8) ? 1 : 0;
    }
    else
    {
        Header->pquant                          = Header->pqindex;

        if (EntryPointHeader->quantizer == 0x01)                                    // 7.1.1.8
        {
            Header->pquantizer                  = Bits.Get(1);
        }
    }

    if (EntryPointHeader->extended_mv == 0x01)
    {
        Header->mvrange                         = BitsDotGetVc1VLC(3, VC1_VLC_LEAF_ZERO);
        Header->mvrange                         = VC1_VLC_RESULT(3, Header->mvrange);
    }

    if ((SequenceHeader->multires == 0x01) &&
        ((Header->ptype == VC1_PICTURE_CODING_TYPE_I) || (Header->ptype == VC1_PICTURE_CODING_TYPE_P)))
    {
        Header->respic                         = Bits.Get(2);
    }

    if (Header->ptype == VC1_PICTURE_CODING_TYPE_P)
    {
        for (Header->mvmode = 0; Header->mvmode < 4; Header->mvmode++)
            if (Bits.Get(1) == 0x01)                                                            // MVMODE
            {
                break;
            }

        Header->mvmode                          = (Header->pquant > 12) ? MvModeLowRate[Header->mvmode] : MvModeHighRate[Header->mvmode];

        if (Header->mvmode == VC1_MV_MODE_INTENSITY_COMP)
        {
            unsigned int LumaScale;
            unsigned int LumaShift;

            for (Header->mvmode2 = 0; Header->mvmode2 < 3; Header->mvmode2++)
                if (Bits.Get(1) == 0x01)                                                        // MVMODE2
                {
                    break;
                }

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
    {
        Header->mvmode                          = Bits.Get(1);    // MVMODE
    }

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
        SE_INFO(group_frameparser_video,  "Picture header (%d):-\n", PictureNo++);
        SE_INFO(group_frameparser_video,  "    interpfrm     : %6d\n", Header->interpfrm);
        SE_INFO(group_frameparser_video,  "    frmcnt        : %6d\n", Header->frmcnt);
        SE_INFO(group_frameparser_video,  "    rangeredfrm   : %6d\n", Header->rangeredfrm);
        SE_INFO(group_frameparser_video,  "    ptype         : %6d\n", Header->ptype);
        SE_INFO(group_frameparser_video,  "    bfraction_num : %6d\n", Header->bfraction_numerator);
        SE_INFO(group_frameparser_video,  "    bfraction_den : %6d\n", Header->bfraction_denominator);
        SE_INFO(group_frameparser_video,  "    rndctrl       : %6d\n", Header->rndctrl);
        SE_INFO(group_frameparser_video,  "    bf            : %6d\n", Header->bf);
        SE_INFO(group_frameparser_video,  "    pqindex       : %6d\n", Header->pqindex);
        SE_INFO(group_frameparser_video,  "    halfqp        : %6d\n", Header->halfqp);
        SE_INFO(group_frameparser_video,  "    pquant        : %6d\n", Header->pquant);
        SE_INFO(group_frameparser_video,  "    pquantizer    : %6d\n", Header->pquantizer);
        SE_INFO(group_frameparser_video,  "    mvrange       : %6d\n", Header->mvrange);
        SE_INFO(group_frameparser_video,  "    respic        : %6d\n", Header->respic);
        SE_INFO(group_frameparser_video,  "    mvmode        : %6d\n", Header->mvmode);
        SE_INFO(group_frameparser_video,  "    mvmode2       : %6d\n", Header->mvmode2);
        SE_INFO(group_frameparser_video,  "    IntComp.Top   : %6d\n", Header->intensity_comp_top);
        SE_INFO(group_frameparser_video,  "    IntComp.Bottom: %6d\n", Header->intensity_comp_bottom);
    }
#endif
    return FrameParserNoError;
}
//}}}
