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

/* Project */
#include <dbus-c++/eventloop-integration.h>
#include <dbus-c++/debug.h>
#include <dbus-c++/pipe.h>

/* DBus */
#include <dbus/dbus.h>

/* STD */
#include <string.h>
#include <cassert>
#include <sys/poll.h>
#include <fcntl.h>
#include <pthread.h>

using namespace DBus;
using namespace std;

BusTimeout::BusTimeout(Timeout::Internal *ti, BusDispatcher *bd)
: Timeout(ti), DefaultTimeout(Timeout::interval(), true, bd)
{
	if (!Timeout::enabled())
		DefaultTimeout::enabled(false);
}

void BusTimeout::toggle()
{
	debug_log("timeout %p toggled (%s)", this, Timeout::enabled() ? "on":"off");

	DefaultTimeout::enabled(Timeout::enabled());
}

void BusTimeout::remove()
{
	DefaultTimeout::remove();
}

BusWatch::BusWatch(Watch::Internal *wi, BusDispatcher *bd)
: Watch(wi),
  DefaultWatch(Watch::descriptor(), POLLHUP | POLLERR |
		  ((Watch::flags() & DBUS_WATCH_READABLE) ? POLLIN : 0) |
		  ((Watch::flags() & DBUS_WATCH_WRITABLE) ? POLLOUT : 0), bd)
{
	if (!Watch::enabled())
		DefaultWatch::enabled(false);
}

void BusWatch::toggle()
{
	debug_log("watch %p toggled (%s)", this, Watch::enabled() ? "on":"off");

	DefaultWatch::enabled(Watch::enabled());
}

void BusWatch::remove()
{
	DefaultWatch::remove();
}

BusDispatcher::BusDispatcher() :
	_running(false)
{
}

void BusDispatcher::enter()
{
	debug_log("entering dispatcher %p", this);

	_running = true;
	_tid = pthread_self();

	while (_running)
	{
		do_iteration();
	}

	debug_log("leaving dispatcher %p", this);
}

void BusDispatcher::leave()
{
	_running = false;

	wake();
}

void BusDispatcher::wake()
{
	DefaultMainLoop::wake();
}

bool BusDispatcher::in_dispatch()
{
	return _running && (pthread_equal(pthread_self(), _tid) != 0);
}

Pipe *BusDispatcher::add_pipe(void(*handler)(const void *data, void *buffer, unsigned int nbyte), const void *data)
{
  Pipe *new_pipe = new Pipe (handler, data);
	pipe_list.push_back (new_pipe);

	return new_pipe;
}

void BusDispatcher::del_pipe (Pipe *pipe)
{
	pipe_list.remove (pipe);
	delete pipe;
}

void BusDispatcher::do_iteration()
{
	dispatch_pending();
	dispatch();

	for (std::list <Pipe*>::iterator p_it = pipe_list.begin ();
			p_it != pipe_list.end ();
			++p_it)
	{
		Pipe* read_pipe = *p_it;
		char buffer[1024]; // TODO: should be max pipe size
		unsigned int nbytes = 0;

		while (read_pipe->read(buffer, nbytes) > 0)
		{
			read_pipe->_handler (read_pipe->_data, buffer, nbytes);
		}
	}
}

Timeout *BusDispatcher::add_timeout(Timeout::Internal *ti)
{
	BusTimeout *bt = new BusTimeout(ti, this);

    {
        Callback_Base<void, DefaultTimeout &> * cb = new Callback<BusDispatcher, void, DefaultTimeout &>(this, &BusDispatcher::timeout_expired);
        bt->expired = cb;
    }
	bt->data(bt);

	debug_log("added timeout %p (%s) (%d millies)",
		  bt,
		  ((Timeout*)bt)->enabled() ? "on":"off",
		  ((Timeout*)bt)->interval()
		  );

	return bt;
}

void BusDispatcher::rem_timeout(Timeout *t)
{
	debug_log("removed timeout %p", t);

	t->remove();
}

Watch *BusDispatcher::add_watch(Watch::Internal *wi)
{
	BusWatch *bw = new BusWatch(wi, this);

    {
	    Callback_Base<void, DefaultWatch &> * cb = new Callback<BusDispatcher, void, DefaultWatch &>(this, &BusDispatcher::watch_ready);
        bw->ready = cb; 
    }
	bw->data(bw);

	debug_log("added watch %p (%s) fd=%d flags=%d",
		bw, ((Watch *)bw)->enabled() ? "on":"off", ((Watch *)bw)->descriptor(), ((Watch *)bw)->flags());

	return bw;
}

void BusDispatcher::rem_watch(Watch *w)
{
	debug_log("removed watch %p", w);

	w->remove();
}

void BusDispatcher::timeout_expired(DefaultTimeout &et)
{
	debug_log("timeout %p expired", &et);

	BusTimeout *timeout = reinterpret_cast<BusTimeout *>(et.data());

	timeout->handle();
}

void BusDispatcher::watch_ready(DefaultWatch &ew)
{
	BusWatch *watch = reinterpret_cast<BusWatch *>(ew.data());

	debug_log("watch %p ready, flags=%d state=%d",
		watch, ((Watch *)watch)->flags(), watch->state()
	);

	int flags = 0;

	if (watch->state() & POLLIN)
		flags |= DBUS_WATCH_READABLE;
	if (watch->state() & POLLOUT)
		flags |= DBUS_WATCH_WRITABLE;
	if (watch->state() & POLLHUP)
		flags |= DBUS_WATCH_HANGUP;
	if (watch->state() & POLLERR)
		flags |= DBUS_WATCH_ERROR;

	watch->handle(flags);
}

/* vim:set noexpandtab sw=4 ts=4 sts=4: */
