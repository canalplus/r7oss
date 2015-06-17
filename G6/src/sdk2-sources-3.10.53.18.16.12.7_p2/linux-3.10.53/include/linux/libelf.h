#ifndef _LIBELF_H_
#define _LIBELF_H_

#ifdef CONFIG_LIBELF_32
#ifdef __LIBELF_WORDSIZE
#undef __LIBELF_WORDSIZE
#endif
#define __LIBELF_WORDSIZE 32
#include "libelf/_libelf.h"
#endif

#ifdef CONFIG_LIBELF_64
#ifdef __LIBELF_WORDSIZE
#undef __LIBELF_WORDSIZE
#endif
#define __LIBELF_WORDSIZE 64
#include "libelf/_libelf.h"
#endif

#endif /* _LIBELF_H_ */
