/*
 * arch/i386/archinit.c
 *
 * Architecture-specific libc initialization
 */

#include <stdint.h>
#include <klibc/compiler.h>
#include <elf.h>
#include <sys/auxv.h>

extern void (*__syscall_entry)(int, ...);

void __libc_archinit(void)
{
	if (__auxval[AT_SYSINFO])
		__syscall_entry = (void (*)(int, ...)) __auxval[AT_SYSINFO];
}
