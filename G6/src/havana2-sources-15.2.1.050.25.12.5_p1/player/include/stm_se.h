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

#ifndef STM_SE_H_
#define STM_SE_H_

// If you need to include this file from C++, should really be including either
// #include "player.h" or "encoder.h" if you are within the Streaming Engine.
// From userspace, you may need to #include <stdint.h> before this file.

#include <stm_registry.h>

#ifdef __cplusplus
extern "C" {
#endif

// internal.h includes the definition of streaming engine data interfaces.
// these interfaces can be used from other sub-systems without causing a
// runtime linkage to the streaming engine (i.e. streaming engine kernel
// module need not be loaded). These sub-systems may include this header
// directly to avoid accidental usage of other streaming engine symbols.
// For this reason internal.h should appear first in the list of includes
// in order to prove it has no dependencies on other SE headers).

#include <stm_se/internal.h> // do not move (see above)
#include <stm_se/types.h>
#include <stm_se/controls.h>
#include <stm_se/streaming_engine.h>
#include <stm_se/playback.h>
#include <stm_se/play_stream.h>
#include <stm_se/encode.h>
#include <stm_se/encode_stream.h>
#include <stm_se/audio_player.h>
#include <stm_se/audio_mixer.h>
#include <stm_se/audio_reader.h>
#include <stm_se/audio_generator.h>
#include <stm_se/controls_pseudomixer.h>
#include <stm_se/deprecated.h>

#ifdef __cplusplus
}
#endif

#endif /* STM_SE_H_ */
