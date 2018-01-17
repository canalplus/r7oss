/*
 *
 *  D-Bus++ - C++ bindings for D-Bus
 *
 *  Copyright (C) 2013 Wyplay
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
#include <dbus-c++/object-manager.h>

#include <dbus-c++/interface.h>
#include <dbus-c++/object.h>


namespace DBus {

using namespace std;



ObjectManagerAdaptor::ObjectManagerAdaptor()
: ::DBus::InterfaceAdaptor("org.freedesktop.DBus.ObjectManager")
{
    register_method(ObjectManagerAdaptor, GetManagedObjects, _GetManagedObjects_stub);
}

const ::DBus::IntrospectedInterface * ObjectManagerAdaptor::introspect() const
{
    static ::DBus::IntrospectedArgument GetManagedObjects_args[] =
    {
        { "objpath_interfaces_and_properties", "a{oa{sa{sv}}}", false },
        { 0, 0, 0 }
    };
    static ::DBus::IntrospectedArgument InterfacesAdded_args[] =
    {
        { "object_path", "o", true },
        { "interfaces_and_properties", "a{sa{sv}}", false },
        { 0, 0, 0 }
    };
    static ::DBus::IntrospectedArgument InterfacesRemoved_args[] =
    {
        { "object_path", "o", true },
        { "interfaces", "a(s)", false },
        { 0, 0, 0 }
    };
    static ::DBus::IntrospectedMethod ObjectManager_methods[] =
    {
        { "GetManagedObjects", GetManagedObjects_args },
        { 0, 0 }
    };
    static ::DBus::IntrospectedMethod ObjectManager_signals[] =
    {
        { "InterfacesAdded", InterfacesAdded_args },
        { "InterfacesRemoved", InterfacesRemoved_args },
        { 0, 0 }
    };
    static ::DBus::IntrospectedProperty ObjectManager_properties[] =
    {
        { 0, 0, 0, 0 }
    };
    static ::DBus::IntrospectedInterface ObjectManager_interface =
    {
        "org.freedesktop.DBus.ObjectManager",
        ObjectManager_methods,
        ObjectManager_signals,
        ObjectManager_properties
    };
    return &ObjectManager_interface;
}


ObjectManagerAdaptor::ManagedObjectsReturn_t & ObjectManagerAdaptor::GetManagedObjects(ManagedObjectsReturn_t & objs)
{
    const ::DBus::ObjectAdaptor * obj = this->object();
    ::DBus::ObjectAdaptorPList child_nodes = ::DBus::ObjectAdaptor::from_path_prefix(obj->path().c_str());

    for (::DBus::ObjectAdaptorPList::const_iterator it = child_nodes.begin() ; it != child_nodes.end() ; ++it) {
        ::DBus::debug_log("%s: child object %s", __PRETTY_FUNCTION__, (*it)->path().c_str());

        std::map< std::string, std::map< std::string, ::DBus::Variant > > & obj_info = objs[(*it)->path()];

        InterfaceAdapterPList ifs;
        (*it)->interfaces(ifs);
        for (InterfaceAdapterPList::const_iterator it_if = ifs.begin() ; it_if!=ifs.end() ; ++it_if) {
            std::map< std::string, ::DBus::Variant > & if_info = obj_info[(*it_if)->name()];

            (*it_if)->get_all_properties(if_info);
        }
    }
    return objs;
}

::DBus::Message ObjectManagerAdaptor::_GetManagedObjects_stub(const ::DBus::CallMessage &call)
{
//        ::DBus::MessageIter ri = call.reader();

    std::map< ::DBus::Path, std::map< std::string, std::map< std::string, ::DBus::Variant > > > argout1;
    GetManagedObjects(argout1);

    ::DBus::ReturnMessage reply(call);
    ::DBus::MessageIter wi = reply.writer();
    wi << argout1;
    return reply;
}


}
