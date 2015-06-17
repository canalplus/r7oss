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

}
