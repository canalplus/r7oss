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


#ifndef __DBUSXX_OBJECT_H
#define __DBUSXX_OBJECT_H

#include <string>
#include <list>

#include "api.h"
#include "interface.h"
#include "connection.h"
#include "message.h"
#include "types.h"

namespace DBus {

/**
 * @brief Generic abstraction of a D-Bus object.
 */
class DXXAPI Object
{
protected:

    /**
     * @brief Constructor.
     *
     * @param[in] conn The connection to be used for communication.
     * @param[in] path The D-Bus path of the object.
     */
	Object(Connection &conn, const Path &path, const char *service);
	
public:

	/**
	 * @brief Destructor
	 */
	virtual ~Object();

	/**
	 * @brief Gets the D-Bus path of the object.
	 *
	 * @return The path of the object.
	 */
	inline const DBus::Path &path() const;

	inline const std::string &service() const;
	
	/**
	 * @brief Gets the connection used for communication.
	 *
	 * @return The connection.
	 */
 	inline Connection &conn();

 	/**
 	 * @brief Sets a default timeout to be used with the object's messages
 	 *
 	 * @param[in] new_timeout The timeout in milliseconds.
 	 */
	void set_timeout(int new_timeout = -1);

	/**
	 * @brief Gets the default timeput to be used with the object's messages.
	 *
	 * @return The timeout in milliseconds.
	 */
	inline int get_timeout() const;

	/**
	 * @cond INTERNAL_API
	 */
	bool canceled();
	void cancel();
	int busy_count();
	bool busy_inc();
	void busy_dec();
	/// @endcond

protected:

	/**
	 * @cond INTERNAL_API
	 */
	inline void path(const Path & p);
	/// @endcond

private:

	DXXAPILOCAL virtual bool handle_message(const Message &) = 0;
	DXXAPILOCAL virtual void register_obj() = 0;
	DXXAPILOCAL virtual void unregister_obj() = 0;

private:

	Connection	_conn;
	DBus::Path	_path;
	std::string	_service;
    int _default_timeout;

    bool _canceled;
    int _busy_count;
};

/*
*/

Connection &Object::conn()
{
	return _conn;
}

const DBus::Path &Object::path() const
{
	return _path;
}

const std::string &Object::service() const
{
	return _service;
}

int Object::get_timeout() const
{
	return _default_timeout;
}

/*
*/

/// @cond INTERNAL_API
class DXXAPI Tag
{
public:

	virtual ~Tag()
	{}
};
/// @endcond

/*
*/

class ObjectAdaptor;

typedef std::list<ObjectAdaptor *> ObjectAdaptorPList;
typedef std::list<std::string> ObjectPathList;

typedef std::list<const InterfaceAdaptor *> InterfaceAdapterPList;

/**
 * @brief Server side abstraction of a D-Bus object.
 */
class DXXAPI ObjectAdaptor : public Object, public virtual AdaptorBase
{
public:

	static ObjectAdaptor *from_path(const Path &path);

	static ObjectAdaptorPList from_path_prefix(const std::string &prefix);

	static ObjectPathList child_nodes_from_prefix(const std::string &prefix);

	struct Private;

	ObjectAdaptor(Connection &conn, const Path &path);

	~ObjectAdaptor();

	inline const ObjectAdaptor *object() const;

public:

	InterfaceAdapterPList & interfaces(InterfaceAdapterPList &) const;

protected:

	class DXXAPI Continuation
	{
	public:

		inline MessageIter &writer();

		inline Tag *tag();

	private:

		Continuation(Connection &conn, const CallMessage &call, const Tag *tag);

		Connection _conn;
		CallMessage _call;
		MessageIter _writer;
		ReturnMessage _return;
		const Tag *_tag;

	friend class ObjectAdaptor;
	};

	void return_later(const Tag *tag);

	void return_now(Continuation *ret);

	void return_error(Continuation *ret, const Error error);

	Continuation *find_continuation(const Tag *tag);

private:

	void _emit_signal(SignalMessage &);

	bool handle_message(const Message &);

	void register_obj();
	void unregister_obj();

	typedef std::map<const Tag *, Continuation *> ContinuationMap;
	ContinuationMap _continuations;

	void method_handler_launcher(InterfaceAdaptor *, const CallMessage &);

friend struct Private;
};

const ObjectAdaptor *ObjectAdaptor::object() const
{
	return this;
}

Tag *ObjectAdaptor::Continuation::tag()
{
	return const_cast<Tag *>(_tag);
}

MessageIter &ObjectAdaptor::Continuation::writer()
{
	return _writer;
}

/*
*/

class ObjectProxy;
/// @cond INTERNAL_API
struct AsyncContext {
    AsyncContext(const char * if_name, const char * method_name, void * user_data, bool on_pool)
    : _if_name(if_name), _method_name(method_name), _user_data(user_data), _on_pool(on_pool)
    {}

    std::string _if_name;
    std::string _method_name;
    //ReturnMessageSlot & _stub_slot;
    void * _user_data;
    bool _on_pool;
};
/// @endcond


typedef std::list<ObjectProxy *> ObjectProxyPList;

/**
 * @brief Client side abstraction of a D-Bus object.
 */
class DXXAPI ObjectProxy : public Object, public virtual ProxyBase
{
public:

    /**
     *
     */
	ObjectProxy(Connection &conn, const Path &path, const char *service = "");
	ObjectProxy(Connection &conn, const Path &path, const char *service, bool threaded_signal);

	~ObjectProxy();

	inline const ObjectProxy *object() const;

	struct Private;

	void change_path(const Path & path);

private:

	Message _invoke_method(CallMessage &);
    
	bool _invoke_method_noreply(CallMessage &call);

	bool _invoke_method_async(CallMessage &, void *, bool);

	bool handle_message(const Message &);
	void handle_async_reply(PendingCall &);

	void register_obj();
	void unregister_obj();

protected:

	/// @private
	void _enable_signal(const InterfaceProxy &, const char *);
	/// @private
	void _disable_signal(const InterfaceProxy &, const char *);

private:

	MessageSlot _filtered;
	bool _threaded_signal;

friend struct Private;
};

const ObjectProxy *ObjectProxy::object() const
{
	return this;
}

} /* namespace DBus */

#endif//__DBUSXX_OBJECT_H
