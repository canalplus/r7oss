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
#ifndef TRANSPORTER_BASE_H
#define TRANSPORTER_BASE_H

#include "transporter.h"


// Awaiting a fix for maximum buffer count
#define TRANSPORTER_STREAM_MAX_CODED_BUFFERS    128

class Transporter_Base_c: public Transporter_c
{
public:
    Transporter_Base_c(void);
    ~Transporter_Base_c(void);

    TransporterStatus_t   Halt(void);

    TransporterStatus_t   RegisterBufferManager(BufferManager_t BufferManager);

    TransporterStatus_t   Input(Buffer_t Buffer);

    TransporterStatus_t   CreateConnection(stm_object_h sink);
    TransporterStatus_t   RemoveConnection(stm_object_h sink);

    TransporterStatus_t   GetControl(stm_se_ctrl_t  Control,
                                     void          *Data);
    TransporterStatus_t   SetControl(stm_se_ctrl_t  Control,
                                     const void    *Data);
    TransporterStatus_t   GetCompoundControl(stm_se_ctrl_t  Control,
                                             void          *Data);
    TransporterStatus_t   SetCompoundControl(stm_se_ctrl_t  Control,
                                             const void    *Data);

protected:
    BufferManager_t               BufferManager;
    BufferType_t                  OutputMetaDataBufferType;

private:
    void                  InitRelay(void);
    void                  TerminateRelay(void);
    void                  DumpViaRelay(Buffer_t Buffer);

    bool                          mRelayInitialized;
    unsigned int                  mRelayType;
    unsigned int                  mRelayIndex;

    DISALLOW_COPY_AND_ASSIGN(Transporter_Base_c);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Transporter_Base_c
\brief Base class implementation for the Transporter classes.

*/

#endif /* TRANSPORTER_BASE_H */
