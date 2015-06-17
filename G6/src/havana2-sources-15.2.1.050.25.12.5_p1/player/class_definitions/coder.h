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

#ifndef CODER_H
#define CODER_H

#include "encoder.h"

enum
{
    CoderNoError                = EncoderNoError,
    CoderError                  = EncoderError,
    CoderNotSupported           = EncoderNotSupported,
    CoderUnsupportedControl     = EncoderUnsupportedControl, // returned by set/get controls when it is not supported yet
    CoderControlNotMatch        = EncoderControlNotMatch, // returned by set/get controls when it does not apply to the current component

    CoderGenericBufferError     = BASE_CODER,
    CoderInvalidInputBufferReference,
    CoderUnsupportedParameters,
    CoderResourceAllocationFailure,
    CoderResourceError,
    CoderAssertLevelError
};

typedef EncoderStatus_t CoderStatus_t;

enum
{
    CoderFnRegisterOutputBufferRing = BASE_CODER,
    CoderFnReleaseCoderBuffer,
    CoderFnInput,

    CoderFnSetModuleParameters
};

class Coder_c  : public BaseComponentClass_c
{
public:
    virtual CoderStatus_t   RegisterOutputBufferRing(Ring_t     Ring) = 0;

    virtual CoderStatus_t   Input(Buffer_t  Buffer) = 0;

    virtual CoderStatus_t   GetBufferPoolDescriptor(Buffer_t    Buffer) = 0;

    virtual CoderStatus_t   ManageMemoryProfile(void) = 0;

    virtual CoderStatus_t   RegisterBufferManager(BufferManager_t   BufferManager) = 0;

    virtual CoderStatus_t   InitializeCoder(void) = 0;

    virtual CoderStatus_t   TerminateCoder(void) = 0;

    //
    // Managing Controls
    //

    virtual CoderStatus_t   GetControl(stm_se_ctrl_t    Control,
                                       void            *Data) = 0;

    virtual CoderStatus_t   SetControl(stm_se_ctrl_t    Control,
                                       const void      *Data) = 0;

    virtual CoderStatus_t   GetCompoundControl(stm_se_ctrl_t    Control,
                                               void            *Data) = 0;

    virtual CoderStatus_t   SetCompoundControl(stm_se_ctrl_t    Control,
                                               const void      *Data) = 0;

    //
    // Low power functions
    //

    virtual CoderStatus_t   LowPowerEnter(void) = 0;

    virtual CoderStatus_t   LowPowerExit(void) = 0;
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Coder_c
\brief Responsible for taking raw frames and encoding them.

The coder class is responsible for taking individual frames and
encoding them into the designated format before being sent for multiplexing
*/

/*! \fn CoderStatus_t Coder_c::RegisterOutputBufferRing(Ring_t Ring)
\brief Register a ring on which encoded frames are to be placed.

\param Ring Pointer to a Ring_c instance

\return Coder status code, CoderNoError indicates success.
*/

/*! \fn CoderStatus_t Coder_c::Input(Buffer_t Buffer)
\brief Accept raw frame data for encode.

Provide an input frame to be encoded as part of this stream

\param Buffer A pointer to a Buffer_c instance of an unencoded frame

\return Coder status code, CoderNoError indicates success.
*/

/*! \fn CoderStatus_t Coder_c::GetBufferPoolDescriptor(Buffer_t Buffer)
\brief Provide details of the output buffers defined by the stream encoder capabilities

The coder object must define the output buffers which encapsulate the
information pertaining to any hardware requirements such as width and height

\param Buffer A pointer to a Buffer_c instance of an unencoded frame

\return Coder status code, CoderNoError indicates success.
*/

#endif /* CODER_H */
