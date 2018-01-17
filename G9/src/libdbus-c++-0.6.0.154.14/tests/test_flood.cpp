/*
 *
 *
 *
 *  Copyright (C) 2010 Wyplay
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

#include <iostream>
#include "test_flood.h"

floodProxy::floodProxy(DBus::Connection &conn, const char *name, const char *path) :
            ::DBus::InterfaceProxy(name),
            DBus::ObjectProxy(conn, path, name)
{
    connect_signal(floodProxy, sign, sign);
}

floodProxy::floodProxy(DBus::Connection &conn, const char *name, const char *path, const char *iname) :
            ::DBus::InterfaceProxy(iname),
            DBus::ObjectProxy(conn, path, name)
{
    connect_signal(floodProxy, sign, sign);
}

floodProxy::~floodProxy()
{
}

void floodProxy::sign( const ::DBus::SignalMessage& sig )
{
    ::DBus::MessageIter ri = sig.reader();

    ::DBus::Int32 value; ri >> value;
    //std::cout << "sign called with " << value << std::endl;
}

::DBus::Int32 floodProxy::meth( const ::DBus::Int32& arg1 )
{
    ::DBus::CallMessage call;
    ::DBus::MessageIter wi = call.writer();

    wi << arg1;
    call.member("meth");
    ::DBus::Message ret = invoke_method(call);
    ::DBus::MessageIter ri = ret.reader();

    ::DBus::Int32 argout;
    ri >> argout;
    return argout;
}

floodAdaptor::floodAdaptor(DBus::Connection &conn, const char *name, const char *path) :
    ::DBus::InterfaceAdaptor(name),
    ::DBus::ObjectAdaptor(conn, path)
{
    register_method(floodAdaptor, meth, meth);
}

void floodAdaptor::sign( const ::DBus::Int32& arg1 )
{
    ::DBus::SignalMessage sig("sign");
    ::DBus::MessageIter wi = sig.writer();
    wi << arg1;
    emit_signal(sig);
}


::DBus::Message floodAdaptor::meth( const ::DBus::CallMessage& call )
{
    ::DBus::MessageIter ri = call.reader();
    ::DBus::Int32 argin1; ri >> argin1;
    //std::cout << "meth called with " << argin1 << std::endl;
    ::DBus::Int32 argout1 = 42;
    ::DBus::ReturnMessage reply(call);
    ::DBus::MessageIter wi = reply.writer();
    wi << argout1;
    return reply;
}

