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

#include "transporter_null.h"

#undef TRACE_TAG
#define TRACE_TAG "Transporter_Null_c"

TransporterStatus_t Transporter_Null_c::Input(Buffer_t    Buffer)
{
    SE_DEBUG(group_encoder_transporter, "\n");

    // Call the Base Class implementation for common processing
    TransporterStatus_t TransporterStatus = Transporter_Base_c::Input(Buffer);
    if (TransporterStatus != TransporterNoError)
    {
        SE_ERROR("Error returned by base class Input method\n");
        return TransporterStatus;
    }

    return TransporterNoError;
}
