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

#ifndef MANIFESTOR_AUDIO_SOURCE_H
#define MANIFESTOR_AUDIO_SOURCE_H

#include "osinline.h"
#include "manifestor_audio.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "allocinline.h"
#include "manifestor_sourceGrab.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_AudioSrc_c"

#define AUDIO_GRAB_DEFAULT_BITSPERSAMPLE          16
#define AUDIO_GRAB_DEFAULT_CHANNELCOUNT            2
#define AUDIO_GRAB_DEFAULT_SAMPLERATEHZ     44000000

typedef enum
{
    AudioGrabStopped = 0,
    AudioGrabMuting,
    AudioGrabPlaying
} ManifestorAudioGrabStatus_t;

/// Audio manifestor based on the grab driver API.
class Manifestor_AudioSrc_c : public Manifestor_Source_c
{
protected:
    ManifestorAudioGrabStatus_t  mStatus;
    bool                         mMute;

public:
    /* Constructor / Destructor */
    Manifestor_AudioSrc_c(void);
    ~Manifestor_AudioSrc_c(void);

// control the grab (mute , play...)
    ManifestorStatus_t  SetModuleParameters(unsigned int   ParameterBlockSize,
                                            void          *ParameterBlock);

// retrieve the channel mapping of the decoded samples
    ManifestorStatus_t  GetChannelConfiguration(enum eAccAcMode *AcMode);

// override Manifestor_base_c class
    ManifestorStatus_t   GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces);

// Method for MemorySink Pull access
    uint32_t        PullFrameRead(uint8_t *captureBufferAddr, uint32_t captureBufferLen);

// specific Init/term connection
    ManifestorStatus_t  initialiseConnection(void);
    ManifestorStatus_t  terminateConnection(void);

// override Manifestor_Source_c
    stm_se_encode_stream_media_t GetMediaEncode() const { return STM_SE_ENCODE_STREAM_MEDIA_AUDIO; };

// method for fading in-out 32bit-sample decoded buffers when coming out / in of mute phase
    uint32_t                     Fade32(int32_t *frame_p, int32_t *capture_p, struct ParsedAudioParameters_s  *AudioParameters, int32_t coeff, uint32_t step);

};

#endif
