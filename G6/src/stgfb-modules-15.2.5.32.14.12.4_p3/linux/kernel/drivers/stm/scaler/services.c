/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplay/services.c
 * Copyright (c) 2000-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/dma-mapping.h>

#include <stmdisplay.h>
#include <vibe_os.h>
#include <vibe_debug.h>


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

