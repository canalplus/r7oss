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
#include <dbus-c++/object.h>
#include "internalerror.h"

#include <cstring>
#include <map>
#include <dbus/dbus.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "message_p.h"
#include "server_p.h"
#include "connection_p.h"

extern "C" {

#include <sys/queue.h>
#include <setjmp.h>
#include <semaphore.h>
#include <errno.h>

#ifdef POOL_THREAD_DEBUG
#include <stdio.h>
#define dbg(fmt, args...) fprintf(stderr, fmt, ##args)
#else
static void dbg(const char *fmt, ...) { }
#endif

#ifndef POOL_THREAD_MAX
#define POOL_THREAD_MAX 40
#endif

#ifndef POOL_THREAD_KEEPALIVE
#define POOL_THREAD_KEEPALIVE 5
#endif

typedef struct pool_s pool_t;

typedef struct thd_s {
	LIST_ENTRY(thd_s) entry;
	sem_t sem;
	sem_t fini_sem;
	pthread_t tid;
	void *(*func) (void *);
	void *arg, *ret;
	pool_t *pool;
	int detached;
	jmp_buf exit;
} thd_t;

struct pool_s {
	LIST_HEAD(, thd_s) pool;
	size_t size;
};

static pool_t pool = { LIST_HEAD_INITIALIZER(pool), 0 };
static size_t thread_count = 0;

static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static LIST_HEAD(, thd_s) thd_running[256];

static pthread_mutex_t objects_state_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline uint8_t hash(pthread_t tid)
{
	uint32_t x = (uint32_t) tid;
	//printf("thd hash %d\n", (uint8_t) (x ^ (x >> 8) ^ (x >> 16) ^ (x >> 24)));
	return x ^ (x >> 8) ^ (x >> 16) ^ (x >> 24);
}

static void free_reusable_thread(thd_t *t)
{
	dbg("_pool_ destroying thread\n");
	sem_destroy(&t->sem);
	sem_destroy(&t->fini_sem);
	::free(t);
}

static void *reusable_thread(thd_t *t)
{
	int loop = 0;
	for ( ; ; ) {
		dbg("_pool_ thd %lx: waiting START\n", t->tid);
		while (sem_wait(&t->sem) == -1 && errno == EINTR) {
			dbg("_pool_ sem_wait(sem) got EINTR");
		}
		dbg("_pool_ thd %lx: awaken with task %p %p (thread reused #%d)\n", t->tid, t->func, t->arg, loop++);
		if (setjmp(t->exit))
			dbg("_pool_ jump !!\n");
		else
			t->ret = t->func(t->arg);
		dbg("_pool_ thd %lx: task finished, returns %p\n", t->tid, t->ret);

		sem_post(&t->fini_sem);
		dbg("_pool_ thd %lx: waiting JOIN\n", t->tid);
		while (sem_wait(&t->sem) == -1 && errno == EINTR) {
			dbg("_pool_ sem_wait(sem) got EINTR(2)");
		}
		dbg("_pool_ thd %lx: JOIN done\n", t->tid);

		if (t->detached) {
			while (sem_wait(&t->fini_sem) == -1 && errno == EINTR) {
				dbg("_pool_ sem_wait(fini_sem) got EINTR");
			}
			dbg("_pool_ thd %lx: DETACHED\n", t->tid);
		}

		/* put back the thread in pool */
		pthread_mutex_lock(&pool_mutex);
		LIST_REMOVE(t, entry);
		if (t->pool->size < POOL_THREAD_KEEPALIVE) {
			LIST_INSERT_HEAD(&t->pool->pool, t, entry);
			++(t->pool->size);
			pthread_mutex_unlock(&pool_mutex);
		} else {
			free_reusable_thread(t);
			--thread_count;
			pthread_mutex_unlock(&pool_mutex);
			pthread_detach(pthread_self());
			pthread_exit(NULL);
		}
	}

	return NULL;
}

static thd_t *new_reusable_thread()
{
	thd_t *t = (thd_t*)::malloc(sizeof(thd_t));
	static int nb;

	dbg("_pool_ new real thread #%d\n", ++nb);

	sem_init(&t->sem, 0, 0);
	sem_init(&t->fini_sem, 0, 0);
	if (pthread_create(&t->tid, NULL, (void* (*)(void*))reusable_thread, t)) {
		::free(t);
		return NULL;
	}

	return t;
}

static thd_t *get_reusable_thread()
{
	thd_t *res;

	pthread_mutex_lock(&pool_mutex);
	res = LIST_FIRST(&pool.pool);
	if (res) {
		LIST_REMOVE(res, entry);
		--(pool.size);
	} else if (thread_count < POOL_THREAD_MAX) {
		if ((res = new_reusable_thread()) != NULL)
			++thread_count;
	} else
		res = NULL;
	if (res == NULL) {
		dbg("%s: No reuseable thread\n", __func__);
		pthread_mutex_unlock(&pool_mutex);
		return NULL;
	}
	LIST_INSERT_HEAD(&thd_running[hash(res->tid)], res, entry);
	res->pool = &pool;
	dbg("_pool_ size:%i total:%i\n", pool.size, thread_count);
	pthread_mutex_unlock(&pool_mutex);

	return res;
}

static thd_t *find_thread(pthread_t tid)
{
	thd_t *res;
	pthread_mutex_lock(&pool_mutex);
	LIST_FOREACH(res, &thd_running[hash(tid)], entry)
		if (res->tid == tid)
			break;
	pthread_mutex_unlock(&pool_mutex);
	return res;
}

static int _pool_pthread_create (pthread_t *__restrict __newthread,
			     __const pthread_attr_t *__restrict __attr,
			     void *(*__start_routine) (void *),
			     void *__restrict __arg)
{
	dbg("_pool_ pthread_create\n");

	if (__attr)
	{
		dbg("_pool_ pthread_create ERROR : non NULL __attr\n");
		return 1; // TODO : Should be EINVAL
	}

	thd_t *t = get_reusable_thread();
	if (t == NULL) {
		dbg("%s: No reuseable thread\n", __func__);
		return 1;
	}
	t->func = __start_routine;
	t->arg = __arg;
	t->detached = 0;

	sem_post(&t->sem);
	dbg("_pool_ --> %lx %p\n", t->tid, t);

	*__newthread = t->tid;
	return 0;
}

static int _pool_pthread_detach (pthread_t __th)
{
	thd_t *t = find_thread(__th);
	dbg("_pool_ pthread_detach %lx (%p)\n", __th, t);
	if (t->detached)
		return -1;
	t->detached = 1;
	sem_post(&t->sem);
	return 0;
}

static void _pool_pthread_exit(void *__retval)
{
	thd_t *t = find_thread(pthread_self());
	dbg("_pool_ pthread_exit !!!!!!!!!! (%p)\n", t);
	if (t) {
		t->ret = __retval;
		longjmp(t->exit, 1);
	} else
		pthread_exit(__retval);
}

} // extern "C"

using namespace DBus;

Object::Object(Connection &conn, const Path &path, const char *service)
: _conn(conn), _path(path), _service(service ? service : ""), _default_timeout(-1),
  _canceled(false), _busy_count(0)
{
}

Object::~Object()
{
}

void Object::set_timeout(int new_timeout)
{
	debug_log("%s: %d millies", __PRETTY_FUNCTION__, new_timeout);
	if(new_timeout < 0 && new_timeout != -1)
		throw ErrorInvalidArgs("Bad timeout, cannot set it");
	_default_timeout = new_timeout;
}

void Object::path(const Path & p)
{
    _path = p;
}

bool Object::canceled()
{
    bool r = false;
//    debug_log("COCO: Object(0x%x) pre canceled: %d", this, _canceled);
    pthread_mutex_lock(&objects_state_mutex);
//    debug_log("COCO: Object(0x%x) canceled: %d", this, _canceled);
    r = _canceled;
    pthread_mutex_unlock(&objects_state_mutex);

    return r;
}

void Object::cancel()
{
//    debug_log("COCO: Object(0x%x) pre cancel count: %d", this, _busy_count);
    pthread_mutex_lock(&objects_state_mutex);
//    debug_log("COCO: Object(0x%x) cancel count %d", this, _busy_count);
    _canceled = true;
    pthread_mutex_unlock(&objects_state_mutex);
}

int Object::busy_count()
{
    int r = 0;
//    debug_log("COCO: Object(0x%x) pre busy_count: %d", this, _busy_count);
    pthread_mutex_lock(&objects_state_mutex);
//    debug_log("COCO: Object(0x%x) busy_count: %d", this, _busy_count);
    r = _busy_count;
    pthread_mutex_unlock(&objects_state_mutex);

    return r;
}

bool Object::busy_inc()
{
//    debug_log("COCO: Object(0x%x) pre busy_inc: %d+1", this, _busy_count);
    pthread_mutex_lock(&objects_state_mutex);
//    debug_log("COCO: Object(0x%x) busy_count: %d+1", this, _busy_count);
    if (_canceled) {
        pthread_mutex_unlock(&objects_state_mutex);
        return false;
    }
    ++_busy_count;
    pthread_mutex_unlock(&objects_state_mutex);

    return true;
}

void Object::busy_dec()
{
//    debug_log("COCO: Object(0x%x) pre busy_dec: %d-1", this, _busy_count);
    pthread_mutex_lock(&objects_state_mutex);
//    debug_log("COCO: Object(0x%x) busy_count: %d-1", this, _busy_count);
    --_busy_count;
    pthread_mutex_unlock(&objects_state_mutex);
}


struct ObjectAdaptor::Private
{
	static void unregister_function_stub(DBusConnection *, void *);
	static DBusHandlerResult message_function_stub(DBusConnection *, DBusMessage *, void *);
	static void *method_handler_launcher_thread(void *);
};

static DBusObjectPathVTable _vtable =
{	
	ObjectAdaptor::Private::unregister_function_stub,
	ObjectAdaptor::Private::message_function_stub,
	NULL, NULL, NULL, NULL
};

void ObjectAdaptor::Private::unregister_function_stub(DBusConnection *conn, void *data)
{
 	//TODO: what do we have to do here ?
}

DBusHandlerResult ObjectAdaptor::Private::message_function_stub(DBusConnection *, DBusMessage *dmsg, void *data)
{
	ObjectAdaptor *o = static_cast<ObjectAdaptor *>(data);

	if (o)
	{
		Message msg(new Message::Private(dmsg));	

		debug_log("in object %s", o->path().c_str());
		debug_log(" got message #%d from %s to %s",
			msg.serial(),
			msg.sender(),
			msg.destination()
		);

		return o->handle_message(msg)
			? DBUS_HANDLER_RESULT_HANDLED
			: DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	else
	{
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
}

typedef std::map<Path, ObjectAdaptor *> ObjectAdaptorTable;
static ObjectAdaptorTable _adaptor_table;

ObjectAdaptor *ObjectAdaptor::from_path(const Path &path)
{
	ObjectAdaptorTable::iterator ati = _adaptor_table.find(path);

	if (ati != _adaptor_table.end())
		return ati->second;

	return NULL;
}

ObjectAdaptorPList ObjectAdaptor::from_path_prefix(const std::string &prefix)
{
	ObjectAdaptorPList ali;

	ObjectAdaptorTable::iterator ati = _adaptor_table.begin();

	size_t plen = prefix.length();

	while (ati != _adaptor_table.end())
	{
		if (!strncmp(ati->second->path().c_str(), prefix.c_str(), plen) && (prefix!=ati->second->path()))
			ali.push_back(ati->second);

		++ati;
	}

	return ali;
}

ObjectPathList ObjectAdaptor::child_nodes_from_prefix(const std::string &prefix)
{
	ObjectPathList ali;

	ObjectAdaptorTable::iterator ati = _adaptor_table.begin();

	size_t plen = prefix.length();

	while (ati != _adaptor_table.end())
	{
	  if (!strncmp(ati->second->path().c_str(), prefix.c_str(), plen))
		{
				std::string p = ati->second->path().substr(plen);
				p = p.substr(0,p.find('/'));
				ali.push_back(p);
		}
		++ati;
	}

	ali.sort();
	ali.unique();

	return ali;
}

ObjectAdaptor::ObjectAdaptor(Connection &conn, const Path &path)
: Object(conn, path, conn.unique_name())
{
	register_obj();
}

ObjectAdaptor::~ObjectAdaptor()
{
    cancel();

	unregister_obj();

	bool busy = false;
	do {
	    busy = (busy_count() != 0);
	    if (busy) {
	        usleep(100000);
	    }
	} while(busy);
}

void ObjectAdaptor::register_obj()
{
	debug_log("registering local object %s", path().c_str());

	if (!dbus_connection_register_object_path(conn()._pvt->conn, path().c_str(), &_vtable, this))
	{
 		throw ErrorNoMemory("unable to register object path");
	}

	_adaptor_table[path()] = this;
}

void ObjectAdaptor::unregister_obj()
{
	_adaptor_table.erase(path());

	debug_log("unregistering local object %s", path().c_str());

	dbus_connection_unregister_object_path(conn()._pvt->conn, path().c_str());
}

InterfaceAdapterPList & ObjectAdaptor::interfaces(InterfaceAdapterPList & ifs) const
{
    for (InterfaceAdaptorTable::const_iterator it = _interfaces.begin() ; it!=_interfaces.end() ; ++it) {
        ifs.push_back(it->second);
    }

    return ifs;
}

void ObjectAdaptor::_emit_signal(SignalMessage &sig)
{
	sig.path(path().c_str());

	conn().send(sig);
}

struct ReturnLaterError
{
	const Tag *tag;
};

class MethodHandlerParam
{
public:
	MethodHandlerParam(ObjectAdaptor *a,
			    InterfaceAdaptor *i, 
			    const CallMessage &c) :
		adaptor(a), iface(i), cmsg(c)
	{}

	ObjectAdaptor*    adaptor;
	InterfaceAdaptor* iface;
	CallMessage       cmsg;
};

void* ObjectAdaptor::Private::method_handler_launcher_thread(void *p)
{
	MethodHandlerParam* param = reinterpret_cast<MethodHandlerParam*>(p);

	debug_log("launching method handler");

	param->adaptor->method_handler_launcher(param->iface, param->cmsg);

	debug_log("method handler exiting ...");

	delete param;

	_pool_pthread_exit(NULL);
}

void ObjectAdaptor::method_handler_launcher(InterfaceAdaptor *ii, const CallMessage &cmsg)
{
	try
	{
		Message ret = ii->dispatch_method(cmsg);
		conn().send(ret);
	}
	catch(Error& e)
	{
		ErrorMessage em(cmsg, e.name(), e.message());
		conn().send(em);
	}
	catch (ReturnLaterError& rle)
	{
		_continuations[rle.tag] = new Continuation(conn(), cmsg, rle.tag);
	}
}

bool ObjectAdaptor::handle_message(const Message &msg)
{
	switch (msg.type())
	{
		case DBUS_MESSAGE_TYPE_METHOD_CALL:
		{
			const CallMessage &cmsg = reinterpret_cast<const CallMessage &>(msg);
			const char *member      = cmsg.member();
			const char *interface   = cmsg.interface();

			debug_log(" invoking method %s.%s", interface, member);

			InterfaceAdaptor *ii = find_interface(interface);
			if (ii)
			{
				pthread_t handler;

				MethodHandlerParam* param = new MethodHandlerParam(this, ii, cmsg);

				if (_pool_pthread_create(&handler, NULL,
						   Private::method_handler_launcher_thread, param)) {

					// TODO: exception?
					debug_log("failed to create method handler");
					delete param;
					return false;
				}

				if (_pool_pthread_detach(handler)) {
					debug_log("failed to detach method handler");
					return false;
				}

				return true;
			}
			else
			{
				return false;
			}
		}
		default:
		{
			return false;
		}
	}
}

void ObjectAdaptor::return_later(const Tag *tag)
{
	ReturnLaterError rle = { tag };
	throw rle;
}

void ObjectAdaptor::return_now(Continuation *ret)
{
	ret->_conn.send(ret->_return);

	ContinuationMap::iterator di = _continuations.find(ret->_tag);

	delete di->second;

	_continuations.erase(di);
}

void ObjectAdaptor::return_error(Continuation *ret, const Error error)
{
	ret->_conn.send(ErrorMessage(ret->_call, error.name(), error.message()));

	ContinuationMap::iterator di = _continuations.find(ret->_tag);

	delete di->second;

	_continuations.erase(di);
}

ObjectAdaptor::Continuation *ObjectAdaptor::find_continuation(const Tag *tag)
{
	ContinuationMap::iterator di = _continuations.find(tag);

	return di != _continuations.end() ? di->second : NULL;
}

ObjectAdaptor::Continuation::Continuation(Connection &conn, const CallMessage &call, const Tag *tag)
: _conn(conn), _call(call), _return(_call), _tag(tag)
{
	_writer = _return.writer(); //todo: verify
}

/*
*/

struct ObjectProxy::Private
{
	static void *sighandler_launcher(void *);
	static void *replyhandler_launcher(void *);
};

ObjectProxy::ObjectProxy(Connection &conn, const Path &path, const char *service, bool threaded_signal)
: Object(conn, path, service),
  _threaded_signal(threaded_signal)
{
	register_obj();
}

ObjectProxy::ObjectProxy(Connection &conn, const Path &path, const char *service)
: Object(conn, path, service),
  _threaded_signal(true)
{
	register_obj();
}

ObjectProxy::~ObjectProxy()
{
    cancel();

    unregister_obj();

    bool busy = false;
    do {
        busy = (busy_count() != 0);
        if (busy) {
            usleep(100000);
        }
    } while(busy);

//    debug_log("COCO: destroying object proxy %p", this);
}

void ObjectProxy::change_path(const Path & path)
{
    unregister_obj();
    Object::path(path);
    register_obj();
}

void ObjectProxy::register_obj()
{
	debug_log("registering remote object %s", path().c_str());

    {
	    Callback_Base<bool, const Message &> * cb = new Callback<ObjectProxy, bool, const Message &>(this, &ObjectProxy::handle_message);
	    _filtered = cb;
    }
	
	conn().add_filter(_filtered);

    for (InterfaceProxyTable::const_iterator ii = _interfaces.begin() ; ii != _interfaces.end() ; ++ii) {
        if (ii->second->is_legacy_mode()) {
            _enable_signal(*(ii->second), NULL);
        }
        else {
            const EnabledSignalsList & enabled_signals = ii->second->get_pre_enabled_signals();
            for (EnabledSignalsList::const_iterator signal = enabled_signals.begin() ; signal != enabled_signals.end() ; ++signal) {
                _enable_signal(*(ii->second), signal->c_str());
            }
        }
    }
}

void ObjectProxy::unregister_obj()
{
	debug_log("unregistering remote object %s", path().c_str());
	
    for (InterfaceProxyTable::const_iterator ii = _interfaces.begin() ; ii != _interfaces.end() ; ++ii) {
        if (ii->second->is_legacy_mode()) {
            _disable_signal(*(ii->second), NULL);
        }
        else {
            const EnabledSignalsList & enabled_signals = ii->second->get_pre_enabled_signals();
            for (EnabledSignalsList::const_iterator signal = enabled_signals.begin() ; signal != enabled_signals.end() ; ++signal) {
                _disable_signal(*(ii->second), signal->c_str());
            }
        }
    }

	conn().remove_filter(_filtered);
}

void ObjectProxy::_enable_signal(const InterfaceProxy & ii, const char * member)
{
    std::string im = "type='signal',path='" + path() + "',interface='" + ii.name() + "'";
    if (0 != member) {
        im = im + ",member='" + member + "'";
    }
    conn().add_match(im.c_str());
}

void ObjectProxy::_disable_signal(const InterfaceProxy & ii, const char * member)
{
    std::string im = "type='signal',path='" + path() + "',interface='" + ii.name() + "'";
    if (0 != member) {
        im = im + ",member='" + member + "'";
    }
    conn().remove_match(im.c_str());
}

Message ObjectProxy::_invoke_method(CallMessage &call)
{
	if (call.path() == NULL)
		call.path(path().c_str());

	if (call.destination() == NULL)
		call.destination(service().c_str());

	return conn().send_blocking_wyplay(call, get_timeout());
}

bool ObjectProxy::_invoke_method_noreply(CallMessage &call)
{
	if (call.path() == NULL)
		call.path(path().c_str());

	if (call.destination() == NULL)
		call.destination(service().c_str());

	return conn().send(call);
}


bool ObjectProxy::_invoke_method_async(CallMessage & call, void * udata, bool on_thread_pool)
{
	if (call.path() == NULL)
		call.path(path().c_str());

	if (call.destination() == NULL)
		call.destination(service().c_str());

	Callback_Base<void, PendingCall&> * reply_cb =
	        new Callback<ObjectProxy, void, PendingCall&>(
	                this,
	                &ObjectProxy::handle_async_reply);

	conn().send_async_wyplay(call, reply_cb,
	        new AsyncContext(call.interface(), call.member(), udata, on_thread_pool),
	        get_timeout());

	return true;
}

class SignalHandlerParam
{
public:
	SignalHandlerParam(ObjectProxy * o, InterfaceProxy *p, const SignalMessage &m):
		object(o), proxy(p), smsg(m)
	{}


	ObjectProxy * object;
	InterfaceProxy* proxy;
	SignalMessage   smsg;
};

void* ObjectProxy::Private::sighandler_launcher(void *p)
{
	SignalHandlerParam* param = reinterpret_cast<SignalHandlerParam*>(p);

//	debug_log("COCO: Object(0x%x) handling signal ...", param->proxy);
	param->proxy->dispatch_signal(param->smsg);

//    debug_log("COCO: Object(0x%x) decrementing ...", param->proxy);
	param->object->busy_dec();
//	debug_log("COCO: Object(0x%x) handled signal ...", param->proxy);

	delete param;

	_pool_pthread_exit(NULL);
}

class AsyncReplyHandlerParam
{
public:
    AsyncReplyHandlerParam(InterfaceProxy *p, const ReturnMessage & rm, AsyncContext * c):
        proxy(p), rmsg(rm), ctx(c)
    {}

    InterfaceProxy * proxy;
    ReturnMessage rmsg;
    AsyncContext * ctx;
};

void * ObjectProxy::Private::replyhandler_launcher(void * p)
{
    AsyncReplyHandlerParam * param = reinterpret_cast<AsyncReplyHandlerParam*>(p);

    param->proxy->dispatch_reply(param->rmsg, param->ctx->_method_name, param->ctx->_user_data);

    delete param->ctx;
    delete param;

    _pool_pthread_exit(NULL);
}

bool ObjectProxy::handle_message(const Message &msg)
{
	switch (msg.type())
	{
		case DBUS_MESSAGE_TYPE_SIGNAL:
		{
			const SignalMessage &smsg = reinterpret_cast<const SignalMessage &>(msg);
			const char *interface	= smsg.interface();
			const char *member	= smsg.member();
			const char *objpath	= smsg.path();

            if (!busy_inc()) {
//                debug_log("COCO: Object(0x%x) trying to inc, but is canceled", this, canceled());
                return false;
            }

            if (objpath != path()) {
                busy_dec();
                return false;
            }

			debug_log("filtered signal %s(in %s) from %s to object %s",
				member, interface, msg.sender(), objpath);

			InterfaceProxy *ii = find_interface(interface);

			if (ii)
			{
				if (_threaded_signal) {
					pthread_t handler;

					SignalHandlerParam* param = new SignalHandlerParam(this, ii, smsg);

					if (_pool_pthread_create(&handler, NULL,
							   Private::sighandler_launcher, param)) {
						// TODO: exception?
	                    {
	                        busy_dec();
	                    }
						debug_log("failed to create signal handler");
						delete param;
						return false;
					}

					if (_pool_pthread_detach(handler)) {
						debug_log("failed to detach signal handler");
						return false;
					}

                    // maintain the same return value logic as InterfaceProxy::dispatch_signal:
                    //     in case of signals, returning false here would allow other proxies of the signal emitting object
                    //     to handle the signal
					return false;
				}
				else {
				    bool dispatchResult = ii->dispatch_signal(smsg);
				    {
				        busy_dec();
				    }
				    return dispatchResult;
				}
			}
			else
			{
			    busy_dec();
				return false;
			}
		}
		default:
		{
			return false;
		}
	}
}

void ObjectProxy::handle_async_reply(PendingCall & pc)
{
    if (canceled())
        return;

    debug_log("handling async call reply");

    AsyncContext * ctx = static_cast<AsyncContext*>(pc.data());

    if (NULL == ctx) {
        debug_log("cannot handle async reply because of NULL context");
        return;
    }

    Message msg = pc.steal_reply();
    if (DBUS_MESSAGE_TYPE_METHOD_RETURN != msg.type()) {
        debug_log("handling async call reply received a wrong message type: %d", msg.type());
        delete ctx;
        return;
    }

    const ReturnMessage & rmsg = static_cast<const ReturnMessage &>(msg);

    InterfaceProxy * i = find_interface(ctx->_if_name);
    if (NULL == i) {
        debug_log("no InterfaceProxy instance found for %s", ctx->_if_name.c_str());
        delete ctx;
        return;
    }


    if (ctx->_on_pool) {
        pthread_t handler;

        AsyncReplyHandlerParam * param = new AsyncReplyHandlerParam(i, rmsg, ctx);

        if (_pool_pthread_create(&handler, NULL,
                Private::replyhandler_launcher, param)) {
            debug_log("failed to create signal handler");
            delete param;
            return;
        }

        if (_pool_pthread_detach(handler)) {
            debug_log("failed to detach signal handler");
            return;
        }

    }
    else {
        i->dispatch_reply(rmsg, ctx->_method_name, ctx->_user_data);
        delete ctx;
    }
}

/* vim:set noexpandtab ts=8 sts=8 sw=8: */
