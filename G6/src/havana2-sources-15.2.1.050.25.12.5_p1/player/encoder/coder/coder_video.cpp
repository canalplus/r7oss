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

#include "coder_video.h"
#include "st_relayfs_se.h"

#undef TRACE_TAG
#define TRACE_TAG "Coder_Video_c"

const struct stm_se_picture_resolution_s Coder_Video_c::ProfileSize[] =
{
    {  352,  288 }, // EncodeProfileCIF,
    {  720,  576 }, // EncodeProfileSD,
    { 1280,  720 }, // EncodeProfile720p,
    { 1920, 1088 }, // EncodeProfileHD,
};

Coder_Video_c::Coder_Video_c(void)
    : Coder_Base_c()
    , mRelayfsIndex(0)
{
    SetGroupTrace(group_encoder_video_coder);
    mRelayfsIndex = st_relayfs_getindex_fortype_se(ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT);
}

Coder_Video_c::~Coder_Video_c(void)
{
    st_relayfs_freeindex_fortype_se(ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT, mRelayfsIndex);
}

CoderStatus_t   Coder_Video_c::Halt(void)
{
    return Coder_Base_c::Halt();
}

CoderStatus_t Coder_Video_c::Input(Buffer_t   Buffer)
{
    SE_DEBUG(group_encoder_video_coder, "\n");

    // Dump coder input buffer via st_relay
    Buffer->DumpViaRelay(ST_RELAY_TYPE_ENCODER_VIDEO_CODER_INPUT + mRelayfsIndex, ST_RELAY_SOURCE_SE);

    // First call the base class
    CoderStatus_t Status = Coder_Base_c::Input(Buffer);
    if (Status != CoderNoError)
    {
        CODER_ERROR_RUNNING("Failed to call base class, Status = %u\n", Status);
        return Status;
    }

    return Status;
}

// /////////////////////////////////////////////////////////////////////////
//
//  The RestrictFramerateTo16Bits function restricts framerate to 16 bits
//  The code was referenced from Rational_c class
//
void Coder_Video_c::RestrictFramerateTo16Bits(uint32_t *FramerateNum, uint32_t *FramerateDen)
{
    uint32_t tmp;
    uint32_t shift = 0;
    uint32_t spare;
    uint32_t new_denominator;
    uint32_t denominator, numerator;
    denominator = *FramerateNum;
    numerator   = *FramerateDen;
    tmp = (denominator | numerator) >> 16;

    if (tmp)
    {
        shift += (tmp & 0xff00) ? 8 : 0;
        shift += ((tmp >> shift) & 0x00f0) ? 4 : 0;
        shift += ((tmp >> shift) & 0x000c) ? 2 : 0;
        shift += ((tmp >> shift) & 0x0002) ? 1 : 0;
        /* We know how much we are going to shift,
         * however we have a problem if the denominator
         * is small and the numerator is large, we may
         * well seriously damage the accuracy, so, if it
         * is possible we scale both numerator and the
         * denominator so that the shift will not hurt the
         * accuracy */
        spare = 15 - shift;

        if (denominator < (1U << spare))
        {
            new_denominator = denominator & (0xffff << (shift + 1));
            numerator = (numerator * new_denominator) / denominator;
            denominator = new_denominator;
        }

        numerator = (numerator + (1U << shift)) >> (shift + 1);
        denominator = (denominator + (1U << shift)) >> (shift + 1);
    }

    /* Some special easy cases
     * The first two are when one of the denominator, or
     * numerator is a multiple of the other, the third
     * involves the use of factors of 10 */
    if ((denominator != 1) && (numerator != 1) && (denominator != 0))
    {
        if ((denominator * (numerator / denominator)) == numerator)
        {
            numerator = numerator / denominator;
            denominator = 1;
        }
        else if ((numerator * (denominator / numerator)) == denominator)
        {
            denominator = denominator / numerator;
            numerator = numerator / numerator;
        }
        else while (((10 * (numerator / 10)) == numerator) &&
                        ((10 * (denominator / 10)) == denominator))
            {
                numerator = numerator / 10;
                denominator = denominator / 10;
            }
    }

    if (0 == denominator)
    {
        SE_INFO(group_encoder_video_coder, "got denominator 0; forcing 1\n");
        denominator = 1;
    }

    *FramerateNum = denominator;
    *FramerateDen = numerator;
}
