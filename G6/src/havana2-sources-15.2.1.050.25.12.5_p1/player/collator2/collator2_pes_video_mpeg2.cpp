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
/// \class Collator2_PesVideoMpeg2_c
///
/// Implements initialisation of collator video class for mpeg2
///

#include "mpeg2.h"
#include "collator2_pes_video_mpeg2.h"

///
Collator2_PesVideoMpeg2_c::Collator2_PesVideoMpeg2_c(void)
    : Collator2_PesVideo_c()
{
    Configuration.CollatorName               = "MPEG2 Collator";
    Configuration.GenerateStartCodeList      = true;
    Configuration.MaxStartCodes              = 256;
    Configuration.StreamIdentifierMask       = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode       = PES_START_CODE_VIDEO;
    Configuration.IgnoreCodesRangeStart      = MPEG2_FIRST_SLICE_START_CODE + 1; // Slice codes other than first
    Configuration.IgnoreCodesRangeEnd        = MPEG2_GREATEST_SLICE_START_CODE;
    Configuration.InsertFrameTerminateCode   = true;                            // Force the mme decode to terminate after a picture
    Configuration.TerminalCode               = MPEG2_SEQUENCE_END_CODE;
    Configuration.ExtendedHeaderLength       = 0;
}

