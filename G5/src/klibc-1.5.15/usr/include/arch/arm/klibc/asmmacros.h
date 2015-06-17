/*
 * usr/include/arch/arm/klibc/asmmacros.h
 *
 * Assembly macros used by ARM system call stubs
 */

#ifndef _KLIBC_ASMMACROS_H
#define _KLIBC_ASMMACROS_H

/* An immediate in ARM can be any 8-bit value rotated by an even number of bits */

#define ARM_VALID_IMM(x)	\
	((((x) & ~0x000000ff) == 0) || \
	 (((x) & ~0x000003fc) == 0) || \
	 (((x) & ~0x00000ff0) == 0) || \
	 (((x) & ~0x00003fc0) == 0) || \
	 (((x) & ~0x0000ff00) == 0) || \
	 (((x) & ~0x0003fc00) == 0) || \
	 (((x) & ~0x000ff000) == 0) || \
	 (((x) & ~0x003fc000) == 0) || \
	 (((x) & ~0x00ff0000) == 0) || \
	 (((x) & ~0x03fc0000) == 0) || \
	 (((x) & ~0x0ff00000) == 0) || \
	 (((x) & ~0x3fc00000) == 0) || \
	 (((x) & ~0xff000000) == 0) || \
	 (((x) & ~0xfc000003) == 0) || \
	 (((x) & ~0xf000000f) == 0) || \
	 (((x) & ~0xc000003f) == 0))

#endif /* _KLIBC_ASMMACROS_H */
