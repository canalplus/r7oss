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

#include <linux/version.h>
#include <dbus-c++/eventloop.h>
#include <dbus-c++/debug.h>
#include <dbus-c++/error.h>
#include "internalerror.h"
#include <dbus-c++/refptr_impl.h>

#include <sys/poll.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <dbus/dbus.h>

using namespace DBus;
using namespace std;

bool operator >= (const struct timespec &t1, const struct timespec &t2)
{
	if (t1.tv_sec > t2.tv_sec) {
		return true;
	}
	else if (t1.tv_sec == t2.tv_sec) {
		return t1.tv_nsec >= t2.tv_nsec;
	}
	else
		return false;
}

struct timespec &addMilis(struct timespec &t, const int milis) {
	uint32_t nsec = t.tv_nsec + (milis % 1000) * 1000000;
	t.tv_nsec = nsec % 1000000000;
	t.tv_sec += milis / 1000 + ((nsec >= 1000000000)? 1 : 0);
	return t;
}

DefaultTimeout::DefaultTimeout(int interval, bool repeat, DefaultMainLoop *ed)
: _enabled(true), _interval(interval), _repeat(repeat), _data(0), _disp(ed)
{
	clock_gettime (CLOCK_MONOTONIC, &_expiration);

	addMilis(_expiration, interval);

	_disp->_mutex_t.lock();
	_disp->_timeouts.push_back(this);
	_disp->_mutex_t.unlock();
	_disp->wake();
}

DefaultTimeout::~DefaultTimeout()
{
}

void DefaultTimeout::enabled(bool e) {
	_enabled = e;
	if (e)
		_disp->wake();
}

DefaultWatch::DefaultWatch(int fd, int flags, DefaultMainLoop *ed)
: _enabled(true), _fd(fd), _flags(flags), _state(0), _data(0), _disp(ed)
{
	_disp->_mutex_w.lock();
	_disp->_watches.push_back(this);
	_disp->_mutex_w.unlock();
	_disp->wake();
}

void DefaultTimeout::remove()
{
	_disp->remove(this);
}

DefaultWatch::~DefaultWatch()
{
}

void DefaultWatch::enabled(bool e) {
	_enabled = e;
	if (e)
		_disp->wake();
}

void DefaultWatch::remove()
{
	_disp->remove(this);
}

DefaultMainLoop::DefaultMainLoop() :
	_mutex_w(true)
{
	// pipe to create a new fd used to unlock a dispatcher at any
	// moment (used by leave function)
	int ret = ::pipe(_fdunlock);
	if (ret == -1) throw Error("PipeError:errno", toString(errno).c_str());

	int flags = ::fcntl(_fdunlock[0], F_GETFL, 0);
	if (flags == -1) throw Error("PipeError:fcntl", toString(errno).c_str());

	flags |= O_NONBLOCK;
	::fcntl(_fdunlock[0], F_SETFL, flags);
}

DefaultMainLoop::~DefaultMainLoop()
{
	_mutex_w.lock();
	_watches.clear();
	_mutex_w.unlock();

	_mutex_t.lock();
	_timeouts.clear();
	_mutex_t.unlock();

	close(_fdunlock[1]);
	close(_fdunlock[0]);
}

void DefaultMainLoop::dispatch()
{
	_mutex_w.lock();

	// +1 for _fdunlock[0]
	int nfd = _watches.size() + 1;

	pollfd fds[nfd];

	DefaultWatches::iterator wi = _watches.begin();

	if (_fdunlock) {
		fds[0].fd = _fdunlock[0];
		fds[0].events = POLLIN;
		fds[0].revents = 0;
	}

	for (nfd = 1; wi != _watches.end(); ++wi)
	{
		if ((*wi)->enabled())
		{
			fds[nfd].fd = (*wi)->descriptor();
			fds[nfd].events = (*wi)->flags();
			fds[nfd].revents = 0;

			++nfd;
		}
	}

	_mutex_w.unlock();

	int wait_min = 10000;

	DefaultTimeouts::iterator ti;

	_mutex_t.lock();

	for (ti = _timeouts.begin(); ti != _timeouts.end(); ++ti)
	{
		if ((*ti)->enabled() && (*ti)->interval() < wait_min)
			wait_min = (*ti)->interval();
	}

	_mutex_t.unlock();

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,23)
	wait_min += 50; // Q&D workarround for old kernels
#endif
	poll(fds, nfd, wait_min);

	struct timespec now;
	clock_gettime (CLOCK_MONOTONIC, &now);

	_mutex_t.lock();

	ti = _timeouts.begin();

	while (ti != _timeouts.end())
	{
		RefPtrI<DefaultTimeout> to = *ti;
		/* increment here since the iterator might get corrupted when the lock
		 * is released */
		++ti;

		if (to->enabled() && now >= to->_expiration)
		{
			_mutex_t.unlock();
			to->expired(*to);
			_mutex_t.lock();

			if (to->_repeat)
			{
				to->_expiration = now;
				addMilis(to->_expiration, to->_interval);
			}

		}

	}

	_mutex_t.unlock();

	_mutex_w.lock();

	for (int j = 1; j < nfd; ++j)
	{
		DefaultWatches::iterator wi;

		for (wi = _watches.begin(); wi != _watches.end();)
		{
			RefPtrI<DefaultWatch> wa = *wi;
			/* increment here since the iterator might get corrupted when the
			 * lock is released */
			++wi;

			if (wa->enabled() && wa->_fd == fds[j].fd)
			{
				if (fds[j].revents)
				{
					wa->_state = fds[j].revents;

					_mutex_w.unlock();
					wa->ready(*wa);
					_mutex_w.lock();

					fds[j].revents = 0;
					break;
				}
			}
		}
	}
	// handle _fdunlock[0]
	if (fds[0].revents != 0) {
		char buff[10];
		while (read(fds[0].fd, buff, 10) == 10)
		{}
	}
	_mutex_w.unlock();
}

void DefaultMainLoop::wake()
{
	int ret = write(_fdunlock[1], "wake", strlen("wake"));
	if (ret == -1) throw Error("WriteError:errno", toString(errno).c_str());
}

void DefaultMainLoop::remove(DefaultWatch *watch)
{
	_mutex_w.lock();
	for (DefaultWatches::iterator it = _watches.begin(); it != _watches.end();)
		if (it->get() == watch)
		{
			_watches.erase(it++);
			/* we used to do remove() here, so don't break for the moment */
		}
		else
			++it;
	_mutex_w.unlock();
}

void DefaultMainLoop::remove(DefaultTimeout *timeout)
{
	debug_log("removing %p", timeout);
	_mutex_t.lock();
	for (DefaultTimeouts::iterator it = _timeouts.begin();
			it != _timeouts.end();)
		if (it->get() == timeout)
		{
			_timeouts.erase(it++);
			/* we used to do remove() here, so don't break for the moment */
		}
		else
			++it;
	_mutex_t.unlock();
}

/* vim:set noexpandtab sw=4 ts=4 sts=4: */
