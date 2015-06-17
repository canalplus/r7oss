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

Source file name : collator.h
Author :           Nick

Definition of the collator class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
01-Nov-06   Created                                         Nick
31-Jul-09   Added GuaranteedNonBlockingWriteSpace() 
	    supported in reversing collator.		    Nick
************************************************************************/

#ifndef H_COLLATOR
#define H_COLLATOR

#include "player.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

enum
{
    CollatorNoError				= PlayerNoError,
    CollatorError				= PlayerError,

    CollatorWouldBlock				= BASE_COLLATOR,				
    CollatorBufferOverflow,
    CollatorBufferUnderflow, ///< Primarily for internal use
    CollatorUnwindStack, ///< Reported to cause execution to unwind (to avoid recursion)
};

typedef PlayerStatus_t	CollatorStatus_t;


// ---------------------------------------------------------------------
//
// Class definition
//

class Collator_c : public BaseComponentClass_c
{
public:

    virtual CollatorStatus_t   RegisterOutputBufferRing(	Ring_t			  Ring ) = 0;

    virtual CollatorStatus_t   InputJump(			bool			  SurplusDataInjected,
								bool			  ContinuousReverseJump	) = 0;

    virtual CollatorStatus_t   InputGlitch(			void ) = 0;

    virtual CollatorStatus_t   Input(				PlayerInputDescriptor_t	 *Input,
								unsigned int		  DataLength,
								void			 *Data,
								bool			  NonBlocking = false,
								unsigned int		 *DataLengthRemaining = NULL ) = 0;

    virtual CollatorStatus_t   FrameFlush(			void ) = 0;

    virtual CollatorStatus_t   DiscardAccumulatedData(		void ) = 0;

    virtual CollatorStatus_t   NonBlockingWriteSpace( unsigned int		 *Size ) { return PlayerNotSupported; }
};

// ---------------------------------------------------------------------
//
// Docuentation
//

/*! \class Collator_c
\brief Responsible for taking demultiplexed input data and creating frames suitable for parsing/decoding.

The collator class is responsible for taking as input blocks of demultiplexed data and creating frames of output suitable for parsing/decoding. This is a list of its entrypoints, and a partial list of the calls it makes, and the data structures it accesses, these are in addition to the standard component class entrypoints, and the complete list of support entrypoints in the Player class. 

The partial list of entrypoints used by this class:

- Empty list.

The partial list of meta data types used by this class:

- There are no input buffers, but as part of the input parameter list, the following is attached:
  - <b>PlayerInputDescriptor</b> Describes the input block.

- Attached to output buffers :-
  - <b>CodedFrameParameters</b>, describes the coded frame output.
  - <b>StartCodeList</b>, optional output attachment for those collators generating a start code scan.
*/

/*! \fn CollatorStatus_t Collator_c::RegisterOutputBufferRing(Ring_t Ring)
\brief Register the ring on which collacted frames are to be placed.

This function is used to register the ring on which collated frame buffers are to be placed. 

\param Ring A pointer to a Ring class instance.

\return Collator status code, CollatorNoError indicates success.
*/

/*! \fn CollatorStatus_t Collator_c::InputJump(bool SurplusDataInjected, bool ContinuousReverseJump)
\brief Demark discontinuous input data.

For more information on discontinous streams see <b>InputJump</b> on \ref input.

\param SurplusDataInjected True if the jump should discard or flush data.
\param ContinuousReverseJump True if a continuous reverse jump has occured.

\return Collator status code, CollatorNoError indicates success.
*/

/*! \fn CollatorStatus_t   Collator_c::InputGlitch(             void )
\brief Demark a glitch in the input data.

For more information on discontinous streams see <b>InputGlitch</b> on \ref input.

\return Collator status code, CollatorNoError indicates success.
*/

/*! \fn CollatorStatus_t Collator_c::Input(PlayerInputDescriptor_t *Input, unsigned int	DataLength, void *Data, bool NonBlocking, unsigned int *DataLengthRemaining )
\brief Accept demultiplexed data for collation.

<b>TODO: This method is not adequately documented.</b>

\param Input A pointer to an input descriptor.
\param DataLength Length of the data block (in bytes).
\param Data A pointer to a block of memory containing the input data.
\param NonBlocking flag indicating the call should not block. This is not supported in the non-reversing collator.
\param DataLengthRemaining returned value indicating the amount of data unconsumed when the function returns CollatorWouldBlock.

\return Collator status code, CollatorNoError indicates success.
*/

/*! \fn CollatorStatus_t Collator_c::FrameFlush()
\brief Assert that the current input data is a complete frame.

This function indicates that the current input data is a complete
frame. It is used in those data streams were a frame is usually 
detected as being complete when the start of the next frame is
detected, to signal that the accumulated data should be tranferred to 
the output ring.

\return Collator status code, CollatorNoError indicates success.
*/

/*! \fn CollatorStatus_t Collator_c::DiscardAccumulatedData()
\brief Discard accumulated data that has not been passed to the output ring.

This function indicates that the current input data that has been accumulated, but not passed onto the output ring, is to be discarded.

\return Collator status code, CollatorNoError indicates success.
*/

/*! \fn CollatorStatus_t Collator_c::NonBlockingWriteSpace()
\brief Get the size of guaranteed non blocking write space

This function indicates how much data can be written with a guarantee that the writes will not block.

\return Collator status code, CollatorNoError indicates success, PlayerNotImplemented will be 
returned for the non-reversing collator which does not include support for this facility.
*/
#endif
