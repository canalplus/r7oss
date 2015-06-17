/************************************************************************
COPYRIGHT (C) STMicroelectronics 2003

Source file name : ring_protected.cpp
Author :           Nick

Implementation of the class defining the interface to a simple ring
storage device.


Date        Modification                                    Name
----        ------------                                    --------
11-Dec-03   Created                                         Nick

************************************************************************/

#include "ring_protected.h"

// ------------------------------------------------------------------------
// Constructor function

RingProtected_c::RingProtected_c( unsigned int MaxEntries )
{
    OS_InitializeMutex( &Lock );

    Limit       = MaxEntries + 1;
    NextExtract = 0;
    NextInsert  = 0;
    Storage     = new unsigned int[Limit];

    InitializationStatus = (Storage == NULL) ? RingNoMemory : RingNoError;
}

// ------------------------------------------------------------------------
// Destructor function

RingProtected_c::~RingProtected_c( void )
{
    OS_TerminateMutex( &Lock );

    if( Storage != NULL )
	delete Storage;
}

// ------------------------------------------------------------------------
// Insert function

RingStatus_t   RingProtected_c::Insert( unsigned int     Value )
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
    return RingNoError;
}

// ------------------------------------------------------------------------
// Extract function

RingStatus_t   RingProtected_c::Extract( unsigned int   *Value )
{

    OS_LockMutex( &Lock );
    if( NextExtract == NextInsert )
    {
	OS_UnLockMutex( &Lock );
	return RingNothingToGet;
    }

    *Value = Storage[NextExtract];

    NextExtract++;
    if( NextExtract == Limit )
	NextExtract = 0;

    OS_UnLockMutex( &Lock );
    return RingNoError;
}

// ------------------------------------------------------------------------
// Flush function

RingStatus_t   RingProtected_c::Flush( void )
{
    OS_LockMutex( &Lock );
    NextExtract	= 0;
    NextInsert	= 0;
    OS_UnLockMutex( &Lock );

    return RingNoError;
}

// ------------------------------------------------------------------------
// Non-empty function

bool   RingProtected_c::NonEmpty( void )
{
    return (NextExtract != NextInsert);
}


