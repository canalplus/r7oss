/*
 * ELF loader API API
 *
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
 * Authors: Bruno Lavigueur (bruno.lavigueur@st.com)
 *
 */

#ifndef _ELF_LOADER_h_
#define _ELF_LOADER_h_

#ifndef __KERNEL_PRINTK__
// Can't include osdev_device.h because there's a conflict with linux elf.h
void printk(const char* format, ...);

#ifdef CONFIG_ARCH_STI
#include <linux/kern_levels.h>  // for kernel 3.10 and above
#else
#define KERN_ERR        "<3>"/* error conditions */
#define KERN_INFO       "<6>"/* informational*/
#endif  // CONFIG_ARCH_STI

#if !defined( __KERNEL__) || defined(CONFIG_PRINTK)
#define pr_info(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) printk(KERN_ERR fmt, ##__VA_ARGS__)
#else
#define pr_info(fmt, ...)
#define pr_err(fmt, ...)
#endif
#endif

#include "sthormAlloc.h"

void host_loadELF(unsigned char* file, HostAddress* block);

int host_validate(char *file);
char *host_loadSegments(char *file);
void host_loadExecSegments(char *file, HostAddress* block);

// JLX: note: stubbed
int host_reloc(int type, char *reloc_addr, char *symbol_addr, int symbol_size);
void host_end(char *P_addr);

#endif /* _ELF_LOADER_h_ */
