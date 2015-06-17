/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : sbag.h

Description: SBAG API

************************************************************************/
#ifndef H_SBAG_API
#define H_SBAG_API

void SBAG_SetWatchpoint(const char* description, uint32_t address, uint32_t size, int interior, uint32_t source_id, uint32_t source_mask);
void SBAG_Start(int skip);
void SBAG_Result(const char* hardware);

#endif // H_SBAG_API
