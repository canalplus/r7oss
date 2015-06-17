/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#include <linux/module.h>

#include "osdev_sched.h"
#include "osdev_device.h"
#include "osdev_user.h"

OSDEV_DeviceLink_t  OSDEV_DeviceList[OSDEV_MAXIMUM_DEVICE_LINKS];
OSDEV_Descriptor_t  *OSDEV_DeviceDescriptors[OSDEV_MAXIMUM_MAJOR_NUMBER];

EXPORT_SYMBOL(OSDEV_DeviceList);
EXPORT_SYMBOL(OSDEV_DeviceDescriptors);
EXPORT_SYMBOL(OSDEV_Open);
EXPORT_SYMBOL(OSDEV_Close);
EXPORT_SYMBOL(OSDEV_Ioctl);

static OSDEV_Status_t CheckHandle(OSDEV_DeviceIdentifier_t Handle, const char *operation)
{
	OSDEV_Status_t Status = OSDEV_NoError;

	if (Handle == NULL) {
		pr_err("Error: %s %s Attempt to %s NULL handle %p\n", __func__, OSDEV_ThreadName(), operation, Handle);
		return OSDEV_Error;
	}

	mutex_lock(&Handle->Descriptor->lock);
	if (Handle->Open == false) {
		pr_err("Error: %s %s Attempt to %s closed handle %p\n", __func__, OSDEV_ThreadName(), operation, Handle);
		Status = OSDEV_Error;
	}
	mutex_unlock(&Handle->Descriptor->lock);

	return Status;
}

OSDEV_Status_t   OSDEV_Open(const char                      *Name,
                            OSDEV_DeviceIdentifier_t        *Handle)
{
	unsigned int     i;
	unsigned int     Major;
	unsigned int     Minor;
	OSDEV_Status_t   Status;

	*Handle = OSDEV_INVALID_DEVICE;

	for (i = 0; i < OSDEV_MAXIMUM_DEVICE_LINKS; i++) {
		if (OSDEV_DeviceList[i].Valid && (strcmp(Name, OSDEV_DeviceList[i].Name) == 0)) {
			Major = OSDEV_DeviceList[i].MajorNumber;
			Minor = OSDEV_DeviceList[i].MinorNumber;

			if (OSDEV_DeviceDescriptors[Major] == NULL) {
				pr_err("Error: %s %s No Descriptor associated with major number %d\n", __func__, OSDEV_ThreadName(), Major);
				return OSDEV_Error;
			}
			// need a mutex to avoid two different threads to select same OpenContext
			mutex_lock(&OSDEV_DeviceDescriptors[Major]->lock);
			for (i = 0; i < OSDEV_MAX_OPENS; i++)
				if (!OSDEV_DeviceDescriptors[Major]->OpenContexts[i].Open) {
					OSDEV_DeviceDescriptors[Major]->OpenContexts[i].Open        = true;
					OSDEV_DeviceDescriptors[Major]->OpenContexts[i].MinorNumber = Minor;
					OSDEV_DeviceDescriptors[Major]->OpenContexts[i].Descriptor  = OSDEV_DeviceDescriptors[Major];
					OSDEV_DeviceDescriptors[Major]->OpenContexts[i].PrivateData = NULL;

					Status = OSDEV_DeviceDescriptors[Major]->OpenFn(&OSDEV_DeviceDescriptors[Major]->OpenContexts[i]);

					if (Status == OSDEV_NoError) {
						*Handle = &OSDEV_DeviceDescriptors[Major]->OpenContexts[i];
					} else {
						OSDEV_DeviceDescriptors[Major]->OpenContexts[i].Open    = false;
					}
					mutex_unlock(&OSDEV_DeviceDescriptors[Major]->lock);
					return Status;
				}
			mutex_unlock(&OSDEV_DeviceDescriptors[Major]->lock);
			pr_err("Error: %s %s Too many opens on device '%s'\n", __func__, OSDEV_ThreadName(),
			       OSDEV_DeviceDescriptors[Major]->Name);
			return OSDEV_Error;
		}
	}

	pr_err("Error: %s %s Unable to find device '%s'\n", __func__, OSDEV_ThreadName(), Name);
	return OSDEV_Error;
}

OSDEV_Status_t   OSDEV_Close(OSDEV_DeviceIdentifier_t     Handle)
{
	OSDEV_Status_t Status = CheckHandle(Handle, "close");
	if (Status == OSDEV_NoError) {
		Status        = Handle->Descriptor->CloseFn(Handle);
		mutex_lock(&Handle->Descriptor->lock);
		Handle->Open  = false;
		mutex_unlock(&Handle->Descriptor->lock);
	}

	return Status;
}

OSDEV_Status_t   OSDEV_Ioctl(OSDEV_DeviceIdentifier_t         Handle,
                             unsigned int                     Command,
                             void                            *Argument,
                             unsigned int                     ArgumentSize)
{
	OSDEV_Status_t Status = CheckHandle(Handle, "ioctl");
	if (Status == OSDEV_NoError) {
		Status = Handle->Descriptor->IoctlFn(Handle, Command, (unsigned long)Argument);
	}

	return Status;
}

