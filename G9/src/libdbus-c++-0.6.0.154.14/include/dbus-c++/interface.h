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


#ifndef __DBUSXX_INTERFACE_H
#define __DBUSXX_INTERFACE_H

#include <string>
#include <map>
#include "api.h"
#include "util.h"
#include "types.h"

#include "message.h"

namespace DBus {

/// @cond INTERNAL_API
//todo: this should belong to to properties.h
struct DXXAPI PropertyData
{
	bool		read;
	bool		write;
	std::string	sig;
	Variant		value;
};
/// @endcond
typedef std::map<std::string, PropertyData>	PropertyTable;

class IntrospectedInterface;

class ObjectAdaptor;
class InterfaceAdaptor;
class SignalMessage;
typedef Slot<bool, const Message&> MessageSlot;

typedef std::map<std::string, InterfaceAdaptor *> InterfaceAdaptorTable;

/**
 * @cond INTERNAL_API
 */

/**
 * @brief Base interface for an entity which represents a D-DBus object on the server side.
 */
class DXXAPI AdaptorBase
{
public:

	virtual const ObjectAdaptor *object() const = 0 ;

protected:

	InterfaceAdaptor *find_interface(const std::string &name);

	virtual ~AdaptorBase()
	{}

	virtual void _emit_signal(SignalMessage &) = 0;

	InterfaceAdaptorTable _interfaces;
};

/*
*/

class ObjectProxy;
class InterfaceProxy;
class CallMessage;

typedef std::map<std::string, InterfaceProxy *> InterfaceProxyTable;

/**
 * @brief Base interface for an entity which represents a D-DBus object on the client side.
 */
class DXXAPI ProxyBase
{
public:

	virtual const ObjectProxy *object() const = 0 ;

protected:

	InterfaceProxy *find_interface(const std::string &name);

	virtual ~ProxyBase()
	{}

	virtual Message _invoke_method(CallMessage &) = 0;

	virtual bool _invoke_method_noreply(CallMessage &call) = 0;

	virtual bool _invoke_method_async(CallMessage &, void *, bool) = 0;

	virtual void _enable_signal(const InterfaceProxy &, const char *) = 0;
	virtual void _disable_signal(const InterfaceProxy &, const char *) = 0;

	InterfaceProxyTable _interfaces;
};

class DXXAPI Interface
{
public:

	Interface(const std::string &name);

	virtual ~Interface();

	inline const std::string &name() const;

private:

	std::string 	_name;
};

/*
*/

const std::string &Interface::name() const
{
	return _name;
}

/// @endcond
/*
*/

typedef std::map< std::string, Slot<Message, const CallMessage &> > MethodTable;

/**
 * @brief Generic representation of a D-Bus interface on the server side.
 */
class DXXAPI InterfaceAdaptor : public Interface, public virtual AdaptorBase
{
public:

    /**
     * @brief Constructor.
     *
     * @param[in] name The name of the interface.
     */
	InterfaceAdaptor(const std::string &name);

	/**
	 * @brief %Callback which handles a received method call D-Bus message.
	 *
	 * @param[in] msg The method call message.
	 *
	 * @return A method return message or an error message.
	 */
	Message dispatch_method(const CallMessage & msg);

	/**
	 * @brief Sends a signal D-Bus message.
	 *
	 * @param[in] sig The signal message.
	 */
	void emit_signal(const SignalMessage & sig);

	/**
	 * @brief Gives the value of a property.
	 *
	 * @param[in] name The name of the property.
	 *
	 * @return Pointer to the value of the property. Can be NULL if the property does not exist.
	 */
	Variant *get_property(const std::string &name);

	/**
	 * @brief Gives the values of all the interface' properties.
	 *
	 * @param[out] ret A map where the (name, value) pairs are to be returned.
	 */
	void get_all_properties(std::map<std::string, Variant> & ret) const;

	/**
	 * @brief Sets a value for a property.
	 *
	 * @param[in] name The name of the property to set.
	 * @param[in] value The value of the property to set.
	 */
	void set_property(const std::string &name, Variant &value);

	/**
	 * @brief Returns introspection information related to this interface.
	 */
	virtual const IntrospectedInterface * introspect() const
	{
		return NULL;
	}

protected:

	/// @cond INTERNAL_API
	MethodTable	_methods;
	PropertyTable	_properties;
	/// @endcond
};

/*
*/

/// @cond INTERNAL_API
typedef std::map< std::string, Slot<void, const SignalMessage &> > SignalTable;
typedef std::list< std::string > EnabledSignalsList;

typedef std::map< std::string, Slot<void, const ReturnMessage&, void*> > ReplyCallbacksTable;
/// @endcond

/**
 * @brief Generic representation of a D-Bus interface on the client side.
 */
class DXXAPI InterfaceProxy : public Interface, public virtual ProxyBase
{
public:

    /**
     * @brief Constructor.
     *
     * @param[in] name The name of the interface.
     */
	InterfaceProxy(const std::string &name);

	/**
	 * @brief Destructor.
	 */
	~InterfaceProxy();

    /**
     * @brief Sends a method call D-Bus message synchronously.
     *
     * @param[in] call The method call message.
     *
     * @return Reply message received from the peer.
     */
	Message invoke_method(const CallMessage & call);

    /**
     * @brief Sends a no-reply method call D-Bus message.
     *
     * @param[in] call The method call message.
     *
     * @return ``true`` is successful.
     */
	bool invoke_method_noreply(const CallMessage &call);

    /**
     * @brief Sends a method call D-Bus message asynchronously.
     *
     * @param[in] call The method call message.
     * @param[in] udata Opaque user data which will be delivered back to the caller when calling the reply callback.
     * @param[in] on_thread_pool true to pass the reply to the thread pool, or false to call the reply callback in the dispatching thread.
     *
     * @return ``true`` is successful.
     */
	bool invoke_method_async(const CallMessage & call, void * udata, bool on_thread_pool);

    /**
     * @brief %Callback which handles a received signal D-Bus message.
     *
     * @param[in] msg The received signal message.
     *
     * @return ``true`` if the signal was handled.
     */
	bool dispatch_signal(const SignalMessage & msg);

    /**
     * @brief %Callback which handles a method call D-Bus message.
     *
     * @param[in] msg The method call return message.
     * @param[in] method The name of the called method.
     * @param[in] udata Opaque user data which was provided with the method call.
     *
     * @return Method-return message or error message.
     */
	bool dispatch_reply(const ReturnMessage & msg, const std::string & method, void * udata);

	/// @cond INTERNAL_API
	bool is_legacy_mode() const;
	void disable_legacy_mode();
    const EnabledSignalsList & get_pre_enabled_signals();
    /// @endcond

protected:

    /// @cond INTERNAL_API
    void pre_enabled_signals_add(const char *);

    void enable_signal(const char *);
    void disable_signal(const char *);
    /// @endcond

protected:

    /// @cond INTERNAL_API
	SignalTable	_signals;
	EnabledSignalsList _pre_enabled_signals;
	ReplyCallbacksTable _replies;

	bool _is_legacy_mode;
	/// @endcond
};

/**
 * @brief Register an adapter callback for a D-Bus method call message.
 *
 * This is one of the elements which help connect a generic message handling adapter/proxy with
 * a specialized one. These are generally to be called from the constructor of a specialized adapter/proxy
 * to register it's callbacks to specific method/signals messages.
 *
 * These constructs are used by the code generated with `dbusxx-xml2cpp` tool, which will generate the boilerplate code
 * for adapters/proxies based on the introspection xml description.
 *
 */
# define dbusxx_register_method(interface, method, callback) \
    { \
		::DBus::Callback_Base< ::DBus::Message, const ::DBus::CallMessage &> * cb = \
            new ::DBus::Callback< interface, ::DBus::Message, const ::DBus::CallMessage &>(this, & interface :: callback);\
	    InterfaceAdaptor::_methods[ #method ] = cb;\
    }

/**
 * @copydoc dbusxx_register_method
 * @deprecated Use @ref dbusxx_register_method instead.
 */
# define register_method(interface, method, callback) \
    dbusxx_register_method(interface, method, callback)

/**
 * @brief Register an adapter data field as storage for a property.
 *
 * This is one of the elements which help connect a generic message handling adapter/proxy with
 * a specialized one. These are generally to be called from the constructor of a specialized adapter/proxy
 * to register it's callbacks to specific method/signals messages.
 *
 * These constructs are used by the code generated with `dbusxx-xml2cpp` tool, which will generate the boilerplate code
 * for adapters/proxies based on the introspection xml description.
 *
 */
# define dbusxx_bind_property(variable, type, can_read, can_write) \
	InterfaceAdaptor::_properties[ #variable ].read = can_read; \
	InterfaceAdaptor::_properties[ #variable ].write = can_write; \
	InterfaceAdaptor::_properties[ #variable ].sig = type; \
	variable.bind(InterfaceAdaptor::_properties[ #variable ]); \
	{::DBus::MessageIter mi = InterfaceAdaptor::_properties[ #variable ].value.writer(); mi << (int32_t)0; }

/**
 * @copydoc dbusxx_bind_property
 * @deprecated Use @ref dbusxx_bind_property instead.
 */
# define bind_property(variable, type, can_read, can_write) \
    dbusxx_bind_property(variable, type, can_read, can_write)

/**
 * @brief Register a proxy callback for a D-Bus signal message.
 *
 * This is one of the elements which help connect a generic message handling adapter/proxy with
 * a specialized one. These are generally to be called from the constructor of a specialized adapter/proxy
 * to register it's callbacks to specific method/signals messages.
 *
 * These constructs are used by the code generated with `dbusxx-xml2cpp` tool, which will generate the boilerplate code
 * for adapters/proxies based on the introspection xml description.
 *
 */
# define dbusxx_connect_signal(interface, signal, callback) \
    {\
        ::DBus::Callback_Base<void, const ::DBus::SignalMessage &> * cb = \
		    new ::DBus::Callback< interface, void, const ::DBus::SignalMessage &>(this, & interface :: callback); \
	    InterfaceProxy::_signals[ #signal ] = cb; \
    }

/**
 * @copydoc dbusxx_connect_signal
 * @deprecated Use @ref dbusxx_connect_signal instead.
 */
# define connect_signal(interface, signal, callback) \
    dbusxx_connect_signal(interface, signal, callback)

/**
 * @brief Register a proxy callback for a D-Bus method call reply message.
 *
 * This is one of the elements which help connect a generic message handling adapter/proxy with
 * a specialized one. These are generally to be called from the constructor of a specialized adapter/proxy
 * to register it's callbacks to specific method/signals messages.
 *
 * These constructs are used by the code generated with `dbusxx-xml2cpp` tool, which will generate the boilerplate code
 * for adapters/proxies based on the introspection xml description.
 *
 */
#define dbusxx_connect_reply(interface, async_call, callback) \
    { \
        ::DBus::Callback_Base< void, const ::DBus::ReturnMessage &, void* > * cb = \
            new ::DBus::Callback< interface, void, const ::DBus::ReturnMessage &, void* >(this, & interface :: callback); \
        InterfaceProxy::_replies[ #async_call ] = cb; \
    }

/**
 * @copydoc dbusxx_connect_reply
 * @deprecated Use @ref dbusxx_connect_reply instead.
 */
# define connect_reply(interface, async_call, callback) \
    dbusxx_connect_reply(interface, async_call, callback)

} /* namespace DBus */



#endif//__DBUSXX_INTERFACE_H
