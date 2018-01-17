/*
 *
 *
 *
 *  Copyright (C) 2011 Wyplay
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

#ifndef __WYTCHMAIN_H__
#define __WYTCHMAIN_H__

#define WYTCHMAIN_BUS_TYPE DBUS_BUS_SYSTEM
#define WYTCHMAIN_BUS_NAME "com.wyplay.wytchmain.b"

#include <dbus-c++/dbus.h>
#include <libwy/wylog.h>

#include <pthread.h>


/**
 *
 */
class WyThreadedDispatcher
{
public:
	/**
	 *
	 */
	WyThreadedDispatcher() : _thread_created(false)
	{

	}
	/**
	 *
	 */
	void start()
	{
		DBus::default_dispatcher = &_dispatcher;

		if (0 == pthread_create(&_thread, NULL, WyThreadedDispatcher::dispatch_entry, &_dispatcher)) {
			_thread_created = true;
		}

		//pthread_detach(dbusThread);
	}

	/**
	 *
	 */
	void join()
	{
		if (_thread_created) {
			pthread_join(_thread, NULL);
		}
	}

	/**
	 *
	 */
	DBus::Dispatcher & dispatcher()
	{
		return _dispatcher;
	}

private:
	/**
	 *
	 */
	static void * dispatch_entry(void * dispatcher)
	{
		if (0==dispatcher) {
			wl_critical("dispatch_entry: no dbus dispatcher");
			return 0;
		}

		DBus::Dispatcher * d = static_cast<DBus::Dispatcher *>(dispatcher);
		d->enter();

		return 0;
	}

private:
	DBus::BusDispatcher _dispatcher;
	pthread_t _thread;
	bool _thread_created;
};

#endif /* __WYTCHMAIN_H__ */
