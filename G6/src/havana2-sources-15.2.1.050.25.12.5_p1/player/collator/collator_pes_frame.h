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

#ifndef H_COLLATOR_PES_FRAME
#define H_COLLATOR_PES_FRAME

#include "collator_pes.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator_PesFrame_c"

class Collator_PesFrame_c : public Collator_Pes_c
{
public:
    Collator_PesFrame_c(void);

    CollatorStatus_t    InternalFrameFlush(bool FlushedByStreamTerminate);
    CollatorStatus_t    InternalFrameFlush(void);

private:
    virtual CollatorStatus_t   DoInput(PlayerInputDescriptor_t *Input,
                                       unsigned int             DataLength,
                                       void                    *Data,
                                       bool                     NonBlocking,
                                       unsigned int            *DataLengthRemaining);

    int         RemainingDataLength;
    int         FrameSize;
};

#endif /* H_COLLATOR_PES_FRAME */
