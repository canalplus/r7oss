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

#ifndef H_ELF_COPIER
#define H_ELF_COPIER

#include "elf.h"
#include "sthormAlloc.h"

void elfCopyText(int filesz, int memsz, HostAddress* block, Elf64_Addr offset, void* source);
void elfCopyData(int filesz, int memsz, Elf64_Addr dest, void* source);

#endif
