/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine Library.

Streaming Engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Streaming Engine Library may alternatively be licensed under a proprietary
license from ST.
************************************************************************/

//
// CRC driver
//
// Manages comms with user space & decoder/preprocessor
//

#ifndef H_DRIVER
#define H_DRIVER

#include <linux/types.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/cdev.h>

#include "checker.h"
#include "crc.h"

int CRCDriver_Init(const char* device_name);
int CRCDriver_Term(void);
int CRCDriver_GetChecker(int minor, CheckerHandle_t* handle);
int CRCDriver_ReleaseChecker(int minor);

#endif // H_DRIVER
