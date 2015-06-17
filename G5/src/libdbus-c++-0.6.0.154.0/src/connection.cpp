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

#include <dbus-c++/debug.h>
#include <dbus-c++/connection.h>

#include <dbus/dbus.h>
#include <string>

#include "internalerror.h"

#include "connection_p.h"
#include "dispatcher_p.h"
#include "server_p.h"
#include "message_p.h"
#include "pendingcall_p.h"

using namespace DBus;

namespace DBus {

class Cond
{
public:
	Cond() {
		pthread_mutex_init(&_mutex, NULL);
		pthread_cond_init(&_cond, NULL);
	}

	~Cond() {
		pthread_mutex_destroy(&_mutex);
		pthread_cond_destroy(&_cond);
	}

	void lock() {
		pthread_mutex_lock(&_mutex);
	}

	void unlock() {
		pthread_mutex_unlock(&_mutex);
	}

	void signal() {
		pthread_cond_signal(&_cond);
	}

	void wait() {
		pthread_cond_wait(&_cond, &_mutex);
	}

private:
	pthread_mutex_t _mutex;
	pthread_cond_t  _cond;
};


class SyncCall
{
public:
	SyncCall(Cond &cond) :
		_pc(NULL), _cond(cond)
	{
		_pcp = new PendingCall::Private(NULL);

        {
		    Callback_Base<void, PendingCall&> * cb = new Callback<SyncCall, void, PendingCall&>(this, &SyncCall::on_reply);
		    _pcp->slot = cb;
        }

		_pc = new PendingCall(_pcp, false);

	}

	~SyncCall() {
		if (_pc)
			delete _pc;
	}

	Message reply() {
		return _pc->steal_reply();
	}

private:

	void on_reply(PendingCall &) {
		debug_log("signal reply (this=%p)", this);

		_cond.lock();

		_cond.signal();

		debug_log("signal sent (this=%p)", this);

		_cond.unlock();
	}

protected:
	PendingCall*          _pc;
	PendingCall::Private* _pcp;
	Cond&                 _cond;

friend class Connection;
};

};

Connection::Private::Private(DBusConnection *c, Server::Private *s, bool priv)
: conn(c) , dispatcher(0), server(s), _mutex(true), _priv(priv)
{
	init();
}

Connection::Private::Private(DBusBusType type)
: _mutex(true), _priv(true)
{
	InternalError e;

	conn = dbus_bus_get_private(type, e);

	if (e) throw Error(e);

	init();
}

Connection::Private::~Private()
{
	debug_log("terminating connection %p", conn);
	dbus_connection_remove_filter(conn, message_filter_stub, &disconn_filter);
	dbus_connection_set_dispatch_status_function(conn, NULL, NULL, NULL);
	dbus_connection_set_wakeup_main_function(conn, NULL, NULL, NULL);

	detach_server();

	disconnect();

	dbus_connection_unref(conn);
}

void Connection::Private::disconnect()
{
	if (_priv && dbus_connection_get_is_connected(conn))
	{
		std::vector<std::string>::iterator i = names.begin();

		while (i != names.end())
		{
			debug_log("%s: releasing bus name %s", dbus_bus_get_unique_name(conn), i->c_str());
			dbus_bus_release_name(conn, i->c_str(), NULL);
			++i;
		}
		dbus_connection_close(conn);
	}
}

void Connection::Private::init()
{
	if (_priv)
	{
        {
		    Callback_Base<bool, const Message &> * cb = new Callback<Connection::Private, bool, const Message &>(
				    this, &Connection::Private::disconn_filter_function
				    );
            disconn_filter = cb;
        }

		dbus_connection_add_filter(conn, message_filter_stub, &disconn_filter, NULL); // TODO: some assert at least
	}

	dbus_connection_set_dispatch_status_function(conn, dispatch_status_stub, this, 0);

	dbus_connection_set_wakeup_main_function(conn, on_wakeup, this, 0);

	dbus_connection_set_exit_on_disconnect(conn, false); //why was this set to true??
}

void Connection::Private::detach_server()
{
/*	Server::Private *tmp = server;

	server = NULL;

	if (tmp)
	{
		ConnectionList::iterator i;

		for (i = tmp->connections.begin(); i != tmp->connections.end(); ++i)
		{
			if (i->_pvt.get() == this)
			{
				tmp->connections.erase(i);
				break;
			}
		}
	}*/
}

bool Connection::Private::do_dispatch()
{
	debug_log("dispatching on %p", conn);

	if (!dbus_connection_get_is_connected(conn))
	{
		debug_log("connection terminated");

		detach_server();

		return true;
	}

	DefaultMutexLock lock(_mutex);
	return dbus_connection_dispatch(conn) != DBUS_DISPATCH_DATA_REMAINS;
}

void Connection::Private::dispatch_status_stub(DBusConnection *dc, DBusDispatchStatus status, void *data)
{
	Private *p = static_cast<Private *>(data);

	if (!dbus_connection_get_is_connected(dc))
		return;

	switch (status)
	{
		case DBUS_DISPATCH_DATA_REMAINS:
		debug_log("some dispatching to do on %p", dc);
		if (p->dispatcher)
		{
			p->dispatcher->queue_connection(p->conn);
			p->dispatcher->wake();
		}
		break;

		case DBUS_DISPATCH_COMPLETE:
		debug_log("all dispatching done on %p", dc);
		break;

		case DBUS_DISPATCH_NEED_MEMORY: //uh oh...
		debug_log("connection %p needs memory", dc);
		break;
	}
}

void Connection::Private::on_wakeup(void *data)
{
	debug_log("explicit wakeup %p", data);
	Private *p = static_cast<Private *>(data);

	if (!dbus_connection_get_is_connected(p->conn))
		return;

	if (p->dispatcher)
	{
		p->dispatcher->queue_connection(p->conn);
		p->dispatcher->wake();
	}
}


DBusHandlerResult Connection::Private::message_filter_stub(DBusConnection *conn, DBusMessage *dmsg, void *data)
{
	MessageSlot *slot = static_cast<MessageSlot *>(data);

	Message msg = Message(new Message::Private(dmsg));

	return slot && !slot->empty() && slot->call(msg) 
			? DBUS_HANDLER_RESULT_HANDLED
			: DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

bool Connection::Private::disconn_filter_function(const Message &msg)
{
	if (msg.is_signal(DBUS_INTERFACE_LOCAL,"Disconnected"))
	{
		debug_log("%p disconnected by local bus", conn);
		dbus_connection_close(conn);

		return true;
	}
	return false;
}

DBusDispatchStatus Connection::Private::dispatch_status()
{
	return dbus_connection_get_dispatch_status(conn);
}

bool Connection::Private::has_something_to_dispatch()
{
	return dispatch_status() == DBUS_DISPATCH_DATA_REMAINS;
}

void Connection::Private::pending_cond_notify(DBusPendingCall *, void *data)
{
	Cond *cond = static_cast<Cond *>(data);

	cond->lock();
	debug_log("signaling pending_call cond");
	cond->signal();
	cond->unlock();
}

void Connection::Private::pending_cond_free(void *data)
{
	Cond *cond = static_cast<Cond *>(data);

	debug_log("freeing pending_call cond %p", data);
	delete cond;
}


Connection Connection::SystemBus(Dispatcher* disp)
{
	return Connection(new Private(DBUS_BUS_SYSTEM), disp);
}

Connection Connection::SessionBus(Dispatcher* disp)
{
	return Connection(new Private(DBUS_BUS_SESSION), disp);
}

Connection Connection::ActivationBus(Dispatcher* disp)
{
	return Connection(new Private(DBUS_BUS_STARTER), disp);
}

Connection::Connection(const char *address, bool priv)
: _timeout(-1)
{
	InternalError e;
	DBusConnection *conn = priv 
		? dbus_connection_open_private(address, e)
		: dbus_connection_open(address, e);

	if (e) throw Error(e);

	_pvt = new Private(conn, NULL, priv);

	setup(default_dispatcher);

	debug_log("connected to %s", address);
}

Connection::Connection(Connection::Private *p, Dispatcher* disp)
: _pvt(p), _timeout(-1)
{
	setup(disp);
}

Connection::Connection(const Connection &c)
: _pvt(c._pvt), _timeout(c._timeout)
{
}

Connection &Connection::operator = (const Connection& c)
{
	if (this != &c)
	{
		_pvt = c._pvt;
	}
	return *this;
}

Connection::~Connection()
{
}

Dispatcher *Connection::setup(Dispatcher *dispatcher)
{
	debug_log("registering stubs for connection %p", _pvt->conn);

	if (!dispatcher) dispatcher = default_dispatcher;

	Dispatcher *prev = _pvt->dispatcher;

	_pvt->dispatcher = dispatcher;

	if (dispatcher)
	{
		dispatcher->queue_connection(_pvt->conn);

		dbus_connection_set_watch_functions(
				_pvt->conn,
				Dispatcher::Private::on_add_watch,
				Dispatcher::Private::on_rem_watch,
				Dispatcher::Private::on_toggle_watch,
				dispatcher,
				0
				);

		dbus_connection_set_timeout_functions(
				_pvt->conn,
				Dispatcher::Private::on_add_timeout,
				Dispatcher::Private::on_rem_timeout,
				Dispatcher::Private::on_toggle_timeout,
				dispatcher,
				0
				);
	}
	else if (prev)
	{
		dbus_connection_set_watch_functions(
				_pvt->conn,
				NULL, NULL, NULL,
				NULL, 0);

		dbus_connection_set_timeout_functions(
				_pvt->conn,
				NULL, NULL, NULL,
				NULL, 0);
	}

	return prev;
}

bool Connection::operator == (const Connection &c) const
{
	return _pvt->conn == c._pvt->conn;
}

bool Connection::register_bus()
{
	InternalError e;

	bool r = dbus_bus_register(_pvt->conn, e);
  
	if (e) throw (e);

	return r;
}

bool Connection::connected() const
{
	return dbus_connection_get_is_connected(_pvt->conn);
}

void Connection::disconnect()
{
//	dbus_connection_disconnect(_pvt->conn); // disappeared in 0.9x
	_pvt->disconnect();
}

void Connection::exit_on_disconnect(bool exit)
{
	dbus_connection_set_exit_on_disconnect(_pvt->conn, exit);
}

bool Connection::unique_name(const char *n)
{
	return dbus_bus_set_unique_name(_pvt->conn, n);
}

const char *Connection::unique_name() const
{
	return dbus_bus_get_unique_name(_pvt->conn);
}

void Connection::flush()
{
	dbus_connection_flush(_pvt->conn);
}

void Connection::add_match(const char *rule)
{
	InternalError e;

	dbus_bus_add_match(_pvt->conn, rule, e);

	debug_log("%s: added match rule %s", unique_name(), rule);

	if (e) throw Error(e);
}

void Connection::remove_match(const char *rule)
{
	InternalError e;
	
	dbus_bus_remove_match(_pvt->conn, rule, e);

	debug_log("%s: removed match rule %s", unique_name(), rule);

	if (e) throw Error(e);
}

bool Connection::add_filter(MessageSlot &s)
{
	DefaultMutexLock lock(_pvt->_mutex);
	debug_log("%s: adding filter", unique_name());
	return dbus_connection_add_filter(_pvt->conn, Private::message_filter_stub, &s, NULL);
}

void Connection::remove_filter(MessageSlot &s)
{
	DefaultMutexLock lock(_pvt->_mutex);
	debug_log("%s: removing filter", unique_name());
	dbus_connection_remove_filter(_pvt->conn, Private::message_filter_stub, &s);
}

bool Connection::send(const Message &msg, unsigned int *serial)
{
	bool ret;

	ret = dbus_connection_send(_pvt->conn, msg._pvt->msg, serial);

	return ret;
}

/*
 * This method has been kept for backward compatibility but might be replaced
 * definitely with send_blocking() Cond and SyncCall should also be removed
 */
Message Connection::send_blocking_wyplay(Message &msg, int timeout)
{
	Cond     cond;
	SyncCall call(cond);

	cond.lock();

	if(!dbus_connection_send_with_reply_and_notify(_pvt->conn, msg._pvt->msg, &(call._pcp->call),
						       PendingCall::Private::notify_stub, call._pc, NULL, timeout))
	{
		cond.unlock();
		throw ErrorNoMemory("Unable to start synchronous call");
	}

	flush();

	debug_log("sync call sent, waiting reply (this=%p)", this);

	cond.wait();

	cond.unlock();

	debug_log("pending call complete (this=%p)\n", this);

	Message reply = call.reply();

	if (reply.is_error()) {
		throw Error(reply);
	}

	return reply;
}

/*
 * This method will use the dispatcher if it has been defined and will dispatch
 * the message itself otherwise.
 */
Message Connection::send_blocking(Message &msg, int timeout)
{
	DBusMessage *reply;
	InternalError e;

	if (timeout == -1)
		timeout = this->_timeout;

	if (_pvt->dispatcher && !_pvt->dispatcher->in_dispatch())
	{
		DBusPendingCall *pending;

		if (!dbus_connection_send_with_reply(_pvt->conn, msg._pvt->msg,
					&pending, timeout))
			throw ErrorNoMemory("Unable to start synchronous call");

		Cond *cond = new Cond();
		cond->lock();

		if (!dbus_pending_call_set_notify(pending, Private::pending_cond_notify,
					cond, Private::pending_cond_free))
		{
			cond->unlock();
			delete cond;

			dbus_pending_call_unref(pending);
			throw ErrorNoMemory("Unable to set_notify on pending call");
		}

		if (!dbus_pending_call_get_completed(pending))
			cond->wait();
		reply = dbus_pending_call_steal_reply(pending);

		cond->unlock();

		dbus_pending_call_unref(pending);

		if (dbus_set_error_from_message (e, reply))
		{
			dbus_message_unref (reply);
			reply = NULL;
		}
	}
	else
		reply = dbus_connection_send_with_reply_and_block(_pvt->conn, msg._pvt->msg, timeout, e);

	if (e) throw Error(e);

	return Message(new Message::Private(reply), false);
}

PendingCall Connection::send_async(Message &msg, int timeout)
{
	DBusPendingCall *pending;

	if (!dbus_connection_send_with_reply(_pvt->conn, msg._pvt->msg, &pending, timeout))
	{
		throw ErrorNoMemory("Unable to start asynchronous call");
	}
	return PendingCall(new PendingCall::Private(pending));
}

PendingCall Connection::send_async_wyplay(Message & msg, PendingCallCallback * cb, AsyncContext * ctx, int timeout)
{
	PendingCall::Private * pcp = new PendingCall::Private(NULL);
	//pcp->slot = new Callback<Pending2Message, void, PendingCall &>(new Pending2Message(s), &Pending2Message::pending);
	pcp->slot = cb;
	PendingCall * pc = new PendingCall(pcp, false);

	if(!dbus_connection_send_with_reply_and_notify(_pvt->conn, msg._pvt->msg, &(pcp->call),
						       PendingCall::Private::notify_stub, pc, NULL, timeout))
	{
		throw ErrorNoMemory("Unable to start asynchronous call");
	}

    pc->data(ctx);

	debug_log("async call sent(this=%p)", this);

	return *pc;
}

void Connection::request_name(const char *name, int flags)
{
	InternalError e;

	debug_log("%s: registering bus name %s", unique_name(), name);

        /*
         * TODO:
         * Think about giving back the 'ret' value. Some people on the list
         * requested about this...
         */
	int ret = dbus_bus_request_name(_pvt->conn, name, flags, e);

	if (ret == -1)
	{
		if (e) throw Error(e);
	}

//	this->remove_match("destination");

	if (name)
	{
		_pvt->names.push_back(name);
		std::string match = "destination='" + _pvt->names.back() + "'";
		add_match(match.c_str());
	}
}

unsigned long Connection::sender_unix_uid(const char *sender)
{
    InternalError e;
    
    unsigned long ul = dbus_bus_get_unix_user(_pvt->conn, sender, e);
    
    if (e) throw Error(e);
    
    return ul;
}

bool Connection::has_name(const char *name)
{	
	InternalError e;

	bool b = dbus_bus_name_has_owner(_pvt->conn, name, e);

	if (e) throw Error(e);

	return b;
}

const std::vector<std::string>& Connection::names()
{
	return _pvt->names;
}

bool Connection::start_service(const char *name, unsigned long flags)
{
	InternalError e;

	bool b = dbus_bus_start_service_by_name(_pvt->conn, name, flags, NULL, e);

	if (e) throw Error(e);
	
	return b;
}

void Connection::set_timeout(int timeout)
{
	_timeout=timeout;
}
	
int Connection::get_timeout()
{
	return _timeout;
}

/* vim:set noexpandtab ts=8 sts=8 sw=8: */
