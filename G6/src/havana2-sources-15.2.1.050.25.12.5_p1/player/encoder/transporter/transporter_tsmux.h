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
#ifndef TRANSPORTER_TSMUX_H
#define TRANSPORTER_TSMUX_H

#include "osinline.h"
#include <stm_te_if_tsmux.h>

#include "ring_generic.h"
#include "transporter_base.h"

class Transporter_TSMux_c : public Transporter_Base_c
{
public:
    Transporter_TSMux_c(void);
    ~Transporter_TSMux_c(void);

    TransporterStatus_t  Halt(void);

    /// This static function allows us to determine compatibility
    /// before creating a complete object
    static TransporterStatus_t Probe(stm_object_h  Sink);

    TransporterStatus_t  CreateConnection(stm_object_h  Sink);
    TransporterStatus_t  RemoveConnection(stm_object_h  Sink);

    TransporterStatus_t  Input(Buffer_t      Buffer);

    // used for stm_te_async_data_interface_src_t *release_buffer implementation: return int
    int                  ReleaseBuffer(struct stm_data_block *block_list,
                                       uint32_t               block_count);

protected:
    stm_object_h                         TSMuxHandle;      //< The Object we will connect to
    RingGeneric_c                       *TSMuxBufferRing;  //< For tracking active buffers

    stm_te_async_data_interface_sink_t   TSMuxInterface;

    // For internal maintenance only
    TransporterStatus_t PurgeBufferRing(void);

private:
    // Debug traces helper function
    void    TraceRing(const char *ActionStr, Buffer_t Buffer);

    DISALLOW_COPY_AND_ASSIGN(Transporter_TSMux_c);
};

#endif // TRANSPORTER_TSMUX_H
