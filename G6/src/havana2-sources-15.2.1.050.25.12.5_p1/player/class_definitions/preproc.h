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

#ifndef PREPROC_H
#define PREPROC_H

#include "encoder.h"

enum
{
    PreprocNoError            = PlayerNoError,
    PreprocError              = PlayerError,
    PreprocUnsupportedControl = EncoderUnsupportedControl, // returned by set/get controls when it is not supported yet
    PreprocControlNotMatch    = EncoderControlNotMatch, // returned by set/get controls when it does not apply to the current component
};

typedef EncoderStatus_t PreprocStatus_t;

enum
{
    PreprocFnRegisterOutputBufferRing   = BASE_PREPROC,
    PreprocFnReleasePreprocBuffer,
    PreprocFnInput,

    PreprocFnSetModuleParameters
};

class Preproc_c  : public BaseComponentClass_c
{
public:
    virtual PreprocStatus_t   ManageMemoryProfile(void) = 0;

    virtual PreprocStatus_t   RegisterOutputBufferRing(Ring_t             Ring) = 0;

    virtual PreprocStatus_t   RegisterBufferManager(BufferManager_t       BufferManager) = 0;

    virtual PreprocStatus_t   OutputPartialPreprocBuffers(void) = 0;

    virtual PreprocStatus_t   Input(Buffer_t          Buffer) = 0;

    virtual PreprocStatus_t   AbortBlockingCalls(void) = 0;

    //
    // Managing Controls
    //

    virtual PreprocStatus_t   GetControl(stm_se_ctrl_t     Control,
                                         void             *Data) = 0;

    virtual PreprocStatus_t   SetControl(stm_se_ctrl_t     Control,
                                         const void       *Data) = 0;

    virtual PreprocStatus_t   GetCompoundControl(stm_se_ctrl_t     Control,
                                                 void             *Data) = 0;

    virtual PreprocStatus_t   SetCompoundControl(stm_se_ctrl_t     Control,
                                                 const void       *Data) = 0;

    //
    // Managing Discontinuity
    //

    virtual PreprocStatus_t   InjectDiscontinuity(stm_se_discontinuity_t    Discontinuity) = 0;


    //
    // Low power functions
    //

    virtual PreprocStatus_t   LowPowerEnter(void) = 0;

    virtual PreprocStatus_t   LowPowerExit(void) = 0;

    // audio specific request: audio decoder => audio manifestor => audio preproc
    // enum eAccAcMode not explicited to avoid header dep
    virtual void              GetChannelConfiguration(int64_t *AcMode) = 0;
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Preproc_c
\brief Responsible for taking raw frames and applying any required preprocessing.

The preproc class is responsible for taking individual frames and
applying any processing prior to being passed to the encoder

For example,
In terms of audio, this may or may not include SRC and volume adjust
In terms of video, this may or may not include FRC, de-interlacing and scaling.
*/

/*! \fn PreprocStatus_t Preproc_c::RegisterOutputBufferRing(Ring_t Ring)
\brief Register a ring on which processed frames are to be placed.

\param Ring Pointer to a Ring_c instance

\return Preproc status code, PreprocNoError indicates success.
*/

/*! \fn PreprocStatus_t Preproc_c::OutputPartialPreprocBuffers()
\brief Request for incompletely processed buffers to be immediately output.

Passes onto the output ring any preproc buffers that are partially
filled, this includes buffers with only one field, or a number
of slices. In the event that several slices have been queued but not
decoded, they should be decoded and the relevant buffer passed on.

\return Preproc status code, PreprocNoError indicates success.
*/

/*! \fn PreprocStatus_t Preproc_c::Input(Buffer_t Buffer)
\brief Accept raw frames of data for preprocessing.

Provide an input frame for a preprocessor job request

\param Buffer A pointer to a Buffer_c instance of an unencoded frame

\return Preproc status code, PreprocNoError indicates success.
*/

#endif /* PREPROC_H */
