/************************************************************************
Copyright (C) 2003-2013 STMicroelectronics. All Rights Reserved.

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
#ifndef ENCODE_STREAM_INTERFACE_H
#define ENCODE_STREAM_INTERFACE_H

#include "base_component_class.h"
#include "encoder_types.h"

class EncodeStreamInterface_c : public BaseComponentClass_c
{
public:
    virtual ~EncodeStreamInterface_c() {}

    virtual Port_c         *GetInputPort() = 0;

    virtual Encoder_c      *GetEncoderObject() = 0;
};

#endif /* ENCODE_STREAM_INTERFACE_H */
