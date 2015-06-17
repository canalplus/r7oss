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
 * Implementation of wrapped functions for some posix APIs.
 *
 * Augment as necessary for unittests on top of posix APIs.
 * Declaration in wrap_posix.h.
 */

#include <pthread.h>
#include "wrap_posix.h"

WRAP_IMPL(pthread_create, int,
          (pthread_t *thread, const pthread_attr_t *attr,
           void *(*start_routine)(void *), void *arg),
          (thread, attr, start_routine, arg));

WRAP_IMPL(pthread_mutex_init, int,
          (pthread_mutex_t *mutex,
           const pthread_mutexattr_t *attr),
          (mutex, attr));

WRAP_IMPL(pthread_cond_init, int,
          (pthread_cond_t *cond,
           const pthread_condattr_t *attr),
          (cond, attr));

WRAP_IMPL(sem_wait, int,
          (sem_t *sem),
          (sem));
