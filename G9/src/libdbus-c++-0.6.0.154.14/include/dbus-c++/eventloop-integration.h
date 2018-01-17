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


#ifndef __DBUSXX_EVENTLOOP_INTEGRATION_H
#define __DBUSXX_EVENTLOOP_INTEGRATION_H

#include <pthread.h>
#include <errno.h>
#include "api.h"
#include "dispatcher.h"
#include "util.h"
#include "eventloop.h"

namespace DBus {

/* 
 * Glue between the event loop and the DBus library
 */

class BusDispatcher;
class Pipe;

/**
 * @brief Bridge between a Timeout and a DefaultTimeout class.
 */
class DXXAPI BusTimeout : public Timeout, public DefaultTimeout
{
    /**
     * @brief Constructor.
     *
     * @param[in] ti The libdbus C timeout opaque handle which it wraps.
     * @param[in] bd The pool based dispatcher to which it subscribes.
     */
	BusTimeout(Timeout::Internal * ti, BusDispatcher * bd);

	/**
	 * @brief Asks the dispatcher to update it's state based in the state received from libdbus C.
	 */
	void toggle();

	/**
	 * @brief unsubscribes from the dispatcher.
	 */
	void remove();

friend class BusDispatcher;
};

/**
 * @brief Bridge between a Watch and a DefaultWatch class.
 */
class DXXAPI BusWatch : public Watch, public DefaultWatch
{
    /**
     * @brief Constructor.
     *
     * @param[in] ti The libdbus C communication event opaque handle which it wraps.
     * @param[in] bd The pool based dispatcher to which it subscribes.
     */
	BusWatch(Watch::Internal *, BusDispatcher *);

    /**
     * @brief Asks the dispatcher to update it's state based in the state received from libdbus C.
     */
	void toggle();

    /**
     * @brief unsubscribes from the dispatcher.
     */
	void remove();

friend class BusDispatcher;
};

/**
 * @class BusDispatcher
 *
 * @brief Bridge between a Dispatcher and a DefaultMainLoop.
 */
class DXXAPI BusDispatcher : public Dispatcher, public DefaultMainLoop
{
public:
    /**
     * @name Initialization/Destruction
     * @{
     */
    /**
     * @brief Constructor.
     */
	BusDispatcher();

	/**
	 * @brief Destructor.
	 */
	~BusDispatcher() {}
	/**
	 * @}
	 */


	/**
	 * @name Dispatching loop control.
	 * @{
	 */

	/**
	 * @brief Implementation of the dispatching loop.
	 *
	 * This method will block the calling thread. The only way to interrupt it is by calling
	 * leave() from another thread.
	 */
	virtual void enter();

	/**
	 * @brief Ends the dispatching loop.
	 */
	virtual void leave();

	/**
	 * @brief Wakes the thread handling the dispatching loop.
	 *
	 * This method will wake the thread handling the dispatching loop, without demanding
	 * the end of the loop.
	 */
	virtual void wake();

	/**
	 * @}
	 */

	/**
	 * @cond INTERNAL_API
	 */
	virtual Pipe *add_pipe(void(*handler)(const void *data, void *buffer, unsigned int nbyte), const void *data);

	virtual void del_pipe (Pipe *pipe);
	/// @endcond

	/**
	 * @private
	 */
	virtual void do_iteration();

	/**
	 * @name Event set control implementation.
	 *
	 * These methods handle are called when libdbus C notifies the application of new timeout and communication events opaque handles.
	 * Accordingly, BusDispatcher creates bridge objects capable of wrapping the opaque handlers as well as interacting with a poll based
	 * dispatcher, in order to update the later of the changes in the set of events it has to supervise.
	 */
	/// @{
	virtual Timeout *add_timeout(Timeout::Internal *);

	virtual void rem_timeout(Timeout *);

	virtual Watch *add_watch(Watch::Internal *);

	virtual void rem_watch(Watch *);
	/// @}

	/**
	 * @name DefaultMainLoop event handlers.
	 * @brief Handle the timeout or communication events detected by DefaultMainLoop.
	 *
	 * These methods are wrapped as Callbacks which are attached to BusTimeout/BusWatch objects. When DefaultMainLoop will intercept an
	 * event corresponding to one of those objects, the callbacks are called.
	 */
	/// @{
	void watch_ready(DefaultWatch &);

	void timeout_expired(DefaultTimeout &);
	/// @}

	virtual bool in_dispatch();

protected:
	bool _running;
	pthread_t _tid;
	std::list <Pipe*> pipe_list;
};

} /* namespace DBus */

#endif//__DBUSXX_EVENTLOOP_INTEGRATION_H

/* vim:set noexpandtab sw=4 ts=4 sts=4: */
