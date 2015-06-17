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

Source file name : collator_pes_video_avs.cpp
Author :           Julian

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
12-Jun-07   Created                                         Julian

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Collator_PesVideoAvs_c
///
/// Implements initialisation of collator video class for AVS
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_video_avs.h"
#include "mpeg2.h"
#include "avs.h"

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
Collator_PesVideoAvs_c::Collator_PesVideoAvs_c( void )
{
    if( InitializationStatus != CollatorNoError )
        return;

    Collator_PesVideoAvs_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Resets and configures according to the requirements of this stream content
///
/// \return void
///
CollatorStatus_t Collator_PesVideoAvs_c::Reset( void )
{
CollatorStatus_t Status;

//

    COLLATOR_DEBUG(">><<\n");

    Status = Collator_PesVideo_c::Reset();
    if( Status != CollatorNoError )
        return Status;

    Configuration.GenerateStartCodeList      = true;
    Configuration.MaxStartCodes              = 256;

    Configuration.StreamIdentifierMask       = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode       = PES_START_CODE_VIDEO;
    Configuration.BlockTerminateMask         = 0xFA;                            // Picture
    Configuration.BlockTerminateCode         = 0xB2;
    Configuration.IgnoreCodesRangeStart      = MPEG2_FIRST_SLICE_START_CODE+1;  // Slice codes other than first
    Configuration.IgnoreCodesRangeEnd        = MPEG2_FIRST_SLICE_START_CODE;
    Configuration.InsertFrameTerminateCode   = true;                            // Force the mme decode to terminate after a picture
    Configuration.TerminalCode               = AVS_VIDEO_SEQUENCE_END_CODE;
    Configuration.ExtendedHeaderLength       = 0;

    Configuration.DeferredTerminateFlag      = false;

    Configuration.StreamTerminateFlushesFrame   = true;         // Use an end of sequence to force a frame flush
    Configuration.StreamTerminationCode         = AVS_VIDEO_SEQUENCE_END_CODE;

    return CollatorNoError;
}
