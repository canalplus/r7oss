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
#ifndef PREPROC_NULL_H
#define PREPROC_NULL_H

#include "preproc_base.h"

class Preproc_Null_c: public Preproc_Base_c
{
public:
    Preproc_Null_c(void);
    ~Preproc_Null_c(void);

    PreprocStatus_t   Halt(void);

    PreprocStatus_t   Input(Buffer_t          Buffer);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Preproc_Null_c
\brief Null implementation of the Preprocessor classes.

This class will function as a starting point for developing a Preprocessor class,
or alternatively as a Null implementation for a use case whereby no preprocessing is required.

It will simply take buffers from its input, and pass them to its output ring.

*/

#endif /* PREPROC_NULL_H */
