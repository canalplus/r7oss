/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

/*
 * Wrapped function helper.
 *
 * This file is a helper for defining easily wrapped functions in
 * test cases.
 *
 * Wrapping functions requires several steps:
 * - declaring with WRAP_DECL the functions to be wrapped,
 * - implementing in a module the wrappers with WRAP_IMPL,
 * - using in test case WRAP_RETURN() for instance to temporarily
 * modify the behavior of the wrapped function,
 * - linking with wrap options (-Wl,--wrap=... option).
 *
 * For instance, wrapping malloc and forcing error in a testcase:

 $ cat wrap_malloc.h
 #include <stdlib.h>
 #include "wrap.h"
 // Declares a wrapping for malloc.
 WRAP_DECL(malloc,void *,(size_t size),(size))

 $ cat wrap_malloc.c
 #include "wrap_malloc.h"
 // Defines the default wrappers for malloc
 WRAP_IMPL(malloc,void *,(size_t size),(size))

 $ cat testcase.c
 #include <stdlib.h>
 #include <assert.h>
 #include <stdio.h>
 #include "wrap_malloc.h"

 int main() {
   // Next call to malloc will return NULL with errno == ENOMEM
   WRAP_RETURN_ERRNO(malloc, NULL, ENOMEM);
   assert(malloc(10) == NULL && errno == ENOMEM); // NULL forced
   assert(malloc(10) != NULL); // call real malloc
   // The two next calls to malloc will return NULL
   WRAP_RETURN_ERRNO_N(malloc, NULL, ENOMEM, 2);
   assert(malloc(10) == NULL && errno == ENOMEM); // NULL forced
   assert(malloc(10) == NULL && errno == ENOMEM); // NULL forced
   assert(malloc(10) != NULL); // call real malloc
   printf("SUCCESS\n");
   return 0;
 }
 $ gcc testcase.c wrap_malloc.c -Wl,--wrap=malloc -Wall
 $ ./a.out
 SUCCESS

 */

#ifndef _WRAP_H
#define _WRAP_H

#include <errno.h>

#define WRAP_DECL(name,type,proto, args)                                \
  extern type __real_##name proto;                                      \
  extern type __wrap_##name proto;                                      \
  extern void _fake_return_errno_##name(int num, type val, int err);    \
 

#define WRAP_IMPL(name,type,proto, args)                                \
  static __thread int  _faked_num_##name;                               \
  static __thread type _faked_val_##name;                               \
  static __thread int _faked_errno_##name;                              \
  void _fake_return_errno_##name(int num, type val, int err) {          \
    _faked_val_##name = val;                                            \
      _faked_num_##name = num;                                          \
        _faked_errno_##name = err;                                      \
  }                                                                     \
  type __wrap_##name proto                                              \
  {                                                                     \
    if (_faked_num_##name > 0) {                                        \
      _faked_num_##name--;                                              \
        errno = _faked_errno_##name;                                    \
          return _faked_val_##name;                                     \
    }                                                                   \
    return __real_##name args;                                          \
  }                                                                     \
 

#define WRAP_RETURN(name,val)               _fake_return_errno_##name(1, val, 0)
#define WRAP_RETURN_N(name,val,n)           _fake_return_errno_##name(n, val, 0)
#define WRAP_RETURN_ERRNO(name,val,err)     _fake_return_errno_##name(1, val, err)
#define WRAP_RETURN_ERRNO_N(name,val,err,n) _fake_return_errno_##name(n, val, err)

#endif /* _WRAP_H */
