/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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

Source file name : osdev_device.c
Author :           Daniel

Takes all the inline symbols in osdev_device.h and make them available
to the rest of the kernel as normal functions. This allows C++
applications access to these symbols without having to include the
Linux headers (which are not C++ safe).

Note that it is not possible to export all the OSDEV_ functions as
functions since some will break quite badly when un-inlined.


Date        Modification                                    Name
----        ------------                                    --------
10-Oct-05   Created                                         Daniel

************************************************************************/

#define EXPORT_SYMBOLS_FOR_CXX

#include "osdev_device.h"

OSDEV_ExportSymbol(OSDEV_Malloc);
OSDEV_ExportSymbol(OSDEV_Free);
OSDEV_ExportSymbol(OSDEV_TranslateAddressToUncached);
OSDEV_ExportSymbol(OSDEV_PurgeCacheAll);
OSDEV_ExportSymbol(OSDEV_FlushCacheRange);
OSDEV_ExportSymbol(OSDEV_InvalidateCacheRange);
OSDEV_ExportSymbol(OSDEV_SnoopCacheRange);
OSDEV_ExportSymbol(OSDEV_PurgeCacheRange);
