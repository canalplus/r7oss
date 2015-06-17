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
#ifndef CODER_NULL_H
#define CODER_NULL_H

#include "coder_base.h"

class Coder_Null_c: public Coder_Base_c
{
public:
    Coder_Null_c(void);
    ~Coder_Null_c(void);

    CoderStatus_t   Input(Buffer_t  Buffer);

    CoderStatus_t   EncodeFrame(Buffer_t    CodedFrameBuffer,
                                Buffer_t    InputBuffer);

    CoderStatus_t   CompletionCallback(Buffer_t Buffer,
                                       unsigned int    DataSize);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Coder_Null_c
\brief Null Coder class implementation

\note This should only be used for development and debug
*/

#endif /* CODER_BASE_H */
