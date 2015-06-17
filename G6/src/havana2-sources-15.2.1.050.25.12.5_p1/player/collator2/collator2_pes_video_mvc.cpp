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
/// \class Collator2_PesVideoMVC_c
///
/// Implements initialisation of collator video class for MVC
/// Note: MVC collating is assisted by the Frame Parser (detects first slice of base picture)
///

#include "h264.h"
#include "collator2_pes_video_mvc.h"

//
Collator2_PesVideoMVC_c::Collator2_PesVideoMVC_c(void)
    : Collator2_PesVideo_c()
{
    Configuration.CollatorName               = "MVC Collator";
    Configuration.GenerateStartCodeList      = true;
    Configuration.MaxStartCodes              = 1000;  // 32 SPS + 32 subset-SPS + 256 PPS + (340 slices x 2 views)
}

