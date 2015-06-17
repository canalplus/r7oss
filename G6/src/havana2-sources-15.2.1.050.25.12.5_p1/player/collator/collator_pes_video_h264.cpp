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
/// \class Collator_PesVideoH264_c
///
/// Implements initialisation of collator video class for H264
///

#include "h264.h"
#include "collator_pes_video_h264.h"

////////////////////////////////////////////////////////////////////////////
///
/// Initialize the class by resetting it.
///
Collator_PesVideoH264_c::Collator_PesVideoH264_c(void)
    : Collator_PesVideo_c()
{
    Configuration.GenerateStartCodeList            = true;
    Configuration.MaxStartCodes                    = 300;  // If someone inserts 32 SPS and 256 PPS
    Configuration.StreamIdentifierMask             = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode             = PES_START_CODE_VIDEO;
    Configuration.BlockTerminateMask               = 0x9b; // Slice normal or IDR
    Configuration.BlockTerminateCode               = 0x01;
    Configuration.IgnoreCodesRanges.NbEntries      = 1;
    Configuration.IgnoreCodesRanges.Table[0].Start = 0xff; // Ignore nothing
    Configuration.IgnoreCodesRanges.Table[0].End   = 0x00;
    Configuration.InsertFrameTerminateCode         = true; // Insert a filler data code, to guarantee thatNo terminal code
    Configuration.TerminalCode                     = 0x0C; // picture parameter sets will always be followed by a zero byte (makes MoreRsbpData implementation simpler)
    Configuration.ExtendedHeaderLength             = 0;
    Configuration.DeferredTerminateFlag            = false;
    Configuration.DetermineFrameBoundariesByPresentationToFrameParser = true;
}

