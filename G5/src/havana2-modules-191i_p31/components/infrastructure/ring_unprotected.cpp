/************************************************************************
COPYRIGHT (C) STMicroelectronics 2003

Source file name : ring_unprotected.cpp
Author :           Nick

Implementation of the class defining the interface to a simple ring
storage device.


Date        Modification                                    Name
----        ------------                                    --------
11-Dec-03   Created                                         Nick

************************************************************************/

#include "ring_unprotected.h"

// ------------------------------------------------------------------------
// Constructor function

RingUnprotected_c::RingUnprotected_c( unsigned int MaxEntries )
{
    Limit       = MaxEntries + 1;
    NextExtract = 0;
    NextInsert  = 0;
    Storage     = new unsigned int[Limit];

    InitializationStatus = (Storage == NULL) ? RingNoMemory : RingNoError;
}

// ------------------------------------------------------------------------
// Destructor function

RingUnprotected_c::~RingUnprotected_c( void )
{
    if( Storage != NULL )
	delete Storage;
}

// ------------------------------------------------------------------------
// Insert function

RingStatus_t   RingUnprotected_c::Insert( unsigned int   Value )
{
unsigned int OldNextInsert;

    OldNextInsert       = NextInsert;
    Storage[NextInsert] = Value;

    NextInsert++;
    if( NextInsert == Limit )
	NextInsert = 0;

    if( NextInsert == NextExtract )
    {
	NextInsert      = OldNextInsert;
	return RingTooManyEntries;
    }

    return RingNoError;
}

// ------------------------------------------------------------------------
// Extract function

RingStatus_t   RingUnprotected_c::Extract( unsigned int *Value )
{
    if( NextExtract == NextInsert )
	return RingNothingToGet;

    *Value = Storage[NextExtract];

    NextExtract++;
    if( NextExtract == Limit )
	NextExtract = 0;

    return RingNoError;
}

// ------------------------------------------------------------------------
// Flush function

RingStatus_t   RingUnprotected_c::Flush( void )
{
    NextExtract	= 0;
    NextInsert	= 0;

    return RingNoError;
}

// ------------------------------------------------------------------------
// Non-empty function

bool   RingUnprotected_c::NonEmpty( void )
{
    return (NextExtract != NextInsert);
}


