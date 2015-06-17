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

#ifndef H_ENCODE_COORDINATOR_INTERFACE
#define H_ENCODE_COORDINATOR_INTERFACE

#include "release_buffer_interface.h"
#include "encode_stream_interface.h"
#include "encoder_types.h"
#include "port.h"

class EncodeCoordinatorInterface_c
{
public:
    // Second phase constructor
    virtual EncoderStatus_t FinalizeInit() = 0;

    // Halt method, to be called before destructor
    // Blocking until thread termination
    virtual void Halt() = 0;

    // virtual destructor
    virtual ~EncodeCoordinatorInterface_c() {}

    // Connect an encode stream to the encode coordinator
    // Returns in InputPort a pointer to the encode coordinator input port to be used for this stream
    virtual EncoderStatus_t Connect(EncodeStreamInterface_c  *EncodeStream,
                                    ReleaseBufferInterface_c *OriginalReleaseBufferItf,
                                    ReleaseBufferInterface_c **EncodeCoordinatorStreamReleaseBufferItf,
                                    Port_c **InputPort) = 0;

    // Disconnects an encode stream from the encode coordinator
    // after this call, pushing buffers to the encode coordinator input port
    // corresponding to this stream is not allowed anymore
    virtual EncoderStatus_t Disconnect(EncodeStreamInterface_c *EncodeStream) = 0;

    // Ask to flush remaining buffers store in the EncodeCoordinator for the specified EncodeStream
    virtual EncoderStatus_t Flush(EncodeStreamInterface_c *EncodeStream) = 0;


    // Signals the encode coordinator thread that one of the streams has received a new decoded frame
    // in its input ring
    virtual void SignalNewStreamInput() = 0;

    // Gives the constant PTS offset to be applied to all streams by the encode coordinator
    // This offset will be used to set the difference between encoded_time and native_time in
    // the stm_se_compressed_frame_metadata_t struct of each encoded frame.
    virtual int64_t GetPtsOffset() = 0;
};

#endif // H_ENCODE_COORDINATOR_INTERFACE
