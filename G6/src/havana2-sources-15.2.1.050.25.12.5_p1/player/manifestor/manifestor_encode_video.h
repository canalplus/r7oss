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
#ifndef MANIFESTOR_ENCODE_VIDEO_H
#define MANIFESTOR_ENCODE_VIDEO_H
#include "player.h"
#include "metadata_helper.h"
#include "manifestor_encode.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_EncodeVideo_c"

/// Video manifestor based on the stgfb core driver API.
class Manifestor_EncodeVideo_c : public Manifestor_Encoder_c
{
public:
    Manifestor_EncodeVideo_c(void);
    ~Manifestor_EncodeVideo_c(void);

protected:
    // Called from the Encode Manifestor. Create an encode input buffer from decoded buffer
    ManifestorStatus_t  PrepareEncodeMetaData(Buffer_t  Buffer, Buffer_t  *EncodeBuffer);

private:
    void setUncompressedMetadata(class Buffer_c    *Buffer,
                                 stm_se_uncompressed_frame_metadata_t *Meta,
                                 struct ParsedVideoParameters_s       *VideoParameters)
    {
        // Fill video uncompressed metadata info
        // Request window_of_interest to be filled-in with parsed parameters when available (e.g. pan-scan)
        VideoMetadataHelper.setVideoUncompressedMetadata(PrimaryManifestationComponent,
                                                         Stream->GetDecodeBufferManager(), Buffer, Meta, VideoParameters, true);
    };

    BaseMetadataHelper_c VideoMetadataHelper;
};

#endif
