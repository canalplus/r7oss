/*
 * Copyright (C) 2012 STMicroelectronics
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "elfCopier.h"

void *memcpy(void *dest, const void *src, size_t count);
void *memset(void *dest, int c          , size_t count);
void sthorm_memcpy(void *dest, void *source, int size);
void sthorm_memset(void *dest, int value   , int size);

void elfCopyText(int filesz, int memsz, HostAddress* block, Elf64_Addr offset, void* source)
{
	memcpy(block->virtual + offset, source, filesz);

	// if necessary, add padding zero
	if (filesz < memsz)
		memset(block->virtual + offset + filesz, 0, memsz - filesz);
}

void elfCopyData(int filesz, int memsz, Elf64_Addr dest, void* source)
{
	sthorm_memcpy((void*)(uint32_t)dest, source, filesz);
        if (filesz < memsz)
        	sthorm_memset((void*)(uint32_t)(dest + filesz), 0, memsz - filesz);
}

