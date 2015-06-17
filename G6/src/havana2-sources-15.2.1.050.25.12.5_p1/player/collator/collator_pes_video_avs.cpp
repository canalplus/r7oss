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
/// \class Collator_PesVideoAvs_c
///
/// Implements initialisation of collator video class for AVS
///

#include "mpeg2.h"
#include "avs.h"
#include "collator_pes_video_avs.h"

//
Collator_PesVideoAvs_c::Collator_PesVideoAvs_c(void)
    : Collator_PesVideo_c()
{
    Configuration.GenerateStartCodeList            = true;
    Configuration.MaxStartCodes                    = 256;
    Configuration.StreamIdentifierMask             = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode             = PES_START_CODE_VIDEO;
    Configuration.BlockTerminateMask               = 0xFA;                            // Picture
    Configuration.BlockTerminateCode               = 0xB2;
    Configuration.IgnoreCodesRanges.NbEntries      = 1;
    Configuration.IgnoreCodesRanges.Table[0].Start = MPEG2_FIRST_SLICE_START_CODE + 1; // Slice codes other than first
    Configuration.IgnoreCodesRanges.Table[0].End   = MPEG2_FIRST_SLICE_START_CODE;
    Configuration.InsertFrameTerminateCode         = true;                            // Force the mme decode to terminate after a picture
    Configuration.TerminalCode                     = AVS_VIDEO_SEQUENCE_END_CODE;
    Configuration.ExtendedHeaderLength             = 0;
    Configuration.DeferredTerminateFlag            = false;
    Configuration.StreamTerminateFlushesFrame      = true;         // Use an end of sequence to force a frame flush
    Configuration.StreamTerminationCode            = AVS_VIDEO_SEQUENCE_END_CODE;
}

