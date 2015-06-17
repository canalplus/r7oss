/************************************************************************
COPYRIGHT (C) STMicroelectronics 2004

Source file name : ring_generic.cpp
Author :           Nick

Implementation of the class defining the interface to a simple ring
storage device.


Date        Modification                                    Name
----        ------------                                    --------
23-Sep-04   Created                                         Nick

************************************************************************/

#include "ring_generic.h"

// ------------------------------------------------------------------------
// Constructor function

RingGeneric_c::RingGeneric_c( unsigned int MaxEntries )
{
    OS_InitializeMutex( &Lock );
    OS_InitializeEvent( &Signal );

    Limit       = MaxEntries + 1;
    NextExtract = 0;
    NextInsert  = 0;
    Storage     = new unsigned int[Limit];

    InitializationStatus = (Storage == NULL) ? RingNoMemory : RingNoError;
}

// ------------------------------------------------------------------------
// Destructor function

RingGeneric_c::~RingGeneric_c( void )
{
    OS_SetEvent( &Signal );
    OS_SleepMilliSeconds( 1 );

    OS_TerminateMutex( &Lock );
    OS_TerminateEvent( &Signal );

    if( Storage != NULL )
	delete Storage;
}

// ------------------------------------------------------------------------
// Insert function

RingStatus_t   RingGeneric_c::Insert( unsigned int       Value )
{
unsigned int OldNextInsert;

    OS_LockMutex( &Lock );

    OldNextInsert       = NextInsert;
    Storage[NextInsert] = Value;

    NextInsert++;
    if( NextInsert == Limit )
	NextInsert = 0;

    if( NextInsert == NextExtract )
    {
	NextInsert      = OldNextInsert;
	OS_UnLockMutex( &Lock );
	return RingTooManyEntries;
    }

    OS_UnLockMutex( &Lock );
    OS_SetEvent( &Signal );

    return RingNoError;
}

// ------------------------------------------------------------------------
// Extract function

RingStatus_t   RingGeneric_c::Extract(  unsigned int    *Value,
					unsigned int     BlockingPeriod )
{
    //
    // If there is nothing in the ring we wait for upto the specified period.
    //

    OS_ResetEvent( &Signal );
    if( (NextExtract == NextInsert) && (BlockingPeriod != RING_NONE_BLOCKING) )
	OS_WaitForEvent( &Signal, BlockingPeriod );

    OS_LockMutex( &Lock );
    if( NextExtract != NextInsert )
    {
	*Value = Storage[NextExtract];

	NextExtract++;
	if( NextExtract == Limit )
	    NextExtract = 0;

	OS_UnLockMutex( &Lock );
	return RingNoError;
    }

    OS_UnLockMutex( &Lock );
    return RingNothingToGet;
}

// ------------------------------------------------------------------------
// Flush function

RingStatus_t   RingGeneric_c::Flush( void )
{
    OS_LockMutex( &Lock );
    NextExtract	= 0;
    NextInsert	= 0;
    OS_UnLockMutex( &Lock );

    return RingNoError;
}

// ------------------------------------------------------------------------
// Non-empty function

bool   RingGeneric_c::NonEmpty( void )
{
    return (NextExtract != NextInsert);
}

