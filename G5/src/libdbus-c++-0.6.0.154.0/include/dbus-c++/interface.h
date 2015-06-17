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

//todo: this should belong to to properties.h
struct DXXAPI PropertyData
{
	bool		read;
	bool		write;
	std::string	sig;
	Variant		value;
};

typedef std::map<std::string, PropertyData>	PropertyTable;

class IntrospectedInterface;

class ObjectAdaptor;
class InterfaceAdaptor;
class SignalMessage;
typedef Slot<bool, const Message&> MessageSlot;

typedef std::map<std::string, InterfaceAdaptor *> InterfaceAdaptorTable;

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

/*
*/

typedef std::map< std::string, Slot<Message, const CallMessage &> > MethodTable;

class DXXAPI InterfaceAdaptor : public Interface, public virtual AdaptorBase
{
public:

	InterfaceAdaptor(const std::string &name);

	Message dispatch_method(const CallMessage &);

	void emit_signal(const SignalMessage &);

	Variant *get_property(const std::string &name);

	void get_all_properties(std::map<std::string, Variant> & ret) const;

	void set_property(const std::string &name, Variant &value);

	virtual const IntrospectedInterface * introspect() const
	{
		return NULL;
	}

protected:

	MethodTable	_methods;
	PropertyTable	_properties;
};

/*
*/

typedef std::map< std::string, Slot<void, const SignalMessage &> > SignalTable;
typedef std::list< std::string > EnabledSignalsList;

typedef std::map< std::string, Slot<void, const ReturnMessage&, void*> > ReplyCallbacksTable;

class DXXAPI InterfaceProxy : public Interface, public virtual ProxyBase
{
public:

	InterfaceProxy(const std::string &name);

	~InterfaceProxy();

	Message invoke_method(const CallMessage &);

	bool invoke_method_noreply(const CallMessage &call);

	bool invoke_method_async(const CallMessage &, void *, bool);

	bool dispatch_signal(const SignalMessage &);

	bool dispatch_reply(const ReturnMessage &, const std::string &, void *);

	bool is_legacy_mode() const;
	void disable_legacy_mode();
    const EnabledSignalsList & get_pre_enabled_signals();

protected:

    void pre_enabled_signals_add(const char *);

    void enable_signal(const char *);
    void disable_signal(const char *);

protected:

	SignalTable	_signals;
	EnabledSignalsList _pre_enabled_signals;
	ReplyCallbacksTable _replies;

	bool _is_legacy_mode;
};

# define register_method(interface, method, callback) \
    { \
		::DBus::Callback_Base< ::DBus::Message, const ::DBus::CallMessage &> * cb = \
            new ::DBus::Callback< interface, ::DBus::Message, const ::DBus::CallMessage &>(this, & interface :: callback);\
	    InterfaceAdaptor::_methods[ #method ] = cb;\
    }

# define bind_property(variable, type, can_read, can_write) \
	InterfaceAdaptor::_properties[ #variable ].read = can_read; \
	InterfaceAdaptor::_properties[ #variable ].write = can_write; \
	InterfaceAdaptor::_properties[ #variable ].sig = type; \
	variable.bind(InterfaceAdaptor::_properties[ #variable ]); \
	{::DBus::MessageIter mi = InterfaceAdaptor::_properties[ #variable ].value.writer(); mi << (int32_t)0; }
	
# define connect_signal(interface, signal, callback) \
    {\
        ::DBus::Callback_Base<void, const ::DBus::SignalMessage &> * cb = \
		    new ::DBus::Callback< interface, void, const ::DBus::SignalMessage &>(this, & interface :: callback); \
	    InterfaceProxy::_signals[ #signal ] = cb; \
    }

#define connect_reply(interface, async_call, callback) \
    { \
        ::DBus::Callback_Base< void, const ::DBus::ReturnMessage &, void* > * cb = \
            new ::DBus::Callback< interface, void, const ::DBus::ReturnMessage &, void* >(this, & interface :: callback); \
        InterfaceProxy::_replies[ #async_call ] = cb; \
    }

} /* namespace DBus */

#endif//__DBUSXX_INTERFACE_H
