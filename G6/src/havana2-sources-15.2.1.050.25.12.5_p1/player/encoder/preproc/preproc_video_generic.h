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
#ifndef PREPROC_VIDEO_GENERIC_H
#define PREPROC_VIDEO_GENERIC_H

#include "preproc_video.h"
#include "preproc_video_scaler.h"
#include "preproc_video_blitter.h"

typedef class Preproc_Video_c *Preproc_Video_t;

class Preproc_Video_Generic_c : public Preproc_Video_c
{
public:
    Preproc_Video_Generic_c(void);
    ~Preproc_Video_Generic_c(void);

    PreprocStatus_t   Halt(void);

    PreprocStatus_t   RegisterOutputBufferRing(Ring_t   Ring);

    PreprocStatus_t   Input(Buffer_t        Buffer);

    PreprocStatus_t   GetControl(stm_se_ctrl_t   Control,
                                 void           *Data);
    PreprocStatus_t   SetControl(stm_se_ctrl_t   Control,
                                 const void     *Data);
    PreprocStatus_t   GetCompoundControl(stm_se_ctrl_t   Control,
                                         void           *Data);
    PreprocStatus_t   SetCompoundControl(stm_se_ctrl_t   Control,
                                         const void     *Data);

    PreprocStatus_t   InjectDiscontinuity(stm_se_discontinuity_t    Discontinuity);

protected:
    Preproc_Video_t   PreprocVideoDelegate;

    OS_Mutex_t        ControlLock;

    Preproc_Video_t   FindPreprocVideo(Buffer_t Buffer);
    bool              AreCapabilitiesMatching(stm_se_uncompressed_frame_metadata_t  *MetaDataDescriptor,
                                              PreprocVideoCaps_t          *Caps);

private:
    DISALLOW_COPY_AND_ASSIGN(Preproc_Video_Generic_c);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Preproc_Video_Generic_c
\brief Video generic implementation of the Preprocessor classes.

This class will function as a starting point for developing a video Preprocessor class,
whereby the blitter or scaler are delegates initialized for actual preprocessing.

*/

#endif /* PREPROC_VIDEO_GENERIC_H */
