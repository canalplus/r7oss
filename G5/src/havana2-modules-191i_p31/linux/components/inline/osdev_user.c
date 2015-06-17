/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

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

Source file name : osdev_user.c - derived from osdev_user.h for os21
Author :           Julian

Contains the useful operating system functions/types
that allow us to implement OS independent device functionality.


Date        Modification                                    Name
----        ------------                                    --------
08-Sep-03   Created                                         Nick
23-Mar-05   Modified for Linux kernel                       Julian

************************************************************************/

/* --- Include the os specific headers we will need --- */

#include "osinline.h"
#include "osdev_device.h"
#include "osdev_user.h"

//
// Actual structure instantiations
//

OSDEV_DeviceLink_t               OSDEV_DeviceList[OSDEV_MAXIMUM_DEVICE_LINKS];
OSDEV_Descriptor_t              *OSDEV_DeviceDescriptors[OSDEV_MAXIMUM_MAJOR_NUMBER];

OSDEV_ExportSymbol( OSDEV_DeviceList );
OSDEV_ExportSymbol( OSDEV_DeviceDescriptors );

// ====================================================================================================================
//      The user level device functions

OSDEV_Status_t   OSDEV_Stat(    const char                      *Name )
{
unsigned int     i;

//

    for( i=0; i<OSDEV_MAXIMUM_DEVICE_LINKS; i++ )
    {
	if( OSDEV_DeviceList[i].Valid && (strcmp( Name, OSDEV_DeviceList[i].Name ) == 0) )
	    return OSDEV_NoError;
    }
    return OSDEV_Error;
}

// -----------------------------------------------------------------------------------------------

OSDEV_Status_t   OSDEV_Open(    const char                      *Name,
				OSDEV_DeviceIdentifier_t        *Handle )
{
unsigned int     i;
unsigned int     Major;
unsigned int     Minor;
OSDEV_Status_t   Status;

//

    *Handle = OSDEV_INVALID_DEVICE;

    for( i=0; i<OSDEV_MAXIMUM_DEVICE_LINKS; i++ )
    {
	if( OSDEV_DeviceList[i].Valid && (strcmp( Name, OSDEV_DeviceList[i].Name ) == 0) )
	{
	    Major = OSDEV_DeviceList[i].MajorNumber;
	    Minor = OSDEV_DeviceList[i].MinorNumber;

	    if( OSDEV_DeviceDescriptors[Major] == NULL )
	    {
		printk( "Error-%s-\nOSDEV_Open - No Descriptor associated with major number %d\n", OS_ThreadName(), Major );
		return OSDEV_Error;
	    }

	    for( i=0; i<OSDEV_MAX_OPENS; i++ )
		if( !OSDEV_DeviceDescriptors[Major]->OpenContexts[i].Open )
		{
		    OSDEV_DeviceDescriptors[Major]->OpenContexts[i].Open        = true;
		    OSDEV_DeviceDescriptors[Major]->OpenContexts[i].MinorNumber = Minor;
		    OSDEV_DeviceDescriptors[Major]->OpenContexts[i].Descriptor  = OSDEV_DeviceDescriptors[Major];
		    OSDEV_DeviceDescriptors[Major]->OpenContexts[i].PrivateData = NULL;

		    Status = OSDEV_DeviceDescriptors[Major]->OpenFn( &OSDEV_DeviceDescriptors[Major]->OpenContexts[i] );

		    if( Status == OSDEV_NoError )
			*Handle                                                 = &OSDEV_DeviceDescriptors[Major]->OpenContexts[i];
		    else
			OSDEV_DeviceDescriptors[Major]->OpenContexts[i].Open    = false;

		    return Status;
		}

	    printk( "Error-%s-\nOSDEV_Open - Too many opens on device '%s'.\n", OS_ThreadName(), OSDEV_DeviceDescriptors[Major]->Name );
	    return OSDEV_Error;
	}
    }

//

    printk( "Error-%s-\nOSDEV_Open - Unable to find device '%s'.\n", OS_ThreadName(), Name );
    return OSDEV_Error;
}

// -----------------------------------------------------------------------------------------------

OSDEV_Status_t   OSDEV_Close( OSDEV_DeviceIdentifier_t     Handle )
{
OSDEV_Status_t   Status;

//

    if( (Handle == NULL) || (!Handle->Open) )
    {
	printk( "Error-%s-\nOSDEV_Open - Attempt to close invalid handle\n", OS_ThreadName() );
	return OSDEV_Error;
    }

//

    Status              = Handle->Descriptor->CloseFn( Handle );
    Handle->Open        = false;

    return Status;
}

// -----------------------------------------------------------------------------------------------

OSDEV_Status_t   OSDEV_Read(      OSDEV_DeviceIdentifier_t         Handle,
						void                            *Buffer,
						unsigned int                     SizeToRead,
						unsigned int                    *SizeRead )
{
    printk( "Error-%s-\nOSDEV_Read - Read not a supported function\n", OS_ThreadName() );
    return OSDEV_Error;
}

// -----------------------------------------------------------------------------------------------

OSDEV_Status_t   OSDEV_Write(     OSDEV_DeviceIdentifier_t         Handle,
						void                            *Buffer,
						unsigned int                     SizeToWrite,
						unsigned int                    *SizeWrote )
{
    printk( "Error-%s-\nOSDEV_Write - Write not a supported function\n", OS_ThreadName() );
    return OSDEV_Error;
}

// -----------------------------------------------------------------------------------------------

OSDEV_Status_t   OSDEV_Ioctl(     OSDEV_DeviceIdentifier_t         Handle,
						unsigned int                     Command,
						void                            *Argument,
						unsigned int                     ArgumentSize )
{
OSDEV_Status_t   Status;

//

    if( (Handle == NULL) || (!Handle->Open) )
    {
	printk( "Error-%s-\nOSDEV_Ioctl - Attempt to use an invalid handle\n", OS_ThreadName() );
	return OSDEV_Error;
    }

//

    Status              = Handle->Descriptor->IoctlFn( Handle, Command, (unsigned long)Argument );

    return Status;
}

// -----------------------------------------------------------------------------------------------

OSDEV_Status_t   OSDEV_Map(       OSDEV_DeviceIdentifier_t          Handle,
						unsigned int                      Offset,
						unsigned int                      Length,
						void                            **MapAddress,
						unsigned int                      Flags )
{
OSDEV_Status_t   Status;

//

    *MapAddress = NULL;

    if( (Handle == NULL) || (!Handle->Open) )
    {
	printk( "Error-%s-\nOSDEV_Map - Attempt to use an invalid handle\n", OS_ThreadName() );
	return OSDEV_Error;
    }

//

    Status              = Handle->Descriptor->MmapFn( Handle, Offset, Length, (unsigned char **)MapAddress );

    // To implement this is userspace then either:
    //
    //  1. Don't
    //  2. Use a magic Offset value (e.g. Offset + OSDEV_MAP_UNCACHED_OFFSET)
    //
    if( Flags & OSDEV_MAP_UNCACHED )
    {
        *MapAddress = OSDEV_TranslateAddressToUncached( *MapAddress );
    }

    return Status;
}

// -----------------------------------------------------------------------------------------------

OSDEV_Status_t   OSDEV_UnMap( OSDEV_DeviceIdentifier_t     Handle,
					    void                        *MapAddress,
					    unsigned int                 Length )
{
    return OSDEV_NoError;
}

