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

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesVideoVc1_c
///
/// Implements initialisation of collator video class for VC1
///

#include "mpeg2.h"
#include "vc1.h"
#include "collator_pes_video_vc1.h"

///
Collator_PesVideoVc1_c::Collator_PesVideoVc1_c(void)
    : Collator_PesVideo_c()
{
    Configuration.GenerateStartCodeList            = true;
    Configuration.MaxStartCodes                    = 256;
    Configuration.StreamIdentifierMask             = 0xff;                            // Video
    Configuration.StreamIdentifierCode             = 0xfd;
    Configuration.SubStreamIdentifierMask          = 0xff;
    Configuration.SubStreamIdentifierCodeStart     = VC1_STREAM_ID_EXTENTION_START;
    Configuration.SubStreamIdentifierCodeStop      = VC1_STREAM_ID_EXTENTION_STOP;
    Configuration.BlockTerminateMask               = 0xfe;                            // allows frame or field
    Configuration.BlockTerminateCode               = VC1_FIELD_START_CODE;
    Configuration.IgnoreCodesRanges.NbEntries      = 1;
    Configuration.IgnoreCodesRanges.Table[0].Start = VC1_SLICE_LEVEL_USER_DATA;
    Configuration.IgnoreCodesRanges.Table[0].End   = VC1_SEQUENCE_LEVEL_USER_DATA;
    Configuration.InsertFrameTerminateCode         = true;                            // Add code to force the mme decode to terminate
    Configuration.TerminalCode                     = VC1_FRAME_START_CODE;            // Configuration.BlockTerminateCode
    Configuration.ExtendedHeaderLength             = 0;
    Configuration.DeferredTerminateFlag            = false;
}

// This is a hack to handle VC1 AP streams with stream id 0xE0(PES_START_CODE_VIDEO) in pes header
unsigned int Collator_PesVideoVc1_c::GetStreamIdentifierCode(unsigned int StreamIdInPesHeader)
{
    return (((StreamIdInPesHeader & PES_START_CODE_MASK) == PES_START_CODE_VIDEO) ? StreamIdInPesHeader : Configuration.StreamIdentifierCode);
}
