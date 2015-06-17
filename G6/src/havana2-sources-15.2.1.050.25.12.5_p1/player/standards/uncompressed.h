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

#ifndef __UNCOMPRESSED_H
#define __UNCOMPRESSED_H

#include "player.h"

#define UNCOMPRESSED_COLOUR_MODE_DEFAULT     0
#define UNCOMPRESSED_COLOUR_MODE_601         1
#define UNCOMPRESSED_COLOUR_MODE_709         2

//

typedef struct UncompressedVideoParameters_s
{
    uint32_t       Width;
    uint32_t       Height;
    Rational_t     PixelAspectRatio;
    Rational_t     FrameRate;
    bool           Interlaced;
    bool           TopFieldFirst;
    uint32_t       ColourMode;
    bool           VideoFullRange;

    UncompressedVideoParameters_s(void)
        : Width(0)
        , Height(0)
        , PixelAspectRatio()
        , FrameRate()
        , Interlaced(false)
        , TopFieldFirst(false)
        , ColourMode(0)
        , VideoFullRange(false)
    {}
} UncompressedVideoParameters_t;

typedef struct UncompressedBufferDesc_s
{
    UncompressedVideoParameters_t Content;
    void                          *Buffer;
    void                          *BufferClass;

    UncompressedBufferDesc_s(void)
        : Content()
        , Buffer(NULL)
        , BufferClass(NULL)
    {}
} UncompressedBufferDesc_t;

#endif // __UNCOMPRESSED_H
