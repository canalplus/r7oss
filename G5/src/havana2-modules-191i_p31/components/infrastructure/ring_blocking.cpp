/************************************************************************
COPYRIGHT (C) STMicroelectronics 2004

Source file name : ring_blocking.cpp
Author :           Nick

Implementation of the class defining the interface to a simple ring
storage device.


Date        Modification                                    Name
----        ------------                                    --------
23-Sep-04   Created                                         Nick

************************************************************************/

#include "ring_blocking.h"

// ------------------------------------------------------------------------
// Constructor function

RingBlocking_c::RingBlocking_c( unsigned int MaxEntries )
{
    OS_InitializeEvent( &Signal );

    Limit       = MaxEntries + 1;
    NextExtract = 0;
    NextInsert  = 0;
    Storage     = new unsigned int[Limit];

    InitializationStatus = (Storage == NULL) ? RingNoMemory : RingNoError;
}

// ------------------------------------------------------------------------
// Destructor function

RingBlocking_c::~RingBlocking_c( void )
{
    OS_SetEvent( &Signal );
    OS_TerminateEvent( &Signal );

    if( Storage != NULL )
	delete Storage;
}

// ------------------------------------------------------------------------
// Insert function

RingStatus_t   RingBlocking_c::Insert( unsigned int      Value )
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

    OS_SetEvent( &Signal );
    return RingNoError;
}

// ------------------------------------------------------------------------
// Extract function

RingStatus_t   RingBlocking_c::Extract( unsigned int    *Value )
{

    while( true )
    {
	OS_ResetEvent( &Signal );

	if( NextExtract != NextInsert )
	{
	    *Value = Storage[NextExtract];

	    NextExtract++;
	    if( NextExtract == Limit )
		NextExtract = 0;

	    return RingNoError;
	}

	OS_WaitForEvent( &Signal, OS_INFINITE );
    }
    return RingNoError;
}

// ------------------------------------------------------------------------
// Flush function

RingStatus_t   RingBlocking_c::Flush( void )
{
    NextExtract	= 0;
    NextInsert	= 0;
    return RingNoError;
}

// ------------------------------------------------------------------------
// Non-empty function

bool   RingBlocking_c::NonEmpty( void )
{
    return (NextExtract != NextInsert);
}

