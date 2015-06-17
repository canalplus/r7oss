/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/vxi/services.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/types.h>
#include <vibe_os.h>


//////////////////////////////////////////////////////////////////////////////
// Services required by C++ code. These are typically part of the C++ run time
// but we dont have a run time!

void *_Znwj(unsigned int size)
{
  if(size>0)
    return vibe_os_allocate_memory(size);

  return NULL;
}

void *_Znaj(unsigned int size)
{
  if(size>0)
    return vibe_os_allocate_memory(size);

  return NULL;
}

void* __builtin_new(size_t size)
{
  if(size>0)
    return vibe_os_allocate_memory(size);

  return NULL;
}

void* __builtin_vec_new(size_t size)
{
  return __builtin_new(size);
}

void _ZdlPv(void *ptr)
{
  if(ptr)
    vibe_os_free_memory(ptr);
}

void _ZdaPv(void *ptr)
{
  if(ptr)
    vibe_os_free_memory(ptr);
}

void __builtin_delete(void *ptr)
{
  if(ptr)
    vibe_os_free_memory(ptr);
}

void __builtin_vec_delete(void* ptr)
{
  __builtin_delete(ptr);
}


void __pure_virtual(void)
{
}

void __cxa_pure_virtual(void)
{
}

