/*
 *
 *  D-Bus++ - C++ bindings for D-Bus
 *
 *  Copyright (C) 2005-2007  Paolo Durante <shackan@gmail.com>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <dbus-c++/util.h>

namespace DBus {

DefaultMutex::DefaultMutex()
{
	pthread_mutex_init(&_mutex, NULL);
}

DefaultMutex::DefaultMutex(bool recursive)
{
	if (recursive)
	{
		pthread_mutex_t recmutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
		_mutex = recmutex;
	}
	else
	{
		pthread_mutex_init(&_mutex, NULL);
	}
}

DefaultMutex::~DefaultMutex()
{
	pthread_mutex_destroy(&_mutex);
}

void DefaultMutex::lock()
{
	pthread_mutex_lock(&_mutex);
}

void DefaultMutex::unlock()
{
	pthread_mutex_unlock(&_mutex);
}

DefaultMutexLock::DefaultMutexLock(DefaultMutex &m) :
	_mutex(m)
{
	m.lock();
}

DefaultMutexLock::~DefaultMutexLock()
{
	_mutex.unlock();
}

#ifndef USE_SYNC
DefaultMutex* RefCnt::Private::_global_atomic_mutex = new DefaultMutex(false);
#endif

class RefCnt::Private
{
public:
	Private();
	Private(const Private &pvt);
	~Private();

	Private &operator = (const Private &pvt);

	int *__ref;
#ifndef USE_SYNC
	static DefaultMutex* _global_atomic_mutex;
#endif

};

RefCnt::Private::Private()
{
	__ref = new int;
	(*__ref) = 1;
}

RefCnt::Private::Private(const RefCnt::Private &pvt):
	__ref(pvt.__ref)
{
}

RefCnt::Private &RefCnt::Private::operator = (const RefCnt::Private &pvt)
{
	__ref = pvt.__ref;
	return *this;
}

RefCnt::Private::~Private()
{
	delete __ref;
}

RefCnt::RefCnt()
{
	__pvt = new Private;
}

RefCnt::RefCnt(const RefCnt &rc)
{
	__pvt = rc.__pvt;
	ref();
}

RefCnt::~RefCnt()
{
	unref();
}

RefCnt &RefCnt::operator = (const RefCnt &ref)
{
	ref.ref();
	unref();
	__pvt = ref.__pvt;
	return *this;
}

bool RefCnt::noref() const
{
#ifdef USE_SYNC
	return __sync_bool_compare_and_swap(__pvt->__ref, 0, 0);
#else
	assert(_global_atomic_mutex);
	Private::_global_atomic_mutex->lock();
	bool result = ((*__pvt->__ref) == 0);
	Private::_global_atomic_mutex->unlock();
	return result;
#endif
}

bool RefCnt::one() const
{
#ifdef USE_SYNC
	return __sync_bool_compare_and_swap(__pvt->__ref, 1, 1);
#else
	assert(Private::_global_atomic_mutex);
	Private::_global_atomic_mutex->lock();
	bool result = ((*__pvt->__ref) == 1);
	Private::_global_atomic_mutex->unlock();
	return result;
#endif
}

void RefCnt::ref() const
{
#ifdef USE_SYNC
	__sync_fetch_and_add(__pvt->__ref, 1);
#else
	assert(Private::_global_atomic_mutex);
	Private::_global_atomic_mutex->lock();
	(*__pvt->__ref)++;
	Private::_global_atomic_mutex->unlock();
#endif
}

void RefCnt::unref() const
{
	int old_ref;

#ifdef USE_SYNC
	old_ref = __sync_fetch_and_sub(__pvt->__ref, 1);
#else
	assert(Private::_global_atomic_mutex);
	Private::_global_atomic_mutex->lock();
	old_ref = (*__pvt->__ref)--;
	Private::_global_atomic_mutex->unlock();
#endif

	if (old_ref < 1)
	{
		debug_log("%p: refcount dropped below zero!", __pvt->__ref);
	}

	if (old_ref == 1)
	{
		delete __pvt;
	}
}

}
