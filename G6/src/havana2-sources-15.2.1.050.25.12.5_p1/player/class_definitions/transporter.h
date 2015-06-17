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

#ifndef TRANSPORTER_H
#define TRANSPORTER_H

#include "encoder.h"

enum
{
    TransporterNoError            = PlayerNoError,
    TransporterError              = PlayerError,
    TransporterUnsupportedControl = EncoderUnsupportedControl, // returned by set/get controls when it is not supported yet
    TransporterControlNotMatch    = EncoderControlNotMatch, // returned by set/get controls when it is not applyied to the current component
};

typedef EncoderStatus_t TransporterStatus_t;

class Transporter_c  : public BaseComponentClass_c
{
public:
    virtual TransporterStatus_t   RegisterBufferManager(BufferManager_t       BufferManager) = 0;

    virtual TransporterStatus_t   Input(Buffer_t    Buffer) = 0;

    virtual TransporterStatus_t   CreateConnection(stm_object_h       sink) = 0;

    virtual TransporterStatus_t   RemoveConnection(stm_object_h       sink) = 0;

    //
    // Managing Controls
    //

    virtual TransporterStatus_t   GetControl(stm_se_ctrl_t Control, void *Data) = 0;

    virtual TransporterStatus_t   SetControl(stm_se_ctrl_t Control, const void *Data) = 0;

    virtual TransporterStatus_t   GetCompoundControl(stm_se_ctrl_t Control, void *Data) = 0;

    virtual TransporterStatus_t   SetCompoundControl(stm_se_ctrl_t Control, const void *Data) = 0;
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Transporter_c
\brief Responsible for taking encoded frames and transporting them to a multiplexor.

The transporter class is responsible for taking encoded data frames and
ensuring that they arrive at their intended output multiplex

This could be implemented as a null mux (discard), a simple file mux, or a more
comprehensive implementation of a multiplexor based in the transport engine.
*/

/*! \fn CoderStatus_t Coder_c::RegisterOutputBufferRing(Ring_t Ring)
\brief Register a ring on which encoded frames are to be placed.

\param Ring Pointer to a Ring_c instance

\return Coder status code, CoderNoError indicates success.
*/

/*! \fn TransporterStatus_t Transporter_c::Input(Buffer_t Buffer)
\brief Accept a coded frame buffer for output.

Provide an output coded frame buffer to be sent for multiplexing

\param Buffer A pointer to a Buffer_c instance of an encoded frame

\return Transporter status code, TransporterNoError indicates success.
*/

#endif /* TRANSPORTER_H */
