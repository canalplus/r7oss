/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : collator_pes_video_vc1.cpp
Author :           Julian

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
12-Jun-07   Created                                         Julian

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesVideoVc1_c
///
/// Implements initialisation of collator video class for VC1
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_video_vc1.h"
#include "mpeg2.h"
#include "vc1.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

////////////////////////////////////////////////////////////////////////////
///
/// Initialize the class by resetting it.
///
/// During a constructor calls to virtual methods resolve to the current class (because
/// the vtable is still for the class being constructed). This means we need to call
/// ::Reset again because the calls made by the sub-constructors will not have called
/// our reset method.
///
Collator_PesVideoVc1_c::Collator_PesVideoVc1_c( void )
{
    if( InitializationStatus != CollatorNoError )
        return;

    Collator_PesVideoVc1_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Resets and configures according to the requirements of this stream content
///
/// \return void
///
CollatorStatus_t Collator_PesVideoVc1_c::Reset( void )
{
CollatorStatus_t Status;

//

    COLLATOR_DEBUG(">><<\n");

    Status = Collator_PesVideo_c::Reset();
    if( Status != CollatorNoError )
        return Status;

    Configuration.GenerateStartCodeList      = true;
    Configuration.MaxStartCodes              = 256;

    Configuration.StreamIdentifierMask       = 0xff;                            // Video
    Configuration.StreamIdentifierCode       = 0xfd;
    Configuration.BlockTerminateMask         = 0xfe;                            // allows frame or field
    Configuration.BlockTerminateCode         = VC1_FIELD_START_CODE;
    Configuration.IgnoreCodesRangeStart      = MPEG2_FIRST_SLICE_START_CODE+1;  // Slice codes other than first
    Configuration.IgnoreCodesRangeEnd        = MPEG2_FIRST_SLICE_START_CODE;
    Configuration.InsertFrameTerminateCode   = true;                            // Add code to force the mme decode to terminate
    Configuration.TerminalCode               = VC1_FRAME_START_CODE;            // Configuration.BlockTerminateCode
    Configuration.ExtendedHeaderLength       = 0;
    Configuration.DeferredTerminateFlag      = false;

    return CollatorNoError;
}
