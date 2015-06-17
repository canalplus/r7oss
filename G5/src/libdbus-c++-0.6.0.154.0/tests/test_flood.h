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

#ifndef TEST_FLOOD_H_
#define TEST_FLOOD_H_

#include "dbus-c++/dbus.h"

#define DBUS_NAME "org.freedesktop.DBus.Flood"
#define DBUS_PATH "/org/freedesktop/DBus/Tests/Flood"

class floodProxy :
    public ::DBus::InterfaceProxy,
    public ::DBus::ObjectProxy
{
public:
        floodProxy(DBus::Connection &conn, const char *name = DBUS_NAME, const char *path = DBUS_PATH);
        floodProxy(DBus::Connection &conn, const char *name, const char *path, const char *iname);
        virtual ~floodProxy();
        virtual void sign( const ::DBus::SignalMessage& sig );

        virtual ::DBus::Int32 meth( const ::DBus::Int32& arg1 );
};

class floodAdaptor :
    public ::DBus::InterfaceAdaptor,
    public ::DBus::ObjectAdaptor
{
public:
    floodAdaptor(DBus::Connection &conn, const char *name = DBUS_NAME, const char *path = DBUS_PATH);

    void sign( const ::DBus::Int32& arg1 );

protected:
    virtual ::DBus::Message meth( const ::DBus::CallMessage& call );
};

#endif /* TEST_FLOOD_H_ */
