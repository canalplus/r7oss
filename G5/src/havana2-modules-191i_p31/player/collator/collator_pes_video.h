/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : collator_pes_video.h
Author :           Nick

Definition of the base collator pes class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
19-Apr-07   Created from existing collator_pes_video.h      Daniel

************************************************************************/

#ifndef H_COLLATOR_PES_VIDEO
#define H_COLLATOR_PES_VIDEO

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Collator_PesVideo_c : public Collator_Pes_c
{
public:

    //
    // Collator class functions
    //

    virtual CollatorStatus_t   Input(	PlayerInputDescriptor_t	 *Input,
                                        unsigned int		  DataLength,
                                        void			 *Data,
					bool			  NonBlocking = false,
					unsigned int		 *DataLengthRemaining = NULL );

    CollatorStatus_t   InternalFrameFlush(      void );

    //
    // A frame flush overload that allows the specific setting of a flushed by terminate flag.
    // This allows such things as the frame parser to know that it can give display indices to 
    // all frames including this one, because any frames that follow will be part of what is 
    // strictly a new stream. This ensures that last framnes will be seen.
    //

    CollatorStatus_t   InternalFrameFlush(      bool                    FlushedByStreamTerminate );

    //
    // Some strange additions by someone
    //

protected:

     bool TerminationFlagIsSet;

public:
	Collator_PesVideo_c() { TerminationFlagIsSet = false; }

};

#endif

