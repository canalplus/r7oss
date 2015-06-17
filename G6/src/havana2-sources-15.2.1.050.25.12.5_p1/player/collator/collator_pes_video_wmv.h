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

#ifndef H_COLLATOR_PES_VIDEO_WMV
#define H_COLLATOR_PES_VIDEO_WMV

#include "collator_pes_frame.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator_PesVideoWmv_c"

/*
The wmv collator is a frame collator as Wmv streams
do not contain start codes so frame length is passed from the container parser
in a private structure. This is passed into the player using the privade data
structure in the pes header.
*/

class Collator_PesVideoWmv_c : public Collator_PesFrame_c
{
public:
    Collator_PesVideoWmv_c(void) : Collator_PesFrame_c()
    {
        SetGroupTrace(group_collator_video);

        Configuration.GenerateStartCodeList = true;
        Configuration.StreamIdentifierMask  = 0xff;
        Configuration.StreamIdentifierCode  = 0xfd;
    }
};

#endif /* H_COLLATOR_PES_VIDEO_WMV*/
