/*
 *  This file is required for compilation of the ALSA sources and is
 *  present in latest GLIBC 2.1 libraries.
 */

#ifndef __BYTE_SWAP_H
#define __BYTE_SWAP_H 1

#include <endian.h>
#include <bytesex.h>
#include <asm/byteorder.h>

#ifndef bswap_16
#define bswap_16(x) __swab16((x))
#endif

#ifndef bswap_32
#define bswap_32(x) __swab32((x))
#endif

#ifndef bswap_64
#define bswap_64(x) __swab64((x))
#endif

#endif
