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
#ifndef TRANSPORTER_MEMSRC_H
#define TRANSPORTER_MEMSRC_H

#include "osinline.h"
#include <stm_memsrc.h>
#include <stm_memsink.h>
#include <stm_registry.h>

#include "allocinline.h"
#include "player.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "ring_generic.h"
#include "transporter_base.h"

class Transporter_MemSrc_c : public Transporter_Base_c
{
public:
    Transporter_MemSrc_c(void);
    ~Transporter_MemSrc_c(void);

    TransporterStatus_t  Halt(void);

    /// This static function allows us to determine compatibility
    /// before creating a complete object
    static TransporterStatus_t Probe(stm_object_h  Sink);

    TransporterStatus_t  Input(Buffer_t Buffer);
    TransporterStatus_t  CreateConnection(stm_object_h  sink);
    TransporterStatus_t  RemoveConnection(stm_object_h  sink);

    friend int pull_encode_source_se(stm_object_h src_object,
                                     struct stm_data_block *block_list,
                                     uint32_t block_count,
                                     uint32_t *filled_blocks);
    friend int pull_encode_test_for_source_se(stm_object_h src_object, uint32_t *size);

protected:
    // Buffer management
    Buffer_t          CurrentBuffer;
    RingGeneric_c    *CompressedBufferRing;

    // Source Frame buffer info
    void             *FramePhysicalAddress;
    void             *FrameVirtualAddress;
    uint32_t          FrameSize;

    // specific Init/term connection functions
    TransporterStatus_t InitialiseConnection(void);
    TransporterStatus_t TerminateConnection(void);

    TransporterStatus_t PrepareEncodeBuffer(stm_se_compressed_frame_metadata_t *encodeMetadata);

    // Method for MemorySink Pull access
    int32_t         PullFrameRead(uint8_t *MemsinkBufferAddr, uint32_t MemsinkBufferLen);
    int32_t         PullFrameAvailable();
    int32_t         PullFrameWait();
    void            PullFramePostRead();

private:
    // Connection info
    bool                                Connected;
    struct stm_data_interface_pull_sink PullSinkInterface;

    // Only one attachment allowed
    stm_object_h       SinkHandle;

    // Debug traces helper function
    void    TraceRing(const char *ActionStr, Buffer_t Buffer);

    DISALLOW_COPY_AND_ASSIGN(Transporter_MemSrc_c);
};

#endif  /* TRANSPORTER_MEMSRC_H */
