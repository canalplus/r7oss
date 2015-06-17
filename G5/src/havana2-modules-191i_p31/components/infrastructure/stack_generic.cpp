/************************************************************************
COPYRIGHT (C) STMicroelectronics 2007

Source file name : stack_generic.cpp
Author :           Nick

Implementation of the class defining the interface to a simple stack
storage device.


Date        Modification                                    Name
----        ------------                                    --------
08-Jan-07   Created                                         Nick

************************************************************************/

#include "stack_generic.h"

// ------------------------------------------------------------------------
// Constructor function

StackGeneric_c::StackGeneric_c( unsigned int MaxEntries )
{
    OS_InitializeMutex( &Lock );

    Limit       = MaxEntries;
    Level       = 0;
    Storage     = new unsigned int[Limit];

    InitializationStatus = (Storage == NULL) ? StackNoMemory : StackNoError;
}

// ------------------------------------------------------------------------
// Destructor function

StackGeneric_c::~StackGeneric_c( void )
{
    OS_TerminateMutex( &Lock );

    if( Storage != NULL )
	delete Storage;
}

// ------------------------------------------------------------------------
// Insert function

StackStatus_t   StackGeneric_c::Push( unsigned int       Value )
{
    OS_LockMutex( &Lock );
    if( Level == Limit )
    {
	OS_UnLockMutex( &Lock );
	return StackTooManyEntries;
    }

    Storage[Level++]    = Value;
    OS_UnLockMutex( &Lock );

    return StackNoError;
}

// ------------------------------------------------------------------------
// Extract function

StackStatus_t   StackGeneric_c::Pop(    unsigned int    *Value )
{
    //
    // If there is nothing in the Stack we return
    //

    if( Level != 0 )
    {
	OS_LockMutex( &Lock );
	*Value = Storage[--Level];
	OS_UnLockMutex( &Lock );

	return StackNoError;
    }

    return StackNothingToGet;
}


// ------------------------------------------------------------------------
// Flush function

StackStatus_t   StackGeneric_c::Flush( void )
{
    Level	= 0;
    return StackNoError;
}


// ------------------------------------------------------------------------
// Non-empty function

bool   StackGeneric_c::NonEmpty( void )
{
    return (Level != 0);
}

