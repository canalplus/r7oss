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

#ifndef H_ACTIVE_EDGE_INTERFACE
#define H_ACTIVE_EDGE_INTERFACE

class ActiveEdgeInterface_c
{
public:

    virtual ~ActiveEdgeInterface_c() {}

    virtual OS_Status_t Start() = 0;

    virtual void Stop() = 0;

    virtual PlayerStatus_t   CallInSequence(PlayerSequenceType_t      SequenceType,
                                            PlayerSequenceValue_t     SequenceValue,
                                            PlayerComponentFunction_t Fn,
                                            ...) = 0;

    virtual void DiscardUntilMarkerFrame() = 0;
    virtual bool isDiscardingUntilMarkerFrame() = 0;

    virtual Port_c *GetInputPort() = 0;
    virtual RingStatus_t   Insert(uintptr_t Value) = 0;
};

#endif // H_ACTIVE_EDGE_INTERFACE
