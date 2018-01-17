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


#ifndef __DBUSXX_EVENTLOOP_H
#define __DBUSXX_EVENTLOOP_H

#include <list>
#include <time.h>

#include "api.h"
#include "util.h"

namespace DBus {

/*
 * these Default *classes implement a very simple event loop which
 * is used here as the default main loop, if you want to hook
 * a different one use the Bus *classes in eventloop-integration.h
 * or the Glib::Bus *classes as a reference
 */

class DefaultMainLoop;

/**
 * @class DefaultTimeout
 *
 * @brief Abstraction of a timeout event which works in conjunction with DefaultMainLoop.
 */

class DXXAPI DefaultTimeout
{
public:

    /**
     * @name Initialization/Destruction.
     */
    /// @{
	DefaultTimeout(int interval, bool repeat, DefaultMainLoop *);
	/// @}

	/**
	 * @name Setters/Getters of specific attributes.
	 */
	/// @{
	bool enabled(){ return _enabled; }
	void enabled(bool e);

	int interval(){ return _interval; }
	void interval(int i){ _interval = i; }

	bool repeat(){ return _repeat; }
	void repeat(bool r){ _repeat = r; }
	/// @}

	/**
	 * @name Callback.
	 */
	/// @{
	void *data(){ return _data; }
	void data(void *d){ _data = d; }

	Slot<void, DefaultTimeout &> expired;
	/// @}

	void remove();
protected:
    /**
     * @name Initialization/Destruction.
     */
    /// @{
	virtual ~DefaultTimeout();
	/// @}

private:

	bool _enabled;

	int _interval;
	bool _repeat;

	struct timespec _expiration;

	void *_data;

	DefaultMainLoop *_disp;

friend class DefaultMainLoop;
friend class RefPtrI<DefaultTimeout>;
};

typedef std::list< RefPtrI<DefaultTimeout> > DefaultTimeouts;

/**
 * @class DefaultWatch
 *
 * @brief Abstraction of a communication event which works in conjunction with DefaultMainLoop.
 */
class DXXAPI DefaultWatch
{
public:

    /**
     * @name Initialization/Destruction.
     */
    /// @{
	DefaultWatch(int fd, int flags, DefaultMainLoop *);
	/// @}

	/**
	 * @name Setters/Getters of specific attributes.
	 */
	/// @{
	bool enabled(){ return _enabled; }
	void enabled(bool e);

	int descriptor(){ return _fd; }

	int flags(){ return _flags; }
	void flags(int f){ _flags = f; }
	/// @}

	int state(){ return _state; }

	void invalidate() { _enabled=false; _fd=-1; }

	/**
	 * @name Callback.
	 */
	/// @{
	void *data(){ return _data; }
    void data(void *d){ _data = d; }

	Slot<void, DefaultWatch &> ready;
	/// @}

	void remove();
protected:
    /**
     * @name Initialization/Destruction.
     */
    /// @{
	virtual ~DefaultWatch();
	/// @}

private:

	bool _enabled;

	int _fd;
	int _flags;
	int _state;

	void *_data;

	DefaultMainLoop *_disp;

friend class DefaultMainLoop;
friend class RefPtrI<DefaultWatch>;
};

typedef std::list< RefPtrI<DefaultWatch> > DefaultWatches;

/**
 * @class DefaultMainLoop.
 *
 * @brief Poll-based event handler.
 *
 * This is a thread-safe implementation of a poll based event handler. It works with
 * an externally provided, modifiable, set of file descriptors, as well as with a set
 * of timeouts.
 */
class DXXAPI DefaultMainLoop
{
public:

    /**
     * @name Initialization/Destruction.
     */
    /// @{
    /**
     * @brief Constructor
     */
	DefaultMainLoop();

	/**
	 * @brief Destructor.
	 */
	virtual ~DefaultMainLoop();
	/// @}

	/**
	 * @brief Executes a event listening cycle.
	 *
	 * This method waits for events on the available set of file descriptors, or for
	 * one of the configured timeouts to expire.
	 *
	 * This methid will block the caller's thread, and it can be interrupted with a call
	 * if wake from a different thread in the absence of any activity on a file descriptor
	 * or a timeout expiration.
	 */
	virtual void dispatch();

protected:
	/**
	 * @brief Signals the thread executing dispatch() to wake up and finish the on-going dispatch cycle.
	 */
	void wake();

	/**
	 * @brief Removes the watch given as argument from the set handled by dispatch.
	 */
	void remove(DefaultWatch *);

	/**
	 * @brief Removes the timeout given as argument from dispatch processing.
	 */
	void remove(DefaultTimeout *);

	int _fdunlock[2];

private:

	DefaultMutex _mutex_t;
	DefaultTimeouts _timeouts;

	DefaultMutex _mutex_w;
	DefaultWatches _watches;
	bool _watches_removed;

friend class DefaultTimeout;
friend class DefaultWatch;
};

} /* namespace DBus */

#endif//__DBUSXX_EVENTLOOP_H

/* vim:set noexpandtab sw=4 ts=4 sts=4: */
