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
#ifndef CODER_VIDEO_H
#define CODER_VIDEO_H

#include "coder_base.h"

class Coder_Video_c : public Coder_Base_c
{
public:
    Coder_Video_c(void);
    ~Coder_Video_c(void);

    CoderStatus_t   Halt(void);

    CoderStatus_t   Input(Buffer_t  Buffer);

protected:
    const static struct stm_se_picture_resolution_s ProfileSize[];

    void RestrictFramerateTo16Bits(uint32_t *FramerateNum, uint32_t *FramerateDen);

private:
    unsigned int    mRelayfsIndex;
};

#endif /* CODER_VIDEO_H */
